param(
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][string]$FleckCompiler,
    [Parameter(Mandatory)][string]$EmsdkRoot,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'out'),
    [string]$CMake = 'cmake',
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$FleckCompiler = (Resolve-Path -LiteralPath $FleckCompiler).Path
$EmsdkRoot = (Resolve-Path -LiteralPath $EmsdkRoot).Path
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)." }
}

$toolchain = Join-Path $EmsdkRoot 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'
$pythonPath = (Get-Command $Python -CommandType Application).Source
Invoke-Checked $CMake @('-S', $PSScriptRoot, '-B', $BuildDirectory, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DSunfleck_DIR=$SdkRoot/lib/cmake/Sunfleck", "-DSUNFLECK_FLECKC=$FleckCompiler",
    "-DPython3_EXECUTABLE=$pythonPath")
Invoke-Checked $CMake @('--build', $BuildDirectory)

$web = Join-Path $BuildDirectory 'web'
$licenses = Join-Path $web 'licenses'
$upstream = Join-Path $EmsdkRoot 'upstream/emscripten'
Copy-Item -LiteralPath "$upstream/LICENSE" -Destination "$licenses/Emscripten.txt" -Force
$cache = if ($env:EM_CACHE) { $env:EM_CACHE } else { Join-Path $upstream 'cache' }
$freeType = Get-ChildItem -LiteralPath "$cache/ports/freetype" -Recurse -Filter FTL.TXT | Select-Object -First 1
if (!$freeType) { throw 'FreeType license is missing from the Emscripten port cache.' }
Copy-Item -LiteralPath $freeType.FullName -Destination "$licenses/FreeType.txt" -Force
foreach ($library in @('libcxx', 'libcxxabi', 'libunwind', 'compiler-rt')) {
    Copy-Item -LiteralPath "$upstream/system/lib/$library/LICENSE.TXT" -Destination "$licenses/$library.txt" -Force
}
Invoke-Checked $pythonPath @((Join-Path $PSScriptRoot 'tools/package.py'), $web,
    (Join-Path $BuildDirectory 'sunfleck-docs.zip'), (Join-Path $cache 'ports'))
Write-Host "Documentation: $web"
