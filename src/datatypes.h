#ifndef DATATYPES_H
#define DATATYPES_H

#include <stdint.h> // For uint64_t, uint32_t, uint16_t, uint8_t
#include <stdbool.h> // For bool
#include <limits.h>  // For PATH_MAX (though we'll define our own if not available)

// Define MAX_PATH_LEN if PATH_MAX is not suitably defined or to have a consistent buffer
#ifndef MAX_PATH_LEN
    #ifdef PATH_MAX
        #define MAX_PATH_LEN PATH_MAX
    #else
        #define MAX_PATH_LEN 4096 // A common fallback
    #endif
#endif

// Forward declaration for the tree node structure
struct DirContextTreeNode;

typedef enum {
    NODE_TYPE_FILE,
    NODE_TYPE_DIRECTORY
} NodeType;

// Structure for ignore rules parsed from .dircontxtignore
typedef struct {
    char pattern[MAX_PATH_LEN];
    bool is_dir_only;      // True if pattern explicitly targets directories (e.g., "dist/")
    bool is_wildcard_prefix_match; // True for patterns like "build/*" (matches start of path)
    bool is_wildcard_suffix_match; // True for patterns like "*.log" (matches end of name)
    bool is_exact_name_match;    // True for patterns like "file.txt" (matches name directly)
    // Add more flags if supporting more complex globbing, e.g., negation
} IgnoreRule;

// Structure for representing a file or directory in our in-memory tree
typedef struct DirContextTreeNode {
    NodeType type;
    char relative_path[MAX_PATH_LEN]; // Path relative to the root of the dircontxt operation
    uint64_t last_modified_timestamp; // Unix timestamp (seconds since epoch)

    // --- For files ---
    uint64_t content_offset_in_data_section; // Byte offset in the Data Section of .dircontxt
    uint64_t content_size;                   // Size of the file's content in bytes
    char disk_path[MAX_PATH_LEN];      // Absolute path on disk (used during creation phase)

    // --- For directories ---
    struct DirContextTreeNode **children; // Array of pointers to child nodes
    uint32_t num_children;                // Current number of children
    uint32_t children_capacity;           // Allocated capacity for the children array
} DirContextTreeNode;

#endif // DATATYPES_H
