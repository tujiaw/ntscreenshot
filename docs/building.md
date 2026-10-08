# 从源码构建

[English](building.en.md)

CMake 是唯一维护中的构建系统。当前只支持和发布 Windows 10/11 x64。所有依赖和构建产物都应保留在源码仓库外或已忽略的目录中。

## Windows

需要：

- Visual Studio 2022，包含「使用 C++ 的桌面开发」和 Windows SDK
- Qt 6.8.x MSVC 2022 64-bit
- Git、CMake、Ninja（可来自 Qt 维护工具或 Visual Studio）
- [OpenCV 4.14.0 Windows 预编译包](https://github.com/opencv/opencv/releases/download/4.14.0/opencv-4.14.0-windows.exe)

运行 OpenCV 自解压文件，例如解压到 `C:\deps`。确认 `C:\deps\opencv\build\OpenCVConfig.cmake` 存在。不要将解压后的依赖提交到 Git。

### 标准 CMake 流程

在「x64 Native Tools Command Prompt for VS 2022」中执行：

```powershell
git clone https://github.com/tujiaw/ntscreenshot.git
cd ntscreenshot

$env:QTDIR = 'C:\Qt\6.8.3\msvc2022_64'
$env:OpenCV_DIR = 'C:\deps\opencv\build'
cmake --preset windows-opencv-release
cmake --build --preset windows-opencv-release --parallel
ctest --preset windows-opencv-release
```

构建输出为 `build/cmake-x64-Release/ntscreenshot.exe`。

### Windows 辅助脚本

辅助脚本会自动初始化 MSVC 环境，适合普通 PowerShell：

```powershell
.\scripts\build-win.ps1 `
  -Config Release `
  -Platform x64 `
  -QtDir $env:QTDIR `
  -OpenCvDir $env:OpenCV_DIR
```

脚本默认只编译。需要在构建目录旁部署 Qt 和 OpenCV 运行库时，额外传入 `-Deploy`。需要 vcpkg 的贡献者仍可使用 `windows-vcpkg-release` Preset，但该方式会从源码构建依赖。

### 生成绿色包

```powershell
.\scripts\package-win.ps1 `
  -Config Release `
  -Platform x64 `
  -QtDir $env:QTDIR `
  -OpenCvDir $env:OpenCV_DIR `
  -Zip
```

可分发目录和压缩包输出到 `dist/`。

### 缩小 Windows 压缩包

打包脚本使用 MSVC 的 `dumpbin.exe` 递归查找实际导入的第三方 DLL，避免把整个 OpenCV/vcpkg 的运行库目录带进包里。它保留 Qt 部署工具提供的插件，但不包含未使用的 OpenCV 视频插件。普通 PowerShell 中会自动定位 MSVC；也可以传入 `-DumpbinExe`。没有传入 `-OpenCvDir` 时，脚本会先从应用构建目录的 CMake 缓存读取 OpenCV 路径。

进一步缩小 OpenCV 时，可以使用预编译包附带的 `sources` 目录。在 x64 VS 开发者 PowerShell 中执行（修改源码路径以匹配本机）：

```powershell
cmake -S C:/deps/opencv/sources -B build/opencv-minimal -G Ninja `
  -C cmake/OpenCvMinimal.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/opencv-minimal --parallel
$env:OpenCV_DIR = (Resolve-Path build/opencv-minimal).Path
.\scripts\build-win.ps1 -QtDir $env:QTDIR -OpenCvDir $env:OpenCV_DIR -Reconfigure
.\scripts\package-win.ps1 -QtDir $env:QTDIR -OpenCvDir $env:OpenCV_DIR -Zip
```

该配置只构建图像处理、图像编码、特征匹配、目标检测和修复所需的模块及其依赖，并保留二维码解码和默认 CPU 优化。没有显式指定 OpenCV 路径或设置 `OpenCV_DIR` 时，构建辅助脚本会优先使用 `build/opencv-minimal` 中已编译的依赖。首次配置可能需要联网下载 OpenCV 的第三方构建依赖。重新使用不同的 OpenCV 配置时，应使用新的依赖构建目录。AI 对话和划词已迁至独立的 auto-browser 项目，截图程序无需安装或打包 Qt WebEngine。

### 常见问题

- **找不到 Qt6Config.cmake：** `QTDIR` 应指向套件根目录（含 `lib/cmake/Qt6`），不是 Qt Creator 的安装根。
- **找不到 OpenCVConfig.cmake：** `OpenCV_DIR` 应指向预编译包中的 `opencv\build`。
- **找不到 CMake / Ninja：** 用 Qt 或 VS 安装，或给脚本传 `-CMakeExe` / `-NinjaExe`。
- **打包后的程序立刻退出且退出码为 0：** ntscreenshot 只能运行一个实例，请先退出已有的托盘进程。

更多运行问题见 [FAQ](faq.md)。

## 提交前检查

不要提交构建目录、依赖安装树、IDE 生成文件、发布包、密钥、日志、剪贴板数据库或搜索索引。发 PR 前运行：

```powershell
ctest --preset windows-opencv-release
.\scripts\check-architecture.ps1
.\scripts\check-repository.ps1
```

### 一步构建并打包

```powershell
./scripts/build-win.ps1 -Package
```

输出 ZIP 与 SHA256 校验文件；`-Deploy` 保留用于本地运行库部署。
