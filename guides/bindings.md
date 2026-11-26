# 属性绑定与变更通知

Width、Height、MinWidth/MaxWidth、MinHeight/MaxHeight、Margin、HorizontalAlignment、VerticalAlignment 支持 OneWay/OneTime，默认 OneWay。尺寸使用整数，外边距和对齐使用字符串；具体范围、Auto 和模板绑定见 [通用布局属性](layout-properties.md)。

Fleck 在目标属性上声明 `{Binding ...}`，C、C++、C# 共用原生属性绑定引擎。一个节点可以有多个绑定，一个源也可以驱动多个节点。本文描述当前实现的子集；它不要求 WPF 运行时。

## 语法与目标属性

```xml
<Button Content="{Binding ButtonText}"
        IsEnabled="{Binding CanIncrement}"
        Command="{Binding Increment}"/>
<TextBlock Text="{Binding Count, StringFormat='Count: {0}'}"/>
<TextBox Text="{Binding Path=Name, Mode=TwoWay, UpdateSourceTrigger=LostFocus}"/>
<Slider Value="{Binding Volume}"/>
```

| 配置 | 规则 |
|---|---|
| Path | 区分大小写的导出名称；允许字母、数字、下划线及点分隔路径，每段不能以数字开头，最长 128 字节 |
| Mode | Default、OneWay、OneTime、TwoWay；默认 Default，由目标属性决定 |
| UpdateSourceTrigger | PropertyChanged（默认）或 LostFocus；只适用于可回写属性 |
| StringFormat | 空或恰好一个 `{0}`；仅用于单向文本显示，格式属于目标绑定 |

字符串选项可用单引号或双引号包围，内部的逗号不分隔选项；反斜杠转义下一个字符。XML 引号还需遵守 XML 实体规则。文本以 `{}` 开头表示后续按字面量处理，例如 `Content="{}{Binding Name}"` 显示 `{Binding Name}`。

| 目标 | 源类型 | 默认模式 |
|---|---|---|
| Button.Content | 字符串、布尔、整数、浮点数，转换为显示文本 | OneWay |
| 支持文字的控件 Text | 同上；TwoWay 要求字符串 | 编辑器 TwoWay，其余 OneWay |
| IsEnabled | 布尔；父容器禁用会同时阻止子控件交互 | OneWay |
| Slider、SpinBox、SplitView 的 Value | 整数或数值为整数的浮点数，0–100 | TwoWay |
| ProgressBar.Value | 同上 | OneWay |
| CheckBox、RadioButton、Switch、ToggleButton 的 Value | 布尔或 0/1 数值 | TwoWay |
| 可交互控件 Command | 同 App 的 Command 引用 | OneWay |

Value 沿用控件已有的整数精度，不会静默截断浮点数。Button.Content 当前只表示文本，不承载任意对象或子控件。TwoWay 需要可写源属性；只读属性可显式指定 OneWay。命令的执行方式与绑定方向独立，C++ 生成页面可保留 `CommandMode="Async"`。

OneWay 已支持“模型变更 → 控件更新”，无需 TwoWay。TwoWay 在此基础上增加“用户编辑控件 → 写回模型”，当前只适用于可编辑的 Text/Value；Button.Content、TextBlock.Text 等显示属性不支持 TwoWay。例如按钮文案应写 `Content="{Binding buttonText, Mode=OneWay}"`，也可省略 Mode。可编辑文本使用 `TextBox.Text="{Binding name, Mode=TwoWay}"`，C++ 模型同时提供 `name()` getter 和 `name(std::string)` setter。

TextArea 与单行编辑器绑定同一模型的同一路径时，TextArea 也禁止输入或回写换行，避免共享源更新后单行编辑器无法显示。

相同源可使用不同 StringFormat；同一目标属性不能重复声明。Button.Text 与 Button.Content 指向同一内部属性。对节点调用普通 setter 会解除该属性的绑定，其他属性绑定保留；修改源属性才能持续驱动 UI。ClearBinding 解除绑定并保留当前值，UpdateSource 可立即提交等待 LostFocus 的输入。

## C：函数式属性与通知

```c
static ui_result read_count(void* context, ui_binding_value* value)
{
    value->kind = UI_VALUE_INT;
    value->integer = ((Counter*)context)->count;
    return UI_OK;
}

ui_model* model = NULL;
ui_model_create(app, &model);
const ui_model_property property = {
    sizeof(property), 1, "Count", UI_VALUE_INT, read_count, NULL, &counter
};
ui_model_add_property(model, &property);

/* 修改业务状态后通知；通知在 UI 线程调用。 */
++counter.count;
ui_model_notify_property_changed(model, "Count");
```

示例省略状态码分支；实际应用应检查每个 ui_result。传入 NULL/空名称刷新全部属性；未知名称返回 InvalidArgument。getter 返回的 UTF-8 指针由业务保持有效，框架在返回后立即复制。setter 接收的值仅在回调期间有效，需保存字符串时自行复制。回调不能抛出异常，也不能销毁模型或应用。

`ui_model_add_text_source`、`ui_model_add_command` 可适配已持有的源和命令。文本源更新会自动通知关联模型，销毁源会解除相应绑定并保留显示值。显式 `ui_fleck_data_context` 导出也会适配为属性模型，不进行全局名称查找。

XML 使用 `ui_app_load_fleck_with_model`，二进制使用 `ui_app_load_fleck_resource_with_model`，生成 C 提供 `<symbol>_with_model`。不使用 Fleck 时可直接调用 `ui_node_bind_property`。完整 C 示例见 [计数器](../../samples/counter/c/fleck/main.c)。

## C++：契约、属性适配与连接

生成器以 Path 对应同名公开字段或无参数 getter；TwoWay 字段自动写回，getter 属性使用同名单参数 setter。绑定字段／方法由 Fleck 声明确定，不需要业务模型重复注册。可读写接口示例：

```cpp
std::string name() const { return name_; }
void name(std::string value)
{
    name_ = std::move(value);
    changed_.notify("name");
}
auto subscribePropertyChanged(sunfleck::PropertyChangedHandler handler)
{
    return changed_.subscribe(std::move(handler));
}
```

模型成员包含 `sunfleck::PropertyChangedEvent changed_`。`NotifiesPropertyChanged` concept 要求 subscribePropertyChanged 接收 handler 并返回 PropertyChangedConnection。模型可以自行实现契约，连接析构必须退订；无需继承框架基类。

`FleckPage` 自动持有连接并在析构时退订。普通模型仍可调用 `page.refresh()` 或 `page.notifyPropertyChanged(path)`。生成器也支持 getter 返回 TextSource 引用；普通属性无需使用 TextSource。通知仅描述属性名称，读取由原生 getter 完成。

构建时运行 `fleckc --format cpp --symbol load_counter main.fleck counter_bindings.hpp`，在模型定义之后包含生成头。生成的 `sunfleck::generated::load_counter::bind(app, model)` 自动创建页面所需的属性适配器及命令，不加载页面或读取 getter，可通过 `bindings->properties().describe("CounterModel")` 导出元数据。`bindings->load(xml)` 加载原始 XML；生成的 `load(app, model, xml)` 合并这两步。原有 `load(app, model)` 继续加载生成的 C 页面。

同步命令方法可以无参数或接受 `sunfleck::Node`；异步方法可以无参数或接受 `sunfleck::CancellationToken`，由 Fleck 的 `CommandMode` 选择。命令所有权、异常桥接、取消和销毁由 `FleckPage` 管理，业务通过 `takeError()` 读取异常。模型必须活到绑定对象销毁后。自动生成只负责适配和命令创建；字段赋值后仍需通知。新增绑定名称或改变命令模式需要重新构建，已有绑定的布局和文本可继续实时刷新。完整用法见 [C++ 独立计数器](../../core/tests/development/fixtures/legacy-demos/counter-cpp/README.md)。

不使用生成器时，PropertyModel 接收类型化 getter/setter：

```cpp
sunfleck::PropertyModel properties(app);
properties.property("name", [&] { return model.name(); },
                    [&](std::string value) { model.name(std::move(value)); });
properties.bind(editor, UI_PROPERTY_TEXT, "name");
properties.notify("name");
```

生成器要求 Path 是 C++ 成员名。点分路径可在 PropertyModel 中按完整名称显式注册，不会自动遍历对象。生成加载器、模型及属性适配器必须早于 App 析构。

## C#：INotifyPropertyChanged

```csharp
using (var properties = app.CreatePropertyModel(viewModel))
{
    app.LoadFleck(xml, properties);
    app.OpenWindow();
    app.Run();
}
```

viewModel 可直接实现 System.ComponentModel.INotifyPropertyChanged。适配器导出支持类型的公开、非索引属性；公开 setter 用于 TwoWay。支持 string、bool、常见有符号整数及 byte/ushort/uint、float/double 和 Sunfleck.Command。通知空名称刷新全部，通知未导出的属性会忽略。

也可不使用反射：`app.CreatePropertyModel().Add("Count", () => count)`，为可写属性传入 setter；AddCommand 注册命令。需要手动刷新时调用 NotifyPropertyChanged。getter/setter 异常沿现有托管回调桥接，在同步调用或 Poll 返回后传播，不穿过 C ABI。

适配器保活委托并管理事件订阅，Dispose 自动退订；App.Dispose 会释放未手动释放的适配器。创建、绑定和释放在 UI 线程执行。完整实现见 [C# 计数器](../../samples/counter/csharp/fleck/Program.cs)。

## 线程、批量更新与异步命令

模型通知和控件访问在 UI 线程执行。后台任务（包括 Async 命令）通过 `ui_app_post`、C++ `app.post`／`app.invokeAsync` 或 C# `app.Post`／`app.InvokeAsync` 投递更新。命令使用原业务对象，不复制模型。C# 普通模型的后台 PropertyChanged 仍自动排队，在 UI 线程读取属性；这不保证业务模型本身线程安全，推荐把模型修改和通知一起投递到 UI 线程。

队列最多 1024 项，Poll 单次平台等待上限 16 ms。投递成功后队列接管上下文，执行或取消时在 UI 线程释放；投递失败仍由调用方释放。应用销毁前必须停止并等待后台投递者，不能使用已经失效的 App 句柄。C# 对 Post 与 Dispose 的本地互斥不能代替业务任务取消。

BeginUpdate/EndUpdate 可嵌套，在最外层 EndUpdate 合并重复通知。通知不会自动发现计算属性依赖，例如修改 Count 后应显式通知依赖它的 CanIncrement。无变化的值不会重复设置目标。单个目标转换失败时保留其原值并返回错误；通知批次不是业务数据事务，其他有效目标可以更新。setter 应先校验再修改数据。

### Async 命令与中途通知

C、C++、C# 共用原生异步命令调度。Fleck 的 CommandMode 支持 Sync（省略时的默认值）和 Async；同步命令在 UI 线程执行，异步命令在后台执行。Async 必须绑定异步命令；异步命令也不能显式切换为 Sync，以免 UI 线程等待后台任务时阻塞 UI 投递。纯代码节点可调用 `ui_node_set_command_mode`、`Node.setCommandMode` 或 C# `Node.SetCommandMode`，其 Default 模式遵循注册命令的类型。

同一命令实例默认 SingleFlight：运行期间重复触发会忽略；同一模型的其他命令、UI 属性通知和 TwoWay 写回仍然可用。不同命令同时访问同一资源时由业务层协调。`ui_model_is_running`／C++ `PropertyModel::running` 仅观察该模型绑定的命令，不锁住整个模型。

UI 回调直接修改原模型并通知绑定。已完成的修改不会因后续异常或取消自动回滚；需要事务语义时由业务自行实现。OneTime 不接收后续通知；已经解绑的目标也不接收。短于一帧的中间状态可能合并。包含正在执行命令的 Fleck 页面暂缓热刷新，命令结束后继续应用排队的页面变更。

**C#** 通过 App 创建异步命令，只需一个返回 Task 的委托：

```csharp
using var command = app.CreateAsyncCommand(async cancellationToken =>
{
    await Task.Delay(100, cancellationToken); // 可替换为实际 I/O
    await app.InvokeAsync(() =>
    {
        model.Count++;
        properties.NotifyPropertyChanged(nameof(model.Count));
    });
});
```

委托从后台开始，框架不安装 UI SynchronizationContext，`await` 后不会自动回到 UI 线程。`InvokeAsync(Action, CancellationToken)` 在 UI 线程调用时立即执行，否则排队；返回的 Task 表示 UI 回调完成，回调异常通过该 Task 传播。命令内的 `InvokeAsync`／`Post` 自动继承当前命令的取消信号（跨 await 有效），也可以显式传入额外令牌。取消尚未开始的回调会跳过它；已开始执行的回调不会被强行中断。不要在 UI 线程同步等待后台命令。

`Command.Cancel()` 在 UI 线程请求取消，`Command.IsRunning` 查询状态。未处理的业务异常在 Poll 的 UI 线程传播；被命令捕获处理的异常不会再次上报。完整最小案例见 [tests/development/fixtures/legacy-demos/counter](../../core/tests/development/fixtures/legacy-demos/counter/README.md)。

**C++** 生成绑定调用原模型的方法，支持无参数或接受 `sunfleck::CancellationToken`；模型可以不可复制并持有系统资源。例如模型保存 `App& app_`：

```cpp
void increment(sunfleck::CancellationToken token)
{
    // 执行耗时操作，并在适当位置检查 token.cancelled()。
    token.throwIfCancelled();
    app_.invokeAsync([this] {
        ++count_;
        changed_.notify("count");
    }, token).get(); // 这里只能在后台线程等待
}
```

`invokeAsync` 返回 `std::future<void>`，取消未执行回调时以 `Error(UI_CANCELLED)` 结束；不能在 UI 线程对它 `.get()`。`post` 不等待。当前原生命令执行线程的投递默认继承取消信号，额外工作线程应显式传递 token；token 是借用句柄，仅在本次 execute 回调返回前有效，子任务须在返回前结束。页面保持原通知订阅，TextSource 也可使用，但必须在 UI 线程访问。异常由 `page.takeError()` 获取。

**C** 使用 `ui_command_create_async` 和 **version=2** 的 `ui_async_command_options`：`execute(context, token)` 在后台操作原对象；可选 `cancel(context)` 和 `completed(context, result)` 在 UI 线程执行。通过 `ui_app_post_cancellable` 投递 UI 更新，`ui_cancellation_requested` 检查取消，`ui_command_cancel` 请求取消。完成回调可以处理正常结果、错误或取消；销毁命令时不再调用 completed。投递的 release 回调即使取消也会执行，负责释放上下文。C 回调不得抛出异常。完整示例见 [C 计数器](../../samples/counter/c/fleck/main.c)。

销毁命令、绑定模型、页面或 App 会请求取消相关工作、取消待执行的 UI 回调，再等待工作结束。最后一个窗口关闭时也请求取消；独立窗口仍存活时不会关闭整个 App 的调度。业务应把 CancellationToken 传入可取消的 I/O，或检查取消标记；框架不会强行终止不配合取消的线程，也不会自动替换按钮文案。原模型／资源必须活到命令结束；未绑定模型的独立命令须在释放其业务资源前显式销毁。C# 当前将 Task 的完整生命周期桥接到原生工作线程，每次活动命令占用一个原生工作线程。

这是一次破坏性 API 调整：删除旧 prepare/read/commit/release 快照协议和 `PropertyModel.CreateAsyncCommand`，不保留兼容入口。C# 改用 `App.CreateAsyncCommand`；C 使用 v2 结构；C++ 模型不再要求复制／赋值。旧二进制与托管绑定不能混用，必须一起重新编译；原生入口拒绝 v1 结构。

## 文本集合

在 UI 线程上给属性模型启用文本集合后，Fleck 可通过 `{Binding Count}` 显示项数，通过 `{Binding Revision}` 观察内容或顺序变化。这两个路径由集合保留，均为只读整数。启用前已注册同名路径会报错。

```csharp
using var rows = app.CreatePropertyModel();
var names = new System.Collections.ObjectModel.ObservableCollection<string> { "甲", "乙" };
rows.ObserveTextCollection(names);
app.LoadFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock Text='{Binding Count}'/></Window>", rows);
names.Add("丙");  // 页面显示 3
names.Move(2, 0); // 丙保留项目 ID，Revision 增加
```

C 使用 `ui_model_enable_text_collection`、`ui_collection_splice`、`ui_collection_move`，C++ 对应 `enableTextCollection`、`spliceItems`、`moveItems`。移动目的索引按移除原区段后的列表计算。每个新增项获得独立 ID，包括同名项和替换项。无效索引或 UTF-8 会拒绝整批修改。集合提交后，绑定刷新错误仍会报告给调用方。C# 观察源的增删、替换、移动和重置须在 UI 线程发出，模型释放时解除订阅。

文本集合可用作 ItemsControl、ListView、ComboBox 的 ItemsSource。模型属性 `Rows` 返回已启用集合的模型时，页面可写 `<ListView ItemsSource="{Binding Rows}"/>`。Fleck 绑定在下一轮 Poll 呈现更新。ListView 使用等高文本行，根据视口和滚动位置生成容器。检查器的 `items.count`、`firstRealized` 和 `realized` 分别表示项目总数、首个容器索引和当前容器数量。C/C++ 生成行句柄在该行离开实现范围、删除或换源后失效，业务层应保存项目 ID。选择跟随同一项目，删除选中项后选取原索引处的后继，删除末项则选取前一项。

使用 `SpliceModels` 可为每个项目附加同一 App 内的属性模型。下面的模板绑定项目模型上的 `Caption`，项目属性通知会更新已经呈现的行。

```xml
<ListView ItemsSource="{Binding Rows}">
  <ListView.ItemTemplate>
    <DataTemplate><Border><TextBlock Text="{Binding Caption}"/></Border></DataTemplate>
  </ListView.ItemTemplate>
</ListView>
```

需要独立的显示顺序时，可在另一个属性模型上调用 C# `view.SetCollectionView(rows, CollectionSort.Ascending, "in")`。筛选按标签执行区分大小写的子串匹配，排序按 UTF-8 字节序，标签相同的项目保留源顺序。源模型继续接受编辑，视图只读，筛选和排序保留项目 ID。

只读视图支持按完整标签分组。调用 `SetGroupByLabel(true)` 后，`GroupCount` 和 `GetGroup(i)` 给出各组标签、起始索引和项数。组的顺序按筛选、排序后的首次出现位置决定。组内维持原相对顺序，源集合变更后重算范围。

ListView 和 ItemsControl 的 `ItemTemplateSelector` 接受带 `Key` 的 DataTemplate。Key 与项目标签精确匹配，未命中时使用 ItemTemplate，再回退到项目文字。TreeView 的 HierarchicalDataTemplate 使用 ItemsSource 绑定子集合，并在展开时生成子项。

DataGrid 通过 DataGrid.Columns 中的 DataGridTextColumn 配置 Binding、Header 和 Width。省略列定义时从对象模型的标量属性生成列。可写字符串、整数、浮点数和布尔值支持单元格提交或取消。非法输入恢复原值，`GetValidation(PropertyTarget.Text)` 提供状态、错误代码和消息。行列按视口生成。排序键通知会在 Poll 时更新行序。

## 格式、范围与验证

页面资源保留组件、模板、多文件位置、布局和显式默认值；版本迁移见 [SDK 迁移](../../core/docs/sdk-migration.md)。TextBinding、独立 BindingMode/TextFormat 和裸命令名不再解析。旧文本源 API 仍可用于纯代码控件或作为新属性模型的数据源适配。

普通子节点继承有效 DataContext。子树可绑定父模型的模型引用作为新上下文，再用子模型的属性名绑定叶子节点。显式设置空上下文会截断继承，清除局部值后恢复继承。组件还支持 Self 和 TemplatedParent。内建属性可绑定到点分模型路径，例如 `Selected.Caption`。首次连接时需要能解析叶子类型，替换上级模型后重新订阅。C# 可用下面的模型注册代码声明多输入转换，页面通过 `{Binding Display}` 读取结果。转换结果只读。见 [DataContext 与属性继承](property-system.md)。

```csharp
model.AddConverter("Label", values => $"{values[0]}: {values[1]}")
     .AddMultiBinding("Display", "Label", "Prefix", "Selected.Caption");
```

C 使用 `ui_model_add_converter` 和 `ui_model_add_multi_binding` 注册相同计算。输入顺序与注册时一致，一个输入可以用于单值转换。Fleck 的单值形式是 `{Binding Caption, Converter=Label}`，所注册的转换器须接受一个输入。多个输入按声明顺序传入同一模型注册的转换器。

```xml
<TextBlock.Text>
  <MultiBinding Converter="Label">
    <Binding Path="Prefix"/>
    <Binding Path="Selected.Caption"/>
  </MultiBinding>
</TextBlock.Text>
```

使用 `{Binding Text, ElementName=input}` 可读取同一名称作用域内的内建属性，使用 `{Binding FontSize, RelativeSource=FindAncestor, AncestorType=StackPanel}` 可读取最近匹配祖先的属性。C++ 生成页面可在业务模型的 `configureConverters(PropertyModel&)` 中注册转换器。

[属性专项测试](../../core/tests/property_bindings.cpp) 对照 XML、生成 C/C++、二进制页面的模型结果与 Headless 像素，覆盖多属性、格式、TwoWay、失焦提交、通知合并、不可复制模型的异步执行、跨线程投递、解绑及错误输入。[托管属性测试](../../devtools/tests/csharp/PropertyBindingTests.cs) 验证标准通知接口、后台事件、委托保活、异常与退订；[异步命令测试](../../devtools/tests/csharp/AsyncCommandTests.cs) 覆盖原资源、独立命令、异常重试、取消与关闭。


### 组件绑定

组件公开属性支持字符串、布尔、整数、浮点数、命令和模型引用。`DataContext` 默认继承调用方，清空或替换后会重新连接内部绑定；双向绑定使用 `RelativeSource=TemplatedParent` 访问模板所属控件。模型和命令由所属 App 管理，组件解绑时不会销毁业务对象。

组件公开属性的绑定按声明类型匹配。`StringFormat` 和 `UpdateSourceTrigger=LostFocus` 用于内部视觉节点的文本绑定；直接绑定自定义属性时不支持这两个选项，校验器会报告错误。需要格式化显示或失焦编辑时，将绑定写在 UserControl 或 ControlTemplate 的 TextBlock/TextBox 上。

C++ `UserControl::Subscription` 和 C# `Subscribe` 返回的订阅对象可在页面刷新或 App 销毁后安全释放。C 的普通数字订阅标识随组件失效；需要独立释放时使用 `ui_component_subscribe_scoped` 和 `ui_component_subscription_destroy`。仍在使用的控件、模型和节点句柄必须遵守各自的所属 App 与 UI 线程约束。
