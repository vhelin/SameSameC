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

Complete the shared improvements and the bounded follow-ups 2a-2s below before
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
- [x] **2j. Advancing-pointer copy/fill loops:** replace repeated address
  reconstruction, then select profitable target-local loop/block instructions.
- [x] **2k. Constant byte arithmetic and multiplication setup:** specialize
  byte shifts/multiplies and remove unused setup from existing constant word chains.
- [x] **2l. Proven bounded-counter narrowing:** use byte execution for counters
  whose ranges and observable uses permit it, without changing source semantics.
- [x] **2m. Lowering-aware costs and local address reuse:** refine existing
  profitability estimates and address caching rather than adding a framework.
- [x] **2n. Leaf-frame and helper-contract audit:** remove only measured,
  provably redundant frame work or preservation within the existing ABI.
- [x] **2o. Arithmetic-produced locals and existing pure-expression folds:**
  extend safe local promotion and make existing simplifications effective.
- [x] **2p. Advancing-pointer reductions and scaled induction:** extend proven
  loop/address retention beyond copy/fill to sums and complex calculation loops.
- [x] **2q. Variable-count word shifts:** use size-neutral bounded shift loops
  for proven small counts, preserving the original full-width fallback.
- [x] **2r. Operand-aware word comparisons and branch selection:** avoid
  unnecessary materialization and choose branches by legality and actual cost.
- [x] **2s. Shared arithmetic bodies with an explicit size/speed policy:**
  price bank-local helper sharing against inline code without hidden tradeoffs.
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
explicit pre-SM83 scope is steps 1, 2 and 2a-2s; priorities 3-5 below supply
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

- [x] Recognize bounded simple loops using existing analysis; maintain advancing
  addresses instead of reconstructing `base + index` on every iteration.
- [x] Compare scalar pointer loops, tiny straight-line sequences, `DJNZ`,
  unrolled `LDI` and `LDIR` using full setup/preservation costs. Keep a generic
  word-count fallback; competitor builtin lowering does not prove arbitrary
  source loops are recognized or eligible.
- [x] Preserve DE's frame-pointer contract, source-order overlap behavior,
  zero/256 counts, address wrap, clobbers and observable memory effects.
  Seed-plus-`LDIR` fills introduce reads and must not replace store-only loops
  where those reads are observable. Keep instruction selection target-local.

Acceptance: reduce byte-copy/fill bytes and cycles without regression, and
measure a real decoder/array workload. Check complete buffers and guards,
zero/one/255/256 counts, word wraps, overlapping source/destination in both
directions, calls, aliases, register pressure and complete frame footprint.

Step 2j checkpoint (2026-10-20):

- Added one bounded Z80-emission helper using existing verified TAC, allocator
  intervals, location/address resolution and assignment emitters. It recognizes
  canonical zero-based, unit-stride unsigned word-counter loops with a stable
  word bound or positive constant, one byte store and at most one byte read.
  Eligible affine offsets wrap at 16 bits. Verified TAC, allocator assignments,
  frame layout and calling conventions remain unchanged; there is no new IR,
  analysis framework or cross-block register/address cache.
- Copies advance HL/DE directly: one byte uses a load/store, two bytes use `LDI`
  and a final load/store, and larger/dynamic counts use `LDIR`. The original
  forward load-then-store order is preserved, including overlapping copies;
  this is not a `memmove` transformation. DE is saved only in an already
  allocated private word slot that is dead during the fused loop. If none is
  available, retain the original emission rather than adding stack usage.
- Fills remain store-only: 1-3 constant bytes use straight-line stores,
  4-256 use `DJNZ`, and dynamic/larger word counts use a guarded two-level
  byte-count loop. Do not seed an `LDIR` fill and introduce extra memory reads.
  Zero counts skip the body; observed exit-counter values are reconstructed
  through the existing assignment emitter. Both incoming paths establish the
  existing IX=DE fact before it is reused.
- Unsupported forms keep existing emission: signed/nonzero/strided counters,
  byte-width dynamic bounds, word arrays, internal branches, calls or changing
  pointers in the body, escaped or mutable scalar inputs, live extra
  temporaries, external control-flow entries, inline assembly and pure-assembly
  functions. Fixed global/local byte arrays and stable pointer arguments are
  supported; mutable global pointer/bound/fill scalars are not.

Against immediate parent `66ab52f` (the compiler from the step 2h/2i checkpoint):

| Workload | Kernel bytes, before -> after | Kernel T-states / 17 calls | Kernel T-states / 256-byte call |
|---|---:|---:|---:|
| `copyBytes` | 130 -> 76 | 490,891 -> 26,659 | 131,965 -> 5,804 |
| `fillBytes` | 67 -> 60 | 190,327 -> 32,239 | 50,700 -> 7,722 |

- Copy is 41.5% smaller with 94.6% fewer aggregate kernel T-states; fill is
  10.4% smaller with 83.1% fewer. On the 256-byte call, benchmark kernel stack
  reads/writes fall from 4,105/1,029 to 14/4 for copy and from 1,286/2 to 9/2
  for fill. These counters are not the allocator harness's frame-traffic metric.
- Wrapper bytes/cycles are unchanged. Copy's SP high-water remains 25 bytes,
  stack-access extent improves 25 -> 24 and complete frame footprint stays 27.
  Fill remains 20/20/22 respectively; maximum below-SP access stays 8 for both.
- Measured real decoder/array controls are unchanged: `unpackPackBits` is
  565 bytes / 159,912 T-states, `calculationArrayWorkload` is 1,656 / 6,372,748,
  and `calculationPackedCrc` is 913 / 12,801,227. The other benchmark workloads
  and all 28 separately executed real routines' cycles/frame traffic are also
  unchanged. This batch does not claim a decoder/array/CRC improvement.
- The full eight-configuration comparison passes 104 builds / 1,856 calls:
  all eight tracked metrics are non-regressing in every row, all 78 external
  controls and all 13 no-RA rows are exact, and all 26 normal/traced SSC
  assemblies, ROMs and measurements match. The existing 24 independent timing
  checks pass, plus eight separate 256-byte copy checks against z88dk-ticks
  with exact whole-call cycle, value and complete-buffer agreement.
- Added 38 generated loop variants (22 selected, 16 conservative fallbacks)
  plus eight arithmetic controls: 2,944 independent value/full-buffer executions
  per mode at 64 seeds. Covers zero through 512, 255/256/257 transitions,
  16-bit affine-offset wrap, overlap in both directions, wide/local-array
  frames, post-loop counter reads, calls, escapes, byte bounds and pressure.
  All 456 individual variant/boundary cases are non-regressing in cycles and
  frame reads/writes against the saved parent; fallback cases remain exact.
  Parent/current default and all-spill ROMs match, normal/traced ROMs match,
  and the saved parent fails the new selection assertion as intended.
- Qualification passes all 175 fixtures without snapshot changes; 12 allocator
  cohorts with 32 immutable verifier-rejection controls each; 28 real routines;
  traced/ASan identity for 70 ROMs; and a separate 12-seed ASan byte-loop run
  through 512. All 25 condition/comparison unit tests pass in release, traced
  and ASan builds, with 300 identical ROM/counter images; 18 comparison tests
  also pass under Python `-O`. MSVC builds, Python syntax/diagnostics and strict
  ANSI C normal/traced compilation pass; the latter excludes only existing
  unused-parameter warnings.
- Both complete calculation ROMs rebuild identically in normal/traced mode:
  used bytes remain 20,677 and 33,134, not their 65,536-byte padded sizes.
  Production growth is **322 net C lines and one helper**, confined to the
  Z80 emitter; the allocator core and pass 5 are unchanged. The helper reuses
  existing lowering/address machinery instead of adding a parallel optimizer.
- Retained qualification, saved-parent controls and independent timing evidence:
  `C:\cygwin64\tmp\advancing-loops-20261020\final`.

#### 2k. Constant Byte Arithmetic And Multiplication Setup

- [x] Specialize constant byte shifts in the existing emitter; compare unrolled
  shifts with accumulator rotate-and-mask sequences for unsigned values,
  including transfers, result width and flag/register clobbers.
- [x] Add only profitable constant byte multiplication sequences within the
  existing arithmetic path. Cost additions and subtractions for the actual
  operand width; do not assume byte signed-digit choices also suit word results.
- [x] Select existing constant word multiply chains before unnecessary generic
  operand setup, DE saves/loads and restoration. Preserve the established word
  shift/multiply paths, budgets and conservative fallbacks.

Acceptance: exhaustively check byte operands for selected constants and shift
counts, plus signed/promoted results, word boundaries, aliases and pressure.
Show exact instruction-selection changes and measured bytes/cycles; preserve
the compiler's existing shift semantics and default size/speed contract.

Step-2k checkpoint (2026-10-20):

- Merged the two byte-shift helpers. Constant counts select unrolled byte
  shifts, accumulator doubling, or shorter rotate-and-mask sequences, including
  transfers and masks in the cost. Zero and oversized counts avoid runtime
  loops; dynamic counts retain the existing implementation.
- Promoted operations enter the byte path only for an actual byte source and
  byte result: left shifts, or unsigned right shifts. Signed promoted right
  shifts and genuine word operations keep their existing word behavior.
  Constant counts retain their original byte/word width, including propagated
  256 and negative counts; they are not blindly truncated to eight bits.
- Shared constant-multiply selection runs before generic operand setup and
  reuses existing result stores and BC materialization. Byte results compare
  addition, subtraction and modulo-byte negation sequences within a 15-byte
  budget, including source preservation and result transfer. Word chains retain
  their 18-byte budget; byte operands widened to a word use a stricter 13-byte
  budget against the smaller byte runtime multiply. Full-word products never
  use byte-only modulo shortcuts, and unprofitable constants retain the
  generic implementation. Selected chains omit unused DE saves/loads/restores.
- Representative RA function measurements against the saved immediate parent
  (`60c1006`); cycles below cover 256 exhaustive byte-input calls, not a single
  operation:

  | Case | Function bytes before -> after | T-states before -> after |
  |---|---:|---:|
  | Byte result `*3` | 56 -> 45 | 152,320 -> 66,048 |
  | Byte result `*15` | 56 -> 48 | 152,320 -> 69,120 |
  | Byte result `*255` | 56 -> 44 | 152,320 -> 65,024 |
  | Widened `*255` plus `*7` | 98 -> 91 | 288,256 -> 212,992 |
  | Byte result `<<4` | 47 -> 47 | 75,008 -> 68,096 |
  | Byte result `<<7` | 52 -> 46 | 85,760 -> 66,816 |

- All 104 comparison builds / 1,856 calls are non-regressing in size, cycles
  and stack measurements; all 78 external-compiler rows remain exact. RA array,
  stencil, tile and path kernels shrink respectively 1,656 -> 1,641,
  1,740 -> 1,735, 1,232 -> 1,212 and 1,648 -> 1,643 bytes. Their aggregate
  kernel T-states fall 6,372,748 -> 6,351,978; 4,405,850 -> 4,400,146;
  2,165,246 -> 2,150,676; and 7,139,013 -> 7,099,333. No-RA improves too;
  caller overhead stays exact. All 26 normal/traced comparison images match.
- Added 76 arithmetic variants to the existing generator, plus eight controls:
  84 functions / 21,504 executions per mode with all 256 byte inputs. Cover
  signed/unsigned values, both constant operand orders, assignment aliases,
  count boundaries, widened products and dynamic fallbacks. Saved-parent,
  release, traced and ASan runs pass the independent oracles; every function's
  size, cycles and frame traffic are non-regressing. Current images match in
  all five build modes across release/traced/ASan, default/no-RA stay identical,
  and the saved parent fails the new instruction-selection assertion.
- All 175 fixtures pass: refreshed 37 exact opcode regions in 16 fixtures
  against saved-parent bytes (34 smaller, three same-size), plus the byte
  shift-result register assertions, retaining lifecycle/alias/spill checks.
  All 14 allocator cohorts pass 32 immutable verifier-rejection controls each;
  28 real routines have non-regressing cycles/frame traffic; 80 traced/ASan
  ROMs match. The 24 standard independent timing checks pass, plus 80 additional
  selected-arithmetic checks using `z88dk-ticks`, exact values and full buffers.
- All 25 condition/comparison tests pass in release, traced and ASan builds,
  with 300 identical ROM/counter images; 18 comparison tests also pass under
  Python `-O`. MSVC, Python syntax/diagnostics and strict ANSI C normal/traced
  builds pass; the latter excludes only pre-existing unused-parameter warnings.
  Complete calculation ROM used sizes are 20,677 -> 20,677 and
  33,134 -> 32,819 bytes, with identical normal/traced 65,536-byte padded images.
- Production growth is **31 net C lines and zero net functions**. Reused the
  existing address/result machinery, removed duplicated word materialization
  and obsolete assembly comments, and left pass 5 and allocator contracts
  unchanged. Retained qualification and saved-parent evidence:
  `C:\cygwin64\tmp\byte-arithmetic-20261020\final`.

#### 2l. Proven Bounded-Counter Narrowing

- [x] Reuse bounded-loop analysis to prove initialization, bounds and updates
  before selecting byte execution for a source word counter.
- [x] Check all counter uses, values observed after exit, conversions, escapes
  and wrap behavior. Reject unknown ranges or interfering updates; do not infer
  safety from array undefined-behavior assumptions borrowed from another compiler.
- [x] Start with the CRC's proven 0-7 bit counter, which SDCC executes in one
  byte while SameSameC retains BC. Measure register availability and compare/
  increment costs before considering a separate countdown/`DJNZ` selection.

Acceptance: reduce a measured hot-loop cost without changing language-level
types or observable values. Cover zero-trip loops, nested loops, early exits,
post-loop reads, 255/256 boundaries, wrapping/escaped counters and unchanged
fallbacks. Keep any byte-counter instruction choice target-specific.

Step-2l checkpoint (2026-10-20):

- Extracted the existing copy/fill loop's initialization, guard and latch checks
  into one shared target-local counter proof. Byte selection requires a source
  `u16` temporary, an initialization and constant upper bound in 0-255, exactly
  two definitions, a guarded unit increment and one canonical backedge. Reject
  side entries, extra writes, address-taking, inline assembly and calls inside
  the loop; unknown bounds, signed counters and 256/wrapping cases keep the
  existing word paths. This does not infer ranges from array undefined behavior.
- Reused the existing unsigned byte comparison emitter and direct increment
  paths. A retained BC counter compares C and uses `INC C`; a spilled word
  counter compares/increments its low byte without carry handling. Original
  word initialization keeps the high byte zero, including on zero-trip and
  early-exit paths. Source types, word-sized backing slots and full-word reads
  after the loop remain unchanged, including subsequent signed narrowing.
- BC remains reserved as a word pair: B is not opportunistically allocated to
  another value. No allocator metadata, frame packing, ABI or pass-5 changes,
  and no countdown/`DJNZ` rewrite or value reconstruction. The CRC's existing
  BC reservation is sufficient: its guard is three bytes shorter, and its
  increment drops from six to four T-states.
- Immediate-parent (`7ac07df`) RA workload measurements; kernel bytes include
  the same required helpers, and cycles cover the existing benchmark inputs:

  | Kernel | Bytes before -> after | T-states before -> after |
  |---|---:|---:|
  | Packed CRC | 913 -> 910 | 12,801,227 -> 12,026,571 |
  | Stencil | 1,735 -> 1,717 | 4,400,146 -> 4,336,246 |
  | Tile | 1,212 -> 1,209 | 2,150,676 -> 2,073,424 |

  CRC is 6.1% faster, stencil 1.5% and tile 3.6%. All 104 comparison builds /
  1,856 calls are non-regressing in size, cycles and stack measurements.
  All 78 external rows, 13 no-RA rows and caller costs stay exact; all 26
  normal/traced comparison images match.
- Extended the existing word-counter family by 30 variants and moved it into a
  dedicated cohort rather than adding a parallel generator. Its 38 variants
  plus eight controls run 2,944 cases per mode with 64 seeds. Cover zero trips,
  nonzero starts, nested loops, break/continue, post-loop word reads, signed
  conversions, 255/256 boundaries, escaped reads, interfering updates, calls,
  reused counters and pressure. Nineteen variants select byte execution; the
  other nineteen retain exact sizes, cycles and frame traffic.
- Every focused function is non-regressing against the saved parent. Parent,
  release, traced and ASan executions pass independent value/full-buffer oracles;
  current images match across all five build modes in release/traced/ASan.
  Parent/current default and all-spill images stay identical, and the saved
  parent fails the new selection assertion. All 24 standard independent timing
  checks pass, plus 34 additional counter/fallback/workload checks using
  `z88dk-ticks`, exact values and complete buffers.
- All 175 fixtures pass without snapshot updates; 15 allocator cohorts pass
  32 immutable verifier-rejection controls each; all 28 real routines have
  non-regressing cycles/frame traffic; and 85 traced/ASan ROMs match. All
  25 condition/comparison tests pass in release, traced and ASan, with 300
  identical ROM/counter images; 18 comparison tests also pass under Python
  `-O`. MSVC, Python syntax/diagnostics and strict ANSI C normal/traced builds
  pass; the latter excludes only pre-existing unused-parameter warnings.
- Complete calculation ROM used sizes fall 20,677 -> 20,674 and
  32,819 -> 32,798 bytes, with identical normal/traced padded images.
  Production growth is **99 net C lines and one shared helper**, including the
  removed duplicate copy/fill checks. No new allocator state or test helpers.
  Qualification, source/binary hashes and saved-parent evidence:
  `C:\cygwin64\tmp\bounded-counter-20261020\final`.
- A separate alias-written counter already fails in the saved parent and
  remains unchanged in the final compiler. Its reproduction is retained as
  `known-parent-escape-before-init.json` in that evidence directory, not counted
  as a passing control or silently fixed here. The escaped-read fallback above
  passes independently; the unrelated alias-write optimizer issue remains open.

#### 2m. Lowering-Aware Costs And Local Address Reuse

- [x] Extend current profitability hooks to account for actual address setup,
  transfers, width extension and saves/restores where estimates currently omit
  them. Keep frequency and size/cycle policy explicit.
- [x] Reuse existing address helpers/cache for proven nearby addresses within
  basic blocks rather than introducing parallel state or a new optimizer.
- [x] Invalidate facts on relevant register writes, calls, assembly and control
  boundaries. Do not retain cached addresses across joins without proving every
  incoming path establishes the same value.

Acceptance: correlate changed decisions with emitted sequences and measured
bytes/cycles/frame traffic. Exercise cache hits and invalidation, wide offsets,
pressure and unknown-cost fallbacks. Preserve immutable verification and
normal/traced equivalence; do not add a dry-run framework without measured need.

Step-2m checkpoint (2026-10-20):

- Replaced the existing IX=DE Boolean with one known IX=DE+offset fact, not a
  second cache. The shared address helper reuses signed displacements in
  `[-128,126]`, reserving displacement 127 for a word's high byte. Nearby rebases
  use one or two INC/DEC IX instructions only: 2/4 bytes and 10/20 T-states beat
  the full 6-byte/29-T-state setup. Three adjustments cost 30 T-states and are
  rejected. The same fact supports existing pointer-base reads and arithmetic
  save/restore paths; removed the duplicate uncached address emitter and its
  now-unused ADD IX helper, retaining the byte helper's existing forced-cold
  operand contract through the shared emitter.
- Corrected the existing profitability callback's byte array-index transfer
  charge: A/B require a 4-T-state/1-byte move into C; C requires none. The focused
  A candidate's estimated saving changes from 38 T-states/6 bytes to 34/5.
  Audited the existing signed-index extension delta against its actual
  preserving/non-preserving sequences: worst-case 27 versus 16 T-states, seven
  versus four bytes, and no extra saves/restores. Keep the existing explicit
  frequency estimate, 256-instruction bound, cycles-without-byte-growth policy
  and unknown-cost fallback. Early allocation cannot assume an emitter cache
  fact or a not-yet-assigned temporary spill offset; late address choices use
  the actual proven fact instead of adding a dry-run framework.
- IX/DE writes, calls, unknown raw assembly and ordinary labels invalidate the
  fact. Compiler-generated assembly variable accesses retain it only while
  their actual register writes permit it. No new facts cross joins. Existing
  arithmetic frame saves restore the matching fact; the linker copy helper
  demonstrably preserves IX and POP DE restores its original frame relation.
  TAC, allocation metadata, frame layout and ABI contracts are unchanged.
- Added 24 address/pressure/boundary/barrier probes through the existing
  generator and independent oracle, plus its eight arithmetic controls:
  32 functions and 2048 executions per mode. Cover 2..384-byte arrays, both
  profitable adjustment thresholds, the three-step fallback, bounded high-byte
  displacements, globals, calls, raw IX-clobbering assembly, joins, multiply/
  divide, unknown shift costs and signed/unsigned byte indexes. Parent/current
  sources are identical; the saved parent fails the new selection assertions.
  A representative wide-frame probe drops from 264 to 200 bytes and from
  94784 to 75008 T-states over 64 calls. Frame reads/writes remain unchanged;
  these are address-setup savings, not a claimed frame-traffic reduction.
- The full 104-row/1856-call comparison passes, with all 78 external and 13
  no-RA C controls exact. Pathfinding drops from 1643 to 1439 kernel bytes and
  from 7099333 to 6235713 aggregate kernel T-states; caller costs and complete
  frame-footprint observations are unchanged. In the separate real-project
  corpus, actor and path workloads improve from 33003283 to 26504847 and from
  25373751 to 22229194 T-states, respectively.
- Explicit safety tradeoff: three hand-written assembly helpers previously
  carried an unproved IX fact through raw assembly. Invalidating it adds two
  cold setups, 12 bytes/58 T-states per call, in RA and no-RA modes.
  Their three-call bank wrapper adds 174 T-states. These are recorded
  exceptions, not hidden non-regression claims; all results, buffers and frame
  accesses remain correct. The unrelated alias-written counter failure recorded
  in 2l still returns 42740 rather than 50814 and remains open.
- Used whole-ROM bytes change from 20674 to 20650 and from 32798 to 31604.
  Padded sizes remain 65536 and normal/traced images match. All 175 fixtures,
  16 immutable-verifier cohorts, 28 real routines, 90 traced/ASan allocator ROM
  identities, 300 three-build condition ROM identities, strict normal/traced
  C89 builds, three MSVC builds and 105 independent timing checks pass.
  Strict pass-5 compilation excludes its existing transposed-calloc warning,
  independently reproduced in the saved parent, as well as unused parameters.
- Updated 40 exact opcode regions in 16 fixtures from passing-parent/current
  ROMs, preserving all non-opcode SSC source. Growing regions explicitly
  include the required raw-assembly barriers. Net production change:
  **16 fewer C lines and two fewer functions**. Evidence, saved 392790f sources/
  binaries and the commit-bound qualification summary are retained in
  `C:\cygwin64\tmp\lowering-costs-20261020\final`. No push.

#### 2n. Leaf-Frame And Helper-Contract Audit

- [x] Measure leaf prologue/epilogue and call-heavy costs after the preceding
  improvements; identify redundant work in existing frame/call lowering.
- [x] Audit existing helper input, result and preserved-register metadata.
  Tighten only demonstrably over-conservative clobbers; qualify actual helper
  effects before removing saves or retaining values across them.
- [x] Keep the existing DE/frame and caller/callee ABI contracts. Frame-pointer
  omission or register arguments require a separate explicit, gated proposal,
  not a silent convention change to force an optimization.

Acceptance: record measured safe removals, or explicitly close the audit with
no profitable supported case. For changes, verify mixed widths, nested,
recursive, banked, cross-file and assembly-facing calls, exact SP/DE restoration
and complete frame footprint. Report caller and kernel costs separately.

##### Step 2n Checkpoint (2026-10-20)

Complete within the existing ABI, against immediate parent `708bbaf`. Reuse
the existing frame-prefix policy, final frame size and generic setup emitters;
no new production functions, allocator rules, convention or helper entry points.
Net production change: **10 additional C lines**, confined to the Z80 prologue.

- Normal callers already enter with SP at the saved DE's low byte, `DE-3`.
  When the final frame needs zero to four further bytes, use that many `DEC SP`
  instructions instead of `LD HL,offset; ADD HL,DE; LD SP,HL`. The zero-byte
  case needs no setup. Keep the generic path for five or more bytes.
- The full setup costs 5 bytes/27 T-states; each `DEC SP` costs 1 byte/6
  T-states. Entries save 1-5 bytes and 3-27 T-states. Do not substitute PUSH:
  arguments already occupy the incoming frame and must not be overwritten.
  DE, all argument/result offsets, the final SP and complete frame footprint
  remain unchanged. Pure-assembly functions and special `mainmain` handling
  keep their existing paths; no frame-pointer omission is introduced.
- Keep the existing IX-proven return shortcut and generic epilogue. The audit
  found no further measured, supported epilogue or helper-save removal.
  The setup cost gate is Z80-local, not an assumed SM83 instruction policy.

The helper audit distinguishes the inner arithmetic body from the complete
TAC's operand setup, extension and result storage:

| Existing helper | Inputs and results | Why frame preservation remains necessary |
| --- | --- | --- |
| Byte multiply | H * E -> HL | Loading E replaces part of the frame pointer; the helper also writes D and B. |
| Word multiply/divide/modulo | BC and DE -> HL, or CA quotient/HL remainder | The inner helpers preserve their DE operand, not the original frame pointer that operand setup replaced. |
| Byte divide/modulo | H / E -> A quotient/B remainder | The inner helper preserves DE, but loading its E operand already changed the frame pointer. |
| Bank-local initializer copy | DE source, HL target, BC byte count | The copy advances DE/HL and consumes BC; IX/IY preservation already supports the existing address-cache restoration. |

All these helper/emitter bodies and the TAC clobber descriptors remain
unchanged. Helper-only preservation is not permission to weaken masks that
also cover address materialization, signed extension and result moves. Keep
DE saves and the full ordinary-call/raw-assembly barriers.

Measured focused leaf code bytes, including existing epilogues:

| Further frame bytes | Parent -> current bytes | Entry T-states saved |
| ---: | ---: | ---: |
| 0 | 11 -> 6 | 27 |
| 1 | 26 -> 22 | 21 |
| 2 | 32 -> 29 | 15 |
| 3 | 42 -> 40 | 9 |
| 4 | 55 -> 54 | 3 |
| 5, generic fallback | 65 -> 65 | 0 |

The full 104-row/1856-call comparison passes. All kernel bytes/cycles, all
complete frame observations and all 78 external controls remain exact.
The packed-CRC RA caller wrapper improves **85 -> 82 bytes**, **469 -> 454
T-states/call** (7973 -> 7718 over 17 calls); its kernel remains 910 bytes/
12026571 aggregate T-states. The no-RA wrapper improves 97 -> 96 bytes and
545 -> 542 T-states/call. Other comparison rows, including the 126-byte/
729-T-state multiply/add caller, remain unchanged. All 28 real-project
routines retain exact cycles and frame reads/writes. Used ROM decreases
**20650 -> 20637** bytes for calculations-1 and **31604 -> 31591** for
calculations-2, with normal/traced image identity. These are bounded small-frame
wins, not a claim that general call-heavy overhead or competitor parity is solved.

Eight new cases extend the existing call-staging cohort rather than adding
another generator/cohort. They cover zero through five bytes, byte/word and
signed arguments, word returns, argument integrity and fallback. Existing
cases retain nested calls, recursion, wide frames, escaping arguments and
mixed-width staging; the complete projects cover cross-file/banked and
assembly-facing calls.

Qualification also exposed a synthetic-entry error: the native execution
harnesses initialized SSC callees/wrappers at `DE-1`, not the existing saved-DE
entry position `DE-3`. Absolute prologue setup had hidden it. Correct both
harnesses and the independent timing bootstrap; raw RET instruction probes
explicitly retain their `DE-1` entry. Add exact entry-SP regression checks and
keep external compiler conventions unchanged. Saved-parent focused execution
and mixed-version tests use the same corrected entry, not a relaxed oracle.
Explicit frame-size comments preserve static frame accounting for both old
absolute and new zero/relative prologues.

Release/traced/ASan qualification passes: 175-fixture coverage, including 26
final affected-fixture rechecks; 16 immutable allocator cohorts and 28 real
routines; 90 allocator and 300 condition ROM identities; 26 combined condition/
comparison unit tests plus 19 optimized comparison units per build; strict C89
normal/traced compilation. The 24-function focused cohort executes 1536 calls
per mode/build with complete buffer and SP/DE checks. There are **192 independent
timing checks**, including every setup threshold, and **40 mixed-version
builds/340 guarded calls** covering both old/new directions. All 55 updated
opcode regions in 26 fixtures retain their lengths and all non-opcode source;
only relocated addresses change. The previously documented escaped-counter
alias defect still returns 42740 rather than 50814; it is not counted as passing.

Saved-parent binaries/source, raw reports and commit-bound qualification are
retained in `C:\cygwin64\tmp\leaf-helper-20261020\final`. No push. At this
checkpoint, steps 2a-2n were complete; the newly planned follow-ups 2o-2s below
now precede initial SM83 work.

### Profile-Guided Follow-ups After Step 2n (2026-10-20)

Complete these five bounded steps in order before the initial SM83 backend.
The fresh comparison at `141338d` passes 104 builds/1856 calls and 24 independent
timing checks. Workload fingerprints and all 78 external control rows match the
earlier profiles; results exactly reproduce the step 2n qualification.
Reports and historical analysis are retained in
`C:\cygwin64\tmp\z80-profile-141338d-20261020`.

Against the profile after 2d, RA kernels are geometrically 14.6% smaller and
use 40.4% fewer cycles. Remaining gaps include multiply/add at 70 bytes versus
stock SDCC's 40, shift/restore at 83 versus 51, and byte sum at 102 versus 47.
These competitor sizes use its size profile. Kernel-cycle parity across the
whole corpus is not general parity: excluding copy/fill, RA still uses 1.38
times stock SDCC's speed-profile cycles; the five large array/stencil/tile/
pathfinding/CRC workloads use 1.55 times. Keep caller work separate: the
multiply/add wrapper is 126 bytes/729 T-states versus stock SDCC's 20/114.
The older 161/917 caller figures belong to the checkpoint before step 2e.

Source guidance comes from SDCC r17016's operand-aware
[Z80 generator](https://svn.code.sf.net/p/sdcc/code/!svn/bc/17016/trunk/sdcc/src/z80/gen.c),
[loop induction](https://svn.code.sf.net/p/sdcc/code/!svn/bc/17016/trunk/sdcc/src/SDCCloop.c)
and [branch rules](https://svn.code.sf.net/p/sdcc/code/!svn/bc/17016/trunk/sdcc/src/z80/peeph.def),
plus runtime selection in sccz80 shipped with z88dk 2.4. Reuse the existing
[TAC optimizers and local promotion](compiler/pass_5.c) and
[Z80 emitters](compiler/pass_6_z80.c), not another optimizer or rule engine.
Do not repeat completed 2j-2n work or treat these candidates as measured gains.
The existing checkpoint gates apply to each implementation batch.

#### 2o. Arithmetic-Produced Locals And Existing Pure-Expression Folds

- [x] Extend the existing local-promotion eligibility where proven safe for
  multiply/divide/modulo-produced named scalars. The multiply/add kernel previously
  stored its product to the frame and immediately reloaded it; prefer existing
  temporary retention and location resolution over another forwarding helper.
- [x] Diagnose why the existing XOR-cancellation rule does not remove the two
  XORs in shift/restore. Correct eligibility or pass ordering where justified;
  do not add a duplicate rule or assume SDCC already cancels this example.
- [x] Preserve matching types, promotions, operand stability, observable reads,
  escapes, aliases and call/assembly barriers. Keep the full arithmetic TAC
  clobber contracts and necessary DE saves audited in 2n.

Acceptance: show the source/TAC/assembly cause and remove proven product frame
traffic or redundant pure work with measured byte/cycle gains. Check mixed
widths, signed boundaries, narrowing, multiple definitions/uses, escaping
locals and intervening calls/writes. Measure caller, kernel and complete frame
footprint separately; retain conservative fallbacks.

#### Step 2o Checkpoint (2026-10-20)

Reuse the existing local-promotion pass and three existing producer/consumer
predicates rather than adding another forwarding helper or optimizer pass.
Eligible word multiply/divide/modulo results now reach existing temporary
retention. Reuse the same producer predicate for word shifts: after cancellation,
shift/restore's initial shift otherwise becomes a named destination that cannot
share temporary storage, increasing its frame footprint by two bytes.
No production function is added; production C grows by **six net lines**.

The existing XOR rule rejected later reads of the first named destination even
when the second XOR overwrote that same destination with the restored value.
Skip that later-read rejection only for this same-destination case. Keep the
matching-type, stable-mask, escape, call and assembly guards; different
destinations and changed masks retain their original behavior.

Keep byte arithmetic-produced locals conservative. Reuse the existing
`word_definitions` proof before promoting a new arithmetic producer: a named
word product of two byte operands otherwise becomes a byte temporary and loses
its high byte. Proven word-width mixed operands and shifts still qualify.
Also keep **DIV/MOD consumers** on the original named-input path. Unrestricted
promotion broke the existing shared quotient/remainder computation in product/
restore and array workloads; reject that version and add a sharing regression.
Arithmetic helper bodies, full TAC clobber masks, necessary DE saves and the
public ABI remain unchanged.

Against parent `b39815e` (the qualified `141338d` compiler), RA measurements are:

| Workload | Kernel bytes before/after | Kernel T-states before/after | Observed whole-call frame bytes before/after |
| --- | ---: | ---: | ---: |
| Multiply/add | 70 / 58 | 20098 / 18806 | 26 / 24 |
| Product/restore | 147 / 135 | 42415 / 41123 | 30 / 28 |
| Shift/restore | 83 / 59 | 12042 / 9934 | 22 / 22 |
| Array | 1641 / 1605 | 6351978 / 6241578 | 90 / 88 |
| Stencil | 1717 / 1693 | 4336246 / 4267798 | 120 / 120 |

Cycles aggregate 17 calls for the three small kernels and 20 for each workload;
frame observations are maxima, not global RAM or unexecuted-path bounds.
Multiply/add removes two product frame reads and two writes per call, saving
76 T-states. Shift/restore removes both redundant XORs without enlarging its
frame. Caller costs are unchanged, including multiply/add's 126 bytes/729
T-states per call. All 26 RA/no-RA rows are non-regressing in bytes, cycles,
caller costs and observed stack/frame metrics against the immediate parent.
No-RA also benefits from XOR cancellation: shift/restore shrinks 131 to 83 bytes.
Whole used calculation ROMs shrink **20637 to 20625** and **31591 to 31555**
bytes; padded files remain 65536 bytes.

Add 31 cases to the existing scalar-word generator/oracle and reuse the existing
call-staging cohort to stay within one ROM bank; add no generator or cohort.
The focused 41-function cohort passes **10496 calls per reported mode/build**
with independent results, complete buffers and SP/DE checks. Cover signed and
mixed-width products, byte-to-word widening, narrowing, redefinitions, multiple
uses, escaping aliases, calls/writes, shifts, XOR guards and quotient/remainder
sharing. The saved parent fails the new promotion selection; the rejected
unrestricted version fails the division-input fallback.

Final release/traced/ASan qualification passes: all **175 fixtures without
snapshot updates**, 16 immutable allocator cohorts and 28 real routines,
90 allocator and 300 condition ROM identities, 26 combined condition/comparison
unit tests plus 19 optimized comparison units per build, and strict normal/
traced C89 compilation. The full comparison passes **104 builds/1856 calls**,
unchanged corpus fingerprints and all **78 external controls**. There are
**36 independent timing checks** and **12 mixed-version builds/228 guarded
calls** in both old/new directions. The separately documented escaped-counter
alias defect remains 42740 rather than 50814 and is not counted as passing.

Saved parent binaries, source hashes, rejected-version controls and final
qualification are retained in `C:\cygwin64\tmp\arithmetic-locals-20261020`.
No push. Step 2p is next; 2q-2s remain pending before the initial SM83 backend.

#### 2p. Advancing-Pointer Reductions And Scaled Induction

- [x] Extend the existing bounded loop proofs and byte-loop emission beyond
  copy/fill, starting with byte sums. Retain a profitable accumulator and
  advance a proven pointer instead of rebuilding `base + index` each iteration.
- [x] Then consider row pointers and constant-stride/scaled induction in
  array, tile and stencil loops. Reuse existing address scheduling and target
  costs rather than introducing general loop promotion or a new IR.
- [x] Prove invariant bases, bounds, stride, alias behavior and address/value
  wrap. Preserve source-order memory effects, observable final counters,
  zero/256 and full word counts, DE restoration and call/assembly barriers.

Acceptance: reduce repeated address/frame work in byte sum and demonstrate a
measured win in at least one complex calculation workload. Check complete
buffers/guards, zero/one/255/256/65535 counts, pointer and arithmetic wraps,
aliases, calls, exits and register-pressure fallbacks. Keep memory accesses
valid; use bounded-exit or lowering probes for wide-count fallbacks rather
than invalid buffer ranges. Price full setup, preservation and frame footprint,
not only the inner loop.

#### Step 2p Checkpoint (2026-10-20)

Extend the existing `_generate_asm_byte_loop_z80` copy/fill emitter for two
bounded unsigned-byte reductions. For a signed or unsigned word-local sum,
retain the accumulator in HL and advance the source pointer in IY. For a
fixed-address byte-memory sum, retain the source pointer in HL and the target
address in IY, but **read and write the target every iteration**, in original
TAC order. This preserves aliases where a target write changes a later source
read; do not hoist an aliased target value out of the loop.

Reuse `_is_z80_bounded_counter` for an isolated named-counter loop as well as
its existing temporary-counter case. Prove the initialization, one `+1` update,
matching bound/backedge/exit and absence of escaped counter addresses or
unsupported incoming branches. This enables pathfinding's reused named `i`
without changing whole-function local promotion. Named support is restricted
to the whole-loop assignment entry; standalone counter narrowing retains its
temporary-only guards. Publish the final named counter after the loop.

Reuse existing location/address helpers, displacement/count reloads and escape/
liveness checks. Dynamic zero counts skip setup; full-width counts use BC;
constant counts 1-256 use B/`DJNZ`, including B=0 for 256. The new reductions
need no pushes, scratch slots or additional SP displacement, and leave DE as
the frame pointer. Reject escaped/changing invariants, calls/assembly, early
exits, extra live temporaries and unsupported operations. Signed-byte widening,
word-array and stride-two cases remain on their original paths.

Considered row/scaled induction in array, tile and stencil routines. Their
multiple dependent accesses, changing cursors and calls do not fit this bounded
straight-line emitter. Keep those cases conservative rather than adding another
loop optimizer; the fixed-target pathfinding reduction supplies the required
complex-workload win. No production function is added and no new pass or cohort
is introduced. Production C grows by **103 net lines**, replacing 49 old lines.
Allocator/clobber policy, arithmetic helper bodies and the public ABI are
unchanged.

Against qualified parent `bb003e3`, RA measurements are:

| Workload | Kernel bytes before/after | Kernel T-states before/after | Observed whole-call frame bytes before/after |
| --- | ---: | ---: | ---: |
| Byte sum | 102 / 99 | 240653 / 76126 | 24 / 24 |
| Pathfinding | 1439 / 1385 | 6235713 / 5661993 | 187 / 187 |

Cycles aggregate 17 byte-sum and 20 pathfinding calls. Byte sum uses 68.4% fewer
kernel T-states and pathfinding 9.2% fewer. Caller costs are unchanged: 126
bytes, with 12291/14460 aggregate caller T-states respectively. All 26 SSC
RA/no-RA rows are non-regressing in all eight size/cycle/stack/frame metrics;
all no-RA rows are unchanged. Reject the first DE-accumulator prototype's
103-byte byte sum despite its speed win; the final IY-source form satisfies
both size and speed gates. Whole used calculation ROMs are **20625 / 20625**
and **31555 / 31455** bytes; padded files remain 65536 bytes. The separate
67-case real sorting check also improves from 6333013 to 6190437 T-states
with less frame traffic.

Add 21 variants to the existing `scalar_loop_memory` generator/oracle, not a
new generator or cohort. The focused 46 functions pass **11776 calls per
reported mode/build**, including complete buffers/guards and SP/DE restoration.
Cover zero/one/255/256/257/512 counts, valid pointer/arithmetic wraps, signed
word accumulators, mutable target aliases, reused/final counters, escaped
counter/accumulator/bound, calls, exits and pressure fallbacks. Check 65534/
65535 through bounded early-exit fallbacks rather than invalid huge buffers.
The saved parent passes the same numerical oracles but fails the new
advancing-sum selection.

The existing copy/fill case with a final counter stored after the loop now
qualifies too: 114 to 92 bytes. Update its selection expectation, not its
numerical oracle. All 46 copy/fill-cohort functions are non-regressing against
the parent over 2944 calls per reported mode; 384 additional parent/current
calls confirm that this fill's observed whole-frame footprint stays 14 bytes
with RA, with no SP/access-extent regression.

Final release/traced/ASan qualification passes all **175 fixtures without
snapshot updates**, 16 immutable allocator cohorts and 28 real routines,
90 allocator and 300 condition ROM identities, 26 combined condition/comparison
units plus 19 optimized comparison units per build, and strict normal/traced
C89 compilation. The full comparison passes **104 builds/1856 calls** with
unchanged corpus fingerprints and all **78 external controls**. There are
**88 independent timing checks** and **12 mixed-version builds/240 guarded
calls**, covering both old/new directions and additional 257/511/512-byte sums.
The separately documented escaped-before-initialization counter defect remains
42740 rather than 50814 and is not counted as passing.

Saved parent binaries, frozen source hashes, the rejected size-regression
control and final qualification are retained in
`C:\cygwin64\tmp\pointer-reductions-20261020`. No push. Step 2q is next;
2r-2s remain pending before the initial SM83 backend.

#### 2q. Variable-Count Word Shifts

- [x] Extend the existing word-shift helper's variable path; do not duplicate
  the constant-count lowering completed in 2k. The current path repeatedly
  tests and decrements a 16-bit BC count.
- [x] Under the selected no-code-growth policy, prove the complete count is
  below 16 and handle zero before using a bounded byte-count loop such as
  `DJNZ`. Unknown full-width counts retain the original loop. Preserve the
  existing result for every 16-bit count; never merely truncate the count,
  mask it to 15 or borrow a competitor's undefined-count behavior.
- [x] Select using complete setup and clobber costs, preserving live operand
  locations and flags. Keep instruction selection Z80-local; do not assume
  `DJNZ` exists on SM83.

Acceptance: measure shift/restore and a shift-heavy CRC/calculation workload,
including setup, bytes and cycles. Use independent numerical oracles for both
directions, word edge values and counts 0, 1, 7, 8, 15, 16, 17, 255, 256 and
65535 plus seeded full-width cases. Cover byte/word count operands, retained
registers, spills and unchanged constant/signed paths.

#### 2q checkpoint (2026-10-20)

Complete the **user-selected size-neutral-only policy**, not a generic
full-width guard. The guarded prototype reduced shift/restore cycles but grew
its kernel from 59 to 79 bytes; reject it and retain its measurements as a
negative control. Unknown counts, including 16 through 65535, keep the original
full BC test/decrement loop with no truncation or implicit mask.

Reuse the existing word-shift helper's instruction body for constant, bounded
variable and fallback paths. Select the bounded path only with RA enabled,
without force-all-spill, for an unsigned word value and unsigned byte/word
temporary count. Reuse existing temporary metadata to recognize a single-write
AND with a nonnegative constant below 16, or the existing bounded-counter proof
with maximum 15. Reject changed count definitions and unsupported producers;
signed and widened-byte value paths remain unchanged.

For proven counts, retain the full BC zero test, copy C to B and use `DJNZ`.
The complete helper remains **8 bytes left / 11 bytes right**, matching the old
variable loops. Zero costs the same; counts 1-15 save **20*n+6 T-states** inside
the helper. Setup, the full AF/BC/HL clobber contract, DE/frame preservation,
the public ABI and byte-shift lowering are unchanged. No production function,
pass or cohort is added. Production C grows by **13 net lines**, replacing 34
old lines rather than duplicating the shift body.

Against qualified parent `4a8b776`, RA measurements are:

| Workload | Kernel bytes before/after | Kernel T-states before/after | Observed whole-call frame bytes before/after |
| --- | ---: | ---: | ---: |
| Shift/restore | 59 / 59 | 9934 / 9934 | 22 / 22 |
| Tile/CRC calculation | 1209 / 1209 | 2073424 / 2049176 | 128 / 128 |

Cycles aggregate 17 shift/restore and 20 tile calls. Tile uses **1.17% fewer
kernel T-states**. Caller costs remain 126 bytes and 12393/14460 aggregate
T-states respectively. All 26 SSC RA/no-RA rows keep exactly the same kernel
bytes and are non-regressing in all eight size/cycle/stack/frame metrics;
all no-RA rows are unchanged. Whole used calculation ROMs remain **20625**
and **31455** bytes; padded files remain 65536 bytes. In the real-routine
corpus, heap cycles improve 28405701 to 28372061 and tile cycles 6072556 to
6005276, without additional frame traffic.

Expand the existing `matrix_shift` family from 2 to 32 variants and place it
in the existing divmod cohort. Reuse the generator/oracle: cover byte/word
counts, full-width edge/seeded counts, both directions, live-value pressure,
narrowing, aliases, far frames, signed/widened controls, masks 31/255/65535
and a count changed after masking. The focused 40 functions pass **2560 calls
per reported mode** on the saved parent and final traced compiler, plus 640
per release/ASan mode. Eight proven-shift functions additionally pass **2048
calls per mode/version** over 256 cases each, with no cycle/frame regressions.
Every focused function keeps its size. Unsupported shift-function bytes and
the entire constant/signed-literal cohort match the parent; default, all-spill
and no-RA focused ROMs are also byte-identical. The parent passes the numerical
oracles but fails the new bounded-loop selection assertion.

Final release/traced/ASan qualification passes all **175 fixtures without
snapshot updates**, 16 immutable allocator cohorts and 28 real routines,
90 matching traced/ASan allocator images and 300 condition images across the
three builds, plus strict normal/traced C89 compilation. The full comparison
passes **104 builds/1856 calls**, unchanged corpus fingerprints and all
**78 external controls**, with **76 independent timing checks** and
**12 mixed-version builds/268 guarded calls**. The separately documented
escaped-before-initialization counter defect remains 42740 rather than 50814
and is not counted as passing.

Saved parent binaries, frozen source hashes, rejected prototypes and final
qualification are retained in
`C:\cygwin64\tmp\variable-word-shifts-20261020`. No push. Step 2r is next;
2s remains pending before the initial SM83 backend.

#### 2r. Operand-Aware Word Comparisons And Branch Selection

- [x] Extend the existing comparison emitter for profitable operand layouts.
  Compare retained unsigned words against indexed frame bytes or constants
  with a subtract/borrow chain and one carry decision where legal, rather
  than materializing a word subtraction or issuing two conditional jumps.
- [x] Preserve complete-word equality and the signed/unsigned distinction,
  live operands, flags and fallback paths. A high-byte zero flag alone does
  not prove word equality; keep signed overflow handling correct.
- [x] Select `JP` versus `JR` only with a proven displacement/target and an
  explicit cost decision. Do not blindly shorten all jumps: on ordinary Z80,
  an unconditional or taken `JR` can be slower than `JP`. Keep the existing
  default's speed behavior unless a tradeoff is explicitly selected.

Acceptance: show the eliminated materialization/branches and measured bytes/
cycles in byte sum and a complex loop. Check all relations, signed boundaries,
equal high bytes with unequal low bytes, carry/borrow edges and retained/spilled
operands. Test forward/backward and near/out-of-range branches, including the
relative displacement limits; unknown ranges keep the safe absolute form.

#### Step 2r Checkpoint: Operand-Aware Unsigned Word Comparisons

Extend the existing word-comparison emitter, reusing its location resolution,
byte materialization, IX address/load helpers and unsigned jump tail. With RA
enabled and force-all-spill disabled, compare a retained HL/BC unsigned word
against a frame/global word or representable constant using low-byte `SUB`,
high-byte `SBC` and one carry decision. Reverse subtraction for `>` and `<=`
instead of testing equality separately. Two retained word registers use this
chain only when removing the extra branch makes the complete sequence smaller;
otherwise retain the smaller original `SBC HL,BC`.

An indexed operand's load/subtract setup falls from **9 bytes / 57 T-states**
to **8 bytes / 46 T-states**, before the branch. Equality still uses full-word
Z, and signed comparisons retain full-word overflow/sign handling. Signed or
widened operands and unsupported locations keep their original paths. Constant
eligibility is limited to **-32768 through 65535**: an initially overbroad
prototype silently truncated invalid literals, so reject that prototype and
preserve the parent's linker diagnostic for out-of-range operands. All valid
qualification images and metrics are identical before and after this guard.
The existing full clobber contract, DE/frame handling and public ABI remain
unchanged. No production function, pass or cohort is added; production C grows
by **47 net lines** within the existing comparison function.

Keep the existing absolute `JP` helpers. No proven-range shortening pass or
unselected size/speed tradeoff is introduced: `JP` takes 10 T-states, whereas
unconditional/taken `JR` takes 12. Existing control tests now execute mapped
NOP padding and verify forward/backward hypothetical relative displacements
**-129, -128, 127 and 128**, retaining safe absolute branches at every boundary.

Against qualified parent `0c8b224`, the official RA comparison improves:

| Workload | Kernel bytes before/after | Kernel T-states before/after | Observed whole-call frame bytes before/after |
| --- | ---: | ---: | ---: |
| Stencil calculation | 1693 / 1691 | 4267798 / 4256974 | 120 / 120 |
| Pathfinding calculation | 1385 / 1384 | 5661993 / 5647913 | 187 / 187 |
| PackBits | 565 / 547 | 159912 / 159268 | 39 / 39 |
| Packed CRC | 910 / 892 | 12026571 / 12023460 | 47 / 47 |

Cycles aggregate the unchanged per-workload comparison cases. Caller costs
remain **126 bytes / 14460 T-states** for each calculation, **151 / 13888** for
PackBits and **82 / 7718** for packed CRC. All 26 SSC RA/no-RA rows are
non-regressing in all eight size/cycle/stack/frame metrics; all no-RA rows
remain unchanged. Whole used calculation ROMs fall **20625 to 20607** and
**31455 to 31439** bytes; padded images remain 65536 bytes.

The stock byte sum already uses step 2p's fused countdown and stays at
**99 bytes / 76126 kernel T-states**, with a 24-byte observed frame footprint.
Do not undo that fusion to manufacture a comparison win. A separate paired-byte
sum, outside the legal straight-line fusion shape, improves **167 to 166 bytes**
and **1845042 to 1811690 aggregate kernel T-states** over 32 independent cases.
It sums complete pairs and ignores an unmatched last byte, so its totals are
not directly comparable with the stock sum. All five modes pass full-buffer,
guard and SP/DE checks; frame reads/writes stay **51249 / 15185**.

Reuse the existing comparison families and condition-test methods rather than
adding a generator or oracle. Cover all relations and operand orders, computed
retained words, two-register comparisons, pressure, far frames, signed/equality
fallbacks and carry/borrow edges. Each full condition run executes **57600
relational/Boolean cases**. Three existing 44-function comparison matrices pass
**11264 calls per reported mode/matrix** on the saved parent and final traced
build, plus **704** on release/ASan. Every focused function is size/cycle/frame
non-regressing; byte, signed and equality fallback assembly is unchanged.
Default, all-spill and no-RA focused ROMs match the parent. A separate 12-function
two-register probe passes **768 edge calls per mode/version**, with four
profitable selections. Four invalid-literal cases require the standard
`Out of 16-bit range.` linker rejection on both operand sides, not truncation.

Final release/traced/ASan qualification passes all **175 fixtures without
snapshot updates**, 16 immutable allocator cohorts, 28 real routines,
90 traced/ASan allocator image identities, 300 condition image identities
across all three builds and strict normal/traced C89 compilation. The full
comparison passes **104 builds/1856 calls** with unchanged corpus fingerprints
and all **78 external controls**. There are **40 independent timing checks**,
including **16 mixed-version builds/292 guarded calls** in both caller/callee
directions. All 52 frozen source hashes match the qualified sources.
The separately documented escaped-before-initialization counter defect still
returns 42740 rather than 50814 on both parent and current compiler; it is not
counted as passing.

Saved parent binaries, frozen sources, rejected prototypes and final guarded
qualification are retained in `C:\cygwin64\tmp\word-comparisons-20261020`.
Commit locally without pushing. Step 2s is next before the initial SM83 backend.

#### 2s. Shared Arithmetic Bodies With An Explicit Size/Speed Policy

- [x] Measure repeated inline multiply/divide/modulo bodies in standalone and
  combined programs; the array workload currently contains five such bodies.
  Reuse the existing arithmetic-body emitters and linker helper-generation
  patterns before adding runtime entry points or another abstraction.
- [x] Emit shared bank-local helpers only where their complete cost is
  justified. Include setup, `CALL`/`RET`, result moves, DE preservation,
  clobbers, stack footprint and bank placement; count shared code once.
- [x] Define and document the size/speed choice before changing behavior.
  Keep inlining where faster or sharing does not pay. Do not silently slow the
  existing default, replace the immutable comparison modes, or change the
  public C ABI. Add distinct benchmark modes if a new profile is introduced.

Acceptance: report per-workload and combined-ROM bytes/cycles, helper counts,
call frequency, break-even points and any explicitly accepted slowdown. Check
single/repeated uses, byte/word and signed boundaries, quotient/remainder reuse,
defined zero-divisor behavior, nested/banked calls and full SP/DE restoration.
Retain the existing good constant arithmetic paths and full TAC contracts.

**Step 2s checkpoint: approved opt-in arithmetic sharing**

The measured prototype saved 68 bytes in the array kernel but added 74,520
T-states and two stack bytes. The user explicitly selected a separate opt-in
size profile rather than slowing the existing default. The linker now accepts
`-share-arithmetic`, which must precede `-o`:

```text
samesamecl -lsms -share-arithmetic -o game.sms main.asm
```

Ordinary linking remains inline. The compiler only adds begin/end comments to
the four existing generic arithmetic emitters; removing those comments restores
the parent emitter source exactly. The existing combined-assembly writer groups
byte-identical marked bodies by kind, ROM bank and slot, leaving single uses,
RAM bodies and unmarked/unsupported code inline. It emits one leaf helper per
repeated group and replaces each body with `CALL`, retaining all operand setup,
result moves and outer DE preservation. No new production function or second
arithmetic implementation was added: production C grows by **121 net lines**.

For a body of B bytes used N times, complete body/helper storage changes from
`N*B` to `3*N+B+1`, including every CALL and the helper RET. The four bodies are
11/15/35/16 bytes (byte multiply, word multiply, word divide/modulo, byte
divide/modulo); their helpers are 12/16/36/17 bytes. **Two uses break even
profitably for all four**, so single uses remain inline. Every actual helper
invocation costs exactly **27 additional T-states** and **two stack bytes**;
there is no hidden setup saving or new register-argument ABI.

Helpers are emitted in the caller's existing bank/slot, without bank switching.
Their dotted private names cannot collide with C identifiers. Leading
underscores were rejected during qualification because WLA makes such labels
section-local. Existing constant-specialized paths, adjacent DIV/MOD remainder
reuse, TAC clobbers, aliases and DE/IX/IY/frame contracts remain unchanged.
Signed boundary controls preserve the parent's existing divider bit-pattern
semantics, including its defined zero-divisor behavior; this is not a new
signed-division implementation.

The benchmark retains its original eight default modes and immutable gates.
Opt-in comparisons require their ordinary partner in the same invocation:

```text
--modes ssc-ra ssc-no-ra ssc-ra-shared ssc-no-ra-shared
```

Shared-mode gates require non-growing kernel/dependency bytes, identical caller
bytes/cycles, nonnegative cycle deltas divisible by 27, and at most two extra
stack/frame bytes. The report includes helper bytes, static call sites and
dynamic calls. No ordinary regression allowance was relaxed.

Qualified standalone RA results over the unchanged guarded corpus:

| Kernel | Inline/shared bytes | Inline/shared kernel T-states | Inline/shared frame bytes | Helpers / static sites / actual calls |
| --- | ---: | ---: | ---: | ---: |
| Array | 1605 / 1537 | 6241578 / 6316098 | 88 / 90 | 2 / 5 / 2760 |
| Stencil | 1691 / 1655 | 4256974 / 4286782 | 120 / 122 | 2 / 4 / 1104 |
| Pathfinding | 1384 / 1376 | 5647913 / 5685713 | 187 / 189 | 1 / 2 / 1400 |

Other RA rows and all caller costs are unchanged. All **104 ordinary comparison
builds/1856 calls**, their eight cost metrics and the **78 external controls**
remain identical to parent `4211174`.

The combined finite 13-kernel program reuses the existing functions, wrappers
and 232-case corpus in one bank. Complete used ROM counts helpers, callers,
data and system overhead once:

| Mode | Inline/shared used ROM | Inline/shared kernel T-states | Inline/shared peak frame |
| --- | ---: | ---: | ---: |
| RA | 8579 / 8443 | 30622577 / 30767540 | 187 / 189 |
| No RA | 11099 / 10963 | 52004913 / 52149876 | 193 / 195 |

Each mode saves **136 bytes**, with two helpers and nine static sites.
Instrumented execution observes **5369 actual calls**, accounting exactly for
the **144963 extra T-states** and two additional stack reads/writes per call.
Caller totals remain 165891 T-states (RA) and 168603 (no RA). This is a bounded
calculation corpus, not a cycle measurement of the projects' infinite UI loops.

Complete real-project ROM builds also improve:

| Project | Inline/shared used ROM | Saving | Helpers / static sites |
| --- | ---: | ---: | ---: |
| calculations-1 | 20607 / 20587 | 20 bytes | 1 / 3 |
| calculations-2 | 31439 / 31171 | 268 bytes | 6 / 22 |

The latter helpers are bank-local across banks 0, 1, 2 and 3; both default and
shared full-project images agree between release and traced builds.

Release, traced and ASan condition/policy and paired-profile suites pass.
New persistent arithmetic tests reuse existing generators/oracles for **64000
numerical executions per build**; banked cross-file, nested and recursive
controls add **672 guarded calls**. Five allocator modes retain single-use ROM
identity, and six malformed-marker cases must fail explicitly. The isolated
fixture suite passes **175/175**, normal/traced strict C89 checks pass, and
**372 condition images** agree across builds, with the original 300 still
parent-identical. Traced/ASan agree on **90 allocator images**, including the
16 immutable-verifier cohorts and 28 real-routine controls.

Additional qualification covers **32 mixed-version builds/592 guarded calls**
in both caller/callee directions, **15 signed-word builds/15210 calls**, and
the combined program's **4 builds/928 calls**. There are **276 final independent
timing checks**, not just emulator cycle estimates. All **62 frozen source
hashes** match the qualified files. The known escaped-before-initialization
counter defect remains 42740 rather than 50814 on parent and current compiler;
it is not counted as passing.

Qualification crossed midnight. Ten allocator project images and two whole-ROM
controls differed from the preserved parent only at SDSC build-day byte
`0x7fe6` and derived SMS checksum byte `0x7ffa`. Reassembling the parent's
unchanged combined assembly with the pinned tools on the same day reproduced
each complete default ROM exactly. Original parent artifacts remain untouched;
no bytes were masked and no parent baseline was refreshed.

Sources, parent binaries, rejected prototypes, same-day controls, guarded
results and the consolidated qualification are retained in
`C:\cygwin64\tmp\arithmetic-sharing-20261020`.
Commit locally without pushing. The finite Z80 sequence through 2s is complete;
the next implementation target is the initial SM83 backend.

Register arguments/results remain a separate explicit, versioned or opt-in ABI
investigation under priority 4 below, not a sixth mandatory pre-SM83 step.
The measured 126/729 versus 20/114 wrapper gap justifies further study, not
silent caller/callee changes or removing necessary frame saves. Address the
existing-ABI opportunities in 2o-2r first; any later ABI proposal needs dedicated
mixed-version, cross-file, banked, recursive and assembly-facing compatibility
tests and documentation.

#### Post-2s checkpoint: unsigned power-of-two arithmetic [DONE]

Complete the previously measured unsigned division/remainder improvement
without adding another mandatory pre-SM83 step. Reuse
`turn_some_muls_and_divs_into_shifts()` in `compiler/pass_5.c`: exact positive
integer divisors **1 through 32768** become logical shifts for division and
`divisor - 1` masks for remainder when both existing promoted operand types
prove unsigned. This includes `/1` and `%1`; evaluating an operand's call or
increment must still happen exactly once.

The existing small multiply/divide shifts remain in their early position.
Only the new unsigned folds run after `optimize_il()`, through the same
function and its updated internal declaration. A rejected early placement
interacted with negative unsigned constant propagation. Parent/current
diagnostic controls now preserve that existing constant-expression handling;
the pre-existing negative constant-conversion defects are not numerical
oracle passes. No new type pass, AST resolver, emitter, production function,
ABI or arithmetic-helper protocol was added: **11 net production C lines**,
plus a one-line internal header signature change.

Signed-promoted operands, negative/non-power divisors and fractional literals
retain their arithmetic fallback. In particular, directly dividing a byte
by a large signed-promoting literal is not treated as proof of unsigned word
arithmetic; an explicitly widened unsigned word is covered. Exact literal
matching avoids the old cast-based classification of fractions as powers.
Existing accepted fractional-literal fallback behavior remains accepted.
Literal zero still fails compilation; `65536` and `-65536` still reach the
assembler and fail there, rather than becoming a valid-looking shift/mask.

Measured complete kernel/dependency code and kernel T-state totals over the
same **77 edge/pseudorandom inputs**, with allocation enabled:

| Expression | Parent/current code bytes | Parent/current kernel T-states | Parent/current frame footprint |
| --- | ---: | ---: | ---: |
| `u16 % 16` | 68 / 36 | 95891 / 14784 | 24 / 22 |
| `u16 / 256` | 70 / 31 | 97443 / 13321 | 24 / 22 |
| `u16 % 256` | 68 / 36 | 96827 / 14784 | 24 / 22 |

Allocation-disabled results also improve: current code is **48 / 43 / 48
bytes**, respectively, versus **80 / 80 / 80**. Current kernel totals are
**20636 / 19173 / 20636 T-states**, versus **101743 / 102679 / 102679**;
frame footprint decreases from 26 to 24 bytes. All callers remain exactly
**126 bytes / 729 T-states per call**. These measurements cover 12
parent/current builds, 924 guarded calls and 12 independent timing checks.

The two added persistent tests reuse the existing branch-condition test class,
five-mode generator and guarded executor. Each release/traced/ASan run checks
**110190 unsigned arithmetic executions** over byte/word/widened/truncated
contexts, all 16 divisor powers, reconstruction and sequenced side effects.
Another **660 executions** check signed, negative, non-power and fractional
fallbacks, with six explicit compiler/linker rejection controls. Seven
supported fallback routines retain parent-identical complete ROMs and assembly
in all five allocator modes.

All three configurations pass **33 condition tests** and **21 optimized-policy
tests**. The isolated fixture suite passes **175/175** without snapshot
updates; normal/traced strict C89 checks pass. All **392 condition images**
agree across builds, with the original 372 parent-identical. Traced/ASan agree
on **90 allocator images**, including the existing immutable-verifier cohorts
and real-routine controls. The known escaped-before-initialization counter
defect remains unchanged and is not counted as passing.

The original 13-kernel corpus does not exercise these new constant forms:
all **104 ordinary comparison rows / 1856 calls**, including the 78 external
compiler controls, remain exactly unchanged. Traced and ASan each reproduce
the prior **52 paired sharing rows / 928 calls**. There are **148 independent
timing checks** across the final comparison and focused measurement runs.
All 62 frozen source hashes match; no benchmark baseline was refreshed.

Complete default/shared project ROMs remain parent-identical in all three
configurations: **20607 / 20587** used bytes for calculations-1 and
**31439 / 31171** for calculations-2. Do not claim these focused arithmetic
gains as an improvement in those unchanged whole-project figures.

Sources, parent binaries, rejected early placement, constant/literal controls,
full images, numerical results and consolidated qualification are retained in
`C:\cygwin64\tmp\unsigned-powers-20261021`.
Commit this qualified batch locally without pushing. The next finite-roadmap
implementation target remains the initial SM83 backend; further Z80
architecture investigations are separate user-requested work.

#### 2t. Improve Scalar/Address Residency Within The Existing ABI [IN PROGRESS]

User-requested extension after the unsigned power-of-two checkpoint. Prioritize
the remaining frame traffic rather than accumulating arithmetic peepholes.
The qualified research at `fcd36fc` attributes these own-function costs:

| Workload | SameSameC / stock SDCC own bytes | SameSameC frame setup + indexed accesses |
| --- | ---: | ---: |
| Array | 1292 / 516 | 762 |
| Stencil | 1513 / 868 | 966 |
| Tile | 1209 / 688 | 867 |
| Path | 1265 / 737 | 840 |
| Packed CRC | 345 / 120 | 140 |

These exclude wrappers, callees and runtime libraries. Indexed accesses include
necessary array work, so the columns are budgets, not removable-byte promises.
Dynamic tracing separates local-array data from scalar/frame accesses: Tile
spends 66.0% of its kernel cycles in frame-address setup and scalar-frame-access
instructions despite having no internal calls.

- Reuse existing copy propagation, local promotion, CFG/join reconciliation,
  location resolution and address emitters. Do not introduce another allocator
  or a parallel expression/address framework.
- Fold stable single-use pointer/address copy chains into their consumers and
  rematerialize cheap addresses instead of spilling them where proven cheaper.
- Extend scalar/address residency through loops and CFG joins only when every
  incoming path proves the same value/base; retain conservative call, alias,
  inline-assembly and frame/register-clobber barriers.
- Preserve evaluation order, declared-width conversions, signed extension,
  pointer wraparound and byte/pair overlap. Do not blindly narrow locals:
  the private byte-counter probe made CRC 15 bytes larger and 25.7% slower,
  while Tile shrank 66 bytes but became 11.0% slower.

Acceptance: regressions covering the folded chain and rejected aliases,
conversions, side effects and control-flow cases; non-growing complete
kernel/caller/frame costs and a measured real-workload size/speed improvement.
Run all allocator modes, immutable verification, pinned compiler controls and
full calculation ROMs. Keep source growth visible and reuse or remove existing
logic rather than adding overlapping special cases.

**First implementation checkpoint: spilled pointer-store copies.** Reuse the
existing final allocation metadata and array-write emitter to fold an adjacent,
single-definition/single-use, actually spilled word-pointer snapshot into its
store. Skip only dead TACs; require the same function and original pointer node,
and reject retained bases, spill-transfer boundaries and special I/O labels.
Keep useful BC/HL pointer retention, no-RA/all-spill lowering, the allocated TAC
and its frame slots unchanged. This adds 32 production C lines and no functions;
it is not a second copy-propagation or address-emission framework.

Against `fcd36fc`, complete isolated bodies including their live dependencies:

| Workload | Bytes before -> after | Kernel T-states before -> after (20 calls) |
| --- | ---: | ---: |
| Array | 1605 -> 1569 | 6241578 -> 6164298 (1.24% less) |
| Stencil | 1691 -> 1655 | 4256974 -> 4218334 (0.91% less) |
| Tile | 1209 -> 1191 | 2049176 -> 2003606 (2.22% less) |

Their callers remain 126 bytes / 14460 T-states over those 20 calls, and frame
footprints remain 88/120/128 bytes respectively. The other ten measured
workloads, all 13 no-RA rows and all 78 pinned external controls are unchanged.
Do not sum isolated reductions and present them as whole-program savings:

| Complete project | Default used ROM before -> after | Shared used ROM before -> after |
| --- | ---: | ---: |
| calculations-1 | 20607 -> 20607 | 20587 -> 20587 |
| calculations-2 | 31439 -> 31115 | 31171 -> 30847 |

Release/traced/ASan qualification covers 34 condition/policy tests plus 21
optimized-policy tests per build, including 2925 pointer executions per build
with signed/unsigned byte/word elements, negative indices, local/global pointer
objects, aliases, calls and sequenced side effects. Check both positive folding
and retained-base/no-RA/all-spill rejection. Run immutable allocation and real
cohorts, 175 isolated fixtures without snapshot updates, the original 104-row /
1856-call comparison and paired shared profiles. All pass, including 136
independent timing checks; 402 condition images and 90 allocator images match
across builds, and all 392 pre-existing condition images match the parent.
Evidence is retained in
`C:\cygwin64\tmp\residency-2t-20261021`.

**Second implementation checkpoint: word-sized load locals.** Extend the
existing named-local promotion to scalar word locals initialized by array
reads. Preserve the declared local width and signedness rather than inheriting
the element type; retain conservative byte-local narrowing, escape, alias,
call and multiple-definition checks. Reuse existing BC candidate selection and
word-read result emission. Recognize the existing in-place frame-word increment
as preserving general registers only for the exact local/argument, width and
unallocated-operand shape; globals, statics and other arithmetic keep their
existing clobbers. No new emitter or production functions are needed: this
checkpoint adds 22 net production C lines.

The existing frame-boundary index-copy cohort exposed a speculative eviction
assumption: a nearer word candidate can choose the other register pair instead
of displacing the live value. Reuse endpoint legality to keep checking the
remaining path when either pair is possible. The immutable verifier initially
rejected this case and remains unchanged; do not bypass its rejection.

Against the first checkpoint at `c488078`, complete isolated bodies and live
dependencies, with kernel timings over the same 20 calls:

| Workload | Bytes before -> after | Kernel T-states before -> after | Maximum frame footprint |
| --- | ---: | ---: | ---: |
| Array | 1569 -> 1559 | 6164298 -> 6147298 (0.28% less) | 88 -> 88 |
| Tile | 1191 -> 1181 | 2003606 -> 1990358 (0.66% less) | 128 -> 126 |
| Path | 1384 -> 1384 | 5647913 -> 5647913 | 187 -> 186 |

All three callers remain 126 bytes / 14460 T-states. The other ten workloads
and all 91 no-RA/pinned external control rows are unchanged. Shared profiles
retain the same size/cycle reductions and reduce Path's frame from 189 to 188.
Measure full projects separately rather than summing isolated improvements:

| Complete project | Default used ROM before -> after | Shared used ROM before -> after |
| --- | ---: | ---: |
| calculations-1 | 20607 -> 20607 | 20587 -> 20587 |
| calculations-2 | 31115 -> 31091 | 30847 -> 30823 |

Release/traced/ASan qualification covers 35 condition/policy tests and 21
optimized-policy tests per build. The new 88-function load regression executes
19800 independent return/memory/port oracles per build across all five modes:
signed/unsigned byte/word elements, word-local conversions, negative indices,
increments, aliases, calls, loops, escapes, redefinitions and byte narrowing.
Assert actual spill-free BC selection as well as conservative fallbacks.
Expand the independent BC endpoint expectations for the newly supported loads;
10200 endpoint checks, 65 frame-increment controls and all existing 32 immutable
rejections / seven accepts pass in normal and quiet modes.

Rerun all qualification after fixing the speculative eviction: 175 isolated
fixtures without snapshot updates, the 104-row / 1856-call comparison, paired
shared profiles, 136 independent timing checks and 2320 non-growing individual
SSC samples. All 412 condition images and 90 allocator images match across
builds, as do the complete default/shared ROMs. Evidence, including the rejected
prototypes and verifier diagnostics, is retained in
`C:\cygwin64\tmp\scalar-loads-2t-20261021`.

**Third investigation checkpoint: loop-carried word locals [NO-GO].** Test
broader reuse of the existing scalar promotion and CFG/loop allocator against
the qualified parent at `54a0230`; do not enable a change merely because it
removes a named local. All trial production changes were reverted.

- Unrestricted promotion of repeated definitions admits locals reused across
  independent loops. The existing loop reservation planner correctly rejects
  consumers outside its reachable region: Array's reused index and Tile's
  reused inner-loop counter expose this limitation. Keep those checks intact;
  function-wide renaming is not a substitute for loop-scoped value ownership.
- Restricting promotion to two definitions avoids those compile failures but
  still disables existing advancing-loop lowering. A promoted checksum live
  across an unrelated loop is enough to trigger the emitter's conservative
  live-temp barrier. Stencil's frame shrinks by two bytes without a code or
  timing improvement; that does not justify the other regressions.
- Requiring one enclosing backedge, retaining the existing unit-counter
  policy, and allowing arithmetic/shift definitions promotes Tile's CRC, but
  does not reduce its bytes or cycles. A private final-metadata check can
  preserve unrelated advancing loops when a spanning temp is fully spilled
  and untouched. Even then, promoting the word accumulator itself defeats
  the existing advancing word-sum emitter.

Each of the three completed restricted variants ran the same 26 SSC/no-RA
rows and 464 oracle calls. The important rejected changes are:

| Rejected case | Bytes before -> after | Kernel T-states before -> after |
| --- | ---: | ---: |
| Path, without preserving its unrelated advancing loop | 1384 -> 1438 | 5647913 -> 6221633 (10.16% more) |
| sumBytes, even after preserving unrelated loops | 99 -> 98 | 76126 -> 220663 (189.87% more) |

These are isolated-kernel results, not whole-ROM measurements. Do not accept
the one-byte sum reduction, the frame-only improvement, or unqualified
promotion in exchange for those losses. No new production code, functions,
verifier relaxation or snapshot updates are retained from this investigation.
Trial sources, reports and rejection diagnostics are retained in
`C:\cygwin64\tmp\loop-values-2t-20261021`.

Before extending promotion again, reuse the existing loop/join ownership
analysis to separate values belonging to independent loops, preserve the
existing advancing sum/copy/fill lowering, and prove that arithmetic locals
actually stay in registers across the relevant clobbers. Cost promotion
against the complete specialized loop, not just the removed frame store.
Keep these as 2t prerequisites rather than moving the unresolved traffic into
an ungated ABI or frame-layout change.

**Fourth implementation checkpoint: proven forward-join frame addresses.**
Reuse the emitter's existing IX frame-offset state instead of extending scalar
promotion or adding another CFG/address framework. Remember the latest emitted
conditional and unconditional branch states. At a label, retain the known
offset only when the current fallthrough state and every explicit incoming
edge prove the same frame address. An unrecorded edge, backward edge or
conflicting offset keeps the old invalidation. Disable this extension for
functions containing inline assembly and for default/no-RA/all-spill modes;
retain all existing call, frame and index-register clobber barriers.

The narrower shift/bitwise-local and BC-endpoint prototype made no improvement
in 26 SSC/no-RA rows / 464 calls and was removed completely. Tile's CRC is still
rejected by existing loop discovery because its common latch block does not
define the value. Do not weaken that ownership proof to force promotion.
The accepted alternative adds **31 net production C lines and no functions**;
there are no allocator-core, endpoint-policy, verifier or ABI changes.

Against `010b0a6`, the complete isolated kernels improve as follows:

| Kernel | Bytes before -> after | Kernel T-states before -> after |
| --- | ---: | ---: |
| Array | 1559 -> 1517 | 6147298 -> 6094692 (0.86% fewer) |
| Stencil | 1655 -> 1577 | 4218334 -> 4108018 (2.62% fewer) |
| Tile | 1181 -> 1145 | 1990358 -> 1914842 (3.79% fewer) |
| Path | 1384 -> 1354 | 5647913 -> 5539685 (1.92% fewer) |
| PackBits | 547 -> 535 | 159268 -> 151525 (4.86% fewer) |
| Packed CRC | 892 -> 874 | 12023460 -> 11404716 (5.15% fewer) |

Other kernels, callers and frame footprints do not grow. These are not summed
whole-ROM savings: measured calculations-1 used ROM falls from 20607 to
20437 bytes, and calculations-2 from 31091 to 30555. Shared-arithmetic ROMs
fall from 20587 to 20417 and from 30823 to 30287 respectively.

Add 30 independently modeled branch/frame functions and 10800 executions per
build across all five allocator modes. Cover small and large frame offsets,
nested/multiple incoming paths, backedges, break/continue, calls, aliases,
globals, early returns and an IX-clobbering assembly block. The parent passes
the numeric/memory oracles but fails the new exact selection assertion:
eligible diamonds require one IX materialization rather than three.
The existing immutable 32 rejection cases, seven acceptances, 10200 endpoint
checks and 65 frame-increment controls remain unchanged.

Qualification includes 36 condition/policy tests and 21 optimized-policy tests
per release/traced/ASan build, the
104-row / 1856-call pinned comparison, paired 52-row / 928-call shared profiles,
136 independent timing checks and 2320 non-growing individual SSC samples.
All 422 condition images, 90 allocator images and complete default/shared
ROMs match across builds. All 175 fixtures pass after seven byte vectors in
two fixtures receive only
absolute branch-target relocation updates because other functions shrink;
their source bodies and complete symbolic instruction streams are identical
to the parent, with every old/new vector checked against its corresponding
ROM. No instruction selection or immutable expectation is relaxed.
Evidence and source/commit bindings are retained in
`C:\cygwin64\tmp\shift-residency-2t-20261021`.

**Fifth implementation checkpoint: exact incoming-edge frame witnesses.**
Replace the two single-branch snapshots with bounded caches of eight exact
instruction/offset witnesses per branch kind. Reuse the same label scan and
frame-offset proof, and share conditional/unconditional recording rather than
duplicating it. This adds **four net production C lines and no functions**.
Every incoming edge must still match the current fallthrough frame base;
missing or evicted witnesses, conflicting offsets and backward edges retain
the old invalidation. Existing call, inline-assembly and clobber barriers,
default/no-RA/all-spill behavior, allocator policy and ABI remain unchanged.

Keep the caches separate: otherwise many conditional branches could evict an
unconditional witness that the previous implementation retained indefinitely.
The latest witness of each kind is always available, preserving the earlier
optimization while allowing joins with several recorded predecessors.
A smaller same-target branch-run prototype made no measured improvement and
was discarded; do not retain an extra recognition path for it.

Against `b924000`, complete isolated kernels improve as follows:

| Kernel | Bytes before -> after | Kernel T-states before -> after |
| --- | ---: | ---: |
| Array | 1517 -> 1505 | 6094692 -> 6092372 (0.04% fewer) |
| Stencil | 1577 -> 1553 | 4108018 -> 4055470 (1.28% fewer) |
| Tile | 1145 -> 1121 | 1914842 -> 1905388 (0.49% fewer) |
| Path | 1354 -> 1318 | 5539685 -> 5393409 (2.64% fewer) |
| PackBits | 535 -> 517 | 151525 -> 150974 (0.36% fewer) |
| Packed CRC | 874 -> 850 | 11404716 -> 11402744 (0.02% fewer) |

Other kernels, callers and frame footprints do not grow. Independently measured
whole-ROM used bytes fall from 20437 to 20413 for calculations-1 and from
30555 to 30315 for calculations-2. Shared-arithmetic ROMs fall from 20417 to
20393 and from 30287 to 30047 respectively; these are not summed kernel wins.

Extend the existing frame oracle to 51 functions / 18360 executions per build,
partitioned by frame size to fit the existing ROM-bank limit. Cover short
circuits, calls between edges, conflicting bases, the exact eight/nine-witness
eviction boundary and 17-branch cross-kind pressure. The parent passes the
numeric/memory oracles but fails the new short-circuit selection assertion:
two IX materializations instead of one. The ninth same-kind incoming edge
still forces conservative materialization; unrelated branch-kind pressure
does not discard the original single-edge proof.

Qualification uses all three builds, 36 condition/policy and 21 optimized-policy
tests per build, the unchanged immutable controls, the full pinned comparison,
paired shared profiles and complete default/shared ROMs. All 175 fixtures pass;
two byte vectors in one fixture need only absolute branch-target relocation
updates, with unchanged source bodies and symbolic instructions checked against
both ROMs. All 442 condition images and 90 allocator images match across builds,
and 2320 individual SSC samples are non-growing with 136 independent timing
checks. Evidence and source/commit bindings are retained in
`C:\cygwin64\tmp\repeated-joins-2t-20261021`.

**Sixth implementation checkpoint: actual fallthrough predecessors.**
Reuse the existing exact-edge cache, but do not merge in a nonexistent linear
predecessor after an emitted unconditional jump or void/value return.
Initialize the join state from the first real incoming witness, then require
every remaining predecessor to agree. Missing/evicted witnesses, backward
edges, unknown states and conflicting real predecessors remain conservative;
a later valid witness cannot override an earlier invalid one.

Track emitted chunks rather than the last consumed source TAC: folded pointer
stores and optimized byte loops have real fallthrough even when bulk lowering
consumes a source backedge. Preserve all existing call, assembly, frame and
register-clobber barriers and default/no-RA/all-spill behavior. This adds
**13 net production C lines and no functions**, without allocator-core,
policy, verifier, cache-capacity or ABI changes.

Against `040d9b9`, complete isolated kernels improve as follows:

| Kernel | Bytes before -> after | Kernel T-states before -> after |
| --- | ---: | ---: |
| PackBits | 517 -> 505 | 150974 -> 150684 (0.19% fewer) |
| Packed CRC | 850 -> 832 | 11402744 -> 11399786 (0.03% fewer) |

Other kernels, callers and frame footprints do not grow. Independently measured
whole-ROM used bytes fall from 20413 to 20395 for calculations-1 and from
30315 to 30279 for calculations-2. Shared-arithmetic ROMs fall from 20393 to
20375 and from 30047 to 30011 respectively; do not sum isolated kernel savings
and call them a ROM measurement.

Extend the existing frame oracle to 57 functions / 20520 executions per build
across all five allocator modes. Cover early void/value returns and eight/nine
incoming edges after a terminating return, while retaining short-circuit call,
alias, conflicting-base, pressure and assembly controls. The parent passes the
numeric/memory oracles but needs two IX materializations instead of one in the
return cases, and five instead of four in the call case. The ninth incoming
edge still forces conservative materialization, even without fallthrough.

Qualification passes 36 condition/policy and 21 optimized-policy tests per
release/traced/ASan build, unchanged immutable controls and all 175 fixtures
without snapshot updates. The 104-row / 1856-call pinned comparison, paired
52-row / 928-call shared profiles and 136 independent timing checks pass;
all 2320 individual SSC samples are non-growing. All 442 condition images, 90 allocator
images and complete default/shared ROMs match across builds.
Evidence and source/commit bindings are retained in
`C:\cygwin64\tmp\fallthrough-joins-2t-20261021`.

**2t remains in progress.** The accepted checkpoints remove redundant copy/load
traffic, proven forward-join frame setups and some backing storage, not general
loop/CFG scalar residency.
Continue with costed scalar/address residency across loops/joins and the
existing alias/clobber proofs before 2u.

#### 2u. Investigate An Indexed Frame And Allocatable DE [PLANNED]

After 2t, prototype an explicitly gated per-function frame strategy using the
existing target policy and location/address helpers. SameSameC reserves DE as
its frame pointer; SDCC can retain values in DE while using an indexed frame.
Repeated frame-address setup alone occupies 192 bytes in Tile and 354 in Path.

Measure whether persistent indexed addressing and another allocatable pair
outweigh index-register pressure, pointer operations and call/helper saves.
Audit flags, displacement limits, local-array addresses, prologue/epilogue,
frame restoration and every full operand/result clobber contract. Do not
silently change the public ABI or duplicate the emitter. If external contracts
must change, version/gate them and retain compatibility adapters.

Acceptance: compare complete bodies, callers, dependencies, cycles and frame
footprints against the qualified default; test mixed versions, cross-file,
banked, recursive and assembly-facing calls. Promote the prototype only after
measured net wins; otherwise record a bounded no-go result.

#### 2v. Reduce Measured Internal Call Transport [PLANNED]

Use the existing call/frame lowering after 2t/2u identify the residual cost.
Array's three internal transport sequences occupy 241 bytes, excluding
argument-expression evaluation and callee bodies. The three-argument wrapper
example is 126 bytes / 729 T-states versus stock SDCC's 20 / 114.

First remove redundant transport within the existing ABI. Investigate explicit
register arguments/results or narrowly costed inlining only where justified,
using SDCC/z88dk calling conventions as design evidence, not copied code.
Preserve argument evaluation, caller/callee saves, result lifetime and bank
boundaries. Public ABI changes require an explicit compatible convention.

Count retained shared helpers and every call site: the private Array inlining
probe saved 286 isolated bytes but grew the caller body 27 bytes; Stencil saved
170 isolated bytes but grew its body 8 bytes. These are not whole-program ROM
savings if the old helpers remain live elsewhere. Reject blanket inlining and
benchmark-only specializations.

Acceptance: measured complete-program ROM and hot-call improvements with
mixed-width, indirect, nested, recursive, cross-file, banked, mixed-version and
assembly-facing compatibility controls. Keep generic/default fallbacks and do
not amortize a local regression away in an aggregate.

The source provenance, exact 52-image ledger, 411 guarded research calls and
21 independent timing checks for these three steps are retained in
`C:\cygwin64\tmp\major-codegen-study-20261021`.

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

Steps 1, 2, 2a-2s and the unsigned power-of-two checkpoint are complete.
The user-requested extension is the bounded sequence **2t-2v**, with
**2t, scalar/address residency within the existing ABI**, currently in progress.
The initial SM83 backend remains the next backend target after this extension.
Keep the qualified Z80 behavior and shared core covered throughout; do not
expand this sequence into every possible Z80 optimization.
