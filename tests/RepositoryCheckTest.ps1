param(
  [Parameter(Mandatory = $true)]
  [string]$Checker
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture = Join-Path $tempRoot ("ntscreenshot-repository-check-{0}" -f [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null

function Invoke-Git([string[]]$Arguments) {
  & git -C $fixture @Arguments | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "git command failed: $($Arguments -join ' ')" }
}

function Write-FixtureFile([string]$RelativePath, [string]$Content) {
  $path = Join-Path $fixture $RelativePath
  $directory = Split-Path -Parent $path
  if ($directory -and -not (Test-Path $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
  }
  Set-Content -LiteralPath $path -Value $Content -Encoding UTF8
}

try {
  Invoke-Git @('init', '-b', 'main')
  Write-FixtureFile 'README.md' '[broken](missing.md)'
  Write-FixtureFile '.env' 'EXAMPLE=value'
  Invoke-Git @('add', '.')

  $badOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $Checker -Root $fixture 2>&1
  if ($LASTEXITCODE -eq 0) { throw 'Repository check unexpectedly accepted the invalid fixture.' }
  $badText = $badOutput -join "`n"
  if ($badText -notmatch 'Sensitive filename' -or $badText -notmatch 'Broken local Markdown link') {
    throw "Repository check did not report the expected failures:`n$badText"
  }

  Remove-Item -LiteralPath (Join-Path $fixture '.env')
  $required = @(
    'README.md', 'README.en.md', 'LICENSE', 'CONTRIBUTING.md', 'SECURITY.md', 'PRIVACY.md',
    'CODE_OF_CONDUCT.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md',
    'CMakeLists.txt', 'vcpkg.json'
  )
  foreach ($file in $required) { Write-FixtureFile $file '# fixture' }
  Write-FixtureFile 'README.md' '[license](LICENSE)'
  Invoke-Git @('add', '-A')

  $goodOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $Checker -Root $fixture 2>&1
  if ($LASTEXITCODE -ne 0) { throw "Repository check rejected the valid fixture:`n$($goodOutput -join "`n")" }
  Write-Host 'Repository check negative and positive fixtures passed.'
} finally {
  $resolvedFixture = [IO.Path]::GetFullPath($fixture)
  if (-not $resolvedFixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove unexpected fixture path: $resolvedFixture"
  }
  Remove-Item -Recurse -Force -LiteralPath $resolvedFixture -ErrorAction SilentlyContinue
}
