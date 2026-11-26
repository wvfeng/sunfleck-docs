const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const { createRequire } = require('node:module');
const modules = process.env.SUNFLECK_BROWSER_TEST_MODULES || 'tests/web';
const { chromium } = createRequire(path.resolve(modules, 'package.json'))('playwright-core');

async function nodes(page) {
  return page.evaluate(() => window.documentation.snapshot().views[0].nodes);
}

async function waitForText(page, name, present = true) {
  await page.waitForFunction(({ name, present }) => {
    const visible = window.documentation.snapshot().views[0].nodes.some(node => node.name === name);
    return visible === present;
  }, { name, present });
}

async function click(page, name) {
  await waitForText(page, name);
  const node = (await nodes(page)).find(item => item.name === name && item.enabled);
  assert.ok(node, name);
  await page.mouse.click(node.x + node.width / 2, node.y + node.height / 2);
}

async function route(page, id) {
  await page.waitForFunction(id => document.body.dataset.page === id, id);
}

async function dragAndCopy(page, node, width) {
  await page.evaluate(() => {
    delete document.body.dataset.copied;
    window.addEventListener('copy', event => {
      document.body.dataset.copied = event.clipboardData.getData('text/plain');
    }, { once: true });
  });
  await page.mouse.move(node.x + 1, node.y + 10);
  await page.mouse.down();
  await page.mouse.move(node.x + width, node.y + 10, { steps: 8 });
  await page.mouse.up();
  await page.keyboard.press('Control+c');
  return page.locator('body').getAttribute('data-copied');
}

async function run() {
  const base = process.argv[2] || 'http://127.0.0.1:8790/';
  const output = process.argv[3] || 'out/documentation/evidence';
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  await fs.mkdir(output, { recursive: true });
  try {
    for (const renderer of ['canvas2d', 'webgl2']) {
      const page = await browser.newPage({ viewport: { width: 1280, height: 820 } });
      const errors = [];
      page.on('pageerror', error => errors.push(error.message));
      await page.goto(new URL(`?verify=1&renderer=${renderer}#/Button`, base).href);
      await route(page, 'Button');
      const code = (await nodes(page)).find(node => node.role === 'status' && node.name.startsWith('<StackPanel'));
      assert.ok(code);
      const snippet = await dragAndCopy(page, code, 92);
      assert.ok(snippet && snippet.length < code.name.length && code.name.startsWith(snippet), snippet);
      await click(page, '确认');
      await waitForText(page, '已确认');
      await click(page, '禁用按钮');
      await page.waitForFunction(() => !window.documentation.snapshot().views[0].nodes.some(node =>
        node.name === '确认' && node.enabled));
      await page.screenshot({ path: path.join(output, renderer + '-button.png') });
      await click(page, 'API');
      await waitForText(page, 'Button 属性');
      await waitForText(page, '确认', false);
      await waitForText(page, 'Content');
      const apiRows = await nodes(page);
      assert.ok(apiRows.some(node => node.name === 'Content'));
      assert.ok(!apiRows.some(node => node.name === 'Width'));
      const rowHeight = apiRows.find(node => node.name === 'Command').y - apiRows.find(node => node.name === 'Content').y;
      await page.screenshot({ path: path.join(output, renderer + '-api.png') });
      assert.ok(rowHeight > 0 && rowHeight <= 40, `Button.Content row height: ${rowHeight}`);
      const contentCell = apiRows.find(node => node.role === 'status' && node.name === 'Content');
      assert.ok(contentCell?.height >= 28, `Button.Content text cell height: ${contentCell?.height}`);
      await click(page, '通用属性');
      await page.mouse.move(550, 530);
      await waitForText(page, 'Width');
      await waitForText(page, 'Content', false);
      await click(page, '指南');
      await waitForText(page, '使用指南');
      const paragraph = (await nodes(page)).find(node => node.name.startsWith('优先用 Content'));
      assert.ok(paragraph);
      const prose = await dragAndCopy(page, paragraph, 104);
      assert.ok(prose && prose.length < paragraph.name.length && paragraph.name.startsWith(prose), prose);
      await page.evaluate(() => { location.hash = '#/Image'; });
      await route(page, 'Image');
      await waitForText(page, '阿波罗 17 号拍摄的地球 · NASA');
      const photoCaption = (await nodes(page)).find(node => node.name === '阿波罗 17 号拍摄的地球 · NASA');
      const clip = { x: photoCaption.x + 44, y: photoCaption.y - 180, width: 144, height: 144 };
      const photo = await page.screenshot({ clip });
      await page.screenshot({ path: path.join(output, renderer + '-image.png') });
      await click(page, '隐藏图片');
      await waitForText(page, '显示图片');
      assert.notDeepEqual(await page.screenshot({ clip }), photo);
      await click(page, '显示图片');
      await waitForText(page, '隐藏图片');
      assert.deepEqual(await page.screenshot({ clip }), photo);
      await click(page, 'API');
      await waitForText(page, 'Source');
      await click(page, '通用属性');
      await waitForText(page, 'Width');
      await waitForText(page, 'Source', false);
      await click(page, '快速开始');
      await route(page, 'quick');
      for (const language of ['C++', 'C#', 'C']) {
        await click(page, language);
        assert.ok(!(await nodes(page)).some(node => node.name === `${language} SDK 下载` && node.enabled));
        await waitForText(page, `${language} · Fleck 页面`);
        await click(page, '使用 Fleck');
        await waitForText(page, `${language} · 纯代码界面`);
        await waitForText(page, `${language} · Fleck 页面`, false);
        await click(page, '使用 Fleck');
        await waitForText(page, `${language} · Fleck 页面`);
      }
      await page.screenshot({ path: path.join(output, renderer + '-quick.png') });
      await page.keyboard.press('Control+k');
      await page.keyboard.insertText('Slider');
      await click(page, 'Slider 滑块');
      await route(page, 'Slider');
      await page.setViewportSize({ width: 390, height: 780 });
      await click(page, '目录');
      await click(page, 'Button 按钮');
      await route(page, 'Button');
      await waitForText(page, '确认');
      await click(page, 'API');
      await waitForText(page, '组件属性');
      await waitForText(page, 'CommandMode');
      const mode = (await nodes(page)).find(node => node.name === 'CommandMode');
      await page.mouse.click(mode.x + 6, mode.y + mode.height / 2);
      await page.waitForFunction(() => window.documentation.snapshot().views[0].nodes.some(node =>
        node.name.startsWith('CommandMode ·') && node.name.includes('异步执行器') && node.height >= 42));
      await page.screenshot({ path: path.join(output, renderer + '-mobile-api.png') });
      await click(page, '指南');
      await waitForText(page, '使用指南');
      await page.screenshot({ path: path.join(output, renderer + '-mobile.png') });
      await click(page, '目录');
      await page.mouse.move(100, 520);
      await page.mouse.wheel(0, 565);
      const longTitle = 'ContentPresenter 内容呈现器';
      await page.waitForFunction(name => window.documentation.snapshot().views[0].nodes.some(node =>
        node.name === name && node.role === 'button' && node.height > 34), longTitle);
      const rows = (await nodes(page)).filter(node => node.role === 'button');
      const longRow = rows.find(node => node.name === longTitle);
      assert.equal(rows.find(node => node.name === 'Card 卡片')?.height, 34);
      assert.equal(rows.find(node => node.name === 'DockPanel 停靠面板')?.height, 34);
      await page.mouse.click(longRow.x + longRow.width - 12, longRow.y + longRow.height / 2);
      await route(page, 'ContentPresenter');
      await page.setViewportSize({ width: 1280, height: 820 });
      for (const id of ['DockHost', 'DockSplit', 'Ellipse']) {
        await page.evaluate(id => { location.hash = '#/' + id; }, id);
        await route(page, id);
        await waitForText(page, '实时预览');
      }
      const dockToolTitle = 'DockTool 停靠工具';
      const dockSplitTitle = 'DockSplit 停靠分割';
      await page.mouse.move(100, 550);
      await page.mouse.wheel(0, 10000);
      await waitForText(page, dockToolTitle);
      const originalY = (await nodes(page)).find(node => node.name === dockToolTitle).y;
      await click(page, dockToolTitle);
      await route(page, 'DockTool');
      await waitForText(page, dockToolTitle);
      assert.equal((await nodes(page)).find(node => node.name === dockToolTitle).y, originalY);
      const projectName = '示例项目';
      await waitForText(page, 'main.fleck');
      const project = (await nodes(page)).find(node => node.role === 'treeitem' && node.name === projectName);
      assert.ok(project);
      await page.mouse.click(project.x + 14, project.y + project.height / 2);
      await waitForText(page, 'main.fleck', false);
      await page.mouse.click(project.x + 14, project.y + project.height / 2);
      await click(page, 'main.fleck');
      await click(page, dockSplitTitle);
      await route(page, 'DockSplit');
      await page.evaluate(() => { location.hash = '#/Ellipse'; });
      await route(page, 'Ellipse');
      await waitForText(page, dockToolTitle);
      assert.equal((await nodes(page)).find(node => node.name === dockToolTitle).y, originalY);
      await page.setViewportSize({ width: 390, height: 780 });
      await click(page, '目录');
      await page.mouse.move(100, 550);
      await page.mouse.wheel(0, 10000);
      await click(page, dockToolTitle);
      await route(page, 'DockTool');
      await page.waitForFunction(name => !window.documentation.snapshot().views[0].nodes.some(node =>
        node.role === 'button' && node.name === name), dockToolTitle);
      await click(page, '目录');
      await page.waitForFunction(name => window.documentation.snapshot().views[0].nodes.some(node =>
        node.role === 'button' && node.name === name), dockToolTitle);
      assert.equal(await page.locator('#status').textContent(), '');
      assert.deepEqual(errors, []);
      await page.close();
      console.log(`${renderer}: text selection, bound API lists, live image source and documentation navigation passed`);
    }
  } finally {
    await browser.close();
  }
}

run().catch(error => { console.error(error); process.exitCode = 1; });
