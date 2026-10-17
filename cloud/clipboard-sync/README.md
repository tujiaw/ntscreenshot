# ntscreenshot 统一收藏同步服务

所有用户共用一个 Vercel 项目、一套 Neon 数据库和一个私有 Blob 存储。
管理员只需配置一次；桌面端内置 `https://ntscreenshot.vercel.app`，用户只需登录 GitHub。
收藏按 GitHub 数字 ID 隔离，不同用户无法访问彼此的数据。普通剪切板历史不上传。

## 管理员一次性配置

目标 Vercel 项目：`ntscreenshot`，Root Directory 为 `cloud/clipboard-sync`，Node.js 24。

现有存储连接可直接使用：

| 用途 | 已有资源 | 项目环境变量 |
| --- | --- | --- |
| 文字、会话、同步事件 | `neon-apricot-pebble` | `nt_DATABASE_URL` |
| 收藏图片 | 私有 Blob `ntscreenshot-clipboard` | `NT_STORE_ID` |

数据库变量保持 Sensitive，不需要复制、解密或发给用户。Blob 使用 Vercel 自动提供的
OIDC 凭据；连接前缀也兼容标准 `DATABASE_URL`、`BLOB_STORE_ID`。

1. 在 GitHub **Settings → Developer settings → OAuth Apps** 创建用于 ntscreenshot 的
   OAuth App，开启 **Enable Device Flow**。主页填写项目主页或统一服务地址。
   Device Flow 不使用回调，GitHub 注册页面要求填写时可填写主页。
2. 在 Vercel 的 `ntscreenshot` 项目环境变量中添加 **`GITHUB_CLIENT_ID`**，用于 Production。
   这是公开应用标识；当前流程不需要 Client Secret，不申请仓库和私人邮箱权限。
3. 部署服务。首次请求会自动创建 `clipboard_` 开头的表及索引，不修改数据库中其他项目的表。
4. 检查 `https://ntscreenshot.vercel.app/v1/health?check=storage`。成功时返回服务标识和
   `loginConfigured: true`。若为 `false`，补充 `GITHUB_CLIENT_ID` 后重新部署。

生产 API 对公众开放，以 GitHub 授权后的应用会话鉴权。预览部署保留 Vercel 访问保护。
不需要每个用户创建 Vercel 项目、连接数据库、配置地址或持有任何存储密钥。

## 用户使用

打开 **剪切板 → 收藏 → 云同步 → 使用 GitHub 登录**。
点击“复制验证码并打开 GitHub”，输入验证码并授权；程序自动完成登录并开始同步。
在其他设备登录同一 GitHub 账号，即可继续使用自己的收藏。

首次登录会合并本机和云端收藏；退出登录会停止同步，保留本机和云端收藏。
为避免混入其他人的数据，已关联的本机收藏需使用原 GitHub 账号继续同步。

## 服务行为

- 本地 SQLite 优先；离线操作持久化，联网后重试。变化后约 1.5 秒同步，后台每 5 分钟检查。
- 每条文字最大 100 KiB、每张 PNG 最大 2 MiB、7200 万像素。超限条目仅保留本机，不阻塞其他收藏。
- 默认每账号图片空间 100 MiB、累计同步日志 16 MiB / 50000 条事件，避免单个账号耗尽共享环境。
  额度由服务器统一控制；用户无需配置。
- 服务器按账号序列化事务，确保同步游标不会跳过未提交的操作。客户端时间不决定冲突胜负。
- 上传操作 ID 保证重试幂等，取消收藏使用删除标记，离线设备不会自动复活已删除条目。
- 图片路径为 `clipboard/<GitHub ID>/<SHA-256>.png`，仅通过鉴权后的服务端读取私有 Blob。
  客户端不持有 Neon、Blob 或 Vercel 管理凭据。
- GitHub 令牌只用于登录交换，不持久化；应用会话有效期 90 天，服务端只保存摘要，
  客户端原始凭据保存在 Windows 凭据管理器。离线退出无法保证即时撤销服务端会话。
- 使用 HTTPS 和账号隔离；当前没有端到端加密，服务运营者可以读取收藏。
- 删除收藏后云端历史事件及图片暂时保留，以支持长期离线设备补同步。
  永久清除数据需由管理员按 `user_id` 清理相关表及对应 Blob 前缀，并让该账号的客户端重置同步状态。

免费资源额度由整个服务共同消耗。管理员可在 Vercel / Neon 控制台监控用量、备份和调整账户配额。
预览环境测试应连接独立的测试分支或测试存储，避免把生产用户数据暴露给未经审核的代码。

## 开发与验证

```powershell
npm ci
npm run check
npm test
```

测试使用 PGlite 的真实 PostgreSQL 执行生产迁移和事务，覆盖账号隔离、双设备增删、分页、
批量回滚、重复请求、存储配额、授权取消与限流。Blob 和 GitHub 上游在测试中模拟，不上传用户数据。
桌面回归测试验证登录界面、会话恢复、图片下载和断网重试。

服务入口为 `api/sync.js`，`vercel.json` 将 `/v1/*` 路由到此函数。
发布者若更换正式域名，应统一修改 CMake 的 `NT_CLIPBOARD_SYNC_URL` 并重新发布客户端；
客户端不会采纳旧版本保存的自建服务地址或复用其他服务的会话。
