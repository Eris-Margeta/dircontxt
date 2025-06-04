#include "llm_formatter.h"
#include "datatypes.h"   // For DirContextTreeNode, NodeType
#include "dctx_reader.h" // For dctx_read_file_content
#include "utils.h"       // For log_info, log_error, log_debug

#include <ctype.h> // <<<<<<<<<<<<<<<<<<<<<<< ADDED THIS LINE (for isprint)
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // For timestamp conversion (optional, for human-readable dates if needed)

// --- Static Helper Function Declarations ---

// Recursively writes the directory tree manifest to the output file.
static void write_manifest_entry_recursive(FILE *llm_fp,
                                           const DirContextTreeNode *node,
                                           int indent_level, int *id_counter);

// Writes a single file's content block to the output file.
static bool
write_file_content_block(FILE *llm_fp, const DirContextTreeNode *file_node,
                         const char *file_node_id_str, // e.g., "F001"
                         FILE *dctx_binary_fp,
                         uint64_t data_section_start_offset_in_dctx_file);

// Basic heuristic to check if content is likely binary
static bool is_likely_binary(const char *buffer, size_t size);

// Helper to recursively call write_file_content_block
static bool write_all_file_content_blocks_recursive(
    FILE *llm_fp, const DirContextTreeNode *node, int *file_id_counter,
    FILE *dctx_binary_fp, uint64_t data_section_start_offset_in_dctx_file);

// --- Implementation of Static Helper Functions ---

static void write_manifest_entry_recursive(FILE *llm_fp,
                                           const DirContextTreeNode *node,
                                           int indent_level, int *id_counter) {
  if (node == NULL)
    return;

  char node_id_str[20]; // Buffer for IDs like "ROOT", "F001", "D002"

  for (int i = 0; i < indent_level; ++i) {
    fprintf(llm_fp, "  ");
  }

  if (node->type == NODE_TYPE_DIRECTORY) {
    if (indent_level == 0) { // Root node
      strcpy(node_id_str, "ROOT");
    } else {
      snprintf(node_id_str, sizeof(node_id_str), "D%03d", (*id_counter)++);
    }
    fprintf(llm_fp, "[D] %s (ID:%s, MOD:%lld)\n", node->relative_path,
            node_id_str, (long long)node->last_modified_timestamp);

    for (uint32_t i = 0; i < node->num_children; ++i) {
      write_manifest_entry_recursive(llm_fp, node->children[i],
                                     indent_level + 1, id_counter);
    }
  } else { // NODE_TYPE_FILE
    snprintf(node_id_str, sizeof(node_id_str), "F%03d", (*id_counter)++);
    fprintf(llm_fp, "[F] %s (ID:%s, MOD:%lld, SIZE:%lld", node->relative_path,
            node_id_str, (long long)node->last_modified_timestamp,
            (long long)node->content_size);

    const char *ext = strrchr(node->relative_path, '.');
    bool likely_binary_by_ext = false;
    if (ext) {
      const char *binary_exts[] = {
          ".png",    ".jpg", ".jpeg", ".gif",   ".bmp",  ".tiff",
          ".ico",                                        // Images
          ".mp3",    ".wav", ".aac",  ".ogg",   ".flac", // Audio
          ".mp4",    ".mov", ".avi",  ".mkv",   ".webm", // Video
          ".exe",    ".dll", ".so",   ".dylib", // Executables/Libraries
          ".o",      ".a",   ".lib",            // Object/Static Libs
          ".zip",    ".gz",  ".tar",  ".bz2",   ".rar",  ".7z", // Archives
          ".pdf",    ".doc", ".docx", ".xls",   ".xlsx", ".ppt",
          ".pptx",                            // Documents
          ".bin",    ".dat", ".iso",  ".img", // Binary data/images
          ".class",  ".jar",                  // Java
          ".pyc",                             // Python bytecode
          ".sqlite", ".db"                    // Databases
      };
      for (size_t i = 0; i < sizeof(binary_exts) / sizeof(binary_exts[0]);
           ++i) {
        // For case-insensitive extension comparison, you might use strcasecmp
        // (POSIX) or convert both ext and binary_exts[i] to lower/upper before
        // strcmp.
        if (strcasecmp(ext, binary_exts[i]) == 0) {
          likely_binary_by_ext = true;
          break;
        }
      }
    }
    if (likely_binary_by_ext) {
      fprintf(llm_fp, ", CONTENT:BINARY_HINT");
    }
    fprintf(llm_fp, ")\n");
  }
}

static bool is_likely_binary(const char *buffer, size_t size) {
  if (size == 0)
    return false;
  int non_printable_count = 0;
  // Check a smaller portion for performance on very large "text" files that
  // might have sparse binary-like data
  const size_t check_limit = size < 512 ? size : 512;

  for (size_t i = 0; i < check_limit; ++i) {
    if (buffer[i] == '\0')
      return true; // NUL byte is a very strong indicator for C-style strings
    // Check for non-printable characters, excluding common whitespace
    if (!isprint((unsigned char)buffer[i]) && buffer[i] != '\n' &&
        buffer[i] != '\r' && buffer[i] != '\t') {
      non_printable_count++;
    }
  }
  // If a significant percentage of the checked portion is non-printable, assume
  // binary. Adjust threshold as needed. 20% might be a reasonable starting
  // point.
  if (check_limit > 0 &&
      ((float)non_printable_count / (float)check_limit > 0.20)) {
    return true;
  }
  return false;
}

static bool
write_file_content_block(FILE *llm_fp, const DirContextTreeNode *file_node,
                         const char *file_node_id_str, FILE *dctx_binary_fp,
                         uint64_t data_section_start_offset_in_dctx_file) {
  if (file_node->type != NODE_TYPE_FILE)
    return true;

  fprintf(llm_fp, "\n<FILE_CONTENT_START ID=\"%s\" PATH=\"%s\">\n",
          file_node_id_str, file_node->relative_path);

  if (file_node->content_size == 0) {
    // Empty file, no content to write between tags
  } else {
    char *content_buffer =
        (char *)malloc(file_node->content_size); // Exact size needed
    if (content_buffer == NULL) {
      log_error("llm_formatter: Failed to allocate buffer for file content of "
                "'%s' (size %llu).",
                file_node->relative_path,
                (unsigned long long)file_node->content_size);
      fprintf(llm_fp,
              "[ERROR: Could not allocate memory to read file content]\n");
    } else {
      if (!dctx_read_file_content(
              dctx_binary_fp, data_section_start_offset_in_dctx_file, file_node,
              content_buffer, file_node->content_size)) {
        log_error("llm_formatter: Failed to read content for file '%s' from "
                  ".dircontxt.",
                  file_node->relative_path);
        fprintf(
            llm_fp,
            "[ERROR: Could not read file content from .dircontxt binary]\n");
      } else {
        if (is_likely_binary(content_buffer, file_node->content_size)) {
          fprintf(llm_fp, "[BINARY CONTENT PLACEHOLDER - Size: %llu bytes]\n",
                  (unsigned long long)file_node->content_size);
        } else {
          if (fwrite(content_buffer, 1, file_node->content_size, llm_fp) !=
              file_node->content_size) {
            log_error("llm_formatter: Failed to write content of '%s' to LLM "
                      "text file: %s",
                      file_node->relative_path, strerror(errno));
            fprintf(llm_fp,
                    "[ERROR: Failed to write content to output file]\n");
            // This indicates a problem with writing to llm_fp, might be a more
            // critical error
          }
        }
      }
      free(content_buffer);
    }
  }

  fprintf(llm_fp, "</FILE_CONTENT_END ID=\"%s\">\n", file_node_id_str);
  return true; // Returning true to allow processing of other files, errors are
               // logged. The main caller (generate_llm_context_file) will track
               // overall success.
}

static bool write_all_file_content_blocks_recursive(
    FILE *llm_fp, const DirContextTreeNode *node, int *file_id_counter,
    FILE *dctx_binary_fp, uint64_t data_section_start_offset_in_dctx_file) {
  if (node == NULL)
    return true;

  char node_id_str[20]; // For Fxxx ID

  if (node->type == NODE_TYPE_FILE) {
    snprintf(node_id_str, sizeof(node_id_str), "F%03d", (*file_id_counter)++);
    // write_file_content_block now logs errors but returns true to continue.
    // We don't need to check its return value here to stop the whole process,
    // unless we want to define certain errors from it as critical.
    write_file_content_block(llm_fp, node, node_id_str, dctx_binary_fp,
                             data_section_start_offset_in_dctx_file);
  } else if (node->type == NODE_TYPE_DIRECTORY) {
    for (uint32_t i = 0; i < node->num_children; ++i) {
      // Continue recursion even if one branch had issues, errors are logged.
      write_all_file_content_blocks_recursive(
          llm_fp, node->children[i], file_id_counter, dctx_binary_fp,
          data_section_start_offset_in_dctx_file);
    }
  }
  return true; // Always return true to indicate the traversal part completed.
               // Actual success of writing is logged by individual functions.
}

bool generate_llm_context_file(
    const char *llm_txt_filepath, const DirContextTreeNode *root_node,
    const char *dctx_binary_filepath,
    uint64_t data_section_start_offset_in_dctx_file) {
  if (llm_txt_filepath == NULL || root_node == NULL ||
      dctx_binary_filepath == NULL) {
    log_error("llm_formatter: Invalid arguments (NULL path or root_node).");
    return false;
  }

  FILE *llm_fp = fopen(llm_txt_filepath, "w");
  if (llm_fp == NULL) {
    log_error(
        "llm_formatter: Failed to open LLM context file '%s' for writing: %s",
        llm_txt_filepath, strerror(errno));
    return false;
  }

  FILE *dctx_binary_fp = NULL;
  bool overall_success = true;

  // Write Preamble and Instructions
  fprintf(llm_fp, "[DIRCONTXT_LLM_SNAPSHOT_V1.2]\n\n");
  fprintf(llm_fp, "<INSTRUCTIONS>\n");
  fprintf(llm_fp, "1. Manifest: The \"DIRECTORY_TREE\" section below lists all "
                  "files and directories.\n");
  fprintf(llm_fp, "   - Each entry: [TYPE] RELATIVE_PATH (ID:UNIQUE_ID, "
                  "MOD:UNIX_TIMESTAMP, SIZE:BYTES)\n");
  fprintf(llm_fp, "   - TYPE is [D] for directory, [F] for file.\n");
  fprintf(llm_fp, "   - SIZE is for files only.\n");
  fprintf(llm_fp, "   - Binary files may be noted with (CONTENT:BINARY_HINT or "
                  "CONTENT:BINARY_PLACEHOLDER).\n");
  fprintf(llm_fp, "2. Content Access: To read a specific file:\n");
  fprintf(llm_fp, "   - Find its UNIQUE_ID from the DIRECTORY_TREE.\n");
  fprintf(
      llm_fp,
      "   - Search for the marker: <FILE_CONTENT_START ID=\"UNIQUE_ID\">\n");
  fprintf(llm_fp, "   - The content is between this marker and "
                  "<FILE_CONTENT_END ID=\"UNIQUE_ID\">\n");
  fprintf(llm_fp, "</INSTRUCTIONS>\n\n");

  // Write Directory Tree Manifest
  fprintf(llm_fp, "<DIRECTORY_TREE>\n");
  int manifest_id_counter = 1;
  write_manifest_entry_recursive(llm_fp, root_node, 0, &manifest_id_counter);
  fprintf(llm_fp, "</DIRECTORY_TREE>\n");

  // Open the .dircontxt binary file to read actual file contents
  dctx_binary_fp = fopen(dctx_binary_filepath, "rb");
  if (dctx_binary_fp == NULL) {
    log_error("llm_formatter: Failed to open .dircontxt binary '%s' for "
              "reading content: %s",
              dctx_binary_filepath, strerror(errno));
    overall_success = false; // Critical failure, cannot write content blocks
  } else {
    int file_content_id_counter = 1; // Reset for content block IDs
    // write_all_file_content_blocks_recursive is designed to log its own errors
    // and continue We don't change overall_success based on its return here
    // unless it indicates a truly global failure. The main point is to try and
    // write as much as possible.
    write_all_file_content_blocks_recursive(
        llm_fp, root_node, &file_content_id_counter, dctx_binary_fp,
        data_section_start_offset_in_dctx_file);
  }

  fprintf(llm_fp, "\n[END_DIRCONTXT_LLM_SNAPSHOT]\n");

  // Cleanup
  if (dctx_binary_fp != NULL) {
    fclose(dctx_binary_fp);
  }
  if (llm_fp != NULL) {
    if (fflush(llm_fp) ==
        EOF) { // Ensure all data is written before checking ferror or closing
      log_error("llm_formatter: Error flushing LLM context file '%s': %s",
                llm_txt_filepath, strerror(errno));
      overall_success = false;
    }
    if (ferror(llm_fp)) { // Check for any write errors that occurred on llm_fp
      log_error(
          "llm_formatter: A write error occurred on LLM context file '%s'.",
          llm_txt_filepath);
      overall_success = false;
    }
    if (fclose(llm_fp) == EOF) {
      log_error("llm_formatter: Error closing LLM context file '%s': %s",
                llm_txt_filepath, strerror(errno));
      overall_success = false; // An error on close is significant
    }
  }

  if (overall_success) {
    log_info("llm_formatter: Successfully generated LLM context file: %s",
             llm_txt_filepath);
  } else {
    log_error("llm_formatter: Errors occurred during generation of LLM context "
              "file: %s",
              llm_txt_filepath);
    // Consider if a partial file should be removed or kept.
    // For now, it's kept, but the log indicates issues.
  }

  return overall_success;
}
