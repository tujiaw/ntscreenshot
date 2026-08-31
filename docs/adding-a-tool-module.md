# 新增个人工具模块

以下流程用于新增一个可以独立演进的工具，例如 JSON 格式化器、文件批处理器或编码转换器。

## 1. 创建模块目录

```text
src/modules/json_formatter/
|-- CMakeLists.txt
|-- JsonFormatterModule.h
|-- JsonFormatterModule.cpp
`-- ui/
    |-- JsonFormatterWidget.h
    `-- JsonFormatterWidget.cpp
```

不需要后台任务或长期资源的工具可以省略 `JsonFormatterModule`，由应用外壳按需创建 Widget。

## 2. 声明模块源码

在模块自己的 `CMakeLists.txt` 中声明，不要编辑根源码清单：

```cmake
target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/JsonFormatterModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/JsonFormatterWidget.cpp
)
```

然后只在 `src/modules/CMakeLists.txt` 添加一次：

```cmake
add_subdirectory(json_formatter)
```

## 3. 接入生命周期（可选）

需要监听剪贴板、注册热键、启动定时器或持有后台线程时，实现 `IToolModule`：

```cpp
class JsonFormatterModule final : public IToolModule {
public:
    QString id() const override { return QStringLiteral("json_formatter"); }
    void initialize() override;
    void shutdown() override;
};
```

在 `app/main.cpp` 的组合根通过 `modules.emplaceModule<MyModule>(...)` 注册实例。注册器取得唯一所有权，模块不得自行寻找全局 Registry，也不要再实现单例 `instance()`。

模块需要设置、导航或其他服务时，通过构造函数显式声明依赖并由组合根传入。不要通过静态函数、`qApp` 属性或新的服务定位器绕过依赖注入。

## 4. 暴露最小契约

若其他模块需要调用此工具，在 `contracts/` 暴露只包含必要操作的接口。实现类、数据库模型和内部 Widget 保持在模块内部。优先通过信号发送结果，避免共享可变全局状态。

## 5. 完成检查

- 模块没有包含 `app/` 头文件。
- 模块没有包含另一个模块的内部头文件。
- 公共逻辑只有被至少两个模块使用时才提升到 `core` 或 `shared`。
- 后台线程、热键、监听器在 `shutdown()` 中可重复且安全地释放。
- Debug、Release 至少完成一次目标平台构建。

运行自动边界检查，防止新增向上依赖或跨模块内部依赖：

```powershell
.\scripts\check-architecture.ps1
```
