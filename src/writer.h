#ifndef WRITER_H
#define WRITER_H

#include "datatypes.h" // For DirContextTreeNode
#include <stdbool.h>
#include <stdio.h> // For FILE* (though typically not in .h for opaque types, here for clarity)

// --- Constants for the .dircontxt format ---
#define DIRCONTXT_FILE_SIGNATURE                                               \
  "DIRCTXTV1" // 8 bytes, including potential null terminator if treated as
              // string
#define DIRCONTXT_SIGNATURE_LEN                                                \
  8 // Ensure this matches the actual length written, including null if any

// --- Core Writing Function ---

// Writes the in-memory directory tree and file contents to a .dircontxt file.
//
// Parameters:
//   output_filepath: The full path to the .dircontxt file to be
//   created/overwritten. root_node: Pointer to the root of the in-memory
//   DirContextTreeNode tree.
//              This tree should have file node content_offset_in_data_section
//              and content_size fields PRE-CALCULATED by a preliminary pass if
//              they are not calculated during this write. (Our approach will
//              calculate them during the write process).
//
// Returns:
//   True if the file was written successfully, false otherwise.
bool write_dircontxt_file(const char *output_filepath,
                          DirContextTreeNode *root_node);

// --- Helper Function Declarations (potentially static in writer.c, but useful
// for understanding) --- These would typically be static within writer.c if not
// exposed, but declaring them here can clarify the process. For now, we'll
// assume they are internal to writer.c.

// (Internal helper: Serializes a single tree node to a header buffer/file)
// void serialize_node_to_header(const DirContextTreeNode *node, FILE
// *header_stream);

// (Internal helper: Recursively traverses the tree to write header data and
// gather file data) bool
// serialize_tree_and_collect_data_recursive(DirContextTreeNode *node,
//                                                FILE *header_stream, /* Stream
//                                                for header part */ FILE
//                                                *data_stream,   /* Stream for
//                                                concatenated file data part */
//                                                uint64_t
//                                                *current_data_offset_accumulator);

#endif // WRITER_H
