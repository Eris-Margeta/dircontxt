#include "llm_formatter.h"
#include "datatypes.h"
#include "dctx_reader.h"
#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // For strcasecmp (POSIX)
#include <time.h>

// --- Static Helper Function Declarations ---
// MODIFIED: write_manifest_entry_recursive now takes non-const node to store ID
static void write_manifest_entry_recursive(FILE *llm_fp,
                                           DirContextTreeNode *node,
                                           int indent_level,
                                           int *shared_id_counter);

static bool
write_file_content_block(FILE *llm_fp, const DirContextTreeNode *file_node,
                         // const char *file_node_id_str, // No longer needed,
                         // use node->generated_id_for_llm
                         FILE *dctx_binary_fp,
                         uint64_t data_section_start_offset_in_dctx_file);

static bool is_likely_binary(const char *buffer, size_t size);

// MODIFIED: write_all_file_content_blocks_recursive no longer needs its own ID
// counter
static bool write_all_file_content_blocks_recursive(
    FILE *llm_fp, const DirContextTreeNode *node, FILE *dctx_binary_fp,
    uint64_t data_section_start_offset_in_dctx_file);

// --- Implementation of Static Helper Functions ---

// MODIFIED: Takes non-const DirContextTreeNode *node to store
// generated_id_for_llm
static void write_manifest_entry_recursive(FILE *llm_fp,
                                           DirContextTreeNode *node,
                                           int indent_level,
                                           int *shared_id_counter) {
  if (node == NULL)
    return;

  for (int i = 0; i < indent_level; ++i) {
    fprintf(llm_fp, "  ");
  }

  // Generate and store ID in the node
  if (node->type == NODE_TYPE_DIRECTORY) {
    if (indent_level == 0) {
      strcpy(node->generated_id_for_llm, "ROOT");
    } else {
      snprintf(node->generated_id_for_llm, sizeof(node->generated_id_for_llm),
               "D%03d", (*shared_id_counter)++);
    }
    fprintf(llm_fp, "[D] %s (ID:%s, MOD:%lld)\n", node->relative_path,
            node->generated_id_for_llm,
            (long long)node->last_modified_timestamp);

    for (uint32_t i = 0; i < node->num_children; ++i) {
      write_manifest_entry_recursive(llm_fp, node->children[i],
                                     indent_level + 1, shared_id_counter);
    }
  } else { // NODE_TYPE_FILE
    snprintf(node->generated_id_for_llm, sizeof(node->generated_id_for_llm),
             "F%03d", (*shared_id_counter)++);
    fprintf(llm_fp, "[F] %s (ID:%s, MOD:%lld, SIZE:%lld", node->relative_path,
            node->generated_id_for_llm,
            (long long)node->last_modified_timestamp,
            (long long)node->content_size);

    const char *ext = strrchr(node->relative_path, '.');
    bool likely_binary_by_ext = false;
    if (ext) {
      const char *binary_exts[] = {
          ".png", ".jpg",  ".jpeg",   ".gif", ".bmp",  ".tiff", ".ico",
          ".mp3", ".wav",  ".aac",    ".ogg", ".flac", ".mp4",  ".mov",
          ".avi", ".mkv",  ".webm",   ".exe", ".dll",  ".so",   ".dylib",
          ".o",   ".a",    ".lib",    ".zip", ".gz",   ".tar",  ".bz2",
          ".rar", ".7z",   ".pdf",    ".doc", ".docx", ".xls",  ".xlsx",
          ".ppt", ".pptx", ".bin",    ".dat", ".iso",  ".img",  ".class",
          ".jar", ".pyc",  ".sqlite", ".db"};
      for (size_t i = 0; i < sizeof(binary_exts) / sizeof(binary_exts[0]);
           ++i) {
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
  const size_t check_limit = size < 512 ? size : 512;

  for (size_t i = 0; i < check_limit; ++i) {
    if (buffer[i] == '\0')
      return true;
    if (!isprint((unsigned char)buffer[i]) && buffer[i] != '\n' &&
        buffer[i] != '\r' && buffer[i] != '\t') {
      non_printable_count++;
    }
  }
  if (check_limit > 0 &&
      ((float)non_printable_count / (float)check_limit > 0.20)) {
    return true;
  }
  return false;
}

// MODIFIED: Removed file_node_id_str parameter, uses node->generated_id_for_llm
static bool
write_file_content_block(FILE *llm_fp, const DirContextTreeNode *file_node,
                         FILE *dctx_binary_fp,
                         uint64_t data_section_start_offset_in_dctx_file) {
  if (file_node->type != NODE_TYPE_FILE)
    return true;
  if (file_node->generated_id_for_llm[0] == '\0') { // Check if ID was generated
    log_error("llm_formatter: Skipping content block for file '%s' due to "
              "missing generated ID.",
              file_node->relative_path);
    return true; // Continue with other files
  }

  fprintf(llm_fp, "\n<FILE_CONTENT_START ID=\"%s\" PATH=\"%s\">\n",
          file_node->generated_id_for_llm, file_node->relative_path);

  if (file_node->content_size == 0) {
    // Empty file
  } else {
    char *content_buffer = (char *)malloc(file_node->content_size);
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
          }
        }
      }
      free(content_buffer);
    }
  }

  fprintf(llm_fp, "</FILE_CONTENT_END ID=\"%s\">\n",
          file_node->generated_id_for_llm);
  return true;
}

// MODIFIED: Removed file_id_counter parameter
static bool write_all_file_content_blocks_recursive(
    FILE *llm_fp, const DirContextTreeNode *node, FILE *dctx_binary_fp,
    uint64_t data_section_start_offset_in_dctx_file) {
  if (node == NULL)
    return true;

  if (node->type == NODE_TYPE_FILE) {
    write_file_content_block(llm_fp, node, dctx_binary_fp,
                             data_section_start_offset_in_dctx_file);
  } else if (node->type == NODE_TYPE_DIRECTORY) {
    for (uint32_t i = 0; i < node->num_children; ++i) {
      write_all_file_content_blocks_recursive(
          llm_fp, node->children[i], dctx_binary_fp,
          data_section_start_offset_in_dctx_file);
    }
  }
  return true;
}

// MODIFIED: generate_llm_context_file now takes non-const root_node
bool generate_llm_context_file(
    const char *llm_txt_filepath,
    DirContextTreeNode *root_node, // Made non-const
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

  fprintf(llm_fp, "<DIRECTORY_TREE>\n");
  // MODIFIED: Use a single shared counter for Dxxx and Fxxx IDs
  int shared_id_counter = 1;
  write_manifest_entry_recursive(
      llm_fp, root_node, 0, &shared_id_counter); // Pass non-const root_node
  fprintf(llm_fp, "</DIRECTORY_TREE>\n");

  dctx_binary_fp = fopen(dctx_binary_filepath, "rb");
  if (dctx_binary_fp == NULL) {
    log_error("llm_formatter: Failed to open .dircontxt binary '%s' for "
              "reading content: %s",
              dctx_binary_filepath, strerror(errno));
    overall_success = false;
  } else {
    // MODIFIED: No longer pass separate file_id_counter
    write_all_file_content_blocks_recursive(
        llm_fp, root_node, dctx_binary_fp,
        data_section_start_offset_in_dctx_file);
  }

  fprintf(llm_fp, "\n[END_DIRCONTXT_LLM_SNAPSHOT]\n");

  if (dctx_binary_fp != NULL) {
    fclose(dctx_binary_fp);
  }
  if (llm_fp != NULL) {
    if (fflush(llm_fp) == EOF) {
      log_error("llm_formatter: Error flushing LLM context file '%s': %s",
                llm_txt_filepath, strerror(errno));
      overall_success = false;
    }
    if (ferror(llm_fp)) {
      log_error(
          "llm_formatter: A write error occurred on LLM context file '%s'.",
          llm_txt_filepath);
      overall_success = false;
    }
    if (fclose(llm_fp) == EOF) {
      log_error("llm_formatter: Error closing LLM context file '%s': %s",
                llm_txt_filepath, strerror(errno));
      overall_success = false;
    }
  }

  if (overall_success) {
    log_info("llm_formatter: Successfully generated LLM context file: %s",
             llm_txt_filepath);
  } else {
    log_error("llm_formatter: Errors occurred during generation of LLM context "
              "file: %s",
              llm_txt_filepath);
  }

  return overall_success;
}
