
/*
 * Copyright original "C version" author: unknown
 * Copyright (C) 2016 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#include <stdio.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rkcrc.h"
#include "rkafp.h"

#if defined(DEBUG)
 #define D(x)   x
#else
 #define D(x)
#endif


const char* appname;



uint32_t filestream_crc( FILE* fs, size_t stream_len )
{
    char buffer[1024*16];

    uint32_t crc = 0;

    unsigned read_len;
    while( stream_len &&
            (read_len = fread( buffer, 1, sizeof(buffer), fs ) ) != 0 )
    {
        RKCRC( crc, buffer, read_len );
        stream_len -= read_len;
    }

    return crc;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// unpack functions

int create_dir( char* dir )
{
    char* sep = dir;

    while( ( sep = strchr( sep, '/' ) ) != NULL )
    {
        *sep = '\0';

        if( mkdir( dir, 0755 ) != 0 && errno != EEXIST )
        {
            printf( "Can't create directory: %s\n", dir );
            return -1;
        }

        *sep = '/';
        sep++;
    }

    return 0;
}


int extract_file( FILE* fp, off_t offset, size_t len, const char* path )
{
    char    buffer[1024*16];

    FILE*   fp_out = fopen( path, "wb" );

    if( !fp_out )
    {
        printf( "Can't open/create file: %s\n", path );
        return -1;
    }

    fseeko( fp, offset, SEEK_SET );

#if 1
    while( len )
    {
        size_t  ask = len < sizeof(buffer) ? len : sizeof(buffer);
        size_t  got = fread( buffer, 1, ask, fp );

        if( ask != got )
        {
            printf(
                "extraction of file '%s' is bad; insufficient length in container image file\n"
                "ask=%zu got=%zu\n",
                path,
                ask,
                got
                );
            break;
        }

        fwrite( buffer, got, 1, fp_out );
        len -= got;
    }
#else
     while( len )
     {
         size_t read_len = len < sizeof(buffer) ? len : sizeof(buffer);
         read_len = fread( buffer, 1, read_len, fp );

         if( !read_len )
             break;

         fwrite( buffer, read_len, 1, fp_out );
         len -= read_len;
     }

#endif

    fclose( fp_out );

    return 0;
}


int unpack_update( const char* srcfile, const char* dstdir )
{
    UPDATE_HEADER header;

    FILE* fp = fopen( srcfile, "rb" );

    if( !fp )
    {
        err( EXIT_FAILURE, "%s: can't open file '%s'", __func__, srcfile );
    }

    if( sizeof(header) != fread( &header, 1, sizeof(header), fp ) )
    {
        err( EXIT_FAILURE, "%s: can't read image header from file '%s'", __func__, srcfile );
    }

    if( strncmp( header.magic, RKAFP_MAGIC, sizeof(header.magic) ) != 0 )
    {
        err( EXIT_FAILURE, "%s: invalid header magic id in file '%s'", __func__, srcfile );
    }

    fseek( fp, 0, SEEK_END );

    unsigned filesize = ftell(fp);

    if( filesize - 4 < header.length )
    {
        printf( "update_header has length greater than file's length, cannot check crc\n" );
    }
    else
    {
        fseek( fp, header.length, SEEK_SET );

        uint32_t crc;

        unsigned readcount;
        readcount = fread( &crc, 1, sizeof(crc), fp );

        if( sizeof(crc) != readcount )
        {
            fprintf( stderr, "Can't read crc checksum, readcount=%d header.len=%u\n",
                readcount, header.length );
        }

        printf( "Checking file's crc..." );
        fflush( stdout );

        fseek( fp, 0, SEEK_SET );

        if( crc != filestream_crc( fp, header.length ) )
        {
            if( filesize-4 > header.length )
                fprintf( stderr, "CRC mismatch, however the file's size was bigger than header indicated\n");
            else
                err( EXIT_FAILURE, "%s: invalid crc for file '%s'", __func__, srcfile );
        }
        else
            printf( "OK\n" );
    }

    printf( "------- UNPACKING %d parts -------\n", header.num_parts );

    if( header.num_parts )
    {
        char dir[4096];

        for( unsigned i = 0; i < header.num_parts; i++ )
        {
            UPDATE_PART* part = &header.parts[i];

            printf( "%-32s0x%08x  0x%08x",
                    part->filename,
                    part->pos,
                    part->size
                    );

            D(printf( " (%-7u) padded_size:0x%08x (%-6u)  nand_size:0x%08x (%-6u)",
                part->size,
                part->padded_size,
                part->padded_size,
                part->nand_size,
                part->nand_size
                );)

            printf( "\n" );

            if( strcmp( part->filename, "SELF" ) == 0 )
            {
                printf( "Skip SELF file.\n" );
                continue;
            }

            if( memcmp( part->name, "parameter", 9 ) == 0 )
            {
                part->pos   += 8;
                part->size  -= 12;
            }

            snprintf( dir, sizeof(dir), "%s/%s", dstdir, part->filename );

            if( -1 == create_dir( dir ) )
                continue;

            if( part->pos + part->size > header.length )
            {
                fprintf( stderr, "Invalid part: %s\n", part->name );
                continue;
            }

            extract_file( fp, part->pos, part->size, dir );
        }
    }

    fclose( fp );

    return 0;

unpack_fail:

    if( fp )
    {
        fclose( fp );
    }

    return -1;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// pack functions

struct PACK_PART
{
    char        name[32];
    char        filename[60];
    uint32_t    nand_addr;
    uint32_t    nand_size;
};

struct PARTITION
{
    char        name[32];
    uint32_t    start;
    uint32_t    size;
};


struct PACK_IMAGE
{
    uint32_t    version;

    char        machine_model[0x22];
    char        machine_id[0x1e];
    char        manufacturer[0x38];

    uint32_t    num_package;
    PACK_PART   packages[16];

    uint32_t    num_partition;
    PARTITION   partitions[16];
};

static PACK_IMAGE package_image;

int parse_partitions( char* str )
{
    char*   parts;
    char*   part;
    char*   token1 = NULL;
    char*   ptr;

    PARTITION* p_part;

    int i;

    parts = strchr( str, ':' );

    if( parts )
    {
        *parts = '\0';
        parts++;
        part = strtok_r( parts, ",", &token1 );

        for( ; part; part = strtok_r( NULL, ",", &token1 ) )
        {
            p_part = &(package_image.partitions[package_image.num_partition]);

            p_part->size = strtol( part, &ptr, 16 );
            ptr = strchr( ptr, '@' );

            if( !ptr )
                continue;

            ptr++;
            p_part->start = strtol( ptr, &ptr, 16 );

            for( ; *ptr && *ptr != '('; ptr++ )
                ;

            for( i = 0, ptr++; i < sizeof(p_part->name) && *ptr && *ptr != ')'; i++, ptr++ )
            {
                p_part->name[i] = *ptr;
            }

            if( i < sizeof(p_part->name) )
                p_part->name[i] = '\0';
            else
                p_part->name[i - 1] = '\0';

            package_image.num_partition++;
        }

        for( i = 0; i < package_image.num_partition; ++i )
        {
            p_part = &package_image.partitions[i];
        }
    }

    return 0;
}


int action_parse_key( char* key, char* value )
{
    if( strcmp( key, "FIRMWARE_VER" ) == 0 )
    {
        unsigned a, b, c;
        sscanf( value, "%d.%d.%d", &a, &b, &c );
        package_image.version = (a << 24) + (b << 16) + c;
    }
    else if( strcmp( key, "MACHINE_MODEL" ) == 0 )
    {
        package_image.machine_model[sizeof(package_image.machine_model) - 1] = 0;
        strncpy( package_image.machine_model, value,
                sizeof(package_image.machine_model) );

        if( package_image.machine_model[sizeof(package_image.machine_model) - 1] )
            return -1;
    }
    else if( strcmp( key, "MACHINE_ID" ) == 0 )
    {
        package_image.machine_id[sizeof(package_image.machine_id) - 1] = 0;
        strncpy( package_image.machine_id, value,
                sizeof(package_image.machine_id) );

        if( package_image.machine_id[sizeof(package_image.machine_id) - 1] )
            return -1;
    }
    else if( strcmp( key, "MANUFACTURER" ) == 0 )
    {
        package_image.manufacturer[sizeof(package_image.manufacturer) - 1] = 0;
        strncpy( package_image.manufacturer, value,
                sizeof(package_image.manufacturer) );

        if( package_image.manufacturer[sizeof(package_image.manufacturer) - 1] )
            return -1;
    }
    else if( strcmp( key, "CMDLINE" ) == 0 )
    {
        char*   param, * token1 = NULL;
        char*   param_key, * param_value;


        param = strtok_r( value, " ", &token1 );

        while( param )
        {
            param_key = param;
            param_value = strchr( param, '=' );

            if( param_value )
            {
                *param_value = '\0';
                param_value++;

                if( strcmp( param_key, "mtdparts" ) == 0 )
                {
                    parse_partitions( param_value );
                }
            }

            param = strtok_r( NULL, " ", &token1 );
        }
    }

    return 0;
}


int parse_parameter( const char* fname )
{
    char    line[4096];
    char*   startp;
    char*   endp;
    char*   key;
    char*   value;
    FILE*   fp;

    if( ( fp = fopen( fname, "r" ) ) == NULL )
    {
        printf( "Can't open file: %s\n", fname );
        return -1;
    }

    while( fgets( line, sizeof(line), fp ) != NULL )
    {
        startp = line;
        endp = line + strlen( line ) - 1;

        if( *endp != '\n' && *endp != '\r' && !feof( fp ) )
            break;

        // trim line
        while( isspace( *startp ) )
            ++startp;

        while( isspace( *endp ) )
            --endp;

        endp[1] = 0;

        if( *startp == '#' || *startp == 0 )
            continue;

        key = startp;
        value = strchr( startp, ':' );

        if( !value )
            continue;

        *value = '\0';
        value++;

        action_parse_key( key, value );
    }

    if( !feof( fp ) )
    {
        printf( "parameter file has a very long line that I cannot handle!\n" );
        fclose( fp );
        return -3;
    }

    fclose( fp );

    return 0;
}


static PARTITION first_partition =
{
    "parameter",
    0,
    0x2000
};


PARTITION* find_partition_byname( const char* name )
{
    PARTITION* p_part;

    for( int i = package_image.num_partition - 1; i >= 0; i-- )
    {
        p_part = &package_image.partitions[i];

        if( strcmp( p_part->name, name ) == 0 )
            return p_part;
    }

    if( strcmp( name, first_partition.name ) == 0 )
    {
        return &first_partition;
    }

    return NULL;
}


PACK_PART* find_package_byname( const char* name )
{
    PACK_PART* p_pack;

    for( int i = package_image.num_partition - 1; i >= 0; i-- )
    {
        p_pack = &package_image.packages[i];

        if( strcmp( p_pack->name, name ) == 0 )
            return p_pack;
    }

    return NULL;
}


void append_package( const char* name, const char* path )
{
    PARTITION*   p_part;
    PACK_PART*   p_pack = &package_image.packages[package_image.num_package];

    strncpy( p_pack->name, name, sizeof(p_pack->name) );
    strncpy( p_pack->filename, path, sizeof(p_pack->filename) );

    p_part = find_partition_byname( name );

    if( p_part )
    {
        p_pack->nand_addr   = p_part->start;
        p_pack->nand_size   = p_part->size;
    }
    else
    {
        p_pack->nand_addr   = (unsigned) -1;
        p_pack->nand_size   = 0;
    }

    package_image.num_package++;
}


int get_packages( const char* fname )
{
    char    line[4096];
    char*   startp;
    char*   endp;
    char*   name;
    char*   path;

    FILE*   fp = fopen( fname, "r" );

    if( !fp )
    {
        printf( "Can't open file '%s'\n", fname );
        return -1;
    }

    while( fgets( line, sizeof(line), fp ) != NULL )
    {
        startp = line;
        endp = line + strlen( line ) - 1;

        if( *endp != '\n' && *endp != '\r' && !feof( fp ) )
            break;

        // trim line
        while( isspace( *startp ) )
            ++startp;

        while( isspace( *endp ) )
            --endp;

        endp[1] = 0;

        if( *startp == '#' || *startp == 0 )
            continue;

        name = startp;

        while( *startp && *startp != ' ' && *startp != '\t' )
            startp++;

        while( *startp == ' ' || *startp == '\t' )
        {
            *startp = '\0';
            startp++;
        }

        path = startp;

        append_package( name, path );
    }

    if( !feof( fp ) )
    {
        printf( "File '%s' has a long which is too long for me, sorry!\n", fname );
        fclose( fp );
        return -3;
    }

    fclose( fp );

    return 0;
}


int import_package( FILE* fp_out, UPDATE_PART* pack, const char* path )
{
    char    buf[2048];      // must be 2048 for param part
    size_t  readlen;

    pack->pos = ftell( fp_out );

    FILE*   fp_in = fopen( path, "rb" );

    if( !fp_in )
    {
        fprintf( stderr, "Cannot open input file '%s'\n", path );
        return -1;
    }

    if( strcmp( pack->name, "parameter" ) == 0 )
    {
        uint32_t crc = 0;
        PARAM_HEADER* header = (PARAM_HEADER*) buf;

        memcpy( header->magic, "PARM", sizeof(header->magic) );

        readlen = fread( buf + sizeof(*header), 1,
                        sizeof(buf) - sizeof(*header) - sizeof(crc), fp_in );

        header->length = readlen;
        RKCRC( crc, buf + sizeof(*header), readlen );

        readlen += sizeof(*header);

        memcpy( buf + readlen, &crc, sizeof(crc) );
        readlen += sizeof(crc);
        memset( buf + readlen, 0, sizeof(buf) - readlen );

        fwrite( buf, 1, sizeof(buf), fp_out );
        pack->size += readlen;
        pack->padded_size += sizeof(buf);
    }
    else
    {
        do {
            readlen = fread( buf, 1, sizeof(buf), fp_in );

            if( readlen == 0 )
                break;

            if( readlen < sizeof(buf) )
                memset( buf + readlen, 0, sizeof(buf) - readlen );

            fwrite( buf, 1, sizeof(buf), fp_out );
            pack->size += readlen;
            pack->padded_size += sizeof(buf);
        } while( !feof( fp_in ) );
    }

    fclose( fp_in );

    return 0;
}


void append_crc( FILE* fp )
{
    uint32_t crc = 0;
    off_t file_len = 0;

    fseeko( fp, 0, SEEK_END );
    file_len = ftello( fp );

    if( file_len == (off_t) -1 )
        return;

    fseek( fp, 0, SEEK_SET );

    printf( "Adding CRC...\n" );

    crc = filestream_crc( fp, file_len );

    fseek( fp, 0, SEEK_END );
    fwrite( &crc, 1, sizeof(crc), fp );
}


int pack_update( const char* srcdir, const char* dstfile )
{
    UPDATE_HEADER header;

    char    buf[4096];

    printf( "------ PACKAGE ------\n" );
    memset( &header, 0, sizeof(header) );

    snprintf( buf, sizeof(buf), "%s/%s", srcdir, "parameter" );

    if( parse_parameter( buf ) )
        return -1;

    snprintf( buf, sizeof(buf), "%s/%s", srcdir, "package-file" );

    if( get_packages( buf ) )
        return -1;

    FILE* fp = fopen( dstfile, "wb+" );

    if( !fp )
    {
        printf( "Can't open file \"%s\": %s\n", dstfile, strerror( errno ) );
        goto pack_failed;
    }

    fwrite( &header, sizeof(header), 1, fp );

    for( int i = 0; i < package_image.num_package; ++i )
    {
        strcpy( header.parts[i].name, package_image.packages[i].name );
        strcpy( header.parts[i].filename, package_image.packages[i].filename );
        header.parts[i].nand_addr   = package_image.packages[i].nand_addr;
        header.parts[i].nand_size   = package_image.packages[i].nand_size;

        if( strcmp( package_image.packages[i].filename, "SELF" ) == 0 )
            continue;

        snprintf( buf, sizeof(buf), "%s/%s", srcdir, header.parts[i].filename );
        printf( "Adding file: %s\n", buf );
        import_package( fp, &header.parts[i], buf );
    }

    memcpy( header.magic, "RKAF", sizeof(header.magic) );
    strcpy( header.manufacturer, package_image.manufacturer );
    strcpy( header.model, package_image.machine_model );
    strcpy( header.id, package_image.machine_id );

    header.length = ftell( fp );
    header.num_parts = package_image.num_package;
    header.version = package_image.version;

    for( int i = header.num_parts - 1; i >= 0; --i )
    {
        if( strcmp( header.parts[i].filename, "SELF" ) == 0 )
        {
            header.parts[i].size = header.length + 4;
            header.parts[i].padded_size = (header.parts[i].size + 511) / 512 * 512;
        }
    }

    fseek( fp, 0, SEEK_SET );
    fwrite( &header, sizeof(header), 1, fp );

    append_crc( fp );

    fclose( fp );

    printf( "------ OK ------\n" );

    return 0;

pack_failed:

    if( fp )
    {
        fclose( fp );
    }

    return -1;
}


void usage()
{
    printf( "USAGE:\n"
            "\t%s <-pack|-unpack> <Src> <Dest>\n\n"
            "Examples:\n"
            "\t%s -pack <src_dir> update.img\tpack files\n"
            "\t%s -unpack update.img <out_dir>\tunpack files\n",
            appname, appname, appname
            );
}


int main( int argc, char** argv )
{
    int ret = 0;

    appname = strrchr( argv[0], '/' );;

    if( appname )
        ++appname;
    else
        appname = argv[0];

    fprintf( stderr, "%s version: " __DATE__ "\n\n", appname );

    if( argc < 3 )
    {
        usage();
        return 1;
    }

    if( strcmp( argv[1], "-pack" ) == 0 && argc == 4 )
    {
        ret = pack_update( argv[2], argv[3] ) ;

        if( ret == 0 )
            printf( "Packed OK.\n" );
        else
            printf( "Packing failed!\n" );
    }
    else if( strcmp( argv[1], "-unpack" ) == 0 && argc == 4 )
    {
        ret = unpack_update( argv[2], argv[3] );

        if( ret == 0 )
            printf( "UnPacked OK.\n" );
        else
            printf( "UnPack failed!\n" );
    }
    else
    {
        usage();
        ret = 2;
    }

    return ret;
}
