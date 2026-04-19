#!/bin/bash

set -e

BUILD=false
CLEAN=false

while getopts "bc" opt; do
    case "$opt" in
        b) BUILD=true ;;
        c) CLEAN=true ;;
        ?) echo "Usage: $0 [-b] [-c]"; exit 1 ;;
    esac
done

if $CLEAN; then
    [ -d p_cache ] && rmdir p_cache
    [ -f pache.out ] && rm pache.out
fi

if $BUILD; then
    bash scripts/build.sh
fi

bash scripts/run.sh
