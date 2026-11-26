import createModule from './sunfleck.js';
import { attachHost, loadHostedFont } from './host.js';

const canvas = document.querySelector('canvas');
const status = document.querySelector('#status');
let module;
let host;

function check(result, stage) {
  if (result !== 0) throw new Error(`Sunfleck ${stage}: ${result}`);
}

function withText(text, callback) {
  const bytes = new TextEncoder().encode(text);
  const pointer = module._sf_alloc(bytes.length + 1);
  if (!pointer) throw new Error('Out of memory');
  try {
    module.HEAPU8.set(bytes, pointer);
    module.HEAPU8[pointer + bytes.length] = 0;
    return callback(pointer, bytes.length);
  } finally {
    module._sf_free(pointer);
  }
}

function route() {
  const id = location.hash.replace(/^#\//, '').split('?')[0];
  if (id === '' || id === 'home') return 'overview';
  try { return decodeURIComponent(id); } catch { return 'not-found'; }
}

function updatePage() {
  document.body.dataset.page = module.UTF8ToString(module._sf_route());
  document.title = module.UTF8ToString(module._sf_title()) + ' · Sunfleck';
  status.textContent = '';
  host?.paint();
}

function navigate() {
  try {
    check(withText(route(), id => module._sf_navigate(id)), 'navigation');
    updatePage();
  } catch (error) {
    status.textContent = '暂时无法打开页面。请结束当前操作后重试。';
    console.error(error);
  }
}

function drain() {
  const action = module.UTF8ToString(module._sf_action());
  if (!action) return;
  const value = module.UTF8ToString(module._sf_action_value());
  if (action === 'navigate') {
    if (route() === value) queueMicrotask(navigate);
    else location.hash = '#/' + value;
  } else if (action === 'copy') {
    const show = text => {
      status.textContent = text;
      setTimeout(() => { if (status.textContent === text) status.textContent = ''; }, 2200);
    };
    if (navigator.clipboard) navigator.clipboard.writeText(value).then(() => show('已复制代码'), () => show('复制失败'));
    else show('当前站点无法访问剪贴板');
  } else if (action === 'external') {
    window.open(value, '_blank', 'noopener,noreferrer');
  }
}

async function loadSampleImage() {
  const response = await fetch('./blue-marble.jpg');
  if (!response.ok) throw new Error(`HTTP ${response.status}: blue-marble.jpg`);
  const bitmap = await createImageBitmap(await response.blob());
  let pixels;
  const width = Math.min(360, bitmap.width);
  const height = Math.max(1, Math.round(bitmap.height * width / bitmap.width));
  try {
    const surface = document.createElement('canvas');
    surface.width = width;
    surface.height = height;
    const context = surface.getContext('2d');
    context.drawImage(bitmap, 0, 0, width, height);
    pixels = context.getImageData(0, 0, width, height).data;
  } finally {
    bitmap.close();
  }
  const pointer = module._sf_alloc(pixels.length);
  if (!pointer) throw new Error('Image allocation failed');
  try {
    module.HEAPU8.set(pixels, pointer);
    check(module._sf_sample_image(pointer, width, height), 'sample image');
  } finally {
    module._sf_free(pointer);
  }
}

try {
  const response = await fetch('./sunfleck.data');
  if (!response.ok) throw new Error(`HTTP ${response.status}: sunfleck.data`);
  let packageBytes = await response.arrayBuffer();
  module = await createModule({ getPreloadedPackage: () => {
    const bytes = packageBytes;
    packageBytes = null;
    return bytes;
  } });
  await loadHostedFont(module.FS.readFile('/sunfleck/font.ttf'));
  const mono = module.FS.readFile('/sunfleck/mono.ttf');
  const font = module._sf_alloc(mono.length);
  if (!font) throw new Error('Out of memory');
  try {
    module.HEAPU8.set(mono, font);
    check(module._sf_font(font, mono.length), 'mono font');
  } finally {
    module._sf_free(font);
  }
  await loadSampleImage();
  check(withText(route(), id => module._sf_start(id)), 'start');
  host = attachHost(document.querySelector('canvas'), {
    setRenderMode: mode => module._sf_render_mode(mode),
    commands: () => {
      const sizePointer = module._sf_alloc(4);
      if (!sizePointer) throw new Error('Command allocation failed');
      try {
        const pointer = module._sf_draw_commands(sizePointer);
        const size = new DataView(module.HEAPU8.buffer).getUint32(sizePointer, true);
        if (!pointer || !size) throw new Error('Draw commands unavailable');
        return module.HEAPU8.subarray(pointer, pointer + size);
      } finally { module._sf_free(sizePointer); }
    },
    selectView: id => module._sf_select_view(id),
    snapshot: () => JSON.parse(module.UTF8ToString(module._sf_snapshot())),
    semanticAction: (id, action) => withText(id, pointer => module._sf_semantic_action(pointer, action)),
    resize: (w, h, dpi) => module._sf_resize(w, h, dpi),
    pointer: (kind, x, y) => module._sf_pointer(kind, x, y),
    key: (kind, shift) => module._sf_key(kind, shift),
    focus: value => module._sf_focus(value),
    scroll: (x, y, dy) => module._sf_scroll(x, y, dy),
    contextMenu: (x, y) => module._sf_context_menu(x, y),
    readText: () => {
      const state = JSON.parse(module.UTF8ToString(module._sf_text_read()));
      if (state?.error) throw new Error(`Text read failed: ${state.error}`);
      return state;
    },
    updateText: (session, revision, text, first, last, backward) =>
      withText(text, (pointer, size) => module._sf_text_update(session, revision, pointer, size, first, last, backward)),
    previewText: (session, revision, text, first, last, backward, cs, ce) =>
      withText(text, (pointer, size) => module._sf_text_preview(session, revision, pointer, size, first, last, backward, cs, ce)),
    hitText: (session, revision, x, y) => module._sf_text_hit(session, revision, x, y),
    scrollText: (session, revision, dx, dy) => module._sf_text_scroll(session, revision, dx, dy),
    tick: () => { const result=module._sf_tick(); queueMicrotask(drain); return result; },
    pixels: (w, h) => {
      // Reacquire after every native call: memory growth invalidates old views.
      const pointer = module._sf_pixels();
      return module.HEAPU8.subarray(pointer, pointer + w * h * 4);
    },
    stop: () => module._sf_stop(),
  });

  window.addEventListener('hashchange', navigate);
  document.addEventListener('copy', event => {
    if (document.activeElement !== canvas || !event.clipboardData) return;
    check(module._sf_select_view(0), 'select main view');
    const selected = module.UTF8ToString(module._sf_selected_text());
    if (!selected) return;
    event.clipboardData.setData('text/plain', selected);
    event.preventDefault();
  });
  canvas.addEventListener('pointerleave', event => {
    if (event.pointerType === 'mouse' && !event.buttons) {
      check(module._sf_select_view(0), 'select main view');
      check(module._sf_pointer(1, -1, -1), 'clear hover');
    }
  });
  window.addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault();
      canvas.focus({ preventScroll: true });
      module._sf_search_focus();
      host.paint();
    }
  }, true);
  if (new URL(location.href).searchParams.has('verify')) {
    window.documentation = { snapshot: () => JSON.parse(module.UTF8ToString(module._sf_snapshot())) };
  }
  updatePage();
  document.body.dataset.ready = 'true';
} catch (error) {
  status.textContent = '文档加载失败。请刷新重试。\n' + String(error);
  console.error(error);
}
