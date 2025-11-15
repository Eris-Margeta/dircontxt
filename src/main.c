#include <libgen.h> // For basename()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For stat() used in file_exists

#include "config.h"
#include "datatypes.h"
#include "dctx_reader.h"
#include "diff.h"
#include "ignore.h"
#include "llm_formatter.h"
#include "platform.h"
#include "utils.h"
#include "version.h"
#include "walker.h"
#include "writer.h"

// --- Constants ---
#define APP_NAME "dircontxt"
#define APP_VERSION "0.1.0"

// --- Function Declarations ---
static void print_usage(void);
static bool file_exists(const char *filepath);
static bool determine_output_filepaths(
    const char *target_dir_abs_path, char *dctx_output_filepath_out,
    size_t dctx_buffer_size, char *llm_output_filepath_out,
    size_t llm_buffer_size, char *diff_filepath_out, size_t diff_buffer_size,
    const char *version_string);

// --- Main Function ---
int main(int argc, char *argv[]) {
  AppConfig config;
  load_app_config(&config);

  log_info("%s v%s starting.", APP_NAME, APP_VERSION);

  if (argc != 2 ||
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    print_usage();
    return EXIT_SUCCESS;
  }
  if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
    return EXIT_SUCCESS;
  }

  // --- 1. Path Resolution and Initial Setup ---
  char target_dir_abs_path[MAX_PATH_LEN];
  char dctx_filepath[MAX_PATH_LEN];
  char llm_txt_filepath[MAX_PATH_LEN];
  char diff_filepath[MAX_PATH_LEN];

  if (!platform_resolve_path(argv[1], target_dir_abs_path, MAX_PATH_LEN)) {
    log_error("Failed to resolve target directory path: %s", argv[1]);
    return EXIT_FAILURE;
  }
  log_info("Target directory resolved to: %s", target_dir_abs_path);

  // --- 2. Versioning Logic ---
  char old_version[32] = {0};
  char new_version[32] = {0};
  DirContextTreeNode *old_tree = NULL;

  determine_output_filepaths(target_dir_abs_path, dctx_filepath, MAX_PATH_LEN,
                             llm_txt_filepath, MAX_PATH_LEN, diff_filepath,
                             MAX_PATH_LEN, "");

  if (file_exists(llm_txt_filepath) && file_exists(dctx_filepath)) {
    log_info("Existing context and binary files found. Running in update/diff "
             "mode.");
    if (!parse_version_from_file(llm_txt_filepath, old_version,
                                 sizeof(old_version))) {
      log_error("Could not parse version. Starting over with V1.");
      safe_strncpy(old_version, "V1", sizeof(old_version));
    }
    calculate_next_version(old_version, new_version, sizeof(new_version));

    log_info("Loading previous state from %s", dctx_filepath);
    uint64_t old_data_offset;
    if (!dctx_read_and_parse_header(dctx_filepath, &old_tree,
                                    &old_data_offset)) {
      log_error("Failed to read previous binary file. Old state ignored.");
      old_tree = NULL;
    }
  } else {
    if (file_exists(llm_txt_filepath) && !file_exists(dctx_filepath)) {
      log_info("Warning: Text file found but required binary archive is "
               "missing. Cannot perform diff.");
    }
    log_info("Creating new V1 snapshot.");
    safe_strncpy(new_version, "V1", sizeof(new_version));
    safe_strncpy(old_version, "V1", sizeof(old_version));
  }

  log_info("Current context version will be: %s", new_version);

  determine_output_filepaths(target_dir_abs_path, dctx_filepath, MAX_PATH_LEN,
                             llm_txt_filepath, MAX_PATH_LEN, diff_filepath,
                             MAX_PATH_LEN, new_version);

  // --- 3. Scan Current Directory State ---
  IgnoreRule *ignore_rules = NULL;
  int ignore_rule_count = 0;
  if (!load_ignore_rules(target_dir_abs_path,
                         platform_get_basename(dctx_filepath), &ignore_rules,
                         &ignore_rule_count)) {
    log_error("Failed to load ignore rules.");
    if (old_tree)
      free_tree_recursive(old_tree);
    return EXIT_FAILURE;
  }

  int processed_items = 0;
  DirContextTreeNode *new_tree = walk_directory_and_build_tree(
      target_dir_abs_path, ignore_rules, ignore_rule_count, &processed_items);
  if (new_tree == NULL) {
    log_error("Failed to walk directory and build new tree.");
    if (old_tree)
      free_tree_recursive(old_tree);
    free_ignore_rules_array(ignore_rules, ignore_rule_count);
    return EXIT_FAILURE;
  }
  log_info("Directory walk completed. Processed %d items (files/dirs).",
           processed_items);

  // --- 4. Overwrite Binary and Generate Diff ---
  int exit_code = EXIT_SUCCESS;

  log_info("Writing binary archive to: %s", dctx_filepath);
  if (!write_dircontxt_file(dctx_filepath, new_tree)) {
    log_error("Failed to write the .dircontxt binary file. Cannot proceed.");
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  if (old_tree != NULL) {
    log_info("Comparing new state to previous state...");
    DiffReport *report = compare_trees(old_tree, new_tree);
    if (report && report->has_changes) {
      log_info("Changes detected. Generating diff file: %s", diff_filepath);
      uint64_t new_data_offset = 0;
      DirContextTreeNode *temp_tree_for_diff = NULL;
      if (dctx_read_and_parse_header(dctx_filepath, &temp_tree_for_diff,
                                     &new_data_offset)) {
        generate_diff_file(diff_filepath, report, temp_tree_for_diff,
                           dctx_filepath, new_data_offset, old_version,
                           new_version);
        free_tree_recursive(temp_tree_for_diff);
      }
    } else {
      log_info("No changes detected since version %s.", old_version);
    }
    free_diff_report(report);
  }

  // --- 5. Generate Text Output based on Config ---
  if (config.output_mode == OUTPUT_MODE_BINARY_ONLY) {
    log_info("Skipping text file generation as per binary-only mode.");
    // Clean up old text/diff files if they exist
    if (file_exists(llm_txt_filepath))
      remove(llm_txt_filepath);
    if (file_exists(diff_filepath))
      remove(diff_filepath);
  } else { // This covers BOTH and TEXT_ONLY modes
    log_info("Generating LLM context file: %s", llm_txt_filepath);
    uint64_t final_data_offset = 0;
    DirContextTreeNode *final_tree_for_llm = NULL;

    if (!dctx_read_and_parse_header(dctx_filepath, &final_tree_for_llm,
                                    &final_data_offset)) {
      log_error("Failed to read back binary. Cannot generate text file.");
      exit_code = EXIT_FAILURE;
    } else {
      if (!generate_llm_context_file(llm_txt_filepath, final_tree_for_llm,
                                     dctx_filepath, final_data_offset,
                                     new_version)) {
        log_error("Failed to generate .llmcontext.txt file.");
        exit_code = EXIT_FAILURE;
      }
      free_tree_recursive(final_tree_for_llm);
    }
  }

cleanup:
  // --- 6. Final Memory Free ---
  if (old_tree)
    free_tree_recursive(old_tree);
  if (new_tree)
    free_tree_recursive(new_tree);
  free_ignore_rules_array(ignore_rules, ignore_rule_count);

  log_info("dircontxt run finished.");
  return exit_code;
}

static void print_usage(void) {
  printf("Usage: %s <target_directory>\n", APP_NAME);
  printf("Creates a versioned context snapshot of the specified directory.\n");
  printf("Behavior is controlled by ~/.config/dircontxt/config\n\n");
  printf("Options:\n");
  printf("  -h, --help     Show this help message and exit.\n");
  printf("  -v, --version  Show version information and exit.\n");
}

static bool file_exists(const char *filepath) {
  if (filepath == NULL || filepath[0] == '\0')
    return false;
  struct stat buffer;
  return (stat(filepath, &buffer) == 0);
}

static bool determine_output_filepaths(
    const char *target_dir_abs_path, char *dctx_output_filepath_out,
    size_t dctx_buffer_size, char *llm_output_filepath_out,
    size_t llm_buffer_size, char *diff_filepath_out, size_t diff_buffer_size,
    const char *version_string) {
  char *target_basename = get_directory_basename(target_dir_abs_path);
  if (!target_basename)
    return false;

  char *parent_dir = platform_get_dirname(target_dir_abs_path);
  if (!parent_dir) {
    free(target_basename);
    return false;
  }

  char dctx_filename[MAX_PATH_LEN];
  snprintf(dctx_filename, MAX_PATH_LEN, "%s.dircontxt", target_basename);
  platform_join_paths(parent_dir, dctx_filename, dctx_output_filepath_out,
                      dctx_buffer_size);

  char llm_filename[MAX_PATH_LEN];
  snprintf(llm_filename, MAX_PATH_LEN, "%s.llmcontext.txt", target_basename);
  platform_join_paths(parent_dir, llm_filename, llm_output_filepath_out,
                      llm_buffer_size);

  // Diff file is only created for an update (e.g., "V1.1", "V1.2", etc.)
  if (version_string != NULL && strchr(version_string, '.') != NULL) {
    char diff_filename[MAX_PATH_LEN];
    snprintf(diff_filename, MAX_PATH_LEN, "%s.llmcontext-%s-diff.txt",
             target_basename, version_string);
    platform_join_paths(parent_dir, diff_filename, diff_filepath_out,
                        diff_buffer_size);
  } else {
    if (diff_filepath_out != NULL && diff_buffer_size > 0)
      diff_filepath_out[0] = '\0';
  }

  free(target_basename);
  free(parent_dir);
  return true;
}
