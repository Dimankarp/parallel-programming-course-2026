#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BEAR="bear --append -- "
CXX=${CXX:-g++}
CXXFLAGS="-O2 -std=c++20 -pthread"
OUTPUT="bench"

if [ "$1" == "--tsan" ]; then
    CXX="clang++"
    CXXFLAGS="-fsanitize=thread -g -O1 -std=c++20 -pthread"
    OUTPUT="bench_tsan"
    echo "Building with ThreadSanitizer: $OUTPUT..."
else
    echo "Building benchmark: $OUTPUT..."
fi

$BEAR $CXX $CXXFLAGS src/*.cpp -o "$OUTPUT"
echo "Build successful: $OUTPUT"
