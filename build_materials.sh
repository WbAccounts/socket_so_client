#!/bin/bash

set -e
set -x

ROOT_PATH=$(dirname $(readlink -f $0))
OUTPUT_PATH=$ROOT_PATH/../changed_files
FULLDATA_PATH=$OUTPUT_PATH/fulldata
STRIP_CMD="strip --strip-unneeded"
STRIP_OPT="-o"
OBJCOPY_CMD="objcopy"

SERVER_PATH=../socket_engine_server

common_src=(
    libSocketClientMgr.so
    $SERVER_PATH/libSocketServerMgr.so
)

common_dst=(
    Frameworks/libSocketClientMgr.so
    Frameworks/libSocketServerMgr.so
)

arm64_src=()
arm64_dst=()
mips64_src=()
mips64_dst=()
x64_src=()
x64_dst=()
sw64_src=()
sw64_dst=()
x86_src=()
x86_dst=()

function usage()
{
    echo "usage: bash $0 [version] [platform] [materials...]"
    exit 0
}

PLATFORM=`uname -m | sed "s/aarch64/arm64/g" | sed "s/mips64el/mips64/g" | sed "s/x86_64/x64/g" | sed "s/sw_64/sw64/g"`
VERSION=""
array_src=()
array_dst=()

function init_cross_compile()
{
    if [ x"$1" == x"$PLATFORM" ]; then return; fi
    case $1 in
        arm64)
            echo "cross compile $1 on $PLATFORM"
            PLATFORM=$1
            ;;
        mips64)
            echo "cross compile $1 on $PLATFORM"
            PLATFORM=$1
            export CROSS_COMPILE_TARGET=$1
            export ARCH=mips64el
            export CROSS_COMPILE_SYSROOT=/opt/cross-gcc-4.9.3-n64-loongson-rc6.1

            CROSS_TOOLS_PREFIX=mips64el-loongson-linux
            CROSS_QT_PATH_BASENAME=qt-4.8.6-for-mips64
            CROSS_QT_RPATH=$CROSS_COMPILE_SYSROOT/usr/lib
            CROSS_LD_LIBRARY_PATH=$CROSS_COMPILE_SYSROOT/usr/x86_64-unknown-linux-gnu/mips64el-loongson-linux/lib:$CROSS_COMPILE_SYSROOT/usr/lib

            CROSS_INCLUDEPATH=" -I$CROSS_COMPILE_SYSROOT/usr/include/"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/mips64el-loongson-linux/include/c++/4.9.3"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/mips64el-loongson-linux/include/c++/4.9.3/mips64el-loongson-linux"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/lib/gcc/mips64el-loongson-linux/4.9.3/include"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/include/glib-2.0"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/lib/glib-2.0/include"
            CROSS_LIBS=" -L$CROSS_COMPILE_SYSROOT/usr/lib"
            CROSS_LIBS+=" -L$CROSS_COMPILE_SYSROOT/usr/x86_64-unknown-linux-gnu/mips64el-loongson-linux/lib"
            CROSS_LIBS+=" -L$CROSS_COMPILE_SYSROOT/usr/mips64el-loongson-linux/lib64"
            CROSS_LIBS+=" -L$CROSS_COMPILE_SYSROOT/qt-4.8.6-for-mips64/lib"
            ;;
        x86)
            echo "cross compile $1 on $PLATFORM"
            PLATFORM=$1
            export CFLAGS=-march=i686
            export CXXFLAGS=-march=i686
            export CPPFLAGS=-march=i686
            ;;
        sw64)
            echo "cross compile $1 on $PLATFORM"
            PLATFORM=$1
            export CROSS_COMPILE_TARGET=$1
            export ARCH=sw_64
            export CROSS_COMPILE_SYSROOT=/usr/sw/swgcc710-cross

            CROSS_TOOLS_PREFIX=sw_64sw6a-sunway-linux-gnu
            CROSS_QT_PATH_BASENAME=qt-4.8.7-for-sw64
            CROSS_QT_RPATH=$CROSS_COMPILE_SYSROOT/qt-4.8.7-for-sw64/lib
            CROSS_LD_LIBRARY_PATH=$CROSS_COMPILE_SYSROOT/usr/lib:$CROSS_COMPILE_SYSROOT/lib

            CROSS_INCLUDEPATH=" -I$CROSS_COMPILE_SYSROOT/usr/include/"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/sw_64sw6a-sunway-linux-gnu/include"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/lib/gcc/sw_64sw6a-sunway-linux-gnu/7.1.0/include"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/include/c++/7.1.0/"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/include/c++/7.1.0/sw_64sw6a-sunway-linux-gnu"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/include/c++/7.1.0/sw_64sw6a-sunway-linux-gnu/bits"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/include/glib-2.0"
            CROSS_INCLUDEPATH+=" -I$CROSS_COMPILE_SYSROOT/usr/lib/glib-2.0/include"
            CROSS_LIBS=" -L$CROSS_COMPILE_SYSROOT/lib"
            CROSS_LIBS=" -L$CROSS_COMPILE_SYSROOT/usr/lib"
            CROSS_LIBS+=" -L$CROSS_COMPILE_SYSROOT/qt-4.8.7-for-sw64/lib"
            ;;
        *)
            ;;
    esac
    if [ x"$CROSS_COMPILE_SYSROOT" != x"" ];then
        GCC_BIN=$CROSS_COMPILE_SYSROOT/usr/$CROSS_TOOLS_PREFIX/bin
        QT_BIN=$CROSS_COMPILE_SYSROOT/$CROSS_QT_PATH_BASENAME/bin
        export PATH=$QT_BIN:$GCC_BIN:$CROSS_COMPILE_SYSROOT/usr/bin:$PATH

        export CC=${CROSS_TOOLS_PREFIX}-gcc
        export CXX=${CROSS_TOOLS_PREFIX}-g++
        export AR=${CROSS_TOOLS_PREFIX}-ar
        export RANLIB=${CROSS_TOOLS_PREFIX}-ranlib
        export NM=${CROSS_TOOLS_PREFIX}-nm
        export AS=${CROSS_TOOLS_PREFIX}-as
        export LD=${CROSS_TOOLS_PREFIX}-ld
        export STRIP=${CROSS_TOOLS_PREFIX}-strip

        export LD_LIBRARY_PATH=$CROSS_LD_LIBRARY_PATH:$LD_LIBRARY_PATH

        CFLAGS=$CROSS_INCLUDEPATH
        CFLAGS+=$CROSS_LIBS
        export CFLAGS=$CFLAGS
        export CPPFLAGS=$CFLAGS
        export CXXFLAGS=$CFLAGS

        export QT_INCLUDEPATH=$(echo $CROSS_INCLUDEPATH | sed "s/-I/ /g")
        export QT_LIBS=$CROSS_LIBS
        export QT_RPATH=$CROSS_QT_RPATH

        export CMAKE_CROSS_COMPILE_ARGS=" -DCMAKE_CXX_COMPILER=$GCC_BIN/g++ -DCMAKE_AR=$GCC_BIN/ar -DCMAKE_RANLIB=$GCC_BIN/ranlib -DCMAKE_LINKER=$GCC_BIN/ld "
    fi
}

function init_platform()
{
    for p in "arm64" "mips64" "x64" "sw64" "x86"
    do
        if [ x"$1" = x"$p" ]; then init_cross_compile $1; return; fi
    done
    echo "Platform format error, need [arm64|mips64|x64|sw64|x86]"
    usage
}

function init()
{
    if [ "`echo $1 | grep "[0-9][.][0-9][.][0-9][.][0-9]\{4\}" | wc -l`" -eq 1 ]; then
        VERSION=$1
        echo "Set version: $1"
    else
        echo "Version format error, need x.x.x.xxxx"
        usage
    fi

    init_platform $2

    rm -rf $OUTPUT_PATH/*

    array_src=${PLATFORM}_src
    eval array_src=$(echo "$"{$array_src[@]})
    array_src=( $array_src )
    for f in ${common_src[@]}
    do
        array_src[${#array_src[@]}]=$f
    done
    #echo ${array_src[*]}

    array_dst=${PLATFORM}_dst
    eval array_dst=$(echo "$"{$array_dst[@]})
    array_dst=( $array_dst )
    for f in ${common_dst[@]}
    do
        array_dst[${#array_dst[@]}]=$f
    done
    #echo ${array_dst[*]}
}

function build()
{ 
    for m in $@
    do
        cd $ROOT_PATH
        if [ "`make -f makefile_materials | grep $m | wc -l`" != "0" ]; then
            for (( i=0; i<${#array_dst[@]}; i++ ))
            do
                if [ "`echo ${array_dst[$i]} | grep $m | wc -l`" = "1" ]; then
                    make -f makefile_materials VERSION=$VERSION $m
                    copy_materials ${array_src[$i]} ${array_dst[$i]}
                    break
                fi
            done
        else
            echo "Unsupported parameter [$m]"
        fi
    done
}

function strip_cp_elf()
{
    if [ ! -d "`dirname $2`" ]; then
        mkdir -p "`dirname $2`"
    fi
    $STRIP_CMD "$1" $STRIP_OPT "$2"
}

function cp_elf()
{
    if [ ! -d "`dirname $2`" ]; then
        mkdir -p "`dirname $2`"
    fi
    cp -f "$1" "$2"
}

function copy_materials()
{
    materials_path=$OUTPUT_PATH/$PLATFORM
    fulldata_path=$FULLDATA_PATH/$PLATFORM
    strip_cp_elf $ROOT_PATH/$1 $materials_path/$2
    cp_elf $ROOT_PATH/$1 $fulldata_path/$2
}

function package()
{
    cd $OUTPUT_PATH
    zip -r $PACKAGE_NAME $PLATFORM
    rm -rf $PLATFORM
}

function run()
{
    init $@
    shift 2
    build $@
#    package
}

run $@
