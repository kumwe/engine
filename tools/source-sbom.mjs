import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
const git = (...args) => execFileSync('git', args, {encoding:'utf8'}).trim();
const commit = git('rev-parse', 'HEAD');
const paths = execFileSync('git', ['ls-files', '-z'], {encoding:'utf8'}).split('\0').filter(Boolean).sort();
const files = paths.map((file, index) => {
  const bytes = execFileSync('git',['show',`${commit}:${file}`]);
  return ({
  SPDXID:`SPDXRef-File-${index}`, fileName:`./${file}`,
  checksums:[{algorithm:'SHA1',checksumValue:createHash('sha1').update(bytes).digest('hex')},{algorithm:'SHA256',checksumValue:createHash('sha256').update(bytes).digest('hex')}],
  licenseConcluded:'NOASSERTION', licenseInfoInFiles:['NOASSERTION'], copyrightText:'NOASSERTION',
}); });
const verification = createHash('sha1').update(files.map(file => file.checksums[0].checksumValue).sort().join('')).digest('hex');
const document = {
  spdxVersion:'SPDX-2.3', dataLicense:'CC0-1.0', SPDXID:'SPDXRef-DOCUMENT',
  name:'Kumwe Engine development source inventory', documentNamespace:`https://github.com/kumwe/engine/sbom/${commit}`,
  creationInfo:{created:new Date(git('show','-s','--format=%cI','HEAD')).toISOString().replace('.000Z','Z'),creators:['Tool: kumwe-engine-source-sbom']},
  packages:[{SPDXID:'SPDXRef-Engine',name:'kumwe/engine',versionInfo:commit,downloadLocation:`git+https://github.com/kumwe/engine.git@${commit}`,filesAnalyzed:true,packageVerificationCode:{packageVerificationCodeValue:verification},licenseConcluded:'Apache-2.0',licenseDeclared:'Apache-2.0',copyrightText:'NOASSERTION',supplier:'Organization: Kumwe'}],
  files,
  relationships:[{spdxElementId:'SPDXRef-DOCUMENT',relatedSpdxElement:'SPDXRef-Engine',relationshipType:'DESCRIBES'},...files.map(file => ({spdxElementId:'SPDXRef-Engine',relatedSpdxElement:file.SPDXID,relationshipType:'CONTAINS'}))],
};
process.stdout.write(JSON.stringify(document,null,2)+'\n');
