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

- `core` - Qt-independent logic
  - `action` - the rebase plan
    - `Action` = one todo line
      - type (`PICK`, `DROP`, `SQUASH`, `FIXUP`, `REWORD`, `EDIT`)
      - commit
      - tree/conflict
    - Actions form a linked list owned by `ActionsManager`
    - `action::Converter` serializes the list back to Git's todo file
  - `conflict` - conflict detection and resolution
    - `ConflictManager` stores all conflict resolutions
      - Resolutions are cached and reapplied when the plan changes
  - `git` - libgit2 wrapper
    - `RepoSession.h` - repository information and repository lifetime
    - `types.h` - RAII handles and OID formatting
    - `GitGraph<Data>` - commit graph between two revisions (used by the old/result graph panels)
    - `diff.h` - structured Git diff
    - `parser.h` - parses Git's todo file
    - `commit.h`, `comment.h`, `head.h`, `paths.h`, `error.h` - helpers
  - `patch` - commit splitting
    - `PatchSplitter` builds a new patch from selected diff lines
    - `patch::split()` creates two commits from one `Action` and a patch
  - `rebase` - rebase plan analysis
    - `RebaseAnalyzer.h` - analyzes the rebase plan
    - `RebasePlan.h` - rebase plan with additional data such as the revision number
    - `RebaseAnalysis.h` - analyzer results
    - `RebaseSession.h` - holds `RepoSession`, `ConflictManager` and `RebasePlan`
- `app` - application logic, exposed through Qt signals
  - `rebase`
    - `RebaseController.h` - applies rebase plan modifications through an undo stack and emits `analysisChanged`, `errorOccurred` and `blockedByOpenEditors`
      - Modifications run through a gate (`runGuarded`) that blocks changes while a conflict editor they could invalidate is open
      - Gate scope depends on the action: none (message edits), per file (resolution changes), or global (move, split, type)
      - A blocked action is stored and runs only if the user discards the open editors
    - `RebasePlanModel.h` - Qt model over `RebaseAnalysis`. All modifications go through `RebaseController`
  - `conflict`
    - `ConflictEditor.h` - conflict editor template saved in settings (`save`/`load`); `createEditor(...)` replaces the template placeholders with concrete values
    - `ConflictEditorSession.h` - one open conflict editor, can be discarded
    - `ConflictEditorManager.h` - owns all open editor sessions and reports their events
- `state` - load and save
- `gui` - application widgets
  - `style` - colors and styles
  - `git` - git widgets
  - `rebase` - rebase widgets
  - `widget` - common widgets
