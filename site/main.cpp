#include <sunfleck/ui.h>
#include <emscripten.h>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <string_view>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <algorithm>
namespace { ui_app* app=nullptr; std::string text_state_json; }
#include "application.inc"
extern "C" {
EMSCRIPTEN_KEEPALIVE const unsigned char* sf_pixels() { uint32_t w, h; return ui_host_pixels(app, &w, &h); }
EMSCRIPTEN_KEEPALIVE int sf_render_mode(unsigned mode)
{
    const auto result = ui_host_set_render_mode(app, mode);
    if (result == UI_OK) { docs::render_mode = mode; }
    return result;
}
EMSCRIPTEN_KEEPALIVE const uint8_t* sf_draw_commands(uint32_t* size)
{
    const uint8_t* bytes = nullptr;
    if (ui_host_draw_commands(app, &bytes, size) != UI_OK) { *size = 0; return nullptr; }
    return bytes;
}
EMSCRIPTEN_KEEPALIVE int sf_select_view(unsigned id) { return ui_host_select_view(app, id); }
EMSCRIPTEN_KEEPALIVE int sf_semantic_action(const char* id, unsigned action) { return ui_host_semantic_action(app, std::strtoull(id, nullptr, 10), action); }
EMSCRIPTEN_KEEPALIVE const char* sf_snapshot()
{
    static std::string json;
    size_t size = 0;
    if (ui_host_snapshot(app, nullptr, &size) != UI_OK) { return "{\"views\":[]}"; }
    json.resize(size);
    if (ui_host_snapshot(app, json.data(), &size) != UI_OK || size <= 1) { return "{\"views\":[]}"; }
    return json.c_str();
}
EMSCRIPTEN_KEEPALIVE int sf_resize(unsigned w, unsigned h, unsigned dpi)
{
    const auto result = ui_host_resize(app, w, h, dpi);
    if (result != UI_OK) { return result; }
    docs::logical_width = w * 96 / std::max(1u, dpi);
    docs::layout();
    if (docs::navigation_offset) {
        ui_scroll_to(docs::find("site-nav-scroll"), 0, *docs::navigation_offset);
        docs::navigation_offset.reset();
    }
    return result;
}
EMSCRIPTEN_KEEPALIVE int sf_pointer(int action, int x, int y) { return ui_host_pointer(app, ui_pointer_action(action), x, y); }
EMSCRIPTEN_KEEPALIVE int sf_key(int key, int shift) { return ui_host_key_event(app, ui_host_key(key), shift); }
EMSCRIPTEN_KEEPALIVE int sf_focus(int value) { return ui_host_focus(app, value); }
EMSCRIPTEN_KEEPALIVE int sf_scroll(int x, int y, int dy) { return ui_host_scroll(app, x, y, dy); }
EMSCRIPTEN_KEEPALIVE int sf_context_menu(int x, int y) { return ui_host_context_menu(app, x, y); }
EMSCRIPTEN_KEEPALIVE const char* sf_selected_text()
{
    static std::string text;
    size_t size = 0;
    if (ui_host_selected_text(app, nullptr, &size) != UI_OK || size <= 1) { return ""; }
    text.resize(size);
    return ui_host_selected_text(app, text.data(), &size) == UI_OK ? text.c_str() : "";
}
EMSCRIPTEN_KEEPALIVE void* sf_alloc(unsigned size) { return std::malloc(size); }
EMSCRIPTEN_KEEPALIVE void sf_free(void* data) { std::free(data); }
EMSCRIPTEN_KEEPALIVE int sf_text_update(unsigned session, unsigned revision, const char* text, unsigned size, unsigned first, unsigned last, unsigned backward)
{ return ui_host_text_update(app, session, revision, text, size, first, last, backward); }
EMSCRIPTEN_KEEPALIVE int sf_text_preview(unsigned session, unsigned revision, const char* text, unsigned size, unsigned first, unsigned last, unsigned backward, unsigned cs, unsigned ce)
{
    const ui_host_composition_range range{sizeof(range), 1, first, last, backward, cs, ce};
    return ui_host_text_preview(app, session, revision, text, size, &range);
}
EMSCRIPTEN_KEEPALIVE int sf_text_hit(unsigned session, unsigned revision, int x, int y)
{
    unsigned offset = 0; const auto result = ui_host_text_hit_test(app, session, revision, x, y, &offset);
    return result == UI_OK ? int(offset) : -int(result);
}
EMSCRIPTEN_KEEPALIVE int sf_text_scroll(unsigned session, unsigned revision, int dx, int dy)
{ return ui_host_text_scroll(app, session, revision, dx, dy); }
EMSCRIPTEN_KEEPALIVE const char* sf_text_read()
{
    ui_host_text_state s{}; s.struct_size = sizeof(s); s.version = 1;
    const auto result = ui_host_text_read(app, &s);
    if (result != UI_OK) { text_state_json = "{\"error\":" + std::to_string(result) + "}"; return text_state_json.c_str(); }
    if (!s.active) { return "null"; }
    char fields[1024];
    std::snprintf(fields, sizeof(fields), "{\"session\":%u,\"revision\":%u,\"multiline\":%u,\"password\":%u,\"left\":%d,\"top\":%d,\"right\":%d,\"bottom\":%d,\"clipLeft\":%d,\"clipTop\":%d,\"clipRight\":%d,\"clipBottom\":%d,\"selectionStart\":%u,\"selectionEnd\":%u,\"selectionBackward\":%u,\"foreground\":%u,\"background\":%u,\"fontSize\":%u,\"textLeft\":%d,\"textTop\":%d,\"textRight\":%d,\"textBottom\":%d,\"lineHeight\":%u,\"caretX\":%d,\"caretY\":%d,\"scrollX\":%d,\"scrollY\":%d,\"text\":\"",
        s.session, s.revision, s.multiline, s.password, s.left, s.top, s.right, s.bottom, s.clip_left, s.clip_top, s.clip_right, s.clip_bottom,
        s.selection_start, s.selection_end, s.selection_backward, s.foreground, s.background, s.font_size, s.text_left, s.text_top, s.text_right, s.text_bottom, s.line_height, s.caret_x, s.caret_y, s.scroll_x, s.scroll_y);
    text_state_json = fields;
    for (unsigned i = 0; i < s.text_size; ++i) {
        const auto c = static_cast<unsigned char>(s.text[i]);
        if (c == '"' || c == '\\') { text_state_json += '\\'; text_state_json += char(c); }
        else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); text_state_json += escaped; }
        else { text_state_json += char(c); }
    }
    text_state_json += "\"}";
    return text_state_json.c_str();
}
EMSCRIPTEN_KEEPALIVE void sf_stop()
{
    if (app) {
        ui_app_destroy(app);
        app = nullptr;
        docs::current = nullptr;
    }
}
}
