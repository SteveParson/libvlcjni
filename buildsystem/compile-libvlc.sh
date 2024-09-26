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
# JNI build can be disabled for testing/CI purpose
AVLC_BUILD_JNI=1
while [ $# -gt 0 ]; do
    case $1 in
        help|--help)
            echo "Use -a to set the ARCH"
            echo "Use --release to build in release mode"
            exit 1
            ;;
        a|-a)
            ANDROID_ABI=$2
            shift
            ;;
        release|--release)
            AVLC_RELEASE=1
            ;;
        --package-contribs)
            AVLC_MAKE_PREBUILT_CONTRIBS=1
            ;;
        --with-prebuilt-contribs)
            AVLC_USE_PREBUILT_CONTRIBS=1
            ;;
        --no-jni)
            AVLC_BUILD_JNI=0
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
    echo "NDK v26 needed, got $REL, cf. https://developer.android.com/ndk/downloads/"
    exit 1
fi

############
# VLC PATH #
############
LIBVLCJNI_SRC_DIR="$(cd "$(dirname "$0")"; pwd -P)/.."
# Fix path if the script is sourced from vlc-android
if [ -d $LIBVLCJNI_SRC_DIR/libvlcjni ];then
    LIBVLCJNI_SRC_DIR=$LIBVLCJNI_SRC_DIR/libvlcjni
fi

if [ -f $LIBVLCJNI_SRC_DIR/src/libvlc.h ];then
    VLC_SRC_DIR="$LIBVLCJNI_SRC_DIR"
elif [ -f $PWD/src/libvlc.h ];then
    VLC_SRC_DIR="$PWD"
elif [ -d $LIBVLCJNI_SRC_DIR/vlc ];then
    VLC_SRC_DIR=$LIBVLCJNI_SRC_DIR/vlc
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
###########################
# VLC BOOTSTRAP ARGUMENTS #
###########################

VLC_CONTRIB_ARGS="\
    --disable-aribb24 \
    --disable-aribb25 \
    --disable-caca \
    --disable-chromaprint \
    --disable-dca \
    --disable-a52 \
    --disable-faad2 \
    --disable-fontconfig \
    --disable-goom \
    --disable-kate \
    --disable-libmpeg2 \
    --disable-mad \
    --disable-medialibrary \
    --disable-mpcdec \
    --disable-rav1e \
    --disable-samplerate \
    --disable-schroedinger \
    --disable-sidplay2 \
    --disable-srt \
    --disable-vnc \
    --disable-vncclient \
    --disable-x265 \
    --enable-ad-clauses \
    --enable-dvdnav \
    --enable-dvdread \
    --enable-fluidlite \
    --enable-gme \
    --enable-harfbuzz \
    --enable-jpeg \
    --enable-libarchive \
    --enable-libdsm \
    --enable-libplacebo \
    --enable-lua \
    --enable-microdns \
    --enable-mpg123 \
    --enable-nfs \
    --enable-smb2 \
    --enable-soxr \
    --enable-upnp \
    --enable-vorbis \
    --enable-vpx \
    --enable-zvbi \
"

###########################
# VLC CONFIGURE ARGUMENTS #
###########################

VLC_CONFIGURE_ARGS="\
    --disable-alsa \
    --disable-caca \
    --disable-dbus \
    --disable-dca \
    --disable-a52 \
    --disable-decklink \
    --disable-dv1394 \
    --disable-faad \
    --disable-fluidsynth \
    --disable-goom \
    --disable-jack \
    --disable-libva \
    --disable-linsys \
    --disable-mad \
    --disable-mtp \
    --disable-nls \
    --disable-notify \
    --disable-projectm \
    --disable-pulse \
    --disable-qt \
    --disable-samplerate \
    --disable-schroedinger \
    --disable-shared \
    --disable-sid \
    --disable-skins2 \
    --disable-svg \
    --disable-udev \
    --disable-update-check \
    --disable-v4l2 \
    --disable-vcd \
    --disable-vlc \
    --disable-vlm \
    --disable-vnc \
    --disable-xcb \
    --enable-avcodec \
    --enable-avformat \
    --enable-bluray \
    --enable-chromecast \
    --enable-dvbpsi \
    --enable-dvdnav \
    --enable-dvdread \
    --enable-fluidlite \
    --enable-gles2 \
    --enable-gme \
    --enable-jpeg \
    --enable-libass \
    --enable-libxml2 \
    --enable-live555 \
    --enable-lua \
    --enable-matroska \
    --enable-mod \
    --enable-mpg123 \
    --enable-opensles \
    --enable-opus \
    --enable-smb2 \
    --enable-sout \
    --enable-swscale \
    --enable-taglib \
    --enable-vorbis \
    --enable-zvbi \
    --with-pic \
"

########################
# VLC MODULE BLACKLIST #
########################

VLC_MODULE_BLACKLIST="
    access_(bd|shm|imem)
    addons.*
    alphamask
    aout_file
    audiobargraph_[av]
    audioscrobbler
    ball
    blendbench
    bluescreen
    clone
    dtstofloat32
    dynamicoverlay
    erase
    export
    fb
    gestures
    gradient
    grain
    hotkeys
    logger
    magnify
    mediadirs
    mirror
    mosaic
    motion
    motionblur
    motiondetect
    netsync
    oldrc
    osdmenu
    podcast
    posterize
    psychedelic
    puzzle
    real
    remoteosd
    ripple
    rss
    sap
    scene
    sharpen
    speex_resampler
    stats
    stream_filter_record
    t140
    visual
    wall
    yuv
    .dummy
"

###########################
# Build buildsystem tools #
###########################

export PATH="$VLC_SRC_DIR/extras/tools/build/bin:$PATH"
echo "Building tools"
(cd $VLC_SRC_DIR/extras/tools && ./bootstrap)
avlc_checkfail "buildsystem tools: bootstrap failed"
make -C $VLC_SRC_DIR/extras/tools $MAKEFLAGS
avlc_checkfail "buildsystem tools: make failed"

VLC_CONTRIB="$VLC_SRC_DIR/contrib/$TARGET_TUPLE"


#############
# BOOTSTRAP #
#############

if [ ! -f $VLC_SRC_DIR/configure ]; then
    echo "Bootstraping"
    (cd $VLC_SRC_DIR && ./bootstrap)
    avlc_checkfail "vlc: bootstrap failed"
fi

############
# Contribs #
############

echo "Building the contribs"

VLC_CONTRIB_DIR=$VLC_SRC_DIR/contrib/contrib-android-${TARGET_TUPLE}
VLC_CONTRIB_OUT_DIR=$VLC_SRC_DIR/contrib/${TARGET_TUPLE}

mkdir -p $VLC_CONTRIB_OUT_DIR/lib/pkgconfig
avlc_gen_pc_file $VLC_CONTRIB_OUT_DIR/lib/pkgconfig EGL 1.1
avlc_gen_pc_file $VLC_CONTRIB_OUT_DIR/lib/pkgconfig GLESv2 2

mkdir -p $VLC_CONTRIB_DIR/lib/pkgconfig

# TODO: VLC 4.0 won't rm config.mak after each call to bootstrap. Move it just
# before ">> config.make" when switching to VLC 4.0
rm -f $VLC_CONTRIB_DIR/config.mak

# gettext
which autopoint >/dev/null
if [ ! $? -eq 0 ];then
    VLC_CONTRIB_ARGS="$VLC_CONTRIB_ARGS --enable-gettext"
else
    VLC_CONTRIB_ARGS="$VLC_CONTRIB_ARGS --disable-gettext"
fi

(cd $VLC_CONTRIB_DIR && ANDROID_ABI=${ANDROID_ABI} ANDROID_API=${ANDROID_API} \
    ../bootstrap --host=${TARGET_TUPLE} ${VLC_CONTRIB_ARGS})
avlc_checkfail "contribs: bootstrap failed"

if [ "$AVLC_USE_PREBUILT_CONTRIBS" -gt "0" ]; then
    # Fetch prebuilt contribs
    if [ -z "$VLC_PREBUILT_CONTRIBS_URL" ]; then
        make -C $VLC_CONTRIB_DIR prebuilt
        avlc_checkfail "Fetching prebuilt contribs failed"
    else
        make -C $VLC_CONTRIB_DIR prebuilt PREBUILT_URL="$VLC_PREBUILT_CONTRIBS_URL"
        avlc_checkfail "Fetching prebuilt contribs from ${VLC_PREBUILT_CONTRIBS_URL} failed"
    fi
    # list packages to be built
    make -C $VLC_CONTRIB_DIR TARBALLS="$VLC_TARBALLS" list
    # build native tools
    make -C $VLC_CONTRIB_DIR TARBALLS="$VLC_TARBALLS" tools
else
    # Some libraries have arm assembly which won't build in thumb mode
    # We append -marm to the CFLAGS of these libs to disable thumb mode
    [ ${ANDROID_ABI} = "armeabi-v7a" ] && echo "NOTHUMB := -marm" >> $VLC_CONTRIB_DIR/config.mak

    echo "EXTRA_CFLAGS=${VLC_CFLAGS}" >> $VLC_CONTRIB_DIR/config.mak
    echo "EXTRA_CXXFLAGS=${VLC_CXXFLAGS}" >> $VLC_CONTRIB_DIR/config.mak
    echo "CC=${CROSS_CLANG}" >> $VLC_CONTRIB_DIR/config.mak
    echo "CXX=${CROSS_CLANG}++" >> $VLC_CONTRIB_DIR/config.mak
    echo "AR=${CROSS_TOOLS}ar" >> $VLC_CONTRIB_DIR/config.mak
    echo "AS=${CROSS_TOOLS}as" >> $VLC_CONTRIB_DIR/config.mak
    echo "RANLIB=${CROSS_TOOLS}ranlib" >> $VLC_CONTRIB_DIR/config.mak
    echo "LD=${CROSS_TOOLS}ld" >> $VLC_CONTRIB_DIR/config.mak
    echo "NM=${CROSS_TOOLS}nm" >> $VLC_CONTRIB_DIR/config.mak
    echo "STRIP=${CROSS_TOOLS}strip" >> $VLC_CONTRIB_DIR/config.mak

    # list packages to be built
    make -C $VLC_CONTRIB_DIR TARBALLS="$VLC_TARBALLS" list

    make -C $VLC_CONTRIB_DIR TARBALLS="$VLC_TARBALLS" $MAKEFLAGS fetch
    avlc_checkfail "contribs: make fetch failed"

    #export the PATH
    # Make
    make -C $VLC_CONTRIB_DIR TARBALLS="$VLC_TARBALLS" $MAKEFLAGS -k || make -C $VLC_CONTRIB_DIR TARBALLS="$VLC_TARBALLS" $MAKEFLAGS -j1
    avlc_checkfail "contribs: make failed"

    # Make prebuilt contribs package
    if [ "$AVLC_MAKE_PREBUILT_CONTRIBS" -gt "0" ]; then
        make -C $VLC_CONTRIB_DIR package
        avlc_checkfail "Creating prebuilt contribs package failed"
    fi
fi

mkdir -p $VLC_BUILD_DIR

#############
# CONFIGURE #
#############

if [ ${ANDROID_API} -lt "26" ]; then
    # android APIs < 26 have empty sys/shm.h headers that triggers shm detection but it
    # doesn't have any shm functions and/or symbols. */
    export ac_cv_header_sys_shm_h=no
fi

# always use fixups for search.h and tdestroy
export ac_cv_header_search_h=no
export ac_cv_func_tdestroy=no
export ac_cv_func_tfind=no

if [ ! -e $VLC_BUILD_DIR/config.h -o "$AVLC_RELEASE" = 1 ]; then
    VLC_CONFIGURE_DEBUG=""
    if [ ! "$AVLC_RELEASE" = 1 ]; then
        VLC_CONFIGURE_DEBUG="--enable-debug --disable-branch-protection"
    fi

    BEFORE_VLC_BUILD_DIR=$(pwd -P)
    cd $VLC_BUILD_DIR
    CFLAGS="${VLC_CFLAGS}" \
    CXXFLAGS="${VLC_CFLAGS} ${VLC_CXXFLAGS}" \
    CC="${CROSS_CLANG}" \
    CXX="${CROSS_CLANG}++" \
    NM="${CROSS_TOOLS}nm" \
    STRIP="${CROSS_TOOLS}strip" \
    RANLIB="${CROSS_TOOLS}ranlib" \
    AR="${CROSS_TOOLS}ar" \
    AS="${CROSS_TOOLS}as" \
    PKG_CONFIG_LIBDIR=$VLC_SRC_DIR/contrib/$TARGET_TUPLE/lib/pkgconfig \
    ../configure --host=$TARGET_TUPLE \
        --with-contrib=${VLC_SRC_DIR}/contrib/${TARGET_TUPLE} \
        --prefix=${VLC_BUILD_DIR}/install/ \
        ${EXTRA_PARAMS} ${VLC_CONFIGURE_ARGS} ${VLC_CONFIGURE_DEBUG}
    cd "$BEFORE_VLC_BUILD_DIR"
    avlc_checkfail "vlc: configure failed"
fi

############
# BUILDING #
############

echo "Building"
make -C $VLC_BUILD_DIR $MAKEFLAGS
avlc_checkfail "vlc: make failed"
make -C $VLC_BUILD_DIR install
avlc_checkfail "vlc: make install failed"

##################
# libVLC modules #
##################

REDEFINED_VLC_MODULES_DIR=${VLC_BUILD_DIR}/install/lib/vlc/plugins
rm -rf ${REDEFINED_VLC_MODULES_DIR}
mkdir -p ${REDEFINED_VLC_MODULES_DIR}

echo "Generating static module list"
blacklist_regexp=
for i in ${VLC_MODULE_BLACKLIST}
do
    if [ -z "${blacklist_regexp}" ]
    then
        blacklist_regexp="${i}"
    else
        blacklist_regexp="${blacklist_regexp}|${i}"
    fi
done

VLC_MODULES=$(avlc_find_modules ${VLC_BUILD_DIR}/modules)
DEFINITION="";

SYMBOLS_TO_REDEFINE=""
# add a symbol to the SYMBOLS_TO_REDIFINE list
avlc_add_symbol_to_redefine() {
    SYMBOLS_TO_REDEFINE="${SYMBOLS_TO_REDEFINE} $1"
}

# find and return the path of $2 inside $1
avlc_find_lib() {
    find $1 -name '$2'
}

# get all global symbols of a library and add them to SYMBOLS_TO_REDIFINE list
avlc_add_lib_to_redefine() {
    avlc_add_symbol_to_redefine "$("${CROSS_TOOLS}nm" -g $(avlc_find_lib ${VLC_BUILD_DIR} $1) | grep ' T ' | cut -f3 -d ' ')"
}

# Generic symbols
avlc_add_symbol_to_redefine AccessOpen
avlc_add_symbol_to_redefine AccessClose
avlc_add_symbol_to_redefine StreamOpen
avlc_add_symbol_to_redefine StreamClose
avlc_add_symbol_to_redefine OpenDemux
avlc_add_symbol_to_redefine CloseDemux
avlc_add_symbol_to_redefine DemuxOpen
avlc_add_symbol_to_redefine DemuxClose
avlc_add_symbol_to_redefine OpenFilter
avlc_add_symbol_to_redefine CloseFilter
avlc_add_symbol_to_redefine Open
avlc_add_symbol_to_redefine Close

#libvlc_json
avlc_add_symbol_to_redefine json_read
avlc_add_symbol_to_redefine json_parse_error
avlc_add_lib_to_redefine libvlc_json.a

BUILTINS="const void *vlc_static_modules[] = {\n";
for file in $VLC_MODULES; do
    outfile=${REDEFINED_VLC_MODULES_DIR}/$(basename $file)
    name=$(echo $file | sed 's/.*\.libs\/lib//' | sed 's/_plugin\.a//');
    symbols=$("${CROSS_TOOLS}nm" -g $file)

    # ensure that all modules have differents symbol names
    entry=$(avlc_get_symbol "$symbols" _)
    copyright=$(avlc_get_symbol "$symbols" copyright)
    license=$(avlc_get_symbol "$symbols" license)
    cat <<EOF > ${REDEFINED_VLC_MODULES_DIR}/syms
$entry vlc_entry__$name
$copyright vlc_entry_copyright__$name
$license vlc_entry_license__$name
EOF
    for sym in ${SYMBOLS_TO_REDEFINE}; do
        echo "$sym ${sym}__${name}" >> ${REDEFINED_VLC_MODULES_DIR}/syms
    done
    cmd="${CROSS_TOOLS}objcopy --redefine-syms ${REDEFINED_VLC_MODULES_DIR}/syms $file $outfile"
    ${cmd} || (echo "cmd failed: $cmd" && exit 1)

    DEFINITION=$DEFINITION"int vlc_entry__$name (int (*)(void *, void *, int, ...), void *);\n";
    BUILTINS="$BUILTINS vlc_entry__$name,\n";
done;
BUILTINS="$BUILTINS NULL\n};\n"; \
printf "/* Autogenerated from the list of modules */\n#include <unistd.h>\n$DEFINITION\n$BUILTINS\n" > $VLC_OUT_PATH/libvlcjni-modules.c

DEFINITION=""
BUILTINS="const void *libvlc_functions[] = {\n";
for func in $(cat $VLC_SRC_DIR/lib/libvlc.sym)
do
    DEFINITION=$DEFINITION"int $func(void);\n";
    BUILTINS="$BUILTINS $func,\n";
done
BUILTINS="$BUILTINS NULL\n};\n"; \
printf "/* Autogenerated from the list of modules */\n#include <unistd.h>\n$DEFINITION\n$BUILTINS\n" > $VLC_OUT_PATH/libvlcjni-symbols.c

rm ${REDEFINED_VLC_MODULES_DIR}/syms

###########################
# NDK-Build for libvlc.so #
###########################

VLC_MODULES=$(avlc_find_modules ${REDEFINED_VLC_MODULES_DIR})
VLC_CONTRIB_LDFLAGS=$(for i in $(/bin/ls $VLC_CONTRIB/lib/pkgconfig/*.pc); do avlc_pkgconfig --libs $i; done |xargs)

# Lua contrib doesn't expose a pkg-config file with libvlc 3.x and is
# not probed by the previous command in VLC_CONTRIB_LDFLAGS, so probe
# whether it was detected or add it manually to the LDFLAGS.
if ! avlc_pkgconfig --exists lua; then
    VLC_CONTRIB_LDFLAGS="$VLC_CONTRIB_LDFLAGS '$VLC_CONTRIB/lib/liblua.a'"
fi

echo -e "ndk-build vlc"

$NDK_BUILD -C $LIBVLCJNI_SRC_DIR/libvlc \
    APP_STL="c++_shared" \
    APP_CPPFLAGS="-frtti -fexceptions" \
    VLC_SRC_DIR="$VLC_SRC_DIR" \
    VLC_BUILD_DIR="$VLC_BUILD_DIR" \
    VLC_CONTRIB="$VLC_CONTRIB" \
    VLC_CONTRIB_LDFLAGS="$VLC_CONTRIB_LDFLAGS" \
    VLC_MODULES="$VLC_MODULES" \
    APP_BUILD_SCRIPT=jni/libvlc.mk \
    APP_PLATFORM=android-${ANDROID_API} \
    APP_ABI=${ANDROID_ABI} \
    NDK_PROJECT_PATH=jni \
    NDK_TOOLCHAIN_VERSION=clang \
    NDK_DEBUG=${NDK_DEBUG}
avlc_checkfail "ndk-build libvlc failed"

libvlc_pc_dir="$LIBVLCJNI_SRC_DIR/libvlc/jni/pkgconfig/${ANDROID_ABI}"
mkdir -p "${libvlc_pc_dir}"

PC_PREFIX="$(cd $LIBVLCJNI_SRC_DIR/libvlc/jni/; pwd -P)" \
PC_LIBDIR="$(cd $LIBVLCJNI_SRC_DIR/libvlc/jni/libs/${ANDROID_ABI}; pwd -P)" \
PC_INCLUDEDIR="$(cd $VLC_SRC_DIR/include/; pwd -P)" \
PC_CFLAGS="-I\${includedir}" \
PC_LIBS="-L\${libdir}" \
avlc_gen_pc_file "${libvlc_pc_dir}" libvlc 4.0.0

if [ "$AVLC_BUILD_JNI" = "1" ]; then
    $NDK_BUILD -C $LIBVLCJNI_SRC_DIR/libvlc \
        APP_STL="c++_shared" \
        APP_CPPFLAGS="-frtti -fexceptions" \
        VLC_SRC_DIR="$VLC_SRC_DIR" \
        VLC_BUILD_DIR="$VLC_BUILD_DIR" \
        APP_BUILD_SCRIPT=jni/Android.mk \
        APP_PLATFORM=android-${ANDROID_API} \
        APP_ABI=${ANDROID_ABI} \
        NDK_PROJECT_PATH=jni \
        NDK_TOOLCHAIN_VERSION=clang \
        NDK_DEBUG=${NDK_DEBUG}
    avlc_checkfail "ndk-build libvlcjni failed"
fi

# Remove gdbserver to avoid conflict with libvlcjni.so debug options
rm -f $VLC_OUT_PATH/libs/${ANDROID_ABI}/gdb*

} # avlc_build()

if [ "$AVLC_SOURCED" != "1" ]; then
    avlc_build
fi
