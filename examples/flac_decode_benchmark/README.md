# FLAC Decode Benchmark for ESP32

Measures FLAC decoding performance on ESP32 devices. Reports per-frame timing statistics and Real-Time Factor (RTF).

## Features

- Embedded FLAC data (no filesystem required)
- Per-frame timing with min/max/avg/stddev
- Real-Time Factor (RTF) calculation
- Support for ESP32 and ESP32-S3

## Audio Source Preparation

The benchmark requires FLAC audio data embedded in a C header file. A placeholder file is included, but you need to generate the real one with actual audio.

### Recommended Source

Use public domain music from [Musopen on Archive.org](https://archive.org/details/MusopenCollectionAsFlac):
- Beethoven Symphony No. 3 "Eroica" - Czech National Symphony Orchestra
- Same source used by the micro-opus benchmark

### Steps

1. **Download a FLAC file** from Musopen or another public domain source

2. **Extract a 10-30 second clip** using ffmpeg:
   ```bash
   ffmpeg -i input.flac -ss 60 -t 30 -c:a flac -ar 44100 -sample_fmt s16 clip.flac
   ```

   Options:
   - `-ss 60`: Start at 60 seconds into the file
   - `-t 30`: Extract 30 seconds of audio
   - `-ar 44100`: Sample rate 44.1 kHz
   - `-sample_fmt s16`: 16-bit samples

3. **Generate the C header file**:
   ```bash
   python convert_flac.py -i clip.flac -o src/test_audio_flac.h -v test_audio_flac_data
   ```

## Building and Running

### PlatformIO

```bash
# Build and flash for ESP32 DevKit
pio run -e esp32dev -t upload -t monitor

# Build and flash for ESP32-S3
pio run -e esp32s3 -t upload -t monitor
```

### ESP-IDF

```bash
idf.py set-target esp32   # or esp32s3
idf.py build
idf.py flash monitor
```

## Expected Output

```
========================================
   FLAC Decode Benchmark for ESP32
========================================

Input data size: 524288 bytes (512.0 KB)
Reading FLAC header...

=== FLAC Stream Info ===
Sample rate: 44100 Hz
Channels: 2
Bit depth: 16
Max block size: 4096 samples
Total samples: 1323000
Audio duration: 30.00 seconds

Allocating output buffer: 16384 bytes

Starting decode benchmark...
Data to decode: 524246 bytes
  Decoded 100 frames...
  Decoded 200 frames...
  Decoded 300 frames...
End of stream reached.

=== Benchmark Results ===
Frames decoded: 323
Total decode time: 1234.56 ms

Per-frame timing:
  Min: 3200 us
  Max: 4100 us
  Avg: 3821.5 us
  Std: 187.3 us

=== Real-Time Factor ===
RTF: 0.0411
Status: FASTER than real-time (24.3x)

Benchmark complete.
```

## Interpreting Results

### Real-Time Factor (RTF)

RTF = decode_time / audio_duration

- **RTF < 1.0**: Faster than real-time (good)
- **RTF = 1.0**: Exactly real-time
- **RTF > 1.0**: Slower than real-time (cannot stream)

### Expected Performance

| Device | Clock | Expected RTF | Speedup |
|--------|-------|--------------|---------|
| ESP32 | 240 MHz | 0.07-0.12 | 8-15x |
| ESP32-S3 | 240 MHz | 0.03-0.04 | 25-35x |

Performance varies based on:
- FLAC compression level
- Block size
- Number of channels
- Bit depth

## Configuration

### sdkconfig.defaults

- CPU frequency: 240 MHz
- Watchdogs disabled (for accurate timing)
- PSRAM enabled (for boards with PSRAM)
- Main task stack: 8KB
- Log level: WARN (reduced overhead)

### Decoder Settings

The benchmark disables CRC checking for maximum speed:
```cpp
decoder.set_crc_check_enabled(false);
```

To benchmark with CRC checking enabled (more realistic for production), modify `main.cpp`.

## File Structure

```
flac_decode_benchmark/
├── src/
│   ├── CMakeLists.txt         # ESP-IDF component file
│   ├── main.cpp               # Benchmark code
│   └── test_audio_flac.h      # Generated FLAC data header
├── CMakeLists.txt             # ESP-IDF project file
├── convert_flac.py            # FLAC to C header converter
├── platformio.ini             # PlatformIO configuration
├── sdkconfig.defaults         # ESP-IDF settings
└── README.md                  # This file
```

## Troubleshooting

### "Failed to read FLAC header"
- Ensure you've generated the header file with valid FLAC data
- Check that the FLAC file is not corrupted

### "Failed to allocate output buffer"
- The ESP32 may not have enough RAM
- Try a smaller FLAC clip or reduce block size

### Very slow performance
- Verify CPU frequency is 240 MHz
- Check that optimization flags (-O2) are enabled
- Ensure you're not running in debug mode
