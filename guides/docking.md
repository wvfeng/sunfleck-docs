# 原生 Dock

Dock 在 C++ 核心实现，C、C++、C# 与 Fleck 共享模型、布局和绘制。支持 Windows、Linux X11 与 Headless，参考 AvalonDock 的使用场景，但不依赖 WPF，也不兼容其布局文件。

## 创建工作区

每个独立窗口会话支持一个活动工作区、一个主 DockHost 和多个浮动窗口。App 的默认窗口和新增 Window 分别保存自己的工作区；同一会话的停用 Fleck 页面也可保留布局。新增会话通过 [窗口上下文入口](devtools.md#同步刷新与页面会话) 配置。文档与工具使用独立标签组，可以拆分、合并或浮动：

```cpp
#include <sunfleck/dock.hpp>

sunfleck::App app{UI_AUTO};
app.setWindowOptions("Workspace", 1100, 760);
auto host = app.createNode(UI_DOCK_HOST);
app.setRoot(host);
sunfleck::Dock dock{host};
auto editor = app.createNode(UI_TEXT_AREA);
editor.setText("Document contents");
auto document = dock.add("editor", "Document", editor);
auto explorer = dock.add("explorer", "Explorer", app.createNode(UI_PANEL), UI_DOCK_TOOL_KIND);
explorer.move(&document, UI_DOCK_POSITION_LEFT);
dock.captureDefault();
app.openWindow();
app.run();
```

Dock、DockItem、Node 借用 App 的生命周期，所有操作在 App 的 UI 线程执行。项目 ID 在工作区内唯一，最多 128 个 UTF-8 字节；持久化依赖该 ID 的稳定性。注册的内容必须属于同一 App 和窗口会话，且尚未挂到父节点、作为会话根或由其他 Dock 项使用。

内容注册后由工作区管理位置，通过 Dock 接口移动，不要调用 `ui_node_append` 换父。移动、浮动和关闭不销毁内容节点，保留文本、绑定、光标及选区。内部标题、标签和预览直接生成绘制命令，不分配业务节点。Fleck 刷新删除/替换项目或移除工作区时，退役句柄会失效，契约见 [刷新与重建](devtools.md#fleck-刷新与原生重建)。可在 [组件展厅](../../samples/README.md) 的 Dock 页体验。

## API 与状态

| 操作 | C API | C++ DockItem / Dock | C# DockItem / DockWorkspace |
|---|---|---|---|
| 注册、查找 | `ui_dock_register/find` | `add/find` | `Register/Find` |
| 移动单项、整组 | `ui_dock_move/move_group` | `move/moveGroup` | `Move/MoveGroup` |
| 激活 | `ui_dock_activate` | `activate` | `Activate` |
| 隐藏、显示 | `ui_dock_hide/show` | `hide/show` | `Hide/Show` |
| 请求关闭 | `ui_dock_close` | `close` | `Close` |
| 浮动单项、整组 | `ui_dock_float` | `floatWindow` | `Float` |
| 自动隐藏、固定 | `ui_dock_auto_hide/pin` | `autoHide/pin` | `AutoHide/Pin` |
| 标题、图标 | `ui_dock_set_title/set_icon` | `setTitle/setIcon` | `SetTitle/SetIcon` |
| 保存、恢复 | `ui_dock_save/restore` | `save/restore` | `Save/Restore` |
| 默认布局、重置 | `ui_dock_capture_default/reset` | `captureDefault/reset` | `CaptureDefault/Reset` |

表中同前缀函数采用缩写，完整声明见 [C API](../../core/include/sunfleck/ui.h)。注册时初始化 `ui_dock_item_options.struct_size`、`version=1`，并指定类型、能力及逻辑最小尺寸。CanClose、CanFloat、CanAutoHide 对应 `UI_DOCK_CAN_CLOSE`、`UI_DOCK_CAN_FLOAT`、`UI_DOCK_CAN_AUTO_HIDE`；自动隐藏仅适用于工具。窗口小于布局最小尺寸之和时，按比例压缩区域。

工具关闭后隐藏，Show 或固定时恢复原分组/拆分位置；原组或浮窗已清理时按返回信息重建。文档关闭后移出布局，仍可显式 Show。移动仅限同一工作区，不支持跨 App 拖拽。

`ui_dock_set_event` 接收 Closing、Changed、Activated。Closing 返回非零取消关闭，浮动组关闭时逐项请求，拒绝关闭的项目继续保留。回调内可更新内容属性，禁止布局重入、替换根、再次 Poll 或销毁 App。C/C++ 不应让异常跨越 C ABI。

```csharp
var dock = app.CreateDock(host);
dock.Event += (_, args) =>
{
    if (args.Kind == DockEventKind.Closing && args.Item?.Id == "editor" && hasUnsavedChanges)
    {
        args.Cancel = true;
    }
};
```

C# 保活委托并在同步操作或 Poll 返回后重新抛出回调异常；Closing 抛异常视为取消，App.Dispose 释放保活引用。

## 窗口与交互

- 拖动标签移动单项并调整顺序，拖动组标题移动整组；中心合并，边缘拆分，窗口外释放浮动。拖动浮窗自绘标题也可回停；五向指示器提供固定命中区，落点预览随方向更新。Esc 或捕获丢失取消操作，分隔条用于缩放。
- 标题、标签和右侧停靠菜单位于同一行，高度取 `dock.title_height`、`dock.tab_height` 较大值。可关闭标签各有关闭按钮，宽度不足 48 个逻辑像素时省略内嵌按钮，可通过菜单关闭活动项。关闭未激活标签不切换当前文档。
- 停靠菜单提供 Float、Float group、Dock、Auto hide/Pin、Hide/Close，不可用项禁用。左右自动隐藏标题整段顺时针旋转 90°，从上到下阅读，上下边栏横排；长度按字体测量，空间不足时优先压缩长标签。

各原生窗口独立维护表面、输入、焦点和捕获，由 App 统一 Poll。包含 Dock 的主窗和浮窗使用随主题切换的自绘标题栏，提供最小化、最大化/还原、关闭、拖动、双击和边缘缩放。普通非 Dock 窗口保留系统装饰。Windows 使用每窗口 DPI；X11 使用会话 DPI，并优先使用窗口管理器移动/缩放协议，无支持时由客户端处理。

浮窗是普通顶层窗口，可被主窗或其他应用遮挡；Windows 不设置原生 owner，X11 用窗口组关联代替 transient 关系。主窗关闭时 App 关闭该会话的浮窗，其他独立窗口继续运行。Dock 布局和 API 的浮窗 ID 仅在所属工作区内有效，调试快照使用 App 内唯一的窗口 ID。浮窗内容默认留 4 个逻辑像素边距，可用 `dock.floating_padding`（0–64）调整；主窗仍使用 `window_padding`。

## 布局持久化

先注册内容，再按稳定 ID 恢复布局。CaptureDefault 在业务初始布局配置完成后调用；未显式调用时首次 Poll 捕获，Reset 恢复默认布局。

C 的 `ui_dock_save` 用两次调用查询大小和填充缓冲区，长度包含结尾 NUL；restore 输入长度不包含 NUL。C++ 返回 string，C# 返回 string。文件路径、读写和保存时机由应用负责，库仅操作内存。

JSON `version=1` 保存主树/浮动树、比例、活动项、可见状态、隐藏项返回位置及浮窗逻辑坐标。树 axis 为 0（标签）、1（水平拆分）、2（垂直拆分），比例为 1–9999 / 10000。主题、标题、图标和业务内容不保存。

恢复先完整校验，再整体替换；不支持的版本、重复键/ID、非法树、能力冲突及损坏数据返回诊断并保留当前布局。已移除 ID 跳过，新注册项按默认布局加入。浮窗创建时将位置校正到可用屏幕范围。

```csharp
File.WriteAllText(path, dock.Save(), System.Text.Encoding.UTF8);
// 下次启动先注册相同 ID 的内容，再恢复。
dock.Restore(File.ReadAllText(path, System.Text.Encoding.UTF8));
```

## Fleck 页面

```xml
<Window xmlns="urn:sunfleck:v1" Width="1100" Height="760">
  <DockHost Id="workspace">
    <DockSplit Orientation="Horizontal" Ratio="25">
      <DockGroup>
        <DockTool Id="explorer" Title="Explorer" MinWidth="120">
          <TextBlock Text="Tool content"/>
        </DockTool>
      </DockGroup>
      <DockGroup>
        <DockDocument Id="editor" Title="Document" Icon="D">
          <TextArea Text="{Binding documentText, Mode=TwoWay}"/>
        </DockDocument>
      </DockGroup>
    </DockSplit>
  </DockHost>
</Window>
```

DockHost 包含一个 DockSplit 或 DockGroup；DockSplit 恰有两个子区域，Ratio 为第一项占比 1–99；DockGroup 内只能包含相同类型的 DockDocument 或 DockTool，Value 为选中索引。项目支持 Title、Icon、CanClose、CanFloat、CanAutoHide、MinWidth、MinHeight。

以上绑定需由应用提供 documentText 文本源。XML、生成 C、C++ 模型头和 v5 `.flkb` 使用同一模型，加载与 DataContext 见 [Fleck](fleck.md)。

## 主题与验证

Dock 默认继承全局主题。分组统一裁剪标题、标签与内容的圆角外框，Dock 根 TextArea 不叠加输入框边框；内容内部的编辑器保留普通控件样式。[主题与 Dock 皮肤](themes.md) 集中维护资源表、模板契约和绘制回调；[三个主题示例](../../themes/examples/README.md) 展示递进接入。

`dock` 覆盖模型、持久化、交互、主题、自动隐藏文字与像素；`dock_theme` 比较 XML、C、C++、二进制主题结果；`win32_dock*`、`x11_dock*` 验证多窗口交互。C# API 与展厅测试覆盖托管回调及跨入口一致性。运行方式见 [构建与测试](../../core/docs/building.md)，平台覆盖和人工验收见 [验证与复现](../../core/docs/validation.md)。
