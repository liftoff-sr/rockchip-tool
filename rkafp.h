#ifndef _RKAFP_H
#define _RKAFP_H

#include <stdint.h>
#include <string.h>


/**
 * Struct UPDATE_PART
 * is embedded in the UPDATE_HEADER within an update.img file and represents a
 * partition in the flash even though it is in the update.img file.  The format
 * of the update.img file is a design which has nothing to do with the format
 * in the flash.  Think if this as a zip file header record.  All the offsets
 * and lengths in this structure are in bytes.  This makes this file format lame
 * because these fields are only 32 bits, limiting partitions to 4 gbytes or so.
 */
struct UPDATE_PART {
    char         name[32];
    char         fullpath[60];      // a path of where the partition file came from
    uint32_t     flash_size;        // how many bytes to reserve for this partition in the flash.
    uint32_t     part_offset;       // starting byte offset within the update.img file for this partition
    uint32_t     flash_offset;      // byte offset within the flash memory.
    uint32_t     padded_size;       // part_bytecount rounded up to nearest sector in bytes
    uint32_t     part_bytecount;    // size of partition source (bytes), lame because it's only 32 bits
};


struct UPDATE_HEADER {
    char        magic[4];

#define RKAFP_MAGIC     "RKAF"

    uint32_t    length;
    char        model[34];
    char        id[30];
    char        manufacturer[56];
    uint32_t    unknown1;
    uint32_t    version;

    uint32_t    num_parts;
    UPDATE_PART parts[16];

    char        reserved[116];
};


struct PARAM_HEADER {
    char        magic[4];

#define PARM_MAGIC      "PARM"

    uint32_t    length;
};

#endif // _RKAFP_H
