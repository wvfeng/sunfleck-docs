# 开发工具接入

本文介绍 Fleck 语言服务、模型元数据、运行时检查及刷新流程。IDE 操作见 [Windows 快速开始](windows-quickstart.md)，测试入口见 [开发工具测试说明](../../devtools/tests/development/README.md)。

## 构建

### 对象树悬停高亮

VS Code 扩展 0.2.5 的对象树支持鼠标悬停，在应用中显示对应实例的蓝色边框及半透明填充。高亮使用布局 bounds 并按 visible 裁剪，不改变布局、焦点或输入命中。C/C++ 和 C# 调试 Pipe 的快照能力增加 `highlight`；请求为 `{ "method": "request", "request": "highlight", "object": "对象ID" }`，ID 为字符串，`"0"` 清除。语言服务的 `sunfleck/debugRequest` 通过 `method: "highlight"` 和 `object` 转发。

原生接口 `ui_app_set_debug_highlight(app, id, lease_ms)` 接受 1–5000 毫秒租约，C++/C# 对应 setDebugHighlight/SetDebugHighlight。编辑器每 250 毫秒续期，Pipe 服务固定 1000 毫秒租约；离开树行、视图隐藏或关闭时清除。异常退出或断连依靠租约清理，删除的对象在下一次 Poll 清理。调试器暂停期间应用无法重绘，恢复后再处理清除。新视图使用 Webview 提供鼠标事件，保留键盘导航、源码定位及拾取；Visual Studio 尚无对应的悬停 UI。

先按 [开发工具构建](../../core/docs/building.md#开发工具构建) 配置并构建 `out/devtools`。依赖与通用测试由构建文档维护，IDE 专项与诊断入口见 [测试说明](../../devtools/tests/development/README.md)；这里说明生成工具的用法及应用接入。

共享工具位于 `out/devtools/tools/development/Sunfleck.Development.dll`：

```sh
dotnet out/devtools/tools/development/Sunfleck.Development.dll lsp
dotnet out/devtools/tools/development/Sunfleck.Development.dll model application.dll application.fleckmodel.json Namespace.ViewModel
```

`model` 只检查类型及 Portable PDB，不构造模型或调用 getter。继承成员读取其声明模块的 PDB；缺失、非 Portable 或不匹配的 PDB 不提供源码位置。未指定类型时导出公开类，指定类型可包含内部业务模型。

## 模型元数据

格式为版本化 JSON，路径必须是运行时实际导出的名称；`Counter.Value` 只是导出名，不表示可以任意遍历对象：

```json
{
  "version": 1,
  "models": [{
    "version": 1,
    "name": "CounterModel",
    "properties": [{
      "path": "Counter.Value",
      "type": "integer",
      "writable": true,
      "source": { "uri": "file:///project/model.c", "line": 12, "column": 1 }
    }]
  }]
}
```

类型包括 `string`、`bool`、`integer`、`number`、`command`。位置从 1 开始；Fleck 映射中的行列使用 UTF-16，字节范围使用 UTF-8。运行时 JSON 的对象 ID 和序列 ID 是十进制字符串，编辑器不能把原生指针当对象标识。

命令属性可带可选的 `commandModes: ["Sync", "Async"]`，描述其静态可执行模式。直接注册命令时自动推断；动态 getter 可通过 C `ui_model_set_command_modes`、C++ `PropertyModel::setCommandModes`、C# `SetCommandModes` 显式补充，传 0/None 清除已知能力。此元数据不执行 getter，也不改变运行时绑定语义。C# 静态导出可用 `[CommandModes(CommandExecutionModes.Sync | CommandExecutionModes.Async)]` 标注命令属性。未声明能力时保持兼容；LSP 只对已知能力检查不兼容模式，隐式 CommandMode 使用默认 Sync，过期元数据降低诊断严重性。

`fleckc --schema` 的 Theme/模板规则与解析器共用 `src/fleck_schema.h`。`templateElements.elements` 提供每类元素的角色约束，`themeResources` 提供颜色、度量资源键及数值范围。语言服务据此补全 Resource.Key、组合 State 和模板按钮角色，并在悬停中显示范围。

C 使用 `ui_model_add_properties` 原子注册属性表，使用 `ui_model_set_property_source` 提供声明位置，`ui_model_describe` 导出完整注册表。C++ `PropertyModel::property` 自动记录调用位置，`describe` 导出元数据。参考 `tests/development/fixtures/model-metadata/c`、`tests/development/fixtures/model-metadata/cpp`；应用和独立元数据导出器应调用同一注册声明。

```cmake
include(path/to/Sunfleck/build/Sunfleck.Development.cmake)
add_executable(my_model_exporter model_export.c)
target_link_libraries(my_model_exporter PRIVATE sunfleck)
sunfleck_add_model_metadata(my_models EXPORTER my_model_exporter OUTPUT my.fleckmodel.json)
```

该导出器接受输出路径作为第一个参数，由构建系统在导出器变化后重新运行。

C# 项目导入 `build/Sunfleck.Development.targets`，设置 `SunfleckDevelopmentTool`，并声明模型项：

```xml
<PropertyGroup>
  <SunfleckDevelopmentTool>绝对路径/Sunfleck.Development.dll</SunfleckDevelopmentTool>
</PropertyGroup>
<ItemGroup>
  <SunfleckModel Include="Namespace.ViewModel" />
</ItemGroup>
<Import Project="path/to/Sunfleck/build/Sunfleck.Development.targets" />
```

构建结束后输出程序集同目录的 `.fleckmodel.json`。`examples/simple/csharp/fleck` 已接入，CMake 为该示例设置工具路径与构建依赖。

## 页面源码映射

```sh
fleckc --format binary --source-map page.fleckmap --source-uri file:///project/page.fleck page.fleck page.flkb
fleckc --format c --source-map page.fleckmap --source-uri file:///project/page.fleck page.fleck page.c
fleckc --schema schema.json
```

也可调用 CMake `sunfleck_compile_page(name INPUT page.fleck OUTPUT page.flkb FORMAT binary)` 自动生成资源及相邻 `.fleckmap`。生成 C/二进制加载后显式调用 `ui_app_attach_fleck_source_map`（C++ `attachFleckSourceMap`、C# `AttachFleckSourceMap`）；XML 直接保留源码范围，通过 `ui_app_set_fleck_source_uri` 设置页面 URI。

Sidecar 校验版本、内容 fingerprint、节点类型/Id 和范围后整体提交；过期文件不会覆盖已有映射。fingerprint 仅用来检测误配，不是安全签名。`.flkb` 继续使用 v5 格式，不强制携带源码。

## 绑定诊断与快照

- `ui_app_get_binding_diagnostics` 返回诊断列表；`ui_app_set_diagnostic_callback` 注册变更回调。正常 Poll 返回时投递，也可显式 `ui_app_dispatch_diagnostics`。
- C++：`bindingDiagnostics`、`setDiagnosticCallback`、`dispatchDiagnostics`。C#：`GetBindingDiagnostics`、`BindingDiagnosticsChanged`、`DispatchBindingDiagnostics`。
- 日志接入使用独立的 `ui_app_set_diagnostic_log_callback`（C++ `setDiagnosticLogCallback`、C# `BindingDiagnosticLog` 事件），不占用检查器的完整快照回调。日志回调收到同一 v1 JSON 信封，但 `diagnostics` 只包含自上次投递后改变的记录，包括恢复状态。首次注册会投递仍保留的历史；传入 NULL 或取消最后一个订阅后停止。相同错误在两次派发之间仍会合并，通过 `occurrences` 读取累计次数；此出口不是每次失败的审计流水。回调在 UI 线程执行，JSON 仅在回调期间有效，不允许重入派发或替换日志接收方。由应用选择文件、控制台或日志库并处理写入失败。
- 诊断包括页面/节点/模型 ID、页面源码、目标属性、路径、错误码、阶段、期望/实际类型、累计次数及 active 状态。恢复后保留记录并将 active 置为 false。`source` 保留元素位置，新增的 `propertySource` 定位失败 Binding 的属性值；没有属性映射时回退元素位置。两端诊断树区分失败与已恢复记录，选择记录可查看完整详情并通过源码导航定位属性；旧快照使用 `source` 回退。程序节点标注「代码」。
- `ui_app_get_debug_snapshot` 返回当前节点、模型和绑定诊断；`ui_node_get_debug_id` / `ui_app_find_debug_node` 用稳定 ID 查节点。查询不会额外调用业务 getter，PasswordBox 文本不导出。

JSON 接口先传 NULL 查询包含结尾 NUL 的所需字节数，再分配缓冲区读取；缓冲区不足返回错误并更新所需大小。除明确支持的后台通知/投递外，遵守现有 UI 线程规则。

同一 Pipe 名称由一个应用持有。名称占用或访问失败时启动同步报错并包含名称；为多个应用配置不同名称，再在检查器中选择目标 Pipe。重复启用当前名称保持原会话；切换失败保留原连接。应用暂停或断开时，对象树保留最近快照、详情及采集时间，恢复连接后更新。

C# 框架在创建 `App` 时读取启动环境中的 `SUNFLECK_DEBUG_PIPE`，配置非空名称后自动开启本机 Named Pipe 调试服务，释放 App 时关闭。VS Code 设置 `sunfleck.debugPipe` 为相同名称；`tests/development/fixtures/legacy-demos/counter` 的 F5 和 Run／Watch 入口已配置，业务代码无需读取环境变量或调用调试 API。未配置该变量时不启动服务，也不依赖托管库的 Debug／Release 编译模式。需要手动控制时仍可调用 `App.EnableDebugServer("app-name")` 覆盖连接，传入 null 关闭。应用在 UI 线程采集内存状态，附带进程 ID、`debugSession` 和 `debugProtocol`。C/C++ 可用 `ui_app_enable_debug_server(app, "app-name")`／`App.enableDebugServer("app-name")` 手动启用，传 null 关闭；创建参数加入 `UI_DEBUG_PIPE` 后从 `SUNFLECK_DEBUG_PIPE` 自动接入。原生监听由 App 持有并在 UI Poll 线程推进，使用当前用户 ACL、拒绝远程连接、同名独占及有界输入。

共享语言服务的 `sunfleck/debugSnapshot`、`sunfleck/debugRequest`、`sunfleck/debugLocate` 均要求 `pipe`。每次请求建立连接，连接及响应共用 750 毫秒期限；App 端单次读取／执行／写入最多等待两秒。App 未启动、暂停或退出时返回请求错误，语言服务继续运行，检查器下次轮询自动重试。

App Pipe 协议采用 UTF-8 单行 JSON，请求为 `{"method":"snapshot"}` 或 `{"method":"request","request":"pick"}`；结果包装在 `result`，错误包装在 `error`。状态查询使用 `snapshot` 方法，控制命令为 `pick`、`cancelPick`，直接返回执行结果；不支持任意业务求值、模型属性修改或远程执行。客户端断开不会终止 App 监听。

共享服务的 `sunfleck/debugLocate` 接受 `pipe`、本地源码 `uri` 和从 1 开始的 UTF-16 `line`/`column`，返回匹配的 `nodes`，按最小源码范围优先排序。范围末端不包含在内；两个 IDE 使用同一规则，多个窗口的同一源码对象仍保留各自的字符串 ID。服务拒绝远程源码 URI，不执行源码或业务 getter。

Windows 源码路径同时接受 `file:///C:/…` 和 VS Code 生成的 `file:///c%3A/…`。源码反查、工作区模型范围及运行时模型关联共用本地路径归一化；URI 仅解码一次，中文、空格及文件名中的字面 `%20` 保留正确含义，原生绝对路径不做 URI 解码。远程 URI 和 UNC 路径仍被拒绝。

在 VS Code 中配置 `sunfleck.debugPipe` 后自动连接，也可执行「Sunfleck: 连接调试应用」使用配置的名称或输入 Pipe 名称。在资源管理器的「Sunfleck 对象树」查看窗口、节点和绑定诊断，点击节点查看属性和定位源码。App 暂停或退出时显示等待／连接中断并自动重试，最近的内存快照保留供查看。

VS Code 0.2.2 起，打开 `.code-workspace` 时自动连接同样读取文件夹 `.vscode/settings.json`：优先当前编辑文件所在文件夹，否则选择首个配置了 Pipe 的文件夹，再回退到工作区设置；不要求先打开 Fleck。`serverPath`／`dotnetPath` 支持文件夹设置，共享语言服务的启动配置从首个工作区文件夹解析。

执行「Sunfleck: 断开调试应用」（Disconnect Debug Application），或使用树标题栏的关闭按钮，释放检查器、停止轮询并撤下该视图的运行时模型覆盖。关闭不结束业务应用。命令等待在途读取结束后释放订阅，避免迟到响应重新注册视图；扩展正常退出时先释放检查器，再停止语言服务。

「Sunfleck: 重启语言服务」保留当前 Pipe 名称和视图，恢复后重新同步模型。连续崩溃遵循客户端恢复预算：三分钟内前四次自动恢复，第五次停止，需手动重启；轮询不会绕过停止决定。App 进程替换后，检查器连接同名 Pipe 的新会话。

检查器会按需启动随包共享服务，无需先打开 Fleck 文档；随后首次打开 Fleck 文档会复用已启动的服务。仅释放检查订阅时，若服务尚未运行则不为断开操作启动它。

连接解析错误要求停止时，输出保留错误详情，并提示执行「Sunfleck: 重启语言服务」（Restart Language Server）；检查器轮询不会绕过停止决定。显式重启后，仍打开的检查器可重新同步活模型。

将光标放在 Fleck 元素内，执行「Sunfleck: 从源码定位运行时对象」可反查当前快照中的节点。命中多个对象时，按窗口与对象 ID 选择目标。可用方向键移动，按 Enter 确认。

在 Visual Studio 启动前设置 `SUNFLECK_DEBUG_PIPE` 为 App 的 Pipe 名称。打开 Fleck 文档后，编辑器右侧显示窗口、对象树、绑定诊断和选中对象的只读属性。点击「Go to source」可定位选中对象的本地源码，跨文件时会打开目标文件。将光标放在 Fleck 元素内，点「Find objects at caret」可从当前快照选择对应的对象。

在 VS Code 受信任工作区执行「Sunfleck: 运行开发任务」，可选择名称以 `Sunfleck: ` 开头的工作区任务。任务定义使用 VS Code 的工作区任务配置，可分别指向原生监听脚本或 C# `dotnet watch`。

Visual Studio 也可在启动前通过 `SUNFLECK_DEVELOPMENT_LAUNCHER` 指定绝对路径的开发启动器 EXE。快照面板显示「Run configured development launcher」，以 EXE 所在目录为工作目录启动它。启动器自身负责构建参数、状态文件和热加载配置。

Windows 应用运行时可在 VS Code 对象树点「拾取运行时对象」，或在 Visual Studio 面板点「Pick running object」。面板经共享服务通过 Pipe 投递 UI 线程请求。移动鼠标可高亮客户端区域中命中的对象，点击选择后退出，Esc 可取消。C/C++/C# 应用也可以通过 `ui_app_set_debug_picking`、`setDebugPicking` 或 `SetDebugPicking` 直接进入拾取。

Windows 拾取期间拦截普通键盘/字符、滚轮、业务鼠标点击、上下文菜单及剪贴板编辑消息，避免修改编辑器、滚动内容或执行业务命令；Esc 释放或 `cancelPick` 请求退出拾取。IME 生命周期和系统窗口管理消息继续处理。存在控件按下/拖拽、Dock 拖拽、窗口标题按钮按下、原生 Dock 移动或活动 IME 组合时，进入拾取返回 InvalidState，调用方需等交互结束后重试。

本机 `win32_inspector_protocol` 以独立 stdio 服务通过 Pipe 连接真实 App，通过 Windows 原生消息检查主窗、Dock 浮窗、独立窗口及 ComboBox 弹层 ListItem，核对输入隔离、取消、退役节点拒绝和关闭主窗后的独立窗口拾取。该回归无需 IDE，证据包含协议往返和运行时快照；合成 IME 消息仅验证忙碌状态。它不代表两端实际 IDE 面板、物理输入、真实输入法或外部视觉已验收，复现入口见 [构建与测试](../../core/docs/building.md#测试与输出)。

## Fleck 刷新与原生重建

### 同步刷新与页面会话

默认主窗口之外，可用 C `ui_app_create_window`、C++ `App.createWindow`、C# `App.CreateWindow` 创建独立窗口会话。通过 `ui_window_invoke` / `Window.withContext` / `Window.WithContext` 的同步回调选择目标，再调用现有 App API 加载页面、创建节点、查找 ID、配置窗口、刷新或设置状态钩子。退出回调自动恢复原会话，C++/C# 回调异常也会在恢复后重新抛出。例如：

```cpp
sunfleck::App app(UI_SOFTWARE);
app.loadFleck(primary_xml);
auto secondary = app.createWindow();
secondary.withContext([&] { app.loadFleck(secondary_xml); });
app.openWindow();
secondary.open();
secondary.withContext([&] { app.queueFleckRefresh(updated_xml); });
app.run();
```

每个独立窗口保存自己的活动/停用页面、Dock 布局和浮窗、刷新队列/报告及 Capture/Complete 钩子。同 ID 的节点和 Dock 项目可在不同会话并存；节点挂接、根节点和锚点要求同一会话。模型、文本源、命令及主题资源仍由 App 共享。原生输入、Dock 事件和刷新回调自动选中所属会话；其他 App 操作默认作用于主会话。新建程序工作区应在所属会话回调内执行；已有 Dock 句柄的操作自动解析所属会话。

回调只能在 UI 线程同步执行，不能重入 Poll/Run、销毁 App、创建会话或切换到另一会话；允许同一会话嵌套。Poll/原生输入分派和业务回调中也不允许显式切换会话。Window 是 App 所有的借用句柄，不单独释放；关闭后不能重新打开，资源保留到 App 销毁。默认主窗口可以不开启，关闭一个窗口只结束其会话和所属浮窗，最后一个已打开会话结束后 App.Poll/Run 才返回 EXITED。

调试快照汇总所有会话，窗口 ID 在 App 内唯一；窗口和 Dock 项目的 `ownerWindow` 指向所属独立窗口，主会话 ID 为 0。每个主窗口记录自己的 `fleckRefresh`，顶层同名字段为调用时当前会话的兼容视图。Dock API 和保存布局中的浮窗 ID 仍只在工作区内有效，不能与快照窗口 ID 混用。

`ui_app_refresh_fleck`、C++ `refreshFleck` 和 C# `RefreshFleck` 提供已打开的 Windows、X11 或 Headless 页面刷新入口；X11 分支已接入，Linux 构建和桌面运行待集中验证。非 Dock 页面的根节点可替换。DockHost 作为根节点时，修改声明 ID 会保留其运行时身份和工作区。以 Panel 包裹同一个带稳定 ID 的 DockHost，或移除该 Panel，也会保留布局与浮窗。刷新可以添加或移除整个声明式工作区，原生窗口装饰随工作区增删切换。移除时关闭浮窗，并使原生 Dock 工作区及项目句柄失效。C# 释放相关事件订阅，旧包装在访问时抛出 ObjectDisposedException。程序注册的工作区和项目由应用管理。带稳定 ID 的同类型节点可重排，也可插入、删除及替换非根节点。删除或替换后，原生裸句柄失效，可通过对象 ID 查询存活节点。C# 包装在访问旧节点或旧 Dock 项目时抛出异常。Dock 刷新支持增删声明式项目，保留仍存在的项目和浮窗，并更新声明的标题、图标、最小尺寸和默认布局。新项目优先加入同一声明组中仍停靠在主窗的同类项目。调用 Dock Reset 可应用新的默认布局。异步命令运行中、正在进行 Windows IME 组合、标题按钮捕获/原生窗口拖动中或要删除仍有待提交草稿的节点时返回错误。

同一个 App 可在打开窗口前加载多份 Fleck 页面。每次加载后用 `ui_app_get_fleck_root` 获取根节点，通过 `ui_app_set_root` 切换当前页面。页面各自保留对象 ID、源码映射、Dock 布局和 DataContext 适配器。切走时关闭该页浮窗，切回后在 Poll 中恢复。按 Fleck ID 查找、窗口对象树和热刷新作用于当前页面。可用 `ui_app_discard_fleck_page` 释放停用页面。C++ 提供 `fleckRoot` 和 `discardFleckPage`，C# 提供 `GetFleckRoot` 和 `DiscardFleckPage`。若刷新替换了根节点，重新获取该页的根节点后再切换。

上述加载／切换在选定窗口会话内进行；多个独立窗口可各自显示不同页面，使用前述窗口上下文选择目标。X11 同步和队列刷新复用同一暂存/提交路径。X11 当前采用服务端或定位式预编辑，无法可靠获知输入法组合的起止；若本次刷新会删除或替换仍持有 XIM 焦点的编辑器，同步入口返回 InvalidState，队列等待焦点离开后重试，主窗和浮窗均适用。同类型、稳定身份的编辑器可继续复用。退役节点的待粘贴目标会清理，迟到的剪贴板结果不再写入。复杂业务状态、多窗口及 Dock 的状态迁移需要按实际页面复验。

在声明式 Dock 工作区中用程序注册的项目会随刷新保留句柄、内容和当前布局。声明项目不能占用同名程序项目的 ID；移除仍包含程序项目的整个工作区时刷新会被拒绝。列表、下拉框、标签页和导航页按仍存活的子项身份保留选择，因此重排不会改变所选对象；所选项被移除时使用新声明默认值。复选框、开关、切换按钮、滑块和数字框保留未绑定的运行值。修改对应 XML 默认值或增加 Binding 后，使用新声明。

单选按钮以直接父节点为分组。分组的单选成员、默认值及 Value 绑定都未改变时，重排保留用户选择；修改默认值、增删成员、跨组移动或增加绑定后，整个受影响分组采用新声明，避免保留出两个选中项。

同 ID 的 `DockDocument` / `DockTool` 可以互换。项目和外层节点按类型替换，旧句柄失效；带稳定 ID 且类型不变的内部编辑器可保留身份与草稿。运行布局将不同类型的标签分组，浮窗保留窗口 ID 和尺寸，隐藏/关闭状态继续保留。自动隐藏的工具转为文档时回到主窗停靠；新能力禁止当前浮动状态时事务失败，旧布局和节点保持可用。Reset 使用新声明布局。布局规范化会保留空主区的 ID，避免只有浮窗时反复恢复产生无意义的布局变化。

### 等待队列

文件监听可用 `ui_app_queue_fleck_refresh`、C++ `queueFleckRefresh`、C# `QueueFleckRefresh` 投递最新 XML。接口复制内容，新请求覆盖尚未执行的旧请求；C 传 NULL/0、C++ `cancelFleckRefresh`、C# 传 null 取消。空字符串仍是需要解析的请求。Poll 在输入、IME、拖拽、回调、异步命令或未提交草稿允许时执行；临时忙碌不报告为终态失败。页面切换、模型销毁、解析/绑定失败会终止该请求并保留旧页面。回调中排入的新请求不会被正在执行的请求结果覆盖。

通过 `ui_app_get_fleck_refresh_status`（`struct_size` 和 `version=1` 必填）、C++ `fleckRefreshStatus`、C# `GetFleckRefreshStatus` 查询序号、结果、等待状态、耗时及诊断。队列保存页面和模型的稳定身份，不持有调用方 XML 缓冲区或悬空模型指针。C# 在 Poll 完成和回调访问前同步刷新序号，使被删除或替换的旧包装安全失效。计数器的 `--watch-fleck` 使用此公共队列。

`pending` 包括正在 Poll 安全点执行的请求，因此 getter 或业务状态回调在提交期间查询节点时，托管包装继续跟踪本次刷新。提交及状态记录完成后再清除；回调中排入的新请求继续保持等待状态。

### 业务状态钩子

应用自有状态通过 `ui_app_set_fleck_refresh_state_callback`、C++ `setFleckRefreshStateCallback` 或 C# `SetFleckRefreshStateCallback` 接入。Capture 在解析成功、开始暂存前执行，可把业务选择、筛选条件或额外草稿保存在应用自己的上下文中；返回非成功值拒绝本次刷新，旧页保留。Capture 成功后，Complete 对本次暂存尝试执行一次，参数带实际结果。只有结果成功时才按 Fleck ID 或对象 ID 重新解析新控件并恢复；失败或忙碌时释放暂存状态。公共等待队列的重试可能多次执行这对回调。解析失败或进入 Capture 前已经忙碌时不调用。

回调在 UI 线程执行，可以查询节点和设置内容，不能重入 Poll/刷新、切换/销毁页面或替换钩子。Complete 失败不撤销已提交页面，通过现有回调错误通道报告。状态内存和持久化由应用管理，库不遍历或序列化任意业务对象。C# 在成功的 Complete 回调前使退役包装失效；应重新调用 `FindFleckNode`，不能继续使用被替换的旧包装。传空回调取消注册。可运行的三语言示例位于 `tests/development/fixtures/refresh-state`，构建目标为 `refresh_state_c`、`refresh_state_cpp`、`refresh_state_csharp`。

### 监听构建与重启

Windows 应用可从 VS x64 开发者 PowerShell 运行 `tools/development/watch-native.ps1`，传入 CMake 构建目录、目标、可执行文件和要监听的源码目录。脚本合并短时间内的连续修改，构建成功后启动暂存副本，避免运行中的 EXE 锁定 CMake 输出。构建失败时保留旧进程。`-Once` 只验证构建产物。可传入 `-StateFile`，脚本通过 `SUNFLECK_RESTART_STATE_FILE` 将路径传给应用，并等待旧窗口正常退出后启动新进程。三语言代码计数器使用此路径保存和恢复十进制计数；状态文件由应用读写。

VS Code 可以用「Sunfleck: 运行开发任务」启动监听器。在受信任工作区的 `.vscode/tasks.json` 中添加以下任务。使用现有 `out/devtools` 构建目录，并从 VS x64 开发者 PowerShell 启动 VS Code，使任务继承原生构建环境。

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Sunfleck: Restart C# counter on change",
      "type": "process",
      "command": "powershell.exe",
      "args": ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools/development/watch-native.ps1",
        "-BuildDirectory", "out/devtools", "-Target", "counter_csharp_code",
        "-Executable", "out/devtools/examples/simple/csharp/code/CounterCode.exe",
        "-SourceDirectory", "examples/simple/csharp/code", "-StateFile", "out/devtools/csharp-counter-state.txt"],
      "options": { "cwd": "${workspaceFolder}" },
      "problemMatcher": []
    }
  ]
}
```

三语言任务共用监听器参数。复制任务时按下表替换，并为每种语言使用独立状态文件。C/C++ 先执行 `cmake -S <源码目录> -B <构建目录> -G Ninja -DUI_BUILD_CSHARP=OFF -DUI_BUILD_DEMOS=OFF` 配置独立工程；C# 使用上文整仓构建。三语言示例均可通过 `SUNFLECK_DEBUG_PIPE` 配置调试连接。原生示例启用 `UI_DEBUG_PIPE`；监听器在构建成功后先释放旧进程的独占 Pipe，再启动新会话。

| 参数 | C | C++ | C# |
| --- | --- | --- | --- |
| `-BuildDirectory` | `out/devtools/c-state` | `out/devtools/cpp-restart` | `out/devtools` |
| `-Target` | `counter_c_code` | `counter_cpp_code` | `counter_csharp_code` |
| `-Executable` | `out/devtools/c-state/counter_c_code.exe` | `out/devtools/cpp-restart/counter_cpp_code.exe` | `out/devtools/examples/simple/csharp/code/CounterCode.exe` |
| `-SourceDirectory` | `examples/simple/c/code` | `examples/simple/cpp/code` | `examples/simple/csharp/code` |


在任一任务的参数末尾添加 `-NoHotReload` 可关闭保存触发重建。监听器仍完成初次构建和启动，保留状态文件交接；C# 应用继续使用已配置的 Pipe 接受调试连接。应用正常关闭后任务返回应用退出码。

Dock 界面状态可通过 `ui_dock_save` / `ui_dock_restore`（C++ `Dock::save` / `Dock::restore`）跨 App 实例保存和恢复。应用将返回的布局 JSON 写入自己的状态文件，重新创建同一工作区及其稳定项目 ID 后调用恢复。布局和业务计数可以分别保存。

### C# Hot Reload

C# 的上述监听任务使用重建重启；进程内方法体更新使用 `dotnet watch`。

C# Fleck 计数器可用 XML 启动并观察兼容的页面变更。应用通过已配置的 Pipe 接受调试连接：

```powershell
$env:SUNFLECK_DEBUG_PIPE = 'sunfleck-example'
dotnet out/devtools/examples/simple/csharp/fleck/CounterFleck.dll --watch-fleck examples/simple/csharp/fleck/counter.fleck
```

传入 `--live-model-file` 和目标 `.fleckmodel.json` 路径可发布运行时类型元数据。示例在启动和收到 .NET 元数据更新通知后更新该文件。将路径加入 VS Code 的 `sunfleck.modelFiles` 即可使用已有的元数据刷新机制。Fleck 计数器设置 `SUNFLECK_RESTART_STATE_FILE` 后，每次异步增量提交时会先写回计数，重新启动时读取。属性改名时应同步更新 Fleck 的绑定路径。对无法应用的 C# 编辑，可用 `dotnet watch --non-interactive` 让监听器自动重启。参见 [.NET CLI 文档](https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-watch)。

同一示例也可通过 `dotnet watch` 接收 C# 方法体更新。先用 CMake 构建原生库，再从仓库根目录运行以下命令。修改 `CounterModel.Caption` 的返回值后，代码更新会在应用的 UI 循环中刷新现有绑定。

```powershell
$env:SunfleckNativePath = (Resolve-Path out/devtools/sunfleck_native.dll).Path
dotnet watch --project examples/simple/csharp/fleck/CounterFleck.csproj run -- --watch-fleck examples/simple/csharp/fleck/counter.fleck
```

兼容方法体／Getter 更新通过元数据更新通知刷新现有绑定，同进程更新保留已有快照连接。不兼容编辑由 `dotnet watch` 重建重启，状态恢复依赖应用持久化最后已提交数据。

#### portable PDB 回退

使用 .NET SDK 11.0.100 构建 `net10.0` 时，`dotnet watch` 的不兼容编辑重启曾因 portable PDB 被占用而报 CS2012。诊断发现 watcher 会话持有较大的 PDB 映射，对照实验中提前结束会话后重建成功。工作区 watcher 副本仅用于复现。Watch 示例使用 embedded PDB，F5 使用 portable PDB，各自按对应配置复验。C# 热重载和重启回退的后续开发与验收暂缓。[PDB 诊断工具](../../devtools/tests/development/README.md#pdb-诊断工具)


### MSVC Edit and Continue

在 VS x64 开发者 PowerShell 中为 C++ 计数器建立独立 Debug 构建目录：

```powershell
cmake -S examples/simple/cpp/code -B out/devtools/msvc-enc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=EditAndContinue -DCMAKE_EXE_LINKER_FLAGS_DEBUG="/DEBUG /INCREMENTAL" -DUI_ENABLE_D3D11=OFF
cmake --build out/devtools/msvc-enc
ctest --test-dir out/devtools/msvc-enc --output-on-failure
```

用 Visual Studio 调试该 Debug EXE，在 `increment` 中修改方法体，再点击按钮并确认原进程执行了新逻辑。本机 VS 2026 已在 /ZI、v145、增量链接的独立工程中通过。支持的改动及链接选项见 [Microsoft 的 C++ 热重载说明](https://learn.microsoft.com/en-us/visualstudio/debugger/supported-code-changes-cpp)，复验入口见 [实际宿主测试](../../devtools/tests/development/README.md#visual-studio-实际宿主与-enc)。

## VS Code

开发工具界面默认使用简体中文。在「设置」中搜索 `sunfleck.language`，选择 `zh-CN` 或 `en`。命令面板、快照对象树和 Fleck 语言服务的绑定提示随设置切换。快照中的业务文本、运行时原始诊断及原生解析器返回的诊断按原文显示。

打开 `.fleck` 文件后可执行 VS Code 的「格式化文档」。格式化按当前缩进设置调整元素之间的空白，保留属性和绑定表达式。不完整 XML 或含混合文本内容的文档会保留原样。Fleck 默认启用保存时格式化。要覆盖默认设置，在用户或工作区的 `settings.json` 中设置：

```json
{
  "[fleck]": {
    "editor.defaultFormatter": "sunfleck.sunfleck",
    "editor.formatOnSave": true
  }
}
```

将 `editor.formatOnSave` 设为 `false` 可关闭 Fleck 保存时格式化。

先构建共享工具，再在 `editors/vscode` 执行 `npm ci --ignore-scripts`、`npm run check`、`npm run package`；打包工具需要 Node 20.18 或更新版本。打包会复制当前构建平台的共享服务和原生库；Windows x64 可附加 `--target win32-x64` 参数。安装生成的 VSIX，打开 Fleck 文件。

- `sunfleck.serverPath`：共享工具 DLL 路径。留空查找扩展附带的 `server` 或工作区 `out/devtools/tools/development`。
- `sunfleck.dotnetPath`：默认 `dotnet`。
- `sunfleck.modelFiles`：元数据文件路径列表，可相对工作区，也可使用 `${workspaceFolder}`；留空发现工作区的 `.fleckmodel.json`。
- `Sunfleck: 重启语言服务`：重新连接服务。

元数据文件变化时更新补全与诊断，包括通过 `sunfleck.modelFiles` 显式配置的工作区外绝对路径。VS Code 多根工作区按文件夹解析模型。每个文件夹可以设置自己的 `sunfleck.modelFiles`，同一文件夹内有多个同名模型时选择当前构建配置。元数据缺失或比源码/程序集旧时显示不确定性，语法功能继续工作。尚未打包共享工具时应设置 `serverPath`。

## Visual Studio

通过 Edit → Advanced → Format Document（默认 Ctrl+K、Ctrl+D）格式化当前 Fleck 文档。通过 Tools → Options → Sunfleck → Fleck 启用 Format on save，默认关闭。两个入口都使用当前编辑器的 Tab 大小和空格设置。

快照面板顶部的「开发工具界面语言」下拉框可在「简体中文」和「English」之间切换。编辑器内可按 `Ctrl+Alt+Shift+F11` 打开下拉框，或按 `Ctrl+Alt+Shift+L` 直接切换。默认简体中文，选项按用户保存并立即应用到同一实例中打开的快照面板。Fleck 语言服务在下次启动 Visual Studio 时读取保存的语言。

需要 Visual Studio 17.14 或更新的 x64 编辑器及 .NET 10：

```sh
dotnet build editors/visualstudio/Sunfleck.VisualStudio.csproj
```

生成 `editors/visualstudio/bin/Debug/net472/Sunfleck.VisualStudio.vsix`；包中包含共享服务与原生库，不自动安装或部署到用户 IDE。可设置 MSBuild `SunfleckServerDirectory` 指向其他服务输出目录。

构建会执行 `tests/development/check-vsix.ps1`，校验清单中的每个资产路径及随包服务文件。MEF 清单使用实际程序集名 `Sunfleck.VisualStudio.dll`，`Fleck.pkgdef` 将 `.fleck` 注册到通用代码编辑器，避免由 XML 编辑器接管。

启动 VS 前通过环境变量配置当前模型：

- `SUNFLECK_MODEL_FILES`：用平台路径分隔符（Windows 为分号）分隔的元数据绝对路径。
- `SUNFLECK_DEVELOPMENT_TOOL`：可选，共享服务 DLL 的覆盖路径。
- `SUNFLECK_DOTNET`：可选，dotnet 可执行文件。

`.fleck` 内容类型的语言客户端通过 stdio 连接同一个 LSP，复用绑定诊断和源码跳转；语法着色使用共享语义 token。

未指定 `SUNFLECK_MODEL_FILES` 时，Tools → Options → Sunfleck → Fleck 的 Model files 可配置分号分隔的路径，相对路径以解决方案目录为基准。两者均为空时，按已加载项目的目录发现元数据，排除 `.git`、`.vs`、`node_modules`、`obj`、其他已加载项目的根目录及非活动构建配置的目录。每个项目使用独立模型范围，客户端每两秒检查项目/配置、文件集合和元数据修改时间并通知语言服务。项目外页面或特殊输出布局也可使用显式配置。

## LSP 协议与验证

协议使用标准 `Content-Length` framing、UTF-8 JSON、UTF-16 位置和完整文档同步。提供 completion、hover、definition、documentSymbol、semanticTokens/full、onTypeFormatting、formatting、publishDiagnostics。`initializationOptions.modelFiles` 指定元数据路径，`workspace/didChangeWatchedFiles` 触发重读。

动态注册可通过 `sunfleck/setModels` 发送 `{ "models": [...] }` 设置当前 LSP 连接的完整显式动态集合，可使用请求或通知；成功请求返回 JSON `null`。该集合应用于本连接的默认目录及当前／后续配置的项目范围，不跨连接共享。后续消息整体替换，省略的模型不再属于显式集合，`models: []` 清空集合。模型名区分大小写；同名模型覆盖完整属性集合，不与静态属性逐项合并，不同名模型继续共存。

服务先验证全部模型、属性及集合内重名，再提交显式集合；无效请求返回 `-32602`，保留此前完整集合。重复发送相同集合不会产生同名歧义。配置切换或元数据文件重读保留当前显式集合；被移除的显式模型恢复使用该时刻有效的静态元数据。客户端重启／新连接不会继承旧连接的显式集合，调用方应按需重发。VS Code 当前界面没有直接发送 `setModels` 的公开命令，该方法属于共享 LSP 协议。

### 运行时模型自动同步

两个 IDE 的检查器每 500 毫秒通过现有 stdio 连接请求 `sunfleck/debugSnapshot`。共享服务将返回快照中的模型声明自动用于补全、悬停、定义跳转及绑定诊断，无需额外发送 `setModels`。同步只读取注册元数据，不额外执行业务 getter、命令或 setter。

节点 `source` 新增两个可选字段：`model` 为页面声明的 `Window Model` 名称，`modelId` 为装载该页时实际传入或生成的模型 ID（十进制字符串，`"0"` 表示没有模型）。服务用 `source.uri`、`source.model`、`source.modelId` 关联顶层 `models` 注册表，采用实际实例的完整属性集合，包括未绑定属性、读写能力、命令模式及声明源码。XML、二进制和生成 C 共用该关联；后两条路径仍须附带正确的 `.fleckmap` 才有原始源文件信息。没有 Binding 的页面也可提供模型元数据。

自动模型只对快照中对应的精确本地源文件生效，不向整个工作区广播同名模型；Windows 路径比较忽略大小写。语言查询按「当前源文件的运行时模型 → 显式 `setModels` → 文件/项目范围的静态模型」取值。只覆盖同名模型，不修改原来的显式集合或静态目录；重新配置、监视文件重读及显式集合更新仍保留当前运行时覆盖。没有名称、源 URI、实例关联的旧快照或代码页面继续使用显式/静态元数据，不猜测模型类名。

此顺序按模型名决定整组属性、命令能力及声明位置，不以某个属性是否存在来向下补齐。有效运行时覆盖消失或该订阅读取失败后，恢复的是**最近一次成功提交**的显式集合；该集合不含此模型时才使用当前配置的静态模型。运行时同名冲突仍返回不确定性，显式／静态元数据均不能掩盖冲突。冲突消失或同名 Pipe 恢复后，原订阅下一次成功读取重新应用运行时模型；低优先级集合在此期间的更新保留，供之后回退使用。

| 方法 | 参数与生命周期 |
| --- | --- |
| `sunfleck/debugSnapshot` | `{ "pipe": "app-name", "owner": "view-token" }`。正常读取后完整替换订阅的模型集合。`owner` 可选，提供时为 1–128 字符的非空白字符串；未提供时使用该 Pipe 的默认订阅 |
| `sunfleck/debugDisconnect` | 使用相同 `pipe` 和可选 `owner`，移除该订阅并返回 `{}`；重复移除无副作用，不发送运行时命令 |

两个客户端为每个检查视图生成独立 `owner`。VS Code 更换连接或释放扩展、VS 文档边栏释放时，发送 disconnect；若读取尚未返回，先等待该次读取结束再移除，避免已释放视图被迟到响应重新注册。同一 Pipe 的其他视图仍可保留自己的订阅。其他客户端使用默认订阅时，应在结束检查后显式 disconnect。

VS Code 每窗口保留一棵检查器树，更换连接会释放旧树；读取未完成时，新旧 owner 的生命周期可能短暂重叠。在途读取释放和独立 owner 回归由 `inspector-client.js` 覆盖；VS 实际多边栏交互后续验收。

元数据随后续快照完整替换：属性移除、页面退役、模型销毁、同名 Pipe 的进程替换均不累积旧属性。多个存活页面/订阅对同一源文件、同一模型名提供相同元数据时可以共存；内容不同时返回 `model_uncertain`，直到冲突消失。不会选择最后一次到达的实例，也不会在冲突期间用静态模型掩盖运行时歧义。属性路径重复、模型 ID 重复等损坏元数据或快照读取失败会移除本订阅的覆盖并返回 LSP 错误；语言服务继续处理后续消息。其他订阅保留到各自更新或释放。

相同模型集合的轮询不重复发布诊断，时间戳或界面值变化不会单独触发模型诊断刷新。连接失败时撤下本订阅的运行时模型，语言功能使用显式／静态元数据；对象树仍可显示最近一次完整结果。App 恢复或新进程使用同名 Pipe 启动后，下一次成功轮询重新同步。

发包时在实际 VS Code 和 Visual Studio 宿主中复验补全、诊断、F12、对象树、断点暂停与继续、退出重连、扩展重载和多实例切换。Windows 的 IDE 验收对应报告中的 VSIX 和宿主版本，Linux 的窗口、刷新与 XIM 记录见 [Linux/Web 基线](../../core/docs/validation.md#m9-linuxweb-基线)。模型自动同步的定向入口见 [构建与测试](../../core/docs/building.md#测试与输出)。

Windows 下可对现有结构刷新测试采样进程内存。每 100 次刷新输出一行 CSV，数值为字节，分别记录进程私有提交量和工作集。采样覆盖 Headless 夹具的 1,000 次刷新，可用以下 PowerShell 命令复现：

```powershell
$env:SUNFLECK_MEASURE_REFRESH_MEMORY = '1'
try {
    ctest --test-dir out/devtools -R '^development_metadata$' --repeat until-fail:3 -V
} finally {
    Remove-Item Env:SUNFLECK_MEASURE_REFRESH_MEMORY
}
```


## 组件元数据与依赖

语言服务器从项目的 Fleck 定义、代码声明及生成的 `.fleckcomponents.json` 交换元数据读取组件和模板信息，为所有命名空间标签（包括内建标签）、公开属性、模板名称提供补全和定义跳转。C# 直接用 Roslyn 分析项目源代码，C/C++ 使用与运行时共享的声明导出器；两者都不会为补全调用构造函数或 getter。Visual Studio 和 VS Code 使用同一语言服务器。

`RunFleckFile` 监视页面目录下的组件和模板文件，将变更资源与页面一起排队到 UI 安全点。解析或验证失败时保留当前树；恢复文件后再次提交。状态迁移以组件实例路径、局部 Id 和类型匹配，没有稳定 Id 的组件不保证恢复状态。重新加载后应重新查询组件实例和节点句柄，不能持有已退出页面的句柄。

动态内存 XML 使用 App 已注册资源，或显式资源表，不从工作目录推测依赖。`fleckc --source-map` 输出多文件位置映射，发布时可选附带映射用于定位组件内部节点；运行组件本身不需要映射或原始 XML。
