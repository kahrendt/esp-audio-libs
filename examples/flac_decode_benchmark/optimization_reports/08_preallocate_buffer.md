# Optimization Report: Pre-allocate Block Samples Buffer

## Summary
- **Status**: REVERTED (No measurable improvement)
- **Validation**: 73 pass, 9 fail - CORRECT
- **Baseline**: 1237.09 ms
- **Result**: 1237.09 ms
- **Delta**: 0 ms (No change)
- **Improvement**: 0%

## Objective

Move the `block_samples_` buffer allocation from `decode_frame()` to `read_header()` to eliminate the per-frame null pointer check.

## Changes Attempted

### Original Code in `decode_frame()`:
```cpp
FLACDecoderResult FLACDecoder::decode_frame(const uint8_t *buffer, size_t buffer_length,
                                            uint8_t *output_buffer, uint32_t *num_samples) {
  // ... setup ...

  *num_samples = 0;

  if (!this->block_samples_) {
    this->block_samples_ = (int32_t *) FLAC_MALLOC(this->max_block_size_ * this->num_channels_ * sizeof(int32_t));
  }
  if (!this->block_samples_) {
    return FLAC_DECODER_ERROR_MEMORY_ALLOCATION_ERROR;
  }

  // ... rest of decode_frame ...
}
```

### Attempted Change:

**In `read_header()` (after STREAMINFO parsing):**
```cpp
  // Pre-allocate block_samples_ buffer now that we know max_block_size_ and num_channels_
  // This eliminates the per-frame null check in decode_frame()
  if (!this->block_samples_) {
    this->block_samples_ = (int32_t *) FLAC_MALLOC(this->max_block_size_ * this->num_channels_ * sizeof(int32_t));
    if (!this->block_samples_) {
      return FLAC_DECODER_ERROR_MEMORY_ALLOCATION_ERROR;
    }
  }

  this->reset_bit_buffer();
  return FLAC_DECODER_SUCCESS;
```

**In `decode_frame()` (replaced allocation with assert):**
```cpp
  *num_samples = 0;

  // block_samples_ is pre-allocated in read_header() after STREAMINFO is parsed
  assert(this->block_samples_ != nullptr);

  if (this->bytes_left_ == 0) {
```

## Analysis

The optimization was expected to be low impact from the start, and testing confirmed this:

### Why No Improvement?

1. **Single Allocation**: The buffer allocation only happens once (on the first call to `decode_frame()`). Subsequent calls just perform the null check.

2. **Branch Prediction**: Modern CPUs (including ESP32-S3) have branch predictors. After the first frame, the null check becomes a highly predictable "not taken" branch. The CPU likely predicts this correctly 100% of the time after the first decode.

3. **Minimal Overhead**: A single null pointer check per frame (288 frames total) adds approximately:
   - 1 load instruction
   - 1 compare instruction
   - 1 branch instruction (predicted)

   This is negligible compared to the ~4,295 microseconds average per-frame decode time.

4. **Measurement Resolution**: Even if there was a tiny improvement (e.g., <0.1 ms total), it would be below the measurement noise threshold.

### Calculation

If we assume the null check takes ~3 CPU cycles on ESP32-S3 at 240MHz:
- Time per check: 3 / 240,000,000 = 12.5 nanoseconds
- Total for 288 frames: 288 * 12.5 ns = 3.6 microseconds
- Percentage of total decode time: 3.6 us / 1,237,090 us = 0.0003%

This is far below any measurable improvement threshold.

## Benchmark Results

| Run | Time (ms) | Real-time Factor |
|-----|-----------|------------------|
| 1 | 1237.09 | 24.3x |
| 2 | 1237.09 | 24.3x |
| 3 | 1237.09 | 24.3x |

Results were perfectly consistent, confirming no measurable difference from baseline.

## Decision

**REVERTED** - The optimization adds code complexity (allocation in two places, or an assert that could confuse debugging) without providing any measurable benefit. The original lazy allocation pattern is clean, correct, and effectively free due to branch prediction.

## Lessons Learned

1. **Not all "obvious" optimizations help**: Moving work out of a hot loop sounds good, but when that work only happens once and subsequent iterations are essentially free due to branch prediction, there's no gain.

2. **Know your baseline**: The per-frame decode time of ~4.3 ms dwarfs any microsecond-level optimization to frame setup code.

3. **Focus on hot paths**: Previous optimizations targeted code running thousands of times per frame (Rice decoding, bit buffer reads). Those are where real gains exist.

4. **Measure first**: This optimization was correctly predicted to be low-impact, and testing confirmed it. Always measure before committing changes.
