param(
  [string]$SourceRoot = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $SourceRoot) {
  $SourceRoot = Join-Path $PSScriptRoot '..\src'
}
$SourceRoot = (Resolve-Path $SourceRoot).Path

# Modules that still receive the application controller through explicit
# constructor injection. Keep this boundary explicit so it can be replaced by
# narrower interfaces without allowing hidden service-locator dependencies.
$legacyAppIncludes = @(
  'modules/capture/CaptureModule.cpp',
  'modules/capture/long_screenshot/LongScreenshotWidget.cpp',
  'modules/capture/pin/PinWidget.cpp',
  'modules/capture/screenshot/Screenshot.cpp',
  'modules/capture/screenshot/ScreenshotActionController.cpp',
  'modules/settings/Settings.cpp'
)

$legacyCrossModuleIncludes = @(
  'modules/settings/Settings.cpp->assistant',
  'modules/settings/Settings.cpp->local_search'
)

$legacyCoreIncludes = @(
  'settings/SettingModel.cpp->modules/assistant/runtime/tools/LlmTool.h'
)

$violations = [System.Collections.Generic.List[string]]::new()
$sourceFiles = Get-ChildItem $SourceRoot -Recurse -File -Include '*.cpp', '*.h' |
  Where-Object { $_.FullName -notmatch '\\libs\\' }

foreach ($file in $sourceFiles) {
  $relative = $file.FullName.Substring($SourceRoot.Length + 1).Replace('\', '/')
  $lineNumber = 0

  foreach ($line in [IO.File]::ReadLines($file.FullName)) {
    $lineNumber++

    if ($line -match '\b(GetInstance|instance)\s*\(' -or
        $line -match '\bSingleton\s*<') {
      $violations.Add("${relative}:${lineNumber}: application singletons are forbidden; inject the dependency from app/main.cpp")
    }

    if ($line -notmatch '^\s*#include\s+"([^"]+)"') {
      continue
    }

    $include = $matches[1]
    if ($relative.StartsWith('core/') -and
        $include -match '^(app|modules|shared)/') {
      $coreRelative = $relative.Substring('core/'.Length)
      if ($legacyCoreIncludes -notcontains "${coreRelative}->${include}") {
        $violations.Add("${relative}:${lineNumber}: core cannot include $include")
      }
      continue
    }

    if ($relative.StartsWith('shared/') -and
        $include -match '^(app|modules)/') {
      $violations.Add("${relative}:${lineNumber}: shared cannot include $include")
      continue
    }

    if ($relative -match '^modules/([^/]+)/') {
      $sourceModule = $matches[1]

      if ($include.StartsWith('app/') -and $legacyAppIncludes -notcontains $relative) {
        $violations.Add("${relative}:${lineNumber}: modules cannot include app internals ($include)")
        continue
      }

      if ($include -match '^modules/([^/]+)/') {
        $targetModule = $matches[1]
        $exception = "${relative}->${targetModule}"
        if ($targetModule -ne $sourceModule -and
            $legacyCrossModuleIncludes -notcontains $exception) {
          $violations.Add("${relative}:${lineNumber}: module '$sourceModule' cannot include module '$targetModule'")
        }
      }
    }
  }
}

if ($violations.Count -gt 0) {
  $violations | ForEach-Object { Write-Error $_ }
  exit 1
}

Write-Host 'Architecture dependency check passed.'
