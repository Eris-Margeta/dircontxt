# dircontxt Project TODO

This file tracks the upcoming features and improvements for the `dircontxt` utility.
## Urgent fixes

- fix the behavior where output file header iterates version number e.g. V1.1 -> V1.2 on a run where no changes are detected in the files. In such cases we should not iterate version number.

- add a flag to specify the output folder that overrides default output folder which is the parent directory 

-fix output mode from user's directory config file. There is no output mode - we should always create binary and text files by default. 

-add CLI behavior on running the command without the path argument. For example if the command is run such as "dctx ."; we automatically create output files as we do currently. If the command is ran without any arguments such as "dctx" inside a folder, then we need to start a CLI which prompts our utility behavior (defaults are set inside parentheses): 
 -- First question: specify context folder path (.):
 -- Second question: create default .dircontxtignore(Y/n):
 -- Third question: specify output folder (choice): (parent folder [../.] | .dircontxt/ | custom path ). Default is parent folder, If .dircontxt/ folder is chosen we must create the folder. If custom path is chosen we must enable the user to enter a valid path.
 -- Fourth question: add .dircontxt files to .gitignore (recommended) [WARNING! Your repo size will double if dctx files are not ignored] (Y/n). If no then nothing. If yes, then create .gitignore if it doesn't exist, if it does add the lines to ignore the .dircontxt/ folder and any text files inside the repo named by the naming convention of our output files. so any text files that end in llmcontext.txt (our main output), any text files that fall under our diff files which are named as [foldername].llmcontext-[version_number]-diff.txt. We must also add the binary files that are named as [foldername].dircontxt (our file extension).

- Check and make sure that ignore rules are consistent in logic through the hierarchy of three tiers of file ignore rules (AND), meaning if something is hardcoded to be ignored like .git folder, adding that same folder in tier 2 which is user config file under ~/.config/dircontxt/ignore will NOT cancel the ignored files (double negation), but instead it will remain ignored. We also must implement this behavior for the third tier of ignored files configuration which is the .dircontxtignore file in the context folder. If .git/ folder is added there meaning it's added three times in total accross the tiers - we will still correctly ignore those files. 
## - Quality of Life & Usability

-   [ ] **Implement `make install` Target**
    -   Add `install` and `uninstall` targets to the `Makefile`.
    -   This will allow users to install the `dctx` binary to a system-wide path like `/usr/local/bin` using a standard C project convention.

-   [ ] **Add Command-Line Arguments**
    -   Implement command-line flags to provide more flexible, one-off control.
    -   `--force-v1`: Add a flag to force a fresh V1 snapshot, ignoring any existing state.
    -   `--verbose`: A flag to enable debug logging for a single run, which is easier than recompiling.

-   [ ] **Improve Error Handling for Corrupted State**
    -   Enhance `dctx_reader.c` to more gracefully handle a corrupted or partially written `.dircontxt` file.
    -   If corruption is detected, the program should inform the user and offer a clear path forward (e.g., automatically creating a fresh V1 snapshot).

## - Advanced Feature Enhancements

-   [ ] **Enhance Ignore Pattern Syntax**
    -   Upgrade the parser in `ignore.c` to support more advanced `.gitignore`-style patterns.
    -   **Globstar (`**`):** Implement support for matching directories at any depth (e.g., `foo/**/bar`).
    -   **General Wildcards (`*`):** Implement support for wildcards in the middle of a pattern (e.g., `config-*-prod.json`).

-   [ ] **Refine Diff Output**
    -   Currently, the diff file shows the *entire* content of a modified file.
    -   Investigate implementing a line-by-line diff (similar to `git diff`) for the `<FILE_CONTENT_START>` blocks to make the diffs even more concise and useful. This is a highly advanced feature.
