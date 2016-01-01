#ifndef _RKAFP_H
#define _RKAFP_H

#include <stdint.h>

typedef struct update_part {
    char         name[32];
    char         filename[60];
    uint32_t     nand_size;
    uint32_t     pos;
    uint32_t     nand_addr;
    uint32_t     padded_size;
    uint32_t     size;
} UPDATE_PART;


typedef struct update_header {
    char        magic[4];

#define RKAFP_MAGIC "RKAF"

    uint32_t    length;
    char        model[0x22];
    char        id[0x1e];
    char        manufacturer[0x38];
    uint32_t    unknown1;
    uint32_t    version;
    uint32_t    num_parts;

    UPDATE_PART parts[16];
    char        reserved[0x74];
} UPDATE_HEADER;


typedef struct param_header {
    char        magic[4];
    uint32_t    length;
} PARAM_HEADER;

#endif // _RKAFP_H
