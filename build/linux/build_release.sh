#!/bin/bash
# Packages the Linux build produced by build.sh.
# Run build_exe.bat on Windows to produce the Windows package.

cd "$(dirname "$0")/../.." || exit 1

./build/linux/build.sh || exit 1

rm -f lite-linux.zip
if command -v zip >/dev/null; then
  (cd build/dist && zip -r ../../lite-linux.zip lite data)
else
  tar -czf lite-linux.tar.gz -C build/dist lite data
  echo "zip not found; created lite-linux.tar.gz"
fi
