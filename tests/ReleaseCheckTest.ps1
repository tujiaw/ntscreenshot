param(
  [Parameter(Mandatory = $true)]
  [string]$Checker,
  [Parameter(Mandatory = $true)]
  [string]$Root
)

$ErrorActionPreference = 'Stop'

$ErrorActionPreference = 'Continue'
$badOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $Checker -Root $Root -Tag v9.9.9 2>&1
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -eq 0) {
  throw "Release checker accepted a mismatched tag:`n$($badOutput -join "`n")"
}

$ErrorActionPreference = 'Continue'
$goodOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $Checker -Root $Root -Tag v0.1.0 2>&1
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) {
  throw "Release checker rejected the project version:`n$($goodOutput -join "`n")"
}

Write-Host 'Release checker red/green behavior passed.'
