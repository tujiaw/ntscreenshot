param(
  [Parameter(Mandatory = $true)][string]$InstallerPath,
  [Parameter(Mandatory = $true)][string]$PackageDir,
  [Parameter(Mandatory = $true)][string]$WorkDir,
  [string]$UpgradeInstallerPath = '',
  [string]$UpgradePackageDir = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$package = (Resolve-Path -LiteralPath $PackageDir).Path
if (-not $UpgradeInstallerPath) { $UpgradeInstallerPath = $installer }
if (-not $UpgradePackageDir) { $UpgradePackageDir = $package }
$upgrade = (Resolve-Path -LiteralPath $UpgradeInstallerPath).Path
$upgradePackage = (Resolve-Path -LiteralPath $UpgradePackageDir).Path
$work = [IO.Path]::GetFullPath($WorkDir)
if (Test-Path -LiteralPath $work) { throw 'Installer test requires a new, isolated work directory.' }
$appId = '{51CD680B-9FF1-4AC2-ABA9-68E6A4B6BAF2}_is1'
$userKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$appId"
$machineKey = "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$appId"
if ((Test-Path $userKey) -or (Test-Path $machineKey)) {
  throw 'An ntscreenshot installation already exists. Run this test on a clean Windows runner.'
}
New-Item -ItemType Directory -Path $work | Out-Null
$appDir = Join-Path $work 'app'
$group = 'ntscreenshot Installer QA ' + [guid]::NewGuid().ToString('N')
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) "$group\ntscreenshot.lnk"
$profile = Join-Path $work 'userdata/ClipboardLite'
New-Item -ItemType Directory -Force -Path $profile | Out-Null
$favorites = Join-Path $profile 'history.db'
'user favorites must survive' | Set-Content -LiteralPath $favorites
$favoritesHash = (Get-FileHash -LiteralPath $favorites).Hash
$note = Join-Path $appDir 'user-added-note.txt'

function Invoke-Setup([string]$Path, [string]$Log) {
  $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/LANG=english',
    "/DIR=`"$appDir`"", "/GROUP=`"$group`"", '/MERGETASKS=!desktopicon', "/LOG=`"$Log`"")
  $process = Start-Process -FilePath $Path -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
  if ($process.ExitCode -ne 0) { throw "Setup failed: $($process.ExitCode). See $Log" }
}
function Assert-Installed([string]$Source) {
  $sourceExe = Join-Path $Source 'ntscreenshot.exe'
  $installedExe = Join-Path $appDir 'ntscreenshot.exe'
  if ((Get-FileHash $sourceExe).Hash -ne (Get-FileHash $installedExe).Hash) { throw 'Installed executable differs from the package.' }
  $version = (Get-Item $sourceExe).VersionInfo
  $expectedVersion = '{0}.{1}.{2}' -f $version.FileMajorPart, $version.FileMinorPart, $version.FileBuildPart
  $entry = Get-ItemProperty -LiteralPath $userKey
  if ($entry.DisplayVersion -ne $expectedVersion) { throw 'Uninstall entry has the wrong version.' }
  if ($entry.InstallLocation.TrimEnd('\') -ne $appDir.TrimEnd('\')) { throw 'Installer did not use the test directory.' }
  if (Test-Path $machineKey) { throw 'Installer created a machine-wide entry.' }
  if (-not (Test-Path -LiteralPath $shortcut)) { throw 'Start menu shortcut is missing.' }
  foreach ($file in @('platforms/qwindows.dll', 'sqldrivers/qsqlite.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $appDir $file))) { throw "Installed runtime is missing: $file" }
  }
}
try {
  Invoke-Setup $installer (Join-Path $work 'install.log')
  Assert-Installed $package
  'user-created file must survive' | Set-Content -LiteralPath $note
  Invoke-Setup $upgrade (Join-Path $work 'upgrade.log')
  Assert-Installed $upgradePackage
  if (-not (Test-Path -LiteralPath $note)) { throw 'Upgrade deleted an unmanaged file.' }
} finally {
  $uninstaller = Join-Path $appDir 'unins000.exe'
  if ((Test-Path $userKey) -and (Test-Path -LiteralPath $uninstaller)) {
    $entry = Get-ItemProperty -LiteralPath $userKey
    if ($entry.InstallLocation.TrimEnd('\') -ne $appDir.TrimEnd('\')) { throw 'Refusing to uninstall outside the test directory.' }
    $log = Join-Path $work 'uninstall.log'
    $process = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$log`"") `
      -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Uninstall failed: $($process.ExitCode)" }
  }
}
if (Test-Path $userKey) { throw 'Uninstall entry was not removed.' }
if (Test-Path -LiteralPath (Join-Path $appDir 'ntscreenshot.exe')) { throw 'Uninstall left the managed executable.' }
if (Test-Path -LiteralPath $shortcut) { throw 'Uninstall left the start menu shortcut.' }
if (-not (Test-Path -LiteralPath $note)) { throw 'Uninstall deleted an unmanaged file.' }
if ((Get-FileHash -LiteralPath $favorites).Hash -ne $favoritesHash) { throw 'User data changed during installation or uninstall.' }
Write-Host 'Installer smoke test passed: per-user install, upgrade, shortcuts, uninstall, data preservation.'
