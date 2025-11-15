#ifndef DIFF_H
#define DIFF_H

#include "datatypes.h"

// Enum to describe the type of change
typedef enum { ITEM_ADDED, ITEM_REMOVED, ITEM_MODIFIED } ChangeType;

// Struct to hold info about one changed item
typedef struct {
  ChangeType type;
  const DirContextTreeNode *node; // Pointer to the node in the NEW tree (for
                                  // ADDED/MODIFIED) or OLD tree (for REMOVED)
} DiffEntry;

// Struct to hold the complete comparison report
typedef struct {
  bool trees_are_different;
  DiffEntry *changes;
  int num_changes;
  // We could also have separate lists for added, removed, modified
} DiffReport;

// Compares two directory trees and returns a report of the differences.
// The caller is responsible for freeing the report with free_diff_report().
DiffReport *compare_trees(const DirContextTreeNode *old_tree,
                          const DirContextTreeNode *new_tree);

// Frees the memory allocated for a DiffReport.
void free_diff_report(DiffReport *report);

#endif // DIFF_H
