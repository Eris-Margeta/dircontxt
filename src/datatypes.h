#ifndef DATATYPES_H
#define DATATYPES_H

#include <limits.h> // For PATH_MAX (though we'll define our own if not available)
#include <stdbool.h> // For bool
#include <stdint.h>  // For uint64_t, uint32_t, uint16_t, uint8_t

// Define MAX_PATH_LEN if PATH_MAX is not suitably defined or to have a
// consistent buffer
#ifndef MAX_PATH_LEN
#ifdef PATH_MAX
#define MAX_PATH_LEN PATH_MAX
#else
#define MAX_PATH_LEN 4096 // A common fallback
#endif
#endif

// Forward declaration for the tree node structure
struct DirContextTreeNode;

typedef enum { NODE_TYPE_FILE, NODE_TYPE_DIRECTORY } NodeType;

// Structure for ignore rules parsed from .dircontxtignore
typedef struct {
  char pattern[MAX_PATH_LEN];
  bool is_dir_only;
  bool is_wildcard_prefix_match;
  bool is_wildcard_suffix_match;
  bool is_exact_name_match;
} IgnoreRule;

// Structure for representing a file or directory in our in-memory tree
typedef struct DirContextTreeNode {
  NodeType type;
  char relative_path[MAX_PATH_LEN];
  uint64_t last_modified_timestamp;

  // --- For files ---
  uint64_t content_offset_in_data_section;
  uint64_t content_size;
  char disk_path[MAX_PATH_LEN];

  // --- For directories ---
  struct DirContextTreeNode **children;
  uint32_t num_children;
  uint32_t children_capacity;

  // --- ADDED FOR LLM FORMATTER ID STORAGE ---
  char generated_id_for_llm[20]; // To store IDs like "F001", "D002", "ROOT"

} DirContextTreeNode;

#endif // DATATYPES_H
