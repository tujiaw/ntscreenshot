param(
  [string]$Root = '',
  [string]$Tag = '',
  [int]$MaxTrackedFileMiB = 50
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $Root) { $Root = Join-Path $PSScriptRoot '..' }
$repoRoot = (Resolve-Path $Root).Path
$failures = [Collections.Generic.List[string]]::new()

function Add-Failure([string]$Message) {
  $script:failures.Add($Message)
}

function Get-TrackedFiles {
  $output = & git -C $repoRoot ls-files -z
  if ($LASTEXITCODE -ne 0) { throw "git ls-files failed in $repoRoot" }
  return @($output -split "`0" | Where-Object { $_ })
}

$trackedFiles = Get-TrackedFiles
$requiredFiles = @(
  'README.md', 'README.en.md', 'LICENSE', 'CONTRIBUTING.md', 'SECURITY.md', 'PRIVACY.md',
  'CODE_OF_CONDUCT.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md',
  'CMakeLists.txt', 'vcpkg.json'
)

foreach ($required in $requiredFiles) {
  if ($required -notin $trackedFiles) { Add-Failure "Required file is not tracked: $required" }
}

$forbiddenDirectoryPattern = '(^|/)(build|dist|vcpkg_installed|buildtrees|packages|\.vs|\.cache)(/|$)'
$forbiddenExtensionPattern = '\.(exe|dll|lib|pdb|ilk|obj|pch|zip|7z|rar|tar|gz|dmp)$'
$sensitiveNamePattern = '(^|/)(\.env($|\.)|id_rsa($|\.)|credentials\.json$|secrets?\.(json|ya?ml|ini)$|base\.ini$|history\.db$|.*\.(pem|pfx|p12|key)$)'
$maxBytes = $MaxTrackedFileMiB * 1MB

foreach ($relativePath in $trackedFiles) {
  $normalized = $relativePath -replace '\\', '/'
  if ($normalized -match $forbiddenDirectoryPattern) { Add-Failure "Forbidden generated directory: $relativePath" }
  if ($normalized -match $forbiddenExtensionPattern) { Add-Failure "Forbidden binary or archive: $relativePath" }
  if ($normalized -match $sensitiveNamePattern -and $normalized -notmatch '\.env\.example$') {
    Add-Failure "Sensitive filename: $relativePath"
  }

  $fullPath = Join-Path $repoRoot $relativePath
  if ((Test-Path -LiteralPath $fullPath) -and (Get-Item -LiteralPath $fullPath).Length -gt $maxBytes) {
    Add-Failure "Tracked file exceeds $MaxTrackedFileMiB MiB: $relativePath"
  }
}

$textExtensions = @('.md', '.txt', '.json', '.yml', '.yaml', '.cmake', '.ps1', '.sh', '.cpp', '.h', '.hpp')
$privateKeyMarker = 'BEGIN ' + 'PRIVATE KEY'
$githubTokenMarker = 'gh' + 'p_[A-Za-z0-9]{30,}'
$awsKeyMarker = 'AK' + 'IA[0-9A-Z]{16}'

foreach ($relativePath in $trackedFiles) {
  $fullPath = Join-Path $repoRoot $relativePath
  if (-not (Test-Path -LiteralPath $fullPath)) { continue }
  if ([IO.Path]::GetExtension($relativePath).ToLowerInvariant() -notin $textExtensions) { continue }
  $content = Get-Content -Raw -LiteralPath $fullPath -ErrorAction SilentlyContinue
  if ($null -eq $content) { continue }
  if ($content -match [regex]::Escape($privateKeyMarker) -or
      $content -match $githubTokenMarker -or
      $content -match $awsKeyMarker) {
    Add-Failure "Possible credential material: $relativePath"
  }
}

foreach ($relativePath in $trackedFiles | Where-Object { $_ -match '\.md$' }) {
  $fullPath = Join-Path $repoRoot $relativePath
  if (-not (Test-Path -LiteralPath $fullPath)) { continue }
  $content = Get-Content -Raw -LiteralPath $fullPath
  $matches = [regex]::Matches(
    $content,
    '(?<![A-Za-z0-9_\\/])!?(?:\[[^\]\r\n]*\])\((<[^>]+>|[^)\s]+)(?:\s+"[^"]*")?\)'
  )
  foreach ($match in $matches) {
    $target = $match.Groups[1].Value.Trim()
    if ($target -match '^(https?://|mailto:|#)') { continue }
    if ($target.StartsWith('<') -and $target.EndsWith('>')) { $target = $target.Substring(1, $target.Length - 2) }
    $target = ($target -split '#', 2)[0]
    if (-not $target) { continue }
    $target = [Uri]::UnescapeDataString($target)
    $resolved = Join-Path (Split-Path -Parent $fullPath) $target
    if (-not (Test-Path -LiteralPath $resolved)) {
      Add-Failure "Broken local Markdown link in ${relativePath}: $target"
    }
  }
}

$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'
$manifestPath = Join-Path $repoRoot 'vcpkg.json'
$changelogPath = Join-Path $repoRoot 'CHANGELOG.md'
$cmakeContent = ''
if (Test-Path -LiteralPath $cmakePath) {
  $cmakeContent = Get-Content -Raw -LiteralPath $cmakePath
}
$cmakeMatch = [regex]::Match(
  $cmakeContent,
  'project\s*\(\s*ntscreenshot\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)',
  [Text.RegularExpressions.RegexOptions]::IgnoreCase
)
$cmakeVersion = ''
if ($cmakeMatch.Success) {
  $cmakeVersion = $cmakeMatch.Groups[1].Value
}

if ($cmakeVersion) {
  if (-not (Test-Path -LiteralPath $manifestPath)) {
    Add-Failure 'vcpkg.json is missing; cannot verify version-semver.'
  } else {
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $manifestVersion = $manifest.'version-semver'
    if (-not $manifestVersion) {
      Add-Failure 'vcpkg.json does not define version-semver.'
    } elseif ($manifestVersion -ne $cmakeVersion) {
      Add-Failure "Version mismatch: CMake=$cmakeVersion, vcpkg.json=$manifestVersion."
    }
  }

  if (-not (Test-Path -LiteralPath $changelogPath)) {
    Add-Failure 'CHANGELOG.md is missing; cannot verify the version heading.'
  } else {
    $changelogContent = Get-Content -Raw -LiteralPath $changelogPath
    $headingPattern = '(?m)^## \[' + [regex]::Escape($cmakeVersion) + '\](?:\s|$)'
    if ($changelogContent -notmatch $headingPattern) {
      Add-Failure "CHANGELOG.md has no heading for $cmakeVersion."
    }
  }
} elseif ($Tag) {
  Add-Failure 'Cannot read the ntscreenshot project version from CMakeLists.txt.'
}

if ($Tag) {
  if ($Tag -match '^v([0-9]+\.[0-9]+\.[0-9]+)$') {
    $tagVersion = $Matches[1]
    if ($cmakeVersion -and $tagVersion -ne $cmakeVersion) {
      Add-Failure "Tag version $tagVersion does not match project version $cmakeVersion."
    }
  } else {
    Add-Failure "Release tag must use vMAJOR.MINOR.PATCH: $Tag"
  }
}

if ($failures.Count -gt 0) {
  Write-Host "Repository check failed with $($failures.Count) issue(s):" -ForegroundColor Red
  $failures | Sort-Object -Unique | ForEach-Object { Write-Host "  - $_" }
  exit 1
}

Write-Host "Repository check passed for $($trackedFiles.Count) tracked files." -ForegroundColor Green
