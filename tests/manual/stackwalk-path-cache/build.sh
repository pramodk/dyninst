#!/usr/bin/env bash
set -euo pipefail

: "${DYNINST:?set DYNINST to the Dyninst installation prefix}"
: "${CXX:=c++}"

cd "$(dirname "$0")"
mkdir -p root/app

gcc -O1 -g -shared -fPIC foo.c -o root/app/libfoo.so
gcc -O1 -g target.c -o root/app/target \
  -Lroot/app -lfoo -Wl,-rpath,/app

while read -r library; do
  mkdir -p "root$(dirname "$library")"
  cp -L "$library" "root$library"
done < <(ldd root/app/target | grep -o '/[^ ]*' | grep -v "^$PWD")

for source in walk16.cpp walk_many.cpp; do
  "$CXX" -std=c++17 -O2 "$source" -o "${source%.cpp}" \
    -I"$DYNINST/include" \
    -L"$DYNINST/lib" -L"$DYNINST/lib64" \
    -Wl,-rpath,"$DYNINST/lib" -Wl,-rpath,"$DYNINST/lib64" \
    -lstackwalk -lpcontrol -lcommon
done
