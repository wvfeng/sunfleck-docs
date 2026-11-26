const assert = require('node:assert/strict');
const path = require('node:path');
const { createRequire } = require('node:module');
const modules = process.env.SUNFLECK_BROWSER_TEST_MODULES || 'tests/web';
const { chromium } = createRequire(path.resolve(modules, 'package.json'))('playwright-core');

async function run() {
  const base = process.argv[2] || 'http://127.0.0.1:8791/nested/guide/';
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 820 } });
    const ready = id => page.waitForFunction(page => document.body.dataset.page === page, id);
    await page.goto(new URL('?verify=1#/Button', base).href);
    await ready('Button');
    await page.reload();
    await ready('Button');
    const resources = await (await page.request.get(new URL('resources.json', base).href)).json();
    assert.ok(resources.files.some(file => file.path === 'sunfleck.wasm'));
    const wasm = await page.request.get(new URL('sunfleck.wasm', base).href);
    assert.match(wasm.headers()['content-type'], /application\/wasm/);
    await page.evaluate(() => { location.hash = '#/does-not-exist'; });
    await ready('not-found');
    await page.goto(new URL('?verify=1#/Icon', base).href);
    await ready('not-found');
    await page.evaluate(() => { location.hash = '#/quick'; });
    await ready('quick');
    await page.route('**/sunfleck.data', route => route.fulfill({ status: 503, body: 'unavailable' }));
    await page.reload();
    await page.waitForFunction(() => document.querySelector('#status').textContent.includes('HTTP 503'));
    await page.unroute('**/sunfleck.data');
    await page.reload();
    await ready('quick');
    console.log('Relocated static package, deep links, missing routes and resource recovery passed.');
  } finally {
    await browser.close();
  }
}

run().catch(error => { console.error(error); process.exitCode = 1; });
