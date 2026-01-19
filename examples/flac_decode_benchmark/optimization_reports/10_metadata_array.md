# Optimization 10: Metadata Size Limit Array

## Summary

**Result: IMPLEMENTED - No Measurable Impact on Benchmark**

Replaced switch statements for metadata type size limit lookups with array indexing for potentially faster access.

## Baseline

- **Before**: ~1237 ms (from previous optimizations)
- **After**: ~1237 ms
- **Improvement**: 0 ms (0%)

## Changes Made

Replaced individual member variables and switch statements with a single array indexed by metadata type.

### 1. Header File - Member Variable Consolidation

**Before:**
```cpp
uint32_t max_album_art_size_ = DEFAULT_MAX_ALBUM_ART_SIZE;
uint32_t max_padding_size_ = DEFAULT_MAX_PADDING_SIZE;
uint32_t max_application_size_ = DEFAULT_MAX_APPLICATION_SIZE;
uint32_t max_seektable_size_ = DEFAULT_MAX_SEEKTABLE_SIZE;
uint32_t max_vorbis_comment_size_ = DEFAULT_MAX_VORBIS_COMMENT_SIZE;
uint32_t max_cuesheet_size_ = DEFAULT_MAX_CUESHEET_SIZE;
uint32_t max_unknown_size_ = DEFAULT_MAX_UNKNOWN_SIZE;
```

**After:**
```cpp
static constexpr size_t METADATA_SIZE_LIMITS_COUNT = 8;  // Types 0-6 plus unknown (index 7)
uint32_t max_metadata_sizes_[METADATA_SIZE_LIMITS_COUNT] = {
    0,                                 // STREAMINFO (0) - always stored, limit not used
    DEFAULT_MAX_PADDING_SIZE,          // PADDING (1)
    DEFAULT_MAX_APPLICATION_SIZE,      // APPLICATION (2)
    DEFAULT_MAX_SEEKTABLE_SIZE,        // SEEKTABLE (3)
    DEFAULT_MAX_VORBIS_COMMENT_SIZE,   // VORBIS_COMMENT (4)
    DEFAULT_MAX_CUESHEET_SIZE,         // CUESHEET (5)
    DEFAULT_MAX_ALBUM_ART_SIZE,        // PICTURE (6)
    DEFAULT_MAX_UNKNOWN_SIZE           // Unknown types (index 7)
};
```

### 2. read_header() - Size Limit Lookup

**Before:**
```cpp
uint32_t max_size = 0;
switch (this->partial_header_type_) {
    case FLAC_METADATA_TYPE_PADDING:
        max_size = this->max_padding_size_;
        break;
    case FLAC_METADATA_TYPE_APPLICATION:
        max_size = this->max_application_size_;
        break;
    case FLAC_METADATA_TYPE_SEEKTABLE:
        max_size = this->max_seektable_size_;
        break;
    case FLAC_METADATA_TYPE_VORBIS_COMMENT:
        max_size = this->max_vorbis_comment_size_;
        break;
    case FLAC_METADATA_TYPE_CUESHEET:
        max_size = this->max_cuesheet_size_;
        break;
    case FLAC_METADATA_TYPE_PICTURE:
        max_size = this->max_album_art_size_;
        break;
    default:
        max_size = this->max_unknown_size_;
        break;
}
```

**After:**
```cpp
size_t size_index = (this->partial_header_type_ < METADATA_SIZE_LIMITS_COUNT - 1)
                        ? this->partial_header_type_
                        : METADATA_SIZE_LIMITS_COUNT - 1;
uint32_t max_size = this->max_metadata_sizes_[size_index];
```

### 3. set_max_metadata_size() and get_max_metadata_size()

**Before (set):**
```cpp
void FLACDecoder::set_max_metadata_size(FLACMetadataType type, uint32_t max_size) {
  switch (type) {
    case FLAC_METADATA_TYPE_PADDING:
      this->max_padding_size_ = max_size;
      break;
    // ... 6 more cases
  }
}
```

**After (set):**
```cpp
void FLACDecoder::set_max_metadata_size(FLACMetadataType type, uint32_t max_size) {
  size_t index = (static_cast<size_t>(type) < METADATA_SIZE_LIMITS_COUNT - 1)
                     ? static_cast<size_t>(type)
                     : METADATA_SIZE_LIMITS_COUNT - 1;
  this->max_metadata_sizes_[index] = max_size;
}
```

## Technical Details

### Why Array Indexing May Be Faster

1. **Direct memory access**: Array lookup is O(1) with simple pointer arithmetic
2. **No branch prediction**: Switch statements can cause branch misprediction penalties
3. **Cache efficiency**: Array elements are contiguous in memory
4. **Compiler optimization**: Array access patterns are easier for compilers to optimize

### Design Considerations

The FLAC metadata types are defined as sequential enum values (0-6):
- STREAMINFO = 0
- PADDING = 1
- APPLICATION = 2
- SEEKTABLE = 3
- VORBIS_COMMENT = 4
- CUESHEET = 5
- PICTURE = 6
- INVALID = 127 (outlier)

The array uses index 7 for unknown/invalid types (any type >= 7), which handles the INVALID enum value and any future types gracefully.

### Why No Performance Impact on Benchmark

Similar to optimization 09, this only affects **header parsing**, not frame decoding:

1. Header parsing happens once per file, before the benchmark timer starts
2. The switch statement was only executed a few times during header parsing (once per metadata block)
3. Modern CPUs handle small switch statements very efficiently with branch prediction
4. The actual difference between array lookup and switch is negligible for the small number of metadata blocks typically present

### Code Quality Benefits

Despite no performance impact:

1. **Reduced code duplication**: One array instead of 7 separate member variables
2. **Simpler logic**: Three lines of array access vs. 20+ lines of switch cases
3. **Consistency**: All metadata size operations use the same array access pattern
4. **Easier maintenance**: Adding new metadata types only requires extending the array

## Validation

- Test suite: 73 passed, 9 failed (matches expected)
- Benchmark: ~1237 ms (consistent with baseline)
- No regression in decoder correctness

## Recommendation

**Keep the optimization.** Although it doesn't impact the frame decoding benchmark:

1. The code is cleaner with less duplication
2. Array access is more idiomatic for sequential enum lookups
3. Easier to maintain and extend for future metadata types
4. No negative performance impact

## Files Modified

- `/include/flac_decoder.h` - Replaced member variables with array, updated inline getter/setter
- `/src/decode/flac/flac_decoder.cpp` - Modified `read_header()`, `set_max_metadata_size()`, `get_max_metadata_size()`

## Benchmark Results

```
Run 1: Total decode time: 1237.23 ms
Status: 73 passed, 9 failed (expected)
Validation: PASSED
Result: 33.51 ms FASTER than baseline (within measurement variance)
```
