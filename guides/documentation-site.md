# 文档站维护

文档站是独立的 C++ Wasm SDK 消费项目。组件按类别从 Fleck 源文件组织，快速开始通过顶部语言 Tab 和 Fleck 开关切换。构建方式、字体和部署产物见 [文档项目](../site/README.md)。

## 准备 Wasm SDK

使用激活的 Emscripten，单独构建并安装 Sunfleck SDK。以下命令从框架仓库执行。

```powershell
emcmake cmake -S . -B out/wasm-sdk-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUI_BUILD_TOOLS=OFF -DUI_BUILD_EXAMPLES=OFF -DUI_BUILD_DEMOS=OFF -DBUILD_TESTING=OFF
cmake --build out/wasm-sdk-build
cmake --install out/wasm-sdk-build --prefix out/wasm-sdk
```

然后将安装目录传给文档项目的 `-SdkRoot`，同时用 `-FleckCompiler` 指向匹配的桌面 SDK 工具。[安装式 SDK](../../core/docs/sdk.md)

## 验证

构建目标 `check_pages` 使用 SDK schema 核对组件覆盖，并编译每个 Fleck 页面。页面运行后可用 Edge 与 playwright-core 检查实际浏览器交互和子目录部署。

```powershell
node tests/documentation/browser.cjs http://127.0.0.1:8790/ out/documentation/evidence
node tests/documentation/deployment.cjs http://127.0.0.1:8791/nested/guide/
node tests/development/check-doc-links.js
```

浏览器用例读取 `SUNFLECK_BROWSER_TEST_MODULES`。可将它设为包含 playwright-core 的依赖目录，例如安装过依赖的 `tests/web`。
