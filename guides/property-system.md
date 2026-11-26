# 属性值、优先级与统一访问

内建统一访问覆盖当前 `ui_property` 的属性标识（包含 Text/Content 别名），包括 Spacing、Padding、Weight 和 DataContext；自定义组件使用同一拥有型值存储和优先级容器，并提供按名称查询的描述、有效值、基础值、来源和清除接口。

已经实现类型化值、属性覆盖层、统一节点/组件 DataContext、声明 Inherits 的自定义属性传播，以及浮点测量/排列和按依赖失效的布局缓存。Fleck 文档根和元素的 Resources 支持内建类型的静态样式。无 Key 的 Style 按 TargetType 匹配，带 Key 的样式通过 `Style="{StaticResource key}"` 选择。BasedOn 引用声明作用域内同一目标类型的样式。Resources 可以直接列出条目或包一层 ResourceDictionary。属性和 Setter 可以通过 StaticResource 引用先前声明的 Resource 字面值。内联合并字典写在 ResourceDictionary.MergedDictionaries 中，后列出的字典覆盖先前字典的同名键，当前字典的直接条目优先。局部资源对元素自身及后代生效，兄弟分支使用各自的作用域。Setter 写入 Style 层，清除局部值后即可读回。动画用法见[动画](animation.md)。

元素属性和样式 Setter 均可使用 `{DynamicResource Accent}`。当前支持字体、文本外观、Border 外观、变换及 Opacity。运行时依次查询元素、逻辑祖先、应用和主题。直接设置或清除局部属性会解除该层的动态引用。通过 C 的 `ui_node_set_resource`、C++ 的 `Node::setResource` 或 C# 的 `Node.SetResource` 可覆盖元素资源，清除覆盖后恢复该处 Fleck 声明或下一层资源。C 使用 `ui_app_set_resource`、`ui_app_set_theme_resource`，传入空值指针可删除条目。C++ 使用 `App::setResource`、`clearResource` 及对应的主题方法。C# 可调用 `app.SetThemeResource("Fade", 0.75)` 和 `app.SetResource("Fade", 0.5)`。删除应用覆盖会显露当前主题值，删除最后一个来源会清除对应样式值。

控件可在 Fleck 中声明 `ValidationStates` 状态组。某个已连接的绑定在读取、刷新或回写时失败后，控件根据输入焦点进入 `InvalidFocused` 或 `InvalidUnfocused`。全部绑定恢复后进入 `Valid`。例如，模型拒绝 TextBox 的双向回写时，可以用状态 Storyboard 改变边框颜色，错误细节仍可通过绑定诊断查询。

Style 可以通过 Trigger 或 MultiTrigger 的 Condition 比较控件的 IsEnabled、Value、Visibility、IsHitTestVisible、ClipToBounds、IsTabStop、TabIndex 或 IsFocusScope。多个条件全部匹配时，Setter 的外观值进入 Trigger 层。声明顺序靠后的匹配项优先，退出条件后恢复当前 Style 或资源值。父节点禁用会改变后代的 IsEnabled 条件。DataTrigger 的 Binding 单向读取 DataContext 导出的属性，按该属性类型解析比较值。模型通知和上下文替换会重新计算条件，显式空上下文会停用该条件。[按钮样式示例](../snippets/fleck/styles.fleck) 展示了同一基础样式中的高度、颜色和禁用透明度。

## 描述与值

`ui_property_describe(kind, property, descriptor)` 返回名称、类型、按控件确定的默认值、可绑定/双向标志及测量/排列/绘制/输入依赖。调用方先设置 `struct_size=sizeof(ui_property_descriptor)` 和 `version=1`；错误的版本、大小或控件/属性组合返回 `UI_ERROR_INVALID_ARGUMENT`，不改写输出。

`ui_node_set_property` / `ui_node_get_property` 与已有 setter/getter 使用相同节点状态和验证。Text/Content 是字符串，Value/Weight 是整数，尺寸为 Length，Spacing/Padding 为 double，IsEnabled 是布尔，Command 是所属 App 的借用句柄。绑定仍在绑定层执行原有文本格式化及数值适配；通用 setter 不隐式转换错误类型。

`ui_node_clear_property` 移除该槽位的局部值及绑定，并显露较低优先级的值，没有其他层时恢复元数据默认值。它与 `ui_node_clear_binding` 不同：后者保留当前值作为局部快照。清除单个属性仍须满足 Min/Max 等现有约束；失败不会移除旧绑定或修改值。HorizontalAlignment 清除时也恢复旧 Button 的默认内容宽度行为。

`ui_node_get_property_source` 返回 Default、Inherited、Theme、Style、Trigger、Template、Local、Binding 或 Animation。显式设置默认值仍是 Local；OneTime 仍标记为 Binding，直到显式解绑或赋值。Text/Content 共享同一槽位。普通文本源也显示为 Binding。

IsEnabled 的读取结果包含祖先禁用约束，来源则描述节点自身的值槽位。检查器同时给出局部值和 `effectiveValue`，避免把祖先造成的禁用误判为本节点的局部设置。

字符串返回值为借用数据，应在下次读取/修改该节点前复制；C# 自动复制。所有节点访问保留 UI 线程、所属 App 及节点生命周期约束。描述查询不依赖 App。

## C / C++ / C#

```c
ui_binding_value width = {0};
width.kind = UI_VALUE_LENGTH;
width.integer = UI_LENGTH_PIXELS;
width.number = 240.5;
ui_node_set_property(node, UI_PROPERTY_WIDTH, &width);
ui_property_source source;
ui_node_get_property_source(node, UI_PROPERTY_WIDTH, &source);
ui_node_clear_property(node, UI_PROPERTY_WIDTH);
```

C++ Node 提供 `setPropertyValue`、`propertyValue`、`propertySource`、`clearProperty`，保留 C 的借用字符串契约和原有类型化尺寸方法。

```csharp
var metadata = BuiltInPropertyDescriptor.Describe(NodeKind.TextBlock, PropertyTarget.Width);
node.SetProperty(PropertyTarget.Width, 240);
var source = node.GetPropertySource(PropertyTarget.Width);
node.ClearProperty(PropertyTarget.Width);
```

C# `GetProperty` 的整数结果为 `long`，字符串为托管副本，Command 使用已注册的托管对象；来自外部原生代码且没有托管包装的非空命令会明确抛出异常。类型化 FrameworkElement 提供 `ClearValue(property)` 与 `GetValueSource(property)`。

Spacing/Padding 仅适用于容器；Weight 适用于原有视觉节点。它们支持 OneWay/OneTime 绑定和显式动画覆盖层。Spacing 的 API 默认值为 -1（使用主题间距），Padding 默认 0，Weight 默认 1。清除局部值恢复对应默认或较低层，Overlay 等容器仍保留原有布局语义。

```xml
<StackPanel Spacing="{Binding Gap}" Padding="{Binding Insets}"
            MinWidth="{Binding Preferred}" Weight="{Binding Allocation}">
  <TextBlock Text="Layout metrics from the model"/>
</StackPanel>
```

Spacing/Padding 模型路径导出 double 或整数，Weight 导出整数，MinWidth 可导出 Length、double 或整数。C/C++/C# 的属性标识分别为 `UI_PROPERTY_SPACING`、同名 C++ 枚举及 `PropertyTarget.Spacing` 等。生成的三语言元素描述同步标记为可绑定，C# 元素还提供类型化 `Spacing/Padding/Weight` 读写属性。`ui_node_set_layout` 会先验证并准备两个值，再一起提交；失败保留两个原值及绑定。ui_node_set_weight 单独设置 Weight；旧 SetTrack 已删除。成对设置只写 Local 层，活动动画仍覆盖新的基础值。

## Fleck、资源与检查器

现有属性的可绑定性与双向标志由原生描述生成到 Fleck schema。字体属性见 [字体与文本](fonts.md)；关键帧与播放控制见 [属性动画](animation.md)。

完整树资源保留 `Width="Auto"`、`IsEnabled="true"` 或空 Text 等显式默认值声明。XML 与新编译资源的来源信息一致。不兼容版本及无握手 Builder 被明确拒绝，重建方式见 [SDK 迁移](../../core/docs/sdk-migration.md)。

调试快照的内建属性新增 `valueSource`，IsEnabled 新增 `effectiveValue`；VS Code 属性提示展示这些信息。真实 IDE 宿主验收与源码/自动化测试分别记录。

## 验证

- `property_system`：默认/局部/绑定来源、别名、清除与订阅解除、非法更新原子性、类型/版本校验、跨线程访问、XML/资源一致性。
- `element_types_c`：真实 C11 调用描述、清除、读取及来源接口。
- `csharp_api` 中 `PropertySystemTests`：托管 ABI 布局、字符串、绑定、类型化控件、快照和 Unicode。
- `utf8_text`：UTF-8 与平台 wchar_t 的往返，UTF-16 代理对、最高码点、嵌入 NUL 和非法序列。

## 类型化值与字面量

`ui_binding_value` 新增 Color、Thickness、CornerRadius、Point、Size、Matrix、Length 和 Enum。模型、组件默认值和分层赋值都同步复制字符串与几何数据；复制或移动内部存储后不会保留临时对象的指针。非有限数字、负 Size/CornerRadius、无效 Length 单位、非法颜色和错误坐标数量被拒绝，保留原值。

| 类型 | C ABI 表示 | Fleck 字面量 |
|---|---|---|
| Color | `integer` 中的 32 位 ARGB | `#RRGGBB`、`#AARRGGBB` |
| Thickness | `geometry` 指向 left/top/right/bottom 四个 double | `4`、`4,8`、`1,2,3,4` |
| CornerRadius | top-left/top-right/bottom-right/bottom-left 四个 double | `4`、`1,2,3,4` |
| Point / Size | 两个 double | `2.5,-4` / `120.5,30` |
| Matrix | m11/m12/m21/m22/offset-x/offset-y 六个 double | `1,0,0,1,12.5,0` |
| Length | `number` 加 `integer` 中的单位枚举 | `Auto`、`Infinity`、`24.5` |
| Enum | `integer` | 枚举的十进制数值 |

这些类型目前可用于组件和模型；内建 Margin 已迁移为 Thickness；Width/Height 和 Min/Max 已使用 Length，尺寸语义见 [布局属性](layout-properties.md)。几何返回值也是借用数据，需在对应组件或属性修改/销毁前复制。

C 的 `ui_value_parse` 将几何写入调用方提供的 double 数组，失败不修改输出或数组。C++ 提供 `PropertyModel::parse<T>`、`encode`、`decode`，支持 `ui_color/ui_thickness/ui_point/...` 和枚举。C# 提供同名值类型和 `PropertyValues.Parse<T>`；模型和组件在原生调用期间持有分配的几何缓冲区。

C++ 元数据导出器可序列化类型化默认值。C# 声明可使用 `[FleckProperty(Default = "2.5,4")] public Point Offset ...`，生成器导出类型及默认字面量；编辑器使用同一生成器发现类型并保留 F12 源位置。原生组件 manifest 中几何、颜色、长度与枚举的默认值使用字符串。

## 属性优先级

基础值按 Local/Binding、Template、Trigger、Style、Theme、Inherited、Default 解析；Animation 覆盖基础值。`ui_node_get_base_property` 跳过 Animation。更低优先级的更新也要验证，错误更新不破坏现有层；相同有效值不会再次重置文本光标和选区。

```csharp
node.SetPropertyLayer(PropertyTarget.Width, ValuePrecedence.Style, 100);
node.SetPropertyLayer(PropertyTarget.Width, ValuePrecedence.Animation, 150);
// GetProperty 返回 Length(150)，GetBaseProperty 返回 Length(100)。
node.ClearPropertyLayer(PropertyTarget.Width, ValuePrecedence.Animation);
// 恢复当前基础值；若期间绑定变化，则使用最新绑定值。
```

Animation 层只接受元数据标记为 Animatable 的属性。Timeline 和 Storyboard 按时钟采样并将呈现值写入该层，支持的类型和播放控制见[属性动画](animation.md)。DataContext 与声明 Inherits 的自定义属性沿树自动传播；其他未声明 Inherits 的属性不会自动继承。

自定义组件提供 `ui_component_describe_property`、`ui_component_get_base_property`、`ui_component_get_property_source`、`ui_component_set_property_layer` 和 `ui_component_clear_property_layer`。C++ 对应 `describeProperty/baseValue/valueSource/setLayer/clearLayer`；C# 对应 `DescribeProperty/GetBaseValue/GetValueSource/SetLayer/ClearLayer`。组件可显式声明下述元数据；未声明时沿用按值类型推断可动画性及原有绑定行为。

组件绑定源在 Animation 覆盖期间继续更新基础值；移除覆盖层后恢复最新基础值。组件的既有 `Set` 保留 TwoWay 写回语义，覆盖其他层不写绑定源。清除组件 Local 层解除相应绑定。页面刷新保留适用的基础层并丢弃旧 Animation 层。命令销毁会清理所有已存储层中的引用，防止清除较高层后恢复已销毁对象。

检查器同时展示组件属性来源和类型化几何值。原生 schema 输出 `animatable` 与 `propertyFlags`。

## 节点与组件 DataContext

C 提供 `ui_node_set_data_context`、`ui_node_clear_data_context`、`ui_node_get_data_context`、`ui_node_get_data_context_source` 和 `ui_node_bind_context_property`。C++ 对应 `Node::setDataContext/clearDataContext/dataContext/dataContextSource/bindContext`；C# 对应 `Node.DataContext`、`DataContextSource`、`ClearDataContext()` 和 `BindContext(...)`。

```csharp
var panel = app.CreateNode(NodeKind.StackPanel);
var label = app.CreateNode(NodeKind.TextBlock);
panel.Append(label);
label.BindContext(PropertyTarget.Text, "Caption", PropertyBindingMode.OneWay);
using var model = app.CreatePropertyModel().Add("Caption", () => "Hello");
panel.DataContext = model; // 激活 label 的继承绑定。
label.DataContext = null; // 显式空值截断继承，并移除该绑定提供的局部值。
label.ClearDataContext(); // 恢复继承并读取当前模型值。
```

未设置的普通节点、组件宿主和模板内部节点继承父节点；显式空值仍是 Local。模型销毁时局部引用变为空，依赖该上下文的绑定值恢复较低层或默认值，声明保留为休眠状态；不保留旧文本快照或悬空句柄。OneTime 也适用。已有样式和动画覆盖不被移除。绑定可在上下文为空时声明，随后设置上下文或挂接到有上下文的父节点时激活。显式 `ui_node_bind_property` / `PropertyModel.Bind` 始终使用指定模型，不随 DataContext 改变。

切换前先读取并验证所有受影响的普通节点绑定，缺失路径、类型错误、非法目标值、只读 TwoWay 等不会先发布部分子树值。绑定声明在空上下文期间保留；重新激活时订阅新模型，旧模型通知不再修改节点。Fleck `{Binding ...}` 在普通节点、组件属性和模板内部均接入此通路，XML 与编译资源使用相同实现。Self/TemplatedParent 仍明确指向所属组件属性模型。检查器显示有效模型、来源及 `usesDataContext`。

页面成功刷新按作用域、Id 和兼容类型保留运行时代码设置的普通节点上下文，包括显式空值和清除操作；未命名根节点按兼容根节点身份保留。页面提供的默认模型仍可更新，不会误当成运行时覆盖迁移。刷新失败不发布新页面。

DataContext 使用 `UI_PROPERTY_DATA_CONTEXT` / `PropertyTarget.DataContext`，类型为借用的 Model 句柄，描述包含 Bindable 和 Inherits。通用读取、基础值、来源、分层设置和清除与专用 API 共用通路。显式空值覆盖样式上下文；清除局部值先显露样式，清除最后一个值层才恢复父级继承。DataContext 不可动画，也不接受 TwoWay。

```xml
<StackPanel DataContext="{Binding SelectedCustomer}">
  <TextBlock Text="{Binding Name}"/>
  <TextBlock Text="{Binding City}" DataContext="{Binding Address}"/>
  <TextBlock DataContext="null" Text="{Binding DeferredCaption}"/>
</StackPanel>
```

DataContext 的绑定从父级上下文读取，其他属性从本节点的有效上下文读取；与 XML 属性顺序无关。页面根节点的 DataContext 绑定使用加载时传入的页面模型。嵌套模型切换先准备叶子绑定，非法更新保留旧子树。显式 null 允许休眠绑定；完全没有模型或显式上下文的错误页面仍被拒绝。XML、生成页面和 编译资源采用相同语义。生成的三语言元素描述包含 DataContext，C# 元素提供类型化访问。

编辑器提供 DataContext 补全、绑定类型诊断和属性 F12。模型句柄当前不携带嵌套模型的静态类型描述；处于上下文覆盖之下的路径明确提示元数据未知，不用页面模型生成错误的缺失路径诊断、补全或跳转。

组件原有 SetDataContext/get_data_context 与宿主的通用 DataContext 属性共用同一份值层，不再维护单独缓存。C 新增 `ui_component_clear_data_context`，C++/C# 对应 `FrameworkElement.clearDataContext/ClearDataContext`。组件也支持 null 字面量、样式上下文、显式空值、清除恢复与刷新保留；模板替换从宿主继承当前值。普通节点位于模板内部时同样可建立局部上下文边界。

组件自定义属性按上下文绑定可使用 `ui_node_bind_registered_context_property`、C++ `Node::bindContext(RegisteredProperty, ...)`、C# `Node.BindContext(RegisteredProperty, ...)`。没有模型时保留休眠声明，恢复继承后重新连接；显式模型绑定不随上下文改变。组件 TwoWay 在重连后写入新模型，清除 Local 解除连接。

切换预先复制组件目标值、解析 Self 属性依赖，并用待提交的组件值继续计算 TemplateBinding 所提供的嵌套 DataContext。缺失路径、目标类型错误或 Validate/Coerce 失败不会先发布部分组件/模板值。模型销毁清理继承连接与模板文字，保留较低值层。自定义属性自身的 Inherits 传播见下节。

## 自定义属性元数据

`ui_component_property.metadata` 可指向 `ui_component_property_metadata`（struct_size/version=1），声明现有 `ui_property_flags`、Validate、Coerce 和借用的 context。注册时复制元数据，应用必须让回调上下文存活至 App 销毁。`ui_component_type.version` 必须使用 `UI_COMPONENT_TYPE_VERSION`（2），旧版属性数组布局在读取前以 VersionMismatch 拒绝。

C++ 可用 `sunfleck::componentPropertyMetadata(flags, validate, coerce, context)` 构造描述。C# 对应 `ComponentProperty` 的 `Effects`、`Validate`、`Coerce` 参数：

```csharp
new ComponentProperty("Amount", typeof(int), 0,
    Effects: PropertyEffects.Bindable | PropertyEffects.Animatable | PropertyEffects.AffectsRender,
    Validate: value => (int)value! >= 0,
    Coerce: value => Math.Min((int)value!, 100))
```

`Effects: null` 采用推断标志；`Effects: PropertyEffects.None` 明确禁止作为绑定目标及动画目标。TwoWay 要同时允许 Bindable 和 TwoWay；不可插值的属性不能声明 Animatable。AffectsMeasure 使节点及祖先的尺寸缓存失效，AffectsArrange 使节点排列缓存失效；纯绘制/输入变化复用有效几何。当前仍会遍历依赖并生成整帧绘图命令，尚未实现子树绘图缓存或独立合成阶段。

解析完优先级后，先 Coerce，再 Validate；两者都作用于最终选中的值。类型和对象所有权仍由框架验证。失败保留所有层和原订阅；移除高优先级层时，如果显露的基础值非法，也保留原覆盖层。基础值读取返回应用请求的原始值，有效值读取返回强制处理后的结果。页面刷新对保留的基础层重新解析，不把原始值直接当成呈现值。

Validate/Coerce 在提交前执行；原生回调抛出 `std::bad_alloc` 返回 OutOfMemory，其他异常返回 InvalidState，旧有效值与绑定保留。提交后的属性变更通知是观察回调，应用不应抛出异常；框架不承诺回滚回调已经产生的外部业务副作用。此边界与非法属性更新、校验失败及页面刷新失败的原子性不同。

回调在 UI 线程执行，包括实例初始化时的默认值解析（早于 Initialize 生命周期回调）。回调不能修改当前组件属性；此类重入返回 InvalidState。C# 桥接复制字符串和几何输出，并在注册失败或 App 销毁时释放回调存储。原生 Coerce 输出中的指针必须保持有效直到属性操作返回，不能指向回调局部栈对象。

Fleck 本身通过组件注册类型获得这些语义。声明式 C# 可用 `[FleckProperty(Effects = PropertyEffects.Bindable | PropertyEffects.Animatable)]`；生成器将 flags 写入 manifest，编辑器补全、绑定诊断与检查器读取同一标志。C/C++ 元数据导出器也输出 flags；存在原生回调时必须指定可执行 registrar，回调本身不序列化进资源。编辑器静态校验不执行 Validate/Coerce 或生命周期应用代码，运行时加载仍执行完整校验。

自定义属性可以声明 Inherits；内建与组件声明属性可通过下述运行时标识访问。

## 运行时属性标识

`ui_node_resolve_property(node, name, &id)` 按节点解析内建或组件声明属性。`ui_property_id` 为不含地址的 64 位标识，0 无效。组件声明优先于同名内建属性；原有 `ui_property` 枚举 API 仍可明确访问宿主的内建属性。组件的每个注册类型拥有自己的属性身份，同类型实例共享身份但不共享属性值；不同 App 中同名注册类型的身份不同。跨类型、跨 App 使用组件标识会失败，不会误写另一属性。

标识在 App 生命周期内跨页面刷新、模板替换与编译资源加载保持稳定，失败注册不会回收已分配标识。不要将数值写入 Fleck、manifest、编译资源或应用持久化文件；新 App 应按名称重新解析。内建标识可跨 App 使用，但仍检查目标节点是否支持该属性。全部入口遵循 UI 线程约束。

| C ABI | C++ Node | C# Node |
|---|---|---|
| `ui_node_resolve_property` | `resolveProperty` | `ResolveProperty` |
| `ui_node_describe_registered_property` | `describeProperty` | `DescribeProperty` |
| `ui_node_get_registered_property` | `propertyValue` | `GetProperty` |
| `ui_node_get_registered_base_property` | `basePropertyValue` | `GetBaseProperty` |
| `ui_node_get_registered_property_source` | `propertySource` | `GetPropertySource` |
| `ui_node_set_registered_property` | `setPropertyValue/setPropertyLayer` | `SetProperty/SetPropertyLayer` |
| `ui_node_clear_registered_property` | `clearProperty/clearPropertyLayer` | `ClearProperty/ClearPropertyLayer` |
| `ui_node_bind_registered_property` | `bindProperty` | `BindProperty` |
| `ui_node_bind_registered_context_property` | `bindContext` | `BindContext` |

C++ 和 C# 使用 `RegisteredProperty` 区别于旧枚举。描述返回声明者的命名空间和名称、值类型、flags、只读标志及默认值。C 的描述和绑定选项均要求 struct_size 与 version=1；未知版本明确返回 VersionMismatch。默认值及描述字符串由 App 持有；有效值读取沿用原借用规则，需在后续更新前复制。

```csharp
var node = app.FindFleckNode("control");
var amount = node.ResolveProperty("Amount"); // 自定义组件声明属性。
var metadata = node.DescribeProperty(amount);
node.SetPropertyLayer(amount, ValuePrecedence.Style, 10);
node.BindProperty(amount, model, "Amount", PropertyBindingMode.OneWay);
var effective = node.GetProperty(amount);
var basis = node.GetBaseProperty(amount);
node.ClearProperty(amount); // 解除局部绑定，显露 Style=10。
```

注册入口与原入口共用值层、校验/强制值、通知和 TwoWay 通路。动画层不写回源，清除 Local 解除绑定。通用写入口拒绝只读属性；组件所有者仍可通过原有 `Set` 发布只读状态。组件绑定目前仅支持 PropertyChanged 且不支持 StringFormat；新入口在变更前拒绝其他选项，不会静默忽略。普通内建属性保留其已有触发器和格式规则。

Fleck 组件属性绑定也解析并使用同一运行时标识，不改变 XML 写法、组件元数据补全和 F12 规则。检查器属性条目增加字符串形式的 `propertyId`，组件属性同时包含 `ownerNamespace/ownerName`，避免 JavaScript 数字精度损失。

DataContext 可在普通节点和组件宿主上解析，原组件 API 与注册标识使用同一值层。布局缓存及其失效规则见 [增量测量与排列](#增量测量与排列)。

## 自定义属性继承

组件属性元数据可声明 `UI_PROPERTY_INHERITS` / `PropertyEffects.Inherits`。相同 App、相同注册类型的后代实例从祖先继承该属性的有效值，中间的普通容器、模板节点和其他组件类型不截断传播。不同注册类型的同名属性有不同身份，不会因名字相同自动互相继承；当前组件类型注册没有基类属性重用机制。

继承优先级低于 Theme、Style、Trigger、Template、Local/Binding 和 Animation。较高层遮盖期间仍更新继承层，清除后显露最新继承值。一个后代覆盖自身值后，其后代继承这个有效值；继承不会给后代创建祖先的 TwoWay 绑定，也不会写回祖先绑定源。未赋值的元数据默认值不形成继承来源，显式赋默认值则形成继承来源。DataContext 的 null 边界只影响 DataContext，不阻断其他属性。

传播先计算整棵受影响子树的值层、强制值和验证结果。后代验证期间尝试设置正在传播的祖先属性会返回 InvalidState。调用 TwoWay 模型写入回调前，运行时已为本次值发布准备好连接与通知列表。任一后代验证失败会保留父级、兄弟和子树原值；挂接子树失败时也保持未挂接状态。值相等时仍维护来源与被遮盖的继承层，但不重复发布有效值变化。Inherits 属性的 `Inherited` 层由框架管理，应用不能直接设置/清除该层；应设置祖先，或通过自身较高优先级层覆盖。

C#/C++ 新增现有 C ABI `ui_component_create` 的包装：`App.CreateComponent(...)` 和 `FrameworkElement::create(...)`，返回 App 所拥有的实例。它们允许以代码组合已注册组件，并保留注册的托管类型和生命周期回调。例如，在已注册带 `Level`（默认 7、Inherits）的 `Scope` 及其带 `children` 容器的模板后：

```csharp
var parent = app.CreateComponent("urn:inherit", "Scope");
var child = app.CreateComponent("urn:inherit", "Scope");
parent.Set("Level", 10);
parent.FindNode("children").Append(child.Node);
// child.Get<int>("Level") == 10，来源为 Inherited。
child.SetLayer("Level", ValuePrecedence.Style, 5);
parent.Set("Level", 11); // child 仍为 5，但继承层已经更新。
child.ClearLayer("Level", ValuePrecedence.Style); // 恢复 11。
```

C#/C/C++ 元数据导出、manifest、生成器和编辑器均保留 Inherits 标志。声明式 C# 使用 `[FleckProperty(Effects = PropertyEffects.Bindable | PropertyEffects.Inherits)]`。这些传播规则覆盖当前组件属性；字体等内建属性按各自的 Inherits 元数据传播。

## 增量测量与排列

运行时按节点保存两个方向的自然尺寸、带约束尺寸和排列结果。绘制前比较精确依赖快照（不使用可能碰撞的散列），覆盖原生输入直接更新、文字源、子节点顺序、DPI、主题度量和已注册字体变化。尺寸变化沿祖先路径失效；未变化兄弟复用其测量，父级分配槽位变化则重新排列。对齐变化只清除排列结果，主题颜色、IsEnabled、悬停和光标变化保留有效几何。没有脏内容时仍不请求新帧。

检查器每个节点的 `layoutWork` 提供累计 `measure`、`arrange`、`measureHits`、`arrangeHits`、`invalidations`；VS Code 属性面板展示同一组数据。measure 包含各方向自然尺寸及约束尺寸的实际计算次数，不能直接等同于帧数。该实现仍遍历树检查依赖并生成绘图命令，不代表绘图命令缓存或独立合成已经完成。

类型化数字字面量在 Windows 与当前 Emscripten 上采用相同的十进制语法：允许小数和指数，保留负零与可表示的次正规数，拒绝非有限数、溢出、下溢至零、前导加号、十六进制及尾随字符。Emscripten 标准库缺少浮点 from_chars 时使用显式 C 数字 locale，不修改应用 locale。`tests/value_literals.cpp` 在两种目标运行；失败保持输出值和调用方几何缓冲区。
