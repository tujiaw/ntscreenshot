# 项目结构与模块边界

本项目采用“模块化单体”结构：仍然发布一个桌面应用，但每个个人工具以独立模块维护，应用层只负责启动、装配和窗口协调。

```text
src/
|-- app/                         # 组合根：启动、模块注册、应用外壳
|   |-- crash/                   # 崩溃处理
|   |-- shell/                   # 主窗口、托盘和全局快捷键
|   |-- ModuleRegistry.*         # 模块所有权、查询和生命周期管理
|   `-- WindowManager.*          # 跨模块命令路由，不持有工具窗口
|-- core/                        # 稳定、可复用且不依赖具体工具
|   |-- foundation/              # 常量、字符串、时间、异步等基础能力
|   |-- imaging/                 # 图像处理与区域检测
|   |-- modules/                 # 模块公共契约
|   |-- network/                 # 通用网络能力
|   |-- platform/                # 跨平台接口及 native 实现
|   |-- runtime/                 # 日志、单实例等运行时能力
|   |-- settings/                # 全局设置模型
|   `-- theme/                   # 主题和图标策略
|-- shared/
|   `-- ui/                      # 多个模块共用的无业务 Widget
|-- modules/                     # 按业务能力纵向组织的个人工具
|   |-- assistant/               # AI Agent、工具执行器与聊天 UI
|   |-- capture/                 # 截图、标注、贴图、长截图、GIF
|   |-- clipboard/               # 剪贴板历史与 AI Fill
|   |-- settings/                # 设置界面
|   |-- text_selection/          # 全局文本选择工具
|   `-- local_search/            # 本地文件、目录与应用快速搜索
|-- libs/                        # 暂时保留的第三方源码
`-- resource/                    # Qt 资源与运行时素材
```

每个一级目录和业务模块拥有自己的 `CMakeLists.txt`。根构建文件只负责发现依赖和进入 `src/`，不再维护全项目源码大列表。

## 依赖方向

新增代码应遵循以下单向依赖：

```text
app  --> modules --> shared --> core
  \-----------> shared/core
```

- `core` 不得包含 `app/`、`modules/` 或 `shared/` 的头文件。
- `shared` 只能依赖 `core`，不能知道具体业务模块。
- 模块可以依赖 `core` 和 `shared`，但模块之间不得直接包含对方的内部头文件。
- 模块间协作通过位于模块 `contracts/` 中的小接口、Qt signal，或由 `app` 层进行编排。
- `app` 是唯一允许知道所有模块的组合根。
- 不要把新增业务代码放回 `common`、`view`、`controller` 这类横切目录。

`app/main.cpp` 是依赖组合根。`WindowManager`、`SettingModel` 等应用服务由组合根创建，并通过模块构造函数逐层注入；业务源码禁止新增 `instance()`、`GetInstance()` 或 `Singleton<T>`。确实需要进程级状态的平台实现应限制在对应 `.cpp` 内部，不向模块暴露全局访问入口。

目前少量截图和设置代码仍通过显式构造参数依赖 `WindowManager`，全局设置迁移逻辑也仍引用 AI 工具元数据。这些边界由架构检查白名单明确记录，后续可继续收窄为模块 contracts；新增模块不应复制这种跨层依赖。

## 模块生命周期

所有独立模块实现 `core/modules/IToolModule.h`，并由 `app/ModuleRegistry` 通过 `unique_ptr` 统一拥有。注册器负责：

- 防止重复模块 ID；
- 按类型或 ID 查询模块；
- 按注册顺序初始化；
- 按相反顺序关闭；
- 在注册器析构时执行幂等清理。

当前统一注册了 `clipboard`、`capture`、`assistant`、`settings`、`text_selection`、`local_search` 和应用 `shell`。`WindowManager` 只把调用路由到对应模块，不持有各工具的界面对象。

## 模块内部结构

小模块可以保持扁平。模块变复杂后按需拆分，而不是预先创建空目录：

```text
modules/my_tool/
|-- CMakeLists.txt
|-- MyToolModule.h/.cpp           # 可选：生命周期入口
|-- contracts/                    # 供其他层调用的稳定小接口
|-- domain/                       # 与 Qt UI 无关的业务规则
|-- application/                  # 用例编排
|-- infrastructure/               # 文件、网络、数据库、系统 API
`-- ui/                           # Widget、Model/View、对话框
```

只有在模块确实出现这些职责时才创建对应目录。避免为简单工具套用不必要的层级。

## 第三方代码

`src/libs/` 仅用于现有 vendored 依赖，禁止加入第一方业务代码。新的第三方库优先通过 CMake package 管理；确需离线 vendoring 时，后续统一迁移至仓库根 `third_party/`。
