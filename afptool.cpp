
/*
 * original "C version" author is unknown
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
#include <string>
#include <vector>
#include <map>

#include "rkcrc.h"
#include "rkafp.h"
#include "rkrom.h"

#define VERSION     "Jan  4 2016"


#if defined(DEBUG)
 #define D(x)   x
#else
 #define D(x)
#endif


const char* appname;


/// Round up to nearest multiple of sector size of 512
inline unsigned round_up( unsigned aSize )
{
    return ((aSize + 511)/512) * 512;
}


uint32_t filestream_crc( FILE* fs, size_t stream_len )
{
    unsigned char buffer[1024*16];

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
            fprintf( stderr, "%s: can't create directory: %s\n", __func__, dir );
            return -1;
        }

        *sep++ = '/';
    }

    return 0;
}


int extract_file( FILE* fp, off_t offset, size_t len, const char* fullpath )
{
    char    buffer[1024*16];

    FILE*   fp_out = fopen( fullpath, "wb" );

    if( !fp_out )
    {
        fprintf( stderr, "%s: can't open/create file: %s\n", __func__, fullpath );
        return -1;
    }

    fseeko( fp, offset, SEEK_SET );

    while( len )
    {
        size_t  ask = len < sizeof(buffer) ? len : sizeof(buffer);
        size_t  got = fread( buffer, 1, ask, fp );

        if( ask != got )
        {
            fprintf( stderr,
                "extraction of file '%s' is bad; insufficient length in container image file\n"
                "ask=%zu got=%zu\n",
                fullpath,
                ask,
                got
                );
            break;
        }

        fwrite( buffer, got, 1, fp_out );
        len -= got;
    }

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
        fprintf( stderr,
            "%s: update_header has length greater than file's length, cannot check crc\n",
            __func__
            );
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
                    part->fullpath,
                    part->flash_offset,
                    part->part_bytecount
                    );

            printf( "\n" );

            if( !strcmp( part->fullpath, "SELF" ) )
            {
                printf( "Skip SELF file.\n" );
                continue;
            }

            if( !strcmp( part->fullpath, "RESERVED" ) )
            {
                printf( "Skip RESERVED file.\n" );
                continue;
            }

            if( memcmp( part->name, "parameter", 9 ) == 0 )
            {
                part->flash_offset   += sizeof(PARAM_HEADER);
                part->part_bytecount -= sizeof(PARAM_HEADER) + 4;    // CRC + PARM_HEADER
            }

            snprintf( dir, sizeof(dir), "%s/%s", dstdir, part->fullpath );

            if( -1 == create_dir( dir ) )
                continue;

            if( part->flash_offset + part->part_bytecount > header.length )
            {
                fprintf( stderr, "Invalid part: %s\n", part->name );
                continue;
            }

            extract_file( fp, part->flash_offset, part->part_bytecount, dir );
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

struct PACKAGE
{
    std::string     name;
    std::string     fullpath;

    PACKAGE( const char* aName = "", const char* aPath = "" ) :
        name( aName ),
        fullpath( aPath )
    {
    }

    void Show( FILE* fp )
    {
        fprintf( fp, "name:%-34s  fullpath:%s\n",
            name.c_str(), fullpath.c_str() );
    }
};


struct PARTITION
{
    std::string     name;
    unsigned        sector_start;   // at what starting sector (sector=512)
    unsigned        sector_count;   // how many 512 sectors

    PARTITION( const std::string& aName = "", unsigned aSectorStart = 0, unsigned aSectorCount = 0 ) :
        name( aName ),
        sector_start( aSectorStart ),
        sector_count( aSectorCount )
    {
        D( Show( stderr ); )
    }

    void Show( FILE* fp )
    {
        fprintf( fp, "%s: name:%-20s start:0x%08x size:0x%08x\n",
            __func__,
            name.c_str(), sector_start, sector_count );
    }
};


typedef std::vector<PACKAGE>    PACKAGES_BASE;
typedef std::vector<PARTITION>  PARTITIONS_BASE;

PARTITION FirstPartition( "parameter", 0, 0x2000 );


/**
 * Struct PARAMETERS
 * holds stuff from the parameter file.
 */
struct PARAMETERS
{
    unsigned        version;

    std::string     machine_model;
    std::string     machine_id;
    std::string     manufacturer;

    void Show( FILE* fp )
    {
        fprintf( fp, "version:%d.%d.%d  machine:%s  mfg:%s\n",
            version >> 24,  0xff & (version >> 16),  0xffff & version,
            machine_model.c_str(),
            manufacturer.c_str()
            );
    }
};


struct PACKAGES : public PACKAGES_BASE      // a std::vector
{
    int GetPackages( const char* package_file );

    void Show( FILE* fp )
    {
        fprintf( fp, "num_packages:%zu\n", size() );

        for( unsigned i=0; i < size();  ++i )
            (*this)[i].Show( fp );
    }

    PACKAGE* FindByName( const char* name )
    {
        for( unsigned i = 0; i < size(); ++i )
        {
            PACKAGE* pack = &(*this)[i];

            if( pack->name == name )
                return pack;
        }

        return NULL;
    }
};


struct PARTITIONS : public PARTITIONS_BASE  // a std::vector
{
    void Show( FILE* fp )
    {
        fprintf( fp, "num_partitions:%zu\n", size() );

        for( unsigned i=0;  i < size();  ++i )
            (*this)[i].Show( fp );
    }

    PARTITION* FindByName( const std::string& aName )
    {
        for( unsigned i=0;  i < size();  ++i )
        {
            PARTITION* part = &(*this)[i];

            if( part->name == aName )
                return part;
        }

        if( aName ==  FirstPartition.name )
        {
            return &FirstPartition;
        }

        return NULL;
    }

    int Parse( char* str );
};


PARAMETERS  Parameters;

PACKAGES    Packages;

PARTITIONS  Partitions;


int PARTITIONS::Parse( char* str )
{
    clear();

    char*   parts = strchr( str, ':' );

    if( parts )
    {
        char*   token1 = NULL;

        ++parts;

        char* tok = strtok_r( parts, ",", &token1 );

        for( ; tok; tok = strtok_r( NULL, ",", &token1 ) )
        {
            char*    ptr;
            unsigned sector_count = strtoul( tok, &ptr, 0 );

            ptr = strchr( ptr, '@' );

            if( !ptr )
                continue;

            ++ptr;

            unsigned sector_start = strtoul( ptr, &ptr, 0 );

            for( ; *ptr && *ptr != '('; ptr++ )
                ;

            ++ptr;

            char* name_start = ptr;

            int i;
            for( i = 0; i < sizeof( ((PARTITION*)0)->name) && *ptr && *ptr != ')'; ++i )
                ++ptr;

            std::string name( name_start, i );

            Partitions.push_back( PARTITION( name, sector_start, sector_count ) );
        }
    }

    return 0;
}


int action_parse_key( char* key, char* value )
{
    int ret = 0;

    if( strcmp( key, "FIRMWARE_VER" ) == 0 )
    {
        unsigned a, b, c;

        sscanf( value, "%d.%d.%d", &a, &b, &c );
        Parameters.version = ROM_VERSION( a, b, c );
    }
    else if( strcmp( key, "MACHINE_MODEL" ) == 0 )
    {
        Parameters.machine_model = value;
    }
    else if( strcmp( key, "MACHINE_ID" ) == 0 )
    {
        Parameters.machine_id = value;
    }
    else if( strcmp( key, "MANUFACTURER" ) == 0 )
    {
        Parameters.manufacturer = value;
    }
    else if( strcmp( key, "CMDLINE" ) == 0 )
    {
        char*   token1 = NULL;
        char*   param = strtok_r( value, " ", &token1 );

        while( param )
        {
            char*   param_key = param;
            char*   param_value = strchr( param, '=' );

            if( param_value )
            {
                *param_value++ = '\0';

                if( strcmp( param_key, "mtdparts" ) == 0 )
                {
                    ret = Partitions.Parse( param_value );
                }
            }

            param = strtok_r( NULL, " ", &token1 );
        }
    }

    return ret;
}


int parse_parameter( const char* fname )
{
    int     ret = 0;
    char    line[4096];

    FILE*   fp = fopen( fname, "r" );

    if( !fp )
    {
        printf( "Can't open file: %s\n", fname );
        return -1;
    }

    while( fgets( line, sizeof(line), fp ) != NULL )
    {
        char*   startp = line;
        char*   endp   = line + strlen( line ) - 1;

        if( *endp != '\n' && *endp != '\r' && !feof( fp ) )
        {
            printf( "parameter file has a very long line that I cannot handle!\n" );
            fclose( fp );
            ret = -3;
            break;
        }

        // trim line
        while( isspace( *startp ) )
            ++startp;

        while( isspace( *endp ) )
            --endp;

        endp[1] = 0;

        // skip UTF-8 BOM
        if( startp[0] == (char)0xEF && startp[1] == (char)0xBB && startp[2] == (char)0xBF)
            startp += 3;

        if( *startp == '#' || *startp == 0 )
            continue;

        char*   key   = startp;
        char*   value = strchr( startp, ':' );

        if( !value )
            continue;

        *value++ = '\0';

        action_parse_key( key, value );
    }

    fclose( fp );

    return 0;
}


int PACKAGES::GetPackages( const char* fname )
{
    int     ret = 0;
    char    line[4096];

    clear();

    FILE*   fp = fopen( fname, "r" );

    if( !fp )
    {
        fprintf( stderr, "%s: can't open file '%s'\n", __func__, fname );
        fprintf( stderr, "Every project needs a 'package-list' file in the source directory\n" );
        return -1;
    }

    while( fgets( line, sizeof(line), fp ) != NULL )
    {
        char*   startp = line;
        char*   endp = line + strlen( line ) - 1;

        if( *endp != '\n' && *endp != '\r' && !feof( fp ) )
        {
            fprintf( stderr,
                "File '%s' has a line which is too long for me, sorry!\n", fname );
            ret = -3;
            break;
        }

        // trim line
        while( isspace( *startp ) )
            ++startp;

        while( isspace( *endp ) )
            --endp;

        endp[1] = 0;

        if( *startp == '#' || *startp == 0 )
            continue;

        char* name = startp;

        while( *startp && *startp != ' ' && *startp != '\t' )
            startp++;

        while( *startp == ' ' || *startp == '\t' )
        {
            *startp = '\0';
            startp++;
        }

        char* path = startp;

        push_back( PACKAGE( name, path ) );
    }

    fclose( fp );

    return ret;
}


/**
 * Function import_package
 * copies an external file into this update image.
 */
int import_package( FILE* fp_update, UPDATE_PART* pack, const char* path )
{
    char    buf[2048];      // must be 2048 for param part
    size_t  readlen;

    pack->part_offset = ftell( fp_update );

    FILE*   fp_in = fopen( path, "rb" );

    if( !fp_in )
    {
        fprintf( stderr, "%s: cannot open input file '%s'\n", __func__, path );
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

        fwrite( buf, 1, sizeof(buf), fp_update );

        pack->part_bytecount  += readlen;
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

            fwrite( buf, 1, sizeof(buf), fp_update );
            pack->part_bytecount += readlen;
            pack->padded_size    += sizeof(buf);
        } while( !feof( fp_in ) );
    }

    fclose( fp_in );

    return 0;
}


void append_crc( FILE* fp )
{
    fseeko( fp, 0, SEEK_END );

    off_t file_len = ftello( fp );

    if( file_len == (off_t) -1 )
        return;

    fseek( fp, 0, SEEK_SET );

    printf( "Adding CRC...\n" );

    uint32_t crc = filestream_crc( fp, file_len );

    fseek( fp, 0, SEEK_END );
    fwrite( &crc, 1, sizeof(crc), fp );
}


typedef std::map< std::string, unsigned >   PAD_MAP;

static unsigned find_in_map( const std::string& aName, const PAD_MAP& aMap )
{
    PAD_MAP::const_iterator it = aMap.find( aName );

    if( it == aMap.end() )
        return 0;

    return it->second;
}


// convert bytes to 512 byte sectors
#define BYTES2SECTORS(x)      unsigned((uint64_t(x)+511)/512)

unsigned partition_padding( unsigned aSize, const std::string& aPartitionName )
{
    // tables of bootloader partion names and sizes

    // minimums:
    // partition may not be smaller than this.
    static const PAD_MAP minimums = {
        { "bootloader",     BYTES2SECTORS(16*1024*1024) },            // 1*1024*1024 = a megabyte
        { "boot",           BYTES2SECTORS(16*1024*1024) },

        // This is a hack for my 32 gbyte emmc, gives me a 6 gbyte swap
        // partition without having to supply an image file.
        { "swap",           BYTES2SECTORS(6*1024*1024*1024ULL) },     // 1024*1024*1024 = gigabyte
    };

    // paddings:
    // to add to the end of respective parition's input file size.
    static const PAD_MAP paddings = {
        { "bootloader",     BYTES2SECTORS(1*1024*1024) },
        { "recover-script", BYTES2SECTORS(1*1024*1024) },
        { "linuxroot",      BYTES2SECTORS(5*1024*1024) },
    };

    aSize += find_in_map( aPartitionName, paddings );

    unsigned minimum = find_in_map( aPartitionName, minimums );

    if( aSize < minimum )
        aSize = minimum;

    return aSize;           // return value is in 512 byte sized "sectors".
}


int compute_cmdline( const char* srcdir )
{
    char    buf[4096];

    snprintf( buf, sizeof(buf), "%s/%s", srcdir, "package-file" );

    if( Packages.GetPackages( buf ) )
        return -1;

    struct stat st;

    fprintf( stderr, "fragment for CMDLINE:\n" );

    printf( "mtdparts=rk29xxnand:" );

    // The contents of the package-list file drive this loop.
    // All offsets and sizes are in units of 512 bytes, i.e. a sector.

    unsigned flash_offset = 0x2000;     // start of flash allocation in sectors
    for( unsigned i=0; i < Packages.size();  ++i )
    {
        int failed = stat( Packages[i].fullpath.c_str(), &st );

        if( failed )
            st.st_size = 0;

        unsigned file_sectors = BYTES2SECTORS( st.st_size );

        file_sectors += partition_padding( file_sectors, Packages[i].name );

        if( failed &&
            Packages[i].fullpath != "RESERVED" &&
            Packages[i].name != "swap" )
        {
            fprintf( stderr,
                "%s: unable to open '%s' partition's file '%s'\n",
                __func__,
                Packages[i].name.c_str(),
                Packages[i].fullpath.c_str()
                );
            return -2;
        }

        unsigned cur_flash_sectors = file_sectors;      // partition size in sectors
        unsigned cur_flash_offset  = flash_offset;      // partition offset in sectors

        flash_offset += cur_flash_offset;

        if( i )
            printf( "," );

        D( Packages[i].Show( stderr ); )

        if( i == Packages.size()-1 )
        {
            // The last linux partition is set to expand on first boot using the
            // '-' size field, so make sure of this partition name in your
            // "package-file".  For linux it's sensibly "linuxroot".
            printf( "-@0x%x(%s)",
                cur_flash_offset,
                Packages[i].name.c_str()
                );
        }
        else
        {
            printf( "0x%x@0x%x(%s)",
                cur_flash_sectors,
                cur_flash_offset,
                Packages[i].name.c_str()
                );
        }
    }

    printf( "\n" );

    return 0;
}


int pack_update( const char* srcdir, const char* dstfile )
{
    int     ret = 0;
    char    buf[4096];

    printf( "------ PACKAGE ------\n" );

    snprintf( buf, sizeof(buf), "%s/%s", srcdir, "parameter" );

    if( parse_parameter( buf ) )
        return -1;

    snprintf( buf, sizeof(buf), "%s/%s", srcdir, "package-file" );

    if( Packages.GetPackages( buf ) )
        return -1;

    FILE* fp_update = fopen( dstfile, "wb+" );

    if( !fp_update )
    {
        fprintf( stderr, "Can't open file \"%s\": %s\n", dstfile, strerror( errno ) );
        return -1;
    }

    UPDATE_HEADER header;

    memset( &header, 0, sizeof(header) );

    // put out an inaccurate place holder, planning to come back later and update it.
    fwrite( &header, sizeof(header), 1, fp_update );

    unsigned i;
    for( i=0;  i < Packages.size() && i<16;  ++i )
    {
        if( Packages[i].name.size() > sizeof( header.parts[i].name ) )
        {
            fprintf( stderr, "%s: package name '%s' is too long by %zu bytes\n",
                __func__,
                Packages[i].name.c_str(),
                Packages[i].name.size() - sizeof( header.parts[i].name )
                );

            return -4;
        }

        if( Packages[i].fullpath.size( ) > sizeof( header.parts[i].fullpath ) )
        {
            fprintf( stderr, "%s: package fullpath '%s' is too long by %zu bytes\n",
                __func__,
                Packages[i].fullpath.c_str(),
                Packages[i].fullpath.size( ) - sizeof( header.parts[i].fullpath )
                );

            return -5;
        }

        strncpy( header.parts[i].name, Packages[i].name.c_str(), sizeof(header.parts[i].name) );
        strncpy( header.parts[i].fullpath, Packages[i].fullpath.c_str(), sizeof(header.parts[i].fullpath) );

        if( Packages[i].fullpath == "SELF" )
            continue;

        if( Packages[i].fullpath == "RESERVED" )
            continue;

        snprintf( buf, sizeof(buf), "%s/%s", srcdir, header.parts[i].fullpath );
        printf( "Adding bootloader partition: %-24s  using: %s\n", header.parts[i].name, buf );

        ret = import_package( fp_update, &header.parts[i], buf );
        if( ret )
        {
            break;
        }

        PARTITION* p = Partitions.FindByName( Packages[i].name );

        if( p )
        {
            header.parts[i].flash_offset = p->sector_start;
            header.parts[i].flash_size   = p->sector_count;
        }
        else
        {
            header.parts[i].flash_offset = ~0;
            header.parts[i].flash_size   = 0;
        }
    }

    memcpy( header.magic, "RKAF", sizeof(header.magic) );
    strncpy( header.manufacturer, Parameters.manufacturer.c_str(), sizeof(header.manufacturer) );
    strncpy( header.model, Parameters.machine_model.c_str(), sizeof(header.model) );
    strncpy( header.id, Parameters.machine_id.c_str(), sizeof(header.id) );

    header.length = ftell( fp_update );
    header.num_parts = i;
    header.version = Parameters.version;

    for( i = 0; i< header.num_parts; ++i )
    {
        if( strcmp( header.parts[i].fullpath, "SELF" ) == 0 )
        {
            header.parts[i].part_bytecount  = header.length + 4;
            header.parts[i].flash_size = round_up( header.parts[i].part_bytecount );
            break;
        }
    }

    fseek( fp_update, 0, SEEK_SET );
    fwrite( &header, sizeof(header), 1, fp_update );

    append_crc( fp_update );


    fclose( fp_update );

    printf( "------ OK ------\n" );

    return ret;
}


void usage()
{
    printf( "USAGE:\n"
            "\t%s <-pack | -unpack> <src_dir> <out_dir>\n"
            "\t\t or\n"
            "\t%s -CMDLINE <src_dir>\n\n"
            "Examples:\n"
            "\t%s -pack src_dir update.img\tpack files\n"
            "\t%s -unpack update.img out_dir\tunpack files\n"
            "\t%s -CMDLINE src_dir > cmdline\tcapture CMDLINE fragment into cmdline\n",
            appname, appname, appname, appname, appname
            );
}


int main( int argc, char** argv )
{
    int ret = EXIT_SUCCESS;

    appname = strrchr( argv[0], '/' );;

    if( appname )
        ++appname;
    else
        appname = argv[0];

    fprintf( stderr, "%s version: " VERSION "\n\n", appname );

    if( argc < 3 )
    {
        usage();
        return EXIT_FAILURE;
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

    else if( strcmp( argv[1], "-CMDLINE" ) == 0 && argc == 3 )
    {
        ret = compute_cmdline( argv[2] );
    }

    else
    {
        usage();
        ret = EXIT_FAILURE;
    }

    return ret;
}
