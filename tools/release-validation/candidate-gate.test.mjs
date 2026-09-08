import assert from 'node:assert/strict';
import test from 'node:test';
import {candidateReference, referenceFromPulls, validateRecord, validateObserved} from './candidate-gate.mjs';
const engine='a'.repeat(40),tree='b'.repeat(40),binding='c'.repeat(40),bindingTree='d'.repeat(40),sha='e'.repeat(64);
const reference={uri:`https://raw.githubusercontent.com/kumwe/extension-sdk/${'f'.repeat(40)}/evidence/native/candidate.yaml`,sha256:sha};
const block='<!-- kumwe-engine-candidate/v1\n'+JSON.stringify(reference)+'\n-->';
function fixture(){
 const record={schema:'kumwe-engine-candidate-attestation/v1',change_set:'KUMWE-CS-2026-043',
  engine_candidate:{repository:'https://github.com/kumwe/engine',tested_commit:engine,tested_tree:tree,source_archive_sha256:sha,handoff_sha256:sha},
  extension_candidate:{repository:'https://github.com/kumwe/kumwe-engine',tested_commit:binding,tested_tree:bindingTree},
  semantic_inputs:[{owner:'kumwe/conversion',version:'0.1.5',manifest_or_corpus_sha256:sha}],
  build:{network_disabled:true,toolchains:['fixture compiler'],operating_systems:['Linux'],architectures:['x86_64'],php_modes_and_versions:['PHP 8.5 NTS']},
  verification:{cmake_consumer:'fixture consumer',pie_source_build:'fixture PIE',module_load_and_reflection:'fixture native module',abi_capability_handshake:'fixture tuple',corpus_results:['fixture parity'],lifecycle_sanitizer_leak:['fixture ASan/UBSan/Valgrind'],hostile_input_and_refusal:['fixture refusal']},
  ci:{run_url:'https://github.com/kumwe/kumwe-engine/actions/runs/123',artifact_url:'https://github.com/kumwe/kumwe-engine/actions/runs/123/artifacts/456'},
  status:'passed',observed_at:'2026-09-08T00:00:00Z',observed_by:'Synthetic test fixture only',publishing_permitted:false};
 const actual={mergedPullHead:engine,handoffChangeSet:'KUMWE-CS-2026-043',candidateTree:tree,mergedTree:tree,archiveSha256:sha,handoffSha256:sha,bindingTree,
  lock:{commit:engine,archive_sha256:sha},run:{id:123,head_sha:binding,head_repository:{full_name:'kumwe/kumwe-engine'},path:'.github/workflows/ci.yml',name:'Native binding candidate',event:'pull_request',status:'completed',conclusion:'success'},
  jobs:['source-release-preparation','binding','address-undefined-sanitizers','clean-pie','whole-boundary-benchmarks'].map((name,i)=>({id:i+1,name,status:'completed',conclusion:'success'})),
  artifact:{id:456,workflow_run:{id:123,head_sha:binding},expired:false,digest:'sha256:'+sha},semanticInputs:[{owner:'kumwe/conversion',version:'0.1.5',sha256:sha}]};
 return {record,actual};
}
test('valid schema and original candidate identities survive an identical-tree merge',()=>{
 const {record,actual}=fixture();assert.equal(validateRecord(record),record);validateObserved(record,actual);
 assert.deepEqual(candidateReference(reference),reference);
 assert.deepEqual(referenceFromPulls([{merged_at:'2026-09-08',merge_commit_sha:engine,base:{ref:'main',repo:{full_name:'kumwe/engine'}},head:{sha:engine,repo:{full_name:'kumwe/engine'}},body:block}],engine),reference);
});
test('only immutable exact SDK evidence coordinates qualify',()=>{
 for(const value of [null,{}, {...reference,uri:reference.uri.replace('f'.repeat(40),'main')}, {...reference,uri:reference.uri.replace('kumwe/extension-sdk','other/extension-sdk')}, {...reference,uri:reference.uri.replace('/native/','/../')},{...reference,sha256:'pending'},{...reference,extra:true}])assert.throws(()=>candidateReference(value));
});
test('missing, unrelated, duplicated or malformed merged PR references cannot authorize publication',()=>{
 const good={merged_at:'2026-09-08',merge_commit_sha:engine,base:{ref:'main',repo:{full_name:'kumwe/engine'}},head:{sha:engine,repo:{full_name:'kumwe/engine'}},body:block};
 for(const pulls of [[],[{...good,body:''}],[{...good,base:{ref:'another-branch',repo:{full_name:'kumwe/engine'}}}],[{...good,head:{sha:engine,repo:{full_name:'other/engine'}}}],[{...good,merged_at:null}],[{...good,merge_commit_sha:binding}],[{...good,base:{repo:{full_name:'other/engine'}}}],[{...good,body:block+'\n'+block}],[good,good],[{...good,body:'<!-- kumwe-engine-candidate/v1\nnot-json\n-->'}]])assert.throws(()=>referenceFromPulls(pulls,engine));
});
test('the complete authoritative schema is mandatory',()=>{
 for(const field of Object.keys(fixture().record)){const {record}=fixture();delete record[field];assert.throws(()=>validateRecord(record),field);}
});
for(const [name,mutate] of Object.entries({
 'unrelated handoff change set':(r,a)=>{r.change_set='KUMWE-CS-2099-999';},'unrelated merged PR head':(r,a)=>{a.mergedPullHead=binding;},
 'failed attestation':(r,a)=>{r.status='failed';},'superseded attestation':(r,a)=>{r.status='superseded';},
 'publication authority substituted':(r,a)=>{r.publishing_permitted=true;},'online consumer':(r,a)=>{r.build.network_disabled=false;},
 'missing corpus evidence':(r,a)=>{r.verification.corpus_results=[];},'missing sanitizer evidence':(r,a)=>{r.verification.lifecycle_sanitizer_leak=[];},
 'changed merged tree':(r,a)=>{a.mergedTree=bindingTree;},'different candidate tree':(r,a)=>{a.candidateTree=bindingTree;},
 'changed original archive':(r,a)=>{a.archiveSha256='f'.repeat(64);},'changed handoff':(r,a)=>{a.handoffSha256='f'.repeat(64);},
 'different binding tree':(r,a)=>{a.bindingTree=tree;},'different embedded commit':(r,a)=>{a.lock.commit=binding;},
 'different embedded archive':(r,a)=>{a.lock.archive_sha256='f'.repeat(64);},'different CI source':(r,a)=>{a.run.head_sha=engine;},
 'substitute workflow':(r,a)=>{a.run.path='.github/workflows/other.yml';},'failed CI':(r,a)=>{a.run.conclusion='failure';},
 'missing benchmark lane':(r,a)=>{a.jobs.pop();},'skipped binding lane':(r,a)=>{a.jobs[1].conclusion='skipped';},
 'duplicate job identity':(r,a)=>{a.jobs[1].id=a.jobs[0].id;},'wrong artifact run':(r,a)=>{a.artifact.workflow_run.id=999;},
 'wrong artifact source':(r,a)=>{a.artifact.workflow_run.head_sha=engine;},'expired artifact':(r,a)=>{a.artifact.expired=true;},
 'unidentified artifact bytes':(r,a)=>{delete a.artifact.digest;},'missing owner API or corpus':(r,a)=>{a.semanticInputs.push({owner:'kumwe/computation',version:'0.1.1',sha256:sha});},
 'wrong owner version':(r,a)=>{r.semantic_inputs[0].version='0.1.4';}
}))test(name+' refuses publication',()=>{const {record,actual}=fixture();mutate(record,actual);assert.throws(()=>validateObserved(record,actual));});
