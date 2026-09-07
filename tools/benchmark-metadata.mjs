import {cpus, platform, release, arch} from 'node:os';
import {execFileSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
const run = (command,args) => execFileSync(command,args,{encoding:'utf8'}).trim();
console.log(JSON.stringify({source:run('git',['rev-parse','HEAD']),platform:platform(),os_release:release(),architecture:arch(),cpu:cpus()[0]?.model,cpu_count:cpus().length,compiler:run(process.env.CXX ?? 'c++',['--version']),cmake:run('cmake',['--version']),corpus_sha256:JSON.parse(readFileSync('resources/capabilities.json','utf8')).corpus_sha256,build_flags:readFileSync('build/CMakeCache.txt','utf8').split('\n').filter(line => /^CMAKE_(BUILD_TYPE|CXX_FLAGS.*):/.test(line))},null,2));
