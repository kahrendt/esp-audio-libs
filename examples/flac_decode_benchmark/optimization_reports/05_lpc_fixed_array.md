# Optimization Report: LPC Coefficients Fixed Array

## Summary
- **Status**: PASS
- **Validation**: 73 pass, 9 fail - CORRECT
- **Baseline**: 1274.93 ms (23.5x real-time)
- **Result**: 1270.74 ms (23.6x real-time)
- **Delta**: 4.19 ms (FASTER)
- **Improvement**: 0.33%

## Changes Made
Replaced dynamic `std::vector<int32_t>` allocation for LPC coefficients with a fixed-size stack array to eliminate heap allocation overhead during frame decoding.

### Files Modified

1. **`src/decode/flac/flac_lpc.h`**
   - Removed `#include <vector>` header
   - Changed function signatures for `restore_linear_prediction_32bit` and `restore_linear_prediction_64bit` from `const std::vector<int32_t> &coefs` to `const int32_t *coefs, std::size_t coefs_size`

2. **`src/decode/flac/flac_lpc.cpp`**
   - Replaced `#include <algorithm>` with `#include <cstdlib>` (for std::abs)
   - Updated both `restore_linear_prediction_*` functions to use `coefs` pointer and `coefs_size` parameters directly instead of calling `.data()` and `.size()` on a vector

3. **`src/decode/flac/flac_decoder.cpp`**
   - Changed `FIXED_COEFFICIENTS` from array of vectors to individual static arrays with a pointer array:
     ```cpp
     static const int32_t FIXED_COEFFICIENTS_1[] = {1};
     static const int32_t FIXED_COEFFICIENTS_2[] = {-1, 2};
     static const int32_t FIXED_COEFFICIENTS_3[] = {1, -3, 3};
     static const int32_t FIXED_COEFFICIENTS_4[] = {-1, 4, -6, 4};
     static const int32_t *const FIXED_COEFFICIENTS[] = {...};
     ```
   - In `decode_lpc_subframe`: Replaced `std::vector<int32_t> coefs` with `int32_t coefs[32]` (max LPC order per FLAC spec)
   - Updated all calls to `restore_linear_prediction_*` and `can_use_32bit_lpc` to use the new pointer+size interface

## Analysis
The optimization shows a small but measurable improvement of 4.20 ms (0.33% faster).

**Why the improvement is modest:**

1. **Stack allocation is fast**: Modern allocators, especially those on ESP32, have optimized small allocation paths. The vector only allocates up to 32 `int32_t` values (128 bytes), which is a relatively small allocation.

2. **Allocation frequency**: LPC subframes appear frequently in FLAC files (most audio frames use LPC encoding), so the allocation happens many times per file. However, the vector's internal allocator can often reuse the same memory region.

3. **ESP32 memory architecture**: The ESP32's heap allocator (esp-idf) is reasonably efficient for small, short-lived allocations, which limits the potential gains from this optimization.

4. **Overhead eliminated**:
   - No heap allocation/deallocation per LPC subframe
   - No vector constructor/destructor overhead
   - No resize operation with potential reallocation check
   - Direct pointer access instead of vector's bounds checking (in debug builds)

5. **Why not bigger gains**: The majority of decoding time is spent in the actual LPC prediction loop (multiply-accumulate operations), not in coefficient storage. The assembly-optimized `restore_linear_prediction_*_asm` functions do the heavy lifting.

## Conclusion
This optimization is worth keeping. While the 0.33% improvement is modest, it eliminates unnecessary heap allocation overhead with no downside - the code is simpler, uses less memory (no vector metadata overhead), and is more predictable in terms of memory usage. The fixed array approach also aligns better with the FLAC specification's guaranteed maximum LPC order of 32.
