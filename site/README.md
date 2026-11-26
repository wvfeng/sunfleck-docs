# Sunfleck 组件文档

文档应用使用 C++ Wasm SDK，正文和界面使用 Fleck。左侧按用途分组，每个内置组件都有实时示例、可复制的 Fleck 和 API。快速开始分别提供 C、C++ 和 C# SDK 的 Fleck 与纯代码教程。

## 独立构建

准备已激活且与 Wasm SDK 匹配的 Emscripten、Ninja、CMake 3.25+、Python 3.9+，以及同版本桌面 SDK 的 `fleckc`。字体放置方法见 [字体说明](assets/fonts/README.md)。

在 `docs/site` 目录运行以下命令。SDK 和工具链从工作区的 `sdk/local` 读取。

```powershell
$sdk = (Resolve-Path '../../sdk/local').Path
$python = "$sdk/emsdk/python/3.9.2-nuget_64bit/python.exe"
$env:EM_CONFIG = (Resolve-Path "$sdk/emsdk/.emscripten").Path
$env:EM_CACHE = (Resolve-Path "$sdk/emscripten-cache").Path
$env:PATH = "$sdk/bin;$(Split-Path $python);$env:PATH"

./build.ps1 -SdkRoot "$sdk/web-wasm32" -FleckCompiler "$sdk/windows-x64/bin/fleckc.exe" -EmsdkRoot "$sdk/emsdk" -Python $python
& $python -m http.server 8790 --bind 127.0.0.1 --directory out/web
```

打开 [本地预览](http://127.0.0.1:8790/#/Button)。输出目录 `out/web` 可静态部署，ZIP 为 `out/sunfleck-docs.zip`。站点支持 `#/Button` 和 `#/quick` 等深链接。

## EdgeOne Makers 部署

关联 `sunfleck-docs` 仓库后，将 Makers 的根目录设为 `site`，框架预设选「其他」或「自定义」。[edgeone.json](edgeone.json) 指定安装命令 `true`、编译命令 `npm run build` 和输出目录 `out/makers/site/web`。如果控制台要求填写这些字段，使用相同的值。

构建命令在 Linux x86_64 上运行，需要 Bash、Git、curl、tar 和 Python 3.9+。它下载 Core `v1.0.0` 的 Wasm SDK，并安装指定版本的 CMake、Ninja、Emscripten 和字体。构建时，文档站和 Node.js 版 `fleckc` 都链接 SDK 中已编译的 Core 库。`fleckc` 只编译同一标签下的工具入口 `tools/fleckc.cpp`，然后校验各个文档页面。站点输出包含 HTML、JavaScript、Wasm、字体数据和许可证。[Core v1.0.0 Release](https://github.com/wvfeng/sunfleck-core/releases/tag/v1.0.0)

## 内容维护

- 组件页在 `content/components/<分类>/<组件>.fleck`，分别声明用途、实时示例、指南，以及需要重点解释的 API。属性类型和默认值来自 SDK schema。
- 项目教程在 `content/quickstart/`。教程使用解压后的各语言 SDK。下载入口统一在 [sdk-links.json](sdk-links.json) 配置，空值显示为不可点击。
- 卡片、代码块、API 列表和 Tab 模板封装在 `components/`。API 列表通过 `ItemsSource` 绑定，`VisualElement` 和 `LayoutElement` 的属性行由其派生组件共享。生成器展开项目内的槽位并用 SDK 的 `fleckc` 校验每页。[pages.py](tools/pages.py)
- Image 示例使用随站点打包的阿波罗 17 号地球照片，原图来自 [NASA 图库](https://images.nasa.gov/details/as17-148-22727)。
