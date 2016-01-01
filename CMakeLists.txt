
if( DEFINED CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Set to either \"Release\" or \"Debug\"" )
else()
    set( CMAKE_BUILD_TYPE Release CACHE STRING "Set to either \"Release\" or \"Debug\"" )
endif()

project( rk-tools )

cmake_minimum_required( VERSION 2.8.12 FATAL_ERROR )

find_package( OpenSSL )

include_directories( ${OPENSSL_INCLUDE_DIR} )

set( CMAKE_C_FLAGS_DEBUG   "-g3 -ggdb3 -DDEBUG" )
set( CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb3 -DDEBUG" )


add_definitions( -DUSE_OPENSSL -Wno-unused-result )

add_executable( afptool
    afptool.cpp
    )


add_executable( img_maker
    img_maker.cpp
    )
target_link_libraries( img_maker
    ${OPENSSL_CRYPTO_LIBRARY}
    )


add_executable( img_unpack
    img_unpack.cpp
    )
target_link_libraries( img_unpack
    ${OPENSSL_CRYPTO_LIBRARY}
    )


add_executable( rkkernel
    rkkernel.cpp
    )


add_executable( rkcrc
    rkcrc.cpp
    )


install(
    TARGETS
        afptool img_maker img_unpack rkkernel rkcrc
    DESTINATION
        bin
    )
