// Exercise lookup's asynchronous loading without a browser or dependencies.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const markdown = fs.readFileSync(path.resolve(__dirname, '../../../../docs/api-lookup.md'), 'utf8');
const script = markdown.match(/<script>([\s\S]*?)<\/script>/)[1];

async function check(fails) {
  const elements = Object.fromEntries(['api-lookup-q', 'api-lookup-meta', 'api-lookup-results']
    .map(id => [id, { value: '', textContent: '', innerHTML: '', addEventListener(_, fn) { this.input = fn; } }]));
  let resolve;
  const response = new Promise(done => { resolve = done; });
  vm.runInNewContext(script, {
    document: { getElementById: id => elements[id] },
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
    assert.match(result, /href="\/docs\/reference\/"/);
    assert.match(elements['api-lookup-meta'].textContent, /Could not load/);
    query.value = 'retry'; query.input();
    assert.match(elements['api-lookup-meta'].textContent, /Could not load/);
  } else {
    assert.match(result, /BuildAstTreeDepGraph::ProcessIf/);
    assert.doesNotMatch(result, /SgNode/);
    assert.match(elements['api-lookup-meta'].textContent, /1 matching symbols of 2; showing 1/);
    query.value = 'absent'; query.input();
    assert.match(elements['api-lookup-results'].innerHTML, /No matching symbols/);
    query.value = ''; query.input();
    assert.match(elements['api-lookup-results'].innerHTML, /SgNode/);
  }
}
(async () => { await check(false); await check(true); })().catch(error => { console.error(error); process.exitCode = 1; });
