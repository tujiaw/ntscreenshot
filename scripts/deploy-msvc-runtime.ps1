param(
  [Parameter(Mandatory = $true)][string]$TargetDir,
  [ValidateSet('x64', 'x86')][string]$Architecture = 'x64'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$target = (Resolve-Path -LiteralPath $TargetDir).Path
$candidates = @()
if ($env:VCToolsRedistDir) {
  $candidates += Get-ChildItem -Path (Join-Path $env:VCToolsRedistDir "$Architecture\Microsoft.VC*.CRT\msvcp140.dll") -File -ErrorAction SilentlyContinue
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
  $paths = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
      -find "VC\Redist\MSVC\**\$Architecture\Microsoft.VC*.CRT\msvcp140.dll")
  $candidates += $paths | Where-Object { $_ -notmatch '\\(onecore|spectre)\\' } | ForEach-Object { Get-Item -LiteralPath $_ }
}
if ($candidates.Count -gt 0) {
  $runtime = ($candidates | Sort-Object -Property @{ Expression = {
      [version]('{0}.{1}.{2}.{3}' -f $_.VersionInfo.FileMajorPart, $_.VersionInfo.FileMinorPart,
        $_.VersionInfo.FileBuildPart, $_.VersionInfo.FilePrivatePart)
    }; Descending = $true } | Select-Object -First 1).DirectoryName
  Get-ChildItem -LiteralPath $runtime -Filter '*.dll' -File | Copy-Item -Destination $target -Force
  Write-Host "==> App-local MSVC runtime: $runtime"
}
$required = @('msvcp140.dll', 'vcruntime140.dll')
if ($Architecture -eq 'x64') { $required += 'vcruntime140_1.dll' }
foreach ($name in $required) {
  if (-not (Test-Path -LiteralPath (Join-Path $target $name))) {
    throw "App-local runtime is missing: $name. Install the MSVC build tools with its redistributable components."
  }
}
