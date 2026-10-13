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
run(harness, [], output, 'verifier.log');
const verifierLog = readFileSync(join(output, 'verifier.log'), 'utf8');
if ((verifierLog.match(/result=rejected unchanged=yes status=complete/g) || []).length !== 14 ||
  (verifierLog.match(/result=accepted unchanged=yes status=complete/g) || []).length !== 5)
  throw new Error('Verifier coverage count mismatch');

const programs = [];
for (let variant = 0; variant < 8; variant++) {
  const constant = 1 + variant * 7;
  const functions = [
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
    const name = `ra_${family}_${variant}`;
    programs.push({name, family, variant, constant, widths, returnWidth,
      source: `u${returnWidth * 8} ${name}(${parameters}) { ${body} }`});
  }
}
const source = 'u8 ra_buffer[128];\n' + programs.map(program => program.source).join('\n') + '\nvoid main(void) {}\n';
writeFileSync(join(output, 'generated.ssc'), source);
const modes = [['default', []], ['no_ra', ['-no-ra']], ['all_spill', ['-ra-all-spill']], ['ra', ['-ra']]];

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
  const hal = {
    tStateCount: 0,
    readMemory: address => memory[address & 65535],
    writeMemory: (address, value) => {
      address &= 65535;
      if (!(address >= frame - 256 && address < frame - 1) &&
          !(address >= bufferAddress && address < bufferAddress + buffer.length))
        throw new Error(`Out-of-bounds write ${address.toString(16)}`);
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
    buffer: memory.slice(bufferAddress, bufferAddress + buffer.length), instructions, cycles: hal.tStateCount};
}

const totals = Object.fromEntries(['default', 'all_spill', 'ra'].map(mode => [mode, {cases: 0, instructions: 0, cycles: 0}]));
for (const program of programs) {
  for (let seed = 0; seed < cases; seed++) {
    const byte = (seed * 73 + program.variant * 19) & 255;
    const other = (seed * 29 + 17) & 255;
    const third = (seed * 47 + 3) & 255;
    const buffer = Uint8Array.from({length: 128}, (_, index) => (index * 47 + seed * 11 + program.variant) & 255);
    const expectedBuffer = buffer.slice();
    let parameters = [byte, other, third];
    let expected;
    switch (program.family) {
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
      const args = parameters.slice();
      if (['pointer', 'walk', 'nested', 'join_call'].includes(program.family)) args[0] = image.symbols.get('ra_buffer');
      try {
        const result = execute(image.rom, image.symbols.get(program.name), args, program.widths,
          buffer, image.symbols.get('ra_buffer'), program.returnWidth);
        if (result.value !== expected || !Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer)))
          throw new Error(`Expected ${expected}, got ${result.value}; buffer_equal=${Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer))}`);
        totals[mode].cases++;
        totals[mode].instructions += result.instructions;
        totals[mode].cycles += result.cycles;
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
console.log(`PASS:verifier_failures=14 immutable=yes differential_cases=${totals.ra.cases} families=11 variants=8 default_no_ra=identical artifacts=${output}`);

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
    realTotals[project] = {};
    for (const name of routines) {
      const routineTotals = Object.fromEntries(Object.keys(totals).map(mode => [mode, {cases: 0, instructions: 0, cycles: 0}]));
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
            routineTotals[mode].instructions += result.instructions;
            routineTotals[mode].cycles += result.cycles;
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