# Optimization Report: Frame Sync Search Optimization

## Summary
- **Status**: PASS (reverted - no improvement)
- **Validation**: 73 pass, 9 fail - CORRECT
- **Baseline**: 1270.74 ms (23.6x real-time)
- **Result**: 1273.01 ms (23.6x real-time)
- **Delta**: +2.27 ms (SLOWER)
- **Improvement**: -0.18% (slight regression)

## Changes Attempted
Replaced the byte-by-byte frame sync search with a `memchr`-based approach:

1. **Original Implementation**: Uses `read_aligned_byte()` in a loop to read bytes one at a time through the bit buffer system, checking each byte for 0xFF and then validating the next byte.

2. **Optimized Implementation**:
   - Resets the bit buffer to work directly with the raw byte array
   - Uses `memchr()` to find 0xFF bytes (often SIMD-optimized in standard libraries)
   - Checks the next byte directly for a valid sync pattern (0xF8-0xFB)
   - Handles the 0xFF 0xFF edge case by continuing search from the second 0xFF

## Analysis

The `memchr` optimization showed a **slight regression** (2.27 ms slower) rather than an improvement. This is due to several factors:

### 1. Frame Sync is Almost Always at Buffer Start
In normal FLAC decoding, the decoder moves sequentially from frame to frame. After decoding one frame, the next frame's sync code (0xFF 0xF8-0xFB) is typically the very next bytes. The loop typically finds the sync pattern within 1-2 bytes.

### 2. Overhead of Buffer State Management
The `memchr` approach requires:
- Calling `reset_bit_buffer()` to synchronize the bit buffer with the raw buffer
- Pointer arithmetic to manage search positions
- Recalculating buffer indices after finding sync

This overhead exceeds the benefit of `memchr` when the search typically succeeds on the first iteration.

### 3. `read_aligned_byte()` is Already Efficient
The existing implementation uses `read_uint(8)` which is an inline function. For short searches, the function call overhead is minimal and the bit buffer likely already contains the needed bytes.

### 4. When Would `memchr` Help?
The `memchr` optimization would only help when searching through large amounts of non-frame data (e.g., recovering from stream corruption, or if there were non-audio data interspersed with frames). This is a rare error-recovery scenario, not the common case.

## Conclusion
This optimization was **not worth keeping** and has been reverted. The existing byte-by-byte implementation is already optimal for the common case where frames are properly aligned and sequential. The `memchr` approach adds overhead that outweighs its benefits for typical FLAC streams.
