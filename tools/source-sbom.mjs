import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
const git = (...args) => execFileSync('git', args, {encoding:'utf8'}).trim();
const commit = git('rev-parse', 'HEAD');
// Inventory exactly the committed export; test-only PHP oracles are export-ignored.
const paths = execFileSync('bash', ['-o','pipefail','-c','git archive --format=tar HEAD | tar -tf -'],
  {encoding:'utf8',maxBuffer:16*1024*1024}).trim().split('\n').filter(path => path && !path.endsWith('/')).sort();
const files = paths.map((file, index) => {
  const bytes = execFileSync('git',['show',`${commit}:${file}`], {maxBuffer:32*1024*1024});
  return ({
  SPDXID:`SPDXRef-File-${index}`, fileName:`./${file}`,
  checksums:[{algorithm:'SHA1',checksumValue:createHash('sha1').update(bytes).digest('hex')},{algorithm:'SHA256',checksumValue:createHash('sha256').update(bytes).digest('hex')}],
  licenseConcluded:'NOASSERTION', licenseInfoInFiles:['NOASSERTION'], copyrightText:'NOASSERTION',
}); });
const pcre = JSON.parse(execFileSync('git', ['show', `${commit}:resources/pcre2-source.json`], {encoding:'utf8'}));
const verification = createHash('sha1').update(files.map(file => file.checksums[0].checksumValue).sort().join('')).digest('hex');
const document = {
  spdxVersion:'SPDX-2.3', dataLicense:'CC0-1.0', SPDXID:'SPDXRef-DOCUMENT',
  name:'Kumwe Engine development source inventory', documentNamespace:`https://github.com/kumwe/engine/sbom/${commit}`,
  creationInfo:{created:new Date(git('show','-s','--format=%cI','HEAD')).toISOString().replace('.000Z','Z'),creators:['Tool: kumwe-engine-source-sbom']},
  packages:[{SPDXID:'SPDXRef-Engine',name:'kumwe/engine',versionInfo:commit,downloadLocation:`git+https://github.com/kumwe/engine.git@${commit}`,filesAnalyzed:true,packageVerificationCode:{packageVerificationCodeValue:verification},licenseConcluded:'NOASSERTION',licenseDeclared:'Apache-2.0 AND BSD-3-Clause AND BSD-2-Clause AND PHP-3.01',copyrightText:'NOASSERTION',supplier:'Organization: Kumwe'}, {SPDXID:'SPDXRef-PCRE2',name:'PCRE2',versionInfo:`10.42+${pcre.commit}`,downloadLocation:`git+https://github.com/PCRE2Project/pcre2.git@${pcre.commit}`,filesAnalyzed:false,licenseConcluded:'BSD-3-Clause AND BSD-2-Clause',licenseDeclared:'BSD-3-Clause AND BSD-2-Clause',copyrightText:'Copyright (c) 1997-2022 University of Cambridge'}],
  files,
  relationships:[{spdxElementId:'SPDXRef-Engine',relatedSpdxElement:'SPDXRef-PCRE2',relationshipType:'DEPENDS_ON'},{spdxElementId:'SPDXRef-DOCUMENT',relatedSpdxElement:'SPDXRef-Engine',relationshipType:'DESCRIBES'},...files.map(file => ({spdxElementId:'SPDXRef-Engine',relatedSpdxElement:file.SPDXID,relationshipType:'CONTAINS'}))],
};
document.packages[0].licenseDeclared += ' AND Unicode-3.0';
document.packages.push({SPDXID:'SPDXRef-Unicode',name:'Unicode character data',
  versionInfo:'17.0.0 casing / 15.1.0 NFC',downloadLocation:'https://www.unicode.org/Public/',
  filesAnalyzed:false,licenseConcluded:'Unicode-3.0',licenseDeclared:'Unicode-3.0',
  copyrightText:'Copyright Unicode, Inc.'});
document.relationships.push({spdxElementId:'SPDXRef-Engine',relatedSpdxElement:'SPDXRef-Unicode',relationshipType:'DEPENDS_ON'});
process.stdout.write(JSON.stringify(document,null,2)+'\n');
