# 通用布局属性

内置视觉节点、UserControl 和 Control 宿主共用这些属性。Fleck、代码接口、绑定和模板使用同一份运行时契约。

| 属性 | 值 | 默认值 |
|---|---|---|
| Width、Height | 0..10000000 的有限浮点逻辑像素，或 Auto | Auto |
| MinWidth、MinHeight | 0..10000000 的有限浮点逻辑像素 | 0 |
| MaxWidth、MaxHeight | 0..10000000 的有限浮点逻辑像素，或 Infinity | Infinity |
| Margin | 1、2 或 4 个逗号分隔有限浮点数，每项 -4096..4096 | 0 |
| HorizontalAlignment | Stretch、Left、Center、Right | Stretch |
| VerticalAlignment | Stretch、Top、Center、Bottom | Stretch |

Margin="8" 表示四边为 8；Margin="8,4" 表示左右 8、上下 4；Margin="1,2,3,4" 依次为左、上、右、下。负值会向对应方向延伸，但仍受父节点裁剪。所有值按 DPI 缩放，例如 200% DPI 下 Width="100" 占 200 个设备像素。

```xml
<Window xmlns="urn:sunfleck:v1" Width="480" Height="240">
  <StackPanel Padding="16" Spacing="8">
    <TextBlock Text="紧凑的一行" Height="24" Margin="0,4"/>
    <Overlay Height="100">
      <TextBlock Text="右下角" Width="120" Height="24"
                 MinWidth="80" MaxWidth="180" Margin="8"
                 HorizontalAlignment="Right" VerticalAlignment="Bottom"/>
    </Overlay>
    <Button Content="填满可用宽度" HorizontalAlignment="Stretch"/>
  </StackPanel>
</Window>
```

测量先得到显式尺寸或自动尺寸，再用 Min/Max 限制，最后加上 Margin；父布局的 Spacing 另计。排列先扣除 Margin，然后在可用区域内对齐。显式 Width/Height 优先于 Stretch，Min/Max 可以约束显式尺寸。Min 大于 Max 会报错；非法代码设置或绑定值不会修改当前有效属性。高度不足时裁剪文字，不缩小字体。

纵向 StackPanel 按子项所需高度排列，因此垂直对齐通常没有剩余空间可分配；Grid、Flex、Overlay 或显式高度的组件宿主可以提供较高的区域。Grid 按行列定义分配空间。Flex 使用 Width 或 MinWidth 作为基础宽度，再按 Weight 分配增量。未设置 HorizontalAlignment 的旧 Button 保持原有内容宽度；显式 Stretch 可使按钮填满可用宽度。

DockPanel 的自动尺寸按子项的停靠方向和顺序计算，最后一个子项填充剩余区域。例如两个左右排列、Height="30" 的子项，在 Padding="0" 时只需要 30 高；不会额外保留 100 像素。顶部/底部停靠项消耗高度，左侧/右侧停靠项共享剩余高度，Spacing、Margin 和 Padding 参与计算。外部父布局仍可拉伸 DockPanel，显式 Height 和 MinHeight 仍然有效。

DockDocument/DockTool 的 MinWidth/MinHeight 同时更新 Dock 布局约束。Dock 自身保留至少 32 个逻辑像素的面板下限；未设置时也保留既有 Dock 默认值。小数 MinWidth/MinHeight 在模型、刷新及 DPI 缩放中保留精度，到实际分割边界才向外取整。例如 64.25 在 150% DPI 下需要 97 个设备像素，不先取整到 65。旧 Dock 添加项目/Builder 便利接口的最小尺寸参数仍为整数；需要小数时使用内容节点的通用尺寸属性或 Fleck。通用 Max 和对齐约束的是内容视觉区域，不改变停靠分组的结构或分割比例。

Border 的厚度和 Padding 共同形成子内容区域，参见 [Border 容器](border.md)。

[Visibility 与命中控制](visibility.md) 决定节点是否占位、绘制或参与指针命中。

## 间距与内边距

容器的 Spacing/Padding 使用 0..64 的有限浮点逻辑像素。Padding 为四边相同的单值，默认 0；Spacing 默认使用主题间距，Fleck 可显式写 `Spacing="Theme"`，代码值为 -1。其他负数、NaN 和 Infinity 均被拒绝。两者与 Margin 一样按 DPI 缩放并保留小数，Weight 仍是整数。

C 的 `ui_node_set_layout(node, 1.25, 2.75)`、C++ `node.setLayout(1.25, 2.75)` 和 C# `node.SetLayout(1.25, 2.75)` 原子更新两个局部基础值：任一值非法时保留两个旧值及绑定；动画覆盖期间只更新基础值。单独设置一个属性不会改写另一个属性。C# 生成的 `SpacingProperty`/`PaddingProperty` 及读写属性使用 double，整数或 double 模型均可绑定。主题模板自身的 Padding 保留独立整数契约。

## 绑定与代码

这些属性支持 OneWay 和 OneTime，默认 OneWay；不支持 TwoWay 或 LostFocus。尺寸绑定可使用 Length、double 或整数，标量 Width/Height 的 -1 表示 Auto；Margin 绑定使用 Thickness，对齐绑定使用字符串。普通 Binding 使用当前数据上下文，TemplateBinding 使用所属控件属性：

```xml
<TextBlock Width="{Binding LabelWidth}"
           Margin="{Binding LabelMargin}"
           HorizontalAlignment="{Binding LabelAlignment}"/>
```

```xml
<Slider Width="{TemplateBinding Extent}" Margin="{TemplateBinding Inset}"/>
```

C 使用 ui_node_set_layout_value / ui_node_get_layout_value，属性标识为 UI_PROPERTY_WIDTH、UI_PROPERTY_MIN_WIDTH、UI_PROPERTY_MAX_WIDTH、UI_PROPERTY_MIN_HEIGHT、UI_PROPERTY_MAX_HEIGHT、UI_PROPERTY_MARGIN、UI_PROPERTY_HORIZONTAL_ALIGNMENT、UI_PROPERTY_VERTICAL_ALIGNMENT。尺寸使用 UI_VALUE_LENGTH，外边距使用 UI_VALUE_THICKNESS（geometry 指向左、上、右、下四个 double），对齐使用 UI_VALUE_STRING；返回几何与字符串借用至节点下一次布局属性修改。Height 也使用通用属性入口。旧 ui_node_set_height / ui_node_get_height 是整数便利接口（-1 或 0..4096）；读取小数 Height 返回 InvalidState 且不修改输出，避免静默截断。

C++ 的 Node 提供 setWidth/width、setMinWidth/minWidth 等尺寸方法、setMargin/margin 和 setHorizontalAlignment/horizontalAlignment、setVerticalAlignment/verticalAlignment。类型描述仍可用于 PropertyModel 绑定。C# 的 Node 和 FrameworkElement（包含自定义 UserControl/Control）提供同名属性，内置类型的 WidthProperty 等描述支持类型化 Bind。

## 编译资源与编辑器

生成 C/C++ 页面嵌入完整树资源，发布无需 XML。资源与运行时成套升级，版本检查及重建步骤见 [SDK 迁移](../../core/docs/sdk-migration.md)。

三个语言 SDK 的补全与 F12 来源相同，公共描述位于 VisualElement。Width/Height 提供 Auto 补全，对齐提供枚举值补全；未知属性、非法 Margin、非法对齐以及矛盾的 Min/Max 均有诊断。模板或页面布局修改可热刷新；失败保留原视觉树和状态。

验证覆盖实际布局坐标、负外边距、约束与对齐组合、200% DPI、绑定更新与解绑、模板属性绑定、四种加载方式以及旧资源读取。自定义 Panel 测量回调属于 M3 后续工作。属性值层与继承支持见 [统一属性系统](property-system.md)。

## TextBlock 的内容宽度

Width 是尺寸，Weight 是 Grid/Flex 分配剩余空间时使用的整数权重；Weight 不接受 Auto，在 DockPanel 中不决定宽度。

TextBlock 在 DockPanel 的左右停靠位置、WrapPanel 或非 Stretch 的水平对齐下，Auto 宽度按文字内容测量，不再回退到固定 100，也不添加隐式左右留白。若需要留白请使用 Margin。Width="60" 固定宽度，Width="Auto" 恢复内容测量，文字变化会重新测量。在 StackPanel/Overlay 等容器的 Stretch 槽位中，Auto 仍可被父布局拉伸；DockPanel 最后一个子项仍填充剩余区域。

运行时核对方法见 [属性审计](../../core/docs/validation.md#选择测试)。检查区分默认值、赋值结果、实际尺寸和父布局相关的效果。


测量与排列缓存按节点生效：纯外观变化复用几何，尺寸变化使祖先测量失效，槽位变化重新排列但可复用未变化兄弟的测量。DPI、主题指标、字体注册和子节点重排均参与缓存依赖；检查器 `layoutWork` 可观察实际执行次数。Width/Height 及 Min/Max 公开尺寸已使用 Length，支持小数；Width/Height 接受 Auto，MaxWidth/MaxHeight 接受 Infinity。普通容器的自动高度测量显式传播无限宽度，有限 MaxWidth 仍约束换行；Grid/Flex 的无界行按内容宽度测量，不将无限空间分给权重轨道。Windows 文字自然宽度不再用一百万像素占位，显式换行仍保留。自定义 Panel 可通过 `ui_panel_callbacks` 提供测量和排列回调。测量、普通容器排列、Margin 与 Windows TextBlock 排版已保留小数，绘制与当前设备像素命中边界最后取整；例如 150% DPI 下 100 个 Height="1" 的连续元素总高为 150，而不是逐项取整后的 200。滚动容器的内容范围、当前位置和滑块拖动保留小数，不再向上取整最大偏移；在 100%/150%/200% DPI 下，滚动到底时内容边缘与视口对齐。DocumentView 的既有公开滚动接口仍以整数设备像素返回，在读取边界四舍五入。Dock 装饰和部分编辑控件的像素几何仍待后续迁移。

## 类型化小数外边距

```cpp
node.setMargin({0.25, -0.5, 0.75, 1.25});
ui_thickness margin = node.margin();
```

```csharp
label.Margin = new Thickness(0.25, -0.5, 0.75, 1.25);
```

Fleck 仍使用 `Margin="0.25,-0.5,0.75,1.25"`；绑定源和 TemplateBinding 的组件属性改为 Thickness，不再传字符串。NaN、Infinity 或超出范围的分量被拒绝，原有效值保持不变。动画值层覆盖 Margin 时，局部/绑定更新只修改基础值；清除动画层恢复当前基础值。时序与关键帧见 [属性动画](animation.md)。

运行示例：[fractional-layout.fleck](../snippets/fleck/fractional-layout.fleck)。

## PreferredWidth 迁移

API v3 删除 PreferredWidth，尺寸只有一条属性通路。它曾同时充当自动测量提示、Flex 基础宽度及 DockHost 默认高度，行为不适合继续扩展。按用途迁移：

| 原用法 | 替代写法 | 行为 |
|---|---|---|
| 固定宽度，通常配 Weight="0" | Width="120.5" | 固定轨道宽度，仍受 Min/Max 约束 |
| Flex 基础宽度并继续增长 | Width="Auto" MinWidth="120.5" Weight="1" | 从最小宽度分配额外空间 |
| PreferredWidth="0" | 删除属性或 Width="Auto" | 恢复父布局的自动尺寸规则；Width="0" 则明确为零宽 |
| DockHost 的旧高度提示 | Height="480" | 高度独立于 Width |
| 同时设置 PreferredWidth 与 Width | 删除 PreferredWidth | 保留已有显式 Width |

`MinWidth` 是硬下限；它不保证与旧的可压缩宽度提示在拥挤容器内完全等价。`MaxWidth` 只限制实际宽度，不参与额外空间的重新分配；Grid 中独立的轨道尺寸由行列定义控制。

旧 Fleck 属性会报错，失败刷新保留当前页面，不提供静默别名。C 的 `ui_node_set_track`、C# 的 `SetTrack` 已删除：用通用 Width/MinWidth 属性加 `ui_node_set_weight` / `SetWeight`；C++ 使用 `setWidth` / `setMinWidth`。尺寸绑定可继续接受整数或 double 模型源并规范化为 Length。C# 尺寸属性类型为 `Length`，数字隐式转换为 Pixels，自动与无限值请写 `Length.Auto`、`Length.Infinity`。


## 二维 Grid

每个 Grid 默认有一个星号行和一个星号列。子节点默认位于第 0 行、第 0 列。多个子节点可以共享一个单元格，按声明顺序绘制。API v5 / 编译资源 v16 起，原有单行页面需要显式列定义与列号，或者改用 Flex。

```xml
<Grid RowDefinitions="Auto,*" ColumnDefinitions="90,2*,*" Spacing="6">
  <TextBlock Text="项目"/>
  <TextBox Grid.Column="1" Grid.ColumnSpan="2"/>
  <Border Grid.Row="1" Grid.ColumnSpan="3" Background="#EEF2F6"/>
</Grid>
```

固定轨道使用逻辑像素，Auto 轨道按内容测量，星号轨道按权重分配剩余空间。文本使用跨度内的实际宽度测量换行高度。每轴支持 1 到 256 个显式轨道，超出已声明范围的位置落在最后一个轨道，跨度截断到边界。

C 使用 `ui_grid_set_tracks` 和 `ui_node_set_grid_cell`，C++ 使用 `setGridTracks` 和 `setGridCell`，C# 使用 `SetGridTracks` 和 `SetGridCell`。两个轴的定义一起生效，参数非法时保留原来的行与列。

## 方向、层叠与绝对定位

StackPanel 的默认方向为 Vertical，WrapPanel 和 Separator 的默认方向为 Horizontal。水平 StackPanel 按子节点期望宽度从左至右排列。垂直 WrapPanel 从上至下排列，高度不足时另起一列。用 Spacing 控制相邻项目及行列的间距。

Canvas 按子节点的 Canvas.Left 和 Canvas.Top 相对于内边距定位，单位为逻辑像素，默认值均为 0。坐标支持负数及小数。Canvas 的内容可以重叠，子节点的位置与大小不增加 Canvas 的期望尺寸。C、C++ 和 C# 可分别通过 ui_node_set_canvas_position、setCanvasPosition 和 SetCanvasPosition 一次更新两个坐标。

```xml
<Canvas Height="80">
  <Border Canvas.Left="8" Canvas.Top="12" Width="64" Height="32" Background="#4668AA"/>
  <Button Canvas.Left="24.5" Canvas.Top="20" Panel.ZIndex="1" Content="编辑"/>
</Canvas>
```

Panel.ZIndex 控制同一父节点下的绘制与命中顺序。值较高的节点在上，相同值按声明顺序绘制，最后绘制的可交互节点优先接收命中。改变层级会保留布局槽位及逻辑子节点顺序。
ClipToBounds 是可绑定的布尔属性，默认为 true，延续已有页面的裁剪行为。布局容器设为 false 后，子节点可在祖先允许的区域内越界绘制与命中。滚动视口始终裁剪其内容。

ContentControl、ContentPresenter 和 Button 接受一个视觉子节点。图文按钮可将水平 StackPanel 作为这个子节点。没有视觉内容时使用 Content 文本。组件定义内的 ContentPresenter 可以呈现调用方提供的单个子节点，缺少呈现器或有多个候选时加载报错。调用方内容保持自己的名称作用域，通过组件宿主继承 DataContext 和字体。

通过 ui_node_get_parent、ui_node_get_child_count、ui_node_get_child 可以选择 UI_LOGICAL_TREE 或 UI_VISUAL_TREE 查询父子关系。组件的逻辑子节点是调用方内容，视觉子节点是模板根。投影内容的视觉父节点是 ContentPresenter。
## 共享轨道、滚动与自定义 Panel

同一布局根内可使用 ColumnDefinitions="Auto@Label,*" 为首列指定共享尺寸组。RowDefinitions 使用相同语法，行与列分别分组。组名以字母或下划线起始。运行时调用 setSharedSizeGroup 或 SetSharedSizeGroup，传入空字符串移除成员。不同 DPI 下的轨道使用相同的逻辑尺寸。

ScrollView 的 HorizontalScrollBarVisibility 默认 Disabled，VerticalScrollBarVisibility 默认 Auto。Hidden 允许滚动并隐藏滚动条，Visible 始终预留滚动条位置。ScrollTo 使用逻辑像素。BringIntoView 调整目标所在的各层 ScrollView，使目标进入视口。

Fleck 页面可以声明一个带 Id 的 Panel，代码取得节点后注册测量与排列回调。回调收到参与布局的子节点所需尺寸，单位为逻辑像素。测量返回期望宽高，排列按子节点声明顺序填写相对于内容区域的矩形。C 使用 ui_panel_set_callbacks，C++ 使用 setPanelCallbacks，C# 使用 SetPanelCallbacks。回调在 UI 线程同步调用，运行时在回调期间拒绝重入修改，替换或移除注册时释放上下文。
Viewbox 接受一个视觉子节点，保持宽高比在可用空间内居中缩放。表单、图文按钮和 Viewbox 的组合示例见 [shared-layout.fleck](../snippets/fleck/shared-layout.fleck)。
