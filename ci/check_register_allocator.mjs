import {spawnSync} from 'node:child_process';
import {mkdtempSync, mkdirSync, readFileSync, writeFileSync, readdirSync, openSync, closeSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const output = process.env.RA_EXECUTION_DIR || mkdtempSync(join(tmpdir(), 'ra-execution-'));
const emulatorPath = process.env.RA_Z80_EMULATOR;
if (!emulatorPath) throw new Error('Set RA_Z80_EMULATOR to z80-emulator@2.3.0/dist/index.js');
const emulatorVersion = JSON.parse(readFileSync(resolve(dirname(emulatorPath), '../package.json'), 'utf8')).version;
if (emulatorVersion !== '2.3.0') throw new Error('Expected pinned z80-emulator@2.3.0');
const {Z80} = await import(pathToFileURL(resolve(emulatorPath)).href);
const compiler = process.env.SAMESAMECC || 'samesamecc';
const linker = process.env.SAMESAMECL || 'samesamecl';
const cases = Number(process.env.RA_CASES || 64);
if (!Number.isInteger(cases) || cases < 1 || cases > 4096) throw new Error('RA_CASES must be 1..4096');
const comparisonForms = Number(process.env.RA_COMPARISON_FORMS || 0);
if (![0, 1, 2].includes(comparisonForms)) throw new Error('RA_COMPARISON_FORMS must be 0..2');
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
let verifierFailures;
for (const flags of [[], ['-q']]) {
  const log = flags.length ? 'verifier-quiet.log' : 'verifier.log';
  run(harness, flags, output, log);
  const verifierLog = readFileSync(join(output, log), 'utf8');
  if (!verifierLog.includes('status=missing_or_clobbered_definition'))
    throw new Error('Verifier failure diagnostics missing');
  verifierFailures = (verifierLog.match(/result=rejected unchanged=yes status=complete/g) || []).length;
  if (verifierFailures !== 32 ||
    (verifierLog.match(/result=accepted unchanged=yes status=complete/g) || []).length !== 7 ||
    !verifierLog.includes('allocation_mutation_test: invalid_tail unchanged=yes status=complete') ||
    !verifierLog.includes('allocation_mutation_test: displaced_arg2 cleared=yes status=complete') ||
    !verifierLog.includes('allocation_policy_test: register_pairs=25 independent_units=yes status=complete') ||
    !/allocation_policy_test: lowering_effects=\d+ direct_bc_complement=yes scratch_hl_rejected=yes full_barriers=yes status=complete/.test(verifierLog) ||
    !/allocation_policy_test: direct_bc_bitwise=yes unsigned_widening=yes signed_right_fallback=yes bc_overlap_rejected=yes status=complete/.test(verifierLog) ||
    !/allocation_policy_test: endpoints=10200 supported=\d+ rejected=\d+ independent_table=yes status=complete/.test(verifierLog) ||
    !verifierLog.includes('spill_slot_test: cases=10 overlap_width_boundary_redefinition_controls=yes tacs_unchanged=yes status=complete') ||
    !verifierLog.includes('spill_slot_test: fixed_prefix=yes shared_suffix=yes shared_nonoverlap_prefix=yes invalid_fixed=5 immutable=yes status=complete') ||
    !verifierLog.includes('allocation_cost_test: nearer_rejected=yes farther_reused=yes status=complete') ||
    !verifierLog.includes('allocation_cost_test: weighted_cycles=yes static_bytes=yes instruction_bound=256 fallback=next_use immutable=yes status=complete') ||
    !verifierLog.includes('allocation_cost_test: split_reload_overlap_controls=4 status=complete') ||
    !verifierLog.includes('allocation_cost_test: rejected_first_alternative=yes known_alternative_over_unknown_fallback=yes status=complete') ||
    !verifierLog.includes('candidate_definition_test: read_before_write=yes previous_definition_excluded=yes status=complete'))
    throw new Error('Verifier coverage count mismatch');
}

const programs = [];
const realLoopBody = readFileSync(join(root, 'tests/z80/sms/calculations-1/bank_2.ssc'), 'utf8')
  .split('void calculateAndPrint3(u8 *chars, u8 one, u8 zero) {')[1].split('\n  extra1 =')[0];
const realIndexBody = realLoopBody.split('__z80_out')[0];
const realWhileBody = readFileSync(join(root, 'tests/z80/sms/calculations-1/bank_1.ssc'), 'utf8')
  .match(/^  while \(start < chars\) \{[\s\S]*?^  \}/m)[0];
for (let variant = 0; variant < 12; variant++) {
  const constant = 1 + variant * 7;
  const operator = ['+', '-', '&', '|', '^'][variant % 5];
  const comparison = ['==', '!=', '<', '>', '<=', '>='][variant % 6];
  const byteComparison = comparisonForms ? '128' : 'c';
  const wordComparison = comparisonForms ? '32768' : 'c';
  const signedComparison = comparisonForms ? '(0-1)' : 'c';
  const signedWord = variant === 4 || variant === 5 || variant >= 8;
  const expression = variant & 1 ? `saved${operator}c` : `c${operator}saved`;
  const inner = variant & 4 ? 'column' : '0';
  const call = `u8 called = ra_signed_helper(${variant & 1 ? 'previous' : `alias[start+row+${inner}]`},selector,row);`;
  const functions = [
    ['matrix_byte', 'u8 a, u8 b, u8 c', `u8 saved = a^b; return ${expression};`, [1, 1, 1], 1],
    ['matrix_word', 'u16 a, u16 b, u16 c', `u16 saved = ~b; return ${expression};`, [2, 2, 2], 2],
    ['matrix_affinity', 'u16 a, u16 b, u16 c', `u16 live = a+b; return ${variant < 4 ? '(~c)' : 'live'} ${['+', '&', '|', '^'][variant % 4]} ${variant < 4 ? 'live' : '(~c)'};`, [2, 2, 2], 2],
    ['matrix_rank', 'u16 a, u16 b, u16 c', `u16 live = ~a; u16 saved = ~c; return live ${['+', '&', '|', '^'][variant % 4]} saved;`, [2, 2, 2], 2],
    ['matrix_index_affinity', 'u16 a', `${variant ? 'u8 *target = &ra_buffer[0]; ' : ''}u16 index = ~a; return ${variant ? 'target' : 'ra_buffer'}[index];`, [2], 1],
    ['matrix_index_copy', variant === 9 ? 'u8 *target, u8 *alias, u16 a, u8 b' : `${variant === 5 ? 's8' : variant === 4 ? 'u8' : 'u16'} a, u8 b`, [
      'u16 index = a; return ra_buffer[index];',
      's16 index = a; u8 *target = &ra_buffer[32]; return target[index];',
      'u16 index = a; return ra_words[index];',
      's16 index = a; u16 *target = &ra_words[16]; return target[index];',
      'u8 index = a; return ra_buffer[index];',
      's8 index = a; u8 *target = &ra_buffer[32]; return target[index];',
      'u16 index = a; ra_buffer[index] = b; return ra_buffer[index];',
      'u16 index = a; u16 copied = index; return ra_buffer[copied];',
      'u16 index = 7; return ra_buffer[index];',
      'u16 index = a; target[index] = b; return alias[index];',
      'u16 index = a; ra_buffer[index++] = b; return index;',
      'u16 index = a; ra_words[index++] = b; return index;'
    ][variant], variant === 9 ? [2, 2, 2, 1] : [variant === 4 || variant === 5 ? 1 : 2, 1], [2, 3, 10, 11].includes(variant) ? 2 : 1],
    ['matrix_bc_alu', 'u16 a, u16 b, u16 c', variant < 10 ? `u16 saved = a${operator}b; return c${variant < 5 ? '-' : '+'}saved;` : variant === 10 ?
      'u16 live = a+c; u16 saved = b+c; return (live-saved)^live;' :
      'u8 small = a; u16 saved = b+c; return (saved+small)^small;', [2, 2, 2], 2],
    ['matrix_scalar_uses', `${[4, 7, 9, 11].includes(variant) ? 'u8' : [6, 8].includes(variant) ? 's8' : 'u16'} a, ${[4, 7, 9, 11].includes(variant) ? 'u8' : [6, 8].includes(variant) ? 's8' : 'u16'} b, ${[4, 7, 9, 11].includes(variant) ? 'u8' : [6, 8].includes(variant) ? 's8' : 'u16'} c`, [
      'u16 saved = a^b; if (saved < c) { return a; } return b;',
      'u16 saved = a^b; return saved << (c&15);',
      'u16 saved = a&15; return b << saved;',
      'u16 saved = a&127; return ra_buffer[saved];',
      'u8 saved = a^b; return ra_signed_helper(saved,b,c);',
      'u16 saved = a^b; return calculationAdd3(saved,b,c);',
      's8 saved = a^b; if (saved < c) { return 1; } return 0;',
      'u8 saved = a^b; return saved >> (c&7);',
      's8 saved = a; u8 *target = &ra_buffer[32]; return target[saved];',
      'u8 saved = a^b; u8 called = ra_signed_helper(saved,b,c); return called^saved;',
      'u16 saved; if (b&1) { saved = a; } else { saved = c; } return (saved+b)^saved;',
      'u8 saved = a; ra_pointer_helper(&saved,b); return saved;'
    ][variant], [4, 6, 7, 8, 9, 11].includes(variant) ? [1, 1, 1] : [2, 2, 2], 2],
    ['matrix_real_index', 'u8 *chars, u8 one, u8 zero', `${realIndexBody} return index;`, [2, 1, 1], 2],
    ['matrix_source_loop', variant ? 'u8 *start, u8 *chars' : 'u8 *chars, u8 one, u8 zero',
      variant ? realWhileBody : `${realLoopBody} return index;`, variant ? [2, 2] : [2, 1, 1], variant ? 0 : 2],
    ['matrix_compare_byte', 'u8 a, u8 b, u8 c', `u8 saved = ${comparisonForms === 2 ? 'a' : 'a^b'}; if (${variant < 6 ? 'saved' : byteComparison} ${comparison} ${variant < 6 ? byteComparison : 'saved'}) { return a; } return b;`, [1, 1, 1], 1],
    ['matrix_compare_word', 'u16 a, u16 b, u16 c', `u16 saved = ${comparisonForms === 2 ? 'a' : '~b'}; if (${variant < 6 ? 'saved' : wordComparison} ${comparison} ${variant < 6 ? wordComparison : 'saved'}) { return a; } return b;`, [2, 2, 2], 2],
    ['matrix_compare_signed', 's8 a, s8 b, s8 c', `s8 saved = ${comparisonForms === 2 ? 'a' : 'a^b'}; if (${variant < 6 ? 'saved' : signedComparison} ${comparison} ${variant < 6 ? signedComparison : 'saved'}) { return a; } return b;`, [1, 1, 1], 1],
    ['matrix_extend', 's8 a, s8 b, u16 c', `s8 saved = a^b; s16 extended = saved; return ${variant & 1 ? 'c+extended' : 'extended+c'};`, [1, 1, 2], 2],
    ['matrix_low_byte', `${signedWord ? 's16' : 'u16'} a, u16 b, u16 c`, `${signedWord ? 's16' : 'u16'} saved = ~a; ${variant >= 6 ? 's8' : 'u8'} narrowed = ${variant < 2 ? (variant & 1 ? 'b+saved' : 'saved+b') : 'saved'}; ${variant >= 6 ? 's16 extended = narrowed;' : ''} return ${variant < 2 ? 'c+narrowed' : variant & 1 ? `${variant >= 6 ? 'extended' : 'narrowed'}+(b+c)` : `(b+c)+${variant >= 6 ? 'extended' : 'narrowed'}`};`, [2, 2, 2], 2],
    ['matrix_shift', 'u16 a, u16 b, u8 c', `u16 saved = ~a; return saved ${variant & 1 ? '>>' : '<<'} (c&15);`, [2, 2, 1], 2],
    ['matrix_sentinel', variant >= 8 ? 's8 a, u8 b, u8 c' : variant === 2 || variant === 3 ? 's8 a, s8 b, u16 c' : 'u16 a, u16 b, u16 c', [
      'u16 live = a+b; u16 saved = ~c; return (live+saved)^live;',
      'u16 live = a+b; u16 saved = ~c; return (live^saved)+live;',
      's8 saved = a^b; s16 wide = saved; u16 live = c+c; return wide+live;',
      's8 saved = a^b; s16 wide = saved; u16 live = c+c; return live+wide;',
      'u16 live = a+b; u8 low = c; return (live+low)^live;',
      'u16 live = a+b; u8 low = c; return (low+live)^live;',
      'u16 live = a+b; return (live << (c&15))^live;',
      'u16 live = a+b; return (live >> (c&15))^live;',
      'u8 *target = &ra_buffer[32]; u8 live = b^c; target[a] = live; u16 loaded = target[a]; return loaded+live;',
      'u8 *target = &ra_buffer[32]; u8 live = b^c; s8 index = a^1; target[index] = live; u16 loaded = target[index]; return loaded+live;'
    ][variant], variant >= 8 ? [1, 1, 1] : variant === 2 || variant === 3 ? [1, 1, 2] : [2, 2, 2], 2],
    ['matrix_structural', 'u8 *target, u8 *alias, u8 start, u8 count, u8 selector, u8 value', `u16 total = value; u8 saved = selector^value; u8 row = 0; while (row < count) { ${variant & 4 ? 'u8 column = 0; while (column < 2) {' : ''} u8 previous = target[start+row+${inner}]; ${variant & 1 ? call : ''} if ((previous^selector)&1) { alias[start+row+${inner}] = previous+saved; } else { target[start+row+${inner}] = previous^saved; } ${variant & 1 ? '' : call} total = total+target[start+row+${inner}]+called; ${variant >= 8 ? `u8 narrowed = total; if (row&1) { total = ${variant & 2 ? 'narrowed+total' : 'total+narrowed'}; } else { total = total+(selector&7); }` : ''} ${variant & 4 ? 'column = column+1; }' : ''} row = row+1; } return total;`, [2, 2, 1, 1, 1, 1], 2],
    ['scalar_byte', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; return (c+c)+saved;', [1, 1, 1], 1],
    ['scalar_signed_byte', 's8 a, s8 b, s8 c', 's8 saved = a^b; return (c|7)^saved;', [1, 1, 1], 1],
    ['scalar_signed_word', 's16 a, s16 b, s16 c', 's16 saved = a^b; return (c|7)^saved;', [2, 2, 2], 2],
    ['scalar_widen', 'u8 a, u8 b, u8 c', 'u16 saved = a+b; return (c+c)+saved;', [1, 1, 1], 2],
    ['scalar_word', 'u16 a, u16 b, u16 c', 'u16 saved = a^b; return (c+c)+saved;', [2, 2, 2], 2],
    ['scalar_reassigned_word', 'u16 a, u16 b, u16 c', `u16 saved = a^b; u16 first = c+saved; ${variant ? 'if (a&1) {' : ''} saved = b^c; ${variant ? '}' : ''} return (first+saved)^saved;`, [2, 2, 2], 2],
    ['scalar_reassigned_byte', `${variant ? 's8' : 'u8'} a, ${variant ? 's8' : 'u8'} b, ${variant ? 's8' : 'u8'} c`, `${variant ? 's8' : 'u8'} saved = a^b; ${variant ? 's8' : 'u8'} first = c+saved; saved = a^c; return (first+saved)^saved;`, [1, 1, 1], 1],
    ['scalar_lifetime_hole', 'u16 a, u16 b, u16 c', `u16 saved = a^b; u16 first = c+saved; ${variant ? 'u8 called = ra_signed_helper(a,b,c);' : 'u8 called = 0;'} u16 middle = b+c+called; saved = a+c; return (first^middle)+(saved^middle);`, [2, 2, 2], 2],
    ['scalar_source_reassigned', 'u16 a, u8 b, u8 c', `s16 result = 0x7521; u8 low = result; ra_buffer[0] = low; result = (((a|b)%0x1001)<<4)-10000; result ^= 0x4321; u8 next = result; ra_buffer[1] = next; result = ((((${variant ? '(c|a)|(b|c)' : '(a|c)|(c|b)'})%0x1001)<<4)-(c|10000)); result ^= (0x4321|c); return result;`, [2, 1, 1], 2],
    ['scalar_source_bitwise', 'u16 a, u8 b, u8 c', `return ((((${variant ? '(c|a)|(b|c)' : '(a|c)|(c|b)'}) % 0x1001) << 4) - (c|10000)) ^ (0x4321|c);`, [2, 1, 1], 2],
    ['scalar_copy', 'u16 a, u16 b, u16 c', 'u16 saved = a; return (b+c)+saved;', [2, 2, 2], 2],
    ['scalar_complement_sub', 'u16 a, u16 b, u16 c', 'return (a+b)-(~c);', [2, 2, 2], 2],
    ['scalar_multi_word', 'u16 a, u16 b, u16 c', 'u16 left = a+b; u16 saved = ~c; return ((left+saved)^saved)-saved;', [2, 2, 2], 2],
    ['scalar_multi_rmw', 'u16 a, u16 b, u16 c', 'u16 saved = ~c; return ((a+saved)^saved)+b;', [2, 2, 2], 2],
    ['scalar_multi_byte', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; return ((c+saved)^saved)+saved;', [1, 1, 1], 1],
    ['scalar_rank_fallback', `u16 a, u16 b, ${variant ? 'u8' : 'u16'} c`, `u16 saved = ~a; u16 first = saved+b; u16 second = saved+first; if (c&${1 << variant}) { ${variant ? 'return second+saved;' : 'second = second+saved;'} } return second;`, [2, 2, variant ? 1 : 2], 2],
    ['scalar_frequency_loop', 'u8 a, u8 b, u8 c', `u8 value = a^${constant}; while (value < b) { value = value+1; } return value;`, [1, 1, 1], 1],
    ['scalar_split_byte', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; return ((((first+loaded)^saved)+saved)^saved)+saved;', [1, 1, 1], 1],
    ['scalar_region', 'u8 a, u8 b, u8 c', `u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; u8 out = ((((first+loaded)^saved)+saved)^saved)+saved; if (a < b) { ${variant === 1 ? 'ra_buffer[2] = out;' : ''} out = out+1; } return ((out^saved)+saved)^saved;`, [1, 1, 1], 1],
    ['scalar_region_escape', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; if (a < b) { ra_pointer_helper(&saved,c); } return saved+b;', [1, 1, 1], 1],
    ['scalar_call_region', 'u8 a, u8 b, u8 c', `u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; u8 out = ((((first+loaded)^saved)+saved)^saved)+saved; u8 called = ra_pointer_helper(ra_buffer,ra_signed_helper(${variant === 1 ? 'ra_recursive_helper(2,a)' : 'a'},b,c)); return ((((out^saved)+saved)^saved)+saved)^called;`, [1, 1, 1], 1],
    ['scalar_loop_pressure', 'u8 a, u8 b, u8 c', 'u8 left = a; u8 right = b; while (left < right) { left = left+1; right = right-1; } return left^right;', [1, 1, 1], 1],
    ['scalar_loop_call', 'u8 a, u8 b, u8 c', 'u8 saved = a^b; u8 value = b; while (c) { value = ra_pointer_helper(ra_buffer,value)+saved; c = c-1; } return value;', [1, 1, 1], 1],
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
    if (variant >= 8 && !family.startsWith('matrix_')) continue;
    if (family.startsWith('matrix_') && variant >= ({matrix_byte: 10, matrix_word: 10,
      matrix_compare_byte: 12, matrix_compare_word: 12, matrix_compare_signed: 12,
      matrix_low_byte: 10, matrix_structural: 12, matrix_sentinel: 10, matrix_affinity: 8, matrix_rank: 4,
      matrix_index_copy: 12, matrix_bc_alu: 12, matrix_scalar_uses: 12, matrix_real_index: 1}[family] || 2)) continue;
    if (comparisonForms && family !== 'arithmetic' && !family.startsWith('matrix_compare_')) continue;
    if (process.env.RA_FAMILIES && family !== 'arithmetic' && !process.env.RA_FAMILIES.split(',').includes(family)) continue;
    if (family.startsWith('matrix_') && ['matrix_index_copy', 'matrix_bc_alu', 'matrix_scalar_uses', 'matrix_real_index', 'matrix_source_loop'].includes(family) !==
      (process.env.RA_VALUE_CASES === '1')) continue;
    const scalarFamily = family.startsWith('scalar_') || family === 'word_bc';
    if (family !== 'arithmetic' && (process.env.RA_MATRIX_CASES === '1' ?
      !family.startsWith('matrix_') : family.startsWith('matrix_') || scalarFamily !== (process.env.RA_SCALAR_CASES === '1'))) continue;
    if (scalarFamily && family !== 'word_bc' && variant > 1) continue;
    const name = `ra_${family}_${variant}`;
    programs.push({name, family, variant, constant, widths, returnWidth,
      pointerParameters: family === 'matrix_source_loop' ? variant ? [0, 1] : [0] : family === 'matrix_real_index' ? [0] : family === 'matrix_structural' || (family === 'matrix_index_copy' && variant === 9) ? [0, 1] :
        ['pointer', 'walk', 'nested', 'join_call', 'scalar_remat_pointer'].includes(family) ? [0] : [],
      source: `${returnWidth ? `${family.startsWith('scalar_signed') ? 's' : 'u'}${returnWidth * 8}` : 'void'} ${name}(${parameters}) { ${body} }`});
  }
}
const preamble = 'u8 ra_buffer[128];\nu16 ra_words[64];\ns8 ra_signed_helper(u8 a, u8 b, u8 c) { return a+b+c; }\nu8 ra_pointer_helper(u8 *target, u8 value) { target[0] = value; return target[1]; }\nu8 ra_recursive_helper(u8 depth, u8 value) { if (depth) { u8 saved = value^depth; u8 nested = ra_recursive_helper(depth-1,value+1); return nested^saved; } return value; }\n' +
  (programs.some(program => ['matrix_real_index', 'matrix_scalar_uses', 'matrix_source_loop'].includes(program.family)) ?
    readFileSync(join(root, 'tests/z80/sms/calculations-1/main.ssc'), 'utf8').match(/^void convertNumbersToChars16\([^;]+\) \{[\s\S]*?^\}/m)[0] + '\n' +
    readFileSync(join(root, 'tests/z80/sms/calculations-1/main.ssc'), 'utf8').match(/^u16 calculationAdd3\([^;]+\) \{[\s\S]*?^\}/m)[0] + '\n' : '');
const source = preamble + programs.map(program => program.source).join('\n') + '\nvoid main(void) {}\n';
writeFileSync(join(output, 'generated.ssc'), source);
const modes = [['default', []], ['no_ra', ['-no-ra']], ['all_spill', ['-ra-all-spill']],
  ['ra', ['-ra']], ['ra_quiet', ['-ra', '-q']]];

function readImage(directory) {
  const symbols = new Map();
  for (const line of readFileSync(join(directory, 'linked.sym'), 'utf8').split(/\r?\n/)) {
    const match = line.match(/^00:([0-9a-f]+)\s+(\S+)$/i);
    if (match) symbols.set(match[2], parseInt(match[1], 16));
  }
  const rom = readFileSync(join(directory, 'linked.sms'));
  run('wla-z80', ['-i', '-o', 'counters.o', 'linked.sms.combined.asm'], directory, 'counter-assembler.log');
  writeFileSync(join(directory, 'counters.link'), '[objects]\ncounters.o\n');
  run('wlalink', ['-i', 'counters.link', 'counters.sms'], directory, 'counter-linker.log');
  if (!rom.equals(readFileSync(join(directory, 'counters.sms'))))
    throw new Error(`Listing-enabled ROM mismatch in ${directory}`);
  const transfers = new Map();
  const staticMetrics = {rom_image_bytes: rom.length, register_move_sites: 0};
  let cause = 'other_frame', tacCause = cause, argumentsInFrame = [], transferOpen = false, section = '';
  for (const line of readFileSync(join(directory, 'linked.sms.combined.lst'), 'utf8').split(/\r?\n/)) {
    const source = line.slice(60).trim();
    if (/^\.SECTION\b/.test(source)) {
      if (transferOpen) throw new Error('Unterminated allocator transfer');
      cause = tacCause = 'other_frame';
      argumentsInFrame = [];
      section = source.match(/^\.SECTION "([^"]+)"/)[1];
    }
    if (/^; TAC:/.test(source)) {
      if (transferOpen) throw new Error('Allocator transfer crossed TAC boundary');
      cause = tacCause = /^; TAC: spill /.test(source) ? 'allocator_preserve' :
        /\breturn\b/.test(source) ? 'abi_result' : /\b[A-Za-z_]\w*\(/.test(source) ? 'abi_frame' : 'other_frame';
    }
    const argument = source.match(/^; TAC: variable .* size (\d+) offset (-?\d+) type argument$/);
    if (argument) argumentsInFrame.push([Number(argument[2]), Number(argument[1])]);
    const begin = source.match(/^; allocator transfer begin kind=(reload|preserve) bytes=([12])$/);
    if (begin) {
      if (transferOpen) throw new Error('Nested allocator transfer');
      transferOpen = true;
      cause = `allocator_${begin[1]}`;
    }
    if (/^; (?:reload .* from spill slot into|preserve retained .* in spill slot)/.test(source)) {
      if (!transferOpen && process.env.RA_BASELINE !== '1') throw new Error('Allocator transfer end without begin');
      transferOpen = false;
      cause = tacCause;
    }
    if (!transferOpen) {
      if (/^; copy argument \d+/.test(source)) cause = 'abi_argument';
      if (/^; (?:return value ->|copy return value to)/.test(source)) cause = 'abi_result';
      if (/^; old stack frame address ->/.test(source)) cause = 'abi_frame';
    }
    const address = line.match(/^\s*\d+\s+([0-9a-f]{4})\s+[0-9a-f]{4}\s+([0-9a-f]{4})\s+([0-9a-f]{4,8})\s+/i);
    if (!address || !/^[A-Z][A-Z0-9]*(?:\s|$)/.test(source)) continue;
    const opcode = rom[parseInt(address[3], 16)];
    if ((opcode & 0xc0) === 0x40 && (opcode & 7) !== 6 && ((opcode >> 3) & 7) !== 6)
      staticMetrics.register_move_sites++;
    if (parseInt(address[1], 16) === 0)
      transfers.set(parseInt(address[2], 16), {cause, argumentsInFrame, section,
        branch: tacCause === 'other_frame' && /^(?:JP|JR)\s+(?:[A-Z]+,\s*)?_label_/.test(source)});
  }
  if (transferOpen || !transfers.size) throw new Error('Incomplete execution attribution map');
  return {rom, symbols, transfers, staticMetrics};
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
writeFileSync(join(output, 'static-summary.json'), JSON.stringify(Object.fromEntries(
  Array.from(compiled, ([mode, image]) => [mode, image.staticMetrics])), null, 2));
const allocatorLog = readFileSync(join(output, 'ra/compiler.log'), 'utf8');
if (process.env.RA_BASELINE !== '1') {
  if (/scalar_copy_promotion function=ra_pressure_\d+ variable=first .+scope=linear_definitions/.test(allocatorLog))
    throw new Error('Mixed-width reassignment was promoted');
  if (programs.some(program => program.family === 'scalar_reassigned_word')) {
    for (const name of ['reassigned_word_0', 'lifetime_hole_0', 'lifetime_hole_1', 'source_reassigned_0', 'source_reassigned_1']) {
      if (programs.some(program => program.name === `ra_scalar_${name}`) &&
          !new RegExp(`scalar_copy_promotion function=ra_scalar_${name} variable=(?:saved|result) .+scope=linear_definitions`).test(allocatorLog))
        throw new Error(`Missing proven definition promotion: ${name}`);
    }
    if (/scalar_copy_promotion function=ra_scalar_(?:reassigned_word_1|reassigned_byte_[01]) variable=saved /.test(allocatorLog))
      throw new Error('Ambiguous or byte-width reassignment was promoted');
    const ranges = Array.from(allocatorLog.matchAll(/linear_scan retain_interval function=ra_scalar_reassigned_word_0 block=\d+ r(\d+) 16-bit in (HL|BC) producer_tac=(\d+)\([^\n]+?consumer_tac=(\d+)\(/g));
    if (ranges.length !== 2 || ranges[0][1] !== ranges[1][1] || Number(ranges[0][4]) >= Number(ranges[1][3]))
      throw new Error('Reassigned definitions did not release their disjoint retained lifetimes');
  }
  const quietAllocatorLines = readFileSync(join(output, 'ra_quiet/compiler.log'), 'utf8')
    .split(/\r?\n/).filter(line => /^register_allocator_core: |^register_allocator: (?:rejection_summary|loop_retention_coverage|target_active_slots_release|join_path_state_release|post_promotion_reconciliation) /.test(line));
  if (quietAllocatorLines.join('\n') !==
      (allocatorLog.match(/^register_allocator: rejection_summary .+$/gm) || []).join('\n'))
    throw new Error('Quiet allocator lifecycle output or rejection summary mismatch');
  if (!/^register_allocator_core: next_use .+status=found$/m.test(allocatorLog) ||
      !/^register_allocator_core: liveness_solve .+status=converged$/m.test(allocatorLog))
    throw new Error('Allocator lifecycle trace coverage missing');
}
for (const program of process.env.RA_BASELINE === '1' ? [] : programs) {
  const lines = allocatorLog.split(/\r?\n/).filter(line => line.includes(`function=${program.name} `));
  if (program.family === 'matrix_bc_alu' && program.variant < 5 &&
      !lines.some(line => /linear_scan retain .*16-bit in BC .*consumer_tac=\d+\(SUB arg2\)/.test(line)))
    throw new Error(`BC ALU right operand not exercised: ${program.name}`);
  if (program.family === 'matrix_scalar_uses' &&
      ([0, 1, 2, 3, 4, 5, 6, 7, 9].includes(program.variant) !==
       lines.some(line => /scalar_copy_promotion .*variable=saved /.test(line))) && program.variant !== 8)
    throw new Error(`Scalar use promotion mismatch: ${program.name}`);
  if (program.family === 'matrix_index_affinity' && program.variant === 0 &&
      (!lines.some(line => /profitability .*phy=HL known=yes /.test(line)) ||
       !lines.some(line => /profitability .*phy=BC known=yes /.test(line)) ||
       !lines.some(line => /placement_rank .*original=HL selected=BC known=yes .*mutation=no$/.test(line))))
    throw new Error(`Cost-ranked word-index affinity missing: ${program.name}`);
  if (program.family === 'scalar_split_byte' &&
      (lines.filter(line => line.includes('scalar_split_transfer') && line.endsWith('kind=store')).length !== 1 ||
       lines.filter(line => line.includes('scalar_split_transfer') && line.endsWith('kind=reload')).length !== 1 ||
       lines.filter(line => line.includes('reload_reuse')).length !== 3))
    throw new Error(`Split transfer counts changed: ${program.name}`);
  if (program.family === 'scalar_cost_reject_byte' &&
      (lines.some(line => line.includes('scalar_split_transfer')) ||
       !lines.some(line => {
         const score = line.match(/profitability .*known=yes saved_cycles=(-?\d+) saved_bytes=(-?\d+) .*decision=3 /);
         return score && (Number(score[1]) <= 0 || Number(score[2]) < 0);
       })))
    throw new Error(`Unprofitable split accepted: ${program.name}`);
  if (['scalar_region', 'scalar_call_region'].includes(program.family) &&
      (lines.filter(line => /scalar_split_transfer.*kind=store$/.test(line)).length !== 1 ||
       lines.filter(line => /scalar_split_transfer.*kind=reload$/.test(line)).length !== 2 ||
       lines.filter(line => line.includes('reload_reuse')).length !== (program.family === 'scalar_region' ? 5 : 6)))
    throw new Error(`Function-region transfers changed: ${program.name}`);
  if (program.family === 'scalar_loop_pressure' &&
      ['B', 'C'].some(register => !lines.some(line =>
        line.includes('loop_retention_apply_preflight') && line.includes(`selected=${register} `) && line.includes('application=applied'))))
    throw new Error(`Two-value loop retention missing: ${program.name}`);
  if (['scalar_region_escape', 'scalar_narrow'].includes(program.family) &&
      lines.some(line => /scalar_copy_promotion .*variable=saved /.test(line)))
    throw new Error(`Unsafe scalar promoted: ${program.name}`);
  if (program.family === 'scalar_loop_call' && lines.some(line =>
      line.includes('loop_retention_apply_preflight') && line.includes('application=applied')))
    throw new Error(`Call-clobbered loop retained: ${program.name}`);
  if (program.family.startsWith('scalar_remat_')) {
    const assembly = readFileSync(join(output, 'ra/main.asm'), 'utf8').split(`${program.name}:`)[1].split('.ENDS')[0];
    if (assembly.includes('; rematerialize ') !== (program.family !== 'scalar_remat_pointer'))
      throw new Error(`Constant-address rematerialization mismatch: ${program.name}`);
  }
}

const transferCauses = ['allocator_preserve', 'allocator_reload', 'abi_argument', 'abi_result', 'abi_frame', 'other_frame'];
const executionMetrics = ['instructions', 'cycles', 'frame_reads', 'frame_writes', 'register_moves', 'unmapped_instructions', 'entry_backedges', 'callee_backedges',
  ...transferCauses.flatMap(cause => [`${cause}_reads`, `${cause}_writes`])];

function execute(code, entry, parameters, widths, buffer, bufferAddress, returnWidth, transfers = new Map(), portsAllowed = false) {
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
  let frameReads = 0, frameWrites = 0, instructionMetadata;
  const portWrites = [];
  const traffic = Object.fromEntries(transferCauses.flatMap(cause => [`${cause}_reads`, `${cause}_writes`]).map(metric => [metric, 0]));
  const hal = {
    tStateCount: 0,
    readMemory: address => {
      address &= 65535;
      if (address >= frame - 256 && address < frame - 1) {
        frameReads++;
        let cause = instructionMetadata?.cause || 'other_frame';
        if (cause === 'other_frame' && instructionMetadata?.argumentsInFrame.some(([offset, size]) =>
          address >= ((cpu.regs.de + offset) & 65535) && address < ((cpu.regs.de + offset) & 65535) + size)) cause = 'abi_argument';
        traffic[`${cause}_reads`]++;
      }
      return memory[address];
    },
    writeMemory: (address, value) => {
      address &= 65535;
      if (!(address >= frame - 256 && address < frame - 1) &&
          !(address >= bufferAddress && address < bufferAddress + buffer.length))
        throw new Error(`Out-of-bounds write ${address.toString(16)}`);
      if (address >= frame - 256 && address < frame - 1) {
        frameWrites++;
        traffic[`${instructionMetadata?.cause || 'other_frame'}_writes`]++;
      }
      memory[address] = value & 255;
    },
    contendMemory: () => {}, contendPort: () => {}, readPort: () => 0,
    writePort: (address, value) => {
      if (!portsAllowed) throw new Error('Unexpected port write');
      if (![0xbe, 0xbf].includes(address & 255)) throw new Error('Unsupported captured port');
      portWrites.push([address & 255, value & 255]);
    }
  };
  const cpu = new Z80(hal);
  cpu.regs.pc = entry;
  cpu.regs.de = frame;
  cpu.regs.sp = frame - 1;
  let instructions = 0, registerMoves = 0, unmappedInstructions = 0, entryBackedges = 0, calleeBackedges = 0;
  while (cpu.regs.pc !== sentinel) {
    if (++instructions > 20000 || cpu.regs.pc >= 0xc000) throw new Error('Instruction bound/PC escape');
    instructionMetadata = transfers.get(cpu.regs.pc);
    if (!instructionMetadata) unmappedInstructions++;
    const opcode = memory[cpu.regs.pc];
    if ((opcode & 0xc0) === 0x40 && (opcode & 7) !== 6 && ((opcode >> 3) & 7) !== 6)
      registerMoves++;
    const previousPC = cpu.regs.pc;
    cpu.step();
    if (instructionMetadata?.branch && cpu.regs.pc <= previousPC &&
        transfers.get(cpu.regs.pc)?.section === instructionMetadata.section) {
      if (instructionMetadata.section === transfers.get(entry)?.section) entryBackedges++;
      else calleeBackedges++;
    }
  }
  if (cpu.regs.de !== frame || cpu.regs.sp !== frame + 1) throw new Error('Frame/stack restoration failure');
  if (transferCauses.reduce((count, cause) => count + traffic[`${cause}_reads`], 0) !== frameReads ||
      transferCauses.reduce((count, cause) => count + traffic[`${cause}_writes`], 0) !== frameWrites)
    throw new Error('Frame traffic attribution is not a partition');
  return {value: memory[resultOffset] + (returnWidth === 2 ? memory[resultOffset + 1] * 256 : 0),
    buffer: memory.slice(bufferAddress, bufferAddress + buffer.length), instructions, cycles: hal.tStateCount,
    frame_reads: frameReads, frame_writes: frameWrites, register_moves: registerMoves,
    unmapped_instructions: unmappedInstructions, entry_backedges: entryBackedges,
    callee_backedges: calleeBackedges, port_writes: portWrites, ...traffic};
}

const counterProbe = execute(Uint8Array.of(0x21, 0, 0xc1, 0x06, 42, 0x48, 0x79, 0x77, 0x7e, 0xc9),
  0, [], [], new Uint8Array(1), 0xc100, 0);
if (counterProbe.register_moves !== 2 || counterProbe.instructions !== 7 ||
    counterProbe.cycles !== 49 || counterProbe.buffer[0] !== 42)
  throw new Error('Executed register-move counter calibration failed');
const trafficProbe = execute(Uint8Array.of(0xdd, 0x21, 0xf8, 0xcf, 0x06, 42, 0x78,
  0xdd, 0x70, 0, 0xdd, 0x4e, 0, 0xdd, 0x71, 1, 0xdd, 0x7e, 1, 0xdd, 0x77, 2, 0xdd, 0x6e, 2, 0xc9),
  0, [], [], new Uint8Array(0), 0, 0, new Map([[7, 'allocator_preserve'], [10, 'allocator_reload'],
    [13, 'abi_argument'], [16, 'abi_argument'], [19, 'abi_result'], [22, 'abi_result']]
    .map(([address, cause]) => [address, {cause, argumentsInFrame: []}])));
if (trafficProbe.cycles !== 149 || trafficProbe.instructions !== 10 || trafficProbe.register_moves !== 1 ||
    trafficProbe.frame_reads !== 3 || trafficProbe.frame_writes !== 3 ||
    ['allocator_preserve_writes', 'allocator_reload_reads', 'abi_argument_reads', 'abi_argument_writes',
      'abi_result_reads', 'abi_result_writes'].some(metric => trafficProbe[metric] !== 1))
  throw new Error('Frame transfer-cause counter calibration failed');
const stackProbe = execute(Uint8Array.of(0xc5, 0xc1, 0xc9), 0, [], [], new Uint8Array(0), 0, 0,
  new Map([0, 1].map(address => [address, {cause: 'abi_frame', argumentsInFrame: []}])));
if (stackProbe.instructions !== 3 || stackProbe.cycles !== 31 || stackProbe.register_moves !== 0 ||
    stackProbe.abi_frame_reads !== 2 || stackProbe.abi_frame_writes !== 2)
  throw new Error('ABI stack counter calibration failed');
const branchProbe = execute(Uint8Array.of(0x06, 2, 0x10, 0xfe, 0xc9), 0, [], [], new Uint8Array(0), 0, 0,
  new Map([0, 2, 4].map(address => [address, {cause: 'other_frame', argumentsInFrame: [], section: 'probe', branch: address === 2}])));
if (branchProbe.instructions !== 4 || branchProbe.cycles !== 38 || branchProbe.entry_backedges !== 1 || branchProbe.callee_backedges !== 0)
  throw new Error('Source-loop backedge counter calibration failed');
const portCode = Uint8Array.of(0x3e, 42, 0xd3, 0xbe, 0xc9);
if (JSON.stringify(execute(portCode, 0, [], [], new Uint8Array(0), 0, 0, new Map(), true).port_writes) !== '[[190,42]]')
  throw new Error('Software port-sink calibration failed');
let rejectedPort = false;
try { execute(portCode, 0, [], [], new Uint8Array(0), 0, 0); }
catch (error) { rejectedPort = String(error).includes('Unexpected port write'); }
if (!rejectedPort) throw new Error('Unexpected port write was not rejected');
console.log('PASS:counter_calibration=register_moves,indexed_transfers,abi_stack,backedges,port_sink frame_partition=exclusive');

const totals = Object.fromEntries(['default', 'all_spill', 'ra'].map(mode => [mode,
  Object.fromEntries(['cases', ...executionMetrics].map(metric => [metric, 0]))]));
const programTotals = {};
const shapes = new Map(programs.map(program => [program.name,
  program.source.replace(program.name, 'generated').replace(/ra_arithmetic_\d+/g, 'ra_arithmetic').replace(/\b\d+\b/g, '#')]));
const uniqueInputs = new Set();
for (const program of programs) {
  programTotals[program.name] = Object.fromEntries(Object.keys(totals).map(mode => [mode,
    Object.fromEntries(['cases', ...executionMetrics].map(metric => [metric, 0]))]));
  for (let seed = 0; seed < cases; seed++) {
    const byte = (seed * 73 + program.variant * 19) & 255;
    const other = (seed * 29 + 17) & 255;
    const third = (seed * 47 + 3) & 255;
    const buffer = Uint8Array.from({length: 128}, (_, index) => (index * 47 + seed * 11 + program.variant) & 255);
    const expectedBuffer = buffer.slice();
    let parameters = [byte, other, third];
    let expected, expectedPorts = [];
    switch (program.family) {
      case 'matrix_byte':
      case 'matrix_word':
      case 'matrix_compare_byte':
      case 'matrix_compare_word':
      case 'matrix_compare_signed': {
        const word = program.family.endsWith('_word');
        const mask = word ? 65535 : 255;
        if (word) parameters = [(seed * 7919 + program.variant * 1009) & mask,
          (seed * 5113 + 65535) & mask, (seed * 3571 + 32767) & mask];
        if (program.family.startsWith('matrix_compare_') && seed < 65) {
          const edges = word ? [0, 1, 255, 256, 32767, 32768, 65534, 65535] :
            [0, 1, 2, 127, 128, 129, 254, 255];
          const saved = edges[Math.floor(Math.max(0, seed - 1) / 8)];
          parameters = word ? [byte, mask ^ saved, edges[Math.max(0, seed - 1) % 8]] :
            [byte, byte ^ saved, edges[Math.max(0, seed - 1) % 8]];
          if (seed === 0) parameters = word ? [0, 65535 ^ 68, 68] :
            program.family === 'matrix_compare_signed' ? [184, 75, 97] : [0, 68, 68];
          if (comparisonForms === 2) parameters[0] = saved;
        }
        const [first, second, last] = parameters;
        let saved = comparisonForms === 2 ? first : word ? second ^ mask : first ^ second;
        if (program.family === 'matrix_compare_signed') saved = saved >= 128 ? saved - 256 : saved;
        if (program.family.startsWith('matrix_compare_')) {
          const lastValue = comparisonForms ? (program.family === 'matrix_compare_signed' ? -1 : word ? 32768 : 128) :
            program.family === 'matrix_compare_signed' && last >= 128 ? last - 256 : last;
          const [left, right] = program.variant >= 6 ? [lastValue, saved] : [saved, lastValue];
          expected = [left === right, left !== right, left < right, left > right,
            left <= right, left >= right][program.variant % 6] ? first : second;
        } else {
          const [left, right] = program.variant & 1 ? [saved, last] : [last, saved];
          expected = [left + right, left - right, left & right, left | right, left ^ right][program.variant % 5] & mask;
        }
        break;
      }
      case 'matrix_extend': {
        parameters[2] = (seed * 3571 + 32767) & 65535;
        const saved = byte ^ other;
        expected = (parameters[2] + (saved >= 128 ? saved - 256 : saved)) & 65535;
        break;
      }
      case 'matrix_low_byte': {
        parameters = [(seed * 7919 + (program.variant % 2) * 1009) & 65535,
          (seed * 5113 + 65535) & 65535, (seed * 3571 + 32767) & 65535];
        if (seed > 0 && seed <= 64) {
          const edges = [0, 1, 127, 128, 255, 256, 32768, 65535];
          parameters = [edges[Math.floor((seed - 1) / 8)], edges[(seed - 1) % 8],
            edges[(seed + 2) % 8]];
        }
        const narrowed = (parameters[0] ^ 65535) & 255;
        expected = (parameters[2] + (program.variant < 2 ?
          ((parameters[0] ^ 65535) + parameters[1]) & 255 :
          parameters[1] + (program.variant >= 6 && narrowed >= 128 ? narrowed - 256 : narrowed))) & 65535;
        break;
      }
      case 'matrix_index_affinity':
        parameters = [65535 - seed % buffer.length];
        expected = buffer[seed % buffer.length];
        break;
      case 'matrix_index_copy': {
        const word = [2, 3, 11].includes(program.variant);
        const signed = [1, 3, 5].includes(program.variant);
        const offset = signed ? word ? 16 : 32 : 0;
        const index = program.variant === 8 ? 7 : signed ? seed % (offset * 2 - 1) - offset + 1 : seed % (word ? 64 : 128);
        parameters = [index & (program.variant === 5 ? 255 : 65535), other];
        if (program.variant === 9) {
          parameters = [0, seed & 1 ? 0 : 16, seed % 64, other];
          expectedBuffer[parameters[2]] = other;
          expected = expectedBuffer[parameters[1] + parameters[2]];
        } else {
          if (program.variant === 6) expectedBuffer[index] = other;
          if (program.variant >= 10) {
            expectedBuffer[index * (word ? 2 : 1)] = other;
            if (word) expectedBuffer[index * 2 + 1] = 0;
            expected = index + 1;
          } else expected = word ? buffer[(offset + index) * 2] + buffer[(offset + index) * 2 + 1] * 256 : expectedBuffer[offset + index];
        }
        break;
      }
      case 'matrix_bc_alu': {
        const edges = [0, 1, 127, 128, 255, 256, 32768, 65535];
        parameters = seed < 64 ? [edges[Math.floor(seed / 8)], edges[seed % 8], edges[(seed + 3) % 8]] :
          [(seed * 7919) & 65535, (seed * 5113 + 65535) & 65535, (seed * 3571 + 32767) & 65535];
        const [first, second, last] = parameters;
        const saved = [first + second, first - second, first & second, first | second, first ^ second][program.variant % 5] & 65535;
        expected = program.variant < 10 ? (program.variant < 5 ? last - saved : last + saved) & 65535 :
          program.variant === 10 ? ((((first + last) & 65535) - ((second + last) & 65535)) & 65535) ^ ((first + last) & 65535) :
          ((second + last + (first & 255)) & 65535) ^ (first & 255);
        break;
      }
      case 'matrix_scalar_uses': {
        if (program.widths[0] === 2) parameters = [(seed * 7919) & 65535, (seed * 5113 + 65535) & 65535, (seed * 3571 + 32767) & 65535];
        if (program.variant === 8) parameters[0] = ((seed % 63) - 31) & 255;
        const [first, second, last] = parameters;
        const saved = first ^ second, called = (saved + second + last) & 255;
        expected = [saved < last ? first : second, (saved << (last & 15)) & 65535, (second << (first & 15)) & 65535,
          buffer[first & 127], (called >= 128 ? called - 256 : called) & 65535, (saved + second + last) & 65535,
          (saved >= 128 ? saved - 256 : saved) < (last >= 128 ? last - 256 : last) ? 1 : 0,
          saved >>> (last & 7), buffer[32 + (first >= 128 ? first - 256 : first)], called ^ saved,
          ((((second & 1) ? first : last) + second) & 65535) ^ ((second & 1) ? first : last), second][program.variant];
        break;
      }
      case 'matrix_real_index':
      case 'matrix_source_loop': {
        if (program.family === 'matrix_source_loop' && program.variant) {
          const start = 32, count = seed % 64;
          parameters = [start, start + count];
          expected = 0;
          expectedPorts = Array.from(buffer.slice(start, start + count), value => [0xbe, value]);
          break;
        }
        parameters = [0, 1, 0];
        const text = '$0115 = $0115 = $0115 = $0115';
        for (let index = 0; index < text.length; index++) {
          const character = text[index];
          expectedBuffer[index * 2] = character === '$' ? 4 : character === '=' ? 0x1d : character === ' ' ? 0 : parseInt(character, 16) + 0x10;
          expectedBuffer[index * 2 + 1] = 0;
        }
        expected = text.length * 2;
        if (program.family === 'matrix_source_loop')
          expectedPorts = [[0xbf, 0], [0xbf, 0x7b], ...Array.from(expectedBuffer.slice(0, expected), value => [0xbe, value])];
        break;
      }
      case 'matrix_rank':
      case 'matrix_affinity': {
        const edges = [0, 1, 127, 128, 255, 256, 32768, 65535];
        parameters = seed < 64 ? [edges[Math.floor(seed / 8)], edges[seed % 8], edges[(seed + 3) % 8]] :
          [(seed * 7919) & 65535, (seed * 5113 + 65535) & 65535, (seed * 3571 + 32767) & 65535];
        const live = program.family === 'matrix_rank' ? parameters[0] ^ 65535 : (parameters[0] + parameters[1]) & 65535;
        const saved = parameters[2] ^ 65535;
        expected = [live + saved, live & saved, live | saved, live ^ saved][program.variant % 4] & 65535;
        break;
      }
      case 'matrix_sentinel': {
        const edges = [0, 1, 127, 128, 255, 256, 32768, 65535];
        parameters = [(seed * 7919) & 65535, (seed * 5113 + 65535) & 65535,
          (seed * 3571 + 32767) & 65535];
        if (seed < 64) parameters = [edges[Math.floor(seed / 8)], edges[seed % 8], edges[(seed + 3) % 8]];
        const [first, second, last] = parameters;
        const live = (first + second) & 65535;
        const saved = last ^ 65535;
        if (program.variant < 2) expected = program.variant ? ((live ^ saved) + live) & 65535 : ((live + saved) & 65535) ^ live;
        else if (program.variant < 4) {
          parameters[0] &= 255; parameters[1] &= 255;
          const byte = parameters[0] ^ parameters[1];
          expected = ((last + last) + (byte >= 128 ? byte - 256 : byte)) & 65535;
        } else if (program.variant < 6) expected = ((live + (last & 255)) & 65535) ^ live;
        else if (program.variant < 8) expected = ((program.variant & 1 ? live >>> (last & 15) : live << (last & 15)) & 65535) ^ live;
        else {
          parameters = [((seed % 63) - 31) & 255, other, third];
          const index = parameters[0] ^ (program.variant & 1);
          expectedBuffer[32 + (index >= 128 ? index - 256 : index)] = other ^ third;
          expected = (other ^ third) * 2;
        }
        break;
      }
      case 'matrix_shift': {
        parameters[0] = (seed * 7919 + program.variant * 1009) & 65535;
        parameters[1] = (seed * 5113 + 65535) & 65535;
        const saved = parameters[0] ^ 65535;
        expected = (program.variant & 1 ? saved >>> (third & 15) : saved << (third & 15)) & 65535;
        break;
      }
      case 'matrix_structural': {
        parameters = [0, program.variant & 2 ? 0 : 8, seed & 15, program.variant >= 8 ? seed % 8 : 1 + seed % 4, other, byte];
        expected = byte;
        for (let row = 0; row < parameters[3]; row++) {
          for (let column = 0; column < (program.variant & 4 ? 2 : 1); column++) {
            const index = parameters[2] + row + column;
            const alias = parameters[1] + index;
            const previous = expectedBuffer[index];
            let called = (previous + other + row) & 255;
            if ((previous ^ other) & 1) expectedBuffer[alias] = (previous + (other ^ byte)) & 255;
            else expectedBuffer[index] = previous ^ other ^ byte;
            if (!(program.variant & 1)) called = (expectedBuffer[alias] + other + row) & 255;
            expected = (expected + expectedBuffer[index] + called) & 65535;
            if (program.variant >= 8)
              expected = (expected + (row & 1 ? expected & 255 : other & 7)) & 65535;
          }
        }
        break;
      }
      case 'scalar_region':
      case 'scalar_call_region': {
        const saved = byte ^ other;
        const first = (third + saved) & 255;
        let value = (((first + buffer[1]) & 255) ^ saved) + saved;
        value = (((value & 255) ^ saved) + saved) & 255;
        if (program.family === 'scalar_region') {
          if (program.variant === 1 && byte < other) expectedBuffer[2] = value;
          value = (value + (byte < other ? 1 : 0)) & 255;
          expected = ((((value ^ saved) + saved) & 255) ^ saved);
        } else {
          const calledByte = program.variant === 1 ? ((byte + 2) & 255) ^ ((byte + 1) & 255) ^ byte ^ 3 : byte;
          expectedBuffer[0] = (calledByte + other + third) & 255;
          value = (((value ^ saved) + saved) & 255) ^ saved;
          expected = ((value + saved) & 255) ^ buffer[1];
        }
        break;
      }
      case 'scalar_region_escape':
        expected = ((byte < other ? third : byte ^ other) + other) & 255;
        break;
      case 'scalar_loop_pressure': {
        const iterations = Math.max(0, Math.ceil((other - byte) / 2));
        expected = (byte + iterations) ^ (other - iterations);
        break;
      }
      case 'scalar_loop_call': {
        parameters[2] = seed & 7;
        let value = other;
        for (let iteration = 0; iteration < parameters[2]; iteration++) {
          expectedBuffer[0] = value;
          value = (buffer[1] + (byte ^ other)) & 255;
        }
        expected = value;
        break;
      }
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
      case 'scalar_rank_fallback':
      case 'scalar_reassigned_word':
      case 'scalar_lifetime_hole':
      case 'scalar_narrow':
      case 'word_bc': {
        parameters = [(seed * 7919 + program.variant * 1009) & 65535, (seed * 5113 + 65535) & 65535, (seed * 3571 + 32767) & 65535];
        const [first, second, thirdWord] = parameters;
        if (program.family === 'scalar_lifetime_hole') {
          const called = program.variant ? ((first & 255) + (second & 255) + (thirdWord & 255)) & 255 : 0;
          const middle = (second + thirdWord + called) & 65535;
          expected = ((((thirdWord + (first ^ second)) & 65535) ^ middle) + (((first + thirdWord) & 65535) ^ middle)) & 65535;
          break;
        }
        if (program.family === 'scalar_reassigned_word') {
          const initial = first ^ second;
          const saved = !program.variant || (first & 1) ? second ^ thirdWord : initial;
          expected = (((thirdWord + initial + saved) & 65535) ^ saved) & 65535;
          break;
        }
        if (program.family === 'scalar_rank_fallback') {
          const saved = first ^ 65535;
          expected = (saved + ((saved + second) & 65535)) & 65535;
          if (thirdWord & (1 << program.variant)) expected = (expected + saved) & 65535;
          break;
        }
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
      case 'scalar_reassigned_byte': expected = (((third + (byte ^ other) + (byte ^ third)) & 255) ^ byte ^ third) & 255; break;
      case 'scalar_source_reassigned':
      case 'scalar_source_bitwise': {
        parameters = [(seed * 7919 + 32767) & 65535, other, third];
        if (seed < 4) parameters = [[0, 65535, 0x6543, 32768][seed], [0, 255, 0x95, 128][seed], [0, 255, 0, 1][seed]];
        const [first, second, last] = parameters;
        if (program.family === 'scalar_source_reassigned') {
          expectedBuffer[0] = 0x21;
          expectedBuffer[1] = (((((first | second) % 0x1001) << 4) - 10000) ^ 0x4321) & 255;
        }
        expected = ((((((first | last) | (last | second)) % 0x1001) << 4) - (last | 10000)) ^ (0x4321 | last)) & 65535;
        break;
      }
      case 'scalar_frequency_loop': {
        if (seed < 4) parameters = [[255, 0, 252, 247][seed] ^ program.constant, [0, 1, 255, 255][seed], third];
        expected = Math.max(parameters[0] ^ program.constant, parameters[1]);
        break;
      }
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
    uniqueInputs.add(JSON.stringify([shapes.get(program.name), parameters, Buffer.from(buffer).toString('base64')]));
    for (const mode of Object.keys(totals)) {
      const image = compiled.get(mode);
      const bufferAddress = image.symbols.get(program.family === 'scalar_remat_word' ||
        (program.family === 'matrix_index_copy' && [2, 3, 11].includes(program.variant)) ? 'ra_words' : 'ra_buffer');
      const args = parameters.slice();
      for (const index of program.pointerParameters) args[index] += bufferAddress;
      try {
        const result = execute(image.rom, image.symbols.get(program.name), args, program.widths,
          buffer, bufferAddress, program.returnWidth, image.transfers, program.family === 'matrix_source_loop');
        if (result.unmapped_instructions) throw new Error(`${program.name}: unmapped executed instructions`);
        if (program.family === 'matrix_source_loop' && result.entry_backedges !== (program.variant ? seed % 64 : expected))
          throw new Error(`${program.name}: source-loop backedges=${result.entry_backedges}, expected=${program.variant ? seed % 64 : expected}`);
        if (program.family === 'scalar_frequency_loop' && result.entry_backedges !== expected - (parameters[0] ^ program.constant))
          throw new Error(`${program.name}: frequency-loop backedges=${result.entry_backedges}, expected=${expected - (parameters[0] ^ program.constant)}`);
        if (mode === 'ra' && process.env.RA_BASELINE !== '1' &&
            ['scalar_split_byte', 'scalar_split_word', 'scalar_call_region'].includes(program.family)) {
          const bytes = program.family === 'scalar_split_word' ? [2, 4] : program.family === 'scalar_call_region' ? [1, 2] : [1, 1];
          if (result.allocator_preserve_writes !== bytes[0] || result.allocator_reload_reads !== bytes[1])
            throw new Error(`${program.name}: explicit transfer byte counts differ`);
        }
        if ((program.returnWidth && result.value !== expected) || !Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer)) ||
            result.port_writes.length !== expectedPorts.length ||
            result.port_writes.some(([port, value], index) => port !== expectedPorts[index][0] || value !== expectedPorts[index][1]))
          throw new Error(`Expected ${expected}, got ${result.value}; buffer_equal=${Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer))}`);
        for (const counters of [totals[mode], programTotals[program.name][mode]]) {
          counters.cases++;
          for (const metric of executionMetrics) counters[metric] += result[metric];
        }
      } catch (error) {
        const retained = new Set([program]);
        for (const caller of retained)
          for (const candidate of programs)
            if (caller.source.includes(`${candidate.name}(`)) retained.add(candidate);
        const reduced = preamble + Array.from(retained, entry => entry.source).join('\n') + '\nvoid main(void) {}\n';
        const reduction = {original_functions: programs.length, retained_functions: retained.size,
          seed, parameters, expected, results: {}, reproduced: false};
        writeFileSync(join(output, 'failure-reduced.ssc'), reduced);
        for (const replayMode of new Set(['default', mode])) {
          const directory = join(output, 'failure-reduced', replayMode);
          mkdirSync(directory, {recursive: true});
          writeFileSync(join(directory, 'main.ssc'), reduced);
          try {
            run(compiler, ['-mz80', '-rs', '3', ...modes.find(entry => entry[0] === replayMode)[1],
              '-o', 'main.asm', 'main.ssc'], directory, 'compiler.log');
            run(linker, ['-lsms', '-v', '-o', 'linked.sms', 'main.asm'], directory, 'linker.log');
            const replay = readImage(directory);
            const address = replay.symbols.get(program.family === 'scalar_remat_word' ||
              (program.family === 'matrix_index_copy' && [2, 3, 11].includes(program.variant)) ? 'ra_words' : 'ra_buffer');
            const replayArgs = parameters.slice();
            for (const index of program.pointerParameters) replayArgs[index] += address;
            const result = execute(replay.rom, replay.symbols.get(program.name), replayArgs,
              program.widths, buffer, address, program.returnWidth, replay.transfers, program.family === 'matrix_source_loop');
            reduction.results[replayMode] = {value: result.value,
              correct: (!program.returnWidth || result.value === expected) && Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer)) &&
                JSON.stringify(result.port_writes) === JSON.stringify(expectedPorts)};
          } catch (replayError) {
            reduction.results[replayMode] = {correct: false, error: String(replayError)};
          }
        }
        reduction.reproduced = !reduction.results[mode].correct &&
          (mode === 'default' || reduction.results.default.correct);
        writeFileSync(join(output, 'failure-reduction.json'), JSON.stringify(reduction, null, 2));
        writeFileSync(join(output, 'failure.json'), JSON.stringify({mode, seed, program, parameters: args,
          expected, expected_ports: expectedPorts, buffer: Array.from(buffer), error: String(error), reduction}, null, 2));
        throw new Error(`Failure ${program.name} seed=${seed} mode=${mode}: ${error}; artifacts=${output}`);
      }
    }
  }
}
for (const [mode, totalsForMode] of Object.entries(totals))
  console.log(JSON.stringify({mode, ...totalsForMode, compile_seconds: compiled.get(mode).compileSeconds}));
writeFileSync(join(output, 'execution-summary.json'), JSON.stringify(totals, null, 2));
writeFileSync(join(output, 'program-execution-summary.json'), JSON.stringify(programTotals, null, 2));
const qualityTotals = {...programTotals};
if (process.env.RA_FAMILIES?.split(',').includes('matrix_source_loop') &&
    programs.filter(program => program.family === 'matrix_source_loop').length !== 2)
  throw new Error('Both source-loop cases must be selected');
const coverage = {functions: programs.length, unique_shapes: new Set(shapes.values()).size,
  shape_definition: 'normalized_source_not_unique_cfg',
  source_loops: programs.filter(program => program.family === 'matrix_source_loop').map(program => ({name: program.name,
    source: `calculations-1/${program.variant ? 'bank_1.ssc:calculateAndPrint5:while' : 'bank_2.ssc:calculateAndPrint3:for'}`})),
  unique_shape_input_tuples: uniqueInputs.size, cases_per_mode: totals.ra.cases,
  retained_registers: Array.from(new Set(Array.from(readFileSync(join(output, 'ra/main.asm'), 'utf8')
    .matchAll(/; retained (?:arg[12]|result) in (HL|BC|A|B|C)\b/g), match => match[1]))).sort(),
  node: process.version, emulator: emulatorVersion};
writeFileSync(join(output, 'coverage-summary.json'), JSON.stringify(coverage, null, 2));
console.log(JSON.stringify({coverage}));
console.log(`PASS:verifier_failures=${verifierFailures} immutable=yes differential_cases=${totals.ra.cases} families=${new Set(programs.map(program => program.family)).size} default_no_ra=identical artifacts=${output}`);

if (process.env.RA_REAL_PROJECTS !== '0') {
  const realTotals = {};
  for (const [project, routines] of [
    ['calculations-1', ['calculationAdd3', 'calculationMultiplyAdd', 'calculationXorRoundTrip', 'calculationQuotientRoundTrip', 'convertNumbersToChars16']],
    ['calculations-2', ['calculationProductRestore', 'calculationShiftRestore', 'calculationPartitionRestore', 'convertNumbersToChars16']]
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
    writeFileSync(join(output, project, 'static-summary.json'), JSON.stringify(Object.fromEntries(
      Array.from(images, ([mode, image]) => [mode, image.staticMetrics])), null, 2));
    realTotals[project] = {};
    for (const name of routines) {
      const routineTotals = Object.fromEntries(Object.keys(totals).map(mode => [mode,
        Object.fromEntries(['cases', ...executionMetrics].map(metric => [metric, 0]))]));
      realTotals[project][name] = routineTotals;
      qualityTotals[`${project}/${name}`] = routineTotals;
      for (let seed = 0; seed < cases; seed++) {
        const value = (seed * 7919 + 32767) & 65535;
        const other = (seed * 5113 + 17) & 65535;
        const third = (seed * 3571 + 3) & 65535;
        let parameters;
        let expected;
        let buffer = new Uint8Array(0), expectedBuffer = buffer, bufferAddress = 0, returnWidth = 2;
        switch (name) {
          case 'convertNumbersToChars16': {
            bufferAddress = 0xc100;
            buffer = Uint8Array.from({length: 128}, (_, index) => (index * 47 + seed * 11) & 255);
            const start = buffer[120] = seed & 31;
            expectedBuffer = buffer.slice();
            parameters = [value, bufferAddress, bufferAddress + 120];
            returnWidth = 0;
            const digits = value.toString(16).padStart(4, '0');
            for (let index = 0; index < digits.length; index++) {
              const digit = parseInt(digits[index], 16);
              expectedBuffer[start + 2 * index] = digit + (digit < 10 ? 0x10 : 0x17);
              expectedBuffer[start + 2 * index + 1] = 0;
            }
            expectedBuffer[120] = start + 8;
            break;
          }
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
            const result = execute(image.rom, image.symbols.get(name), parameters, parameters.map(() => 2), buffer, bufferAddress, returnWidth, image.transfers);
            if ((returnWidth && result.value !== expected) || !Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer)))
              throw new Error(`Expected ${expected}, got ${result.value}; buffer_equal=${Buffer.from(result.buffer).equals(Buffer.from(expectedBuffer))}`);
            routineTotals[mode].cases++;
            for (const metric of executionMetrics) routineTotals[mode][metric] += result[metric];
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
  console.log(`PASS:real_routines=${Object.values(realTotals).reduce((count, routines) => count + Object.keys(routines).length, 0)} cases_per_routine=${cases} default_no_ra=identical artifacts=${output}`);
}

const qualityRows = Object.entries(qualityTotals).map(([routine, modes]) => ({routine,
  cycles_delta: modes.ra.cycles - modes.default.cycles,
  register_moves_delta: modes.ra.register_moves - modes.default.register_moves,
  frame_traffic_delta: modes.ra.frame_reads + modes.ra.frame_writes - modes.default.frame_reads - modes.default.frame_writes,
  allocator_preserve_bytes: modes.ra.allocator_preserve_writes, allocator_reload_bytes: modes.ra.allocator_reload_reads}));
const outliers = Object.fromEntries(['cycles', 'register_moves', 'frame_traffic'].map(metric =>
  [metric, qualityRows.filter(row => row[`${metric}_delta`] > 0).sort((left, right) => right[`${metric}_delta`] - left[`${metric}_delta`]).map(row => row.routine)]));
writeFileSync(join(output, 'quality-summary.json'), JSON.stringify({baseline: 'default',
  register_moves: 'executed unprefixed byte-register LD instructions; stack/exchange traffic is not a register LD',
  frame_traffic: 'exclusive byte-access categories; other_frame remains unclassified', per_routine: qualityRows, increases: outliers}, null, 2));
console.log(JSON.stringify({quality_outliers: Object.fromEntries(Object.entries(outliers).map(([metric, routines]) => [metric, routines.length]))}));

if (process.env.RA_SCALAR_CASES !== '1' && process.env.RA_MATRIX_CASES !== '1') {
  for (const cohort of process.env.RA_MATRIX_CASES === '0' ? ['scalar'] :
    ['scalar', 'matrix', 'matrix_literals', 'matrix_memory', 'matrix_values']) {
    Object.assign(process.env, {RA_SCALAR_CASES: cohort === 'scalar' ? '1' : '0',
      RA_MATRIX_CASES: cohort === 'scalar' ? '0' : '1', RA_REAL_PROJECTS: '0',
      RA_VALUE_CASES: cohort === 'matrix_values' ? '1' : '0',
      RA_COMPARISON_FORMS: cohort === 'matrix_literals' ? '1' : cohort === 'matrix_memory' ? '2' : '0',
      RA_EXECUTION_DIR: join(output, cohort), RA_VERIFIER: harness});
    run(process.execPath, [fileURLToPath(import.meta.url)], output, `${cohort}-execution.log`);
    process.stdout.write(readFileSync(join(output, `${cohort}-execution.log`), 'utf8'));
  }
}