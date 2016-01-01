#ifndef _RKAFP_H
#define _RKAFP_H

#include <stdint.h>

struct UPDATE_PART {
    char         name[32];
    char         filename[60];
    uint32_t     nand_size;
    uint32_t     pos;
    uint32_t     nand_addr;
    uint32_t     padded_size;
    uint32_t     size;
};


struct UPDATE_HEADER {
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
};


struct PARAM_HEADER {
    char        magic[4];
    uint32_t    length;
};

#endif // _RKAFP_H
