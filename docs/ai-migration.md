# AI 对话与划词迁移

AI 对话、浏览器工具、聊天历史和 Windows 划词功能已迁至独立仓库 [auto-browser](https://github.com/tujiaw/auto-browser)。ntscreenshot 保留截图、标注、长截图、GIF、贴图、剪贴板、本地搜索、OCR、图床与 HTTP 服务。剪贴板中独立的 AI 表单填充功能继续保留。

截图程序不再链接或打包 Qt WebEngine / WebChannel。AI 对话快捷键、模型厂商、划词操作和对话窗口设置在 auto-browser 中管理。

旧配置文件和聊天记录留在原目录。首次运行 auto-browser 前可导入：

```powershell
./auto-browser.exe --import-ntscreenshot "$env:LOCALAPPDATA/ntscreenshot"
```

传入目录应包含 `config/base.ini` 和可选的 `assistant_chat_session.json`。导入仅复制 AI、划词、外观和窗口设置；不覆盖新项目已有值，不导入截图、图床、剪贴板数据或开机启动项。新应用在 `%LOCALAPPDATA%/auto-browser/` 使用独立配置。

在迁移快捷键前退出旧版 ntscreenshot，避免两者注册同一对话快捷键。新版 ntscreenshot 已不再注册该快捷键。

两个程序同时运行时，auto-browser 会排除 ntscreenshot 的窗口，避免划词回退复制操作的 Ctrl+C 提前完成截图。截图覆盖层同时设置原生窗口属性 `AutoBrowser.IgnoreTextSelection`，供独立划词程序识别；旧版截图程序通过进程名兼容排除。更新后需退出并重新启动 auto-browser。

构建截图便携包只需 `./scripts/build-win.ps1 -Package`，输出 `dist/ntscreenshot-x64-Release.zip` 和对应 SHA256 校验文件。auto-browser 的构建与测试方式见其 README。
