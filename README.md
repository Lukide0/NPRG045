# Git Shuffle

A tool for editing Git history: conveniently reordering commits in a branch before publishing or merging.

## Requirements

- Git
- CMake >= 3.25
- C++20 compiler
- Qt >= 6.8 with the following components installed: `Widgets`, `Core`, `Xml`

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

## Documentation

- [User Documentation](./docs/README.md)
- [Developer Documentation](./docs/DEV.md)
