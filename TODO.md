# dircontxt Project TODO

This file tracks the upcoming features and improvements for the `dircontxt` utility.

## V1.1 - Quality of Life & Usability

-   [ ] **Implement `make install` Target**
    -   Add `install` and `uninstall` targets to the `Makefile`.
    -   This will allow users to install the `dctx` binary to a system-wide path like `/usr/local/bin` using a standard C project convention.

-   [ ] **Add Command-Line Arguments**
    -   Implement command-line flags to provide more flexible, one-off control.
    -   `--output-mode [both|text|binary]`: Override the global `config` file's `OUTPUT_MODE`.
    -   `--force-v1`: Add a flag to force a fresh V1 snapshot, ignoring any existing state.
    -   `--verbose`: A flag to enable debug logging for a single run, which is easier than recompiling.

-   [ ] **Improve Error Handling for Corrupted State**
    -   Enhance `dctx_reader.c` to more gracefully handle a corrupted or partially written `.dircontxt` file.
    -   If corruption is detected, the program should inform the user and offer a clear path forward (e.g., automatically creating a fresh V1 snapshot).

## V1.2 - Advanced Feature Enhancements

-   [ ] **Enhance Ignore Pattern Syntax**
    -   Upgrade the parser in `ignore.c` to support more advanced `.gitignore`-style patterns.
    -   **Globstar (`**`):** Implement support for matching directories at any depth (e.g., `foo/**/bar`).
    -   **General Wildcards (`*`):** Implement support for wildcards in the middle of a pattern (e.g., `config-*-prod.json`).

-   [ ] **Refine Diff Output**
    -   Currently, the diff file shows the *entire* content of a modified file.
    -   Investigate implementing a line-by-line diff (similar to `git diff`) for the `<FILE_CONTENT_START>` blocks to make the diffs even more concise and useful. This is a highly advanced feature.
