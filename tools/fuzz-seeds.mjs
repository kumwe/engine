import {mkdirSync, writeFileSync} from 'node:fs';
mkdirSync('artifacts/fuzz-seeds', {recursive: true});
mkdirSync('artifacts/fuzz-regressions', {recursive: true});
const seeds = [Buffer.from('KEC1\x01\x00\x00\x00\x01\x00\x00\x00', 'binary')];
const canonicalMetadata = Buffer.from(JSON.stringify({wire_version: 1,
  corpus_digest: '84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250',
  profile: 'kumwe-canonical-json/generic-v1', operation: 'encode'}));
const canonicalHeader = Buffer.alloc(8);
canonicalHeader.write('KEC1'); canonicalHeader.writeUInt32LE(canonicalMetadata.length, 4);
const frame = (tag, payload = Buffer.alloc(0)) => {
  const header = Buffer.alloc(5); header[0] = tag; header.writeUInt32LE(payload.length, 1);
  return Buffer.concat([header, payload]);
};
const bytes = input => {
  const payload = Buffer.from(input); const length = Buffer.alloc(4); length.writeUInt32LE(payload.length);
  return Buffer.concat([length, payload]);
};
const batchMetadata = JSON.stringify({wire_version: 1, limits: {max_input_bytes: 1048576,
  max_output_bytes: 1048576, max_documents: 64, max_findings: 64,
  max_instructions: 100000, max_milliseconds: 30000}});
const batchCount = Buffer.alloc(4); batchCount.writeUInt32LE(1);
seeds.push(Buffer.concat([Buffer.from('KEB1'), bytes(batchMetadata), batchCount,
  bytes('row'), bytes('{"fields":{},"lines":null}') ]));
const one = Buffer.alloc(4); one.writeUInt32LE(1);
for (const value of [frame(0), frame(4, Buffer.from('0000000000000080', 'hex')),
  frame(5, Buffer.from('a\x00b')), frame(6, Buffer.concat([one, frame(5, Buffer.from('key')), frame(5, Buffer.from('value'))]))]) {
  seeds.push(Buffer.concat([canonicalHeader, canonicalMetadata, value]));
}
for (const [op, p, s, rp, rs, mode, left, right] of [
  [0, 10, 2, 0, 0, 0, '-0.00', ''],
  [1, 10, 2, 10, 2, 0, '-1.00', '2.00'],
  [2, 12, 2, 12, 8, 0, '25000.00', '0.04938240'],
  [3, 8, 4, 8, 2, 2, '1.2350', ''],
  [0, 65, 65, 0, 0, 0, '-0.' + '1'.repeat(65), ''],
]) {
  const header = Buffer.alloc(30);
  header.write('KED1'); header.writeUInt32LE(1, 4); header.writeUInt32LE(1048576, 8); header.writeBigUInt64LE(1000000000n, 12);
  [op,p,s,rp,rs,mode].forEach((item,index) => header[20+index] = item);
  header.writeUInt16LE(left.length, 26); header.writeUInt16LE(right.length, 28);
  seeds.push(Buffer.concat([header, Buffer.from(left), Buffer.from(right)]));
}
seeds.forEach((seed, index) => writeFileSync(`artifacts/fuzz-seeds/${index}`, seed));
