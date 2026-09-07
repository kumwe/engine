import {mkdirSync, writeFileSync} from 'node:fs';
mkdirSync('artifacts/fuzz-seeds', {recursive: true});
mkdirSync('artifacts/fuzz-regressions', {recursive: true});
const seeds = [Buffer.from('KEC1\x01\x00\x00\x00\x01\x00\x00\x00', 'binary')];
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
