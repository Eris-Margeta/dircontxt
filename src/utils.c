#include "utils.h"
#include "platform.h" // For PLATFORM_DIR_SEPARATOR

#include <errno.h>  // For errno, perror
#include <stdarg.h> // For va_list, va_start, va_end
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- String Utilities ---

char *safe_strncpy(char *dest, const char *src, size_t n) {
  if (n == 0)
    return dest;
  strncpy(dest, src, n - 1);
  dest[n - 1] = '\0'; // Ensure null termination
  return dest;
}

void trim_trailing_newline(char *str) {
  if (str == NULL)
    return;
  size_t len = strlen(str);
  while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
    str[--len] = '\0';
  }
}

// --- File I/O Utilities ---

char *read_line_from_file(FILE *fp) {
  if (fp == NULL)
    return NULL;

  size_t buffer_size = 128; // Initial buffer size
  char *buffer = (char *)malloc(buffer_size);
  if (buffer == NULL) {
    perror("read_line_from_file: malloc failed for buffer");
    return NULL;
  }

  size_t current_pos = 0;
  int c;

  while ((c = fgetc(fp)) != EOF && c != '\n') {
    if (current_pos >= buffer_size - 1) { // -1 for null terminator
      buffer_size *= 2;                   // Double the buffer size
      char *new_buffer = (char *)realloc(buffer, buffer_size);
      if (new_buffer == NULL) {
        perror("read_line_from_file: realloc failed for buffer");
        free(buffer);
        return NULL;
      }
      buffer = new_buffer;
    }
    buffer[current_pos++] = (char)c;
  }

  buffer[current_pos] = '\0'; // Null-terminate the string

  if (c == EOF && current_pos == 0) { // EOF and no characters read
    free(buffer);
    return NULL;
  }

  return buffer;
}

// --- Error Handling ---
// Simple logging for now, can be expanded (e.g., to log to a file, different
// levels)
#ifndef NDEBUG // Only enable log_debug if not NDEBUG (NDEBUG is often defined
               // for release builds)
#define DEBUG_LOGGING_ENABLED 1
#else
#define DEBUG_LOGGING_ENABLED 0
#endif

void log_error(const char *message_format, ...) {
  fprintf(stderr, "[ERROR] ");
  va_list args;
  va_start(args, message_format);
  vfprintf(stderr, message_format, args);
  va_end(args);
  fprintf(stderr, "\n");
}

void log_info(const char *message_format, ...) {
  printf("[INFO] ");
  va_list args;
  va_start(args, message_format);
  vprintf(message_format, args);
  va_end(args);
  printf("\n");
}

void log_debug(const char *message_format, ...) {
  if (DEBUG_LOGGING_ENABLED) {
    printf("[DEBUG] ");
    va_list args;
    va_start(args, message_format);
    vprintf(message_format, args);
    va_end(args);
    printf("\n");
  }
}

// --- Tree Utilities ---

void free_tree_recursive(DirContextTreeNode *node) {
  if (node == NULL) {
    return;
  }
  if (node->type == NODE_TYPE_DIRECTORY) {
    for (uint32_t i = 0; i < node->num_children; ++i) {
      free_tree_recursive(node->children[i]);
    }
    free(node->children); // Free the array of child pointers
  }
  free(node); // Free the node itself
}

DirContextTreeNode *create_node(NodeType type,
                                const char *relative_path_in_archive,
                                const char *disk_path_for_stat) {
  DirContextTreeNode *node =
      (DirContextTreeNode *)malloc(sizeof(DirContextTreeNode));
  if (node == NULL) {
    perror("create_node: malloc failed");
    return NULL;
  }

  node->type = type;
  safe_strncpy(node->relative_path, relative_path_in_archive, MAX_PATH_LEN);
  safe_strncpy(node->disk_path, disk_path_for_stat,
               MAX_PATH_LEN); // Store disk path for statting

  struct stat stat_buf;
  if (platform_get_file_stat(disk_path_for_stat, &stat_buf) == 0) {
    node->last_modified_timestamp = platform_get_mod_time(&stat_buf);
  } else {
    log_error("Failed to stat %s, setting timestamp to 0.", disk_path_for_stat);
    node->last_modified_timestamp = 0; // Or handle error more gracefully
  }

  node->content_offset_in_data_section = 0; // Will be set later for files
  node->content_size = 0;                   // Will be set later for files

  node->children = NULL;
  node->num_children = 0;
  node->children_capacity = 0;

  if (type == NODE_TYPE_DIRECTORY) {
    // Initialize children array with a small capacity for directories
    node->children_capacity = 4; // Initial capacity
    node->children = (DirContextTreeNode **)malloc(
        node->children_capacity * sizeof(DirContextTreeNode *));
    if (node->children == NULL) {
      perror("create_node: malloc failed for children array");
      free(node);
      return NULL;
    }
  }
  return node;
}

bool add_child_to_parent_node(DirContextTreeNode *parent,
                              DirContextTreeNode *child) {
  if (parent == NULL || parent->type != NODE_TYPE_DIRECTORY || child == NULL) {
    return false;
  }

  if (parent->num_children >= parent->children_capacity) {
    uint32_t new_capacity =
        (parent->children_capacity == 0) ? 4 : parent->children_capacity * 2;
    DirContextTreeNode **new_children = (DirContextTreeNode **)realloc(
        parent->children, new_capacity * sizeof(DirContextTreeNode *));
    if (new_children == NULL) {
      perror("add_child_to_parent_node: realloc failed");
      return false; // Child is not added, but also not lost if allocated
                    // separately
    }
    parent->children = new_children;
    parent->children_capacity = new_capacity;
  }

  parent->children[parent->num_children++] = child;
  return true;
}

// Get the base name of a directory (e.g., "myfolder" from "/path/to/myfolder/"
// or "/path/to/myfolder") The caller is responsible for freeing the returned
// string.
char *get_directory_basename(const char *path) {
  if (path == NULL || path[0] == '\0') {
    return strdup("."); // Return "." for empty or NULL path
  }

  char temp_path[MAX_PATH_LEN];
  safe_strncpy(temp_path, path, MAX_PATH_LEN);

  // Remove trailing slashes
  size_t len = strlen(temp_path);
  while (len > 0 && temp_path[len - 1] == PLATFORM_DIR_SEPARATOR) {
    temp_path[--len] = '\0';
  }

  // If path was all slashes (e.g., "///"), temp_path is now empty. Return "/"
  if (len == 0 && path[0] == PLATFORM_DIR_SEPARATOR) {
    return strdup(PLATFORM_DIR_SEPARATOR_STR);
  }
  // If path was empty or became empty (e.g. from ".")
  if (len == 0) {
    return strdup(".");
  }

  const char *basename_ptr = platform_get_basename(temp_path);
  if (basename_ptr ==
      NULL) { // Should not happen with our platform_get_basename
    return strdup(".");
  }
  return strdup(basename_ptr); // Make a copy as platform_get_basename might
                               // point into temp_path
}

void print_tree_recursive(const DirContextTreeNode *node, int indent_level) {
  if (node == NULL)
    return;

  for (int i = 0; i < indent_level; ++i) {
    printf("  ");
  }

  if (node->type == NODE_TYPE_DIRECTORY) {
    printf("[%s/] (mod: %llu, children: %u)\n", node->relative_path,
           (unsigned long long)node->last_modified_timestamp,
           node->num_children);
    for (uint32_t i = 0; i < node->num_children; ++i) {
      print_tree_recursive(node->children[i], indent_level + 1);
    }
  } else { // NODE_TYPE_FILE
    printf("%s (mod: %llu, offset: %llu, size: %llu)\n", node->relative_path,
           (unsigned long long)node->last_modified_timestamp,
           (unsigned long long)node->content_offset_in_data_section,
           (unsigned long long)node->content_size);
  }
}
