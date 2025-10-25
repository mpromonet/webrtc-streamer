#!/bin/bash
# Build and run AAC decoder test

set -e

CXX=${CXX:-g++}
CXXFLAGS="-std=c++20 -Wall -Wextra"

# Check if FFmpeg is available
if pkg-config --exists libavcodec libavutil 2>/dev/null; then
    echo "Building AAC decoder test with FFmpeg support..."
    CXXFLAGS="$CXXFLAGS -DHAVE_FFMPEG"
    LIBS=$(pkg-config --cflags --libs libavcodec libavutil)
else
    echo "Building AAC decoder test without FFmpeg support..."
    LIBS=""
fi

# Build
$CXX $CXXFLAGS test_aac_decoder.cpp -o test_aac_decoder $LIBS

# Run
echo ""
echo "Running AAC decoder test..."
echo "=============================="
./test_aac_decoder
