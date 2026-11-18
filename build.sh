#!/bin/bash
#
# build.sh - compila launcher + MSX bare metal per Raspberry Pi 3 (32 bit)
# e prepara la cartella out/sd da copiare sulla scheda SD.
#
# Requisiti (Debian/Ubuntu):
#   sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi \
#                    libstdc++-arm-none-eabi-newlib texinfo git make
#
set -e
cd "$(dirname "$0")"
ROOT=$(pwd)

mkdir -p build
if [ ! -d build/circle-stdlib ]; then
    git clone --recursive https://github.com/smuehlst/circle-stdlib.git build/circle-stdlib
fi
if [ ! -f build/circle-stdlib/install/arm-none-circle/lib/libc.a ]; then
    (cd build/circle-stdlib && ./configure -r 3 -n && make -j"$(nproc)")
fi

make -C launcher -j"$(nproc)" CIRCLESTDLIB="$ROOT/build/circle-stdlib"
make -C msx      -j"$(nproc)" CIRCLESTDLIB="$ROOT/build/circle-stdlib"

# firmware Raspberry Pi
FW=build/firmware
mkdir -p "$FW"
for f in bootcode.bin start.elf fixup.dat LICENCE.broadcom; do
    [ -f "$FW/$f" ] || curl -sSfL -o "$FW/$f" "https://raw.githubusercontent.com/raspberrypi/firmware/master/boot/$f"
done

# cartella SD
OUT=out/sd
rm -rf "$OUT"
mkdir -p "$OUT"
cp -r sdcard/. "$OUT/"
cp "$FW"/* "$OUT/"
cp launcher/kernel-launcher.img "$OUT/kernel8-32.img"
cp launcher/kernel-launcher.img "$OUT/kernel7.img"
cp msx/kernel-msx.img "$OUT/msx/kernel-msx.img"

echo
echo "Fatto: copia il contenuto di $OUT nella scheda SD (vedi LEGGIMI.txt)."
