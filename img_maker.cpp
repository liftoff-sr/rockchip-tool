
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>

#include "rkrom.h"
#include "rkafp.h"
#include "md5.h"

static const char* progname;


/**
 * Function import_data
 *
 * @param infile which file to import
 */
unsigned import_data( const char* infile, void* head, size_t head_len, FILE* fp )
{
    unsigned readlen;
    char buffer[1024*16];

    FILE* in_fp = fopen( infile, "rb" );

    if( !in_fp )
        goto import_end;

    // read in the UPDATE_HEADER to a special place in caller.
    readlen = fread( head, 1, head_len, in_fp );

    if( readlen )
    {
        fwrite( head, 1, readlen, fp );
    }

    unsigned len;
    while( (len = fread( buffer, 1, sizeof(buffer), in_fp ) ) != 0 )
    {
        fwrite( buffer, 1, len, fp );
        readlen += len;
    }

import_end:

    if( in_fp )
        fclose( in_fp );

    return readlen;
}


void append_md5sum( FILE* fp )
{
    MD5_CTX md5_ctx;
    char buffer[1024*16];

    MD5_Init( &md5_ctx );

    fseek( fp, 0, SEEK_SET );

    unsigned len;
    while( (len = fread( buffer, 1, sizeof(buffer), fp ) ) != 0 )
    {
        MD5_Update( &md5_ctx, buffer, len );
    }

    MD5_Final( (unsigned char*) buffer, &md5_ctx );

    for( int i = 0; i < 16; ++i )
    {
        fprintf( fp, "%02x", buffer[i] );
    }
}


int pack_rom( int chiptype,
        const char* loader_filename,
        int majver,
        int minver,
        int subver,
        const char* image_filename,
        const char* outfile )
{
    time_t nowtime;
    struct tm local_time;

    RKFW_HEADER     rom_hdr;
    UPDATE_HEADER   rkaf_hdr;

    BOOTLOADER_HEADER    loader_hdr;

    rom_hdr.chip = chiptype;
    rom_hdr.version = ROM_VERSION(majver, minver, subver);

    if( chiptype == 0x50 )
    {
        rom_hdr.code = 0x01030000;
    }
    else if( chiptype == 0x60 )
    {
        rom_hdr.code = 0x01050000;
    }
    else if( chiptype == 0x70 )
    {
        rom_hdr.code = 0x01060000;
    }
    else if( chiptype == 0x80 )
    {
        rom_hdr.code = 0x01060000;
    }

    nowtime = time( NULL );
    localtime_r( &nowtime, &local_time );

    rom_hdr.year = local_time.tm_year + 1900;
    rom_hdr.month = local_time.tm_mon + 1;
    rom_hdr.day  = local_time.tm_mday;
    rom_hdr.hour = local_time.tm_hour;
    rom_hdr.minute = local_time.tm_min;
    rom_hdr.second = local_time.tm_sec;

    FILE* fp = fopen( outfile, "wb+" );

    if( !fp )
    {
        fprintf( stderr, "Can't open file %s\n, reason: %s\n", outfile, strerror( errno ) );
        goto pack_fail;
    }

    // temporarily fill the file with an incomplete header at its beginning.
    if( 1 != fwrite( &rom_hdr, sizeof(rom_hdr), 1, fp ) )
        goto pack_fail;

    printf( "rom version: %x.%x.%x\n",
            (rom_hdr.version >> 24) & 0xFF,
            (rom_hdr.version >> 16) & 0xFF,
            (rom_hdr.version) & 0xFFFF );

    printf( "build time: %d-%02d-%02d %02d:%02d:%02d\n",
            rom_hdr.year, rom_hdr.month, rom_hdr.day,
            rom_hdr.hour, rom_hdr.minute, rom_hdr.second );

    printf( "chip: %x\n", rom_hdr.chip );

    fprintf( stderr, "generate image...\n" );

    // open the boot loader, typically <uboot's_full_name>.bin
    rom_hdr.loader_length = import_data( loader_filename,
            &loader_hdr,
            sizeof(loader_hdr),
            fp );

    if( rom_hdr.loader_length < sizeof(loader_hdr) )
    {
        fprintf( stderr, "boot loader file '%s' is not long enough\n", loader_filename );
        goto pack_fail;
    }

    rom_hdr.image_offset = rom_hdr.loader_offset + rom_hdr.loader_length;
    rom_hdr.image_length = import_data( image_filename, &rkaf_hdr, sizeof(rkaf_hdr), fp );

    if( rom_hdr.image_length < sizeof(rkaf_hdr) )
    {
        fprintf( stderr, "invalid rom :\"\%s\"\n", image_filename );
        goto pack_fail;
    }

    rom_hdr.unknown2 = 1;

    rom_hdr.system_fstype = 0;

    int i;
    for( i = 0; i < rkaf_hdr.num_parts; ++i )
    {
        if( strcmp( rkaf_hdr.parts[i].name, "backup" ) == 0 )
            break;
    }

    if( i < rkaf_hdr.num_parts )
        rom_hdr.backup_endpos =
            (rkaf_hdr.parts[i].flash_offset + rkaf_hdr.parts[i].flash_size) / 0x800;
    else
        rom_hdr.backup_endpos = 0;

    fseek( fp, 0, SEEK_SET );

    if( 1 != fwrite( &rom_hdr, sizeof(rom_hdr), 1, fp ) )
        goto pack_fail;

    fprintf( stderr, "append md5sum...\n" );

    append_md5sum( fp );    // compute checksum on entire output file and append

    fclose( fp );
    fprintf( stderr, "success!\n" );

    return 0;

pack_fail:

    if( fp )
        fclose( fp );

    return -1;
}


void usage()
{
    fprintf( stderr, "USAGE:\n"
            "\t%s <chiptype> <loader> <major_version> <minor_version> <sub_version> <input_image> <out_image>\n\n"
            "Example for an RK32 board:\n"
            "\t%s -rk32 Loader.bin 4 4 0 rawimage.img rkimage.img\n"
            "\n"
            "Options:\n"
            "\t<chiptype>: -rk29 | -rk30 | -rk31 | -rk32\n",
            progname, progname
            );
}


int main( int argc, char** argv )
{
    progname = strrchr( argv[0], '/' );

    if( !progname )
        progname = argv[0];
    else
        ++progname;

    fprintf( stderr, "%s version: " __DATE__ "\n", progname );

    // loader, majorver, minorver, subver, oldimage, newimage
    if( argc == 8 )
    {
        if( strcmp( argv[1], "-rk29" ) == 0 )
        {
            pack_rom( 0x50, argv[2], atoi( argv[3] ), atoi( argv[4] ),
                    atoi( argv[5] ), argv[6], argv[7] );
        }
        else if( strcmp( argv[1], "-rk30" ) == 0 )
        {
            pack_rom( 0x60, argv[2], atoi( argv[3] ), atoi( argv[4] ),
                    atoi( argv[5] ), argv[6], argv[7] );
        }
        else if( strcmp( argv[1], "-rk31" ) == 0 )
        {
            pack_rom( 0x70, argv[2], atoi( argv[3] ), atoi( argv[4] ),
                    atoi( argv[5] ), argv[6], argv[7] );
        }
        else if( strcmp( argv[1], "-rk32" ) == 0 )
        {
            pack_rom( 0x80, argv[2], atoi( argv[3] ), atoi( argv[4] ),
                    atoi( argv[5] ), argv[6], argv[7] );
        }
        else
        {
            usage();
            return 1;
        }
    }
    else
    {
        usage();
        return 1;
    }

    return 0;
}
