# AAC Audio Support

## Overview

webrtc-streamer now supports AAC audio decoding when FFmpeg is available. This allows streaming AAC audio from RTSP sources, MKV files, or other supported input formats to WebRTC clients.

## Requirements

AAC support requires FFmpeg development libraries:
- `libavcodec` (for AAC decoding)
- `libavutil` (for FFmpeg utilities)

### Installation

#### Ubuntu/Debian:
```bash
sudo apt-get install libavcodec-dev libavutil-dev
```

#### Fedora/RHEL:
```bash
sudo dnf install ffmpeg-devel
```

#### macOS (Homebrew):
```bash
brew install ffmpeg
```

## Build

When building webrtc-streamer, CMake will automatically detect FFmpeg:

```bash
cmake -B build -S .
cmake --build build
```

If FFmpeg is found, you'll see:
```
FFmpeg found - AAC audio decoding will be enabled
FFMPEG_FOUND = TRUE
```

If FFmpeg is not found:
```
FFmpeg not found - AAC audio decoding will be disabled
FFMPEG_FOUND = 
```

## Usage

AAC audio streams are automatically detected and decoded when present in the source. The codec is identified as `mpeg4-generic` in RTSP/SDP descriptions.

### Example RTSP source with AAC audio:

```bash
./webrtc-streamer -u rtsp://example.com/stream_with_aac_audio
```

### Example configuration with AAC:

```json
{
  "urls": {
    "mystream": {
      "video": "rtsp://camera.local/video",
      "audio": "rtsp://camera.local/audio"
    }
  }
}
```

## Supported AAC Configurations

- **Sample rates**: 8kHz, 16kHz, 24kHz, 32kHz, 44.1kHz, 48kHz
- **Channels**: Mono (1) and Stereo (2)
- **Format**: AAC-LC (Low Complexity) via MPEG4-GENERIC

## Technical Details

### Implementation

1. **AudioDecoderFactory**: Custom audio decoder factory that extends WebRTC's builtin factory
2. **AACDecoder**: FFmpeg-based decoder implementing `webrtc::AudioDecoder` interface
3. **Format Support**: Automatically handles AAC streams identified as "mpeg4-generic"

### FFmpeg API Compatibility

The implementation supports both:
- FFmpeg 5.0+ (using `av_channel_layout_default()`)
- FFmpeg 4.x (using deprecated `channels` and `channel_layout` fields)

### Build-time Configuration

AAC support is controlled by the `HAVE_FFMPEG` preprocessor definition:
- When FFmpeg is available: AAC decoder is compiled and available
- When FFmpeg is not available: Only WebRTC's builtin codecs (Opus, PCMU, PCMA, etc.) are supported

## Troubleshooting

### FFmpeg not detected

If FFmpeg is installed but not detected, ensure `pkg-config` can find it:

```bash
pkg-config --modversion libavcodec libavutil
```

If this fails, set `PKG_CONFIG_PATH`:

```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### Build errors

If you get linker errors related to FFmpeg, ensure both development headers and libraries are installed.

### Runtime issues

Enable verbose logging to see codec negotiations:

```bash
./webrtc-streamer -vv -u rtsp://your-stream
```

Look for log messages like:
```
AudioDecoderFactory: AAC/MPEG4-GENERIC codec supported
Creating AAC decoder for format: mpeg4-generic freq: 48000 channels: 2
AAC decoder initialized: 48000Hz, 2 channels
```

## Limitations

- Only AAC-LC (Low Complexity) profile is supported
- HE-AAC (High Efficiency) may have issues depending on FFmpeg version
- AAC+ (AAC with SBR) is supported but may require FFmpeg configuration

## See Also

- [WebRTC Streamer Documentation](../README.md)
- [FFmpeg AAC Decoder Documentation](https://ffmpeg.org/ffmpeg-codecs.html#aac)
