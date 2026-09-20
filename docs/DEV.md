# Developer documentation

GitShuffle is a GUI app that replaces Git's interactive rebase editor.
It parses the rebase todo file, lets the user reorder, edit, and split commits, detects conflicts, and writes the result back for `git rebase` to apply.
Uses **libgit2** for all Git operations except starting a rebase.

## Project layout

- `include/`, `src/` - headers and implementation
- `resources/` - fonts, icons, Qt stylesheets
- `tools/` - script templates
- `vendor/libgit2` - libgit2 submodule
- `cmake/` - helper cmake scripts

## App lifecycle

1. `main.cpp` - sets up `QApplication`, CLI flags, constructs `App`
2. `App` switches between 3 pages:
   - **Welcome** - pick a repo or load a save file
   - **Rebase selection** - start a rebase
   - **Rebase View** - the main editor

Startup paths:

- normal launch -> **Welcome** -> `App::openRepo()`
- `--edit-todo <file>` -> `App::openRepoCLI()`

## Integration with `git rebase`

1. GUI (`App::startNewRebase()`) or `tools/rebase-run.sh` (runs `git rebase` with `GIT_SEQUENCE_EDITOR` set to `<app> --edit-todo <file>`)
2. `git rebase` invokes the editor with the `--edit-todo <file>` option
3. `App::saveTodoFile` (via `action::Converter`) rewrites the todo file from the current plan; the app exits and `git rebase` executes it

## Architecture

- `action` - the rebase plan
  - `Action` = one todo line
    - type (`PICK`, `DROP`, `SQUASH`, `FIXUP`, `REWORD`, `EDIT`)
    - commit
    - tree/conflict
  - Actions form a linked list owned by the `ActionsManager` singleton
  - `action::Converter` serializes the list back to Git's todo file
- `conflict` - conflict detection and resolution
  - For each action, calls `git_cherrypick_commit` **in memory** against the previous action's resulting tree
  - `ConflictManager` contains all conflict resolutions
  - Resolutions are cached and reapplied if the plan changes
- `git` - libgit2 wrapper
  - `types.h` - RAII handles and OID formatting
  - `GitGraph<Data>` - commit DAG between two revisions (used for old/result graph panels)
  - `diff.h` - structured Git diff and conflict diff
  - `parser.h` - parses Git's todo file
  - `commit.h`, `head.h`, `paths.h`, `error.h` - helpers
- `patch` - splitting commit
  - `PatchSplitter` builds a new patch from selected diff lines
  - `patch::split()` creates two commits from one `Action` and a patch
- `state`
  - `AppState`
    - repo handle
    - rebase head/onto
    - conflict
  - `State::save`/`State::load` - (de)serializes actions, resolutions, root commit to a save file (independent of `git rebase`'s own state)
  - `Command`/`CommandHistory` - undo/redo stack
    - every edit is a `Command` pushed to `CommandHistory`
