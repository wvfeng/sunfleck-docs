# 属性动画

在 Fleck 中用 `控件名.Storyboard` 声明同一控件的多个属性轨道，加载页面后从事件或命令中启动。C 调用 `ui_node_begin_storyboard`，C++ 调用 `beginStoryboard()`，C# 调用 `BeginStoryboard()`。例如，取得示例中名为 `banner` 的 Border 后启动它，即可同时改变透明度和平移位置。[Fleck 示例](../snippets/fleck/animation.fleck)

每条 Timeline 指定一个可动画的内置 Property 和正整数 DurationMs。帧位置从 0 到 1 严格递增，必须包含两个端点。Value 按目标属性类型解析。长度帧使用有限的像素值。颜色逐个 ARGB 通道插值，Point、Thickness 和矩阵逐分量插值。Easing 指定进入该关键帧的缓动，支持 Linear、EaseIn、EaseOut 和 EaseInOut。

DelayMs 期间呈现首帧。RepeatCount 默认 1，0 表示无限重复。AutoReverse 开启时一个重复周期包含正播和倒播两段。HoldEnd 开启时保留结束值，停止后显示当时的基础值。再次启动同一属性会替换其现有动画。

代码也可直接创建轨道。下面的 C# 调用将透明度在 300 毫秒内从 0 增至 1，并在结束后恢复基础值。

```csharp
node.StartAnimation(PropertyTarget.Opacity, new Timeline(
    [new KeyFrame(0, 0.0), new KeyFrame(1, 1.0, Easing.EaseOut)], 300));
```

C 的对应入口为 `ui_node_start_animation` 和 `ui_node_control_animation`，帧值在启动时复制。C++ 和 C# 的播放控制接受目标属性、Pause、Resume、Seek 或 Stop 操作。Seek 的位置是从开始播放算起的毫秒数，包含延迟。测试可以暂停并定位到精确帧。生产播放通过 App 的 Poll 推进。

示例中的按钮同时演示高度展开、收起和轻微旋转。在 C# 中可直接将播放接到按钮操作。

```csharp
var feedback = app.FindFleckNode("feedback");
feedback.SetAction(button => button.BeginStoryboard());
app.FindFleckNode("banner").BeginStoryboard();
```

测试可在启动前调用 `app.AdvanceAnimationClock(0)`，之后每次传入增量毫秒数并调用 Poll。完成回调由 App 在 UI 线程分发，每条轨道完成时提供节点和属性。刷新或销毁后，旧节点的待分发通知随目标失效。Windows 原生应用创建时读取系统客户端动画选项，应用也可通过 SetReducedMotion 直接选择终态。检查器快照的 animations 数组显示活动属性及暂停、保持状态。
