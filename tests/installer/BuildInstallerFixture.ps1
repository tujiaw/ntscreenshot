param([Parameter(Mandatory = $true)][string]$OutDir, [string]$Version = '1.2.3')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid fixture version.' }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$output = (Resolve-Path -LiteralPath $OutDir).Path
$source = Join-Path $output 'fixture.cs'
@"
using System.Reflection;
[assembly: AssemblyFileVersion("$Version.0")]
public class Fixture { public static void Main() {} }
"@ | Set-Content -LiteralPath $source -Encoding ascii
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $compiler /nologo /target:winexe /platform:x64 "/out:$output/ntscreenshot.exe" $source
if ($LASTEXITCODE -ne 0) { throw 'Cannot build installer test fixture.' }
Remove-Item -LiteralPath $source
# This fixture exercises installation semantics; it is never launched as an app.
foreach ($file in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'Qt6Sql.dll',
    'platforms/qwindows.dll', 'sqldrivers/qsqlite.dll')) {
  $path = Join-Path $output $file
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
  'installer test payload' | Set-Content -LiteralPath $path
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../../LICENSE') -Destination $output
