param(
  [ValidateSet('Debug','Release')]
  [string]$Config = 'Release',

  [ValidateSet('x64','Win32')]
  [string]$Platform = 'x64',

  [string]$QtDir = '',

  [string]$ExePath = '',

  [string]$VcpkgInstalledDir = '',

  [string]$OpenCvDir = '',

  [ValidateSet('x64-windows','x86-windows')]
  [string]$VcpkgTriplet = 'x64-windows',

  [string]$OutDir = 'dist',

  [string]$DumpbinExe = '',

  [switch]$InPlace,

  [switch]$Zip
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-RepoRoot {
  return (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}

function Resolve-QtDir([string]$QtDir) {
  $candidates = @()
  if ($QtDir) { $candidates += $QtDir }
  foreach ($envName in @('QTDIR', 'QT_DIR', 'Qt6_DIR')) {
    $value = [Environment]::GetEnvironmentVariable($envName)
    if ($value) { $candidates += $value }
  }
  $candidates += @(
    'C:\Qt\6.8.3\msvc2022_64',
    'C:\Qt\6.8.2\msvc2022_64'
  )
  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path (Join-Path $candidate 'bin\windeployqt.exe'))) {
      return (Resolve-Path $candidate).Path
    }
  }
  throw 'Cannot find Qt installation with windeployqt.exe. Pass -QtDir or set QTDIR.'
}

function Resolve-ExePath([string]$RepoRoot, [string]$Explicit, [string]$Platform, [string]$Config) {
  if ($Explicit) {
    if (-not (Test-Path $Explicit)) { throw "Exe not found: $Explicit" }
    return (Resolve-Path $Explicit).Path
  }

  foreach ($candidate in @(
      (Join-Path $RepoRoot ("build\cmake-{0}-{1}\ntscreenshot.exe" -f $Platform, $Config)),
      (Join-Path $RepoRoot ("build\bin\{0}\{1}\ntscreenshot.exe" -f $Platform, $Config))
    )) {
    if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
  }
  throw 'Cannot find ntscreenshot.exe. Build first or pass -ExePath.'
}

function Resolve-VcpkgRuntimeDir(
  [string]$RepoRoot,
  [string]$SourceDir,
  [string]$InstalledDir,
  [string]$Triplet,
  [string]$Config
) {
  $installedCandidates = @()
  if ($InstalledDir) { $installedCandidates += $InstalledDir }
  $environmentDir = [Environment]::GetEnvironmentVariable('VCPKG_INSTALLED_DIR')
  if ($environmentDir) { $installedCandidates += $environmentDir }
  $installedCandidates += @(
    (Join-Path $SourceDir 'vcpkg_installed'),
    (Join-Path $RepoRoot 'vcpkg_installed')
  )

  foreach ($candidate in $installedCandidates) {
    if (-not $candidate) { continue }
    $runtimeDir = if ($Config -eq 'Debug') {
      Join-Path $candidate (Join-Path $Triplet 'debug\bin')
    } else {
      Join-Path $candidate (Join-Path $Triplet 'bin')
    }
    if (Test-Path $runtimeDir) {
      return (Resolve-Path $runtimeDir).Path
    }
  }

  throw "Cannot find vcpkg runtime directory. Pass -VcpkgInstalledDir for $Triplet ($Config)."
}

function Get-PathSizeBytes([string]$Path) {
  if (-not (Test-Path $Path)) {
    return 0
  }
  if ((Get-Item $Path).PSIsContainer) {
    $files = Get-ChildItem $Path -Recurse -File -ErrorAction SilentlyContinue
    if ($files) {
      return ($files | Measure-Object Length -Sum).Sum
    }
    return 0
  }
  return (Get-Item $Path).Length
}

function Resolve-Dumpbin([string]$Explicit) {
  if ($Explicit) { return (Resolve-Path -LiteralPath $Explicit).Path }
  $command = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
  if ($command) { return $command.Source }
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (Test-Path -LiteralPath $vswhere) {
    $candidates = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe')
    if ($candidates.Count -gt 0) { return $candidates[0] }
  }
  throw 'Cannot find dumpbin.exe. Install MSVC, run from a VS developer shell, or pass -DumpbinExe.'
}

function Get-RequiredRuntimeDlls([string]$Executable, [string]$RuntimeDir, [string]$Dumpbin, [string[]]$OtherRuntimeDirs) {
  # Follow PE imports recursively instead of copying every DLL in a dependency
  # installation. Qt and its dynamically loaded plugins are handled by windeployqt.
  # OpenCV video plugins are not used by this application's image-only operations.
  $available = @{}
  foreach ($dll in (Get-ChildItem -LiteralPath $RuntimeDir -Filter '*.dll' -File)) {
    $available[$dll.Name] = $dll.FullName
  }
  $required = @{}
  $pending = [System.Collections.Generic.Queue[string]]::new()
  $pending.Enqueue($Executable)
  while ($pending.Count -gt 0) {
    $binary = $pending.Dequeue()
    $imports = @(& $Dumpbin /nologo /dependents $binary)
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect runtime dependencies of $binary." }
    foreach ($line in $imports) {
      if ($line -notmatch '^\s+([^\s]+\.dll)\s*$') { continue }
      $name = $Matches[1]
      if (-not $available.ContainsKey($name)) {
        if ($name -match '^(api-ms-|ext-ms-)') { continue }
        $foundElsewhere = $false
        foreach ($directory in $OtherRuntimeDirs) {
          if (Test-Path -LiteralPath (Join-Path $directory $name)) {
            $foundElsewhere = $true
            break
          }
        }
        if ($name -like 'opencv_*.dll' -or -not $foundElsewhere) {
          throw "Required DLL not found in ${RuntimeDir}: $name (imported by $binary)"
        }
        continue
      }
      if ($required.ContainsKey($name)) { continue }
      $required[$name] = $available[$name]
      $pending.Enqueue($available[$name])
    }
  }
  if (-not ($required.Keys | Where-Object { $_ -like 'opencv_*.dll' })) {
    throw "No imported OpenCV runtime DLL was found for $Executable in $RuntimeDir."
  }
  return @($required.Values | Sort-Object)
}

function Remove-IfExists([string]$Path) {
  if (-not (Test-Path $Path)) {
    return 0
  }
  $size = Get-PathSizeBytes $Path
  Remove-Item -Recurse -Force $Path
  return $size
}

function Remove-DeployRuntimeBloat([string]$TargetDir) {
  Write-Host '==> Pruning unnecessary runtime files'

  $removedBytes = 0
  foreach ($relativePath in @(
      'qmltooling',
      'qml',
      'sqldrivers\qsqlmimer.dll',
      'sqldrivers\qsqlodbc.dll',
      'sqldrivers\qsqlpsql.dll'
    )) {
    $fullPath = Join-Path $TargetDir $relativePath
    $removedBytes += Remove-IfExists $fullPath
  }

  if ($removedBytes -gt 0) {
    Write-Host ("    Removed {0:N2} MB of unused runtime files" -f ($removedBytes / 1MB))
  } else {
    Write-Host '    No runtime bloat found'
  }
}

function Remove-BuildArtifacts([string]$TargetDir) {
  Write-Host '==> Pruning build artifacts from output directory'

  $removedBytes = 0
  foreach ($relativePath in @(
      'CMakeFiles',
      'ntscreenshot_autogen',
      'build.ninja',
      'CMakeCache.txt',
      'cmake_install.cmake',
      '.ninja_deps',
      '.ninja_log',
      '.qt'
    )) {
    $fullPath = Join-Path $TargetDir $relativePath
    $removedBytes += Remove-IfExists $fullPath
  }

  if ($removedBytes -gt 0) {
    Write-Host ("    Removed {0:N2} MB of build artifacts" -f ($removedBytes / 1MB))
  } else {
    Write-Host '    No build artifacts found'
  }
}

function Copy-PackageDocuments([string]$RepoRoot, [string]$StageDir, [string]$QtDir, [string]$SourceExe, [string]$VcpkgInstalledDir, [string]$VcpkgTriplet, [string]$OpenCvRoot) {
  foreach ($document in @('LICENSE', 'PRIVACY.md', 'THIRD_PARTY_NOTICES.md')) {
    $sourceDocument = Join-Path $RepoRoot $document
    if (Test-Path $sourceDocument) {
      Copy-Item -Force -Path $sourceDocument -Destination $StageDir
    } else {
      Write-Warning "Package document is missing: $document"
    }
  }

  $licenseDir = Join-Path $StageDir 'licenses'
  New-Item -ItemType Directory -Force -Path $licenseDir | Out-Null
  $lucideLicense = Join-Path $RepoRoot 'src\resource\licenses\lucide-LICENSE'
  if (Test-Path -LiteralPath $lucideLicense) {
    Copy-Item -LiteralPath $lucideLicense -Destination (Join-Path $licenseDir 'Lucide-ISC.txt') -Force
  }
  $qhotkeyLicense = Join-Path $RepoRoot 'src\libs\QHotkey\LICENSE'
  if (Test-Path $qhotkeyLicense) {
    Copy-Item -Force -Path $qhotkeyLicense -Destination (Join-Path $licenseDir 'QHotkey-BSD-3-Clause.txt')
  }

  if ($OpenCvRoot) {
    $licenseCandidates = @((Join-Path $OpenCvRoot 'LICENSE'), (Join-Path $OpenCvRoot '..\LICENSE.txt'))
    $cachePath = Join-Path $OpenCvRoot 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cachePath) {
      foreach ($line in (Get-Content -LiteralPath $cachePath)) {
        if ($line -match '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$') {
          $licenseCandidates += Join-Path $Matches[1] 'LICENSE'
        }
      }
    }
    foreach ($candidate in $licenseCandidates) {
      if (-not (Test-Path -LiteralPath $candidate)) { continue }
      Copy-Item -LiteralPath $candidate -Destination (Join-Path $licenseDir 'OpenCV.txt') -Force
      break
    }
  }

  foreach ($qtLicenseCandidate in @(
      (Join-Path $QtDir 'LICENSES'),
      (Join-Path $QtDir '..\..\Licenses')
    )) {
    if (-not (Test-Path $qtLicenseCandidate)) { continue }
    Copy-Item -Recurse -Force -Path $qtLicenseCandidate -Destination (Join-Path $licenseDir 'Qt')
    break
  }

  $installedCandidates = @()
  if ($VcpkgInstalledDir) { $installedCandidates += $VcpkgInstalledDir }
  $installedCandidates += Join-Path (Split-Path -Parent $SourceExe) 'vcpkg_installed'
  foreach ($installedCandidate in $installedCandidates) {
    $shareDir = Join-Path $installedCandidate (Join-Path $VcpkgTriplet 'share')
    if (-not (Test-Path $shareDir)) { continue }
    $vcpkgLicenseDir = Join-Path $licenseDir 'vcpkg'
    New-Item -ItemType Directory -Force -Path $vcpkgLicenseDir | Out-Null
    Get-ChildItem -Path $shareDir -Filter 'copyright' -Recurse -File | ForEach-Object {
      $portName = Split-Path -Leaf (Split-Path -Parent $_.FullName)
      Copy-Item -Force -Path $_.FullName -Destination (Join-Path $vcpkgLicenseDir ("{0}.txt" -f $portName))
    }
    break
  }
}

function Invoke-WinDeploy(
  [string]$QtRoot,
  [string]$DeployExe,
  [string]$TargetDir,
  [string]$Config
) {
  $windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
  $qtMode = if ($Config -eq 'Debug') { '--debug' } else { '--release' }
  $deployArgs = @(
    $qtMode,
    '--force',
    '--compiler-runtime',
    '--no-translations',
    '--dir', $TargetDir,
    $DeployExe
  )

  Write-Host '==> Deploying runtime dependencies'
  Write-Host "    Exe      : $DeployExe"
  Write-Host "    Target   : $TargetDir"
  Write-Host "    Qt       : $QtRoot"

  & $windeployqt @deployArgs
  if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE."
  }
}

$repoRoot = Resolve-RepoRoot
$qtRoot = Resolve-QtDir -QtDir $QtDir
$sourceExe = Resolve-ExePath -RepoRoot $repoRoot -Explicit $ExePath -Platform $Platform -Config $Config
$sourceDir = Split-Path -Parent $sourceExe
$dumpbin = Resolve-Dumpbin -Explicit $DumpbinExe
$stageToDist = $Zip -or (-not $InPlace)

if ($stageToDist) {
  $outputRoot = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repoRoot $OutDir }
  $targetDir = Join-Path $outputRoot ("ntscreenshot-{0}-{1}" -f $Platform, $Config)
  if (Test-Path $targetDir) { Remove-Item -Recurse -Force -LiteralPath $targetDir }
  New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
  Copy-Item -Force -Path $sourceExe -Destination $targetDir
  $deployExe = Join-Path $targetDir (Split-Path -Leaf $sourceExe)
  Write-Host "==> Staging package: $targetDir"
} else {
  $targetDir = $sourceDir
  $deployExe = $sourceExe
}

Invoke-WinDeploy -QtRoot $qtRoot -DeployExe $deployExe -TargetDir $targetDir -Config $Config

Write-Host '==> Copying third-party runtime DLLs'
$otherRuntimeDirs = @($targetDir, (Join-Path $qtRoot 'bin'),
  (Join-Path $env:WINDIR 'System32'), (Join-Path $env:WINDIR 'SysWOW64'))
$openCvRoot = $OpenCvDir
$hasCachedOpenCvDir = $false
if (-not $openCvRoot) {
  # Match the runtime to the actual build rather than an unrelated installation.
  $sourceCache = Join-Path $sourceDir 'CMakeCache.txt'
  if (Test-Path -LiteralPath $sourceCache) {
    foreach ($line in (Get-Content -LiteralPath $sourceCache)) {
      if ($line -match '^OpenCV_DIR:[^=]+=(.+)$') {
        $hasCachedOpenCvDir = $true
        $cachedOpenCvRoot = $Matches[1]
        foreach ($relativeBin in @('bin', 'x64\vc17\bin', 'x64\vc16\bin')) {
          if (Test-Path -LiteralPath (Join-Path $cachedOpenCvRoot $relativeBin)) {
            $openCvRoot = $cachedOpenCvRoot
            break
          }
        }
        # vcpkg caches point to share/opencv4, whose runtime is handled below
        # using VcpkgInstalledDir and the selected configuration/triplet.
        break
      }
    }
  }
}
if (-not $openCvRoot -and -not $hasCachedOpenCvDir) {
  $openCvRoot = [Environment]::GetEnvironmentVariable('OpenCV_DIR')
}
if ($openCvRoot -and (Test-Path (Join-Path $openCvRoot 'OpenCVConfig.cmake'))) {
  $binCandidates = @(
    (Join-Path $openCvRoot 'x64\vc17\bin'),
    (Join-Path $openCvRoot 'x64\vc16\bin'),
    (Join-Path $openCvRoot 'bin')
  )
  $copied = 0
  foreach ($binDir in $binCandidates) {
    if (-not (Test-Path $binDir)) { continue }
    if (-not (Get-ChildItem -LiteralPath $binDir -Filter 'opencv_*.dll' -File)) { continue }
    $dlls = @(Get-RequiredRuntimeDlls -Executable $sourceExe -RuntimeDir $binDir -Dumpbin $dumpbin -OtherRuntimeDirs $otherRuntimeDirs)
    $dlls | Copy-Item -Force -Destination $targetDir
    $copied = $dlls.Count
    Write-Host ("    OpenCV  : {0} ({1} DLLs)" -f $binDir, $copied)
    break
  }
  if ($copied -eq 0) {
    throw "No OpenCV runtime DLL was found under $openCvRoot."
  }
} else {
  $vcpkgRuntimeDir = Resolve-VcpkgRuntimeDir -RepoRoot $repoRoot -SourceDir $sourceDir `
    -InstalledDir $VcpkgInstalledDir -Triplet $VcpkgTriplet -Config $Config
  $runtimeDlls = @(Get-RequiredRuntimeDlls -Executable $sourceExe -RuntimeDir $vcpkgRuntimeDir -Dumpbin $dumpbin -OtherRuntimeDirs $otherRuntimeDirs)
  $runtimeDlls | Copy-Item -Force -Destination $targetDir
  Write-Host ("    vcpkg   : {0} ({1} DLLs)" -f $vcpkgRuntimeDir, $runtimeDlls.Count)
}

Remove-DeployRuntimeBloat -TargetDir $targetDir

if ($stageToDist) {
  Remove-BuildArtifacts -TargetDir $targetDir
  Copy-PackageDocuments -RepoRoot $repoRoot -StageDir $targetDir -QtDir $qtRoot `
    -SourceExe $sourceExe -VcpkgInstalledDir $VcpkgInstalledDir -VcpkgTriplet $VcpkgTriplet -OpenCvRoot $openCvRoot
}

if ($Zip) {
  $outputRoot = Split-Path -Parent $targetDir
  New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
  $zipPath = Join-Path $outputRoot ("ntscreenshot-{0}-{1}.zip" -f $Platform, $Config)
  if (Test-Path $zipPath) { Remove-Item -Force -LiteralPath $zipPath }
  Compress-Archive -Path (Join-Path $targetDir '*') -DestinationPath $zipPath -CompressionLevel Optimal -Force
  $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash.ToLowerInvariant()
  "$hash  $([IO.Path]::GetFileName($zipPath))" | Set-Content -Encoding ascii "$zipPath.sha256"
  Write-Host "==> Package archive: $zipPath"
  Write-Host ("    ZIP size : {0:N2} MiB" -f ((Get-Item -LiteralPath $zipPath).Length / 1MB))
}

$totalBytes = (Get-ChildItem $targetDir -Recurse -File -ErrorAction SilentlyContinue |
  Measure-Object Length -Sum).Sum
Write-Host '==> Deploy finished'
Write-Host ("    Size     : {0:N2} MB" -f ($totalBytes / 1MB))
Write-Host "    Run from : $targetDir"
