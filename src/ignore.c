#include "ignore.h"
#include "platform.h" // For platform_join_paths, PLATFORM_DIR_SEPARATOR_STR
#include "utils.h"    // For log_debug, trim_trailing_newline, safe_strncpy

#include <ctype.h> // For isspace
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to add a rule to the dynamic array
static bool add_rule_to_list(IgnoreRule rule, IgnoreRule **rules_array_out,
                             int *rule_count_out, int *capacity_out) {
  if (*rule_count_out >= *capacity_out) {
    int new_capacity = (*capacity_out == 0) ? 8 : *capacity_out * 2;
    IgnoreRule *new_array = (IgnoreRule *)realloc(
        *rules_array_out, new_capacity * sizeof(IgnoreRule));
    if (new_array == NULL) {
      perror("add_rule_to_list: realloc failed");
      return false;
    }
    *rules_array_out = new_array;
    *capacity_out = new_capacity;
  }
  (*rules_array_out)[*rule_count_out] = rule; // Struct copy
  (*rule_count_out)++;
  return true;
}

// Parses a single line from the ignore file into an IgnoreRule struct.
// Modifies the pattern in place (e.g., removes trailing slash for dir_only).
bool parse_ignore_pattern_line(const char *orig_line, IgnoreRule *rule_out) {
  if (orig_line == NULL || rule_out == NULL)
    return false;

  char line_buffer[MAX_PATH_LEN];
  safe_strncpy(line_buffer, orig_line, MAX_PATH_LEN);
  trim_trailing_newline(line_buffer); // Remove \n or \r\n

  // Trim leading whitespace
  const char *line = line_buffer;
  while (isspace((unsigned char)*line))
    line++;

  // Trim trailing whitespace (after newline removal)
  char *end = (char *)line + strlen(line) - 1;
  while (end >= line && isspace((unsigned char)*end))
    end--;
  *(end + 1) = '\0';

  if (line[0] == '\0' || line[0] == '#') { // Skip empty lines or comments
    return false;                          // Not a rule to add
  }

  memset(rule_out, 0, sizeof(IgnoreRule)); // Initialize rule
  safe_strncpy(rule_out->pattern, line, MAX_PATH_LEN);

  size_t len = strlen(rule_out->pattern);

  // Rule 1: If a pattern ends with a slash, it is removed for the purpose of
  // matching, but it would only find a match with a directory.
  if (len > 0 && rule_out->pattern[len - 1] == PLATFORM_DIR_SEPARATOR) {
    rule_out->is_dir_only = true;
    rule_out->pattern[len - 1] = '\0'; // Remove trailing slash for matching
    len--;                             // Update length
  }

  // Simplified wildcard handling:
  // 1. "foo/*" (matches anything under foo/, foo itself is not matched by this
  // rule directly unless also is_dir_only)
  // 2. "*.txt" (matches any file or dir ending with .txt)
  // More complex globbing (e.g., **, ?, character classes) is not handled here.

  if (len > 1 && rule_out->pattern[len - 1] == '*' &&
      rule_out->pattern[len - 2] == PLATFORM_DIR_SEPARATOR) {
    // Pattern like "dir/*"
    rule_out->is_wildcard_prefix_match = true;
    rule_out->pattern[len - 2] =
        PLATFORM_DIR_SEPARATOR;        // Keep the separator: "dir/"
    rule_out->pattern[len - 1] = '\0'; // Remove the "*"
    // This rule effectively means "starts with rule_out->pattern"
  } else if (len > 1 && rule_out->pattern[0] == '*' &&
             rule_out->pattern[1] == '.') {
    // Pattern like "*.log"
    rule_out->is_wildcard_suffix_match = true;
    // The pattern remains as "*.log", but the flag indicates how to match.
    // We'll effectively match against "ends with .log" (the part after '*')
  } else {
    // Exact name match or simple prefix (if not dir_only and not
    // wildcard_prefix_match)
    rule_out->is_exact_name_match = true;
  }

  // Gitignore has more complex rules about matching paths from root vs.
  // anywhere. For simplicity, if a pattern contains no slashes, it matches a
  // name anywhere. If it contains slashes, it's matched relative to the
  // dircontxt root. Our current `is_wildcard_prefix_match` (e.g. `build/`)
  // implies matching from root. `is_exact_name_match` (e.g. `foo.c` or
  // `src/bar.c`) is also clear.

  log_debug("Parsed ignore rule: pattern='%s', dir_only=%d, wc_prefix=%d, "
            "wc_suffix=%d, exact=%d",
            rule_out->pattern, rule_out->is_dir_only,
            rule_out->is_wildcard_prefix_match,
            rule_out->is_wildcard_suffix_match, rule_out->is_exact_name_match);
  return true;
}

bool load_ignore_rules(const char *base_dir_path,
                       const char *output_filename_to_ignore,
                       IgnoreRule **rules_array_out, int *rule_count_out) {
  *rules_array_out = NULL;
  *rule_count_out = 0;
  int capacity = 0;

  // 1. Add default internal ignores
  IgnoreRule default_rule;

  // Ignore the .dircontxtignore file itself
  memset(&default_rule, 0, sizeof(IgnoreRule));
  safe_strncpy(default_rule.pattern, DEFAULT_IGNORE_FILENAME, MAX_PATH_LEN);
  default_rule.is_exact_name_match = true;
  if (!add_rule_to_list(default_rule, rules_array_out, rule_count_out,
                        &capacity))
    return false;
  log_debug("Added default ignore: %s", DEFAULT_IGNORE_FILENAME);

  // Ignore the output .dircontxt file
  if (output_filename_to_ignore && output_filename_to_ignore[0] != '\0') {
    memset(&default_rule, 0, sizeof(IgnoreRule));
    safe_strncpy(default_rule.pattern, output_filename_to_ignore, MAX_PATH_LEN);
    default_rule.is_exact_name_match =
        true; // Output file is usually in parent or current dir
    if (!add_rule_to_list(default_rule, rules_array_out, rule_count_out,
                          &capacity))
      return false;
    log_debug("Added default ignore for output file: %s",
              output_filename_to_ignore);
  }
  // Could add other common ones like ".git/" here if desired
  // memset(&default_rule, 0, sizeof(IgnoreRule));
  // safe_strncpy(default_rule.pattern, ".git", MAX_PATH_LEN);
  // default_rule.is_dir_only = true;
  // default_rule.is_exact_name_match = true; // Match name ".git" as a
  // directory if (!add_rule_to_list(default_rule, rules_array_out,
  // rule_count_out, &capacity)) return false;

  // 2. Load rules from .dircontxtignore file
  char ignore_filepath[MAX_PATH_LEN];
  if (!platform_join_paths(base_dir_path, DEFAULT_IGNORE_FILENAME,
                           ignore_filepath, MAX_PATH_LEN)) {
    log_error("Failed to construct path for .dircontxtignore in %s",
              base_dir_path);
    // Not a fatal error, just means no custom ignores will be loaded.
  } else {
    FILE *fp = fopen(ignore_filepath, "r");
    if (fp != NULL) {
      log_info("Loading ignore rules from: %s", ignore_filepath);
      char *line;
      while ((line = read_line_from_file(fp)) != NULL) {
        IgnoreRule rule;
        if (parse_ignore_pattern_line(line, &rule)) {
          if (!add_rule_to_list(rule, rules_array_out, rule_count_out,
                                &capacity)) {
            free(line);
            fclose(fp);
            return false; // Allocation error
          }
        }
        free(line);
      }
      fclose(fp);
    } else {
      log_info("No %s file found in %s, or cannot be read. Using default "
               "ignores only.",
               DEFAULT_IGNORE_FILENAME, base_dir_path);
    }
  }
  return true;
}

bool should_ignore_item(
    const char *item_relative_path, // e.g., "src/file.c" or "dist/"
    const char *item_name,          // e.g., "file.c" or "dist"
    bool is_item_dir, const IgnoreRule *rules, int rule_count) {
  if (rules == NULL || rule_count == 0) {
    return false;
  }

  for (int i = 0; i < rule_count; ++i) {
    const IgnoreRule *rule = &rules[i];

    // Rule: Directory-only patterns only match directories
    if (rule->is_dir_only && !is_item_dir) {
      continue; // This rule is for directories, but item is a file
    }

    // --- Matching Logic ---

    if (rule->is_wildcard_suffix_match) { // e.g., "*.log"
      // Pattern stored as "*.log", actual pattern part is ".log"
      const char *suffix_to_match =
          strchr(rule->pattern, '.'); // Assumes *. Suffix starts with '.'
      if (suffix_to_match == NULL)
        suffix_to_match = rule->pattern + 1; // fallback if no '.' e.g. '*test'

      size_t item_name_len = strlen(item_name);
      size_t suffix_len = strlen(suffix_to_match);
      if (item_name_len >= suffix_len &&
          strcmp(item_name + (item_name_len - suffix_len), suffix_to_match) ==
              0) {
        log_debug("Item '%s' ignored by suffix rule '%s'", item_relative_path,
                  rule->pattern);
        return true;
      }
    } else if (rule->is_wildcard_prefix_match) { // e.g., "build/*" (stored as
                                                 // "build/")
      // Matches if item_relative_path starts with rule->pattern
      // Example: rule->pattern = "build/", item_relative_path =
      // "build/output/file.txt"
      if (strncmp(item_relative_path, rule->pattern, strlen(rule->pattern)) ==
          0) {
        // Additional check: if rule is "foo/" and item is "foobar/", it
        // shouldn't match. The character after the prefix in item_relative_path
        // must be a separator or end of string.
        size_t rule_pattern_len = strlen(rule->pattern);
        if (item_relative_path[rule_pattern_len - 1] ==
            PLATFORM_DIR_SEPARATOR) { // rule is "foo/"
          // item_relative_path starts with "foo/", so it's a match
          log_debug("Item '%s' ignored by prefix rule '%s*'",
                    item_relative_path, rule->pattern);
          return true;
        }
      }
    } else if (rule->is_exact_name_match) {
      // Simplistic: if pattern has no separator, match against item_name.
      // If pattern has separator, match against item_relative_path.
      // This is a basic interpretation of gitignore's "match from root" or
      // "match anywhere".
      if (strchr(rule->pattern, PLATFORM_DIR_SEPARATOR) == NULL) {
        // Pattern has no slashes, e.g., "node_modules" or ".DS_Store"
        if (strcmp(item_name, rule->pattern) == 0) {
          log_debug("Item name '%s' (in '%s') ignored by exact name rule '%s'",
                    item_name, item_relative_path, rule->pattern);
          return true;
        }
      } else {
        // Pattern has slashes, e.g., "src/config.h"
        if (strcmp(item_relative_path, rule->pattern) == 0) {
          log_debug("Item path '%s' ignored by exact path rule '%s'",
                    item_relative_path, rule->pattern);
          return true;
        }
        // Handle case where rule is "foo/bar" and item is "foo/bar/" (a
        // directory)
        if (is_item_dir) {
          char item_path_with_slash[MAX_PATH_LEN];
          snprintf(item_path_with_slash, MAX_PATH_LEN, "%s%c",
                   item_relative_path, PLATFORM_DIR_SEPARATOR);
          if (strcmp(item_path_with_slash, rule->pattern) == 0) {
            log_debug("Item dir path '%s' ignored by exact path rule '%s'",
                      item_path_with_slash, rule->pattern);
            return true;
          }
        }
      }
    }
    // If rule->is_dir_only was true, and we are here, it means is_item_dir was
    // also true. The pattern (with trailing slash removed) should match
    // item_name. This is somewhat covered by exact_name_match if item_name is
    // compared. Let's refine: if is_dir_only, the rule->pattern (e.g. "dist")
    // must match item_name ("dist").
    if (rule->is_dir_only && strcmp(item_name, rule->pattern) == 0) {
      log_debug("Item dir '%s' ignored by dir_only rule '%s/'", item_name,
                rule->pattern);
      return true;
    }

  } // End of for loop iterating through rules

  return false; // No rule matched
}

void free_ignore_rules_array(IgnoreRule *rules_array, int rule_count) {
  // Patterns within IgnoreRule structs are char arrays, not separately
  // allocated strings, so just freeing the array itself is enough.
  if (rules_array != NULL) {
    free(rules_array);
  }
  // rule_count is just an int, no freeing needed.
}
