@echo off
setlocal EnableExtensions

rem ============================================================
rem  ntscreenshot 一键编译（双击本文件即可）
rem
rem  默认：Release / x64
rem  输出：build\cmake-x64-Release\ntscreenshot.exe
rem
rem  想传别的参数，就在命令行里追加，例如：
rem    build.bat -Deploy        编译并部署 Qt/OpenCV 运行时 DLL（这样才能直接运行）
rem    build.bat -Clean         先清理再编译
rem    build.bat -Config Debug  编译 Debug 版本
rem    build.bat -Reconfigure   强制重新执行 CMake 配置
rem ============================================================

rem 本机 Qt / OpenCV 的安装位置。脚本的自动探测找不到这两个路径，
rem 所以在这里写死；换机器时只需要改下面两行。
set "QTDIR=C:\Qt\Qt6.8\6.8.3\msvc2022_64"
set "OpenCV_DIR=C:\deps\opencv\build"

set "BUILD_PS1=%~dp0scripts\build-win.ps1"
set "RC=1"

if not exist "%BUILD_PS1%" (
  echo [错误] 找不到构建脚本: %BUILD_PS1%
  echo        请确认 build.bat 放在项目根目录下。
  goto :done
)

if not exist "%QTDIR%\lib\cmake\Qt6\Qt6Config.cmake" (
  echo [错误] Qt 目录无效: %QTDIR%
  echo        请修改本脚本顶部的 QTDIR 变量。
  goto :done
)

if not exist "%OpenCV_DIR%\OpenCVConfig.cmake" (
  echo [错误] OpenCV 目录无效: %OpenCV_DIR%
  echo        请修改本脚本顶部的 OpenCV_DIR 变量。
  goto :done
)

rem 没传参数时用默认的 Release/x64；传了就把参数原样交给 build-win.ps1
set "PS_ARGS=%*"
if not defined PS_ARGS set "PS_ARGS=-Config Release -Platform x64"

echo ============================================================
echo  ntscreenshot 编译
echo    Qt     : %QTDIR%
echo    OpenCV : %OpenCV_DIR%
echo    参数   : %PS_ARGS%
echo ============================================================
echo.

rem QTDIR / OpenCV_DIR 已在本进程设好，build-win.ps1 会自己读这两个环境变量。
rem 不加 -ExecutionPolicy Bypass：本机策略为 RemoteSigned，本地脚本可直接执行。
powershell -NoProfile -File "%BUILD_PS1%" %PS_ARGS%
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
  echo [成功] 输出: %~dp0build\cmake-x64-Release\ntscreenshot.exe
) else (
  echo [失败] 编译失败，返回码 %RC%
)

:done
rem 只有双击运行时才暂停（此时 cmd 是为本脚本临时启动的），
rem 在已打开的终端里执行则直接返回，不会卡住自动化流程。
rem 用绝对路径调用 find，避免 PATH 里的 MSYS find 抢先命中。
echo %cmdcmdline% | %SystemRoot%\System32\find.exe /i "%~f0" >nul
if not errorlevel 1 pause

exit /b %RC%
