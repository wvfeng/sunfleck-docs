"""Compose the project's Fleck pages and check them against the installed SDK."""
import copy
from itertools import count
import json
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as xml
from pathlib import Path
from api_zh import DESCRIPTIONS, TYPES

ROOT = Path(__file__).resolve().parents[1]
FLECK = "{urn:sunfleck:v1}"
CATEGORIES = {
    "basic": "基础与文本",
    "layout": "布局",
    "input": "表单与输入",
    "data": "数据展示",
    "navigation": "导航",
    "feedback": "反馈与浮层",
    "drawing": "绘图",
    "dock": "停靠工作区",
}


def read(path):
    node = xml.parse(path).getroot()
    for child in node.iter():
        child.tag = child.tag.removeprefix(FLECK)
    return node


def markup(node):
    return xml.tostring(node, encoding="unicode", short_empty_elements=True)


def element(tag, **attrs):
    return xml.Element(tag, {key: str(value) for key, value in attrs.items()})


def label(text, **attrs):
    return element("TextBlock", Text=text, TextWrapping="Wrap", **attrs)


def component(name, replacements, **slots):
    node = read(ROOT / "components" / f"{name}.fleck")
    for parent in list(node.iter()):
        for index, child in reversed(list(enumerate(parent))):
            if child.tag == "Slot":
                parent.remove(child)
                parent.insert(index, slots[child.attrib["Name"]])
        for key, value in parent.attrib.items():
            if value in replacements:
                parent.set(key, replacements[value])
            elif value.endswith("-KEY"):
                parent.set(key, value.replace("KEY", replacements["KEY"]))
    return node


def code_block(key, title, source):
    return component("CodeBlock", {"KEY": key, "LABEL": title, "SOURCE": source.strip()})


def api_row(value, owner, description=None):
    name = value["name"]
    default = value.get("defaultValue", "")
    if default is None:
        default = "空"
    elif isinstance(default, bool):
        default = "真（true）" if default else "假（false）"
    elif not isinstance(default, str):
        default = json.dumps(default, ensure_ascii=False)
    custom_description = description
    description = description or DESCRIPTIONS.get(name)
    if not description or not re.search(r"[\u3400-\u9fff]", description):
        raise ValueError(f"{owner}.{name}: missing Chinese description")
    if not custom_description and not value.get("description") and value["type"] not in ("bool", "model"):
        options = value.get("values", [])
        if options:
            description += "可用值为 " + "、".join(options) + "。"
    return (name, TYPES[value["type"]], default or "—", description, owner)


def tab_strip(tabs):
    strip = element("WrapPanel", Spacing=8, Height=36)
    for key, caption in tabs:
        strip.append(component("Tab", {"KEY": key, "LABEL": caption}))
    return strip


def section(body, tabs, title, description, group):
    body.append(label(group, FontSize=14, Height=28, Foreground="#0D7896", FontWeight="SemiBold"))
    body.append(label(title, FontSize=38, FontWeight="Bold", Foreground="#112F45"))
    body.append(label(description, FontSize=16, Foreground="#506A7E"))
    body.append(tab_strip(tabs))


def selectable_text(node, indices):
    if node.get("Id") == "example-preview" or node.tag in ("Button", "ListView"):
        return
    if node.tag == "TextBlock":
        if not node.get("Id"):
            node.set("Id", f"selectable-{next(indices)}")
        return
    for child in node:
        selectable_text(child, indices)


def component_page(name, source, schema, category, base_attributes, parent):
    title = source.attrib["Title"]
    summary, demo, guide, descriptions = source.find("StackPanel")
    assert [item.get("Id") for item in (summary, demo, guide, descriptions)] == ["summary", "demo", "guide", "api"]
    body = element("StackPanel", Spacing=18)
    section(body, [("example", "示例"), ("api", "API"), ("guide", "指南")],
            f"{name} {title}", summary.attrib["Text"], category)

    live_demo = copy.deepcopy(demo)
    live_demo.attrib.pop("Id")
    if len(live_demo) == 1:
        live_demo = live_demo[0]
    snippet = copy.deepcopy(live_demo)
    xml.indent(snippet, space="  ")
    example = component("ExampleCard", {}, Demo=live_demo,
                        Source=code_block("demo", "Fleck 示例", markup(snippet)))
    example.set("Id", "panel-example")
    body.append(example)

    props = {row.attrib["Id"]: row.attrib["Text"] for row in descriptions}
    attributes = {attr["name"]: attr for attr in schema["attributes"] if attr["name"] != "Icon"}
    if props.keys() - attributes.keys():
        raise ValueError(f"{name}: unknown properties {sorted(props.keys() - attributes.keys())}")
    if not any(node.tag == name for node in live_demo.iter()):
        raise ValueError(f"{name}: example must contain the component")
    api = element("StackPanel", Id="panel-api", Visibility="Collapsed", Spacing=6)
    api.append(label(f"{name} 属性", FontSize=22, FontWeight="Bold"))
    api.append(tab_strip([("api-component", "组件属性"), ("api-common", "通用属性")]))
    rows = [api_row(attributes[attr], name, props.get(attr))
            for attr in dict.fromkeys((*props, *attributes))
            if attr not in base_attributes or attributes[attr] != base_attributes[attr]]
    if rows:
        specific = component("ApiList", {"PANEL": "panel-api-component", "LIST": "api-component-rows",
                                          "ROWS": "{Binding ComponentRows}", "HEIGHT": str(min(len(rows), 8) * 36 + 8)})
    else:
        specific = element("StackPanel", Id="panel-api-component", Spacing=4)
        specific.append(label("此组件的属性均来自通用属性。"))
    common = component("ApiList", {"PANEL": "panel-api-common", "LIST": "api-common-rows",
                                   "ROWS": "{Binding InheritedRows}", "HEIGHT": "296"})
    common.set("Visibility", "Collapsed")
    api.append(label("{Binding PropertyDetail}", FontSize=14, MinHeight=26))
    api.extend((specific, common))
    children = schema.get("children", [])
    if children and all(isinstance(value, str) for value in children):
        api.append(label("允许的子元素为 " + "、".join(children)))
    body.append(api)
    guide.set("Id", "panel-guide")
    guide.set("Visibility", "Collapsed")
    guide.insert(0, label("使用指南", FontWeight="Bold", FontSize=22))
    body.append(guide)
    return body, rows, parent


def tutorials():
    page = read(ROOT / "content/quickstart.fleck")
    body = page.find("StackPanel")
    links = json.loads((ROOT / "sdk-links.json").read_text(encoding="utf-8"))
    languages = {"c": "C", "cpp": "C++", "csharp": "C#"}
    if links.keys() != languages.keys():
        raise ValueError("sdk-links.json must contain c, cpp, and csharp")
    slot = body.find("Slot")
    body.insert(list(body).index(slot), tab_strip(languages.items()))
    body.remove(slot)
    for key, language in languages.items():
        tutorial = read(ROOT / f"content/quickstart/{key}.fleck")
        panel = tutorial.find("StackPanel")
        panel.set("Id", f"panel-{key}")
        if key != "c":
            panel.set("Visibility", "Collapsed")
        panel.insert(1, element("Hyperlink", Id=f"sdk-{key}", Text=f"{language} SDK 下载", Height=30,
                                IsEnabled="true" if links[key] else "false"))
        for mode in panel.findall("StackPanel"):
            for index, source in reversed(list(enumerate(mode))):
                if source.tag == "TextArea":
                    filename = source.attrib["Id"].partition("-")[2]
                    mode.remove(source)
                    mode.insert(index, code_block(f"{mode.attrib['Id']}-{index}", filename, source.attrib["Text"]))
        body.append(panel)
    return body, links


def catalog(schema):
    builtins = {item["name"]: item for item in schema["elements"]}
    attributes = [{attr["name"]: attr for attr in item["attributes"]} for item in builtins.values()]
    visual = {key: value for key, value in attributes[0].items()
              if all(item.get(key) == value for item in attributes)}
    layout = {**visual, **{key: attributes[0][key] for key in ("Spacing", "Padding")}}
    overview = read(ROOT / "content/overview.fleck").find("StackPanel")
    slot = overview.find("Slot")
    overview.remove(slot)
    quick, links = tutorials()
    pages = [("overview", "组件总览", "开始", overview, [], ""),
             ("quick", "快速开始", "开始", quick, [], "")]
    covered = set()
    for directory, category in CATEGORIES.items():
        box = element("StackPanel", Spacing=8)
        box.append(label(category, FontSize=22, FontWeight="SemiBold", Foreground="#112F45"))
        entries = element("WrapPanel", Spacing=8)
        for source in sorted((ROOT / "content/components" / directory).glob("*.fleck")):
            name = source.stem
            if name in covered or name not in builtins:
                raise ValueError(f"Invalid component page {source}")
            covered.add(name)
            page = read(source)
            has_layout = any(attr["name"] == "Padding" for attr in builtins[name]["attributes"])
            base = layout if has_layout else visual
            parent = "LayoutElement" if has_layout else "VisualElement"
            pages.append((name, f"{name} {page.attrib['Title']}", category,
                          *component_page(name, page, builtins[name], category, base, parent)))
            entries.append(element("Button", Id=f"catalog-{name}", Text=f"{name} {page.attrib['Title']}", Width=174, Height=36))
        box.append(entries)
        overview.append(box)
    if builtins.keys() - covered:
        raise ValueError(f"Missing component pages: {sorted(builtins.keys() - covered)}")
    missing = read(ROOT / "content/not-found.fleck").find("StackPanel")
    pages.append(("not-found", "页面不存在", "", missing, [], ""))
    return pages, links, [api_row(value, "VisualElement") for value in visual.values()], [
        api_row(layout[key], "LayoutElement") for key in ("Spacing", "Padding")]


def cpp_text(text):
    if ')FLECK"' in text:
        raise ValueError("Unexpected C++ raw string delimiter")
    return f'R"FLECK({text})FLECK"'


def cpp_rows(name, rows):
    return [f"constexpr ApiProperty {name}[] = {{",
            *("    {{" + ", ".join(map(cpp_text, row)) + "}}," for row in rows), "};"]


def build(fleckc, output):
    with tempfile.TemporaryDirectory(dir=output.parent) as scratch:
        folder = Path(scratch)
        schema_file = folder / "schema.json"
        subprocess.run([fleckc, "--schema", schema_file], check=True)
        pages, links, visual, layout = catalog(json.loads(schema_file.read_text(encoding="utf-8")))
        shell = (ROOT / "shell.fleck").read_text(encoding="utf-8")
        button_template = ROOT / "components/NavigationButton.fleck"
        generated = [f"constexpr auto shell = {cpp_text(shell)};",
                     f"constexpr std::string_view navigation_button = {cpp_text(button_template.read_text(encoding='utf-8'))};",
                     *cpp_rows("visual_properties", visual), *cpp_rows("layout_properties", layout)]
        for route, _, _, _, rows, _ in pages:
            if rows:
                generated.extend(cpp_rows(f"properties_{route}", rows))
        generated.append("constexpr Page pages[] = {")
        for route, title, group, body, rows, parent in pages:
            selectable_text(body, count())
            source = markup(body)
            page_file = folder / f"{route}.fleck"
            page_file.write_text(shell.replace("<!--content-->", source), encoding="utf-8")
            subprocess.run([fleckc, "--format", "binary", "--resource", button_template,
                            page_file, folder / "page.flkb"], check=True)
            ids = [node.attrib["Id"] for node in body.iter() if "Id" in node.attrib]
            if len(ids) != len(set(ids)) or any(not re.fullmatch(r"[\w.-]+", value) for value in ids):
                raise ValueError(f"{route}: invalid or duplicate Id")
            components = (route, title, group, source, " ".join(ids))
            row_data = f"properties_{route}" if rows else "{}"
            generated.append("    {" + ", ".join(map(cpp_text, components)) + f", {row_data}, {cpp_text(parent)}" + "},")
        generated.append("};")
        for language, link in links.items():
            if link and not link.startswith("https://"):
                raise ValueError(f"{language}: SDK download must use HTTPS")
            generated.append(f"constexpr auto sdk_{language} = {cpp_text(link)};")
        output.write_text("\n".join(generated) + "\n", encoding="utf-8")


if __name__ == "__main__":
    build(sys.argv[1], Path(sys.argv[2]))
