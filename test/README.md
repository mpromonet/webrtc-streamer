# AAC Decoder Tests

This directory contains tests for the AAC audio decoder implementation.

## Requirements

- C++20 compiler (g++ or clang++)
- FFmpeg development libraries (libavcodec, libavutil)
- pkg-config

## Running Tests

### Quick Test

```bash
./build_and_test.sh
```

This will:
1. Check for FFmpeg availability
2. Compile the test with FFmpeg support (if available)
3. Run the AAC decoder initialization tests

Alternatively, if you prefer using make:
```bash
make test  # if Makefile is present (may be ignored by .gitignore)
```

### Manual Build

```bash
# With FFmpeg
g++ -std=c++20 -DHAVE_FFMPEG test_aac_decoder.cpp -o test_aac_decoder \
    $(pkg-config --cflags --libs libavcodec libavutil)

# Without FFmpeg (fallback test)
g++ -std=c++20 test_aac_decoder.cpp -o test_aac_decoder
```

### Clean

```bash
rm -f test_aac_decoder
```

## Test Coverage

The test verifies:
- AAC decoder can be initialized with different sample rates (16k, 32k, 44.1k, 48k Hz)
- Both mono and stereo configurations work
- FFmpeg AAC codec is available
- Decoder context can be created and opened
- Memory is properly managed (no leaks)

## Expected Output

With FFmpeg available:
```
=== AAC Decoder Test ===

Testing 48kHz Stereo... ✓ AAC decoder initialized: 48000Hz, 2 channels
PASS
Testing 44.1kHz Stereo... ✓ AAC decoder initialized: 44100Hz, 2 channels
PASS
Testing 48kHz Mono... ✓ AAC decoder initialized: 48000Hz, 1 channels
PASS
Testing 32kHz Stereo... ✓ AAC decoder initialized: 32000Hz, 2 channels
PASS
Testing 16kHz Mono... ✓ AAC decoder initialized: 16000Hz, 1 channels
PASS

✓ All tests passed!
```

Without FFmpeg:
```
AAC decoder test skipped - FFmpeg not available
Build with FFmpeg support to enable AAC decoding
```

## Notes

- This is a unit test for the AAC decoder initialization
- It does not test actual audio decoding (requires test AAC files)
- The test uses a simplified version of the decoder for testing purposes
- Full integration tests require the complete webrtc-streamer build environment
