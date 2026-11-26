# Web API 支持清单与消费契约

以下接口运行于单浏览器主线程，渲染使用 WebGL2 或 Canvas 2D。Windows Edge、Chrome 和 macOS Safari 的系统输入法、读屏和拖放覆盖范围见对应验收记录。[Windows 浏览器验收](../../core/docs/validation.md#m9-linuxweb-平台专项) · [Safari 验收](../../core/docs/validation.md#macos-safari-交互)

## 支持索引

逐项 C ABI 清单见 [生成索引](../../core/docs/web-api-inventory.json)，包含每个公开入口的状态、C++ 包装文件和 C# P/Invoke 声明位置。没有出现浏览器兼容承诺的入口统一标为 `not-in-web-acceptance`，不能因它能链接就标为已支持；C# interop 文件存在也不表示回调已适配。索引通过 `python tools/web/api-inventory.py --check` 检查漂移。

| 范围 | C / C++ 入口 | C# 入口 | Web 行为与验证 |
|---|---|---|---|
| App / Fleck | `ui_app_create`、`ui_app_load_fleck*` 的 XML/上下文/模型入口、`ui_app_open_window`；`ui.hpp` | `App`、`FleckDataContext`、`LoadFleck` | Headless + 外部宿主渲染；`open_window` 打开宿主视图，不打开 OS 窗口。smoke、workspace |
| 节点与文本源 | `ui_node_set/get_text`、`ui_text_source_*`、动作和绑定；`ui.hpp` | `Node`、`TextSource`、`SetAction` | UI 线程访问；代表控件与 GC 后回调通过。text-input、workspace |
| 属性模型 | `ui_model_*`、`ui_node_bind_property` 的首版路径；`ui.hpp` | `CreatePropertyModel<T>`、`Add<T>`、通知 | 使用泛型保留公开属性或显式注册；只有 object 的反射入口有裁剪提示。workspace 的实际 publish |
| 同步命令 | `ui_command_create` 与节点命令绑定 | `CreateCommand` | 静态反向回调 + GCHandle；不得从回调重入 Poll。smoke |
| 异步命令 | `ui_command_create_external`、`ui_completion_complete/release` | `CreateAsyncCommand`、`InvokeAsync` | C++ 回调/完成令牌，C# 原生 Task/HttpClient；无需自建 .NET 调度。smoke、workspace、hosted_runtime |
| 宿主推进 | `ui_app_poll(app, 0)` | `Poll(0)` | 浏览器帧/计时器驱动；非零超时返回 Unsupported；后台节流不保证完成时限 |
| 像素与视图 | `ui_host_select_view/pixels/snapshot/capabilities` | `HostedSurface.SelectView/CopyPixels/Snapshot/Capabilities` | Poll 推进整个 App，输入与读像素选择具体视图；浮窗 ID 失效后拒绝访问。workspace、window-state |
| GPU 命令 | `ui_host_set_render_mode/draw_commands` | `SetCommandRendering/CopyDrawCommands` | App 级模式、按视图读取 v1 矩形字节流，按需 CPU 回退；renderer、hosted_runtime、hosted_views |
| 指针/键盘/尺寸 | `ui_host_resize/pointer/scroll/context_menu/key_event/focus` | `HostedSurface` 对应方法 | 设备像素坐标，96 DPI 为 1x；捕获取消不激活控件；仅首版导航键集合。text-input、workspace、window-state |
| 编辑协议 | `ui_host_text_read/update/preview/hit_test/scroll` | `ReadText/UpdateText/PreviewText/HitText/ScrollText` | UTF-8 内容、UTF-16 选区、session/revision；草稿不发布绑定值。hosted_text、text-input |
| 字体注册 | `ui_host_set_font` | 浏览器样例注册入口 | FreeType 字体内存复制，创建 App 前调用；TTC face 0，最多 64 MiB；没有自动多字体回退 |
| 页面内 Dock | `ui_dock_create/find/float/reset/get_windows`、事件；`dock.hpp` | `DockWorkspace`、`DockItem` | 页面内 Canvas 浮窗；移动、缩放、关闭、回停等代表路径通过。workspace、window-state；其他 Dock 操作不自动继承桌面验收 |
| 自定义皮肤 | `ui_app_register_dock_skin`、绘制回调 | 皮肤注册及绘制回调 | 静态桥接已接入，workspace 验证回调/回退；绘图 API 组合不代表全部浏览器视觉已测 |
| 基础语义 | `ui_host_snapshot/semantic_action` | `Snapshot/SemanticAction` | JSON v1 → DOM/ARIA，隐藏密码内容；AX 树/激活自动检查不等于全面无障碍 |
| 文档阅读 | `ui_document_*`、`UI_DOCUMENT_VIEW`；`ui.hpp` | `Node.LoadDocument`、选择/滚动/事件、`DocumentFonts` | C++ Wasm 文档站验证长文、搜索、标题定位、复制、字体与双渲染器；C# 桌面接口有定向测试，托管 Web 文档组件未单独验收。见 [文档站](documentation-site.md) |
| 关闭与释放 | `ui_app_request_close/destroy`、令牌释放 | `RequestClose/Dispose` | 宿主先停止输入/调度，关闭后迟到完成不得触碰 App；pagehide persisted 只暂停 |

以上实现入口：[C ABI](../../core/include/sunfleck/ui.h)、[C++ 包装](../../core/include/sunfleck/ui.hpp)、[Dock 包装](../../core/include/sunfleck/dock.hpp)、[C# HostedSurface](../../core/bindings/csharp/HostedSurface.cs)、[静态回调](../../core/bindings/csharp/BrowserCallbacks.cs)、[属性回调](../../core/bindings/csharp/BrowserProperties.cs)、[异步执行器](../../core/bindings/csharp/BrowserAsyncCommands.cs)。

## 桌面 API 的替代与限制

| 桌面用法 | 浏览器结果/边界 | 替代方式 |
|---|---|---|
| `ui_app_run` / `App.Run()` | C 返回 Unsupported；C# 抛 PlatformNotSupportedException | 共享 JS 宿主 + Poll(0) |
| Poll 非零超时 | Unsupported，经 C# 包装转为 SunfleckException | 浏览器异步操作，完成后请求推进 |
| `ui_app_create_window` / `CreateWindow()` | Unsupported；独立 Window 会话方法不纳入 Web 承诺 | 页面内 Dock 浮窗 |
| worker v2 `ui_command_create_async` | 注册不等于执行成功；当前无 pthread 构建的 worker 执行返回 Unsupported | C/C++ 使用 external completion；C# 使用浏览器 CreateAsyncCommand |
| Windows 调试拾取、命名管道服务 | 启用操作 Unsupported；禁用/空参数清理可为无操作 | 浏览器开发者工具、基础 snapshot；不承诺桌面 IDE 互通 |
| C# Window.WithContext、诊断/刷新回调、Development/DebugServer | 未纳入 Web 反向回调/完整端到端验收 | 首版不要依赖；按具体需求补静态桥接与回归 |
| Fleck 二进制/生成代码、主题与 Dock 全量接口 | 共享核心已有相关能力，Web 未逐项全覆盖 | 构建机运行 fleckc；新增使用路径前补浏览器用例 |
| 原生剪贴板/文件/对话框与本地路径 I/O | 无通用桌面服务映射 | 文本剪贴板由 DOM 可信事件处理；资源用 fetch/HttpClient 获取字节；文件选择等另接浏览器异步 API |
| 原生线程、D3D11、EGL/GLES | 当前 Web 宿主不提供这些运行模式 | 主线程非阻塞 I/O + WebGL2 / Canvas 2D；Worker 按需求另行接入 |

## 所有权、错误与发布配套

App/节点、绘制和模型操作遵守 UI 线程及禁止重入约束；节点地址不能跨不同 Wasm 实例传递。C# delegate 上下文由静态入口的 GCHandle 保活，业务不能自行提前释放。C++ `begin` 获得一次 completion 引用并最终 release；取消是请求，业务仍需协作结束和释放；同步 Dispose 不阻塞等待浏览器 I/O。

文本读取指针是借用值；JS 在下次原生调用或内存增长后重新获取视图。更新时版本失效返回 InvalidState，宿主应重新读取；不得把旧编辑会话继续写回。单次 UTF-8 提交上限 1 MiB，DOM 长度上限 262144 UTF-16 单元，选区不得进入代理对中间。

能力查询当前位集合为 Text、Scroll、DockViews、Semantics、ExternalCommands、DrawCommands，不表示完整输入法、全控件可访问性、文件服务或任意后台线程支持。错误应由 C 返回值或 C# 异常通道处理；C ABI 回调不得抛出异常。

必须一起部署同一候选的 Wasm、JS、托管程序集、页面、字体和资源清单。Hosted ABI/JSON 仍不提供跨版本任意混搭保证。应用生产部署、字体许可和资源预算见 [应用交付](../../core/docs/web-delivery.md)。
