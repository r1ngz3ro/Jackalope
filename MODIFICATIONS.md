# ZIP Range Tracking Feature Implementation

## Overview
This document describes the modifications made to the Jackalope fuzzer to integrate ZIP range tracking functionality directly into the mutator system. The feature allows the fuzzer to automatically detect ranges within ZIP files and focus mutations only on those specific ranges, significantly reducing mutation overhead.

## Changes Made

### 1. ZIPRangeTracker Class (rangetracker.h and rangetracker.cpp)
- Added `ZIPRangeTracker` class that can extract ranges from ZIP file content directly
- Implemented ZIP file signature detection and range extraction logic based on the `2.c` utility
- Added helper function `IsZIPFile()` to validate ZIP format before extracting ranges
- Used same constants and logic as the original `2.c` utility (SIG_PK_CENTRAL, TARGET_FILE_START=0x1FEC, etc.)

### 2. Command Line Options (fuzzer.h and fuzzer.cpp)
- Added `track_zip_ranges` boolean flag to enable ZIP range tracking
- Added `validate_zip_before_range_extraction` flag to control ZIP validation (defaults to true)
- Added command line parsing for both new options:
  - `-track_zip_ranges`: Enables ZIP-based range tracking
  - `-validate_zip_ranges`: Controls whether to validate ZIP files before extracting ranges

### 3. Sample Processing Integration (fuzzer.cpp)
- Modified `SaveSample()` function to extract ZIP ranges when `track_zip_ranges` is enabled
- Added range extraction logic that calls `ZIPRangeTracker::ExtractRanges()` with sample data
- Ensured ranges are stored with each sample in the queue system
- Updated `FuzzJob()` to pass ranges to mutator when either range tracking option is enabled

### 4. Range Tracker Creation (fuzzer.cpp)
- Modified `CreateRangeTracker()` to handle the new ZIP range tracking mode
- When only ZIP range tracking is enabled, returns a basic tracker without SHM overhead

## How It Works

### Range Detection
The `ZIPRangeTracker` analyzes the sample data to:
1. Locate ZIP central directory headers in the last 0x60 bytes of the file
2. Extract the embedded file's position using the central directory information
3. Calculate the target range as: `TARGET_FILE_START + filename_len` to `TARGET_FILE_START + filename_len + uncompressed_size - 1`
4. This matches the same range calculation as the original `2.c` utility: `target file range: %lx-%lx`

### Range-Based Mutation
When `-track_zip_ranges` is used:
1. Samples loaded from the corpus are analyzed for ZIP content
2. Ranges are extracted and stored with each sample in the queue
3. The RangeMutator focuses mutations only within the specified ranges
4. This reduces mutation overhead and focuses on the relevant portions of ZIP files

## Usage

### Command Line Options
```
-track_zip_ranges          Enable ZIP-based range tracking (only mutate in ZIP file ranges)
-validate_zip_ranges=0/1   Enable/disable ZIP validation before range extraction (default: 1)
```

### Example Usage
```
./fuzzer -in input_dir -out output_dir -nthreads 1 -track_zip_ranges -- target_program @@
```

### With validation disabled (for faster processing):
```
./fuzzer -in input_dir -out output_dir -nthreads 1 -track_zip_ranges -validate_zip_ranges=0 -- target_program @@
```

## Benefits

1. **Reduced Overhead**: Focuses mutations only on relevant ranges inside ZIP files
2. **Automatic Processing**: Range detection happens automatically when samples are loaded
3. **No External Dependencies**: Integrated directly into the fuzzer workflow
4. **Maintains Compatibility**: Preserves all existing functionality
5. **Performance**: Significantly reduces mutation overhead for ZIP-based targets

## Files Modified

- `rangetracker.h`: Added ZIPRangeTracker class declaration
- `rangetracker.cpp`: Implemented ZIP range extraction logic
- `fuzzer.h`: Added new member variables for tracking options
- `fuzzer.cpp`: Added option parsing and integration with sample processing
- `CMakeLists.txt`: Restored with original configuration

## Technical Details

The implementation mirrors the logic from the `2.c` utility:
- Searches for SIG_PK_CENTRAL (0x02014B50) signature in the last 0x60 bytes of the file
- Uses TARGET_FILE_START (0x1FEC) as the base offset for calculating target ranges
- Extracts filename_len and uncompressed_size from the central directory header
- Calculates target range as: `[TARGET_FILE_START + filename_len, TARGET_FILE_START + filename_len + uncompressed_size - 1]`

This allows the fuzzer to automatically identify and focus mutations on the actual content inside ZIP files, rather than randomly mutating the entire ZIP structure.