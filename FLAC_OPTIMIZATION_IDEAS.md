# FLAC Decoder Optimization Ideas

This document tracks potential optimizations for the FLAC decoder, based on code analysis.

## Overall Results

| Metric | Original | Final | Improvement |
|--------|----------|-------|-------------|
| **Decode Time** | 1425.52 ms | 1270.74 ms | **-154.78 ms (10.9% faster)** |
| **Real-time Factor** | 0.0475 | 0.0424 | Better |
| **Speed** | 21.0x | 23.6x | **+2.6x faster** |

---

## Completed Optimizations

### 1. Rice Unary Decoding with `__builtin_clz` ✅ KEPT
- **Location**: `src/decode/flac/flac_decoder.cpp`, `read_rice_sint` function
- **Change**: Replaced bit-by-bit loop with `__builtin_clz` to count leading zeros in one instruction
- **Result**: 1425.52 ms → 1348.46 ms (**-5.4%**)
- **Analysis**: Biggest win. This function is called thousands of times per frame for Rice-coded residuals. Using a single CPU instruction instead of a loop provides significant speedup.

### 2. UINT_MASK Array Replacement ✅ KEPT
- **Location**: `src/decode/flac/flac_decoder.cpp`, `read_uint` function
- **Change**: Replaced array lookup with bit manipulation: `(1U << num_bits) - 1`
- **Result**: 1348.46 ms → 1318.55 ms (**-2.2%**)
- **Analysis**: Bit manipulation is faster than memory access in tight loops. The inline function `uint_mask()` computes the mask directly.

### 3. Force Inline Critical Bit Buffer Functions ✅ KEPT
- **Location**: `src/decode/flac/flac_decoder.cpp`
- **Change**: Added `__attribute__((always_inline))` to hot-path functions:
  - `refill_bit_buffer()`
  - `read_uint(std::size_t num_bits)`
  - `read_sint(std::size_t num_bits)`
  - `read_rice_sint(uint8_t param)`
- **Result**: 1318.55 ms → 1279.82 ms (**-2.9%**)
- **Analysis**: Even though functions were marked `inline`, the compiler wasn't always inlining them. Forcing inline eliminates function call overhead for operations called thousands of times per frame.

### 4. Channel Decorrelation Loop Unrolling ✅ KEPT
- **Location**: `src/decode/flac/flac_decoder.cpp`, `decode_subframes` function
- **Change**: Unrolled loops for channel modes 8, 9, 10 to process 4 samples per iteration
- **Result**: 1279.82 ms → 1274.93 ms (**-0.4%**)
- **Analysis**: Small but consistent gain. Reduces loop overhead by 75%. The compiler may already partially unroll, limiting additional gains.

### 5. Replace std::vector with Fixed Array for LPC Coefficients ✅ KEPT
- **Location**: `src/decode/flac/flac_decoder.cpp`, `decode_lpc_subframe` function
- **Change**:
  - Replaced `std::vector<int32_t> coefs` with `int32_t coefs[32]`
  - Updated `FIXED_COEFFICIENTS` from vector array to pointer array
  - Changed `flac_lpc.h/cpp` interface from vector to pointer+size
- **Result**: 1274.93 ms → 1270.74 ms (**-0.3%**)
- **Analysis**: Eliminates heap allocation overhead per LPC subframe. Gain is modest because ESP32's allocator is already efficient for small allocations, but code is now simpler and more predictable.

### 6. Frame Sync Search Optimization ❌ REVERTED
- **Location**: `src/decode/flac/flac_decoder.cpp`, `find_frame_sync` function
- **Attempted Change**: Replace byte-by-byte search with `memchr`-based approach
- **Result**: 1270.74 ms → 1273.01 ms (**+0.2% regression**)
- **Analysis**: The optimization actually hurt performance because:
  1. Frame sync is typically found within 1-2 bytes (frames are sequential)
  2. Overhead of buffer state management outweighed `memchr` benefits
  3. `memchr` would only help during error recovery (rare case)
- **Conclusion**: Original implementation is already optimal for normal operation.

---

## Remaining Optimization Ideas (Not Yet Tested)

### 7. Batch Metadata Reading
- **Location**: `src/decode/flac/flac_decoder.cpp`, `read_header` function
- **Expected Impact**: LOW
- **Complexity**: LOW
- **Description**: Per-byte metadata reads push into vector. Could use `memcpy` from input buffer instead of `read_uint` calls for metadata blocks.
- **Note**: Only affects header parsing, not frame decoding. Low priority.

### 8. Sample Output Loop Optimization
- **Location**: `src/decode/flac/flac_decoder.cpp`, `write_samples_*` functions
- **Expected Impact**: MEDIUM
- **Complexity**: LOW
- **Description**: Multiple write_samples functions have inner loops iterating over channels and bytes. Create specialized fast paths or use SIMD-based byte shuffling/interleaving.

### 9. Pre-allocate Block Samples in read_header()
- **Location**: `src/decode/flac/flac_decoder.cpp`, `decode_frame` function
- **Expected Impact**: LOW
- **Complexity**: LOW
- **Description**: Block samples buffer check happens every frame. Pre-allocate in `read_header()` instead of lazily in `decode_frame()`.

### 10. Metadata Size Limit Array Instead of Switch
- **Location**: `src/decode/flac/flac_decoder.cpp`, `read_header` function
- **Expected Impact**: LOW
- **Complexity**: LOW
- **Description**: Large switch statement for metadata type checking could be replaced with array indexing.
- **Note**: Only affects header parsing, not frame decoding. Low priority.

---

## Assembly/Platform-Specific Optimizations (Higher Complexity)

### 11. Channel Decorrelation in Xtensa Assembly
- **Location**: `src/decode/flac/flac_decoder.cpp`, channel decorrelation loops
- **Expected Impact**: MEDIUM
- **Complexity**: HIGH
- **Description**: Simple arithmetic operations done in C++ could be vectorized in Xtensa assembly with loop unrolling and pipelining.
- **Note**: C++ loop unrolling already provides 0.4% gain. Assembly might add another 1-2%.

### 12. Extended LPC Assembly Optimization
- **Location**: `src/decode/flac/flac_lpc_asm.h` and assembly files
- **Expected Impact**: HIGH
- **Complexity**: HIGH
- **Description**: Assembly exists for orders 1-12 but could be optimized further. Add specialized case for order 1 (very common), use wider register widths for higher orders.

### 13. ESP32-S3 SIMD Investigation
- **Expected Impact**: VARIABLE
- **Complexity**: HIGH
- **Description**: Investigate if ESP32-S3 has additional SIMD capabilities not currently used. The S3 has vector extensions that could accelerate LPC and decorrelation.

### 14. CRC Calculation Assembly
- **Location**: `src/decode/flac/flac_crc.cpp`
- **Expected Impact**: LOW-MEDIUM
- **Complexity**: MEDIUM
- **Description**: Table-based CRC in C++ could be optimized in assembly with table prefetch hints and multi-byte processing.

---

## Key Insights from Optimization Work

1. **Focus on hot paths**: The biggest gains came from optimizing code that runs thousands of times per frame (Rice decoding, bit buffer reads).

2. **Compiler hints matter**: `always_inline` provided measurable gains (2.9%) even when functions were already marked `inline`.

3. **Not all optimizations help**: The frame sync optimization looked good on paper but actually hurt performance. Always measure!

4. **Cumulative small gains add up**: Four optimizations under 1% each still contributed ~4% combined.

5. **Measure, don't assume**: The predicted "MEDIUM" impact optimizations ranged from -5.4% to +0.2% in practice.

---

## Benchmark Infrastructure

Automated testing scripts are available in `examples/flac_decode_benchmark/`:

```bash
./benchmark.sh                    # Full validation + benchmark
./benchmark.sh --quick            # Skip validation, just benchmark
./benchmark.sh --no-clean         # Incremental build (faster)
./benchmark.sh --save-baseline    # Save current result as baseline
```

Detailed reports for each optimization are in `examples/flac_decode_benchmark/optimization_reports/`.
