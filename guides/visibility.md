# 可见性与指针命中

所有视觉节点和组件宿主都有 `Visibility` 与 `IsHitTestVisible`。它们使用统一属性系统，支持 C/C++/C#、Fleck、绑定、编译资源、检查器和编辑器导航。

| Visibility | 布局占位 | 绘制 | 输入 |
|---|---|---|---|
| Visible（默认） | 参与 | 绘制 | 受 IsEnabled 和命中设置控制 |
| Hidden | 保留测量和占位 | 隐藏整个子树 | 子树不能交互 |
| Collapsed | 排除尺寸、轨道和子项间距 | 隐藏整个子树 | 子树不能交互 |

```xml
<StackPanel Spacing="8">
  <TextBlock Text="Always visible"/>
  <Border Visibility="Hidden" Height="40" Background="#eeeeee"/>
  <Button Visibility="{Binding ActionVisibility}" Content="Run"/>
  <Border IsHitTestVisible="false" BorderBrush="#557799" BorderThickness="1">
    <TextBlock Text="Visible to the user; pointer events pass through"/>
  </Border>
</StackPanel>
```

StackPanel、Grid、Flex、WrapPanel、DockPanel、SplitView、Overlay 和滚动容器排除 Collapsed 子项。Hidden 仍消耗相同布局空间。逻辑子项编号保持稳定，选择器的方向键跳过 Hidden/Collapsed 或禁用项。Dock 专用布局的分组、标签和停靠关系继续由 Dock API 管理，视觉属性不删除工作区结构。

`IsHitTestVisible` 默认为 true。祖先设置 false 会阻止子树的指针命中、滚动和拖拽；子节点的 true 不能绕过祖先。该属性不禁用键盘 Tab 导航和已有键盘焦点。隐藏或禁用节点则立即清理子树的焦点、文字会话、鼠标按下和拖拽捕获；重新显示后不会完成隐藏前未结束的点击。迟到的输入法会话更新会被拒绝。

```csharp
var border = new Sunfleck.Elements.Border(app);
border.Visibility = Visibility.Hidden;
border.IsHitTestVisible = false;
border.Node.ClearProperty(PropertyTarget.Visibility); // 恢复默认 Visible
```

C/C++ 的 Visibility 使用 `UI_VALUE_ENUM`，值为 `UI_VISIBLE`、`UI_HIDDEN` 或 `UI_COLLAPSED`；IsHitTestVisible 使用 `UI_VALUE_BOOL`。C# 使用 `Visibility` 枚举。默认绑定方向是 OneWay，也支持 OneTime；非法枚举值保留原状态。两项属性不接受动画值层。需要移除局部设置时使用统一 ClearProperty，基础值按既定优先级恢复。

[布局属性](layout-properties.md) · [Border](border.md) · [统一属性系统](property-system.md)
