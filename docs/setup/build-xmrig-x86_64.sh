#!/bin/zsh
# Rebuild the x86_64 (Intel) slice of bin/xmrig with the donate minimum at 0, on an Apple Silicon Mac.
#
# Why: the official Intel release of XMRig has kMinimumDonateLevel = 1, so --donate-level=0 in the job
# file is ignored and 1 minute in 100 goes to the XMRig devs. This builds the same 6.26.0 source with
# both donate constants at 0, against static libuv / hwloc / OpenSSL (the versions XMRig's own
# scripts/build_deps.sh pins), for macOS 11+, like the official binary.
#
# The arm64 slice is kept byte-identical (signature included), so the Apple Silicon Macs run exactly
# the binary they ran before. Output: $WORK/xmrig-universal. Check it, then copy it over bin/xmrig.
#
#   ./docs/setup/build-xmrig-x86_64.sh [work dir]      (default: /tmp/xmrig-x86-build; no spaces allowed)
set -euo pipefail

ROOT=${0:A:h:h:h}
WORK=${1:-/tmp/xmrig-x86-build}
XMRIG_TAG=v6.26.0
UV=1.51.0 SSL=3.0.16 HWLOC=2.12.1
FLAGS="-mmacosx-version-min=11.0"
[[ $WORK == *" "* ]] && { echo "work dir must not contain spaces (autotools)"; exit 1; }

mkdir -p "$WORK/dl" "$WORK/deps"
cd "$WORK"
rm -rf src && mkdir src
git -C "$ROOT/vendor/xmrig" archive "$XMRIG_TAG" | tar -x -C src
sed -i '' -e 's/kDefaultDonateLevel = 1;/kDefaultDonateLevel = 0;/' \
          -e 's/kMinimumDonateLevel = 1;/kMinimumDonateLevel = 0;/' src/src/donate.h
grep -q "kMinimumDonateLevel = 0;" src/src/donate.h

cd dl
curl -fsSL -o uv.tgz    "https://dist.libuv.org/dist/v$UV/libuv-v$UV.tar.gz"
curl -fsSL -o ssl.tgz   "https://github.com/openssl/openssl/releases/download/openssl-$SSL/openssl-$SSL.tar.gz"
curl -fsSL -o hwloc.tgz "https://download.open-mpi.org/release/hwloc/v${HWLOC%.*}/hwloc-$HWLOC.tar.gz"
for f in uv.tgz ssl.tgz hwloc.tgz; do tar -xzf $f; done

cmake -S "libuv-v$UV" -B build-uv -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_BUILD_TYPE=Release -DLIBUV_BUILD_SHARED=OFF -DLIBUV_BUILD_TESTS=OFF -DLIBUV_BUILD_BENCH=OFF \
  -DCMAKE_INSTALL_PREFIX="$WORK/deps"
cmake --build build-uv -j 10 && cmake --install build-uv

(cd "openssl-$SSL" && ./Configure darwin64-x86_64-cc no-shared no-asm no-zlib no-comp no-dgram no-filenames \
  no-cms no-tests $FLAGS --prefix="$WORK/deps" --libdir=lib && make -j 10 build_libs && make install_dev)

(cd "hwloc-$HWLOC" && ./configure --host=x86_64-apple-darwin --disable-shared --enable-static --disable-io \
  --disable-libudev --disable-libxml2 --disable-cairo --disable-readme CC="clang -arch x86_64" CFLAGS="-O2 $FLAGS" \
  --prefix="$WORK/deps" && make -j 10 -C hwloc && make -C hwloc install && make -C include install)

# CMAKE_SYSTEM_PROCESSOR: XMRig's cmake/cpu.cmake reads it, and on an arm64 host it would add -march=armv8-a.
cd "$WORK"
rm -rf build-x86
cmake -S src -B build-x86 -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_BUILD_TYPE=Release -DXMRIG_DEPS="$WORK/deps" -DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew
cmake --build build-x86 -j 10

# Sign only the new slice, then merge: the arm64 slice (and its signature) stays exactly as shipped.
cp build-x86/xmrig new-x86_64 && codesign -s - -f new-x86_64
lipo "$ROOT/bin/xmrig" -thin arm64 -output current-arm64
lipo -create current-arm64 new-x86_64 -output xmrig-universal
lipo xmrig-universal -thin arm64 -output check-arm64 && cmp check-arm64 current-arm64
codesign -v xmrig-universal

arch -x86_64 ./xmrig-universal --dry-run --no-color --donate-level=0 -o 127.0.0.1:3 -u x | grep DONATE
echo "Built $WORK/xmrig-universal. Self-check (slow under Rosetta): arch -x86_64 $WORK/xmrig-universal --bench=1M"
