import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import Ajv from 'ajv/dist/2020.js';
import YAML from 'yaml';

const here = path.dirname(fileURLToPath(import.meta.url));
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const schemaBytes = fs.readFileSync(path.join(here, 'engine-candidate-attestation.v1.schema.json'));
const validate = new Ajv({ strict: false, allErrors: true }).compile(JSON.parse(schemaBytes));
const requiredJobs = ['source-release-preparation', 'binding', 'address-undefined-sanitizers',
  'clean-pie', 'whole-boundary-benchmarks'];
const requireFact = (condition, reason) => { if (!condition) throw new Error(reason); };
const commit = value => typeof value === 'string' && /^[a-f0-9]{40}$/.test(value);

export function semanticInputs(contracts) {
  const inputs = contracts.modules.flatMap(module => {
    const r = module.semantic_release;
    return [...new Set([r.corpus_sha256, r.api_digest, r.capability_digest, r.service_map_digest,
      ...Object.values(r.corpus_digests || {})].filter(Boolean))].map(digest => ({ owner: r.repository, version: r.version, sha256: digest }));
  });
  const baseline = contracts.computation_baseline;
  for (const digest of new Set([baseline.api_digest, baseline.capability_digest, baseline.service_map_digest,
    ...Object.values(baseline.corpus_digests)].filter(Boolean)))
    inputs.push({ owner: baseline.repository, version: baseline.version, sha256: digest });
  return inputs;
}

export function candidateReference(value) {
  requireFact(value && Object.keys(value).sort().join(',') === 'sha256,uri', 'Candidate reference must contain only uri and sha256.');
  requireFact(typeof value.uri === 'string' && /^https:\/\/raw\.githubusercontent\.com\/kumwe\/extension-sdk\/[a-f0-9]{40}\/evidence\/[A-Za-z0-9_./-]+\.ya?ml$/.test(value.uri)
    && !value.uri.split('/').some(p => p === '.' || p === '..'), 'Candidate reference must identify immutable SDK evidence YAML.');
  requireFact(typeof value.sha256 === 'string' && /^[a-f0-9]{64}$/.test(value.sha256), 'Candidate YAML SHA256 is required.');
  return value;
}

function mergedPulls(pulls, mergedCommit, defaultBranch) {
  requireFact(Array.isArray(pulls), 'Merged pull request inventory is malformed.');
  requireFact(typeof defaultBranch === 'string' && defaultBranch.length > 0, 'The actual default branch is required.');
  return pulls.filter(p => p.merged_at && p.merge_commit_sha === mergedCommit
    && p.base?.repo?.full_name === 'kumwe/engine' && p.base.ref === defaultBranch
    && p.head?.repo?.full_name === 'kumwe/engine' && commit(p.head.sha));
}

export function referenceFromPulls(pulls, mergedCommit, defaultBranch = 'main') {
  const references = [];
  for (const pull of mergedPulls(pulls, mergedCommit, defaultBranch)) {
    const body = pull.body || '';
    const matches = [...body.matchAll(/<!-- kumwe-engine-candidate\/v1\n([^]*?)\n-->/g)];
    requireFact(matches.length <= 1, 'A merged pull request cannot contain competing candidate references.');
    if (matches.length) references.push(candidateReference(JSON.parse(matches[0][1])));
  }
  requireFact(references.length === 1, 'Exactly one candidate reference in the observed merged Engine PR is required.');
  return references[0];
}

export function validateRecord(record) {
  requireFact(validate(record), 'Candidate attestation schema failed: ' + JSON.stringify(validate.errors));
  requireFact(record.engine_candidate.repository === 'https://github.com/kumwe/engine'
    && record.extension_candidate.repository === 'https://github.com/kumwe/kumwe-engine', 'Candidate repository identity differs.');
  requireFact(commit(record.engine_candidate.tested_tree) && commit(record.extension_candidate.tested_tree), 'Candidate Git tree identities must be exact.');
  requireFact(record.status === 'passed' && record.publishing_permitted === false,
    'A passing non-publishing cross-build attestation is required.');
  requireFact(record.build.network_disabled === true, 'Both candidate consumer builds must have networking disabled.');
  for (const key of ['toolchains', 'operating_systems', 'architectures', 'php_modes_and_versions'])
    requireFact(record.build[key].length > 0 && record.build[key].every(x => x.trim()), 'Candidate build evidence is incomplete: ' + key);
  for (const key of ['corpus_results', 'lifecycle_sanitizer_leak', 'hostile_input_and_refusal'])
    requireFact(record.verification[key].length > 0 && record.verification[key].every(x => x.trim()), 'Candidate verification evidence is incomplete: ' + key);
  return record;
}

export function validateObserved(record, actual) {
  validateRecord(record);
  const engine = record.engine_candidate, binding = record.extension_candidate;
  requireFact(actual.handoffChangeSet === record.change_set, 'Candidate change set differs from its exact original handoff.');
  requireFact(actual.mergedPullHead === engine.tested_commit, 'The attested candidate is not the observed merged Engine PR head.');
  requireFact(actual.candidateTree === engine.tested_tree && actual.mergedTree === engine.tested_tree,
    'The attested Engine candidate tree differs from the actual candidate or merged source.');
  requireFact(actual.archiveSha256 === engine.source_archive_sha256 && actual.handoffSha256 === engine.handoff_sha256,
    'The original candidate archive or handoff digest differs; merged-source identities cannot substitute.');
  requireFact(actual.bindingTree === binding.tested_tree, 'Binding candidate tree differs from the attested source.');
  requireFact(actual.lock.commit === engine.tested_commit && actual.lock.archive_sha256 === engine.source_archive_sha256,
    'Binding did not embed the exact attested Engine candidate archive.');
  const run = actual.run;
  requireFact(run.head_sha === binding.tested_commit && run.head_repository?.full_name === 'kumwe/kumwe-engine'
    && run.path === '.github/workflows/ci.yml' && run.name === 'Native binding candidate'
    && ['pull_request', 'push', 'workflow_dispatch'].includes(run.event)
    && run.status === 'completed' && run.conclusion === 'success', 'Binding candidate CI is not the exact successful source workflow.');
  requireFact(actual.jobs.length >= requiredJobs.length
    && actual.jobs.every(j => j.status === 'completed' && j.conclusion === 'success' && Number.isSafeInteger(j.id))
    && new Set(actual.jobs.map(j => j.id)).size === actual.jobs.length
    && requiredJobs.every(name => actual.jobs.some(j => j.name === name)), 'All five binding candidate jobs must pass; skipped or missing lanes refuse publication.');
  requireFact(actual.artifact.workflow_run?.id === run.id && actual.artifact.workflow_run?.head_sha === binding.tested_commit
    && actual.artifact.expired === false && /^sha256:[a-f0-9]{64}$/.test(actual.artifact.digest || ''),
    'Candidate evidence artifact is missing, expired or belongs to another source run.');
  const inputs = new Set(record.semantic_inputs.map(x => `${x.owner}\0${x.version.replace(/^v/, '')}\0${x.manifest_or_corpus_sha256}`));
  for (const source of actual.semanticInputs) {
    requireFact(inputs.has(`${source.owner}\0${source.version}\0${source.sha256}`),
      `Candidate semantic evidence does not cover ${source.owner} ${source.version} ${source.sha256}.`);
  }
}

function command(root, ...args) {
  const result = spawnSync(args[0], args.slice(1), { cwd: root, encoding: null, maxBuffer: 100_000_000, timeout: 180000 });
  requireFact(result.status === 0, `Candidate identity command failed: ${args[0]}; ${result.stderr?.toString().slice(-1500)}`);
  return result.stdout;
}

async function request(url, raw = false) {
  const parsed = new URL(url);
  requireFact(['api.github.com', 'raw.githubusercontent.com'].includes(parsed.hostname), 'Unsupported candidate evidence origin.');
  const headers = { 'User-Agent': 'Kumwe-native-candidate-publisher-gate' };
  if (parsed.hostname === 'api.github.com') {
    headers.Accept = 'application/vnd.github+json';
    if (process.env.GH_TOKEN) headers.Authorization = `Bearer ${process.env.GH_TOKEN}`;
  }
  const response = await fetch(url, { headers, redirect: 'error', signal: AbortSignal.timeout(60000) });
  requireFact(response.ok, `Candidate evidence request failed: HTTP ${response.status} at ${url}`);
  const bytes = Buffer.from(await response.arrayBuffer());
  requireFact(bytes.length <= 2_000_000, 'Candidate evidence exceeds its bounded size.');
  return raw ? bytes : JSON.parse(bytes.toString('utf8'));
}

async function contents(repo, file, sha) {
  const value = await request(`https://api.github.com/repos/${repo}/contents/${file}?ref=${sha}`);
  requireFact(value.type === 'file' && value.encoding === 'base64', 'Candidate source file is missing or malformed.');
  return JSON.parse(Buffer.from(value.content, 'base64').toString('utf8'));
}

export async function verifyCandidate(root, mergedCommit) {
  requireFact(commit(mergedCommit), 'An exact merged Engine commit is required.');
  requireFact(command(root, 'git', 'rev-parse', 'HEAD').toString().trim() === mergedCommit, 'Candidate gate checkout differs from the publisher source.');
  let reference;
  const defaultBranch = process.env.DEFAULT_BRANCH;
  const pulls = await request(`https://api.github.com/repos/kumwe/engine/commits/${mergedCommit}/pulls?per_page=100`);
  requireFact(pulls.length < 100, 'Candidate merged PR inventory exceeds its bounded verification limit.');
  const uri = process.env.ENGINE_CANDIDATE_ATTESTATION_URI, sha256 = process.env.ENGINE_CANDIDATE_ATTESTATION_SHA256;
  if (uri || sha256) reference = candidateReference({ uri, sha256 });
  else {
    reference = referenceFromPulls(pulls, mergedCommit, defaultBranch);
  }
  const bytes = await request(reference.uri, true);
  requireFact(hash(bytes) === reference.sha256, 'External candidate YAML digest differs.');
  const record = validateRecord(YAML.parse(bytes.toString('utf8'), { uniqueKeys: true, maxAliasCount: 20 }));
  const engine = record.engine_candidate, binding = record.extension_candidate;
  const matchingPulls = mergedPulls(pulls, mergedCommit, defaultBranch).filter(p => p.head.sha === engine.tested_commit);
  requireFact(matchingPulls.length === 1, 'The original candidate must be the exact observed merged Engine PR head.');
  if (!uri && !sha256)
    requireFact(JSON.stringify(referenceFromPulls(matchingPulls, mergedCommit, defaultBranch)) === JSON.stringify(reference),
      'The candidate reference belongs to another merged pull request.');
  // Fetching immutable source objects does not change HEAD or the tested tree.
  if (spawnSync('git', ['cat-file', '-e', engine.tested_commit + '^{commit}'], { cwd: root }).status !== 0)
    command(root, 'git', 'fetch', '--no-tags', 'https://github.com/kumwe/engine.git', engine.tested_commit);
  const handoffBytes = command(root, 'git', 'show', engine.tested_commit + ':MIGRATION-HANDOFF.md');
  const handoffParts = handoffBytes.toString('utf8').split(/^---\s*$/m);
  requireFact(handoffParts.length >= 3 && handoffParts[0].trim() === '', 'Candidate handoff frontmatter is missing.');
  const actual = {
    mergedPullHead: matchingPulls[0].head.sha,
    handoffChangeSet: YAML.parse(handoffParts[1], { uniqueKeys: true, maxAliasCount: 20 }).change_set,
    candidateTree: command(root, 'git', 'rev-parse', engine.tested_commit + '^{tree}').toString().trim(),
    mergedTree: command(root, 'git', 'rev-parse', 'HEAD^{tree}').toString().trim(),
    archiveSha256: hash(command(root, 'git', 'archive', '--format=tar', engine.tested_commit)),
    handoffSha256: hash(handoffBytes),
    bindingTree: (await request(`https://api.github.com/repos/kumwe/kumwe-engine/git/commits/${binding.tested_commit}`)).tree?.sha,
    lock: await contents('kumwe/kumwe-engine', 'resources/engine-lock.json', binding.tested_commit),
  };
  const runMatch = /^https:\/\/github\.com\/kumwe\/kumwe-engine\/actions\/runs\/([1-9][0-9]*)$/.exec(record.ci.run_url);
  const artifactMatch = /^https:\/\/github\.com\/kumwe\/kumwe-engine\/actions\/runs\/([1-9][0-9]*)\/artifacts\/([1-9][0-9]*)$/.exec(record.ci.artifact_url);
  requireFact(runMatch && artifactMatch && runMatch[1] === artifactMatch[1], 'Candidate CI and artifact must identify one exact binding workflow run.');
  actual.run = await request(`https://api.github.com/repos/kumwe/kumwe-engine/actions/runs/${runMatch[1]}`);
  actual.jobs = [];
  for (let page = 1; page <= 100; page++) {
    const result = await request(`https://api.github.com/repos/kumwe/kumwe-engine/actions/runs/${runMatch[1]}/jobs?per_page=100&page=${page}`);
    requireFact(Array.isArray(result.jobs) && Number.isSafeInteger(result.total_count), 'Binding job inventory is malformed.');
    actual.jobs.push(...result.jobs);
    if (actual.jobs.length === result.total_count) break;
    requireFact(result.jobs.length > 0 && actual.jobs.length < result.total_count && page < 100, 'Binding job inventory is incomplete or inconsistent.');
  }
  actual.artifact = await request(`https://api.github.com/repos/kumwe/kumwe-engine/actions/artifacts/${artifactMatch[2]}`);
  const contracts = JSON.parse(fs.readFileSync(path.join(root, 'resources/contracts.json')));
  actual.semanticInputs = semanticInputs(contracts);
  validateObserved(record, actual);
  return { schema: 'kumwe-candidate-publication-gate/v1', status: 'passed', reference,
    tested_engine_commit: engine.tested_commit, tested_engine_tree: engine.tested_tree,
    merged_engine_commit: mergedCommit, merged_engine_tree: actual.mergedTree,
    tested_binding_commit: binding.tested_commit, candidate_archive_sha256: actual.archiveSha256,
    binding_workflow: record.ci.run_url, artifact: record.ci.artifact_url,
    validator_schema_sha256: hash(schemaBytes), publishing_attestation: false };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try { console.log(JSON.stringify(await verifyCandidate(path.resolve(process.argv[2]), process.argv[3]), null, 2)); }
  catch (error) { console.error('Native candidate gate refused: ' + error.message); process.exitCode = 1; }
}
