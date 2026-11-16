param([string]$Executable)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (!$Executable) { $Executable = Join-Path $repoRoot 'build-windows/Debug/aerosim_gui.exe' }
$resolvedExe = (Resolve-Path -LiteralPath $Executable).Path
$running = @(Get-Process aerosim_gui -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $resolvedExe })
if ($running.Count) { Write-Host 'This workbench is already running; preserving its parameter drafts.'; exit 0 }
$env:QT_ACCESSIBILITY = '1'
$env:QT_QPA_PLATFORM = 'windows'
Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
Remove-Item Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue
Start-Process -FilePath $resolvedExe -ArgumentList '--workbench' -WorkingDirectory (Split-Path -Parent $resolvedExe) -WindowStyle Normal
