# DocumentView 文档阅读组件

DocumentView 使用 Sunfleck 布局、字体与绘制命令显示构建期转换的文档数据。C、C++、C# 共用同一实现。

## Fleck 与 C API

```xml
<Window xmlns="urn:sunfleck:v1"><DocumentView Id="article"/></Window>
```

通过 `ui_app_find_fleck_node` 获取节点，再调用 `ui_document_load` 载入 UTF-8 JSON。数据复制后由节点持有；解析失败保留旧文档。文档加载后会在下一次绘制时排版。

```c
const char data[] = "{\"version\":1,\"blocks\":[{\"kind\":\"paragraph\",\"spans\":[{\"text\":\"Hello\"}]}]}";
ui_document_load(article, data, sizeof(data) - 1);
```

## 数据与交互契约

文档格式 `version: 1`，`blocks` 支持 heading、paragraph、quote、list、code、table、rule、image。每个块包含 spans；span 提供 text、可选 href、flags（1 粗体、2 斜体、4 代码字体）和高亮类别 color。标题使用唯一 id 与 level。代码块保存 language 与 raw；图片采用构建期解码的 RGB 行。

`ui_document_scroll_to` 定位标题；`ui_document_get_scroll` 与 `ui_document_set_scroll` 读写设备像素滚动位置。`ui_document_selection` 使用先查询长度、再读取缓冲区的两次调用方式返回 UTF-8。`ui_document_select_all` 选择完成排版的全文。

`ui_document_take_event` 返回 Link 或 Copy 事件及文本；查询所需大小不消费事件，成功写入缓冲区后消费。应用负责执行浏览器导航或剪贴板操作，组件不执行 HTML、脚本或远程资源。

所有 API 使用 App 所属 UI 线程；页面销毁后不得继续使用原生借用句柄。C# Node 使用现有刷新版本检查拒绝已替换节点。

## C++ 与 C#

C++ Node 提供 loadDocument、scrollToHeading、documentScroll、setDocumentScroll、documentSelection、selectDocument 、scrollDocumentHorizontal 和 takeDocumentEvent。

C# Node 提供 LoadDocument、ScrollToHeading、DocumentScroll、DocumentSelection、SelectDocument 、ScrollDocumentHorizontal 和 TakeDocumentEvent；创建节点时使用 NodeKind.DocumentView。

## 字体与渲染

Windows 原生与 Windows Headless 使用 DirectWrite，继承节点的 FontFamily、FontSize、FontWeight 等文字属性。应用内存字体使用 `ui_app_register_font`，C++ `App::registerFont` 或 C# `App.RegisterFont` 注册，再通过 FontFamily 选择；注册、注销及字体属性变化会使文档排版缓存失效。详见[字体与文字排版](fonts.md)。本站提供 Noto 字体与许可。

Windows 标题、正文、列表、引用和代码采用整段塑形；表格由制表符分列、换行符分行，每个单元格独立塑形，跨 Span 的组合字符仍属于同一个文字簇。单元格样式参与列宽和行高计算，末行不要求结尾换行。表格保留原列顺序，各单元格内部按 FlowDirection 排版。选区、链接命中及双向文字的点击位置使用相同的文字簇几何；复制保留原始制表符和换行。

组件按宽度、DPI、文字属性和字体资源版本缓存排版，只栅格化可见区域。标题、选区和链接命中共用布局坐标；代码与表格通过 `ui_document_scroll_horizontal` 局部横向滚动。

FreeType 后端仍可用 `ui_document_set_font`（C++ `setDocumentFont`、C# `DocumentFonts.Register`）在创建 App 前注册复制的字体数据，role 0 为正文，role 1 为代码，role 2 为符号回退。Linux 与 Web 的文字后端和塑形范围见 [字体与文字服务](fonts.md)。DocumentView 提供阅读、选择和复制，完整读屏仍需验收。
