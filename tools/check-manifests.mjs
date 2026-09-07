import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
const read = file => readFileSync(file, 'utf8');
const abi = read('resources/abi-symbols.txt').trim().split('\n');
const declared = [...read('include/kumwe/engine/engine.h').matchAll(/KUMWE_ENGINE_API (?:kumwe_engine_v1_status|void) (kumwe_engine_v1_\w+)\(/g)].map(item => item[1]).sort();
assert.deepEqual(declared, abi, 'Every public function must have exactly one manifest owner');
const inventory = JSON.parse(execFileSync('ctest', ['--test-dir',process.argv[2] ?? 'build','--show-only=json-v1'], {encoding:'utf8'}));
const names = new Set(inventory.tests.map(test => test.name));
const nonempty = value => typeof value === 'string' && value.trim().length > 0;
function checkOwnership(data) {
  assert.equal(data.schema, 'kumwe-test-ownership/v1'); assert.equal(data.package, 'kumwe/engine');
  assert.equal(data.api_manifest, 'resources/abi-symbols.txt');
  assert.deepEqual(Object.keys(data.exports).sort(), abi);
  const tests = list => {
    assert.ok(Array.isArray(list) && list.length > 0);
    assert.equal(new Set(list).size, list.length);
    for (const name of list) assert.ok(names.has(name), `Unknown discovered CTest ${name}`);
  };
  for (const item of Object.values(data.exports)) { tests(item.behavior); tests(item.boundary); }
  assert.equal(data.conformance.status, 'owned'); assert.ok(nonempty(data.conformance.rationale));
  tests(data.conformance.tests);
  assert.ok(Array.isArray(data.conformance.corpora) && data.conformance.corpora.length > 0);
  for (const path of [...data.conformance.corpora, ...data.architecture]) {
    assert.ok(nonempty(path) && !path.includes('..') && !path.startsWith('/') && existsSync(path));
  }
  assert.ok(Array.isArray(data.architecture) && data.architecture.length > 0);
  assert.match(data.host.baseline, /^[a-f0-9]{40}$/);
  assert.ok(nonempty(data.host.repository));
  assert.ok(Array.isArray(data.host.transfers) && data.host.transfers.length === 0, 'This draft has no App adoption');
  assert.ok(Array.isArray(data.host.retained_responsibilities) && data.host.retained_responsibilities.length > 0);
  assert.ok(data.host.retained_responsibilities.every(nonempty));
}
const ownership = JSON.parse(read('tests/ownership.json'));
checkOwnership(ownership);
// Verify that weakening the ownership declaration is rejected by the actual validator.
for (const mutate of [
  data => delete data.exports[abi[0]],
  data => data.exports[abi[0]].behavior = [],
  data => data.exports[abi[0]].boundary = ['imaginary-test'],
  data => data.conformance.status = 'future',
  data => data.conformance.corpora = ['missing-corpus.tsv'],
  data => data.host.baseline = 'main',
]) {
  const changed = structuredClone(ownership); mutate(changed);
  assert.throws(() => checkOwnership(changed), 'Ownership gate must refuse negative fixture');
}
const contracts = JSON.parse(read('resources/contracts.json'));
assert.equal(contracts.completion_claim, false); assert.equal(contracts.abi_frozen, false);
assert.deepEqual(contracts.modules.map(module => module.module), ['decimal','definition_vm','document_batch','report','canonical_streaming']);
const decimal = contracts.modules[0];
assert.equal(createHash('sha256').update(readFileSync(decimal.corpus)).digest('hex'), decimal.corpus_sha256);
const capabilities = JSON.parse(read('resources/capabilities.json'));
assert.equal(capabilities.corpus_sha256, decimal.corpus_sha256);
assert.equal(capabilities.semantic_source, decimal.semantic_source);
assert.equal(capabilities.semantic_release_verified, false);
assert.deepEqual(capabilities.capabilities, ['decimal-batch-draft/1']);
console.log(`${abi.length} ABI exports own behavior/boundary tests; exact corpus and six negative ownership fixtures passed`);
