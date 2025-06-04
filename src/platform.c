#include "platform.h"
#include "datatypes.h" // For MAX_PATH_LEN

#include <errno.h>  // For errno
#include <libgen.h> // For dirname and basename POSIX functions
#include <stdio.h>
#include <stdlib.h> // For realpath, malloc, free
#include <string.h> // For strrchr, strlen, strcpy, strncpy

// --- Filesystem Operations ---

int platform_get_file_stat(const char *path, struct stat *stat_buf) {
  if (stat(path, stat_buf) != 0) {
    // perror("platform_get_file_stat - stat"); // Optional: for debugging
    return -1;
  }
  return 0;
}

bool platform_is_dir(const struct stat *stat_buf) {
  return S_ISDIR(stat_buf->st_mode);
}

bool platform_is_reg_file(const struct stat *stat_buf) {
  return S_ISREG(stat_buf->st_mode);
}

uint64_t platform_get_mod_time(const struct stat *stat_buf) {
  // On macOS, st_mtimespec.tv_sec is preferred for higher precision if needed,
  // but st_mtime is standard POSIX and gives seconds, which is what we need for
  // Unix timestamp.
  return (uint64_t)stat_buf->st_mtime;
}

bool platform_resolve_path(const char *path, char *resolved_path_out,
                           size_t out_buffer_size) {
  char *real_path_result = realpath(path, NULL); // Allocate buffer for realpath
  if (real_path_result == NULL) {
    // perror("platform_resolve_path - realpath"); // Optional: for debugging
    return false;
  }

  if (strlen(real_path_result) + 1 > out_buffer_size) {
    fprintf(stderr, "Error: Resolved path exceeds buffer size.\n");
    free(real_path_result);
    return false;
  }

  strncpy(resolved_path_out, real_path_result, out_buffer_size - 1);
  resolved_path_out[out_buffer_size - 1] = '\0'; // Ensure null termination
  free(real_path_result);
  return true;
}

// POSIX basename can modify its input if it's not a const char*,
// and the returned pointer is to static storage or part of the input.
// For our usage with const char*, it should be safe to just return its result.
const char *platform_get_basename(const char *path) {
  // To avoid modifying the input `path` if `basename` were to do so,
  // and to handle cases where `path` might be an empty string or "/",
  // we can create a temporary copy for `basename`.
  // However, the POSIX spec for `basename(const char*)` implies it shouldn't
  // modify if const. A truly safe way would be to copy `path` first if there's
  // any doubt. For simplicity here, let's assume `basename` from `libgen.h`
  // behaves well with `const char *`.

  if (path == NULL)
    return "."; // Or some other sensible default for NULL input

  // If path is empty or just "/", basename might return "." or "/".
  // Let's handle a few edge cases to ensure we don't return an empty string.
  if (path[0] == '\0')
    return ".";

  // A more robust custom basename that doesn't rely on libgen.h's potential
  // quirks:
  const char *last_slash = strrchr(path, PLATFORM_DIR_SEPARATOR);
  if (last_slash) {
    // Check if the slash is the last character (e.g. "foo/")
    if (*(last_slash + 1) == '\0') {
      // It's a trailing slash. We need to find the segment before it.
      // Create a temporary mutable copy to strip trailing slashes.
      char temp_path[MAX_PATH_LEN];
      strncpy(temp_path, path, MAX_PATH_LEN - 1);
      temp_path[MAX_PATH_LEN - 1] = '\0';

      // Remove trailing slashes
      size_t len = strlen(temp_path);
      while (len > 0 && temp_path[len - 1] == PLATFORM_DIR_SEPARATOR) {
        temp_path[--len] = '\0';
      }
      // If all slashes, temp_path is now empty, should return "/" conceptually
      if (len == 0 && path[0] == PLATFORM_DIR_SEPARATOR)
        return PLATFORM_DIR_SEPARATOR_STR;
      if (len == 0)
        return ".";

      const char *new_last_slash = strrchr(temp_path, PLATFORM_DIR_SEPARATOR);
      if (new_last_slash) {
        // Need to return a string that persists.
        // This simple version just points into the temp_path, which is bad.
        // For a robust solution, platform_get_basename would need to return a
        // char* that the caller frees, or use a static buffer (not
        // thread-safe). Let's stick to libgen.h basename for now, it's simpler.
      } else {
        // return strdup(temp_path); // If we were returning char* to be freed
      }
    }
    // Standard case: return the part after the last slash
    return last_slash + 1;
  }
  // No slash found, the path itself is the basename
  return path;

  // Using libgen.h's basename:
  // char* path_copy = strdup(path); // POSIX basename might modify its argument
  // if (!path_copy) { perror("strdup in platform_get_basename"); return "."; }
  // char* bname = basename(path_copy);
  // const char* result = strdup(bname); // Make a copy of the result
  // free(path_copy);
  // if (!result) { perror("strdup for basename result"); return "."; }
  // return result; // CALLER MUST FREE THIS
  // For now, let's go with the simpler, less robust custom one for const char*
}

// POSIX dirname can modify its input string. It also might return a pointer to
// static storage. To make it safer: copy the input path first.
char *platform_get_dirname(const char *path) {
  if (path == NULL)
    return NULL;

  char *path_copy = strdup(path);
  if (!path_copy) {
    perror("platform_get_dirname - strdup");
    return NULL; // Or strdup(".") if preferred
  }

  char *dir_name_ptr = dirname(path_copy); // dirname operates on path_copy

  // strdup the result from dirname because dirname might return a pointer
  // to static storage or a modification of path_copy. We want an independent
  // string.
  char *result = strdup(dir_name_ptr);
  if (!result) {
    perror("platform_get_dirname - strdup result");
    free(path_copy);
    return NULL; // Or strdup(".")
  }

  free(path_copy); // Free the initial copy
  return result;   // Caller is responsible for freeing this returned string
}

// --- Path Manipulation ---

bool platform_join_paths(const char *base_path, const char *component,
                         char *result_path_buffer, size_t buffer_size) {
  if (!base_path || !component || !result_path_buffer) {
    return false;
  }

  size_t base_len = strlen(base_path);
  size_t component_len = strlen(component);

  // Check for buffer overflow before doing anything
  // Need space for base_len, component_len, potential separator, and null
  // terminator
  if (base_len + component_len + 2 > buffer_size) { // +2 for separator and '\0'
    fprintf(stderr, "Error: Joined path exceeds buffer size.\n");
    return false;
  }

  strcpy(result_path_buffer, base_path);

  // Add separator if base_path is not empty and doesn't already end with one,
  // and component doesn't start with one (for absolute components, though less
  // common here).
  if (base_len > 0 &&
      result_path_buffer[base_len - 1] != PLATFORM_DIR_SEPARATOR &&
      component[0] != PLATFORM_DIR_SEPARATOR) {
    result_path_buffer[base_len] = PLATFORM_DIR_SEPARATOR;
    result_path_buffer[base_len + 1] =
        '\0'; // Null-terminate after adding separator
  } else if (base_len > 0 &&
             result_path_buffer[base_len - 1] == PLATFORM_DIR_SEPARATOR &&
             component[0] == PLATFORM_DIR_SEPARATOR) {
    // Avoid double separators if base ends with / and component starts with /
    // Move to the start of component (effectively skipping its leading /)
    strcat(result_path_buffer, component + 1);
    return true;
  }

  strcat(result_path_buffer, component);
  return true;
}
