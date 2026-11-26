# Windows：VS Code 三语言开发

仓库内可从计数器和组件展厅开始。下面的 C# 计数器夹具还演示 Fleck 页面刷新、对象树与调试配置。[示例入口](../../samples/README.md)

## 独立 Demos

仓库外的 `Demos` 目录包含 `c`、`cpp` 和 `csharp` 三个独立项目，各自附带 SDK、语言服务、Fleck 页面、模板及 F5 配置。打开已有项目的工作区，或在其目录运行：

```powershell
pwsh -NoProfile -File ./start.ps1 -Action Open
```

`Open` 先构建再打开项目，`Run` 启动，`Test` 执行无窗口验证。使用已安装的 Sunfleck 扩展，在 `main.fleck` 中可用 F12 定位控件类型、`Value` 和 `Score`。换机验证时一并迁移这三个项目，再使用目标平台的 SDK 重建。

## 准备与打开项目

需要 Windows、Visual Studio C++ 桌面工具（MSVC、Windows SDK）、CMake 3.25+、Ninja、支持 .NET 10 的 SDK、Node.js、PowerShell 7 和 VS Code。F5 使用官方 C# 扩展 `ms-dotnettools.csharp`，安装 C# Dev Kit 时也会安装它。

在仓库根目录执行：

```powershell
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter/start.ps1
```

脚本在基础构建缺失时准备 `out/windows-dev` 中的原生库和语言服务，构建计数器，然后通过扩展开发模式加载仓库中的 Sunfleck Fleck。无需先安装 VSIX。资源管理器顶层应显示 **COUNTER**，F5 配置应显示 **简单计数器**。

已安装 Sunfleck Fleck 扩展且已准备基础构建时，也可以用“文件 → 打开文件夹”打开 `tests/development/fixtures/legacy-demos/counter`。项目自己的 `.vscode` 包含启动、任务和语言配置；不需要打开整个仓库。

修改框架原生代码、C# 绑定或语言服务后，先结束使用这些库的调试应用和服务，再更新基础构建：

```powershell
pwsh -NoProfile -File tools/development/windows-dev.ps1 -Action Prepare
```

计数器夹具的详细操作见 [C# 计数器](../../core/tests/development/fixtures/legacy-demos/counter/README.md)和 [C++ 计数器](../../core/tests/development/fixtures/legacy-demos/counter-cpp/README.md)。

## 提示、格式化和页面更新

打开 `main.fleck`，按 `Ctrl+Space` 查看建议，`Shift+Alt+F` 格式化，`Ctrl+S` 保存时也会格式化。模型提示来自构建生成的元数据；新增 C# 属性后重新构建项目。

按 **F5** 启动“简单计数器”。运行时修改 `main.fleck` 的文字或布局并保存，应用会更新同一窗口；无效页面保留上一次成功界面，修正并保存后恢复。调试器停在断点时 UI 线程停止，继续运行后才处理页面更新。这是运行中的应用刷新，静态设计预览尚未提供。

只运行应用可在仓库根目录执行：

```powershell
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter/start.ps1 -Action Run
```

## 打开对象树与定位元素

F5 配置通过 `SUNFLECK_DEBUG_PIPE` 传入固定名称，框架创建 App 时自动开启 Named Pipe，扩展自动连接，业务代码无需调试接入语句。Run／Watch 脚本也配置相同连接。在 VS Code 左侧“资源管理器”中展开 **Sunfleck 对象树**；找不到时按 `Ctrl+Shift+P`，执行 **Sunfleck: 连接调试应用**。检查器直接读取 App 内存数据。

App 未启动时显示等待连接；启动后显示树，停止后可等待下一次 F5 自动重连。选择对象可查看详情和定位源码，也可从面板启动拾取。查看连接错误时打开“输出”，选择 Sunfleck 通道。配置与协议见 [开发工具接入](devtools.md#绑定诊断与快照)。

## 可选的 C# 热重载

先结束 F5，再从“终端 → 运行任务”选择 **C# 热重载**，或执行：

```powershell
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter/start.ps1 -Action Watch
```

修改 `CounterModel.Caption` 的字符串可观察兼容 Getter 更新；修改命令方法体会在后续调用中生效。Fleck 保存刷新仍可使用。无法原地应用的代码改动按终端提示重启；必要时 `Ctrl+C` 停止后重新运行，业务状态是否保留由应用负责。

Watch 使用 `bin/Watch` 和 embedded PDB，F5 使用 `bin/Debug` 和 portable PDB。SDK 版本和回退复现方法见 [portable PDB 回退](devtools.md#portable-pdb-回退)。
