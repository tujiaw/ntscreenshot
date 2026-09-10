param(
  [ValidateSet('Debug','Release')]
  [string]$Config = 'Release',

  [ValidateSet('x64','Win32')]
  [string]$Platform = 'x64',

  [string]$QtDir = '',

  [string]$CMakeExe = '',

  [string]$NinjaExe = '',

  [string]$VcpkgRoot = '',

  [string]$OpenCvDir = '',

  [ValidateSet('x64-windows','x86-windows')]
  [string]$VcpkgTriplet = 'x64-windows',

  [switch]$Deploy,

  [switch]$Clean,

  [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-RepoRoot {
  return (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}

function Get-EnvValue([string]$Name) {
  # Read an environment variable from the current process first, then fall back to
  # the persistent User and Machine scopes so dependencies are still found when the
  # script is launched from a shell that does not inherit them (e.g. a non-VS prompt).
  foreach ($scope in @([System.EnvironmentVariableTarget]::Process,
                       [System.EnvironmentVariableTarget]::User,
                       [System.EnvironmentVariableTarget]::Machine)) {
    $value = [Environment]::GetEnvironmentVariable($Name, $scope)
    if ($value) { return $value }
  }
  return ''
}

function Get-DefaultQtDirs {
  # Return MSVC Qt 6 kits installed under the default Qt layout (C:\Qt\<ver>\msvc*),
  # newest version first, as candidate roots that expose a Qt6 CMake package.
  $kits = [System.Collections.Generic.List[object]]::new()
  $qtInstallRoot = 'C:\Qt'
  if (-not (Test-Path $qtInstallRoot)) { return @() }
  foreach ($versionDir in (Get-ChildItem -Path $qtInstallRoot -Directory -ErrorAction SilentlyContinue)) {
    foreach ($kitDir in (Get-ChildItem -Path $versionDir.FullName -Directory -ErrorAction SilentlyContinue)) {
      if ($kitDir.Name -notlike 'msvc*') { continue }
      if (-not (Test-Path (Join-Path $kitDir.FullName 'lib\cmake\Qt6\Qt6Config.cmake'))) { continue }
      $version = $null
      [void][version]::TryParse($versionDir.Name, [ref]$version)
      $kits.Add([pscustomobject]@{
          Path    = $kitDir.FullName
          Version = $version
          Kit     = $kitDir.Name
        })
    }
  }
  return @($kits | Sort-Object -Property @{Expression = 'Version'; Descending = $true},
      @{Expression = 'Kit'; Descending = $true} | Select-Object -ExpandProperty Path)
}

function Resolve-QtDir([string]$QtDir) {
  $candidates = [System.Collections.Generic.List[string]]::new()
  if ($QtDir) { $candidates.Add($QtDir) }
  foreach ($envName in @('QTDIR', 'Qt6_DIR')) {
    $value = Get-EnvValue $envName
    if ($value) { $candidates.Add($value) }
  }
  foreach ($dir in (Get-DefaultQtDirs)) { $candidates.Add($dir) }
  $candidates.Add('C:\Qt\6.8.3\msvc2022_64')
  $candidates.Add('C:\Qt\6.8.2\msvc2022_64')

  $seen = @{}
  foreach ($candidate in $candidates) {
    if (-not $candidate) { continue }
    $full = $candidate
    if (-not [IO.Path]::IsPathRooted($full)) { $full = Join-Path (Get-Location) $candidate }
    $key = $full.TrimEnd('\').ToLowerInvariant()
    if ($seen.ContainsKey($key)) { continue }
    $seen[$key] = $true
    if (Test-Path (Join-Path $full 'lib\cmake\Qt6\Qt6Config.cmake')) {
      return (Resolve-Path $full).Path
    }
  }
  throw 'Cannot find a Qt 6 (MSVC) installation. Install under C:\Qt, or pass -QtDir / set QTDIR.'
}

function Resolve-Tool([string]$Explicit, [string[]]$Candidates, [string]$Name) {
  if ($Explicit -and (Test-Path $Explicit)) {
    return (Resolve-Path $Explicit).Path
  }
  $cmd = Get-Command $Name -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  foreach ($candidate in $Candidates) {
    if (Test-Path $candidate) {
      return (Resolve-Path $candidate).Path
    }
  }
  throw "Cannot find $Name. Pass -CMakeExe / -NinjaExe or add it to PATH."
}

function Get-DefaultVcpkgDirs {
  # Well-known default vcpkg checkouts, so a zero-argument build can find a clone
  # even when VCPKG_ROOT is not set.
  $roots = @()
  foreach ($candidate in @(
      'C:\vcpkg',
      'C:\tools\vcpkg',
      'C:\dev\vcpkg',
      (Join-Path $env:USERPROFILE 'vcpkg')
    )) {
    if ($candidate -and (Test-Path (Join-Path $candidate 'scripts\buildsystems\vcpkg.cmake'))) {
      $roots += (Resolve-Path $candidate).Path
    }
  }
  return $roots
}

function Resolve-VcpkgRoot([string]$Explicit) {
  $candidates = [System.Collections.Generic.List[string]]::new()
  if ($Explicit) { $candidates.Add($Explicit) }
  $envRoot = Get-EnvValue 'VCPKG_ROOT'
  if ($envRoot) { $candidates.Add($envRoot) }
  foreach ($dir in (Get-DefaultVcpkgDirs)) { $candidates.Add($dir) }

  $seen = @{}
  foreach ($candidate in $candidates) {
    if (-not $candidate) { continue }
    $key = $candidate.TrimEnd('\').ToLowerInvariant()
    if ($seen.ContainsKey($key)) { continue }
    $seen[$key] = $true
    if (Test-Path (Join-Path $candidate 'scripts\buildsystems\vcpkg.cmake')) {
      return (Resolve-Path $candidate).Path
    }
  }
  throw 'Cannot find vcpkg. Pass -VcpkgRoot or set VCPKG_ROOT, or use -OpenCvDir for a local OpenCV installation.'
}

function Get-DefaultOpenCvDirs {
  # Locate locally-built OpenCV trees from their common default install locations.
  $found = [System.Collections.Generic.List[string]]::new()
  foreach ($candidate in @(
      'C:\opencv\build',
      'C:\tools\opencv\build',
      'C:\deps\opencv\build',
      'C:\dev\opencv\build',
      (Join-Path $env:USERPROFILE 'opencv\build')
    )) {
    if ($candidate -and (Test-Path (Join-Path $candidate 'OpenCVConfig.cmake'))) {
      $found.Add((Resolve-Path $candidate).Path)
    }
  }
  # Layout of an official source clone: <profile>\opencv\<repo>\build
  $profileOpenCv = Join-Path $env:USERPROFILE 'opencv'
  if (Test-Path $profileOpenCv) {
    foreach ($repo in (Get-ChildItem -Path $profileOpenCv -Directory -ErrorAction SilentlyContinue)) {
      $candidate = Join-Path $repo.FullName 'build'
      if (Test-Path (Join-Path $candidate 'OpenCVConfig.cmake')) {
        $found.Add((Resolve-Path $candidate).Path)
      }
    }
  }
  return @($found)
}

function Resolve-OpenCvDir([string]$Explicit) {
  $candidates = [System.Collections.Generic.List[string]]::new()
  if ($Explicit) { $candidates.Add($Explicit) }
  $envDir = Get-EnvValue 'OpenCV_DIR'
  if ($envDir) { $candidates.Add($envDir) }
  foreach ($dir in (Get-DefaultOpenCvDirs)) { $candidates.Add($dir) }

  $seen = @{}
  foreach ($candidate in $candidates) {
    if (-not $candidate) { continue }
    $full = $candidate
    if (-not [IO.Path]::IsPathRooted($full)) { $full = Join-Path (Get-Location) $candidate }
    $key = $full.TrimEnd('\').ToLowerInvariant()
    if ($seen.ContainsKey($key)) { continue }
    $seen[$key] = $true
    if (Test-Path (Join-Path $full 'OpenCVConfig.cmake')) {
      return (Resolve-Path $full).Path
    }
  }
  throw 'Cannot find an OpenCV build (OpenCVConfig.cmake). Pass -OpenCvDir or set OpenCV_DIR.'
}

function Get-VsInstallationPaths {
  # List every VS installation (any edition, including BuildTools) whose
  # VsDevCmd.bat is present, newest first. Uses vswhere when available and falls
  # back to scanning the well-known installation roots.
  $installations = [System.Collections.Generic.List[object]]::new()

  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (-not (Test-Path $vswhere)) {
    $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
  }
  if (Test-Path $vswhere) {
    # vswhere does not guarantee a newest-first ordering, so sort on the reported
    # installationVersion explicitly. Taking its first entry as-is can select a
    # stale BuildTools install, whose old CMake then fails the minimum version check.
    $instances = & $vswhere -all -products * -requires Microsoft.Component.MSBuild -format json |
      ConvertFrom-Json
    foreach ($instance in @($instances)) {
      if (-not $instance) { continue }
      $path = $instance.installationPath
      if (-not $path) { continue }
      if (-not (Test-Path (Join-Path $path.Trim() 'Common7\Tools\VsDevCmd.bat'))) { continue }
      $version = $null
      [void][version]::TryParse($instance.installationVersion, [ref]$version)
      $installations.Add([pscustomobject]@{ Path = $path.Trim(); Version = $version })
    }
  }

  if ($installations.Count -eq 0) {
    # vswhere ships with full Visual Studio but not necessarily with BuildTools-only
    # installs, so scan the default roots directly as a fallback. The year in the
    # install path orders these the same way installationVersion orders the rest.
    foreach ($base in @((Join-Path $env:ProgramFiles 'Microsoft Visual Studio'),
                        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio'))) {
      if (-not (Test-Path $base)) { continue }
      foreach ($year in (Get-ChildItem -Path $base -Directory -ErrorAction SilentlyContinue)) {
        foreach ($edition in (Get-ChildItem -Path $year.FullName -Directory -ErrorAction SilentlyContinue)) {
          if (-not (Test-Path (Join-Path $edition.FullName 'Common7\Tools\VsDevCmd.bat'))) { continue }
          $version = $null
          [void][version]::TryParse($year.Name, [ref]$version)
          $installations.Add([pscustomobject]@{ Path = $edition.FullName; Version = $version })
        }
      }
    }
  }

  return @($installations |
      Sort-Object -Property @{Expression = 'Version'; Descending = $true} |
      Select-Object -ExpandProperty Path)
}

function Get-VsInstallationPath {
  $installations = @(Get-VsInstallationPaths)
  if ($installations.Count -eq 0) { return '' }
  # Newest installation wins; any of them works as long as it exposes VsDevCmd.bat.
  return $installations[0]
}

function Get-VsDevCmd {
  $installationPath = Get-VsInstallationPath
  if (-not $installationPath) { return '' }
  $candidate = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'
  if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
  return ''
}

function Quote-Cmd([string]$Value) {
  return '"' + ($Value -replace '"', '""') + '"'
}

function To-CMakePath([string]$Value) {
  return ($Value -replace '\\', '/')
}

$repoRoot = Resolve-RepoRoot

# Put the VS Installer directory on PATH so VsDevCmd/vcvars can resolve vswhere.exe
# even when this script is run from a non-developer shell.
$vsInstallerDir = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
if (Test-Path $vsInstallerDir) {
  $env:PATH = $vsInstallerDir + ';' + $env:PATH
}

$qtRoot = Resolve-QtDir -QtDir $QtDir
$vsInstallPath = Get-VsInstallationPath
$cmakeCandidates = @(
  (Join-Path $qtRoot '..\..\Tools\CMake_64\bin\cmake.exe'),
  'C:\Qt\Tools\CMake_64\bin\cmake.exe'
)
$ninjaCandidates = @(
  (Join-Path $qtRoot '..\..\Tools\Ninja\ninja.exe'),
  'C:\Qt\Tools\Ninja\ninja.exe'
)
if ($vsInstallPath) {
  $cmakeCandidates += Join-Path $vsInstallPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
  $ninjaCandidates += Join-Path $vsInstallPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
}
$cmake = Resolve-Tool -Explicit $CMakeExe -Candidates $cmakeCandidates -Name 'cmake.exe'
$ninja = Resolve-Tool -Explicit $NinjaExe -Candidates $ninjaCandidates -Name 'ninja.exe'
# Prefer a locally installed OpenCV (explicit -OpenCvDir, an OpenCV_DIR env var, or a
# default install location); fall back to a vcpkg checkout when none can be found.
$openCvRoot = ''
$vcpkg = ''
if ($OpenCvDir -or (Get-EnvValue 'OpenCV_DIR')) {
  $openCvRoot = Resolve-OpenCvDir -Explicit $OpenCvDir
} else {
  $defaultOpenCvDirs = @(Get-DefaultOpenCvDirs)
  if ($defaultOpenCvDirs.Count -gt 0) {
    $openCvRoot = $defaultOpenCvDirs[0]
  } else {
    $vcpkg = Resolve-VcpkgRoot -Explicit $VcpkgRoot
  }
}
$vsDevCmd = Get-VsDevCmd
$buildDir = Join-Path $repoRoot ("build\cmake-{0}-{1}" -f $Platform, $Config)

if ($Clean -and (Test-Path $buildDir)) {
  Remove-Item -Recurse -Force $buildDir
}

if ($Reconfigure -and (Test-Path $buildDir)) {
  Remove-Item -Recurse -Force (Join-Path $buildDir 'CMakeCache.txt') -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$configureArgs = @(
  '-S', (Quote-Cmd $repoRoot),
  '-B', (Quote-Cmd $buildDir),
  '-G', 'Ninja',
  ('-DCMAKE_BUILD_TYPE={0}' -f $Config),
  ('-DCMAKE_PREFIX_PATH={0}' -f (Quote-Cmd (To-CMakePath $qtRoot)))
)
if ($openCvRoot) {
  $configureArgs += ('-DOpenCV_DIR={0}' -f (Quote-Cmd (To-CMakePath $openCvRoot)))
} else {
  $configureArgs += (
    ('-DCMAKE_TOOLCHAIN_FILE={0}' -f (Quote-Cmd (To-CMakePath (Join-Path $vcpkg 'scripts\buildsystems\vcpkg.cmake')))),
    ('-DVCPKG_TARGET_TRIPLET={0}' -f $VcpkgTriplet)
  )
}

$buildArgs = @(
  '--build', (Quote-Cmd $buildDir),
  '--config', $Config
)

Write-Host '==> Building ntscreenshot (CMake + Ninja)'
Write-Host "    Config   : $Config"
Write-Host "    Platform : $Platform"
Write-Host "    Qt       : $qtRoot"
Write-Host "    CMake    : $cmake"
Write-Host "    Ninja    : $ninja"
if ($vsDevCmd) {
  Write-Host "    VS dev   : $vsDevCmd"
} else {
  Write-Host "    VS dev   : (not found; compiler from PATH / CMake cache)"
}
if ($openCvRoot) {
  Write-Host "    OpenCV   : $openCvRoot"
} else {
  Write-Host "    vcpkg    : $vcpkg ($VcpkgTriplet)"
}
Write-Host "    BuildDir : $buildDir"

$cmdSteps = @()
if ($vsDevCmd) {
  $hostArch = if ([Environment]::Is64BitOperatingSystem) { 'x64' } else { 'x86' }
  $targetArch = if ($Platform -eq 'x64') { 'x64' } else { 'x86' }
  $cmdSteps += ('call "{0}" -no_logo -host_arch={1} -arch={2}' -f $vsDevCmd, $hostArch, $targetArch)
} else {
  Write-Warning 'No Visual Studio developer environment (VsDevCmd.bat) was found; relying on a compiler already available on PATH or cached by CMake.'
}
$cmdSteps += ('"{0}" {1}' -f $cmake, ($configureArgs -join ' '))
$cmdSteps += ('"{0}" {1}' -f $cmake, ($buildArgs -join ' '))
$cmdLine = $cmdSteps -join ' && '

& cmd.exe /d /s /c $cmdLine
if ($LASTEXITCODE -ne 0) {
  throw "CMake build failed with exit code $LASTEXITCODE."
}

$exePath = Join-Path $buildDir 'ntscreenshot.exe'
Write-Host '==> Build finished'
Write-Host "    Output   : $exePath"
if ($Deploy) {
  Write-Host '==> Deploying runtime dependencies'
  $deployParams = @{
    Config = $Config
    Platform = $Platform
    QtDir = $qtRoot
    ExePath = $exePath
    InPlace = $true
  }
  if ($openCvRoot) {
    $deployParams.OpenCvDir = $openCvRoot
  } else {
    $deployParams.VcpkgInstalledDir = Join-Path $buildDir 'vcpkg_installed'
    $deployParams.VcpkgTriplet = $VcpkgTriplet
  }
  & (Join-Path $PSScriptRoot 'package-win.ps1') @deployParams
}
