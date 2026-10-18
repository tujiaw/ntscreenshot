param(
  [string]$PackageDir = 'dist/ntscreenshot-x64-Release',
  [string]$OutDir = 'dist',
  [string]$Version = '',
  [string]$CompilerPath = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not [IO.Path]::IsPathRooted($PackageDir)) { $PackageDir = Join-Path $repoRoot $PackageDir }
if (-not [IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $repoRoot $OutDir }
$package = (Resolve-Path -LiteralPath $PackageDir).Path
$outputPath = [IO.Path]::GetFullPath($OutDir).TrimEnd('\', '/')
if ($outputPath -eq $package.TrimEnd('\', '/') -or $outputPath.StartsWith($package.TrimEnd('\', '/') + '\', [StringComparison]::OrdinalIgnoreCase)) {
  throw 'Installer output must be outside the package source directory.'
}
foreach ($required in @('ntscreenshot.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll',
    'Qt6Sql.dll', 'platforms/qwindows.dll', 'sqldrivers/qsqlite.dll', 'LICENSE')) {
  if (-not (Test-Path -LiteralPath (Join-Path $package $required) -PathType Leaf)) {
    throw "Package is incomplete: $required. Run scripts/package-win.ps1 first."
  }
}
$info = (Get-Item -LiteralPath (Join-Path $package 'ntscreenshot.exe')).VersionInfo
$binaryVersion = '{0}.{1}.{2}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart
if (-not $Version) { $Version = $binaryVersion }
if ($Version -notmatch '^\d+\.\d+\.\d+$' -or $Version -ne $binaryVersion) {
  throw "Installer version $Version does not match executable version $binaryVersion."
}
if (-not $CompilerPath) {
  $compiler = Get-Command ISCC.exe -ErrorAction SilentlyContinue
  if ($compiler) { $CompilerPath = $compiler.Source }
  else {
    foreach ($root in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, (Join-Path $env:LOCALAPPDATA 'Programs'))) {
      foreach ($edition in @('Inno Setup 7', 'Inno Setup 6')) {
        $candidate = Join-Path $root "$edition\ISCC.exe"
        if (Test-Path -LiteralPath $candidate) { $CompilerPath = $candidate; break }
      }
      if ($CompilerPath) { break }
    }
  }
}
if (-not $CompilerPath -or -not (Test-Path -LiteralPath $CompilerPath)) {
  throw 'Inno Setup 6.7 or later is required. Install Inno Setup or pass -CompilerPath to ISCC.exe.'
}
& (Join-Path $PSScriptRoot 'deploy-msvc-runtime.ps1') -TargetDir $package -Architecture x64
$licenses = Join-Path $package 'licenses'
New-Item -ItemType Directory -Force -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'installer/Languages/LICENSE.InnoSetup.txt') -Destination (Join-Path $licenses 'InnoSetup.txt') -Force
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$output = (Resolve-Path -LiteralPath $OutDir).Path
$scriptPath = Join-Path $repoRoot 'installer/ntscreenshot.iss'
& $CompilerPath "/DPackageDir=$package" "/DOutputDir=$output" "/DAppVersion=$Version" $scriptPath
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed: $LASTEXITCODE" }
$installer = Join-Path $output "ntscreenshot-$Version-x64-Setup.exe"
if (-not (Test-Path -LiteralPath $installer)) { throw 'Inno Setup did not produce the expected installer.' }
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath "$installer.sha256" -Encoding ascii
Write-Host "==> Installer: $installer"
