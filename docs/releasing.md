# 发布流程

版本号使用 `vMAJOR.MINOR.PATCH`，须与 `CMakeLists.txt` 中的 `project(... VERSION ...)` 以及 `vcpkg.json` 的 `version-semver` 一致。

GitHub 是主要公开仓库。发布前应确认默认分支、Release 工作流和私下漏洞报告均已启用；若同步镜像到其他平台，两边应使用相同的 Tag。

1. 更新 `CMakeLists.txt` 和 `vcpkg.json` 中的版本。
2. 把 [CHANGELOG.md](../CHANGELOG.md) 里 Unreleased 的条目移到带日期的版本标题下。
3. 运行：

   ```powershell
   .\scripts\check-repository.ps1 -Tag v0.1.0
   ```

4. 干净的 Windows Release 构建，并跑 CTest。
5. 打绿色包，在干净的 Windows 用户目录下启动一次。
6. 通过评审合并后再打 annotated tag，例如 `git tag -a v0.1.0 -m "ntscreenshot 0.1.0"`。
7. 推送 Tag。以 GitHub Actions 产物为准；若工作流暂未启用，则上传本地校验过的 zip 与 SHA-256。
8. 下载发布附件，核对校验和，再对外宣布。

## 回滚

发布前验证失败：删掉本地 Tag，修好后再打新 Tag。已经公开的 Release 不要静默覆盖同一文件却保留旧校验和；应撤回或发补丁版本，并说明原因。

不要发布未提交工作区打出来的包，也不要在测试失败时强行发版。
