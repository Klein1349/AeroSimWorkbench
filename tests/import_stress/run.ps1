param(
    [Parameter(Mandatory)][string]$LocalStack,
    [string]$BuildDirectory,
    [string]$OutputDirectory,
    [string]$SamplesDirectory,
    [switch]$SkipBuild,
    [string[]]$Phases = @('boundary','stress','permissions','supplements','ui','larger','recheck')
)
$ErrorActionPreference = 'Stop'
$repo = (Get-Item -LiteralPath (Join-Path $PSScriptRoot '../..')).FullName
$LocalStack = (Get-Item -LiteralPath $LocalStack).FullName
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo 'build-import-stress' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo ('out/import-stress-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$env:AEROSIM_LOCAL_STACK = $LocalStack
$env:AEROSIM_IMPORT_TEST_BIN = Join-Path $BuildDirectory 'Debug'
$env:AEROSIM_IMPORT_TEST_OUTPUT = $OutputDirectory
if ($SamplesDirectory) { $env:AEROSIM_IMPORT_SAMPLES = [IO.Path]::GetFullPath($SamplesDirectory) }
$cmake = Join-Path $LocalStack 'tools/cmake-3.18.2/bin/cmake.exe'
if (-not $SkipBuild) {
    $ribbon = Join-Path $BuildDirectory 'deps/SARibbon'
    if (-not (Test-Path -LiteralPath $ribbon)) {
        New-Item -ItemType Directory -Force -Path (Split-Path $ribbon) | Out-Null
        Copy-Item -LiteralPath (Join-Path $LocalStack 'deps/SARibbon') -Destination $ribbon -Recurse
    }
    & $cmake -S $repo -B $BuildDirectory -G 'Visual Studio 15 2017 Win64' "-DCMAKE_PREFIX_PATH=$LocalStack/deps/qt/5.12.12/msvc2017_64" "-DVTK_DIR=$LocalStack/v" "-DSARIBBON_SOURCE_DIR=$ribbon" "-DAEROSIM_CGNS_ROOT=$LocalStack/deps/cgns-4.4.0-debug" -DAEROSIM_BUILD_GUI=ON -DAEROSIM_BUILD_TESTS=ON -DAEROSIM_BUILD_VISUALIZATION=ON -DAEROSIM_BUILD_CHARTS=OFF "-DAEROSIM_EIGEN_ROOT=$LocalStack/deps/v/ThirdParty/eigen/vtkeigen" -DAEROSIM_BUILD_MESH_VIEWER=ON "-DCMAKE_PROJECT_AeroSimWorkbench_INCLUDE=$PSScriptRoot/inject.cmake"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    & $cmake --build $BuildDirectory --config Debug --parallel 3
    if ($LASTEXITCODE -ne 0) { throw 'Production build failed' }
    $instance = Get-Content -LiteralPath "$BuildDirectory/CMakeCache.txt" | Where-Object { $_ -like 'CMAKE_GENERATOR_INSTANCE:INTERNAL=*' } | ForEach-Object { $_.Substring($_.IndexOf('=') + 1) }
    foreach ($target in 'import_cases','lifecycle_probe','ui_import_only') {
        & "$instance/MSBuild/15.0/Bin/MSBuild.exe" "$BuildDirectory/acceptance/$target.vcxproj" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /nologo /verbosity:minimal /nodeReuse:false
        if ($LASTEXITCODE -ne 0) { throw "Probe build failed: $target" }
    }
}
New-Item -ItemType Directory -Force -Path "$OutputDirectory/generator" | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$generatorSource = (Get-Item -LiteralPath (Join-Path $PSScriptRoot 'BoundaryMesh.cs')).FullName
$generatorExe = [IO.Path]::GetFullPath((Join-Path $OutputDirectory 'generator/BoundaryMesh.exe'))
& $csc /nologo /optimize+ /target:exe "/out:$generatorExe" $generatorSource
if ($LASTEXITCODE -ne 0) { throw 'Fixture generator build failed' }
foreach ($phase in $Phases) {
    if ($phase -in @('boundary','stress')) { & python "$PSScriptRoot/run_import_tests.py" $phase }
    else { & python "$PSScriptRoot/extra_checks.py" $phase }
    if ($LASTEXITCODE -ne 0) { throw "Runner setup error: $phase" }
}
Write-Output "Results: $OutputDirectory/results.jsonl"
Write-Output 'A completed runner does not mean every case passed; inspect status and guard records.'
