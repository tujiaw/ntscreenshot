param(
  [Parameter(Mandatory = $true)]
  [string]$Checker,
  [Parameter(Mandatory = $true)]
  [string]$Root
)

$ErrorActionPreference = 'Stop'

$cmakeContent = Get-Content -Raw -LiteralPath (Join-Path $Root 'CMakeLists.txt')
$versionMatch = [regex]::Match($cmakeContent, '(?im)^\s*project\s*\(\s*ntscreenshot\s+VERSION\s+(\d+)\.(\d+)\.(\d+)')
if (-not $versionMatch.Success) {
  throw 'Cannot read the project version from CMakeLists.txt.'
}
$major = [int]$versionMatch.Groups[1].Value
$minor = [int]$versionMatch.Groups[2].Value
$patch = [int]$versionMatch.Groups[3].Value
$goodTag = "v$major.$minor.$patch"
$badTag = "v$major.$minor.$($patch + 1)"

$ErrorActionPreference = 'Continue'
$badOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $Checker -Root $Root -Tag $badTag 2>&1
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -eq 0) {
  throw "Release checker accepted a mismatched tag:`n$($badOutput -join "`n")"
}

$ErrorActionPreference = 'Continue'
$goodOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $Checker -Root $Root -Tag $goodTag 2>&1
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) {
  throw "Release checker rejected the project version:`n$($goodOutput -join "`n")"
}

Write-Host 'Release checker red/green behavior passed.'
