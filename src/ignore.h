#ifndef IGNORE_H
#define IGNORE_H

#include "datatypes.h" // For IgnoreRule, MAX_PATH_LEN
#include <stdbool.h>

#define DEFAULT_IGNORE_FILENAME ".dircontxtignore"

// --- Core Ignore List Functions ---

// Loads ignore rules from the .dircontxtignore file in the specified base
// directory. Also adds default ignore patterns (like the output file itself).
//
// Parameters:
//   base_dir_path: Absolute path to the directory where .dircontxtignore should
//   be found. rules_array_out: Pointer to an array of IgnoreRule structs. Will
//   be allocated/reallocated. rule_count_out: Pointer to an integer to store
//   the number of rules loaded. output_filename_to_ignore: The name of the
//   .dircontxt file being generated, to ignore it.
//
// Returns:
//   True if successful (even if no ignore file found or no rules loaded), false
//   on critical error.
bool load_ignore_rules(const char *base_dir_path,
                       const char *output_filename_to_ignore,
                       IgnoreRule **rules_array_out, int *rule_count_out);

// Checks if a given item (file or directory) should be ignored based on the
// loaded rules.
//
// Parameters:
//   item_relative_path: Path of the item relative to the dircontxt root (e.g.,
//   "src/file.c" or "dist/"). item_name: Just the name of the item (e.g.,
//   "file.c" or "dist"). is_item_dir: True if the item is a directory, false if
//   it's a file. rules: Array of loaded IgnoreRule structs. rule_count: Number
//   of rules in the array.
//
// Returns:
//   True if the item should be ignored, false otherwise.
bool should_ignore_item(const char *item_relative_path, const char *item_name,
                        bool is_item_dir, const IgnoreRule *rules,
                        int rule_count);

// Frees the memory allocated for the ignore rules array.
void free_ignore_rules_array(IgnoreRule *rules_array, int rule_count);

// --- Helper for Parsing ---
// (Could be static in ignore.c if not needed elsewhere, but good for testing to
// have it here) Parses a single line from the ignore file into an IgnoreRule
// struct. Modifies the pattern in place (e.g., removes trailing slash for
// dir_only).
bool parse_ignore_pattern_line(const char *line, IgnoreRule *rule_out);

#endif // IGNORE_H
