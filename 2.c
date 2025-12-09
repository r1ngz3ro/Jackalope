#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define SIG_PK_CENTRAL 0x02014B50
#define READ_SIZE 0x60
#define target_file_start 0x1FEc

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

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s file.zip\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    /* seek to end */
    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);
    printf("File length: 0x%lx bytes\n", file_len);

    if (file_len < READ_SIZE) {
        fprintf(stderr, "File too small\n");
        fclose(f);
        return 1;
    }

    /* read last 0x60 bytes */
    fseek(f, -READ_SIZE, SEEK_END);
    unsigned char buf[READ_SIZE];
    fread(buf, 1, READ_SIZE, f);

    /* search for signature */
    for (int i = 0; i <= READ_SIZE - 4; i++) {
        uint32_t sig = *(uint32_t *)(buf + i);

        if (sig == SIG_PK_CENTRAL) {
            zip_central_dir_header *hdr =
                (zip_central_dir_header *)(buf + i);

            printf("Found central directory header at offset -0x%x\n", READ_SIZE - i);
            printf("Uncompressed size: 0x%p bytes\n",
                   hdr->uncompressed_size);
	    printf("File name length: %u bytes\n", hdr->filename_len);
	    printf("target file range: %lx-%lx \n",target_file_start+hdr->filename_len, target_file_start +hdr->filename_len+ hdr->uncompressed_size - 1);


            fclose(f);
            return 0;
        }
    }

    printf("Central directory signature not found\n");
    fclose(f);
    return 0;
}

