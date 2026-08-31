# 从源码构建

[English](building.en.md)

CMake 是唯一维护中的构建系统。当前只支持和发布 Windows 10/11 x64。所有依赖和构建产物都应保留在源码仓库外或已忽略的目录中。

## Windows

需要：

- Visual Studio 2022，包含「使用 C++ 的桌面开发」和 Windows SDK
- Qt 6.8.x MSVC 2022 64-bit，并勾选 WebEngine
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
