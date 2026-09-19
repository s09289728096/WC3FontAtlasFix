#!/bin/sh
set -eu
# Linux x86_64 host toolchain; output target remains Windows x86.
cd "$(dirname "$0")/.."
mkdir -p .tools
archive=.tools/llvm-mingw.tar.xz
url=https://github.com/mstorsjo/llvm-mingw/releases/download/20250613/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64.tar.xz
if [ ! -f "$archive" ]; then
    curl -fL --retry 2 "$url" -o "$archive.download"
    mv "$archive.download" "$archive"
fi
printf '%s  %s\n' 936f82221fa4ad4ff1829f28f1cdf4c1e304cfd589323212e2b7ef8be428784a "$archive" | sha256sum -c -
if [ ! -d .tools/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64 ]; then
    tar -xJf "$archive" -C .tools
fi
