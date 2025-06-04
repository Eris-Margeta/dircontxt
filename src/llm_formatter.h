#ifndef LLM_FORMATTER_H
#define LLM_FORMATTER_H

#include "datatypes.h" // For DirContextTreeNode
#include <stdbool.h>

// --- Core LLM Context File Generation Function ---

// Generates an LLM-friendly text file from a parsed .dircontxt tree and its
// binary file.
//
// Parameters:
//   llm_txt_filepath: Path to the .llmcontext.txt file to be
//   created/overwritten. root_node: Pointer to the root of the in-memory
//   DirContextTreeNode tree (parsed from .dircontxt). dctx_binary_filepath:
//   Path to the original .dircontxt binary file (needed to read raw file
//   contents). data_section_start_offset_in_dctx_file: The byte offset in the
//   .dircontxt binary file
//                                           where the actual file data section
//                                           begins.
//
// Returns:
//   True if the LLM context file was generated successfully, false otherwise.
bool generate_llm_context_file(const char *llm_txt_filepath,
                               const DirContextTreeNode *root_node,
                               const char *dctx_binary_filepath,
                               uint64_t data_section_start_offset_in_dctx_file);

#endif // LLM_FORMATTER_H
