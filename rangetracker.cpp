/*
Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "common.h"
#include "rangetracker.h"

#include <algorithm>

// ZIP file format constants
#define SIG_PK_CENTRAL 0x02014B50
#define SIG_PK_LOCAL 0x04034B50
#define READ_SIZE 0x60
#define TARGET_FILE_START 0x1FEC

#pragma pack(push, 1)
typedef struct {
    uint32_t signature;
    uint16_t version_made;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
    uint16_t comment_len;
    uint16_t disk_start;
    uint16_t int_attr;
    uint32_t ext_attr;
    uint32_t local_header_offset;
} zip_central_dir_header;
#pragma pack(pop)

void ConstantRangeTracker::ExtractRanges(std::vector<Range>* ranges) {
  ranges->push_back({ from, to });
}

SHMRangeTracker::SHMRangeTracker(char* name, size_t size) {
  shm.Open(name, size);
  data = (uint32_t *)shm.GetData();
  data[0] = 0;
  max_ranges = (size - sizeof(uint32_t)) / sizeof(uint32_t);
}

SHMRangeTracker::~SHMRangeTracker() {
  shm.Close();
}

void SHMRangeTracker::ExtractRanges(std::vector<Range>* ranges) {
  uint32_t* buf = data;
  size_t numranges = *buf; buf++;

  if (!numranges) return;

  if (numranges > max_ranges) {
    WARN("Number of ranges exceeds buffer size.");
    numranges = max_ranges;
  }

  std::vector<Range> tmpranges;
  tmpranges.resize(numranges);
  for (size_t i = 0; i < numranges; i++) {
    tmpranges[i].from = *buf; buf++;
    tmpranges[i].to = *buf; buf++;
  }

  ConsolidateRanges(tmpranges, *ranges);
}

void SHMRangeTracker::ConsolidateRanges(std::vector<Range>& inranges, std::vector<Range>& outranges) {
  if (inranges.empty()) return;

  std::sort(inranges.begin(), inranges.end());

  Range* lastrange = NULL;
  Range* currange = NULL;

  outranges.push_back(inranges[0]);

  for (size_t i = 1; i < inranges.size(); i++) {
    lastrange = &outranges[outranges.size() - 1];
    currange = &inranges[i];

    if (currange->from <= lastrange->to) {
      if (currange->to > lastrange->to) {
        lastrange->to = currange->to;
      }
    } else {
      outranges.push_back(*currange);
    }
  }

  // printf("SHMRangeTracker::ConsolidateRanges %zd %zd\n", inranges.size(), outranges.size());
}

// Helper function to validate if the data looks like a proper ZIP file - check for local file header at the beginning
bool IsZIPFile(const char* data, size_t size) {
  if (size < 4) return false;

  // Check for PK signature at the beginning (local file header)
  uint32_t sig = *(uint32_t *)data;
  return (sig == SIG_PK_LOCAL);
}

void ZIPRangeTracker::ExtractRanges(std::vector<Range>* ranges, const char* sample_data, size_t sample_size, bool validate_zip) {
  // Clear any existing ranges
  ranges->clear();

  if (sample_size < READ_SIZE) {
    return;
  }

  // Optionally validate that this looks like a ZIP file
  if (validate_zip && !IsZIPFile(sample_data, sample_size)) {
    return;
  }

  // Search in the last READ_SIZE bytes for the central directory signature
  size_t search_start = (sample_size >= READ_SIZE) ? sample_size - READ_SIZE : 0;

  for (size_t i = search_start; i <= sample_size - 4; i++) {
    uint32_t sig = *(uint32_t *)(sample_data + i);

    if (sig == SIG_PK_CENTRAL) {
      if (i + sizeof(zip_central_dir_header) <= sample_size) {
        zip_central_dir_header *hdr = (zip_central_dir_header *)(sample_data + i);

        // Calculate the target range inside the ZIP file
        size_t range_start = TARGET_FILE_START + hdr->filename_len;
        size_t range_end = TARGET_FILE_START + hdr->filename_len + hdr->uncompressed_size - 1;

        // Make sure the range is within the bounds of the sample
        if (range_start < sample_size && range_end < sample_size && range_start <= range_end) {
          Range range;
          range.from = range_start;
          range.to = range_end;
          ranges->push_back(range);
          // Found a range, no need to search further
          break;
        }
      }
    }
  }
}
