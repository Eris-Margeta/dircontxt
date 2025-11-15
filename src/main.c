#include <libgen.h> // For basename()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h" // ADDED: For AppConfig
#include "datatypes.h"
#include "dctx_reader.h"
#include "ignore.h"
#include "llm_formatter.h"
#include "platform.h"
#include "utils.h"
#include "walker.h"
#include "writer.h"

// --- Constants ---
#define APP_NAME "dircontxt"
#define APP_VERSION "0.1.0"

// --- Function Declarations ---
static void print_usage(void);
static bool determine_output_filepaths(const char *target_dir_abs_path,
                                       char *dctx_output_filepath_out,
                                       size_t dctx_buffer_size,
                                       char *llm_output_filepath_out,
                                       size_t llm_buffer_size);

// --- Main Function ---
int main(int argc, char *argv[]) {
  // 1. Load Application Configuration
  AppConfig config;
  load_app_config(&config);

  log_info("%s v%s starting.", APP_NAME, APP_VERSION);

  if (argc < 2 || argc > 2) {
    if (argc == 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
      print_usage();
      return EXIT_SUCCESS;
    }
    if (argc == 2 &&
        (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
      return EXIT_SUCCESS;
    }
    print_usage();
    return EXIT_FAILURE;
  }

  const char *target_dir_arg = argv[1];
  char target_dir_abs_path[MAX_PATH_LEN];
  char dctx_filepath[MAX_PATH_LEN];
  char llm_txt_filepath[MAX_PATH_LEN];

  // 2. Resolve target directory and determine output paths
  if (!platform_resolve_path(target_dir_arg, target_dir_abs_path,
                             MAX_PATH_LEN)) {
    log_error("Failed to resolve target directory path: %s", target_dir_arg);
    return EXIT_FAILURE;
  }
  if (!determine_output_filepaths(target_dir_abs_path, dctx_filepath,
                                  MAX_PATH_LEN, llm_txt_filepath,
                                  MAX_PATH_LEN)) {
    log_error("Failed to determine output filepaths.");
    return EXIT_FAILURE;
  }
  log_info("Target directory resolved to: %s", target_dir_abs_path);

  // 3. Load Ignore Rules
  IgnoreRule *ignore_rules = NULL;
  int ignore_rule_count = 0;
  const char *dctx_filename_basename = platform_get_basename(dctx_filepath);
  if (!load_ignore_rules(target_dir_abs_path, dctx_filename_basename,
                         &ignore_rules, &ignore_rule_count)) {
    log_error("Failed to load ignore rules. Critical error.");
    return EXIT_FAILURE;
  }
  log_info("Loaded %d ignore rules.", ignore_rule_count);

  // 4. Walk Directory and Build Tree
  int processed_items = 0;
  DirContextTreeNode *tree = walk_directory_and_build_tree(
      target_dir_abs_path, ignore_rules, ignore_rule_count, &processed_items);
  if (tree == NULL) {
    log_error("Failed to walk directory and build tree for: %s",
              target_dir_abs_path);
    free_ignore_rules_array(ignore_rules, ignore_rule_count);
    return EXIT_FAILURE;
  }

#if DEBUG_LOGGING_ENABLED
  log_debug("--- In-memory Tree Structure ---");
  print_tree_recursive(tree, 0);
  log_debug("---------------------------------");
#endif

  // 5. Generate Output Files Based on Configuration
  bool binary_file_written = false;
  bool text_file_written = false;
  int exit_code = EXIT_SUCCESS;

  // The binary file is a necessary intermediate for creating the text file.
  // We always generate it, but may delete it later if the user only wants the
  // text file.
  if (config.output_mode == OUTPUT_MODE_BOTH ||
      config.output_mode == OUTPUT_MODE_BINARY_ONLY ||
      config.output_mode == OUTPUT_MODE_TEXT_ONLY) {
    log_info("Writing binary archive to: %s", dctx_filepath);
    if (!write_dircontxt_file(dctx_filepath, tree)) {
      log_error("Failed to write .dircontxt binary file to: %s", dctx_filepath);
      exit_code = EXIT_FAILURE;
      goto cleanup;
    }
    binary_file_written = true;
    log_info("Successfully created binary .dircontxt file (%d items included).",
             processed_items);
  }

  if (config.output_mode == OUTPUT_MODE_BOTH ||
      config.output_mode == OUTPUT_MODE_TEXT_ONLY) {
    log_info("Generating LLM context file: %s", llm_txt_filepath);
    DirContextTreeNode *tree_for_llm = NULL;
    uint64_t data_section_offset = 0;
    if (!dctx_read_and_parse_header(dctx_filepath, &tree_for_llm,
                                    &data_section_offset)) {
      log_error(
          "Failed to read back .dircontxt header. Cannot generate text file.",
          dctx_filepath);
      exit_code = EXIT_FAILURE;
      goto cleanup;
    }

    if (!generate_llm_context_file(llm_txt_filepath, tree_for_llm,
                                   dctx_filepath, data_section_offset)) {
      log_error("Failed to generate .llmcontext.txt file at: %s",
                llm_txt_filepath);
      exit_code = EXIT_FAILURE;
    } else {
      text_file_written = true;
      log_info("Successfully generated .llmcontext.txt file: %s",
               llm_txt_filepath);
    }
    free_tree_recursive(tree_for_llm);
  }

  // If the mode was 'text_only', we remove the intermediate binary file.
  if (config.output_mode == OUTPUT_MODE_TEXT_ONLY && binary_file_written) {
    if (remove(dctx_filepath) == 0) {
      log_info("Removed intermediate binary file as per text-only mode: %s",
               dctx_filepath);
    } else {
      log_error("Failed to remove intermediate binary file: %s", dctx_filepath);
    }
  }

cleanup:
  // 6. Final Cleanup
  if (tree != NULL) {
    free_tree_recursive(tree);
  }
  free_ignore_rules_array(ignore_rules, ignore_rule_count);

  log_info("dircontxt run finished.");
  return exit_code;
}

static void print_usage(void) {
  printf("Usage: %s <target_directory>\n", APP_NAME);
  printf("Creates a context snapshot of the specified directory.\n");
  printf("Behavior is controlled by ~/.config/dircontxt/config\n\n");
  printf("Options:\n");
  printf("  -h, --help     Show this help message and exit.\n");
  printf("  -v, --version  Show version information and exit.\n");
}

static bool determine_output_filepaths(const char *target_dir_abs_path,
                                       char *dctx_output_filepath_out,
                                       size_t dctx_buffer_size,
                                       char *llm_output_filepath_out,
                                       size_t llm_buffer_size) {
  char *target_basename = get_directory_basename(target_dir_abs_path);
  if (!target_basename) {
    log_error("Failed to get basename for target directory: %s",
              target_dir_abs_path);
    return false;
  }

  char dctx_filename[MAX_PATH_LEN];
  snprintf(dctx_filename, MAX_PATH_LEN, "%s.dircontxt", target_basename);

  char llm_filename[MAX_PATH_LEN];
  snprintf(llm_filename, MAX_PATH_LEN, "%s.llmcontext.txt", target_basename);

  free(target_basename);

  char *parent_of_target_dir = platform_get_dirname(target_dir_abs_path);
  if (!parent_of_target_dir) {
    log_error("Failed to get parent directory of: %s", target_dir_abs_path);
    return false;
  }

  bool success = true;
  if (!platform_join_paths(parent_of_target_dir, dctx_filename,
                           dctx_output_filepath_out, dctx_buffer_size)) {
    success = false;
  }
  if (success &&
      !platform_join_paths(parent_of_target_dir, llm_filename,
                           llm_output_filepath_out, llm_buffer_size)) {
    success = false;
  }

  free(parent_of_target_dir);
  return success;
}
