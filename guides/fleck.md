# Fleck 页面与绑定

Fleck 使用 XML 声明节点和绑定，业务逻辑由 C、C++ 或 C# 提供。下面介绍页面；独立 Theme 文档及其编译方式见 [主题与 Dock 皮肤](themes.md)。

组件清单、常用属性、子节点规则和可运行的预览页面见 [Fleck 组件与效果预览](fleck-components.md)。

## 页面格式

页面以 `urn:sunfleck:v1` 命名空间的 Window 为根，必须在打开窗口前加载：

```xml
<Window xmlns="urn:sunfleck:v1" Title="Example" Width="400" Height="240">
  <StackPanel>
    <TextBlock Id="count" Text="Count: 0"/>
    <Button Id="increment" Content="Increment"/>
  </StackPanel>
</Window>
```

解析器接受 UTF-8、XML 1.0 声明、注释、预定义实体和数字字符引用。拒绝 DTD、外部实体和网络加载，错误诊断包含错误码和位置。[最小测试页面](../../core/tests/fixtures/welcome.fleck)

页面、组件和模板使用统一资源保存完整树信息。编译器、资源和运行时必须成套部署，版本规则见 [SDK 迁移](../../core/docs/sdk-migration.md)。

内置属性契约由 `fleckc --schema` 导出，核对方法见 [属性审计](../../core/docs/validation.md#选择测试)。通用 Width/Height、Min/Max、Margin 与对齐的语义、绑定和代码接口见 [通用布局属性](layout-properties.md)。这份清单覆盖现有 Sunfleck 功能，不代表 WPF 属性全集。Anchor 仅支持 Label 和弹出层。

## 加载与编译

| 方式 | 产物 | 入口 |
|---|---|---|
| XML 动态加载或预览 | `.fleck` | `ui_app_load_fleck` / `fleck_preview` |
| 生成 C 节点代码（默认） | `.c` | 生成的页面函数 |
| 二进制节点资源 | `.flkb` | `ui_app_load_fleck_resource` |
| C++ 模型绑定 | `.c` 加 `.hpp` | 生成的类型化加载函数 |

构建 `fleckc` 后，从 `out/debug` 执行：

```powershell
.\fleckc.exe --symbol load_counter ../../examples/simple/cpp/fleck/counter.fleck counter_page.c
.\fleckc.exe --format cpp --symbol load_counter ../../examples/simple/cpp/fleck/counter.fleck counter_bindings.hpp
.\fleckc.exe --format binary ../../examples/simple/cpp/fleck/counter.fleck counter.flkb
```

Linux 将 `.\fleckc.exe` 换为 `./fleckc`。示例 CMake 已接入这些生成步骤，无需手动执行。C++ 模型绑定的两个生成命令必须使用相同 `--symbol`；省略时入口名为 `sunfleck_load_page`。旧编译资源须用当前 fleckc 重新生成，加载器拒绝不匹配版本。

C 代码和二进制资源都保留节点、属性、ID、锚点与绑定名称，运行时不需要原始 XML；使用资源加载的应用仍需分发 `.flkb`。

初次加载与开窗后更新使用不同接口。Windows、X11 和 Headless 的事务刷新、同一 App 的独立窗口、状态迁移和句柄失效规则见 [开发工具接入](devtools.md#fleck-刷新与原生重建)。

## 属性绑定与模型

绑定写在目标属性上，每个组件可同时绑定多个属性：

```xml
<Window xmlns="urn:sunfleck:v1" Model="MainModel">
  <StackPanel>
    <TextBlock Text="{Binding count, StringFormat='Count: {0}'}"/>
    <Button Content="{Binding buttonText}"
            IsEnabled="{Binding canIncrement}"
            Command="{Binding increment}" CommandMode="Async"/>
  </StackPanel>
</Window>
```

支持 Path、Mode、UpdateSourceTrigger、StringFormat。展示属性默认 OneWay，编辑器 Text、可交互 Value 默认 TwoWay。命令默认 Sync；C、C++、C# 均可绑定异步命令并声明 CommandMode="Async"。异步方法操作原模型，通过 UI 投递更新属性并通知；同一命令实例默认 SingleFlight，已完成的修改不会因后续异常自动回滚。框架不自动替换按钮文案。

C 使用 `ui_model` 注册 getter/setter 后显式通知；C++ 通过 concept 检测属性通知契约，生成 getter/setter 适配；C# 直接接入 `INotifyPropertyChanged`。原生 XML、生成代码和二进制加载均使用同一绑定引擎。详细语法、属性范围、线程与生命周期见 [属性绑定与变更通知](bindings.md)。

原生显式模型入口为 `ui_app_load_fleck_with_model`、`ui_app_load_fleck_resource_with_model` 和生成的 `<symbol>_with_model`。对于已持有 TextSource/Command 的应用，也可通过 `ui_fleck_data_context` / C++ DataContext / C# FleckDataContext 显式导出；加载器将其适配为属性模型，后续源变化自动通知。该方式仍使用相同的 `{Binding ...}` 语法，不进行应用级名称回退。

C++ 构建期模型绑定使用全局命名空间中的模型类型；Path 对应公开字段或无参数 getter，getter 属性可通过同名的单参数 setter 回写。命令方法按 CommandMode 自动适配。先包含模型定义及生成头，再加载：

```cpp
MainModel model;
auto page = sunfleck::generated::load_counter::load(app, model);
app.openWindow();
page->run();
```

普通模型可调用 `page.refresh()`；满足通知契约时自动订阅，页面析构自动退订。生成器也支持 getter 返回 TextSource 引用作为显式源适配。异步异常可通过 `takeError()` 读取。旧的 TextBinding、独立 BindingMode/TextFormat 和裸命令名写法已移除。

[计数器模型](../../samples/counter/cpp/fleck/main_model.hpp) · [C 函数式接入](../../samples/counter/c/fleck/main.c) · [C# 属性通知示例](../../samples/counter/csharp/fleck/Program.cs)

## Dock 页面

DockHost、DockSplit、DockGroup、DockDocument、DockTool 使用同一套原生 Dock 模型。Orientation/Ratio、Value、Title/Icon、能力与最小尺寸都会保留到生成代码及二进制资源。[Dock 语法与生命周期](docking.md#fleck-页面) · [Dock 回归页面](../../core/tests/fixtures/dock/workspace.fleck)

页面和主题使用不同的根、二进制标记及加载 API，不能混用；主题编译输出的 `.flkb` 应交给主题加载 API。

## 跨文件资源字典

`<ResourceDictionary Source="themes/base.fleck"/>` 可用作元素资源字典，也可放在 `ResourceDictionary.MergedDictionaries` 中。外部文件以带 Fleck 命名空间的 `ResourceDictionary` 为根。其内部引用的文件先按当前字典所在目录查找。字典可导出具名值、显式样式和隐式样式。

运行时通过 `ui_app_register_fleck_resources` 或对应语言包装提供资源名称及 UTF-8 内容。一个批次内可以先列引用方再列依赖项。使用 fleckc 时，通过 `--resource` 或 `--resource-directory` 提供字典文件，生成的页面携带解析后的样式和值。

## 视觉状态

在节点的 `<Button.VisualStateGroups>` 等属性元素中声明具名 `VisualStateGroup`。每个 `VisualState` 可包含一个作用于该节点的 Storyboard。需要保持终值时，在 Timeline 上设置 `HoldEnd="true"`。切换到同组的空状态会释放该组仍然占用的动画属性，恢复当前基础值。

按钮的 `CommonStates` 支持 `Normal`、`MouseOver`、`Pressed` 和 `Disabled`，按指针和可用状态选取，缺省状态回退到已声明的 `Normal`。业务代码可通过 C 的 `ui_node_go_to_visual_state`、C++ 的 `goToVisualState` 或 C# 的 `GoToVisualState` 按组名和状态名切换。[按钮按下过渡示例](../snippets/fleck/animation.fleck)

## 激活事件与路由命令

鼠标或键盘激活按钮时，Preview 处理器从逻辑树根依次运行到按钮，随后运行按钮上的 Direct 处理器，再从按钮向根执行 Bubble 处理器。三个阶段均可返回已处理，终止本次激活并阻止默认命令。C、C++、C# 分别使用 `ui_node_set_activation_handler`、`setActivationHandler` 和 `SetActivationHandler` 注册或清除指定阶段的回调。

`<Button Command="{RoutedCommand Save}"/>` 激活时从按钮开始，沿逻辑祖先查找名为 `Save` 的命令绑定。可用 C 的 `ui_node_set_command_binding`、C++ 的 `setCommandBinding` 或 C# 的 `SetCommandBinding` 在容器上注册命令。异步命令沿用原有的并发与取消机制。

`<StackPanel.InputBindings><PointerBinding Action="Down" Phase="Preview" Command="Select"/></StackPanel.InputBindings>` 在该容器的预览阶段查找并执行路由命令。Action 可选 Down、Move、Up、Cancel。Win32 取消操作或失去捕获、宿主调用 UI_POINTER_CANCEL 时，Cancel 路由到仍在拖动或按下的控件，处理后释放拖动状态。Phase 可选 Preview、Direct、Bubble，默认 Bubble。成功匹配的命令会处理该事件。代码也可用 `ui_node_set_pointer_handler`、`setPointerHandler` 或 `SetPointerHandler` 注册回调，获得源节点、当前处理节点及窗口客户区像素坐标。回调返回真会停止传播，处理按下或松开可阻止默认点击。

`<StackPanel.InputBindings><KeyBinding Gesture="Ctrl+S" Command="Save"/></StackPanel.InputBindings>` 将快捷键关联到同名路由命令。焦点是快捷键事件源，缺少焦点时以页面根为源。可选的 `Phase="Preview"` 从根向事件源查找，`Direct` 仅检查事件源，默认的 `Bubble` 从事件源向祖先查找。先匹配的快捷键取得处理权，同一事件至多执行一次命令。C 可用 `ui_node_set_key_binding` 指定阶段，C++ 和 C# 可用带阶段参数的 `setInputBinding` 和 `SetInputBinding` 重载。F1 至 F24 可直接绑定，也可组合 Ctrl、Alt、Shift，例如 F5 或 Shift+F10。字母 A 至 Z 和数字 0 至 9 需要至少一个 Ctrl 或 Alt 修饰键。C 和 C++ 可使用 UI_KEY_F5 等常量，C# 可传入 FunctionKey.F5。事务刷新复用页面上的命名节点时，代码注册的快捷键、路由命令源和命令处理器继续工作。Fleck 中增删的快捷键按新页面生效，同一按键以代码注册为先。Windows 使用键盘消息，Headless 测试可调用 `ui_host_key_gesture` 并检查 `handled`。

在容器的 `StackPanel.CommandBindings` 中声明 `<CommandBinding Command="Save" Executed="{Binding SaveCommand}"/>`，按钮即可使用该容器当前 DataContext 上的命令。异步按钮设置 `CommandMode="Async"`。模型在异步命令执行期间销毁会取消关联的工作。

## 跨文件控件复用

页面可用命名空间标签引用独立 `UserControl`，也可使用代码声明的 `Control` 与独立 `ControlTemplate`。每次实例化都有独立名称作用域和数据上下文。动态加载前注册资源与行为类型，构建加载由生成的注册入口完成。完整定义、绑定语法及 C/C++/C# 例子见 [可复用组件](fleck-components.md#可复用组件) 和 [reusable 示例](../../core/tests/fixtures/reusable/README.md)。

## 键盘导航与焦点

`TabIndex` 接受 0 至 2147483647 的整数，默认值为 2147483647。Tab 按索引从小到大遍历当前窗口的可交互节点，同一索引保留原有遍历顺序。Shift+Tab 反向遍历，两端可循环。`IsTabStop="false"` 让 Tab 跳过该节点，鼠标仍可激活它。两个属性均支持绑定、样式 Setter 和清除局部值。

方向键在普通控件间按当前可见位置移动焦点。编辑器、滑块及列表沿用各自的键盘操作。

在容器上设置 `IsFocusScope="true"` 后，键盘或鼠标取得焦点的控件会记录在最近的逻辑作用域中。内外作用域分别保存记录。C 使用 `ui_node_get_logical_focus` 查询，使用 `ui_node_focus` 聚焦控件或恢复作用域内的焦点。C++ 对应 `logicalFocus()/focus()`，C# 对应 `LogicalFocus/Focus()`。目标须已参与窗口布局。目标被隐藏、禁用或移除后，查询返回空。
