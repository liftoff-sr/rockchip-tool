#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <string.h>

#include "rkcrc.h"


#define MAGIC_CODE "KRNL"

const char* progname;

struct KRNL_HEADER
{
    char        magic[4];
    uint32_t    length;

    KRNL_HEADER()
    {
        memcpy( magic, MAGIC_CODE, 4 );
        length = 0;
    }
};


int pack_krnl( FILE* fp_in, FILE* fp_out )
{
    char buf[1024];
    KRNL_HEADER header;

    uint32_t crc = 0;

    fwrite( &header, sizeof(header), 1, fp_out );

    unsigned readlen;
    while( (readlen = fread( buf, 1, sizeof(buf), fp_in )) != 0 )
    {
        header.length += readlen;
        fwrite( buf, 1, readlen, fp_out );
        RKCRC( crc, buf, readlen );
    }

    fwrite( &crc, sizeof(crc), 1, fp_out );
    fseek( fp_out, 0, SEEK_SET );
    fwrite( &header, sizeof(header), 1, fp_out );

    printf( "%04X\n", crc );

    return 0;
}


int unpack_krnl( FILE* fp_in, FILE* fp_out )
{
    KRNL_HEADER header;
    char        buf[1024*16];

    fprintf( stderr, "unpacking..." );
    fflush( stderr );

    if( sizeof(header) != fread( &header, 1, sizeof(header), fp_in ) )
    {
        err( EXIT_FAILURE, "%s: cannot read header from input file", __func__ );
    }

    fseek( fp_in, header.length + sizeof(header), SEEK_SET );

    uint32_t    file_crc;

    if( sizeof(file_crc) != fread( &file_crc, 1, sizeof(file_crc), fp_in ) )
    {
        err( EXIT_FAILURE, "%s: cannot read crc from input file", __func__ );
    }

    size_t length = header.length;

    fseek( fp_in, sizeof(header), SEEK_SET );

    uint32_t crc = 0;

    unsigned readlen;
    while( length && (readlen = fread( buf, 1, sizeof(buf), fp_in )) != 0 )
    {
        length -= readlen;
        fwrite( buf, 1, readlen, fp_out );
        RKCRC( crc, buf, readlen );
    }

    if( file_crc != crc )
        fprintf( stderr, "WARNING: bad crc checksum\n" );

    fprintf( stderr, "OK\n" );
    return 0;
}


void help()
{
    fprintf( stderr, "usage: %s [-pack|-unpack] <input> <output>\n", progname );
    exit( EXIT_FAILURE );
}


int main( int argc, char** argv )
{
    progname = strrchr( argv[0], '/' );

    if( !progname )
        progname = argv[0];
    else
        ++progname;

    int     action = 0;

    if( argc != 4 )
    {
        help();
    }

    FILE* fp_in = fopen( argv[2], "rb" );

    if( !fp_in )
    {
        err( EXIT_FAILURE, "%s: can't open input file '%s'", __func__, argv[2] );
    }

    FILE* fp_out = fopen( argv[3], "wb" );

    if( !fp_out )
    {
        err( EXIT_FAILURE, "%s: can't open output file '%s'", __func__, argv[3] );
    }

    if( strcmp( argv[1], "-pack" ) == 0 )
    {
        pack_krnl( fp_in, fp_out );
    }
    else if( strcmp( argv[1], "-unpack" ) == 0 )
    {
        unpack_krnl( fp_in, fp_out );
    }
    else
    {
        help();
    }

    return 0;
}
