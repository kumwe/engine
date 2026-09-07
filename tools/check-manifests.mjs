import assert from 'node:assert/strict';
import {readFileSync, lstatSync, realpathSync, mkdtempSync, symlinkSync, rmSync, mkdirSync} from 'node:fs';
import {resolve, sep} from 'node:path';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
const read = file => readFileSync(file, 'utf8');
const root = realpathSync('.');
function repositoryFile(path) {
  assert.ok(typeof path === 'string' && path.length > 0 && !path.startsWith('/') && !path.includes('\\'));
  const parts = path.split('/');
  assert.ok(parts.every(part => part !== '' && part !== '.' && part !== '..'));
  let target = root;
  for (const part of parts) { target += sep + part; assert.equal(lstatSync(target).isSymbolicLink(), false); }
  assert.ok(lstatSync(target).isFile());
  assert.equal(realpathSync(target), resolve(root,path));
}
const abi = read('resources/abi-symbols.txt').trim().split('\n');
const declared = [...read('include/kumwe/engine/engine.h').matchAll(/KUMWE_ENGINE_API (?:kumwe_engine_v1_status|void) (kumwe_engine_v1_\w+)\(/g)].map(item => item[1]).sort();
assert.deepEqual(declared, abi, 'Every public function must have exactly one manifest owner');
const statuses = [...read('include/kumwe/engine/engine.h').matchAll(/#define KUMWE_ENGINE_V1_(\w+) UINT32_C\((\d+)\)/g)].map(match => [match[1],Number(match[2])]);
assert.deepEqual(statuses, [['OK',0],['INVALID_INPUT',1],['UNSUPPORTED_VERSION',2],['INCOMPATIBLE_CAPABILITY',3],['INCOMPATIBLE_CORPUS',4],['INVALID_PROGRAM',5],['EXHAUSTED_LIMIT',6],['CANCELLED',7],['INTERNAL_FAILURE',8]], 'Draft status registry must change deliberately');
const handoff = read('MIGRATION-HANDOFF.md').match(/\n  public_manifests:\n([\s\S]*?)\n  intentionally_excluded:/)?.[1];
assert.ok(handoff, 'Native handoff must bind the public manifests');
const handoffEntries = [...handoff.matchAll(/path: \"([^\"]+)\"\n\s+sha256: \"([a-f0-9]{64})\"/g)];
assert.deepEqual(handoffEntries.map(entry => entry[1]), ['resources/abi-symbols.txt','resources/capabilities.json','resources/contracts.json','tests/ownership.json','include/kumwe/engine/engine.h']);
for (const [,path,digest] of handoffEntries) assert.equal(createHash('sha256').update(readFileSync(path)).digest('hex'),digest, `Stale handoff manifest ${path}`);
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
    repositoryFile(path);
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
  data => data.conformance.corpora = ['corpus'],
  data => data.conformance.corpora = ['corpus/./decimal/decimal-v1.tsv'],
]) {
  const changed = structuredClone(ownership); mutate(changed);
  assert.throws(() => checkOwnership(changed), 'Ownership gate must refuse negative fixture');
}
mkdirSync('artifacts', {recursive:true});
const fixture = mkdtempSync('artifacts/ownership-links-');
try {
  symlinkSync(resolve('corpus'), fixture + '/corpus', 'dir');
  const changed = structuredClone(ownership);
  changed.conformance.corpora = [fixture + '/corpus/decimal/decimal-v1.tsv'];
  assert.throws(() => checkOwnership(changed), 'Intermediate symlink cannot establish repository test ownership');
} finally { rmSync(fixture, {recursive:true,force:true}); }
const contracts = JSON.parse(read('resources/contracts.json'));
assert.equal(contracts.completion_claim, false); assert.equal(contracts.abi_frozen, false);
assert.deepEqual(contracts.modules.map(module => module.module), ['decimal','definition_vm','document_batch','report','canonical_streaming']);
const decimal = contracts.modules[0];
assert.equal(createHash('sha256').update(readFileSync(decimal.corpus)).digest('hex'), decimal.corpus_sha256);
const capabilities = JSON.parse(read('resources/capabilities.json'));
assert.equal(capabilities.corpus_sha256, decimal.corpus_sha256);
assert.equal(capabilities.semantic_source, decimal.semantic_source);
assert.equal(capabilities.semantic_release_verified, false);
assert.deepEqual(capabilities.capabilities, ['decimal-batch-draft/1', ...contracts.modules.slice(1).flatMap(module => module.profiles ?? [module.profile])]);
for (const module of contracts.modules) {
  assert.equal(createHash('sha256').update(readFileSync(module.corpus)).digest('hex'), module.corpus_sha256);
  assert.equal(module.release_verified, false);
}
assert.deepEqual(capabilities.corpora.map(corpus => corpus.path), ownership.conformance.corpora,
  'Every advertised corpus must have exactly one conformance owner');
assert.equal(new Set(capabilities.corpora.map(corpus => corpus.path)).size, capabilities.corpora.length);
for (const corpus of capabilities.corpora) {
  repositoryFile(corpus.path);
  assert.equal(createHash('sha256').update(readFileSync(corpus.path)).digest('hex'), corpus.sha256,
    `Stale runtime corpus ${corpus.path}`);
}
const document = contracts.modules.find(module => module.module === 'document_batch');
const bundle = JSON.parse(read(document.corpus));
assert.equal(bundle.schema, 'kumwe-document-profile-corpus/v1');
assert.equal(bundle.profile, document.profile);
assert.ok(Array.isArray(bundle.corpora) && bundle.corpora.length >= 4);
assert.equal(new Set(bundle.corpora.map(corpus => corpus.id)).size, bundle.corpora.length);
for (const corpus of bundle.corpora) {
  assert.match(corpus.id, /^[a-z][a-z0-9-]*$/);
  const path = `corpus/document/${corpus.id}.json`;
  repositoryFile(path);
  assert.equal(createHash('sha256').update(readFileSync(path)).digest('hex'), corpus.sha256);
  assert.ok(capabilities.corpora.some(entry => entry.path === path && entry.sha256 === corpus.sha256));
}
assert.equal(createHash('sha256').update(readFileSync(document.preparation_corpus)).digest('hex'), document.preparation_corpus_sha256);
for (const contract of capabilities.computation.contracts) {
  const owner = contracts.modules.find(module => (module.profiles ?? [module.profile]).includes(contract.profile));
  assert.ok(owner, `Unknown runtime semantic profile ${contract.profile}`);
  assert.equal(contract.corpus_digest, contract.profile === 'normalized-preparation-draft/1'
    ? owner.preparation_corpus_sha256 : owner.corpus_sha256);
}
const runtime = JSON.parse(execFileSync(resolve(process.argv[2] ?? 'build', 'kumwe-engine-conformance'), [], {encoding:'utf8'}));
assert.match(runtime.computation.build_digest, /^[a-f0-9]{64}$/);
assert.equal(runtime.completion_claim, false);
assert.equal(runtime.semantic_release_verified, false);
assert.deepEqual(runtime.computation.contracts, capabilities.computation.contracts);
console.log(`${abi.length} ABI exports own behavior/boundary tests; exact corpus and nine negative ownership fixtures passed`);
