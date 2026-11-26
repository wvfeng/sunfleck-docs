#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if [[ "$(uname -s):$(uname -m)" != 'Linux:x86_64' ]]; then
    echo 'The Makers build requires a Linux x86_64 build runner.' >&2
    exit 1
fi

for tool in curl git python3 tar; do
    if ! command -v "$tool" >/dev/null; then
        echo "Missing build tool: $tool" >&2
        exit 1
    fi
done

sdk_version=v1.0.0
emsdk_version=3.1.56
cmake_version=3.31.8
ninja_version=1.13.1
work="$PWD/out/makers"
release="https://github.com/wvfeng/sunfleck-core/releases/download/$sdk_version"
emsdk="$work/emsdk-$emsdk_version"
cmake_tools="$work/cmake-$cmake_version-linux-x86_64"
ninja_tools="$work/ninja-$ninja_version"
fonts="$work/fonts"
host_sdk="$work/sdk-$sdk_version/linux-x64"
wasm_sdk="$work/sdk-$sdk_version/web-wasm32"
site="$work/site/web"
jobs=4
mkdir -p "$work" "$fonts"

if [[ ! -x "$cmake_tools/bin/cmake" ]]; then
    curl -fsSL --retry 3 \
        "https://github.com/Kitware/CMake/releases/download/v$cmake_version/cmake-$cmake_version-linux-x86_64.tar.gz" \
        -o "$work/cmake.tar.gz"
    tar -xzf "$work/cmake.tar.gz" -C "$work"
fi
if [[ ! -x "$ninja_tools/ninja" ]]; then
    curl -fsSL --retry 3 "https://github.com/ninja-build/ninja/releases/download/v$ninja_version/ninja-linux.zip" \
        -o "$work/ninja.zip"
    python3 -m zipfile -e "$work/ninja.zip" "$ninja_tools"
    chmod +x "$ninja_tools/ninja"
fi
export PATH="$cmake_tools/bin:$ninja_tools:$PATH"

download_sdk() {
    local platform=$1 directory=$2 archive temporary
    archive="$work/Sunfleck-SDK-$platform.tar.gz"
    curl -fsSL --retry 3 "$release/Sunfleck-SDK-$platform.tar.gz" -o "$archive"
    temporary=$(mktemp -d "$work/sdk.XXXXXXXX")
    tar -xzf "$archive" -C "$temporary"
    mkdir -p "$(dirname "$directory")"
    mv "$temporary" "$directory"
}

fleckc="$host_sdk/bin/fleckc"
if [[ ! -x "$fleckc" ]]; then
    download_sdk linux-x64 "$host_sdk"
fi
if [[ ! -f "$wasm_sdk/lib/cmake/Sunfleck/SunfleckConfig.cmake" ]]; then
    download_sdk web-wasm32 "$wasm_sdk"
fi

if ! "$fleckc" --schema >/dev/null; then
    echo 'The Linux SDK compiler could not run on this build image. Building fleckc from the same release.' >&2
    for compiler in cc c++; do
        if ! command -v "$compiler" >/dev/null; then
            echo "Missing compiler for the fleckc fallback: $compiler" >&2
            exit 1
        fi
    done
    core="$work/sunfleck-core-${sdk_version#v}"
    if [[ ! -f "$core/CMakeLists.txt" ]]; then
        curl -fsSL --retry 3 "https://codeload.github.com/wvfeng/sunfleck-core/tar.gz/refs/tags/$sdk_version" \
            -o "$work/core.tar.gz"
        tar -xzf "$work/core.tar.gz" -C "$work"
    fi
    host_build="$work/host-$sdk_version"
    cmake -S "$core" -B "$host_build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF -DUI_BUILD_TOOLS=ON -DUI_PLATFORM_X11=OFF \
        -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON
    cmake --build "$host_build" --target fleckc --parallel "$jobs"
    fleckc="$host_build/fleckc"
fi

if [[ ! -f "$emsdk/emsdk" ]]; then
    git clone --depth 1 --branch "$emsdk_version" https://github.com/emscripten-core/emsdk.git "$emsdk"
fi
"$emsdk/emsdk" install "$emsdk_version"
"$emsdk/emsdk" activate "$emsdk_version"
set +u
source "$emsdk/emsdk_env.sh"
set -u
export EM_CACHE="$work/emscripten-cache"

cjk_font="$fonts/NotoSansCJKsc-Regular.otf"
mono_font="$fonts/NotoSansMono.ttf"
curl -fsSL --retry 3 \
    'https://raw.githubusercontent.com/notofonts/noto-cjk/523d033d6cb47f4a80c58a35753646f5c3608a78/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf' \
    -o "$cjk_font"
curl -fsSL --retry 3 \
    'https://raw.githubusercontent.com/google/fonts/097bc1b8c04c3224087c2ae95f7a923859b778cf/ofl/notosansmono/NotoSansMono%5Bwdth%2Cwght%5D.ttf' \
    -o "$mono_font"

emcmake cmake -S . -B "$work/site" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    "-DSunfleck_DIR=$wasm_sdk/lib/cmake/Sunfleck" \
    "-DSUNFLECK_FLECKC=$fleckc" \
    "-DSUNFLECK_DOC_FONT=$cjk_font" "-DSUNFLECK_DOC_MONO=$mono_font"
cmake --build "$work/site" --target documentation --parallel "$jobs"

cp "$emsdk/upstream/emscripten/LICENSE" "$site/licenses/Emscripten.txt"
freetype_license=$(find "$EM_CACHE/ports/freetype" -type f -name FTL.TXT -print -quit)
if [[ -z "$freetype_license" ]]; then
    echo 'FreeType license is missing from the Emscripten port cache.' >&2
    exit 1
fi
cp "$freetype_license" "$site/licenses/FreeType.txt"
for library in libcxx libcxxabi libunwind compiler-rt; do
    cp "$emsdk/upstream/emscripten/system/lib/$library/LICENSE.TXT" "$site/licenses/$library.txt"
done
python3 tools/package.py "$site" "$work/sunfleck-docs.zip" "$EM_CACHE/ports"
