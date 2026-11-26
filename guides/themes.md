# 主题与 Dock 皮肤

Sunfleck 提供 Fluent 2、Material 3、Radix Themes、Apple Liquid Glass 和 Sunfleck Lite 五套主题，均支持浅色、深色及 Normal、Compact 密度。新应用默认使用 Sunfleck Lite 浅色、Normal 密度；独立 Dock 示例默认使用 Compact。

## 使用内置主题

- C：`ui_app_set_builtin_theme(app, UI_BUILTIN_MATERIAL_3, 1, 0)`，最后两个参数分别为深色、紧凑模式。
- C++：`app.setTheme(UI_BUILTIN_MATERIAL_3, true)`。
- C#：`app.SetTheme(BuiltinTheme.Material3, dark: true)`。

颜色、尺寸和形状可通过下面的主题描述继承及覆盖。普通控件使用现有主题机制，部件模板和绘制回调仅适用于 Dock。

Apple Liquid Glass 对窗口内已绘制背景进行模糊采样和半透明着色，叠加高光、边缘光及轻阴影。它不采样桌面、不调用 Apple 系统材质。软件、D3D11 与 GLES3 消费相同的抗锯齿绘制命令。

## 自定义主题入口

流程为：**选择基础主题 → 编写资源 → 按需添加模板或注册回调 → 加载并应用主题**。

`ui_app_load_theme` / `sunfleck::loadTheme` / `App.LoadTheme` 加载 UTF-8 Theme 文档或编译主题资源。`ui_app_apply_theme` 接收 `ui_theme_descriptor`（struct_size、version=1、base、dark、compact、skin 和资源片段）；C++ `applyTheme`、C# `ApplyTheme` 提供对应简便接口。资源片段包含 Resource 和 DockTemplate 元素，无 Theme 外层。所有字符串在调用内复制。

这两个入口继承基础主题的全部形状，只覆盖显式资源。原有 `ui_theme` ABI 和数组大小保持不变；旧 `ui_app_set_theme` 会恢复 Sunfleck Lite 形状并清除 Dock 扩展资源。调用 `ui_app_set_builtin_theme` 也会清除此前的扩展资源；需要保留自定义片段时应再次通过 applyTheme/ApplyTheme 应用。

主题切换更新主窗口、浮动窗口和自动隐藏浮层，尺寸变化触发重新布局，业务内容保留原状态。

### 1. 资源覆盖

从 [资源示例](../../themes/examples/resources/theme.fleck) 开始：

```xml
<Theme Version="1" Base="Material3" Dark="true" Compact="false">
  <Resource Key="primary" Value="#c9a778"/>
  <Resource Key="dock.title.background" Value="#302b26"/>
  <Resource Key="dock.tab_height" Value="32"/>
  <Resource Key="dock.radius" Value="6"/>
</Theme>
```

Base 支持 Fluent2、Material3、Radix、LiquidGlass、SunfleckLite，缺省 SunfleckLite。Version 缺省为 1。资源名区分大小写，颜色为 `#RRGGBB`。

| 资源 | 含义 |
|---|---|
| `background/panel/card/text/primary/on_primary/selected/control/focus/divider/scrim` | 现有全局颜色，Dock 自动继承 |
| `gap/control_height/window_padding/gallery_gap` | 现有逻辑尺寸 |
| `button_radius/input_radius/card_radius/popup_radius` | 显式覆盖基础主题圆角 |
| `dock.<part>.background/foreground/border/active/hover/disabled` | Dock 部件颜色覆盖 |
| `dock.title_height/tab_height/splitter_width/rail_width` | 组标题、标签、分隔条和自动隐藏栏逻辑尺寸 |
| `dock.window_title_height` | 自绘窗口标题栏逻辑高度，绘制时至少为 28，缺省为 32 与 control_height 中较大值 |
| `dock.floating_padding` | 浮窗内容外边距，默认 4 个逻辑像素，可设为 0–64；主窗口仍使用 `window_padding` |
| `dock.tab.accent` | 活动标签底部强调线，默认 primary |
| `dock.group.border` | 分组细边框，默认 divider |
| `dock.window_close.hover_foreground` | 窗口关闭按钮悬停时的图标颜色 |
| `dock.auto_hide_width/auto_hide_height` | 自动隐藏展开区尺寸 |
| `dock.radius` | Dock 部件圆角 |
| `custom.<name>` | 模板可引用的命名颜色 |

`<part>` 为 `title/tab/close/pin/float/splitter/indicator/preview/auto_hide/menu/menu_item/window_title/window_minimize/window_maximize/window_close`。常用默认映射如下：

| 部件 | 默认映射与形状 |
|---|---|
| 组标题、窗口标题 | panel 背景、text 文字 |
| 标签 | 活动为 selected，其他为 control；divider 边框和 2 逻辑像素 primary 活动强调线 |
| 标签、分组外轮廓 | 继承 input_radius，上限 8；dock.radius 可覆盖 |
| 组内操作按钮 | 静止沿用下方底色，悬停底板内缩；显式 background 可覆盖 |
| 预览、指示器 | primary 预览及图形，card 指示器底色 |
| 分隔条 | panel 底色、divider 中线，交互时用 primary |
| 窗口外框、关闭按钮 | 活动框 primary、非活动框 divider；关闭悬停为红色 |

标签文字预留关闭按钮空间。边界裁剪、自动隐藏文字方向及窗口行为由框架管理，见 [Dock](docking.md#窗口与交互)。

### 2. Fleck 部件模板

参考 [模板示例](../../themes/examples/templates/theme.fleck)。模板仅作用于 Dock 装饰，不创建业务 Node：

```xml
<DockTemplate Part="Title">
  <Stack Role="Drag" Background="{Background}" Padding="2">
    <Text Text="{Icon}" Width="24"/>
    <Text Text="{Title}" Foreground="{Foreground}"/>
    <Button Role="Menu" Text="v" Width="28"/>
    <Button Role="Close" Text="X" Width="28"/>
  </Stack>
</DockTemplate>
```

Part 支持 Title、Tab、Close、Pin、Float、Splitter、Indicator、Preview、AutoHide、Menu、MenuItem、WindowTitle、WindowMinimize、WindowMaximize、WindowClose。State 可为 Normal、Active、Hover、Pressed、Disabled，并可用 `|` 组合；匹配当前状态且包含最多状态位的模板优先。Disabled 用于能力或类型限制导致的不可停靠预览与指示器。能力不允许的操作按钮自动从模板布局中排除；文档不显示 Pin。MenuItem 的 `{Title}` 为当前菜单项的操作名称。

模板元素为 Fill、Border、Text、Button、Stack。可用属性：Background、Foreground、Text、Role、Orientation、Radius、Thickness、Width、Height、Padding。Stack 缺省水平，指定尺寸的子项固定占位，其余平分剩余空间；其他元素的子项在内部区域叠放。Padding 缩进子元素区域，需要文字内边距时将 Text 包在带 Padding 的容器内。Border 的线条颜色来自 Foreground。

`{Title}`、`{Id}`、`{Icon}` 为文字上下文；`{Background}`、`{Foreground}`、`{Border}` 为当前部件颜色，也可引用全局或 custom 颜色。AutoHide 的 Text 与绘制回调中的 ui_draw_text 自动采用边栏文字方向，左右均从上到下阅读，皮肤无需手动旋转；部件边界与命中坐标仍为窗口像素。

Role=Drag 标记拖拽区域，Button Role 为 Menu、Close、Pin 或 Float。框架负责捕获、目标判断、能力限制和操作，模板布局决定部件命中区域。Title 模板包含操作按钮时不追加默认组按钮与标签关闭按钮；Tab 模板包含按钮时不追加该标签的默认关闭按钮。绘制裁剪在当前部件内。

WindowTitle 和三个 Window 控制部件的模板用于绘制；窗口按钮的位置、命中区域、拖动和缩放始终由框架管理，不通过模板 Button Role 重排窗口控制。WindowTitle 的 `{Title}` 为窗口标题。窗口部件沿用回调 → 模板 → 内置绘制的回退顺序，使用同一 CPU/GPU 路径。

### 3. 代码绘制回调

[C/C++ 示例](../../themes/examples/callback/accent_tabs.h) 和 [C# 示例](../../themes/examples/callback/AccentTabs.cs) 注册 AccentTabs，再加载 [引用回调的主题](../../themes/examples/callback/theme.fleck)：

```c
ui_app_register_dock_skin(app, "AccentTabs", accent_tabs, NULL);
/* 读取 theme.fleck 为内存缓冲区后： */
ui_app_load_theme(app, text, length, &diagnostic);
```

```csharp
AccentTabs.Register(app);
app.LoadTheme(File.ReadAllText("theme.fleck"));
```

回调接收部件、窗口像素边界、DPI、标题、ID、图标、状态、能力与解析后的颜色。图标是可选 UTF-8 字形或皮肤识别的名称；回调可按名称绘制自己的图像。`DrawContext` 提供 Fill、Stroke、Text、Image、PushClip、PopClip，全部进入现有 CPU/GPU 命令路径，不暴露原生绘图句柄。

坐标均为**当前窗口像素**；资源尺寸使用逻辑像素，可按 dpi/96 缩放。Image 为同步复制的 RGBA 像素。框架保护部件裁剪边界，用户只能弹出自己 Push 的裁剪。上下文仅在回调期间有效，禁止保留或跨线程使用，绘制期间禁止修改 App/Dock。C# 会检查上下文失效和线程。

每个部件依次尝试具名回调、匹配的模板、内置绘制。回调返回 0/false 表示未处理，该次绘制命令丢弃并继续回退；返回非零/true 完成处理。回调只改变绘制，框架仍维护命中和操作。名称必须显式注册；加载引用未知皮肤的主题失败并保留当前主题，不自动加载 DLL。

### 编译主题

在仓库根目录执行，以下为 Windows Debug 产物；Linux 将工具路径改为 `./out/debug/fleckc`：

```powershell
.\out\debug\fleckc.exe --format binary docs/snippets/themes/templates/theme.fleck out/debug/theme.flkb
.\out\debug\fleckc.exe --format c --symbol load_theme docs/snippets/themes/templates/theme.fleck out/debug/theme.c
.\out\debug\fleckc.exe --format cpp --symbol load_theme docs/snippets/themes/templates/theme.fleck out/debug/theme.hpp
```

生成的 C 函数或 C++ inline 函数签名均为 `ui_result load_theme(ui_app*, ui_fleck_diagnostic*)`；C++ 封装应用可传入 `app.nativeHandle()`。编译主题使用 `FLKTH` v1 包装，由主题 API 加载；页面资源统一使用 `FLECK` v10；旧版本的迁移要求见 [布局属性](layout-properties.md)，两类资源分别使用对应加载入口。



## 自定义控件的布局模板

`ControlTemplate` 是独立 Fleck 文档，负责自定义 `Control` 的视觉节点组合。它与主题中的绘制模板分别处理布局和外观：模板中的 Button、Slider 等节点继续使用当前主题。类型注册指定默认模板，调用处使用 `Template="controls:CompactRatingTemplate"` 替换；`TemplateBinding` 单向读取控件属性，双向编辑使用 `RelativeSource=TemplatedParent`。示例与契约见 [Fleck 组件](fleck-components.md#模板)。首版没有模板触发器和 ContentPresenter。
