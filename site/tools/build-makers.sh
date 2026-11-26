#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if [[ "$(uname -s):$(uname -m)" != 'Linux:x86_64' ]]; then
    echo 'The Makers build requires a Linux x86_64 build runner.' >&2
    exit 1
fi

for tool in curl node python3 tar; do
    if ! command -v "$tool" >/dev/null; then
        echo "Missing build tool: $tool" >&2
        exit 1
    fi
done

sdk_version=v1.0.0
emscripten_version=3.1.56
emscripten_revision=9d106be887796484c4aaffc9dc45f48a8810f336
cmake_version=3.31.8
ninja_version=1.13.1
work="$PWD/out/makers"
release="https://github.com/wvfeng/sunfleck-core/releases/download/$sdk_version"
toolchain="$work/emscripten-$emscripten_version"
cmake_tools="$work/cmake-$cmake_version-linux-x86_64"
ninja_tools="$work/ninja-$ninja_version"
fonts="$work/fonts"
core="$work/sunfleck-core-${sdk_version#v}"
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
    mkdir -p "$ninja_tools"
    # Preserve the executable mode stored in the official ZIP.
    "$cmake_tools/bin/cmake" -E chdir "$ninja_tools" "$cmake_tools/bin/cmake" -E tar xf "$work/ninja.zip" ninja
    "$ninja_tools/ninja" --version
fi
export PATH="$cmake_tools/bin:$ninja_tools:$PATH"

if [[ ! -f "$wasm_sdk/lib/cmake/Sunfleck/SunfleckConfig.cmake" ]]; then
    archive="$work/Sunfleck-SDK-web-wasm32.tar.gz"
    curl -fsSL --retry 3 "$release/Sunfleck-SDK-web-wasm32.tar.gz" -o "$archive"
    mkdir -p "$wasm_sdk"
    tar -xzf "$archive" -C "$wasm_sdk"
fi
if [[ ! -f "$core/tools/fleckc.cpp" ]]; then
    curl -fsSL --retry 3 "https://codeload.github.com/wvfeng/sunfleck-core/tar.gz/refs/tags/$sdk_version" \
        -o "$work/core.tar.gz"
    tar -xzf "$work/core.tar.gz" -C "$work"
fi

if [[ ! -x "$toolchain/emscripten/emcc" ]]; then
    archive="$work/emscripten-$emscripten_version.tar.xz"
    curl -fsSL --retry 3 \
        "https://storage.googleapis.com/webassembly/emscripten-releases-builds/linux/$emscripten_revision/wasm-binaries.tar.xz" \
        -o "$archive"
    unpack=$(mktemp -d "$work/emscripten.XXXXXXXX")
    # CMake reads xz archives without calling a system xz executable.
    cmake -E chdir "$unpack" cmake -E tar xf "$archive"
    cmake -E rename "$unpack/install" "$toolchain"
    cmake -E rm -f "$archive"
fi

export PATH="$toolchain/emscripten:$PATH"
export EM_CONFIG="$toolchain/.emscripten"
export EM_CACHE="$work/emscripten-cache"
cat > "$EM_CONFIG" <<'PY'
import os
import shutil

toolchain = os.path.dirname(os.environ["EM_CONFIG"])
LLVM_ROOT = os.path.join(toolchain, "bin")
BINARYEN_ROOT = toolchain
NODE_JS = shutil.which("node")
PY
emcc --version

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
    "-DSUNFLECK_FLECKC_SOURCE_DIR=$core" \
    "-DSUNFLECK_DOC_FONT=$cjk_font" "-DSUNFLECK_DOC_MONO=$mono_font"
cmake --build "$work/site" --target documentation --parallel "$jobs"

cp "$toolchain/emscripten/LICENSE" "$site/licenses/Emscripten.txt"
freetype_license=$(find "$EM_CACHE/ports/freetype" -type f -name FTL.TXT -print -quit)
if [[ -z "$freetype_license" ]]; then
    echo 'FreeType license is missing from the Emscripten port cache.' >&2
    exit 1
fi
cp "$freetype_license" "$site/licenses/FreeType.txt"
for library in libcxx libcxxabi libunwind compiler-rt; do
    cp "$toolchain/emscripten/system/lib/$library/LICENSE.TXT" "$site/licenses/$library.txt"
done
python3 tools/package.py "$site" "$work/sunfleck-docs.zip" "$EM_CACHE/ports"
