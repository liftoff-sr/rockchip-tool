#ifndef RKROM
#define RKROM

#include <stdint.h>
#include <string.h>

#pragma pack(1)

// a as major version number, b as minor version number, c as revision number
#define ROM_VERSION(a,b,c)      (((a) << 24) + ((b) << 16) + (c))

#define RK_ROM_HEADER_CODE      "RKFWf"


struct RKFW_HEADER
{
    char        head_code[4];   // Fixed header "RKFW"

    uint16_t    head_len;               // 0x04
    uint32_t    version;                // 0x06 see ROM_VERSION()
    uint32_t    code;                   // 0x0a

    uint16_t    year;                   // 0x0e Creation date and time
    uint8_t     month;                  // 0x10
    uint8_t     day;                    // 0x11
    uint8_t     hour;                   // 0x12
    uint8_t     minute;                 // 0x13
    uint8_t     second;                 // 0x14

    uint32_t    chip;                   // 0x15

    uint32_t    loader_offset;          // 0x19
    uint32_t    loader_length;          // 0x1f

    uint32_t    image_offset;           // 0x23
    uint32_t    image_length;           // 0x27

    uint32_t    unknown1;               // 0x2b
    uint32_t    unknown2;               // 0x2f
    uint32_t    system_fstype;          // 0x31
    uint32_t    backup_endpos;          // 0x35

    uint8_t     reserved[0x66-0x39];    // 0x39

    RKFW_HEADER()
    {
        memset( this, 0, sizeof(RKFW_HEADER) );
        memcpy( head_code, "RKFW", 4 );
        head_len   =  sizeof( RKFW_HEADER );    // 0x66
        loader_offset = sizeof( RKFW_HEADER );  // 0x66
    }
};


struct BOOTLOADER_HEADER {
    char        magic[4];
    uint16_t    head_len;
    uint32_t    version;
    uint32_t    unknown1;

    uint16_t    build_year;
    uint8_t     build_month;
    uint8_t     build_day;
    uint8_t     build_hour;
    uint8_t     build_minute;
    uint8_t     build_second;

    /* 104 (0x68) bytes */

    uint32_t    chip;
};


#pragma pack()

#endif // RKROM
