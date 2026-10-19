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

Complete the shared improvements and the bounded follow-ups 2a-2n below before
the initial SM83 backend (step 3). This does not require every Z80-specific
optimization. The initial SM83 backend should still guide further shared-core
extraction.

- [x] **1. Direct branch-only conditions:** remove unnecessary Boolean
  temporaries through shared TAC lowering, preserving value-producing Booleans.
  - [x] Lower final relational comparisons, scalar tests and negation directly.
  - [x] Qualify signedness, promotions, side effects and all loop forms.
  - [x] Complete compound logical lowering with short-circuit evaluation tests.
  - [x] Record first-batch gains and pass the full checkpoint gates.
- [x] **2. Bounded loop-state improvements:** retain profitable counters,
  accumulators and CRC state using existing analysis and legality checks; keep
  register choices and costs target-specific. Stop before speculative expansion.
- [x] **2a. Byte sum/copy and induction addressing:** reduce repeated address
  reconstruction and loop overhead through existing pointer/array emission;
  qualify target-local byte-loop instruction choices.
- [x] **2b. Remaining frame traffic and accumulator retention:** remove proven
  redundant loads/stores in byte loops and CRC without general CFG promotion
  or weaker alias, clobber and fallback checks.
- [x] **2c. Leaf and caller overhead:** reduce redundant argument/result copies
  and frame setup within the existing ABI first; measure wrappers separately.
- [x] **2d. Stack footprint and pre-SM83 qualification:** reduce proven
  unnecessary frame slots, spills or saves, measure the complete frame footprint
  as well as SP high-water, and pass all checkpoint gates.
- [x] **2e. Constant addresses and direct loads (new step 1):** fold constant
  array offsets and reuse direct byte/word loads instead of rebuilding addresses.
- [x] **2f. Shared quotient/remainder computation (new step 2):** reuse both
  division results for matching `/` and `%`; simplify redundant pure expressions.
- [x] **2g. Masked-condition branch folding (new step 3):** avoid materializing
  single-use masked values when a direct bit/flag test suffices.
- [x] **2h. Further loop-state and address retention (new step 4):** retain
  eligible accumulators/counters and advance addresses without weaker safety checks.
- [x] **2i. Argument staging within the existing ABI (new step 5):** eliminate
  proven redundant caller-temporary copies before considering a new convention.
- [ ] **2j. Advancing-pointer copy/fill loops:** replace repeated address
  reconstruction, then select profitable target-local loop/block instructions.
- [ ] **2k. Constant byte arithmetic and multiplication setup:** specialize
  byte shifts/multiplies and remove unused setup from existing constant word chains.
- [ ] **2l. Proven bounded-counter narrowing:** use byte execution for counters
  whose ranges and observable uses permit it, without changing source semantics.
- [ ] **2m. Lowering-aware costs and local address reuse:** refine existing
  profitability estimates and address caching rather than adding a framework.
- [ ] **2n. Leaf-frame and helper-contract audit:** remove only measured,
  provably redundant frame work or preservation within the existing ABI.
- [ ] **3. Initial SM83 backend:** establish correct Game Boy code generation,
  target-specific addressing/spills/calls and execution tests. Use actual
  requirements to identify reusable code; do not assume IX/IY, `DJNZ` or `LDIR`.
  - [ ] Reuse shared loop proofs, but select SM83 auto-increment/decrement HL
    accesses and `LDH` only with their target-specific legality checks.
  - [ ] Qualify SM83 stack-relative addressing, spill/frame handling and calls
    without carrying over Z80 block-instruction or register assumptions.
- [ ] **4. Further target-specific improvements:** continue induction-address,
  instruction-selection, call and arithmetic work beyond the bounded follow-ups
  with both backends covered. These extensions are not prerequisites for initial
  Game Boy support. Keep the separate 6502 backend/policy boundary described below.

The numbered detail sections below describe optimization priorities, not a
requirement to finish every Z80 optimization before implementing SM83. The
explicit pre-SM83 scope is steps 1, 2 and 2a-2n; priorities 3-5 below supply
additional implementation guidance for the loop, call and arithmetic follow-ups.

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

**First-batch boundary:** compound logical lowering and its short-circuit
side-effect tests remained open. The completion batch below closes that work.

#### Compound Logical Completion (2026-10-19)

Step 1 is now complete. Compared with parent `f50ded7`, the existing shared
expression evaluator splits compound conditions before emitting their operands.
AND/OR and nested negation branch directly; calls, indexed reads, guarded
division and increments on an unselected right-hand side are not evaluated.
This intentionally corrects the previous eager evaluation of logical operands,
including logical expressions used as values. Required 0/1 values in arithmetic,
comparisons, array indices, arguments and returns remain materialized.

The old eager postfix logical emitter is removed. No production functions are
added, and the four changed compiler files contain 20 fewer lines overall.
The existing expression APIs carry branch orientation rather than duplicating
comparison emission. These changes use shared TAC only, not Z80 instructions
or assumptions about a future SM83 register set.

Qualification also exposed two coupled issues. Function-call argument lowering
must restore the enclosing expression's promotion: a word-returning call with
a final byte argument must not truncate a truth test. Compound joins also
exercise conditional-false spill edges. All seven core validation stages now
agree with the existing planner and target adapter: false/fallthrough stores
belong after the anchor, true/jump stores before it.
[The existing core regression](tests/z80/sms/allocator_ra_core_liveness_extraction/core_liveness_storage_test.c)
covers the complete conditional-false spill pipeline and rejects before-anchor
approval without invoking the target callback. Existing invalid-input,
immutable-verifier, fixed-prefix and rollback controls remain intact.

Changed RA measurements against `f50ded7`:

| Kernel | Code bytes before -> after | Kernel T-states before -> after | Max stack bytes before -> after |
| --- | ---: | ---: | ---: |
| PackBits decoder | 926 -> 626 | 218149 -> 206881 | 42 -> 39 |
| PackBits plus CRC | 1335 -> 1035 | 22097231 -> 22049036 | 54 -> 51 |

The other eleven RA kernels and every caller-wrapper metric are unchanged.
No metric regresses in either SameSameC mode against the immediate parent,
not just the older historical baseline. All 78 pinned competitor rows still
reproduce exactly; all 104 builds / 1856 calls / 24 independent timing checks
pass. The RA/stock-SDCC geometric code-size ratio is now 1.98 using its size
setting, and the cycle ratio is 2.03 using its speed setting. The historical
baseline, fingerprints and external tool settings are unchanged.

Full RA builds use 22977 bytes in
[calculations-1](tests/z80/sms/calculations-1), down from 23277, and 35589 bytes
in [calculations-2](tests/z80/sms/calculations-2), unchanged. Both padded ROMs
remain 65536 bytes. The existing compressed asset and decompression/CRC
workload are retained.

[The condition regression](ci/test_branch_conditions.py) now executes 61530
cases per host build: 43200 relational/Boolean boundary cases, 17640 compound
logical/value/side-effect cases and 690 control-flow cases. Coverage includes
all four condition/value forms, signed subconditions, high-byte truth values,
operator precedence, short-circuit call order, guarded reads/division,
for/while/do loops, continue/break and wrapping pre/post increments.
Default, all-spill and RA results match independent oracles, including complete
buffer contents; default/no-RA and traced/quiet ROM equivalence also pass.
Normal Release, traced Release and traced ASan builds pass all cases and emit
the same 70 ROMs byte for byte.

All 175 compiler fixtures pass without byte-snapshot changes. The full
64-seed allocator corpus and four-seed ASan corpus pass across all six
generated cohorts and 28 real routines, retaining all 32 immutable rejection
controls. The 17 comparison-tool tests pass normally and with `python -O`.
Changed C sources pass strict GCC ANSI/O3 checks; all three MSVC builds retain
the first batch's settings and warnings-as-errors gates.

At this checkpoint, the next target was bounded loop-state work, not SM83 yet.

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

#### Bounded Loop-State Checkpoint (2026-10-19)

Step 2 is complete within its bounded scope. Profiling showed that named word
counters stayed in their frames even when the existing loop allocator could
retain equivalent TAC temps. Byte fill repeatedly reloaded its counter for the
comparison, array index and increment; CRC loops repeatedly loaded and stored
their bit counters.

The existing scalar-promotion pass now exposes canonical unsigned word
counters: one initialization, one unit increment, a backward loop edge, stable
word width, and no earlier counter reads. Existing loop-definition discovery
filters non-loop-carried word live-ins before candidate selection. Shared path
planning stops reverse traversal at the loop header rather than extending an
inner counter's lifetime through its enclosing loop. Exit consumers still
reserve the paths they actually need.

Z80 legality and emission now support retained BC counters in comparisons and
indices. Operand reversal reuses existing comparison/materialization helpers.
Word equality/inequality against zero tests H/L directly; constant word bitwise
operations use immediate bytes instead of clobbering BC. Constant single-bit
shifts and retained-index array operations report their actual BC preservation.
Existing cost ranking, pair-overlap checks, reservations, transactions and the
immutable verifier remain authoritative. No production functions or new
allocation framework are added. Existing frame-cache invalidation remains
conservative across calls, assembly and joins; no incoming cache state is guessed.

Measured RA kernel results against parent `8559b1e`, using the unchanged pinned
comparison corpus:

| Kernel | Code bytes | Total kernel cycles | Maximum stack bytes |
| --- | ---: | ---: | ---: |
| Byte fill | 118 -> 81 | 419324 -> 262114 | 24 -> 24 |
| PackBits + CRC | 1035 -> 1004 | 22049036 -> 16272236 | 51 -> 49 |
| Tile/CRC workload | 1427 -> 1380 | 3183806 -> 2594520 | 128 -> 126 |

These are executed frame-region accesses summed over the same inputs, not
static spill-comment counts or buffer accesses:

| Kernel | Frame reads | Frame writes |
| --- | ---: | ---: |
| Byte fill | 13338 -> 7646 (-42.7%) | 3840 -> 1920 (-50.0%) |
| PackBits + CRC | 420869 -> 272901 (-35.2%) | 200872 -> 122536 (-39.0%) |
| Tile/CRC workload | 64770 -> 50014 (-22.8%) | 30014 -> 22202 (-26.0%) |

All 104 builds / 1856 calls / 24 independent timing checks pass. All 78
competitor rows reproduce exactly; neither SSC mode regresses in code, wrapper,
kernel cycles or stack against the immediate parent. Array, path and stencil
kernels also improve through BC-preserving emission. Byte sum/copy remain
unchanged. Fresh full calculation builds use 22929 bytes in calculations-1
(was 22977) and 35409 in calculations-2 (was 35589); padded ROMs remain 65536.

The existing allocator runner adds ten counter shapes covering 0/1/255/256
iterations, word wraparound, nesting, CRC updates, continue/break/early return,
calls, address escape and reused definitions. Independent values, complete
buffer checks and exact backedge counts are verified. Its positive word-counter
case requires at least 30% fewer executed frame reads and writes than no-RA.
The 64-seed six-cohort/real-project corpus and four-seed ASan corpus pass,
including all 32 immutable rejection controls. All 175 fixtures pass, including
new nested-path/exit-read controls; exact-byte expectations track the smaller
zero tests, immediate bitwise operations, cheaper BC indices and relocations.
All 61530 condition executions pass per Release/traced/ASan configuration, with
70 linked ROMs byte-identical across configurations. Strict GCC/MSVC builds and
the 17 comparison-tool tests, normally and under `python -O`, also pass.

This is not general CFG scalar promotion: ambiguous reassignment, signed/byte
multi-definitions, escaped values, unsupported consumers and pressure/call/asm
conflicts keep their conservative fallbacks. CRC bit counters are retained;
multi-definition CRC accumulators need not be. The measured follow-ups below
are now required before initial SM83 work; general CFG promotion, unrestricted
accumulator retention and cross-block frame-cache propagation are not.

#### Required Pre-SM83 Follow-ups

The fresh normal Release comparison at `cdacc98` passed all 104 builds, 1856
calls and 24 independent timing checks; all 78 competitor rows reproduced
exactly. Keep these starting anchors and the original `d1e7db1` baseline.
Measure each follow-up against its immediate parent too, so an earlier step's
gain cannot stand in for that step's own improvement.

**2a. Byte sum/copy and induction addressing**

- Starting point: sum/copy kernels are 139/148 bytes versus stock SDCC's size
  profile at 47/59 bytes. Their cycles are 2.98/2.96 times its speed profile.
- Reuse priority 3 below for advancing addresses, cheaper loop control and
  legal Z80 loop selection. Do not duplicate address emitters or assume Z80
  instructions exist on SM83.
- Acceptance: strictly improve both sum and copy kernel bytes and cycles, plus
  at least one real decoder/array workload. Preserve zero/one/255/256 counts,
  wraparound, overlap/source order, aliases, guards and generic fallbacks.

##### Step 2a Checkpoint (2026-10-19)

Complete within the measured scope. Reuse the existing constant normalization,
word arithmetic and comparison emitters; no production functions or allocation
framework are added.

- Retained word unit steps use `INC BC` / `DEC BC` directly instead of copying
  through HL.
- An in-place word increment in a local/spill frame slot increments its low
  byte directly and updates the high byte only on carry. Both operands must
  describe the same two-byte storage and have actual word types; byte
  widening, different destinations and other locations retain existing paths.
- Word address/index steps of plus/minus 256 increment/decrement H instead of
  materializing a second word and adding/subtracting it. Constant-left addition
  uses the existing operand swap before allocation.
- Unsigned word greater-or-equal exits use one carry test; zero already implies
  no borrow. Signed comparisons keep their separate overflow-aware path.

These are bounded selections for existing counter/address TAC, not a new
cross-iteration induction-pointer rewrite. Array access order, index widths,
aliases and overlap behavior are unchanged. No `DJNZ`, `LDIR`, byte-counter
narrowing or speculative pointer-range assumptions are introduced.

RA results against immediate parent `ea10114` (compiler code `cdacc98`), with
identical sources, inputs and pinned competitor settings:

| Kernel | Code bytes | Total kernel cycles | Cycle reduction |
| --- | ---: | ---: | ---: |
| Byte sum | 139 -> 131 | 435254 -> 381521 | 12.3% |
| Byte copy | 148 -> 137 | 599437 -> 529673 | 11.6% |
| Byte fill | 81 -> 77 | 262114 -> 247026 | 5.8% |
| PackBits decoder | 626 -> 582 | 206881 -> 171316 | 17.2% |
| PackBits + CRC | 1004 -> 948 | 16272236 -> 14931174 | 8.2% |
| Tile/CRC workload | 1380 -> 1283 | 2594520 -> 2400124 | 7.5% |

All 104 builds / 1856 calls / 24 independent timing checks pass. All 78
competitor rows reproduce exactly. Neither SSC mode regresses against the
immediate parent in kernel/wrapper bytes, kernel/caller cycles or observed
maximum SP displacement. Stack high-water is unchanged; further frame traffic,
accumulator retention and footprint work remain in 2b and 2d.

Fifteen new generated word-step cases cover carry/wrap boundaries, unsigned/
signed byte extension before increment, non-in-place and wide-frame fallbacks,
page steps in both operand orders, pointer steps and both directions of
overlapping forward copies with complete buffer checks. The original generator
limited this family to eight variants; 2b corrects that filter and qualifies all
fifteen in a separate single-bank cohort. The 64-seed six-cohort
allocator/28-real-routine corpus and four-seed ASan corpus pass, preserving all
32 immutable verifier rejection controls. All 175 compiler fixtures pass after
updating five byte snapshots and two frame-fallback emission assertions.
The 61530 condition executions pass per Release/traced/ASan configuration; all
140 intermediate/linked condition ROMs match across builds. Strict ANSI GCC
normal/traced checks, MSVC builds and the 17 comparison-tool tests normally and
under `python -O` pass.

Fresh full calculation ROMs use 22457 bytes (was 22929) and 34886 bytes (was
35409); padded images remain 65536 bytes. Normal Release and traced ROMs are
identical. The next implementation target is follow-up 2b, not SM83 yet.

**2b. Remaining frame traffic and accumulator retention**

- Post-2a starting point: 256-byte fill takes 66067 kernel T-states and 514 kernel
  stack writes versus stock SDCC's size profile at 29837 and four. SSC performs
  2054 kernel stack reads versus 776; counter retention alone is not enough.
- Profile remaining address, byte-value, accumulator and CRC reload/store
  chains. Reuse existing promotion, liveness, legality, location resolution and
  cache invalidation; retain only proven supported values.
- Acceptance: strictly reduce executed fill frame reads/writes and cycles,
  and frame traffic in a real CRC/decoder workload. Keep calls, joins, escapes,
  inline assembly, pressure fallback and immutable verifier rejection covered.

##### Step 2b Checkpoint (2026-10-19)

Complete within the measured scope. Reuse the existing array-base, index and
write-value materializers; no production functions or allocation rules are
added. Supported RA byte stores keep their destination in HL and use A for
the byte instead of moving the address through `PUSH HL` / `POP IY`. Existing
retained A sources remain supported, including signed-index preservation.
Frame/global pointer loads reuse the same folded-IX or IY fallback policy as
array reads. Byte stores from memory no longer fetch an unused high source
byte, in both RA and non-RA emission.

Word destinations, BC bases and conflicting physical value sources retain
their existing IY paths. Calls, joins, escapes, inline assembly and pressure
retain the existing legality, clobber and verifier contracts. No incoming
frame-cache propagation, speculative accumulator/CRC recurrence retention,
ABI change or general CFG promotion is introduced. The CRC gain below comes
from its real decoder's byte-store chain, not a claim of general CRC retention.

RA results against immediate parent `016ed97`, using identical inputs and
counting executed kernel stack/frame-region accesses across all calls:

| Kernel | Code bytes | Total kernel T-states | Stack reads | Stack writes | Max SP bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Byte fill | 77 -> 68 | 247026 -> 190446 | 7646 -> 4817 | 1920 -> 34 | 24 -> 22 |
| Byte copy | 137 -> 131 | 529673 -> 491010 | 17111 -> 15225 | 5727 -> 3841 | 27 -> 25 |
| PackBits decoder | 582 -> 570 | 171316 -> 160369 | 4990 -> 4456 | 1812 -> 1278 | 39 -> 37 |
| PackBits + CRC | 948 -> 936 | 14931174 -> 14752742 | 258111 -> 249407 | 107746 -> 99042 | 49 -> 47 |

The 256-byte fill call specifically drops from 66067 to 50707 kernel T-states,
2054 to 1286 executed stack reads and 514 to two stack writes. Sum is unchanged.
The smaller SP high-water removes temporary pointer-transfer pushes; it is not
a claim that below-SP frame storage or complete RAM usage has shrunk.

All 104 builds / 1856 calls / 24 independent timing checks pass; all 78 pinned
competitor rows reproduce exactly. Both SSC modes are non-regressing against
the immediate parent in kernel/wrapper bytes, kernel/caller cycles and observed
maximum SP displacement. Wrapper costs are unchanged and remain the 2c target.

Sixteen new generated byte-store cases cover constants, retained A and frame
values, word truncation, signed indices, zero indices, global pointers, wide
frames, branch joins, escaped values, calls, physical-value/word-store fallback,
overlap and ordered aliasing with complete buffer oracles. Separate word-step
and byte-store cohorts keep all-spill ROMs within the existing single-bank
harness limit; all fifteen intended word-step variants now actually execute.
The eight-cohort 64-seed allocator corpus, 28 real routines and four-seed ASan
corpus pass, preserving all 32 immutable verifier rejection controls.

All 175 compiler fixtures pass after updating fifteen byte snapshots and two
exact address-policy assertions. The 61530 condition executions pass per
Release/traced/ASan configuration, with all 140 intermediate/linked condition
ROMs identical across builds. Strict ANSI GCC normal/traced checks, MSVC builds
and the 17 comparison-tool tests normally and under `python -O` pass.

Fresh full calculation ROMs use 21185 bytes (was 22457) and 33937 bytes (was
34886). Padded images remain 65536 bytes and normal/traced images are identical.
The next implementation target is follow-up 2c, not SM83 yet.

**2c. Leaf and caller overhead**

- Starting point: the multiply/add native-call test wrapper remains 172 bytes
  and 971 T-states versus stock SDCC's 20 bytes and 114 T-states.
- Reuse priority 4 below. Remove redundant copies/setup within the current ABI
  before considering an explicit, gated convention change.
- Acceptance: strictly reduce that wrapper's bytes and cycles and demonstrate
  a whole-call win in a call-heavy workload. Report wrapper/kernel costs
  separately and preserve all existing caller/callee compatibility contracts.

##### Step 2c Checkpoint (2026-10-19)

Complete within the existing ABI. Reuse the current call transport, frame-prefix
descriptors, location resolution and IX=DE cache; no allocator rules, register
convention or frame layout change. The production change removes two superseded
helpers and is four lines smaller overall.

- Form the caller's new frame directly as SP-1, initialize IX from SP using the
  existing saved-frame offset, and hand the frame to the callee with `EX DE,HL`
  instead of two byte copies. Pure-assembly calls keep their direct-call path.
- Skip the unused high-byte read when truncating a memory word argument to a
  byte. Word transport and signed/unsigned widening retain their existing paths.
- Store global word call results directly from HL, after existing returned-byte
  extension and retained-result dispatch. Byte, local, far and retained result
  paths keep their existing contracts.
- Reuse IX for return-SP restoration only when the existing cache already
  proves IX=DE. Invalidate it after decrementing IX; otherwise retain the
  generic epilogue. No cache propagation across calls or joins is added.

RA results against immediate parent `e6e5c7c`, using identical inputs. T-states
are totals across all calls; the whole-call column includes kernel and wrapper:

| Routine (calls) | Kernel bytes | Wrapper bytes | Kernel T-states | Wrapper T-states | Whole-call T-states |
| --- | ---: | ---: | ---: | ---: | ---: |
| Multiply/add (17) | 71 -> 70 | 172 -> 161 | 20217 -> 20098 | 16507 -> 15589 | 36724 -> 35687 |
| Array workload (20) | 1806 -> 1790 | 158 -> 147 | 7784538 -> 7742998 | 17780 -> 16700 | 7802318 -> 7759698 |
| Path workload (20) | 1738 -> 1732 | 158 -> 147 | 7198647 -> 7198007 | 17780 -> 16700 | 7216427 -> 7214707 |
| PackBits + CRC (17) | 936 -> 927 | 96 -> 85 | 14752742 -> 14752198 | 8891 -> 7973 | 14761633 -> 14760171 |

The multiply/add wrapper specifically improves from 172 to 161 bytes and 971
to 917 T-states per call. Its kernel separately saves one byte and seven
T-states per call. The call-heavy array workload saves 42620 whole-call
T-states, about 0.55%; this is a bounded improvement, not competitor parity.
Stock SDCC's size-profile wrapper remains 20 bytes / 114 T-states.

All 104 builds / 1856 calls / 24 independent timing checks pass; all 78 pinned
competitor rows reproduce exactly. Both SSC modes are non-regressing against
the parent in kernel/wrapper bytes, kernel/caller cycles and maximum SP
displacement. SP high-water is unchanged throughout: these are instruction
and transport savings, not a frame-footprint improvement. Step 2d remains.

Eight new generated call cases cover word/global results, byte truncation,
signed and unsigned byte results widened into global words, nested calls, wide
frames, recursion and escaping pointers. Independent boundary-value oracles
check the complete buffer and adjacent globals. The full eight-cohort 64-seed
allocator corpus, 28 real routines and four-seed ASan corpus pass. Final
strengthened cases also pass focused 64-seed traced/ASan checks and complete
scalar-cohort rechecks, preserving all 32 immutable verifier rejection controls.

All 175 fixtures pass, including 35 rechecks after updating 83 exact-byte marker
ranges and one direct-global-store policy assertion. Old/new callers and
callees interoperate in both directions: 12 mixed-version builds / 228 guarded
calls pass with exact SP/DE restoration. The 61530 condition executions pass per
Release/traced/ASan configuration, with all 140 intermediate/linked condition
ROMs identical across builds. Strict ANSI GCC normal/traced checks, MSVC builds
and the 17 comparison-tool tests normally and under `python -O` pass.

Fresh full calculation ROMs use 20764 bytes (was 21185) and 33421 bytes (was
33937). Padded images remain 65536 bytes and normal/traced images are identical.
The next implementation target is follow-up 2d, not SM83 yet.

**2d. Stack footprint and pre-SM83 qualification**

- Post-2c starting point: observed SP high-water is 22 bytes for fill (stock
  SDCC: eight), 47 for PackBits/CRC and 126 for tile/CRC. This is not total RAM use:
  SSC also accesses frame storage below SP.
- Remove only proven unnecessary frame slots, spills or saves through existing
  frame layout and lifetime handling. Record below-SP frame extent as well as
  SP high-water; moving storage below SP is not a footprint improvement.
- Acceptance: demonstrate a smaller verified frame/stack footprint in at least
  one representative workload, with nested-call, frame-safety and pressure
  controls intact. Pass all implementation-batch gates and rebuild both full
  calculation ROMs before starting step 3; mark 2a-2d complete with actual
  measurements, not parity claims.

##### Step 2d Checkpoint (2026-10-19)

Complete within the bounded footprint goal. Existing loop/interval retention
can leave a spill slot reserved even when every operand already uses the same
physical register. The existing spill-layout pass now omits that unused backing
only after scanning all active uses in the function. Missing or inconsistent
physical annotations, call arguments, explicit spills, reload/store transfers
and inline assembly retain memory backing. Fixed spill owners and the entire
prefix through their lowest destination remain unchanged; only the suffix can
shrink. Default and forced-all-spill layouts are unchanged.

No new retention, clobber or slot-sharing policy is introduced. The existing
poison-offset path prevents an omitted slot from silently becoming a memory
fallback. No production helper is added; the coverage proof adds 36 lines to
the existing layout function.

The comparison runner now measures the deepest frame access below entry SP,
the largest access below the instruction's SP, and the full observed call span.
The full span includes reserved stack space, below-SP frame accesses and the
two-byte entry return address. Ordinary PUSH/POP accesses are not misclassified
as below-SP storage when emulator callbacks precede the SP update.

RA results against immediate parent `cddbdf1`, in bytes across identical calls:

| Routine | SP high-water | Deepest access below entry SP | Full call span |
| --- | ---: | ---: | ---: |
| Byte fill | 22 -> 20 | 20 -> 20 | 24 -> 22 |
| Shift/restore | 22 -> 20 | 20 -> 20 | 24 -> 22 |
| Partition/restore | 28 -> 26 | 28 -> 26 | 30 -> 28 |
| Stencil workload | 120 -> 118 | 120 -> 118 | 122 -> 120 |
| Path workload | 187 -> 185 | 187 -> 185 | 189 -> 187 |
| PackBits + CRC | 47 -> 45 | 47 -> 45 | 49 -> 47 |
| Tile + CRC | 126 -> 126 | 126 -> 126 | 128 -> 128 |

The below-SP access extent does not increase: fill remains eight bytes and
PackBits/CRC ten. These are actual two-byte footprint reductions, not storage
moved below SP. Kernel/wrapper code bytes and T-states are unchanged throughout
both SSC modes. The stock SDCC size-profile fill still needs only eight bytes
of SP displacement / ten bytes of full call span; parity is not claimed.

All 104 builds / 1856 calls / 24 independent timing checks pass, with all 78
pinned competitor controls exact. Final footprint accounting re-executes all
104 images and all 26 immediate-parent SSC images, reproducing their original
five metrics exactly. All 26 SSC rows are non-regressing against the parent in
those metrics and the three new footprint observations. The historical
baseline is unchanged.

Fourteen native backing-slot cases cover all five retained registers,
incomplete/mixed coverage, real call arguments, explicit memory transfers and
disabled/all-spill modes. Existing fixed-owner controls also pin an otherwise
unused retained slot in the protected prefix. All 32 immutable verifier
rejections remain intact. The eight-cohort 64-seed allocator corpus, 28 real
routines and four-seed ASan corpus pass, including nested calls, recursion,
escapes, wide frames, aliasing and pressure fallback.

All 175 fixtures pass, including seven rechecks after updating four byte-marker
ranges and three exact-policy fixtures. All 174 available fixture assemblies
differ only in qualified frame offsets. The 61530 condition executions pass per
Release/traced/ASan configuration; all 140 intermediate/linked condition ROMs
are identical across builds. Strict ANSI GCC normal/traced checks, MSVC builds
and all 18 comparison-tool tests normally and under `python -O` pass. Measurement
probes separately cover below-SP storage, ordinary calls and untouched reserved
stack space.

Both full calculation projects rebuild successfully. Used ROM remains 20764
and 33421 bytes; padded images remain 65536 bytes and normal/traced images are
identical. Steps 1, 2 and 2a-2d are qualified. Further frame optimization remains
optional, not a claim that these finite tests establish every program's maximum
RAM use.

### Measured Follow-ups After Step 2d

The post-2d profile at `dc828c8` passes 104 builds, 1856 calls and 24 independent
timing checks, with all 78 pinned competitor controls unchanged. Against the
previous profile at `cdacc98`, geometric mean kernel size falls 6.3% and kernel
T-states 9.1%. Current size/cycle ratios are 1.788/1.714 against stock SDCC,
1.508/1.605 against z88dk/SDCC and 1.120/1.010 against sccz80. Size uses competitor
size settings; cycles use speed settings. These are corpus results, not general
C or whole-game parity.

The next five steps are 2e-2i above, in this order. Reuse existing TAC passes,
location materializers, transport and emitters; avoid another allocator framework.
Keep semantic proofs CPU-neutral and instruction selection target-specific.
Each step must pass the implementation-batch gates below before being checked.

#### 2e. Constant Addresses And Direct Loads

- Fold constant array indices into known object addresses with the correct
  element stride and target address arithmetic. Distinguish an array object
  from a pointer variable whose stored address must still be loaded.
- Reuse existing load/result transport for direct word and byte reads, preserving
  byte signedness, widening/truncation and physical-register contracts.
- Keep dynamic indices, pointer bases, port operations and unsupported address
  forms on their existing safe paths. Cover zero/nonzero indices, signed offsets,
  byte/word arrays, aliases, retained operands and wide frames.
- Measure wrappers separately: the multiply/add caller still builds the address
  of `bench_params[1]` using two index additions instead of a direct word load.

Acceptance: exact assembly controls and independent value/buffer oracles prove
the new selections and conservative fallbacks. Reduce constant-address setup
and the 161-byte/917-T-state caller without changing the ABI or regressing any
reference workload. Record compiler source growth and remove superseded logic.

##### Step 2e Checkpoint (2026-10-19)

Complete within the constant array-object read scope. The existing array-read
emitter folds byte/word constant indices into global or local object addresses,
with 16-bit address wrap. Global word reads use `LD HL,(address)`; global byte
reads reuse the existing HL address materializer and `LD L,(HL)`, without adding
an A clobber. Local reads reuse the existing IY materializer at the folded frame
offset. Existing signed/unsigned extension and result transport remain shared.
No TAC, ABI, allocator legality or arithmetic policy changes are required.

Pointer variables still load their stored addresses. Retained bases, dynamic
indices and port operations retain their existing paths; array writes are not
changed. No production helper is added: the existing emitter grows by 37 net
lines. Five new cases extend the existing index-copy family rather than adding
another harness or execution cohort.

RA wrapper measurements against immediate parent `dc828c8`:

| Caller | Wrapper bytes | Wrapper T-states per call |
| --- | ---: | ---: |
| Multiply/add | 161 -> 126 | 917 -> 729 |
| Product, shift and partition/restore | 161 -> 126 | 917 -> 729 |
| Array, stencil, tile and path workloads | 147 -> 126 | 835 -> 723 |
| Byte sum, copy and fill | 147 -> 126 | 835 -> 723 |
| PackBits decoder | 179 -> 151 | 1020 -> 868 |
| PackBits + CRC | 85 -> 85 | 469 -> 469 |

Kernel bytes/T-states and all stack/frame observations are unchanged in both
SSC modes. All 26 SSC rows are non-regressing in all eight measured fields.
The full 104-build/1856-call comparison and 24 independent timing checks pass;
all 78 pinned competitor controls remain exact. Kernel-only competitor ratios
therefore remain at the post-2d values above; these gains are caller overhead.
All 26 SSC benchmark assemblies and ROMs are identical between normal and traced
builds, with identical measurements.

The focused 64-seed index-copy corpus passes value/full-buffer oracles in
default, all-spill and RA modes, and passes against the pre-optimization compiler
at `cddbdf1` as a separate baseline control. New cases cover global word indices
0/1/63, signed byte indices 0/1/127 with explicit widening, word-to-byte
truncation, and byte/word local arrays spanning a wide frame. Existing pointer,
alias, signed-index and port controls remain intact. Raw assembler probes pin
immediate versus indirect addressing and positive/negative 16-bit address wrap.

All 175 fixtures pass, including three rechecks after updating ten byte-marker
ranges and one exact-policy assertion. Before updating snapshots, all 174
available fixture assemblies are proved identical outside the qualified
constant-read replacements; unchanged instruction sequences and frame layouts
are not normalized away. The new policy assertions pin both byte and word
folded frame offsets.

The full eight-cohort 64-seed allocator corpus, 28 real routines, four-seed ASan
corpus and all 32 immutable verifier rejection controls pass. All 50 traced/ASan
allocator and real-project ROMs match. The existing condition corpus and 18
comparison-tool tests pass under normal/traced/ASan builds; all 140 condition
ROMs match, and the comparison tests also pass under `python -O`. MSVC builds,
Pylance syntax validation and strict ANSI GCC normal/traced checks pass.

Both complete calculation projects rebuild. Used ROM is 20764 bytes (unchanged)
and 33393 bytes (28 fewer than 33421); padded images remain 65536 bytes and
normal/traced images match. No whole-program competitor parity is claimed.

#### 2f. Shared Quotient/Remainder Computation

- Reuse the existing divider's quotient and remainder for matching operand pairs
  when no intervening mutation, alias, clobber or observable effect invalidates it.
- Start with the duplicate divisions in partition/restore; preserve the compiler's
  defined zero-divisor behavior and all result lifetimes/widths.
- Simplify redundant pure arithmetic, including consecutive XORs with the same
  unchanged mask, through existing TAC optimization rather than new runtime helpers.

Acceptance: independently check both results, zero and word boundaries, changed
operands and fallback cases; measure standalone and shared-dependency bytes/cycles.
Do not replace division identities without proving their exceptional cases.

##### Step 2f Implementation Checkpoint

The existing Z80 arithmetic dispatcher now reuses the remainder from a preceding
division of the same local scalar/argument operands. Operand identities, actual
types and promotions must match, and the quotient must be stored in a local or
spill slot without overwriting either input. Only dead TACs and non-emitting
scalar declarations may intervene. Array initializers, calls, writes, labels and
other operations remain barriers. Global/temporary operands, physical quotient
destinations and reverse-order pairs conservatively use the original path.

Both byte and word emitters reuse their existing result transport, including
truncation and sign/zero extension. The divider itself and its zero-divisor
behavior are unchanged. There is no new runtime helper, DIVMOD TAC or speculative
division-identity replacement. Superseded commented copies of both divider
algorithms were removed instead of duplicating their result-storage code.

The existing adjacent-expression TAC optimizer also cancels matching XOR pairs
when promotions agree and the intermediate has no other observable use. It
handles single-use temporaries and unescaped local scalars without explicit calls,
checking later uses and rejecting relevant control-flow/assembly barriers.
Changed masks, narrowed intermediates and retained masked values keep their
existing behavior. Net production growth across the two compiler files is
**43 lines, with no new functions**.

The full pinned comparison passes: **104 builds, 1856 calls and 24 independent
timing checks**, with all **78 external controls exact**. Against immediate
parent `6546a16`, the RA-mode results are:

| Workload | Kernel bytes before / after | Kernel T-states before / after | Calls |
| --- | ---: | ---: | ---: |
| Product/restore | 196 / 147 | 61998 / 42415 | 17 |
| Partition/restore | 192 / 119 | 61426 / 39321 | 17 |
| Array workload | 1790 / 1668 | 7742998 / 6403660 | 20 |
| Stencil workload | 1813 / 1740 | 4641994 / 4405850 | 20 |
| Path workload | 1732 / 1659 | 7198007 / 7172413 | 20 |

Kernel bytes include required dependencies, so the workload rows also measure
the benefit of shared calculation routines. Both RA and non-RA improve in these
five workloads; the other eight workloads are unchanged. Geometric RA kernel
size falls **6.8%**, and kernel T-states **7.9%**. All 26 SSC rows are
non-regressing in all eight measured fields; wrappers and every stack/frame
footprint field are unchanged. The immutable original baseline is retained.
Normal/traced assembly, ROMs and metrics match for all 26 SSC benchmark rows.

The existing execution runner gains one dedicated divmod cohort, not another
harness. Its 38 new variants separately check quotient snapshots and remainder
returns, byte/word and signed-extension boundaries, runtime zero divisors,
changed operands, overwritten inputs, reverse/repeated pairs, calls, aliases,
bulk initialization and XOR cancellation/fallbacks. The 64-seed focused run
checks 2944 calls per executed mode. The same independent value/full-buffer
oracles pass against the saved parent compiler, while the new assembly assertion
rejects that parent as intended. All five normal/traced focused ROMs match.

All **175 fixtures pass without snapshot updates**. The full nine-cohort
64-seed allocator corpus, 28 real routines, four-seed ASan corpus and all 32
immutable verifier rejection controls pass; all 55 traced/ASan allocator and
real-project ROMs match. The condition corpus and 18 comparison-tool tests pass
in normal/traced/ASan builds, all 140 condition ROMs match, and the comparison
tests also pass under `python -O`. MSVC builds, strict ANSI GCC normal/traced
checks for both changed compiler files and Pylance syntax validation pass.

Both complete calculation projects rebuild with identical normal/traced ROMs.
Used ROM falls **20764 -> 20691** and **33393 -> 33207** bytes; padded images
remain 65536 bytes. Evidence is retained privately under
`C:\cygwin64\tmp\shared-divmod-20261019`, including the full comparison and
aggregate qualification record. No whole-program competitor parity is claimed.

#### 2g. Masked-Condition Branch Folding

- Fold single-use mask/zero-test conditions into direct bit or flag tests.
  The CRC high-bit condition currently materializes an entire masked word first.
- Preserve value-producing masks, other live uses, widths, side effects and
  short-circuit behavior. Keep target instruction legality in the backend.

Acceptance: qualify taken/not-taken branches, byte/word masks and boundary bits,
plus shared/value-use fallbacks. Reduce CRC condition work and rerun the existing
condition execution corpus; do not infer correctness from a smaller listing.

##### Step 2g Implementation Checkpoint

The existing Z80 ALU dispatcher now folds a constant AND followed by a zero
equality/inequality branch when verified temporary metadata proves exactly one
definition and use. The nonzero mask must fit entirely in one byte of the actual
source, promoted source and result. Single-bit masks use `BIT` directly on the
existing physical register byte or indexed frame byte; other one-byte masks
reuse accumulator materialization and the existing immediate AND flags.

Location/address resolution, branch emission and instruction attribution reuse
existing machinery. Only dead TACs may intervene; the emitter consumes the
adjacent branch without changing verified TAC. Global sources, shared/coalesced
temporaries, raw masked values, dynamic/multi-byte masks, nonzero comparisons,
extension/truncation hazards and intervening operations retain the original
path. Calls and short-circuit effects are not skipped. All instruction selection
stays Z80-local. Net production growth is **67 lines in one existing function's
path, with no new functions, runtime helpers or TAC opcodes**.

The full pinned comparison passes: **104 builds, 1856 calls and 24 independent
timing checks**, with all **78 external controls exact**. Against immediate
parent `5474336`, these four RA-mode workloads improve:

| Workload | Kernel bytes before / after | Kernel T-states before / after | Calls |
| --- | ---: | ---: | ---: |
| Packed CRC | 927 / 915 | 14752198 / 12802502 | 17 |
| Tile workload | 1264 / 1232 | 2375502 / 2165246 | 20 |
| Array workload | 1668 / 1656 | 6403660 / 6372748 | 20 |
| Path workload | 1659 / 1648 | 7172413 / 7139013 | 20 |

The CRC high-bit test now uses `BIT 7,(IX-10)` and the original zero branch,
without constructing a masked word. CRC kernel T-states fall **13.2%**, and
tile T-states **8.9%**. Across all 13 RA workloads, geometric kernel size falls
**0.4%** and kernel T-states **1.9%**. The other nine RA rows and all 13 non-RA
rows are unchanged. All 26 SSC rows are non-regressing in all eight fields;
wrappers and every stack/frame footprint field are unchanged. Normal/traced
benchmark assembly, ROMs and metrics match. The immutable baseline is retained.

The existing condition runner now covers **772 typed mask/form combinations**
and **64848 additional value/branch/width/side-effect executions per build**.
These include byte/word and signed boundaries, both operand orders, taken and
not-taken branches, raw/shared values, narrowing, source mutation, dynamic masks,
physical sum results, wide frames and effectful calls/short-circuit evaluation.
The same generated sources and independent value/full-buffer oracles pass
against the saved parent compiler; only the new instruction-selection test
rejects that parent as intended. All 25 condition/comparison tests pass in
normal/traced/ASan builds. Their 150 program ROMs and 150 independently assembled
counter images match across all three builds. The 18 comparison-tool tests also
pass under `python -O`.

All **175 fixtures qualify**, including one intentional low-byte mask snapshot
and trace-assertion update: `LD A,(IX-6); AND A,1; SUB A,0` becomes
`BIT 0,(IX-6)`, reducing that marked sequence from 24 to 21 bytes. The unchanged
parent snapshots were checked first; the updated fixture passes on recheck.
The full nine-cohort 64-seed allocator corpus, 28 real routines, four-seed ASan
corpus and all 32 immutable verifier rejection controls pass. All 55 traced/ASan
allocator and real-project ROMs match. MSVC builds, strict ANSI GCC normal/traced
checks and Pylance syntax validation of the condition tests pass.

Both complete calculation projects rebuild with identical normal/traced ROMs.
Used ROM falls **20691 -> 20679** and **33207 -> 33134** bytes; padded images
remain 65536 bytes. Evidence is retained privately under
`C:\cygwin64\tmp\masked-branches-20261020`, including the full comparison and
aggregate qualification record. No whole-program competitor parity is claimed.

#### 2h. Further Loop-State And Address Retention

- Reuse bounded loop analysis to retain eligible counters/accumulators and replace
  repeated base-plus-index reconstruction with advancing addresses.
- Prioritize sum/copy and CRC. A 256-byte copy still performs 4105 kernel stack
  reads and 1029 writes, versus stock SDCC speed mode's 1548 reads and eight writes.
- Preserve alias, escape, clobber, overflow, boundary and fallback contracts.
  Consider block instructions only after proving zero-count and overlap semantics;
  do not generalize Z80 register/instruction assumptions into the shared core.

Acceptance: reduce measured frame traffic and kernel cycles in byte loops and
at least one realistic workload. Check full buffers, wraps, overlap direction,
calls/pressure fallbacks and complete frame footprint, not just SP displacement.

#### 2i. Argument Staging Within The Existing ABI

- Coalesce single-use argument staging only when source evaluation order,
  intervening effects, aliases, widths and final argument slots are proved safe.
- Avoid loading a value into a caller-frame temporary only to copy it into the
  callee frame immediately afterwards. Reuse existing call transport descriptors.
- Preserve banked, cross-file, recursive, nested, indirect and assembly-facing
  contracts. Any register-argument ABI remains a separate explicit, gated change.

Acceptance: reduce caller bytes/cycles without altering the convention; cover
mixed-width arguments, side-effect/alias fallbacks, nested calls and exact SP/DE
restoration. Keep kernel and wrapper measurements separate.

Checkpoint (2h/2i combined): reuse the existing local optimizer, allocation
metadata, target-transparency predicate and emitters. The production change is
**174 net C lines, with no new production functions**, against parent `678a93e`.
No new IR operation, register-argument convention or shared-core Z80 assumption
is introduced.

For 2h, retain the bounded unsigned word index in BC across an in-place word
accumulation when its single-use source is proved to be an unsigned byte array
read. Add the low byte and increment the accumulator's high byte only on carry;
keep the backing accumulator current, including when its address escapes.
The existing target predicate and emitter use the same proof. Signed-byte and
word-array sources keep the old full-width path; calls and pressure retain
their existing allocation fallbacks.

Also reuse the array reader's byte in L for an immediately following local-byte
comparison against a constant. Keep the original backing store for later reads
and aliases. Skip only dead TACs: labels, calls and other instructions are
barriers. Word comparisons retain their old path. This removes a real decoder
frame reload without adding a register cache, CFG promotion or speculative
incoming-path state.

Immediate-parent RA kernel measurements from the unchanged comparison corpus:

| Workload | Kernel bytes | Kernel T-states | Kernel stack reads | Kernel stack writes |
| --- | ---: | ---: | ---: | ---: |
| Sum, 17 calls | 130 -> 102 | 381402 -> 240653 | 10544 -> 5320 | 2932 -> 1480 |
| PackBits decoder, 16 calls | 567 -> 565 | 160257 -> 159912 | 4456 -> 4433 | 1278 -> 1278 |
| Packed CRC, 17 calls | 915 -> 913 | 12802502 -> 12801227 | 214591 -> 214506 | 99042 -> 99042 |

For a single 256-byte sum, kernel T-states fall **101862 -> 63872**, stack
reads **2827 -> 1415** and writes **775 -> 387**. Maximum SP/access extent
falls **24 -> 22** bytes and complete frame footprint **26 -> 24** bytes.
The realistic 64-call CRC run separately falls **48197696 -> 48192896**
T-states and **807744 -> 807424** frame reads. These decoder/CRC gains are
small, not competitor parity. Copy/fill and all other comparison rows remain
unchanged; general advancing-address and block-instruction selection remains
further target-specific work, not an unqualified rewrite in this checkpoint.

For 2i, extend the existing single-use copy optimizer rather than creating a
second call transport path. Coalesce staging only with one write/read,
conversion identity and a stable constant, immutable temporary or proven
unescaped caller local. Named word constants additionally require one reaching
call use, no escape/control/assembly ambiguity and no later read before
redefinition or end. Preserve ordinary non-call/switch substitutions and all
existing banked, cross-file, recursive and assembly-facing conventions.

Focused RA caller sections and total T-states over 64 seeds:

| Scenario | Caller bytes | T-states |
| --- | ---: | ---: |
| Local word across nested call | 180 -> 156 | 88704 -> 80128 |
| Constant across multiple nested calls | 267 -> 231 | 133376 -> 119936 |
| Mixed-width transport | 295 -> 259 | 142208 -> 129344 |
| Recursive nested calls | 206 -> 188 | 215840 -> 200720 |
| Large-frame pressure/named word constant | 590 -> 548 | 275840 -> 261120 |

Escaped-local writes, direct argument updates and mutable-global snapshots are
three exact byte/cycle/traffic fallbacks. The conservative-mode ROMs remain
identical to the parent. A named-constant guard was tightened after detecting
four extra frame reads/writes in `calculateAndPrint5`; final qualification has
no such regression. With decoder forwarding, that real routine improves
**1803691 -> 1803541** T-states and **31984 -> 31974** frame reads.

Qualification: all **175 fixtures pass without snapshot changes**. The full
**104-build / 1856-call** pinned comparison and 24 independent timing checks
pass; all **26 SSC rows are non-regressing in all eight recorded metrics** and
all 78 external controls remain exact. Wrappers are unchanged. Normal/traced
assembly and ROMs match for all 26 SSC builds.

The existing execution harness covers 17 loop/read-forwarding variants and
eight staging variants, with independent values/full-buffer oracles, boundary
bytes, zero/one/127/128 counts, word wraps, forward overlap, signed/word-source
fallbacks, wide frames, aliases, calls and label barriers. Saved-parent oracles
pass and the parent rejects each new selection assertion. The full 11-cohort
64-seed allocator corpus, all 28 real routines, four-seed ASan corpus and all
32 immutable verifier rejection controls pass. All 65 traced/ASan program ROMs
match; no real routine regresses in cycles, frame reads or writes.

MSVC builds and strict ANSI GCC normal/traced checks pass for both changed C
files; Pylance reports no harness syntax errors. Condition/comparison suites
pass all 25 tests in release, traced and ASan configurations, with 300 identical
program/counter images; all 18 comparison tests also pass under `python -O`.
Both complete calculation projects rebuild with identical normal/traced ROMs:
used ROM **20679 -> 20677** and **33134 -> 33134** bytes, still padded to 65536.
Evidence is retained privately under
`C:\cygwin64\tmp\loop-call-qualified-20261020`.

### Source-Guided Follow-ups After Step 2i (2026-10-20)

Complete these five bounded steps in order before initial SM83 work. The source
study examined [stock SDCC r17016](https://svn.code.sf.net/p/sdcc/code/!svn/bc/17016/trunk/sdcc/src/z80/gen.c),
its allocator and SM83 rules, and sccz80 plus the SDCC patches shipped with
[z88dk 2.4](https://github.com/z88dk/z88dk/releases/tag/v2.4). The shipped patch
targets SDCC r15248, not the measured z88dk/SDCC binary's r15242; treat it as
algorithmic guidance, not exact binary provenance.

Reuse existing TAC/loop analysis, target costs, location resolution and emitters.
Do not replace the allocator, import a large peephole rule engine or duplicate
completed constant word shifts/multiplies, quotient/remainder reuse and masked
branches. These are prospective improvements, not measured gains or completed
implementation claims. Existing checkpoint gates apply to every code change.

#### 2j. Advancing-Pointer Copy/Fill Loops

- [ ] Recognize bounded simple loops using existing analysis; maintain advancing
  addresses instead of reconstructing `base + index` on every iteration.
- [ ] Compare scalar pointer loops, tiny straight-line sequences, `DJNZ`,
  unrolled `LDI` and `LDIR` using full setup/preservation costs. Keep a generic
  word-count fallback; competitor builtin lowering does not prove arbitrary
  source loops are recognized or eligible.
- [ ] Preserve DE's frame-pointer contract, source-order overlap behavior,
  zero/256 counts, address wrap, clobbers and observable memory effects.
  Seed-plus-`LDIR` fills introduce reads and must not replace store-only loops
  where those reads are observable. Keep instruction selection target-local.

Acceptance: reduce byte-copy/fill bytes and cycles without regression, and
measure a real decoder/array workload. Check complete buffers and guards,
zero/one/255/256 counts, word wraps, overlapping source/destination in both
directions, calls, aliases, register pressure and complete frame footprint.

#### 2k. Constant Byte Arithmetic And Multiplication Setup

- [ ] Specialize constant byte shifts in the existing emitter; compare unrolled
  shifts with accumulator rotate-and-mask sequences for unsigned values,
  including transfers, result width and flag/register clobbers.
- [ ] Add only profitable constant byte multiplication sequences within the
  existing arithmetic path. Cost additions and subtractions for the actual
  operand width; do not assume byte signed-digit choices also suit word results.
- [ ] Select existing constant word multiply chains before unnecessary generic
  operand setup, DE saves/loads and restoration. Preserve the established word
  shift/multiply paths, budgets and conservative fallbacks.

Acceptance: exhaustively check byte operands for selected constants and shift
counts, plus signed/promoted results, word boundaries, aliases and pressure.
Show exact instruction-selection changes and measured bytes/cycles; preserve
the compiler's existing shift semantics and default size/speed contract.

#### 2l. Proven Bounded-Counter Narrowing

- [ ] Reuse bounded-loop analysis to prove initialization, bounds and updates
  before selecting byte execution for a source word counter.
- [ ] Check all counter uses, values observed after exit, conversions, escapes
  and wrap behavior. Reject unknown ranges or interfering updates; do not infer
  safety from array undefined-behavior assumptions borrowed from another compiler.
- [ ] Start with the CRC's proven 0-7 bit counter, which SDCC executes in one
  byte while SameSameC retains BC. Measure register availability and compare/
  increment costs before considering a separate countdown/`DJNZ` selection.

Acceptance: reduce a measured hot-loop cost without changing language-level
types or observable values. Cover zero-trip loops, nested loops, early exits,
post-loop reads, 255/256 boundaries, wrapping/escaped counters and unchanged
fallbacks. Keep any byte-counter instruction choice target-specific.

#### 2m. Lowering-Aware Costs And Local Address Reuse

- [ ] Extend current profitability hooks to account for actual address setup,
  transfers, width extension and saves/restores where estimates currently omit
  them. Keep frequency and size/cycle policy explicit.
- [ ] Reuse existing address helpers/cache for proven nearby addresses within
  basic blocks rather than introducing parallel state or a new optimizer.
- [ ] Invalidate facts on relevant register writes, calls, assembly and control
  boundaries. Do not retain cached addresses across joins without proving every
  incoming path establishes the same value.

Acceptance: correlate changed decisions with emitted sequences and measured
bytes/cycles/frame traffic. Exercise cache hits and invalidation, wide offsets,
pressure and unknown-cost fallbacks. Preserve immutable verification and
normal/traced equivalence; do not add a dry-run framework without measured need.

#### 2n. Leaf-Frame And Helper-Contract Audit

- [ ] Measure leaf prologue/epilogue and call-heavy costs after the preceding
  improvements; identify redundant work in existing frame/call lowering.
- [ ] Audit existing helper input, result and preserved-register metadata.
  Tighten only demonstrably over-conservative clobbers; qualify actual helper
  effects before removing saves or retaining values across them.
- [ ] Keep the existing DE/frame and caller/callee ABI contracts. Frame-pointer
  omission or register arguments require a separate explicit, gated proposal,
  not a silent convention change to force an optimization.

Acceptance: record measured safe removals, or explicitly close the audit with
no profitable supported case. For changes, verify mixed widths, nested,
recursive, banked, cross-file and assembly-facing calls, exact SP/DE restoration
and complete frame footprint. Report caller and kernel costs separately.

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
- `max_stack_bytes` retains its original meaning: displacement below entry SP.
  `max_stack_access_bytes` measures the deepest stack/frame access below entry
  SP, and `max_below_sp_bytes` records accesses below the instruction's stack
  pointer. `max_frame_footprint_bytes` combines reserved SP span and frame
  accesses, including the two-byte entry return address. This is observed
  whole-call storage, not global RAM, interrupt headroom or an unexecuted-path
  bound. These additional observations do not regenerate the historical
  five-metric baseline; compare them against a consistently remeasured parent.
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

Steps 1, 2 and 2a-2i are complete. Current implementation target:
**step 2j, advancing-pointer copy/fill loops**, followed by 2k-2n and then
**step 3, initial SM83 backend**. Keep the qualified Z80 behavior and shared core
covered throughout; do not expand this finite sequence into every possible Z80 optimization.
