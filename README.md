# Git Shuffle

A tool for editing Git history: conveniently reordering commits in a branch before publishing or merging.

## Building

```bash
# Quick build
./build.sh

# Manual build
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## Tools

After building, the following scripts are generated in the build directory:

- `tools/wrapper.sh` the rebase editor wrapper, intended to be used as `GIT_SEQUENCE_EDITOR`
- `tools/git_env.sh` sets `GIT_SEQUENCE_EDITOR` to the wrapper in the current shell

Alternatively, set the variable manually:

```bash
export GIT_SEQUENCE_EDITOR=/tools/wrapper.sh
```

## Documentation

- [User Documentation](./docs/README.md)
- [Developer Documentation](./docs/DEV.md)
