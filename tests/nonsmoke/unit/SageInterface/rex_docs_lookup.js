// Exercise lookup's asynchronous loading without a browser or dependencies.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const script = fs.readFileSync(path.resolve(__dirname, '../../../../docs/javascripts/api-lookup.js'), 'utf8');

async function check(fails, root = '/docs/', limit = 200) {
  const elements = Object.fromEntries(['api-lookup-q', 'api-lookup-meta', 'api-lookup-results']
    .map(id => [id, { value: '', textContent: '', innerHTML: '', dataset: { limit }, addEventListener(_, fn) { this.input = fn; } }]));
  let resolve;
  const response = new Promise(done => { resolve = done; });
  vm.runInNewContext(script, {
    document: { getElementById: id => elements[id], currentScript: { src: `https://example.org${root}javascripts/api-lookup.js` } },
    window: { location: { pathname: '/docs/api-lookup/', href: 'https://example.org/docs/api-lookup/' } },
    URL, console: { error() {} }, fetch: () => response,
  });
  const query = elements['api-lookup-q'];
  query.value = 'ProcessIf';
  query.input(); // The query arrives before the network response.
  resolve({ ok: !fails, status: 503, json: async () => ({ symbols: [
    { name: 'BuildAstTreeDepGraph::ProcessIf', kind: 'function', url: 'reference/BuildAstTreeDepGraph/ProcessIf/' },
    { name: 'SgNode', kind: 'record', url: 'reference/SgNode/' },
  ] }) });
  await new Promise(done => setImmediate(done));
  const result = elements['api-lookup-results'].innerHTML;
  if (fails) {
    assert.ok(result.includes(`href="${root}reference/"`));
    assert.match(elements['api-lookup-meta'].textContent, /Could not load/);
    query.value = 'retry'; query.input();
    assert.match(elements['api-lookup-meta'].textContent, /Could not load/);
  } else {
    assert.match(result, /BuildAstTreeDepGraph::ProcessIf/);
    assert.ok(result.includes(`href="${root}reference/BuildAstTreeDepGraph/ProcessIf/"`));
    assert.doesNotMatch(result, /SgNode/);
    assert.match(elements['api-lookup-meta'].textContent, /1 matching symbols of 2; showing 1/);
    query.value = 'absent'; query.input();
    assert.match(elements['api-lookup-results'].innerHTML, /No matching symbols/);
    query.value = ''; query.input();
    assert.equal(elements['api-lookup-results'].innerHTML, '');
    assert.match(elements['api-lookup-meta'].textContent, /2 symbols indexed/);
    query.value = 's'; query.input();
    assert.equal((elements['api-lookup-results'].innerHTML.match(/class="api-lookup-row"/g) || []).length, Math.min(2, limit));
  }
}
function checkTables() {
  const makeTable = descriptions => ({
    classes: new Set(),
    classList: { add(name) { this.owner.classes.add(name); } },
    style: { setProperty() {} },
    querySelector() { return { querySelectorAll: () => [{ textContent: 'Name' }, { textContent: 'Description' }] }; },
    querySelectorAll(selector) {
      if (selector.endsWith('td:last-child')) return descriptions.map(innerHTML => ({ innerHTML }));
      return descriptions.map(() => ({ querySelector: () => ({ querySelector: () => ({ textContent: 'Example' }) }) }));
    },
  });
  const empty = makeTable(['', ' ']);
  const documented = makeTable(['', '<p>Keep this contract.</p>']);
  const tables = [empty, documented];
  tables.forEach(table => { table.classList.owner = table; });
  const code = fs.readFileSync(path.resolve(__dirname, '../../../../docs/javascripts/reference-table-sizing.js'), 'utf8');
  vm.runInNewContext(code, { document: { readyState: 'complete', querySelectorAll: () => tables } });
  assert.ok(empty.classes.has('ref-no-descriptions'));
  assert.ok(!documented.classes.has('ref-no-descriptions'));
  assert.ok(tables.every(table => table.classes.has('ref-two-col')));
}

(async () => {
  checkTables();
  await check(false); await check(true);
  await check(false, '/', 1);
  await check(false, '/nested/reference-desk/', 8);
  vm.runInNewContext(script, { document: { getElementById: () => null } });
})().catch(error => { console.error(error); process.exitCode = 1; });
