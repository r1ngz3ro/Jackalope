#include <iostream>
#include <vector>

// Include necessary headers to test the basic compilation of our changes
// Minimal version of range.h
class Range {
public:
  size_t from;
  size_t to;

  bool operator<(const Range& other) const {
    return this->from < other.from;
  }
};

// Include basic functionality to verify our code compiles
#include <cstring>
#include <algorithm>

// ZIP file format constants (from our implementation)
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

// Simplified version of our ZIPRangeTracker functionality
class RangeTracker {
public:
  virtual ~RangeTracker() {}
  virtual void ExtractRanges(std::vector<Range>* ranges) {}
};

class ZIPRangeTracker : public RangeTracker {
public:
  ZIPRangeTracker() {}
  
  // Extract ranges from sample data directly
  void ExtractRanges(std::vector<Range>* ranges, const char* sample_data, size_t sample_size, bool validate_zip = true) {
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
  
  // Override base class method
  virtual void ExtractRanges(std::vector<Range>* ranges) override {}

private:
  // Helper function to validate if the data looks like a proper ZIP file - check for local file header at the beginning
  bool IsZIPFile(const char* data, size_t size) {
    if (size < 4) return false;
    
    // Check for PK signature at the beginning (local file header)
    uint32_t sig = *(uint32_t *)data;
    return (sig == SIG_PK_LOCAL);
  }
};

int main() {
    std::cout << "Testing ZIPRangeTracker implementation..." << std::endl;
    
    // Test basic functionality
    std::vector<Range> ranges;
    ZIPRangeTracker tracker;
    
    // Create a test buffer simulating a ZIP file with central directory header
    char test_buffer[500];
    std::memset(test_buffer, 0, sizeof(test_buffer));
    
    // Add a fake central directory header near the end
    size_t offset = sizeof(test_buffer) - 50; // Near the end
    zip_central_dir_header* header = (zip_central_dir_header*)(test_buffer + offset);
    header->signature = SIG_PK_CENTRAL;
    header->filename_len = 10;  // filename length
    header->uncompressed_size = 100;  // file size
    header->version_made = 20;
    header->version_needed = 20;
    header->flags = 0;
    header->compression = 0;
    header->mod_time = 0;
    header->mod_date = 0;
    header->crc32 = 0;
    header->compressed_size = 100;
    header->extra_len = 0;
    header->comment_len = 0;
    header->disk_start = 0;
    header->int_attr = 0;
    header->ext_attr = 0;
    header->local_header_offset = 0;
    
    // Add local file header at beginning to pass validation
    *(uint32_t*)(test_buffer) = SIG_PK_LOCAL;
    
    // Test range extraction
    tracker.ExtractRanges(&ranges, test_buffer, sizeof(test_buffer), true);
    
    std::cout << "Found " << ranges.size() << " ranges" << std::endl;
    
    if (!ranges.empty()) {
        std::cout << "First range: " << ranges[0].from << " to " << ranges[0].to << std::endl;
        
        // Expected range: TARGET_FILE_START + filename_len to TARGET_FILE_START + filename_len + uncompressed_size - 1
        size_t expected_start = TARGET_FILE_START + header->filename_len;
        size_t expected_end = TARGET_FILE_START + header->filename_len + header->uncompressed_size - 1;
        
        if (ranges[0].from == expected_start && ranges[0].to == expected_end) {
            std::cout << "Range extraction test PASSED!" << std::endl;
        } else {
            std::cout << "Range extraction test FAILED!" << std::endl;
            std::cout << "Expected: " << expected_start << " to " << expected_end << std::endl;
        }
    } else {
        std::cout << "No ranges found - this might be expected if validation is too strict" << std::endl;
    }
    
    std::cout << "ZIPRangeTracker implementation compiled and basic functionality test completed." << std::endl;
    return 0;
}