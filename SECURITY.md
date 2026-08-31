# 安全策略 / Security Policy

[English](#security-policy)

## 支持范围

安全修复做在最新默认分支上，并随下一版本发布。旧的开发快照不提供补丁。

## 如何报告

请优先通过 GitHub 仓库 Security 页的 **Private vulnerability reporting** 报告漏洞；若该入口不可用，请在 GitHub 上私下联系仓库维护者。

**不要**开公开 Issue、讨论或 Pull Request 贴利用细节、密钥或用户隐私数据。

请尽量包含：

- 受影响版本或 commit
- 操作系统与相关设置
- 复现步骤或概念验证
- 预期影响
- 若有缓解办法也可一并说明

收到后会在七天内确认。维护者会评估、修复并协商披露时间；希望署名的报告人会被致谢。请在公开讨论前留出合理修复窗口。

## 范围说明

设置里的 API 密钥以未加密形式存在本机 INI 中，这是已文档化的限制，不视为「密钥保管边界」本身的漏洞。以下仍在范围内：非预期的远程泄露、提权、任意代码执行、访问用户未选择的路径。

可选 AI 助手能调用本地文件、URL、Python 和命令行。请只对可信服务商开启，使用受限 Token，并以普通用户运行。把模型输出和抓取内容视为不可信输入。

仓库检查只能发现部分明显的密钥文件名、Token 模式、二进制和大文件。检查通过不等于完成安全审计。

---

# Security Policy

## Supported versions

Fixes land on the latest default branch and ship in the next release. Older development snapshots are not supported.

## Reporting a vulnerability

Please use **Private vulnerability reporting** on the GitHub repository's Security page. If it is unavailable, contact the repository maintainers privately through GitHub.

Do not open a public issue, discussion, or pull request containing exploit details, credentials, or private user data.

Include affected version or commit, OS and configuration, reproduction steps or a proof of concept, expected impact, and any suggested mitigation.

You should receive an acknowledgement within seven days. Maintainers will investigate, coordinate a fix and disclosure timeline, and credit reporters who wish to be named. Please allow a reasonable remediation window before public disclosure.

## Scope notes

Settings may contain API credentials in an unencrypted local INI file. That is a documented limitation, not a secret-storage boundary. Unintended remote disclosure, privilege escalation, arbitrary code execution, or access outside user-selected paths remain in scope.

The optional assistant can reach local files, URLs, Python, and the shell. Enable it only with trusted providers, restricted tokens, and a standard user account. Treat model output and fetched content as untrusted.

Repository checks catch a limited set of obvious secret names, token patterns, binaries, and oversized files. A passing check is not a security review.
