#! /bin/sh
set -e

#############
# FUNCTIONS #
#############

diagnostic()
{
    echo "$@" 1>&2;
}

fail()
{
    diagnostic "$1"
    exit 1
}

# Try to check whether a patch file has already been applied to the current directory tree
# Warning: this function assumes:
# - The patch file contains a Message-Id header. This can be generated with `git format-patch --thread ...` option
# - The patch has been applied with `git am --message-id ...` option to keep the Message-Id in the commit description
check_patch_is_applied()
{
    patch_file=$1
    diagnostic "Checking presence of patch $1"
    message_id=$(grep -E '^Message-Id: [^ ]+' "$patch_file" | sed 's/^Message-Id: \([^\ ]+\)/\1/')
    if [ -z "$message_id" ]; then
        diagnostic "Error: patch $patch_file does not contain a Message-Id."
        diagnostic "Please consider generating your patch files with the 'git format-patch --thread ...' option."
        diagnostic ""
        exit 1
    fi
    if [ -z "$(git log --grep="$message_id")" ]; then
        diagnostic "Cannot find patch $patch_file in tree, aborting."
        diagnostic "There can be two reasons for that:"
        diagnostic "- you forgot to apply the patch on this tree, or"
        diagnostic "- you applied the patch without the 'git am --message-id ...' option."
        diagnostic ""
        exit 1
    fi
}

RESET=0
while [ $# -gt 0 ]; do
    case $1 in
        help|--help|-h)
            echo "Use -b to bypass libvlc source checks (vlc custom sources)"
            exit 0
            ;;
        --reset)
            RESET=1
            ;;
        -b)
            BYPASS_VLC_SRC_CHECKS=1
            ;;
        *)
            diagnostic "$0: Invalid option '$1'."
            diagnostic "$0: Try --help for more information."
            exit 1
            ;;
    esac
    shift
done

####################
# Fetch VLC source #
####################

VLC_TESTED_HASH=c78826ba4499677e5bc7b6f3d57c733284e31afa
VLC_REPOSITORY=https://code.videolan.org/videolan/vlc.git
VLC_BRANCH=master
if [ ! -d "vlc" ]; then
    diagnostic "VLC sources: not found, cloning"
    git clone "${VLC_REPOSITORY}" vlc -b ${VLC_BRANCH} --single-branch || fail "VLC sources: git clone failed"
    cd vlc
    diagnostic "VLC sources: resetting to the VLC_TESTED_HASH commit (${VLC_TESTED_HASH})"
    git reset --hard ${VLC_TESTED_HASH} || fail "VLC sources: VLC_TESTED_HASH ${VLC_TESTED_HASH} not found"
    diagnostic "VLC sources: applying custom patches"
    # Keep Message-Id inside commits description to track them afterwards
    git am --message-id ../libvlcjni/libvlc/patches/*.patch || fail "VLC sources: cannot apply custom patches"
    cd ..
else
    diagnostic "VLC source: found sources, leaving untouched"
fi
if [ "$BYPASS_VLC_SRC_CHECKS" = 1 ]; then
    diagnostic "VLC sources: Bypassing checks (required by option)"
elif [ $RESET -eq 1 ]; then
    cd vlc
    git reset --hard ${VLC_TESTED_HASH} || fail "VLC sources: VLC_TESTED_HASH ${VLC_TESTED_HASH} not found"
    for patch_file in ../libvlcjni/libvlc/patches/*.patch; do
        git am --message-id $patch_file
        check_patch_is_applied "$patch_file"
    done
    cd ..
else
    diagnostic "VLC sources: Checking VLC_TESTED_HASH and patches presence"
    diagnostic "NOTE: checks can be bypass by adding '-b' option to this script."
    cd vlc
    git cat-file -e ${VLC_TESTED_HASH} 2> /dev/null || \
        fail "Error: Your vlc checkout does not contain the latest tested commit: ${VLC_TESTED_HASH}"
    for patch_file in ../libvlcjni/libvlc/patches/*.patch; do
        check_patch_is_applied "$patch_file"
    done
    cd ..
fi
