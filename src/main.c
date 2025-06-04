#include <libgen.h> // For basename() to get target directory name for output file
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datatypes.h"
#include "ignore.h"
#include "platform.h"
#include "utils.h"
#include "walker.h"
#include "writer.h"

// --- Constants ---
#define APP_NAME "dircontxt"
#define APP_VERSION "0.1.0"

// --- Function Declarations ---
static void print_usage(void);
static bool determine_output_filepath(const char *target_dir_abs_path,
                                      const char *input_target_arg,
                                      char *output_filepath_out,
                                      size_t buffer_size);

// --- Main Function ---
int main(int argc, char *argv[]) {
  log_info("%s v%s starting.", APP_NAME, APP_VERSION);

  if (argc < 2 || argc > 2) { // Expects one argument: target directory
    if (argc == 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
      print_usage();
      return EXIT_SUCCESS;
    }
    if (argc == 2 &&
        (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
      // Version is already printed by log_info
      return EXIT_SUCCESS;
    }
    print_usage();
    return EXIT_FAILURE;
  }

  const char *target_dir_arg = argv[1];
  char target_dir_abs_path[MAX_PATH_LEN];
  char output_filepath[MAX_PATH_LEN];

  // 1. Resolve target directory to absolute path
  if (!platform_resolve_path(target_dir_arg, target_dir_abs_path,
                             MAX_PATH_LEN)) {
    log_error("Failed to resolve target directory path: %s", target_dir_arg);
    return EXIT_FAILURE;
  }
  log_info("Target directory resolved to: %s", target_dir_abs_path);

  // 2. Determine output file path
  //    Output file will be named <target_dir_name>.dircontxt and placed in the
  //    *parent* of target_dir_abs_path OR if target_dir_arg is ".", it will be
  //    <current_dir_name>.dircontxt in the current dir's parent.
  if (!determine_output_filepath(target_dir_abs_path, target_dir_arg,
                                 output_filepath, MAX_PATH_LEN)) {
    log_error("Failed to determine output filepath.");
    return EXIT_FAILURE;
  }
  log_info("Output file will be: %s", output_filepath);

  // 3. Load Ignore Rules
  IgnoreRule *ignore_rules = NULL;
  int ignore_rule_count = 0;
  // Pass the basename of the output file to ignore it by default.
  // `platform_get_basename` result here is safe as `output_filepath` string
  // outlives this call.
  const char *output_filename_basename = platform_get_basename(output_filepath);

  if (!load_ignore_rules(target_dir_abs_path, output_filename_basename,
                         &ignore_rules, &ignore_rule_count)) {
    log_error("Failed to load ignore rules. Critical error.");
    // free_ignore_rules_array(ignore_rules, ignore_rule_count); // Already
    // handled if realloc failed
    return EXIT_FAILURE;
  }
  log_info("Loaded %d ignore rules.", ignore_rule_count);
  for (int i = 0; i < ignore_rule_count; ++i) {
    log_debug("Ignore Rule %d: pattern='%s', dir_only=%d, wc_prefix=%d, "
              "wc_suffix=%d, exact=%d",
              i, ignore_rules[i].pattern, ignore_rules[i].is_dir_only,
              ignore_rules[i].is_wildcard_prefix_match,
              ignore_rules[i].is_wildcard_suffix_match,
              ignore_rules[i].is_exact_name_match);
  }

  // 4. Walk Directory and Build Tree
  int processed_items = 0;
  DirContextTreeNode *root_node = walk_directory_and_build_tree(
      target_dir_abs_path, ignore_rules, ignore_rule_count, &processed_items);

  if (root_node == NULL) {
    log_error("Failed to walk directory and build tree for: %s",
              target_dir_abs_path);
    free_ignore_rules_array(ignore_rules, ignore_rule_count);
    return EXIT_FAILURE;
  }
  log_info("Directory tree built. Root node relative path: '%s'",
           root_node->relative_path);

#if DEBUG_LOGGING_ENABLED // Conditionally compile debug tree print
  log_debug("--- In-memory Tree Structure ---");
  print_tree_recursive(root_node, 0);
  log_debug("-------------------------------");
#endif

  // 5. Write .dircontxt File
  log_info("Writing to .dircontxt file: %s", output_filepath);
  if (!write_dircontxt_file(output_filepath, root_node)) {
    log_error("Failed to write .dircontxt file to: %s", output_filepath);
    free_tree_recursive(root_node);
    free_ignore_rules_array(ignore_rules, ignore_rule_count);
    return EXIT_FAILURE;
  }

  log_info("Successfully created %s (%d items included).", output_filepath,
           processed_items);

  // 6. Cleanup
  free_tree_recursive(root_node);
  free_ignore_rules_array(ignore_rules, ignore_rule_count);

  return EXIT_SUCCESS;
}

// --- Helper Function Implementations ---

static void print_usage(void) {
  printf("Usage: %s <target_directory>\n", APP_NAME);
  printf("Creates a .dircontxt file for the specified target directory.\n");
  printf("The output file will be named <target_directory_name>.dircontxt \n");
  printf("and placed in the parent directory of <target_directory>.\n");
  printf("If <target_directory> is '.', the output is "
         "<current_folder_name>.dircontxt \n");
  printf("in the parent of the current working directory.\n\n");
  printf("Options:\n");
  printf("  -h, --help     Show this help message and exit.\n");
  printf("  -v, --version  Show version information and exit.\n");
}

static bool determine_output_filepath(
    const char
        *target_dir_abs_path,     // Absolute path of the directory to archive
    const char *input_target_arg, // Original argument (e.g., "." or "myfolder")
    char *output_filepath_out, size_t buffer_size) {
  char dir_to_get_name_from[MAX_PATH_LEN];
  safe_strncpy(dir_to_get_name_from, target_dir_abs_path, MAX_PATH_LEN);

  // Use utils get_directory_basename to handle potential trailing slashes
  // correctly
  char *target_basename = get_directory_basename(dir_to_get_name_from);
  if (!target_basename) {
    log_error("Failed to get basename for target directory: %s",
              dir_to_get_name_from);
    return false;
  }

  char output_filename[MAX_PATH_LEN];
  snprintf(output_filename, MAX_PATH_LEN, "%s.dircontxt", target_basename);
  free(target_basename); // Free the string returned by get_directory_basename

  // Output file goes into the PARENT of the target_dir_abs_path
  char *parent_of_target_dir = platform_get_dirname(target_dir_abs_path);
  if (!parent_of_target_dir) {
    log_error("Failed to get parent directory of: %s", target_dir_abs_path);
    return false;
  }

  // If target_dir_abs_path was something like "/", its parent is also "/".
  // Ensure we don't try to create "/.dircontxt" or similar without permissions.
  // For simplicity, let's assume paths are typical user-space paths.
  if (strcmp(parent_of_target_dir, PLATFORM_DIR_SEPARATOR_STR) == 0 &&
      strcmp(target_dir_abs_path, PLATFORM_DIR_SEPARATOR_STR) == 0) {
    // Trying to process root itself, outputting to root. Might be an issue.
    // Or just let platform_join_paths handle it, it might be fine depending on
    // OS.
  }

  if (!platform_join_paths(parent_of_target_dir, output_filename,
                           output_filepath_out, buffer_size)) {
    log_error("Failed to join parent path '%s' and output filename '%s'",
              parent_of_target_dir, output_filename);
    free(parent_of_target_dir);
    return false;
  }

  free(
      parent_of_target_dir); // Free the string returned by platform_get_dirname
  return true;
}
