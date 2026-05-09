#!/bin/bash

set -euo pipefail

RED=""
YELLOW=""
GREEN=""
BLUE=""
RESET=""
BOLD=""

WRAPPER="@CMAKE_CURRENT_BINARY_DIR@/wrapper.sh"

# check if running in TTY
if [[ -t 1 ]] && command -v tput &> /dev/null; then
    RED=$(tput setaf 1)
    YELLOW=$(tput setaf 3)
    GREEN=$(tput setaf 2)
    BLUE=$(tput setaf 4)
    RESET=$(tput sgr0)
    BOLD=$(tput bold)
fi

error() {
    echo "${RED}${BOLD}error:${RESET} $*" >&2
}

warn() {
    echo "${YELLOW}${BOLD}warning:${RESET} $*" >&2
}

info() {
    if [ "$VERBOSE" -eq 1 ]; then
        echo "${BLUE}${BOLD}info:${RESET} $*"
    fi
}


print_help() {
    cat <<'EOF'
Usage:
  <command> <branch-name|commit-hash> [options]

Arguments:
  <branch-name|commit-hash>
      Target branch or commit hash.

Options:
  --default, -d <branch-name>
      Specify the default branch to use instead of the current branch.

  --dry-run
      Print the command that would be executed without running it.

  --verbose, -v
      Enable verbose output for debugging and detailed logs.

  --help
      Show this help message and exit.
EOF
}


is_commit() {
    git rev-parse --verify "$1^{commit}" &> /dev/null
}

is_local_branch() {
    git show-ref --verify --quiet "refs/heads/$1"
}

is_remote_branch() {
    local branch="$1"

    for remote in $(git remote); do
        if git show-ref --verify --quiet "refs/remotes/$remote/$branch"; then
            return 0
        fi
    done
    return 1
}

is_branch() {
    local branch="$1"
    is_local_branch "$branch" || is_remote_branch "origin/$branch"
}

rebase_and_exit() {
    if [[ -z "${1-}" ]]; then
        run env GIT_SEQUENCE_EDITOR="$WRAPPER" git rebase -i --root
    else
        run env GIT_SEQUENCE_EDITOR="$WRAPPER" git rebase -i "$1"
    fi

    exit $?
}

run() {
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "${GREEN}${BOLD}[dry-run]:${RESET} $*"
        return 0
    fi

    "$@"
}


if [ $# -lt 1 ]; then
    error "Missing argument"
    print_help
    exit 1
fi


TARGET=""
DEFAULT_BRANCH=""
VERBOSE=0
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--default)
            if [ $# -lt 2]; then
                error "option requires argument: $1"
                exit 1
            fi

            if [ -n "$DEFAULT_BRANCH" ]; then
                error "option specified multiple times: $1"
                exit 1
            fi

            DEFAULT_BRANCH="$2"
            shift 2
            ;;

        -d=*|--default=*)
            if [ -n "$DEFAULT_BRANCH" ]; then
                error "option specified multiple times: $1"
                exit 1
            fi

            DEFAULT_BRANCH="${1#*=}"

            if [ -z "$DEFAULT_BRANCH" ]; then
                error "empty argument for $1"
                exit 1
            fi

            shift
            ;;

        --dry-run)
            DRY_RUN=1
            shift
            ;;

        -h|--help)
            print_help
            exit 0
            ;;

        -v|--verbose)
            VERBOSE=1
            shift
            ;;

        -*)
            error "unknown option $1"
            exit 1
            ;;
        *)
            if [ -n "$TARGET" ]; then
                error "multiple arguments provided"
                exit 1
            fi
            TARGET="$1"
            shift
            ;;
    esac
done

# trim whitespace
TARGET=$(echo "$TARGET" | xargs)

if [ -z "$TARGET" ]; then
    error "Argument cannot be empty"
    print_help
    exit 1
fi

if is_branch "$TARGET"; then
    info "the provided argument ('$TARGET') is a branch"

    # use current branch as default branch
    if [ -z "$DEFAULT_BRANCH" ]; then
        DEFAULT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
        info "the current branch ('$DEFAULT_BRANCH') is being used as the default branch"
    fi

    # special case
    if [ "$TARGET" = "$DEFAULT_BRANCH" ]; then
        warn "rebasing the default branch '$DEFAULT_BRANCH'"

        rebase_and_exit
    fi


    if is_local_branch "$TARGET"; then
        BRANCH="$TARGET"
        info "the provided branch ('$BRANCH') is local"
    else

        for remote in $(git remote); do
            if git show-ref --verify --quiet "refs/remotes/$remote/$TARGET"; then
                BRANCH="$remote/$TARGET"
                break
            fi
        done

        if [ -z "$BRANCH" ]; then
            error "Branch '$TARGET' does not exist locally or remotely"
            exit 1
        fi

        info "the provided branch ('$BRANCH') is remote"
    fi

    COMMIT=$(git merge-base "$DEFAULT_BRANCH" "$BRANCH")
    info "the merge base for the branch is '$COMMIT'"

elif is_commit "$TARGET"; then
    COMMIT=$(git rev-parse --verify "$TARGET^{commit}")
    info "the provided argument ('$TARGET') is a commit"
else
    error "'$TARGET' is not a valid commit hash or branch name"
    exit 1
fi

rebase_and_exit "$COMMIT"
