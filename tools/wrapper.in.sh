#!/bin/sh

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <git-todo-file>" >&2
    exit 1
fi

GIT_SHUFFLE_BIN="${GIT_SHUFFLE_BIN:-$<TARGET_FILE:git_shuffle>}"

"$GIT_SHUFFLE_BIN" "$1"
