# Sunfleck Docs

[使用指南](guides)与 [Wasm 文档站](site/README.md)。站点使用匹配的 Wasm SDK 与桌面 `fleckc`，输出可部署到静态站点。SDK 下载地址由 [sdk-links.json](site/sdk-links.json) 管理。


## VS Code 调试

在 Sunfleck 多根工作区的 CMake 视图中选中 docs，选择配置预设 `vs2026-debug` 并配置项目。在“运行和调试”中选择 `Sunfleck Docs: 在 Edge 调试` 按 F5。启动任务会构建站点并在本机 8790 端口提供预览。

文档站使用 Ninja 和 Emscripten 3.1.56。预设从工作区的 `sdk/local/emsdk` 读取已激活的 Emscripten，从 `sdk/local/bin` 查找 Ninja，并使用 `sdk/local/web-wasm32` 与 `sdk/local/windows-x64` 中配套的 SDK 和 fleckc。字体设置见[文档站字体](site/assets/fonts/README.md)。
