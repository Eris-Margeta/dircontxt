#include "ignore.h"
#include "platform.h" // For platform_join_paths, PLATFORM_DIR_SEPARATOR_STR, PLATFORM_DIR_SEPARATOR
#include "utils.h" // For log_debug, log_info, log_error, read_line_from_file, trim_trailing_newline, safe_strncpy

#include <ctype.h> // For isspace
#include <errno.h> // For errno in case fopen fails, etc.
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

  rule_out->is_dir_only = false;
  rule_out->is_wildcard_prefix_match = false;
  rule_out->is_wildcard_suffix_match = false;
  rule_out->is_exact_name_match =
      false; // Will be set to true if no other type matches

  // Rule 1: If a pattern ends with a slash, it is removed for the purpose of
  // matching, but it would only find a match with a directory.
  if (len > 0 && rule_out->pattern[len - 1] == PLATFORM_DIR_SEPARATOR) {
    rule_out->is_dir_only = true;
    rule_out->pattern[len - 1] = '\0'; // Remove trailing slash for matching
    len--;                             // Update length
  }

  // Simplified wildcard handling:
  if (len > 1 && rule_out->pattern[len - 1] == '*' &&
      rule_out->pattern[len - 2] == PLATFORM_DIR_SEPARATOR) {
    // Pattern like "dir/*" -> means "dir/" and everything under it
    rule_out->is_wildcard_prefix_match = true;
    // Pattern becomes "dir/", the '*' is implicit for "everything under"
    rule_out->pattern[len - 1] = '\0'; // Remove the '*' but keep the slash
    // is_dir_only might also be true if original was "dir/*/"
  } else if (len > 1 && rule_out->pattern[0] == '*' &&
             rule_out->pattern[1] == '.') {
    // Pattern like "*.log"
    rule_out->is_wildcard_suffix_match = true;
    // The pattern remains as "*.log", the flag indicates suffix matching on the
    // part after '*'
  } else {
    // If not a recognized wildcard type, it's an exact name match.
    // This also catches patterns like "foo" that might be directories (if
    // is_dir_only is true) or files.
    rule_out->is_exact_name_match = true;
  }

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
    default_rule.is_exact_name_match = true;
    if (!add_rule_to_list(default_rule, rules_array_out, rule_count_out,
                          &capacity))
      return false;
    log_debug("Added default ignore for output file: %s",
              output_filename_to_ignore);
  }

  char ignore_filepath[MAX_PATH_LEN];
  if (!platform_join_paths(base_dir_path, DEFAULT_IGNORE_FILENAME,
                           ignore_filepath, MAX_PATH_LEN)) {
    log_error("Failed to construct path for .dircontxtignore in %s",
              base_dir_path);
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
            return false;
          }
        }
        free(line);
      }
      fclose(fp);
    } else {
      if (errno != 0 &&
          errno != ENOENT) { // ENOENT (No such file or directory) is fine
        log_info("Could not read %s in %s: %s. Using default ignores only.",
                 DEFAULT_IGNORE_FILENAME, base_dir_path, strerror(errno));
      } else {
        log_info("No %s file found in %s. Using default ignores only.",
                 DEFAULT_IGNORE_FILENAME, base_dir_path);
      }
    }
  }
  return true;
}

bool should_ignore_item(const char *item_relative_path, const char *item_name,
                        bool is_item_dir, const IgnoreRule *rules,
                        int rule_count) {
  if (rules == NULL || rule_count == 0) {
    return false;
  }

  // Gitignore logic: a pattern without a slash will match a name anywhere.
  // A pattern with a slash is matched from the root.
  // A pattern ending with a slash specifically matches a directory.

  for (int i = 0; i < rule_count; ++i) {
    const IgnoreRule *rule = &rules[i];
    bool matched = false;

    // If rule is dir_only, item must be a directory
    if (rule->is_dir_only && !is_item_dir) {
      continue;
    }

    if (rule->is_wildcard_suffix_match) {             // e.g., "*.log"
      const char *suffix_pattern = rule->pattern + 1; // Skip the '*'
      size_t item_name_len = strlen(item_name);
      size_t suffix_pattern_len = strlen(suffix_pattern);
      if (item_name_len >= suffix_pattern_len &&
          strcmp(item_name + (item_name_len - suffix_pattern_len),
                 suffix_pattern) == 0) {
        matched = true;
      }
    } else if (rule->is_wildcard_prefix_match) { // e.g., "build/*" (stored as
                                                 // "build/")
      // Rule pattern is "build/"
      // Item relative path should start with "build/"
      if (strncmp(item_relative_path, rule->pattern, strlen(rule->pattern)) ==
          0) {
        matched = true;
      }
    } else if (rule->is_exact_name_match) {
      // If pattern contains no slash, it matches against item_name.
      // If pattern contains a slash, it's matched relative to the root
      // (item_relative_path).
      if (strchr(rule->pattern, PLATFORM_DIR_SEPARATOR) == NULL) {
        // No slash in pattern: match item_name
        if (strcmp(item_name, rule->pattern) == 0) {
          matched = true;
        }
      } else {
        // Slash in pattern: match item_relative_path
        // For directories, item_relative_path doesn't have trailing slash
        // unless it's the root. Rule pattern (if from "foo/") has trailing
        // slash removed. So, compare "foo" with "foo" or "foo/bar" with
        // "foo/bar"
        if (strcmp(item_relative_path, rule->pattern) == 0) {
          matched = true;
        }
      }
    }

    // If the rule was dir_only (e.g. from "foo/"), and an exact match was
    // intended for the directory name itself.
    if (!matched && rule->is_dir_only && rule->is_exact_name_match) {
      if (strcmp(item_name, rule->pattern) == 0) {
        matched = true;
      }
    }

    if (matched) {
      log_debug(
          "Item '%s' (name: '%s', is_dir: %d) ignored by rule: pattern='%s', "
          "dir_only=%d, wc_prefix=%d, wc_suffix=%d, exact=%d",
          item_relative_path, item_name, is_item_dir, rule->pattern,
          rule->is_dir_only, rule->is_wildcard_prefix_match,
          rule->is_wildcard_suffix_match, rule->is_exact_name_match);
      return true;
    }
  }
  return false;
}

void free_ignore_rules_array(IgnoreRule *rules_array, int rule_count) {
  (void)rule_count;
  if (rules_array != NULL) {
    free(rules_array);
  }
}
