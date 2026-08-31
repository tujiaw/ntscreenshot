# 参与贡献

[English](#contributing)

感谢你愿意改进 ntscreenshot。小而完整、能说明如何验证的改动最好审。

## 提 Issue 之前

- 先看 [常见问题](docs/faq.md) 并搜索已有 Issue。
- 写明系统版本、ntscreenshot 版本或 commit、能否稳定复现。
- 日志和截图请去掉密钥、个人路径、剪贴板和聊天内容。
- 安全问题按 [安全策略](SECURITY.md) 私下报告，不要发公开 Issue。

## 开发流程

1. Fork 仓库，从默认分支拉出主题分支。
2. 按 [构建指南](docs/building.md) 配置工程。
3. 改动保持单一目的；能测的补测试。
4. 提交前在本机跑构建和检查。
5. 按仓库模板开 Pull Request。

提交说明建议用 [Conventional Commits](https://www.conventionalcommits.org/)：

```text
fix: prevent duplicate clipboard records
docs: clarify Windows build setup
```

## 代码约定

- C++17，沿用现有 Qt 写法与命名。
- 构建系统只改 CMake，不再维护 Visual Studio 工程文件。
- 不要提交生成文件、构建产物、下载的依赖、密钥或用户数据。
- 用户可见行为变化时，同步改 FAQ / README / 更新日志中对应的一句。

新工具放在 `src/modules/<name>`，实现现有模块生命周期，不要引入全局单例。平台、设置、图像、网络等可复用能力放 `src/core`；跨模块 UI 放 `src/shared`。依赖从 `src/app/main.cpp` 注入，模块之间不要直接包含对方内部头文件。详见 [项目结构](docs/project-structure.md) 与 [新增模块](docs/adding-a-tool-module.md)。

## 提交前至少跑

```powershell
.\scripts\build-win.ps1 -Config Release -Platform x64
ctest --test-dir build/cmake-x64-Release --output-on-failure
.\scripts\check-architecture.ps1
.\scripts\check-repository.ps1
```

若改了打包脚本，请再打一份绿色包并启动 `dist` 里的 exe。

## Pull Request

说明改了什么、为什么、怎么测的，以及兼容性和隐私影响。不相关的改动可能会被要求拆开。提交即表示同意以项目的 Apache-2.0 协议授权该贡献。

---

# Contributing

Thanks for helping improve ntscreenshot. Small, focused changes with clear validation are the easiest to review.

## Before an issue

- Read the [FAQ](docs/faq.en.md) and search existing issues.
- Include OS, ntscreenshot version or commit, and whether it reproduces.
- Strip keys, personal paths, clipboard contents, and chat from logs and screenshots.
- Report security issues privately using [Security](SECURITY.md).

## Workflow

1. Fork and branch from the default branch.
2. Follow the [build guide](docs/building.en.md).
3. Keep the change scoped; add tests where practical.
4. Run the build and checks locally.
5. Open a pull request with the repository template.

Use [Conventional Commits](https://www.conventionalcommits.org/), for example `fix:` / `docs:`.

## Code style

- C++17 and existing Qt idioms.
- CMake only; Visual Studio project files are not maintained.
- Do not commit generated files, build outputs, downloaded dependencies, credentials, or user data.
- Update the FAQ, README, or changelog when user-visible behavior changes.

New tools belong under `src/modules/<name>` and must follow the module lifecycle — no new global singletons. See [project structure](docs/project-structure.md) (Chinese) and [adding a tool module](docs/adding-a-tool-module.md).

## Validation

```powershell
.\scripts\build-win.ps1 -Config Release -Platform x64
ctest --test-dir build/cmake-x64-Release --output-on-failure
.\scripts\check-architecture.ps1
.\scripts\check-repository.ps1
```

For packaging changes, also produce the portable archive and launch it.

By contributing, you agree that your contribution is licensed under Apache-2.0.
