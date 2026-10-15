import {spawnSync} from 'node:child_process';
import {mkdtempSync, mkdirSync, readFileSync, writeFileSync, readdirSync, openSync, closeSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const output = process.env.RA_EXECUTION_DIR || mkdtempSync(join(tmpdir(), 'ra-execution-'));
const emulatorPath = process.env.RA_Z80_EMULATOR;
if (!emulatorPath) throw new Error('Set RA_Z80_EMULATOR to z80-emulator@2.3.0/dist/index.js');
const {Z80} = await import(pathToFileURL(resolve(emulatorPath)).href);
const compiler = process.env.SAMESAMECC || 'samesamecc';
const linker = process.env.SAMESAMECL || 'samesamecl';
const cases = Number(process.env.RA_CASES || 64);
if (!Number.isInteger(cases) || cases < 1 || cases > 4096) throw new Error('RA_CASES must be 1..4096');
mkdirSync(output, {recursive: true});

function run(command, args, cwd, log) {
  const descriptor = openSync(join(cwd, log), 'w');
  let result;
  try {
    result = spawnSync(command, args.map(argument => argument.replaceAll('\\', '/')),
      {cwd, stdio: ['ignore', descriptor, descriptor]});
  } finally {
    closeSync(descriptor);
  }
  if (result.error || result.status !== 0)
    throw new Error(`${command} failed; see ${join(cwd, log)}: ${result.error || result.status}`);
}

const fixture = join(root, 'tests/z80/sms/allocator_ra_core_liveness_extraction');
const sources = readdirSync(join(root, 'compiler')).filter(name => name.endsWith('.c') &&
  name !== 'main.c' && name !== 'pass_5.c').map(name => join(root, 'compiler', name));
const harness = process.env.RA_VERIFIER || join(output,
  process.platform === 'win32' ? 'allocation_verify_test.exe' : 'allocation_verify_test');
if (!process.env.RA_VERIFIER)
  run(process.env.CC || 'gcc', ['-ansi', '-pedantic', '-Wall', '-DNDEBUG', '-o', harness,
    join(fixture, 'allocation_verify_test.c'), ...sources, '-lm'], output, 'verifier-build.log');
for (const flags of [[], ['-q']]) {
  const log = flags.length ? 'verifier-quiet.log' : 'verifier.log';
  run(harness, flags, output, log);
  const verifierLog = readFileSync(join(output, log), 'utf8');
  if ((verifierLog.match(/result=rejected unchanged=yes status=complete/g) || []).length !== 16 ||
    (verifierLog.match(/result=accepted unchanged=yes status=complete/g) || []).length !== 7 ||
    !verifierLog.includes('allocation_mutation_test: invalid_tail unchanged=yes status=complete') ||
    !verifierLog.includes('allocation_mutation_test: displaced_arg2 cleared=yes status=complete') ||
    !verifierLog.includes('spill_slot_test: cases=8 overlap_width_boundary_redefinition_controls=yes tacs_unchanged=yes status=complete') ||
    !verifierLog.includes('allocation_cost_test: nearer_rejected=yes farther_reused=yes status=complete') ||
    !verifierLog.includes('candidate_definition_test: read_before_write=yes previous_definition_excluded=yes status=complete'))
    throw new Error('Verifier coverage count mismatch');
}

const programs = [];
for (let variant = 0; variant < 8; variant++) {
  const constant = 1 + variant * 7;
  const operator = ['+', '-', '&', '|', '^'][variant % 5];
  const functions = [
    ['scalar_byte', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; return (c+c)+saved;', [1, 1, 1], 1],
    ['scalar_signed_byte', 's8 a, s8 b, s8 c', 's8 saved = a^b; return (c|7)^saved;', [1, 1, 1], 1],
    ['scalar_signed_word', 's16 a, s16 b, s16 c', 's16 saved = a^b; return (c|7)^saved;', [2, 2, 2], 2],
    ['scalar_widen', 'u8 a, u8 b, u8 c', 'u16 saved = a+b; return (c+c)+saved;', [1, 1, 1], 2],
    ['scalar_word', 'u16 a, u16 b, u16 c', 'u16 saved = a^b; return (c+c)+saved;', [2, 2, 2], 2],
    ['scalar_copy', 'u16 a, u16 b, u16 c', 'u16 saved = a; return (b+c)+saved;', [2, 2, 2], 2],
    ['scalar_complement_sub', 'u16 a, u16 b, u16 c', 'return (a+b)-(~c);', [2, 2, 2], 2],
    ['scalar_multi_word', 'u16 a, u16 b, u16 c', 'u16 left = a+b; u16 saved = ~c; return ((left+saved)^saved)-saved;', [2, 2, 2], 2],
    ['scalar_multi_rmw', 'u16 a, u16 b, u16 c', 'u16 saved = ~c; return ((a+saved)^saved)+b;', [2, 2, 2], 2],
    ['scalar_multi_byte', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; return ((c+saved)^saved)+saved;', [1, 1, 1], 1],
    ['scalar_split_byte', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; return ((((first+loaded)^saved)+saved)^saved)+saved;', [1, 1, 1], 1],
    ['scalar_cost_reject_byte', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; return ((first+loaded)^saved)+saved;', [1, 1, 1], 1],
    ['scalar_split_word', 'u16 a, u16 b, u16 c', 'u16 saved = ~c; u16 first = a+saved; u16 loaded = ra_buffer[1]; return ((first+loaded)^saved)+b+saved;', [2, 2, 2], 2],
    ['scalar_word_index', 'u16 a, u16 b, u8 c', `u16 out = a+b; ra_buffer[${constant}] = c; return out+ra_buffer[${constant}];`, [2, 2, 1], 2],
    ['scalar_narrow', 'u16 a, u16 b, u16 c', 'u8 saved = a+b; return (c+c)+saved;', [2, 2, 2], 2],
    ['scalar_before_call', 'u8 a, u8 b, u8 c', `u8 called = ra_arithmetic_${variant}(a,b,c); u8 saved = a^b; return (called+called)+saved;`, [1, 1, 1], 1],
    ['scalar_after_call', 'u8 a, u8 b, u8 c', `u8 saved = a^b; u8 out = (c+c)+saved; ra_buffer[0] = ra_arithmetic_${variant}(a,b,c); return out;`, [1, 1, 1], 1],
    ['scalar_call_argument', 'u8 a, u8 b, u8 c', `u8 saved = a^b; return ra_arithmetic_${variant}(saved,b,c);`, [1, 1, 1], 1],
    ['scalar_across_call', 'u8 a, u8 b, u8 c', `u8 saved = a^b; u8 called = ra_arithmetic_${variant}(a,b,c); return (called+c)+saved;`, [1, 1, 1], 1],
    ['scalar_signed_call', 'u8 a, u8 b, u8 c', 'u16 first = a; return first+ra_signed_helper(a,b,c);', [1, 1, 1], 2],
    ['scalar_slots', 'u8 a, u8 b, u8 c', 'ra_buffer[0] = ra_buffer[a]^ra_buffer[b]^(a+b); return ra_buffer[c]^ra_buffer[b+1]^(b+c);', [1, 1, 1], 1],
    ['scalar_remat_byte', 'u8 first, u8 second', 'u8 *base = &ra_buffer[3]; u8 out = base[first]; out = out+base[second]; return out;', [1, 1], 1],
    ['scalar_remat_word', 'u8 first, u8 second', 'u16 *base = &ra_words[3]; u16 out = base[first]; out = out+base[second]; return out;', [1, 1], 2],
    ['scalar_remat_pointer', 'u8 *source, u8 first, u8 second', 'u8 *base = &source[3]; u8 out = base[first]; out = out+base[second]; return out;', [2, 1, 1], 1],
    ['word_bc', 'u16 a, u16 b, u16 c', `return (c+c)+(a${operator}b);`, [2, 2, 2], 2],
    ['arithmetic', `u8 a, u8 b, u8 c`, `return (a+b)+((c+${constant})+c);`, [1, 1, 1], 1],
    ['word', 'u16 a, u16 b, u16 c', `return (a+b)+((c+${constant})+c);`, [2, 2, 2], 2],
    ['branch', 'u8 a, u8 b, u8 c', `u8 out; if (a < b) { out = (a+c)^${constant}; } else { out = (b+c)^${constant}; } return out;`, [1, 1, 1], 1],
    ['loop', 'u8 count, u8 value', `while (count) { value = (value+${constant})^count; count = count-1; } return value;`, [1, 1], 1],
    ['array', 'u8 first, u8 second, u8 a, u8 b', 'return ra_buffer[first]^ra_buffer[second]^(a+b);', [1, 1, 1, 1], 1],
    ['pointer', 'u8 *target, u8 left, u8 right, u8 value', `target[left+right] = target[left+right]+value+${constant}; return target[left+right];`, [2, 1, 1, 1], 1],
    ['call', 'u8 a, u8 b, u8 c', `return ra_arithmetic_${variant}(a,b,c)^${constant};`, [1, 1, 1], 1],
    ['pressure', 'u8 left, u8 right, u8 value', `u16 first = left+right; u16 second = right+value; u16 third = left+value; u16 fourth = first^${constant}; first = first+ra_arithmetic_${variant}(left,right,value); return (first+second)^(third+fourth);`, [1, 1, 1], 2],
    ['walk', 'u8 *target, u8 start, u8 count, u8 value', `u8 index = start; u8 total = value; while (count) { u8 previous = target[index]; target[index] = previous+total+${constant}; total = total^previous; index = index+${1 + variant % 3}; count = count-1; } return total;`, [2, 1, 1, 1], 1],
    ['nested', 'u8 *target, u8 start, u8 rows, u8 columns', `u8 row = 0; u16 total = ${constant}; while (row < rows) { u8 column = 0; while (column < columns) { u8 index = start+row*8+column; u8 previous = target[index]; total = total+previous+row; target[index] = previous^column^${constant}; column = column+1; } row = row+1; } return total;`, [2, 1, 1, 1], 2],
    ['join_call', 'u8 *target, u8 index, u8 selector, u8 value', `u16 retained = target[index]+${constant}; u8 called = ra_arithmetic_${variant}(value,selector,target[index]); if (selector&${1 << (variant % 4)}) { target[index] = called+value; } else { target[index+1] = called^value; retained = retained+selector; } return retained+target[index]+target[index+1];`, [2, 1, 1, 1], 2]
  ];
  for (const [family, parameters, body, widths, returnWidth] of functions) {
    if (process.env.RA_FAMILIES && family !== 'arithmetic' && !process.env.RA_FAMILIES.split(',').includes(family)) continue;
    const scalarFamily = family.startsWith('scalar_') || family === 'word_bc';
    if (family !== 'arithmetic' && scalarFamily !== (process.env.RA_SCALAR_CASES === '1')) continue;
    if (scalarFamily && family !== 'word_bc' && variant > 1) continue;
    const name = `ra_${family}_${variant}`;
    programs.push({name, family, variant, constant, widths, returnWidth,
      source: `${family.startsWith('scalar_signed') ? 's' : 'u'}${returnWidth * 8} ${name}(${parameters}) { ${body} }`});
  }
}
const source = 'u8 ra_buffer[128];\nu16 ra_words[64];\ns8 ra_signed_helper(u8 a, u8 b, u8 c) { return a+b+c; }\n' + programs.map(program => program.source).join('\n') + '\nvoid main(void) {}\n';
writeFileSync(join(output, 'generated.ssc'), source);
const modes = [['default', []], ['no_ra', ['-no-ra']], ['all_spill', ['-ra-all-spill']],
  ['ra', ['-ra']], ['ra_quiet', ['-ra', '-q']]];

function readImage(directory) {
  const symbols = new Map();
  const symbolFile = readdirSync(directory).find(name => name.endsWith('.sym'));
  if (!symbolFile) throw new Error(`Missing symbol file in ${directory}`);
  for (const line of readFileSync(join(directory, symbolFile), 'utf8').split(/\r?\n/)) {
    const match = line.match(/^00:([0-9a-f]+)\s+(\S+)$/i);
    if (match) symbols.set(match[2], parseInt(match[1], 16));
  }
  return {rom: readFileSync(join(directory, 'linked.sms')), symbols};
}

const compiled = new Map();
for (const [mode, flags] of modes) {
  const directory = join(output, mode);
  mkdirSync(directory, {recursive: true});
  writeFileSync(join(directory, 'main.ssc'), source);
  const start = performance.now();
  run(compiler, ['-mz80', '-rs', '3', ...flags, '-o', 'main.asm', 'main.ssc'], directory, 'compiler.log');
  const compileSeconds = (performance.now() - start) / 1000;
  run(linker, ['-lsms', '-v', '-o', 'linked.sms', 'main.asm'], directory, 'linker.log');
  const image = readImage(directory);
  for (const name of ['ra_buffer', ...programs.map(program => program.name)])
    if (!image.symbols.has(name)) throw new Error(`Missing symbol ${name} in ${mode}`);
  compiled.set(mode, {...image, compileSeconds});
}
if (!compiled.get('default').rom.equals(compiled.get('no_ra').rom))
  throw new Error('Default/-no-ra ROM mismatch');
if (!compiled.get('ra').rom.equals(compiled.get('ra_quiet').rom))
  throw new Error('Traced/quiet allocator ROM mismatch');
if (!readFileSync(join(output, 'ra/main.asm')).equals(readFileSync(join(output, 'ra_quiet/main.asm'))))
  throw new Error('Traced/quiet allocator assembly mismatch');
const allocatorLog = readFileSync(join(output, 'ra/compiler.log'), 'utf8');
for (const program of process.env.RA_BASELINE === '1' ? [] : programs) {
  const lines = allocatorLog.split(/\r?\n/).filter(line => line.includes(`function=${program.name} `));
  if (program.family === 'scalar_split_byte' &&
      (lines.filter(line => line.includes('scalar_split_transfer') && line.endsWith('kind=store')).length !== 1 ||
       lines.filter(line => line.includes('scalar_split_transfer') && line.endsWith('kind=reload')).length !== 1 ||
       lines.filter(line => line.includes('reload_reuse')).length !== 3))
    throw new Error(`Split transfer counts changed: ${program.name}`);
  if (program.family === 'scalar_cost_reject_byte' &&
      (lines.some(line => line.includes('scalar_split_transfer')) ||
       !lines.some(line => /profitability .*known=yes saved_cycles=-7 saved_bytes=-4 .*decision=3 /.test(line))))
    throw new Error(`Unprofitable split accepted: ${program.name}`);
  if (program.family.startsWith('scalar_remat_')) {
    const assembly = readFileSync(join(output, 'ra/main.asm'), 'utf8').split(`${program.name}:`)[1].split('.ENDS')[0];
    if (assembly.includes('; rematerialize ') !== (program.family !== 'scalar_remat_pointer'))
      throw new Error(`Constant-address rematerialization mismatch: ${program.name}`);
  }
}

function execute(code, entry, parameters, widths, buffer, bufferAddress, returnWidth) {
  const memory = new Uint8Array(65536).fill(0xa5);
  memory.set(code.subarray(0, 0xc000));
  memory.set(buffer, bufferAddress);
  const frame = 0xd000;
  const sentinel = 0x7fff;
  memory[frame - 1] = sentinel & 255;
  memory[frame] = sentinel >> 8;
  const resultOffset = frame - 3 - returnWidth;
  let offset = resultOffset;
  parameters.forEach((value, index) => {
    offset -= widths[index];
    memory[offset] = value & 255;
    if (widths[index] === 2) memory[offset + 1] = value >> 8;
  });
  let frameReads = 0, frameWrites = 0;
  const hal = {
    tStateCount: 0,
    readMemory: address => {
      address &= 65535;
      if (address >= frame - 256 && address < frame - 1) frameReads++;
      return memory[address];
    },
    writeMemory: (address, value) => {
      address &= 65535;
      if (!(address >= frame - 256 && address < frame - 1) &&
          !(address >= bufferAddress && address < bufferAddress + buffer.length))
        throw new Error(`Out-of-bounds write ${address.toString(16)}`);
      if (address >= frame - 256 && address < frame - 1) frameWrites++;
      memory[address] = value & 255;
    },
    contendMemory: () => {}, contendPort: () => {}, readPort: () => 0,
    writePort: () => { throw new Error('Unexpected port write'); }
  };
  const cpu = new Z80(hal);
  cpu.regs.pc = entry;
  cpu.regs.de = frame;
  cpu.regs.sp = frame - 1;
  let instructions = 0;
  while (cpu.regs.pc !== sentinel) {
    if (++instructions > 20000 || cpu.regs.pc >= 0xc000) throw new Error('Instruction bound/PC escape');
    cpu.step();
  }
  if (cpu.regs.de !== frame || cpu.regs.sp !== frame + 1) throw new Error('Frame/stack restoration failure');
  return {value: memory[resultOffset] + (returnWidth === 2 ? memory[resultOffset + 1] * 256 : 0),
    buffer: memory.slice(bufferAddress, bufferAddress + buffer.length), instructions, cycles: hal.tStateCount,
    frame_reads: frameReads, frame_writes: frameWrites};
}

const totals = Object.fromEntries(['default', 'all_spill', 'ra'].map(mode => [mode, {cases: 0, instructions: 0, cycles: 0, frame_reads: 0, frame_writes: 0}]));
const programTotals = {};
for (const program of programs) {
  programTotals[program.name] = Object.fromEntries(Object.keys(totals).map(mode => [mode, {cases: 0, instructions: 0, cycles: 0, frame_reads: 0, frame_writes: 0}]));
  for (let seed = 0; seed < cases; seed++) {
    const byte = (seed * 73 + program.variant * 19) & 255;
    const other = (seed * 29 + 17) & 255;
    const third = (seed * 47 + 3) & 255;
    const buffer = Uint8Array.from({length: 128}, (_, index) => (index * 47 + seed * 11 + program.variant) & 255);
    const expectedBuffer = buffer.slice();
    let parameters = [byte, other, third];
    let expected;
    switch (program.family) {
      case 'scalar_signed_call': {
        const called = (byte + other + third) & 255;
        expected = (byte + (called >= 128 ? called - 256 : called)) & 65535;
        break;
      }
      case 'scalar_slots':
        parameters = [1 + (seed & 15), 1 + ((seed * 3) & 15), 17 + (seed & 15)];
        expectedBuffer[0] = buffer[parameters[0]] ^ buffer[parameters[1]] ^ ((parameters[0] + parameters[1]) & 255);
        expected = buffer[parameters[2]] ^ buffer[parameters[1] + 1] ^ ((parameters[1] + parameters[2]) & 255);
        break;
      case 'scalar_remat_byte':
      case 'scalar_remat_word':
      case 'scalar_remat_pointer': {
        parameters = [seed & 15, (seed * 7) & 15];
        const width = program.family === 'scalar_remat_word' ? 2 : 1;
        const first = (3 + parameters[0]) * width;
        const second = (3 + parameters[1]) * width;
        expected = width === 2 ? ((buffer[first] | buffer[first + 1] << 8) + (buffer[second] | buffer[second + 1] << 8)) & 65535 :
          (buffer[first] + buffer[second]) & 255;
        if (program.family === 'scalar_remat_pointer') parameters.unshift(0);
        break;
      }
      case 'scalar_word_index':
        parameters = [(seed * 7919 + program.variant * 1009) & 65535, (seed * 5113 + 65535) & 65535, third];
        expectedBuffer[program.constant] = third;
        expected = (parameters[0] + parameters[1] + third) & 65535;
        break;
      case 'scalar_word':
      case 'scalar_signed_word':
      case 'scalar_copy':
      case 'scalar_complement_sub':
      case 'scalar_multi_word':
      case 'scalar_multi_rmw':
      case 'scalar_split_word':
      case 'scalar_narrow':
      case 'word_bc': {
        parameters = [(seed * 7919 + program.variant * 1009) & 65535, (seed * 5113 + 65535) & 65535, (seed * 3571 + 32767) & 65535];
        const [first, second, thirdWord] = parameters;
        if (program.family === 'scalar_split_word') {
          const saved = thirdWord ^ 65535;
          expected = (((((first + saved + buffer[1]) & 65535) ^ saved) + second) + saved) & 65535;
          break;
        }
        if (program.family.startsWith('scalar_multi_')) {
          const saved = thirdWord ^ 65535;
          expected = program.family === 'scalar_multi_word' ?
            ((((first + second + saved) & 65535) ^ saved) - saved) & 65535 :
            ((((first + saved) & 65535) ^ saved) + second) & 65535;
          break;
        }
        if (program.family === 'scalar_signed_word') {
          expected = (thirdWord | 7) ^ first ^ second;
          break;
        }
        if (program.family === 'scalar_complement_sub') {
          expected = (first + second - (thirdWord ^ 65535)) & 65535;
          break;
        }
        const saved = program.family === 'scalar_narrow' ? (first + second) & 255 :
          program.family === 'scalar_copy' ? first :
          program.family === 'scalar_word' ? first ^ second :
          [first + second, first - second, first & second, first | second, first ^ second][program.variant % 5];
        expected = ((program.family === 'scalar_copy' ? second + thirdWord : 2 * thirdWord) + saved) & 65535;
        break;
      }
      case 'scalar_byte': expected = (2 * third + (byte ^ other)) & 255; break;
      case 'scalar_multi_byte': {
        const saved = byte ^ other;
        expected = ((((third + saved) & 255) ^ saved) + saved) & 255;
        break;
      }
      case 'scalar_split_byte':
      case 'scalar_cost_reject_byte': {
        const saved = byte ^ other;
        expected = (((((third + saved + buffer[1]) & 255) ^ saved) + saved)) & 255;
        if (program.family === 'scalar_split_byte') expected = ((expected ^ saved) + saved) & 255;
        break;
      }
      case 'scalar_signed_byte': expected = ((third | 7) ^ byte ^ other) & 255; break;
      case 'scalar_widen': expected = 2 * third + ((byte + other) & 255); break;
      case 'scalar_before_call': expected = (2 * ((byte + other + 2 * third + program.constant) & 255) + (byte ^ other)) & 255; break;
      case 'scalar_after_call':
        expectedBuffer[0] = (byte + other + 2 * third + program.constant) & 255;
        expected = (2 * third + (byte ^ other)) & 255;
        break;
      case 'scalar_call_argument': expected = ((byte ^ other) + other + 2 * third + program.constant) & 255; break;
      case 'scalar_across_call': expected = (byte + other + 3 * third + program.constant + (byte ^ other)) & 255; break;
      case 'word':
        parameters = [(seed * 7919 + program.variant * 1009) & 65535, (seed * 5113 + 65535) & 65535, (seed * 3571 + 32767) & 65535];
        expected = (parameters[0] + parameters[1] + 2 * parameters[2] + program.constant) & 65535;
        break;
      case 'arithmetic': expected = (byte + other + 2 * third + program.constant) & 255; break;
      case 'call': expected = ((byte + other + 2 * third + program.constant) & 255) ^ program.constant; break;
      case 'branch': expected = (((byte < other ? byte : other) + third) & 255) ^ program.constant; break;
      case 'loop':
        parameters = [seed & 7, byte];
        expected = byte;
        for (let count = parameters[0]; count; count--) expected = ((expected + program.constant) & 255) ^ count;
        break;
      case 'array':
        parameters = [seed & 31, (seed * 13 + program.variant) & 31, byte, other];
        expected = buffer[parameters[0]] ^ buffer[parameters[1]] ^ ((byte + other) & 255);
        break;
      case 'pointer': {
        parameters = [0, seed & 7, (seed >> 3) & 7, byte];
        const index = parameters[1] + parameters[2];
        expected = expectedBuffer[index] = (buffer[index] + byte + program.constant) & 255;
        break;
      }
      case 'pressure': {
        const first = (byte + other) & 255;
        const second = (other + third) & 255;
        const combined = (byte + other + 2 * third + program.constant) & 255;
        expected = ((first + combined + second) & 65535) ^ ((((byte + third) & 255) + (first ^ program.constant)) & 65535);
        break;
      }
      case 'walk':
        parameters = [0, seed & 31, 1 + seed % 12, byte];
        expected = byte;
        for (let iteration = 0; iteration < parameters[2]; iteration++) {
          const index = parameters[1] + iteration * (1 + program.variant % 3);
          const previous = expectedBuffer[index];
          expectedBuffer[index] = (previous + expected + program.constant) & 255;
          expected ^= previous;
        }
        break;
      case 'nested':
        parameters = [0, seed & 63, 1 + seed % 4, 1 + (seed >> 2) % 4];
        expected = program.constant;
        for (let row = 0; row < parameters[2]; row++) {
          for (let column = 0; column < parameters[3]; column++) {
            const index = parameters[1] + row * 8 + column;
            const previous = expectedBuffer[index];
            expected = (expected + previous + row) & 65535;
            expectedBuffer[index] = previous ^ column ^ program.constant;
          }
        }
        break;
      case 'join_call': {
        parameters = [0, seed & 63, other, byte];
        const index = parameters[1];
        const called = (byte + other + 2 * buffer[index] + program.constant) & 255;
        let retained = (buffer[index] + program.constant) & 255;
        if (other & (1 << (program.variant % 4))) expectedBuffer[index] = (called + byte) & 255;
        else {
          expectedBuffer[index + 1] = called ^ byte;
          retained += other;
        }
        expected = (retained + expectedBuffer[index] + expectedBuffer[index + 1]) & 65535;
        break;
      }
      default: throw new Error('Unknown family');
    }
    for (const mode of Object.keys(totals)) {
      const image = compiled.get(mode);
      const bufferAddress = image.symbols.get(program.family === 'scalar_remat_word' ? 'ra_words' : 'ra_buffer');
      const args = parameters.slice();
      if (['pointer', 'walk', 'nested', 'join_call', 'scalar_remat_pointer'].includes(program.family)) args[0] = bufferAddress;
      try {
        const result = execute(image.rom, image.symbols.get(program.name), args, program.widths,
          buffer, bufferAddress, program.returnWidth);
        if (result.value !== expected || !Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer)))
          throw new Error(`Expected ${expected}, got ${result.value}; buffer_equal=${Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer))}`);
        for (const counters of [totals[mode], programTotals[program.name][mode]]) {
          counters.cases++;
          for (const metric of ['instructions', 'cycles', 'frame_reads', 'frame_writes']) counters[metric] += result[metric];
        }
      } catch (error) {
        writeFileSync(join(output, 'failure.json'), JSON.stringify({mode, seed, program, parameters: args,
          expected, buffer: Array.from(buffer), error: String(error)}, null, 2));
        throw new Error(`Failure ${program.name} seed=${seed} mode=${mode}: ${error}; artifacts=${output}`);
      }
    }
  }
}
for (const [mode, totalsForMode] of Object.entries(totals))
  console.log(JSON.stringify({mode, ...totalsForMode, compile_seconds: compiled.get(mode).compileSeconds}));
writeFileSync(join(output, 'execution-summary.json'), JSON.stringify(totals, null, 2));
writeFileSync(join(output, 'program-execution-summary.json'), JSON.stringify(programTotals, null, 2));
console.log(`PASS:verifier_failures=16 immutable=yes differential_cases=${totals.ra.cases} families=${new Set(programs.map(program => program.family)).size} variants=8 default_no_ra=identical artifacts=${output}`);

if (process.env.RA_REAL_PROJECTS !== '0') {
  const realTotals = {};
  for (const [project, routines] of [
    ['calculations-1', ['calculationAdd3', 'calculationMultiplyAdd', 'calculationXorRoundTrip', 'calculationQuotientRoundTrip']],
    ['calculations-2', ['calculationProductRestore', 'calculationShiftRestore', 'calculationPartitionRestore']]
  ]) {
    const images = new Map();
    for (const [mode, flags] of modes) {
      const directory = join(output, project, mode);
      mkdirSync(directory, {recursive: true});
      const sources = ['interrupts', 'main', 'bank_1', 'bank_2', 'bank_3'];
      for (const name of sources) {
        writeFileSync(join(directory, `${name}.ssc`), readFileSync(join(root, 'tests/z80/sms', project, `${name}.ssc`)));
        const bank = name.startsWith('bank_') ? ['-b', name.slice(-1), '-s', '2'] : [];
        run(compiler, [...bank, '-mz80', '-rs', '3', ...flags, '-o', `${name}.asm`, `${name}.ssc`], directory, `${name}.compiler.log`);
      }
      run(linker, ['-lsms', '-ro', '4', '-v', '-o', 'linked.sms', ...sources.map(name => `${name}.asm`)], directory, 'linker.log');
      const image = readImage(directory);
      for (const name of routines)
        if (!image.symbols.has(name)) throw new Error(`Missing bank-zero routine ${project}/${name}/${mode}`);
      images.set(mode, image);
    }
    if (!images.get('default').rom.equals(images.get('no_ra').rom)) throw new Error(`${project}: default/-no-ra ROM mismatch`);
    if (!images.get('ra').rom.equals(images.get('ra_quiet').rom)) throw new Error(`${project}: traced/quiet allocator ROM mismatch`);
    realTotals[project] = {};
    for (const name of routines) {
      const routineTotals = Object.fromEntries(Object.keys(totals).map(mode => [mode, {cases: 0, instructions: 0, cycles: 0, frame_reads: 0, frame_writes: 0}]));
      realTotals[project][name] = routineTotals;
      for (let seed = 0; seed < cases; seed++) {
        const value = (seed * 7919 + 32767) & 65535;
        const other = (seed * 5113 + 17) & 65535;
        const third = (seed * 3571 + 3) & 65535;
        let parameters;
        let expected;
        switch (name) {
          case 'calculationAdd3': parameters = [value, other, third]; expected = (value + other + third) & 65535; break;
          case 'calculationMultiplyAdd': parameters = [value, other, third]; expected = (value * other + third) & 65535; break;
          case 'calculationXorRoundTrip': parameters = [value, other]; expected = value; break;
          case 'calculationQuotientRoundTrip': parameters = [value, 1 + other % 65535]; expected = value; break;
          case 'calculationProductRestore': {
            const multiplier = 1 + seed % 31;
            parameters = [value & 1023, multiplier, seed % multiplier]; expected = parameters[0]; break;
          }
          case 'calculationShiftRestore': {
            const shift = seed % 8;
            parameters = [value >> shift, shift, other]; expected = parameters[0]; break;
          }
          case 'calculationPartitionRestore': parameters = [value, 1 + other % 65535, third]; expected = value; break;
          default: throw new Error('Unknown real routine');
        }
        for (const mode of Object.keys(totals)) {
          const image = images.get(mode);
          try {
            const result = execute(image.rom, image.symbols.get(name), parameters, parameters.map(() => 2), new Uint8Array(0), 0, 2);
            if (result.value !== expected) throw new Error(`Expected ${expected}, got ${result.value}`);
            routineTotals[mode].cases++;
            for (const metric of ['instructions', 'cycles', 'frame_reads', 'frame_writes']) routineTotals[mode][metric] += result[metric];
          } catch (error) {
            writeFileSync(join(output, 'failure.json'), JSON.stringify({project, name, mode, seed, parameters, expected, error: String(error)}, null, 2));
            throw new Error(`Failure ${project}/${name} seed=${seed} mode=${mode}: ${error}; artifacts=${output}`);
          }
        }
      }
      console.log(JSON.stringify({project, routine: name, ...routineTotals}));
    }
  }
  writeFileSync(join(output, 'real-execution-summary.json'), JSON.stringify(realTotals, null, 2));
  console.log(`PASS:real_routines=7 cases_per_routine=${cases} default_no_ra=identical artifacts=${output}`);
}

if (process.env.RA_SCALAR_CASES !== '1') {
  Object.assign(process.env, {RA_SCALAR_CASES: '1', RA_REAL_PROJECTS: '0',
    RA_EXECUTION_DIR: join(output, 'scalar'), RA_VERIFIER: harness});
  run(process.execPath, [fileURLToPath(import.meta.url)], output, 'scalar-execution.log');
  process.stdout.write(readFileSync(join(output, 'scalar-execution.log'), 'utf8'));
}