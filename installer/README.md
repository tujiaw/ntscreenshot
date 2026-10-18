# Windows installer

`ntscreenshot.iss` creates a current-user, x64 installer with Inno Setup 6.7 or later.
The stable AppId identifies upgrades. Install files live under
`%LocalAppData%\Programs\ntscreenshot`; settings, `ClipboardLite/history.db`, credentials,
and search indexes live outside the installation directory and are not removed on uninstall.
The desktop shortcut is opt-in. Setup uses Windows Restart Manager to ask about files in use;
it does not force-close the application.

```powershell
./scripts/build-win.ps1 -Package -Installer
# Or use an existing staged portable directory:
./scripts/package-installer-win.ps1 -PackageDir dist/ntscreenshot-x64-Release
```

The package script checks the executable version, includes app-local MSVC runtime DLLs,
and writes `ntscreenshot-VERSION-x64-Setup.exe` plus its `.sha256` file. Use `-CompilerPath`
if `ISCC.exe` is not in a standard Inno Setup directory.

GitHub's `Windows installer` workflow runs after a successful `Release` workflow. It
downloads the published portable ZIP, verifies its checksum, builds the installer,
tests installation/upgrade/uninstall on a clean runner, then uploads the installer to
the same release. Manual dispatch accepts an existing release tag for backfilling or retrying;
there is no need to move an existing tag or rebuild the application.

Pushes touching the installer run a small lifecycle fixture independently of the full
Qt/OpenCV build. These fixtures are never published. The real release installer is
tested separately before publishing; the tests never launch the application or change
the runner's real clipboard database.

The bundled `Languages/ChineseSimplified.isl` is unchanged from
[Inno Setup 6.7.1](https://github.com/jrsoftware/issrc/blob/is-6_7_1/Files/Languages/Unofficial/ChineseSimplified.isl),
maintained by Zhenghan Yang. SHA-256:
`7d544b9bb1d142cfa11f2e5d3cc8abe2e55f8e066c5124e3772675aa236e1278`.
The upstream license is retained as `Languages/LICENSE.InnoSetup.txt`.
