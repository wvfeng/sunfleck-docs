# C# 接入

绑定面向 .NET 10 和 C# 14，支持 Windows x64、Linux X11、macOS ARM64 和 Headless。托管项目使用 P/Invoke 和 UTF-8 封送，无第三方 NuGet 依赖。[窗口刷新和热重载](devtools.md#fleck-刷新与原生重建)

## 构建与运行

最小业务示例见 [同步／异步计数器](../../core/tests/development/fixtures/legacy-demos/counter/README.md)。`App.RunFleckFile(path, model, watch: true)` 加载页面、打开窗口并运行到关闭；相对路径使用当前工作目录。默认监听保存并事务刷新页面，临时读文件失败会重试，解析失败保留旧页面。可用 `GetFleckRefreshStatus()` 读取刷新诊断；初次加载失败仍抛出异常。`.NET Hot Reload` 后由绑定程序集的更新回调通知该模型刷新。`watch: false` 则只加载一次并运行。

准备与目标架构匹配的 .NET 10 SDK 和原生构建工具。Windows 使用 Visual Studio x64 开发者终端。[原生构建依赖](../../core/docs/building.md)

```sh
cmake --preset debug -DUI_BUILD_CSHARP=ON
cmake --build --preset debug
ctest --preset debug -LE interactive
```

Linux 默认用 Xvfb 执行 X11 测试，可直接运行 `ctest --preset debug`；启用 GLES3、容器及 Release 配置见 [构建与测试](../../core/docs/building.md)。

macOS 构建可使用系统自带的 Make。使用 `Renderer.Metal` 时，在图形会话的主线程运行窗口和事件循环。[macOS 构建](../../core/docs/building.md#macos) · [安装式 SDK 消费](../../core/docs/sdk.md#macos-安装与复验)

| 示例 | Windows 产物（相对于 `out/debug`） |
|---|---|
| 计数器代码版 | `examples/simple/csharp/code/CounterCode.exe` |
| 计数器 Fleck 版 | `examples/simple/csharp/fleck/CounterFleck.exe` |
| 展厅代码版 | `examples/showcase/csharp/code/ShowcaseCode.exe` |
| 展厅 Fleck 版 | `examples/showcase/csharp/fleck/ShowcaseFleck.exe` |

Linux 使用相同目录，应用启动文件不带 `.exe`；也可运行 `dotnet CounterCode.dll`。自定义目录中的 .NET 运行时可通过 `DOTNET_ROOT` 指定。展厅两版包含六页，场景和源码入口统一见 [示例索引](../../samples/README.md)。

## 生命周期与回调

App、TextSource、Command、PropertyModel 实现 IDisposable。Node、DockWorkspace、DockItem 借用所属 App；创建、使用和释放都在 App 的 UI 线程进行。先释放模型适配器、文本源和命令，再释放应用。后台任务可用 App.Post 投递 UI 更新；标准 PropertyChanged 的后台事件由适配器自动投递。

Fleck 刷新或停用页面销毁可提前释放节点、工作区和项目；访问退役 Node 抛出 SunfleckException（InvalidState），退役 Dock 包装抛出 ObjectDisposedException。刷新后按 ID 重新查询所需对象。状态捕获/恢复及包装失效时机见 [开发工具接入](devtools.md#fleck-刷新与原生重建)。

```csharp
using (var app = new App(Renderer.Software))
{
    app.SetWindowOptions("Hello Sunfleck", 360, 180);
    var button = app.CreateNode(NodeKind.Button);
    button.Text = "Click";
    button.SetAction(node => node.Text = "Clicked");
    app.SetRoot(button);
    app.OpenWindow();
    app.Run();
}
```

应用保活注册的托管委托，回调异常在 Poll 或对应同步操作返回后重新抛出。默认按钮操作采用 SingleFlight；DeferAction/CompleteAction 用于标记异步操作的进行状态，不负责切换线程。

同一个 App 可创建独立窗口，Window 借用 App 的生命周期，不实现 IDisposable：

```csharp
var secondary = app.CreateWindow();
secondary.WithContext(current => current.LoadFleck(secondaryXml));
secondary.Open();
secondary.WithContext(current => current.QueueFleckRefresh(updatedXml));
// App.Poll/Run 统一驱动全部窗口；关闭主窗口不终止仍打开的 secondary。
secondary.RequestClose();
```

页面、Dock、刷新队列与状态委托按窗口隔离；模型、文本源和命令可共享。WithContext 同步执行并自动恢复上下文，异常在恢复后抛出；其中不能调用 Poll/Run/Dispose、创建窗口或切换到其他窗口。App.Dispose 后 Window 访问抛出 ObjectDisposedException。完整原生/C++ 契约见 [独立窗口与页面刷新](devtools.md#同步刷新与页面会话)。

## Fleck、Dock 和主题

| 需求 | 托管入口 | 详细契约 |
|---|---|---|
| 加载 XML / 二进制页面 | `LoadFleck` / `LoadFleckResource`，传 PropertyModel 或显式源导出 FleckDataContext | [页面绑定](fleck.md) |
| 接入标准属性通知 | `CreatePropertyModel(viewModel)`，支持 INotifyPropertyChanged | [属性绑定](bindings.md) |
| 后台命令与中途通知 | `App.CreateAsyncCommand(async token => ...)`，用 `await app.InvokeAsync(...)` 更新原模型，Fleck 声明 CommandMode="Async" | [异步命令](bindings.md#async-命令与中途通知) |
| 获取工作区、注册或查找项目 | `CreateDock(host)`、`Register` / `Find` | [Dock](docking.md) |
| 移动、浮动、隐藏、固定、关闭 | DockItem 的 `Move` / `MoveGroup` / `Float` / `AutoHide` / `Pin` / `Hide` / `Show` / `Close` | [状态与关闭取消](docking.md#api-与状态) |
| 保存和恢复布局 | `Save` / `Restore`，`CaptureDefault` / `Reset` | [布局持久化](docking.md#布局持久化) |
| 应用主题与皮肤 | `LoadTheme` / `ApplyTheme` / `RegisterDockSkin` | [主题与 Dock 皮肤](themes.md) |

FleckException 提供错误位置与原生状态码。Closing 事件设置 Cancel 可取消关闭；绘制委托返回 false 继续回退到模板或内置绘制。DrawContext 仅在回调期间、原 UI 线程有效。

[计数器代码](../../samples/counter/csharp/code/Program.cs) · [Fleck 计数器](../../samples/counter/csharp/fleck/Program.cs)

## 部署

保留构建输出中的应用启动文件、应用程序集、`.deps.json`、`.runtimeconfig.json`、`Sunfleck.dll` 及对应平台的原生库。Windows 使用 `sunfleck_native.dll`，Linux 使用 `libsunfleck_native.so`，macOS 使用 `libsunfleck_native.dylib`。Fleck 示例还需同目录的 `.flkb`，并安装相同架构的 .NET 10 Runtime。

原生库使用 Release 构建；Windows 目标机需要匹配的 MSVC 运行库，Linux 需提供所选后端及 FreeType 的运行依赖。GPU 模式、ANGLE DLL 和回退规则统一见 [渲染与部署](rendering.md)。

### UserControl 与 Control

`App.RegisterComponent<T>` 注册代码行为，`UserControl` 提供 `FindNode`、`Get/Set`、`Bind`、`SetDataContext` 和直接事件订阅，`Control` 还提供 `FindTemplatePart` 与 `ApplyTemplate`。C# 生成器从 `FleckComponent` 等特性生成注册代码，因此发布运行不依赖原始 Fleck 文件。浏览器目标使用静态 UnmanagedCallersOnly 回调；桌面和浏览器共享同一组件契约。

类型继承关系是 `FrameworkElement → Control → UserControl`。内建标签类型集中在 `Sunfleck.Elements` 命名空间，以 `TextBox.TextProperty` 等静态描述提供类型、绑定方向和源码位置；现有 `Node` API 仍可用。

无自定义属性的布局组件也有实际代码类型：

```csharp
namespace Example;
public sealed partial class UserCard : Sunfleck.UserControl { }
```

当 Fleck 的 `Class="Example.UserCard"`、`Namespace="urn:example:controls"` 已声明身份时，不必重复添加 `FleckComponent` 特性。如果没有手写这个类，源生成器会生成同名空类。构建生成的源文件放在 `obj/<Configuration>/<TargetFramework>/fleck-generated`；语言服务在未构建时为生成类型提供临时源码定义，以支持 F12。自定义控件继续在类上声明 `FleckComponent` 及默认模板，并用 `FleckProperty` 标注可绑定属性。

内建布局声明中的非绑定属性也有类型描述；只有原生节点支持的绑定目标具有运行时读写器。此类型描述机制不包含 WPF 的属性值优先级、样式触发器和通用属性继承。
