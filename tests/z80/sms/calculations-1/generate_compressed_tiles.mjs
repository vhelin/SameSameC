import {writeFileSync} from 'node:fs';

// Original synthetic tile patterns: blank tiles, varied rows and solid fills.
const bytes = Uint8Array.from([
  ...Array(128).fill(0),
  ...Array.from({length: 64}, (_, index) => (index * 37) & 255),
  ...Array(16).fill(255),
  ...Array(16).fill(170),
  ...Array.from({length: 32}, (_, index) => (index * 13 + 7) & 255)
]);
const packed = [];
for (let cursor = 0; cursor < bytes.length;) {
  let end = cursor + 1;
  while (end < bytes.length && end - cursor < 128 && bytes[end] === bytes[cursor]) end++;
  if (end - cursor > 1) {
    packed.push(257 - (end - cursor), bytes[cursor]);
    cursor = end;
  } else {
    const start = cursor++;
    while (cursor < bytes.length && cursor - start < 128 &&
        (cursor + 1 === bytes.length || bytes[cursor] !== bytes[cursor + 1])) cursor++;
    packed.push(cursor - start - 1, ...bytes.slice(start, cursor));
  }
}

function crc16(data) {
  let crc = 65535;
  for (const byte of data) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; bit++) crc = ((crc << 1) ^ ((crc & 32768) ? 0x1021 : 0)) & 65535;
  }
  return crc;
}

if (crc16(Buffer.from('123456789')) !== 0x29b1) throw new Error('CRC-16/CCITT-FALSE check failed');
writeFileSync(new URL('tiles.packbits', import.meta.url), Uint8Array.from(packed));
console.log(`tiles.packbits: ${bytes.length} -> ${packed.length} bytes, CRC-16/CCITT-FALSE=$${crc16(bytes).toString(16).toUpperCase()}`);
