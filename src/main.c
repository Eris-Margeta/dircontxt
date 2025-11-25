/* src/main.c */
#include <libgen.h> // For basename()
#include <stdarg.h> // For va_list, va_start, va_end
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
#define APP_VERSION "0.1.2"

// --- Function Declarations ---
static void print_usage(void);
static bool file_exists(const char *filepath);
static bool determine_output_filepaths(
    const char *target_dir_abs_path, char *dctx_output_filepath_out,
    size_t dctx_buffer_size, char *llm_output_filepath_out,
    size_t llm_buffer_size, char *diff_filepath_out, size_t diff_buffer_size,
    const char *version_string);

// Helpers for content verification
static DirContextTreeNode *find_node_by_path(DirContextTreeNode *root,
                                             const char *path);
static bool files_content_identical(const char *disk_path,
                                    const char *dctx_path,
                                    uint64_t dctx_data_offset,
                                    const DirContextTreeNode *old_node);
static void filter_false_positives(DiffReport *report,
                                   DirContextTreeNode *old_root,
                                   DirContextTreeNode *new_root,
                                   const char *dctx_filepath,
                                   uint64_t old_data_offset);

// Internal forced logging to debug release builds
static void log_forced_debug(const char *fmt, ...) {
  va_list args;
  printf("[DEBUG] ");
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}

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

  // --- 2. Load Previous State (If Exists) ---
  char old_version[32] = {0};
  char new_version[32] = {0};
  DirContextTreeNode *old_tree = NULL;
  uint64_t old_data_offset = 0;
  bool is_update_mode = false;

  // Determine paths without version string to find existing files
  determine_output_filepaths(target_dir_abs_path, dctx_filepath, MAX_PATH_LEN,
                             llm_txt_filepath, MAX_PATH_LEN, diff_filepath,
                             MAX_PATH_LEN, "");

  if (file_exists(llm_txt_filepath) && file_exists(dctx_filepath)) {
    is_update_mode = true;
    if (!parse_version_from_file(llm_txt_filepath, old_version,
                                 sizeof(old_version))) {
      log_error(
          "Could not parse version from existing text file. Assuming V1.");
      safe_strncpy(old_version, "V1", sizeof(old_version));
    }

    log_info("Loading previous state from %s (Version: %s)", dctx_filepath,
             old_version);
    if (!dctx_read_and_parse_header(dctx_filepath, &old_tree,
                                    &old_data_offset)) {
      log_error("Failed to read previous binary file. Old state ignored.");
      old_tree = NULL;
      is_update_mode = false;
    }
  } else {
    log_info("No valid previous state found. Starting fresh.");
  }

  // --- 3. Scan Current Directory State (New Tree) ---
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

  // --- 4. Compare and Determine Next Version ---
  DiffReport *report = NULL;
  bool has_actual_changes = false;

  if (is_update_mode && old_tree != NULL) {
    log_info("Comparing new state to previous state...");
    report = compare_trees(old_tree, new_tree);

    if (report && report->has_changes) {
      // Perform deep content verification to ignore "touch" updates
      // (timestamps changed but content identical)
      filter_false_positives(report, old_tree, new_tree, dctx_filepath,
                             old_data_offset);
    }

    if (report && report->has_changes) {
      has_actual_changes = true;
      log_info("Changes detected (%d items modified/added/removed).",
               report->count);
    } else {
      log_info("No actual content changes detected.");
    }
  } else {
    // New snapshot
    has_actual_changes = true;
  }

  // Calculate Version String
  if (is_update_mode) {
    if (has_actual_changes) {
      calculate_next_version(old_version, new_version, sizeof(new_version));
    } else {
      // No changes: Keep the old version
      safe_strncpy(new_version, old_version, sizeof(new_version));
    }
  } else {
    safe_strncpy(new_version, "V1", sizeof(new_version));
    safe_strncpy(old_version, "V1", sizeof(old_version));
  }

  log_info("Snapshot Version: %s", new_version);

  // Update filepaths with the final determined version (affects Diff filename)
  determine_output_filepaths(target_dir_abs_path, dctx_filepath, MAX_PATH_LEN,
                             llm_txt_filepath, MAX_PATH_LEN, diff_filepath,
                             MAX_PATH_LEN, new_version);

  // --- 5. Overwrite Binary Archive ---
  // We write this AFTER comparison because comparison reads from the OLD
  // binary.
  int exit_code = EXIT_SUCCESS;

  log_info("Writing binary archive to: %s", dctx_filepath);
  if (!write_dircontxt_file(dctx_filepath, new_tree)) {
    log_error("Failed to write the .dircontxt binary file. Cannot proceed.");
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }

  // --- 6. Generate Outputs (Diff and Context) ---
  if (has_actual_changes && is_update_mode) {
    // Generate Diff
    log_info("Generating diff file: %s", diff_filepath);

    // We re-read the binary header just to get the offset of the NEW file
    uint64_t new_data_offset = 0;
    DirContextTreeNode *temp_tree_for_diff = NULL;

    if (dctx_read_and_parse_header(dctx_filepath, &temp_tree_for_diff,
                                   &new_data_offset)) {
      generate_diff_file(diff_filepath, report, temp_tree_for_diff,
                         dctx_filepath, new_data_offset, old_version,
                         new_version);
      free_tree_recursive(temp_tree_for_diff);
    }
  } else if (!has_actual_changes && is_update_mode) {
    // Cleanup any old diff file if we are re-running on same version
    if (diff_filepath[0] != '\0' && file_exists(diff_filepath)) {
      remove(diff_filepath);
    }
  }

  // Generate Main Context File
  if (config.output_mode == OUTPUT_MODE_BINARY_ONLY) {
    log_info("Skipping text file generation as per binary-only mode.");
    if (file_exists(llm_txt_filepath))
      remove(llm_txt_filepath);
  } else {
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
  // --- 7. Final Memory Free ---
  if (report)
    free_diff_report(report);
  if (old_tree)
    free_tree_recursive(old_tree);
  if (new_tree)
    free_tree_recursive(new_tree);
  free_ignore_rules_array(ignore_rules, ignore_rule_count);

  log_info("dircontxt run finished.");
  return exit_code;
}

// --- Helper Functions ---

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

static DirContextTreeNode *find_node_by_path(DirContextTreeNode *root,
                                             const char *path) {
  if (!root)
    return NULL;
  if (strcmp(root->relative_path, path) == 0)
    return root;

  if (root->type == NODE_TYPE_DIRECTORY) {
    for (uint32_t i = 0; i < root->num_children; ++i) {
      const char *child_path = root->children[i]->relative_path;
      size_t child_len = strlen(child_path);
      if (strncmp(path, child_path, child_len) == 0) {
        if (path[child_len] == PLATFORM_DIR_SEPARATOR ||
            path[child_len] == '\0') {
          DirContextTreeNode *found =
              find_node_by_path(root->children[i], path);
          if (found)
            return found;
        }
      }
    }
  }
  return NULL;
}

static bool files_content_identical(const char *disk_path,
                                    const char *dctx_path,
                                    uint64_t dctx_data_offset,
                                    const DirContextTreeNode *old_node) {
  FILE *disk_fp = fopen(disk_path, "rb");
  if (!disk_fp) {
    log_forced_debug("   -> Verification failed: Could not open disk file '%s'",
                     disk_path);
    return false;
  }

  FILE *dctx_fp = fopen(dctx_path, "rb");
  if (!dctx_fp) {
    log_forced_debug("   -> Verification failed: Could not open binary '%s'",
                     dctx_path);
    fclose(disk_fp);
    return false;
  }

  bool identical = true;
  char buf1[4096];
  char buf2[4096];

  uint64_t abs_offset =
      dctx_data_offset + old_node->content_offset_in_data_section;
  if (fseek(dctx_fp, (long)abs_offset, SEEK_SET) != 0) {
    log_forced_debug("   -> Verification failed: fseek error in binary.");
    identical = false;
    goto done;
  }

  size_t remaining = old_node->content_size;
  while (remaining > 0) {
    size_t to_read = (remaining < sizeof(buf1)) ? remaining : sizeof(buf1);

    size_t read1 = fread(buf1, 1, to_read, disk_fp);
    size_t read2 = fread(buf2, 1, to_read, dctx_fp);

    if (read1 != to_read || read2 != to_read) {
      log_forced_debug("   -> Verification failed: Read size mismatch "
                       "(Disk:%zu, Bin:%zu, Expected:%zu)",
                       read1, read2, to_read);
      identical = false;
      break;
    }
    if (memcmp(buf1, buf2, to_read) != 0) {
      // Log first few bytes of difference for debugging
      log_forced_debug(
          "   -> Verification failed: Content mismatch at some byte.");
      identical = false;
      break;
    }
    remaining -= to_read;
  }

done:
  fclose(disk_fp);
  fclose(dctx_fp);
  return identical;
}

static void filter_false_positives(DiffReport *report,
                                   DirContextTreeNode *old_root,
                                   DirContextTreeNode *new_root,
                                   const char *dctx_filepath,
                                   uint64_t old_data_offset) {
  if (!report || report->count == 0)
    return;

  int new_count = 0;
  for (int i = 0; i < report->count; ++i) {
    DiffEntry *entry = &report->entries[i];
    bool keep = true;

    if (entry->type == ITEM_MODIFIED && entry->node_type == NODE_TYPE_FILE) {
      DirContextTreeNode *new_node =
          find_node_by_path(new_root, entry->relative_path);
      DirContextTreeNode *old_node =
          find_node_by_path(old_root, entry->relative_path);

      if (new_node && old_node) {
        if (new_node->content_size == old_node->content_size) {
          // Sizes match, timestamps differ. Check content.
          if (files_content_identical(new_node->disk_path, dctx_filepath,
                                      old_data_offset, old_node)) {
            keep = false; // Identical content -> False positive
            log_forced_debug(
                "Ignoring metadata-only change for: %s (Content Identical)",
                entry->relative_path);
          } else {
            log_forced_debug("Confirmed content change for: %s",
                             entry->relative_path);
          }
        } else {
          log_forced_debug("Confirmed size change for: %s",
                           entry->relative_path);
        }
      }
    } else {
      // Force log addition/removal to see if path matching is the issue
      if (entry->type == ITEM_ADDED)
        log_forced_debug("Diff: ADDED %s", entry->relative_path);
      if (entry->type == ITEM_REMOVED)
        log_forced_debug("Diff: REMOVED %s", entry->relative_path);
    }

    if (keep) {
      if (i != new_count) {
        report->entries[new_count] = report->entries[i];
      }
      new_count++;
    }
  }

  report->count = new_count;
  report->has_changes = (new_count > 0);
}
