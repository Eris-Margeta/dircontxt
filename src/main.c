#include <libgen.h> // For basename() to get target directory name for output file
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datatypes.h"
#include "dctx_reader.h" // Added
#include "ignore.h"
#include "llm_formatter.h" // Added
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
  char dctx_filepath[MAX_PATH_LEN];    // For the binary .dircontxt file
  char llm_txt_filepath[MAX_PATH_LEN]; // For the .llmcontext.txt file

  // 1. Resolve target directory to absolute path
  if (!platform_resolve_path(target_dir_arg, target_dir_abs_path,
                             MAX_PATH_LEN)) {
    log_error("Failed to resolve target directory path: %s", target_dir_arg);
    return EXIT_FAILURE;
  }
  log_info("Target directory resolved to: %s", target_dir_abs_path);

  // 2. Determine output file paths (for both .dircontxt and .llmcontext.txt)
  if (!determine_output_filepaths(target_dir_abs_path, dctx_filepath,
                                  MAX_PATH_LEN, llm_txt_filepath,
                                  MAX_PATH_LEN)) {
    log_error("Failed to determine output filepaths.");
    return EXIT_FAILURE;
  }
  log_info("Binary output file will be: %s", dctx_filepath);
  log_info("LLM text output file will be: %s", llm_txt_filepath);

  // 3. Load Ignore Rules
  IgnoreRule *ignore_rules = NULL;
  int ignore_rule_count = 0;
  const char *dctx_filename_basename = platform_get_basename(dctx_filepath);
  // We should also ignore the .llmcontext.txt file if it exists during the walk
  const char *llm_filename_basename = platform_get_basename(llm_txt_filepath);

  // To pass multiple default ignores, load_ignore_rules would need adjustment,
  // or we add them sequentially. For now, let's primarily focus on ignoring
  // dctx_filepath. A more robust load_ignore_rules could take an array of
  // default filenames. For simplicity now, we only pass one. The other will be
  // ignored if it matches a generic rule.
  if (!load_ignore_rules(target_dir_abs_path, dctx_filename_basename,
                         &ignore_rules, &ignore_rule_count)) {
    log_error("Failed to load ignore rules. Critical error.");
    return EXIT_FAILURE;
  }
  log_info("Loaded %d ignore rules.", ignore_rule_count);

  // 4. Walk Directory and Build Tree (for writing the binary .dircontxt)
  int processed_items = 0;
  DirContextTreeNode *tree_for_writing = walk_directory_and_build_tree(
      target_dir_abs_path, ignore_rules, ignore_rule_count, &processed_items);

  if (tree_for_writing == NULL) {
    log_error("Failed to walk directory and build tree for: %s",
              target_dir_abs_path);
    free_ignore_rules_array(ignore_rules, ignore_rule_count);
    return EXIT_FAILURE;
  }
  log_info(
      "Directory tree built for binary writer. Root node relative path: '%s'",
      tree_for_writing->relative_path);

#if DEBUG_LOGGING_ENABLED
  log_debug("--- In-memory Tree Structure (for writing) ---");
  print_tree_recursive(tree_for_writing, 0);
  log_debug("---------------------------------------------");
#endif

  // 5. Write .dircontxt Binary File
  log_info("Writing to binary .dircontxt file: %s", dctx_filepath);
  if (!write_dircontxt_file(dctx_filepath, tree_for_writing)) {
    log_error("Failed to write .dircontxt binary file to: %s", dctx_filepath);
    free_tree_recursive(tree_for_writing);
    free_ignore_rules_array(ignore_rules, ignore_rule_count);
    return EXIT_FAILURE;
  }
  log_info(
      "Successfully created binary .dircontxt file: %s (%d items included).",
      dctx_filepath, processed_items);

  // We are done with tree_for_writing, free it.
  // The reader will build its own tree from the file.
  free_tree_recursive(tree_for_writing);
  tree_for_writing = NULL;

  // --- Generation of .llmcontext.txt file ---
  log_info("Attempting to generate LLM context text file...");

  DirContextTreeNode *tree_for_llm = NULL;
  uint64_t data_section_offset = 0;

  // 6. Read the .dircontxt binary file's header to reconstruct the tree for LLM
  // formatting
  if (!dctx_read_and_parse_header(dctx_filepath, &tree_for_llm,
                                  &data_section_offset)) {
    log_error("Failed to read and parse the created .dircontxt binary file "
              "header from: %s",
              dctx_filepath);
    log_error("Cannot generate .llmcontext.txt file.");
    // tree_for_llm should be NULL if dctx_read_and_parse_header fails and
    // cleans up.
    free_ignore_rules_array(ignore_rules, ignore_rule_count);
    return EXIT_FAILURE; // Or a different error code
  }
  log_info(
      ".dircontxt binary file header parsed successfully for LLM formatting.");

#if DEBUG_LOGGING_ENABLED
  log_debug("--- In-memory Tree Structure (read for LLM) ---");
  print_tree_recursive(tree_for_llm, 0);
  log_debug("----------------------------------------------");
#endif

  // 7. Generate the .llmcontext.txt file
  if (!generate_llm_context_file(llm_txt_filepath, tree_for_llm, dctx_filepath,
                                 data_section_offset)) {
    log_error("Failed to generate .llmcontext.txt file at: %s",
              llm_txt_filepath);
    // generate_llm_context_file should have logged specific errors.
  } else {
    log_info("Successfully generated .llmcontext.txt file: %s",
             llm_txt_filepath);
  }

  // 8. Final Cleanup
  if (tree_for_llm != NULL) {
    free_tree_recursive(tree_for_llm);
  }
  free_ignore_rules_array(ignore_rules, ignore_rule_count);

  return EXIT_SUCCESS;
}

// --- Helper Function Implementations ---

static void print_usage(void) {
  printf("Usage: %s <target_directory>\n", APP_NAME);
  printf("Creates a .dircontxt binary file and a .llmcontext.txt file\n");
  printf("for the specified target directory.\n");
  printf("Output files will be named <target_directory_name>.dircontxt and\n");
  printf("<target_directory_name>.llmcontext.txt, placed in the parent "
         "directory\n");
  printf("of <target_directory>.\n\n");
  printf("Options:\n");
  printf("  -h, --help     Show this help message and exit.\n");
  printf("  -v, --version  Show version information and exit.\n");
}

static bool determine_output_filepaths(const char *target_dir_abs_path,
                                       char *dctx_output_filepath_out,
                                       size_t dctx_buffer_size,
                                       char *llm_output_filepath_out,
                                       size_t llm_buffer_size) {
  char dir_to_get_name_from[MAX_PATH_LEN];
  safe_strncpy(dir_to_get_name_from, target_dir_abs_path, MAX_PATH_LEN);

  char *target_basename = get_directory_basename(dir_to_get_name_from);
  if (!target_basename) {
    log_error("Failed to get basename for target directory: %s",
              dir_to_get_name_from);
    return false;
  }

  char dctx_filename[MAX_PATH_LEN];
  snprintf(dctx_filename, MAX_PATH_LEN, "%s.dircontxt", target_basename);

  char llm_filename[MAX_PATH_LEN];
  snprintf(llm_filename, MAX_PATH_LEN, "%s.llmcontext.txt", target_basename);

  free(target_basename); // Free the string returned by get_directory_basename

  char *parent_of_target_dir = platform_get_dirname(target_dir_abs_path);
  if (!parent_of_target_dir) {
    log_error("Failed to get parent directory of: %s", target_dir_abs_path);
    return false;
  }

  bool success = true;
  if (!platform_join_paths(parent_of_target_dir, dctx_filename,
                           dctx_output_filepath_out, dctx_buffer_size)) {
    log_error("Failed to join parent path '%s' and dctx filename '%s'",
              parent_of_target_dir, dctx_filename);
    success = false;
  }
  if (success &&
      !platform_join_paths(parent_of_target_dir, llm_filename,
                           llm_output_filepath_out, llm_buffer_size)) {
    log_error("Failed to join parent path '%s' and llm filename '%s'",
              parent_of_target_dir, llm_filename);
    success = false;
  }

  free(parent_of_target_dir);
  return success;
}
