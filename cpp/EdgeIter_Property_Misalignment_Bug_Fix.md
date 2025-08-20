# EdgeIter Property Misalignment Bug

## Bug Description

The GraphAr C++ `EdgeIter` class had a critical bug where property values would become misaligned during iteration, particularly when:

1. **Segmented traversal**: Starting iteration from a specific position (e.g., skipping to edge 2000+)
2. **Chunk boundary crossing**: When iteration crosses vertex-chunk or edge-chunk boundaries
3. **Direct property access**: Using `it.property<T>()` vs `(*it).property<T>()`

## Symptoms

- Property values (especially `creationDate`) would be out of sync with the expected order
- Different access patterns (`it.property()` vs `(*it).property()`) would return different values
- Runtime errors like "Failed to open ... part10/chunk0" when accessing non-existent chunk files
- AddressSanitizer errors in some cases

## Root Cause

The `EdgeIter` class maintains multiple state layers:

1. **Iterator State**: `vertex_chunk_index_` and `cur_offset_`
2. **Property Reader State**: Each `AdjListPropertyArrowChunkReader` has its own:
   - `vertex_chunk_index_` (internal chunk tracking)
   - `chunk_index_` (edge chunk within vertex chunk)
   - `seek_offset_` (position within chunk)
   - `chunk_table_` (cached chunk data)

The synchronization between these state layers was inconsistent:

### Issue 1: `operator++()`
When crossing chunk boundaries, the method would call `reader.next_chunk()` for property readers, but this could fail if the target chunk doesn't exist. Failed readers would retain stale state while the iterator moved forward.

### Issue 2: `property()` method
The method would call `reader.seek(cur_offset_)` without first ensuring the reader was positioned at the correct vertex chunk. If the reader was on a different chunk, seeking to `cur_offset_` would access the wrong data.

### Issue 3: `operator*()`
Similar to `property()`, this method didn't ensure property readers were synchronized to the correct vertex chunk before seeking.

## The Fix

### Key Changes Made

1. **Enhanced `property()` method**: Added `reader.seek_chunk_index(vertex_chunk_index_)` before seeking to ensure readers are on the correct chunk.

2. **Enhanced `operator*()` method**: Added chunk synchronization before seeking to ensure property readers are aligned.

3. **Improved `operator++()` error handling**: Replaced `reader.next_chunk()` calls with `reader.seek_chunk_index(vertex_chunk_index_)` for more robust synchronization when crossing boundaries.

### Code Changes

#### Before (Buggy)
```cpp
// In property() method
for (auto& reader : property_readers_) {
  reader.seek(cur_offset_);  // May be on wrong chunk!
  // ...
}

// In operator*() method  
for (auto& reader : property_readers_) {
  reader.seek(cur_offset_);  // May be on wrong chunk!
}

// In operator++() method
for (auto& reader : property_readers_) {
  reader.next_chunk();  // May fail and leave stale state
}
```

#### After (Fixed)
```cpp
// In property() method
for (auto& reader : property_readers_) {
  reader.seek_chunk_index(vertex_chunk_index_);  // Ensure correct chunk
  reader.seek(cur_offset_);
  // ...
}

// In operator*() method
for (auto& reader : property_readers_) {
  reader.seek_chunk_index(vertex_chunk_index_);  // Ensure correct chunk
  reader.seek(cur_offset_);
}

// In operator++() method
for (auto& reader : property_readers_) {
  reader.seek_chunk_index(vertex_chunk_index_);  // Robust synchronization
}
```

## Test Case

A comprehensive test was added to `test_graph.cc` that validates:

1. **Consistency between access patterns**: `it.property<T>()` matches `(*it).property<T>()`
2. **Segmented iteration consistency**: Results match sequential iteration regardless of starting position
3. **Cross-chunk boundary handling**: No crashes when crossing chunk boundaries
4. **Property alignment**: Property values remain correctly aligned with source/destination IDs

## Prevention

To prevent similar issues in the future:

1. **Always synchronize before seeking**: Any time you seek within a property reader, ensure it's on the correct chunk first
2. **Use `seek_chunk_index()` for reliability**: Prefer explicit chunk positioning over `next_chunk()` when possible
3. **Test edge cases**: Include tests that cross chunk boundaries and use different iteration patterns
4. **Validate consistency**: Test that different access methods return identical results

## Impact

This fix ensures that:
- Property values remain consistent across all iteration patterns
- No runtime crashes when crossing chunk boundaries  
- Direct property access matches edge object property access
- Segmented iteration produces identical results to sequential iteration