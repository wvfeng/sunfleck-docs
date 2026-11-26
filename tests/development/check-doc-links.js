'use strict';
// Check maintained Markdown, including local evidence links and heading anchors.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../..');
const reportAt = process.argv.indexOf('--report');
const reportPath = reportAt < 0 ? undefined : process.argv[reportAt + 1];
if (reportAt >= 0 && !reportPath) { throw new Error('--report requires an output path'); }
const skipped = new Set(['node_modules', 'bin', 'obj', 'server', '.git', '.vs']);
const files = [path.join(root, 'README.md')];
function collect(directory) {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
        const filename = path.join(directory, entry.name);
        if (entry.isDirectory() && !skipped.has(entry.name)) { collect(filename); }
        else if (entry.isFile() && /\.md$/i.test(entry.name)) { files.push(filename); }
    }
}
for (const directory of ['guides', 'assets', 'snippets', 'tests', 'tools', 'site']) { collect(path.join(root, directory)); }
function withoutFences(text) {
    let fence;
    return text.split('\n').map(line => {
        const marker = /^\s{0,3}(`{3,}|~{3,})/.exec(line)?.[1];
        if (!fence && marker) { fence = marker; return ''; }
        if (fence) {
            if (marker?.[0] === fence[0] && marker.length >= fence.length) { fence = undefined; }
            return '';
        }
        return line;
    }).join('\n');
}
const texts = new Map();
const anchors = new Map();
function read(filename) {
    if (!texts.has(filename)) { texts.set(filename, withoutFences(fs.readFileSync(filename, 'utf8'))); }
    return texts.get(filename);
}
function anchorSet(filename) {
    if (!anchors.has(filename)) {
        const found = new Set();
        const counts = new Map();
        for (const line of read(filename).split('\n')) {
            const heading = /^\s{0,3}#{1,6}\s+(.+?)(?:\s+#+)?\s*$/.exec(line)?.[1];
            if (heading) {
                const slug = heading.replace(/<[^>]*>/g, '').replace(/\[([^\]]+)\]\([^)]*\)/g, '$1')
                    .toLowerCase().replace(/[^\p{L}\p{N}_\-\s]/gu, '').replace(/\s/g, '-');
                const count = counts.get(slug) || 0;
                counts.set(slug, count + 1);
                found.add(slug + (count ? '-' + count : ''));
            }
            for (const match of line.matchAll(/<(?:a|[hH][1-6])\b[^>]*\b(?:id|name)=["']([^"']+)["']/g)) { found.add(match[1]); }
        }
        anchors.set(filename, found);
    }
    return anchors.get(filename);
}
const problems = [];
let checked = 0, checkedAnchors = 0;
for (const filename of files) {
    const relative = path.relative(root, filename).replaceAll('\\', '/');
    const body = read(filename);
    const prose = body.replace(/(`+)[^`\n]*?\1/g, '');
    for (const match of prose.matchAll(/!?\[[^\]\n]*\]\(\s*(<[^>\n]+>|[^\s)]+)(?:\s+["'][^\n]*?["'])?\s*\)/g)) {
        const href = match[1].replace(/^<|>$/g, '');
        if (/^[a-z][a-z\d+.-]*:/i.test(href) || href.startsWith('//')) { continue; }
        checked++;
        const line = prose.slice(0, match.index).split('\n').length;
        try {
            const hash = href.indexOf('#');
            const linkPath = decodeURIComponent((hash < 0 ? href : href.slice(0, hash)).split('?')[0]);
            const anchor = hash < 0 ? '' : decodeURIComponent(href.slice(hash + 1));
            const target = linkPath ? path.resolve(path.dirname(filename), linkPath) : filename;
            if (!fs.existsSync(target)) { problems.push({ file: relative, line, href, reason: 'missing target' }); }
            else if (anchor && /\.md$/i.test(target)) {
                checkedAnchors++;
                if (!anchorSet(target).has(anchor)) { problems.push({ file: relative, line, href, reason: 'missing Markdown anchor' }); }
            }
        } catch (error) { problems.push({ file: relative, line, href, reason: error.message }); }
    }
}
const report = { capturedUtc: new Date().toISOString(), files: files.length, checked, checkedAnchors,
    problems,
    scope: 'Markdown local inline links/anchors in README, docs, examples, editors, tests, tools and apps/documentation. Code examples, archived HTML contents and external URLs are not checked.' };
if (reportPath) { fs.writeFileSync(path.resolve(root, reportPath), JSON.stringify(report, null, 2) + '\n'); }
console.log(JSON.stringify(report, null, 2));
if (problems.length) { process.exitCode = 1; }
