# Building ntscreenshot

[简体中文](building.md)

CMake is the only maintained build system. Windows 10/11 x64 is the only supported and released platform. Keep dependencies and generated files outside the source tree or in ignored directories.

## Windows

Install:

- Visual Studio 2022 with Desktop development with C++ and a Windows SDK
- Qt 6.8.x MSVC 2022 64-bit with WebEngine
- Git, CMake, and Ninja (from the Qt Maintenance Tool or Visual Studio)
- [OpenCV 4.14.0 prebuilt package for Windows](https://github.com/opencv/opencv/releases/download/4.14.0/opencv-4.14.0-windows.exe)

Run the OpenCV self-extracting archive, for example into `C:\deps`, and verify that `C:\deps\opencv\build\OpenCVConfig.cmake` exists. Do not commit the extracted dependencies.

### Standard CMake workflow

Run from an x64 Native Tools Command Prompt for VS 2022:

```powershell
git clone https://github.com/tujiaw/ntscreenshot.git
cd ntscreenshot

$env:QTDIR = 'C:\Qt\6.8.3\msvc2022_64'
$env:OpenCV_DIR = 'C:\deps\opencv\build'
cmake --preset windows-opencv-release
cmake --build --preset windows-opencv-release --parallel
ctest --preset windows-opencv-release
```

The executable is written to `build/cmake-x64-Release/ntscreenshot.exe`.

### Windows helper script

The helper script initializes the MSVC environment automatically and can be run from a regular PowerShell prompt:

```powershell
.\scripts\build-win.ps1 `
  -Config Release `
  -Platform x64 `
  -QtDir $env:QTDIR `
  -OpenCvDir $env:OpenCV_DIR
```

The script builds only by default. Pass `-Deploy` to deploy Qt and OpenCV runtimes next to the executable. Contributors who prefer vcpkg can still use the `windows-vcpkg-release` preset, but it builds dependencies from source.

### Package a portable build

```powershell
.\scripts\package-win.ps1 `
  -Config Release `
  -Platform x64 `
  -QtDir $env:QTDIR `
  -OpenCvDir $env:OpenCV_DIR `
  -Zip
```

The distributable directory and archive are written to `dist/`.

### Common Windows problems

- **Qt6Config.cmake not found:** point `QTDIR` at the kit root (`lib/cmake/Qt6`), not Qt Creator.
- **OpenCVConfig.cmake not found:** point `OpenCV_DIR` at `opencv\build` in the extracted package.
- **CMake or Ninja not found:** install them with Qt or Visual Studio, or pass `-CMakeExe` and `-NinjaExe`.
- **A packaged app exits immediately with code 0:** ntscreenshot is single-instance; exit the existing tray process.

See the [FAQ](faq.en.md) for runtime questions.

## Before contributing

Do not commit build directories, dependency installation trees, generated IDE files, packages, credentials, logs, clipboard databases, or local-search indexes. Before a pull request:

```powershell
ctest --preset windows-opencv-release
.\scripts\check-architecture.ps1
.\scripts\check-repository.ps1
```
