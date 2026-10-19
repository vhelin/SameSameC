# Z80 Code-Generation Improvement Plan

## Scope And Decision

Keep the bounded Z80 allocator v1 complete as documented in
[REGISTER_ALLOCATOR.md](REGISTER_ALLOCATOR.md). Optimize the generated program,
not the number of allocator milestones. Do not replace the proven allocator or
add another optimization framework without a measured need.

The comparison at `d1e7db1` on 2026-10-19 shows substantial remaining opportunities
in control-flow lowering, loop state, address calculation and the calling
convention. Keep those reference results immutable; record implementation
progress and new measurements separately below.

Reuse the current TAC, CFG/liveness, target-policy, location-resolution and
emission helpers. Prefer removing redundant work over adding new functions.
Each implementation batch must show the source/TAC/assembly cause, measured
before/after results, correctness evidence and a local commit. Do not push
unless explicitly requested.

## Measured Starting Point

The committed [baseline](ci/z80_compiler_baseline.json) records 13 unsigned
8/16-bit workloads in eight configurations: SameSameC RA/no-RA, stock SDCC
size/speed, z88dk/sccz80 size/speed and z88dk/SDCC size/speed. Ten workloads reuse
actual calculation-test functions; three exercise byte sum, copy and fill.

All 104 original builds passed 1,856 complete calls against independent expected
values and complete buffer contents. Twelve whole-call timings matched a second
Z80 emulator exactly. Source compatibility changes are limited to C type aliases
and explicit unsigned promotion of byte values before left-shifting by eight.
There are no handwritten assembly kernels in this comparison.

Representative machine-code bytes, using competitors' size settings:

| Workload | SameSameC RA | Stock SDCC | z88dk/SDCC | z88dk/sccz80 |
| --- | ---: | ---: | ---: | ---: |
| Multiply/add | 71 | 40 | 80 | 85 |
| Byte fill | 161 | 38 | 42 | 80 |
| Byte copy | 191 | 59 | 63 | 99 |
| Byte sum | 182 | 47 | 52 | 94 |
| Array | 2065 | 765 | 882 | 1129 |
| Stencil | 2414 | 1064 | 1122 | 1363 |
| Tile decoding/CRC | 1866 | 688 | 664 | 1029 |
| Pathfinding | 2413 | 882 | 917 | 1180 |
| PackBits decoder | 1090 | 418 | 380 | 473 |
| PackBits plus CRC | 1677 | 538 | 491 | 670 |

Per-workload geometric means put SameSameC RA at 2.44 times stock SDCC's size
setting and 2.53 times its speed setting's execution cycles. These are different
settings, not one claimed size/speed optimum. Enabling RA reduces the sums of
standalone kernel code and execution cycles by 16.2% and 15.4%, respectively.

Specific evidence worth retaining:

- Filling 256 bytes takes 164108 kernel T-states in SameSameC RA, versus 29837
  with stock SDCC's size setting. SameSameC performs 2054 kernel stack writes
  versus four. A loop-condition Boolean is stored and reloaded before branching.
- The multiply/add native caller wrapper is 172 bytes / 971 T-states in
  SameSameC versus 20 / 114 in stock SDCC. Wrapper costs are separate from kernel
  measurements; these wrappers also load inputs and store the result.
- Mean PackBits-plus-CRC kernel cost is approximately 2.23 million T-states in
  SameSameC versus 456000 in speed-oriented stock SDCC.
- Whole calculation ROMs use 24066 and 39647 bytes with RA, versus 26862 and
  48400 without it. Their padded file sizes are all 65536 bytes.

### Measurement Boundaries

- `code_bytes` includes the kernel, required user dependencies and compiler
  arithmetic helpers. It excludes the external caller wrapper, CRT/startup,
  packed data and BSS. Shared helpers count once per standalone workload.
- `total_kernel_cycles` covers entry through return, including nested helpers
  and the callee's native prologue/epilogue. Caller work is recorded separately.
- Maximum SP displacement is a stack high-water observation, not a proof of
  total RAM use: SameSameC also accesses frame storage below the current SP.
- A sum of standalone sizes is not the size of one linked application: helper
  sharing and startup costs differ in whole programs.
- This is a selected unsigned-integer corpus, not C conformance, signed/floating
  point, standard-library, bank-switching or whole-game qualification.
- Emulator T-states omit hardware contention, wait states and interrupts.
- Compiler executable bytes and host `-O3`/`-Os` settings are separate from target
  code quality. Do not use host EXE size as a generated-code metric.

## Implementation Order

### Progress Toward Game Boy Support

Complete the shared improvements first, not every Z80-specific optimization.
The initial SM83 backend should guide further shared-core extraction.

- [ ] **1. Direct branch-only conditions:** remove unnecessary Boolean
  temporaries through shared TAC lowering, preserving value-producing Booleans.
  - [x] Lower final relational comparisons, scalar tests and negation directly.
  - [x] Qualify signedness, promotions, side effects and all loop forms.
  - [ ] Complete compound logical lowering with short-circuit evaluation tests.
  - [x] Record first-batch gains and pass the full checkpoint gates.
- [ ] **2. Bounded loop-state improvements:** retain profitable counters,
  accumulators and CRC state using existing analysis and legality checks; keep
  register choices and costs target-specific. Stop before speculative expansion.
- [ ] **3. Initial SM83 backend:** establish correct Game Boy code generation,
  target-specific addressing/spills/calls and execution tests. Use actual
  requirements to identify reusable code; do not assume IX/IY, `DJNZ` or `LDIR`.
- [ ] **4. Further target-specific improvements:** continue induction-address,
  instruction-selection, call and arithmetic work with both backends covered.
  These are not prerequisites for initial Game Boy support. Keep the separate
  6502 backend/policy boundary described below.

The numbered detail sections below describe optimization priorities, not a
requirement to finish every Z80 optimization before implementing SM83.

Do one bounded, measured change at a time. The numeric observations below are
baseline anchors, not promised results. Passing a focused test is not permission
to skip the full checkpoint gates.

### 0. Keep The Measurement Loop Working

- [x] Preserve [the cross-compiler runner](ci/compare_z80_compilers.py), original
  measurements, source/input fingerprints and external tool version pins.
- [x] Reuse the existing mathematical/buffer oracles and pinned native emulator.
  Share native instruction stepping with the allocator regression runner.
- [x] Keep caller overhead separate, retain generated source/assembly/maps/logs,
  reject stale output directories and fail loudly on wrong results or drift.
- [ ] Add a provisioned CI job when the external toolchains can be cached with
  verified versions. Until then, full local comparisons are a checkpoint
  requirement, not a claim that the existing CI already runs these compilers.

### 1. Lower Branch-Only Conditions Directly

Start in [pass_4.c](compiler/pass_4.c), particularly the existing condition-test
and `if`/`while`/`do`/`for` lowering. Reuse existing comparison and jump TAC forms.
Use [pass_5.c](compiler/pass_5.c) only where an existing local rewrite is the
clearer place to remove a redundant value/test chain.

- Distinguish a Boolean used as a value from a condition consumed only by a
  branch. Preserve 0/1 materialization where the program needs that value.
- Emit the true/false jump directly for branch-only comparisons. Preserve
  signedness, mixed-width promotion, side effects and short-circuit evaluation.
- Handle negation and both branch orientations through existing lowering,
  rather than duplicating six comparison emitters.
- Test all relational operators on signed/unsigned byte/word boundaries,
  nested logical expressions, calls in conditions, zero-iteration loops,
  `continue`, `break` and `do`-loop conditions.

Acceptance: targeted branch-only conditions have no Boolean spill/reload chain;
byte fill/sum/copy code and cycles improve strictly from the measured baseline,
with no unexplained regression elsewhere. Keep actual Boolean-value tests.
This is the first implementation batch, before a new calling convention.

#### First Batch: Direct Final Conditions

Implemented against source commit `8052a49`, measured against the unchanged
`d1e7db1` reference. The existing expression/postfix evaluator in
[il.c](compiler/il.c) accepts a false-branch destination. Final comparisons
invert their existing jump operation instead of materializing a Boolean,
joining its 0/1 definitions and reloading it to test against zero. Scalar tests
and trailing negation use the same path. Boolean values, including comparisons
used as array indices, still materialize normally. The superseded pass-4 test
helper was removed; no production functions were added.

Qualification exposed two coupled correctness issues: the dead-store scan must
stop at conditional jumps, and signed word comparisons must account for
subtraction overflow. Both branch and value contexts are covered by the new
boundary tests. The existing spill-layout helper packs its movable suffix
widest-first, preserving fixed spill owners and avoiding a one-byte pathfinding
stack regression. Its mixed-width regression now requires that saving rather
than accepting the old narrower-slot-first layout. Production compiler source
growth is 57 net lines across the five changed files.

Full comparison results with RA enabled:

| Kernel | Code bytes before -> after | Kernel T-states before -> after | Max stack bytes before -> after |
| --- | ---: | ---: | ---: |
| Multiply/add | 71 -> 71 | 20217 -> 20217 | 24 -> 24 |
| Product restore | 197 -> 197 | 62117 -> 62117 | 28 -> 28 |
| Shift restore | 84 -> 84 | 12161 -> 12161 | 22 -> 22 |
| Partition restore | 193 -> 193 | 61545 -> 61545 | 28 -> 28 |
| Array | 2065 -> 1847 | 8172676 -> 7857958 | 88 -> 88 |
| Stencil | 2414 -> 1886 | 5989154 -> 4804082 | 122 -> 122 |
| Tile decoding/CRC | 1866 -> 1427 | 4984256 -> 3183806 | 128 -> 128 |
| Pathfinding | 2413 -> 1918 | 9977593 -> 7868925 | 187 -> 187 |
| PackBits decoder | 1090 -> 926 | 280791 -> 218149 | 42 -> 42 |
| PackBits plus CRC | 1677 -> 1335 | 37937604 -> 22097231 | 54 -> 54 |
| Byte sum | 182 -> 139 | 627415 -> 435254 | 26 -> 24 |
| Byte copy | 191 -> 148 | 791598 -> 599437 | 27 -> 27 |
| Byte fill | 161 -> 118 | 611485 -> 419324 | 26 -> 24 |

Cycles are totals over the unchanged per-kernel input sets, not single-call
timings. Every caller-wrapper metric is unchanged. Neither SameSameC mode
regresses in the reference gate; all 78 competitor rows reproduce exactly.
All 104 builds / 1856 calls / 24 independent whole-call timing checks pass.
The RA/stock-SDCC geometric code-size ratio improves from 2.44 to 2.08 using
SDCC's size setting; the cycle ratio improves from 2.53 to 2.04 using its speed
setting. These remain separate settings, not a joint optimum.

Both full calculation projects rebuild with RA enabled. Used ROM drops from
24066 to 23277 bytes in [calculations-1](tests/z80/sms/calculations-1), and from
39647 to 35589 bytes in [calculations-2](tests/z80/sms/calculations-2). These are
used-byte counts from linker summaries; padded files remain 65536 bytes.

[The condition regression](ci/test_branch_conditions.py) reuses the allocator
build and native CPU helpers. Per host configuration it executes 43200
signed/unsigned/mixed-width relational and Boolean-value cases plus 420
control-flow/call/index cases. Normal Release, traced Release and traced ASan
builds pass and emit the same 35 ROMs byte for byte. MSVC Release uses
`/MD /O2 /Ob2 /DNDEBUG /WX`; ASan adds `/fsanitize=address /Zi`. The changed C
sources also pass GCC `-O3 -ansi -pedantic -Wall -Werror -DNDEBUG`.

The full allocator corpus passes at 64 seeds across all six generated cohorts
and 28 real calculation routines, including immutable verifier/fixed-prefix
controls and default/no-RA and traced/quiet equivalence. The ASan compiler also
passes that corpus at four seeds. The 17 comparison-tool tests pass normally
and with `python -O`. All 175 existing compiler fixtures pass, including both
full calculation builds. Boolean-specific assembly/trace fixtures now explicitly
consume comparison values, retaining their original join-allocation coverage
alongside the new direct-branch shape/execution tests.

Run the new regression with the same compiler/linker/assembler PATH as the
existing allocator tests:

```sh
python -m unittest discover -s ci -p test_branch_conditions.py
```

**Still open:** compound logical lowering and its short-circuit side-effect
tests. This first batch does not mark step 1 complete and does not start SM83.

### 2. Keep Profitable Loop State In Registers

Inspect the remaining hot loops after step 1. Work through existing candidate,
CFG/liveness and target-policy machinery in [pass_5.c](compiler/pass_5.c) and
[register_allocator.c](compiler/register_allocator.c), and existing concrete
location resolution in [pass_6_z80.c](compiler/pass_6_z80.c).

- Profile named loop counters, accumulators and CRC state, not just TAC temps.
- Promote only proven, profitable loop-carried definitions. Reuse current
  legality, cost, alias/escape, pair-overlap and barrier checks.
- Remove redundant frame-cache setup only where incoming state is proven.
  Calls, inline assembly and conflicting paths must invalidate it correctly.
- Do not turn unsupported aliasing or pressure cases into speculative retention.
- Cover zero/one/255/256 iterations, wraparound, nested loops, joins, early exits,
  pressure, calls, address-taking and inline assembly.

Acceptance: demonstrate fewer executed frame accesses in byte loops and CRC,
not merely fewer emitted spill comments. Record code, cycles and stack changes.
Preserve immutable verifier rejection, transaction rollback and fallback tests.

### 3. Maintain Induction Addresses And Select Z80 Loops

Reuse existing array/pointer TAC and Z80 address emitters. Introduce a narrowly
scoped rewrite only when base, stride, bounds, aliases and wraparound are known.

- Replace repeated `base + index` reconstruction with advancing addresses where
  legal. Prefer existing `HL`/`BC` location and transfer scheduling.
- Select byte operations/counters only when their ranges and observable
  wraparound agree with the original types.
- Consider `DJNZ` only with correct zero/256 behavior. Keep a word-count path.
- Consider `LDI`/`LDIR` and fill patterns only when count, overlap direction,
  flags/register clobbers and memory/port side effects permit them.
- Preserve source-order behavior for overlapping forward-copy loops; do not
  silently turn them into `memmove`, or execute a 65536-byte block for count zero.

Acceptance: focused exact-byte tests plus complete buffer/guard checks for
overlap, boundaries and zero count; a measured win in byte copy/fill and at
least one real decoder/array workload. Keep Z80 instruction selection target
local and retain the generic safe path.

### 4. Reduce Leaf And Call Overhead

Do this after branch/loop improvements identify the remaining call cost.
Use the existing call lowering, frame layouts, location resolution and
return-value emitters in [pass_4.c](compiler/pass_4.c) and
[pass_6_z80.c](compiler/pass_6_z80.c).

- First remove provably redundant setup/copies within the existing ABI.
- Measure cheap leaf prologues/epilogues before designing another convention.
- If register arguments/results are justified, make the convention explicit and
  gated. Keep existing cross-file, banked, recursive, indirect/assembly-facing
  contracts working; never silently change only one caller or callee.
- Account for reserved DE, byte/pair overlap, caller/callee saves and result
  lifetime. Include mixed-width arguments and nested calls.

Acceptance: reduce the 172-byte/971-cycle caller example and whole call-heavy
workloads, with full caller/callee/frame correctness. Report wrapper and kernel
metrics separately. An ABI change requires dedicated compatibility tests and
documentation, not just a smaller leaf-function listing.

### 5. Add A Measured Size/Speed Arithmetic Policy

Review existing arithmetic emission in [pass_6_z80.c](compiler/pass_6_z80.c) and
existing linker helper generation before adding runtime entry points.

- Compare repeated inline arithmetic with shared helpers, including all call,
  argument, clobber and return overhead.
- Look for quotient/remainder pairs that can share one division, and eliminate
  redundant arithmetic through existing TAC optimization before adding helpers.
- Preserve already good constant multiply/shift paths.
- Define any size/speed choice explicitly. Do not quietly trade cycles for bytes
  under the existing default and hide it by summing unrelated improvements.
- Exhaustively test bounded byte operations; use word edge cases and independent
  numerical oracles, including zero-divisor behavior defined by the compiler.

Acceptance: show standalone and combined-program code bytes, helper sharing and
cycles. Add distinct benchmark modes if a new optimization profile is introduced;
keep the original baseline/settings available.

### 6. Expand Qualification And Guide Future Backends

- Port a complete representative application to the supported common C subset
  before claiming whole-program parity with competing compilers. Match hardware,
  data, startup requirements and libraries; separate hand-written assembly.
- Add workloads only for a concrete gap: signed arithmetic, wider values,
  pointers/aliases, standard-library idioms or banked calling. Version the corpus
  rather than replacing its old meanings.
- Share branch/loop analysis when implementing SM83. Its lack of IX/IY and some
  Z80 instructions requires different addressing/spill/selection decisions.
- Implement a separate 6502 policy/backend. Do not force it into Z80 register-pair,
  stack-frame or block-instruction assumptions.
- Let real second-backend requirements justify extraction. Do not build a new
  generic framework speculatively.

## Repeating The Competitor Comparison

Use Python 3.10+ and the existing dependency:

```sh
python -m pip install -r ci/requirements.txt
```

Install toolchains outside the repository; do not vendor their binaries or
libraries. The original pinned Windows downloads are:

| Toolchain | Official archive | SHA-256 |
| --- | --- | --- |
| Stock SDCC | [2026-10-18 #17016 snapshot](https://sourceforge.net/projects/sdcc/files/snapshot_builds/x86_64-w64-mingw32/sdcc-snapshot-x86_64-w64-mingw32-20261018-17016-cf-x86.zip/download) | `34c7ec74b0478f77d7617113635d9eb6bff2cfd3eb35c9ed19cc1336e8e90938` |
| z88dk | [2.4 Windows release](https://github.com/z88dk/z88dk/releases/tag/v2.4), `z88dk-win32-2.4.zip` | `26d9880ee2e43077808ac86a4b6247a81f5dadc30563ca7cedc58bc4fb5ccb57` |

The expected code generators identify as SDCC `4.6.4 #17016`, sccz80
`23854-4d530b6eb7-20251002` and z88dk's patched SDCC `4.5.0 #15242`. Stock SDCC
uses its native register ABI; the z88dk backends use their shipped newlib ABI.
These are the measured releases, not a claim that all are the latest releases.

Example PowerShell setup; replace the tool installation paths:

```powershell
$env:PATH = "C:\cygwin64\bin;$PWD\binaries;$PWD\wla-dx\binaries;C:\toolchains\sdcc\bin;C:\toolchains\z88dk\bin;$env:PATH"
$env:ZCCCFG = "C:\toolchains\z88dk\lib\config\"
python ci\compare_z80_compilers.py --output-dir C:\benchmarks\ssc-codegen-new
```

`SAMESAMECC`, `SAMESAMECL`, `SDCC` and `ZCC` optionally select executable paths.
Assembler/linker tools and `z88dk-ticks` must be on PATH. The default output is
a new system-temporary directory. A supplied directory must be empty: no cached
compiler output can pass as a measurement after a rebuild.

POSIX installations use the same Python entry point, their normal executable
PATH and their z88dk `ZCCCFG` directory. Version checks and actual emitted
competitor metrics still apply; the archived download hashes above are Windows
provenance, not hashes of POSIX executables.

For a focused development run:

```sh
python ci/compare_z80_compilers.py --names fillBytes sumBytes copyBytes --modes ssc-ra ssc-no-ra sdcc-size
python -m unittest discover -s ci -p test_compare_z80_compilers.py
```

A subset is explicitly reported as `partial`, not a full comparison. Before
closing a compiler batch, run all 13 workloads and all eight configurations.
The full runner also repeats 24 independent whole-call timing checks across
three workloads/eight configurations, including the 256-byte fill.

### What Fails And What Gets Recorded

- Missing selected tools, changed source/asset/oracle fingerprints, changed
  settings, wrong output, out-of-bounds writes, unexpected I/O, broken returns,
  instruction bounds and independent timing mismatches fail explicitly.
- Fixed inputs remain identical for each competitor, including the 16 PackBits
  boundary/error vectors and 0/255/256-byte loop cases.
- Each build retains generated input, assembly/listings, linked image, symbols,
  logs, actual command lines, measured call samples and metrics.
- `report.json` records partial/full status, tool versions/hashes, baseline
  deltas and SameSameC/competitor geometric ratios. Negative deltas are gains.
  Build seconds are observations, not a statistically qualified speed benchmark.
- Any increase in SameSameC kernel bytes, wrapper bytes, total kernel/caller
  cycles or observed maximum SP displacement fails the reference gate. Inspect
  each workload: an aggregate win does not cancel a local regression.
- Pinned competitors must reproduce their reference metrics exactly. This also
  catches library/peephole changes despite an unchanged version banner.
- `--allow-competitor-drift` is only for separately evaluating new external
  releases or host differences. It records drift and is not a pinned-control
  pass. It never relaxes correctness or SameSameC regression checks.
- There is no automatic baseline-update switch. Keep the historical baseline.
  A deliberate corpus/settings/profile change requires reviewed versioned
  reference data, new independent validation and an explanation of what is no
  longer directly comparable. Do not regenerate references to hide regressions.

The runner reuses [existing calculation oracles](ci/check_register_allocator.py)
and [native CPU stepping](ci/ra_cpu.py); it does not replace allocator verification.
The different native-ABI memory setup and caller/kernel accounting are specific
to this cross-compiler experiment.

## Gates For Every Implementation Batch

1. Record the current commit, compiler build/settings and baseline measurements.
   Use a clean rebuild when changing host flags or trace definitions.
2. Add the smallest regression that exposes the intended source/TAC/assembly
   change. Test legal fallbacks and boundary cases as well as the fast path.
3. Run focused compiler fixtures, then the existing allocator verifier and
   generated/real-program execution corpus. Keep independent oracles, default/
   `-no-ra` equivalence and traced/quiet target-code equivalence.
4. Run the full eight-configuration competitor comparison. Report per-workload
   bytes, cycles, caller overhead and stack observations; preserve exact external
   controls and identify any intentionally accepted tradeoff.
5. At a checkpoint, rebuild both full calculation projects and report used ROM,
   not padded image size. Run the existing full suites and relevant native/
   sanitizer configurations before declaring a substantial new surface stable.
6. Recheck target-local clobbers, aliases, failure propagation, frame safety,
   immutable verification and unsupported conservative fallback.
7. Update this plan with actual results, not optimistic completion claims. Keep
   compiler source growth visible and remove superseded logic. Commit the batch
   locally with the validation summary; do not push.

Current implementation target: **finish step 1's compound logical lowering and
short-circuit qualification**, then begin the bounded loop-state work.
