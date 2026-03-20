#!/bin/bash

set -euo pipefail

git submodule update --init --recursive

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
