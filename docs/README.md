# Git Shuffle - User documentation

## Setup

Build the project first. This produces the app executable `git_shuffle`, along with two helper scripts in your build directory: `tools/wrapper.sh` and `tools/rebase-run.sh`.

- `tools/wrapper.sh` the rebase editor wrapper, used as `GIT_SEQUENCE_EDITOR`.
- `tools/rebase-run.sh` a convenience script that starts the rebase for you in a single step from the terminal.

## Starting a rebase

Git Shuffle offers a few different ways to start a rebase:

### From the GUI

Open the repository directory.
If there's no rebase already in progress, you'll be prompted to choose the branch to rebase, then the starting commit.
Once you click the rebase button, Git Shuffle shows you the command it's about to run and asks you to confirm.
After confirming, the app closes and hands off to `git rebase`, which reopens a new instance of Git Shuffle to edit the plan.
If a rebase is already in progress, just open the repository directory.

### Using `rebase-run.sh`

This script takes a branch or commit and rebases onto it using Git Shuffle.

#### Usage

`tools/rebase-run.sh <branch-name|commit-hash> [options]`

| Argument                     | Description             |
| ---------------------------- | ----------------------- |
| `<branch-name\|commit-hash>` | Target branch or commit |

| Option                                        | Description                                                    |
| --------------------------------------------- | -------------------------------------------------------------- |
| `--default <branch-name>`, `-d <branch-name>` | Use the specified default branch instead of the current branch |
| `--dry-run`                                   | Print the command that would run, without executing it         |
| `--verbose`, `-v`                             | Enable verbose output for detailed logs                        |
| `--help`                                      | Show the help message and exit                                 |

### Using `git rebase` directly

If you'd rather start the rebase with `git rebase -i ...` and have Git Shuffle open as the editor. Point the `GIT_SEQUENCE_EDITOR` environment variable at the wrapper script.

To make this the permanent:

- add the variable to your shell profile (`.bashrc`, `.zshrc`, etc.)
- run `git config sequence.editor <path-to-wrapper>` for a single repository or `git config --global sequence.editor <path-to-wrapper>` for all repositories.

## The app

On launch, you'll see a welcome window with a few ways to open a repository.
After opening the repository you'll see the list of commits in the rebase.

### Editing commits

#### Changing a commit's action

Set a commit's action (pick, squash, fixup, reword, drop) using the mouse or assign a keyboard shortcut to each action in Settings for faster editing.

#### Reordering commits

Reorder commits with the mouse or use configurable keyboard shortcuts (set in Settings) to move the selected commit up or down.

#### Splitting a commit

Select a commit to view its diff, then select the lines you want to split out using the mouse.
Git Shuffle creates a new commit from the selected lines, leaving the remainder in the original commit.

### Resolving conflicts

Git Shuffle flags any commit in the plan that would conflict when applied.
Select the flagged commit and click Resolve.
This puts the repository into a conflicted state, just like a normal git rebase conflict.

Resolve it the way you normally would: edit the files, `git add` the resolved paths, then return to Git Shuffle and mark it resolved to continue.

### Finishing the rebase

If the app was launched via `git rebase` or the provided script it will ask before closing whether you want to execute, save, or discard the plan.
Saving pauses the rebase without executing the plan.
You can edit it later either by reopening the repository in Git Shuffle or by running `git rebase --edit-todo` (if the sequence editor is configured).

### Settings

Open Settings from the top panel under **Edit/Preferences**.
From there you can configure keyboard shortcuts and colors for each action.

### Command-line options

`git_shuffle [options]`

| Option                    | Description                                                                                              |
| ------------------------- | -------------------------------------------------------------------------------------------------------- |
| `--edit-todo <todo-file>` | Launch the app with the given todo file from a valid repository, and exit with the resulting status code |
| `--no-style`              | Disable all styles and fonts                                                                             |
| `--version`, `-v`         | Show version information                                                                                 |
| `--verbose`               | Enable verbose output for detailed logs                                                                  |
| `--help`, `-h`            | Show the help message and exit                                                                           |
| `--help-all`              | Show help, including generic Qt options                                                                  |

## Supported TODO actions

Git Shuffle edits the todo file for single-branch rebases only and supports:

- `pick`
- `drop`
- `reword`
- `squash`
- `fixup`

If your rebase todo file includes any action outside this list, Git Shuffle displays an error and exits with an error code.
