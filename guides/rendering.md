# 渲染与部署

Sunfleck 将统一绘制命令交给 CPU、Skia 原生 D3D12、D3D11、EGL/GLES3 或 Metal 执行。以下线路用于桌面窗口，Headless 始终使用 CPU，只接受 UI_AUTO 或 UI_SOFTWARE。构建开关和系统依赖见 [构建与测试](../../core/docs/building.md)。

## 选择渲染线路

| 平台与线路 | `fleck_preview --renderer=` | C / C++ 枚举 | C# Renderer | 失败行为 |
|---|---|---|---|---|
| Windows / Linux / macOS 纯 CPU | `software` | `UI_SOFTWARE` | `Software` | 不尝试 GPU |
| Windows 自动（默认） | `auto` | `UI_AUTO` | `Auto` | 启用 Skia 时 D3D12 → CPU；否则 D3D11 → CPU |
| Windows 原生 D3D11 严格模式 | `d3d11` | `UI_D3D11` | `D3D11` | 返回错误 |
| Windows ANGLE 并允许回退 | `angle` | `UI_ANGLE` | `Angle` | ANGLE → CPU |
| Windows ANGLE 严格模式 | `gles3` | `UI_GLES3` | `Gles3` | 返回错误 |
| Linux 自动（默认） | `auto` | `UI_AUTO` | `Auto` | 编译 GLES3 时尝试 EGL/GLES3，失败后 CPU；未编译时直接 CPU |
| Linux EGL/GLES3 严格模式 | `gles3` | `UI_GLES3` | `Gles3` | 返回错误 |
| macOS 自动（默认） | `auto` | `UI_AUTO` | `Auto` | Metal → CPU |
| macOS Metal 严格模式 | `metal` | `UI_METAL` | `Metal` | 返回错误 |

严格模式要求对应后端已编译且平台支持。UI_ANGLE 仅适用于 Windows。未编译 GLES3 后端时也可选择该模式，并使用 CPU。

预览工具的渲染参数必须放在第一个参数位置，在仓库根目录运行：

```powershell
.\out\debug\fleck_preview.exe --renderer=angle .\tests\fixtures\welcome.fleck
```

C/C++ 将枚举填入 `ui_app_options.renderer`，C# 例如 `new App(Renderer.Angle)`。这些命令行参数属于 fleck_preview；独立示例通过应用选项选择线路。允许回退的模式在初始化或呈现失败时保留窗口与界面状态，该窗口随后使用软件呈现。

`UI_SHOW_RENDERER` 或 C# 构造参数 `showRenderer` 在标题中显示当前线路。`[CPU]` 表示 Sunfleck 软件栅格器，`[GPU]` 表示 Skia D3D12、D3D11、EGL/GLES3 或 Metal 后端。Mesa 软件驱动也可能显示 `[GPU]`，这个标记不保证硬件加速。

## Windows 依赖

Windows 新绘图路线采用 [Skia 原生 D3D12](../../core/docs/skia-runtime.md)。当前通过 `UI_ENABLE_SKIA_D3D12=ON` 显式启用，需固定候选及正确性补丁；此路线无需 ANGLE。系统无可用硬件 D3D12 或呈现失败时，Auto 回退 CPU。DirectWrite 文字服务不变。

D3D11 默认启用，运行使用系统 D3D11/DXGI，无需 ANGLE。Windows 文本使用 DirectWrite 和系统 ICU 字素分段，原生窗口与 Headless 共用文字服务；需要 Windows 10 1903 或更新版本（最低版本实机验收待补）。系统字体不随框架分发，详见 [字体与文字服务](fonts.md)。

ANGLE 需要显式启用 GLES3。以已安装 ANGLE 头文件的 vcpkg 为例，在 Visual Studio x64 开发者终端配置：

```powershell
cmake --preset debug -DUI_ENABLE_GLES3=ON "-DCMAKE_PREFIX_PATH=$env:VCPKG_ROOT/installed/x64-windows"
cmake --build --preset debug
```

运行时将匹配版本、架构的 `libEGL.dll`、`libGLESv2.dll` 及传递依赖放在应用可执行文件目录。某些 vcpkg 构建还需要 `zlib1.dll`，以实际产物依赖为准。未提供这些库时，UI_ANGLE 回退 CPU，UI_GLES3 返回错误；仅使用 CPU/Skia D3D12/D3D11 的应用无需打包 ANGLE。

## Linux 依赖

桌面需要 X11 和 libXext 运行库，Dock 穿透预览使用 X Shape 1.1；无合成器时使用镂空预览。构建探测到 FreeType 后，文本绘制还需要对应运行库与可用字体。

启用 GLES3 后，运行时动态加载 `libEGL.so.1`、`libGLESv2.so.2`，由 Mesa 或厂商驱动提供实现。发布前应验证同一允许回退的产物在缺少 EGL/GLES、但保留 X11 的环境中可以启动和交互，并另行验证实际硬件驱动。Xvfb/Mesa 流程见 [容器测试](../../core/docs/building.md#linux-容器)。

## macOS 依赖

桌面窗口由 AppKit 管理，窗口和事件循环在主线程运行。Metal 使用 `CAMetalLayer` 呈现，软件路径使用 Core Graphics 位图。两条路径均使用 CoreText 生成文字覆盖率，并按窗口的 backing scale 渲染。用 `fleck_preview --renderer=metal` 可检查严格模式。`UI_AUTO` 在 Metal 初始化或呈现失败时使用软件渲染。[macOS 构建与预览](../../core/docs/building.md#macos) · [GPU 验证记录](../../core/docs/validation.md#原生桌面与-sdk)

## 分发边界

当前支持源码消费及 [安装式 SDK](../../core/docs/sdk.md)，包括 CMake 导出和 C／C++／C# 消费入口。应用需打包自己的资源、匹配的运行库和依赖许可证信息；.NET 应用的文件清单见 [C# 部署](csharp.md#部署)。

## 平台边界

桌面后端覆盖 Windows、Linux X11 和 macOS。X11 后端可在 XWayland 会话运行，并支持接收外部拖放中的文本和本地文件。macOS AppKit 后端已支持基本键鼠交互和文本编辑，系统输入法组合、原生无障碍桥接和外部拖放尚未接入。Safari 中的输入法、VoiceOver 与拖放验收记录见 [macOS Safari 交互](../../core/docs/validation.md#macos-safari-交互)。原生 Wayland、移动端和嵌入式后端尚未实现。
