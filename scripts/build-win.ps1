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

function Resolve-QtDir([string]$QtDir) {
  $candidates = @()
  if ($QtDir) { $candidates += $QtDir }
  foreach ($envName in @('QTDIR', 'Qt6_DIR')) {
    $value = [Environment]::GetEnvironmentVariable($envName)
    if ($value) { $candidates += $value }
  }
  $candidates += @(
    'C:\Qt\6.8.3\msvc2022_64',
    'C:\Qt\6.8.2\msvc2022_64'
  )
  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path (Join-Path $candidate 'lib\cmake\Qt6\Qt6Config.cmake'))) {
      return (Resolve-Path $candidate).Path
    }
  }
  throw 'Cannot find Qt 6 MSVC installation. Set -QtDir or QTDIR.'
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

function Resolve-VcpkgRoot([string]$Explicit) {
  $candidates = @()
  if ($Explicit) { $candidates += $Explicit }
  $environmentRoot = [Environment]::GetEnvironmentVariable('VCPKG_ROOT')
  if ($environmentRoot) { $candidates += $environmentRoot }
  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path (Join-Path $candidate 'scripts\buildsystems\vcpkg.cmake'))) {
      return (Resolve-Path $candidate).Path
    }
  }
  throw 'Cannot find vcpkg. Pass -VcpkgRoot or set VCPKG_ROOT, or use -OpenCvDir for a local OpenCV installation.'
}

function Resolve-OpenCvDir([string]$Explicit) {
  $candidates = @()
  if ($Explicit) { $candidates += $Explicit }
  $environmentDir = [Environment]::GetEnvironmentVariable('OpenCV_DIR')
  if ($environmentDir) { $candidates += $environmentDir }
  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path (Join-Path $candidate 'OpenCVConfig.cmake'))) {
      return (Resolve-Path $candidate).Path
    }
  }
  throw 'Cannot find OpenCVConfig.cmake. Pass -OpenCvDir, for example C:\deps\opencv\build.'
}

function Get-VsInstallationPath {
  $vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (-not (Test-Path $vsWhere)) { return '' }
  $installationPath = & $vsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
  if ($installationPath) { return $installationPath.Trim() }
  return ''
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
$useLocalOpenCv = [bool]$OpenCvDir -or [bool][Environment]::GetEnvironmentVariable('OpenCV_DIR')
$openCvRoot = ''
$vcpkg = ''
if ($useLocalOpenCv) {
  $openCvRoot = Resolve-OpenCvDir -Explicit $OpenCvDir
} else {
  $vcpkg = Resolve-VcpkgRoot -Explicit $VcpkgRoot
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
