## TODO: Implement Advanced Ignore File Handling

Test 3
This task focuses on upgrading the `dircontxt` ignore system to be more robust, automatic, and feature-rich, closely mirroring the functionality of Git's `.gitignore`.

### Phase 1: Design and Scoping

*   [ ] **Define Enhanced Syntax:** Formalize the new patterns to support beyond the current implementation.
    *   `**` for matching directories at any depth.
    *   `!` for negating a pattern (re-including a file that was previously ignored).
    *   Wildcards (`*`) anywhere in the pattern, not just at the start/end.
*   [ ] **Establish a Rule Hierarchy:** Define the order in which ignore rules are applied.
    1.  **Default Rules (Hardcoded):** A built-in list of common ignores (`.git`, `.DS_Store`, etc.) that are always active.
    2.  **Global Ignore File:** A user-specific file (e.g., `~/.config/dircontxt/ignore`) that applies to every project.
    3.  **Project-Specific Ignore File:** The `.dircontxtignore` file in the target directory (highest priority).
*   [ ] **Plan Data Structure Changes:** Determine what modifications are needed for the `IgnoreRule` struct in `datatypes.h` to accommodate the new syntax (e.g., a flag for negation).

### Phase 2: Implementation

*   [ ] **Modify `datatypes.h`:** Update the `IgnoreRule` struct with new fields to represent pattern types (e.g., negation, globstar `**`).
*   [ ] **Enhance `ignore.c` - `parse_ignore_pattern_line()`:** Rewrite the parser to understand and correctly categorize the new, advanced syntax.
*   [ ] **Upgrade `ignore.c` - `load_ignore_rules()`:** Modify this function to implement the new hierarchy:
    1.  First, add the hardcoded default rules to the list.
    2.  Next, search for and load the global ignore file.
    3.  Finally, load the project-specific `.dircontxtignore` file.
*   [ ] **Rewrite `ignore.c` - `should_ignore_item()`:** Overhaul the matching logic to correctly handle the new patterns. The logic must respect the rule hierarchy, especially the `!` negation patterns, which should override any previous matching ignore rule.

### Phase 3: Testing and Documentation

*   [ ] **Expand `Makefile` `run` Target:** Add more complex file and directory structures to the `test_dir` to validate the new ignore rules (e.g., nested folders, files that should be negated).
*   [ ] **Update Documentation:** Thoroughly document the new, advanced ignore syntax and the concept of the global ignore file in the `README.md`.

***

## Prompt for Gemini LLM

Here is a structured prompt designed to guide an AI to perform the implementation tasks described above.

**Prompt Starts Here**
---

### High-Level Goal

Your task is to upgrade the file/directory ignore system in the provided `dircontxt` C project. The goal is to enhance its capabilities to more closely match the functionality of `.gitignore`, including support for more advanced patterns and a hierarchical configuration system (default, global, and project-level ignores).

### Context

You are being provided with the complete source code of the `dircontxt` project in the `[DIRCONTXT_LLM_SNAPSHOT_V1.2]` format. The current system uses a `.dircontxtignore` file and is implemented primarily in `src/ignore.c`, `src/ignore.h`, and `src/datatypes.h`. The current implementation is basic and needs to be significantly expanded.

*(You would paste the full project snapshot from the previous turn here)*

### Core Requirements

1.  **Expand Pattern Syntax:** The ignore system must be updated to parse and correctly match the following patterns:
    *   **Negation:** Patterns starting with `!` should re-include a file that was matched by a previous pattern. For example, if `*.log` is ignored, `!important.log` should ensure that `important.log` is **not** ignored.
    *   **Deep Directory Matching (`**`):** The `**` pattern should match directories at any level. For example, `foo/**/bar` should match `foo/bar`, `foo/a/bar`, `foo/a/b/bar`, etc.
    *   **General Wildcards (`*`):** The `*` wildcard should be usable anywhere in a pattern, not just at the beginning of an extension. For example, `config-*.json` should match `config-dev.json` and `config-prod.json`.

2.  **Implement a Default Ignore List:** A hardcoded, non-configurable list of patterns should be applied by default to every run. This list should be the lowest priority and should include common patterns like:
    *   `.git/`
    *   `.DS_Store`
    *   `node_modules/`

3.  **Implement a Global Ignore File:** The application should look for a global ignore file at a standard user location (e.g., `~/.config/dircontxt/ignore`). If this file exists, its rules should be loaded after the default list but before the project-specific file.

### Implementation Guidance

You should focus your modifications on the following files.

#### 1. `src/datatypes.h`

*   Modify the `IgnoreRule` struct to support the new syntax. I suggest adding an `enum` for pattern complexity and a `bool` for negation.
    ```c
    // Example suggestion
    typedef enum {
        PATTERN_EXACT,
        PATTERN_SUFFIX,
        PATTERN_SUBSTRING, // For general wildcards
        PATTERN_PATH_GLOB  // For patterns with '**'
    } PatternType;

    typedef struct {
      char pattern[MAX_PATH_LEN];
      PatternType type;
      bool is_dir_only;
      bool is_negation; // Add this field
    } IgnoreRule;
    ```

#### 2. `src/ignore.c`

This file will require the most significant changes.

*   **`parse_ignore_pattern_line()`**:
    *   Rewrite this function to detect the new syntax.
    *   Check for a leading `!` to set the `is_negation` flag.
    *   Detect `**` and `*` to set the appropriate `PatternType`.
    *   Remember to strip the `!` from the `pattern` field after setting the flag.

*   **`load_ignore_rules()`**:
    *   Change this function's logic to load rules in the correct order of precedence:
        1.  Start by adding the hardcoded **default rules** to the rules array.
        2.  Next, construct the path to the **global ignore file** (e.g., by getting the `HOME` environment variable). If it exists, parse it and append its rules.
        3.  Finally, look for the **project-specific `.dircontxtignore`** and append its rules.

*   **`should_ignore_item()`**:
    *   This function must be completely overhauled.
    *   It should iterate through all loaded rules from lowest to highest priority.
    *   It should keep track of the latest matching rule. A simple approach is to have a `bool ignored = false;` and an `int last_match_priority = -1;`.
    *   For each rule, check if it matches the item's path.
    *   If a rule matches:
        *   If it's a **negation rule** (`is_negation = true`), the item should be marked as **not ignored**.
        *   If it's a **standard rule**, the item should be marked as **ignored**.
    *   The final decision should be based on the **last rule in the list that matches the item**.

### Deliverables

Please provide the following as your response:

1.  A list of all the files you have modified.
2.  The complete, updated source code for each of the modified files (`datatypes.h` and `ignore.c`).
3.  A brief summary of the changes you made and how they fulfill the requirements.
