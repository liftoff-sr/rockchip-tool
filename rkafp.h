#ifndef _RKAFP_H
#define _RKAFP_H

#include <stdint.h>
#include <string.h>


struct UPDATE_PART {
    char         name[32];
    char         fullpath[60];
    uint32_t     nand_size;
    uint32_t     image_offset;
    uint32_t     nand_offset;
    uint32_t     image_size;
    uint32_t     file_size;
};


struct UPDATE_HEADER {
    char        magic[4];

#define RKAFP_MAGIC "RKAF"

    uint32_t    length;
    char        model[34];
    char        id[30];
    char        manufacturer[56];
    uint32_t    unknown1;
    uint32_t    version;

    uint32_t    num_parts;
    UPDATE_PART parts[16];

    char        reserved[116];

    UPDATE_HEADER()
    {
        memset( this, 0, sizeof(*this) );
    }
};


struct PARAM_HEADER {
    char        magic[4];
    uint32_t    length;
};

#endif // _RKAFP_H
