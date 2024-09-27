#!/bin/sh

set -e

#############
# ARGUMENTS #
#############

AVLC_RELEASE=$RELEASE
# Indicated if prebuilt contribs package
# should be created
AVLC_MAKE_PREBUILT_CONTRIBS=0
# Indicates that prebuit contribs should be
# used instead of building the contribs from source
AVLC_USE_PREBUILT_CONTRIBS=0
while [ $# -gt 0 ]; do
    case $1 in
        help|--help)
            echo "Use -a to set the ARCH"
            echo "Use --release to build in release mode"
            exit 1
            ;;
        a|-a)
            ANDROID_ABI=$2
            LIBVLC_ARCH="-a $2"
            shift
            ;;
        release|--release)
            AVLC_RELEASE=1
            LIBVLC_RELEASE="--release"
            ;;
        --package-contribs)
            AVLC_MAKE_PREBUILT_CONTRIBS=1
            LIBVLC_PACKAGE_CONTRIBS="--package-contribs"
            ;;
        --with-prebuilt-contribs)
            AVLC_USE_PREBUILT_CONTRIBS=1
            LIBVLC_PREBUILT_CONTRIBS="--with-prebuilt-contribs"
            ;;
    esac
    shift
done

# Validate arguments
if [ "$AVLC_MAKE_PREBUILT_CONTRIBS" -gt "0" ] &&
   [ "$AVLC_USE_PREBUILT_CONTRIBS" -gt "0" ]; then
    echo >&2 "ERROR: The --package-contribs and --with-prebuilt-contribs options"
    echo >&2 "       can not be used together."
    exit 1
fi

# Make in //
if [ -z "$MAKEFLAGS" ]; then
    UNAMES=$(uname -s)
    MAKEFLAGS=
    if which nproc >/dev/null; then
        MAKEFLAGS=-j$(nproc)
    elif [ "$UNAMES" = "Darwin" ] && which sysctl >/dev/null; then
        MAKEFLAGS=-j$(sysctl -n machdep.cpu.thread_count)
    fi
fi

#########
# FLAGS #
#########
if [ "${ANDROID_ABI}" = "arm" ] ; then
    ANDROID_ABI="armeabi-v7a"
elif [ "${ANDROID_ABI}" = "arm64" ] ; then
    ANDROID_ABI="arm64-v8a"
fi

# Set up ABI variables
if [ "${ANDROID_ABI}" = "x86" ] ; then
    TARGET_TUPLE="i686-linux-android"
    CLANG_PREFIX=${TARGET_TUPLE}
    PLATFORM_SHORT_ARCH="x86"
elif [ "${ANDROID_ABI}" = "x86_64" ] ; then
    TARGET_TUPLE="x86_64-linux-android"
    CLANG_PREFIX=${TARGET_TUPLE}
    PLATFORM_SHORT_ARCH="x86_64"
elif [ "${ANDROID_ABI}" = "arm64-v8a" ] ; then
    TARGET_TUPLE="aarch64-linux-android"
    CLANG_PREFIX=${TARGET_TUPLE}
    PLATFORM_SHORT_ARCH="arm64"
elif [ "${ANDROID_ABI}" = "armeabi-v7a" ] ; then
    TARGET_TUPLE="arm-linux-androideabi"
    CLANG_PREFIX="armv7a-linux-androideabi"
    PLATFORM_SHORT_ARCH="arm"
else
    echo "Please pass the ANDROID ABI to the correct architecture, using
                compile-libvlc.sh -a ARCH
    ARM:     (armeabi-v7a|arm)
    ARM64:   (arm64-v8a|arm64)
    X86:     x86, x86_64"
    exit 1
fi

# try to detect NDK version
REL=$(grep -o '^Pkg.Revision.*[0-9]*.*' $ANDROID_NDK/source.properties |cut -d " " -f 3 | cut -d "." -f 1)

if [ "$REL" = 26 ]; then
    ANDROID_API=21
else
    echo "NDK v26 needed, cf. https://developer.android.com/ndk/downloads/"
    exit 1
fi

############
# VLC PATH #
############
LIBVLCJNI_ROOT="$(cd "$(dirname "$0")/.."; pwd -P)"
# Fix path if the script is sourced from vlc-android
if [ -d $LIBVLCJNI_ROOT/libvlcjni ];then
    LIBVLCJNI_ROOT=$LIBVLCJNI_ROOT/libvlcjni
fi

if [ -f $LIBVLCJNI_ROOT/src/libvlc.h ];then
    VLC_SRC_DIR="$LIBVLCJNI_ROOT"
elif [ -f $PWD/src/libvlc.h ];then
    VLC_SRC_DIR="$PWD"
elif [ -d $LIBVLCJNI_ROOT/vlc ];then
    VLC_SRC_DIR=$LIBVLCJNI_ROOT/vlc
else
    echo "Could not find vlc sources"
    exit 1
fi

VLC_BUILD_DIR="$(cd $VLC_SRC_DIR/; pwd)/build-android-${TARGET_TUPLE}"

if [ -z $VLC_TARBALLS ]; then
    VLC_TARBALLS="$(cd $VLC_SRC_DIR/;pwd)/contrib/tarballs"
fi
if [ ! -d $VLC_TARBALLS ]; then
    mkdir -p $VLC_TARBALLS
fi

VLC_OUT_PATH="$VLC_BUILD_DIR/ndk"
mkdir -p $VLC_OUT_PATH

#################
# NDK TOOLCHAIN #
#################
host_tag=""
case $(uname | tr '[:upper:]' '[:lower:]') in
  linux*)   host_tag="linux" ;;
  darwin*)  host_tag="darwin" ;;
  msys*)    host_tag="windows" ;;
  *)        echo "host OS not handled"; exit 1 ;;
esac
NDK_TOOLCHAIN_DIR=${ANDROID_NDK}/toolchains/llvm/prebuilt/${host_tag}-x86_64
NDK_TOOLCHAIN_PATH=${NDK_TOOLCHAIN_DIR}/bin
# Add the NDK toolchain to the PATH, needed both for contribs and for building
# stub libraries
CROSS_TOOLS=${NDK_TOOLCHAIN_PATH}/llvm-
CROSS_CLANG=${NDK_TOOLCHAIN_PATH}/${CLANG_PREFIX}${ANDROID_API}-clang

export PATH="${NDK_TOOLCHAIN_PATH}:${PATH}"
NDK_BUILD=$ANDROID_NDK/ndk-build
if [ ! -z "$MSYSTEM_PREFIX" ] ; then
    # The make.exe and awk.exe from the toolchain don't work in msys
    export PATH="$MSYSTEM_PREFIX/bin:/usr/bin:${NDK_TOOLCHAIN_PATH}:${PATH}"
    NDK_BUILD=$NDK_BUILD.cmd
fi

##########
# CFLAGS #
##########

if [ "$NO_OPTIM" = "1" ];
then
     VLC_CFLAGS="-g -O0"
else
     VLC_CFLAGS="-g -O2"
fi

# cf. GLOBAL_CFLAGS from ${ANDROID_NDK}/build/core/default-build-commands.mk
VLC_CFLAGS="${VLC_CFLAGS} -fPIC -fdata-sections -ffunction-sections -funwind-tables \
 -fstack-protector-strong -no-canonical-prefixes"
VLC_CXXFLAGS="-fexceptions -frtti"

# Release or not?
if [ "$AVLC_RELEASE" = 1 ]; then
    VLC_CFLAGS="${VLC_CFLAGS} -DNDEBUG "
    NDK_DEBUG=0
else
    NDK_DEBUG=1
fi

###############
# DISPLAY ABI #
###############

echo "ABI:        $ANDROID_ABI"
echo "API:        $ANDROID_API"
echo "PATH:       $PATH"
echo "VLC_CFLAGS:        ${VLC_CFLAGS}"
echo "VLC_CXXFLAGS:      ${VLC_CXXFLAGS}"

if [ -z "$ANDROID_NDK" ]; then
    echo "Please set the ANDROID_NDK environment variable with its path."
    exit 1
fi

if [ -z "$ANDROID_ABI" ]; then
    echo "Please pass the ANDROID ABI to the correct architecture, using
                compile-libvlc.sh -a ARCH
    ARM:     (armeabi-v7a|arm)
    ARM64:   (arm64-v8a|arm64)
    X86:     x86, x86_64"
    exit 1
fi

avlc_checkfail()
{
    if [ ! $? -eq 0 ];then
        echo "$1"
        exit 1
    fi
}

avlc_find_modules()
{
    echo "$(find $1 -name 'lib*plugin.a' | grep -vE "lib(${blacklist_regexp})_plugin.a" | tr '\n' ' ')"
}

avlc_get_symbol()
{
    echo "$1" | grep vlc_entry_$2|cut -d" " -f 3
}

avlc_gen_pc_file()
{
    echo -n "Generating $2 pkg-config file: "
    echo $1/$(echo $2|tr 'A-Z' 'a-z').pc

    exec 3<> $1/$(echo $2|tr 'A-Z' 'a-z').pc

    [ ! -z "${PC_PREFIX}" ] &&
        echo "prefix=${PC_PREFIX}" >&3
    [ ! -z "${PC_LIBDIR}" ] &&
        echo "libdir=${PC_LIBDIR}" >&3
    [ ! -z "${PC_INCLUDEDIR}" ] &&
        echo "includedir=${PC_INCLUDEDIR}" >&3
    echo "" >&3

    echo "Name: $2" >&3
    echo "Description: $2" >&3
    echo "Version: $3" >&3
    echo "Libs: ${PC_LIBS} -l$2" >&3
    echo "Cflags: ${PC_CFLAGS}" >&3
    exec 3>&-
}

avlc_pkgconfig()
{
    # Enforce pkg-config files coming from VLC contribs
    PKG_CONFIG_PATH="$VLC_CONTRIB/lib/pkgconfig/" \
    PKG_CONFIG_LIBDIR="$VLC_CONTRIB/lib/pkgconfig/" \
    pkg-config "$@"
}

avlc_build()
{

$NDK_BUILD -C $LIBVLCJNI_ROOT/libvlc \
    APP_STL="c++_shared" \
    APP_CPPFLAGS="-frtti -fexceptions" \
    VLC_SRC_DIR="$VLC_SRC_DIR" \
    VLC_BUILD_DIR="$VLC_BUILD_DIR" \
    APP_BUILD_SCRIPT=jni/libvlcjni.mk \
    APP_PLATFORM=android-${ANDROID_API} \
    APP_ABI=${ANDROID_ABI} \
    NDK_PROJECT_PATH=jni \
    NDK_TOOLCHAIN_VERSION=clang \
    NDK_DEBUG=${NDK_DEBUG}
avlc_checkfail "ndk-build libvlcjni failed"

# Remove gdbserver to avoid conflict with libvlcjni.so debug options
rm -f $VLC_OUT_PATH/libs/${ANDROID_ABI}/gdb*

} # avlc_build()

if [ "$AVLC_SOURCED" != "1" ]; then
    avlc_build
fi
