namespace docs {
struct ApiProperty { std::array<const char*, 5> fields; };
struct Page {
    std::string_view id, title, group, content, ids;
    std::span<const ApiProperty> properties;
    std::string_view parent_type;
};
#include "pages.inc"

constexpr char sample_key[] = "blue-marble";
std::vector<uint8_t> sample_pixels;
unsigned sample_width = 0, sample_height = 0;

struct PageModel {
    ui_model* data = nullptr;
    ui_text_source* detail = nullptr;
    ui_text_source* image = nullptr;
};

struct ApiModels {
    ui_app* owner;
    std::map<const ApiProperty*, ui_model*> rows;
    std::map<const Page*, PageModel> pages;
    ui_model* visual = nullptr;
    ui_model* layout = nullptr;

    ui_result collection(ui_model*& model, std::span<const ApiProperty> own,
                         std::span<const ApiProperty> inherited = {})
    {
        std::vector<const char*> labels;
        std::vector<ui_model*> contexts;
        for (const auto group : {inherited, own}) {
            for (const auto& row : group) {
                if (!rows.contains(&row)) {
                    ui_model* item = nullptr;
                    if (const auto status = ui_model_create(owner, &item); status != UI_OK) { return status; }
                    constexpr const char* names[] = {"Name", "Type", "Default", "Description"};
                    for (size_t i = 0; i < std::size(names); ++i) {
                        const ui_model_property property{sizeof(property), 1, names[i], UI_VALUE_STRING,
                            [](void* context, ui_binding_value* value) {
                                value->kind = UI_VALUE_STRING;
                                value->string = *static_cast<const char* const*>(context);
                                return UI_OK;
                            }, nullptr, const_cast<const char**>(&row.fields[i])};
                        if (const auto status = ui_model_add_property(item, &property); status != UI_OK) { return status; }
                    }
                    rows.emplace(&row, item);
                }
                labels.push_back(row.fields[0]);
                contexts.push_back(rows.at(&row));
            }
        }
        ui_model* list = nullptr;
        if (const auto status = ui_model_create(owner, &list); status != UI_OK) { return status; }
        if (const auto status = ui_model_enable_text_collection(list); status != UI_OK) { return status; }
        if (!labels.empty()) {
            if (const auto status = ui_collection_splice_models(list, 0, 0, labels.data(), contexts.data(),
                static_cast<uint32_t>(labels.size())); status != UI_OK) { return status; }
        }
        model = list;
        return UI_OK;
    }

    ui_result for_page(const Page& page, PageModel& model)
    {
        if (page.parent_type.empty()) { return UI_OK; }
        if (const auto found = pages.find(&page); found != pages.end()) { model = found->second; return UI_OK; }
        if (!visual) {
            if (const auto status = collection(visual, visual_properties); status != UI_OK) { return status; }
        }
        if (page.parent_type == "LayoutElement" && !layout) {
            if (const auto status = collection(layout, layout_properties, visual_properties); status != UI_OK) { return status; }
        }
        ui_model* own = nullptr;
        if (const auto status = collection(own, page.properties); status != UI_OK) { return status; }
        if (const auto status = ui_model_create(owner, &model.data); status != UI_OK) { return status; }
        const auto add_collection = [&](const char* path, ui_model* list) {
            const ui_model_property property{sizeof(property), 1, path, UI_VALUE_MODEL,
                [](void* context, ui_binding_value* value) {
                    value->kind = UI_VALUE_MODEL;
                    value->model = static_cast<ui_model*>(context);
                    return UI_OK;
                }, nullptr, list};
            return ui_model_add_property(model.data, &property);
        };
        if (const auto status = add_collection("ComponentRows", own); status != UI_OK) { return status; }
        if (const auto status = add_collection("InheritedRows", page.parent_type == "LayoutElement" ? layout : visual);
            status != UI_OK) { return status; }
        if (const auto status = ui_text_source_create(owner, "", &model.detail); status != UI_OK) { return status; }
        if (const auto status = ui_model_add_text_source(model.data, "PropertyDetail", model.detail); status != UI_OK) { return status; }
        if (page.id == "Image") {
            if (const auto status = ui_text_source_create(owner, sample_key, &model.image); status != UI_OK) { return status; }
            if (const auto status = ui_model_add_text_source(model.data, "SampleImage", model.image); status != UI_OK) { return status; }
        }
        pages.emplace(&page, model);
        return UI_OK;
    }
};

std::unique_ptr<ApiModels> api_models;
PageModel current_model;
struct Action { std::string id; };
std::deque<Action> actions;
std::deque<std::pair<std::string, uint32_t>> activations;
std::string pending_action, action_value, query;
const Page* current = nullptr;
std::string tab = "example", language = "c";
bool common_api = false;
bool use_fleck = true, menu = false;
unsigned logical_width = 1280;
unsigned render_mode = 0;
std::optional<double> navigation_offset;

ui_node* find(const std::string& id)
{
    ui_node* node = nullptr;
    ui_app_find_fleck_node(app, id.c_str(), &node);
    return node;
}

void visible(ui_node* node, bool show)
{
    if (node) {
        ui_binding_value value{UI_VALUE_ENUM};
        value.integer = show ? UI_VISIBLE : UI_COLLAPSED;
        ui_node_set_property(node, UI_PROPERTY_VISIBILITY, &value);
    }
}

void enabled(ui_node* node, bool enable)
{
    if (node) {
        ui_binding_value value{UI_VALUE_BOOL};
        value.integer = enable;
        ui_node_set_property(node, UI_PROPERTY_IS_ENABLED, &value);
    }
}

std::string node_text(ui_node* node)
{
    unsigned size = 0;
    if (!node || ui_node_get_text(node, nullptr, &size) != UI_OK || size == 0) { return {}; }
    std::string result(size, '\0');
    if (ui_node_get_text(node, result.data(), &size) != UI_OK) { return {}; }
    result.resize(size - 1);
    return result;
}

std::string lower(std::string text)
{
    for (auto& ch : text) {
        if (ch >= 'A' && ch <= 'Z') { ch = char(ch - 'A' + 'a'); }
    }
    return text;
}

std::string escape(std::string_view text)
{
    std::string out;
    for (auto ch : text) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '"': out += "&quot;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::string navigation(const Page& selected)
{
    std::string markup;
    std::string_view group;
    unsigned group_index = 0;
    for (const auto& page : pages) {
        if (page.group.empty()) { continue; }
        if (group != page.group) {
            if (!group.empty()) { markup += "</StackPanel>"; }
            group = page.group;
            markup += "<StackPanel Id=\"group-" + std::to_string(group_index++) +
                "\" Spacing=\"4\"><TextBlock Text=\"" + escape(group) +
                "\" Height=\"28\" FontSize=\"13\" FontWeight=\"SemiBold\" Foreground=\"#45677D\"/>";
        }
        markup += "<Border Id=\"nav-row-" + std::string(page.id) + "\" MinHeight=\"34\" CornerRadius=\"5\"";
        if (&selected == &page) { markup += " Background=\"#DCF1F7\""; }
        const auto caption = escape(page.title);
        markup += "><Button Id=\"nav-" + std::string(page.id) + "\" Template=\"docs:NavigationButton\" Content=\"" +
            caption + "\"><ContentControl Content=\"" + caption +
            "\" Foreground=\"#164C65\" Margin=\"6,3\" TextWrapping=\"Wrap\" LineHeight=\"20\" VerticalAlignment=\"Center\"/></Button></Border>";
    }
    markup += "</StackPanel>";
    return markup;
}

void layout()
{
    const bool narrow = logical_width < 760;
    visible(find("site-menu"), narrow);
    visible(find("site-sidebar"), !narrow || menu);
    visible(find("site-main"), !narrow || !menu);
}

void filter_navigation()
{
    const auto next = lower(node_text(find("site-search")));
    if (next == query) { return; }
    query = next;
    std::string_view group;
    unsigned index = 0;
    bool any = false;
    for (const auto& page : pages) {
        if (page.group.empty()) { continue; }
        if (page.group != group) {
            if (!group.empty()) { visible(find("group-" + std::to_string(index++)), any); }
            any = false;
            group = page.group;
        }
        const bool match = lower(std::string(page.title)).find(query) != std::string::npos ||
            std::string(page.group).find(query) != std::string::npos;
        visible(find("nav-row-" + std::string(page.id)), match);
        any = any || match;
    }
    visible(find("group-" + std::to_string(index)), any);
}

void mark_tab(const std::string& key, bool selected)
{
    if (auto* frame = find("tab-frame-" + key)) {
        ui_binding_value value{UI_VALUE_COLOR};
        value.integer = selected ? 0xffcdeaf3 : 0xffeaf0f4;
        ui_node_set_property(frame, UI_PROPERTY_BACKGROUND, &value);
    }
}

void select_tab()
{
    for (std::string_view key : {"example", "api", "guide"}) {
        visible(find("panel-" + std::string(key)), key == tab);
        mark_tab(std::string(key), key == tab);
    }
}

void select_api_group()
{
    visible(find("panel-api-component"), !common_api);
    visible(find("panel-api-common"), common_api);
    mark_tab("api-component", !common_api);
    mark_tab("api-common", common_api);
}

void show_api_property(uint32_t index)
{
    const auto& own = current->properties;
    const bool has_layout = current->parent_type == "LayoutElement";
    const ApiProperty* row = nullptr;
    if (!common_api) {
        if (index < own.size()) { row = &own[index]; }
    } else if (index < std::size(visual_properties)) {
        row = &visual_properties[index];
    } else if (has_layout && index - std::size(visual_properties) < std::size(layout_properties)) {
        row = &layout_properties[index - std::size(visual_properties)];
    }
    if (!current_model.detail) { return; }
    if (!row) { ui_text_source_set(current_model.detail, ""); return; }
    const auto& [name, type, fallback, description, owner] = row->fields;
    const auto detail = std::string(name) + " · " + type + " · 默认值 " + fallback + " · " + owner + "\n" + description;
    ui_text_source_set(current_model.detail, detail.c_str());
}

void select_language()
{
    for (std::string_view key : {"c", "cpp", "csharp"}) {
        std::string name(key);
        visible(find("panel-" + name), key == language);
        mark_tab(name, key == language);
        visible(find(name + "-fleck"), use_fleck);
        visible(find(name + "-code"), !use_fleck);
    }
}

void queue_action(ui_node* node, void* context)
{
    const auto& id = static_cast<Action*>(context)->id;
    uint32_t value = 0;
    ui_node_get_value(node, &value);
    activations.emplace_back(id, value);
}

void handle_action(const std::string& id, uint32_t value)
{
    const std::string_view key(id);
    if (key.starts_with("nav-") || key.starts_with("open-") || key.starts_with("catalog-")) {
        pending_action = "navigate";
        action_value = id.substr(id.find('-') + 1);
    } else if (key.starts_with("copy-")) {
        pending_action = "copy";
        action_value = node_text(find("code-" + id.substr(5)));
    } else if (key.starts_with("sdk-")) {
        const std::string_view url = key == "sdk-c" ? sdk_c : key == "sdk-cpp" ? sdk_cpp : sdk_csharp;
        if (!url.empty()) { pending_action = "external"; action_value = url; }
    } else if (key == "tab-api-component" || key == "tab-api-common") {
        common_api = key == "tab-api-common";
        select_api_group();
        uint32_t index = 0;
        if (auto* list = find(common_api ? "api-common-rows" : "api-component-rows")) {
            ui_node_get_value(list, &index);
        }
        show_api_property(index);
    } else if (key == "api-component-rows" || key == "api-common-rows") {
        show_api_property(value);
    } else if (key == "demo-image-toggle" && current_model.image) {
        ui_text_source_set(current_model.image, value ? "" : sample_key);
        ui_node_set_text(find(id), value ? "显示图片" : "隐藏图片");
    } else if (key.starts_with("tab-")) {
        if (current->id == "quick") {
            language = key.substr(4);
            select_language();
        } else {
            tab = key.substr(4);
            select_tab();
        }
    } else if (id == "site-fleck") {
        use_fleck = value != 0;
        select_language();
    } else if (id == "site-menu") {
        menu = !menu;
        layout();
    } else if (id == "site-registration") {
        pending_action = "external";
        action_value = "https://beian.miit.gov.cn/";
    } else if (id == "demo-button") {
        ui_node_set_text(find("demo-button-result"), "已确认");
    } else if (id == "demo-disabled") {
        enabled(find("demo-button"), value == 0);
    } else if (id == "demo-open" || id == "demo-close") {
        auto* popup = find("demo-popup");
        uint32_t state = 0;
        if (popup) {
            ui_node_get_value(popup, &state);
            ui_node_set_value(popup, id == "demo-open" && !state);
        }
    }
}

void bind(const std::string& id, const std::string& action)
{
    if (auto* node = find(id)) {
        actions.push_back({action});
        ui_node_set_action(node, queue_action, &actions.back());
    }
}

void initialize_demo()
{
    if (current_model.image) { ui_text_source_set(current_model.image, sample_key); }
    if (auto* document = find("demo-document")) {
        constexpr std::string_view sample = R"({"version":1,"blocks":[{"kind":"heading","level":2,"spans":[{"text":"阅读示例"}]},{"kind":"paragraph","spans":[{"text":"拖动鼠标选择这一段文字。"}]}]})";
        ui_document_load(document, sample.data(), sample.size());
    }
}

int open(std::string_view route)
{
    double left = 0, top = 0;
    const bool restore_navigation = app &&
        ui_scroll_get_offset(find("site-nav-scroll"), &left, &top) == UI_OK;
    auto selected = std::find_if(std::begin(pages), std::end(pages), [&](const Page& page) { return page.id == route; });
    if (selected == std::end(pages)) { selected = std::end(pages) - 1; }
    std::string source(shell);
    source.replace(source.find("<!--navigation-->"), sizeof("<!--navigation-->") - 1, navigation(*selected));
    source.replace(source.find("<!--content-->"), sizeof("<!--content-->") - 1, selected->content);
    ui_fleck_diagnostic diagnostic{};
    ui_result result;
    PageModel model;
    // Each Dock example owns its workspace. A new app gives it a fresh host and
    // retires any floating views from the previous example.
    if (!current || current->group == "停靠工作区" || selected->group == "停靠工作区") {
        const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
        ui_app* next = nullptr;
        result = ui_app_create(&options, &next);
        auto next_models = std::make_unique<ApiModels>(ApiModels{next});
        if (result == UI_OK && !sample_pixels.empty()) {
            result = ui_app_register_image_rgba(next, sample_key, sample_pixels.data(), sample_width, sample_height);
        }
        if (result == UI_OK) { result = next_models->for_page(*selected, model); }
        if (result == UI_OK) {
            ui_app_set_builtin_theme(next, UI_BUILTIN_RADIX, 0, 0);
            const ui_fleck_resource_entry appearance{
                "NavigationButton.fleck", navigation_button.data(), navigation_button.size()
            };
            result = ui_app_load_fleck_with_resources(next, source.data(), source.size(), &appearance, 1, model.data, &diagnostic);
        }
        if (result == UI_OK) { result = ui_app_open_window(next); }
        if (result == UI_OK) { result = ui_host_set_render_mode(next, render_mode); }
        if (result == UI_OK) {
            if (app) { ui_app_destroy(app); }
            app = next;
            api_models = std::move(next_models);
        } else if (next) {
            ui_app_destroy(next);
        }
    } else {
        ui_host_select_view(app, 0);
        result = api_models->for_page(*selected, model);
        if (result == UI_OK) { result = ui_app_refresh_fleck(app, source.data(), source.size(), model.data, &diagnostic); }
    }
    if (result != UI_OK) {
        if (diagnostic.message[0]) { std::fprintf(stderr, "%s: %s\n", selected->id.data(), diagnostic.message); }
        return result;
    }
    current = selected;
    current_model = model;
    activations.clear();
    actions.clear();
    bind("site-menu", "site-menu");
    bind("site-home", "open-overview");
    bind("site-quick", "open-quick");
    bind("site-registration", "site-registration");
    for (const auto& page : pages) {
        if (!page.group.empty()) {
            const auto id = "nav-" + std::string(page.id);
            bind(id, id);
        }
    }
    for (auto ids = selected->ids; !ids.empty();) {
        const auto end = ids.find(' ');
        const auto key = ids.substr(0, end);
        if (key.starts_with("selectable-") || key.starts_with("code-")) {
            ui_host_set_text_selectable(find(std::string(key)), 1);
        }
        if (key.starts_with("copy-") || key.starts_with("open-") || key.starts_with("catalog-") ||
            (key.starts_with("tab-") && !key.starts_with("tab-frame-")) || key.starts_with("sdk-") ||
            key == "site-fleck" || key == "demo-button" || key == "demo-open" ||
            key == "demo-close" || key == "demo-disabled" || key == "demo-image-toggle" ||
            key == "api-component-rows" || key == "api-common-rows") {
            const std::string id(key);
            bind(id, id);
        }
        if (end == std::string_view::npos) { break; }
        ids.remove_prefix(end + 1);
    }
    query.clear();
    ui_node_set_text(find("site-search"), "");
    if (current->id == "quick") {
        ui_node_set_value(find("site-fleck"), use_fleck);
        select_language();
    } else {
        tab = "example";
        common_api = false;
        select_tab();
        select_api_group();
        show_api_property(0);
    }
    initialize_demo();
    menu = false;
    layout();
    if (restore_navigation) { navigation_offset = top; }
    ui_scroll_to(find("site-main"), 0, 0);
    return UI_OK;
}
}

extern "C" {
EMSCRIPTEN_KEEPALIVE int sf_sample_image(const uint8_t* rgba, unsigned width, unsigned height)
{
    if (!rgba || !width || !height || width > 512 || height > 512) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        docs::sample_pixels.assign(rgba, rgba + size_t(width) * height * 4);
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
    docs::sample_width = width;
    docs::sample_height = height;
    return UI_OK;
}
EMSCRIPTEN_KEEPALIVE int sf_start(const char* route)
{
    if (app) { return UI_ERROR_INVALID_STATE; }
    return docs::open(route);
}
EMSCRIPTEN_KEEPALIVE int sf_navigate(const char* route) { return docs::open(route); }
EMSCRIPTEN_KEEPALIVE const char* sf_route() { return docs::current->id.data(); }
EMSCRIPTEN_KEEPALIVE const char* sf_title() { return docs::current->title.data(); }
EMSCRIPTEN_KEEPALIVE const char* sf_action() { return docs::pending_action.c_str(); }
EMSCRIPTEN_KEEPALIVE const char* sf_action_value()
{
    docs::pending_action.clear();
    return docs::action_value.c_str();
}
EMSCRIPTEN_KEEPALIVE int sf_tick()
{
    if (auto event = docs::activations.begin(); event != docs::activations.end()) {
        docs::handle_action(event->first, event->second);
        docs::activations.pop_front();
    }
    docs::filter_navigation();
    return ui_app_poll(app, 0);
}
EMSCRIPTEN_KEEPALIVE void sf_search_focus() { ui_node_focus(docs::find("site-search")); }
EMSCRIPTEN_KEEPALIVE int sf_font(const void* bytes, unsigned size) { return ui_document_set_font(1, bytes, size); }
}
