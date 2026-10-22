# Register Allocator Implementation Record

**Status: the bounded Z80 register allocator v1 is complete and remains opt-in
via `-ra`. No mandatory implementation batches remain.** See
[Completed Z80 Scope And Optional Future Work](#completed-z80-scope-and-optional-future-work)
for current guarantees, limitations, qualification and maintenance duties.

The checked phases below record implementation on top of the virtual-temp
pipeline. Unchecked optimization and future-target items are optional extensions,
not prerequisites for completing this Z80 scope.

Bounded Z80 word-register leaf calls are now selected automatically by the
compiler/linker, including across compilation units. No source annotations,
`-ra` or additional optimization flags are required. This ABI optimization is
independent of physical register allocation, including `-no-ra` and all-spill
builds. The old `-z80-regcalls` spelling is accepted but redundant.
Old-ABI entries are omitted when all references are understood and converted;
assembly, older inputs and escaping addresses conservatively retain compatible
entries or ordinary calls. Optional `__z80_regcall` declarations remain useful
only as a stronger ABI contract: they can save additional adapter bytes around
opaque assembly, but forbid old-ABI references and require matching declarations.
Unsupported explicit contracts are errors, not silent fallbacks.
Scope, measured costs and compatibility
tradeoffs are recorded in
[Z80 code generation step 2y](Z80_CODEGEN_PLAN.md#2y-explicit-compatible-internal-register-calls-completed).
It does not change the allocator's general call-clobber or verifier contracts.

Word logical constant-lane simplification now applies in every Z80 mode.
Normal `-ra` additionally reuses bounded, identically typed dynamic shifts and
their masked counts using the existing IL rewrite pass. Scalar inputs must
remain stable; effect/control-flow barriers and a short live-range pressure
proof keep unsupported shapes unchanged. BC-result constant logical operations
reload their shared input into BC, preserving an unrelated HL owner.
No production helper functions, flags or annotations were added. Tile improves
1041 -> 1001 bytes and about 3% in aggregate cycles without growing its frame,
but needs two extra spill-byte writes per encoding iteration. See
[step 2z](Z80_CODEGEN_PLAN.md#2z-constant-byte-lanes-and-bounded-pure-expression-reuse-completed)
for exact scope, full-image costs, tradeoffs and qualification.

Bounded word CRC loops now accept crossing temporaries only with a spill home,
no declared physical owner and no physically retained prefix through the loop
exit. Reuse the existing matcher/emitter; other loops and all type, alias,
transition, counter and control-flow guards retain their previous contracts.
Normal RA PackedCrc improves 784 -> 739 bytes and 11220929 -> 5635305 aggregate
T-states (-49.78%), with frame footprint 47 unchanged. The complete calculations-1
RA ROM saves 45 bytes; allocator-off output and calculations-2 are unchanged.
This adds 15 net production C lines and no functions. See
[step 2aa](Z80_CODEGEN_PLAN.md#2aa-frame-only-crossings-in-bounded-crc-loops-completed)
for scope, ownership exclusions, measurements and qualification.

Normal `-ra` now places bounded single-use `u16` division results directly in BC
for a word multiplication's ARG1, preserving HL across an already-shared
remainder store. It reuses existing result packing/materialization and one
extracted DIV/MOD proof; byte/mixed/signed division, conflicting schedules and
multiple reads retain their fallbacks. PartitionRestore improves 126 -> 116
bytes and 40052 -> 38896 aggregate T-states, with frame footprint 26 -> 24.
Its workload dependencies also improve; both complete RA applications save
10 bytes, with allocator-off output unchanged. See
[step 2ab](Z80_CODEGEN_PLAN.md#2ab-quotient-result-placement-completed)
for exact scope, caller/stack costs and qualification.

Automatically converted word-register microleaves now keep their BC/HL live-ins
and omit argument homes and frames in every allocator mode. The bounded shape
is a direct word return or one supported word arithmetic/logical operation,
with at most two arguments and no stack locals. Reuse existing ABI/location,
arithmetic and return machinery; helpers, branches, memory, pressure and legacy
requirements keep their existing framed paths. No flags or annotations are
needed. A two-argument addition shrinks 40 -> 2 callee bytes and 239 -> 21
callee T-states; caller-inclusive costs improve 87 -> 49 bytes and 514 -> 296
T-states, with stack high-water 17 -> 13 bytes. The existing 13-kernel profiles
and both complete applications are unchanged; these are bounded primitive wins.
See [step 2ac](Z80_CODEGEN_PLAN.md#2ac-register-live-in-frameless-microleaves-completed)
for exclusions, compatibility, source reuse and full qualification.

Bounded known-zero-bit queries now reuse existing types and single-definition
temporary metadata. One shared query enables the existing guarded DJNZ shift
loop for proven byte-sized word counts and avoids frame high-byte reloads in
the existing BC/index materializer. Source widths and allocator ownership are
unchanged; calls/control/memory barriers and unsupported signed/narrow shapes
retain conservative facts, while the older long-lived masked-count proof is
preserved. Over the focused normal-RA cohort, `a << (b & 31)` saves 33.09%
cycles without growing code/frame size; a pressured masked index saves one
byte and one frame-byte read per call. Existing kernel profiles are unchanged;
the complete calculations-1 RA image saves one byte, with no measured application
runtime gain. See
[step 2ad](Z80_CODEGEN_PLAN.md#2ad-bounded-nonliteral-known-bit-facts-completed)
for bounds, type/effect safeguards, costs and qualification.

Normal `-ra` now advances both source addresses in a bounded masked word/byte
checksum loop: a fixed `u16` array advances by two bytes and an invariant `u8`
pointer by one. The existing loop matcher, ownership/address proofs, counter,
logical emitter and final-counter publication are reused. DE preservation must
fit an already-dead spill home; no new frame slot, pushes or helper functions
are introduced. Unsupported types, control/calls, escapes and crossing owners
retain their existing paths. ArrayWorkload improves 1345 -> 1325 bytes and
5646400 -> 5552765 aggregate T-states (-1.66%), with frame footprint 86
unchanged. The complete calculations-2 RA image saves 20 bytes, including with
shared arithmetic; calculations-1 and allocator-off output are unchanged.
This adds 61 net production C lines. See
[step 2ae](Z80_CODEGEN_PLAN.md#2ae-costed-complex-scaled-induction-completed)
for complete setup/preservation costs, conservative exclusions and qualification.

Word-result multiplication now reuses bounded known-zero facts to process a
proven unsigned-byte factor in eight iterations, in either operand position.
The other operand and the product remain word-sized; constant chains, actual
byte-expression widths, signed extension, evaluation order and frame/ownership
contracts are preserved. Unsigned-byte array-read snapshots also feed the
existing BC materializer without weakening producer or effect barriers.
This adds nine net production C lines, no functions, flags or linker changes.
Normal RA Path improves 1241 -> 1238 bytes and 5323781 -> 4688981 aggregate
T-states (-11.92%), with frame footprint unchanged. Array saves another byte,
and allocator-off sumBytes saves one byte and 2.08% cycles.
The user approved keeping the faster specialized loops when old sharing groups
split: shared RA Path grows 5 bytes but becomes 12.55% faster.
Complete ordinary RA calculations-1/2 save 2/13 bytes; their shared RA images
grow 10/25 bytes. Existing sharing timing/traffic controls are unchanged.
See [step 2af](Z80_CODEGEN_PLAN.md#2af-proven-byte-factors-in-word-result-multiplication-completed)
for full costs, the precise initializer snapshot update, conservative fallbacks
and release/traced/ASan qualification. Steps 2ag-2aj record the next opportunities.

---

## 0. Original Baseline: Before Allocator Implementation

Historical starting context, not the current implementation state or tasks.

- `compiler/pass_4.c` generates monotonically increasing temps via `g_temp_r`
  during IL lowering.
- `compiler/pass_5.c::reuse_registers()` is a **virtual-temp lifetime
  compactor**, not a physical allocator.
- `compiler/pass_5.c::compress_register_names()` renumbers temps so the array
  in `struct local_variables.temp_registers` is dense.
- `compiler/pass_5.c::collect_and_preprocess_local_variables_inside_functions()`
  (around line 3595) lays out **every** local and **every** temp on the stack
  frame, computing `offset_to_fp` from a base of `-4 - return_value_size`.
- `compiler/defines.h::struct temp_register` currently has only
  `register_index`, `offset_to_fp`, `size`. No allocator metadata.
- `compiler/pass_6_z80.c::find_stack_offset()` (line 49) unconditionally
  returns a stack offset for any temp; every emitter assumes that.
- The Z80 backend reserves `DE` as a frame pointer and uses `IX`/`IY` as
  short-lived address-calculation scratch. `g_is_ix_de` caches the
  "`IX == DE`" relationship and is invalidated by many emitter sites.
- Function calls do **not** use the Z80 `CALL` instruction directly; the
  backend builds a custom frame with a `PUSH return_label / JP target`
  sequence. This is a hard register barrier.
- Inline asm (`compiler/inline_asm_z80.c`) reads/writes variables as `(@var)`;
  these accesses require memory-backed addressing and register-clobber barriers.
- Allocator regression tests are normal SMS makefile tests under
      `tests/z80/sms/allocator_*`. Each migrated test uses `byte_tester -s main.ssc`
      against tagged bytes in `linked.sms`; the temporary
      `tests/z80/sms/validations` harness has been removed.

---

## 1. Comment Contract (Freeze This First)

The allocator diagnostic comments are still useful when reading generated ASM,
but instruction regressions are now pinned by makefile-driven SMS tests using
`byte_tester`. Keep this debug comment format stable while byte tests cover the
actual ROM output.

- [x] Per-temp summary at function prologue:
      `; register <N> size <S> spill <yes|no> phy <NONE|A|HL|BC|C>`
- [x] Per-TAC retained-value marker on a producer:
      `; retained result in <A|HL|BC|C>`
- [x] Per-TAC retained-value marker on an `arg1` consumer:
      `; retained arg1 in <A|HL|BC|C>`
- [x] Per-TAC retained-value marker on an `arg2` consumer:
      `; retained arg2 in <A|HL|BC|C>`
- [x] Add a code comment in `pass_6_z80.c` pointing at
      `tests/z80/sms/allocator_*` so the format contract is discoverable.
- [x] Replace the temporary `rules.json` regex checks with normal SMS tests
      that use `byte_tester` tags and expected ROM bytes.

**Definition of done:** The makefile tests under `tests/z80/sms/allocator_*`
compile, link, and pass `byte_tester -s main.ssc`, and the diagnostic comments
remain consistent with the helpers added in Phase 4.

---

## 2. Feature Flag And All-Spill Mode (Phase 0)

The allocator must be landable in tiny increments without regressing the rest
of the compiler. Introduce a single switch first.

- [x] Add a global `g_allocator_enabled` (default `NO`) in `pass_6_z80.c` or a
      new `register_allocator_z80.c`.
- [x] Add CLI flags (`-ra` / `-ra-all-spill` / `-no-ra`) parsed in
      `compiler/main.c`.
- [x] When disabled, every code path must behave bit-identically to today.
- [x] When enabled but no temp has been marked retained, generated
      instructions must remain unchanged; allocator metadata comments may be
      added once Phase 3 lands.
- [x] Add forced all-spill validation so allocator metadata can be tested
      without retention.

Current implementation note: `-ra-all-spill` enables allocator metadata while
forcing every temp to remain stack-backed. `pass_5.c` prints
`register_allocator: forced_all_spill ...` and skips retention decisions, so
the prologue comments and spill-slot assignment path are exercised without
register-resident temps. The `allocator_ra_all_spill_mode` SMS test uses a
shape that normal `-ra` would retain, but asserts `phy NONE`, a real spill
slot, no retained-result comment, and pinned ROM bytes under `-ra-all-spill`.
The `allocator_no_ra_default_identity` SMS test compiles the same retained-temp
candidate with the default flags and with `-no-ra`, then compares generated ASM
and linked ROMs byte-for-byte, checks that disabled-mode logs do not contain
`register_allocator:` diagnostics, and pins the default ROM bytes with
`byte_tester`.

**Definition of done:** The compiler builds with the flag off and on, existing
flag-off tests still pass, and forced-all-spill `-ra` output differs only by
allocator metadata comments.

---

## 3. Allocator Metadata

- [x] Extend `struct temp_register` in `compiler/defines.h`:
      ```c
      int spill_required;     /* YES / NO */
      int physical_register;  /* Z80_PHY_NONE | Z80_PHY_A | Z80_PHY_HL */
      int original_register_index; /* pre-compression id, for diagnostics */
      int live_start;         /* first TAC index in function */
      int live_end;           /* last TAC index in function */
      int read_count;
      int write_count;
      ```
- [x] Add `Z80_PHY_NONE/A/HL` constants in `compiler/defines.h`.
- [x] Initialize new fields wherever `temp_registers` is realloc'd in
      `collect_and_preprocess_local_variables_inside_functions()`.
- [x] Propagate `original_register_index` through `compress_register_names()`
      and `reuse_registers()` (write-once, never overwritten by later renaming).
- [x] Preserve `original_register_index` across `tac_copy_arg()` and
      `tac_swap_args()` in `compiler/tac.c`.
- [x] Emit the per-temp `; register N size S spill ... phy ...` comments at
      function prologue from `pass_6_z80.c`.

**Definition of done:** With the flag on but all temps forced
`spill_required=YES, physical_register=NONE`, every function prologue carries
one comment per temp, and all existing tests still pass.

---

## 4. Pass 6 Location Abstraction (Before Touching `find_stack_offset`)

This is the load-bearing refactor. It must land **before** Phase 6 rejects
register-resident temps from `find_stack_offset()`, otherwise the build
breaks.

- [x] Add `struct z80_location { int kind; int offset; int phy; int size; ... }`
      with kinds: `LOC_CONST`, `LOC_GLOBAL`, `LOC_STACK_LOCAL`,
      `LOC_STACK_SPILL`, `LOC_PHY_A`, `LOC_PHY_HL`.
- [x] Add `_resolve_tac_location(t, which_operand, function_node, *out)` that
      returns a `z80_location` for `result`, `arg1`, or `arg2`.
- [x] Add materialize helpers: `_materialize_to_a`, `_materialize_to_hl`.
- [x] Add store helpers: `_store_from_a`, `_store_from_hl`.
- [x] Each helper must invalidate `g_is_ix_de` exactly when the existing
      open-coded path does, no more and no less.
- [x] Keep all signatures ANSI C90 (no `//`, no mixed decl/stmt, no `bool`).

Current implementation note: the location resolver and materialize/store helper
layer is in `compiler/pass_6_z80.c`. `_resolve_tac_location()` returns
`LOC_STACK_SPILL` for spilled temps and can now represent constants, globals,
stack locals, stack spills, `A`, and `HL`. Assignment byte tests verify the
helper paths for `GLOBAL <- CONST`, `GLOBAL <- STACK_LOCAL`, `GLOBAL <- GLOBAL`,
signed 8-to-16 extension, unsigned 8-to-16 extension, and 16-bit global copy.

**Definition of done:** `_resolve_tac_location` returns `LOC_STACK_SPILL` for
every temp when the flag is off, and the helpers produce byte-identical
assembly to the current open-coded loads/stores.

---

## 5. Incremental Emitter Migration

Migrate emitters to use the new helpers **while the allocator still forces
every temp to spill**. This isolates "refactor risk" from "allocator risk".

- [x] `_generate_asm_assignment_z80()`
- [x] `_generate_asm_add_z80()` / `_sub_z80()` (8-bit)
- [x] `_generate_asm_and_z80()` / `_or_z80()` / `_xor_z80()` (8-bit)
- [x] 16-bit variants of the above
- [x] `_generate_asm_complement_z80()`
- [x] Compare/jump lowering (`_generate_asm_jump_*`) for `arg1`/`arg2`
- [x] Return-value lowering
- [x] Function-call argument copying (operands only; the call itself stays a
      hard barrier)
- [x] Shifts now use the location abstraction. Mul/div/mod, get-address, and
      array-read result locations are migrated; their retained operands remain
      disabled for now.
- [x] Array-write and pointer ops: leave on the old path; mark TODO.
- [x] Inline asm: leave on the old path; mark TODO.

After each migration: run `./run_tests.sh`. Output must remain byte-identical
to the pre-refactor baseline.

Current implementation note: assignment, complement, compare/jump,
return-value lowering, function-call argument copying, and the shared
add/sub/and/or/xor helpers now resolve operands through the Z80 location
abstraction. The arithmetic, complement, compare/jump, return-value, and call
argument helpers keep the old IX/IY-based load order so existing byte tests
stay stable, while new location tests cover stack-local, global, constant,
stack-spill, and retained `A`/`HL` paths.

**Definition of done:** Every migrated emitter calls `_resolve_tac_location`
plus a materialize/store helper instead of calling `find_stack_offset()`
directly. Unmigrated emitters still work unchanged.

---

## 6. Tighten `find_stack_offset()`

- [x] Gate the new behavior behind `g_allocator_enabled`.
- [x] When the flag is on, return `FAILED` for any temp whose
      `physical_register != Z80_PHY_NONE`.
- [x] When the flag is off, behave exactly as today.
- [x] Print a diagnostic, not a silent corruption, on FAILED so any
      unmigrated emitter is caught loudly during development.

Current implementation note: the arithmetic, complement, and compare/jump
emitters migrated by the first allocation rule now check retained operands and
results before requesting stack offsets. This keeps the guard active without
breaking retained `A`/`HL` paths.

**Definition of done:** Forcing one temp to `physical_register = Z80_PHY_A`
and routing it through a migrated emitter produces correct code; routing it
through an unmigrated emitter aborts with a clear error.

---

## 7. Frame-Layout Split

- [x] Separate temp metadata initialization from temp spill-slot assignment
      in-place, so allocator decisions happen before frame space is consumed.
- [x] Split `collect_and_preprocess_local_variables_inside_functions()` into:
      - [x] `collect_local_variables_and_temps()` — discovery only.
      - [x] `assign_local_variable_offsets()` — locals always get a slot.
      - [x] `run_register_allocator()` — sets `spill_required` and
        `physical_register` per temp.
      - [x] `assign_spill_slot_offsets()` — only temps with
        `spill_required == YES` get an `offset_to_fp`.
- [x] Only temps with `spill_required == YES` consume temp spill-slot space.
- [x] Temps with `spill_required == NO` must have `offset_to_fp` set to a
      poison value (e.g. `INT_MIN`) so any accidental stack access crashes
      loudly.
- [x] In conservative mode (flag on, allocator returns all-spill), behavior
      must be identical to today.

Current implementation note: the named helper split is now in place. Temp
metadata initialization remains a small internal helper between local offset
assignment and allocator execution.

---

## 8. First Allocation Rule (Single-Use Forwarding)

The smallest useful rule. Picked specifically to satisfy the existing
`allocator_next_arg1_*`, `allocator_next_arg2_*`, `allocator_compare_arg*`,
and `allocator_complement_chain_*` validation cases.

- [x] Initial `-ra` implementation scans each function once and picks temps
      where **all** of the following hold:
  - [x] Defined exactly once.
  - [x] Read exactly once.
  - [x] Producer TAC is `ADD`, `SUB`, `AND`, `OR`, `XOR`, or `COMPLEMENT`.
  - [x] Consumer TAC is the immediately following TAC.
  - [x] No label, jump, call, return, inline asm, or unsupported emitter
        between producer and consumer (always true if "immediately
        following", but assert it).
  - [x] Producer and consumer are both already migrated in Phase 5 for the
        initial arg1/arg2 arithmetic, complement, and compare subset.
- [x] Set `physical_register = Z80_PHY_A` if `size == 1`, `Z80_PHY_HL` if
      `size == 2`. Skip anything else for now.
- [x] Emit `; retained result in A|HL` on the producer line.
- [x] Emit `; retained arg1 in A|HL` on the consumer line.
- [x] Emit `; retained arg2 in A|HL` on the consumer line.
- [x] Do **not** touch assignment producers yet — the existing
      `allocator_assignment_*` cases are negative and assert this.

Current implementation note: next-TAC `arg1` and commutative `arg2` arithmetic
consumers are enabled under `-ra`. Compare consumers are enabled for immediate
`TAC_OP_JUMP_*` consumers in both `arg1` and `arg2`; switch-case lowering is the
current test shape for immediate compare-arg2 coverage. Jump TAC type
propagation now fills missing promoted operand types from resolved operand types
so optimized compare TACs choose the correct 8-bit or 16-bit backend path.
Allocator debug output now prints candidate, skip, and retain decisions; the
swap-argument identity tests also trace the `optimize_for_inc()` TAC swap that
turns `1 + temp` into `temp + 1` before allocation.
Boundary negative tests use the same debug output to confirm immediate
`FUNCTION_CALL` and `RETURN_VALUE` consumers are skipped by the first rule.

**Definition of done:** All `allocator_next_*`, `allocator_compare_*`,
`allocator_complement_chain_*`, `allocator_boundary_*`,
`allocator_assignment_*`, and `allocator_swap_args_identity_*` cases pass
without modification.

---

## 9. Hard Boundaries (Enforce Conservatively)

- [x] Before each of the following, force `physical_register = Z80_PHY_NONE`
      on any temp still considered live:
      - [x] `TAC_OP_FUNCTION_CALL` / `..._USE_RETURN_VALUE` (custom frame)
      - [x] `TAC_OP_RETURN` / function epilogue
      - [x] Any label or jump target
      - [x] Inline asm blocks
      - [x] Unmigrated emitters (array-write and pointer ops)
      - [x] `mainmain` entry point special-case prologue
- [x] Emit an explanatory `; spill: <reason>` comment when a boundary forces
      a spill.

Current implementation note: `pass_5.c` now records `spill_reason` and
`spill_boundary_tac` on temps that are live at a hard boundary, forces those
temps back to `Z80_PHY_NONE`, and makes the first allocation rule skip them
with a debug line. `pass_6_z80.c` emits `; spill: <reason>` beside the temp
metadata comments. The `allocator_ra_boundary_spill_reasons_a` / `_hl` tests
verify byte output plus generated ASM comments for function-call, return, and
unmigrated-emitter spill reasons; label, inline-asm, and `mainmain` boundaries
use the same shared pass-5 enforcement path. After Phases 17 through 19
migrated mul/div/mod, get-address, and array-read results, these tests use
`ARRAY_WRITE` as the remaining unmigrated emitter barrier and grep
`compiler.log` for the exact boundary TAC.

---

## 10. Validation Case Map

All allocator regression tests should be normal SMS test directories with a
lowercase `makefile`, a `main.ssc`, and `byte_tester -s main.ssc` verification.

| Case directory                          | Current purpose                | Notes                                |
| --------------------------------------- | ------------------------------ | ------------------------------------ |
| `allocator_all_spill_metadata`          | negative under `-ra`           | unsupported array-read boundary keeps producer stack-backed |
| `allocator_no_ra_default_identity`      | negative under default / `-no-ra` | disabled allocator output remains byte-identical |
| `allocator_ra_all_spill_mode`           | positive under `-ra-all-spill` | allocator metadata with forced stack-backed temps |
| `allocator_ra_next_arg1_a` / `_hl`      | positive under `-ra`           | implemented first arg1 forwarding    |
| `allocator_ra_next_arg2_a` / `_hl`      | positive under `-ra`           | implemented commutative arg2 forwarding |
| `allocator_ra_compare_arg1_a` / `_hl`   | positive under `-ra`           | compare-to-zero jump consumer        |
| `allocator_ra_compare_arg2_a` / `_hl`   | positive under `-ra`           | switch-case compare consumer         |
| `allocator_ra_complement_chain_a`/`_hl` | positive under `-ra`           | complement producer forwarding       |
| `allocator_ra_swap_args_identity_a`/`_hl` | positive under `-ra`         | identity preserved across TAC swap   |
| `allocator_ra_assignment_chain_a` / `_hl` | negative under `-ra`         | assignment not yet a producer        |
| `allocator_ra_assignment_producer_a`/`_hl` | negative under `-ra`        | assignment not yet a producer        |
| `allocator_ra_boundary_call_argument_a` / `_hl` | negative under `-ra`  | function call blocks retention       |
| `allocator_ra_boundary_return_value_a` / `_hl` | negative under `-ra`   | return value blocks retention        |
| `allocator_ra_find_stack_guard_a` / `_hl` | positive under `-ra`    | retained temp bypasses stack-offset lookup |
| `allocator_ra_frame_no_spill_a` / `_hl` | positive under `-ra`     | retained temp removes frame spill slot |
| `allocator_ra_frame_spill_slot_a` / `_hl` | negative under `-ra`    | `ARRAY_READ` boundary forces spilled temp to consume a frame slot |
| `allocator_ra_assignment_locations_a`  | positive under `-ra`     | assignment helper paths for const, stack-local, and global 8-bit sources |
| `allocator_ra_assignment_extend_hl`    | positive under `-ra`     | assignment helper paths for 8-to-16 extension and 16-bit global copy |
| `allocator_ra_arithmetic_locations_a`  | positive under `-ra`     | 8-bit arithmetic helper paths for stack-local, global, const, and retained `A` |
| `allocator_ra_arithmetic_locations_hl` | positive under `-ra`     | 16-bit arithmetic helper paths for stack-local, global, const, and retained `HL` |
| `allocator_ra_complement_locations_a`  | positive under `-ra`     | 8-bit complement helper paths for stack-local, global, and retained `A` |
| `allocator_ra_complement_locations_hl` | positive under `-ra`     | 16-bit complement helper paths for stack-local, global, and retained `HL` |
| `allocator_ra_compare_locations_a`     | positive under `-ra`     | 8-bit compare/jump helper paths for stack-local, global, const, and retained `A` |
| `allocator_ra_compare_locations_hl`    | positive under `-ra`     | 16-bit compare/jump helper paths for stack-local, global, const, and retained `HL` |
| `allocator_ra_return_locations_a`      | positive under `-ra`     | 8-bit return-value paths for const, stack-local, global, and stack-spill sources |
| `allocator_ra_return_locations_hl`     | positive under `-ra`     | 16-bit return-value paths for const, stack-local, global, stack-spill, and signed extension sources |
| `allocator_ra_call_arguments_a`        | positive under `-ra`     | 8-bit function-call argument paths for const, stack-local, global, and stack-spill sources |
| `allocator_ra_call_arguments_hl`       | positive under `-ra`     | 16-bit function-call argument paths for const, stack-local, global, stack-spill, and signed extension sources |
| `allocator_ra_boundary_spill_reasons_a` / `_hl` | positive under `-ra` | hard-boundary spill reason comments for call, return, and unmigrated emitters |
| `allocator_ra_basic_blocks_debug`      | positive under `-ra`     | Phase 11 basic-block splitting and next-use debug output |
| `allocator_ra_linear_scan_conflict_a` / `_hl` | positive under `-ra` | Phase 11 next-use lookahead, clobber-between skip, and nearer-temp retention |
| `allocator_ra_linear_scan_farther_next_use` | positive/negative under `-ra` | Phase 11 farthest-next-use spill conflict for `A`/`HL`, plus same-consumer clobber boundary |
| `allocator_ra_block_exit_spill_debug` | positive under `-ra` | Phase 11 explicit block-exit spill marking plus retained in-block control cases |
| `allocator_ra_bc_clobber_debug` | positive under `-ra` | Phase 12 conservative Z80 clobber descriptor audit for `BC` and non-`BC` TACs |
| `allocator_ra_bc_retention_hl_conflict` | positive under `-ra` | Phase 12 first `BC` retention path while `HL` remains live |
| `allocator_ra_cfg_edges_debug` | positive under `-ra` | Phase 13 CFG edge and live-in/live-out debug output for branch, jump, and fallthrough edges |
| `allocator_ra_cfg_loop_switch_debug` | positive under `-ra` | Phase 13 loop back-edge and switch CFG/liveness validation |
| `allocator_ra_temp_compaction_debug` | positive under `-ra` | Phase 14 `reuse_registers()` / `compress_register_names()` measurement debug |
| `allocator_ra_interval_reuse_after_spill` | positive under `-ra` | Phase 15 per-definition retained intervals for reused temp slots with earlier spill reasons |
| `allocator_ra_shift_locations` | positive under `-ra` | Phase 16 shifted results and shifted arg1 consumers retained in `A`/`HL` |
| `allocator_ra_mul_div_mod_results` | positive under `-ra` | Phase 17 `MUL`/`DIV`/`MOD` results retained in `A`/`HL` while operands stay stack-backed |
| `allocator_ra_get_address_pointer_bases` | positive under `-ra` | Phase 32/38 computed-index `GET_ADDRESS_ARRAY` pointer bases expose the pointer value as a retained `BC` temp while retaining the index in `HL` |
| `allocator_ra_get_address_computed_bases` | positive under `-ra` | Phase 37 constant-index `GET_ADDRESS_ARRAY arg1` pointer bases use an allocator-exposed temp retained in `HL` |
| `allocator_ra_get_address_bc_base_hl_index` | positive under `-ra` | Phase 38 focused `GET_ADDRESS_ARRAY arg1` pointer-base temp in `BC` plus computed `arg2` index in `HL` |
| `allocator_ra_array_read_pointer_bases` | positive under `-ra` | Phase 33/39 `ARRAY_READ` pointer bases keep 8-bit indexes in `A` and retain 16-bit-index pointer bases in `BC` while the index stays in `HL` |
| `allocator_ra_array_read_computed_bases` | positive under `-ra` | Phase 34 computed `ARRAY_READ arg1` bases stay retained in `HL` through struct-field reads |
| `allocator_ra_array_read_bc_base_hl_index` | positive under `-ra` | Phase 39 focused `ARRAY_READ arg1` pointer-base temp in `BC` plus computed `arg2` index in `HL` |
| `allocator_ra_array_write_bc_base_hl_index` | positive under `-ra` | Phase 40 focused `ARRAY_WRITE result` pointer-base temp in `BC` plus computed `arg2` index in `HL` |
| `allocator_ra_inline_asm_barrier` | positive under `-ra` | Phase 41 inline-asm variable operands resolve through `z80_location` as stack locations while asm remains a hard clobber barrier |
| `allocator_ra_array_init_copy_location` | positive under `-ra` | Phase 42 local array initializer bulk-copy targets resolve through `z80_location` instead of direct stack-offset lookup |
| `allocator_ra_c_retention_a_conflict` | positive under `-ra` | Phase 43 first clobber-aware `C` retention path for 8-bit arithmetic temps that cannot safely stay in `A` |
| `allocator_ra_b_retention_c_conflict` | positive under `-ra` | Phase 44 first clobber-aware `B` retention path when an 8-bit arithmetic temp cannot stay in `A` and `C` is already occupied |
| `allocator_ra_index_c_retention_a_conflict` | positive under `-ra` | Phase 45 first clobber-aware `C` retention path for an 8-bit array/index operand while a store value remains in `A` |
| `allocator_ra_z80_out_c_retention_a_conflict` | positive under `-ra` | Phase 46 retained `C` Z80 output-port operand while the output value remains in `A`, with dedicated pass-6 port/value debug |
| `allocator_ra_pointer_array_smoke` | positive under `-ra` | Phase 47 combined pointer-to-pointer read/write, pointer-array read/write, retained pointer value, and retained pointer index/value coverage |
| `allocator_ra_struct_array_fields` | positive under `-ra` | Phase 48 struct-array field address helper debug plus retained `HL` array-read/write bases |
| `allocator_ra_struct_pointer_fields` | positive under `-ra` | Phase 49 global struct-pointer root plus nested pointer-member dereference coverage, with retained `HL` indirect field bases |
| `allocator_ra_struct_deep_pointer_fields` | positive under `-ra` | Phase 50 two-hop global struct-pointer chain coverage, with `depth`/`member` indirect debug and retained `HL` through both pointer loads |
| `allocator_ra_struct_pointer_increment` | positive/boundary under `-ra` | Phase 51 struct-pointer field increment/decrement helper coverage: retained update values plus explicit multi-read address spills |
| `allocator_ra_split_spill_hl` | positive under `-ra` | Phase 52 first split-spill insertion for retained `HL` array-read bases with later reads, including computed struct-array updates |

No Phase 8 validation cases remain in the future bucket. New coverage should
now be added with Phase 6+ behavior.

- [x] Move the implemented allocator cases out of the temporary validation
      harness and into makefile-driven SMS tests.
- [x] Verify the migrated cases with `byte_tester` against exact ROM bytes.
- [x] After Phase 8, add the remaining future cases above and confirm each case matches
      its expected verdict.

---

## 11. Extend To Linear Scan Inside Basic Blocks

- [x] Split each function's TAC range into basic blocks at labels, jumps,
      returns, calls, inline asm, and unmigrated emitters.
- [x] Compute next-use distance for each temp inside a block.
- [x] Allocate `A` for 8-bit temps and `HL` for 16-bit temps using a simple
      linear scan; spill the temp with the farthest next use on conflict.
  - [x] Replace the immediate-only forwarding pass with a conservative
        block-local scan that looks ahead to the next read inside the current
        basic block.
  - [x] Keep only candidates whose path to the next read is transparent for
        the chosen physical register; skip and debug-print clobbering gaps.
      - [x] Validate a real farthest-next-use conflict where two safe live ranges
        compete for the same physical register.
- [x] Force every register-resident temp to spill at block exits.
- [x] Add one new validation case per new behavior, paired positive and
      boundary-negative.

Current implementation note: `pass_5.c` now runs the hard-boundary spill pass
after first marking live-out temps at label/jump block exits with
`spill_reason=block_exit`, then scans each function into basic-block ranges
under `-ra`. The new block-local scan subsumes the old immediate-only
forwarding rule for arithmetic/complement producers, tracks the next read inside
the block, and retains only paths that are transparent for `A` or `HL`. Debug
output prints `register_allocator: basic_block ...`,
`register_allocator: next_use ...`, `register_allocator: block_exit spill ...`,
`register_allocator: linear_scan retain ...`, and
`register_allocator: linear_scan skip ... reason=clobber_between`. The
`allocator_ra_basic_blocks_debug` SMS test captures compiler output in
`compiler.log`, greps for the block/next-use diagnostics, and pins the generated
ROM bytes with `byte_tester`. The `allocator_ra_linear_scan_conflict_a` and
`allocator_ra_linear_scan_conflict_hl` SMS tests validate next-use lookahead,
clobber rejection, and retaining the nearer safe temp. The
`allocator_ra_linear_scan_farther_next_use` SMS test validates a real competing
live-range conflict for both `A` and `HL`; compiler debug output shows the older
temp first retained, then spilled with `reason=farther_next_use`, while the
nearer temp remains retained through a block-local reused-temp interval. The
same test now includes boundary-negative same-consumer shapes for `A` and `HL`
that must stay at `reason=clobber_between` and must not emit a
`farther_next_use` spill for those functions. The
`allocator_ra_block_exit_spill_debug` SMS test validates explicit block-exit
spill diagnostics and preserves paired in-block retention checks for `A` and
`HL`.

---

## 12. Add `BC` After `A`/`HL` Is Stable

- [x] Audit every emitter for implicit `BC` clobbers (especially loop
      counters, `LDIR`/`LDDR`, shift helpers, mul/div helpers).
- [x] Add a per-emitter clobber descriptor table.
- [x] Allow `BC` for 16-bit temps only where the clobber table proves it is
      free across the TAC.
- [x] Model `B`/`C` subregister overlap with `BC` before allowing 8-bit `BC`
      sub-allocation.
- [x] Extend the comment contract: `phy BC` and `retained ... in BC`.
- [x] Add validation cases for `BC` retention.

Current implementation note: `pass_5.c` now has a conservative Z80 TAC
clobber descriptor table and a `BC`-specific query helper. Under `-ra` and
`DEBUG_PASS_5`, basic-block debug output now includes
`register_allocator: z80_clobber ... may_clobber_bc=<yes|no> clobbers=...`.
The table deliberately marks broad TAC classes conservatively when any backend
emitter path can touch `BC`; for example `ASSIGNMENT` is marked as a possible
`BC` clobber because signed 8-to-16 stores use `BC`, while `COMPLEMENT` is
tracked as `A|HL|IX|FLAGS` and `may_clobber_bc=no`. The
`allocator_ra_bc_clobber_debug` SMS test pins bytes with `byte_tester` and
greps `compiler.log` for `ADD`, signed `ASSIGNMENT`, `JUMP_EQ`, and the
non-`BC` `COMPLEMENT` descriptor case. `Z80_PHY_BC` and `LOC_PHY_BC` now flow
through the location resolver and materialize/store helpers. The first real
allocation into `BC` is deliberately narrow: a 16-bit `COMPLEMENT` result can
be generated directly in `BC` when the consumer is a safe assignment or
commutative 16-bit arg2 consumer, letting an existing `HL` temp remain live.
Debug output marks this with `linear_scan bc_transparent_candidate`,
`linear_scan alternate ... primary=HL alternate=BC`, and
`linear_scan retain ... in BC`. The
`allocator_ra_bc_retention_hl_conflict` SMS test pins the generated bytes and
greps both `compiler.log` and `main.asm` for `phy BC`, `retained result in BC`,
and `retained arg2 in BC`. `Z80_PHY_B` and `Z80_PHY_C` are reserved ids now,
but the allocator still never assigns temps to those 8-bit subregisters. The
block-local scanner models physical-register unit overlap (`BC` owns `B|C`) and
routes `BC` fallback decisions through that predicate before selecting the
alternate register. Debug output prints
`linear_scan overlap_check ... candidate=BC candidate_units=B|C ... conflict=...`,
and `allocator_ra_bc_retention_hl_conflict` greps for the no-conflict case where
`HL` is active but `B`, `C`, and `BC` are free.

---

## 13. Cross-Block Allocation (CFG Liveness)

- [x] Build CFG edges from labels and unconditional/conditional jumps.
- [x] Compute live-in / live-out sets per block.
- [x] Reconcile register states at joins (insert reload at successor or
      spill at predecessor when states disagree).
- [x] Add loop, if/else, and switch validation cases.

Current implementation note: `pass_5.c` now derives debug-visible CFG edges
from the existing allocator basic blocks. Under `DEBUG_PASS_5`, it prints
`register_allocator: cfg_edge ... kind=branch_true`, `branch_false`, `jump`,
and `fallthrough` lines using label targets resolved back to block indices.
It also computes block `use`, `def`, `live_in`, and `live_out` sets by a
successor fixed point over those CFG edges and prints
`register_allocator: liveness ...` diagnostics. This is still a CFG/liveness
debug slice only; cross-block register retention remains disabled. Join
reconciliation is currently conservative and stack-only: live values crossing
join predecessors are already forced to spill at block exits, and successor
blocks reload from stack on demand. The debug verifier prints
`register_allocator: join_reconcile ... policy=stack_only` lines and fails if a
retained physical register reaches a multi-predecessor join before explicit
cross-block state reconciliation exists. The `allocator_ra_cfg_edges_debug` SMS
test pins ROM bytes with `byte_tester` and greps `compiler.log` for if/else
branch, jump, fallthrough, liveness, and join reconciliation diagnostics. The
`allocator_ra_cfg_loop_switch_debug` test pins loop and switch ROM bytes and
greps for a loop back-edge, switch fallthrough, and liveness diagnostics.

---

## 14. Cleanup And Documentation

- [x] Measure whether `reuse_registers()` still reduces spill traffic after
      physical allocation lands. If not, remove it.
- [x] Decide whether `compress_register_names()` is still worth keeping (it
      simplifies allocator arrays and diagnostics; lean towards keeping).
- [x] Replace stale allocator documentation with a link to this file plus a
      one-paragraph status summary.
- [x] Preserve the `g_allocator_enabled` / `-ra` feature flag permanently so
      the register allocator remains an explicit, separate compiler feature.

Current implementation note: `pass_5.c` now prints
`register_allocator: temp_compaction ...` measurements under `DEBUG_PASS_5`
around the first compress, `reuse_registers()`, and second compress. The
`allocator_ra_temp_compaction_debug` SMS test shows a concrete allocator case
where the first compress densifies temp ids (`highest_r=4` to `highest_r=1`),
`reuse_registers()` reduces the allocator-visible temp count from two temps to
one, and the second compress keeps the array dense (`temps=1 highest_r=0`).
The same case now expects two `retain_interval` decisions and a kept spill slot,
because Phase 15 treats reused definitions independently instead of turning the
whole reused temp slot into `spill no`. Keep `compress_register_names()` because
it gives the allocator dense metadata arrays before and after lifetime reuse;
keep `reuse_registers()` because it can still remove spill/retention bookkeeping
for non-overlapping virtual temps. The workspace currently has no
`IMPROVEMENTS.md`; the stale allocator TODO in `README.md` now points here and
summarizes the current feature-flagged status.

---

## 15. Per-Definition Retention For Reused Temp Slots

`reuse_registers()` can collapse unrelated definitions into one temp slot. A
single earlier hard-boundary spill reason must not globally poison later safe
definitions of the same slot.

- [x] Add per-TAC retained physical-register metadata for result, arg1, and
      arg2 operands.
- [x] Teach the Z80 location resolver to prefer per-TAC retained metadata for
      the current operand while keeping the existing whole-temp `spill no` path
      for truly single-use temps.
- [x] Keep a spill slot when only one definition interval is retained, so other
      reused intervals of the same temp slot can still use stack-backed code.
- [x] Stop treating one skipped interval as a whole-function temp-slot
      disqualification.
- [x] Add debug output for reused intervals:
      `register_allocator: linear_scan interval_reuse ...` and
      `register_allocator: linear_scan retain_interval ...`.
- [x] Add an SMS byte-tester case where `r0` is first forced stack-backed by an
      unmigrated `MUL` consumer, then a later reused `r0` definition is safely
      retained in `A`/`HL` with the spill slot kept.

Current implementation note: `struct tac` now carries
`result_physical_register`, `arg1_physical_register`, and
`arg2_physical_register`. `pass_5.c` marks these for retained intervals that
cannot safely become whole-temp `spill no` allocations, and `pass_6_z80.c`
resolves only the marked producer/consumer operands to `LOC_PHY_A`,
`LOC_PHY_HL`, or `LOC_PHY_BC`. The
`allocator_ra_interval_reuse_after_spill` SMS test captures compiler output,
greps the interval-reuse debug decisions, verifies retained comments in
`main.asm`, asserts the temp summary remains `spill yes phy NONE`, and pins the
generated bytes with `byte_tester`. After Phase 17, its earlier hard-boundary
interval uses `ARRAY_READ` rather than `MUL`, because mul/div/mod are no longer
unmigrated emitters.

---

## 16. Shift Emitter Migration

Shifts are a high-impact migration target because pass 5 already rewrites some
multiply/divide-by-power-of-two expressions into `SHIFT_LEFT` / `SHIFT_RIGHT`.

- [x] Migrate `_generate_asm_shift_left_right_z80()` and its 8-bit/16-bit
      helpers to `_resolve_tac_location()`.
- [x] Preserve the old IX-based stack/global load and store shape for
      non-retained operands.
- [x] Support retained `arg1` values in `A` for 8-bit shifts and `HL` / `BC`
      for 16-bit shifts.
- [x] Support retained shift results in `A`, `HL`, and `BC` where the result
      size matches the physical register path.
- [x] Keep shift counts stack-backed for now; do not enable retained `arg2`
      shift counts until the emitter can schedule `arg1` and `arg2` safely.
- [x] Remove `SHIFT_LEFT` and `SHIFT_RIGHT` from the unmigrated-emitter
      hard-boundary list.
- [x] Allow shifts as block-local linear-scan producers and arg1 consumers.
- [x] Add an SMS byte-tester case for retained 8-bit and 16-bit shift results,
      retained ADD-to-shift arg1 values, and the absence of
      `reason=unmigrated_emitter` shift barriers.

Current implementation note: `pass_6_z80.c` now routes the shift emitters
through the Z80 location abstraction while keeping the stack-backed IX paths
recognizable. `pass_5.c` treats shifts as migrated producers and arg1
consumers, but intentionally does not allocate shift-count `arg2` temps yet.
The `allocator_ra_shift_locations` SMS test captures `compiler.log`, greps for
retained shift producer/consumer decisions, verifies the retained comments in
`main.asm`, asserts shifts are no longer reported as unmigrated-emitter
barriers, and pins four marker ranges with `byte_tester`.

---

## 17. Mul/Div/Mod Result Migration

Mul/div/mod routines are clobber-heavy internally, so migrate output locations
first and keep retained input operands disabled until their scheduling is
handled deliberately.

- [x] Migrate `_generate_asm_mul_div_mod_z80_8bit()` result handling to
      `_resolve_tac_location()`.
- [x] Migrate `_generate_asm_mul_div_mod_z80_16bit()` result handling to
      `_resolve_tac_location()`.
- [x] Preserve the old stack/global operand paths for `arg1` and `arg2`; do
      not allow retained mul/div/mod operands yet.
- [x] Support retained 8-bit results in `A` for `MUL`, `DIV`, and `MOD`.
- [x] Support retained 16-bit results in `HL` for `MUL`, `DIV`, and `MOD`,
      including the `DIV` quotient move from `CA` to `HL`.
- [x] Remove `MUL`, `DIV`, and `MOD` from the unmigrated-emitter hard-boundary
      list.
- [x] Allow `MUL`, `DIV`, and `MOD` as block-local linear-scan producers, but
      keep them out of consumer rules.
- [x] Move existing unmigrated-emitter regression shapes from `MUL` to
      `ARRAY_READ` so hard-boundary, metadata, and frame-spill coverage remains
      meaningful.
- [x] Add an SMS byte-tester case for retained 8-bit and 16-bit
      `MUL`/`DIV`/`MOD` results feeding migrated `XOR` consumers.

Current implementation note: `pass_6_z80.c` now resolves mul/div/mod result
locations through the Z80 location abstraction and emits retained-result
comments for `A`, `HL`, and defensive `BC` paths. Operands still use the old
stack/global/constant paths, and `pass_5.c` does not classify mul/div/mod as
consumers. The `allocator_ra_mul_div_mod_results` SMS test captures
`compiler.log`, greps retained producer decisions for all six result cases,
asserts no `reason=unmigrated_emitter` line mentions `MUL`, `DIV`, or `MOD`,
checks retained comments in `main.asm`, and pins six marker ranges with
`byte_tester`. Existing negative tests that previously used `MUL` as a
stack-forcing producer now use `ARRAY_READ` instead, including
`allocator_all_spill_metadata`, `allocator_ra_frame_spill_slot_a` / `_hl`,
`allocator_ra_boundary_spill_reasons_a` / `_hl`, and
`allocator_ra_interval_reuse_after_spill`. The boundary spill tests keep the
function-call spill assertion via debug/ASM greps and avoid pinning an
address-sensitive JP target byte range that differs between WLA-DX paths.

---

## 18. Get-Address Result Migration

`GET_ADDRESS` and `GET_ADDRESS_ARRAY` already compute their address result in
`HL`, so migrate their result locations before attempting retained pointer or
array input operands.

- [x] Migrate `_generate_asm_get_address_z80()` result handling to
      `_resolve_tac_location()`.
- [x] Preserve the old stack/global paths for the base and index operands;
      do not allow retained get-address input operands yet.
- [x] Support retained 16-bit address results in `HL`.
- [x] Keep defensive handling for unexpected `BC`/`A` result locations loud,
      so future allocator expansion cannot silently miscompile addresses.
- [x] Remove `GET_ADDRESS` and `GET_ADDRESS_ARRAY` from the
      unmigrated-emitter hard-boundary list.
- [x] Allow `GET_ADDRESS` and `GET_ADDRESS_ARRAY` as block-local linear-scan
      producers, but keep them out of consumer rules.
- [x] Add an SMS byte-tester case for plain address-of and indexed address-of
      results feeding migrated `XOR` consumers.

Current implementation note: `pass_6_z80.c` now resolves get-address result
locations through the Z80 location abstraction and emits `; retained result in
HL` when the address stays in `HL`. The base label and array index still use
the old stack/global paths, and `pass_5.c` does not classify get-address ops as
consumers. The `allocator_ra_get_address_results` SMS test captures
`compiler.log`, greps retained producer decisions for `GET_ADDRESS` and
`GET_ADDRESS_ARRAY`, verifies retained result/arg comments in `main.asm`,
asserts no `reason=unmigrated_emitter` line mentions `GET_ADDRESS`, and pins
two marker ranges with `byte_tester`.

---

## 19. Array-Read Result Migration

`ARRAY_READ` and the special `__z80_in[...]` path are producer-shaped: they
load the result into `L`/`HL` or `A` before storing it. Migrate only their
result locations first, leaving array base/index operands stack-backed.

- [x] Migrate `_generate_asm_array_read_z80()` result handling to
      `_resolve_tac_location()`.
- [x] Migrate `_generate_asm_z80_in_read_z80()` result handling to
      `_resolve_tac_location()`.
- [x] Preserve the old stack/global paths for array bases, pointer bases,
      indexes, and `__z80_in` ports.
- [x] Support retained 8-bit array-read and `__z80_in` results in `A`.
- [x] Support retained 16-bit array-read results in `HL`.
- [x] Remove `ARRAY_READ` from the unmigrated-emitter hard-boundary list;
      keep `ARRAY_WRITE` as the conservative array barrier.
- [x] Allow `ARRAY_READ` as a block-local linear-scan producer, but keep it
      out of consumer rules.
- [x] Move hard-boundary regression shapes from `ARRAY_READ` to `ARRAY_WRITE`
      so spill-reason and interval-reuse coverage remains meaningful.
- [x] Add an SMS byte-tester case for retained 8-bit array reads, retained
      16-bit array reads, and retained `__z80_in` results feeding migrated
      `XOR` consumers.

Current implementation note: `pass_6_z80.c` now resolves array-read result
locations through the Z80 location abstraction and emits retained-result
comments for `A`, `HL`, and defensive `BC` paths. At this phase, the array
base/index and `__z80_in` port operands intentionally remained stack-backed;
later phases migrated array consumers and Phase 35 migrated retained
`__z80_in` port operands. The
`allocator_ra_array_read_results` SMS test captures `compiler.log`, greps
retained producer decisions for normal 8-bit and 16-bit reads plus
`__z80_in`, verifies retained result/arg comments in `main.asm`, asserts no
`reason=unmigrated_emitter` line mentions `ARRAY_READ`, and pins three marker
ranges with `byte_tester`. Existing hard-boundary tests now use `ARRAY_WRITE`
as the remaining unmigrated array barrier.

---

## 20. Array-Write Value Operand Migration

- [x] Classify `ARRAY_WRITE` value operands (`arg1`) as allocator consumers in
      `pass_5.c`, including the `__z80_out[...] = value` special case.
- [x] Resolve `ARRAY_WRITE` and `__z80_out[...]` value operands through
      `z80_location` while keeping array bases and indexes on the existing
      stack/global paths.
- [x] Add retained value emission for `LOC_PHY_A`, `LOC_PHY_HL`, and defensive
      `LOC_PHY_BC` array-write paths, with direct `(IY+offset)` stores.
- [x] Preserve constant `__z80_out[...]` writes by materializing constants
      directly into `A` before the `OUT` instruction.
- [x] Allow widened 8-bit `ARRAY_READ` results retained in 16-bit locations
      when the resolved result location is 2 bytes, so `ARRAY_READ ->
      ARRAY_WRITE arg1` promotions remain clobber-safe.
- [x] Keep `ARRAY_WRITE` in the unmigrated-emitter list for this slice because
      base/index/pointer paths are not migrated yet.
- [x] Add `tests/z80/sms/allocator_ra_array_write_values` with `compiler.log`
      greps for retained `ARRAY_WRITE arg1` intervals, retained arg comments in
      `main.asm`, and pinned `byte_tester` ranges for 8-bit normal writes,
      16-bit normal writes, and `__z80_out` value writes.

This phase migrates the value side of array writes only. The debug output should
still show `reason=unmigrated_emitter` for `ARRAY_WRITE`, but same-block
producer-to-`ARRAY_WRITE arg1` intervals can now be retained into `A` or `HL`
with a kept spill slot. A full-suite regression exposed a widened array-read
case in `z80/sms/calculations`: an 8-bit table read feeding a 16-bit array write
can have a 2-byte resolved result location. The backend now zero/sign-extends
based on the source item type and stores/retains the full 16-bit value when the
resolved result location requires it.

---

## 21. Array-Write Index Operand Migration

- [x] Classify `ARRAY_WRITE` index operands (`arg2`) as allocator consumers in
      `pass_5.c`, including the `__z80_out[index] = value` special case.
- [x] Resolve `ARRAY_WRITE` and `__z80_out[...]` index operands through
      `z80_location` while keeping array bases and pointer bases on the
      existing stack/global paths.
- [x] Add retained index emission for `LOC_PHY_A`, `LOC_PHY_HL`, and defensive
      `LOC_PHY_BC` paths, materializing normal array indexes into `BC` and
      `__z80_out` ports into `C`.
- [x] Schedule `__z80_out` port materialization before value materialization so
      a retained port in `A` is not overwritten by a retained value.
- [x] Add an `ARRAY_WRITE` signed-index path that can sign-extend `C` into
      `BC` without clobbering a retained write value in `A`.
- [x] Keep `ARRAY_WRITE` in the unmigrated-emitter list for this slice because
      base and pointer paths are not migrated yet.
- [x] Add `tests/z80/sms/allocator_ra_array_write_indexes` with `compiler.log`
      greps for retained `ARRAY_WRITE arg2` intervals, retained arg comments in
      `main.asm`, signed-index preserve-`A` output, and pinned `byte_tester`
      ranges for 8-bit normal indexes, 16-bit normal indexes, `__z80_out`
      ports, and signed-index writes with retained values.

This phase migrates array-write index and port operands only. The generated
debug output should still show `reason=unmigrated_emitter` for `ARRAY_WRITE`,
but same-block producer-to-`ARRAY_WRITE arg2` intervals can now be retained into
`A` or `HL` with a kept spill slot. Full array-write migration still requires
base arrays, pointer bases, and pointer writes to move to `z80_location` before
the hard emitter barrier can be removed.

---

## 22. Array-Write Computed Base Operand Migration

- [x] Treat `ARRAY_WRITE result` temps as base-address reads rather than temp
      definitions in allocator liveness, use/def, and frame-usage bookkeeping.
- [x] Classify `ARRAY_WRITE result` temps as allocator consumers so computed
      bases can participate in same-block retained intervals.
- [x] Preserve and clear per-TAC retained-result metadata for consumer-side
      result operands, with debug output naming the consumer as
      `ARRAY_WRITE result`.
- [x] Resolve normal `ARRAY_WRITE result` operands through `z80_location` in
      `pass_6_z80.c` before generating the target address.
- [x] Materialize retained computed bases from `HL` into `IY` for the write,
      keep a defensive `BC` path for zero-index writes, and fail loudly for
      invalid retained `A` bases or `BC` bases with non-zero indexes.
- [x] Keep `ARRAY_WRITE` in the unmigrated-emitter list for this slice because
      pointer-variable dereference paths and pointer value writes still need
      their own clobber-safe migration.
- [x] Add `tests/z80/sms/allocator_ra_array_write_bases` with real struct-field
      writes that lower through computed `ARRAY_WRITE result` bases, grep
      `compiler.log` for retained base intervals and `operand=result kind=PHY_HL`,
      and pin the emitted ROM bytes with `byte_tester`.

This phase migrates computed array-write base temps, including struct-field
address calculations, without removing the `ARRAY_WRITE` hard barrier. The
debug output should still show `reason=unmigrated_emitter` for `ARRAY_WRITE`,
but the computed base feeding `ARRAY_WRITE result` can now stay in `HL` until
the emitter copies it into `IY` for the final store.

---

## 23. Array-Write Pointer Base Helper Migration

- [x] Move `ARRAY_WRITE` result/base materialization into a shared
      `_materialize_array_write_result_to_iy()` helper that works from the
      resolved `z80_location` instead of open-coded offset branches.
- [x] Route stack, global, and spilled result/base locations through
      `_load_location_address_to_iy()` before the final store address is
      calculated.
- [x] Preserve pointer-variable semantics by detecting result operands whose
      stack/global slot contains the real pointer value, then loading that
      pointer from `(IY+0)/(IY+1)` into `IY` before applying the array index.
- [x] Keep retained-base defensive behavior in one place: reject retained `A`
      bases, allow retained `HL` bases, and allow retained `BC` bases only for
      zero-index writes.
- [x] Add backend debug output of the form
      `register_allocator: array_write_base function=... kind=... memory_pointer_load=yes/no`
      so pointer-base paths can be verified from `compiler.log`.
- [x] Keep `ARRAY_WRITE` in the unmigrated-emitter list for this slice because
      pointer-typed value writes and remaining indirect value edge cases still
      need explicit coverage before the barrier is removed.
- [x] Add `tests/z80/sms/allocator_ra_array_write_pointer_bases` with pointer
      argument writes that retain the written value in `A` and `HL`, grep the
      new base debug output, and pin the pointer-base store bytes with
      `byte_tester`.

This phase migrates pointer-variable base loading to the same location-based
path used by other array-write bases. The generated debug output still shows
`reason=unmigrated_emitter` for `ARRAY_WRITE`, but pointer argument writes now
also show `array_write_base ... kind=STACK_LOCAL memory_pointer_load=yes`,
making it clear that the emitter loads the pointer value from the stack slot
before storing the retained value.

---

## 24. Array-Write Pointer Value Helper Migration

- [x] Move `ARRAY_WRITE arg1` value materialization into a shared
      `_materialize_array_write_value_to_iy()` helper that works from the
      resolved `z80_location`.
- [x] Derive the emitted store width from the destination item type and the
      resolved source location size, so pointer-typed values remain explicit
      16-bit writes.
- [x] Preserve retained value emission for `A`, `HL`, and `BC`, including the
      existing retained-arg comments in `main.asm`.
- [x] Preserve stack/global/spilled source loads through the existing `IX`
      location path, including 8-bit-to-16-bit sign/zero extension where the
      destination requires it.
- [x] Add backend debug output of the form
      `register_allocator: array_write_value function=... kind=... bytes=... source_size=...`
      so pointer-value writes can be verified from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_array_write_pointer_values` with a
      retained pointer expression (`source + 8`) stored through `u8 **target`
      and a stack-backed pointer value store, both pinned with `byte_tester`.

This phase completes the planned pointer-value slice for `ARRAY_WRITE`: pointer
values are now visibly handled as 16-bit `arg1` locations, with retained `HL`
and stack-backed sources both covered. `ARRAY_WRITE` still remains in the
unmigrated-emitter list until the next audit removes the barrier and proves no
stack-only path can still request a retained temp offset.

---

## 25. Array-Write Barrier Removal

- [x] Remove `TAC_OP_ARRAY_WRITE` from the unmigrated-emitter hard-boundary
      predicate now that result/base, arg1 value, and arg2 index paths resolve
      through `z80_location` helpers.
- [x] Keep `ARRAY_WRITE` in the Z80 clobber descriptor table with
      `A|BC|HL|IX|IY|FLAGS`, so unsafe physical retention across the store is
      still rejected by normal clobber-safety checks rather than by a blanket
      stack-only barrier.
- [x] Add allocator debug output of the form
      `register_allocator: migrated_emitter function=... tac=...(ARRAY_WRITE) barrier=removed clobber_checked=yes`
      to make the barrier transition visible in `compiler.log`.
- [x] Update existing `ARRAY_WRITE` allocator regressions to assert the new
      migrated-emitter debug, retained physical operands, no
      `reason=unmigrated_emitter.*ARRAY_WRITE`, and no
      `stack offset requested for retained` diagnostics.
- [x] Add `tests/z80/sms/allocator_ra_array_write_barrier_removed`, a focused
      SMS regression that keeps reused retained intervals around an
      `ARRAY_WRITE`, greps the clobber-checked migrated-emitter output, and
      pins the generated bytes with `byte_tester`.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (67 tests)`.

This phase completes the planned `ARRAY_WRITE` migration. The emitter is no
longer a hard `unmigrated_emitter` spill boundary, but it is still treated as a
normal clobbering Z80 operation. That leaves retained operands free to feed the
location-aware `ARRAY_WRITE` helpers while still preventing values from being
kept in physical registers across a store that would clobber them.

---

## 26. Array-Read Index Operand Migration

- [x] Classify `ARRAY_READ arg2` temps as allocator consumers so computed
      indexes can be retained into `A` or `HL` until the array-read emitter.
- [x] Resolve `ARRAY_READ arg2` through `z80_location` instead of calling
      `find_stack_offset()` directly, preserving the guard for retained temps.
- [x] Reuse the shared array-index materializer for `ARRAY_READ`, including
      retained `A`, retained `HL`, constants, stack locals, globals, and spill
      slots.
- [x] Add backend debug output of the form
      `register_allocator: array_read_index function=... kind=... size=... phy=...`
      so retained index paths can be verified from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_array_read_indexes` with retained 8-bit
      `A` and 16-bit `HL` indexes, debug greps for `array_read_index`, negative
      `stack offset requested for retained` checks, and pinned `byte_tester`
      bytes.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (68 tests)`.

This phase migrates only the `ARRAY_READ` index operand. Array-read result
retention remains intact from the earlier result-location phase. Array-read
base/pointer operands stayed on the conservative stack-backed path at this
point; Phase 33 later moves pointer bases through the location-aware helper.

---

## 27. Get-Address Array Index Operand Migration

- [x] Classify `GET_ADDRESS_ARRAY arg2` temps as allocator consumers so
      computed array indexes can be retained until the address calculation.
- [x] Resolve `GET_ADDRESS_ARRAY arg2` through `z80_location`, preserving the
      loud `find_stack_offset()` guard for retained temps.
- [x] Materialize retained `HL` indexes into `BC` before loading the base
      address into `HL`, so the emitter does not overwrite the retained index
      before it is consumed.
- [x] Preserve the old stack/global/constant index lowering order for
      non-retained operands so existing byte ranges remain stable.
- [x] Add backend debug output of the form
      `register_allocator: get_address_index function=... kind=... size=... phy=...`
      to verify retained get-address indexes from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_get_address_indexes` with retained
      computed indexes for 8-bit and 16-bit arrays. Both indexes are promoted
      to 16-bit address indexes and retained in `HL`, with debug greps,
      negative `stack offset requested for retained` checks, and pinned
      `byte_tester` bytes.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (69 tests)`.

This phase migrates the `GET_ADDRESS_ARRAY` index operand without changing the
remaining stack-backed base/pointer address paths. It also documents the
observed TAC behavior that even 8-bit source indexes may be promoted to 16-bit
address temps before the get-address emitter consumes them.

---

## 28. Shift Count Operand Migration

- [x] Classify `SHIFT_LEFT arg2` and `SHIFT_RIGHT arg2` temps as allocator
      consumers so computed 8-bit shift counts can be retained until the shift
      emitter consumes them.
- [x] Restrict retained shift-count candidates to `A`, matching the existing
      8-bit Z80 shift helper contract and avoiding unsupported `HL`/`BC` count
      paths.
- [x] Keep 16-bit shift-count operands stack-backed for now, because the
      16-bit shift helper still rejects retained `ARG2` values explicitly.
- [x] Add backend debug output of the form
      `register_allocator: shift_count function=... kind=... size=... phy=...`
      to verify retained count operands from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_shift_counts` with retained computed
      counts for both `SHIFT_LEFT arg2` and `SHIFT_RIGHT arg2`, debug greps for
      `linear_scan retain`, `resolve ... kind=PHY_A`, and `shift_count ...
      kind=PHY_A`, a negative `stack offset requested for retained` check, and
      pinned `byte_tester` bytes.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (70 tests)`.

This phase migrated the 8-bit shift-count operand role only. Wider shift counts
remained deliberately stack-backed at this point; Phase 31 later teaches the
16-bit helper to consume retained `BC` counts directly.

---

## 29. Mul/Div/Mod Arg1 Operand Migration

- [x] Classify `MUL arg1`, `DIV arg1`, and `MOD arg1` temps as allocator
      consumers while leaving `arg2` stack-backed until the second-operand
      scheduling is migrated separately.
- [x] Resolve the first mul/div/mod operand through `z80_location` in both the
      8-bit and 16-bit Z80 helpers.
- [x] Materialize retained 8-bit `A` operands into `H` before the 8-bit
      multiply/divide/modulo algorithms run.
- [x] Materialize retained 16-bit `HL` operands into `BC` before the 16-bit
      multiply/divide/modulo algorithms run.
- [x] Add backend debug output of the form
      `register_allocator: mul_div_mod_arg1 function=... op=... kind=... size=... phy=...`
      to verify retained first operands from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_mul_div_mod_arg1` with retained first
      operands for 8-bit and 16-bit `MUL`, `DIV`, and `MOD`, debug greps for
      `linear_scan retain`, `resolve ... kind=PHY_A`/`PHY_HL`, and
      `mul_div_mod_arg1 ... kind=...`, a negative `stack offset requested for
      retained` check, and pinned `byte_tester` bytes.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (71 tests)`.

This phase migrates only the first input operand for mul/div/mod. The second
input operand remains deliberately stack-backed because the helpers still load
it after the first operand has been staged into `H` or `BC`, and that path needs
its own clobber review.

---

## 30. Mul/Div/Mod Arg2 Operand Migration

- [x] Classify `MUL arg2`, `DIV arg2`, and `MOD arg2` temps as allocator
      consumers after the first operand path was migrated and validated.
- [x] Resolve the second mul/div/mod operand through `z80_location` in both the
      8-bit and 16-bit Z80 helpers.
- [x] Materialize retained 8-bit `A` operands into `E` immediately before the
      8-bit multiply/divide/modulo algorithms run.
- [x] Materialize retained 16-bit `HL` operands into `DE` immediately before
      the 16-bit multiply/divide/modulo algorithms run, while preserving the
      existing frame-pointer save/restore around `DE`.
- [x] Preserve stack/global/constant second-operand lowering behavior for
      non-retained operands so normal mul/div/mod code still follows the old
      scheduling path.
- [x] Add backend debug output of the form
      `register_allocator: mul_div_mod_arg2 function=... op=... kind=... size=... phy=...`
      to verify retained second operands from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_mul_div_mod_arg2` with retained second
      operands for 8-bit and 16-bit `MUL`, `DIV`, and `MOD`, debug greps for
      `linear_scan retain`, `resolve ... kind=PHY_A`/`PHY_HL`, and
      `mul_div_mod_arg2 ... kind=...`, a negative `stack offset requested for
      retained` check, and pinned `byte_tester` bytes.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (72 tests)`.

This phase completes the direct retained operand coverage for mul/div/mod:
results, first operands, and second operands are now all location-aware for the
currently supported block-local retention model.

---

## 31. 16-bit Shift Count Operand Migration

- [x] Allow `SHIFT_LEFT arg2` and `SHIFT_RIGHT arg2` temps with 16-bit size to
      use `BC`, matching the 16-bit Z80 shift helper contract of shifting `HL`
      by `BC`.
- [x] Keep 8-bit shift counts restricted to `A`, and reject unsupported
      retained `A`/`HL` counts in the 16-bit helper loudly.
- [x] Add operand-role register selection in the linear scan so a 16-bit
      shift-count temp can be redirected from the default `HL` candidate to
      the clobber-safe `BC` candidate before path-safety checks run.
- [x] Resolve 16-bit shift `arg2` through `z80_location` and consume retained
      `LOC_PHY_BC` counts without reloading from the stack.
- [x] Emit the existing `register_allocator: shift_count ...` backend debug
      line from the 16-bit shift path too, so retained `BC` counts are visible
      in `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_shift_counts_16bit` with retained
      computed counts for 16-bit `SHIFT_LEFT arg2` and `SHIFT_RIGHT arg2`,
      debug greps for `linear_scan retain ... 16-bit in BC`, `resolve ...
      kind=PHY_BC`, and `shift_count ... kind=PHY_BC`, a negative
      `stack offset requested for retained` check, a `retained arg2 in BC` ASM
      check, and pinned `byte_tester` bytes.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (73 tests)`.

This phase completes retained shift-count coverage for the current 8-bit and
16-bit shift helpers: 8-bit counts remain in `A`, while 16-bit counts are now
retained in `BC` and consumed directly by the `HL` by `BC` shift loops.

---

## 32. Get-Address Pointer Base Migration

- [x] Resolve `GET_ADDRESS` / `GET_ADDRESS_ARRAY arg1` through
      `z80_location` instead of calling `find_stack_offset()` directly, so the
      loud retained-temp guard stays effective for get-address base operands.
- [x] Preserve old stack/global address-of behavior for plain variables and
      array symbols while routing the lowering through the shared location
      representation.
- [x] Detect pointer-variable bases for `GET_ADDRESS_ARRAY` and load the
      pointer value from the stack/global slot before adding the retained or
      stack-backed index.
- [x] Use the array/pointee item type when scaling `GET_ADDRESS_ARRAY`
      indexes, so `u8 *base` adds the index once while `u16 *base` adds it
      twice.
- [x] Add backend debug output of the form
      `register_allocator: get_address_base function=... kind=... memory_pointer_load=yes/no`
      to verify pointer-base lowering from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_get_address_pointer_bases` with pointer
      argument bases, retained computed indexes, debug greps for
      `get_address_base ... memory_pointer_load=yes`, negative
      `stack offset requested for retained` checks, and pinned `byte_tester`
      ranges for `u8 *` and `u16 *` bases.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (74 tests)`.

This phase migrates pointer-variable get-address bases to the location-aware
backend path. Native SameSameC lowering for `&base[index]` still names the base
as a label rather than as a computed temp, so this slice proves pointer-base
semantics and retained index coexistence without claiming cross-expression
computed base retention.

---

## 33. Array-Read Pointer Base Migration

- [x] Resolve `ARRAY_READ arg1` through `z80_location` instead of calling
      `find_stack_offset()` directly, so the loud retained-temp guard stays
      effective for array-read base operands.
- [x] Preserve old direct array address behavior for stack/global bases while
      routing the lowering through the shared location representation.
- [x] Detect pointer-variable bases for `ARRAY_READ` and load the pointer value
      from the stack/global slot before adding the retained or stack-backed
      index.
- [x] Preserve `BC` around pointer-value loading whenever the array index is
      nonzero or non-constant, so retained `A`/`HL` indexes survive the base
      pointer load.
- [x] Keep retained computed base handling defensive in the backend helper:
      reject invalid retained `A` bases, allow `HL`/`BC` address bases, and do
      not enable new `ARRAY_READ arg1` allocator classification until a native
      SameSameC test shape proves it.
- [x] Add backend debug output of the form
      `register_allocator: array_read_base function=... kind=... memory_pointer_load=yes/no`
      to verify pointer-base lowering from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_array_read_pointer_bases` with pointer
      argument bases, retained computed indexes, debug greps for
      `array_read_base ... memory_pointer_load=yes`, negative
      `stack offset requested for retained` checks, retained `arg2` ASM
      comments, and pinned `byte_tester` ranges for `u8 *` and `u16 *` reads.
- [x] Rebuild the Cygwin compiler, rebuild the MSVC Release compiler, refresh
      test binaries, and verify both full suites report `DONE (75 tests)`.

This phase migrates pointer-variable array-read bases to the location-aware
backend path. The new test proves that stack-local pointer arguments are
resolved as `STACK_LOCAL`, their pointer value is loaded before indexing, and
retained computed indexes remain in `A` or `HL` through the read.

---

## 34. Array-Read Computed Base Migration

- [x] Classify retained computed `ARRAY_READ arg1` temps as allocator consumers
      when the temp is 16-bit and can stay in `HL` until the array-read base
      materialization.
- [x] Keep computed array-read base retention conservative: reject `A` and
      `BC` for this role in allocator classification, because the migrated
      backend path has a stable direct address path only through `HL`.
- [x] Add defensive 16-bit `HL` classification for `GET_ADDRESS_ARRAY arg1`
      temps, but leave native SMS coverage pending until a SameSameC source
      shape proves that computed base lowering path.
- [x] Harden the Z80 get-address and array-read helpers against unsafe
      retained `BC` base/index combinations and invalid retained `A` bases.
- [x] Fix the 8-bit add/sub/xor/and emitter to consume retained `HL` and `BC`
      low bytes directly through `L` and `C`, so reused temp intervals do not
      fall back to stale stack-only `IX` loads.
- [x] Add `tests/z80/sms/allocator_ra_array_read_computed_bases` with real SMS
      compiler/linker flow, allocator debug greps for `ARRAY_READ arg1`
      retention, backend `array_read_base ... kind=PHY_HL` checks, retained
      low-byte ASM checks, negative retained-stack diagnostics, and byte-tester
      assertions for 8-bit and 16-bit struct-field reads.
- [x] Refresh the affected get-address byte pins now that retained `HL` low
      bytes generate `LD A,L` instead of reloading through `IX`.
- [x] Rebuild the Cygwin compiler, rebuild/copy the MSVC Release compiler, and
      verify both full suites report `DONE (76 tests)`.

This phase enables the native computed base shape proven by SameSameC struct
field reads: `GET_ADDRESS` produces an address temp, `ARRAY_READ arg1` consumes
that retained `HL` base, and the subsequent arithmetic can keep using the low
byte of the retained value safely. `GET_ADDRESS_ARRAY arg1` support is present
defensively, but still needs a native source-level test shape before it can be
called complete.

---

## 35. Z80 In Port Operand Migration

- [x] Migrate the `__z80_in[port]` operand path from direct stack-offset
      lookup to `_resolve_tac_location(t, TAC_USE_ARG2, ...)`, so retained
      computed port temps can be consumed without touching stack-only
      `find_stack_offset()` paths.
- [x] Generalize the Z80 port materializer so `__z80_in` and `__z80_out`
      share the same `LOC_CONST`, stack/global/spill, `PHY_A`, `PHY_HL`, and
      `PHY_BC` handling for loading the port number into `C`.
- [x] Add allocator debug output for the input-port path:
      `register_allocator: z80_in_port function=... kind=... size=... phy=...`.
- [x] Add `tests/z80/sms/allocator_ra_z80_in_ports` with real compiler/linker
      flow, debug greps for retained `ARRAY_READ arg2` ports in `A` and `HL`,
      backend resolve checks, retained-port ASM checks (`LD C,A` / `LD C,L`),
      negative retained-stack diagnostics, and byte-tester assertions.
- [x] Rebuild the Cygwin compiler, rebuild/copy the MSVC Release compiler, run
      focused neighboring port/array tests, and verify both full suites report
      `DONE (77 tests)`.

This phase closes a special array-read path: `__z80_in[left + right]` and
`__z80_in[left ^ right]` now keep the computed port in a retained register and
materialize only the low byte into `C` for `IN A,(C)`. The path remains
block-local and `-ra` stays opt-in.

---

## 36. Function-Call Return Result Migration

- [x] Keep function calls as hard clobber boundaries for values that are live
      before the call, but allow `FUNCTION_CALL_USE_RETURN_VALUE` to start a
      fresh block-local retained interval after the callee returns.
- [x] Teach `propagate_operand_types()` to assign the callee return type to a
      function-call result temp, so direct call-result expressions such as
      `callee(value) ^ mask` keep a usable temp type through compaction.
- [x] Add `FUNCTION_CALL_USE_RETURN_VALUE` as a linear-scan producer while
      leaving plain `FUNCTION_CALL` as a basic-block end reason.
- [x] Migrate the function-call return destination copy in
      `_generate_asm_function_call_z80()` from direct `find_stack_offset()` to
      `_resolve_tac_location(t, TAC_USE_RESULT, ...)`.
- [x] Support retained call results in `A`, `HL`, and defensive `BC` paths,
      with backend debug output:
      `register_allocator: function_call_return function=... callee=... kind=... size=... phy=...`.
- [x] Add `tests/z80/sms/allocator_ra_function_call_results` with real
      compiler/linker flow, debug greps for retained 8-bit and 16-bit call
      results feeding `XOR`, retained-call-result ASM checks, negative
      retained-stack diagnostics, and byte-tester assertions.
- [x] Rebuild the Cygwin compiler, rebuild/copy the MSVC Release compiler,
      run focused call/return validation, and verify both full suites report
      `DONE (78 tests)`.

This phase does not retain arbitrary values across calls. The custom
`PUSH return_label / JP callee` sequence still spills pre-call live ranges; the
new retained interval begins only after the return value has been loaded back
from the callee frame.

---

## 37. GET_ADDRESS_ARRAY Pointer-Base Temp Coverage

- [x] Add an allocator-only lowering for `&pointer[constant]`: pass 4 now
      exposes the pointer base as a short 16-bit temp before
      `GET_ADDRESS_ARRAY`, with debug output:
      `register_allocator: get_address_array_pointer_base_temp function=... label=... r...`.
- [x] Preserve that exposed base temp through `_optimize_il_23()` under `-ra`
      so the linear scan can see the temp as `GET_ADDRESS_ARRAY arg1`.
- [x] Allow `TAC_OP_ASSIGNMENT` as a linear-scan producer only for the narrow
      `GET_ADDRESS_ARRAY arg1` / 16-bit `HL` case; ordinary assignment
      producer cases remain negative.
- [x] Initially keep computed-index pointer bases on the existing stack-local
      base path while the required two-register schedule was not migrated yet;
      Phase 38 below closes that gap with a `BC` base / `HL` index path.
- [x] Add `tests/z80/sms/allocator_ra_get_address_computed_bases` with real
      compiler/linker flow, debug greps for the new pointer-base temp,
      retained `ASSIGNMENT -> GET_ADDRESS_ARRAY arg1`, `PHY_HL` base
      resolution, `memory_pointer_load=no`, negative assignment-to-`XOR`
      greps, retained ASM comments, and byte-tester assertions.
- [x] Verify the new focused test, the older computed-index pointer-base test,
      the existing `ARRAY_READ arg1` computed-base test, and an assignment
      producer negative test.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (79 tests)`.

SameSameC's address-of-array parser accepts `&name[index]`; it does not expose
an arbitrary `&expr[index]` source form. This phase covers the constant-index
native pointer base shape where the pointer value can stay in `HL` through
`GET_ADDRESS_ARRAY arg1`. Phase 38 extends the same allocator-only lowering to
computed indexes by using `BC` for the pointer base when `HL` is already needed
for the index.

---

## 38. GET_ADDRESS_ARRAY Computed-Index Pointer Base BC/HL Path

- [x] Expand the allocator-only `GET_ADDRESS_ARRAY` pointer-base temp lowering
      from constant indexes to computed indexes while keeping `-ra` opt-in.
- [x] Allow `TAC_OP_ASSIGNMENT -> GET_ADDRESS_ARRAY arg1` to choose `BC` only
      when the same consumer already retains computed `arg2` in `HL`; ordinary
      assignment producers remain stack-only outside this narrow address case.
- [x] Keep the existing linear-scan alternate-register checks in charge of the
      final choice, including overlap checks for `B`/`C` units and active `HL`
      intervals.
- [x] Teach `_generate_asm_get_address_z80()` the retained `BC` base plus
      retained `HL` index order: leave `BC` as the pointer base, keep the index
      in `HL`, scale 16-bit element indexes with `ADD HL,HL`, then add the base
      with `ADD HL,BC`.
- [x] Add `tests/z80/sms/allocator_ra_get_address_bc_base_hl_index` with a
      lowercase `makefile`, `main.ssc`, normal compiler/linker flow, allocator
      debug greps, backend comment greps, and byte-tester pins.
- [x] Update `tests/z80/sms/allocator_ra_get_address_pointer_bases` so the
      existing computed-index regression now asserts the retained `BC` base /
      retained `HL` index path instead of the older stack-local base path.
- [x] Read the new debug output and verify the expected path: pointer-base temp
      creation, `primary=HL alternate=BC`, retained `ASSIGNMENT ->
      GET_ADDRESS_ARRAY arg1`, `resolve ... operand=arg1 kind=PHY_BC`,
      `get_address_base ... kind=PHY_BC memory_pointer_load=no`,
      `get_address_index ... kind=PHY_HL`, and `retained arg1 in BC` /
      `retained arg2 in HL` comments.
- [x] Verify focused regressions:
      `allocator_ra_get_address_bc_base_hl_index`,
      `allocator_ra_get_address_pointer_bases`,
      `allocator_ra_get_address_computed_bases`,
      `allocator_ra_assignment_producer_hl`, and
      `allocator_ra_bc_retention_hl_conflict`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (80 tests)`.

This phase closes the last known `GET_ADDRESS_ARRAY` pointer-base case that was
intentionally left on the stack path in Phase 37. The allocator still does not
turn ordinary assignments into register producers; the `BC` allowance is tied to
one consumer shape where the backend has an explicit two-register schedule.

---

## 39. ARRAY_READ 16-Bit Pointer Base BC/HL Path

- [x] Add allocator-only lowering for 16-bit computed-index `ARRAY_READ`
      pointer bases, exposing the pointer variable as a short 16-bit assignment
      temp with debug output:
      `register_allocator: array_read_pointer_base_temp function=... label=... r...`.
- [x] Keep 8-bit computed-index pointer reads on the previous path so their
      index temp can still retain in `A`; the inserted base assignment would be
      an intervening `A` clobber in that shape.
- [x] Preserve assignment-fed `ARRAY_READ arg1` temps through `_optimize_il_23()`
      under `-ra`, matching the existing address-taking preservation rule.
- [x] Allow `TAC_OP_ASSIGNMENT -> ARRAY_READ arg1` in `HL`, and allow the
      alternate `BC` choice only when the same consumer already keeps computed
      `arg2` in `HL`.
- [x] Teach `_generate_asm_array_read_z80()` to handle retained `BC` bases with
      nonzero indexes by copying the base to `IY` before materializing the index
      into `BC`; retained `BC` base plus retained `BC` index remains a loud
      backend error.
- [x] Update `tests/z80/sms/allocator_ra_array_read_pointer_bases` so the `u8 *`
      case asserts the old retained-`A` index/stack-local base path while the
      `u16 *` case asserts `PHY_BC` base, `PHY_HL` index, `memory_pointer_load=no`,
      and retained asm comments.
- [x] Add `tests/z80/sms/allocator_ra_array_read_bc_base_hl_index` with a
      lowercase `makefile`, `main.ssc`, normal compiler/linker flow, allocator
      debug greps, backend comment greps, and byte-tester pins.
- [x] Read the new debug output and verify the expected split: no pointer-base
      temp for `allocatorRaArrayReadPointerBaseA`, retained `A` index there,
      pointer-base temp for the 16-bit case, `primary=HL alternate=BC`, retained
      `ASSIGNMENT -> ARRAY_READ arg1`, `resolve ... operand=arg1 kind=PHY_BC`,
      `array_read_base ... kind=PHY_BC memory_pointer_load=no`, and
      `array_read_index ... kind=PHY_HL`.
- [x] Verify focused regressions:
      `allocator_ra_array_read_bc_base_hl_index`,
      `allocator_ra_array_read_pointer_bases`,
      `allocator_ra_array_read_computed_bases`,
      `allocator_ra_assignment_producer_hl`,
      `allocator_ra_get_address_bc_base_hl_index`, and
      `allocator_ra_array_write_pointer_bases`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (81 tests)`.

This phase extends the two-register pointer-base schedule from
`GET_ADDRESS_ARRAY` to the 16-bit `ARRAY_READ` pointer argument shape. It is
intentionally narrower than all pointer reads: the 8-bit index form already has
a good retained-`A` path, and inserting the pointer-base assignment there would
force the index to spill because assignment remains a conservative clobber.

---

## 40. ARRAY_WRITE 16-Bit Pointer Base BC/HL Path

- [x] Add allocator-only lowering for 16-bit computed-index `ARRAY_WRITE`
      pointer bases, exposing the pointer variable as a short 16-bit assignment
      temp with debug output:
      `register_allocator: array_write_pointer_base_temp function=... label=... r...`.
- [x] Preserve assignment-fed `ARRAY_WRITE result` temps through optimizer
      usage accounting and final dead-assignment removal by treating
      `ARRAY_WRITE result` as a read, matching allocator liveness.
- [x] Mark no-spill retained intervals on the producer and consumer TACs so a
      later same-consumer candidate can see that `arg2` is already retained in
      `HL` before choosing the alternate `BC` base path.
- [x] Allow `TAC_OP_ASSIGNMENT -> ARRAY_WRITE result` in `HL`, and allow the
      alternate `BC` choice only when the same consumer already keeps computed
      `arg2` in `HL`.
- [x] Teach `_generate_asm_array_write_z80()` to handle retained `BC` bases
      with nonzero indexes by copying the base to `IY` before materializing the
      index into `BC`; retained `BC` base plus retained `BC` index remains a
      loud backend error.
- [x] Add `register_allocator: array_write_index ...` debug output so pointer
      write base/index schedules can be checked the same way as pointer reads.
- [x] Add `tests/z80/sms/allocator_ra_array_write_bc_base_hl_index` with a
      lowercase `makefile`, `main.ssc`, normal compiler/linker flow, allocator
      debug greps, backend comment greps, and byte-tester pins.
- [x] Read the new debug output and verify the expected split:
      pointer-base temp emitted, `XOR -> ARRAY_WRITE arg2` retained in `HL`,
      `primary=HL alternate=BC`, retained `ASSIGNMENT -> ARRAY_WRITE result`,
      `resolve ... operand=result kind=PHY_BC`,
      `array_write_base ... kind=PHY_BC memory_pointer_load=no`, and
      `array_write_index ... kind=PHY_HL`.
- [x] Verify focused regressions:
      `allocator_ra_array_write_bc_base_hl_index`,
      `allocator_ra_array_write_pointer_bases`,
      `allocator_ra_array_write_pointer_values`,
      `allocator_ra_array_write_bases`,
      `allocator_ra_array_write_indexes`,
      `allocator_ra_array_read_bc_base_hl_index`,
      `allocator_ra_array_read_pointer_bases`,
      `allocator_ra_get_address_bc_base_hl_index`, and
      `allocator_ra_assignment_producer_hl`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (82 tests)`.

This phase extends the two-register pointer-base schedule to the remaining
known 16-bit computed-index pointer-write shape. Pointer writes with computed
16-bit indexes can now keep the base in `BC` while the index stays in `HL`, and
the optimization remains opt-in behind `-ra`.

---

## 41. Inline-Asm Operand Location Contract

- [x] Keep `TAC_OP_ASM` as a hard allocator boundary and clobber-all TAC:
      `A|BC|DE|HL|IX|IY|SP|FLAGS`.
- [x] Add explicit allocator debug for inline asm:
      `register_allocator: inline_asm_contract ... policy=hard_barrier operand_policy=stack_location ...`.
- [x] Route inline-asm `(@var)` read/write operand lookup through
      `z80_location` instead of direct `find_stack_offset()` calls.
- [x] Keep the operand policy conservative by forcing inline-asm variables to
      stack/global label locations with `Z80_PHY_NONE`; inline asm does not
      consume retained temps directly until explicit operand/clobber constraints
      exist.
- [x] Add `register_allocator: inline_asm_operand ... kind=STACK_LOCAL ...`
      debug output for both reads and writes so the stack-location policy can be
      checked from `compiler.log`.
- [x] Add `tests/z80/sms/allocator_ra_inline_asm_barrier` with a lowercase
      `makefile`, `main.ssc`, normal compiler/linker flow, allocator debug
      greps, clobber greps, negative retained-stack diagnostics, and
      byte-tester pins for `A` and `HL` inline-asm read/write forms.
- [x] Read the new debug output and verify that inline asm reports a hard
      barrier, all-register clobbers, stack-location operands, no retained
      operand locations, and no `stack offset requested for retained`
      diagnostic.
- [x] Verify focused regressions:
      `allocator_ra_inline_asm_barrier`,
      `allocator_ra_frame_no_spill_a`,
      `allocator_ra_frame_no_spill_hl`,
      `allocator_ra_find_stack_guard_a`,
      `allocator_ra_find_stack_guard_hl`,
      `allocator_ra_basic_blocks_debug`,
      `allocator_ra_bc_clobber_debug`, and
      `allocator_ra_array_write_bc_base_hl_index`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (83 tests)`.

This phase removes the direct stack-offset dependency from the inline-asm
variable operand path while intentionally preserving inline asm as a
conservative hard barrier. Future inline-asm allocator work should add an
explicit operand/clobber constraint language before allowing retained physical
registers to flow across or into asm blocks.

---

## 42. Array Initializer Copy Location Contract

- [x] Migrate the non-constant local array initializer bulk-copy helper from a
      direct `find_stack_offset()` call to `z80_location` resolution with
      `Z80_PHY_NONE`.
- [x] Add `_load_location_address_to_hl()` so stack/global addresses can be
      materialized through the location abstraction for helper calls that expect
      their destination pointer in `HL`.
- [x] Add allocator debug for the path:
      `register_allocator: array_init_copy ... kind=... bytes=... policy=location_address`.
- [x] Keep the path conservative: array initializer copy targets must be
      stack/global addresses and must not resolve to a retained physical
      register.
- [x] Add `tests/z80/sms/allocator_ra_array_init_copy_location` with byte and
      word local array initializer coverage, debug greps, a `copy_bytes_bank_`
      backend grep, negative retained-stack diagnostics, and byte-tester pins.
- [x] Read the new debug output and verify both `u8[6]` and `u16[6]` initializer
      targets resolve as `STACK_LOCAL` with `phy=NONE`, byte counts `6` and
      `12`, and no `stack offset requested for retained` diagnostic.
- [x] Verify focused regressions:
      `allocator_ra_array_init_copy_location`,
      `allocator_ra_find_stack_guard_a`,
      `allocator_ra_find_stack_guard_hl`,
      `allocator_ra_inline_asm_barrier`,
      `allocator_ra_array_read_pointer_bases`, and
      `allocator_ra_array_write_pointer_bases`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (84 tests)`.

This phase closes the remaining direct backend stack-offset consumer outside
the central `z80_location` resolver. Local array initializer bulk copies now use
the same address-location contract as migrated inline asm, array read/write,
and pointer-base paths, while still keeping retained registers out of address
only helper calls.

---

## 43. Narrow 8-Bit C Retention For A-Clobber Gaps

- [x] Add `LOC_PHY_C` to the Z80 location abstraction and teach core
      materialize/store helpers to move byte values between `A` and `C`.
- [x] Teach the 8-bit arithmetic emitter to produce retained results in `C`
      and consume retained `arg1`/`arg2` from `C` without disturbing an already
      retained operand in `A`.
- [x] Add conservative allocator candidate logic for `C`: only 8-bit
      arithmetic temps feeding arithmetic consumers, with `SUB` limited to
      retained `arg2`.
- [x] Add a C-transparent path check for intervening 8-bit arithmetic TACs
      that do not use retained `BC`/`C`, allowing `C` to bridge cases where
      `A` would be clobbered.
- [x] Add `linear_scan overlap_check` and `linear_scan alternate ... primary=A
      alternate=C` debug coverage for both clobber-between and active-register
      conflict decisions.
- [x] Add `tests/z80/sms/allocator_ra_c_retention_a_conflict` with debug greps,
      retained comment greps, negative retained-stack diagnostics, and
      byte-tester pins for the new `C` path.
- [x] Update existing focused A-conflict/farther-next-use regressions whose
      byte output improves because 8-bit temps can now remain in `C` instead of
      spilling.
- [x] Verify focused regressions:
      `allocator_ra_c_retention_a_conflict`,
      `allocator_ra_next_arg2_a`,
      `allocator_ra_arithmetic_locations_a`,
      `allocator_ra_bc_retention_hl_conflict`,
      `allocator_ra_linear_scan_conflict_a`,
      `allocator_ra_linear_scan_farther_next_use`, and
      `allocator_ra_find_stack_guard_a`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (85 tests)`.

This phase is the first narrow `B`/`C`-family expansion beyond whole `BC`.
It deliberately starts with `C` only, because the migrated 8-bit arithmetic
emitter can preserve and consume `C` without changing the existing `A`/`HL`/`BC`
contracts. Broader `B`/`C` use still needs per-emitter clobber audits.

---

## 44. Narrow 8-Bit B Retention After C Conflicts

- [x] Add `LOC_PHY_B` to the Z80 location abstraction and teach core
      materialize/store helpers to move byte values between `A` and `B`.
- [x] Teach the 8-bit arithmetic emitter to produce retained results in `B`
      and consume retained `arg1`/`arg2` from `B` without disturbing an already
      retained operand in `A`.
- [x] Add conservative allocator candidate logic for `B`: only 8-bit
      arithmetic temps feeding arithmetic consumers, with `SUB` limited to
      retained `arg2`.
- [x] Add a B-transparent path check for intervening 8-bit arithmetic TACs
      that do not use retained `BC`/`B`, allowing `B` to bridge cases where
      `A` would be clobbered and `C` is unavailable.
- [x] Extend linear-scan active interval tracking for `B` and add `primary=A
      alternate=B` debug coverage for clobber-between and active-register
      conflict decisions.
- [x] Add `tests/z80/sms/allocator_ra_b_retention_c_conflict` with debug
      greps proving `C` conflicts first, `B` is conflict-free, `PHY_B` resolves
      in pass 6, `XOR A,B` is emitted, and byte-tester pins stay exact.
- [x] Verify focused regressions:
      `allocator_ra_b_retention_c_conflict`,
      `allocator_ra_c_retention_a_conflict`,
      `allocator_ra_linear_scan_conflict_a`,
      `allocator_ra_linear_scan_farther_next_use`,
      `allocator_ra_bc_retention_hl_conflict`,
      `allocator_ra_array_read_bc_base_hl_index`,
      `allocator_ra_array_write_bc_base_hl_index`, and
      `allocator_ra_find_stack_guard_a`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report `DONE (86 tests)`.

This phase completes the first intentionally tiny single-byte `B`/`C` pair:
`C` remains the preferred A-clobber alternate, while `B` is used only after
overlap checks prove `C` is occupied and `B` does not overlap an active `BC` or
`B` interval. Broader use of `B`/`C` outside 8-bit arithmetic still needs exact
emitter-by-emitter audits.

---

## 45. Narrow 8-Bit C/B Index Retention

- [x] Allow conservative single-byte `C`/`B` candidates for arithmetic temps
      consumed as `arg2` indexes by `ARRAY_WRITE`, `ARRAY_READ`, and
      `GET_ADDRESS_ARRAY`.
- [x] Keep the producer restriction narrow: only 8-bit `ADD`, `SUB`, `AND`,
      `OR`, and `XOR` temps can use this path.
- [x] Reuse the existing `B`/`C` transparency checks so only intervening TACs
      that do not clobber or overlap the retained single-byte register can be
      crossed.
- [x] Verify the `ARRAY_WRITE arg2` case where the index moves from `A` to
      `C` across an intervening value `ADD`, while the stored value remains
      retained in `A`.
- [x] Add `tests/z80/sms/allocator_ra_index_c_retention_a_conflict` with
      debug greps proving `primary=A alternate=C reason=clobber_between`,
      `PHY_C` index resolution, `PHY_A` value resolution, retained ASM
      comments, and exact byte-tester output.
- [x] Verify focused regressions:
      `allocator_ra_index_c_retention_a_conflict`,
      `allocator_ra_b_retention_c_conflict`,
      `allocator_ra_c_retention_a_conflict`,
      `allocator_ra_array_write_indexes`,
      `allocator_ra_array_read_indexes`,
      `allocator_ra_get_address_indexes`,
      `allocator_ra_z80_in_ports`,
      `allocator_ra_linear_scan_conflict_a`,
      `allocator_ra_linear_scan_farther_next_use`, and
      `allocator_ra_find_stack_guard_a`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report the updated test count.

This phase broadens the Phase 43/44 single-byte model from arithmetic-only
consumers to index/port-like consumers whose emitters already materialize
`B`/`C` through `z80_location`. The first regression pins `C`; `B` remains the
secondary fallback when `C` overlaps an active interval.

---

## 46. Z80 Out Port Retention Debug

- [x] Add allocator debug to `_generate_asm_z80_out_write_z80()` for the
      resolved output port and output value locations.
- [x] Preserve the existing `-ra` gate for this debug; default/no-allocator
      builds do not make this a separate behavior path.
- [x] Add `tests/z80/sms/allocator_ra_z80_out_c_retention_a_conflict` proving
      an 8-bit computed `__z80_out` port falls back from `A` to `C` across the
      intervening output-value `ADD`, while that output value remains in `A`.
- [x] Pin debug greps for `z80_out_port kind=PHY_C`, `z80_out_value
      kind=PHY_A`, retained `ARRAY_WRITE arg2`/`arg1` linear-scan decisions,
      retained ASM comments, `OUT (C),A`, and no retained stack-offset lookup.
- [x] Verify the new ROM byte range with `byte_tester -s main.ssc`.
- [x] Read `compiler.log` and `main.asm` to verify the generated sequence:
      port add retained in `C`, value add retained in `A`, and `OUT (C),A`.
- [x] Verify focused regressions:
      `allocator_ra_z80_out_c_retention_a_conflict`,
      `allocator_ra_index_c_retention_a_conflict`,
      `allocator_ra_array_write_indexes`,
      `allocator_ra_z80_in_ports`,
      `allocator_ra_array_write_values`,
      `allocator_ra_b_retention_c_conflict`,
      `allocator_ra_c_retention_a_conflict`, and
      `allocator_ra_find_stack_guard_a`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report the updated test count.

This phase does not broaden allocation policy; it closes the `__z80_out`
observability gap left by the array-write override and pins the port-specific
consumer path that Phase 45 made eligible for retained `C` indexes.

---

## 47. Pointer Array Smoke Audit

- [x] Add a combined pointer-heavy SMS regression covering pointer-to-pointer
      read, pointer-indexed write, pointer-indexed read, pointer-to-pointer
      write-back, and a global sink assignment in one function.
- [x] Keep allocator behavior opt-in through `-ra`; this phase adds no default
      behavior path and does not broaden allocation policy.
- [x] Pin debug greps proving both `ARRAY_WRITE` emitters are migrated,
      pointer bases load through `memory_pointer_load=yes`, the 8-bit store
      value remains in `A`, the pointer update value remains in `HL`, and the
      computed read index/result path remains in `HL`.
- [x] Pin generated ASM comments for retained `A` and `HL` operands/results,
      plus exact ROM bytes with `byte_tester -s main.ssc`.
- [x] Read `compiler.log` and `main.asm` to verify the generated sequence:
      `cursor[0]` is read through a stack pointer load, `ptr[index]` stores a
      retained `A` value, `source[index]` consumes a retained `HL` index, and
      `cursor[0] = ptr + 1` stores a retained `HL` pointer value.
- [x] Verify focused regressions:
      `allocator_ra_pointer_array_smoke`,
      `allocator_ra_array_write_pointer_values`,
      `allocator_ra_array_write_pointer_bases`,
      `allocator_ra_array_read_pointer_bases`,
      `allocator_ra_get_address_pointer_bases`,
      `allocator_ra_array_read_bc_base_hl_index`,
      `allocator_ra_array_write_bc_base_hl_index`, and
      `allocator_ra_find_stack_guard_a`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report the updated test count.

This phase is a coverage and audit slice, not a new allocator-policy slice. It
locks down a compact real-world pointer pattern similar to the existing
`calculations` pointer-to-pointer cursor update, and keeps the broader
pointer/indirect audit open for remaining struct/dereference/helper paths.

---

## 48. Struct Array Field Address Debug

- [x] Add allocator debug to `generate_il_calculate_struct_access_address()`
      so struct access helper-only address paths print the current function,
      root symbol, root access kind (`direct`, `array`, or `pointer`), result
      temp, and final field type while `-ra` is enabled.
- [x] Keep the debug behind `g_allocator_enabled`; default/no-allocator builds
      do not gain a new diagnostic path.
- [x] Add `tests/z80/sms/allocator_ra_struct_array_fields` covering
      `struct_array[index].byte_value` writes and
      `struct_array[index].word_value` reads.
- [x] Pin debug greps for `struct_access_address ... access=array`, migrated
      `ARRAY_WRITE`, retained `HL` address arithmetic, `PHY_HL` array bases,
      and no retained stack-offset lookup.
- [x] Read `compiler.log` and `main.asm` to verify the generated sequence:
      struct-array base address, index scaling by struct size, field-offset
      addition, retained `HL` base consumed by `ARRAY_WRITE`/`ARRAY_READ`, and
      retained `HL` result consumed by the following `XOR`.
- [x] Verify exact ROM bytes for both read and write paths with
      `byte_tester -s main.ssc`.
- [x] Verify focused regressions:
      `allocator_ra_struct_array_fields`,
      `allocator_ra_array_write_bases`,
      `allocator_ra_array_read_computed_bases`,
      `allocator_ra_get_address_computed_bases`,
      `allocator_ra_array_write_pointer_bases`,
      `allocator_ra_array_read_pointer_bases`,
      `allocator_ra_pointer_array_smoke`, and
      `allocator_ra_find_stack_guard_hl`.
- [x] Rebuild the Cygwin compiler, stage it in the path preferred by
      `run_tests.sh`, and verify both full suites report the updated test count.

This phase is an observability and coverage slice for the remaining
pointer/indirect audit. It does not enable cross-block retention or new spill
insertion; it proves a less-common struct-array helper path still resolves
through retained `HL` locations in the migrated array emitters.

---

## 49. Struct Pointer Field Address Debug

- [x] Add allocator debug to the nested `->` path inside
      `generate_il_calculate_struct_access_address()`:
      `register_allocator: struct_access_indirect function=... from_r... to_r... access=pointer`.
- [x] Keep the debug behind `g_allocator_enabled`; default/no-allocator builds
      do not gain a new diagnostic path.
- [x] Add `tests/z80/sms/allocator_ra_struct_pointer_fields` covering a
      supported global `struct *` root plus nested pointer-member dereference:
      `outer_ptr->inner->byte_value` write and
      `outer_ptr->inner->word_value` read.
- [x] Pin debug greps for `struct_access_indirect`, root
      `struct_access_address ... access=pointer`, retained `HL` address
      arithmetic across the nested pointer load, `PHY_HL` array-read/write
      bases, and no retained stack-offset lookup.
- [x] Read `compiler.log` and `main.asm` to verify the generated sequence:
      load the global struct pointer, add the pointer-member offset, read the
      nested pointer through retained `HL`, add the final field offset, and
      consume retained `HL` in the final `ARRAY_WRITE`/`ARRAY_READ` plus `XOR`.
- [x] Verify exact ROM bytes for both read and write paths with
      `byte_tester -s main.ssc`.
- [x] Verify focused regressions:
      `allocator_ra_struct_pointer_fields`,
      `allocator_ra_struct_array_fields`, `allocator_ra_pointer_array_smoke`,
      `allocator_ra_array_write_pointer_values`,
      `allocator_ra_array_write_pointer_bases`,
      `allocator_ra_array_read_pointer_bases`,
      `allocator_ra_get_address_pointer_bases`,
      `allocator_ra_array_read_bc_base_hl_index`,
      `allocator_ra_array_write_bc_base_hl_index`, and
      `allocator_ra_find_stack_guard_hl`.
- [x] Rebuild the Cygwin compiler, stage it in `binaries/` and
      `windows/Compiler/Release/`, and verify both full suites report
      `DONE (91 tests)`.

This phase is another coverage and observability slice for the remaining
pointer/indirect audit. It proves SameSameC can express global struct-pointer
roots and nested pointer-member dereferences, even though `struct Foo *arg`
function parameters remain a parser limitation. No cross-block retention or
new spill insertion is enabled here; the retained values stay block-local and
opt-in behind `-ra`.

---

## 50. Deep Struct Pointer Field Address Debug

- [x] Extend the allocator-gated nested `->` debug inside
      `generate_il_calculate_struct_access_address()` so each indirect hop
      reports its depth and member label:
      `register_allocator: struct_access_indirect function=... depth=... member=... from_r... to_r... access=pointer`.
- [x] Keep the debug behind `g_allocator_enabled`; default/no-allocator builds
      do not gain a new diagnostic path.
- [x] Add `tests/z80/sms/allocator_ra_struct_deep_pointer_fields` covering a
      supported global `struct *` root plus two nested pointer-member
      dereferences: `root_ptr->middle->leaf->byte_value` write and
      `root_ptr->middle->leaf->word_value` read.
- [x] Pin debug greps for both `depth=1 member=middle` and
      `depth=2 member=leaf`, root `struct_access_address ... access=pointer`,
      retained `HL` address arithmetic through both nested pointer loads,
      `PHY_HL` array-read/write bases, and no retained stack-offset lookup.
- [x] Read `compiler.log` and `main.asm` to verify the generated sequence:
      load the global root pointer, add the `middle` pointer-member offset,
      read the middle pointer through retained `HL`, add the `leaf`
      pointer-member offset, read the leaf pointer through retained `HL`, add
      the final field offset, and consume retained `HL` in the final
      `ARRAY_WRITE`/`ARRAY_READ` plus `XOR`.
- [x] Verify exact ROM bytes for both read and write paths with
      `byte_tester -s main.ssc`.
- [x] Verify focused regressions:
      `allocator_ra_struct_deep_pointer_fields`,
      `allocator_ra_struct_pointer_fields`,
      `allocator_ra_struct_array_fields`, `allocator_ra_pointer_array_smoke`,
      `allocator_ra_array_write_pointer_values`,
      `allocator_ra_array_write_pointer_bases`,
      `allocator_ra_array_read_pointer_bases`,
      `allocator_ra_get_address_pointer_bases`,
      `allocator_ra_array_read_bc_base_hl_index`,
      `allocator_ra_array_write_bc_base_hl_index`, and
      `allocator_ra_find_stack_guard_hl`.
- [x] Rebuild the Cygwin compiler, stage it in `binaries/` and
      `windows/Compiler/Release/`, and verify both full suites report
      `DONE (92 tests)`.

This phase narrows the remaining pointer/indirect audit by proving a deeper
global struct-pointer chain with two nested pointer-member loads. It still does
not add cross-block retention, spill/reload insertion, or parser support for
`struct Foo *arg` function parameters; retained values stay block-local and
opt-in behind `-ra`.

---

## 51. Struct Pointer Increment/Decrement Address Reuse

- [x] Add allocator-gated debug to the struct-access update path in
      `_generate_il_create_increment_decrement()`:
      `register_allocator: struct_access_update function=... op=... address_r... value_r... final_type=... address_uses=2`.
- [x] Keep the diagnostic behind `g_allocator_enabled`; verify `-no-ra` does
      not print `struct_access_update`.
- [x] Add `tests/z80/sms/allocator_ra_struct_pointer_increment` covering an
      8-bit post-increment and a 16-bit post-decrement through a supported
      global `root_ptr->leaf->field` chain.
- [x] Read `compiler.log` and verify each final field address has two uses,
      first as `ARRAY_READ arg1` and later as `ARRAY_WRITE result`.
- [x] Pin the current conservative allocation boundary: the field address
      reports `reason=multi_read_interval` and remains stack-backed because
      the intervening array read/update sequence clobbers the currently
      available address registers.
- [x] Verify the loaded field value remains retained in `HL` through
      `ADD`/`SUB` and into `ARRAY_WRITE arg1` while both array emitters resolve
      the spilled address through `z80_location` without a retained stack
      offset request.
- [x] Verify exact ROM bytes for both update widths with
      `byte_tester -s main.ssc`.
- [x] Verify focused regressions:
      `allocator_ra_struct_pointer_increment`,
      `allocator_ra_struct_deep_pointer_fields`,
      `allocator_ra_struct_pointer_fields`,
      `allocator_ra_struct_array_fields`, `allocator_ra_pointer_array_smoke`,
      `allocator_ra_array_write_pointer_bases`,
      `allocator_ra_array_read_pointer_bases`, and
      `allocator_ra_find_stack_guard_hl`.
- [x] Rebuild the Cygwin compiler, stage it in `binaries/` and
      `windows/Compiler/Release/`, and verify both full suites report
      `DONE (93 tests)`.

This phase closes another helper-only pointer audit item without claiming an
unsafe retention optimization. The address is correctly reloaded from its
spill slot for the read and write; removing that traffic requires the planned
multi-read spill/reload work or a physical register that survives the whole
sequence. Direct unary `*ptr` expressions are not a separate supported
lowering form in the current parser; supported pointer dereferences lower from
indexed or struct-access syntax.

---

## 52. First Split-Spill Insertion For Multi-Read HL Bases

- [x] Add explicit per-TAC metadata for an `ARRAY_READ arg1` retained in `HL`
      that must also be preserved to its existing spill slot for later reads.
- [x] Extend block-local linear scan so a 16-bit multi-read interval can retain
      its producer through the first `ARRAY_READ arg1` instead of always
      reporting `reason=multi_read_interval` and skipping allocation.
- [x] Keep this first insertion narrow: only retained `HL` array-read bases are
      eligible; all other multi-read operand/register combinations retain the
      conservative skip behavior.
- [x] Emit pass-5 diagnostics for candidate splitting and insertion:
      `linear_scan split_spill ... preserve=arg1_to_spill` and
      `linear_scan spill_insert ... destination=existing_spill_slot`.
- [x] Extend the Z80 array-read emitter to store retained `HL` into the temp's
      existing stack slot before the read clobbers it, with
      `spill_insert_emit ... spill_offset=... bytes=2` debug and a generated
      `preserve retained arg1 in spill slot` assembly comment.
- [x] Clear preservation metadata if a retained interval is evicted by the
      nearer-next-use policy, and reject a physical-register fallback away
      from `HL` for this specialized insertion.
- [x] Update `allocator_ra_struct_pointer_increment` so both pointer-field
      updates require retained first-read bases, inserted preservation stores,
      later stack-backed writes, and refreshed exact ROM bytes. The optimized
      test ROM is 30 bytes smaller than its Phase 51 baseline.
- [x] Add `tests/z80/sms/allocator_ra_split_spill_hl` covering 8-bit increment
      and 16-bit decrement of computed struct-array elements, including index
      scaling before the split interval.
- [x] Read both tests' `compiler.log` and `main.asm`; verify one preservation
      store per update, `PHY_HL` on the first array read, the matching existing
      spill slot on the later array write, and no retained stack-offset error.
- [x] Pin exact ROM bytes for all four update sequences with `byte_tester`.
- [x] Verify focused regressions:
      `allocator_ra_split_spill_hl`,
      `allocator_ra_struct_pointer_increment`,
      `allocator_ra_struct_deep_pointer_fields`,
      `allocator_ra_struct_pointer_fields`,
      `allocator_ra_struct_array_fields`, `allocator_ra_pointer_array_smoke`,
      `allocator_ra_array_write_pointer_bases`,
      `allocator_ra_array_read_pointer_bases`, and
      `allocator_ra_find_stack_guard_hl`.
- [x] Rebuild the Cygwin compiler, stage it in `binaries/` and
      `windows/Compiler/Release/`, and verify both full suites report
      `DONE (94 tests)`.

This is the allocator's first targeted spill insertion rather than a coverage-
only phase. It splits one safe block-local interval at a known clobbering
consumer while reusing the stack slot already assigned to the multi-read temp.
General interval splitting, reload placement, other operands/registers, and
cross-block values remain future work; `-ra` remains opt-in.

---

## 53. Explicit Split-Spill Reload Into BC

- [x] Add per-TAC metadata for reloading a spilled `ARRAY_WRITE result` base
      into a physical register after a Phase 52 split-spill preservation.
- [x] Add a narrow pass-5 reload-placement step that finds the next zero-index
      `ARRAY_WRITE result` use of the split temp and selects `BC`.
- [x] Keep reload placement conservative: only Phase 52's 16-bit retained
      `HL` array-read bases and later zero-index writes are eligible; non-zero
      indexes, other operand positions, and other physical registers remain
      stack-backed.
- [x] Emit `linear_scan reload_insert ... operand=result phy=BC` diagnostics
      when pass 5 places the reload.
- [x] Extend the Z80 array-write emitter to load the temp's existing spill slot
      into `BC` after index materialization and before base materialization,
      with `reload_insert_emit ... source_offset=... bytes=2 phy=BC` debug and
      a `reload result from spill slot into BC` assembly comment.
- [x] Update `allocator_ra_struct_pointer_increment` and
      `allocator_ra_split_spill_hl` to require the explicit reload diagnostic,
      `PHY_BC` write-base resolution, generated assembly comment, and refreshed
      exact ROM bytes. Each motivating ROM is another 19 bytes smaller than
      its Phase 52 version.
- [x] Read both tests' `compiler.log` and `main.asm`; verify the first read base
      resolves as `PHY_HL`, the later write base resolves as `PHY_BC`, and the
      updated field value remains retained in `HL`.
- [x] Verify focused regressions for the split-spill tests, existing `BC` base
      plus `HL` index paths, pointer-base writes, retained write values, and
      retained-stack guards.
- [x] Rebuild the Cygwin compiler, stage it in `binaries/` and
      `windows/Compiler/Release/`, and verify both full suites report
      `DONE (94 tests)`.

This phase adds the allocator's first explicit reload placement. The reload is
deliberately attached to one known consumer and scheduled around that emitter's
clobber order. General reload placement for non-zero indexes, other operands
and registers, longer intervals, and cross-block values remains future work;
`-ra` remains opt-in.

---

## 54. Narrow C Comparison Operand Retention

- [x] Extend the conservative 8-bit `C` candidate policy to conditional
      `JUMP_EQ`, `JUMP_NEQ`, `JUMP_LT`, `JUMP_GT`, `JUMP_LTE`, and
      `JUMP_GTE` consumers when the retained temp is `arg1`.
- [x] Keep the new role narrow: arithmetic byte producers only, comparison
      `arg1` only, and the existing clobber/overlap checks still decide whether
      `A` can fall back to `C`.
- [x] Extend the 8-bit comparison emitter so a retained `C` left operand and
      retained `A` right operand emit `LD B,A`, `LD A,C`, and `SUB A,B`
      without stack traffic.
- [x] Add allocator-gated `compare_8bit` pass-6 diagnostics reporting the
      operation and both resolved location/physical-register pairs.
- [x] Add `allocator_ra_compare_bc_operands` for retained `C` in `JUMP_EQ
      arg1` while a nested arithmetic interval independently occupies `B`.
- [x] Add `allocator_ra_compare_c_neq_b_overlap` for the matching `JUMP_NEQ
      arg1` path with the same explicit `B`/`C` non-overlap pressure.
- [x] Pin `primary=A alternate=C`, retained `C` comparison operands, retained
      `B` nested XOR operands, `PHY_C`/`PHY_A` comparison resolution,
      `SUB A,B`, no retained stack lookup, and exact ROM bytes in both tests.
- [x] Verify the existing compare arg/location matrix, prior `B`/`C` conflict
      tests, and farther-next-use regression.
- [x] Rebuild and stage the Cygwin compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (96 tests)`.

This phase expands a real backend consumer without broadening the whole byte
register model. Comparison `arg2` in `C`, comparison operands in `B`, and
other non-arithmetic producers remain future emitter/policy work. The two test
functions live in separate ROMs so exact branch-address bytes remain stable;
`-ra` remains opt-in.

---

## 55. Complete C Arg1 Relational Comparison Coverage

- [x] Add readable `JUMP_EQ`, `JUMP_NEQ`, `JUMP_LT`, `JUMP_GT`, `JUMP_LTE`,
      and `JUMP_GTE` names to allocator-gated `compare_8bit` diagnostics while
      retaining the numeric TAC operation for low-level debugging.
- [x] Add `allocator_ra_compare_c_relational` to exercise retained `C` as
      comparison `arg1` for `<`, `>`, `<=`, and `>=` while each nested XOR
      interval independently occupies `B`.
- [x] Assert all four linear-scan decisions, readable operation names,
      `PHY_C`/`PHY_A` location resolution, four retained `B` operands,
      `SUB A,B`, and no retained stack lookup.
- [x] Pin complete exact ROM byte ranges for all four relational forms,
      including their carry, no-carry, zero-or-carry, and zero-or-no-carry
      branch sequences.
- [x] Verify the Phase 54 equality/inequality tests, the existing 8-bit and
      16-bit comparison arg/location matrix, and prior `B`/`C` conflict tests.
- [x] Rebuild and stage the Cygwin compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (97 tests)`.

Together with Phase 54, this completes source-level test coverage for retained
`C` comparison `arg1` across all six conditional operators. Comparison `arg2`
in `C`, comparison operands in `B`, and non-arithmetic byte producers remain
future emitter/policy work; `-ra` remains opt-in.

---

## 56. Retain 8-Bit Complement Results In C And B

- [x] Extend the narrow 8-bit `C` and `B` candidate policies to accept
      `COMPLEMENT` producers while preserving the existing consumer,
      clobber-transparency, and overlap restrictions.
- [x] Extend the 8-bit complement emitter to store its accumulator result
      directly into retained `C` or `B` through `_store_from_a()` and preserve
      the stable retained-result assembly comments.
- [x] Add allocator-gated `complement_8bit` diagnostics reporting resolved
      source and result location/physical-register pairs.
- [x] Add `allocator_ra_complement_bc_results` with one complement forced into
      `C` across an independent arithmetic operation and one forced into `B`
      while a longer `C` interval remains live.
- [x] Assert the `C` selection, the `C` overlap rejection followed by safe
      `B` selection, `PHY_C`/`PHY_B` emitter resolution, retained producer and
      consumer comments, and no retained stack lookup.
- [x] Pin complete exact ROM ranges proving `LD C,A`/`XOR A,C` and
      `LD B,A`/`XOR A,B` without complement-result spill traffic.
- [x] Verify the existing 8-bit and 16-bit complement matrix, prior `B`/`C`
      overlap and comparison tests, and farther-next-use regression.
- [x] Rebuild and stage the Cygwin compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (98 tests)`.

This phase adds the first non-binary-arithmetic producer to the narrow byte
alternate-register model. Assignment, array/function-call results, loads, and
other non-arithmetic producers still require individual emitter and clobber
proofs before they can target `B` or `C`; `-ra` remains opt-in.

---

## 57. Retain 8-Bit Array Read Results In C

- [x] Extend the narrow 8-bit `C` candidate policy to accept `ARRAY_READ`
      producers while preserving the existing consumer, clobber-transparency,
      and overlap restrictions.
- [x] Extend normal array-read emission to copy an 8-bit result from `L`
      directly into retained `C`, and extend `__z80_in` read emission to copy
      its accumulator result directly into retained `C`.
- [x] Add allocator-gated `array_read_result` and `z80_in_result` diagnostics
      reporting resolved result location, size, and physical register.
- [x] Add `allocator_ra_array_read_c_results` with one normal memory read and
      one port read retained in `C` across an independent arithmetic operation.
- [x] Assert both retained `ARRAY_READ -> XOR arg1` decisions, `PHY_C` emitter
      resolution, retained producer and consumer comments, and no retained
      stack lookup.
- [x] Pin complete exact ROM ranges proving `LD C,L`/`XOR A,C` and
      `LD C,A`/`XOR A,C` without array-read-result spill traffic.
- [x] Verify the existing array-read destination matrix, complement `B`/`C`
      results, and farther-next-use regression.
- [x] Rebuild and stage the Cygwin compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (99 tests)`.

Array reads remain excluded from the `B` producer policy: the operation
clobbers `BC`, so a pre-existing retained `C` interval cannot legally overlap
the producer to force a source-reachable `B` fallback. Assignment and
function-call results still require individual source-reachability, emitter,
and clobber proofs; `-ra` remains opt-in.

---

## 58. Fix Mixed-Width Retained HL Array Index Extension

- [x] Reproduce the `move_sprite` failure with byte-identical old/current
      sources and prove the latest compiler without `-ra` emits a ROM
      byte-for-byte identical to the known-working old compiler output.
- [x] Isolate the allocator-only failure to two `update_sprite` array writes:
      an 8-bit index result retained in `HL` copied stale `H` into `B`, while
      the stack-backed path explicitly zero-extended the index.
- [x] Fix `_materialize_array_index_to_bc()` so retained `HL` indexes use the
      TAC argument type: sign-extend `INT8`, zero-extend `UINT8`, and copy `H`
      only for true 16-bit indexes.
- [x] Extend the allocator-gated `array_write_index` diagnostic with the
      semantic TAC index type so mixed-width temp reuse is directly visible.
- [x] Add a mixed-width temp-reuse case to
      `allocator_ra_array_write_indexes`, pin `LD C,L` / `LD B,0` with exact
      ROM bytes, and retain the existing true-16-bit `LD C,L` / `LD B,H`
      coverage.
- [x] Correct exact-byte baselines in `allocator_ra_get_address_indexes` and
      `allocator_ra_pointer_array_smoke`; both had captured the same stale-`H`
      unsigned-index bug in another shared-helper caller.
- [x] Verify array-read, array-write, get-address, port-index, and pointer-array
      focused regressions, then rebuild `move_sprite` and confirm both affected
      writes contain `LD B,0` with no stale-high-byte sequence.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (99 tests)`.

The allocator may conservatively give a reused temp slot a 16-bit physical
container even when one producer/consumer pair is semantically 8-bit. Backend
materialization must therefore use the TAC operand type for extension rather
than infer value width solely from `z80_location.size`; `-ra` remains opt-in.

---

## 59. Retain 8-Bit Function Call Results In C

- [x] Extend the narrow 8-bit `C` candidate policy to accept
      `FUNCTION_CALL_USE_RETURN_VALUE` producers while preserving existing
      consumer, clobber-transparency, and overlap restrictions.
- [x] Extend function-call return emission to copy an 8-bit result from `L`
      directly into retained `C` after caller-frame restoration.
- [x] Extend allocator-gated `function_call_return` diagnostics with semantic
      and promoted result types alongside resolved location and register data.
- [x] Add `allocator_ra_function_call_c_results` with call results retained in
      `C` across independent `ADD` and `AND` operations, then consumed as XOR
      and OR `arg1` respectively.
- [x] Assert both `primary=A alternate=C` decisions, retained producer/consumer
      intervals, `PHY_C` return emission, semantic/promoted `UINT8` types,
      retained comments, and no retained stack lookup.
- [x] Pin complete exact ROM ranges proving caller-frame restoration followed
      by `LD C,L` and direct `XOR A,C` / `OR A,C` consumption.
- [x] Verify the existing `A`/`HL` function-call result matrix, prior array-read
      and complement `C` producers, and farther-next-use behavior.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (100 tests)`.

Call-result comparison was also probed, but current lowering inserts a boolean
initialization assignment between the call and comparison. Its conservative
`BC` clobber correctly forces the call result to stack, so comparison and `B`
call-result forms remain future clobber/policy work; `-ra` remains opt-in.

---

## 60. Preserve C Across Spilled Constant Assignments

- [x] Keep the generic `ASSIGNMENT` clobber descriptor conservative for
      nonconstant sources and retained/global/local destinations.
- [x] Add a path-level transparency exception only for constant assignments to
      temp registers already forced to spill, whose emitter uses immediate
      stores through `IX` without touching `B`, `C`, or `BC`.
- [x] Add allocator-gated `assignment_transparency` diagnostics reporting the
      function, TAC, temp, constant value, preserved register, destination,
      and spill reason.
- [x] Add `allocator_ra_function_call_compare_c` as a standalone ROM so exact
      call/branch relocation bytes remain stable independently of the two
      equal-sized Phase 59 arithmetic functions.
- [x] Assert the generic assignment clobber remains visible, the narrow
      constant-to-spill exception preserves `C`, and the call result reaches
      `JUMP_EQ arg1` in `PHY_C` against nested arithmetic in `PHY_A`.
- [x] Pin the complete exact ROM range proving spilled boolean initialization,
      `LD C,L`, direct `SUB A,C` comparison, both branches, and sink storage.
- [x] Verify the signed nonconstant assignment clobber audit, existing call
      results, retained relational comparisons, and farther-next-use behavior.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (101 tests)`.

The exception relies on an existing non-`NONE` spill reason and no assigned
physical destination, so it cannot silently make ordinary assignments
transparent. Broader assignment clobber refinement still requires equivalent
destination-specific emitter proofs; `-ra` remains opt-in.

---

## 61. Retain 8-Bit MUL, DIV, And MOD Results In C

- [x] Extend the narrow 8-bit `C` candidate policy to accept `MUL`, `DIV`, and
      `MOD` producers while preserving existing consumer, transparency, and
      overlap restrictions.
- [x] Extend the 8-bit emitter after `DE` restoration to copy multiply results
      from `L`, division results from `A`, and modulo results from `B` into
      retained `C`.
- [x] Add allocator-gated `mul_div_mod_result` diagnostics reporting operation,
      resolved location/register, size, semantic type, and promoted type.
- [x] Use the diagnostics to catch and correct an initial placement in the
      analogous 16-bit helper before linking or accepting the regression.
- [x] Add `allocator_ra_mul_div_mod_c_results` with three independently marked
      blocks in one function: multiply/XOR, divide/OR, and modulo/XOR, each
      retaining the producer in `C` across nested arithmetic in `A`.
- [x] Assert all three retained intervals and result diagnostics, operation-
      specific `LD C,L` / `LD C,A` / `LD C,B` transfers, retained consumer
      comments, and no retained stack lookup.
- [x] Pin complete exact ROM ranges for all three arithmetic implementations
      and their direct `C` consumers.
- [x] Verify the existing 8/16-bit result matrix, both retained operand
      matrices, complement `B`/`C`, and call-result comparison regressions.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (102 tests)`.

`B` result retention remains excluded: each operation itself clobbers `BC`, so
there is no legal pre-existing `C` interval that can force a source-reachable
`B` fallback. Wider and signed result semantics remain on their established
`HL`/`BC` paths; `-ra` remains opt-in.

---

## 62. Retain 8-Bit Shift Results In C

- [x] Extend the narrow 8-bit `C` candidate policy to accept `SHIFT_LEFT` and
      `SHIFT_RIGHT` producers while preserving existing consumer, clobber,
      transparency, and overlap restrictions.
- [x] Extend the 8-bit shift emitter to copy its final result from `B` into
      retained `C` after the shift loop.
- [x] Add allocator-gated `shift_result` diagnostics reporting operation,
      resolved location/register, size, semantic type, and promoted type.
- [x] Add `allocator_ra_shift_c_results` with independently marked left-shift/
      XOR and right-shift/OR blocks, each keeping the shift result live in `C`
      while nested arithmetic occupies `A`.
- [x] Assert both alternate/retained intervals, both `PHY_C` result diagnostics,
      `LD C,B`, direct retained consumers, and no retained stack lookup.
- [x] Pin complete exact ROM ranges for both shift loops and their direct `C`
      consumers.
- [x] Verify all existing shift-count/location suites plus complex-arithmetic,
      complement, and call-result `C` regressions.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (103 tests)`.

The shift loop owns `B` and `A`, so copying the completed byte to `C` is safe
only after the loop. `B` result retention remains excluded because the result
already occupies `B` and shift execution clobbers `BC`; `-ra` remains opt-in.

---

## 63. Retain Computed 8-Bit Shift Values In C

- [x] Extend the narrow 8-bit `C` consumer policy so a computed
      `SHIFT_LEFT` or `SHIFT_RIGHT` `arg1` can survive the later count
      computation without spilling.
- [x] Extend the 8-bit shift emitter to accept retained `arg1` in `C` and copy
      it directly to the loop's working `B` register with `LD B,C`.
- [x] Add allocator-gated `shift_value` diagnostics reporting the resolved
      value location, size, and physical register alongside `shift_count`.
- [x] Add `allocator_ra_shift_values_c` with independently marked computed
      left- and right-shift cases: `ADD`/`XOR` values retained in `C` while
      later `ADD` counts occupy `A`.
- [x] Assert both `primary=A alternate=C` decisions, retained `SHIFT_* arg1`
      intervals, `PHY_C` value diagnostics, `PHY_A` count diagnostics,
      `LD B,C`, and no retained stack lookup.
- [x] Pin complete 47-byte exact ROM ranges for both optimized shift loops.
- [x] Verify all existing shift result/count/location suites plus complement
      and complex-arithmetic `C` regressions.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (104 tests)`.

Source probes also confirmed that ordinary comparison lowering leaves the
later `arg2` producer in `A`, so no unproved comparison-`C` policy was kept.
Likewise, shift-result `B` fallback remains unreachable because the producer
itself clobbers `BC`; `-ra` remains opt-in.

---

## 64. Audit Indexed Arrays Of Struct Pointers

- [x] Add allocator-gated `root_kind` detail to `struct_access_address` so
      direct structs, struct pointers, struct arrays, and arrays of struct
      pointers are distinguishable in lowering diagnostics.
- [x] Add `allocator_ra_struct_pointer_array` with computed-index writes and
      reads through `struct_pointer_array[index]->member`.
- [x] Assert `access=array root_kind=struct_pointer_array` and the following
      pointer dereference at depth one for both functions.
- [x] Assert the retained `HL` chain across root `GET_ADDRESS`, index scaling,
      pointer-table `ARRAY_READ`, member offset, final `ARRAY_WRITE` base, and
      final value `ARRAY_READ`/`XOR` consumer.
- [x] Assert physical `HL` read/write bases, no unmigrated array emitter, and
      no retained stack lookup.
- [x] Pin complete 68-byte write and 95-byte read ROM regions, including index
      computation, pointer lookup, member addressing, and value transfer.
- [x] Verify global and deep struct-pointer fields, struct arrays, pointer-array
      smoke coverage, and pointer-base array reads/writes.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (105 tests)`.

SameSameC currently rejects `struct Name *argument` in function signatures and
does not support typedefs, so pointer-to-struct function arguments have no
source-reachable allocator path. This phase covers the supported indexed
array-of-struct-pointers form instead; `-ra` remains opt-in.

---

## 65. Split And Reload Indexed Struct-Pointer Updates

- [x] Extend allocator-gated `struct_access_update` diagnostics to state the
      helper lowering contract: two address uses, zero-index read/write, and
      split-spill eligibility.
- [x] Add `allocator_ra_struct_pointer_array_update` with computed-index byte
      increment and 16-bit decrement through
      `struct_pointer_array[index]->member`.
- [x] Assert pointer-array root classification, depth-one pointer lookup, and
      `split_candidate=yes` for both helper-generated updates.
- [x] Assert pass 5 retains each final member address in `HL`, preserves it to
      its existing spill slot before the read, and reloads it into `BC` for the
      later write.
- [x] Assert pass 6 emits matching `spill_insert_emit` and
      `reload_insert_emit` offsets, resolves the read base in `HL`, and resolves
      the write base in `BC` without a retained stack lookup.
- [x] Pin complete 79-byte increment and 84-byte decrement ROM regions,
      including computed index scaling, pointer lookup, member addressing,
      preservation, update, reload, and final store.
- [x] Verify the base pointer-array test, global pointer updates, the original
      split-spill regression, and shallow/deep struct field regressions.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (106 tests)`.

This closes the supported helper-update form for computed arrays of struct
pointers using the existing narrow Phase 52/53 split/reload machinery. General
non-zero-index reload placement and unrelated multi-read forms remain future
work; `-ra` remains opt-in.

---

## 66. Audit Indexed Arrays Of Union Pointers

- [x] Correct allocator root diagnostics so unions report `union`,
      `union_pointer`, `union_array`, or `union_pointer_array` instead of being
      mislabeled as struct roots.
- [x] Add `allocator_ra_union_pointer_array` with computed-index byte writes,
      16-bit reads/XORs, byte increments, and 16-bit decrements.
- [x] Assert all four roots report `root_kind=union_pointer_array` and all four
      pointer-table lookups report depth-one indirect access.
- [x] Assert ordinary write/read paths retain root address, scaled index,
      pointer lookup, final base, and read result through `HL`.
- [x] Assert update helpers report `split_candidate=yes`, preserve the member
      address from `HL`, and reload it into `BC` for the write.
- [x] Assert matching pass-6 preservation/reload offsets, physical `HL`/`BC`
      bases, and no retained stack lookup.
- [x] Pin complete 67-byte write, 90-byte read, 78-byte increment, and 80-byte
      decrement ROM regions.
- [x] Verify struct pointer arrays, struct pointer updates, global pointer
      updates, the original split-spill regression, and deep pointer fields.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (107 tests)`.

This closes the equivalent supported union-pointer array and helper-update
forms while preserving the same target-specific Z80 allocation policy. The
target-independent core and future 6502 policy remain explicit rollout work;
`-ra` remains opt-in.

---

## 67. Prefer Retained Values At Indexed Writes

- [x] Add a linear-scan tie-break for an 8-bit `ARRAY_WRITE` whose value and
      index have the same next-use TAC: retain `arg1` in `A` and spill the
      displaced `arg2` index because pass 6 can preserve `A` while
      materializing the index into `BC`.
- [x] Emit `linear_scan prefer_array_write_value` with the displaced and
      retained operands, physical register, consumer TAC, and
      `reason=shared_next_use`.
- [x] Add `allocator_ra_split_reload_nonzero_index` with computed pointer-array
      roots and explicit computed-index read-modify-write forms for an 8-bit
      increment and a 16-bit decrement.
- [x] Assert the byte update retains the final `ADD` result in `A`, resolves
      the displaced index from its spill slot, and writes directly from `A`.
- [x] Use the word update as a paired control that retains the read, `SUB`, and
      final write value in `HL` while loading the independent index from its
      spill slot.
- [x] Pin complete 76-byte byte-update and 85-byte word-update ROM regions and
      assert no retained stack lookup.
- [x] Verify pointer-base writes, the original split-spill test, struct/union
      pointer-array updates, and the pointer-array smoke regression.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (108 tests)`.

Plain `pointer[index]++` and `pointer[index]--` are rejected by the parser, so
the regression uses the supported explicit read-modify-write form. That form
lowers separate index expressions rather than a shared multi-read base temp;
the new tie-break optimizes its source-reachable write-value conflict without
pretending to implement general non-zero-index split/reload insertion. `-ra`
remains opt-in.

---

## 68. Start The Z80 Target-Policy Interface

- [x] Add `register_allocator_target_policy` and route physical-register unit
      lookup through its `get_physical_register_units` hook.
- [x] Move the `A`, `HL`, and overlapping `BC(B,C)` unit mapping behind the
      Z80 policy without changing linear-scan selection or overlap semantics.
- [x] Fail closed when overlap is queried without a target policy and fail the
      allocator scan with a clear diagnostic when an enabled backend has no
      policy.
- [x] Emit allocator-gated `target_policy` diagnostics per function with the
      target, active hook, and overlap model.
- [x] Add `allocator_ra_target_policy_z80` with an 8-bit `C`/`B` conflict case
      and a 16-bit `HL`/`BC` independence case.
- [x] Assert policy selection, unit names, overlap decisions, alternate
      register choices, physical result locations, and no retained stack
      lookup.
- [x] Pin complete 56-byte and 62-byte ROM regions for the two policy cases.
- [x] Verify the original `B`/`C`, `BC`/`HL`, and index/`A` conflict suites
      remain byte-identical.
- [x] Rebuild and stage the compiler in `binaries/` and
      `windows/Compiler/Release/`; verify both full suites report
      `DONE (109 tests)`.

This is the first target-policy extraction slice, not the completed interface.
Candidate legality, instruction clobbers, calling conventions, spill/reload
constraints, and location materialization remain Z80-specific rollout work.
The allocator still runs only under the opt-in `-ra` flag.

---

## 69. Route Candidate Legality Through The Z80 Policy

- [x] Add `is_candidate_allowed_for_physical_register` to
      `register_allocator_target_policy`.
- [x] Move the existing `A`/`HL`/`BC`/`B`/`C` producer, consumer, size, and
      operand-position rules behind the Z80 policy without changing them.
- [x] Keep the target-independent linear-scan call sites on one fail-closed
      policy wrapper instead of calling Z80 legality code directly.
- [x] Extend `target_policy` diagnostics to report both
      `physical_register_units` and `candidate_legality` hooks.
- [x] Extend `allocator_ra_target_policy_z80` assertions while preserving its
      exact 56-byte and 62-byte ROM regions.
- [x] Verify `A` conflicts, `B`/`C` alternates, `BC`/`HL` alternates,
      comparison operands, and array-read base legality with existing
      exact-ROM suites.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (109 tests)`.

Candidate selection now enters Z80-specific legality through the policy.
Instruction clobbers, calling conventions, spill/reload constraints, and
location materialization remain to be extracted. `-ra` remains opt-in.

---

## 70. Route TAC Clobber Descriptors Through The Z80 Policy

- [x] Add `get_tac_clobbers` to `register_allocator_target_policy` and point
      the Z80 policy at its existing per-op descriptor table.
- [x] Route both `BC` path-safety checks and allocator `z80_clobber` debug
      output through one fail-closed policy wrapper.
- [x] Treat a missing clobber hook as `Z80_TAC_CLOBBER_ALL` so an incomplete
      policy cannot retain values unsafely.
- [x] Extend `target_policy` diagnostics with `tac_clobbers` and
      `clobber_model=descriptor_table`.
- [x] Add `allocator_ra_target_policy_clobbers` covering array reads,
      function calls, complement, and return-value emission.
- [x] Assert array-read `IY` clobbers, all-register call barriers,
      complement preservation of `BC`, return `SP` clobbers, and retained
      `C` result/consumer paths.
- [x] Pin complete 47-byte array-read, 83-byte call, and 38-byte complement
      ROM regions and assert no retained stack lookup.
- [x] Verify existing clobber diagnostics, relational comparisons, shifts,
      non-zero indexed writes, and union-pointer updates remain unchanged.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (110 tests)`.

Per-TAC clobber masks now enter the allocator through the target policy.
Target-specific arithmetic/path transparency exceptions, calling conventions,
spill/reload constraints, and location materialization remain to be extracted.
`-ra` remains opt-in.

---

## 71. Route Per-TAC Transparency Through The Z80 Policy

- [x] Add `is_tac_transparent_for_physical_register` to
      `register_allocator_target_policy` and point the Z80 policy at the
      existing `A`/`HL`/`BC`/`B`/`C` transparency rules.
- [x] Keep candidate-path and basic-block path iteration target-independent;
      route only each TAC/register transparency decision through the policy.
- [x] Fail closed when the policy or transparency hook is missing so an
      incomplete target cannot retain a value across an unclassified TAC.
- [x] Add `target_transparency` diagnostics with function, TAC index, named
      operation, physical register, and the policy's yes/no decision.
- [x] Extend `target_policy` diagnostics with `tac_transparency` and
      `transparency_model=per_tac`.
- [x] Add `allocator_ra_target_policy_transparency` covering byte arithmetic
      paths in `A`, `B`, and `C`, plus transparent bookkeeping and blocking
      arithmetic/complement paths in `HL`.
- [x] Assert positive and negative policy decisions, alternate-register
      selection, conservative interval rejection, retained physical results,
      and no retained stack lookup.
- [x] Pin complete 56-byte byte-expression and 118-byte word-expression ROM
      regions.
- [x] Verify existing target-policy, clobber, relational comparison, shift,
      split/reload, and union-pointer regressions remain unchanged.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (111 tests)`.

Per-TAC path transparency now enters the allocator through the target policy;
generic path iteration and its spill/candidate exceptions remain shared.
Calling conventions, spill/reload constraints, and location materialization
remain target-specific rollout work. `-ra` remains opt-in.

---

## 72. Route Call-Result Registers Through The Z80 Policy

- [x] Add `get_call_result_physical_register` to
      `register_allocator_target_policy` and map 8-bit call results to `A`
      and 16-bit call results to `HL` in the Z80 policy.
- [x] Use the call-result hook only for
      `FUNCTION_CALL_USE_RETURN_VALUE`; leave ordinary arithmetic primary
      register selection unchanged.
- [x] Fail closed to `Z80_PHY_NONE` when the policy or call-result hook is
      missing so an incomplete target cannot retain a call result.
- [x] Add `call_result_convention` diagnostics with function, TAC index,
      result width, target, and selected primary physical register.
- [x] Extend `target_policy` diagnostics with `call_result_register` and
      `call_result_model=size_based`.
- [x] Add `allocator_ra_target_policy_call_results` covering primary 8-bit
      `A`, primary 16-bit `HL`, and an 8-bit result that safely falls back
      from policy-selected `A` to `C` across intervening arithmetic.
- [x] Assert policy decisions, linear-scan primary/alternate choices, emitter
      result locations, retained assembly markers, and no retained stack
      lookup.
- [x] Pin complete 79-byte byte-primary, 105-byte word-primary, and 83-byte
      byte-alternate ROM regions.
- [x] Verify existing primary/alternate call-result, call-comparison,
      call-boundary, clobber, transparency, and overlap-policy regressions.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (112 tests)`.

The allocator's primary physical location for a call result now enters through
the target policy. This is only the first calling-convention hook: argument
layout, stack-return transport, return-value emission, and any target-specific
callee/caller preservation contract remain backend-owned. Spill/reload
constraints and location materialization also remain to be extracted. `-ra`
remains opt-in.

---

## 73. Route Call/Return Spill Boundaries Through The Z80 Policy

- [x] Add `get_call_boundary_spill_reason` to
      `register_allocator_target_policy` and classify void/value calls as
      `function_call` and plain/value returns as `return` in the Z80 policy.
- [x] Keep generic label, inline-asm, and unmigrated-emitter hard barriers
      outside the calling-convention hook.
- [x] Fail closed to `unmigrated_emitter` when the policy or hook is missing
      so an incomplete target cannot retain a live value across a call or
      return boundary.
- [x] Add `target_call_boundary` diagnostics with function, TAC index, named
      call/return operation, target, and selected spill reason.
- [x] Extend `target_policy` diagnostics with `call_boundary_spill` and
      `call_boundary_model=call_return`.
- [x] Add `allocator_ra_target_policy_call_boundaries` covering a void call,
      16-bit value-returning call, plain return, and 16-bit return value.
- [x] Assert all four policy classifications, concrete call/return spill
      metadata, retained `HL` call-result behavior, assembly spill markers,
      and no retained stack lookup.
- [x] Pin complete 62-byte void-call, 105-byte value-call, 37-byte plain-return,
      and 58-byte return-value ROM regions.
- [x] Verify existing A/HL call and return boundaries, call results, basic
      blocks, clobbers, transparency, and overlap-policy regressions.
- [x] Replace the real-project `move_sprite` check with a clean multi-bank
      `calculations` build; inspect 13 policy scans and 34 call/return policy
      decisions in its compiler output.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (113 tests)`.

Call/return hard-boundary spill classification now enters through the target
policy while generic CFG and non-call barriers remain shared. Argument layout,
stack-return transport, return-value emission, and target-specific location
materialization remain backend-owned. `-ra` remains opt-in.

---

## 74. Route Split Spill/Reload Constraints Through The Z80 Policy

- [x] Add `can_preserve_split_spill` and
      `get_split_reload_physical_register` to
      `register_allocator_target_policy`.
- [x] Move the Z80 `ARRAY_READ arg1`, 16-bit, retained-`HL` preservation rule
      behind `can_preserve_split_spill` while keeping generic multi-read
      interval discovery shared.
- [x] Move the zero-index 16-bit `ARRAY_WRITE result` reload into `BC` behind
      `get_split_reload_physical_register`, including the Z80 addressing
      constraint on the consumer TAC.
- [x] Fail closed when either hook is missing: no split preservation and no
      physical reload are inserted for an incomplete target policy.
- [x] Add `target_split_spill` and `target_split_reload` diagnostics with
      function, TAC, operation, operand, width, target, and physical register.
- [x] Extend `target_policy` diagnostics with `split_spill`, `split_reload`,
      and `split_model=preserve_and_reload`.
- [x] Add `allocator_ra_target_policy_split_spill` covering byte-field and
      word-field helper updates plus a non-zero pointer write that must not
      enter the specialized split-policy path.
- [x] Assert policy decisions, generic spill/reload insertion, backend
      materialization, assembly markers, the negative path, and no retained
      stack lookup.
- [x] Pin complete 87-byte byte-field, 86-byte word-field, and 58-byte
      non-zero pointer-write ROM regions.
- [x] Verify existing split spill, struct-pointer update, union-pointer update,
      and all target-policy compatibility suites remain unchanged.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (114 tests)`.

The first narrow split spill/reload constraints now enter through target
policy hooks. General interval splitting, arbitrary reload placement, other
operands/registers, cross-block ranges, and target location materialization
remain unfinished. `-ra` remains opt-in.

---

## 75. Route Stack-Return Reservation Through The Z80 Policy

- [x] Add `get_stack_return_value_end_offset` to
      `register_allocator_target_policy`.
- [x] Move the Z80 caller-frame return-slot reservation for void, 8-bit, and
      16-bit results behind the target policy without changing local or spill
      offsets.
- [x] Fail closed when the policy or hook is missing, or when the Z80 policy
      receives an unsupported return width.
- [x] Add `target_return_transport` diagnostics with function, byte size,
      target, return-slot start, and resulting frame-end offset.
- [x] Extend `target_policy` diagnostics with `stack_return_transport` and
      `stack_return_model=frame_slot`.
- [x] Add `allocator_ra_target_policy_return_transport` covering 8-bit,
      16-bit, and void return layouts.
- [x] Assert target-policy selection, exact stack-frame diagnostics, the void
      no-slot path, policy-failure absence, and no retained stack lookup.
- [x] Pin the existing complete 79-byte byte-call, 105-byte word-call, and
      83-byte alternate-call ROM regions as the compatibility gate.
- [x] Verify all earlier target-policy suites retain their exact ROM and debug
      traces after adding the new hook.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (115 tests)`.

The target policy now owns the amount of caller-frame space reserved for a
stack-return value. Argument placement is migrated in Phase 76. The backend's
concrete argument copies and return-slot loads/stores, the fixed
return-address/frame-pointer prefix, and target location materialization
remain unfinished. `-ra` remains opt-in.

---

## 76. Route Stack-Argument Layout Through The Z80 Policy

- [x] Add `get_stack_argument_end_offset` to
      `register_allocator_target_policy` with the current frame end, argument
      byte size, and zero-based argument index.
- [x] Move declaration-ordered Z80 argument-slot allocation behind the target
      policy while leaving ordinary local-variable allocation generic.
- [x] Fail closed when the policy or hook is missing, or when the Z80 policy
      receives an invalid frame end, argument width, or argument index.
- [x] Add `target_argument_layout` diagnostics with function, one-based
      argument index, byte size, target, slot start, and resulting frame end.
- [x] Extend `target_policy` diagnostics with `stack_argument_layout` and
      `argument_model=contiguous_stack`.
- [x] Add `allocator_ra_target_policy_argument_layout` covering one-byte and
      two-byte single arguments, two byte arguments, two word arguments, and
      three byte arguments.
- [x] Assert all nine policy decisions, the no-argument path, policy-failure
      absence, caller-side copies for arguments one through three, and no
      retained stack lookup.
- [x] Pin complete 79-byte byte-call, 105-byte word-call, and 83-byte
      alternate-call ROM regions, including relocated absolute jump targets
      from the added three-argument caller.
- [x] Verify default/`-no-ra` identity and every earlier target-policy suite
      retain their exact ROM and debug behavior.
- [x] Rebuild and stage the compiler; verify both full suites report
      `DONE (116 tests)`.

The target policy now owns contiguous argument-slot placement below the
target-defined return area. Argument value conversion/copy emission, concrete
return-slot loads/stores, the fixed call-frame prefix, and target location
materialization remain unfinished. `-ra` remains opt-in.

---

## 77. Route Argument Transport Classification Through The Z80 Policy

- [x] Add `get_argument_transport` to
      `register_allocator_target_policy` and expose a fail-closed query to the
      Z80 backend.
- [x] Classify direct byte, direct word, signed 8-to-16 extension, unsigned
      8-to-16 extension, and 16-to-8 truncation in the Z80 policy.
- [x] Route function-call argument load, extension, truncation, and target
      width decisions through the selected transport without changing the
      existing instruction sequence.
- [x] Preserve the compatibility-only high-byte load on stack-backed
      16-to-8 truncation so existing ROM output remains exact.
- [x] Fail closed when the target policy or transport hook is missing, or
      when the source/target type pair is unsupported.
- [x] Add `target_argument_transport` diagnostics with caller, callee,
      one-based argument index, source/target types, source location kind,
      selected transport, and target.
- [x] Extend `target_policy` diagnostics with `argument_transport` and
      `argument_transport_model=width_and_extension`.
- [x] Add `allocator_ra_target_policy_argument_transport` covering computed
      byte and word spills, signed and unsigned widening from stack locals,
      word truncation, and a constant word.
- [x] Assert all six policy decisions, exactly six concrete argument copies,
      policy-failure absence, and no retained stack lookup.
- [x] Pin the complete 128-byte transport ROM region.
- [x] Verify default/`-no-ra` identity and the earlier call-argument and
      target-policy suites retain their exact ROM and debug behavior.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (117 tests)`.

The target policy now decides the semantic transport for each concrete call
argument. The Z80 backend still emits the selected loads, extensions, copies,
and stores. Concrete return-slot loads/stores, the fixed call-frame prefix,
and target location materialization remain unfinished. `-ra` remains opt-in.

---

## 78. Route Concrete Return-Slot Access Through The Z80 Policy

- [x] Add `get_return_value_byte_offset` to
      `register_allocator_target_policy` and expose a fail-closed query to the
      Z80 backend.
- [x] Define the Z80 byte return slot at `-4` and the word low/high bytes at
      `-5`/`-4` in target policy.
- [x] Route callee-side byte/word return stores through policy-selected byte
      offsets without changing instruction emission.
- [x] Route caller-side byte/word return loads through the same policy offsets
      before existing sign/zero extension and destination handling.
- [x] Fail closed when the policy or hook is missing, the return width is not
      one or two bytes, or a required byte offset is unavailable.
- [x] Add `target_return_slot_access` diagnostics for `callee_store` and
      `caller_load` with caller/callee identity, width, target, and low/high
      offsets.
- [x] Extend `target_policy` diagnostics with `return_slot_access` and
      `return_slot_model=byte_offsets`.
- [x] Add `allocator_ra_target_policy_return_slot_access` covering signed
      byte, unsigned byte, and word return values across both sides of each
      call boundary.
- [x] Assert all six access decisions, signed/unsigned caller promotion paths,
      policy-failure absence, and no retained stack lookup.
- [x] Pin complete 64-byte signed-byte, 62-byte unsigned-byte, and 69-byte
      word-call ROM regions.
- [x] Verify default/`-no-ra` identity and the earlier return-transport,
      call-result, argument-transport, and base target-policy suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (118 tests)`.

The target policy now owns the concrete byte offsets used by both sides of
stack-return transport. The Z80 backend still emits the selected loads and
stores. Concrete argument loads/stores, the fixed call-frame prefix, and
target location materialization remain unfinished. `-ra` remains opt-in.

---

## 79. Route The Fixed Call-Frame Prefix Through The Z80 Policy

- [x] Add `get_call_frame_prefix_value` to
      `register_allocator_target_policy` and expose a fail-closed query to
      pass 5 and the Z80 backend.
- [x] Define the Z80 frame-prefix end at `-4`, return-SP offset at `-1`, and
      saved-frame-pointer low/high offsets at `-3`/`-2` in target policy.
- [x] Route pass-5 frame layout and stack-frame diagnostics through the
      policy-owned prefix descriptor.
- [x] Route callee return-SP restoration and caller saved-`DE` restoration
      through policy offsets while preserving the custom push/jump sequence.
- [x] Validate the descriptor fail-closed before layout, call setup, caller
      restore, or callee return emission.
- [x] Add `target_call_frame_prefix` diagnostics for `layout`,
      `caller_setup`, `caller_restore`, and `callee_return` roles.
- [x] Extend `target_policy` diagnostics with `call_frame_prefix` and
      `call_frame_model=return_address_and_saved_fp`.
- [x] Add `allocator_ra_target_policy_call_frame_prefix` covering void,
      byte-result, and word-result calls plus compiler-generated
      `mainmain -> main` setup/restoration.
- [x] Assert source-call setup/restore counts, concrete restore emission,
      policy-failure absence, and no retained stack lookup.
- [x] Pin complete 47-byte void, 57-byte byte-result, and 69-byte word-result
      call regions.
- [x] Verify default/`-no-ra` identity and adjacent call-boundary, argument,
      return, return-slot, and base target-policy suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (119 tests)`.

The target policy now owns the fixed return-address/saved-frame-pointer
prefix used by layout and both sides of the custom call sequence. Concrete
argument loads/stores and target location materialization remain unfinished.
`-ra` remains opt-in.

---

## 80. Route Argument Byte Access Through The Z80 Policy

- [x] Add `get_argument_byte_offset` to
      `register_allocator_target_policy` and expose a fail-closed query to the
      Z80 backend.
- [x] Define logical low/high source and target offsets for byte, word,
      signed extension, unsigned extension, and truncation transports.
- [x] Preserve truncation's historical two-byte source read and one-byte
      destination store so existing exact ROM remains unchanged.
- [x] Validate each transport as `1->1`, `2->2`, `1->2`, or `2->1` before
      emitting a call argument.
- [x] Route stack/global source loads and constant/register destination stores
      through policy-selected byte offsets without changing Z80 instructions.
- [x] Add `target_argument_byte_access` diagnostics with transport and
      source/target low/high offsets for every call argument.
- [x] Extend `target_policy` diagnostics with `argument_byte_access` and
      `argument_byte_model=logical_offsets`.
- [x] Add `allocator_ra_target_policy_argument_byte_access` covering global
      byte/word sources, signed and unsigned widening, truncation, and a
      constant word.
- [x] Assert all six access descriptors, exact decision count, policy-failure
      absence, allocator-only diagnostics, and default/`-no-ra` ROM identity.
- [x] Pin the complete 114-byte global-source call region and retain the
      earlier 128-byte stack/local transport region unchanged.
- [x] Verify the focused argument-byte, argument-transport, and base
      target-policy suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (120 tests)`.

The target policy now owns call-argument transport widths and logical byte
offsets. The Z80 backend still owns its concrete registers and instruction
selection. Target location materialization and other policy hooks remain
unfinished. `-ra` remains opt-in.

---

## 81. Route Target Location Classification Through The Z80 Policy

- [x] Add `get_location_kind` to `register_allocator_target_policy` and
      expose a fail-closed query to the Z80 backend.
- [x] Define target-independent semantic sources for constants, globals,
      stack locals, stack spills, and physical registers.
- [x] Define policy-selected Z80 locations for `CONST`, `GLOBAL`,
      `STACK_LOCAL`, `STACK_SPILL`, `PHY_A`, `PHY_HL`, `PHY_BC`, `PHY_B`,
      and `PHY_C`.
- [x] Route `_resolve_z80_location` and retained-register mapping through the
      target policy without changing offset lookup or instruction emission.
- [x] Fail closed when the policy cannot map a semantic source/physical
      register pair.
- [x] Add `target_location_materialization` diagnostics with function,
      operand, semantic source, target kind, offset, width, physical register,
      and target.
- [x] Extend `target_policy` diagnostics with `location_materialization` and
      `location_model=semantic_source_to_target_kind`.
- [x] Add `allocator_ra_target_policy_location_materialization` covering all
      four memory/value sources and all five Z80 physical locations.
- [x] Assert all nine mappings, exactly 80 source-reachable decisions,
      policy-failure absence, and no retained stack lookup.
- [x] Pin complete 56-, 62-, 48-, 43-, 65-, 18-, and 87-byte regions for
      `B`, `BC`, `C`, `A`, `HL`, constant/global, and spill/call paths.
- [x] Verify base target-policy, array-init address, inline-asm stack operand,
      argument transport, retained array-write value, and flag-off identity
      suites remain exact.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (121 tests)`.

The policy now owns semantic-source to target-location classification. The
Z80 backend still chooses and emits concrete address materialization through
`IX`, `IY`, and `HL`; that strategy is the next narrow extraction boundary.
`-ra` remains opt-in.

---

## 82. Route Shared Address Materialization Strategy Through The Z80 Policy

- [x] Add `get_address_materialization_mode` to the target policy and expose
      a fail-closed query for location kind, address target, and stack offset.
- [x] Define target-independent `IX`/`IY`/`HL` address targets and invalid,
      no-op, global-label, frame-address, and frame-displacement modes.
- [x] Preserve Z80 legality asymmetries: constants are IY no-ops but invalid
      IX/HL addresses, and unsupported physical `B` addresses fail closed.
- [x] Route the shared `_load_location_address_to_ix`,
      `_load_location_address_to_iy`, and `_load_location_address_to_hl`
      helpers through policy-selected modes without changing instructions.
- [x] Keep the IX displacement window exactly `[-128, 126]`; route farther
      stack locals and spills through complete frame-address construction.
- [x] Add allocator-only `target_address_materialization` diagnostics with
      target, location kind, offset, mode, folded displacement, and policy.
- [x] Extend target-policy diagnostics with `address_materialization` and
      `address_model=target_register_location_and_cache_profile`.
- [x] Add `allocator_ra_target_policy_address_materialization` covering IX,
      IY, HL, globals, near/far locals, and near/far spill locations.
- [x] Assert exactly 59 decisions, representative mode counts and offsets,
      policy-failure absence, and all five complete exact-ROM regions.
- [x] Verify the Phase 81 location-policy and base target-policy suites retain
      exact ROMs and updated policy contracts.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (122 tests)`.

The shared address helpers now obtain target-register strategy from the Z80
policy. Specialized inline address sequences in individual emitters still
need auditing and extraction. Concrete Z80 instruction emission remains in
the backend, and `-ra` remains opt-in.

---

## 83. Migrate Equivalent Specialized IX Address Paths

- [x] Audit emitter-local global/frame IX construction that bypasses the
      shared policy-driven address helpers.
- [x] Identify instruction-equivalent branches in `GET_ADDRESS_ARRAY` index
      materialization and 16-bit `MUL`/`DIV`/`MOD` second operands.
- [x] Route both branches through `_load_location_address_to_ix` without
      changing concrete Z80 instruction selection or displacement folding.
- [x] Remove the now-dead emitter-local offset variables and keep the ANSI
      C90 warning set limited to the longstanding unused helpers.
- [x] Add allocator-only `target_specialized_address_materialization`
      diagnostics with function, emitter, operand, kind, offset, selected
      mode, folded displacement, address target, and target policy.
- [x] Add `allocator_ra_target_policy_specialized_address_materialization`
      with direct local/global get-address indexes and direct local/global/far
      16-bit multiply/divide/modulo operands.
- [x] Assert exactly five contextual decisions: two get-address index paths
      and three 16-bit arithmetic paths, including far offset `-153`.
- [x] Pin the complete 56-, 60-, 78-, 107-, and 159-byte ROM regions.
- [x] Verify retained get-address index, retained mul/div/mod arg2, Phase 82
      address-policy, and default/`-no-ra` identity suites remain exact.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (123 tests)`.

Instruction-equivalent specialized IX construction now shares the Phase 82
policy path. The instruction-distinct 8-bit mul/div/mod second-operand path,
function-call old-frame IY caching, and other specialized address sequences
still need separate migration. `-ra` remains opt-in.

---

## 84. Route Uncached 8-Bit Arithmetic IX Materialization Through Policy

- [x] Preserve the 8-bit `MUL`/`DIV`/`MOD` arg2 path's unconditional IX setup
      rather than reusing the cached shared IX helper and changing emitted
      instructions.
- [x] Add the `IX_UNCACHED` address target so cache behavior is explicit in
      the target-policy contract instead of implicit in an emitter-local
      address sequence.
- [x] Route local, global, and far-frame 8-bit arithmetic arg2 addresses
      through `_load_location_address_to_ix_uncached` and policy-selected
      frame-displacement, global-label, or frame-address modes.
- [x] Extend contextual specialized-address diagnostics with
      `address_target`, preserving cached `IX` traces for Phase 83 and proving
      `IX_UNCACHED` for all Phase 84 paths.
- [x] Update the policy diagnostic model to
      `address_model=target_register_location_and_cache_profile`.
- [x] Add `allocator_ra_target_policy_uncached_address_materialization` with
      direct local multiply, global divide, and far-frame modulo operands.
- [x] Assert exactly three generic `IX_UNCACHED` decisions and three matching
      contextual `mul_div_mod_8_arg2` decisions, including far offset `-148`.
- [x] Pin the complete 60-, 62-, and 109-byte ROM regions to prove the
      original unconditional IX instruction sequences remain exact.
- [x] Verify the original mul/div/mod arg2, Phase 83 specialized-address,
      Phase 82 shared-address, and default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (124 tests)`.

The instruction-distinct 8-bit arithmetic path now obtains its address mode
from the target policy without adopting cached IX semantics. Function-call
old-frame IY caching and other specialized address sequences remain separate
migration boundaries. `-ra` remains opt-in.

---

## 85. Route Function-Call Old-Frame IY Caching Through Policy

- [x] Audit the function-call argument path's emitter-local `reset_iy` cache
      and preserve its exact near/far threshold and instruction sequences.
- [x] Add the `IY_OLD_FRAME` address target so call-specific old-frame cache
      behavior is explicit in the target-policy address model.
- [x] Select frame displacement for stack offsets greater than `-127` and
      complete frame-address rebasing for offsets at or below `-127`.
- [x] Route stack-local and stack-spill call argument addresses through
      `_load_call_argument_address_to_iy` while leaving constants, globals,
      and retained physical locations on their existing instruction paths.
- [x] Preserve cache transitions exactly: initialize IY for the first near
      source, reuse it for following near sources, rebase for far sources,
      and require initialization again after a far rebase.
- [x] Add generic `target_address_materialization target=IY_OLD_FRAME`
      diagnostics and contextual `target_call_old_frame_address` diagnostics
      with caller, callee, argument, kind, offset, mode, folded offset, and
      cache state before/action/after.
- [x] Add `allocator_ra_target_policy_call_old_frame_address` covering four
      stack-local and four stack-spill arguments.
- [x] Assert exactly eight generic and eight contextual decisions, including
      exact-boundary offset `-127` and the `initialize`, `reuse`, and `rebase`
      cache actions.
- [x] Pin the complete 77- and 102-byte ROM regions.
- [x] Keep the Phase 82 shared-address count scoped to its original IX/IY/HL
      targets as later address targets add independent diagnostics.
- [x] Verify argument transport, call-frame prefix, shared and uncached
      address policy, function-call result, and default/`-no-ra` identity
      suites remain exact.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (125 tests)`.

Function-call stack argument old-frame IY caching is now selected through the
target policy while concrete cache state and Z80 instruction emission remain
in the backend. Other specialized address sequences still need auditing and
migration. `-ra` remains opt-in.

---

## 86. Route Get-Address Base HL Materialization Through Policy

- [x] Audit the remaining emitter-local global/frame HL construction in
      `GET_ADDRESS` and `GET_ADDRESS_ARRAY` base materialization.
- [x] Route memory-backed get-address `arg1` bases through
      `_load_location_address_to_hl` without changing global-label or
      frame-address instruction sequences.
- [x] Keep retained `HL` and `BC` bases on their existing physical-register
      paths and preserve pointer-value loading after address construction.
- [x] Generalize `target_specialized_address_materialization` diagnostics to
      report the actual operand and add contextual `get_address_base arg1`
      decisions with the `HL` address target.
- [x] Add `allocator_ra_target_policy_get_address_base_materialization`
      covering a global array, retained global-pointer control, near local
      array, and far local array.
- [x] Assert exactly three policy-driven contextual decisions: global label,
      local frame address at offset `-25`, and far frame address at offset
      `-165`.
- [x] Assert the pointer base remains retained in `HL` and does not enter the
      memory-backed policy path.
- [x] Pin the complete 56-, 65-, 57-, and 63-byte ROM regions.
- [x] Scope the Phase 83 specialized decision count to its original index and
      16-bit arithmetic emitters as later specialized emitters add independent
      diagnostics.
- [x] Verify pointer/computed/result/index get-address, shared address policy,
      Phase 83 specialized address, and default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (126 tests)`.

Memory-backed get-address base construction now shares the target-policy HL
address path while retained bases and concrete pointer dereference instructions
remain in the backend. Function-call result stores and other specialized
address sequences remain to be audited. `-ra` remains opt-in.

---

## 87. Route Function-Call Result Stores Through IY Address Policy

- [x] Audit the function-call return-value destination path's emitter-local
      global/frame IY construction.
- [x] Route non-retained call-result destinations through
      `_load_location_address_to_iy` without changing global-label or complete
      frame-address instruction sequences.
- [x] Keep retained `A`, `HL`, `BC`, and `C` call results on their existing
      physical-register paths before address materialization.
- [x] Add contextual `function_call_result_store result` diagnostics with
      location kind, offset, selected mode, folded offset, and `IY` target.
- [x] Add `allocator_ra_target_policy_call_result_store_materialization`
      covering a global word destination, near stack-local word destination,
      far stack-local word destination, and retained-`HL` bypass control.
- [x] Assert exactly three policy-driven result-store decisions: global label,
      local frame address at offset `-9`, and far frame address at offset
      `-149`.
- [x] Assert byte/word result metadata through existing call-result diagnostics
      and verify the retained `HL` path does not enter IY materialization.
- [x] Pin the complete 69-, 105-, 147-, and 105-byte ROM regions.
- [x] Use one word-return callee across the fixture after the full matrix
      exposed platform-dependent generated return-label relocation with mixed
      byte/word callees; verify all four region hashes match under Cygwin and
      native Windows compiler/linker paths.
- [x] Verify function-call `A`/`HL`/`C` results, target-policy call results,
      return transport, call-frame prefix, shared address policy, and
      default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (127 tests)`.

Function-call result destinations now share the target-policy IY address path
while retained call results and concrete return-value stores remain in the
backend. Other specialized address sequences still need auditing. `-ra`
remains opt-in.

---

## 88. Route Global Call-Argument Sources Through IY Address Policy

- [x] Audit function-call argument source addressing and identify the direct
      global-label IY construction as an instruction-equivalent policy bypass.
- [x] Route global call-argument sources through
      `_load_location_address_to_iy` while preserving `source_offset = 0` and
      emitter-owned `reset_iy = YES` old-frame cache invalidation.
- [x] Keep constants, retained physical arguments, and stack-local old-frame
      sources on their existing paths.
- [x] Add contextual `function_call_argument_source argument` diagnostics with
      location kind, selected mode, folded offset, and `IY` target.
- [x] Add `allocator_ra_target_policy_call_global_argument_address` covering
      interleaved near-stack/global sources and global byte/word transport.
- [x] Assert five global-label IY decisions and the byte, word, sign-extension,
      zero-extension, and truncation transport classes.
- [x] Assert stack arguments at offsets `-4`, `-5`, and `-6` each initialize
      old-frame IY because each intervening global-label load resets the cache.
- [x] Pin complete 97- and 80-byte ROM regions and verify matching SHA-256
      hashes under Cygwin and native Windows compiler/linker paths.
- [x] Verify old-frame addressing, argument transport, shared address policy,
      call-frame prefix, and default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (128 tests)`.

Global function-call argument sources now share target-policy IY address
selection without moving concrete argument-copy instructions or cache state
into the policy. The global label load still invalidates cached old-frame IY,
and `-ra` remains opt-in.

---

## 89. Close Array-Initializer Target HL Policy Boundary

- [x] Re-audit local non-constant array-initializer bulk-copy targets and
      confirm they already enter the shared `_load_location_address_to_hl`
      policy path introduced by Phases 42 and 82.
- [x] Propagate `_copy_non_const_array_constants` failure from
      `generate_asm_z80` so an invalid target-policy decision fails assembly
      generation instead of being reported and ignored.
- [x] Add contextual `array_initializer_target target` diagnostics after
      successful policy materialization with function, location, mode, offset,
      folded offset, and `HL` address target.
- [x] Add `allocator_ra_target_policy_array_initializer_target` covering near
      and far stack-local byte and word arrays with runtime initializer values.
- [x] Assert four `HL` frame-address decisions at offsets `-10`, `-17`,
      `-150`, and `-157`, plus matching 6- and 12-byte bulk-copy metadata.
- [x] Assert exactly four `copy_bytes_bank_` calls, no physical initializer
      target, no invalid contextual mode, and no HL helper failure diagnostic.
- [x] Pin complete 42-, 66-, 87-, and 148-byte consumer ROM regions and verify
      matching SHA-256 hashes under Cygwin and native Windows toolchains.
- [x] Extend the Phase 42 initializer suite to assert its two source-reachable
      contextual decisions at offsets `-12` and `-21` without changing ROM.
- [x] Verify shared address policy, get-address base policy, original array
      initializer coverage, and default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (129 tests)`.

Array-initializer bulk-copy targets now have explicit contextual target-policy
evidence and fail closed if HL address materialization is rejected. Concrete
copy setup and instructions remain in the Z80 backend, and `-ra` remains
opt-in.

---

## 90. Trace Array Read/Write Base IY Address Policy

- [x] Audit array-read `arg1` and array-write `result` base materialization and
      confirm memory-backed bases already enter `_load_location_address_to_iy`
      while retained `HL`/`BC` bases bypass memory address construction.
- [x] Add function context to the array-read base helper and emit contextual
      `array_read_base_address arg1` diagnostics after successful IY policy
      selection.
- [x] Add matching `array_write_base_address result` diagnostics after
      successful memory-backed IY policy selection.
- [x] Keep pointer-value loading, direct local-array addressing, retained-base
      moves, and concrete index/value instructions in the Z80 backend.
- [x] Add `allocator_ra_target_policy_array_base_address` covering global,
      near-stack, far-stack, and retained bases across reads and writes.
- [x] Assert four read and three write contextual decisions: global labels,
      near frame offsets `-5`, far pointer offset `-148`, and far local-array
      offset `-151` used by both write and read-back.
- [x] Assert pointer bases report `memory_pointer_load=yes`, the far local
      array reports `memory_pointer_load=no`, and retained `HL` controls emit
      base diagnostics but no contextual address-policy decisions.
- [x] Pin complete 41-, 37-, 43-, 39-, 43-, 48-, 21-, and 19-byte ROM regions
      and verify matching SHA-256 hashes under Cygwin and native Windows paths.
- [x] Extend the original array read/write pointer-base suites with contextual
      policy assertions while preserving their exact ROM regions.
- [x] Verify pointer arrays, split/reload bases, shared address policy, and
      default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (130 tests)`.

Memory-backed array bases now expose which read or write emitter owns each IY
policy decision, while retained bases remain traceable as explicit bypasses.
Pointer dereference remains a concrete backend step after address selection,
and `-ra` remains opt-in.

---

## 91. Trace Inline-Asm Variable IX Address Policy

- [x] Audit inline-asm `(@var)` read/write materialization and confirm Phase 41
      already resolves forced stack/global operands through shared IX policy.
- [x] Add contextual `inline_asm_read_address variable` diagnostics after
      successful source IX materialization.
- [x] Add matching `inline_asm_write_address variable` diagnostics after
      successful target IX materialization.
- [x] Preserve inline asm as a hard allocator barrier with stack/global-only
      `Z80_PHY_NONE` operands and the full `A|BC|DE|HL|IX|IY|SP|FLAGS`
      clobber set.
- [x] Add `allocator_ra_target_policy_inline_asm_address` covering byte and
      word reads/writes for globals, near stack locals, and far stack locals.
- [x] Assert six read and six write decisions: global-label mode, folded frame
      displacements `-6` and `-9`, and full frame addresses `-146` and `-149`.
- [x] Assert byte/word operand size, hard-barrier metadata, no physical inline
      asm operand, and no invalid contextual mode.
- [x] Pin complete 14-, 20-, 6-, 12-, 18-, and 24-byte ROM regions and verify
      matching SHA-256 hashes under Cygwin and native Windows toolchains.
- [x] Extend the original Phase 41 barrier suite with its four contextual IX
      decisions at stack offsets `-8` and `-13` without changing ROM.
- [x] Verify shared/specialized IX policy, call boundaries, the original asm
      barrier, and default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (131 tests)`.

Inline-asm variable operands now expose emitter-owned IX policy decisions
without relaxing the conservative hard-barrier contract. Explicit retained
register operands still require a future operand/clobber constraint language,
and `-ra` remains opt-in.

---

## 92. Trace Z80 Port Operand IX Address Policy

- [x] Audit memory-backed `__z80_in` and `__z80_out` port operands and confirm
      both already route through the shared policy-driven IX helper.
- [x] Add contextual `z80_in_port_address arg2` diagnostics immediately after
      successful memory-backed input-port IX materialization.
- [x] Add matching `z80_out_port_address arg2` diagnostics immediately after
      successful memory-backed output-port IX materialization.
- [x] Preserve constant and retained `A`/`HL`/`C` port paths as explicit
      bypasses that emit no contextual memory-address decision.
- [x] Add `allocator_ra_target_policy_z80_port_address` covering global, near
      stack, and far stack input and output port operands.
- [x] Assert exactly three input and three output decisions: global-label
      mode, folded displacement `-4`, and full frame addresses `-146` and
      `-147`.
- [x] Assert matching byte-sized port-location summaries, no physical
      contextual operand, no invalid policy mode, and no failed IX lookup.
- [x] Pin complete 16-, 18-, 18-, 14-, 18-, and 20-byte ROM regions and verify
      matching SHA-256 hashes under Cygwin and native Windows toolchains.
- [x] Extend retained input/output port suites with negative contextual-trace
      assertions while preserving their existing exact ROM.
- [x] Verify shared/specialized IX policy and default/`-no-ra` identity suites.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (132 tests)`.

Memory-backed Z80 port operands now expose emitter-owned IX policy decisions,
while constants and retained ports continue to bypass address construction.
Concrete `IN`/`OUT` instruction selection remains Z80 backend behavior, and
`-ra` remains opt-in.

---

## 93. Trace Complement Operand IX Address Policy

- [x] Audit 8-bit and 16-bit `COMPLEMENT` memory source/result paths and
      confirm their IX construction already routes through the shared
      policy-driven helper.
- [x] Add contextual `complement_8bit_arg1_address` and
      `complement_8bit_result_address` diagnostics immediately after
      successful source/result IX materialization.
- [x] Add matching `complement_16bit_arg1_address` and
      `complement_16bit_result_address` diagnostics in both normal `HL` and
      retained-`BC` computation branches.
- [x] Add an allocator-only 16-bit complement location summary matching the
      existing 8-bit summary so retained `HL`/`BC` results are observable.
- [x] Add `allocator_ra_target_policy_complement_address` covering global,
      near stack, and far stack byte/word sources and spill results plus
      retained `A` and `HL` results.
- [x] Assert exactly four byte-source, three byte-result, four word-source,
      and three word-result decisions, including near offsets `-4`/`-5` and
      `-5`/`-7` plus far offsets `-146`/`-147` and `-149`/`-151`.
- [x] Assert retained `A`/`HL` result summaries, source-only contextual
      decisions, no physical contextual operand, no invalid policy mode, and
      no failed IX lookup.
- [x] Extend retained `B`/`C` and 16-bit `BC` fixtures with exact source-only
      decisions at offsets `-5`, `-7`, and `-11` while preserving exact ROM.
- [x] Pin complete 16-, 14-, 20-, 21-, 28-, 26-, 32-, and 42-byte ROM
      regions (199 bytes total) and verify matching SHA-256 hashes under
      Cygwin and native Windows toolchains.
- [x] Verify original complement location/chain suites, shared address
      policy, and default/`-no-ra` identity remain exact.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (133 tests)`.

Memory-backed complement operands and results now expose emitter-owned IX
policy decisions, while retained `A`/`B`/`C`/`HL`/`BC` results bypass target
address construction. Two attempted retained-source expressions were omitted
because pre-allocator TAC optimization currently fails with a null label;
that unrelated optimizer defect is outside this phase. Concrete complement
instruction selection remains Z80 backend behavior, and `-ra` remains
opt-in.

---

## 94. Trace Assignment Source And Result Address Policy

- [x] Audit assignment source/result materialization and confirm memory-backed
      results already use shared policy-driven IX construction while
      memory-backed sources use shared policy-driven IY construction.
- [x] Add contextual `assignment_result_address` diagnostics immediately
      after successful global/stack result IX materialization.
- [x] Add matching `assignment_arg1_address` diagnostics immediately after
      successful global/stack source IY materialization.
- [x] Preserve constants and retained physical locations as explicit bypasses
      that emit no contextual memory-address decision.
- [x] Add an allocator-only assignment location summary with source/result
      kinds, physical registers, and byte widths.
- [x] Add `allocator_ra_target_policy_assignment_address` covering byte and
      word global copies, near and far stack copies, local-to-global copies,
      signed widening, and unsigned widening.
- [x] Assert exactly 12 assignment summaries, 12 result decisions, and 12
      source decisions, including near IX offsets `-6`/`-9`, far IX offsets
      `-147`/`-151`, and far IY source offsets `-146`/`-149`.
- [x] Assert global-label, frame-displacement, and frame-address modes, both
      `1 -> 2` widening summaries, no physical contextual operand, no invalid
      policy mode, and no failed IX/IY lookup.
- [x] Extend the computed get-address base fixture with two retained-`HL`
      assignment-result summaries and exact per-function result-address counts
      proving the retained assignment itself bypasses IX construction.
- [x] Pin complete 14-, 27-, 38-, 20-, 48-, 57-, 23-, and 20-byte ROM
      regions (247 bytes total) and verify matching SHA-256 hashes under
      Cygwin and native Windows toolchains.
- [x] Verify original assignment location/extension/producer suites, shared
      address/location policy, and default/`-no-ra` identity remain exact.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (134 tests)`.

Memory-backed assignments now expose emitter-owned source IY and result IX
policy decisions without changing concrete copy, sign-extension, or
zero-extension instructions. Retained assignment results bypass target address
construction. A retained `COMPLEMENT -> ASSIGNMENT` source is permitted by
policy but no current source shape survives lowering/optimization to that TAC
pair, so this phase does not claim unreachable physical-source coverage.
`-ra` remains opt-in.

---

## 95. Trace Shift Result Address Policy

- [x] Audit 8-bit and 16-bit shift-result materialization and confirm
      memory-backed results use shared policy-driven IX construction while
      retained physical results return before address construction.
- [x] Add contextual `shift_8bit_result_address` and
      `shift_16bit_result_address` diagnostics immediately after successful
      global/stack result IX materialization.
- [x] Add the missing allocator-only 16-bit shift-result location summary so
      both widths expose operation, location, physical register, and type.
- [x] Preserve constants and retained `A`/`C`/`HL`/`BC` locations as explicit
      bypasses that emit no contextual memory-address decision.
- [x] Add `allocator_ra_target_policy_shift_result_address` covering left and
      right byte/word shifts with global, near-stack, and far-stack results.
- [x] Assert exactly eight shift-result summaries, four byte-result decisions,
      and four word-result decisions, including near offsets `-7`/`-11` and
      far offsets `-147`/`-151`.
- [x] Assert global-label, frame-displacement, and frame-address modes, no
      physical contextual result, and no invalid policy mode.
- [x] Extend retained `A`/`HL` and `C` shift fixtures with negative contextual
      assertions proving those results bypass IX construction.
- [x] Pin complete 27-, 27-, 32-, 56-, 36-, 39-, 56-, and 83-byte ROM
      regions (356 bytes total) and verify matching SHA-256 hashes under
      Cygwin and native Windows toolchains.
- [x] Verify shift count/result controls, shared address/location policy, and
      default/`-no-ra` identity remain exact.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (135 tests)`.

Memory-backed shift results now expose emitter-owned IX policy decisions
without changing concrete shift instruction selection. Retained physical
results bypass target address construction. Existing source-reachable tests
cover retained `A`, `C`, and `HL` results; retained `BC` remains an explicit
emitter bypass but no dedicated source shape currently reaches it. `-ra`
remains opt-in.

---

## 96. Retain And Trace Return-Value Sources

- [x] Audit callee `RETURN_VALUE` source handling and confirm memory-backed
      sources use shared policy-driven IX construction while constants and
      retained `A`/`HL` sources bypass address construction.
- [x] Add contextual `return_value_source_address` diagnostics immediately
      after successful global/stack source IX materialization.
- [x] Add an allocator-only `return_value_source` summary exposing source
      kind, width, physical register, source type, and declared return type.
- [x] Stop treating the temp consumed by the exact terminal `RETURN_VALUE` as
      live across that return boundary; emit `return_boundary_consumer` and
      defer its register choice to target candidate policy.
- [x] Recognize `RETURN_VALUE arg1` as a linear-scan consumer, enabling direct
      8-bit `A` and 16-bit `HL` producer-to-return forwarding.
- [x] Add `allocator_ra_target_policy_return_value_source_address` covering
      global, near-stack, and far-stack byte/word sources, signed and unsigned
      widening, retained `A`/`HL` sources, and constants.
- [x] Assert exactly 22 source summaries and eight contextual IX decisions,
      including near offsets `-5`/`-7`, widening offset `-6`, and far offsets
      `-146`/`-149`.
- [x] Assert exactly two terminal-boundary exemptions, direct `ADD ->
      RETURN_VALUE` retention in `A`/`HL`, and no contextual address decision
      for retained or constant sources.
- [x] Pin complete 22-, 12-, 24-, 28-, 18-, 30-, 19-, 17-, 16-, 25-, and
      11-byte ROM regions (222 bytes total) and verify matching SHA-256 hashes
      under Cygwin and native Windows toolchains.
- [x] Stabilize the original byte/word return-location fixtures by excluding
      environment-sensitive absolute branch targets from marked regions, then
      assert retained temp returns and pin their shorter 16-/25-byte regions.
- [x] Refresh only affected return-slot/transport call addresses; verify call
      result storage and default/`-no-ra` identity remain exact.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (136 tests)`.

Terminal single-use return values can now remain in the target's primary
8-bit or 16-bit register through the callee return emitter, eliminating one
spill and reload. Other return hard-boundary behavior is unchanged, concrete
return-slot stores remain Z80 backend behavior, and `-ra` remains opt-in.

---

## 97. Extract Target-Independent Liveness Solving

- [x] Add `compiler/register_allocator.c` and
      `compiler/register_allocator.h` as the first target-independent
      allocator-core module.
- [x] Move liveness-set indexing and the backward fixed-point solver out of
      `pass_5.c`, while keeping TAC-specific use/def collection in the owning
      pass.
- [x] Pass block count, temp count, CFG edges, use/def sets, and live-in/out
      sets explicitly across the new core boundary.
- [x] Add fail-closed invalid-edge and convergence-limit handling plus a
      concise core diagnostic for empty, converged, invalid, and failed
      solves.
- [x] Add the new module to GCC, CMake, Amiga, and Visual Studio compiler
      build manifests.
- [x] Add `allocator_ra_core_liveness_extraction` with diamond and loop CFG
      shapes, exact solver invocation counts, convergence iterations,
      propagated liveness, and stack-only join assertions.
- [x] Pin complete 88- and 51-byte ROM regions and verify the complete ROM
      SHA-256 `7c18a51fcd33432ace8ace9a1e82876bbed2ce7d481c65e533804f872adccf27`
      under Cygwin and native Windows compilers.
- [x] Verify the strict GCC build and a native MSVC build using the installed
      `v143` override without changing the project's `v141_xp` default.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (137 tests)`.

The first reusable allocator-core boundary now owns generic liveness indexing
and fixed-point solving. TAC semantics, block construction, next-use,
interval construction, linear scan, spilling/splitting, and join
reconciliation remain in `pass_5.c` for later extraction. Existing allocation
behavior is unchanged and `-ra` remains opt-in.

---

## 98. Extract Basic-Block And CFG Construction

- [x] Define normalized allocator instruction descriptors carrying only TAC
      index, liveness/filter state, label identity, block-end reason, and jump
      classification.
- [x] Keep TAC operation classification and per-function filtering in
      `pass_5.c`; pass normalized descriptors into the reusable allocator
      core.
- [x] Move leader-based basic-block partitioning, label-to-block resolution,
      and fallthrough/jump/true/false CFG-edge construction into
      `compiler/register_allocator.c`.
- [x] Add fail-closed capacity, missing-instruction, and missing-target
      handling to the core boundary.
- [x] Emit `register_allocator_core: block_build` and `cfg_build` summaries
      exposing function, input/block/edge counts, algorithm, and status.
- [x] Preserve the two block-build invocations per function used by allocation
      and exit-spill analysis, plus the single CFG build used by allocation.
- [x] Add `allocator_ra_core_cfg_extraction` covering a diamond, loop
      back-edge, function-call boundary, inline-asm boundaries, and return.
- [x] Assert exact block/edge counts, all four CFG edge kinds, five hard
      boundary blocks, and absence of every fail-closed status.
- [x] Pin complete 88-, 51-, and 75-byte ROM regions and verify complete ROM
      SHA-256 `dfbfee0d161a9e414b39ee38d34090e1052505e2d868f097045170bf43913156`
      under Cygwin and native Windows compilers.
- [x] Verify strict GCC and native MSVC builds without changing the project's
      historical `v141_xp` default.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (138 tests)`.

The reusable allocator core now owns basic-block partitioning, CFG edge
construction, liveness indexing, and fixed-point liveness solving. TAC
classification remains in `pass_5.c`, and `-ra` remains opt-in.

---

## 99. Extract Target-Independent Next-Use Search

- [x] Add a callback-driven forward next-use query to
      `compiler/register_allocator.c`, parameterized by instruction activity
      and temp read/write predicates.
- [x] Keep TAC filtering and operand read/write semantics in `pass_5.c` and
      migrate all six allocation, conflict, debug, and split-placement users
      of the former local search.
- [x] Preserve the original search ordering: skip inactive instructions,
      return the first read, stop at an earlier redefinition, and report no
      use at a normal or empty block range end.
- [x] Add fail-closed range/callback validation and
      `register_allocator_core: next_use` diagnostics exposing function,
      temp, range, result, stop reason, and status.
- [x] Add `allocator_ra_core_next_use_extraction` covering byte and word
      farther-next-use conflicts plus two lifetimes compacted onto one reused
      temp.
- [x] Assert exact 7/7/4 core-query counts and TAC positions, competing
      candidates, retained intervals, temp reuse, and absence of invalid or
      unexpected source-reachable redefinition outcomes.
- [x] Add a strict C90 core harness covering inactive-instruction skipping,
      read, redefinition, block-end, empty-range, and invalid-range outcomes.
- [x] Pin complete 26-, 52-, and 44-byte ROM regions and verify complete ROM
      SHA-256 `0aecbf6eea8f43cbc2de43a0796c25433f91760565dfa035f759601166053150`
      under Cygwin and native Windows compilers.
- [x] Verify the strict GCC build and a native MSVC build using the installed
      `v143` override without changing the project's `v141_xp` default.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (139 tests)`.

The reusable allocator core now owns next-use traversal and stopping rules;
the owning pass supplies TAC-specific activity and read/write classification.
Existing allocation behavior is unchanged and `-ra` remains opt-in.

---

## 100. Extract Target-Independent Live-Interval Metadata

- [x] Add a reusable `register_allocator_live_interval` record carrying
      used state, maximum width, read/write counts, and aggregate first/last
      instruction indexes.
- [x] Move interval reset, access accumulation, range extension, size
      widening, and used-interval counting from `pass_5.c` into
      `compiler/register_allocator.c`.
- [x] Keep TAC operand classification in `pass_5.c`, including call-argument
      reads and the special `ARRAY_WRITE result` read contract.
- [x] Replace six parallel `REG_COUNT` arrays with one core interval array and
      copy the completed records into existing temp metadata without changing
      allocator or backend interfaces.
- [x] Fail closed on null storage, invalid capacities, out-of-range temp IDs,
      zero/negative widths, negative instruction indexes, and invalid access
      kinds.
- [x] Emit `register_allocator_core: interval_reset`, `interval_note`, and
      `interval_build` diagnostics exposing function, temp, width, access,
      evolving range/counts, capacity, interval count, and status.
- [x] Add `allocator_ra_core_live_interval_extraction` covering sparse temp
      IDs, size widening, repeated same-instruction reads, same-TAC read/write,
      reused temp lifetimes, adjacent function resets, and a retained
      `ARRAY_WRITE result` base.
- [x] Add a strict C90 core harness covering empty reset, sparse counting,
      out-of-order accesses, repeated reads, all invalid-input paths, and
      reset-after-use behavior.
- [x] Pin complete 59- and 44-byte ROM regions and verify complete ROM SHA-256
      `be82e123b32537cbcafa997f8cb7762691ec783029f7b46821c0d2738fab1357`
      under Cygwin and native Windows compilers with the same linker.
- [x] Verify the strict GCC build, native MSVC `v143` build, and focused
      interval-reuse, temp-compaction, next-use, and split-spill regressions.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (140 tests)`.

The reusable allocator core now owns aggregate temp interval metadata as well
as next-use traversal. TAC access semantics and per-definition linear-scan
scheduling remain in `pass_5.c`, and `-ra` remains opt-in.

---

## 101. Extract Target-Independent Linear-Scan Conflict Decisions

- [x] Add a deterministic core decision API with explicit assign,
      replace-active, and keep-active outcomes for one selected physical
      register slot.
- [x] Move empty/expired-slot decision classification, nearer/farther next-use
      comparison, equal-next-use preference application, and fail-closed
      unavailable-active handling into `compiler/register_allocator.c`.
- [x] Keep TAC candidate discovery, Z80 physical-register selection,
      overlapping-register policy, path safety, the `ARRAY_WRITE` tie-policy
      predicate, and all spill/retain mutation in `pass_5.c`.
- [x] Emit `register_allocator_core: linear_scan_choose` diagnostics exposing
      function, block, slot, instruction, candidate and active endpoints,
      replaceability, tie preference, decision, reason, and status.
- [x] Add `allocator_ra_core_linear_scan_extraction` with exact word
      nearer-next-use displacement and byte shared-consumer tie-preference
      code regions.
- [x] Add a strict C90 core harness covering empty and expired slots, nearer,
      farther, equal keep/replace decisions, unavailable active metadata, and
      invalid candidate, endpoint, state, boolean, and null inputs.
- [x] Preserve the existing `farther_next_use` spill and
      `prefer_array_write_value` traces and verify the existing focused
      farther-next-use and split-reload regressions.
- [x] Pin complete 52- and 76-byte ROM regions and verify complete ROM SHA-256
      `67704c374c4f12c5f1ed08dc07e2021455425f68c95916cf7aeffd8ff40b601f`
      under Cygwin and native Windows compilers with the same linker.
- [x] Verify the strict GCC build and native MSVC `v143` build; the two GCC
      unused-function warnings inherited at this phase were removed after
      Phase 109, and the current GCC/MSVC builds are warning-free.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (141 tests)`.

The reusable core now owns active-slot conflict arbitration after the owning
pass has selected a legal physical register. Per-definition traversal,
candidate and target policy, physical-slot state storage, spilling/splitting,
and join reconciliation remain in `pass_5.c`; `-ra` remains opt-in.

---

## 102. Extract Target-Independent Active-Slot State

- [x] Add a reusable `register_allocator_active_slot` record carrying temp,
      next-use, producer, consumer-operand, and retained-interval state.
- [x] Replace the five duplicated `A`/`HL`/`BC`/`B`/`C` scalar state groups
      in `pass_5.c` with one five-slot core record array while keeping the
      Z80 physical-register-to-slot mapping local.
- [x] Move per-block reset, per-instruction expiration, full-field clearing,
      and assignment into target-independent core APIs.
- [x] Validate every slot before expiration so malformed later records cannot
      partially clear earlier records; reject noncanonical empty records,
      reversed active ranges, invalid booleans, indexes, counts, and nulls.
- [x] Emit `register_allocator_core: active_slots_reset`,
      `active_slots_expire`, `active_slot_expire`, and `active_slot_assign`
      diagnostics with block, instruction, slot, interval, and status data.
- [x] Add `allocator_ra_core_active_slot_extraction` with exact byte and word
      compiler cases covering `A`, `HL`, and `C`, same-slot reassignment, and
      two simultaneous source-reachable expirations.
- [x] Add a strict C90 core harness covering all five abstract slots,
      canonical reset, live-slot preservation, minimum-length intervals,
      opaque operand IDs, one/three/five simultaneous expirations,
      reassignment, retained metadata, transactional failure, and invalid
      public inputs.
- [x] Pin complete 26- and 52-byte ROM regions and verify complete ROM SHA-256
      `66e68eef4c697d197095e1103cbed0019d689f5daa1af3c87df337630f627fe6`
      under Cygwin and native Windows compilers with the same linker.
- [x] Verify strict GCC, native MSVC `v143`, and focused conflict,
      equal-next-use, and Phase 101 regressions.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (142 tests)`.

The reusable core now owns active-slot storage transitions and conflict
arbitration. Per-definition candidate traversal, target slot mapping,
overlap/policy decisions, spilling/splitting, and join reconciliation remain
in `pass_5.c`; `-ra` remains opt-in.

---

## 103. Move Active-Slot Mapping Behind Target Policy

- [x] Add target-policy callbacks for active-slot count, physical-register to
      slot-index mapping, and physical-slot names.
- [x] Move the Z80 `A`/`HL`/`BC`/`B`/`C` mapping to the Z80 policy as slots
      0/1/2/3/4 and remove the corresponding local selection chain from the
      linear-scan loop.
- [x] Validate the target-reported slot count and every physical-register
      mapping before core arbitration or active-slot mutation; fail closed on
      invalid counts, indexes, or names.
- [x] Emit `target_active_slots` and `target_active_slot` diagnostics exposing
      target, slot count, physical register, slot index, name, and status.
- [x] Add `active_slots` to the target-policy contract summary diagnostic and
      update existing strict policy-hook assertions.
- [x] Add `allocator_ra_target_policy_active_slots` with all five Z80 mappings,
      ordered mapping-to-core-assignment assertions, alias-aware `B` and `BC`
      alternatives, invalid-policy guards, and exact byte/word ROM regions.
- [x] Inspect the GCC trace and verify `A -> 0`, `HL -> 1`, `BC -> 2`,
      `B -> 3`, and `C -> 4` all feed matching `active_slot_assign` records.
- [x] Verify the focused GCC fixture and complete ROM SHA-256
      `3af6900387531c1f4857e8b56041556028f1518f5da988a6fcf11e42510fbd99`.
- [x] Verify strict native MSVC `v143`, matching common-linker ROM SHA-256,
      and no new compiler warnings.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (143 tests)`.

The target policy now owns active-slot identity and cardinality while the
reusable core continues to own slot state transitions and conflict arbitration.
Per-definition candidate traversal, broader target candidate policy,
spill/split generalization, and join reconciliation remain in `pass_5.c`;
`-ra` remains opt-in.

---

## 104. Extract Per-Definition Candidate Discovery

- [x] Add a reusable `register_allocator_candidate` record carrying found
      state, temp index, producer, consumer, consumer operand, and whether the
      definition has exactly one read before redefinition.
- [x] Add a callback-driven core discovery API that remains independent of TAC
      opcodes, target registers, and Z80 legality rules.
- [x] Move producer filtering, next-read traversal, consumer-operand lookup,
      and post-consumer read multiplicity into the shared discovery path used
      by the main linear scan and both competing-candidate checks.
- [x] Preserve function boundaries, inactive instructions, read/write
      consumers, redefinition stops, block-end limits, and unsupported
      consumer rejection. The first consumer is block-local; matching the old
      helper, read multiplicity continues through the definition until its
      next write, including reads in later blocks.
- [x] Emit `register_allocator_core: candidate_discover` diagnostics with
      producer, temp, consumer, operand, single-read state, outcome, and reason.
- [x] Add `allocator_ra_core_candidate_discovery` with source-reachable byte
      and word multi-read split intervals plus a single-read nonzero-index case.
- [x] Add a strict C90 core harness covering single and multiple reads,
      read/write consumers, opaque operand IDs, no definition, redefinition,
      block cutoff, inactive reads, unsupported consumers, canonical cleared
      output, invalid ranges, null callbacks, and null output.
- [x] Verify Phase 99 next-use, Phase 101 arbitration, split-spill, and the new
      focused fixture under strict GCC with unchanged exact ROM regions.
- [x] Inspect compiler and harness logs and verify `single_read=no` feeds both
      byte and word split preservation while ordinary candidates remain
      `single_read=yes`.
- [x] Pin complete ROM SHA-256
      `71b0bfb79d01bdf83da7b21545758286eefb11844f5a40be17e36d6827896ab8`.
- [x] Verify native MSVC `v143`, matching common-linker ROM SHA-256, and no new
      compiler warnings.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (144 tests)`.

The reusable core now owns per-definition candidate discovery in addition to
next-use traversal, active-slot state, and conflict arbitration. TAC-specific
producer/consumer classification, target legality and register choice,
spill/split mutation, and join reconciliation remain in `pass_5.c`; `-ra`
remains opt-in.

---

## 105. Extract Temp Storage-State Transitions

- [x] Add a reusable `register_allocator_temp_state` carrying only spill-slot
      requirement and opaque physical-register identity.
- [x] Add a fail-closed retain transition that distinguishes register-only
      single-read/single-write temps from intervals that must remain
      spill-backed.
- [x] Add a fail-closed displacement transition that restores canonical
      spill-backed, physically unassigned state.
- [x] Keep spill-reason classification, boundary metadata, TAC interval
      marking, physical-register naming, and spill/reload emission in the Z80
      integration layer.
- [x] Route linear-scan assignment and farther-next-use replacement through
      the core transitions and propagate transition failures.
- [x] Emit `register_allocator_core: temp_state_retain` and
      `temp_state_spill` diagnostics with prior/result state, storage mode,
      counts, spill constraint, requested register, and outcome.
- [x] Add `allocator_ra_core_temp_state` with exact byte and word ROM regions
      for register-only retention, spill-backed retention, and displacement.
- [x] Add a strict C90 core harness covering both retain modes, register
      reassignment, displacement, repeated displacement, zero/multiple
      reads and writes, invalid booleans, inconsistent canonical states,
      invalid identifiers/counts, null inputs, and transactional failure.
- [x] Inspect compiler and harness traces and verify register-only retention
      removes the spill slot, multi-use retention keeps it, and displacement
      changes `spill=no phy=HL` to `spill=yes phy=NONE` before replacement.
- [x] Verify Phase 101 arbitration, farther-next-use, interval reuse,
      candidate discovery, split-spill/reload, and the new focused fixture
      under strict GCC with unchanged exact ROM regions.
- [x] Pin complete focused ROM SHA-256
      `5ca774ab731bfe3b1d5f10c2cfaa741e4b92a47ef1eef54f27c8bf0fa5742311`
      and verify native MSVC `v143` produces the same ROM with zero warnings
      and zero errors.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (145 tests)`.

The reusable core now owns the target-independent storage-state transitions
used when linear scan retains or displaces a temp. Spill reasons, TAC marking,
target register policy, concrete spill/reload insertion, and cross-block join
reconciliation remain in `pass_5.c`; `-ra` remains opt-in.

---

## 106. Extract Split-Preservation Action Planning

- [x] Add target-independent split-action outcomes for ordinary single-read
      intervals, preservable multi-read intervals, unsupported preservation,
      and preservation-register changes.
- [x] Add a fail-closed core classifier using opaque physical-register IDs
      and canonical preservation metadata rather than TAC or Z80 constants.
- [x] Route the final post-selection split decision through the core while
      leaving target legality, TAC spill/reload flags, and concrete emission
      in `pass_5.c` and the Z80 backend.
- [x] Preserve existing `multi_read_interval` and
      `multi_read_preserve_requires_hl` integration outcomes while exposing
      target-independent rejection reasons.
- [x] Emit `register_allocator_core: split_action` diagnostics with producer,
      consumer, read multiplicity, preservation support/register, selected
      register, action, reason, and status.
- [x] Add `allocator_ra_core_split_action` with exact byte and word ROM regions
      that exercise retained `HL` preservation and later `BC` reloads.
- [x] Add a strict C90 core harness covering all four actions, malformed
      booleans, invalid instruction ordering and identifiers, absent selected
      registers, inconsistent preservation metadata, and single-read metadata
      rejection.
- [x] Inspect compiler and harness traces and verify `action=preserve` precedes
      each spill marker and the later eligible read receives the reload marker.
- [x] Verify candidate discovery, temp-state transitions, split-spill/reload,
      arbitration, and the new focused fixture under strict GCC with unchanged
      exact ROM regions.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify both full suites report
      `DONE (146 tests)`.

The reusable core now decides whether a candidate needs no split action, may
be preserved, or must be rejected because preservation is unsupported or its
approved register changed. Target capability checks, TAC marking, reload-site
discovery, concrete spill/reload insertion, and join reconciliation remain in
`pass_5.c`; `-ra` remains opt-in.

---

## 107. Extract Split Reload-Site Discovery

- [x] Add a reusable `register_allocator_reload_site` result with canonical
      found state and instruction identity.
- [x] Add callback-driven first-next-read discovery that remains independent
      of TAC opcodes, target registers, and Z80 reload constraints.
- [x] Preserve inactive-instruction skipping, redefinition stops, read/write
      precedence, block-end limits, and valid empty block suffixes.
- [x] Preserve the existing first-read-only contract: when the first next read
      is not reload-eligible, reject it without scanning ahead to later reads.
- [x] Move TAC-specific reload eligibility behind an integration callback and
      leave target reload-register selection and TAC mutation in `pass_5.c`.
- [x] Make split-reload placement propagate malformed discovery failures while
      keeping ordinary not-found outcomes as successful no-ops.
- [x] Emit `register_allocator_core: reload_site` diagnostics with range,
      selected instruction, first-read selection policy, outcome, and reason.
- [x] Add `allocator_ra_core_reload_site` with exact byte and word ROM regions
      covering `HL` preservation followed by first-read `BC` reload insertion.
- [x] Add a strict C90 harness covering found sites, inactive reads,
      redefinitions, read/write sites, first-read rejection, no-read and empty
      ranges, canonical clearing, invalid ranges/identifiers, null callbacks,
      and null output.
- [x] Inspect compiler and harness traces and verify preservation precedes the
      spill marker, discovery selects the same first read, reload insertion
      follows target approval, and ineligible first reads do not scan ahead.
- [x] Verify next-use, candidate discovery, split-action planning,
      split-spill/reload emission, and the new focused fixture under strict GCC
      with unchanged exact ROM regions.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (147 tests)`.

The reusable core now owns first-read reload-site discovery and canonical
rejection outcomes. TAC eligibility, target reload-register policy, reload
action planning, concrete spill/reload insertion, and join reconciliation
remain in `pass_5.c`; `-ra` remains opt-in.

---

## 108. Extract Split Reload-Action Planning

- [x] Add target-independent reload actions for no-op and concrete insertion.
- [x] Validate function, block, temp, spill instruction, reload instruction,
      and strict spill-before-reload ordering before planning an action.
- [x] Keep physical-register IDs opaque and classify target no-register
      sentinels solely by equality, including negative sentinel values.
- [x] Classify target refusal as a successful no-op and a selected target
      register as an insertion action without knowing TAC or Z80 semantics.
- [x] Route split reload placement through the core planner after target
      register selection and before TAC mutation, propagating malformed
      planner metadata as failure.
- [x] Emit `register_allocator_core: reload_action` diagnostics with spill and
      reload instructions, opaque register ID, action, reason, and status.
- [x] Add `allocator_ra_core_reload_action` with exact byte and word ROM
      regions covering `HL` preservation and planned `BC` reload insertion.
- [x] Add a strict C90 harness covering insert, target refusal, opaque negative
      sentinels, malformed identities/order, and null function metadata.
- [x] Inspect raw traces and verify reload-site discovery precedes action
      planning, target diagnostics and TAC insertion follow only an insert
      action, and malformed inputs fail closed.
- [x] Verify reload discovery, split-action planning, target split policy, and
      concrete spill/reload emission fixtures retain their exact ROM behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (148 tests)`.

The reusable core now owns reload-site discovery and the post-target-selection
reload action decision. Target reload-register selection, TAC marking, concrete
spill/reload emission, generalized interval splitting, and join reconciliation
remain outside the core; `-ra` remains opt-in.

---

## 109. Extract Reload TAC-Mutation Planning

- [x] Add a reusable `register_allocator_reload_mutation` plan containing
      canonical apply state, instruction, temp identity, and opaque register.
- [x] Canonically clear mutation output before every decision and preserve the
      target's exact no-register sentinel in empty plans.
- [x] Convert reload no-op actions into successful empty mutation plans and
      insert actions into complete mutation plans without depending on TAC.
- [x] Validate function, block, temp, instruction, action, action/register
      consistency, and output storage, failing closed with cleared output.
- [x] Preserve opaque register IDs, including zero as a physical register when
      a target uses a negative no-register sentinel.
- [x] Route `pass_5.c` through mutation preparation before applying the planned
      physical register and reload marker to the concrete TAC.
- [x] Emit `register_allocator_core: reload_mutation` diagnostics with action,
      instruction, temp, opaque register ID, apply state, and status.
- [x] Add `allocator_ra_core_reload_mutation` with exact byte and word ROM
      regions covering planned `BC` mutation and concrete reload emission.
- [x] Add a strict C90 harness covering insert and no-op plans, poisoned-output
      clearing, opaque sentinels, malformed identities/actions, inconsistent
      action/register pairs, null metadata, and null output.
- [x] Inspect raw traces and verify site discovery, action planning, mutation
      planning, target diagnostics, and TAC insertion occur in that order.
- [x] Verify neighboring reload-site/action, split-action, target-policy, and
      concrete spill/reload emission fixtures retain exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (149 tests)`.

The reusable core now plans the complete reload TAC mutation, but `pass_5.c`
still applies it to TAC. Spill-marker mutation, backend spill/reload emission,
generalized interval splitting, and join reconciliation remain outside the
core; `-ra` remains opt-in.

---

## 110. Extract Spill TAC-Mutation Planning

- [x] Add a reusable `register_allocator_spill_mutation` plan containing
      canonical apply state, consumer instruction, temp, opaque operand, and
      opaque physical-register identity.
- [x] Canonically clear mutation output before every decision and preserve the
      target's exact no-register sentinel in empty plans.
- [x] Convert ordinary non-split candidates into successful no-op plans with
      reason `split_not_required`.
- [x] Convert preservation candidates displaced by linear scan into successful
      no-op plans with reason `candidate_not_retained`.
- [x] Convert retained preservation candidates into complete mutation plans
      without depending on TAC opcodes, fields, or Z80 registers.
- [x] Validate function, block, temp, instruction, operand, split action,
      retained boolean, selected register, and output storage, failing closed
      with canonical output when writable.
- [x] Preserve opaque operand/register IDs, including zero values when a target
      uses a negative no-register sentinel.
- [x] Route linear scan through mutation preparation before applying the
      planned `store_retained_to_spill_operand` marker in `pass_5.c`.
- [x] Emit `register_allocator_core: spill_mutation` diagnostics with split
      action, retention outcome, operand, register, apply state, and reason.
- [x] Add `allocator_ra_core_spill_mutation` with exact byte and word ROM
      regions covering planned `HL` preservation and later `BC` reload.
- [x] Add a strict C90 harness covering apply, both no-op reasons, poisoned
      output clearing, opaque IDs/sentinels, rejected and unknown actions,
      malformed identities/booleans, sentinel misuse, and null output.
- [x] Inspect raw traces and verify split action precedes mutation planning,
      only apply plans receive spill markers, and concrete emitter diagnostics
      retain their expected stack offsets and widths.
- [x] Verify neighboring split/reload planners, target split policy, and
      concrete spill/reload emission fixtures retain exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (150 tests)`.

The reusable core now plans both spill and reload TAC mutations. `pass_5.c`
still applies those plans to concrete TAC fields, and `pass_6_z80.c` still
emits target instructions. Generalized interval splitting and join
reconciliation remain outside the core; `-ra` remains opt-in.

---

## 111. Extract Callback-Driven Mutation Application

- [x] Add separate reusable spill and reload mutation-applier callback types so
      reload plans do not require a synthetic operand.
- [x] Add core dispatch for canonical spill and reload mutation plans without
      depending on TAC structures, opcodes, fields, or target registers.
- [x] Validate apply booleans, canonical empty-plan shapes, instruction/temp/
      operand identities, opaque register sentinels, and required callbacks
      before dispatch.
- [x] Treat canonical empty plans as successful skipped applications without
      invoking or requiring a callback.
- [x] Propagate callback failure and distinguish `invalid_input`,
      `callback_failed`, `skipped`, and `complete` diagnostic outcomes.
- [x] Move the only direct spill/reload marker writes in `pass_5.c` behind
      TAC-specific callbacks that validate function, instruction, temp,
      operand, prior register state, and target no-register constraints.
- [x] Emit `register_allocator_core: spill_mutation_apply` and
      `reload_mutation_apply` diagnostics with plan identity and outcome.
- [x] Add `allocator_ra_core_mutation_apply` with exact byte and word ROM
      regions covering callback-applied `HL` spill and `BC` reload markers.
- [x] Add a strict C90 harness covering callback argument transport, opaque
      IDs/context, empty-plan skipping with null callbacks, callback failures,
      malformed apply/empty plans, sentinel misuse, null metadata/callbacks,
      bad blocks, and null plans.
- [x] Inspect raw traces and verify plan preparation, callback dispatch, target
      diagnostics, and legacy insertion diagnostics occur in order for both
      source-reachable paths.
- [x] Verify neighboring spill/reload planners, target split policy, and
      concrete spill/reload emitter fixtures retain exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (151 tests)`.

The reusable core now owns canonical spill/reload planning and callback-driven
application. `pass_5.c` callbacks retain TAC semantics, while `pass_6_z80.c`
retains concrete instruction emission. Generalized interval splitting and join
reconciliation remain outside the core; `-ra` remains opt-in.

---

## 112. Extract Bounded Reload-Chain Discovery

- [x] Add reusable bounded reload-chain discovery that enumerates active reads
      from a valid suffix until block end, redefinition, an ineligible read, or
      output capacity.
- [x] Preserve read-before-write semantics by recording an eligible read and
      then stopping when the same instruction also redefines the temp.
- [x] Add canonical chain metadata containing count, truncation state, and the
      instruction that caused a conservative stop.
- [x] Canonically clear every writable output slot and result field before
      validation so malformed requests fail closed without stale sites.
- [x] Treat capacity exhaustion as a successful truncated discovery while
      treating invalid capacities, ranges, identities, callbacks, and outputs
      as failures.
- [x] Emit `register_allocator_core: reload_chain_site` and `reload_chain`
      diagnostics with ordinal, instruction, capacity, truncation, stop point,
      and stop reason.
- [x] Route `pass_5.c` split-reload placement through the chain API with
      capacity one, preserving the current single-site target behavior and
      exact generated code.
- [x] Preserve the established first-site diagnostic for compatibility with
      existing source-level allocator assertions.
- [x] Add `allocator_ra_core_reload_chain` with exact byte and word ROM regions
      covering the current `HL` preservation and later `BC` reload path.
- [x] Add a strict C90 harness covering three-read chains, capacity truncation,
      inactive instructions, redefinition, ineligible middle reads,
      read/write ordering, empty suffixes, canonical clearing, malformed
      metadata, and null callbacks/outputs.
- [x] Inspect raw source and harness traces and verify every discovered site,
      conservative stop reason, current capacity-one integration, and concrete
      spill/reload emission order.
- [x] Verify neighboring reload-site/action/mutation/application, spill
      mutation, target split-policy, and concrete emitter fixtures retain exact
      behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (152 tests)`.

The reusable core can now discover a bounded chain of later reload sites.
Phase 113 consumes that complete bounded chain while preserving per-site target
approval. Generalized operand/register support and join reconciliation remain;
`-ra` remains opt-in.

---

## 113. Apply Complete Block-Local Reload Chains

- [x] Size reload discovery to the complete basic-block suffix instead of a
      single site, with checked allocation and deterministic cleanup on every
      failure path.
- [x] Iterate discovered sites in instruction order and independently request
      the target reload register, plan the reload action, prepare its canonical
      mutation, and dispatch the TAC callback.
- [x] Preserve fail-closed target behavior by skipping target-rejected sites
      without preventing a later supported site from being considered.
- [x] Propagate action, mutation, and callback failures while releasing bounded
      discovery storage and retaining the existing diagnostic at the failing
      stage.
- [x] Preserve TAC-local reload markers and concrete backend emission so every
      approved consumer independently resolves the original temp's spill slot.
- [x] Add `register_allocator: reload_chain_apply` diagnostics with spill TAC,
      discovered count, suffix capacity, truncation state, and completion or
      allocation-failure status.
- [x] Update `allocator_ra_core_reload_chain` to require complete-suffix
      capacity and the new application-summary diagnostic while retaining its
      exact byte and word ROM regions.
- [x] Add `allocator_ra_core_reload_chain_apply` with exact byte and word ROM
      regions covering current `HL` preservation and `BC` reload emission.
- [x] Add a strict C90 composition harness covering three successful sites,
      an unsupported middle target with a supported later site, callback
      failure at site two, an ineligible middle read, same-instruction
      read/write termination, and an empty chain.
- [x] Inspect raw source and harness traces and verify ordered discovery,
      per-site action/mutation/application, rejected-site skipping, failure
      propagation, and unchanged concrete emitter diagnostics.
- [x] Verify neighboring reload-site/action/mutation/application and spill
      mutation fixtures, target split policy, and concrete split emission retain
      exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (153 tests)`.

Block-local reload discovery and mutation application now handle every
structurally eligible site in the suffix. The Z80 target still approves only
zero-index 16-bit `ARRAY_WRITE result` reloads into `BC`; other operand forms,
registers, cross-block ranges, and join reconciliation remain. `-ra` remains
opt-in.

---

## 114. Extract Canonical Reload Emission Planning

- [x] Add a target-independent `register_allocator_reload_emission` plan with
      canonical emit state, temp, physical register, spill source offset, and
      byte count.
- [x] Add `register_allocator_prepare_reload_emission()` with fail-closed
      validation for reload intent, target-selected register, spill
      availability, positive width, and canonical empty skip metadata.
- [x] Clear caller-provided output before every validation path so malformed
      requests cannot leak stale emission state.
- [x] Keep physical-register identifiers opaque to the reusable core and
      preserve signed target-defined spill offsets and byte widths.
- [x] Route the concrete Z80 array-write spill reload through the canonical
      plan after spill-location resolution.
- [x] Keep Z80-only operand, `BC`, two-byte, address-materialization, and load
      instruction checks in `pass_6_z80.c`.
- [x] Add `register_allocator_core: reload_emission` diagnostics covering
      requested/skipped state, temp, physical register, spill availability,
      source offset, width, canonical completion, and invalid input.
- [x] Add `allocator_ra_core_reload_emission` with exact byte and word ROM
      regions for the existing split-preservation and `BC` reload path.
- [x] Add a strict C90 harness covering word and byte plans, signed positive
      and negative offsets, opaque register zero, canonical skip, malformed
      intent, temp, register, spill state, zero/negative widths, stale skip
      metadata, null function, and null output.
- [x] Inspect raw source and harness traces and verify core plans precede the
      unchanged concrete Z80 diagnostics with byte path `r0/-8/2/BC` and word
      path `r0/-9/2/BC`.
- [x] Update the complete reload-chain application regression to require the
      new core emission diagnostic before its existing concrete checks.
- [x] Verify neighboring mutation, chain, split-policy, spill, and concrete
      emitter fixtures retain exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (154 tests)`.

Reload emission metadata is now canonical and target-independent through the
last boundary before concrete instruction selection. Z80 still owns spill
address materialization and `BC` loads; broader reload operands/registers,
spill emission extraction, and join reconciliation remain. `-ra` remains
opt-in.

---

## 115. Extract Canonical Spill Emission Planning

- [x] Add a target-independent `register_allocator_spill_emission` plan with
      canonical emit state, temp, physical register, spill destination offset,
      and byte count.
- [x] Add `register_allocator_prepare_spill_emission()` with fail-closed
      validation for spill intent, target-selected register, spill
      availability, positive width, and canonical empty skip metadata.
- [x] Clear caller-provided output before every validation path so malformed
      requests cannot leak stale emission state.
- [x] Keep physical-register identifiers opaque to the reusable core and
      preserve signed target-defined spill offsets and byte widths.
- [x] Route the concrete Z80 retained array-read spill through the canonical
      plan after spill-location resolution.
- [x] Keep Z80-only temp operand, `HL`, two-byte, address-materialization, and
      store instruction checks in `pass_6_z80.c`.
- [x] Add `register_allocator_core: spill_emission` diagnostics covering
      requested/skipped state, temp, physical register, spill availability,
      destination offset, width, canonical completion, and invalid input.
- [x] Add `allocator_ra_core_spill_emission` with exact byte and word ROM
      regions for the existing retained-`HL` split-spill path.
- [x] Add a strict C90 harness covering word and byte plans, signed positive
      and negative offsets, opaque register zero, canonical skip, malformed
      intent, temp, register, spill state, zero/negative widths, stale skip
      metadata, null function, and null output.
- [x] Inspect raw source and harness traces and verify core plans precede the
      unchanged concrete Z80 diagnostics with byte path `r0/-8/2/HL` and word
      path `r0/-9/2/HL`.
- [x] Update the neighboring spill-mutation regression to require the new core
      emission diagnostics before its existing concrete checks.
- [x] Verify neighboring mutation, split-policy, reload, pointer, and concrete
      emitter fixtures retain exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `44c739a4577fe77b5c5c6abf78b8911e3a3a9503a3028805be0bc842685eb3b5`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (155 tests)`.

Spill emission metadata is now canonical and target-independent through the
last boundary before concrete instruction selection. Z80 still owns spill
address materialization and `HL` stores; broader spill operands/registers,
longer intervals, and join reconciliation remain. `-ra` remains opt-in.

---

## 116. Extract And Validate The Target Policy Contract

- [x] Move the then-19-hook `register_allocator_target_policy` contract from
      `pass_5.c` into the reusable allocator header using opaque TAC and temp
      pointer declarations; Phase 120 later expands it to 21 hooks.
- [x] Add `register_allocator_validate_target_policy()` with fail-closed
      function, policy-name, and complete callback validation.
- [x] Report the first missing hook with stable target-independent names and
      emit `register_allocator_core: target_policy_validate` diagnostics for
      complete, invalid-input, and incomplete-policy states.
- [x] Validate the selected policy once per allocator-enabled function before
      CFG allocation or any target callback use.
- [x] Keep backend selection and all concrete Z80 callback implementations in
      `pass_5.c`.
- [x] Add `allocator_ra_core_target_policy_contract` with exact byte and word
      ROM regions covering the existing `B`/`BC` alternate-register policy.
- [x] Add a strict C90 harness covering a complete opaque policy, null
      function/policy/name, empty name, and each of the 19 callbacks present in
      Phase 116; Phase 120 extends it to all 21 callbacks.
- [x] Inspect raw harness and production traces and verify core validation
      precedes the existing Z80 target-policy diagnostics.
- [x] Update the neighboring Z80 target-policy regression to require complete
      core validation and reject invalid production policy state.
- [x] Verify neighboring target-policy, split spill/reload, and allocator-core
      fixtures retain exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `3af6900387531c1f4857e8b56041556028f1518f5da988a6fcf11e42510fbd99`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (156 tests)`.

The reusable core now owns the shape and completeness rules of a target
policy, while backend selection and target semantics remain local. Individual
Z80 callbacks, concrete instruction selection, broader spill/reload support,
and join reconciliation remain. `-ra` remains opt-in.

---

## 117. Extract Physical Register Overlap Decisions

- [x] Add `register_allocator_physical_registers_overlap()` to compute overlap
      from opaque physical-register IDs and target-defined unit masks.
- [x] Use an output parameter so malformed queries can return failure while
      conservatively leaving `overlap=yes`.
- [x] Reject null function/policy/name/callback/output and nonpositive target
      unit masks before applying the shared bitmask intersection rule.
- [x] Route the pass-5 active-register conflict checks through the core API
      while preserving target register selection and human-readable Z80 debug.
- [x] Add `register_allocator_core: physical_overlap` diagnostics with real
      function/target names, opaque register IDs, unit masks, decision, and
      complete/invalid status.
- [x] Add `allocator_ra_core_physical_overlap` with exact byte and word ROM
      regions covering disjoint `B`/`C` and `BC`/`HL` allocations.
- [x] Add a strict C90 harness covering same/shared/disjoint masks, symmetry,
      high mask bits, zero/negative masks, missing metadata/callback, null
      output, and fail-closed output clearing.
- [x] Inspect raw production traces and verify the core decisions precede the
      unchanged readable Z80 overlap diagnostics.
- [x] Verify neighboring overlap, target-policy, active-slot, and split
      spill/reload fixtures retain exact behavior.
- [x] Pin complete common-linker ROM SHA-256
      `3af6900387531c1f4857e8b56041556028f1518f5da988a6fcf11e42510fbd99`
      and verify native MSVC `v143` emits identical assembly with zero
      warnings and zero errors.
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (157 tests)`.

Physical-register overlap arithmetic is now target-independent and fail-closed;
targets still define opaque registers and their unit masks. Policy-sized active
slot storage, target callback implementations, and broader register support
remain. `-ra` remains opt-in.

---

## 118. Allocate Policy-Sized Active Slot Storage

- [x] Add `register_allocator_plan_active_slot_storage()` with overflow-safe
      byte sizing for arbitrary positive target slot counts.
- [x] Clear the byte-count output before validation and reject null function,
      null output, nonpositive counts, and size overflow.
- [x] Replace pass 5's fixed `active_slots[5]` stack array with one policy-sized
      allocation per allocator-enabled function.
- [x] Centralize linear-scan failure cleanup so every post-allocation error path
      releases active-slot storage.
- [x] Preserve the current Z80 scheduler's five required named slot indexes
      while accepting policies that expose additional slots.
- [x] Add core `active_slot_storage` and pass-5 allocation/release diagnostics
      with function, target, count, byte size, storage model, and status.
- [x] Add `allocator_ra_core_active_slot_storage` with exact byte and word ROM
      regions covering current five-slot Z80 allocation.
- [x] Add a strict C90 harness covering 1/3/5/7/`INT_MAX` counts, exact byte
      sizing, platform-dependent overflow, invalid inputs, seven-slot reset,
      assignment, expiration, and heap lifecycle.
- [x] Inspect raw production and harness output and verify plan, allocation,
      active-slot use, and release ordering.
- [x] Verify neighboring active-slot, overlap, target-policy, conflict, and
      split spill/reload fixtures retain exact behavior.
- [x] Verify native MSVC `v143` emits identical assembly with zero warnings
      and zero errors (assembly SHA-256
      `f871e6bbead431597d5b071f5426ce94e23198cfe5fc60f1174f9fc45010987b`).
- [x] Pin the complete common-linker ROM SHA-256
      (`66e68eef4c697d197095e1103cbed0019d689f5daa1af3c87df337630f627fe6`).
- [x] Rebuild and stage both compilers; verify the default and Windows suites,
      run sequentially, both report `DONE (158 tests)`.

Linear-scan storage now follows the target policy's slot cardinality instead of
a fixed C array. Z80-specific candidate scheduling still addresses its five
known slots directly; general policy-driven iteration, other targets, broader
spill/reload support, and join reconciliation remain. `-ra` remains opt-in.

---

## 119. Resolve Active Slot Roles Through Target Policy

- [x] Add `register_allocator_resolve_active_slot_roles()` to map an arbitrary
      ordered set of opaque physical-register roles to policy-owned slots.
- [x] Clear every output index before validation and again after any partial
      policy failure so malformed mappings fail transactionally.
- [x] Reject null/empty policy metadata, missing callbacks, nonpositive slot or
      role counts, null arrays, out-of-range indexes, empty names, and duplicate
      role-to-slot mappings.
- [x] Replace every direct `active_slots[0..4]` scheduler read with the resolved
      `A`/`HL`/`BC`/`B`/`C` role indexes.
- [x] Remove the scanner's positional `active_slot_count >= 5` assumption;
      validity now follows the policy's complete, unique role mapping.
- [x] Add per-role and aggregate diagnostics with opaque physical ID, policy
      slot/name, role count, slot count, target, function, and status.
- [x] Add `allocator_ra_core_active_slot_roles` with exact byte/word ROM and a
      strict C90 harness for sparse reordered seven-slot mappings.
- [x] Cover duplicate/out-of-range/unknown mappings, null/empty names, zero
      slots, every missing callback, malformed inputs, and transactional output
      clearing in the new harness.
- [x] Inspect raw production and harness diagnostics to verify role resolution
      precedes storage planning and all malformed mappings fail closed.
- [x] Run neighboring active-slot, overlap, conflict, and target-policy tests.
- [x] Verify warning-free GCC/MSVC builds and matching assembly (SHA-256
      `e3b54ab823f0b598e0f2fc98c01bae260b7b2c96a55a85d4a6a914ef89b03aff`).
- [x] Pin the linked ROM SHA-256
      (`3af6900387531c1f4857e8b56041556028f1518f5da988a6fcf11e42510fbd99`).
- [x] Run default and Windows matrices sequentially and require
      `DONE (159 tests)` from both.

Linear scan no longer assumes that the target stores its five current Z80
roles at slots zero through four. Candidate classes are still selected with
Z80-specific register roles in pass 5; moving that selection behind policy
callbacks, other targets, spill/reload generalization, and join reconciliation
remain. `-ra` remains opt-in.

---

## 120. Move Candidate Register Ordering Behind Target Policy

- [x] Add target hooks for candidate-register count and ordered opaque physical
      register lookup by value width.
- [x] Expand deterministic target-policy completeness validation from 19 to 21
      required hooks and cover both new missing-hook cases.
- [x] Add `register_allocator_resolve_candidate_registers()` with transactional
      output clearing and bounded target callback dispatch.
- [x] Reject null/empty policy metadata, missing callbacks, invalid sizes,
      empty/negative/over-capacity sets, sentinel registers, duplicates, and
      malformed output arguments.
- [x] Define Z80 policy order as 8-bit `A,C,B` and 16-bit `HL,BC`.
- [x] Resolve both candidate sets once per function and use their opaque IDs for
      primary selection and all alternate legality/path/conflict decisions.
- [x] Build active-slot roles from resolved candidate IDs so candidate order and
      slot position remain independently target-owned.
- [x] Add per-candidate and aggregate diagnostics with function, target, width,
      capacity, count, candidate index, opaque physical ID, and status.
- [x] Add `allocator_ra_core_candidate_registers` with exact byte/word ROM and a
      strict C90 harness covering arbitrary positive and negative opaque IDs.
- [x] Cover duplicate/sentinel entries, invalid callback counts, partial-failure
      clearing, missing hooks, malformed policy metadata, and null outputs.
- [x] Inspect raw production/harness diagnostics and verify candidate resolution
      precedes active-slot role resolution with all failures closed.
- [x] Run neighboring policy-contract, candidate, active-slot, overlap, conflict,
      split spill/reload, and return-convention regressions.
- [x] Verify warning-free GCC/MSVC builds and matching assembly (SHA-256
      `da9c6235cbd4e8cdeca0081acd3487abaf6a6fc315ab94fa6acb93a44098425b`).
- [x] Pin the linked ROM SHA-256
      (`3af6900387531c1f4857e8b56041556028f1518f5da988a6fcf11e42510fbd99`).
- [x] Run default and Windows matrices sequentially and require
      `DONE (160 tests)` from both.

The scanner no longer chooses its primary and alternate physical registers by
hardcoded Z80 identifiers. Target-independent candidate arbitration is still
embedded in pass 5, and Z80 legality/emission rules, other targets, generalized
spill/reload, and join reconciliation remain. `-ra` remains opt-in.

---

## 121. Extract Ordered Candidate Arbitration Into Allocator Core

- [x] Add `register_allocator_choose_candidate_register()` to select the first
      fully eligible opaque physical register from an ordered evaluation set.
- [x] Keep target candidate-legality, path-transparency, and physical-overlap
      queries in pass 5 while moving their ordered arbitration into the core.
- [x] Use the core for byte clobber fallback, byte/word active-conflict
      fallback, and word instruction-legality fallback without changing their
      existing trigger asymmetry.
- [x] Reject null/empty metadata, malformed counts and booleans, sentinel and
      duplicate physical IDs, invalid block indexes, and null output pointers.
- [x] Clear selected register/index outputs before validation and after late
      validation failures so arbitration fails transactionally.
- [x] Add per-candidate and aggregate diagnostics with function, block, reason,
      candidate order, opaque physical ID, all eligibility gates, and result.
- [x] Add `allocator_ra_core_candidate_arbitration` with exact byte/word ROM
      checks for `A -> C`, `HL -> BC`, and unsupported-`HL` -> `BC` fallback.
- [x] Add a strict C90 harness covering first-eligible order, every rejection
      gate, no-match success, negative opaque IDs, malformed inputs, duplicate
      and sentinel IDs, and late-failure transactional output clearing.
- [x] Inspect raw production and harness diagnostics and verify all successful
      paths choose the expected candidate and all malformed inputs fail closed.
- [x] Run 15 focused candidate, policy, conflict, overlap, shift, split, and
      return-convention regressions.
- [x] Verify warning-free GCC/MSVC builds and matching GCC/MSVC assembly
      (SHA-256 `78170058e62fa54ca52e202fa5b5ccd5b591532a51400a8fca3933c941d67bd8`).
- [x] Pin the linked ROM SHA-256
      (`1e7fb4b391959c81674993516541201aa8f046b36e3e54c170363f0a6dfbeb79`).
- [x] Run default and Windows matrices sequentially and require
      `DONE (161 tests)` from both.

Ordered candidate arbitration is now reusable and target-independent. Candidate
fact collection and fallback triggers remain in pass 5; broader extraction,
other targets, generalized spill/reload, and join reconciliation remain.
`-ra` remains opt-in.

---

## 122. Extract Callback-Driven Candidate Evaluation Into Allocator Core

- [x] Add `register_allocator_evaluate_candidate_registers()` and a generic
      physical-register predicate callback contract.
- [x] Move ordered candidate fact collection from pass 5 into the reusable
      core while keeping target-specific legality, path, and overlap queries
      behind thin context adapters.
- [x] Preserve lazy gate order so path safety is queried only for legal
      candidates and overlap only for legal, path-safe candidates.
- [x] Prevalidate sentinel and duplicate opaque physical IDs before dispatching
      any target callback.
- [x] Reject null/empty metadata, malformed counts/capacities, missing
      callbacks, and callback values outside the `YES`/`NO` contract.
- [x] Clear the entire evaluation capacity before validation and after any
      partial callback failure so fact collection fails transactionally.
- [x] Add per-candidate and aggregate diagnostics with function, block, reason,
      candidate order, opaque ID, capacity, gate values, and status.
- [x] Add `allocator_ra_core_candidate_evaluation` with exact byte/word ROM
      checks for clobber, active-conflict, and unsupported-primary paths.
- [x] Add a strict C90 harness with 27 return-checked assertions covering lazy
      callback counts, opaque negative IDs, malformed returns and metadata,
      callback suppression, capacity bounds, and transactional clearing.
- [x] Inspect raw production and harness diagnostics and verify successful gate
      order, immediate invalid-callback termination, and fail-closed outputs.
- [x] Run 21 focused candidate, policy, conflict, overlap, call, shift, split,
      and return-convention regressions.
- [x] Verify warning-free GCC/MSVC builds and matching GCC/MSVC assembly
      (SHA-256 `4d3e0913dade7c56f1b7c5c22f6c70f9531a9a4921fbdbcc0a150ac6aeb5a1b7`).
- [x] Pin the linked ROM SHA-256
      (`1e7fb4b391959c81674993516541201aa8f046b36e3e54c170363f0a6dfbeb79`).
- [x] Run default and Windows matrices sequentially and require
      `DONE (162 tests)` from both.

Candidate iteration, lazy fact collection, and first-eligible arbitration are
now reusable core operations. Fallback triggers and target query implementations
remain local to pass 5; broader extraction, other targets, spill/reload
generalization, and join reconciliation remain. `-ra` remains opt-in.

---

## 123. Extract Candidate Fallback Trigger Planning Into Allocator Core

- [x] Add `register_allocator_plan_candidate_fallback()` with explicit actions
      for primary use, unsupported/path/conflict fallback, and rejection.
- [x] Move fallback-trigger precedence from pass 5 into the reusable core while
      preserving target-specific candidate sets and alternate evaluation.
- [x] Preserve the existing order: unsupported primary, unsafe path, active
      primary conflict, then primary use or deferred slot arbitration.
- [x] Preserve Z80 asymmetry: words may fall back on unsupported primaries,
      bytes may fall back on unsafe paths, and both may fall back on conflicts.
- [x] Reject null function metadata, invalid blocks, and every malformed
      primary/fallback boolean with fail-closed diagnostics.
- [x] Add diagnostics with all primary facts, fallback capabilities, selected
      plan, precedence reason, function, block, and status.
- [x] Add `allocator_ra_core_candidate_fallback_plan` with exact byte/word ROM
      checks for path, conflict, and unsupported-primary fallback.
- [x] Add a strict C90 harness covering all six plan actions, unsupported/path
      precedence, conflict deferral, and eight malformed-input cases.
- [x] Inspect raw production and harness diagnostics and verify precedence,
      successful production plans, and absence of retained-stack failures.
- [x] Run 22 focused candidate, policy, conflict, overlap, call, shift, split,
      and return-convention regressions.
- [x] Verify warning-free GCC/MSVC builds and matching GCC/MSVC assembly
      (SHA-256 `ebbe6a6bc67f8ba96f24d36a210a37a345cad3ce29504671ddc83a6939d14a0a`).
- [x] Pin the linked ROM SHA-256
      (`1e7fb4b391959c81674993516541201aa8f046b36e3e54c170363f0a6dfbeb79`).
- [x] Run default and Windows matrices sequentially; both completed with
      `DONE (163 tests)`.

Primary fallback-trigger precedence is now target-independent core logic.
Target-specific fallback capabilities and candidate evaluation remain in pass
5; broader extraction, other targets, spill/reload generalization, and join
reconciliation remain. `-ra` remains opt-in.

---

## 124. Move Candidate Fallback Capabilities Behind Target Policy

- [x] Add `can_fallback_candidate(size, reason)` to the validated target-policy
      contract and identify missing implementations as `candidate_fallback`.
- [x] Add `register_allocator_resolve_candidate_fallbacks()` with transactional
      outputs and explicit unsupported, path, and conflict capability queries.
- [x] Disable every fallback when the ordered candidate set has no alternate,
      while still validating all policy callback results.
- [x] Preserve Z80 asymmetry: 16-bit candidates support unsupported-primary
      fallback, 8-bit candidates support unsafe-path fallback, and both support
      active-conflict fallback.
- [x] Reject null metadata, missing callbacks, invalid sizes/counts, null output
      pointers, and non-boolean policy results with fail-closed diagnostics.
- [x] Replace pass 5's physical-register comparisons and candidate-count tests
      with capabilities resolved through the active target policy.
- [x] Extend target-policy contract tests from 21 to 22 named hooks.
- [x] Add `allocator_ra_core_candidate_fallback_policy` with three exact-ROM
      paths and a strict C90 policy harness covering callback order, byte/word
      asymmetry, single-candidate suppression, and 13 failure cases.
- [x] Inspect raw diagnostics and verify policy resolution precedes planning,
      malformed callbacks clear outputs, and production has no invalid status.
- [x] Run 23 focused candidate, policy, conflict, overlap, call, shift, split,
      and return-convention regressions.
- [x] Verify warning-free GCC/MSVC builds and matching GCC/MSVC assembly
      (SHA-256 `e4ceb9927cae30f6c197c8c1a371a9dab5a788fa0d90daf8b2c1455b07bb33b6`).
- [x] Pin the linked ROM SHA-256
      (`1e7fb4b391959c81674993516541201aa8f046b36e3e54c170363f0a6dfbeb79`).
- [x] Run default and Windows matrices sequentially; both completed with
      `DONE (164 tests)`.

Fallback capabilities now belong to the target-policy contract, while fallback
precedence and alternate suppression belong to the reusable core. Candidate
legality/path queries, concrete TAC semantics, broader register support, other
targets, generalized spill/reload, and join reconciliation remain. `-ra`
remains opt-in.

---

## 125. Extract Primary Candidate Fact Evaluation Into Allocator Core

- [x] Add `register_allocator_evaluate_primary_candidate()` and an explicit
      primary evaluation containing physical register, legality, path safety,
      and active-conflict state.
- [x] Query legality first, query path safety lazily only for legal candidates,
      and collect active state independently after successful legality/path
      callbacks.
- [x] Reuse pass 5's target legality, path-transparency, and physical-overlap
      callbacks instead of directly computing primary facts in linear scan.
- [x] Preserve Z80 primary semantics for `A` and `HL`, including active overlap,
      unsupported-primary, unsafe-path, and active-conflict fallback planning.
- [x] Clear every output fact transactionally before validation and reject null
      metadata, sentinel registers, missing callbacks, and malformed booleans.
- [x] Add diagnostics containing function, block, physical register, all three
      facts, invalid callback fact, and completion status.
- [x] Add `allocator_ra_core_primary_candidate_evaluation` with three exact-ROM
      paths and a strict C90 harness covering callback order, lazy path checks,
      independent active checks, three malformed callbacks, and eight invalid
      input cases.
- [x] Inspect raw diagnostics and verify primary evaluation precedes fallback
      capability resolution and planning on every production path.
- [x] Run 25 focused candidate, policy, conflict, overlap, call, shift, split,
      and return-convention regressions.
- [x] Verify warning-free GCC/MSVC builds and matching GCC/MSVC assembly
      (SHA-256 `747e14d9e1e7cd153b1153c4ddd81a7a93afc69e3657a5372ad1b5b5d46e0c7a`).
- [x] Pin the linked ROM SHA-256
      (`1e7fb4b391959c81674993516541201aa8f046b36e3e54c170363f0a6dfbeb79`).
- [x] Run default and Windows matrices sequentially; both completed with
      `DONE (165 tests)`.

Primary and alternate candidate fact collection are now callback-driven core
operations. Concrete target legality and path/overlap queries remain in pass 5;
broader register support, other targets, generalized spill/reload, and join
reconciliation remain. `-ra` remains opt-in.

---

## 126. Move Equal-Next-Use Preference Behind Target Policy

- [x] Add `prefer_candidate_on_equal_next_use()` to the validated target-policy
      contract and identify missing implementations as
      `equal_next_use_preference`.
- [x] Add `register_allocator_resolve_equal_next_use_preference()` with
      transactional output clearing and fail-closed callback validation.
- [x] Suppress policy dispatch when candidate and active next-use positions
      differ, leaving ordinary nearer/farther arbitration in the core.
- [x] Preserve the Z80 `ARRAY_WRITE` preference for candidate `arg1` over
      active `arg2` when both compete for `A` at the same next-use position.
- [x] Reject null/empty metadata, missing callbacks, invalid blocks, TAC ops,
      operands, physical registers, next-use positions, and output pointers.
- [x] Extend target-policy contract tests from 22 to 23 named hooks.
- [x] Add `allocator_ra_core_equal_next_use_policy` with two exact-ROM paths
      and a strict C90 harness covering policy approval/rejection, unequal-use
      callback suppression, malformed callbacks, and 13 invalid inputs.
- [x] Inspect raw production and harness diagnostics and verify the policy is
      queried only for equal next use before linear-scan arbitration.
- [x] Run 26 focused candidate, policy, conflict, overlap, call, shift, split,
      and return-convention regressions.
- [x] Verify warning-free GCC/MSVC builds and matching GCC/MSVC assembly
      (SHA-256 `f6aa620be4297bc96ff57e93ed8ec74556fe48756870edf296a46a8c12cf6112`).
- [x] Pin the linked ROM SHA-256
      (`67704c374c4f12c5f1ed08dc07e2021455425f68c95916cf7aeffd8ff40b601f`).
- [x] Run default and Windows matrices sequentially; both completed with
      `DONE (166 tests)`.

Equal-next-use arbitration remains target-independent, while the exceptional
Z80 operand/register preference is now policy-owned. Concrete target legality
and path/overlap queries remain in pass 5; broader register support, other
targets, generalized spill/reload, and join reconciliation remain. `-ra`
remains opt-in.

---

## 127. Extract Candidate Interval Boundary Analysis

- [x] Add `register_allocator_interval_is_blocked()` to evaluate whether a
      spill constraint prevents retention from producer to consumer.
- [x] Preserve no-spill, unconditional-spill, missing-boundary, strict
      boundary-between, and boundary-outside semantics without target enums in
      the reusable core.
- [x] Clear the blocked output before validation and reject malformed ranges,
      sentinels, boundaries, metadata, and output pointers.
- [x] Route primary and competing candidate boundary checks through the core
      and fail closed on invalid analysis.
- [x] Add contextual diagnostics with producer, consumer, spill reason,
      boundary, decision, reason, and status.
- [x] Cover seven valid boundary positions and seven invalid inputs in the
      strict C90 `allocator_ra_core_path_analysis` harness.

Candidate spill-boundary interpretation is now target-independent; pass 5
supplies the current Z80 no-spill and unconditional-spill sentinels. `-ra`
remains opt-in.

---

## 128. Extract Candidate Transparent-Range Analysis

- [x] Add `register_allocator_is_transparent_range()` with callback-driven
      instruction transparency from `start + 1` through `end - 1`.
- [x] Preserve immediate rejection at the first non-transparent instruction
      and accept adjacent producer/consumer instructions without callbacks.
- [x] Validate exact callback booleans, clear both outputs transactionally,
      and report the first blocking instruction.
- [x] Route competing-candidate path transparency through the reusable core
      while retaining target TAC interpretation in pass 5.
- [x] Add diagnostics with range bounds, physical register, transparency,
      blocking instruction, reason, and status.
- [x] Cover all-transparent, first-blocker, adjacent, malformed callback, and
      nine invalid-input paths in the strict C90 harness.

Intervening instruction iteration and short-circuit transparency decisions are
now reusable core logic; target transparency callbacks remain policy-owned.
`-ra` remains opt-in.

---

## 129. Extract Candidate Path Arbitration

- [x] Add `register_allocator_evaluate_candidate_path()` to orchestrate the
      complete per-instruction path decision through opaque predicates.
- [x] Preserve lazy order: a nearer candidate accepts immediately; preserving
      overlap, spill-backed constant assignment, and target transparency each
      continue scanning; any remaining instruction rejects the path.
- [x] Preserve the Z80 `HL`/`BC` coexistence exception and the `B`/`C`/`BC`
      spilled-constant exception behind thin pass-5 adapters.
- [x] Validate exact booleans from every predicate, name malformed predicates
      in diagnostics, and clear path/deciding outputs transactionally.
- [x] Add `allocator_ra_core_path_analysis` with five exact-ROM regions for
      byte/word nearer candidates, byte/word blockers, and `HL`/`BC`
      preserving overlap.
- [x] Add 29 strict C90 harness checkpoints covering every lazy branch, four
      malformed callback stages, and 13 path-arbitration invalid inputs.
- [x] Inspect raw production/harness output and verify expected callback
      suppression, deciding instructions, and absence of production failures.
- [x] Run 36 focused boundary, candidate, policy, conflict, overlap, call,
      shift, split, reload, state, and return-convention regressions.
- [x] Complete independent review with no defects or coverage gaps found.
- [x] Verify warning-free GCC/MSVC builds, matching 38-line diagnostics, and
      matching assembly (SHA-256
      `1c53984bb73e49e56733018c8602eb8cf8f5c97f1645adaa4a5b7741a98f1cd9`).
- [x] Pin the linked ROM SHA-256
      (`dc72a114400a4d0a6bfe48c9918a7b57d16f9df66c83ad180eea9b76747a8d82`).
- [x] Run default and Windows matrices sequentially; both completed with
      `DONE (167 tests)`.

Boundary checks, transparent-range scanning, and full candidate-path
arbitration are now reusable core operations. Candidate discovery and concrete
Z80 predicate implementations remain in pass 5; broader register support,
other targets, generalized spill/reload, and join reconciliation remain.
`-ra` remains opt-in.

---

## 130. Consolidate Primary Candidate Decision Orchestration

- [x] Add `register_allocator_decide_primary_candidate()` and an explicit
      decision containing primary facts, all target fallback capabilities, and
      the selected fallback plan.
- [x] Preserve legality-first evaluation, lazy path queries, independent active
      conflict evaluation, policy fallback resolution, and existing fallback
      precedence.
- [x] Route pass 5's primary candidate flow through the consolidated operation
      while keeping TAC legality, path, and overlap facts behind callbacks.
- [x] Clear the complete decision transactionally on invalid inputs, malformed
      callbacks, invalid policy results, or dependent operation failure.
- [x] Add diagnostics containing all input facts, capabilities, plan, and
      completion status.
- [x] Cover all six primary/fallback plan outcomes, lazy callback suppression,
      three malformed fact callbacks, and eight invalid inputs in the strict
      C90 `allocator_ra_core_scan_orchestration` harness.

Primary fact collection, target capability resolution, and fallback precedence
now form one reusable core decision. Alternate selection and concrete target
facts remain outside the core. `-ra` remains opt-in.

---

## 131. Extract Active-Slot Transition Planning

- [x] Add `register_allocator_plan_slot_transition()` with explicit assign,
      replace-active, and keep-active plans.
- [x] Capture displaced temp and retained-interval metadata before replacement
      without exposing TAC or temp-register structures to the core.
- [x] Validate canonical empty/live active-slot state and reject malformed
      ranges, operands, decisions, booleans, and outputs.
- [x] Preserve replace semantics: optionally clear a retained interval, spill
      the displaced temp, retain the candidate, then assign the active slot.
- [x] Preserve assign semantics as retain plus assignment and keep-active
      semantics as a mutation-free plan.
- [x] Add diagnostics containing the decision and every planned mutation.
- [x] Cover assign, keep, retained replacement, and non-retained replacement
      plans plus seven invalid plan inputs in the strict C90 harness.

Linear-scan decisions now produce an inspectable target-independent mutation
plan. Concrete temp and TAC mutation remain callback-owned. `-ra` remains
opt-in.

---

## 132. Extract Ordered Active-Slot Transition Application

- [x] Add `register_allocator_apply_slot_transition()` with target adapters for
      retained-interval clearing, displaced-temp spilling, and candidate
      retention.
- [x] Enforce source order `clear -> spill -> retain -> assign`; suppress every
      callback for keep-active and suppress clear for non-retained intervals.
- [x] Validate callback availability and exact retained booleans before active
      slot assignment, with cleared retained output on every failure.
- [x] Revalidate the current slot against captured displacement metadata before
      callbacks, rejecting stale and double-applied plans without mutation.
- [x] Route pass 5's former inline mutation dispatch through thin adapters while
      preserving all Z80 debug output and temp/TAC behavior.
- [x] Add `allocator_ra_core_scan_orchestration` with three exact-ROM regions,
      strict C90 callback ordering/suppression checks, three callback failures,
      malformed callback output, stale-plan rejection, double-apply rejection,
      and malformed transition coverage.
- [x] Inspect raw production/harness diagnostics and verify all six primary
      plans, assign/replace/keep transitions, exact mutation order, and absence
      of production failures.
- [x] Complete independent review and add its recommended double-apply
      regression; no demonstrated production defect remained.
- [x] Verify warning-free GCC/MSVC builds, matching 1,004-line diagnostics, and
      matching normalized assembly (SHA-256
      `b5b39a26d67cd58776afd2380ac2c4c589f7b74d8c35da7dad6fd008d4760416`).
- [x] Pin the linked ROM SHA-256
      (`5ca774ab731bfe3b1d5f10c2cfaa741e4b92a47ef1eef54f27c8bf0fa5742311`).
- [x] Run default and Windows matrices sequentially; both completed with
      `DONE (168 tests)`.

Primary decision orchestration and active-slot mutation dispatch are now
reusable core operations. Callback failure aborts compilation after the failing
stage; callbacks are intentionally not reversible transactions. Candidate
discovery and concrete Z80 predicates remain in pass 5; broader register
support, other targets, generalized spill/reload, and join reconciliation
remain. `-ra` remains opt-in.

---

## 133. Extract Candidate Range Analysis

- [x] Add `register_allocator_candidate_meets_range()` with explicit
      within-block and strictly-before-decision modes.
- [x] Preserve nearer-candidate semantics by rejecting consumers at or after
      the current decision while allowing endpoint consumers for the
      preserving-overlap path.
- [x] Validate canonical discovered-candidate metadata, range modes, decision
      positions, and output pointers with transactionally cleared eligibility.
- [x] Add diagnostics naming before, at, after, and within-block outcomes.
- [x] Cover all four outcomes plus malformed candidates and five invalid-input
      paths in the strict C90 candidate-probe harness.

Candidate-to-decision range interpretation is now reusable core logic. The
caller still selects the range mode appropriate to its scheduling question.
`-ra` remains opt-in.

---

## 134. Extract Candidate Qualification Arbitration

- [x] Add `register_allocator_qualify_candidate()` with ordered callbacks for
      metadata, interval clearance, target legality, and path transparency.
- [x] Stop at the first rejection, record its named reason and deciding
      instruction, and suppress all later callbacks.
- [x] Require exact `YES`/`NO` callback results, fail closed on malformed
      values, and clear qualification output on every failure.
- [x] Keep TAC metadata, spill boundaries, Z80 legality, and concrete path
      transparency behind thin pass-5 adapters.
- [x] Cover full acceptance, all four rejection stages, callback order and
      suppression, four malformed callbacks, and five invalid inputs.

Qualification ordering and rejection reporting are now target-independent;
the facts themselves remain callback-owned. `-ra` remains opt-in.

---

## 135. Compose Competing-Candidate Probing

- [x] Add `register_allocator_probe_competing_candidate()` to compose
      discovery, range analysis, and qualification into one transaction.
- [x] Treat not-found, range rejection, and qualification rejection as
      successful probe outcomes with explicit rejection reasons.
- [x] Clear the complete probe on invalid input or dependent failure and bound
      producer, block-end, and decision indexes by the instruction count.
- [x] Route both nearer-candidate and HL/BC preserving-overlap checks through
      one pass-5 adapter while retaining their distinct range modes.
- [x] Add `allocator_ra_core_candidate_probe` with four exact-ROM regions and
      a strict C90 harness covering every range/rejection outcome, lazy callback
      behavior, malformed callbacks, not-found discovery, and transactional
      failures.
- [x] Inspect raw production and harness diagnostics, including composed
      deciding instructions, and verify no production probe reports an invalid
      status.
- [x] Run focused candidate, path, scan, conflict, and overlap regressions and
      complete independent review; its decision-bound recommendation was added
      and no demonstrated defect remains.
- [x] Verify warning-free GCC/MSVC builds, matching 1,308-line diagnostics,
      and matching normalized assembly (SHA-256
      `088b8f05d6f05e8e92342d32af5e5e66bfb59522d8968c28d273165e2dbce3b2`).
- [x] Pin the linked ROM SHA-256
      (`1c0e0fb5a04a5c2495cb0bd8a76306080c99d82876c4490ff8de65d1d5c350b7`).
- [x] Fix WLA-DX equal-size section ordering by returning equality correctly
      and using section ID as a deterministic final tie-break; update the
      affected exact-ROM relocation bytes and pass all 191 WLA-DX tests.
- [x] Run both complete SameSameC matrices after the linker fix; default
      `./run_tests.sh` and `./run_tests.sh -windows` each report
      `DONE (169 tests)`.

Competing-candidate discovery and generic qualification orchestration are now
reusable core operations. Concrete TAC queries and Z80 policy facts remain in
pass 5; broader register support, other targets, generalized spill/reload, and
join reconciliation remain. `-ra` remains opt-in.

---

## 136. Compose Ordered Candidate Selection

- [x] Add `register_allocator_select_candidate_register()` to compose ordered
      candidate fact evaluation and first-eligible arbitration into one core
      transaction.
- [x] Clear both the complete evaluation buffer and selected register/index on
      direct validation errors or dependent failures.
- [x] Preserve lazy callback ordering: legality gates path checks, and path
      safety gates conflict checks for each candidate.
- [x] Route pass 5 alternate selection through the composed API while keeping
      concrete TAC legality, path, overlap, and Z80 register sets local.
- [x] Extend the strict C90 candidate-evaluation harness to 44 outcomes,
      including first/third/no selection, callback suppression, three malformed
      callbacks, null context, invalid inputs, dependency failures, and
      transactional output clearing.
- [x] Inspect raw production and harness `candidate_selection` diagnostics and
      verify no production selection reports an invalid status.
- [x] Verify unchanged exact-ROM output for all three candidate-evaluation
      regions, warning-free GCC/MSVC builds, matching 928-line diagnostics,
      and matching normalized assembly (SHA-256
      `cbf0ae77c446c43effe3cba2f9d87b74fcf4ef3d2f7f8fd06ceed431e14f1efa`).
- [x] Run both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (169 tests)`.

Alternate candidate evaluation and arbitration now have one reusable core
entry point. Primary-plan-to-alternate-set dispatch and concrete Z80 facts
remain in pass 5; broader register support, generalized spill/reload, and join
reconciliation remain. `-ra` remains opt-in.

---

## 137. Extract Primary-Plan Dispatch

- [x] Add `register_allocator_dispatch_candidate_plan()` to map every primary
      plan to use-primary, use-alternate, or reject outcomes.
- [x] Preserve required alternate semantics for unsupported/path fallbacks and
      optional alternate semantics for active conflicts, where no eligible
      alternate keeps the primary candidate.
- [x] Invoke one caller-owned alternate selector with explicit path and overlap
      flags while keeping candidate arrays and concrete Z80 predicates local.
- [x] Require exact callback status and coherent selected register/index pairs;
      clear the complete dispatch result on invalid input, malformed callback
      output, or dependent failure.
- [x] Replace pass 5's six-way fallback branch tree with the composed dispatch
      transaction and generic outcome handling.
- [x] Add `allocator_ra_core_candidate_dispatch` with three exact-ROM regions
      and a strict C90 harness covering all six plans, required and optional
      no-alternate outcomes, selector suppression, reason/flag propagation,
      dependency failure, three malformed callbacks, eight invalid inputs, and
      transactional clearing.
- [x] Inspect raw `candidate_dispatch` diagnostics and verify production path,
      conflict, unsupported, and primary outcomes complete without invalid
      status.
- [x] Verify warning-free GCC/MSVC builds, seven focused fixtures under each
      toolchain, matching 934-line diagnostics, and matching normalized
      assembly (SHA-256
      `c8f9774e6f934ffea9701c645fc4c4ede4b1b378c490780d78adcb84c8344d8a`).
- [x] Run both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Primary-plan dispatch is now target-independent, including required versus
optional fallback behavior. Concrete alternate register sets, TAC legality,
path safety, and overlap facts remain callback-owned in pass 5; broader
register support, generalized spill/reload, and join reconciliation remain.
`-ra` remains opt-in.

---

## 138. Compose Candidate Transition Planning

- [x] Add `register_allocator_plan_candidate_transition()` to compose
      equal-next-use policy resolution, linear-scan arbitration, and active-slot
      transition planning into one core transaction.
- [x] Clear the complete aggregate result before validation and again after
      any dependent failure so callers cannot observe partial decisions.
- [x] Preserve pass 5 ordering by keeping split rejection, active-slot lookup,
      and concrete active-temp replaceability checks before the transaction.
- [x] Keep concrete Z80 policy facts local while passing the selected physical
      register, consumer operation/operands, and replaceability into core.
- [x] Extend the strict C90 scan-orchestration harness with assignment,
      replacement, equal-use preference, active retention, callback
      suppression, malformed callback, stale-slot, invalid-input, and
      transactional-clearing coverage.
- [x] Inspect raw production and harness `candidate_transition` diagnostics and
      verify no production transaction reports an invalid status.
- [x] Verify warning-free GCC/MSVC builds, seven focused fixtures under both
      toolchains, matching 1,036-line diagnostics (SHA-256
      `c0a2440921b7de8dc783b640837eaf9389b3bffc011066486aa1dd1f7fbebca1`),
      and matching assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`).
- [x] Run both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Post-slot candidate planning is now one reusable core operation. Concrete
active-temp lookup and replaceability, split handling, transition application,
and Z80 register policy remain at their existing ownership boundaries.
`-ra` remains opt-in.

---

## 139. Extract Active Replaceability Resolution

- [x] Add `register_allocator_resolve_active_replaceability()` to decide
      whether an active slot may be displaced from active-slot state and an
      opaque temp-metadata callback.
- [x] Suppress metadata callbacks for empty and expired slots, query exactly
      once for live occupants, and reject callback results other than `NO` or
      `YES`.
- [x] Clear replaceability output on direct validation and malformed callback
      failures, with explicit empty, expired, available, and missing reasons in
      core diagnostics.
- [x] Compose active replaceability resolution into
      `register_allocator_plan_candidate_transition()` and include the result
      in its transactionally cleared aggregate output.
- [x] Remove active-temp lookup and local replaceability calculation from the
      pass-5 scheduling loop while retaining target-owned metadata lookup in a
      narrow callback.
- [x] Extend the strict C90 scan-orchestration harness with direct empty,
      expired, available, missing, malformed-callback, stale-slot, null/range,
      callback-suppression, aggregate-propagation, and output-clearing tests.
- [x] Inspect raw production and harness `active_replaceability` diagnostics;
      production queries metadata only for the live HL occupant and reports no
      invalid status.
- [x] Verify warning-free GCC/MSVC builds, seven focused fixtures under both
      toolchains, matching 1,044-line diagnostics (SHA-256
      `58a5544d2c355fea89305c7bd09fd446a096e6bca6334918b3431ed7743b987f`),
      matching assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Active-slot replaceability is now target-independent orchestration. Pass 5
retains only the opaque temp-metadata lookup callback; split handling, slot
mapping, transition application, and concrete Z80 policy remain at their
existing ownership boundaries. `-ra` remains opt-in.

---

## 140. Compose Candidate Transition Application

- [x] Add `register_allocator_apply_candidate_transition()` to compose active
      slot-transition application, split-spill mutation preparation, and
      optional spill mutation application in one core operation.
- [x] Preserve callback ordering as clear displaced interval, spill displaced
      temp, retain candidate, assign active slot, then apply the candidate's
      split-spill mutation when required.
- [x] Keep transition and spill mutation contexts opaque and separate so pass
      5 retains target-owned TAC and temp-state callbacks.
- [x] Clear retained-interval and spill-mutation outputs before validation and
      after every dependent failure, with explicit failing-stage diagnostics.
- [x] Replace the three-call post-transition sequence in pass 5 with the
      composed application API without changing target debug messages.
- [x] Extend the strict C90 scan-orchestration harness with assign, keep,
      preserved spill, callback order/suppression, malformed retained output,
      spill callback failure, invalid split, stale transition, null output,
      and aggregate-clearing coverage.
- [x] Inspect raw production and harness `candidate_transition_apply`
      diagnostics; all eight production applications complete in
      `slot_prepare_spill` order with no invalid or dependency-failed status.
- [x] Verify warning-free GCC/MSVC builds, seven focused fixtures under both
      toolchains, matching 1,060-line diagnostics (SHA-256
      `36cfd101da52bcdb6599feb83dbc0f40c134b7fe1dec35110732f5709a3d6718`),
      matching assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Candidate transition application is now target-independent orchestration.
Pass 5 retains the concrete TAC/temp callbacks, split-policy selection, slot
mapping, and target debug messages. `-ra` remains opt-in.

---

## 141. Compose Candidate Selection Finalization

- [x] Add `register_allocator_finalize_candidate_selection()` to compose
      primary-candidate decision, fallback dispatch, and split-action planning
      into one target-independent core transaction.
- [x] Return a cleared aggregate containing the primary decision, dispatch
      evidence, final selected physical register, split action, and whether
      scheduling should proceed.
- [x] Treat unsupported/path and split-policy rejections as successful
      decisions with `proceed=no`, while clearing all output after dependent
      failures and naming the failing stage in diagnostics.
- [x] Preserve callback laziness and order: primary fact callbacks run first,
      alternate selection runs only for fallback plans, and split planning
      consumes the register selected by dispatch.
- [x] Keep target fact and alternate-selection contexts opaque and separate;
      pass 5 still constructs concrete Z80 contexts and owns skip/alternate
      debug messages.
- [x] Replace pass 5's three-call selection sequence with the composed API
      while retaining transition planning and application at their existing
      boundaries.
- [x] Extend the strict C90 scan-orchestration harness with primary,
      alternate, unsupported/path rejection, conflict-primary, unsupported and
      mismatched split, alternate-preserve, selector failure/malformed result,
      malformed fact, invalid input, callback-count, and output-clearing tests.
- [x] Inspect raw production and harness `candidate_selection_finalize`
      diagnostics; all eight production finalizations proceed in
      `decide_dispatch_split` order with no invalid or dependency-failed
      status.
- [x] Verify warning-free GCC/MSVC builds, seven established focused fixtures
      plus two split-specific fixtures under both toolchains, matching 1,068-line
      diagnostics (SHA-256
      `f9431a2f529394311f2014291b4ddb4d051650c4c8c1bc1657fc765e9182dda9`),
      matching assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Candidate selection finalization is now target-independent orchestration.
Pass 5 retains target fact collection, alternate selection, transition
planning/application, and target debug messages. `-ra` remains opt-in.

---

## 142. Compose Candidate Selection Preparation

- [x] Add `register_allocator_prepare_candidate_selection()` to compose spill
      interval blocking and lazy split-preservation capability resolution into
      one target-independent preparation transaction.
- [x] Return a cleared result containing `proceed`, interval-blocking evidence,
      whether preservation was queried/supported, and the preservation
      physical register.
- [x] Treat blocked intervals as successful decisions with `proceed=no`, and
      preserve the old missing-policy behavior as unqueried/unsupported rather
      than failure.
- [x] Preserve callback laziness: blocked and single-read candidates suppress
      the target callback; eligible multi-read candidates query it exactly once
      and reject malformed boolean results transactionally.
- [x] Replace pass 5's local interval gate and split-capability query with the
      composed API while retaining target physical-register selection and
      existing target diagnostics.
- [x] Extend the strict C90 scan-orchestration harness with single-read,
      supported/unsupported multi-read, boundary-between/outside,
      unconditional/missing boundary, absent policy, malformed callback,
      malformed candidate/reasons, null output, callback-count, and
      output-clearing tests.
- [x] Add production assertions to the orchestration and split-spill fixtures;
      raw logs show eight clean single-read preparations in orchestration and
      exactly two successful multi-read preservation queries in split-spill.
- [x] Verify warning-free GCC/MSVC builds, seven established focused fixtures
      plus two split-specific fixtures under both toolchains, matching
      1,076-line diagnostics (SHA-256
      `816fab15cac90e4957b1786ebb5834ee7871ace002972609d997e1aebe1d74f7`),
      matching 266-line direct-core diagnostics (SHA-256
      `d6cd25ff55d55ae3ed1cbf3f1e704c36ee2c9eb6c8497b08133ea3f13e80e0c7`),
      unchanged 389-line assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Candidate selection preparation is now target-independent orchestration. Pass
5 retains target register selection, concrete target policy implementation,
selection finalization, transition planning/application, and target debug
messages. `-ra` remains opt-in.

---

## 143. Compose Candidate Selection Orchestration

- [x] Add `register_allocator_orchestrate_candidate_selection()` to compose
      candidate preparation and selection finalization into one
      target-independent transaction.
- [x] Return a cleared aggregate containing preparation evidence, nested
      primary/dispatch/split finalization evidence, and the final `proceed`
      decision.
- [x] Preserve ordered laziness: blocked intervals complete after preparation
      without invoking evaluation or alternate-selection callbacks; eligible
      candidates finalize exactly once in `prepare_finalize` order.
- [x] Preserve rejection semantics as successful decisions with `proceed=no`,
      while preparation/finalization dependency failures clear the complete
      aggregate and identify the failed stage.
- [x] Replace pass 5's separate preparation/finalization calls with the
      composed API while retaining concrete evaluation/dispatch contexts,
      target skip/alternate/split diagnostics, and transition handling.
- [x] Extend the strict C90 harness with primary success, blocked short-circuit,
      supported and unsupported multi-read, alternate/register mismatch,
      malformed preservation callback, malformed primary fact, invalid spill
      reasons, null output, exact callback-count, and full-clearing tests.
- [x] Add production orchestration assertions to the core and split-spill
      fixtures; raw logs show eight clean transactions in orchestration and
      fourteen in split-spill, including two multi-read `PRESERVE` paths in
      exact prepare/finalize order.
- [x] Verify warning-free GCC/MSVC builds, seven established focused fixtures
      plus two split-specific fixtures under both toolchains, matching
      1,084-line diagnostics (SHA-256
      `ab25e3ad1406e7eaded62214daf0b09561516bad4994fd74611e9d5f07be1ccf`),
      matching 322-line direct-core diagnostics (SHA-256
      `e0cd46a134c5d9e4ea4a7e957afe8743abee138246fe91bee1c76b3f5616970d`),
      unchanged 389-line assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Candidate preparation and finalization now form one target-independent
selection transaction. Pass 5 retains target register selection, concrete
callback contexts, transition planning/application, and target debug messages.
`-ra` remains opt-in.

---

## 144. Compose Candidate Transition Preparation

- [x] Add `register_allocator_prepare_candidate_transition()` to compose
      policy-owned active-slot resolution and candidate transition planning
      into one target-independent transaction.
- [x] Return a cleared result containing the validated slot index/name and the
      complete active-replaceability, equal-next-use, linear-scan, and slot
      transition plan.
- [x] Validate policy slot count, storage cardinality, physical-register slot
      index, and nonempty display name before indexing active-slot storage or
      invoking transition dependencies.
- [x] Preserve dependency laziness and transactional failure: malformed policy
      outputs suppress metadata/preference callbacks, while planner failure
      clears all slot and plan evidence and reports `stage=transition_plan`.
- [x] Replace pass 5's local physical-slot lookup and separate transition-plan
      call with the composed API while retaining target diagnostics and
      callback-driven transition application.
- [x] Extend the strict C90 harness with primary/alternate slot mappings,
      invalid/oversized policy counts, storage mismatch, invalid slot index,
      null/empty names, malformed metadata dependency, missing hook, null
      output, exact policy/dependency callback counts, and full-clearing tests.
- [x] Add production assertions to the orchestration and split-spill fixtures;
      raw logs show eight and fourteen clean `slot_resolve_transition_plan`
      transactions respectively, including `A`, `C`, and `HL` mappings and
      both assign and replace-active decisions.
- [x] Verify warning-free GCC/MSVC builds, seven established focused fixtures
      plus two split-specific fixtures under both toolchains, matching
      1,092-line diagnostics (SHA-256
      `62b8eea080a390dda9a1d55af37f6a00ebe124c90f6a0323f44727c69a0ee2ed`),
      matching 346-line direct-core diagnostics (SHA-256
      `4b6330eb74cb6f4c8e8c2845a4dd39e97298d0e40e6dbb8142de361b76216872`),
      unchanged 389-line assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Active-slot resolution and transition planning now form one target-independent
preparation transaction. Pass 5 retains target callback contexts, target debug
messages, and concrete transition/spill mutation application. `-ra` remains
opt-in.

---

## 145. Execute Candidate Transitions

- [x] Add `register_allocator_execute_candidate_transition()` to compose
      candidate transition preparation and application into one
      target-independent transaction with explicit `prepare_apply` ordering.
- [x] Return both nested preparation and application evidence on success, and
      clear the complete aggregate after invalid input, preparation failure, or
      application failure.
- [x] Preserve immutable pre-mutation active-temp and next-use evidence so
      keep-active and replacement diagnostics remain accurate after slot
      mutation.
- [x] Add an optional preparation observer between validated planning and
      application so pass 5 retains target-owned active-slot diagnostics at
      their established pre-mutation boundary without duplicating policy slot
      resolution.
- [x] Replace pass 5's separate preparation and application calls with the
      composed API while retaining target callback contexts, equal-preference
      diagnostics, split-spill insertion, and `-ra` opt-in behavior.
- [x] Extend the strict C90 harness with assign, preserve, replace-active, and
      keep-active executions; exact observer and mutation callback ordering;
      observer suppression on preparation failure; immutable keep evidence;
      split-spill propagation; retain and spill-application failures; malformed
      policy, invalid split action, null output, and full aggregate clearing.
- [x] Add production execution assertions to the orchestration and split-spill
      fixtures. Raw logs show clean preparation, observer, application, and
      target reporting order, fourteen split-spill executions, preserved spill
      insertion after mutation, and no production invalid or dependency
      failures.
- [x] Verify warning-free GCC/MSVC builds, ordering-sensitive target-policy and
      split-spill fixtures, matching 1,100-line normalized diagnostics
      (SHA-256
      `df2af91f047cb9c664862e09808ce628d8c7d34369de15531bd5905dc6a6121f`),
      426-line direct-core diagnostics (SHA-256
      `ab4267ee9cd01cbd4e8e6d32e6e55d80e180cf51fc8a9cc025887f912be0cf03`),
      unchanged 389-line assembly (SHA-256
      `823868b5e980804a7fd2a45221f5915213f0b09abccb4f1b88faf23dbcae7497`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Candidate transition planning and mutation now form one target-independent
execution transaction. Pass 5 retains target-specific callback contexts and
diagnostics through the validated preparation observer. `-ra` remains opt-in.

---

## 146. Execute Reload Mutations

- [x] Add `register_allocator_execute_reload()` and
      `register_allocator_reload_execution` to compose reload-action planning,
      canonical mutation preparation, and callback application into one
      target-independent transaction with explicit `action_prepare_apply`
      ordering.
- [x] Preserve unsupported target registers as successful no-op executions
      with `action=none` and `apply=no`, allowing later supported reload sites
      in the same chain to continue.
- [x] Clear the complete aggregate after invalid input or dependency failure,
      and report whether action planning, mutation preparation, or application
      rejected the execution. A supported target with no callback is an
      application failure; an unsupported target needs no callback.
- [x] Replace pass 5's separate reload-action, mutation-preparation, and
      mutation-application calls with the composed API while retaining target
      callback context, post-mutation reload diagnostics, complete chain
      traversal, and `-ra` opt-in behavior.
- [x] Extend the strict C90 reload-chain harness with three ordered inserts,
      unsupported-middle continuation, callback failure, supported and
      unsupported null-callback contrasts, six invalid-input paths, and full
      nested-output clearing. Its final 85-line trace has SHA-256
      `2fbe96e8c4c9ae92dee1298ec9183648d45e62b6fc5c7d0b7fd8f8f288a83eda`.
- [x] Add production assertions for exactly two clean composed reload
      executions in the byte/word chain fixture and split-spill fixture, with
      no invalid-input or dependency-failure traces. Raw logs preserve action,
      preparation, application, and target-reporting order.
- [x] Verify warning-free GCC/MSVC builds and seven focused fixtures under
      both toolchains, matching 1,214-line normalized production diagnostics
      (SHA-256
      `320266f11b326484549f0364f1335ad9306e266eac6c2c81e5822d5ca079e406`),
      identical 393-line assembly (SHA-256
      `bed12b497e20b72d787e8e9fa9cad20588c971b78df6323288e4c4d07695ea6a`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Reload action planning and mutation now form one target-independent execution
transaction. Pass 5 retains target-specific mutation callbacks and diagnostics.
`-ra` remains opt-in.

---

## 147. Execute Reload Chains

- [x] Add `register_allocator_execute_reload_chain()` and
      `register_allocator_reload_chain_execution` to compose bounded reload
      discovery, per-site target-register selection, and reload execution into
      one target-independent transaction with explicit
      `discover_select_execute` ordering.
- [x] Return discovered, processed, and applied counts plus per-site execution
      evidence. Unsupported middle sites remain successful no-ops and do not
      prevent later supported sites from executing.
- [x] Clear the aggregate, instruction array, and every per-site execution
      after invalid input or dependency failure, with diagnostics that
      distinguish discovery from execution failures. Permit a null mutation
      callback only when every selected site is unsupported and therefore
      requires no application.
- [x] Replace pass 5's reload discovery and per-site execution loop with the
      composed API while retaining the Z80 register selector, target-specific
      post-transaction diagnostics, concrete TAC mutation callback, allocation
      ownership, and `-ra` opt-in behavior.
- [x] Extend the strict C90 harness with three applied sites, unsupported-middle
      continuation, callback failure and complete clearing, ineligible and
      read/write stops, empty chains, supported/unsupported null-callback
      contrasts, six invalid chain inputs, and full output-array clearing. Its
      final 125-line direct trace has SHA-256
      `fecc57dd468ec13ea02429f3cbf749917c8a66447c47c66f1da41ca9b4bb51c3`.
- [x] Add production assertions to the reload-chain and split-spill fixtures.
      Raw logs show `reload_chain_site`, `reload_chain_select`,
      `reload_execute`, `reload_chain_execute`, `target_split_reload`, and
      `linear_scan reload_insert` in that order for both byte and word paths,
      with no invalid-input or dependency-failure traces.
- [x] Verify warning-free GCC/MSVC builds and seven focused reload/split
      fixtures under both toolchains, matching 2,436-line normalized
      diagnostics (SHA-256
      `34b1629e6c53b1d4c8d28ebf509d91cfbcc4277511e43872a4ea0ebdf630fa0a`),
      identical 786-line assembly (SHA-256
      `14e6f8c4ddeaace9be14335ce1dbd3d0fcab7fe77e729abaf5567f4470eb5cea`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Reload-chain discovery, target selection, and mutation execution now form one
target-independent transaction. Pass 5 retains Z80 selection and diagnostics.
`-ra` remains opt-in.

---

## 148. Plan Reload-Chain Storage

- [x] Add `register_allocator_plan_reload_chain_storage()` and
      `register_allocator_reload_chain_storage` to compute checked byte counts
      for the parallel reload-instruction and per-site execution arrays.
- [x] Reject zero/negative capacities, null inputs, `size_t` multiplication
      overflow, and byte totals that cannot be represented exactly by the
      core's C90 `unsigned long` diagnostics; clear both output sizes on every
      failure.
- [x] Replace pass 5's duplicated element-size allocation arithmetic with the
      validated storage plan while retaining target allocation ownership,
      out-of-memory diagnostics, cleanup order, chain execution, and `-ra`
      opt-in behavior.
- [x] Extend the strict C90 harness with exact one/four/maximum-capacity plans,
      architecture-dependent overflow and diagnostic-representability checks,
      null/zero/negative inputs, complete output clearing, and an allocated
      two-site discovery/selection/mutation lifecycle. Its final 148-line trace
      has SHA-256
      `5a51c01c33f89a01a45bff123df389ad65a6825379ee18cae96bb9c81c0d4f5a`.
- [x] Add production assertions to the reload-chain and split-spill fixtures.
      Raw logs show successful 12-byte instruction and 60-byte execution plans
      before discovery on both byte and word paths, with no invalid-input or
      out-of-memory traces.
- [x] Verify warning-free GCC/MSVC builds and eight focused storage/reload/split
      fixtures under both toolchains, matching 2,440-line normalized
      diagnostics (SHA-256
      `3236fc2a6473649923044934e179e884fc69e4c5305cd62e5a57d8b9b9ce0c6c`)
      and unchanged 786-line assembly (SHA-256
      `14e6f8c4ddeaace9be14335ce1dbd3d0fcab7fe77e729abaf5567f4470eb5cea`),
      and both complete regression matrices; default `./run_tests.sh` and
      `./run_tests.sh -windows` each report `DONE (170 tests)`.

Reload-chain parallel-buffer sizing is now checked and target-independent.
Pass 5 retains allocation lifetime and failure reporting. `-ra` remains opt-in.

---

## 149. Plan Control-Flow Storage

- [x] Add `register_allocator_plan_control_flow_storage()` and
      `register_allocator_control_flow_storage` to derive one basic-block slot
      and two CFG-edge slots per instruction, with checked capacities and byte
      totals for both arrays.
- [x] Reject null/zero/negative inputs, doubled-edge integer overflow,
      `size_t` multiplication overflow, and byte totals that cannot be
      represented exactly by the core's C90 `unsigned long` diagnostics; clear
      all capacities and byte counts on every failure.
- [x] Replace both pass-5 basic-block allocation paths and the CFG edge
      allocation path with the validated storage plan while retaining target
      allocation lifetime, out-of-memory diagnostics, cleanup, and `-ra`
      opt-in behavior.
- [x] Add a strict C90 direct harness with exact one/four/forty-instruction
      plans, null/zero/negative/edge-overflow inputs, architecture-dependent
      maximum-capacity checks, complete clearing, and an allocated conditional
      branch lifecycle through basic-block and CFG construction. Its final
      12-line trace has SHA-256
      `e5a02da872ef3a2a91c3ea9a0736b389438e8c52b47c33b489f4772b3d462729`.
- [x] Add production assertions to the CFG extraction and block-exit fixtures.
      Raw logs show successful storage plans before block construction at both
      pass-5 callers, identical capacities across GCC/MSVC, ABI-correct edge
      byte totals, and no invalid-input traces.
- [x] Verify warning-free GCC/MSVC builds and eight focused CFG, storage,
      liveness, and orchestration fixtures under both toolchains, matching
      2,235-line diagnostics after normalizing only ABI-dependent block/edge
      byte totals (SHA-256
      `beb0493ea79b963b46e2eed3a95bbb4615a05a41c74e4bfd9607ed960d7502f8`)
      and identical 1,059-line assembly (SHA-256
      `dd7091f59169d84115df69896a9c1eb98a55d1aadf40086a58ec0ac333e36030`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)`.
- [x] Verify the complete Windows regression matrix with the hash-verified
      staged MSVC binaries; `./run_tests.sh -windows` reports
      `DONE (170 tests)`.

Basic-block and CFG workspace sizing is now checked and target-independent.
Pass 5 retains allocation lifetime and failure reporting. `-ra` remains opt-in.

---

## 150. Plan Control-Flow Instruction Storage

- [x] Extend `register_allocator_control_flow_storage` and
      `register_allocator_plan_control_flow_storage()` with a checked byte
      total for the normalized instruction-descriptor array.
- [x] Reject instruction-byte `size_t` overflow and totals that cannot be
      represented exactly by the core's C90 `unsigned long` diagnostics;
      clear instruction, block, and edge capacities and byte counts on every
      failure.
- [x] Replace the final raw instruction-array size multiplication in both
      pass-5 control-flow collectors with the validated storage plan while
      retaining allocation lifetime, out-of-memory diagnostics, cleanup, and
      `-ra` opt-in behavior.
- [x] Extend the strict C90 direct harness with exact instruction byte totals,
      complete output clearing, architecture-sensitive maximum-capacity
      behavior, and an instruction/block/edge allocated conditional-CFG
      lifecycle. Its final 12-line Cygwin trace has SHA-256
      `2b0eaf918f614bd3492bdb7aa22b95aa53972ff910405e765d62c02287a3cd76`;
      the MSVC x86 trace correctly rejects the maximum plan when its byte
      totals exceed `ULONG_MAX`.
- [x] Add production assertions to the CFG extraction and block-exit fixtures.
      Raw logs show successful instruction, block, and edge storage planning
      before construction at both pass-5 callers, with matching capacities
      and ABI-correct descriptor byte totals under GCC and MSVC.
- [x] Verify warning-free GCC/MSVC builds and eight focused CFG, storage,
      liveness, and orchestration fixtures under both toolchains. The CFG
      fixture has matching 1,064-line diagnostics after CRLF removal and
      normalization of only ABI-dependent instruction/block/edge byte totals
      (SHA-256
      `f930fa2854eec0755bcafd9d5a424fabca005523624a0cd9821d7dbacd9fc0ab`)
      and identical 532-line assembly (SHA-256
      `bb907b6e1d10c0b28aab1a9d2724c23643204155c3570943353dd55fd6e5b5a6`).
- [x] Verify both complete regression matrices; default `./run_tests.sh` and
      Windows `./run_tests.sh windows` each report `DONE (170 tests)` with
      status zero.

Instruction, basic-block, and CFG-edge workspace sizing now share one checked,
target-independent control-flow storage plan. Pass 5 retains allocation
lifetime and failure reporting. `-ra` remains opt-in.

---

## 151. Plan Liveness Storage

- [x] Add `register_allocator_plan_liveness_storage()` and
      `register_allocator_liveness_storage` to derive the checked cell count,
      per-buffer byte count, and four-buffer total for live-use, live-def,
      live-in, and live-out sets.
- [x] Reject null/nonpositive inputs, `int` liveness-index multiplication
      overflow, `size_t` byte/aggregate overflow, and totals that cannot be
      represented exactly by the core's C90 `unsigned long` diagnostics;
      clear all output evidence on every failure.
- [x] Replace both pass-5 liveness and join-reconciliation allocation paths
      with the checked plan, and pass its byte count into set initialization
      instead of recomputing `block_count * temp_count` locally.
- [x] Add an allocator-core `liveness_storage` diagnostic with block, temp,
      cell, per-buffer byte, aggregate byte, and completion status fields.
      Production traces show each plan immediately before its solve: the
      diamond uses four 9-byte buffers and the loop uses four 5-byte buffers.
- [x] Extend `allocator_ra_core_liveness_extraction` with a strict C90 direct
      harness covering exact 1x1, 4x40, and 9x3 plans; null, zero, negative,
      integer-overflow, null-output, and architecture-sensitive maximum
      inputs; complete clearing; and an allocated two-block/two-temp solver
      lifecycle. Its final 13-line Cygwin trace has SHA-256
      `7826234d9981439ad4d7cfc975f30b583d31de76fb6b53e588100a325a718078`.
- [x] Verify the direct harness with GCC `-ansi -pedantic -Wall -Werror` and
      MSVC `/W4 /WX`. LP64 accepts four `INT_MAX` byte buffers, while MSVC x86
      correctly rejects their 8,589,934,588-byte aggregate as greater than
      `ULONG_MAX`.
- [x] Verify eight focused liveness, CFG, storage, and orchestration fixtures
      under GCC and MSVC, matching 785-line diagnostics after CRLF removal and
      normalization of only the established ABI-dependent control-flow
      descriptor byte totals (SHA-256
      `56d2e0ef8317fea97c574b6f551c25f8ba1ec9fc59af20c606397dccc43a48f8`)
      and identical 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify both complete regression matrices; default `./run_tests.sh` and
      Windows `./run_tests.sh -windows` each report `DONE (170 tests)` with
      status zero.

The four parallel liveness bitsets now use checked, target-independent storage
planning. Pass 5 retains allocation lifetime, use/def collection, diagnostics,
and stack-only join reporting. `-ra` remains opt-in.

---

## 152. Validate Liveness Solver Storage

- [x] Make `register_allocator_solve_liveness()` consume the checked
      `register_allocator_liveness_storage` descriptor from Phase 151 instead
      of recomputing `block_count * temp_count` and its byte count internally.
- [x] Validate function identity, descriptor presence and consistency, all
      four bitset pointers, edge count/storage, and iteration output before
      clearing or indexing any nonempty liveness buffer.
- [x] Add a fail-closed `liveness_solve ... status=invalid_storage` diagnostic
      carrying blocks, edges, temps, cells, per-buffer bytes, aggregate bytes,
      and zero iterations. Rejected calls leave live-in/live-out evidence
      untouched while clearing the iteration count when available.
- [x] Preserve the existing empty-input success contract and converged/edge/
      convergence diagnostics, while using the descriptor's checked cell and
      byte counts for initialization and iteration limits.
- [x] Extend the strict direct harness with 13 malformed solver contracts:
      null function/descriptor/use/def/in/out/iterations, inconsistent cell,
      buffer, and aggregate counts, negative edge count, and missing edge
      storage, plus an `INT_MAX` cell count whose `int` iteration bound cannot
      be represented. Also verify rejection does not mutate bitsets and retain
      the allocated two-block/two-temp solve plus empty solve lifecycle.
- [x] Verify the 28-line direct harness with GCC
      `-ansi -pedantic -Wall -Werror` and MSVC `/W4 /WX`; both traces contain
      exactly 13 `invalid_storage` results. The final 28-line Cygwin trace has
      SHA-256
      `05bfbbfcdd38c2922c6831ac38a49ae79cab1df8a791fc1f1880c4af02da0750`;
      its expected architecture-sensitive maximum-plan result remains the only
      difference from the MSVC x86 trace.
- [x] Verify eight focused liveness, CFG, storage, and orchestration fixtures
      under GCC and MSVC. Valid production diagnostics remain the identical
      785-line Phase 151 stream after CRLF removal and normalization of only
      established ABI-dependent control-flow descriptor bytes (SHA-256
      `56d2e0ef8317fea97c574b6f551c25f8ba1ec9fc59af20c606397dccc43a48f8`),
      and assembly remains identical at 373 lines (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify both complete regression matrices; default `./run_tests.sh` and
      Windows `./run_tests.sh -windows` each report `DONE (170 tests)` with
      status zero.

The liveness solver now accepts only a validated storage layout and no longer
derives buffer bounds from unchecked dimensions. Pass 5 retains allocation,
use/def collection, and stack-only join reporting. `-ra` remains opt-in.

---

## 153. Extract Liveness Use/Def Collection

- [x] Add callback-driven `register_allocator_collect_liveness_use_def()` to
      the target-independent core and route pass 5's per-block liveness input
      construction through it while keeping TAC/register-index adaptation in
      the Z80 integration.
- [x] Preserve read-before-write semantics, first-use-before-definition
      suppression, inactive-instruction skipping, and dense block-local temp
      indexing. The core clears valid output sets before collection.
- [x] Validate function identity, block/range/count inputs, all three
      callbacks, both output buffers, and independent use/def storage before
      mutation. Invalid inputs leave available output evidence untouched;
      non-boolean callback results fail closed, clear both sets, and identify
      the failing predicate, instruction, and temp.
- [x] Add `liveness_use_def` diagnostics carrying block/range dimensions,
      use/definition counts, `order=read_before_write`, and completion status.
      Production emits 28 matching records under GCC and MSVC: two passes over
      the diamond's nine blocks and loop's five blocks.
- [x] Extend the strict direct harness with simultaneous read/write,
      definition-before-later-read suppression, inactive read/write skipping,
      first use after an inactive write, stale-output clearing, and 13 invalid
      input contracts including aliased output storage and evidence
      preservation. A fourteenth case rejects null callback context. Three
      additional cases reject invalid active/read/write callback results and
      verify partial-output clearing. The final GCC and MSVC traces each
      contain 46 lines and identical 18-line use/def sections;
      their SHA-256 values after CRLF removal are respectively
      `85fff882e2f198dd54964d29248cc452535394ca3d475de244f9595fb7a01872`
      and
      `fc2eca8bb90f8452088658f7cbdbb5500c055ea208087b481eb2211b76b1eeca`;
      two established architecture-sensitive storage records explain the full
      trace difference.
- [x] Verify the focused production fixture under GCC and MSVC. After CRLF
      removal and normalization of only established ABI-dependent control-flow
      instruction/edge byte totals, both complete 813-line diagnostics match
      (SHA-256
      `152386fd9da9e848ebc4a91585f4b8251dc286417bf7f0721b58e3ff1dc3ea37`),
      and assembly remains identical at 373 lines (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Liveness use/def set construction is now callback-driven and target-independent.
Pass 5 retains TAC operand interpretation, allocation lifetime, and stack-only
join reporting. `-ra` remains opt-in.

---

## 154. Extract Stack-Only Join Reconciliation Traversal

- [x] Add callback-driven
      `register_allocator_reconcile_stack_only_joins()` to the reusable core
      and route pass 5's join traversal through it while keeping Z80 temp-state
      interpretation and existing target diagnostics local.
- [x] Move predecessor counting, structural-join discovery, live-in traversal,
      stack-versus-retained classification, and retained-live-in rejection into
      core. Edgeless and loop-only CFGs complete with zero joins.
- [x] Validate the Phase 151 liveness descriptor, CFG edge endpoints, live-in
      values, callback dependencies, and callback results fail closed. New
      `register_allocator_core: join_reconcile` diagnostics report graph
      totals, join/temp identity, predecessor count, state/action, and distinct
      invalid-input, invalid-edge, invalid-liveness, invalid-callback, and
      retained-live-in statuses.
- [x] Extend the strict direct harness with a two-live-temp diamond, retained
      failure after an accepted stack temp, invalid join/temp callback results,
      malformed live-in state, edgeless and loop-only graphs, malformed
      storage/edges, null dependencies, and empty dimensions. GCC and MSVC each
      emit 70 lines; their SHA-256 values after CRLF removal are respectively
      `2219db178280810b9fc162c434d400684329e5f4764164032e2bfc192de21b48`
      and
      `f5fabb82133a67975c5cce3301e1cc653297bba33dcff215dccf4d1d4b5f0059`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. The diamond reports two
      structural joins with one live temp; the loop reports no join. After
      CRLF removal and normalization of only established ABI-dependent
      control-flow descriptor bytes, both 816-line diagnostics match
      (SHA-256
      `6e73eaac9f40cc75b828961f7bd11e88e6739ed9c1da41d2d81d49145fbce6f0`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Stack-only join discovery and validation are now target-independent. Pass 5
retains Z80 temp-state interpretation and concrete diagnostics; cross-block
physical-register retention remains intentionally disabled. `-ra` remains
opt-in.

---

## 155. Validate Liveness Buffer Index Resolution

- [x] Replace the public unchecked `register_allocator_liveness_index()` with
      descriptor-driven `register_allocator_resolve_liveness_index()`. Keep
      raw multiply/add private to core loops that first validate the complete
      Phase 151 storage descriptor.
- [x] Route pass 5's use/def buffer slices and liveness debug slices through
      checked resolution. Validate function identity, positive dimensions,
      block/temp bounds, exact cell/byte/aggregate layout, storage/output
      presence, and reset available rejected outputs to `-1` before returning.
- [x] Add `register_allocator_core: liveness_index` diagnostics carrying
      dimensions, requested block/temp, planned cells, resolved index, and
      completion status. Focused production emits 27 valid diamond records and
      15 valid loop records with no invalid resolution; diamond block 8 resolves
      to cell 8 in every collection/debug pass.
- [x] Extend the strict direct harness with first, middle, and last cell
      resolution plus 12 malformed contracts covering null dependencies,
      nonpositive dimensions, negative/out-of-range indexes, and independently
      corrupted cell, byte, and aggregate storage fields. GCC and MSVC each
      emit 89 lines; their SHA-256 values after CRLF removal are respectively
      `8816253f62c94e59a6f3dcca20c293ec3656dade689a65914998224f07fe9317`
      and
      `e7233373817a94e7d7b489139eb8a8110f2d5b2619e794f93c8300a0b639e135`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 858-line diagnostics match (SHA-256
      `b86e3ca3ece1707819081d10afdb61d87343e52edfbc3410ad113011bbdd9b2b`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Every public liveness buffer offset now consumes a validated storage layout;
unchecked arithmetic is private to core loops whose owning entry points first
validate that same layout. `-ra` remains opt-in.

---

## 156. Extract Join Temp-State Classification

- [x] Add `register_allocator_classify_join_temp_state()` to classify the
      existing generic `register_allocator_temp_state` as stack-resident or
      retained at a structural join. Route pass 5's Z80 join callback through
      this core decision while preserving existing target diagnostics and the
      stack-only rejection policy.
- [x] Reuse the temp-state invariant shared by generic retain/spill transitions:
      stack means `spill_required=yes` with the no-register sentinel, while
      retained means `spill_required=no` with a concrete physical register.
      Impossible mixed states fail before join policy evaluation.
- [x] Add `register_allocator_core: join_temp_state` diagnostics carrying join
      dimensions, dense temp index, spill/register state, target no-register
      sentinel, stack result, classification, and completion status. Focused
      production emits exactly one valid record for the diamond's live join
      temp and none for the loop.
- [x] Extend the strict direct harness with both valid classifications and nine
      malformed contracts covering null dependencies, invalid join dimensions,
      invalid spill booleans, spill-with-register, and retained-without-register.
      Rejected calls reset available output to `NO` and preserve input state.
      GCC and MSVC each emit 100 lines; their SHA-256 values after CRLF removal
      are respectively
      `83e8d4602d09390ec9c0e3a3a8d1396e95491c038df979376265b919b7b5b613`
      and
      `1399686d5e41d6afbf3b85a6da5d26770f237a6e4ba5d36197c78da7d69d07dd`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 859-line diagnostics match (SHA-256
      `4e4a80ad772a48515fe25377133e2d9a454e96066a752f0f8934f63ce00d1868`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Join stack-versus-retained classification now consumes the same validated core
temp-state contract as allocation transitions. Pass 5 only adapts Z80 temp
metadata and retains target diagnostics. `-ra` remains opt-in.

---

## 157. Extract Join Action Planning

- [x] Add `register_allocator_plan_join_action()` to map validated join temp
      residency to the existing actions: stack-resident temps reload on demand,
      while retained temps require predecessor spills. Route generic join
      reconciliation through this planner without enabling cross-block
      mutation; stack-only policy still rejects the latter action.
- [x] Reset available rejected outputs to zero and fail closed for null
      dependencies, negative block/temp indexes, non-join predecessor counts,
      and invalid residency booleans.
- [x] Add `register_allocator_core: join_action_plan` diagnostics carrying join
      dimensions, dense temp index, stack residency, planned action, and
      completion status. Focused production emits exactly one valid
      `reload_on_demand` record for the diamond and none for the loop.
- [x] Extend the strict direct harness with both valid action outcomes and six
      malformed contracts. Reconciliation coverage additionally proves that a
      retained diamond temp plans `spill_predecessors` before the existing
      stack-only policy rejects it. GCC and MSVC each emit 112 lines; their
      SHA-256 values after CRLF removal are respectively
      `76d7fdd48899d75526683e7b18e9bcb5a6d5eba7dee9219e0575e01ad6679580`
      and
      `e93be9fec45b551a12d7a36d80267f98fb9b707a7d120e31af943b612291cc0d`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 860-line diagnostics match (SHA-256
      `05f7fdca508de1acaa4363481209cf80fb0df1072e441b968bee120b788970d5`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Join action selection is now an explicit reusable core contract, but concrete
predecessor spill placement and CFG mutation are intentionally deferred.
`-ra` remains opt-in.

---

## 158. Extract Join Predecessor Collection

- [x] Add `register_allocator_collect_join_predecessors()` with count-only and
      caller-buffer collection modes. Preserve CFG edge order so later
      cross-block spill planning can address each predecessor deterministically.
- [x] Use a two-pass contract that validates every edge and required capacity
      before writing predecessor indexes. Rejected calls reset an available
      count to zero and preserve the caller's predecessor buffer.
- [x] Route stack-only reconciliation's local predecessor count loop through
      the reusable count-only mode without changing join policy or target
      mutation. Existing reconciliation edge diagnostics remain intact.
- [x] Add `register_allocator_core: join_predecessors` diagnostics carrying
      function, join block, CFG dimensions, output capacity, predecessor count,
      mode, and completion status. Focused production shows diamond blocks 4
      and 7 each have two predecessors; only block 4 has a live join temp. The
      loop has at most one predecessor per block.
- [x] Extend the strict direct harness with count-only, ordered collection,
      single-predecessor, no-predecessor, and edgeless cases plus 12 malformed
      contracts covering null dependencies, dimensions, block bounds, mode,
      capacity, and invalid source/destination edges. GCC and MSVC each emit
      153 lines; their SHA-256 values after CRLF removal are respectively
      `ba192b61a88961c4524f888523e881229bf5556c7a6fb0265039370ea6e9ef91`
      and
      `659f06d5736e6b2a3dbb3d24b10501de93c1f8dc2da00d2fbea3ea1fd78b722a`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 874-line diagnostics match (SHA-256
      `af563d862f5cfbc6f82f3861312a9582d706cf7c4e9d87762fd4c22333aaba90`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Join reconciliation can now identify predecessor blocks through a reusable,
validated core contract. Mapping those blocks to target-approved terminal spill
sites and applying mutations remain intentionally deferred. `-ra` remains
opt-in.

---

## 159. Plan Join Predecessor Spill Sites

- [x] Add `register_allocator_plan_join_spill_sites()` and the reusable
      `register_allocator_join_spill_site` descriptor. Map each incoming CFG
      edge to its predecessor block, edge kind, terminal anchor TAC, and
      insertion placement while preserving CFG edge order.
- [x] Define explicit placement semantics: jump and conditional edges spill
      before their control-transfer anchor, while label-boundary fallthrough
      spills after the predecessor's ordinary tail instruction.
- [x] Validate the complete CFG, predecessor block ranges, terminal TAC ranges,
      edge/end-reason consistency, fallthrough adjacency, branch-false
      adjacency, and caller capacity before writing any site descriptor.
      Rejected calls reset an available site count and preserve caller storage.
- [x] Add count-only planning to pass 5's join-debug path before stack-only
      reconciliation. This validates every production CFG site without applying
      spills or changing join policy.
- [x] Add `register_allocator_core: join_spill_site` diagnostics carrying join
      block, predecessor, edge kind, anchor TAC, block end reason, placement,
      and status. Focused production maps diamond join block 4 to jump
      predecessor 2 before TAC 10 and fallthrough predecessor 3 after TAC 12;
      join block 7 follows the same pattern. The loop back edge maps before its
      jump at TAC 45.
- [x] Extend the strict direct harness with mixed jump/branch/fallthrough
      ordering, branch-false, count-only, no-site, and capacity cases plus 20
      malformed contracts covering null dependencies, dimensions, bounds,
      modes, edge kinds, terminal ranges, end reasons, and adjacency. GCC and
      MSVC each emit 185 lines; their SHA-256 values after CRLF removal are
      respectively
      `a54e2d88d0a37b42eb419647ec6f6796b502b46d5ed9cbd7fb7f50d11e7e4c30`
      and
      `e64a133aa5b57374c2e28a8766562e3f0062895a3f0c58c49efa3b24c2e98af5`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 899-line diagnostics match (SHA-256
      `02f845f224e086f1519dadd9add1d62c17531b8a69d152ed59745b2e9b2e0c45`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Cross-block join spills now have deterministic generic insertion anchors and
placement semantics. Target approval, canonical spill mutation preparation,
and mutation application remain deferred; `-ra` remains opt-in.

---

## 160. Approve Join Spill Sites Through The Target

- [x] Add `register_allocator_approve_join_spill_sites()` and a generic
      site-approver callback. Validate the complete descriptor batch before
      invoking any target callback, then approve sites in deterministic order
      under an all-or-nothing policy.
- [x] Reset an available approved count to zero on every rejection. Reject
      malformed descriptors, invalid callback results, and target refusals
      without treating partial callback progress as approval.
- [x] Add a Z80 approver that verifies each anchor belongs to the current
      function, before-placement anchors are jump TACs, and after-placement
      anchors are ordinary non-jump TACs. Invoke planning and approval only for
      structural joins; no TAC mutation is performed.
- [x] Add `register_allocator_core: join_spill_approval` and
      `register_allocator: target_join_spill_site` diagnostics carrying site
      order, predecessor, edge, anchor, placement, target decision, aggregate
      count, and policy. Focused production approves jump TACs 10 and 20 before
      their anchors and ordinary `ASSIGNMENT` TAC 12 and `ADD` TAC 22 after
      their anchors. Both diamond joins approve two sites atomically; the loop
      emits no join approval.
- [x] Extend the strict direct harness with all-approved, ordered two-site,
      second-site rejection, invalid callback, empty batch, and 15 malformed
      input/descriptor contracts. Invalid descriptor batches invoke no target
      callback. GCC and MSVC each emit 208 lines; their SHA-256 values after
      CRLF removal are respectively
      `24a57478ad0e5f4d5f482764ee1346dde3869a97a0b427039399b7a91fc8f81c`
      and
      `e8b52829d2b3fc9b42efc15152257664814cdb8ed3e0466f7cc6adefc9cd0e77`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 915-line diagnostics match (SHA-256
      `d3d3325314b97a5254cefb104a76553ff5f412e64f1525df719682f803db8890`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default regression matrix; `./run_tests.sh` reports
      `DONE (170 tests)` with status zero.
- [x] Verify the complete Windows regression matrix; `./run_tests.sh -windows`
      reports `DONE (170 tests)` with status zero.

Every planned join spill site now passes a fail-closed target approval gate.
Canonical mutation descriptors, concrete application, and post-mutation state
validation remain deferred; `-ra` remains opt-in.

---

## 161. Prepare Canonical Join Spill Mutations

- [x] Add `register_allocator_join_spill_mutation` as a target-independent,
      descriptor-only contract carrying apply state, the approved predecessor
      site, temp index, and retained physical register. Stack offsets remain a
      target emission concern.
- [x] Add `register_allocator_prepare_join_spill_mutation()` with fail-closed,
      output-resetting validation. Reload-on-demand produces a canonical empty
      plan; spill-predecessors copies one approved site into one applied plan.
- [x] Validate edge kind and before/after placement consistency, require the
      no-register sentinel for reload plans and a concrete register for spill
      plans, and preserve the caller's site descriptor on every path.
- [x] Prepare descriptors after atomic target approval in pass 5 without
      applying them or changing stack-only reconciliation. Focused production
      naturally prepares two no-op plans for stack-resident `r0` at diamond
      join block 4: jump predecessor 2 before TAC 10 and fallthrough
      predecessor 3 after TAC 12.
- [x] Extend the strict direct harness with retained applied and stack-resident
      empty plans plus 14 malformed contracts covering null dependencies,
      indices, actions, register-state mismatches, descriptor fields, and
      edge/placement mismatches. GCC and MSVC each emit 224 lines; their
      SHA-256 values after CRLF removal are respectively
      `9b12a70f284a843c3575b2bda9e57c39883c0e57e63334e411cc6259b81f3c75`
      and
      `c88b80eae129f9f1e0dc262324978067b49c139eb9342036c26447443ae51686`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 921-line diagnostics match (SHA-256
      `68d7d0eebedc0f9988cb5d8a3cf011d26263f2fafe10f88a4ba0b0c4d51f5243`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default and Windows regression matrices each report
      `DONE (170 tests)` with status zero.

Join spill decisions now become canonical generic descriptors after target
approval. Concrete insertion, target spill emission, post-mutation state
validation, and retained-live-in enablement remain deferred; `-ra` remains
opt-in.

---

## 162. Apply Canonical Join Spill Mutations Through A Callback

- [x] Add `register_allocator_apply_join_spill_mutation()` and a generic
      applier callback carrying join block, approved predecessor site, temp,
      and physical register.
- [x] Validate canonical empty and applied descriptors before dispatch. Empty
      reload-on-demand plans succeed without requiring or invoking a callback;
      applied plans require a concrete register, valid edge/placement pairing,
      and callback.
- [x] Report successful callback application, callback failure, invalid input,
      and empty-plan skips distinctly. The core never mutates the caller's
      descriptor.
- [x] Route pass 5's prepared production plans through the application gate.
      Current stack-resident diamond join block 4 emits two skipped empty-plan
      records; no callback or TAC mutation occurs. A future retained plan fails
      closed until the Z80 insertion callback is supplied.
- [x] Extend the strict direct harness with successful callback capture,
      callback rejection, descriptor preservation, empty-plan skipping, and 14
      malformed contracts covering null dependencies, block/apply state,
      noncanonical empty plans, site fields, edge/placement mismatches, temp,
      register, and callback presence. Invalid descriptors invoke no callback.
      GCC and MSVC each emit 241 lines; their SHA-256 values after CRLF removal
      are respectively
      `bf9975f5cb10113a4fc8fd7303da1a01055dbfeaab42ce6229c36610ba1c3503`
      and
      `77b391aebaf597fa11b993d867783fb73c9f00ac3552c0adadc492df274fdfd4`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. After CRLF removal and
      normalization of only established ABI-dependent control-flow descriptor
      bytes, both 923-line diagnostics match (SHA-256
      `b414f13d4d6c6835574d79b670978a591d7801d6a2cf2f18dec1946d3a097d8a`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default and Windows regression matrices each report
      `DONE (170 tests)` with status zero.

Canonical join spill mutations now pass through the same callback-driven,
fail-closed application boundary as block-local spill/reload mutations.
Concrete Z80 insertion, target spill emission, post-mutation CFG/liveness
validation, and retained-live-in enablement remain deferred; `-ra` remains
opt-in.

---

## 163. Prepare Join Spill Emission Metadata

- [x] Add `register_allocator_join_spill_emission`, preserving the approved
      predecessor site alongside emit state, temp, physical register,
      destination offset, and byte count. Target stack offsets remain inputs
      resolved by pass 5 rather than generic allocator policy.
- [x] Add `register_allocator_prepare_join_spill_emission()`. Validate the
      canonical join mutation first, delegate ordinary slot/offset/width
      validation to `register_allocator_prepare_spill_emission()`, and reset
      output on every failure.
- [x] Map empty reload-on-demand mutations to canonical no-emission metadata.
      Applied predecessor-spill mutations require an available target spill
      slot and preserve before/after placement in the resulting descriptor.
- [x] Integrate emission preparation before the Phase 162 application gate.
      Pass 5 supplies each retained temp's existing frame offset and byte size;
      current stack-resident production plans supply canonical empty values and
      perform no TAC mutation.
- [x] Extend the strict direct harness with applied jump and fallthrough
      emissions, canonical empty output, mutation preservation, nine malformed
      join contracts, and six invalid target spill-metadata contracts. The
      latter remain distinguishable as `invalid_spill_emission`. GCC and MSVC
      each emit 268 lines; their SHA-256 values after CRLF removal are
      respectively
      `f247a4f0c5ca1fe8750c741d55b94cebafde0c517da0013d20664529078d7b94`
      and
      `c614dc9cfb0969dd40236aa98dcc8f5a2292785689a33aa9b277707dea75fcc9`.
      The full traces differ only in the two established ABI-sensitive
      liveness-storage records.
- [x] Verify focused GCC/MSVC production parity. Each natural empty join plan
      emits one generic no-spill record and one join-specific empty-emission
      record. After CRLF removal and normalization of only established
      ABI-dependent control-flow descriptor bytes, both 927-line diagnostics
      match (SHA-256
      `e54e07ab52fc5d614dc9e44e99981de939a9c94d1d8f184ba29867111e7ad61b`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default and Windows regression matrices each report
      `DONE (170 tests)` with status zero.

Approved join spills now carry validated target offset and width metadata up
to the concrete insertion boundary. Z80 TAC insertion, post-insertion
CFG/liveness validation, and retained-live-in enablement remain deferred;
`-ra` remains opt-in.

---

## 164. Apply Join Spill Emissions Through A Callback

- [x] Add `register_allocator_join_spill_emission_applier` and
      `register_allocator_apply_join_spill_emission()`. The generic boundary
      validates the complete canonical descriptor before dispatch and passes
      join block, predecessor placement, temp, physical register, destination
      offset, and byte count to one target callback.
- [x] Skip canonical empty emissions without requiring or invoking a callback.
      Applied emissions require a callback and distinguish target rejection as
      `callback_failed`; malformed descriptors fail closed as `invalid_input`.
- [x] Route pass 5's prepared join emissions through the new application
      boundary. Current production joins remain stack-resident, so both
      predecessor plans emit deterministic empty-plan skips and no TAC is
      inserted.
- [x] Extend the strict direct harness with successful jump and fallthrough
      dispatch, callback rejection, canonical empty skip, descriptor
      preservation, captured callback arguments, and eleven malformed
      contracts that invoke no callback. GCC and MSVC each emit 283 lines.
      After replacing only the two established ABI-sensitive liveness records,
      the traces match with SHA-256
      `3f9af8b96736450a5dd82939fb69a2ea499cb63d6bfb08336478f1cfd5646833`.
- [x] Verify focused GCC/MSVC production parity. Both 927-line diagnostics
      contain exactly two empty application skips and match after normalizing
      only established control-flow descriptor-width fields (SHA-256
      `5231a2a895663c7f96f2cc1907a27f48747f0ba59244e2f1e656a84b2f926828`).
      Exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default and Windows regression matrices each report
      `DONE (170 tests)` with status zero.

Validated join spill metadata now reaches a target callback boundary without
changing current output. Concrete Z80 TAC insertion, post-insertion CFG and
liveness validation, and retained-live-in enablement remain deferred; `-ra`
remains opt-in.

---

## 165. Order Join Spill Sites For Stable Mutation

- [x] Add `register_allocator_order_join_spill_sites()`. Atomically validate
      the complete approved site batch before mutation, then stably order it
      by descending TAC anchor so later concrete insertion cannot shift any
      anchor that has not yet been processed.
- [x] Preserve structural CFG order through target approval and apply the new
      ordering only before pass 5 prepares per-site mutations and emissions.
      Current production remains descriptor-only and performs no TAC changes.
- [x] Add diagnostics for each ordered predecessor and a batch summary with
      the explicit `descending_anchor` policy. Invalid API and site contracts
      are distinguished and leave the complete input batch untouched.
- [x] Extend the strict direct harness with mixed-order sorting,
      already-sorted input, equal-anchor stability, an empty batch, five
      malformed API contracts, seven malformed site contracts, and complete
      input-preservation checks. GCC and MSVC each emit 309 lines. After
      replacing only the two established ABI-sensitive liveness records, the
      traces match with SHA-256
      `c2692ca9322b48a7bb5749c03b0a986a40864a9adc8aeefa8685e1131edf0609`.
- [x] Verify focused GCC/MSVC production parity. Both 933-line diagnostics
      show the natural diamond's approved sites ordered as anchors `12,10`
      and `22,20`; after normalizing only established control-flow descriptor
      widths, they match with SHA-256
      `bb898a0153c58d4f90347d4e2c7077a1e825a21930dc802e3569a1e51ecc8a11`.
      Exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default and Windows regression matrices each report
      `DONE (170 tests)` with status zero.

Approved join sites now have a validated mutation-safe processing order.
Concrete Z80 TAC insertion, post-insertion CFG and liveness validation, and
retained-live-in enablement remain deferred; `-ra` remains opt-in.

---

## 166. Add Ownership-Safe Indexed TAC Insertion

- [x] Add `insert_tac()`, accepting insertion positions from zero through the
      current TAC count. It grows storage through the existing allocator,
      moves ownership-bearing TAC structs without duplicating allocations,
      and returns one canonically initialized empty slot at the requested
      index.
- [x] Reject negative and out-of-range indices without changing TAC count,
      capacity, ordering, or owned pointers. Emit exact `tac_storage`
      diagnostics for invalid input, moved entry count, old/new count, and
      storage capacity.
- [x] Add a dedicated strict harness covering front, middle, and append
      insertion; canonical initialization of every TAC field; preservation of
      three independently owned labels and metadata; two invalid indices; and
      forced growth from 1024 to 2048 entries while moving 1023 TACs.
- [x] Verify strict GCC/MSVC execution. After CRLF removal, both seven-line
      traces match exactly with SHA-256
      `ed5020a1b779771771f1c9d2f4508b2ad7604e0663d108952d7a5ec5e4714c9e`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify focused GCC/MSVC production parity. Existing 933-line diagnostics
      remain unchanged after established ABI normalization (SHA-256
      `bb898a0153c58d4f90347d4e2c7077a1e825a21930dc802e3569a1e51ecc8a11`),
      and exact 373-line assembly remains unchanged (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).
- [x] Verify the complete default and Windows regression matrices each report
      `DONE (170 tests)` with status zero.

The TAC store can now insert a uniquely owned, empty instruction at each
descending join anchor without invalidating pending anchors or duplicating
heap ownership. Join-specific marker population, Z80 spill emission,
post-insertion validation, and retained-live-in enablement remain deferred;
`-ra` remains opt-in.

---

## 167. Populate Join Spill Marker TACs

- [x] Add `TAC_OP_REGISTER_SPILL` and `tac_set_register_spill()` so one
      canonically empty TAC slot can represent a retained temp, source
      physical register, stack-frame destination offset, and byte width.
- [x] Validate the complete canonical TAC payload before marker population,
      while preserving source statement, file, and line metadata. Malformed
      inputs fail without partially changing the slot.
- [x] Add a pass-5 join emission callback that validates the Z80 register/
      width pair and anchor ownership, inserts before or after the approved
      anchor, populates the marker, copies anchor source metadata, and emits
      exact `join_spill_insert` diagnostics.
- [x] Extend TAC printing and the strict insertion harness with seven rejected
      descriptors, valid `HL`/two-byte and `A`/one-byte markers, and exact
      printable marker forms. GCC and MSVC produce identical normalized
      20-line traces with SHA-256
      `61fed4e673f24f519e3f48f368f6d4c9d64c82d12f76606cf1919f3f2d38826f`.
- [x] Verify the complete MSVC `Release|x86` rebuild reports zero warnings and
      errors. Focused production still dispatches two canonical empty plans,
      inserts no markers, emits 933 diagnostic lines, and preserves exact
      373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The dedicated marker and future mutation callback now exist, but production
retained joins remain disabled. Pass-6 Z80 marker emission, global ordering of
all `(temp, site)` insertions, post-insertion CFG/liveness reconstruction, and
retained-live-in enablement remain deferred; `-ra` remains opt-in.

---

## 168. Emit Register Spill Markers In The Z80 Backend

- [x] Add a target-local `z80_emit_register_spill()` backend contract and
      route `TAC_OP_REGISTER_SPILL` through it from pass 6. The emitter writes
      `A`, `B`, or `C` as one byte and writes `HL` as low `L`, high `H` or
      `BC` as low `C`, high `B` into the stack-frame slot addressed from `DE`.
- [x] Validate the complete marker identity, function ownership, integer TAC
      fields, register/width pairing, and signed 16-bit frame offset before
      writing any assembly. Null, fractional, overflowing, structurally
      malformed, and unsupported descriptors fail without partial output.
- [x] Add deterministic `register_allocator_z80: register_spill_emit`
      diagnostics with function, temp, physical register, destination offset,
      width, store count, and completion status.
- [x] Add a strict direct harness covering all five physical registers,
      signed offset boundaries, byte ordering, 21 invalid contracts, and exact
      zero-byte output after every rejection. GCC and MSVC produce identical
      normalized 46-line traces with SHA-256
      `60120a42a260eb31166cf8bd0905721bdcff28064f55a4bac1af39ed4503184d`.
- [x] Integrate the target emitter into CMake, GCC, Amiga, and MSVC builds. The
      complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify focused production still applies only canonical empty join plans,
      emits no register-spill markers, preserves 933 diagnostic lines, and
      preserves exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The Z80 backend can now emit every physical-register form accepted by the join
spill planner, but retained joins remain disabled. Global ordering of all
`(temp, site)` insertions, post-insertion CFG/liveness reconstruction, and
retained-live-in enablement remain deferred; `-ra` remains opt-in.

---

## 169. Order Join Spill Emissions Across The Whole Function

- [x] Add `register_allocator_join_spill_work_item` and
      `register_allocator_order_join_spill_emissions()` so every retained
      `(join, temp, predecessor site)` emission can be validated before any
      TAC mutation and stably ordered across the complete function.
- [x] Order by descending anchor. At equal anchors, process `after` before
      `before` so every callback still addresses the original anchor; preserve
      input order among otherwise equal work items.
- [x] Move pass-5 join emission storage to function scope, conservatively
      bound it by temp count times edge count, append all non-empty plans, and
      perform one validated ordering and callback-application pass after every
      join has been planned. Canonical empty plans retain their existing skip
      diagnostics.
- [x] Add a strict direct matrix covering mixed joins, temps, predecessor edge
      kinds, anchors `[5,2,5,8,5]`, equal-anchor placement precedence, stable
      ties, already-sorted and empty batches, five malformed API contracts,
      nine malformed work-item contracts, and complete input preservation on
      rejection.
- [x] Verify strict GCC/MSVC parity. After replacing only the two established
      ABI-sensitive complete liveness records, both 336-line traces match with
      SHA-256
      `17bab2f2c53287f364b1037979fdb19a6dbb3e94e68f0f7dde93f64707153964`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify focused production emits one function-wide zero-item ordering
      summary per function, preserves both canonical empty-plan skips, inserts
      no spill markers, emits 935 diagnostic lines, and preserves exact
      373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

Join-spill anchors are now mutation-safe across all joins and retained temps
in one function. Production retained joins remain disabled until TAC mutation
is followed by CFG/liveness reconstruction and invariant validation;
`-ra` remains opt-in.

---

## 170. Rebuild Control Flow And Liveness After Join Mutation

- [x] Add `register_allocator_plan_post_mutation_rebuild()` so the core
      atomically validates original/current TAC counts against the number of
      applied join markers and reports an explicit `rebuild=yes/no` decision.
      Invalid input resets the output and fails closed.
- [x] Make join reconciliation report its applied marker count. A nonzero
      count now ends use of the stale pre-mutation liveness sets and hands
      control back to the scan owner for reconstruction.
- [x] Replan storage from the mutated global TAC count, rebuild function basic
      blocks and CFG edges into new allocations, discard the stale storage,
      and recompute liveness before linear scan or reload placement consumes
      block indexes. Emit exact `post_mutation_control_flow` and
      `post_mutation_validation` diagnostics after successful reconstruction.
- [x] Add a strict accounting matrix with three valid decisions, including the
      maximum non-overflowing count, and eight invalid contracts covering null
      input/output, zero and negative counts, shrinkage, mismatched deltas, and
      overflow with output-reset checks.
- [x] Add a concrete synthetic CFG reconstruction test. Inserting one active
      instruction shifts a conditional jump and target label while preserving
      the expected two-block, two-edge topology and branch kinds.
- [x] Verify strict GCC/MSVC parity. After only established ABI-sensitive
      normalization, the 347-line liveness/accounting traces match with
      SHA-256
      `8673a4ed916f190825c51f6b3888480fdcfa4e547476c2a96c14bb17ab900aad`,
      and the 16-line CFG traces match with SHA-256
      `95f9c516c6bc0830f04b57e9c3b3e1d709a32adf5f3a476539978255caf730ee`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify focused production records three explicit zero-insertion
      `rebuild=no` decisions, emits no reconstruction diagnostics, and
      preserves exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The allocator can now discard stale control-flow/liveness state and rebuild it
after join marker insertion. Production retained joins remain disabled because
the producer assignments and register transparency of every incoming path must
be proven before the stack-only boundary can be relaxed. `-ra` remains opt-in.

---

## 171. Prove Cross-Block Join Retention Eligibility

- [x] Add a target-independent join-retention path descriptor and
      `register_allocator_plan_join_retention()` transaction. Require at least
      two unique predecessor paths and validate every definition, anchor,
      physical register, and path fact before selecting a common register.
- [x] Represent an immediate predecessor with no local definition as a valid
      ineligibility fact rather than malformed input, so later reaching-
      definition analysis can extend discovery without changing the planner
      contract.
- [x] Make rejection precedence independent of predecessor order:
      `consumer_unsupported`, `missing_definition`, `definition_unsupported`,
      `path_clobber`, then `register_mismatch`. Reset the selected register and
      preserve every input path on all failures.
- [x] Add a read-only Z80 preflight before join mutation planning. Select the
      current width-specific primary register, locate the first join consumer,
      find immediate predecessor-local definitions, and query existing target
      candidate legality and register-transparency rules.
- [x] Keep production preflight `mode=observe_only`: do not change spill flags,
      physical-register assignments, TACs, or stack-only join reconciliation.
- [x] Prove the natural diamond remains ineligible. One predecessor has no
      local definition and the other definition is unsupported for `A`, so the
      deterministic final reason is `missing_definition`.
- [x] Add strict direct coverage for valid common `A` and `HL` plans, all five
      rejection reasons, nine malformed top-level contracts, ten malformed
      path contracts, duplicate predecessors, selected-output reset, input
      preservation, and adversarial predecessor ordering.
- [x] Verify GCC/MSVC parity after only the two established ABI-sensitive
      liveness-record normalizations. All 387 records match with SHA-256
      `9b23273ee5a5f291b481c82fceb12052852c92262d224c4cfd4a1d7dc2ea628a`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit 942 diagnostic
      lines, insert no register-spill markers, and preserve exact 373-line
      assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The allocator now has a fail-closed all-path eligibility boundary before join
retention. Immediate predecessor-local discovery is intentionally conservative:
reaching definitions through dominating blocks, producer assignment integration,
and cross-block scheduling must be added before enabling the first retained
branch join. `-ra` remains opt-in.

---

## 172. Resolve Join Reaching Definitions Through The CFG

- [x] Add target-independent reaching-definition block facts and a bounded
      `register_allocator_resolve_reaching_definition()` fixed-point query over
      validated CFG edges.
- [x] Seed each block with its last local temp definition, propagate definitions
      through definition-free blocks, merge identical incoming definitions, and
      reject conflicting incoming definitions as `ambiguous`.
- [x] Carry physical-register transparency across every contributing block so
      a unique reaching definition can still fail closed when any path clobbers
      the candidate register.
- [x] Validate block facts, definition indexes, predecessor/temp indexes, edge
      endpoints and kinds, allocation bounds, and output storage before analysis.
      Reset output and preserve all caller facts/edges across every rejection.
- [x] Extend the observe-only Z80 join preflight to query reaching definitions
      only when immediate predecessor-local lookup misses. Collect last-write
      and post-write transparency facts through existing target policy checks.
- [x] Resolve the natural diamond's previously missing predecessor definition
      through its unique dominator to TAC 5. Preserve `transparent=no` because
      the path crosses a target-clobbering conditional branch.
- [x] Keep production retention disabled: both natural-diamond producers remain
      unsupported for `A`, so the final reason advances from
      `missing_definition` to `definition_unsupported` without TAC mutation.
- [x] Add direct graphs for a common dominator through two transparent arms,
      a clobbered arm, conflicting arm definitions, a missing definition, and a
      definition-free cycle, plus seventeen malformed contracts with output
      reset and complete input preservation checks.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      liveness-record normalizations. All 409 records match with SHA-256
      `2ffbcadb90c52fb81b18be62319bc6ba4758cff44207781dbaa7b79d2316d558`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit 952 diagnostic
      lines, including exact reaching-definition and target-transparency facts,
      and preserve exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

Join preflight can now distinguish a genuinely missing value from one that
reaches through dominating blocks, and it retains path-clobber evidence across
that traversal. Producer assignment and allocation-scheduling integration remain
required before the first retained branch join can be enabled. `-ra` remains
opt-in.

---

## 173. Plan Join Producer And Consumer Assignments Atomically

- [x] Add target-independent join-assignment and join-schedule descriptors plus
      `register_allocator_plan_join_schedule()`.
- [x] Convert an eligible all-path retention decision into one assignment for
      each distinct reaching producer and one assignment for the join consumer,
      all using the selected physical register.
- [x] Deduplicate a shared dominating producer while preserving stable
      predecessor order for distinct definitions.
- [x] Accept unassigned operands and compatible existing assignments, but
      reject the complete transaction when any producer or consumer already
      names a different physical register. Report the first producer conflict
      before a consumer conflict without partially populating output.
- [x] Produce a canonical empty `ineligible` schedule when retention selected
      no register. Validate producer-before-consumer ordering, path eligibility,
      capacities, indexes, pointers, and count overflow before writing output.
- [x] Integrate the transaction into Z80 join preflight as `mode=observe_only`.
      Read existing producer-result and consumer-operand assignments without
      changing TACs, temp state, spill policy, or active slots.
- [x] Add direct coverage for distinct producers, shared-producer
      deduplication, compatible preassignment, producer and consumer conflicts,
      ineligible empty plans, insufficient capacity, malformed top-level/path
      contracts, output reset, and complete assignment preservation.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      liveness-record normalizations. All 433 records match with SHA-256
      `a2aee58c680c6692df1cffbfcf263a9044a09a55fb30f73f521ad5fac2f7b09a`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit one canonical
      ineligible zero-assignment schedule, 954 diagnostic lines, and exact
      373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The allocator can now describe the complete TAC assignment set needed by a
proven retained join without mutating compiler state. Production remains
observe-only because the selected physical register must next be reserved
against active intervals across all contributing blocks before applying the
schedule or relaxing block-exit spills. `-ra` remains opt-in.

---

## 174. Plan Cross-Block Active-Slot Reservations

- [x] Add target-independent occupied-interval and join-reservation
      descriptors plus `register_allocator_plan_join_reservation()`.
- [x] Validate a ready join schedule against its independent assignment
      capacity before reading assignments, map the selected physical register
      to a policy-owned active slot, and derive one conservative lexical span
      from the earliest producer through the join consumer.
- [x] Reject malformed assignment roles, registers, instruction indexes,
      producer/consumer ordering, selected slots, occupied intervals, and
      target-supplied physical-unit overlap facts before publishing a plan.
- [x] Ignore the retained temp's own interval and target-declared
      nonoverlapping registers. Treat both ends of every overlapping occupied
      interval as inclusive, report the first conflicting temp deterministically,
      and publish no partial reservation on conflict.
- [x] Require canonical ineligible schedules to carry zero assignments, zero
      capacity, no selected register, and no conflict anchor; produce a matching
      canonical empty reservation.
- [x] Integrate reservation fact collection into Z80 join preflight as
      `mode=observe_only`. Resolve the policy slot and physical-unit overlap
      through existing target hooks, and inspect only current retained live
      intervals without changing active slots, TACs, spills, or assignments.
- [x] Add direct coverage for a ready reservation, same-temp and nonoverlap
      exemptions, inclusive start/end conflicts, stable first-conflict
      precedence, an ineligible schedule, insufficient assignment capacity,
      a noncanonical ineligible schedule, malformed assignments and intervals,
      output reset, and complete input preservation.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      liveness-record normalizations. All 455 records match with SHA-256
      `9b5c75ef08ea07e5cd0e3166200b58b7c80c6702276ddabcf22e2606064c7482`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit one canonical
      ineligible reservation and preflight record, 956 diagnostic lines, and
      exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The allocator can now reject cross-block schedules that collide with current
physical-register occupancy and can describe a mutation-free slot reservation.
The lexical span is deliberately conservative; path-aware reservation
application and coordination with block-exit spill policy remain required before
the first retained branch join can be enabled. `-ra` remains opt-in.

---

## 175. Plan Path-Sensitive Join Reservations Through The CFG

- [x] Add target-independent block-reservation descriptors and
      `register_allocator_plan_join_path_reservations()`.
- [x] Validate normalized basic-block partitions, CFG edges, the ready join
      reservation, and every producer/consumer assignment before graph
      traversal or output publication.
- [x] Find every block lying on a producer-to-consumer route by intersecting
      forward reachability from each producer with reverse reachability from
      the consumer. Reject atomically when any producer cannot reach the
      consumer.
- [x] Form the stable block-index-ordered union of all valid routes, deduplicate
      shared dominator and join blocks, and exclude unrelated branches.
- [x] Derive block-local reservation bounds from the earliest producer in a
      block through the consumer in its block, using full block bounds for
      intervening route blocks. Preserve the policy-selected active slot.
- [x] Validate producer operand zero, positive consumer operand, common
      physical register, producer-before-consumer ordering, block ownership,
      edge kinds, output capacity, and arithmetic bounds before publishing the
      plan. Preserve input and output buffers on every failure.
- [x] Integrate the CFG transaction into Z80 join preflight as
      `mode=observe_only`. A non-ready reservation produces one canonical
      zero-block ineligible plan without changing active slots, assignments,
      spill flags, or TACs.
- [x] Add direct coverage for a two-arm diamond, a shared-dominator route,
      multiple producers, unrelated-block exclusion, stable block ordering,
      ineligible plans, unreachable producers, overlapping block partitions,
      invalid edges and assignments, insufficient capacity, and complete
      input/output preservation.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      liveness-record normalizations. All 483 records match with SHA-256
      `e138e709d1a39d43fda8f6669b1325aac5a80337f1373bdf0c2c1ea288626ab3`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit one canonical
      ineligible path-reservation and preflight record, 958 diagnostic lines,
      and exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The allocator can now replace the conservative lexical reservation with an
explicit union of block-local reservations along contributing CFG paths. The
plan remains mutation-free; callback-driven reservation application and a
narrow block-exit spill exemption must be coordinated before applying the join
assignment schedule. `-ra` remains opt-in.

---

## 176. Apply Join Path Reservations Through Validated Callbacks

- [x] Add a target-independent join-path application result and
      `register_allocator_apply_join_path_reservations()`.
- [x] Validate the complete ready plan before the first callback: canonical
      status, count/capacity, selected slot, lexical bounds, strictly increasing
      block order, block indexes, block-local ranges, and callback presence.
- [x] Dispatch block reservations in deterministic plan order with temp, slot,
      block, and local instruction bounds. Report every completed callback.
- [x] Stop on the first callback failure and report an explicit `partial`
      application with the successfully applied prefix and failed block.
- [x] Accept a canonical ineligible plan without a callback and report a
      zero-application skip. Reset application output on every invalid input or
      invalid plan and preserve reservation input buffers.
- [x] Integrate an isolated Z80 observation callback into join preflight. It
      verifies function ownership and prints contextual block facts without
      mutating allocator slots, TAC assignments, spill state, or TAC storage.
- [x] Add direct coverage for complete three-block application, exact callback
      order and payload, second-callback failure, canonical skip, eight invalid
      top-level contracts, three invalid plans, two invalid block reservations,
      output reset, input preservation, and zero callbacks before validation.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      liveness-record normalizations. All 515 records match with SHA-256
      `ed074d38ef685fd7520d80f56d5a474837adde2919ceaeaa2df9492d5fd558be`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit one canonical
      ineligible application and observe-only preflight record, 960 diagnostic
      lines, and exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The generic core can now execute a validated path-reservation plan and report
the exact applied prefix on target failure. Production uses only isolated
observation state. A real allocator-state adapter must provide rollback or
precommitted atomic storage before block-exit spill policy and TAC assignments
can be changed for the first retained join. `-ra` remains opt-in.

---

## 177. Commit Join Path Reservation State Atomically

- [x] Add a target-independent block-by-slot reservation-state entry, commit
      result, and `register_allocator_commit_join_path_reservations()`.
- [x] Validate the complete incoming path, all existing state entries, output
      capacity, block order, slot identity, and inclusive instruction bounds
      before writing any state.
- [x] Detect conflicts only for overlapping ranges in the same block and slot.
      Report the exact conflicting entry as a successful fail-closed decision,
      leaving the table and count byte-for-byte unchanged.
- [x] Append every path reservation only after validation succeeds and publish
      the new count once, preventing partial state on malformed plans or
      insufficient capacity.
- [x] Preserve canonical ineligible plans as successful zero-write commits and
      reset commit output on every malformed contract.
- [x] Integrate commit into Z80 join preflight with bounded scratch storage and
      `mode=observe_only`; no reservation state survives preflight and no
      active slot, assignment, spill flag, or TAC is changed.
- [x] Add direct coverage for empty-table commit, append, overlapping conflict,
      same-slot nonoverlap, canonical skip, ten invalid top-level contracts,
      malformed plan count, insufficient storage, malformed existing state,
      malformed incoming order, and byte-for-byte no-write guarantees.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      complete-record normalizations. All 555 records match with SHA-256
      `f8acbbfd42d9fc1e5b8274d9af5b6b843c68c45cf225f419d4656ebe21e1f697`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit one canonical
      ineligible atomic commit and observe-only preflight record, 962 diagnostic
      lines, and exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

The generic core can now atomically precommit path-sensitive block reservation
state without callback rollback. Production deliberately uses scratch state;
the next increment must give this table persistent scan ownership and make
candidate, active-slot, and spill policy consult it before enabling retained
join assignments. `-ra` remains opt-in.

---

## 178. Give Join Reservation State Function-Scan Ownership

- [x] Add target-independent bounded storage planning for join-path state with
      checked block-by-temp capacity and exact byte sizing.
- [x] Clear storage output before validation and reject null metadata, zero or
      negative dimensions, integer multiplication overflow, and byte-size
      overflow without exposing stale capacity.
- [x] Allocate one reservation-state table for the complete function CFG and
      retain its count across join reconciliation and linear scan.
- [x] Pass the shared table, capacity, and count through every join preflight
      instead of allocating and discarding per-join scratch state.
- [x] Centralize cleanup so every post-acquisition success or failure releases
      the same function-owned table exactly once.
- [x] Emit deterministic storage, acquire, scan, and release diagnostics with
      capacity, bytes, count, ownership, and mode. Mark scan consumption as
      explicitly disabled until slot and spill policy consult the table.
- [x] Add direct coverage for exact capacity/byte planning, stale-output reset,
      null function/output, invalid dimensions, and portable integer overflow.
- [x] Pin focused production lifecycle records for the diamond, loop, and
      zero-temp main function, including zero committed state for the currently
      ineligible natural join.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      complete-record normalizations. All 562 records match with SHA-256
      `faefa2600f1240d8ca9369789bd516d3470f9ca84f8f5391a26529bf66999e52`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit 973 diagnostic
      lines and exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

Join reservation state now has a bounded owner spanning reconciliation and the
function scan. Production still records no state for the ineligible natural
join, and scan consumption remains disabled. The next increment must make
candidate and active-slot arbitration consult this state before changing
block-exit spill behavior or applying retained assignments. `-ra` remains
opt-in.

---

## 179. Consult Join Reservations During Candidate Arbitration

- [x] Add a target-independent range query over committed join-path state with
      canonical `available`, `owned`, and `conflict` outcomes.
- [x] Validate function, CFG dimensions, slot cardinality, block/slot/temp
      identity, instruction range, state count, and every existing state entry
      before publishing a query result.
- [x] Match only inclusive range overlap in the same block and active slot;
      permit the reservation owner and reject a competing temp without
      changing the state table.
- [x] Clear query output before validation and report the exact matching entry
      and owner for deterministic diagnostics.
- [x] Thread the function-owned reservation table through the shared candidate
      evaluation path so primary and alternate candidates consult it before
      active-register overlap arbitration.
- [x] Fail closed when reservation validation fails and emit
      `join_path_state_candidate` diagnostics for every production query.
- [x] Add direct coverage for owned, conflict, before-range, after-range,
      unrelated block/slot, empty state, eleven invalid contracts, malformed
      existing state, and stale-output clearing.
- [x] Pin the currently source-reachable loop candidate as `available`; the
      natural diamond remains ineligible and commits no reservation state.

Candidate arbitration now honors committed block-by-slot ownership without
synthetic active intervals. No source-reachable allocation changes yet because
the natural join still fails eligibility. `-ra` remains opt-in.

---

## 180. Plan Narrow Block-Exit Spill Exemptions

- [x] Add a target-independent exact-exit spill planner with canonical
      `required` and `exempt` decisions.
- [x] Exempt only when one valid state entry owns the same temp and covers the
      exact block exit; reject ambiguous duplicate ownership fail closed.
- [x] Validate all dimensions, identities, slot bounds, state entries, and
      output before deciding, while preserving the reservation table.
- [x] Move block-exit policy into the function-owned CFG transaction after
      join reconciliation so it consumes the same committed state as scan
      candidate arbitration.
- [x] Preserve established spill metadata precedence by allowing a proven
      required block exit to replace the conservative label fallback while
      leaving stronger hard-boundary reasons untouched.
- [x] Emit exact core plans, production decisions, and mutation diagnostics,
      including the prior reason replaced at the original block boundary.
- [x] Add direct coverage for exact exemption, before/after ranges, another
      temp, unrelated block, empty state, ambiguous ownership, ten invalid
      contracts, malformed entries, and stale-output clearing.
- [x] Pin three required natural-diamond exits, zero production exemptions,
      and the TAC-8 `replaced=label` precedence repair.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      complete-record normalizations. All 595 records match with SHA-256
      `47ff0f377a2affd2482bb1f55c8eb2479fbef7d683274455b15ef12c42c193a8`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit 976 diagnostic
      lines and exact 373-line assembly (SHA-256
      `6565e0c104498ffc470346c91071b96fd826fc9734274c43c470d0096d744186`).

Committed reservations now constrain candidate selection and can narrowly
exempt their owner from a block-exit spill. Production remains behaviorally
unchanged because no natural join is eligible. The next increment must apply
the already-planned producer/consumer assignments atomically and enable one
fully proven branch join before any broader relaxation. `-ra` remains opt-in.

---

## 181. Apply Join Assignments As One Validated Transaction

- [x] Add a target-independent join-assignment application result, one-shot
      transaction callback, and `register_allocator_apply_join_assignments()`.
- [x] Validate the complete ready schedule before dispatch: canonical status,
      count/capacity, instruction bounds, producer/consumer ordering, operand
      roles, common physical register, and no-register sentinel.
- [x] Dispatch exactly one callback for a valid schedule so target mutation is
      all-or-nothing rather than exposing a fallible per-assignment prefix.
- [x] Treat a canonical ineligible schedule as a successful zero-write skip
      without requiring or invoking a callback; clear application output on
      every malformed contract.
- [x] Add a Z80 TAC transaction adapter that validates function ownership,
      temp identity, operand identity, and every empty destination field before
      writing any producer or consumer assignment in deterministic order.
- [x] Apply assignments only after path-reservation commit succeeds; conflicts
      and ineligible joins retain canonical zero-application behavior.
- [x] Add direct coverage for three-site success, callback failure with zero
      destination writes, canonical skip, six invalid top-level contracts, one
      invalid schedule, five malformed assignments, stale-output clearing, and
      zero callbacks before validation.
- [x] Emit exact core transaction, target entry, and production preflight
      diagnostics with assignment count, applied count, and outcome.

Join schedules can now update all producers and the final consumer through one
validated target transaction. The generic core never exposes a partial prefix,
and the Z80 adapter performs a complete read-only validation pass before its
non-failing write pass. `-ra` remains opt-in.

---

## 182. Enable The First Fully Proven Retained Branch Join

- [x] Select `C` for 8-bit join retention and narrowly approve constant
      assignment producers feeding conditional operand one when every path is
      proven transparent and all assignments share `C`.
- [x] Refine Z80 transparency for 8-bit conditional comparisons in `C`. The
      concrete comparison sequence uses `A` and `B`, copies `C` to `A` when
      needed, and does not clobber `C`; retain existing `B`/`BC` clobber rules.
- [x] Preserve all-path proof requirements: unique reaching definitions,
      supported producer and consumer forms, path transparency, common
      register, conflict-free schedule, interval reservation, and atomic state
      commit must all succeed before TAC assignment.
- [x] Commit four block reservations for the natural diamond, atomically apply
      both producer assignments and the join consumer assignment, and exempt
      exactly the three covered predecessor exits from forced spilling.
- [x] Make scan consume the committed state while retaining normal arbitration
      for unrelated temps and blocks. Keep loop and unsupported joins as
      fail-closed controls.
- [x] Update the byte regression to require concrete `LD C,0`, `LD C,1`, and
      retained `LD A,C` behavior with no stack reload for the joined boolean.
- [x] Pin the complete eligibility, reaching-definition, transparency,
      schedule, reservation, commit, assignment, candidate, exit-exemption,
      ownership, and backend-emission diagnostic chain.
- [x] Verify strict GCC/MSVC direct-core parity after only the two established
      ABI-sensitive complete-record normalizations. All 610 records match with
      SHA-256
      `ac1a8b21f147202d7908578936cc408ef700ed2550d60f88e9f8e37a9385a591`.
      The corrected source builds under strict C90 and the complete MSVC
      `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production both emit 1004
      diagnostic lines, exact optimized assembly (SHA-256
      `b0d0ebfd804f951eec892403f12a69a96794715d5ba091cc7e4d3ec8da620190`),
      and exact linked ROM (SHA-256
      `bc80580a235ddc124bf7e127aa97b6e7ba2bef507eb325975da8fd56fb2e8b4a`).

The allocator now retains one real 8-bit value in `C` across both arms of a
branch diamond and consumes it directly at the join. This is the first
source-reachable cross-block physical-register join; broader producer forms,
loop back-edges, and longer-range spill/reload insertion remain fail closed.
`-ra` remains opt-in.

---

## 183. Classify Loop Join Topology

- [x] Add target-independent loop-join topology metadata and
      `register_allocator_classify_loop_join()`.
- [x] Classify a join as `ready` only when it has exactly one lexical entry
      predecessor and one lexical back-edge latch predecessor. Preserve
      canonical `not_loop` and `ambiguous` outcomes for other shapes.
- [x] Validate block and edge counts, join indexes, edge endpoints, edge kinds,
      null storage, and duplicate entry/latch shapes before publishing output.
- [x] Integrate one topology observation per approved multi-predecessor
      production join before live-in temp processing. Keep this integration
      mutation-free and emit stable core and `mode=observe_only` preflight
      diagnostics.
- [x] Add direct coverage for a ready entry-plus-latch loop, forward-only and
      self-edge non-loops, multiple entries, multiple latches, malformed edges,
      six invalid contracts, and stale-output clearing.
- [x] Verify the existing branch diamonds classify as ambiguity rather than
      loops, with two exact core and two exact preflight records.

Loop headers now have an explicit reusable topology proof instead of being
forced through branch-only predecessor assumptions. The current source fixture
has no entry-plus-back-edge live-in join, so production remains observational.
`-ra` remains opt-in.

---

## 184. Plan Wrapped Loop Join Assignments

- [x] Add `register_allocator_plan_loop_join_schedule()` for one entry
      definition before the header consumer and one latch definition after it.
- [x] Emit a canonical transaction-compatible assignment order of entry
      producer, latch producer, then consumer while preserving wrapped lexical
      ordering and one common physical register.
- [x] Detect conflicts independently at entry, latch, and consumer sites and
      preserve exact conflicting instruction identity without partial output.
- [x] Reject malformed topology, producer/consumer ordering, operands,
      registers, instruction bounds, capacities, and null contracts. Preserve
      canonical ineligible scheduling for non-ready topology.
- [x] Add direct coverage for a ready wrapped schedule, all three conflict
      sites, insufficient capacity, canonical skip, four invalid loop shapes,
      eight invalid contracts, exact assignment order, and output reset.
- [x] Verify strict GCC/MSVC parity after only the two established ABI-sensitive
      complete-record normalizations. All 643 records match with SHA-256
      `c7aa0a748cb425e22ebcac6db64f28310e45145ef29797f4bb9689a2d4298975`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native Windows focused production emit 1008 diagnostic
      lines and preserve exact optimized assembly (SHA-256
      `b0d0ebfd804f951eec892403f12a69a96794715d5ba091cc7e4d3ec8da620190`)
      and linked ROM (SHA-256
      `bc80580a235ddc124bf7e127aa97b6e7ba2bef507eb325975da8fd56fb2e8b4a`).

The reusable core can now represent the wrapped producer ordering required by
a loop-carried join without weakening branch schedule validation. A dedicated
source-reachable entry-plus-latch live-in fixture, cyclic path reservations,
and backend activation remain before loop retention can mutate production TACs.
`-ra` remains opt-in.

---

## 185. Plan Cyclic Loop Path Reservations

- [x] Add `register_allocator_plan_loop_join_path_reservations()` for a ready
      entry/header/latch topology and a ready wrapped assignment schedule.
- [x] Reserve the entry predecessor from its producer, reserve the full loop
      header, and reserve every block on a header-to-latch route. Exclude loop
      exits and unrelated blocks.
- [x] Validate block partitions, CFG edge kinds and endpoints, exact topology
      edges, assignment roles/sites/registers, reachability, capacity, and all
      null and range contracts before publishing output.
- [x] Preserve canonical zero-reservation success for ineligible schedules and
      preserve caller output on capacity failure.
- [x] Add direct coverage for the exact four-block reservation plan, exit
      exclusion, capacity failure, unreachable latch, four malformed
      assignments, malformed edges and blocks, ambiguous topology, malformed
      schedule, canonical skip, and six invalid top-level contracts.
- [x] Pin all cyclic planner diagnostics in the permanent focused fixture.
      The strict GCC harness emits 666 records.

The reusable core can now represent cyclic reservation ownership without
pretending a lexical interval spans a back-edge correctly. Production does not
commit these reservations until a source loop supplies a genuine header
live-in temp with entry and latch definitions. `-ra` remains opt-in.

---

## 186. Establish The Source Loop Activation Boundary

- [x] Add a source-reachable four-block `while` fixture with one entry edge,
      one latch back-edge, and exact `topology=ready` production diagnostics.
- [x] Pin the current lowering fact that the condition temp is defined inside
      the header and therefore has `live_in={}`. Require no join-retention plan
      or committed cyclic reservation for that temp.
- [x] Keep the fixture's function-owned reservation state empty and verify its
      local candidate remains available under ordinary block-local scanning.
- [x] Preserve both existing ROM byte tests and the retained branch-diamond
      behavior while adding the loop topology fixture.
- [x] Verify strict GCC/MSVC direct-core parity after only the two established
      ABI-sensitive complete-record normalizations. All 666 records match with
      SHA-256
      `f3e527024c0ff5fc008dc30e949712847d30e94c2aae91bae806dbac556af39b`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify rebuilt Cygwin and native Windows focused production both emit
      1343 diagnostic lines and exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
      Full traces differ only in the unnormalized platform-sized
      `control_flow_storage` byte counts.
- [x] Run the complete default and native Windows regression matrices; both
      pass all 170 tests.

Current `while` lowering emits `label; compute condition temp; test; body;
jump label`, so topology alone cannot produce a loop-carried temporary. Sound
activation requires a lowering or SSA-like transformation that defines one
logical temp in the entry and latch and consumes it live-in at the header. Loop
retention remains fail closed until that proof exists. `-ra` remains opt-in.

---

## 187. Classify Loop Live-In Candidates

- [x] Add `register_allocator_plan_loop_join_candidate()` and reusable
      candidate metadata for one loop header and its solved live-in set.
- [x] Publish `ready` only for exactly one live-in temp under ready loop
      topology. Preserve explicit `ineligible` outcomes for no live-ins and
      non-loop topology, and `ambiguous` outcomes for multiple live-ins or
      ambiguous topology.
- [x] Validate liveness storage dimensions, boolean set values, block/temp
      counts, join indexes, topology statuses, null contracts, and stale-output
      reset before publishing a candidate.
- [x] Add direct coverage for unique, empty, multiple, ambiguous-topology, and
      non-loop results plus malformed liveness, storage, topology, indexes,
      counts, and null contracts.
- [x] Integrate one production candidate census after each loop topology
      classification. Keep it `mode=observe_only` and emit exact core and
      preflight diagnostics without invoking definition, schedule, reservation,
      or mutation logic.
- [x] Pin the current source loop as `candidates=0`, `temp=r-1`,
      `candidate=ineligible`, `reason=no_live_in`. Pin branch joins as
      ambiguous controls.

Production now distinguishes ready loop topology from a usable loop-carried
value instead of treating topology as eligibility. The current source loop
fails closed at the live-in candidate boundary. `-ra` remains opt-in.

---

## 188. Validate Resolved Loop Definitions

- [x] Add `register_allocator_plan_loop_join_definition()` and reusable
      definition metadata for an already-resolved entry definition, latch
      definition, and header consumer.
- [x] Require ready topology, a live-in temp, entry-definition ownership by the
      lexical entry predecessor, latch-definition ownership by the back-edge
      predecessor, consumer ownership by the header, wrapped ordering, and a
      valid consumer operand before publishing `ready`.
- [x] Preserve explicit ineligible reasons for non-ready topology, non-live-in
      temps, missing entry definitions, missing latch definitions, and missing
      consumers. Reject malformed block partitions, topology identities,
      definition ownership, ordering, operands, and null/range contracts.
- [x] Add direct coverage for the ready proof, all five ineligible reasons,
      four invalid definition shapes, malformed topology and block partitions,
      seven invalid contracts, and stale-output reset.
- [x] Keep production definition planning disabled until the candidate census
      returns one real live-in temp and target-local reaching-definition facts
      exist for both entry and latch paths.
- [x] Verify strict GCC/MSVC direct-core parity after only the two established
      ABI-sensitive complete-record normalizations. All 699 records match with
      SHA-256
      `92d5aa559a4b1826fd61987d3f0d9316e1a0c5a61b9b77255ec9131aa1aa8f4f`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify rebuilt Cygwin and native Windows focused production both emit
      1349 diagnostic lines and preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default and native Windows regression matrices; both
      report `DONE (170 tests)`.

The reusable core can now validate all resolved facts needed before wrapped
loop scheduling and cyclic reservations, without manufacturing definitions or
weakening the proof boundary. An attempted duplicated-condition lowering was
rejected after exposing optimizer and synthetic-startup assumptions; activation
still requires a representation-safe loop-carried definition strategy.
`-ra` remains opt-in.

---

## 189. Discover Loop Entry, Latch, And Consumer Facts

- [x] Add `register_allocator_discover_loop_join_facts()` and reusable loop
      fact metadata with explicit ready, topology, missing-entry,
      missing-latch, and no-consumer outcomes.
- [x] Scan the exact entry predecessor and latch blocks backwards to select
      their latest active writes, and scan the header forwards to select its
      first active read and consumer operand.
- [x] Keep discovery target-independent through instruction activity, read,
      write, and consumer-operand callbacks.
- [x] Validate block partitions, topology identities, instruction/temp ranges,
      callback results, callback pointers, non-null callback context, output
      storage, and stale-output reset before publishing ready facts.
- [x] Add direct coverage for ready and inactive-decoy scans, all four
      ineligible reasons, four invalid callback results, two malformed
      topologies, one malformed block partition, and eleven invalid contracts.
- [x] Pin all 24 deterministic discovery outcomes in the permanent focused
      fixture. The strict direct harness now emits 723 records.

The core can now discover the concrete entry, latch, and consumer sites needed
by the Phase 188 validator without branch-oriented reaching-definition logic.
It does not infer or manufacture loop-carried definitions. `-ra` remains
opt-in.

---

## 190. Compose Loop Fact And Definition Preflight

- [x] Integrate fact discovery and resolved-definition validation after a
      unique ready loop candidate in production.
- [x] Emit stable observe-only fact and definition preflight diagnostics.
      Candidate-ineligible and candidate-ambiguous joins emit an explicit
      `facts=skipped reason=candidate_not_ready` record instead.
- [x] Keep wrapped scheduling, cyclic reservation commit, assignment mutation,
      and spill exemptions disabled until discovery and definition validation
      both publish ready results.
- [x] Pin the current two branch joins and source loop as three exact skipped
      preflights, with no core fact discovery or definition preflight invoked.
- [x] Verify strict GCC/MSVC direct-core parity after only the two established
      ABI-sensitive complete-record normalizations. All 723 records match with
      SHA-256
      `2aa1a2dd709cae535c9f4f76ec43ac5ecd8d0b76f38ac83245b11ef2f29f2f80`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify rebuilt Cygwin and native Windows focused production both emit
      1352 diagnostic lines and preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default and native Windows regression matrices; both
      report `DONE (170 tests)` with status zero.

Production now contains the complete observe-only chain from topology through
candidate census, fact discovery, and definition validation. The current
source lowering still produces no unique loop live-in, so the chain stops at
the candidate gate and performs no mutation. `-ra` remains opt-in.

---

## 191. Compose Atomic Loop Retention Plans

- [x] Add `register_allocator_plan_loop_retention()` and reusable composite
      plan metadata for one validated loop definition.
- [x] Compose wrapped entry/latch/consumer assignment scheduling with cyclic
      path reservations into one ready plan containing exactly three
      assignments and all selected loop-block reservations.
- [x] Stage assignments and reservations internally and publish caller output
      only after every nested planner succeeds, preventing partial output when
      scheduling, capacity, or reservation validation fails.
- [x] Preserve canonical ineligible outcomes for definition, schedule, and
      reservation rejection, while rejecting malformed definition statuses,
      capacities, storage, indexes, and null contracts.
- [x] Add direct coverage for ready composition, ineligible definition,
      assignment conflict, reservation-capacity failure, top-level capacity
      failure, malformed definition status, and untouched output on failure.
- [x] Pin the ready plan as three assignments and four stable cyclic block
      reservations in the permanent focused fixture.

The core now has one coherent plan boundary instead of requiring callers to
publish wrapped assignments before cyclic reservations are known to succeed.
Production remains unchanged because the current source loop has no unique
live-in candidate. `-ra` remains opt-in.

---

## 192. Apply Loop Retention As One Transaction

- [x] Add `register_allocator_apply_loop_retention()` and a single callback
      transaction containing the complete assignment and reservation payload.
- [x] Validate plan status, exact producer/consumer roles, instruction and
      operand shapes, common physical register, ordered reservation blocks,
      reservation ranges, capacities, callback/context contracts, and stale
      application output before invoking the callback.
- [x] Report callback rejection as an atomic failure with zero published
      assignments and reservations. Reject invalid callback return values and
      malformed payloads without invoking mutation.
- [x] Preserve canonical successful no-op application for ineligible plans.
- [x] Add direct coverage for applied, callback-rejected, invalid-callback,
      malformed-assignment, malformed-reservation, ineligible, malformed-plan,
      null-context, and null-function outcomes, including callback-count and
      atomicity assertions.
- [x] Pin all composite plan and application diagnostics in the permanent
      fixture. The strict direct harness emits 763 records.
- [x] Verify strict GCC/MSVC parity after only the two established
      ABI-sensitive complete-record normalizations. All 763 records match with
      SHA-256
      `82238f554847f540b0d19dd50067105b83a7c1e72cbcfddd0cac0da7f89c1a24`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify Cygwin and native focused production remain mutation-free at 1352
      diagnostics and preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default and native Windows regression matrices; both
      report `DONE (170 tests)` with status zero.

The reusable loop pipeline can now plan and apply complete retention state
atomically once production discovers a real ready candidate and definition.
This does not yet activate loop mutation or retire the representation-safe
loop-carried temp requirement. `-ra` remains opt-in.

---

## 193. Commit Loop Retention State Atomically

- [x] Add `register_allocator_commit_loop_retention_transaction()` as the
      target-independent commit boundary for cyclic reservations and the
      complete assignment transaction.
- [x] Stage reservation ownership in temporary state, invoke assignment
      application only after reservation validation succeeds, and publish the
      staged state only after assignment application succeeds.
- [x] Preserve the original state count and entries when reservation conflict,
      state capacity, assignment callback, or malformed reservation validation
      rejects the transaction.
- [x] Add direct coverage for successful publication, existing-state conflict,
      insufficient state capacity, assignment rejection, and malformed
      reservation ordering, including state snapshots and callback counts.
- [x] Emit exact `atomic=yes` transaction diagnostics and pin one applied plus
      four rejected outcomes in the permanent focused fixture.

Loop assignments and function-owned cyclic reservation state can now be
committed as one operation instead of exposing partial ownership when target
assignment application fails. `-ra` remains opt-in.

---

## 194. Wire Atomic Loop Retention Into Production

- [x] Add the Z80 loop transaction adapter over function-owned reservation
      state and the existing all-before-any join-assignment adapter.
- [x] Compose fact discovery, definition validation, loop planning, and atomic
      application when production reaches a ready candidate and definition.
- [x] Emit explicit plan and application preflight skips for every non-ready
      candidate. Pin the current two branch joins and source loop as three
      `plan=skipped` and three `application=skipped` decisions.
- [x] Require no production loop transaction for the current source fixture,
      whose header still has no loop-carried live-in temp.
- [x] Expand the direct harness to 791 records and verify strict GCC/MSVC parity
      after only the two established ABI-sensitive normalizations. Both traces
      match with SHA-256
      `2640f3eee0f38ba2ae23d1a57a47ba900238fcafeab951c6e7621ccf8b123bd3`.
      The complete MSVC `Release|x86` rebuild reports zero warnings and errors.
- [x] Verify rebuilt Cygwin and native Windows compiler focused production emit
      1358 diagnostics and preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default and native Windows regression matrices; both
      report `DONE (170 tests)` with status zero.

Production now owns the complete atomic loop-retention path, but current source
lowering still stops at the candidate gate. The remaining activation blocker is
a representation-safe loop-carried temp with entry and latch definitions; no
mutation is claimed until a source fixture reaches that path. `-ra` remains
opt-in.

---

## 195. Classify Loop Candidate Rejection

- [x] Add target-independent loop-candidate reason metadata for topology,
      empty live-in sets, and multiple live-in temps while preserving the
      existing ready, ineligible, and ambiguous status contract.
- [x] Give topology rejection deterministic precedence when both topology and
      candidate cardinality are ambiguous, and reserve `reason=none` for a
      unique ready candidate or validation failure.
- [x] Initialize all candidate output fields before validation so malformed
      storage, liveness, topology, and null-input failures remain fail-closed.
- [x] Add direct assertions for unique, empty, multiple, topology-ambiguous,
      non-loop, malformed-liveness, malformed-topology, and invalid-input
      outcomes, including exact reason diagnostics.

The candidate boundary now distinguishes an absent loop-carried temp from
structural ambiguity. This makes the remaining representation blocker
machine-readable without weakening the live-in requirement. `-ra` remains
opt-in.

---

## 196. Propagate Loop Candidate Reasons In Production

- [x] Translate generic candidate reasons into stable production diagnostics
      and propagate the selected reason through fact-discovery, plan, and
      application skips.
- [x] Pin both branch joins as `loop_topology_ambiguous` and the source loop as
      `no_live_in_temp`, proving that ready topology reaches the representation
      gate rather than being conflated with structural rejection.
- [x] Continue to reject all current production loop mutations: the fixture
      emits no fact-discovery transaction, definition, schedule, reservation,
      or retention transaction for the source loop.
- [x] Keep the strict direct harness at 791 records and verify GCC/MSVC parity
      after normalizing only the two established ABI-sensitive records. The
      normalized trace has SHA-256
      `aedd44d30a7ace7390edfe4f648f20ccf5574103e939b0c5968fdab975322e3b`.
- [x] Verify focused production emits 1358 diagnostics and preserves exact
      assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Rebuild the complete native MSVC `Release|x86` solution with toolset
      `v143` and Windows SDK `10.0`; the build reports zero warnings and
      errors.
- [x] Run the complete default and native Windows regression matrices with
      current binaries staged in the paths selected by `run_tests.sh`; both
      report `DONE (170 tests)` with status zero.

Production diagnostics now identify the precise loop activation blocker: the
source loop carries stack-backed values, while its only temporary is defined
and consumed inside the header and is therefore not live in. The next
implementation step must change representation or lowering safely; candidate
selection must not retain that header-local temp. `-ra` remains opt-in.

---

## 197. Classify Loop-Carried Representation Gaps

- [x] Add `register_allocator_classify_loop_representation()` and reusable
      result metadata that distinguishes an existing ready temp, no promotable
      value, one stack-backed value, multiple stack-backed values, and cases
      where representation classification is not applicable.
- [x] Validate the complete candidate status, reason, count, and temp-index
      contract before classification, and initialize output before every
      failure so malformed callers cannot observe stale representation state.
- [x] Add direct coverage for `temp_ready`, `no_value`,
      `unique_stack_value`, `ambiguous_stack_values`, topology rejection, a
      malformed candidate, and five invalid-input paths.
- [x] Pin all 11 new debug records in the permanent focused fixture. The
      strict direct harness now emits 802 records.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 802 records match with
      SHA-256
      `c868a126d56e53539dabf95ea760516d452ae9923990ba79476f4753fdf4ff87`.
- [x] Verify focused production remains mutation-free at 1358 diagnostics and
      preserves exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)` with status zero. The Windows matrix was intentionally
      not run for this phase at user request.

The core can now describe whether loop activation already has a real temp or
requires target-side discovery of zero, one, or multiple stack-backed values.
This phase does not discover stack identities or activate production mutation;
those remain separate correctness boundaries. `-ra` remains opt-in.

---

## 198. Collect Loop Stack-Value Identities

- [x] Add `register_allocator_collect_loop_stack_values()` and normalized
      stack-value observations so target code can report stable identities
      without exposing TAC representation to the allocator core.
- [x] Select observations only from the loop entry predecessor, header, and
      latch predecessor, ignore observations from unrelated blocks, and
      deduplicate identities in deterministic first-observation order.
- [x] Stage all identities before publication so malformed observations,
      invalid blocks/topology, allocation failure, and insufficient caller
      capacity leave caller storage unchanged and reset the published count.
- [x] Preserve a successful empty result for non-loop or ambiguous topology
      while rejecting malformed topology state.
- [x] Add direct coverage for selected and outside-block observations,
      duplicate identities, stable ordering, capacity rollback, malformed
      observation, topology skip and rejection, overlapping blocks, and four
      null/invalid-input contracts.
- [x] Pin all 13 new debug records in the permanent focused fixture. The
      strict direct harness now emits 815 records.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 815 records match with
      SHA-256
      `ac187c0d53cca25f73afba291dbd6eee5f3b0906f9db74a6d197942117804060`.
- [x] Verify focused production remains mutation-free at 1358 diagnostics and
      preserves exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)` with status zero. The Windows matrix was intentionally
      not run for this phase at user request.

The core can now collect a stable set of loop-local stack-value identities,
but production still needs a conservative Z80 TAC adapter that emits those
observations. This phase does not promote a stack value or activate loop
retention. `-ra` remains opt-in.

---

## 199. Observe Production Loop Stack Identities

- [x] Add a Z80 production adapter that maps label-backed TAC operands to
      deterministic function-local variable indexes by declaration-node
      identity, without exposing pointer values to the generic core.
- [x] Observe `result`, `arg1`, and `arg2` label operands while excluding
      variable declarations and non-local labels, then feed normalized
      observations through the Phase 198 collector and Phase 197 classifier.
- [x] Keep the path observe-only and fail closed on allocation, observation,
      collection, or classification failure; no TAC or allocator state is
      mutated.
- [x] Pin all 13 source-reachable observations, operand-role coverage, stable
      first-observation ordering, collected identities, variable names, and
      final representation in the permanent focused fixture.
- [x] Verify the current loop deduplicates to local identity 1 (`value`) and
      identity 0 (`count`) and reports `ambiguous_stack_values`, proving that
      promotion must choose or isolate one value rather than treating the
      existing source loop as uniquely promotable.
- [x] Verify focused production emits 1378 diagnostics, the direct harness
      remains at 815 records, no loop retention transaction runs, and exact
      assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`)
      remain unchanged.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)` with status zero. The Windows matrix was intentionally
      not run for this phase at user request.

Production can now identify the concrete stack-backed values responsible for
the loop representation gap. The next correctness boundary is conservative
candidate selection or a source fixture with one genuinely loop-carried stack
identity; this phase does not promote either value. `-ra` remains opt-in.

---

## 200. Select A Complete Loop-Carried Stack Value

- [x] Add `register_allocator_select_loop_stack_value()` and reusable role
      metadata for entry write, header read, latch read, latch write, and exit
      read. Require the complete five-role mask before a stack identity can be
      selected for future promotion.
- [x] Return deterministic ready, ineligible, and ambiguous outcomes while
      rejecting invalid role bits, negative identities, duplicate identities,
      malformed counts, and null contracts with canonical reset output.
- [x] Add direct coverage for one complete candidate, no complete candidate,
      multiple complete candidates, an empty value set, duplicate identity,
      invalid role mask, invalid identity, and five invalid-input paths.
- [x] Derive production role masks from existing TAC read/write semantics and
      select identity 1 (`value`) with mask 31 while rejecting identity 0
      (`count`) with mask 14. Keep selection observe-only and mutation-free.
- [x] Discover the loop region by a conservative CFG fixed point so structured
      branched loops can identify exit edges beyond immediate header
      successors. Verify `allocator_ra_array_write_indexes` degrades to an
      ineligible mask instead of aborting and passes all five byte tests.
- [x] Pin all selector outcomes and production role/selection diagnostics in
      the permanent focused fixture. Focused production emits 1384 records,
      the strict direct harness emits 838 records, and no loop retention
      transaction runs.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 838 records match with
      SHA-256
      `132ad46e3eb59e313ad8578b73f3d5fce7d927d332a75821caee29ea7f706df1`.
- [x] Preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)` with status zero. The Windows matrix was intentionally
      not run for this phase at user request.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)` with status zero. The Windows matrix was intentionally
      not run for this phase at user request.

Production can now distinguish a fully loop-carried stack value from partial
loop participants without relying on variable names or observation order. The
next correctness boundary is a validated promotion plan that introduces or
maps a temp without duplicate definitions or unsafe TAC mutation. `-ra`
remains opt-in.

---

## 201. Plan Loop Stack-Value Promotion

- [x] Add `register_allocator_plan_loop_stack_promotion()` and reusable plan
      metadata containing the selected local identity, supported value width,
      and collision-free proposed temp index.
- [x] Validate ready, ineligible, and ambiguous selection invariants plus all
      existing temp indexes before planning. Initialize plan output before
      every failure and reject malformed or duplicate existing temp metadata.
- [x] Accept only current allocator widths of 8 and 16 bits, preserve explicit
      ineligible reasons for selection, unsupported size, and temp collision,
      and publish ready metadata only after every check succeeds.
- [x] Add direct coverage for ready 8-bit and 16-bit plans, ineligible and
      ambiguous selections, unsupported width, proposed-temp collision,
      malformed selection, invalid and duplicate existing temps, and seven
      invalid-input contracts.
- [x] Wire observe-only production planning from the Phase 200 selection using
      the selected local's real width and a maximum-plus-one temp index. The
      focused loop plans identity 1 (`value`), size 8, as fresh `r1` above the
      existing `r0` without creating metadata or rewriting TAC.
- [x] Pin every direct planner outcome and the production preflight record in
      the permanent fixture. Focused production emits 1386 records, the strict
      direct harness emits 854 records, and no loop retention transaction runs.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 854 records match with
      SHA-256
      `244fd424fd47067636d60ddca64883a563e99598a8caf86b0beff8f9f226edf4`.
- [x] Preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)` with status zero. The Windows matrix was intentionally
      not run for this phase at user request.

Production now has validated metadata for a fresh temp representing the
selected loop-carried stack value. The next correctness boundary is atomic
promotion application: create temp metadata and rewrite only the planned TAC
operands without duplicate definitions, then rebuild analysis before enabling
retention. `-ra` remains opt-in.

---

## 202. Plan Loop Stack-Value Rewrites

- [x] Add `register_allocator_plan_loop_stack_rewrites()` and normalized
      rewrite observations containing instruction, operand, stack identity,
      and one loop role.
- [x] Validate the complete promotion plan and every observation before
      publication, ignore observations for other identities, reject duplicate
      instruction/operand targets, and require the complete five-role mask.
- [x] Stage the schedule atomically so insufficient capacity, malformed
      observations, invalid promotion metadata, and incomplete roles publish
      no rewrites and leave caller storage unchanged.
- [x] Add direct coverage for a five-rewrite ready schedule, unrelated
      identities, capacity rollback, incomplete roles, ineligible promotion,
      duplicate targets, malformed observations and promotion, and eight
      invalid-input contracts.
- [x] Extend production role discovery to emit normalized rewrite observations
      and plan the selected `value` rewrite to fresh `r1`: entry result, header
      argument, latch source/result, and exit argument in observation order.
- [x] Keep production observe-only and explicitly assert that no promotion or
      rewrite application runs. The branched-loop regression remains
      ineligible and passes all five byte tests.
- [x] Pin all 20 direct scheduler records and all five production rewrite
      records plus schedule summaries. Focused production emits 1400 records,
      and the strict direct harness emits 874 records.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 874 records match with
      SHA-256
      `4b85d5466afde4cb0870dd82a06cc83ae89cac76979961a51383505648acc205`.
- [x] Preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)` with status zero. The Windows matrix was intentionally
      not run for this phase at user request.

Production now has a complete, validated rewrite schedule but still does not
change TAC or temp metadata. The next correctness boundary is one atomic
application transaction followed by CFG, liveness, and interval rebuild before
loop retention can consume the promoted temp. `-ra` remains opt-in.

---

## 203. Apply Loop Stack Promotion As One Transaction

- [x] Add `register_allocator_apply_loop_stack_promotion()` and a single
      callback transaction containing the validated promotion metadata and
      complete rewrite schedule.
- [x] Revalidate promotion status, rewrite schedule status/count/role union,
      instruction and operand bounds, target temp consistency, and unique
      instruction/operand targets before invoking the callback.
- [x] Publish the applied rewrite count only after callback success. Report
      callback rejection as an atomic failure with zero published rewrites,
      reject invalid callback return values, and preserve a successful no-op
      for ineligible promotion plans.
- [x] Add direct coverage for successful application, callback rejection,
      invalid callback return, ineligible no-op, malformed rewrite, duplicate
      target, incomplete role union, capacity mismatch, and eight invalid-input
      contracts, including callback-count and output-reset assertions.
- [x] Pin all 16 application diagnostics in the permanent focused fixture and
      continue to assert that production does not yet invoke promotion
      application. Focused production emits 1398 records, and the strict direct
      harness emits 890 records.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 890 records match with
      SHA-256
      `0a057f188f856437c2db6c1a6940b88d7229352e05a0496e5590c67efe43dc43`.
- [x] Preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).

The generic core now exposes one validated transaction boundary for promotion
metadata and all operand rewrites. Production remains observe-only because the
target callback must still allocate function-owned temp metadata, rewrite TAC
with rollback, and trigger complete analysis reconstruction. `-ra` remains
opt-in.

---

## 204. Rebuild Analysis After Every Promotion Mutation

- [x] Expand `register_allocator_plan_post_mutation_rebuild()` to account for
      inserted instructions, rewritten operands, and added temps. Require a
      rebuild when any dimension is nonzero, including rewrite-only and
      temp-only promotion mutations, while preserving the unchanged no-rebuild
      path.
- [x] Reject negative mutation dimensions and aggregate mutation-count
      overflow atomically, with canonical output reset on failure.
- [x] Keep production observe-only by passing zero rewritten operands and zero
      added temps on its unchanged path; production continues to report
      `rebuild=no` and does not apply the promotion transaction.
- [x] Pin direct coverage for insertion-only, rewrite-only, temp-only,
      combined-promotion, unchanged, negative-dimension, aggregate-overflow,
      and output-reset behavior. Focused production emits 1398 records, and
      the strict direct harness emits 896 records.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 896 records match with
      SHA-256
      `3abaf66119fdc18a50a88b37042b22c1402e3519be7ae8aa9b9e091af3595a85`.
- [x] Preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`. The Windows matrix was intentionally not run for
      this phase at user request.

Analysis invalidation can now represent every mutation required by stack-value
promotion. Concrete target-side temp/TAC mutation with rollback and invocation
of CFG, liveness, and interval reconstruction remain before loop retention can
be activated. `-ra` remains opt-in.

---

## 205. Commit Promoted Temp And Operand Storage Atomically

- [x] Add `register_allocator_commit_loop_stack_promotion_storage()` as a
      target-independent transaction over normalized operand and temp storage.
      It stages every operand rewrite plus the promoted temp record and
      publishes both arrays and the temp count only after complete validation.
- [x] Validate promotion metadata, instruction and storage dimensions, temp
      capacity and collisions, rewrite bounds and temp consistency, unique
      targets, and each target's current stack identity before allocating or
      publishing staged state.
- [x] Emit one diagnostic for every staged rewrite and an explicit atomic
      committed/rejected transaction diagnostic. Stale operands and duplicate
      targets identify the exact failing rewrite.
- [x] Add direct coverage for a five-rewrite commit and rollback on a stale
      final operand, duplicate target, temp collision, exhausted capacity,
      undersized operand storage, malformed rewrite, and nine invalid-input
      contracts. Rejection cases verify byte-for-byte operand and temp storage
      preservation plus an unchanged temp count.
- [x] Keep production observe-only: no concrete TAC operand or function-owned
      temp metadata is changed, no promotion callback is invoked, and generated
      output remains canonical. Focused production emits 1398 records, and the
      strict direct harness emits 916 records.
- [x] Verify strict GCC/MSVC parity after normalizing line endings and only the
      two established ABI-sensitive records. All 916 records match with
      SHA-256
      `0701a61020f99ea1ceb7ee216c747a69fc898c5f488c3d434caca524eb5c25d6`.
- [x] Preserve exact assembly (SHA-256
      `630c40c18774db41136d0248c142e13bdc1043febce32db907932e3996756771`)
      and linked ROM (SHA-256
      `66a861e5d534669a9ec1e0d255d2de9799a7092cd6b60580441a53e8224ab44a`).
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`. The Windows matrix was intentionally not run for
      this phase at user request.

The core can now prove and execute atomic publication against normalized
storage. The next boundary is a target adapter that snapshots and maps real
function-owned temp metadata and heterogeneous TAC operands into this
transaction, then invokes complete analysis reconstruction before enabling
loop retention. `-ra` remains opt-in.

---

## 206. Stage Real Loop Promotion State Through The Target Adapter

- [x] Extend the Z80 loop-stack storage adapter to map all real TAC result,
      argument-one, and argument-two locations into normalized operand storage
      before invoking the atomic core commit.
- [x] Stage a complete enlarged function-owned temp table alongside the
      normalized transaction, preserving every existing temp record and
      initializing the promoted temp with canonical spill, register, original
      identity, and boundary defaults.
- [x] Derive the promoted temp's inclusive live interval and read/write counts
      from the validated rewrite roles, and reject incomplete metadata before
      any real TAC or temp-table publication.
- [x] Add one no-fail publication branch that rewrites heterogeneous real TAC
      operands and swaps the staged temp table only after all allocation,
      normalized commit, and real-state validation succeeds.
- [x] Keep the production call observe-only for this phase and assert zero
      published rewrites and temps. Existing diagnostics remain byte-for-byte
      stable for five snapshot rewrites and one staged temp.
- [x] Build the compiler under strict GCC C90 flags and run the direct allocator
      core harness successfully. Recompile the focused production fixture and
      verify five exact snapshot records with no `publication=real` record.

The target adapter can now stage and publish the concrete TAC and temp metadata
mutation without a fallible operation after publication starts. Production
still requests snapshot mode. The next increment must invoke real publication,
thread rewritten-operand and added-temp counts into post-mutation rebuild, and
reconstruct CFG, liveness, and intervals before loop retention consumes the new
temp. `-ra` remains opt-in.

---

## 207. Publish One Loop Promotion And Rebuild Analysis

- [x] Activate real publication for one complete loop stack-value promotion.
      Rewrite all five validated TAC operands and atomically replace the
      function-owned temp table with the staged table containing promoted `r1`.
- [x] Stop join reconciliation immediately after publication so no path,
      spill-site, or loop-retention decision can consume stale pre-mutation
      liveness.
- [x] Thread exact rewritten-operand and added-temp counts through the existing
      post-mutation rebuild planner. Rebuild the four-block/four-edge CFG and
      recompute liveness with two temps before block-exit spill policy and
      linear scan resume.
- [x] Preserve conservative semantics in this phase: promoted `r1` remains
      spill-backed at frame offset `-7`, all three covered block exits still
      require spilling, and loop retention is not applied yet.
- [x] Expand the focused production fixture to pin all five real operand
      rewrites, atomic temp publication, mutation handoff, rebuild dimensions,
      pre/post liveness states, three required exits, spill-slot allocation,
      generated TAC comments, and absence of stale post-promotion planning.
- [x] Run the complete focused compiler/assembler/linker fixture, byte tests,
      TAC insertion tests, Z80 spill tests, and direct allocator harness. It
      passes with 1,429 production diagnostics and 916 direct-core records.
- [x] Pin generated assembly SHA-256
      `084f5bf16b72bf63307bb49a38401cf42509c75c246a6804211c726504b00eb9`
      and linked ROM SHA-256
      `647cfd09b279c1e83043beb95ce882930ae8ea6f9555de6891b5984014177ef3`.
- [x] Stage the fresh compiler in every default-test search path and run the
      complete default regression matrix; it reports `DONE (170 tests)`.
      The Windows matrix was intentionally not run for this phase.

Production now performs one concrete, atomic stack-to-temp loop promotion and
rebuilds CFG and liveness before normal allocation resumes. The next increment
must rerun loop reconciliation against the rebuilt two-temp analysis and apply
the existing atomic loop-retention transaction for promoted `r1`, replacing
the conservative block-exit spills only after its cyclic reservations commit.
`-ra` remains opt-in.

---

## 208. Reconcile The Promoted Loop Against Rebuilt Analysis

- [x] Resize function-owned join-path reservation storage after promotion adds
      a temp, preserving any committed entries before a controlled second join
      reconciliation against the rebuilt CFG and liveness state.
- [x] Require the second pass to publish no further structural mutation. Emit
      exact inserted, rewritten, added-temp, and reservation-state counts and
      fail closed if another promotion or TAC insertion is requested.
- [x] Prove rebuilt `r1` is the unique loop live-in and discover its entry
      definition at TAC 55, header consumer at TAC 59 argument two, and latch
      definition at TAC 62.
- [x] Permit initially unassigned loop producers and consumers in the generic
      schedule contract, matching the existing atomic join schedule contract,
      and add direct ready-plan coverage for that state.
- [x] Plan three assignments and three cyclic block reservations for `C`, but
      audit every active promoted-temp operand before publication. Detect two
      uncovered uses, beginning with the latch self-read at TAC 62 argument
      one, and convert the plan to ineligible before any reservation or TAC
      assignment commits.
- [x] Preserve correct conservative output: reservation state remains empty,
      all three block exits still require spilling, and generated assembly and
      ROM remain byte-for-byte identical to Phase 207.
- [x] Expand focused diagnostics for the second candidate/definition pass,
      schedule, cyclic reservation plan, incomplete coverage, empty spill
      mutation order, resized storage, and zero-mutation handoff. The focused
      fixture passes compiler, assembler, linker, byte, TAC insertion, spill,
      and direct-core tests with 1,525 production and 920 direct-core records.
- [x] Preserve assembly SHA-256
      `084f5bf16b72bf63307bb49a38401cf42509c75c246a6804211c726504b00eb9`
      and linked ROM SHA-256
      `647cfd09b279c1e83043beb95ce882930ae8ea6f9555de6891b5984014177ef3`.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`. The Windows matrix was intentionally not run at user
      request.

Production now safely revisits the promoted loop using rebuilt analysis and
proves the existing three-anchor retention plan is incomplete. The next
increment must extend the atomic loop schedule to cover the latch source and
the live exit use, extend reservations through every covered block, and only
then commit retention and exempt block-exit spills. `-ra` remains opt-in.

---

## 209. Cover Every Promoted Loop Use And Commit Retention

- [x] Generalize loop-retention planning, application, and atomic commit from
      exactly three anchor assignments to three anchors plus a validated set
      of supplemental consumers, while preserving producer/consumer role,
      common-register, operand, and duplicate-target invariants.
- [x] Scan the promoted temp's rebuilt live range for active uncovered reads.
      The production fixture discovers the latch self-read at TAC 62 argument
      one and the exit return at TAC 71 argument one, then plans both as
      supplemental consumers of `C`.
- [x] Extend join-path reservation planning through every supplemental
      consumer block and reject consumers not forward-reachable from the loop
      join. Production now commits five assignments and four reservations.
- [x] Rerun the fail-closed operand coverage audit before publication. It
      reports five assignments, zero uncovered operands, and complete status
      before the atomic transaction publishes four reservation-state entries.
- [x] Exempt the three covered block exits from spilling only after that
      transaction commits. The retained latch source lowers as `LD A,C`, the
      retained return lowers as `LD L,C`, and the target loop no longer reads
      promoted `r1` from frame offset `IX-7`.
- [x] Teach Z80 return-value lowering to materialize retained `B`, `C`, and
      `BC` sources, including the required signed or unsigned 8-to-16-bit
      extension behavior.
- [x] Extend the strict direct harness with successful five-assignment plan,
      apply, and commit lifecycles plus duplicate target, conflicting register,
      invalid role, insufficient capacity, and unreachable-block rejection.
      Also prove application and commit independently reject a duplicate
      supplemental target without invoking their callbacks or publishing
      state when a caller bypasses the planner.
- [x] Run the complete focused compiler, assembler, linker, byte, TAC
      insertion, spill, and allocator-core suite. It passes with 1,534
      production diagnostics and 969 direct-core diagnostics.
- [x] Pin generated assembly SHA-256
      `be6bf444d95378bf53d15a15999b3ab20dae67dde504fbef7225ce000c95381c`
      and linked ROM SHA-256
      `14e2035ad382954f2cad520fc0a4d4013b1747b80cef48ebdd53b5614ab48df5`.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`. The Windows matrix was intentionally not run at user
      request.

The first promoted loop-carried temp now remains in `C` across its complete
active live range, including the latch update and live exit, and only committed
coverage suppresses its block-exit spills. The next increments should harden
this path across multiple exits and loop shapes, 16-bit values, multiple
candidates or loops, and register-conflict fallback. `-ra` remains opt-in.

---

## 210. Reserve Complete Paths To Supplemental Loop Consumers

- [x] Close the reservation gap between a loop join and supplemental consumers.
      For each consumer, reserve every block that is both forward-reachable
      from the join and reverse-reachable from that consumer, rather than only
      reserving the consumer block.
- [x] Preserve the existing cyclic region and entry reservation, union each
      supplemental path into that set, deduplicate shared prefixes and merged
      paths, and publish reservations in stable block order.
- [x] Add per-consumer core diagnostics carrying assignment, consumer block,
      complete path-block count, and newly added block count. Production reports
      two path blocks with zero additions for the latch read and three path
      blocks with one addition for the live exit.
- [x] Add a strict direct eight-block CFG with a loop, shared exit prefix, two
      exit arms, and a merge. Consumers on both arms produce seven reservations;
      a post-merge consumer conservatively reserves all eight blocks because
      both arms can reach it.
- [x] Verify capacity failure after path expansion remains atomic. Capacity
      seven rejects the eight-block merged plan, clears the returned plan, and
      leaves caller assignment and reservation evidence untouched.
- [x] Keep the production fixture behavior stable: it still commits five
      assignments and four reservations, reports zero uncovered operands,
      exempts three covered block exits, emits `LD A,C` and `LD L,C`, and does
      not reload promoted `r1` from `IX-7`.
- [x] Run the complete focused compiler, assembler, linker, byte, TAC
      insertion, spill, and allocator-core suite. It passes with 1,536
      production diagnostics and 1,012 direct-core diagnostics.
- [x] Preserve generated assembly SHA-256
      `be6bf444d95378bf53d15a15999b3ab20dae67dde504fbef7225ce000c95381c`
      and linked ROM SHA-256
      `14e2035ad382954f2cad520fc0a4d4013b1747b80cef48ebdd53b5614ab48df5`.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`. The Windows matrix was intentionally not run at user
      request.

Supplemental loop consumers now reserve complete control-flow paths, so a
retained register cannot be silently reused in an intermediate exit block.
The next increments should cover 16-bit promoted loop values, multiple
candidates or loops, and register-conflict fallback. `-ra` remains opt-in.

---

## 211. Retain 16-Bit Loop Values In HL

- [x] Extend production loop-promotion coverage from byte values in `C` to a
      16-bit loop-carried stack value retained in `HL`.
- [x] Verify the join, latch update, branch use, and live exit all receive the
      same `HL` assignment and complete-path reservation coverage.
- [x] Verify generated Z80 code uses the retained register and does not reload
      the promoted word temp from its stack slot.
- [x] Preserve the byte-loop behavior and all strict direct-core assertions.

The first production word loop now exercises width-aware retained assignment
through a complete cyclic and supplemental live range. `-ra` remains opt-in.

---

## 212. Reconcile Sequential Loops To A Fixed Point

- [x] Replace the single post-mutation rescan with a bounded reconciliation
      loop that rebuilds CFG, liveness, and reservation storage after each
      structural mutation and stops on the first mutation-free pass.
- [x] Make exact same-register join-assignment publication idempotent so a
      previously promoted loop can be replayed while a later loop is applied.
- [x] Restrict terminal supplemental-only reservations to their last use while
      retaining full-block coverage for cyclic, entry, and intermediate path
      blocks, preventing false conflicts at adjacent loop boundaries.
- [x] Add two sequential byte loops and verify pass one promotes the first,
      pass two promotes the second, and pass three reaches a stable fixed point.
- [x] Verify both loops remain in `C`, the first assignment replays five sites,
      and the first reservation ends before the second loop begins.

Sequential independent loops can now accumulate promotions across bounded
reconciliation passes without losing prior assignments or reserving unrelated
terminal instructions. `-ra` remains opt-in.

---

## 213. Preserve Stack State For Ineligible Loop Promotions

- [x] Emit an explicit `loop_stack_promotion_fallback` diagnostic when a loop
      stack value cannot satisfy promotion requirements, with action
      `preserve_stack`.
- [x] Add a mixed fixture where the first loop remains stack-backed while a
      later eligible loop is still promoted and retained in `C`.
- [x] Verify fallback is local rather than transaction-wide: the first value
      continues to load from `IX-6`, the second avoids its `IX-10` temp reload,
      and reconciliation reaches a stable fixed point.
- [x] Extend direct reservation tests with exact terminal and intermediate
      supplemental-path endpoints.
- [x] Run the complete focused compiler, assembler, linker, byte, TAC
      insertion, spill, and allocator-core suite successfully.
- [x] Pin generated assembly SHA-256
      `4ef648695f22a262dc13720417ac7c16391538c83e6ef39d89a42d8bea0f053f`
      and linked ROM SHA-256
      `868fa32e16994aa1fe64bbfa6e89bb24bb7fc2a75ba23f47189eb64cb2a8f5c8`.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`.
- [x] Keep the Windows matrix intentionally unrun at user request.

Loop promotion now fails closed per candidate while independent eligible loops
continue to optimize. Remaining loop-retention hardening includes genuine
physical-register reservation conflicts, multiple promotable values in one
loop, nested loops, complex exits, and adversarial iteration-limit coverage.
`-ra` remains opt-in.

---

## 214. Preflight Loop-Retention Reservation Ownership

- [x] Query every planned loop block reservation against committed function-
      scan path state before applying assignments or publishing reservations.
- [x] Preserve exact same-temp ownership while converting a foreign owner in
      the same physical-register slot and overlapping instruction range into
      an ineligible loop-retention plan.
- [x] Emit `loop_retention_conflict_preflight` with selected register, slot,
      planned reservation count, conflicting block/range, owner temp, and
      either `retain` or `preserve_stack` action.
- [x] Keep assignment and reservation publication atomic by rejecting conflicts
      before the existing transaction callback.
- [x] Assert complete no-conflict preflights for the new four-block multi-value
      loop and seven-block nested-loop reservation, alongside the direct core
      owned/available/conflict path-state cases.

Production now has an explicit conservative gate before a loop reservation can
collide with an earlier retained owner. A source-level fixture that reaches the
foreign-owner branch is still needed. `-ra` remains opt-in.

---

## 215. Select The First Of Multiple Loop Candidates

- [x] Make stack-value selection deterministic when multiple identities satisfy
      all required entry/header/latch/exit roles: select the first observation
      while retaining the total candidate count.
- [x] Allow promotion planning to consume a ready first-candidate selection
      with more than one candidate.
- [x] Apply the same deterministic first-live-in rule after rewriting while
      preserving ambiguous loop topology as ineligible.
- [x] Emit `loop_stack_selection_deferred` with selected identity, deferred
      count, `preserve_stack` action, and first-observation ordering.
- [x] Add strict direct-core cases for two-candidate stack selection, promotion,
      and live-in selection.
- [x] Add `allocatorRaCoreLivenessMultiValue`: both byte values satisfy all
      promotion roles, `value1` is promoted and retained in `C`, `value2`
      remains stack-backed at `IX-7`, and pass two is mutation-free.

Loops with multiple eligible stack values now optimize one deterministic
candidate without rejecting the whole loop. Assigning additional candidates to
disjoint registers remains future work. `-ra` remains opt-in.

---

## 216. Handle Unsupported Nested Joins Conservatively

- [x] Guard physical-register reads when a join has no consumer; `TAC_USE_RESULT`
      shares the zero value formerly used to initialize the absent-consumer
      operand and previously caused a null dereference.
- [x] Make an unselected core join schedule canonically ineligible before
      consumer validation, because no consumer assignment will be emitted.
- [x] Add a strict direct-core case proving an unselected schedule accepts an
      absent consumer without modifying caller assignment storage.
- [x] Emit `join_retention_fallback` with `consumer_unsupported` or
      `path_ineligible` and action `skip_join_rewrite`.
- [x] Add `allocatorRaCoreLivenessNestedLoops`: retain the outer value in `C`
      across all seven participating blocks, keep the inner value stack-backed
      at `IX-8`, and safely skip the unsupported inner join.
- [x] Run the complete focused compiler, assembler, linker, byte, TAC, spill,
      and allocator-core suite. It passes with 5,918 compiler-log lines and
      1,014 direct-core lines.
- [x] Pin generated assembly SHA-256
      `b54e775a5701dad0b877ca1296e4954d56ca5d8fc113508107e4a4169ee1c889`
      and linked ROM SHA-256
      `0e0f6e0b17777f6416511e8758012fd0c817ffa937e8f20896cf6bb5edc4c2c3`.
- [x] Run the allocator-enabled `calculations` real-project regression from a
      clean tree; compilation and linking complete successfully.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`.
- [x] Keep the Windows matrix intentionally unrun at user request.

Nested control flow can no longer abort allocation merely because a secondary
join has no supported consumer. Inner-loop promotion, complex exits, and
cross-level register arbitration remain. `-ra` remains opt-in.

---

## 217. Continue Stack Promotion With Retained Live-Ins

- [x] Continue stack-value discovery when a loop already has a retained
      live-in candidate instead of treating that candidate as the end of
      promotion discovery.
- [x] Preserve the mutation-driven fixed point: rewrite one selected stack
      identity, rebuild CFG and liveness, then discover the next identity from
      fresh analysis.
- [x] Emit `loop_stack_search` with `retained_live_in_present` or
      `no_live_in_temp` so each discovery decision remains inspectable.
- [x] Extend `allocatorRaCoreLivenessMultiValue` to promote both eligible byte
      values over two mutation passes and stabilize without mutation on pass
      three.

Multiple eligible stack values in one loop can now become temps without
discarding an already retained live-in. Physical-register ownership remains a
separate conservative decision. `-ra` remains opt-in.

---

## 218. Retain Nested Values In Disjoint Registers

- [x] Build a temporary candidate-liveness view for each loop join and mask
      temps that already own reservations at that join, leaving canonical
      liveness unchanged.
- [x] Continue deterministic candidate selection after a successful retention
      transaction until no unreserved live-in candidate remains.
- [x] Preserve reservations published earlier in the same function scan so
      nested candidates can be checked against outer-loop ownership.
- [x] Add `allocatorRaCoreLivenessNestedDisjoint`: retain the outer byte value
      in `C` and the inner word value in `HL`, including all inner exit reads.
- [x] Add strict direct-core coverage proving overlapping instruction ranges
      in different active slots remain available.

Nested loops can now retain simultaneously live byte and word values when
their target-policy slots are disjoint. `-ra` remains opt-in.

---

## 219. Preserve Stack State On Foreign Nested Ownership

- [x] Exercise the production reservation preflight with nested byte values
      that both require `C`.
- [x] Confirm the inner candidate reports the outer temp as owner of the
      overlapping slot and downgrades to `preserve_stack` before publication.
- [x] Assert the rejected application publishes zero assignments and zero
      reservations, while the inner value remains stack-backed and receives
      required block-exit spills.
- [x] Run the complete focused compiler, assembler, linker, byte, TAC, spill,
      and allocator-core suite. It passes with 9,373 compiler-log lines and
      1,015 direct-core lines.
- [x] Pin generated assembly SHA-256
      `6ad4c60cc1ea5ba31ab05af75ed2f0ca7add22c1c052d00a48f940d6528af4f5`
      and linked ROM SHA-256
      `29069842ed26c2f622725ae3fad42d8b90866bce8ce52bbf335b4cf6337f3bb3`.
- [x] Rebuild and stage the GCC compiler; all staged copies have SHA-256
      `bc46beba284a2c9135063df0c53ce03a80e2f95abda453bef901c6d73572ca25`.
- [x] Run the allocator-enabled `calculations` real-project regression from a
      clean tree; compilation and linking complete successfully.
- [x] Run the complete default regression matrix; it reports
      `DONE (170 tests)`.
- [x] Keep the Windows matrix intentionally unrun at user request.

Nested same-slot ownership now fails closed in the production path without
partially mutating assignments or reservations. Complex exits, adversarial
fixed-point limits, and broader register arbitration remain. `-ra` remains
opt-in.

---

## 220. Retain Values Across Break Exits

- [x] Add target-independent loop-flow profiling for entry, back-edge,
      side-exit, and terminal-exit cardinality.
- [x] Reconstruct the complete natural-loop region backward from its latch so
      side exits are classified independently from loop topology.
- [x] Add `allocatorRaCoreLivenessBreakExit` with one normal exit and one
      `break` edge; promote its stack value, retain it in `C`, and cover all
      six participating blocks with one atomic transaction.
- [x] Pin five rewritten operands, six reservations, pass-two fixed-point
      stabilization, and generated assembly with no residual value-stack
      access.

Single-latch loops can retain promoted values across a `break` side exit when
the normal exit and side-exit paths are both covered by the reservation plan.
`-ra` remains opt-in.

---

## 221. Classify Continue Loops Conservatively

- [x] Profile all back edges entering a candidate loop header instead of
      relying on one selected latch.
- [x] Split ambiguous topology diagnostics into `multiple_entries` and
      `multiple_back_edges` reasons.
- [x] Add `allocatorRaCoreLivenessContinueExit`, whose `continue` statement
      creates a second back edge to the loop header.
- [x] Preserve the value on the stack, publish no assignments or
      reservations, and skip TAC mutation atomically with
      `reason=multiple_back_edges`.

Multiple-latch loops now fail closed with a precise reason. Supporting their
optimization requires a reservation and definition model that represents all
back edges; this phase does not weaken the single-latch proof. `-ra` remains
opt-in.

---

## 222. Retain Values Across Early Returns

- [x] Count distinct terminal return targets reached directly from a loop
      region as part of the reusable loop-flow profile.
- [x] Add `allocatorRaCoreLivenessEarlyReturn` with one in-loop return and one
      normal loop exit returning the same promoted value.
- [x] Retain the promoted value in `C` across both return paths with six
      rewritten operands and six atomic reservations.
- [x] Pin both backend return sources as physical `C`, pass-two fixed-point
      stabilization, and generated assembly with no residual value-stack
      access.
- [x] Add strict direct-core profiles for break, continue, early return, and
      malformed input, edge, and block contracts.
- [x] Run the complete focused suite with 11,281 compiler diagnostics and
      1,025 direct-core diagnostics. Pin generated assembly SHA-256
      `c683bf1345aa71f5b34525f648471a9e1d2fb8eab523259268845977110c244b`
      and linked ROM SHA-256
      `010e117753e9bf8eb6d798eb0edd554bb56e2a9dfd9979b747c365d4b2109887`.
- [x] Rebuild and stage the GCC compiler; all three copies have SHA-256
      `187e52570a642e4aca261133d1b7d204566d597e885ca2d1a9952af7752047b2`.
- [x] Run the clean allocator-enabled `calculations` regression and the
      complete default matrix, which reports `DONE (170 tests)`. Keep the
      Windows matrix intentionally unrun at user request.

Loop retention now covers single-latch side exits and terminal exits while
remaining conservative for multiple back edges. Adversarial fixed-point
limits, broader register arbitration, and general spill/reload support remain.
`-ra` remains opt-in.

---

## 223. Derive A Checked Fixed-Point Budget

- [x] Add `register_allocator_plan_fixed_point_limit()` to derive the
      reconciliation pass limit from the initial instruction capacity rather
      than an incidental control-flow field.
- [x] Reject zero, negative, overflowing, null-function, and null-output
      contracts while resetting caller output before validation.
- [x] Emit `fixed_point_limit` diagnostics with the explicit
      `basis=initial_instructions` contract.

The mutation loop now has a reusable, overflow-checked termination budget
whose bound reflects the finite source of promotion opportunities. `-ra`
remains opt-in.

---

## 224. Classify Fixed-Point Progress

- [x] Add `register_allocator_plan_fixed_point_step()` with canonical
      `rebuild`, `stable`, and `iteration_limit` outcomes.
- [x] Distinguish stability on the final permitted pass from further mutation
      on that pass, so the former succeeds and the latter fails closed.
- [x] Add strict direct-core coverage for all three outcomes, stale-output
      reset, malformed pass and limit ranges, invalid rebuild flags, and null
      contracts.

Fixed-point termination is now an explicit target-independent decision rather
than local loop control in pass 5. `-ra` remains opt-in.

---

## 225. Enforce The Fixed-Point Policy In Production

- [x] Replace the local `max_blocks + 1` guard with the checked core budget and
      per-pass classifier.
- [x] Emit `fixed_point_budget` once per function and
      `fixed_point_step_preflight` after every completed reconciliation pass.
- [x] Preserve existing successful-pass diagnostics and fail closed before
      another rebuild when a mutating pass exhausts the budget.
- [x] Pin all 14 production budgets and 28 pass decisions in the focused
      fixture, including the multi-value loop's two rebuild decisions followed
      by stable convergence and the absence of iteration-limit failures.
- [x] Run the complete focused suite with 11,365 compiler diagnostics and
      1,040 direct-core diagnostics. Preserve generated assembly SHA-256
      `c683bf1345aa71f5b34525f648471a9e1d2fb8eab523259268845977110c244b`
      and linked ROM SHA-256
      `010e117753e9bf8eb6d798eb0edd554bb56e2a9dfd9979b747c365d4b2109887`.
- [x] Rebuild and stage the GCC compiler; all three copies have SHA-256
      `78c5ed6ca4aaafcca6c21f18ef7a2df93f0e29ca2ff87309683b3c0f7e6e25c6`.
- [x] Run the clean allocator-enabled `calculations` regression and the
      complete default matrix, which reports `DONE (170 tests)`. Keep the
      Windows matrix intentionally unrun at user request.

Production fixed-point control now shares the same validated policy as the
direct-core tests. Broader candidate/register arbitration, remaining emitter
audits, and general spill/reload support remain. `-ra` remains opt-in.

---

## 226. Select A Unique Defining Latch

- [x] Add `register_allocator_select_loop_latch()` to inspect every back-edge
      block and publish a canonical ready loop join only when exactly one
      latch writes the candidate value.
- [x] Treat additional back edges without a write as transparent value-carry
      paths; preserve ineligible and ambiguous outcomes for zero or multiple
      defining latches.
- [x] Validate block partitions, CFG edges, callback results, pass inputs, and
      stale output before publishing a selected latch and definition.
- [x] Add strict direct-core coverage for unique, missing, and ambiguous
      writers, invalid callbacks, malformed blocks and edges, and null/input
      contracts.

Multiple back edges are no longer inherently ambiguous when only one latch
updates the retained value. `-ra` remains opt-in.

---

## 227. Collect Stack Roles Across All Latches

- [x] Seed loop-region reconstruction from every back-edge source instead of
      only the first latch reported by legacy topology classification.
- [x] Classify reads and writes in every latch block while preserving entry,
      header, and exit role handling.
- [x] Allow one-entry multi-latch loops to enter stack-value discovery through
      an effective topology without weakening the later retention proof.
- [x] Pin the continue fixture's promoted value with the complete role mask
      `31`, five operand rewrites, and a required CFG/liveness rebuild.

Stack promotion can now expose a loop-carried temp across transparent continue
edges. Physical retention still requires Phase 226's unique defining-latch
proof. `-ra` remains opt-in.

---

## 228. Retain Values Through Transparent Continue Edges

- [x] Integrate defining-latch selection after multi-latch candidate discovery
      and pass the resulting effective topology through fact discovery,
      definition validation, scheduling, and reservation planning.
- [x] Reuse graph-closure reservations to cover both the transparent continue
      path and the updating latch without manufacturing a producer assignment
      on the unchanged path.
- [x] Upgrade `allocatorRaCoreLivenessContinueExit` from conservative stack
      fallback to `C` retention with five assignments and six reservations.
- [x] Pin five spill-exempt pre-exit blocks, physical-`C` return emission, no
      residual `IX-6` value access, one rebuilding pass, and stable convergence
      on pass two.

Continue loops with one updating latch and any number of transparent latches
can now retain their loop-carried value. Multiple defining latches still fail
closed until multi-producer scheduling is implemented. `-ra` remains opt-in.

---

## 229. Collect All Defining Latches

- [x] Add atomic, callback-driven collection of every active definition on a
      loop header's back edges, preserving stable edge order.
- [x] Publish each defining latch, edge, and instruction only after complete
      validation and capacity checks.
- [x] Add strict direct-core coverage for mixed defining and transparent
      latches, insufficient capacity, malformed blocks and edges, callback
      failure, and null/input contracts.

The allocator can now represent all updating latches instead of collapsing
them into a unique-or-ambiguous result. `-ra` remains opt-in.

---

## 230. Plan Multi-Latch Retention

- [x] Generalize loop retention schedules to a contiguous producer prefix
      followed by consumers, while retaining the legacy single-latch wrapper.
- [x] Add each extra latch definition as a supplemental producer and reserve
      the selected physical-register slot across every producer path.
- [x] Keep planning and application fail-closed for duplicate assignments,
      incompatible registers, missing back edges, malformed roles, capacity
      exhaustion, and callback failure.
- [x] Add strict direct-core tests for successful planning and atomic apply,
      producer conflicts, duplicates, malformed topology, and capacity limits.

Multi-producer schedules and reservations are now target-independent core
contracts. `-ra` remains opt-in.

---

## 231. Retain Values Across Multiple Updating Latches

- [x] Recollect all defining latches in production after promotion and use the
      first as the compatibility primary plus the remainder as supplemental
      producers.
- [x] Activate generalized planning, reservation, conflict checking, and
      atomic application for one-entry loops with multiple updating latches.
- [x] Add `allocatorRaCoreLivenessMultiLatchWriters`, where both continue and
      fallthrough latches update the same promoted 8-bit value.
- [x] Pin two collected definitions, seven assignments, six reservations,
      retained `C` updates on both latches, physical-`C` return emission, no
      residual `IX-6` value access, and stable convergence on pass two.

Multiple updating latches can now retain one loop-carried value when every
producer and consumer shares the selected register. Broader register
arbitration and producer forms remain future work. `-ra` remains opt-in.

---

## 232. Validate Loop Register Facts

- [x] Add a target-independent loop-register selection contract over the
      complete producer/consumer assignment set.
- [x] Validate candidate uniqueness, preferred-register membership, producer
      and consumer roles, operand positions, instruction indexes, and bound
      physical-register facts before selection.
- [x] Emit exact per-assignment diagnostics for malformed roles, operands, and
      physical registers without publishing a partial selection.

Loop retention now has one checked register-fact boundary before schedule
construction. `-ra` remains opt-in.

---

## 233. Select A Common Loop Register

- [x] Evaluate the preferred register first, then the remaining target-ordered
      candidates, against every bound producer and consumer fact.
- [x] Preserve the current preferred register when facts are unbound, adopt a
      consistently pinned alternate, and return an ineligible result when no
      candidate satisfies all assignments.
- [x] Add strict direct-core coverage for preferred and alternate selection,
      mixed conflicts, unsupported pins, duplicate candidates, missing
      preferences, malformed assignments, and null/input contracts.

The generic core can now arbitrate a common register instead of requiring the
caller to guess one before inspecting assignment facts. `-ra` remains opt-in.

---

## 234. Activate Policy-Owned Loop Register Arbitration

- [x] Build the complete ordered assignment fact set before loop planning,
      including entry, every updating latch, the header consumer, and all
      supplemental consumers.
- [x] Resolve candidates through the target policy and use the generic
      selection result to choose the active slot and retention register.
- [x] Preserve current code generation for unbound loops: 8-bit values prefer
      `C`, 16-bit values prefer `HL`, and the multi-latch fixture still applies
      seven assignments and six reservations before stabilizing on pass two.
- [x] Pin exact core and production arbitration diagnostics plus existing byte
      and assembly behavior in the focused suite.

Production loop retention can now honor a consistently preassigned alternate
register while preserving legacy preferences. Path-aware alternate viability
and broader producer forms remain future work. `-ra` remains opt-in.

---

## 235. Evaluate Loop Candidate Paths

- [x] Add a target-independent callback contract that evaluates every ordered
      loop-register candidate for complete-path safety.
- [x] Validate candidate, capacity, callback, context, and output contracts
      without publishing partial success.
- [x] Emit exact per-candidate path results and aggregate safe-candidate counts.

Loop register arbitration can now consume explicit path facts rather than
assuming assignment compatibility implies path safety. `-ra` remains opt-in.

---

## 236. Compose Path-Safe Loop Arbitration

- [x] Require candidate-aligned path evaluations during common-register
      selection and reject malformed or mismatched facts.
- [x] Preserve preferred-first ordering while skipping candidates that are
      assignment-compatible but unsafe on at least one loop path.
- [x] Distinguish no common register from no path-safe register and add direct
      coverage for alternate selection and conservative ineligibility.

The generic selector now requires one register that satisfies both complete
assignment and complete-path constraints. `-ra` remains opt-in.

---

## 237. Activate Path-Aware Loop Arbitration

- [x] Derive the complete candidate path ranges from the existing multi-latch
      reservation planner before selecting a physical register.
- [x] Evaluate every target-policy candidate across every reservation range,
      excluding scheduled producer and consumer instructions from intervening
      clobber checks.
- [x] Preserve valid 8-bit multi-latch retention in `C` while rejecting the
      previously unchecked 16-bit loop when both `HL` and `BC` are clobbered
      on a contributing path.
- [x] Pin exact production path diagnostics, alternate decisions, and
      preserve-stack behavior in the focused suite.

Production loop retention now fails closed when no assignment-compatible
candidate is transparent across the complete cyclic path. Broader producer
forms and general cross-block split/reload scheduling remain. `-ra` remains
opt-in.

---

## 238. Track Rejected Loop Registers

- [x] Add a bounded candidate-aligned exclusion ledger for loop-register
      attempts without mutating candidate policy order.
- [x] Record reservation-conflict reason, block, and instruction while
      rejecting duplicate or malformed exclusions.
- [x] Add strict direct-core coverage for initialization, accumulation,
      duplicate rejection, invalid candidates, capacity, locations, and null
      contracts.

Loop arbitration can now retain checked rejection state across downstream
planning attempts. `-ra` remains opt-in.

---

## 239. Select Around Rejected Loop Registers

- [x] Compose the exclusion ledger with assignment-compatible, path-safe
      common-register selection while preserving preferred-first order.
- [x] Select the next viable alternate after one or more exclusions and report
      all-candidates-excluded separately from assignment and path failures.
- [x] Preserve the original selector API for existing callers and add strict
      tests for preferred exclusion, chained fallback, exhaustion, alignment,
      state, count, and null failures.

The generic selector can now resume arbitration after a later candidate-local
failure without reconsidering a rejected physical register. `-ra` remains
opt-in.

---

## 240. Retry Candidate-Specific Reservation Conflicts

- [x] Initialize one exclusion ledger per production loop candidate and expose
      the attempt number in exact preflight diagnostics.
- [x] On a physical-slot reservation conflict, exclude only the selected
      register and rerun common-register arbitration with rebuilt scratch
      planning state.
- [x] Preserve stack after candidate exhaustion while keeping candidate-
      independent coverage failures and transaction failures on their existing
      fail-closed paths.
- [x] Pin all focused production opportunities at attempt one and prove no
      spurious retries in the current source fixtures; direct-core adversarial
      tests cover ordered retry and complete exhaustion.

Production loop retention can now move to another legal register after a late
candidate-specific reservation conflict. Broader producer forms and general
cross-block split/reload scheduling remain. `-ra` remains opt-in.

---

## 241. Admit Assignment-Produced 8-Bit B Loop Values

- [x] Extend the narrow Z80 `B` candidate contract to assignment producers
      while retaining the existing arithmetic and operand-role restrictions.
- [x] Route assignment-specific `B` endpoint checks through that shared
      candidate contract instead of rejecting them before common-register
      arbitration.
- [x] Prove in the source-reachable multi-value loop that both assignment and
      arithmetic producers are legal in `B`, while the preferred `C` remains
      blocked by the independently retained loop value.

Assignment-produced 8-bit loop values can now participate in bounded `B`
arbitration without broadening unsupported consumers or producer classes.
`-ra` remains opt-in.

---

## 242. Complete Physical-B Assignment Materialization

- [x] Classify `RA_LOCATION_PHY_B` as a physical location requiring no IX/IY
      address materialization, matching `A`, `C`, `HL`, and `BC`.
- [x] Preserve fail-closed address policy for stack, global, constant, and
      unsupported location/target combinations.
- [x] Compile and link the source-reachable assignment-to-`B` loop through the
      existing materialize-to-`A` and store-from-`A` backend helpers.

Physical `B` assignment destinations no longer enter the memory-address path.
The complete generated function reaches `.ENDS` and links successfully.
`-ra` remains opt-in.

---

## 243. Pin Assignment-to-B Loop Retention End To End

- [x] Add a completion-only backend diagnostic for physical-`B` assignment
      emission, including source kind, widths, and the concrete transfer path.
- [x] Pin preferred-`C` path rejection, alternate-`B` selection, five operand
      assignments, four path reservations, atomic commit/application, three
      block-exit spill exemptions, and fixed-point convergence.
- [x] Pin exact ROM bytes and generated `LD B,A`/retained-`B` behavior while
      proving the promoted value no longer accesses its former `IX-10` slot.
- [x] Keep the multi-latch fixture conservative: assignment endpoints advance
      through `B` legality, but its later unsupported return endpoint still
      rejects `B` and leaves `C` selected.

The focused source fixture now distinguishes allocator success from backend
completion and catches either policy or emission regressions. `-ra` remains
opt-in.

---

## 244. Admit 8-Bit Array-Read Results In B

- [x] Add `TAC_OP_ARRAY_READ` to the narrow Z80 `B` producer contract while
      preserving the existing size and consumer-operand restrictions.
- [x] Add a source-reachable two-read pressure fixture where the first array
      result occupies `C` and the second must evaluate alternate `B`.
- [x] Pin candidate facts showing `C` blocked by an active-register conflict,
      `B` assignment-compatible, path-safe, and conflict-free.

The linear scanner can now select `B` for a proven 8-bit array-read result
without broadening other producer or consumer classes. `-ra` remains opt-in.

---

## 245. Emit 8-Bit Array-Read Results In B

- [x] Audit the newly reachable backend path and identify that physical `B`
      fell through to the generic memory store after address mode `none`.
- [x] Add an explicit 8-bit physical-`B` result branch that emits `LD B,L` and
      rejects impossible 16-bit `B` results loudly.
- [x] Add a completion-only diagnostic recording the `L`-to-`B` transfer after
      successful instruction emission.

Array reads retained in `B` no longer emit the incorrect `LD (IX+0),L` memory
store or leave the later `B` consumer uninitialized. `-ra` remains opt-in.

---

## 246. Pin Array-Read B Retention End To End

- [x] Pin preferred `C` conflict, alternate `B` arbitration, retained interval,
      two physical array-read results, and backend completion diagnostics.
- [x] Assert generated `LD C,L`, `LD B,L`, retained-`B` consumption, `XOR A,B`,
      complete function emission, and absence of the former IX store.
- [x] Add a 63-byte exact-ROM regression while keeping the prior normal-array
      and Z80-input read tests unchanged.

The focused array-read suite now covers simultaneous `C` and `B` results and
fails on allocator, backend, assembly, or byte-level regressions. `-ra` remains
opt-in.

---

## 247. Admit 8-Bit Shift Results In B

- [x] Add `TAC_OP_SHIFT_LEFT` and `TAC_OP_SHIFT_RIGHT` to the narrow Z80 `B`
      producer contract while preserving existing consumer-role restrictions.
- [x] Add a source-reachable two-shift pressure fixture where the left-shift
      result occupies `C` and the right-shift result must evaluate `B`.
- [x] Pin candidate facts showing active `C` conflicts while `B` remains
      assignment-compatible, path-safe, and conflict-free.

The linear scanner can now select `B` for proven 8-bit shift results without
broadening unsupported operand roles. `-ra` remains opt-in.

---

## 248. Complete Physical-B Shift Results

- [x] Audit the newly reachable backend path and identify that a result already
      computed in `B` fell through to `LD (IX+0),B` with address mode `none`.
- [x] Recognize physical `B` as the shift value register and return without an
      unnecessary copy or address materialization.
- [x] Add a completion-only diagnostic reporting the shift opcode, physical
      result, value register, and `store=none` decision.

Shift results retained in `B` no longer write through a stale IX address before
their consumer. `-ra` remains opt-in.

---

## 249. Pin Shift B Retention End To End

- [x] Pin preferred-`C` conflict, alternate-`B` arbitration, retained interval,
      concrete shift result, and backend completion diagnostics.
- [x] Assert the generated `SLA B`, `LD C,B`, `SRL B`, retained `B`, and
      `XOR A,B` sequence while rejecting stack fallback and the old IX store.
- [x] Add a 62-byte exact-ROM regression while preserving both existing
      left- and right-shift `C` regressions.

The focused shift suite now covers simultaneous `C` and `B` shift results and
fails on allocator, backend, assembly, or byte-level regressions. `-ra` remains
opt-in.

---

## 250. Retain Shift-Produced 8-Bit Loop Values

- [x] Compose loop stack-value promotion with the existing shift producer and
      `C` consumer legality contracts for single-value loops.
- [x] Retain both left- and right-shift latch definitions in `C` across the
      entry, header, latch, and exit paths.
- [x] Preserve fail-closed behavior when an independent loop value crosses the
      shift: the shift clobbers every candidate register and selection reports
      `no_path_safe_register`.

Single shift-updated byte values can now use cross-block loop retention while
multi-value loops remain stack-backed when the shift would destroy another
live value. `-ra` remains opt-in.

---

## 251. Trace Physical-C Shift Completion

- [x] Add a completion-only diagnostic after the concrete `LD C,B` transfer
      for physical-`C` shift results.
- [x] Report the opcode, computation register, transfer, and `store=none`
      decision only after successful instruction emission.
- [x] Verify left and right loop latches reach the same complete backend path
      without materializing their former promoted spill slot.

Physical-`C` shift completion is now directly distinguishable from allocator
selection and generic result-location diagnostics. `-ra` remains opt-in.

---

## 252. Pin Shift Loop Retention End To End

- [x] Add `allocator_ra_shift_loop_results` with retained left/right loops and
      a multi-value path-safety conflict control.
- [x] Pin five endpoint assignments, four path reservations, atomic retention,
      six block-exit exemptions, one fixed-point rebuild per retained loop,
      and conservative stack fallback for the conflict case.
- [x] Assert scoped `SLA B`/`SRL B`, `LD C,B`, retained producer/consumer
      behavior, absence of former value-slot accesses, and two 56-byte exact
      ROM regions.

The focused suite now catches cross-block shift selection, transaction,
backend, assembly, stack-fallback, or byte-level regressions. `-ra` remains
opt-in.

---

## 253. Plan CFG-Authorized Reload Scan Ranges

- [x] Add a reusable allocator-core planner that starts from a split-spill
      block and extends only through unique, contiguous fallthrough
      successors with exactly one predecessor.
- [x] Stop before branches, joins, cycles, or noncontiguous instruction
      ranges instead of treating linear TAC order as executable path order.
- [x] Validate blocks and edges fail-closed and report the selected end block,
      end instruction, extension count, and stop reason.

Reload-chain discovery now has a conservative target-independent CFG boundary
that can authorize straight-line successor blocks without crossing ambiguous
control flow. `-ra` remains opt-in.

---

## 254. Integrate Reload Scan Ranges In Pass 5

- [x] Route split-spill reload placement through the CFG range planner using
      the function's already-built blocks and edges.
- [x] Size reload-chain storage and bound discovery by the authorized range
      instead of unconditionally stopping at the spill block end.
- [x] Add `reload_scan_preflight` diagnostics connecting the core decision to
      the concrete spill TAC and pass-5 reload placement.

Existing same-block split/reload emission remains byte-identical, while pass 5
can now discover supported reload sites in safe straight-line successor
blocks. `-ra` remains opt-in.

---

## 255. Pin Reload Range Topology And Integration

- [x] Extend the strict reload-chain core harness with multi-block
      fallthrough, branch, join, cycle, noncontiguous, and malformed-edge
      cases.
- [x] Compose a two-successor-block authorized range with reload-chain
      discovery, target selection, mutation, and application in the final
      successor block.
- [x] Assert exact stop classifications and verify cycles are rejected before
      they can be counted as accepted extensions; cover conditional and
      unique-jump branch stops plus malformed block ordering.
- [x] Extend the source-level split-spill suites with core/pass-5 range traces
      while preserving reload application, backend diagnostics, and exact ROM.

The reload tests now fail on unsafe CFG widening, integration drift, concrete
reload regressions, or byte-level changes. Operand-specific reload emission
beyond the existing `ARRAY_WRITE result -> BC` path remains unfinished. `-ra`
remains opt-in.

---

## 256. Generalize TAC Split Operand Metadata

- [x] Replace the operand-named split spill/reload booleans with generic
      operand-valued TAC metadata, using `-1` as the canonical absent value.
- [x] Keep the active Z80 path explicit: retained `ARRAY_READ arg1` spills and
      `ARRAY_WRITE result` reloads remain the only emitted split pair.
- [x] Extend canonical empty-TAC and register-spill conversion invariants to
      reject stale split operand metadata.

The TAC representation can now identify arg1, arg2, or result without adding
another role-specific flag. Existing emitted code remains unchanged. `-ra`
remains opt-in.

---

## 257. Propagate Reload Operands Through Core Mutations

- [x] Add operand identity to reload mutation plans and clear it atomically
      with the rest of a rejected or empty plan.
- [x] Add an operand-aware preparation API for arg1, arg2, and result while
      preserving the existing result-oriented API as a compatibility wrapper.
- [x] Forward and validate the planned operand through mutation application
      callbacks and pass-5 integration.

Strict direct-core tests now cover all legal operands, invalid operands,
empty plans, callback forwarding, and result-oriented reload-chain execution.
`-ra` remains opt-in.

---

## 258. Pin Generic Metadata And Existing Emission

- [x] Update TAC insertion tests to require `-1` split operands for every
      canonical empty TAC and preserve spill-conversion fail-closed behavior.
- [x] Pin result-operand reload mutation traces in both source-level
      split-spill fixtures.
- [x] Preserve retained-arg1 spill diagnostics, result reload diagnostics,
      and exact-ROM output for byte and word compound array updates.

The representation and mutation layers are operand-aware, while backend
activation remains deliberately limited to the proven
`ARRAY_READ arg1 -> ARRAY_WRITE result` form. `-ra` remains opt-in.

---

## 259. Generalize Reload Emission Operand Metadata

- [x] Add operand identity to reusable reload-emission metadata and clear it
      to `-1` with every rejected or canonical empty emission.
- [x] Add an operand-aware emission-preparation API for arg1, arg2, and result.
- [x] Preserve the existing reload-emission API as a result-operand
      compatibility wrapper without changing current callers.

The reusable emission boundary now preserves the operand selected by mutation
planning instead of dropping it before target emission. `-ra` remains opt-in.

---

## 260. Enforce Reload Operands In The Z80 Backend

- [x] Pass the TAC reload operand into operand-aware emission preparation.
- [x] Require prepared emission metadata to match the result-only
      `ARRAY_WRITE -> BC` backend contract before emitting spill-slot loads.
- [x] Treat `-1` as no reload while routing all active operands through a
      standalone Z80 validator that rejects arg1/arg2 with an explicit
      `unsupported_operand` diagnostic.

Unsupported backend activation now fails closed rather than silently ignoring
non-result reload metadata. Existing result reload code remains byte-identical.
`-ra` remains opt-in.

---

## 261. Pin Operand-Aware Reload Emission

- [x] Extend strict direct-core tests across arg1, arg2, result, invalid
      operands, stale no-op operands, poisoned-output clearing, and the
      compatibility wrapper.
- [x] Add standalone Z80 backend tests for one supported result operand, two
      unsupported arg operands, and three malformed requests.
- [x] Pin core `operand=0` emission and backend `operand_id=0` diagnostics in
      each individual function in both source-level split-spill fixtures.
- [x] Preserve exact ROM output for byte and word compound array updates.

The focused tests now catch operand loss between mutation and emission,
invalid no-op state, unsupported metadata acceptance, backend contract drift,
and byte-level regressions. `-ra` remains opt-in.

---

## 262. Generalize Reload-Chain Execution By Operand

- [x] Add operand-aware single-site and reload-chain execution APIs while
      retaining result-oriented compatibility wrappers.
- [x] Resolve and validate each discovered site's operand before selecting a
      target register or applying mutation.
- [x] Clear chain and execution outputs atomically when an operand query
      fails.
- [x] Extend the strict core harness with mixed result/arg1/arg2 chains and
      invalid-operand cleanup, and pin all selection/application diagnostics.

Reload-chain execution now preserves operand identity from discovery through
target selection and mutation. Existing result-only callers retain their
original API and behavior. `-ra` remains opt-in.

---

## 263. Integrate Operand-Aware Reload Chains In Pass 5

- [x] Resolve the actual temp-bearing TAC operand at each reload site.
- [x] Use that operand for endpoint eligibility, target register selection,
      physical-register field mutation, and reload TAC metadata.
- [x] Reject preassigned or target-unsupported operand endpoints before core
      mutation application.
- [x] Preserve the existing source-reachable
      `ARRAY_WRITE result -> BC` reload behavior.

Pass 5 no longer assumes every split reload mutates the result field. Target
policy remains the authority for activation, so unsupported endpoints still
fail closed. `-ra` remains opt-in.

---

## 264. Qualify The Bounded Z80 Reload Endpoint Matrix

- [x] Extract the concrete Z80 split-reload endpoint decision into a
      standalone, directly testable target helper.
- [x] Authorize exactly 16-bit `ARRAY_WRITE result -> BC` and emit explicit
      `supported`, `unsupported`, or `invalid_input` diagnostics for every
      query.
- [x] Cover `ARRAY_WRITE`, `ARRAY_READ`, and `GET_ADDRESS_ARRAY` result/arg
      and width variants without activating an unproven non-result backend
      path.
- [x] Add strict C90 tests asserting one supported, eight unsupported, and
      four malformed endpoint decisions.

The first-version Z80 reload endpoint is now explicit and auditable rather
than an implicit static callback. Non-result forms remain unsupported until
both source reachability and backend instruction ordering are proven. `-ra`
remains opt-in.

---

## 265. Apply Reload Chains Atomically

- [x] Add a transaction callback that receives every prepared reload mutation
      only after the complete chain has passed operand, register, and mutation
      validation.
- [x] Keep the existing per-site preparation diagnostics while preventing a
      late callback rejection from leaving partial TAC mutation.
- [x] Add strict mixed result/arg1/arg2 commit coverage and late-rejection
      coverage proving `partial=0`.

Reload-chain mutation is now all-or-nothing. A stale operand, unsupported
register, or target callback rejection clears the execution result and leaves
the TAC unchanged. `-ra` remains opt-in.

---

## 266. Discover And Apply Reloads Across The CFG

- [x] Add target-independent breadth-first reload discovery over reachable
      basic blocks, with join/cycle deduplication and deterministic instruction
      ordering.
- [x] Reject the whole schedule when any reachable path is unsafe or capacity
      is truncated instead of accepting a partial graph.
- [x] Replace pass 5's unique-fallthrough reload range with graph discovery
      and one atomic graph transaction.
- [x] Pin diamond, join, cycle, path-stop, callback-rejection, and backward
      schedule behavior in the strict core harness and source-level fixtures.

Split reload placement can now cover safe branch joins and loop paths without
confusing linear TAC order with control-flow reachability. Existing source
activation remains limited to the proven Z80 endpoint. `-ra` remains opt-in.

---

## 267. Complete Bounded Z80 Qualification

- [x] Reject malformed, overlapping, or non-increasing basic-block ranges and
      out-of-range CFG edges before graph traversal.
- [x] Add adversarial truncation, overlapping-block, malformed-edge, unsafe
      path, backward-schedule, and atomic callback-rejection tests.
- [x] Preserve exact ROM for both split-spill source fixtures and pass the
      designated `calculations` real-project allocator regression.
- [x] Rebuild and hash-match the Cygwin compiler in all staged test paths with
      no compiler warnings.
- [x] Run the full plain non-Windows integration suite; the Windows matrix is
      intentionally excluded from this phase.

The bounded first working Z80 register allocator is complete. Unsupported
reload operands and unproven backend forms remain fail-closed, and allocator
behavior remains opt-in behind `-ra`.

---

## 268. Make Repeated Pointer-Base Reads Source-Reachable

- [x] Prove that ordinary repeated reads through a pointer local remain
      label-backed after existing IL optimization, rather than assuming a
      generic reload-core test proves source reachability.
- [x] Promote a single-definition, non-escaping pointer local to a temp before
      temp compaction when at least two direct array-base reads occur within
      one basic block.
- [x] Reject calls, branch/loop spans, inline-asm barriers and tracked asm
      references, address-taking, additional definitions, non-base uses, and
      computed 16-bit-index forms that introduce a separate base-copy TAC.
- [x] Preserve pointer declaration metadata on temp read bases so element
      signedness, index scaling, and promoted result widths remain distinct
      from the 16-bit base container.
- [x] Remove the unused pointer declaration TAC so promotion does not allocate
      both a dead local slot and a new base spill slot.

Promotion uses function-label ranges because TAC function ownership is not yet
populated at this early pass-5 stage. It is disabled under default/`-no-ra` and
forced all-spill modes. This is bounded direct-base promotion, not general
local-variable promotion or alias analysis. `-ra` remains opt-in.

---

## 269. Enable ARRAY_READ Arg1 Reloads Into HL

- [x] Authorize exactly 16-bit `ARRAY_READ arg1 -> HL` alongside the existing
      16-bit `ARRAY_WRITE result -> BC` split-reload endpoint.
- [x] Reload the read base after index materialization into `BC`, before
      copying the base to `IY`, without destroying a pending index value.
- [x] Validate operand, temp, retained location, spill backing, and canonical
      emission metadata before emitting the reload instructions.
- [x] Carry the actual operand name into pass-5 reload diagnostics rather than
      printing `result` for every endpoint.
- [x] Exercise mixed read/write reloads through one atomic CFG transaction.

The target-policy split-spill fixture now has a source-reachable two-mutation
chain: a later read reloads `arg1` into `HL`, then an indexed write reloads its
`result` base into `BC`. Other operands, widths, and target forms remain
fail-closed. No additional registers or relaxed call/asm contracts are added.

---

## 270. Qualify Read Reloads And Measure Their Benefit

- [x] Add four exact-ROM SMS cases for repeated zero-index byte reads,
      computed byte indexes, repeated word reads, and signed-byte-to-word
      promotion, including complete two-site reload chains.
- [x] Add eight unsafe-source rejection cases for branches, loops, computed
      16-bit-index base copies, asm barriers, tracked asm operands outside the
      live span, escaped addresses, reassignment, and calls.
- [x] Extend strict C90 endpoint coverage to two supported, ten unsupported,
      and four malformed requests; retain the atomic CFG rejection tests.
- [x] Preserve forced all-spill behavior and default/`-no-ra` assembly identity
      using the same output filename for both compilations.
- [x] Preserve the original split-spill exact-ROM fixtures and pin the new
      mixed read/write source sequence.
- [x] Rebuild the Cygwin compiler without warnings and hash-match its staged
      test binary.
- [x] Run the full plain integration suite: `DONE (173 tests)`, including
      `calculations-1` and `calculations-2`, which both compile under `-ra`.
- [x] Build the native MSVC Release compiler/linker with installed toolset and
      SDK overrides, then hash-match the per-project staged binaries.
- [x] Run the native Windows-mode integration suite: `DONE (173 tests)`,
      including both allocator-enabled calculations fixtures.

The mixed read/write probe's body shrinks from 111 bytes with the pre-batch
allocator to 74 bytes, saving 37 bytes without increasing its 8-byte frame.
Its base-transport instruction sequences save an estimated 179 Z80 T-states
without wait states; this is an instruction-cost estimate, not emulator timing.
The four new positive ROM spans contain 49, 84, 107, and 71 bytes respectively.
The signed case pins byte loads and sign extension, not a widened word read.
Both full integration suites and their allocator-enabled real-project gates
pass. `-ra` remains opt-in.

---

## 271. Consolidate Array-Base Reload Emission

- [x] Reuse one reload path for `ARRAY_READ arg1 -> HL` and
      `ARRAY_WRITE result -> BC`, removing the duplicate write helper.
- [x] Preserve endpoint restrictions, operand-specific traces, byte order,
      and index-before-read-reload ordering; existing exact-ROM tests pass.

---

## 272. Reuse Spill Resolution And Preservation

- [x] Fold retained read-base preservation into the same transfer helper,
      removing another duplicate helper without adding production functions.
- [x] Reuse `_resolve_tac_location` on a local TAC copy with retention cleared
      to resolve spill backing without mutating the original operand metadata.
- [x] Keep canonical spill/reload planning and existing IX load/store helpers;
      legacy split-spill and mixed read/write exact-ROM tests pass.

---

## 273. Compact Promotion Walks And Qualify Consolidation

- [x] Track crossed barriers during the existing eligibility walk and remove
      dead pointer declarations during rewriting, eliminating two extra loops.
- [x] Preserve all four positive and eight negative source cases, forced
      all-spill behavior, default/`-no-ra` identity, and existing ROM bytes.
- [x] Compile both changed sources with `-ansi -pedantic -Wall -Werror`.
- [x] Pass the full plain integration suite: `DONE (173 tests)`.
- [x] Build native MSVC Release binaries and hash-match their staged copies.
- [x] Pass the full native Windows integration suite: `DONE (173 tests)`.

Production code is 77 lines smaller, with two fewer helpers and no new
production functions. Supported source scope and ROM bytes are unchanged;
the previous 37-byte optimization and unchanged frame survive.

---

## 274. Share Pointer-Base Load Decisions

- [x] Reuse the address-base predicate for array reads and writes, removing
      two duplicates while preserving plain address-of and retained-base rules.
- [x] Preserve pointer-base and repeated-read exact-ROM fixtures.

---

## 275. Unify Array-Base Materialization

- [x] Reuse one base-to-IY path for reads and writes, removing the write helper
      while preserving constant-base rules, BC index handling, and trace names.
- [x] Preserve pointer, BC-base/HL-index, and mixed reload exact-ROM fixtures.

---

## 276. Reuse Type Sizes And Qualify Consolidation

- [x] Remove four width-only wrappers and use existing scalar type sizes at
      the write-value call site; retain non-scalar location-width fallback.
- [x] Preserve scalar, pointer, struct/union, and signed exact-ROM cases using
      existing tests, including all-spill and default/`-no-ra` identity checks.
- [x] Pass strict C90 compilation and native MSVC Release builds; hash-match
      both compilers and the native linker with their staged copies.
- [x] Pass the full plain integration suite: `DONE (173 tests)`.
- [x] Pass the full native Windows integration suite: `DONE (173 tests)`.

This batch removes 103 production-code lines and seven helpers without adding
files or allocation capabilities. Existing ROM bytes and frame sizes remain.

---

## 277. Reuse Address-Index Materialization

- [x] Replace duplicate nonconstant get-address index loads and extension with
      `_materialize_array_index_to_bc`; keep explicit constant-zero BC loading.
- [x] Preserve early retained-index handling and the BC-base/HL-index path.
- [x] Pass six existing address fixtures with both compiler builds.

---

## 278. Reuse Port-Output Value Materialization

- [x] Use `_materialize_to_a` for constant and retained A/HL/BC output values,
      preserving trace comments and the BC/nonconstant-port restriction.
- [x] Keep IX memory loads and the existing supported register set unchanged.
- [x] Pass existing value, port-conflict, and target-policy fixtures with both
      compiler builds.

---

## 279. Reuse Port-Input Stores And Qualify Consolidation

- [x] Use `_store_from_a` for port-input results; preserve A/HL/BC/C support,
      H/B zero extension, and declared-width memory-result handling.
- [x] Pass eleven existing focused address/port fixtures across the batch on
      Cygwin and native builds, including exact-ROM and trace checks.
- [x] Match all 88 before/after assembly hashes: eleven sources, two compilers,
      and default, `-no-ra`, `-ra-all-spill`, and `-ra` modes.
- [x] Pass strict `-ansi -pedantic -Wall -Werror` compilation, clean editor
      diagnostics, native MSVC Release build, and staged binary hash checks.

This batch removes 57 production-code lines with no new helpers or test files.
Assembly, allocation capabilities, ROM bytes, and frame sizes are unchanged.
Qualification is scoped to the touched emitters; the preceding batch passed
both full 173-test suites. No emulator timing or Valgrind claim is made.

---

## 280. Consolidate Retained Array-Read Results

- [x] Share A/B/C byte-result transfers and reuse `_store_from_hl` for HL/BC
      word results without broadening the supported register/width combinations.
- [x] Preserve retained-result comments and the B-specific completion trace.
- [x] Pass existing result, pointer-base, BC-base/index, and reload fixtures.

---

## 281. Reuse Complement Byte Transfers

- [x] Use `_materialize_to_a` for constant/A complement sources, keeping IX
      memory loads and the existing supported source kinds.
- [x] Remove redundant result-location flags and share the low-byte store;
      preserve signed/unsigned high-byte extension and retained A/B/C stores.
- [x] Pass six existing complement chain, location, and target-policy fixtures.

---

## 282. Consolidate Array-Base Tracing And Qualify Reuse

- [x] Reuse the base-to-IY helper's trace block for reads and writes, removing
      two read-side duplicates without changing materialization/trace order.
- [x] Pass thirteen focused fixtures across the batch on Cygwin and native
      builds, including B-result, exact-ROM, reload, and trace checks.
- [x] Match all 88 before/after assembly hashes across eleven sources, both
      compilers, and default, `-no-ra`, `-ra-all-spill`, and `-ra` modes.
- [x] Pass strict C90 compilation, clean editor diagnostics, native MSVC
      Release build, and staged binary hash checks.

This batch removes 65 production-code lines with no new helpers or test files.
Allocation capabilities, assembly bytes, and frame sizes remain unchanged.
Verification is focused on the touched emitters; full integration suites,
Valgrind, and emulator timing were not rerun for this consolidation batch.

---

## 283. Consolidate Word Complement Loading

- [x] Reuse `_materialize_to_hl` for constant/HL sources and share A/memory
      low-byte promotion logic, preserving signedness and promoted-type gates.
- [x] Remove redundant result flags while keeping IX memory addressing and
      BC-result operand handling unchanged.
- [x] Pass six existing complement chain, location, and target-policy fixtures.

---

## 284. Reuse Word Shift Result Transfers

- [x] Reuse `_store_from_hl` for retained HL/BC and word memory results;
      remove redundant HL/BC result flags and repeated retained-source comments.
- [x] Preserve byte B/C result transfers, trace text, source promotion, count
      handling, and both shift loops.
- [x] Pass all seven existing shift fixtures.

---

## 285. Reuse Byte Shift Transfers And Qualify Consolidation

- [x] Reuse `_materialize_to_a` for constant/A counts, keep IX memory loads,
      remove redundant result flags, and share the memory low-byte store.
- [x] Preserve retained-result rules, B/C traces, and signed high-byte extension.
- [x] Pass thirteen focused complement/shift fixtures on Cygwin and native
      builds, including exact-ROM and trace checks.
- [x] Match all 104 before/after assembly hashes across thirteen sources, both
      compilers, and default, `-no-ra`, `-ra-all-spill`, and `-ra` modes.
- [x] Pass strict C90 compilation, clean editor diagnostics, native MSVC
      Release build, and staged binary hash checks.

This batch removes 62 production-code lines with no new helpers or test files.
Allocation capabilities, assembly bytes, and frame sizes remain unchanged.
Full integration suites, Valgrind, and emulator timing were not rerun.

---

## Suggested Next Batches

These are optional improvements, not requirements for the bounded Z80 allocator.
Completed work is checked below. Work in order, revising priorities
from measured results. Keep `-ra` opt-in and calls/inline asm as hard barriers.
Reuse existing helpers, target policy, CFG/liveness data, mutation plans, and
fixtures; do not add wrapper functions or callback layers just to extract code.

### Batch 286-288: Measure And Reduce Cost

- [x] **286. Establish a repeatable quality baseline.** Use `calculations-1`,
      `calculations-2`, and existing pointer/loop fixtures in all four modes.
      Record ROM bytes, frame sizes, spill/reload and register-copy counts,
      compile time, and allocator memory. Keep estimated instruction cycles
      separate from emulator measurements. Reuse the existing test harness.
- [x] **287. Consolidate remaining duplicated decisions and transfers.**
      Audit existing candidate/split plans and emitter materialization before
      introducing abstractions. Add a source-reachable BC word-complement
      fixture before changing that uncovered path. Require identical assembly
      and traces for refactors, and track net production lines/helper counts.
- [x] **288. Introduce conservative target-aware cost preferences.** Start
      with the existing equal-next-use preference hook and candidate ordering.
      Compare transfer, spill, reload, and address-setup costs; use existing CFG
      loop information where available. Keep legality ahead of profitability
      and expand arbitration only after the baseline demonstrates a benefit.

Batch results:

- Run `bash ci/measure_register_allocator.sh` with freshly staged compiler,
      linker, and WLA tools on PATH. `RA_BENCHMARK_DIR` selects the output directory;
      positional fixture names override the seven-fixture default corpus. The
      driver reuses existing makefiles, preserves logs, assembly and ROM snapshots,
      and writes four-mode measurements to `summary.tsv`. It builds `linked.sms`
      rather than running mode-specific exact-byte expectations in other modes.
- Initial Cygwin GCC baseline (used ROM bytes; total/max static frame bytes):

| Fixture | Default / `-no-ra` ROM | All-Spill ROM | `-ra` ROM | Default Frame | `-ra` Frame |
| --- | ---: | ---: | ---: | ---: | ---: |
| calculations-1 | 24468 | 25404 | 23829 | 283 / 101 | 277 / 101 |
| calculations-2 | 17510 | 17510 | 16381 | 259 / 98 | 255 / 98 |
| allocator_ra_array_read_reload | 1703 | 1721 | 1467 | 129 / 14 | 120 / 14 |
| allocator_ra_array_write_pointer_bases | 233 | 233 | 215 | 25 / 13 | 22 / 11 |
| allocator_ra_shift_loop_results | 452 | 452 | 379 | 26 / 9 | 27 / 10 |
| allocator_ra_core_liveness_extraction | 1861 | 1861 | 1549 | 124 / 12 | 131 / 15 |
| allocator_ra_core_equal_next_use_policy | 357 | 357 | 313 | 34 / 17 | 34 / 17 |

- Frame totals/maxima are static function-frame measurements, not dynamic
      call-stack depth. Spill/reload counts cover inserted markers, not all memory
      accesses; register-copy counts are a proxy, not instruction-cycle costs.
      Compile seconds are wall times from comparable debug-output-enabled builds,
      summed over translation units. `workspace_plan_bytes` sums reported storage
      plans, including repeated plans; it is not allocator peak memory or RSS.
      Peak-memory and emulator-cycle measurements remain unimplemented.
- Phase 287 shares A/HL/B/C index transfer and extension within the existing
      materializer, keeping supported widths, sign extension, A preservation, and
      trace text. The BC word-complement path remains untouched without a dedicated
      source-reachable fixture. Original/current emitters produced identical ROMs
      in 24 corpus builds and identical assembly in 40 translation units after
      normalizing only randomized static-symbol suffixes.
- Initial Phase 288 evaluation found a profitable ARRAY_WRITE value-in-A tie,
      but no qualifying new ties in exploratory array-base/word-index, byte-shift,
      or byte-compare probes. The completion therefore qualifies that existing
      source-reachable case rather than enabling an unmeasured operand preference.
- Phase 288 completion replaces the role predicate in the existing equal-next-use
      hook with an explicit target transfer-byte comparison. For ARRAY_WRITE in A,
      retaining the value (arg1) costs zero consumer transfers; retaining the index
      (arg2) requires the one-byte `LD C,A` transfer. Prefer only strictly lower
      cost; equal costs and other operations/registers retain the existing order.
      Generic legality, unequal-next-use handling, and mutation plans remain ahead
      of this cost check. No new helper, callback, or candidate order is added.
- A current-code control compiler with the preference disabled uses 314 ROM bytes
      and seven register copies versus 313 bytes and six copies with it enabled.
      Total/max frame sizes stay 34/17 and planned storage stays 17160 bytes;
      inserted spill/reload counts are zero in both builds. The cost comparison
      therefore models the measured transfer difference, not hypothetical savings
      in spill, reload, or address setup. No loop-weighted or global cost model is
      claimed: those contexts are not available in this bounded callback.
- The original and cost-based predicates produce identical ROMs in all 28 builds
      of the seven-fixture/four-mode corpus. This is an explicit cost model for an
      already profitable choice, not an additional size improvement over the
      preceding compiler. Richer arbitration remains gated on source-reachable
      measurements. Default, `-no-ra`, and all-spill behavior is unchanged.
- Strict C90, GCC/MSVC builds and staged hashes, target-cost trace assertions,
      exact-byte checks, and existing unequal/invalid-policy tests passed on both
      compilers. Full suites were not rerun for this output-preserving cost-only
      change. Production grows by 12 net lines with no new functions or test files.
- Production source shrinks by 33 lines with no new helpers or policy APIs.
      The new measurement driver is separate from compiler production code.
- Validation: both full integration suites passed on 2026-10-11, each reporting
      `DONE (173 tests)`, with freshly staged Cygwin GCC/native MSVC binaries.
      All 28 native/Cygwin corpus ROM pairs match byte-for-byte. Strict C90,
      focused exact-byte/trace tests, editor diagnostics, benchmark assertions,
      and binary hash checks passed. Valgrind, sanitizers, peak-memory profiling,
      and emulator execution/timing were not run in this batch.

### Batch 289-291: Remove Avoidable Moves And Reloads

- [x] **289. Coalesce compatible copies.** Prefer the same physical register
      for producer/consumer copies when liveness, width, overlapping units,
      and instruction constraints permit it. Reuse candidate selection and
      assignment plans; do not merge temp identities without dataflow proof.
      Pin removed moves and conflicting-copy fallback in existing fixtures.
- [x] **290. Rematerialize cheap pure values.** Begin with constants and
      stable addresses when reconstructing them costs less than spill/reload.
      Check flags, scratch clobbers, frame lifetime, and every use; never treat
      mutable memory loads as pure. Reuse location materialization and preserve
      backing storage whenever any consumer still needs it.
- [x] **291. Broaden profitable split/reload endpoints incrementally.**
      Choose one source-reachable operand/register combination at a time,
      extending the existing target-approved reload-chain machinery. Account
      for all uses and total emitted cost, avoid redundant reloads, and retain
      whole-plan rollback and stack fallback for unsupported schedules.

Batch results (bounded implementations):

- Reuse the pointer-local promotion walk as `_promote_register_allocator_local_values`
      rather than adding a separate scalar analysis. Its new scalar case is limited
      to a nonstatic `u8` local defined by adding two `u8` parameters, with exactly
      one subsequent ADD use in the same block. Existing escape, reassignment,
      call, inline-asm, and control-flow checks remain fail-closed. Preserve the
      declared byte type when replacing the local with a temp; operand types have
      not yet been propagated at this stage, so eligibility uses parameter nodes.
- The existing candidate/path/overlap machinery retains the promoted right
      value in C across a left-hand byte ADD, then consumes it directly. The focused
      source case removes a stack store/load and a local frame byte. The final
      fixture including negative cases shrinks from 346 to 342 ROM bytes and
      34 to 33 total frame bytes versus the unmodified compiler under `-ra`,
      with one additional register copy (3 to 4); maximum frame size stays 14.
      Default, `-no-ra`, and all-spill assembly and ROM are byte-identical.
- Extend the existing byte assignment-chain fixture with exact bytes for the
      improvement and address-escape, inline-asm barrier, multiple-use, and
      reassignment fallbacks. The original chain and word/extension fixtures remain
      unchanged. Scalar promotion alone adds 23 net production lines.
- Phase 289 additionally removes the A round trip from full-width memory-to-BC
      assignments using existing IY byte-load helpers, saving two instructions.
      Retained HL-to-HL assignments reuse the existing word store helper and emit
      no copy, saving four instructions. Existing allocation decisions and width,
      overlap, extension, and stack-copy fallbacks remain unchanged. Existing
      pointer and split/reload fixtures pin these source-reachable reductions.
- Phase 290 reconstructs a zero-index global-array address with one immediate
      HL/BC load at a target-approved reload site instead of two spill-slot loads.
      The existing transfer emitter validates the reload and retains its original
      spill stores/backing slot. Its local backward definition check stops at a
      definition, label, jump, return, call, or asm barrier; computed indices and
      memory-loaded pointers keep the stack path. No mutable memory is treated
      as pure, and no new address materializer or persistent metadata is added.
- Phase 291 enables a word ASSIGNMENT arg1 reload into HL through the existing
      atomic reload-graph transaction. Pointer promotion accepts only a copy into
      a temp immediately feeding indexed address-taking, not arbitrary base copies.
      The existing assignment emitter consumes the reloaded source before any
      later use; all remaining uses keep their spill backing and approved reloads.
      Unsupported array-read base copies still fail closed in the original fixture.
- The final expanded pointer fixture shrinks from 699 to 660 ROM bytes and from
      22 to 18 register copies under `-ra`; total/max static frame sizes remain
      66/17. Both new fixtures have byte-identical default, `-no-ra`, and all-spill
      assembly and ROM versus the original compiler. The seven-fixture corpus
      has no ROM/frame regressions: six fixtures are unchanged, while the array-read
      reload fixture shrinks from 1467 to 1465 ROM bytes and from 22 to 20 copies.
- Production code grows by 71 net lines across the two existing passes. No new
      functions, policy APIs, or test files are added; shared emitters, helpers,
      proof/rewrite walks, and atomic plans are reused. This batch reduces emitted
      ROM/copy traffic, not compiler source size. General copy scheduling, temp
      identity merging, constants, nonzero/local-address rematerialization, and
      additional split endpoints are not claimed as implemented.
- Phase 288 was not expanded during this batch: the byte-compare probe reached
      no tied conflict and was removed. Its later bounded cost-model qualification
      is recorded above; no broader allocation policy is enabled.
- Strict C90, focused exact-byte/trace checks, editor diagnostics, staged-binary
      hash checks, and the four-mode corpus measurements passed. All eight native
      MSVC/Cygwin GCC ROM pairs for the two expanded fixtures match byte-for-byte.
      The full Cygwin GCC suite reported `DONE (173 tests)` on 2026-10-11.
      The native MSVC suite subsequently reported `DONE (173 tests)` and
      `PASS:native_full_suite` on 2026-10-11 with the verified native compiler.
      Valgrind, sanitizers, peak-memory profiling, and emulator execution/timing
      were not run.

### Batch 292-294: Improve Register And CFG Coverage

- [x] **292. Close useful B/C producer and consumer gaps.** Prioritize
      source-reachable call-result, arithmetic, and nonconstant assignment
      cases identified by the baseline. Validate B/C versus BC overlap and
      destructive operand ordering. Extend existing emitters rather than
      cloning them; receiving a call result must not weaken the call barrier.
- [x] **293. Improve edge-specific join reconciliation.** Build on current
      branch retention and atomic reservation plans, not a second CFG engine.
      Permit transfers only on the required edges; handle critical edges before
      placing edge-only code in a predecessor shared by other successors.
      Test diamonds, multiple exits, overlap conflicts, and rollback.
- [x] **294. Broaden profitable loop-carried producer forms.** Extend beyond
      the currently enabled assignment/shift shapes using existing multi-latch
      and continue-path analysis. Start with one arithmetic-produced value;
      qualify multiple live values, nested loops, and early exits separately.
      Keep memory-dependent values conservative around writes and aliasing,
      and reject retention when its transfers/spills outweigh saved accesses.

#### Batch 292-294 Results

- Phase 292 admits nonconstant byte assignment producers through the existing
      C consumer predicate and assignment emitter. Constant assignment policy,
      calls, width checks, destructive operand order, and B/C-versus-BC overlap
      gates remain unchanged. The existing B/C conflict fixture now pins a
      parameter assignment into C and its atomic loop transaction with exact
      linked bytes. No new emitter or production function is added.
- Phase 293 places false-branch reconciliation spills after the conditional,
      so the unrelated taken successor does not execute them. Existing insertion
      and approval callbacks are reused. Taken-edge spills retain their shared
      backing-store behavior; no new taken-edge-only transfer or critical-edge
      splitting is claimed. The core harness tests a shared predecessor, diamond,
      post-consumer cycle, overlap rejection, and transactional rollback.
- Ordinary forward-join schedules now reject backward definitions and stop
      route traversal at the consumer. A newly reachable comparison loop exposed
      invalid reservations beyond that consumer; its source regression now
      compiles without weakening the dedicated loop transaction rules. Routes
      that execute before the consumer remain reserved regardless of source
      order; the core harness separately pins a later-block detour and a
      post-consumer cycle.
- Phase 294 qualification found that an ordinary ADD-carried byte was already
      retained in C by the preceding compiler. This batch adds exact-byte and
      atomic-transaction coverage, not a new gain for that shape. A proposed
      second B-carried value was unsafe: header comparison materialization writes
      B, while the loop path check previously skipped reservation endpoints.
      Checking all unassigned endpoint TACs now rejects that retention and keeps
      the second value stack-backed. Existing multiple-value, nested-loop,
      continue, and exit regressions remain the qualification surface.
- B/C byte arithmetic transparency is consolidated into one existing helper,
      also covering nonoverlapping byte assignments through A. Word assignments,
      overlapping destinations, calls, and memory-dependent paths stay
      conservative. Production code is eleven lines smaller with one fewer helper;
      no new test files or production APIs are added.
- Step 294 remained unchecked at the end of commit `854ec01`: broader profitable
      loop producer coverage was not demonstrated. Unproven B consumer/emitter
      extensions were removed rather than counted as an optimization. The
      subsequent bounded completion is recorded below.
- Identical pre-batch corpus sources were measured with isolated preceding and
      final Cygwin compilers across default, `-no-ra`, `-ra-all-spill`, and `-ra`.
      All seven fixtures retain their ROM sizes, frame sizes, spill/reload counts,
      register-copy counts, and cumulative planned workspace sizes. All 21
      non-RA ROM pairs and six of seven RA ROM pairs are byte-identical. The core
      liveness RA ROM changes its unsafe B retention to stack access and gains
      safe C assignment retention elsewhere, with no net size growth (1549 bytes).
      Real projects calculations-1 and calculations-2 remain byte-identical in
      all modes; their RA ROM sizes remain 23829 and 16381 bytes respectively.
- The final source passes strict ANSI C90 warning-as-error syntax checks,
      editor diagnostics, four focused exact-byte/trace fixtures (including the
      BC complement contract), and native MSVC Release build/staged hash checks.
      Sequential final GCC and native MSVC full suites both reported
      `DONE (173 tests)` on 2026-10-11. Valgrind, sanitizers, peak-memory profiling,
      and emulator execution/timing were not run; compile timing samples are not
      evidence of a performance gain.

#### Step 294 Completion: Complement And Arithmetic Loops

- Recognize `COMPLEMENT` arg1 reads in the existing consumer adapter and admit
      byte C operands in its target predicate. The byte complement emitter uses
      the existing `_materialize_to_a` helper; its duplicated source dispatch is
      merged without changing memory loads or traces. No new production helpers,
      APIs, or test files are added; production code is nine lines smaller.
- The existing loop fixture now pins a complement-produced byte latch and a
      subtraction-produced latch consumed by a complement header. Both retain
      C using the existing atomic loop transaction and exact linked-byte tests.
      The preceding compiler rejects the complement-header source during loop
      fact discovery because the consumer adapter returns an invalid operand.
      Plain ADD and subtraction loops were already supported and are not new
      optimization gains.
- Against the safe `-ra-all-spill` control, the two new tagged regions shrink
      from 69 to 41 and 76 to 48 bytes respectively, saving 28 bytes each. Both
      function frames stay at 7 bytes, with no inserted spill or reload. These
      measurements qualify the enabled forms, not a general loop cost model.
- Mixed complement/subtraction continue paths remain conservatively stack-backed
      by the existing topology analysis. Multiple-value, nested-loop, early-exit,
      alias/write, B/C-versus-BC overlap, call, and inline-assembly policies are
      not widened. Existing core regressions retain their rejection/rollback
      checks; this completion does not enable arbitrary multi-latch producers.
- Eight focused fixtures pass, including the complement-location/chain/BC tests
      and the core loop/CFG harness. Identical preceding/current corpus sources
      produce all 28 byte-identical ROM pairs across seven fixtures/four modes;
      the expanded loop fixture also preserves all three non-RA ROMs. The real
      calculations projects have no new size gain or regression.
- Strict ANSI C90 checks, editor diagnostics, and native MSVC build/staged hash
      checks pass. Final sequential GCC and native MSVC suites both reported
      `DONE (173 tests)` on 2026-10-11. Valgrind, sanitizers, peak-memory profiling,
      and emulator execution/timing were not run.

### Batch 295-297: Strengthen Correctness And Qualification

- [x] **295. Add a reusable debug allocation verifier.** Validate assigned
      reads against reaching definitions, overlapping live register units,
      spill/reload widths and offsets, and target clobbers. Run it after TAC
      mutation/reanalysis and exercise failure/rollback paths. Reuse existing
      descriptors and validation APIs instead of duplicating their rules.
- [x] **296. Add execution-based differential regressions.** Generate bounded,
      deterministic arithmetic/branch/loop/pointer cases and compare observable
      results for `-ra` versus all-spill/default modes in a proven Z80 emulator.
      Keep failing seeds and minimize cases into existing fixtures. Exact-ROM
      tests remain valuable but do not replace execution-based correctness.
- [x] **297. Qualify real-program gains and stop unprofitable expansion.**
      Reuse the baseline corpus and add supported pointer-heavy workloads only
      where coverage is missing. Run both full integration suites and relevant
      sanitizer checks with freshly staged binaries. Report ROM/frame/cycle
      gains alongside compile-time and source-size costs; reject unexplained
      regressions and keep default/`-no-ra` compatibility intact.

### Step 295 Completed: Verifier Integration And Byte-Clobber Repairs

- Final debug verification rebuilds control flow and reuses liveness storage
      to replay reaching register owners. Reload/spill preflight uses the
      existing emission planners; assigned BC producers reuse the existing
      HL-preservation proof. Mutation-only diagnostics remain at the mutation
      call site rather than counting final verification as a mutation.
- Verification exposed unsafe nearer-candidate shortcuts: nested arithmetic
      overwrote an outer sum retained in A, and an intervening array read
      overwrote a retained BC byte. Byte-register shortcuts now require target
      transparency at both the intervening producer and its consumer. The
      existing regressions assert safe C/A arithmetic or stack-backed array
      values instead of preserving the corrupting instruction sequences.
- Strict ANSI C90 compilation, editor diagnostics, staged-binary comparisons,
      and both sequential full suites pass on 2026-10-11: GCC and native MSVC
      each report `DONE (173 tests)`. The verifier adds 194 production lines
      within existing functions; no new production helper or API was added.
- A scratch `z80-emulator@2.3.0` execution check runs the exact tagged fixture
      bytes with a fixed frame pointer and a 500-instruction limit. All 4,096
      arithmetic and 1,024 array-read seeds match their mathematical results;
      the preceding corrupting byte sequences fail all corresponding seeds.
      This checks those regions, not complete ROM execution or differential
      default/all-spill/RA programs. The deprecated package is installed only
      in scratch space, not added as a repository dependency.
- The completed verifier rejects negative, fractional, overflowing, or
      nonfinite temp indexes before converting them to integers. It checks
      byte/word widths and signed-frame spill/reload ranges before liveness
      reads metadata. Owner propagation has a checked, monotone bit-count
      convergence limit. Writes and reloads invalidate overlapping physical
      units through the existing target descriptors.
- `allocation_verify_test.c` in the existing liveness fixture invokes the
      actual static verifier, not an extracted implementation. Its five valid
      and fourteen invalid cases cover reaching definitions, BC/C overlap,
      inline-asm clobbers, malformed indexes/storage, transfer metadata, joins,
      and loops. Each call checks that TACs and temp metadata remain unchanged,
      including failure. Existing transaction rejection/rollback harnesses
      remain part of the full suite; verification itself does not mutate TACs.
- The completed production diff is 263 added / 21 removed lines, net 242,
      rather than the initial net 194 above. Existing liveness storage and
      target/transfer planners are reused; there is no new production function,
      API, or parallel allocator implementation. This correctness work adds
      code rather than claiming a net source-size improvement.

### Step 296 Completed: Reproducible Differential Execution

- `ci/check_register_allocator.py` builds the verifier harness and generates
      eight deterministic variants each of byte/word arithmetic, branches,
      bounded loops, nested array reads, pointer updates, and calls. It compiles
      and links actual default, `-no-ra`, all-spill, and RA ROMs, resolves linker
      symbols, and executes complete functions. The current runner uses Python
      and the native `z80==1.2.0` emulator; the historical measurements below
      used `z80-emulator@2.3.0`.
- Default/all-spill/RA results and updated buffer contents must match independent
      mathematical expectations. Default/`-no-ra` ROMs must match byte-for-byte.
      Execution also checks allowed memory writes, DE/SP restoration, and an
      instruction limit. A failure retains generated source, mode logs/ROMs,
      function, seed, parameters, and expected values in the output directory.
      The demonstrated byte-clobber cases are pinned in existing fixtures;
      no further compiler failing seed was found in this generated workload.
- The emulator stays an external, pinned test dependency, installed with
      Python 3.10+ and `python -m pip install -r ci/requirements.txt`. No Node.js,
      JavaScript emulator, `RA_Z80_EMULATOR` path or vendored emulator is needed.
      `SAMESAMECC`, `SAMESAMECL`, and `CC` optionally select tools.
      `RA_VERIFIER` reuses a prebuilt harness,
      including one built with native ASan. `RA_EXECUTION_DIR` selects retained
      artifacts and `RA_CASES` selects 1..4096 seeds per generated function
      (default 64). For the local Cygwin tools in PowerShell:

```powershell
$env:PATH = "C:\cygwin64\bin;$PWD\binaries;$PWD\wla-dx\binaries;$env:PATH"
$env:RA_CASES = "512"
python -m pip install -r ci\requirements.txt
python ci\check_register_allocator.py
```

- Cygwin GCC and the isolated native ASan compiler each pass 28,672 executions
      per mode (56 functions x 512 seeds), with identical instruction/cycle
      totals. Default/all-spill use 1,361,400 instructions / 19,696,232 cycles;
      RA uses 1,164,792 / 14,854,856, about 24.6% fewer cycles. This is the
      generated workload's RA-versus-all-spill result, not a new batch gain or
      a measurement of real-project runtime. The earlier 5,120 tagged-region
      controls are separate evidence and are not included in these counts.

### Step 297 Completed: Local Qualification And Costs

- The existing seven-fixture/four-mode corpus compares the current compiler
      with HEAD's pass 5 linked against identical surrounding compiler objects.
      All 21 non-RA ROM pairs and six RA pairs are identical. Only the repaired
      equal-next-use RA fixture changes: 313 to 312 used ROM bytes, six to five
      register copies, with frame sizes 34 total / 17 maximum unchanged.
      All seven default/`-no-ra` pairs are identical.
- Both calculations programs keep their RA ROM/frame totals: 23,829/277 and
      16,381/255 bytes. Relative to their all-spill builds, the existing gains
      are 1,575 and 1,129 ROM bytes and six and four total frame bytes. This
      batch does not claim a new real-program size or runtime improvement;
      pointer-heavy coverage already exists in the corpus and execution runner.
- Single-run aggregate RA compile time is 2.758 to 2.891 seconds (about +4.8%,
      noisy). Cumulative workspace-plan trace bytes are 7,243,804 to 13,532,108
      because final debug reanalysis plans storage again. This is cumulative
      allocation planning, not peak or retained memory. No wider candidate
      expansion is justified by these qualification results.
- Fresh Cygwin GCC passes strict C90 compilation and `DONE (173 tests)` on
      2026-10-12; staged compiler hash is `cc1c5152f42b9e4c...`. The sequential
      native MSVC suite also passes `DONE (173 tests)`, with staged compiler
      `147d1fb3ed54fa18...` and linker `c328e6a365ca39c2...` hashes matching
      their release outputs. Editor diagnostics and the owned diff check pass.
- Local Cygwin cannot link ASan/UBSan (`-lasan`/`-lubsan` unavailable). The
      installed native MSVC ASan compiler and verifier are built only in scratch
      directories without changing pinned project settings or normal binaries.
      Its compiler passes the generated four-mode builds/execution check, and
      its verifier passes all nineteen cases without an ASan report. UBSan,
      leak checking, peak-memory profiling, and real-project cycle measurement
      were not available or performed. No WSL tooling is used for these gates.
- This completes bounded local qualification, not exhaustive certification.
      Keep broader sanitizer/fuzz coverage and real-project runtime measurement
      as explicit follow-up work; do not claim new gains from unchanged ROMs.

### Batch 298-300: Consolidation, Structural Coverage, And Measurements

- [x] **298. Reduce final-verifier overhead without weakening verification.**
      Reuse the stable CFG and owner buffers, avoid a second liveness solve,
      and retain target-policy, malformed-input, and immutable-state checks.
- [x] **299. Expand execution coverage with structural workloads.** Add live
      values across calls, bounded pointer walks, nested array loops, and
      branch-dependent writes to the existing differential runner. Minimize
      demonstrated failures into existing exact-byte fixtures.
- [x] **300. Measure actual-program cycles and compiler resource costs.**
      Execute bounded pure routines from both calculations ROMs and extend
      the existing corpus script with repeated timing and actual peak RSS.
- [x] Final integration gate: freshly staged Cygwin GCC and native MSVC each
      report `DONE (173 tests)` on 2026-10-13. Staged outputs match their
      builds; compiler hashes are `f03aaeac2505acb9...` and `87e90d44a3f16cc3...`.

### Step 298 Completed: Reuse Final Analysis

- The final owner replay uses two existing buffers instead of four and skips
      the ordinary liveness fixed point and its duplicate debug traversal.
      Initial and post-mutation liveness solves remain unchanged. Redundant
      operand-type storage is removed in favor of normalized temp indexes.
- The final verifier reuses the last stable CFG when the subsequent allocation
      stages change only operand/register metadata. A changed TAC count still
      triggers reconstruction. This is a property of the current final stages,
      not a general guarantee that arbitrary same-count CFG rewrites are safe;
      a future structural rewrite must explicitly invalidate that analysis.
- All nineteen actual-verifier cases still accept/reject as expected and leave
      TACs and temp metadata unchanged. Existing CFG, liveness, rollback, and
      mutation tests retain their semantic assertions; only measured redundant
      analysis counts are reduced. This consolidation removes four net lines
      from pass 5 and adds no production helper or API.

### Step 299 Completed: Structural Execution And Compare Repair

- The existing runner now generates 88 functions: eight variants across eleven
      families. The four added families exercise several live values across a
      call, strided pointer read/modify/write loops, nested bounded array loops,
      and branch joins around a call and pointer writes. The buffer grows from
      32 to 128 bytes; every case still checks its entire contents, allowed
      writes, return value, frame/stack restoration, and instruction bounds.
- The independent mixed-width oracle follows verified compiler semantics:
      two byte operands wrap before assignment to a word local. A retained
      failing join/call seed then exposed a real backend bug: a byte condition
      held in a reused word temp resolved to HL but the eight-bit compare read
      `(IX+0)`. Comparisons now consume retained HL/BC low bytes, reusing the
      existing materializer rather than adding another helper.
- The existing `allocator_ra_compare_c_relational` fixture gains a minimized
      mixed-width case and exact bytes requiring `LD A,L`. In the liveness
      fixture, only six linked jump-address bytes change because another
      repaired compare shortens an untagged function by two bytes. No byte
      controls or verifier checks are removed.
- Current Cygwin GCC and native MSVC ASan each pass 45,056 generated executions
      per mode (88 functions x 512 seeds), with identical totals. Default and
      all-spill use 6,835,704 instructions / 97,395,368 cycles; RA uses
      6,299,640 / 82,408,712. The 15.4% cycle difference describes this expanded
      workload, not a new optimization gain or an exhaustive fuzz campaign.
      Default/`-no-ra` ROMs remain identical. Artifacts and failing seeds use
      the existing output directory and failure-report mechanism.

### Step 300 Completed: Real Routines, Repeated Timing, And Peak RSS

- The same runner compiles all five source units of each calculations project
      in all four modes, then executes seven actual bank-zero pure routines:
      add, multiply/add, XOR round trip, quotient/remainder round trip, product
      restore, shift restore, and partition restore. Inputs keep divisors
      nonzero and restoration cases within their nonoverflow bounds. Expected
      results are independent arithmetic, not results copied from another mode.
- Both current toolchains pass 3,584 real-routine executions per mode (seven
      routines x 512 seeds), with identical results, instructions, and cycles.
      Default/all-spill total 7,970,542 cycles and RA totals 7,503,598, about
      5.9% fewer cycles for these routines. This measures existing RA benefits,
      not a new batch speedup. It does not execute the whole programs, bank
      switching, interrupts, graphics, or SMS hardware.
- `RA_REAL_PROJECTS=0` skips only the real-project extension during a focused
      generated-workload check. By default it runs; the per-routine results
      are retained in `real-execution-summary.json`. Existing emulator/tool
      selectors and `RA_CASES` apply to both workloads.
- `ci/measure_register_allocator.sh` accepts `RA_BENCHMARK_REPEATS=1..31`
      (default one). `compile_seconds` is the median of per-build compiler-time
      sums; minimum, maximum, sample count, and per-invocation peak RSS are
      separate columns. Repeated linked ROMs must be identical. Set `RA_TIME`
      to GNU time to measure actual maximum resident memory; without it the
      peak column explicitly says `unavailable`, never workspace-plan bytes.
- Five samples per fixture/mode were collected against HEAD's pass 5/backend
      with identical surrounding sources, and against the current compiler.
      Local aggregate RA medians are 3.140 to 2.800 seconds (about -10.8%).
      Disabled-mode timings also improve, so ordering, host load, and process
      startup noise prevent attributing that entire difference to verification.
      Maximum observed RA peak RSS is 13,440 to 13,460 KiB, essentially flat.
      Cumulative planned workspace is 13,532,108 to 7,246,724 bytes (-46.4%);
      this last counter still is not peak or retained memory.
- The calculations RA ROM/frame totals remain 23,829/277 and 16,381/255 bytes.
      The liveness RA fixture changes from 1,549 to 1,547 used ROM bytes with
      frame sizes unchanged, due to the compare repair. Production source is
      one net line smaller across pass 5 and the backend; no new production
      function, API, allocator, or project dependency is introduced.
- Native ASan compiler and actual verifier are freshly rebuilt in scratch
      space. They pass all nineteen verifier cases, generated workloads, and
      real-project builds/execution without an ASan report. Strict C90 and
      editor diagnostics pass. Local GNU time 1.9 is also unpacked only in
      scratch space; no system installation or permission changes are required.
      UBSan, Valgrind, leak checking, upstream emulator tests, whole-program
      execution, and broader random-program certification remain unverified.

For repeated local corpus measurements with an available GNU time executable:

```bash
RA_TIME=/path/to/time RA_BENCHMARK_REPEATS=5 \
  RA_BENCHMARK_DIR=/tmp/ra-corpus ./ci/measure_register_allocator.sh
```

For each implementation batch, commit its code, focused regressions, and results
together without pushing. Prefer net consolidation; any new helper or API must
remove real duplication or enable a demonstrated optimization. Keep C90 and
target-policy validation gates. Leave a separate 6502 backend and explicit
inline-asm constraints postponed until Z80 quality gains justify broader scope.

### Next Quality Roadmap: Steps 301-315

These are optional quality improvements beyond the completed bounded allocator,
not a new definition of 100%. Checked items have completion evidence below;
remaining items are plans, not implemented capabilities. Work in the order
below, revising later scope when measurements disagree with the hypotheses.
The immediate goal is to make more useful values and operand combinations
allocatable, not to add another allocator framework. Preserve opt-in `-ra`,
default/`-no-ra` behavior, the existing ABI, and conservative unknown clobbers.

The pre-301 evidence favored eligibility work: retained calculations scans had
160 `unsupported_physical_register` skips versus 15 `clobber_between` skips.
These are diagnostic attempts, not unique missed values or runtime frequencies.
That baseline scan was block-local with narrowly patterned scalar promotion,
specialized multi-use preservation, and next-use arbitration with only a
bounded array-write transfer-cost preference. Do not infer expected speedups
from these counts or treat further policy extraction as an optimization gain.

### Batch 301-303: Expose And Remove Immediate Eligibility Limits

- [x] **301. Separate allocation, verification, and tracing.** Move semantic
      join/loop reconciliation out of diagnostic-only conditional compilation.
      Keep required validation independent of logging and provide a quiet path
      without simply disabling `DEBUG_PASS_5`. Reuse existing controls where
      practical. Add compact per-function summaries of skipped candidates by
      producer/consumer op, operand, width, register, and reason; distinguish
      repeated probes from unique opportunities. Quiet and traced builds must
      produce identical allocation decisions and ROMs, and verification must
      still reject the existing malformed cases. Measure trace bytes and
      compile time separately from generated-code quality.
- [x] **302. Broaden safe scalar-local promotion.** Extend the existing local
      promotion walk beyond byte additions of two byte arguments followed by
      one addition use. Start with single-definition, single-use byte/word
      locals in straight-line code, using supported arithmetic, bitwise, and
      copy producers/consumers. Preserve operand widths and extension rules.
      Exclude address-taken, escaping, static/global, inline-asm-visible, or
      otherwise externally observable storage and unsupported alias effects.
      Prove safety over the relevant lifetime rather than rejecting a function
      merely because it contains an unrelated call. Add positive and negative
      cases to existing fixtures and independent execution oracles; demonstrate
      removed memory traffic and a measured ROM or cycle benefit.
- [x] **303. Expand the highest-value operand/register combinations.** Use
      step 301 summaries to choose a common unsupported shape, checking the
      emitter as well as candidate policy. Prioritize useful HL/BC word paths
      and existing materializers before adding register classes. Consider
      commutative operand placement only where evaluation effects and clobbers
      permit it; do not commute subtraction or shifts. Keep width, overlap,
      producer/consumer clobber, and mixed-width low-byte checks explicit.
      Require both rejection tests and an execution-tested improvement over
      the preceding compiler; broad allowlist relaxation alone is not done.

**Implementation and results (2026-10-14):**

- Reconciliation, liveness, owner verification, and their shared helpers no
      longer depend on `DEBUG_PASS_5`. Existing `-q` suppresses routine allocator
      traces through one C90 tuple-argument macro, not a new printing function.
      Errors remain unconditional. Earlier compiler passes and compact final
      rejection summaries still produce output; this is not a silent compiler.
      Summaries group producer/consumer/operand/width/primary register/reason and
      count final producer/next-use scan decisions, not repeated register probes
      or dynamic frequencies. Missing candidates never reaching selection are not
      represented as unique optimization opportunities.
- Extended the existing scalar/pointer promotion walk, not a second pass, to
      supported single-definition/single-use byte/word arithmetic, bitwise, and
      copy forms. Calls outside a scalar's relevant lifetime no longer reject the
      whole function; direct call-argument references and lifetime crossings remain
      excluded, as do address-taken, repeated/redefined, static/global, and
      asm-observed values. Byte source widths are checked from declaration nodes
      before operand-type propagation. Word-to-byte and unproven byte-temp source
      shapes stay stack-backed after an execution control exposed lost truncation.
- Summaries identified the dominant word assignment/post-increment array-index
      shape, but its real intervening clobbers prevented a demonstrated gain; the
      trial allowance was removed. Instead, extended existing `SUB arg2` discovery
      and the BC predicate only for word `COMPLEMENT` producers, reusing the
      already-supported emitter. Non-complement/other-register cases remain
      rejected. Operand order is unchanged. Existing BC overlap/path checks keep
      the left operand in HL while producing the complemented right operand in BC.
- Reused two existing fixtures and the differential runner. It now runs the
      original 88-function cohort and a separate 42-function scalar/word cohort
      automatically, rather than overflowing bank zero with one large program.
      New controls cover signed byte/word values, widening, narrowing, call
      positions/arguments, and subtraction order. Per-function dynamic metrics
      distinguish eligibility gains from repeated test inputs. The original
      target-transparency golden required only changed frame displacements after
      promoting its single-use `held` local; opcode sequence and policy assertions
      are unchanged.
- Matched pre-batch compiler versus current compiler, same surrounding sources:
      scalar cohort RA cycles **9,062,912 -> 8,752,640** across 21,504 executions
      (3.4% fewer), instructions **698,368 -> 689,152**. Byte XOR-local examples
      use **230 -> 200 cycles/invocation**; the complemented-word subtraction uses
      **457 -> 305**, independently executed with full return/buffer/ABI checks.
      These are targeted workload gains, not whole-program speedups. The original
      generated cohort and seven existing real-routine totals are unchanged.
- Nine-fixture matched corpus: **33/36 raw ROM pairs identical**, including
      all 27 default/`-no-ra`/all-spill controls. RA changes: calculations-1
      **23,829 -> 23,805 ROM bytes**, aggregate frames **277 -> 273**; expanded
      scalar fixture **1,041 -> 1,029**, frames **134 -> 131**; expanded BC fixture
      **440 -> 416**, frames **50 -> 46**. Other corpus ROMs/frames unchanged.
      Three build samples per mode with actual GNU-time RSS: aggregate traced-RA
      median compile-time sums **2.900 -> 3.140 s**, flags-off **0.990 -> 0.990 s**;
      maximum peak RSS **13,436 -> 13,456 KiB**. Timing runs were baseline-first
      and overlapped qualification, so this is a cost observation, not a precise
      performance estimate. Do not hide the traced compile-time increase.
- Alternating three current-compiler traced/quiet samples on the original
      generated source: median **1.460 -> 0.185 s**, logs **7,265,370 -> 783,243
      bytes**, with matching generated assembly/ROMs and mandatory verification.
      Traced range **1.454-1.499 s**, quiet **0.183-0.191 s**. This measures logging
      cost separately from allocation quality, not a default-mode optimization.
- No new production functions or files. Consolidation removes duplicate
      declarations, debug compilation guards, and an unreachable path-state copy.
      Four touched production files are **net -3 nonblank lines**, **net +60 total
      lines** including spacing; the trace macro/flag are the only new shared
      surface. Preserve the existing header's unrelated line-ending change outside
      this batch's staged declarations. `-ra` remains opt-in.
- Strict C90, focused fixtures, actual immutable verifier (5 accepted and 14
      rejected cases in both trace modes), quiet/traced generated assembly/ROM
      identity, and independent execution passed. Both Cygwin and native-ASan
      compilers completed **70,144 executions per executed mode**: 45,056 original
      generated, 21,504 scalar/word, and 3,584 real-routine executions. Toolchain
      summaries match exactly; instrumented compiler/verifier runtime dependencies
      were checked and no ASan report occurred. Counts include repeated input
      patterns and do not claim unique shapes or exhaustive coverage. The full
      Cygwin and native Windows suites passed **173 tests each**, run sequentially.
- UBSan, Valgrind/leak checking, upstream emulator tests, and full SMS/hardware
      execution were not run. Arbitrary multi-use retention, narrowing promotion,
      graph-wide lifetimes, and profitable post-increment indexes remain future
      work; steps 304-315 remain unchecked.

### Batch 304-306: Retain Reused Values And Price Allocation Decisions

- [x] **304. Support general same-block multi-use retention.** Extend existing
      candidate discovery and interval state so a value can remain resident
      through several reads until its last read, redefinition, or actual
      clobber. Start with stable-width, unambiguous definitions and straight-line
      locals/temps; do not confuse a read-modify-write with another read of the
      old value. Keep all consumers and overlapping physical units consistent.
      Remove the need for immediate spill backing when no later path needs it,
      while retaining fail-closed handling for unsupported cases. Test repeated
      arithmetic, array-base use, pressure, and intervening clobbers against
      independent results and memory-access counts.
- [x] **305. Generalize preservation and reload placement only where needed.**
      Reuse the existing split-spill/reload planners and atomic application for
      more than the specialized word-HL array-base case. Track whether backing
      storage already holds the current value; avoid duplicate stores/reloads,
      store before a destructive use when necessary, and reload at the first
      actual demand. Cover byte/word widths, BC/B/C overlap, redefinitions,
      joins, and failed-plan rollback. Do not enable arbitrary cross-block
      retention before all-path safety is proven. Show reduced executed
      transfers, not just additional retained-value diagnostics.
- [x] **306. Add a bounded target-aware profitability model.** Extend existing
      arbitration rather than building a second selector. Compare avoided
      stack traffic with producer/consumer transfers, spill/reload costs, and
      rematerialization in both encoded bytes and Z80 cycles. State the chosen
      objective and use conservative loop weights where known; label estimated
      frequency separately from measured execution. Keep legality independent
      of profitability, retain deterministic ties, and use next use as a
      fallback when costs are unknown. Validate both beneficial and deliberately
      rejected choices, including pressure and cases where nearer use is more
      expensive; retain only measured improvements without unexplained corpus
      regressions.

**Implementation and measured results (2026-10-15):**

- Extended the existing promotion, candidate, path, interval-marking, and owner
      verification code for stable-width same-block repeated reads. Discovery
      checks reads before writes so a later read-modify-write still consumes the
      old definition; that write then ends its lifetime. Every supported read
      receives the physical annotation, including non-destructive byte C and
      word BC arithmetic reads. Complete single-definition coverage can remove
      backing; compacted names with other definitions may retain their slot.
      Ambiguous double-operand reads, unsupported widths, clobbers, escaping
      locals, and unproven cross-block lifetimes remain conservative.
- Reused the split/reload planners, transactional callbacks, target validation,
      and existing array-base transfer helper for scalar arg1/arg2 byte/word
      preservation. Backing is written before destructive use and reloaded on
      demand. A same-block resident reload is reused until a real clobber,
      redefinition, or boundary. Failed-tail mutation remains atomic; displaced
      arg2 cleanup now clears its matching preservation marker. Pointer-base
      transfers stay two bytes regardless of array element width. Cached IX
      addressing and existing byte-store primitives avoid constructing a second
      raw spill TAC and redundantly restoring the frame base.
- Profitability extends the existing transition observer and planner, not a
      second selector. The objective is positive estimated cycle savings with
      no estimated byte growth. A 256-instruction scan compares indexed-stack
      payloads, physical transfers, preservation/reload, and the existing stable
      global-address rematerialization. Frequency is explicitly estimated at
      weight one, including loops; no trip count is invented. Unknown shapes,
      incomparable choices, and exact ties keep deterministic next-use behavior.
      These are bounded payload estimates, not exact final address-cache costs.
      Accepted byte/word split examples estimate 23/9 saved cycles and zero byte
      growth; measured savings are 52/38 cycles because cached address setup is
      cheaper. The short negative control estimates -7 cycles/-4 bytes and is
      rejected. A planner test keeps the farther reused value over nearer use.
- Extended existing fixtures and the differential runner, without new test
      files. The scalar cohort is now 54 functions/21 families; the original
      cohort remains 88 functions/11 families. Added repeated byte/word reads,
      definition-ending read-modify-write, byte/word clobber splits, and a
      deliberately rejected short split. The actual-code verifier accepts seven
      valid and rejects 16 malformed cases immutably in both trace modes, with
      separate invalid-tail rollback, displaced-arg2 cleanup, discovery, and
      pressure-cost checks. `RA_FAMILIES` selects matched subsets; explicit
      `RA_BASELINE=1` skips only new diagnostic expectations, never independent
      result, memory, ABI, or ROM-identity checks.
- Matched pre-batch compiler and final compiler on the same eight arithmetic
      helpers plus eight new byte-reuse/split functions, 512 seeds each:
      RA cycles **2,653,696 -> 2,500,096** (5.8% fewer), instructions unchanged at
      **202,752**, frame reads **57,344 -> 48,128**, frame writes
      **22,528 -> 21,504**, and used ROM **983 -> 943 bytes**. Frame counters
      include argument/return/ABI traffic, not only temp spills. Per invocation:
      repeated byte **249 -> 189 cycles**, byte split **421 -> 369**, word split
      **712 -> 674**; the rejected short split remains **383**. Byte split uses
      one preservation store, one demand reload, and three resident-reload
      reuses. All matched disabled ROMs are identical. New word-redefinition
      shapes are independently checked, not used as baseline speedup controls:
      the saved baseline rejects that cohort with
      `missing_or_clobbered_definition`; the final compiler executes it correctly.
- The established 88-function, 42-function scalar, and seven real-routine
      execution counters are unchanged from the preceding compiler. The matched
      nine-fixture corpus has **34/36 raw ROM pairs identical**, including all
      **27 default/`-no-ra`/all-spill controls**. The scalar fixture uses
      **1,029 -> 1,023 ROM bytes**, aggregate frames **131 -> 130**; the expanded
      BC fixture uses **472 -> 466 bytes**, frames unchanged at **57**. The latter
      includes the new definition-boundary regression, not an old-corpus
      performance claim. Calculations-1/2 and the other five fixtures have
      unchanged ROMs and frames.
- Three alternating baseline/candidate measurement rounds in isolated copies
      used the existing measurement script and actual GNU-time RSS. Aggregate
      traced-RA median compile-time sums are **2.960 -> 2.980 s**, ranges
      **2.880-3.010 -> 2.930-3.150 s**; flags-off medians **0.780 -> 0.860 s**.
      Maximum traced-RA peak RSS is **13,452 -> 13,468 KiB**. This is noisy
      local cost evidence, not a compile-time improvement. Resource samples
      preceded the final one-line preservation-marker cleanup; all 36 corpus
      ROMs were rebuilt and matched afterward. No new production
      functions, files, or public APIs: existing transfer/path/mutation code is
      reused. The three production sources are **net +296 total lines**,
      **net +295 nonblank lines**; this batch adds capability, not a net source
      reduction. Duplicate raw spill construction and unused scan bookkeeping
      were removed rather than adding emitter helpers.
- Strict C90, focused byte fixtures, traced/quiet full assembly and ROM
      identity, default/`-no-ra` identity, and independent execution passed.
      Final Cygwin and rebuilt native-ASan compiler/verifier runs each completed
      **76,288 executions per executed mode**: 45,056 original generated,
      27,648 scalar, and 3,584 real-routine cases. Counters match exactly across
      toolchains, with no ASan report. Repeated seeds/input patterns are not
      unique shapes or exhaustive coverage. Freshly staged, hash-compared
      frozen-source Cygwin and native Windows full suites passed **173 tests
      each**, run sequentially after the final preservation-marker fix.
- `-ra` remains opt-in and the ABI is unchanged. Arbitrary CFG-wide retention,
      full value identity, coalescing, and general spill-slot reuse remain later
      work. UBSan, Valgrind/leak checking, upstream emulator tests, and full SMS
      hardware/bank-switching execution were not run. Steps 307-315 are untouched.

### Batch 307-309: Separate Values From Storage And Remove Redundant Traffic

- [x] **307. Preserve definition-specific identity through allocation.**
      Separate logical value/live-range identity from compacted temp names and
      eventual stack slots. Reuse existing instruction/definition metadata
      where sound; examine the placement of `reuse_registers()` rather than
      adding parallel lifetime bookkeeping. Preserve byte/word semantics when
      the same compacted name has differently typed definitions, and keep
      mutation identities valid when TAC indexes move. Full SSA is not required.
      First require output-preserving behavior on supported paths and verifier
      coverage for redefinitions, width changes, joins, loops, and stale plans
      before enabling broader optimization based on the new identity.
- [x] **308. Coalesce copies and broaden safe rematerialization.** Use actual
      value lifetimes and register constraints to remove avoidable copies and
      reloads. Extend existing constant/address rematerialization rather than
      duplicating it; compare recomputation with transport using step 306.
      Recompute only side-effect-free values whose inputs remain valid, never
      an aliased memory load or volatile/observable access. Preserve signed
      extension, partial-register overlap, and parallel-move ordering. Add
      controls where coalescing increases pressure or rematerialization costs
      more, and qualify net bytes/cycles rather than counting deleted moves.
- [x] **309. Reuse actual spill backing by proven lifetimes.** Assign/share
      spill slots after physical allocation using the lifetimes of values that
      truly need memory, including CFG live segments and reload demands.
      Existing virtual-temp reuse is not absent; improve or replace its storage
      role rather than layering another unrelated compactor over it. Keep
      escaping/address-taken/asm-visible locals separate, respect widths and
      frame-offset limits, and preserve all planned transfer offsets. Require
      verifier and execution coverage for branches, loops, calls, and overlapping
      byte/word spills, with a demonstrated frame-size reduction and no
      unexplained instruction-cost increase.

**Implementation and measured results (2026-10-15):**

- Enabled Z80 allocation now bypasses early `reuse_registers()`, keeping logical
      temp names distinct until allocation and final storage layout. Existing
      definition-scoped discovery, original-register annotations, mutation
      identities, and name compression remain in use; no second identity table
      or SSA framework was added. Default, `-no-ra`, forced all-spill, and other
      backends retain their old reuse path. Read-modify-write definitions still
      need the existing definition-aware proofs, not just a unique temp name.
- Precise byte/word metadata lets existing retention and assignment emission
      remove intermediate word copies. Word arithmetic now reuses the existing
      BC operand materializer instead of two duplicated memory/extension blocks.
      This also fixes a newly exposed retained byte call result in A being read
      as an IX stack location when widened. Signed extension and BC/B/C overlap
      use the existing materializer rules. Copy removal is local, not a general
      parallel-copy or CFG-wide coalescer.
- Extended existing stable-global-array address rematerialization to nonzero,
      integral, nonnegative in-bounds constant indices, including word-element
      scaling. The existing transition cost observer uses the same eligibility
      bounds and immediate-address cost. Mutable pointer loads, variable indices,
      and out-of-bounds indices are not substituted. New byte/word controls each
      save 28 executed cycles and two frame reads per invocation; the mutable
      pointer control explicitly requires no rematerialization. Existing pressure
      and deliberately unprofitable split controls remain active.
- Actual backing reuse is in the existing final slot-assignment function, after
      physical allocation. It rebuilds CFG/liveness with existing helpers and
      bounds each single-definition temp's complete lifetime, extending bounds
      through live-in/live-out blocks and backedges. Disjoint call-related memory
      lifetimes can share; genuine live-across-call overlaps cannot. Every owner
      of a proposed slot is checked, including inclusive instruction endpoints,
      and a narrower slot cannot hold a later word. Locals remain separately
      allocated. Multiple writes and unsupported hard boundaries conservatively
      prevent sharing. Functions with absolute spill-transfer TACs retain their
      planned offsets wholesale; this is not general lifetime-hole packing.
- Reused the actual-code verifier fixture for eight storage controls: disjoint
      word/byte reuse, overlap, loop/backedge overlap, redefinition, call overlap,
      insufficient width, forced all-spill compatibility, and absolute-transfer
      preservation. Every case requires unchanged TAC bytes. The existing seven
      accepted and 16 immutable rejected allocation cases, traced/quiet modes,
      stale-plan and atomic-tail tests remain. New executed scalar coverage adds
      signed call widening, sequential spill demand, byte/word constant-address
      rematerialization, and a mutable-pointer negative: 64 scalar functions in
      26 families, alongside the original 88 functions and seven real routines.
- Matched pre-batch and final compilers on identical generated sources, 512
      seeds per function. Original-cohort instructions **6,299,640 -> 6,009,848**,
      cycles **82,408,712 -> 78,272,776**, frame reads
      **1,770,368 -> 1,718,144**, and writes **931,840 -> 843,776**. Scalar
      instructions **1,121,280 -> 1,123,840**, cycles **14,274,048 -> 14,244,864**,
      reads **285,696 -> 281,600**, and writes unchanged at **154,624**.
      Combined generated cycles fall 4.3%, used ROM **17,606 -> 17,288 bytes**,
      aggregate frames **1,826 -> 1,682**. The sequential spill control demonstrates
      actual slot reuse and a **9 -> 8 byte** frame, not just virtual-name reuse.
- These gains are not uniform. Pressure cases cost **+15 cycles/invocation**
      after byte call-result materialization; signed-call widening costs
      **+34.5 mean cycles/invocation** using the existing preserving-A sign
      extension. Nested-loop and join/call cases save **4,033,536** and **163,840**
      cycles respectively. The seven executed real routines have unchanged
      instruction, cycle, and memory-access totals. Those pure bank-zero routines
      do not execute the real projects' hardware-facing main loops.
- The nine-fixture corpus has **28/36 raw ROM pairs identical**, including all
      **27 default/`-no-ra`/all-spill controls**. Calculations-1 ROM
      **23,805 -> 23,688 bytes**, aggregate frames **273 -> 291**, largest frame
      **101 -> 105**; Calculations-2 ROM **16,381 -> 16,299**, frames **255 -> 254**,
      largest **98 -> 97**. The remaining Calculations-1 frame increase reflects
      conservative separate backing for unshareable/redefined values; `init`
      grows **7 -> 13**, while the independently executed quotient routine drops
      **15 -> 13**. Read-reload and core-liveness fixtures save two ROM bytes each.
      Both byte-index/address fixtures grow nine ROM bytes while their aggregate
      frames shrink three: the now-byte index is conservatively stack-backed
      instead of using its former mixed-width word interval. Their word-index
      BC/HL overlap assertions remain. Corpus spill/reload marker counts do not
      increase. Exact temp, width, frame, byte, and analysis-count expectations
      were updated in existing fixtures, without new test files.
- Three alternating isolated baseline/candidate resource rounds give traced-RA
      median compile-time sums **3.560 -> 3.730 s**, ranges
      **3.020-3.590 -> 3.350-3.800 s**; flags-off medians **1.020 -> 1.060 s**.
      Maximum traced-RA peak RSS is **13,476 -> 13,492 KiB**, measured by GNU time.
      Concurrent matrix activity makes these noisy cost samples, not a compile
      speedup. The added final-layout liveness pass has a measurable cost.
      No new production functions, files, or public APIs: the two production
      files are **net +57 total lines**, **net +58 nonblank lines**. Reusing the
      operand materializer removes 48 duplicated emitter lines; the batch adds
      capability but is not a net production source reduction.
- Strict C90 and editor diagnostics pass. Final Cygwin and rebuilt native-ASan
      compiler/verifier runs each pass **81,408 executions per executed mode**:
      45,056 original, 32,768 scalar, and 3,584 real-routine cases. Aggregate and
      per-program execution counters match exactly across toolchains, with no
      ASan report. Generated traced/quiet full assembly and ROM identity, and
      all matched disabled-mode ROM controls, pass. Freshly staged, hash-compared
      final Cygwin and native Windows full suites each pass **173 tests**, run
      sequentially after the final call-lifetime refinement.
- `-ra` remains opt-in, the ABI is unchanged, and steps 310-315 are untouched.
      Repeated seeds are not unique shapes or exhaustive coverage. General SSA,
      arbitrary CFG retention/coalescing, and packing multiply-defined or fixed
      spill-transfer lifetimes remain unsupported. UBSan, Valgrind/leak checking,
      upstream emulator tests, and full SMS hardware/bank-switch execution were
      not run.

### Batch 310-312: Extend Allocation Across Useful Regions

- [x] **310. Use function-level live segments for region decisions.** Build on
      existing CFG/liveness and definition identity to represent lifetime holes
      and values crossing ordinary block boundaries. Start with unambiguous
      straight-line regions, then joins where all incoming states can be
      reconciled. Reuse current reservations and transaction machinery; do not
      retain across a boundary based only on linear TAC position or an
      unchanged instruction count. Explicitly invalidate analysis after
      structural mutations. Test edge-specific transfers, critical-edge
      placement, incompatible incoming locations, and all-path rollback before
      replacing conservative stack-backed decisions.
- [x] **311. Retain profitable loop-carried values under pressure.** Extend
      current loop support to multiple simultaneously live values and useful
      nested-loop regions, using estimated or measured hotness and actual
      register-unit conflicts. Account for entry, backedge, and exit transfers
      and resolve any parallel-copy cycles safely. Keep break, continue,
      multiple-latch, redefinition, and conflicting-inner-loop negatives.
      Require execution-tested removal of repeated loop-body memory traffic
      and report total dynamic cycles including setup/exit costs; an additional
      recognized loop shape alone is not an optimization result.
- [x] **312. Limit call-related spills to the values and regions needing them.**
      Preserve the existing ABI and caller-clobber rules. Split live ranges
      around calls and choose saves/reloads or rematerialization using proven
      value liveness and cost, rather than keeping an entire lifetime in memory.
      Only use narrower clobber summaries for callees with validated transitive
      effects; unknown/external calls and unconstrained inline asm remain hard
      barriers. Do not silently introduce callee-saved registers or change
      argument/return conventions. Test live values across nested calls,
      recursion where supported, pointer writes, and mixed-width returns, with
      frame/stack restoration and observable-memory checks.

                  Batch 310-312 implementation and qualification:

                  - Single-definition, non-escaping scalar locals now reach the existing
                        function CFG/liveness and split/reload graph across ordinary labels,
                        branches, and calls. This changes value eligibility, not boundary
                        ownership proofs. Existing all-path reservations, atomic transactions,
                        mutation-triggered rebuilds, block-exit spills, and final verification
                        still decide resident segments. A scalar passed directly as a call
                        argument, an address-taken local, unsupported narrowing, redefinition,
                        or an asm barrier remains excluded. Pointer promotion remains bounded
                        to its previous single-block/call-free rules.
                  - Existing loop selection can now put a second byte value in B: byte
                        SUB arg1 and comparison arg2 have explicit legality, a resident B
                        comparison operand needs no address/materialization round trip, and
                        RA-only literal comparisons use the existing immediate SUB emitter
                        instead of overwriting B. Default, `-no-ra`, and forced-all-spill
                        emission remain unchanged. Register-unit conflicts, path clobbers,
                        entry/backedge/exit reservations, and parallel-copy rejection still
                        use existing machinery; no new register, scheduler, or helper was added.
                  - Calls keep the full existing clobber set and ABI. Eligible scalar
                        values can have a resident prefix, one preservation store, and later
                        independently proven reload segments instead of an entirely
                        memory-resident lifetime. No callee summary, callee-saved convention,
                        or retention through an unknown call/asm was introduced.
                  - The existing execution runner now covers 88 original functions,
                        74 scalar functions in 31 families, and seven real bank-zero routines.
                        Five new scalar families cover a branch region (including a branch
                        pointer-write variant), nested signed-return/pointer-writing calls
                        with a bounded recursive-call variant,
                        two simultaneously updated loop-header values, calls inside a loop,
                        and address escape. Exact gates require one store/two reloads and
                        five/six reload reuses in the region/call controls, B/C loop ownership,
                        and no unsafe escape/narrowing/call-argument promotion or call-loop
                        retention. Existing join/critical-edge, rollback, nested-loop,
                        break/continue, multiple-latch, conflict, and redefinition controls
                        remain active. Unsupported critical edges and incompatible incoming
                        locations still fall back; arbitrary cross-label reload reuse was not
                        enabled. Recursive calls use the same full clobber rules, with a
                        scalar live across recursion checked against a closed-form oracle.
                  - Against saved pre-batch `f4f1e66`, matching sources at 512 seeds gave
                        86,528 executions per mode/toolchain, with independently checked
                        results, observable writes, bounds, and DE/SP restoration. Cygwin GCC
                        and native MSVC ASan produced identical counters and ROMs in every
                        mode. Aggregate RA cycles fell 114,616,610 -> 112,465,448 (1.88%),
                        frame reads 2,288,022 -> 2,220,274, and writes 1,206,546 -> 1,174,720;
                        no generated function regressed in cycles. Including setup/exit, the
                        region control fell 709,000 -> 625,032 cycles, nested-call region
                        3,056,640 -> 2,955,264, and pressure loop 6,203,700 -> 4,975,674.
                        That loop's reads fell 69,878 -> 24,658 and writes 67,830 -> 45,220.
                        The call-loop negative retained no loop value and kept the same
                        memory traffic; its smaller compares alone saved 18,432 cycles.
                  - Combined generated ROM fell 19,394 -> 19,172 bytes; frames
                        1,837 -> 1,821. Register copies rose 626 -> 636, preservation stores
                        10 -> 14, and reloads 8 -> 16. These extra bounded transfers replace
                        repeated memory reads, not a claim of transfer-free allocation.
                        Calculations-1 ROM fell 23,688 -> 23,675 and calculations-2
                        16,299 -> 16,288; frame totals/maxima stayed 291/105 and 254/97,
                        with unchanged copy counts and no explicit inserted spill/reload.
                        Baseline/current default, `-no-ra`, and all-spill ROMs matched for
                        both generated cohorts and both real projects; trace/quiet matched.
                  - Three alternating two-project measurement rounds, concurrent with
                        other validation, gave summed RA compiler-time medians
                        2.08 -> 2.10 seconds (ranges 2.01-2.18 and 2.04-2.18), measured peak
                        RSS 13,260 -> 13,264 KiB. Disabled-mode times were noisier. No compiler
                        time or memory improvement is claimed; cumulative workspace-plan
                        counts are not peak memory. Production growth is 14 lines across
                        two existing files, with no new functions, APIs, or test files.
                  - Strict C90/warnings and editor diagnostics passed. Fresh staged
                        Cygwin/native binaries were compared before tests. Native compiler
                        and actual-source verifier ASan runs reported no errors; the seven
                        accepted/16 immutable rejected cases and eight spill-layout controls
                        passed in traced and quiet modes. The final sequential Cygwin and
                        native Windows full suites each passed all 173 tests. Only tagged
                        byte expectations and measured trace/ownership assertions changed
                        in existing fixtures; executable fixture sources were preserved.
                        UBSan, Valgrind/leak checking, exhaustive CFG coverage, upstream
                        emulator tests, and SMS hardware/bank switching remain unrun.

### Batch 313-315: Close Coverage Gaps And Qualify The Result

- [x] **313. Systematically check policy/emitter/verifier agreement.** Extend
      existing tests to enumerate supported operation/operand/width/register
      combinations, physical-unit overlap, and implicit clobbers. Verify actual
      emitted behavior independently, not only descriptor agreement; a reaching
      owner proof cannot detect an emitter reading the wrong location. Track
      definition identity through transfers and mutations. Include signed
      extension, low-byte uses of word locations, and rejection of unsupported
      combinations. This is broader coverage, not permission to postpone such
      checks for combinations introduced in earlier batches.
- [x] **314. Expand structural differential generation and failure reduction.**
      Reuse `ci/check_register_allocator.py` and its independent oracles. Vary
      program structure, live-value pressure, aliasing within valid buffers,
      signed/mixed widths, branch paths, nested loops, and call placement rather
      than merely increasing seeds or constants. Avoid undefined inputs and
      report unique shapes/input tuples separately from execution counts.
      Retain and minimize failing sources/seeds into existing fixtures. Include
      representative pointer/loop routines from real projects and state hardware
      or emulator limits explicitly; preserve pinned/reproducible tooling.
- [x] **315. Requalify gains, costs, and remaining limits.** Reuse the existing
      corpus and real-routine execution tools with matched surrounding sources
      and freshly staged Cygwin/MSVC binaries. Interleave or alternate baseline
      and candidate timing runs, report dispersion and actual peak RSS, and
      retain disabled-mode negative controls. Report ROM, frames, executed
      transfers, dynamic cycles, compile time, and net production source/API
      changes. Run both full suites, the allocation verifier, differential
      execution, strict C90, and available sanitizers; name unrun checks.
      Keep only explained tradeoffs and demonstrated benefits. Record which
      original eligibility limits remain and decide the next work from that
      evidence, without making `-ra` unconditional or claiming exhaustive
      certification.

                  Qualification for 313-315 (bounded, not exhaustive certification):

                  - The existing actual-source verifier harness enumerates 10,200 endpoint
                        combinations: 34 operation/operand forms, 14 producer kinds, signed and
                        unsigned byte/word widths, constant versus temporary assignment sources,
                        and A/HL/BC/B/C. An explicit expected table checks legal and rejected
                        endpoints independently of the target callbacks. All 25 physical-unit
                        pairs are checked against independent masks. Actual reaching-owner tests
                        now include all 15 call/return-value-call/asm register clobbers and a
                        word-ALU scratch-clobber rejection: seven accepted and 32 rejected cases
                        remain immutable in traced and quiet modes, alongside eight spill-layout
                        controls and the existing definition, mutation/rollback, and cost checks.
                        Endpoint acceptance is not a proof that intervening instructions preserve
                        a value, nor does this table independently execute every matrix cell.

                  - Independent execution exposed and fixed two allocator-only failures.
                        A retained signed-byte ALU left operand was read from IX+0 instead of its
                        physical register; the existing HL materializer and sign-extension helper
                        now handle it, preserving an A right operand when necessary. General
                        BC-producing word operations were incorrectly treated as preserving HL;
                        the shared allocator/verifier exception now permits only the existing
                        direct-to-BC complement producer. The retained minimal generated cases
                        are `matrix_extend` and `matrix_shift`, with the latter also checked by
                        an immutable actual-source verifier rejection. The pre-fix reduced runs
                        returned 32894 instead of 32784 for (0,17,32767), and 24 instead of 65528
                        for the word shift. Production growth is ten net lines, with no new
                        production functions, APIs, files, ABI changes, or weakened call/asm masks.

                  - The existing runner adds a separate 68-function matrix cohort to avoid
                        overflowing bank zero: both ALU operand orders, all six guarded unsigned
                        comparison forms in both orders, signed equality and extension, low-byte
                        word-result use, computed shift counts, and aliasing/nonaliasing pointer
                        writes with branch, nested-loop, pressure, and call-placement variations.
                        The original/scalar/matrix cohorts contain 88/74/68 functions and
                        11/37/57 literal-normalized source shapes, respectively. At 512 seeds,
                        they cover 26,624/25,600/24,576 distinct full shape/input tuples within
                        each cohort, versus 45,056/37,888/34,816 executions per mode. Repeated
                        arithmetic helper controls are intentional; these are not counts of
                        globally distinct CFGs. All five retained register classes are exercised
                        across the cohorts. Nine real bank-zero routines include both projects'
                        actual pointer-writing number formatters, with an independent digit
                        oracle checking the entire output buffer and index-pointer update.
                        Software shift/division loops execute; hardware-facing source loops and
                        bank switching are not qualified. Node 22.18.0 and pinned z80-emulator
                        2.3.0 were used for that qualification. The current Python runner
                        instead pins its native emulator in `ci/requirements.txt`.

                  - Failures retain their source, exact seed/input, assembly, ROM, and logs.
                        The existing catch path dependency-slices generated functions and replays
                        default and failing modes to establish whether the reduction reproduces
                        the failure. The real widening failure reduced from 16 generated
                        functions to one plus the shared preamble, with default passing and RA
                        failing. This is function/dependency reduction, not a general statement
                        reducer or input minimizer. Three separate default-mode limitations were
                        retained rather than fixed or silently counted as passes: a word-to-byte
                        copy chain loses truncation (expected 33021, got 32765); signed byte
                        ordering for (184,75,97) returns 75 instead of 184; and unsigned `>`
                        equality for (0,68,68) returns 0 instead of 68. Passing narrowing cases
                        use an arithmetic result, signed comparisons cover equality/inequality,
                        and generated unsigned ordering explicitly guards equality. These are
                        deliberate qualification boundaries, not claims of general C correctness.

                  - Fresh staged Cygwin GCC, native MSVC Release, and native MSVC ASan runs
                        each passed 122,368 executions per mode over the three cohorts and nine
                        real routines. Exact per-function counters and all 25 ROM controls agree
                        across toolchains; traced/quiet ROM and assembly agree. The pre-batch
                        compiler's previously passing cohorts and all nine real routines have
                        identical outputs and counters; its new-matrix RA failures are not a
                        valid performance baseline. Default, -no-ra, and forced-all-spill matrix
                        ROMs also match the pre-batch compiler. Both sequential full suites
                        passed all 173 tests, strict ANSI C90/-Wall/-Werror and editor checks
                        passed, and ASan reported no compiler/verifier failures. Final artifacts
                        are `/tmp/ra315-final-{baseline,cygwin,native-asan,native-release}`; saved
                        pre-batch compilers are under `/tmp/ra315-baseline`.

                  - Across the final expanded workload, default/RA dynamic cycles are
                        206,226,888/161,671,072 (21.61% fewer with RA), frame reads
                        4,378,892/3,226,322, and frame writes 2,800,242/1,665,216. These transfers
                        include ABI and argument/result traffic, not just explicit allocation
                        spills. The three generated ROM totals are 35,066/28,394 bytes, frames
                        2,952/2,661 bytes, and static register copies 536/923. RA has 14 explicit
                        preservation stores and 16 reloads versus zero in default mode. These
                        are aggregate opt-in/default comparisons, not new speedups from this
                        correctness batch. Previously passing generated ROMs and dynamic counts
                        are unchanged. Both real pointer routines use 2,601,356 versus 2,864,764
                        cycles per 512 calls. Real project RA ROMs remain 23,675/16,288 bytes,
                        frame totals/maxima 291/105 and 254/97, and copies 689/595, with no
                        explicit inserted spills/reloads. Default project ROMs are 24,468/17,510;
                        the first project's default frame total/max is smaller (283/101).

                  - Three alternating baseline/current two-project GNU-time rounds, isolated
                        from the full suites' fixture directories, measured RA compiler seconds
                        [2.27,2.11,2.10]/[2.01,2.16,2.22], medians 2.11/2.16, with actual peak
                        RSS 13,260/13,264 KiB. Other validation was concurrent, so these noisy
                        samples support no compile-time or memory improvement claim. Current
                        default median was 0.75 seconds and peak RSS 13,004 KiB: RA still costs
                        substantially more compiler time. Workspace-plan bytes are not peak RSS.

                  - Original eligibility limits remain: opt-in RA only, nonescaping supported
                        scalar definitions, single-block pointer promotion, stack ABI arguments,
                        full call/asm barriers, conservative incompatible joins/critical edges,
                        and profitable/proven atomic transfers only. No arbitrary SSA, exhaustive
                        CFG/endpoint execution, or general cross-label reuse is claimed. UBSan,
                        Valgrind/leaks, upstream emulator tests, and SMS hardware remain unrun.
                        Evidence favors addressing the retained default-mode conversion and
                        comparison failures in a separately scoped follow-up before widening
                        eligibility or considering unconditional RA. No later batch is started.

### Follow-On Quality Roadmap: Steps 316-339

The following batches track follow-on work. Completed steps are checked only
after their qualification gates pass. They improve correctness,
allocation quality, compile cost, and maintainability beyond the completed
bounded Z80 milestone; they do not reopen that milestone or make `-ra`
unconditional. Do not begin with a full SSA rewrite, graph-coloring replacement,
new target, or further generic extraction without a demonstrated need. Reuse
the existing allocator, materializers, verifier, fixtures, and execution runner.

Steps 316-318 address three independently reproduced default-mode wrong-code
cases, not allocator-only failures. Their intended semantic/output corrections
must be isolated and documented. Outside those demonstrated corrections,
preserve default/`-no-ra`/forced-all-spill compatibility. Record fresh matched
baselines after the corrections; do not count their effects as allocator gains.
Every later optimization needs a before/after case with independent execution,
and every consolidation needs output/allocation equivalence. Static rejection
counts identify audit targets, not dynamic savings or guaranteed opportunities.

### Batch 316-318: Correct The Known Baseline Semantics

- [x] **316. Preserve truncation through word-to-byte copy chains.** Retain the
      reduced `u16 saved=~a; u8 narrowed=saved; return (b+c)+narrowed;`
      failure in existing tests, including the case expected to return 33021
      rather than 32765. Fix the owning conversion/copy path instead of hiding
      it through allocator eligibility. Cover byte boundaries, signedness,
      narrowing followed by widening, and both ALU operand positions. Execute
      default, forced-all-spill, and RA against an independent typed oracle;
      require `-no-ra` agreement with corrected default behavior.
- [x] **317. Correct signed-byte relational ordering.** Retain the unguarded
      `s8 saved=a^b; if(saved<c) return a; return b;` reproduction, whose
      byte inputs (184,75,97) should return 184 rather than 75. Fix the actual
      signed comparison lowering and cover all six comparison forms, equality,
      both operand orders, constants/registers/memory, and -128/-1/0/127.
      Keep signed ordering cases in the executable matrix rather than limiting
      qualification to equality or adding source guards around failures.
- [x] **318. Correct unsigned relational equality boundaries.** Retain the
      unguarded `u8 saved=a^b; if(saved>c) return a; return b;` reproduction
      for (0,68,68), expected 68 rather than 0. Check all six comparison forms,
      both orders, equality and adjacent values at byte/word boundaries. Remove
      the matrix's equality guards only after independent execution passes in
      every relevant mode. Requalify the three semantic fixes together and
      establish a corrected baseline before widening allocation eligibility.

                  Qualification for 316-318 (baseline semantics, not allocation gains):

                  - Saved pre-edit compilers reproduce all three retained default-mode
                        failures: narrowing returns 32765 instead of 33021; signed ordering
                        returns 75 instead of 184 for (184,75,97); unsigned byte `>` returns
                        0 instead of 68 for (0,68,68). An unguarded word equality counterpart
                        also returns 0 instead of 65467. Existing failure reduction retains
                        the exact sources/inputs under `/tmp/ra318-before-*`.
                  - `_optimize_il_23` no longer substitutes an untruncated word source for
                        an assignment that narrows to a byte. The existing assignment and
                        extension emitters perform the conversion; no new pass or allocator
                        eligibility exception was added. Tests cover all four signed/unsigned
                        word-to-byte combinations, widening signed bytes through s16, both
                        ALU operand positions, and boundary pairs. Byte relational emission
                        now uses sign XOR overflow for signed ordering; strict unsigned `>`
                        excludes equality in both byte and word emission. Full call/asm masks,
                        ABI, register eligibility, and opt-in behavior remain unchanged.
                  - The existing runner removes unsigned equality guards and executes all
                        six signed/unsigned byte and unsigned word comparisons in both orders.
                        Two small additional cohorts reuse the same generator/oracle for
                        computed-value/literal and memory-value/literal forms. Signed negative
                        one uses `(0-1)`: unary minus in that expression position is rejected
                        by the existing parser, which was not changed. The first 65 seeds
                        retain exact failures and an eight-by-eight boundary grid; later seeds
                        retain varied full-range inputs. No new test file or dependency was
                        added. Eight existing byte tags were refreshed only for the added
                        equality-skipping jump and resulting branch-address relocations;
                        executable fixture sources and allocation assertions are unchanged.
                  - Fresh Cygwin GCC, native MSVC Release, and native MSVC ASan compilers
                        each pass 176,640 executions per mode at 512 seeds: 88 original,
                        74 scalar, 86 matrix, 44 computed-literal, 44 memory-literal functions,
                        and nine real routines. The five generated cohorts have 11/37/75/37/37
                        normalized shapes and 26,624/25,600/32,792/15,896/15,896 unique full
                        shape/input tuples within their cohorts. These are not globally unique
                        CFG counts. Actual-source verifier checks remain seven accepted and
                        32 immutable rejected cases, eight layout controls, 10,200 endpoint
                        combinations, and 25 physical-unit pairs, in traced and quiet modes.
                        All 35 ROM pairs and generated/real execution counters match between
                        Cygwin and each native toolchain. Default/`-no-ra` ROMs and traced/quiet
                        RA ROM/assembly agree. ASan reports no compiler/verifier errors.
                  - Both sequential authoritative full suites pass all 173 tests, with
                        fresh hash-matched staged binaries. Strict C90/-Wall/-Werror and editor
                        diagnostics pass. The original/scalar generated sources, ten mode ROMs,
                        and all original/scalar/real dynamic counters match pre-batch artifacts.
                        Both real projects' ROM, frame, and static-copy totals are unchanged;
                        the first project's existing RA frame regression is not repaired here.
                  - The corrected expanded workload totals default/all-spill/RA cycles
                        225,581,587/225,689,107/176,419,171, frame reads
                        4,663,612/4,665,660/3,411,698, and frame writes
                        3,022,948/3,024,996/1,769,660. Frame traffic includes ABI transfers.
                        These establish a new matched correctness baseline, not a speedup
                        introduced by this batch; the expanded matrix is not comparable with
                        the guarded pre-batch workload. Final execution artifacts are under
                        `/tmp/ra318-final-{cygwin,native-release,native-asan}`; corrected Cygwin
                        and native Release baseline binaries are retained in those final dirs.
                  - Production growth is 25 net lines in two existing files, with no new
                        production functions, APIs, files, or abstractions. Existing optimizer,
                        type-priority, jump, and XOR helpers are reused. All 19 protected dirty
                        tracked-file hashes remain unchanged. No compile-time/peak-memory
                        improvement is claimed; controlled resource profiling, UBSan,
                        Valgrind/leaks, exhaustive emitter/CFG execution, upstream emulator
                        tests, and SMS hardware/bank switching remain unrun. Signed word
                        relational ordering is not certified by this batch. No later step
                        is started and `-ra` remains opt-in.

### Batch 319-321: Align Allocation With Actual Lowering Effects

- [x] **319. Describe selected-lowering register effects.** Extend existing
      descriptors/policy data to express actual reads, writes, scratch units,
      and clobbers for supported operand locations and widths. Distinguish
      direct-to-BC complement from word ALU paths that compute through HL;
      include BC/B/C overlap and preservation needed by extension/index code.
      Use those facts in candidate/path decisions without weakening full
      call/asm barriers. Avoid a second parallel target-policy framework.
- [x] **320. Consolidate typed operand materialization.** Reuse the existing
      A/HL/BC/index materializers across duplicated emitter paths, making
      sign extension, truncation, scratch use, and preservation explicit in
      their current contracts. Remove superseded location handling as each
      path migrates. Preserve unsupported-combination failures and require
      matching assembly/ROMs for behavior-preserving migrations; separately
      qualify any intentional instruction improvement with actual execution.
- [x] **321. Independently test lowering/effect agreement.** Extend the
      existing harness/runner with live sentinel values across each newly
      described lowering variant, including overlapping byte/word units,
      destructive operands, signed widening, and low-byte word uses. Compare
      actual execution with independent expectations, not only allocator and
      verifier callbacks that share the same assumptions. Retain immutable
      rejection and atomic rollback tests for illegal effects/locations.

                  Qualification for 319-321 (output-preserving consolidation):

                  - The existing Z80 transparency predicate now combines physical read,
                        write, and scratch units instead of separate B/C/BC branches. Typed
                        byte/word rules preserve read-only BC and reject overlapping writes.
                        Direct word complement into BC has A scratch but no HL write; word
                        ALU paths remain scratch-HL barriers. The verifier uses this selected
                        effect directly instead of rediscovering a competing candidate.
                        Other paths remain conservative; full call/asm masks, ABI, and
                        candidate endpoints are unchanged. This is not a complete model of
                        flags or every IX/IY scratch instruction.
                  - Existing HL materialization merges identical A/B/C zero-extension
                        cases. Complement-to-BC and word-shift memory loads reuse the typed
                        BC/index materializer, preserving extension, preservation mode,
                        instructions, and unsupported-type failures. Production shrinks by
                        48 lines across two existing files; no functions, APIs, abstractions,
                        dependencies, production files, or byte-tag changes were added.
                  - The actual-source harness adds 150 independent selected-effect checks,
                        including BC/B/C writes, read-only BC, direct complement, scratch-HL
                        ALU, and full barriers. Existing seven accepted/32 immutable rejected
                        verifier cases, rollback tests, eight storage controls, 10,200 endpoint
                        combinations, and 25 register-unit pairs still pass. Ten matrix
                        sentinel variants independently execute live words across complement,
                        signed widening, truncation, destructive shifts, and signed memory/
                        computed indices with a live byte and complete buffer expectations.
                  - Saved pre-batch and current Cygwin compilers, native MSVC Release,
                        and native MSVC ASan each pass 181,760 cases per executed mode at
                        512 seeds. Cohorts contain 88/74/96/44/44 functions with 11/37/85/37/37
                        normalized shapes, plus nine real routines. All 35 ROM pairs and
                        per-function execution/coverage summaries match baseline versus
                        current and Cygwin versus each native build. All 50 generated
                        ROM/assembly controls and 1,173 generated allocation/transfer
                        decision entries match baseline exactly. Real-project ROMs match;
                        randomized internal labels prevent literal real assembly identity.
                        Default/`-no-ra` and traced/quiet identities pass; no executed-cycle,
                        frame-traffic, or real-project frame regression was introduced or
                        repaired. No speedup or compile-time/peak-memory benefit is claimed.
                  - Fresh binaries were staged and SHA-matched; strict C90/-Wall/-Werror,
                        editor diagnostics, and current-source compiler/verifier ASan checks
                        pass. Both sequential authoritative full suites pass all 173 tests.
                        Artifacts: `/tmp/ra321-final-{baseline,cygwin,native-release,native-asan}`;
                        full-suite logs: `/tmp/ra321-{cygwin,native}-full.log`. All 19 protected
                        dirty tracked-file hashes are unchanged. UBSan, Valgrind/leaks,
                        exhaustive CFG/emitter coverage, upstream emulator tests, and SMS
                        hardware/bank switching remain unrun. `-ra` remains opt-in; no later
                        batch is started.

### Batch 322-324: Select Placements By Cost

- [x] **322. Rank all legal placements before committing a transition.**
      Replace first-eligible arbitration plus a later single-register cost
      override with comparison of the existing legal A/HL/BC/B/C alternatives.
      Include displaced-owner costs and the opportunity cost of BC occupying
      both B and C. Keep qualification read-only until the selected plan passes
      existing atomic preflight. Add a case where the first legal placement is
      inferior to another legal placement, with measured emitted benefit.
- [x] **323. Reduce copies through placement affinity.** Prefer compatible
      source/result locations when the source dies at the operation, and use
      legal commutative operand ordering to avoid unnecessary materialization.
      Price moves and scratch preservation rather than rewarding retention
      alone. Keep subtraction, comparisons, shifts, aliased operands, and
      BC/B/C overlap as explicit safety controls. Demonstrate fewer executed
      moves or cycles, not merely a lower static copy count.
- [x] **324. Make the cost model lowering-aware and loop-weighted.** Replace
      unexplained fixed transfer approximations with costs consistent with
      the selected emitter paths, including IX/IY setup, typed extension,
      preservation, reloads, and existing rematerialization. Weight uses and
      transfers by bounded loop-frequency estimates rather than uniformly by
      one; identify estimates and unknowns in diagnostics. Make the current
      256-instruction/unknown-cost fallback explicit and test it. Retain
      negative profitability controls and the cycles/no-byte-growth policy
      unless a separately explained objective change is approved.

                  Qualification for 322-324 (bounded cost-ranked placements):

                  - Existing policy alternatives A/C/B or HL/BC are qualified read-only
                        before atomic application, including alternatives hidden by initial
                        rejection. The existing transition observer prices the candidate
                        against displaced-owner reuse; copied slots exclude only the trial
                        owner, so other BC/B/C overlaps and reservations still reject. No
                        multi-owner eviction or new mutation protocol is introduced. Known
                        alternatives must improve cycles without worsening saved bytes;
                        unknown costs retain the established next-use placement fallback.
                  - Existing BC complement lowering now admits commutative left operands
                        and word array-read indices. Legal HL/BC index alternatives score
                        68/76 saved cycles; BC wins and independently executes two fewer
                        register copies, eight fewer cycles, and two fewer bytes per call.
                        Four left-complement arithmetic forms save 76 cycles, four executed
                        instructions and two frame reads/writes per call, and 12 ROM bytes
                        per function.
                        Reverse-order, simultaneous-complement, pointer-base, SUB, shift,
                        comparison, alias, call/asm, and partial-register controls remain.
                  - Named indexed/move/address/immediate costs replace numeric transfer
                        approximations; commutative materialization and HL-to-BC index
                        copies follow existing lowering. Signed-byte index preservation
                        charges its worst-case 11-cycle/three-byte difference. Existing
                        reload/rematerialization and spill setup are priced. Containing
                        backward CFG-edge spans estimate frequencies 1/4/16/64, capped at
                        64; cycles are weighted, static bytes are not. These are estimates,
                        not measured frequencies or natural-loop analysis. IX/IY cache
                        setup savings are conservatively excluded except for spill setup;
                        this is not instruction-exact cache or whole-program costing.
                        Producer/consumer/reload/bound unknowns are explicit. Actual-source
                        tests prove weights 4/64, static bytes, immutable TAC/slot inputs,
                        and unchanged next-use decisions beyond the 256-instruction bound.
                        The negative control still rejects byte growth (+5 cycles/-1 byte).
                  - Saved pre-batch/current Cygwin and current native Release/ASan each
                        pass 188,928 cases per executed mode at 512 seeds. Existing cohorts
                        contain 88/74/110/44/44 functions with 11/37/99/37/37 normalized
                        shapes plus nine real routines. All 21 saved/current flags-off ROM
                        controls and 70 Cygwin/native ROM comparisons pass, with identical
                        cross-toolchain per-function counters and traced/quiet identities.
                        Five generated routines and both real convertNumbersToChars16
                        routines improve; no executed function regresses. Aggregate RA
                        cycles fall 181,536,179 -> 181,097,907, instructions 14,467,127 ->
                        14,449,719, frame reads 3,503,346 -> 3,491,058 and writes 1,817,276
                        -> 1,804,988. Each real formatter saves 272 cycles and eight frame
                        reads/writes per call. Complete buffers and DE/SP remain checked.
                  - Matrix ROM payload falls by 50 bytes. Real-project RA ROM payloads
                        fall 23,675 -> 23,275 and 16,288 -> 16,128 bytes; frame totals/maxima
                        become 290/105 and 253/97 (previously 291/105 and 254/97). Static
                        register copies increase 689 -> 769 and 595 -> 627; smaller ROM
                        and faster executed routines do not imply fewer copies everywhere.
                        The first project's default frame remains smaller at 283/101.
                        Five-sample Cygwin RA compile medians increase 1.14 -> 1.41 seconds
                        (ranges 1.12-1.15 / 1.38-1.42) and 0.73 -> 0.87 (0.73-0.75 /
                        0.85-0.89); maximum RSS is 13,352 -> 13,356 and 13,220 -> 13,200 KiB.
                        Default medians are 0.29 -> 0.30 and 0.22 -> 0.22 seconds. Runs
                        are ordered baseline/current, with concurrent qualification;
                        no compiler-throughput or peak-memory improvement is claimed.
                  - Production adds 97 net lines in the existing pass, removes duplicate
                        transition-context initialization, and reuses preparation, scoring,
                        unit/path checks, and atomic application. No production functions,
                        public APIs, dependencies, new production/test files, or second cost
                        framework are added. Existing runner/harness additions total 52 net
                        lines. Three makefiles update only measured read-only probe counts;
                        actual transition/rollback checks remain. Transparency tag 02 is
                        measured at 106 versus 118 bytes, with its frame 21 -> 19 bytes.
                  - Strict C90/-Wall/-Werror, editor diagnostics, fresh current-source
                        compiler/verifier ASan, 150 effect checks, 10,200 endpoint cells,
                        25 unit pairs, seven accepted/32 immutable rejected verifier cases,
                        eight layout controls and rollback checks pass. Both sequential
                        full suites pass all 173 tests with fresh SHA-matched binaries.
                        All 19 protected user edits are preserved, excluding only the owned
                        diagnostic-count hunk in the already-dirty makefile from the hash
                        comparison; its unrelated blank-line deletion remains unstaged.
                        Artifacts: `/tmp/ra324-final-{baseline,cygwin,native-release,native-asan}`,
                        `/tmp/ra324-bench-{before,after}`, `/tmp/ra324-{cygwin,native}-full.log`.
                        UBSan, Valgrind/leaks, exhaustive CFG/emitter execution, upstream
                        emulator tests, signed-word relational ordering, and SMS hardware/
                        bank switching remain unqualified. `-ra` remains opt-in; no later
                        batch is started.

### Batch 325-327: Expand Evidence-Backed Value Coverage

- [x] **325. Audit assignments feeding array indexes first.** The retained
      two-project summaries contain 189 producer/next-use rejections: 174
      unsupported forms and 15 clobbers; 160 are assignment-to-array-index
      operands. Separate constants and copy chains better handled by existing
      propagation/rematerialization from values worth retaining. Enable only
      proven, profitable byte/word index forms, with signedness, index scaling,
      pointer-base conflicts, and bounded aliasing execution controls. Recount
      rejections and measure actual real-routine benefit; eliminating a static
      rejection is not itself completion.
- [x] **326. Broaden profitable word placements in BC.** Audit the current
      complement-only general BC producer rule against existing word emitters,
      especially ALU results consumed as right operands. Add supported forms
      only after their HL scratch and B/C interference are represented by the
      lowering-effect contract. Include SUB right operands, computed shift
      counts, simultaneous retained HL values, and displaced byte owners.
      Require a useful independently executed case for each expansion and
      retain rejection of incompatible live-register combinations.
- [x] **327. Promote more supported scalar uses.** Extend the existing
      single-definition, nonescaping scalar path to proven comparison, shift,
      index, and by-value call-argument uses currently excluded by its consumer
      allowlist. Materialize stack ABI arguments at the boundary; this does
      not permit registers to survive calls. Preserve address-taking/asm
      exclusions, dominance/definition identity, and exact conversion semantics.
      Keep multi-definition promotion and wider pointer scope deferred unless
      a measured case justifies a separately tested extension of existing
      reaching-definition machinery; do not silently relax those invariants.

                        Qualification (325-327): Reused the existing candidate policy,
                        effect contract, scalar promotion, type propagation, cost observer,
                        and execution runner. No new production functions, public APIs,
                        dependencies, or files. Consolidated three repeated pointer-base
                        rules; net production growth is 42 lines. Runner/harness growth is
                        145 lines, separately from fixture expectation/documentation edits.
                        - Assignment audit: the corrected, matched pre-batch summaries now
                        contain 77 rejections (62 unsupported, 15 clobbers), including 48
                        assignment-to-write-index snapshots, not the older 189/160 counts.
                        Constants/copy chains remain handled by existing propagation and
                        rematerialization. Only nonconstant, already-word assignment sources
                        may newly retain BC for read/write/address indexes; byte-to-word
                        conversions are not silently admitted. Word increments/decrements
                        by immediate one and word-to-word copies have their actual BC
                        effects represented. Current summaries have 18 rejections: one
                        unsupported, 15 clobbers, two negative-profitability decisions;
                        none are assignment-index rejections.
                        - BC expansion is limited to the five existing word ALU producers
                        feeding ALU right operands, including SUB. Their existing emitters
                        still clobber HL and overlapping B/C owners; no direct-BC ALU
                        lowering or simultaneous-live-HL exemption was invented. Charge
                        both actual HL-to-BC result moves, and reuse the existing proven
                        pending-BC-complement path in HL costing. Each new producer has
                        an independently executed SUB-right case saving 68 cycles and two
                        frame reads/writes per call. Computed shifts, simultaneous HL
                        values, displaced byte values, and rejected effects remain covered.
                        - Single-definition, nonescaping scalar promotion now accepts
                        comparison, shift, index, and by-value call uses. Same-width
                        declaration metadata preserves signed extension without widening
                        proven byte temporaries into unnecessary word slots. Call arguments
                        still materialize stack ABI slots, with full call/asm barriers;
                        call cases show no executed cycle gain. Definition/dominance,
                        address-taking, asm, multi-definition, and narrowing exclusions
                        remain; wider pointer/multi-definition promotion is not enabled.
                        - Six bounded cohorts execute 405 generated functions plus nine
                        original real routines at 512 seeds: 211,968 cases per executed
                        mode/toolchain. New matrix_values coverage uses the existing cohort
                        recursion because combining it with matrix exceeds bank-zero ROM.
                        Literal-normalized shapes per cohort are 11/37/99/37/37/38, not
                        global CFG counts. Independent full-buffer, DE/SP, signed/scaled
                        index, alias, conversion, call, escape, and interference controls
                        pass. Current Cygwin/native release/native ASan ROMs match in 80
                        comparisons; all 24 default/no-ra/all-spill saved/current images
                        match. Traced/quiet identity and per-routine counter identity pass.
                        - Matched RA totals: cycles 206345219 -> 204174339; instructions
                        16566193 -> 16468913; frame reads 3948274 -> 3888882 and writes
                        2071740 -> 2012348. Sixty generated routines improve, none regress.
                        This includes
                        the unchanged software prefix of calculateAndPrint3, stopping
                        before hardware I/O and calling the original formatter: 1824 fewer
                        cycles and 48 fewer frame reads/writes per call, checked against
                        an independent full-buffer text oracle. This is not full banked
                        calculateAndPrint3 or hardware qualification. The original nine
                        real-routine controls are unchanged.
                        - Real-project payloads 23275 -> 22649 and 16128 -> 16048 bytes;
                        frames total/max 290/105 -> 288/105 and 253/97 -> 253/97. Static
                        register copies rise 769 -> 779 and 627 -> 643, so no global copy
                        reduction is claimed. The first project's default frame 283/101
                        remains smaller. Two get-address fixture tags each shrink
                        five bytes through byte-index C retention; their source functions
                        and word-index byte tags are unchanged. No timing/RSS improvement
                        is claimed; this batch does not add a profiling framework.
                        - Strict C90/Werror and editor checks pass; current-source native
                        compiler/verifier ASan passes. The harness independently checks
                        350 lowering effects, 10200 endpoints (2718 supported), 25 unit
                        pairs, seven accepted/32 immutable rejections, eight layout and
                        atomic rollback controls. Both sequential full suites pass all
                        173 tests using fresh, hash-matched Cygwin and native binaries.
                        All 19 protected user edits remain unchanged. Artifacts:
                        `/tmp/ra327-final-{baseline,cygwin,native-release,native-asan}`,
                        `/tmp/ra327-{cygwin,native}-full.log`, and measured summaries.
                        A separate minimized default-mode branch-initializer dead-store
                        defect also reproduces with the saved pre-batch compiler; retained
                        under `/tmp/ra327-coverage-after/failure-reduced.ssc` and
                        `/tmp/ra327-baseline-bug`, outside this promotion work. UBSan,
                        Valgrind/leaks, exhaustive CFG/emitter coverage, signed-word
                        relational ordering, and SMS hardware/bank switching remain
                        unqualified. The allocator remains opt-in; no later batch started.

### Batch 328-330: Improve Loop Retention And Frame Storage

- [ ] **328. Price loop retention and split placement together.** Use existing
      CFG/loop and reservation machinery to compare retaining a value through
      a loop with storing/reloading or rematerializing at proven boundaries.
      Account for hot repeated uses, exits, calls, nested-loop pressure, and
      competing BC versus B/C ownership. Require actual loop execution with
      zero/one/multiple iterations and cold-path controls; avoid moving spill
      work into a hot loop or adding per-iteration copies for a static win.
- [x] **329. Diagnose and reduce the measured frame regression.** Trace the
      first calculations project's default/RA frame totals 283/291 and maxima
      101/105 to individual functions, promoted values, and storage lifetimes.
      Do not assume spill-slot reuse restrictions caused this regression.
      Remove demonstrated unnecessary slots/lifetime expansion while retaining
      ABI storage and valid preservation destinations. Report per-function
      maximum frame size alongside cycles/ROM; explain any retained tradeoff.
- [x] **330. Share spill storage using proven memory interference.** Improve
      existing CFG-adjusted enclosing lifetime bounds where mutually exclusive
      paths or dead segments demonstrably permit safe sharing. Address the
      current whole-function sharing opt-out when a REGISTER_SPILL TAC exists:
      preserve its absolute-offset contract, or safely resolve transfers after
      layout, before allowing reuse. Test branches, loops, calls, reused
      definitions, byte/word widths, and immutable failed layout validation.
      Require an actual frame reduction without added incorrect transfers.

      Qualification (328-330): The layout changes reuse the existing CFG,
      liveness, operand-read/write predicates, and slot allocator. No production
      functions, APIs, dependencies, files, or transfer instructions were added;
      production grows by 35 net lines. Existing enclosing bounds remain a fast
      conservative filter; overlapping bounds now require actual interference
      in a common block, including live-in/live-out and same-instruction uses.
      Repeated definitions may share only under the same proof. Existing width,
      unsupported-boundary, and initial-read exclusions remain in force.

      REGISTER_SPILL no longer disables sharing for its entire function. Layout
      through the deepest absolute spill destination remains unchanged, including
      every preceding slot; only the suffix can share. No transfer is retargeted.
      Invalid fixed destinations fail before modifying TACs, temporary metadata,
      or the layout offset. The existing actual-source harness now checks ten
      layouts (exclusive branches, interference, loops, calls, repeated
      definitions, byte/word widths, forced-all-spill, fixed transfers), a
      two-byte suffix reduction behind an unchanged fixed destination, and five
      immutable invalid-offset/type cases. The new sharing cases failed with
      the old restrictions before the corresponding production changes.

      The old first-project 283/291 frame comparison predates recent batches;
      the matched pre-batch baseline is default 283/101 and RA 288/105 total/max.
      Individual function frames below include ABI and local storage, not just
      temporary slots. Names omit randomized static-symbol suffixes.

      | calculations-1 function | Default | Before RA | After RA |
      | --- | ---: | ---: | ---: |
      | calculateAndPrint5 | 23 | 25 | 23 |
      | calculateAndPrint3 | 22 | 24 | 22 |
      | calculateAndPrint4 | 22 | 24 | 22 |
      | main | 3 | 3 | 3 |
      | init | 7 | 13 | 7 |
      | calculateAndPrintTrampoline (static) | 3 | 3 | 3 |
      | calculateAndPrint (static) | 101 | 105 | 102 |
      | convertNumbersToChars16 | 13 | 15 | 12 |
      | convertNumbersToChars16QUICK | 12 | 10 | 10 |
      | printCalculationLine | 19 | 20 | 19 |
      | calculationAdd3 | 15 | 11 | 11 |
      | calculationMultiplyAdd | 15 | 13 | 13 |
      | calculationXorRoundTrip | 13 | 9 | 9 |
      | calculationQuotientRoundTrip | 15 | 13 | 13 |
      | Total / maximum | 283 / 101 | 288 / 105 | 269 / 102 |

      In init, four disjoint two-byte comparison temporaries had separate slots
      solely because their boolean construction has multiple definitions. They
      now share one slot, explaining the six-byte reduction without changing
      the hardware-facing loop instructions. The maximal function reuses three
      unnecessary bytes, but its promoted byte zero (r106) remains live across
      scalar computations and still needs backing under this proof. Its maximum
      is therefore still one byte above default; no universal frame superiority
      is claimed. ABI storage and live preservation destinations are retained.

      calculations-2 RA total/max falls 253/97 -> 242/96 (default 259/98).
      Per-function changes: init 13 -> 7, convertNumbersToChars16 15 -> 12,
      printCalculationLine 20 -> 19, static calculateAndPrintComplex 97 -> 96;
      every other function is unchanged. Real payloads remain 22649 and 16048
      bytes; static copies remain 779 and 643. The nine executed original real
      routines and all 405 generated routines retain exactly the pre-batch
      instruction/cycle/frame-access counters: no cycle gain or regression and
      no added transfers. Generated RA cycles stay 191824381 over 207360 calls;
      including the nine real routines gives 211968 cases per mode/toolchain.
      Real frame reductions cover compiled whole projects, not execution of
      SMS hardware or bank-switched driver functions.

      Step 328 remains open after audit: full-loop retention uses the existing
      path/reservation-compatible selector, whereas ordinary split placement
      uses the bounded weighted observer. No joint retention/split/rematerialize
      cost policy was enabled or qualified here. Existing zero/one/multiple
      iteration, pressure, nested-loop, exit, branch and call controls execute
      unchanged; this is not evidence of a new joint-placement benefit. Adding
      a second cost model without such evidence would add unqualified code.

      Cygwin, native Release, and native ASan execute all 211968 cases per mode
      at 512 seeds with identical counters, 80 cross-toolchain ROM comparisons,
      and 24 unchanged pre-batch default/no-ra/all-spill ROMs. Strict C90/Werror,
      editor diagnostics, current-source ASan verifier and compiler checks pass.
      Existing 350 lowering-effect, 10200 endpoint, 25 overlap, seven accepted /
      32 immutable rejected allocation and rollback controls pass. Both sequential
      full suites pass all 173 tests using fresh, hash-matched Cygwin and native
      binaries.
      Artifacts: /tmp/ra330-final-{cygwin,native-release,
      native-asan}, /tmp/ra330-measured-results.json, /tmp/ra330-verifier.log and
      /tmp/ra330-{cygwin,native}-full.log. UBSan, Valgrind/leaks, upstream emulator
      tests, exhaustive CFGs, compile-time/RSS profiling, hardware/bank switching
      and signed-word relational qualification remain unrun. The pre-existing
      default conditional-initializer bug recorded in 325-327 is not changed.

### Batch 331-333: Reduce Compiler Work And Diagnostic Overhead

- [x] **331. Make quiet mode quiet and profile matched builds.** Successful
      allocator discovery/next-use, reload-site, loop-definition and lifecycle
      diagnostics now use existing trace control. Errors, rejected approval /
      reconciliation failures, incomplete coverage and intentional rejection
      summaries remain visible. Existing runner checks owned lifecycle output,
      identical rejection summaries, retained verifier failure diagnostics and
      literal same-basename traced/quiet assembly plus ROM identity. Other
      compiler debug output and pre-existing address/Z80 transport diagnostics
      in user-edited files are deliberately untouched; quiet logs are not empty.
- [ ] **332. Reuse per-definition use information within function bounds.**
      If profiling confirms repeated next-use/path scans are material, build
      or reuse ordered use/definition observations in the existing analysis
      storage instead of repeatedly scanning the remainder of global TACs.
      Preserve reads-before-writes, redefinition stops, all operand roles, and
      deterministic candidate ordering. Invalidate/rebuild observations after
      mutations using the existing rebuild boundary; retain allocation/ROM
      equivalence and demonstrate a compile-time benefit without excess memory.
      **Deferred after profiling:** ten matched instrumented matrix compilations
      per mode accumulated only 0.40 / 0.50 seconds of sampled CPU, with
      substantial instrumentation overhead. Active-instruction checks appeared
      in samples, but next-use/path scanning was not established as a material
      cost. No ordered-use cache, new storage or invalidation machinery added.
- [ ] **333. Consolidate redundant analysis and workspace work.** Use measured
      profiles to remove duplicate CFG/liveness reconstruction or temporary
      allocations when the analyzed TACs and metadata are unchanged. Reuse
      existing function-owned storage and explicit invalidation rather than
      global caches or another ownership abstraction. Keep post-mutation
      verification and fail-closed checks. If no material duplication is found,
      revise/defer this item explicitly instead of adding infrastructure to
      satisfy it; require measured time/RSS benefit for retained changes.
      **Deferred after measured trials:** a direct TAC-index lookup with generic
      fallback did not improve matched wall time and was removed. Sharing one
      instruction table between basic-block and CFG collection and reusing the
      existing rebuild owner removed two helpers, but 40 alternating quiet
      samples were slower: original median 119.06 -> 123.51 ms (ranges
      115.63-136.62 / 119.16-147.43), matrix 124.01 -> 129.27 ms
      (121.71-141.78 / 125.38-148.68). Peak RSS was effectively unchanged
      (13484 / 13484 and 12740 / 12736 KiB). Trial assembly matched, but the
      consolidation was removed rather than retaining an unproven optimization.

      Qualification for 331-333: twelve alternating, isolated, uninstrumented
      Cygwin samples compare saved 330 and logging-only compilers on identical
      original/matrix sources. Wall values below are process wall milliseconds,
      including launch overhead; peak RSS is GNU time %M, not cumulative planned
      allocations. No throughput claim is made for traced mode or either trial.

      | Source / Mode | Median Before -> After (Range Before / After), ms | Log Bytes Before -> After | Peak RSS Before -> After, KiB |
      | --- | --- | --- | --- |
      | Original quiet | 211.35 -> 118.19 (200.24-231.84 / 114.46-151.17) | 899285 -> 186137 | 13768 -> 13728 |
      | Matrix quiet | 224.13 -> 123.15 (221.35-301.30 / 119.43-156.75) | 1098777 -> 176831 | 12776 -> 12744 |
      | Original traced | 1827.65 -> 1833.33 (1804.06-1848.13 / 1797.33-1852.01) | 9874723 -> 9874723 | 13540 -> 13544 |
      | Matrix traced | 1929.45 -> 1936.44 (1910.71-2061.48 / 1914.76-1979.83) | 11220609 -> 11220609 | 12804 -> 12808 |

      All measured assembly outputs match. Actual quiet/traced profiles are
      retained separately; their instrumented timings are not throughput data.
      Final 512-seed Cygwin, native Release and current-source ASan execution
      runs pass 211968 cases per mode/toolchain (405 generated functions and
      nine original real routines). All 120 saved-330/current ROM comparisons,
      80 cross-toolchain comparisons, 18 complete same-toolchain allocator
      traces and execution counters match; RA remains
      16468913 instructions, 204174339 cycles, 3888882 frame reads and 2012348
      writes. Strict C90/Werror, editor diagnostics, native warning-free builds
      and actual-source traced/quiet verifier controls pass. Both sequential
      full suites pass all 173 tests using fresh, hash-matched Cygwin/native
      staged compilers; logs: /tmp/ra333-{cygwin,native}-full.log. Net production
      change is +5 lines; runner +12, functions/APIs/dependencies/files +0.
      No allocation, lowering, frame-layout or mutation policy changed.
      Artifacts: /tmp/ra333-profile/{quiet,trace}, /tmp/ra333-timing/samples.json,
      /tmp/ra333-consolidation-timing/samples.json,
      /tmp/ra333-final-{cygwin,native-release,native-asan} and
      /tmp/ra333-measured-results.json. UBSan, Valgrind/leaks, upstream emulator
      tests, exhaustive CFGs, hardware/bank switching and signed-word relational
      qualification remain unrun; the pre-existing default initializer bug is
      untouched. Step 328 remains open and 334+ is not started.

### Batch 334-336: Measure Quality Beyond Static Retention Counts

- [x] **334. Measure executed copies and transfer causes.** Extend the existing
      emulator runner to distinguish dynamic register moves, explicit allocator
      preservation/reloads, and ABI/argument/result frame traffic. Keep static
      ROM/copy counts separately labeled, and validate counter attribution on
      small known instruction sequences. Report per-routine results and outliers
      so aggregate cycle wins cannot hide copy-heavy or frame-heavy regressions.
- [x] **335. Expand structural coverage around the changed decisions.** Reuse
      existing generated cohorts and failure reduction for competing legal
      placements, copy affinity, live-range holes, loop pressure, conversions,
      call arguments, and overlapping units. Keep unguarded baseline regressions
      from 316-318 and both successful/rejected allocation controls. Vary CFGs
      and operand relationships, report unique shapes separately from seeds,
      and retain reproducible failures without claiming exhaustive coverage.
- [x] **336. Qualify representative real software loops.** Extend the current
      calculations-based execution corpus with actual source loops, pointer
      indexing, and call boundaries affected by these changes, using independent
      oracles and complete memory/DE/SP checks. Keep the existing real routines
      as controls. Separate source-loop coverage from emulator/library helper
      loops and unrolled formatters, and state bank-zero/software-only limits;
      do not imply SMS hardware or bank-switching certification.

      **334-336 implementation and qualification:** Reused the existing runner,
      WLA listing, emulator, generated families, independent oracles, reducer,
      and per-routine totals. The only production addition is two lines emitting
      a begin comment around existing allocator transfers; instruction bytes,
      allocation decisions, frame layout and mutation policy are unchanged.
      Runner net growth is 172 lines (205 added/33 removed), including shared
      metric lists replacing duplicated lists. No new production/test helper
      functions, public APIs, dependencies or tracked files were introduced.
      No allocation improvement or compile-time benefit is claimed for this
      measurement/coverage batch. Steps 328, 332 and 333 remain open; 337+ has
      not started.

      Executed register moves count unprefixed byte-register LD instructions,
      excluding immediate/memory/HALT forms, exchanges and indexed-register
      forms; this is not a universal logical-copy count. Frame byte accesses
      are partitioned exclusively into explicit allocator preserve/reload,
      ABI argument/result/frame, and remaining other_frame traffic, using
      existing TAC/emitter annotations. Unknown executed instructions fail
      generated qualification; all generated and nine real routines report
      zero unmapped instructions. Static padded ROM image bytes and register-LD
      sites are separate in static-summary.json, not dynamic counts or payload
      size. Passive listing-enabled reassembly must reproduce every ROM byte.
      Known controls check 7 instructions/49 cycles/2 register moves, indexed
      transfers at 10/149 with six exclusive byte causes, PUSH/POP at 3/31 with
      two ABI reads/writes, branch counting at 4/38, and ordered software port
      capture plus unexpected-port rejection. Actual byte split, word split
      and call-region controls require preserve/reload byte counts 1/1, 2/4
      and 1/2 per invocation respectively.

      Four additional matrix_structural variants combine zero-to-seven loop
      iterations, calls before/after branches, overlapping/disjoint pointer
      aliases, conditional word-to-byte truncation and swapped operand order.
      Previous unguarded correctness regressions and actual-source acceptance,
      immutable rejection, overlapping-unit and rollback controls remain.
      Shapes below mean normalized source, not distinct CFGs or an exhaustive
      search. Shape/input tuples are reported separately from execution seeds:

      | Cohort | Functions | Shapes | Shape/Input Tuples |
      | --- | ---: | ---: | ---: |
      | original | 88 | 11 | 26624 |
      | scalar | 74 | 37 | 25600 |
      | matrix | 114 | 103 | 45592 |
      | matrix_literals | 44 | 37 | 15896 |
      | matrix_memory | 44 | 37 | 15896 |
      | matrix_values | 47 | 40 | 17664 |

      Two additional extracted calculations-1 cases execute the original
      calculateAndPrint3 prefix (four formatter calls, pointer indexing and
      source for-loop), and the original calculateAndPrint5 pointer-while
      block with supplied start/end pointers. They do not execute the rest of
      either hardware-facing function. Independent formatting/buffer and
      ordered port-sequence oracles retain full 128-byte buffer, write guards,
      DE/SP restoration and instruction bounds; the while case spans 0..63
      bytes. Named TAC-CFG backedges in entry/callee sections are separated;
      anonymous arithmetic/formatter lowering loops are excluded. Both cases
      require exact expected entry iterations: 45824 backedges per mode at
      512 seeds. The nine original full-project real routines remain controls.
      Port writes are captured in software only; other routines still reject
      them. This is bank-zero software qualification, not SMS hardware, port
      timing, bank-switching or whole-application certification.

      Cygwin GCC, native MSVC Release and native MSVC ASan each pass 215040
      cases per mode at 512 seeds (411 generated cohort entries plus nine real
      routines). All 120 matched-333/current ROM pairs, 80 cross-toolchain ROM
      pairs and 18 complete same-toolchain allocator traces match. Original
      execution metrics match the expanded-source baseline; new metrics agree
      across all current toolchains. RA totals: 20900785 instructions,
      263026947 cycles, 5011186 frame reads, 2651836 writes, 1640646 register
      moves, 15360 preservation bytes and 15360 reload bytes. Default has
      330285515 cycles and 846888 register moves. Per-routine quality summaries
      flag all increases: 290 cohort/routine entries increase register moves;
      none increases summed cycles or frame traffic in these sampled cases.
      Largest copy increases are source_loop_0 (+81920 moves, -3918336 cycles)
      and scalar_loop_pressure_0/1 (+56550/+56500 moves). These are explained
      register-traffic tradeoffs, not a claim that every individual case wins.

      Strict C90/Werror, editor diagnostics, warning-free native builds and
      current-source ASan compiler/verifier controls pass (350 effects, 10200
      endpoints, 25 pairs, seven accepts/32 immutable rejects, ten layouts,
      five invalid fixed layouts and rollback). Both sequential full suites
      pass 173 tests with freshly staged/hash-matched compilers; logs are
      /tmp/ra336-{cygwin,native}-full.log. Evidence: /tmp/ra336-measured-results.json
      and /tmp/ra336-{final,baseline}-{cygwin,native-release,native-asan}, including
      per-routine quality, static/execution/coverage summaries and source maps.
      Baseline provenance was corrected before comparison: the saved
      ra333-final-native-*/baseline-samesamecc.exe copies are actually Cygwin;
      authentic native baselines are ra333-native-{release,asan}/bin/samesamecc.exe
      (SHA256 prefixes 2d4bfcf498ff/72588848e0a3). All final comparisons use the
      verified authentic outputs; old mislabeled copies were left untouched.
      UBSan, Valgrind/leaks, upstream emulator tests, exhaustive CFGs and
      signed-word relational qualification remain unrun; the pre-existing
      default conditional-initializer bug remains outside this batch.

### Batch 337-339: Consolidate And Requalify The Quality Work

- [x] **337. Remove superseded production machinery.** Review touched policy,
      cost, materialization, and orchestration paths for duplicated decisions,
      forwarding-only layers, and obsolete exceptions introduced or replaced
      by these batches. Consolidate within existing functions/modules when it
      removes real complexity; do not add phase-specific wrappers or extract
      abstractions solely for a future backend. Report net production lines,
      functions, public API, and test-code changes separately, with equivalence
      checks for output-preserving cleanup and no unrelated formatting churn.
- [x] **338. Requalify correctness across modes and toolchains.** Run focused
      actual-source verifier/layout tests, independent generated/real execution,
      strict C90/warnings, freshly staged Cygwin and native MSVC builds, both
      sequential full suites, and available sanitizer gates. Check corrected
      default/`-no-ra` agreement, forced-all-spill controls, traced/quiet
      identity, atomic failures, and cross-toolchain outputs. Name intentional
      semantic differences from the pre-316 baseline and every unrun check
      explicitly.
- [ ] **339. Publish measured quality/cost results and choose further work.**
      Compare each optimization with its corrected matched baseline, including
      per-function cycles, executed copies/transfers, ROM, total/maximum frames,
      rejection reasons, quiet/traced compile time, log volume, and actual peak
      RSS. Alternate isolated timing runs and report dispersion. Separate
      baseline correctness fixes, output-preserving cleanup, and allocation
      gains; retain only explained tradeoffs and demonstrated benefits. Decide
      from remaining real misses whether broader value representation/global
      allocation is warranted, rather than precommitting to a rewrite. Keep
      `-ra` opt-in and leave unprofitable or unproven work explicitly open.

      **337-339 implementation and measured results:** Consolidated identical
      address-materialization returns and first/better placement-winner updates
      inside the existing pass-5 functions. The first-candidate fallback,
      unknown-cost rejection/fallback, strict cycle improvement, no-byte-growth
      rule and active-owner checks are unchanged. Removed seven net production
      lines (five added/12 removed); functions/public APIs/dependencies +0.
      The existing measurement script adds seven net lines (14 added/seven
      removed), reusing its loops/AWK to include ra_quiet, per-sample log bytes
      and rejection-decision totals. No runner, fixture, helper or tracked file
      was added. This is output-preserving cleanup, not an allocation gain or
      a demonstrated compiler speed/RSS optimization.

      **338 qualification:** Strict C90/Werror and actual-source traced
      and quiet verifier/layout controls pass, including 350 lowering effects,
      10200 endpoint cells, 25 register-unit pairs, seven accepts/32 immutable
      rejects, ten layouts, five invalid fixed-layout cases and rollback.
      Focused address/split execution passes 1280 cases per mode; the full
      placement matrix passes 7296 at 64 seeds. Fresh Cygwin GCC, native MSVC
      Release and native MSVC ASan compiler/verifier builds each pass 215040
      cases per mode at 512 seeds (411 generated cohort entries plus nine real
      routines, including the two extracted source loops). All 120 matched-336
      ROM pairs, 80 cross-toolchain ROM pairs, 18 exact same-platform diagnostic
      streams and all per-routine counters, static/coverage/quality summaries
      match. Every current dynamic metric is identical across toolchains.
      Default/-no-ra and traced/quiet identities remain mandatory; forced
      all-spill and atomic-failure controls remain enabled. Both sequential
      full suites pass all 173 tests with freshly staged, hash-matched Cygwin
      and native binaries; all 19 protected user-file hashes are unchanged.

      Intentional semantic differences from pre-316 remain the independently
      tested word-to-byte truncation correction, signed-byte sign/overflow
      relational ordering and strict unsigned equality-boundary correction.
      These are baseline correctness repairs, not allocation wins; this batch
      changes none of them. UBSan, Valgrind/leaks, upstream emulator tests,
      exhaustive CFGs, signed-word relational qualification, SMS hardware/port
      timing and bank switching remain unrun. The pre-existing default-mode
      conditional-initializer bug remains outside this batch. `-ra` stays opt-in.

      **339 bounded report, remaining attribution open:** Reused the existing
      measurement script for eight isolated before/after rounds on both full
      calculations builds, reversing the version order on alternating rounds.
      Each mode/project has eight samples per version; compiler times below
      sum compiler invocations within the build, not linker/make wall time.
      Every sample produces the same ROM as its matched corrected-336 baseline.
      GNU time reports actual maximum child-compiler RSS; cumulative workspace
      trace fields are not peaks and quiet-mode zero trace activity does not
      mean zero allocation. Raw per-sample logs and dispersion are retained.

      | Project / Mode | Before Median Seconds (Range) | After Median Seconds (Range) | Before/After Median RSS KiB |
      | --- | --- | --- | --- |
      | calculations-1 / default | 0.345 (0.32..0.41) | 0.355 (0.32..0.37) | 12924/12924 |
      | calculations-1 / ra | 1.625 (1.57..1.95) | 1.690 (1.58..1.96) | 13200/13200 |
      | calculations-1 / ra_quiet | 0.240 (0.21..0.32) | 0.245 (0.22..0.29) | 13136/13136 |
      | calculations-2 / default | 0.250 (0.23..0.30) | 0.260 (0.23..0.33) | 12866/12872 |
      | calculations-2 / ra | 0.930 (0.91..1.25) | 0.950 (0.91..1.29) | 13056/13054 |
      | calculations-2 / ra_quiet | 0.190 (0.18..0.22) | 0.200 (0.19..0.23) | 13008/13012 |

      Timing ranges overlap; no speed or material RSS benefit is established.
      Current traced/quiet build-log median bytes are 8473138/337992 for
      calculations-1 and 4687288/283662 for calculations-2; before/after raw
      build-log sizes vary, while the generated allocator traces match exactly.
      Full no_ra/all_spill samples and all min/max values are in the artifact.
      Rejection reasons remain eight clobber, one unprofitable and one unsupported
      physical endpoint decisions in calculations-1; seven clobber and one
      unprofitable decisions in calculations-2. These are static decisions,
      not executed misses or evidence that rejected placements would win.

      | Project | Default/RA Used ROM Bytes | Default/RA Total Frame Bytes | Default/RA Maximum Frame Bytes | Default/RA Static Copies |
      | --- | ---: | ---: | ---: | ---: |
      | calculations-1 | 24468/22649 | 283/269 | 101/102 | 545/779 |
      | calculations-2 | 17510/16048 | 259/242 | 98/96 | 534/643 |

      These bundled RA/default values are unchanged by this cleanup. Static
      copies use the measurement script's existing register-LD definition,
      distinct from the runner's executed byte-register-LD counter. The larger
      calculations-1 maximum is in static calculateAndPrint in main.ssc; that
      hardware-facing routine is compiled, not independently executed here.
      Smaller total frames do not prove every routine's frame improves.

      Representative per-function results below sum 512 independently checked
      inputs. ROM is the WLA function-section size, not padded image bytes;
      frames come from the existing emitted prologues. Complete values for all
      420 cohort/routine entries, copies/transfers and rejection reasons are
      retained alongside baseline/current per-function static equality checks.

      | Routine | Default/RA Cycles | Default/RA Executed Moves | Default/RA Section Bytes | Default/RA Frame Bytes |
      | --- | ---: | ---: | ---: | ---: |
      | calculations-1/calculationAdd3 | 214528/136704 | 0/0 | 67/43 | 15/11 |
      | calculations-1/calculationMultiplyAdd | 766182/727270 | 0/0 | 86/74 | 15/13 |
      | calculations-1/convertNumbersToChars16 | 2864764/2462092 | 22016/28160 | 911/779 | 13/12 |
      | extracted source for-loop | 37481984/33563648 | 102400/184320 | 2252/1849 | 18/18 |
      | extracted pointer while-loop | 9873152/9324800 | 0/16128 | 126/121 | 9/9 |

      Aggregate RA remains 20900785 instructions, 263026947 cycles, 5011186
      frame reads, 2651836 writes, 1640646 register moves and 15360 bytes each
      of explicit preservation/reloads. The 290 move-heavy cohort/routine
      entries remain visible; none has increased summed cycles or frame
      traffic over default in this corpus, not a guarantee for every input.
      **339 historical batch attribution:** Rebuilt boundaries 318, 321, 324,
      327, 330, 333 and 339 with every compiler module changed between them
      (pass_5.c, pass_6_z80.c, register_allocator.c) taken from that commit.
      Unchanged common sources, including protected user edits, headers, tools
      and strict C90/Werror repository -g/-O0 flags are held fixed. Manifests
      record commits and source/compiler/runner/linker/verifier hashes. This
      attributes historical allocator/emitter changes against current common
      code, not exact historical releases or individual interacting features.
      An earlier pass-5-only pilot is not the whole-batch cost comparison.

      All seven builds pass the same current 420-entry independent corpus at
      512 seeds (215040 cases per mode), including full-buffer/DE/SP guards and
      both source loops. All 168 flags-off ROM comparisons pass; all 40 rebuilt
      current/qualified-current images and every current per-routine counter,
      section size and frame match. The current verifier is a separate control,
      not evidence that old pass-5 internals pass today's structural tests.

      Deltas below use identical inputs, not the smaller historical corpora.
      Frame accesses count executed byte reads plus writes, including ABI;
      ROM is whole-project used bytes. C1/C2 denote calculations-1/2.

      | Boundaries | RA Cycle Delta | Executed Move Delta | Frame Access Delta | ROM Delta C1/C2 | Frame Total/Max Delta C1; C2 |
      | --- | ---: | ---: | ---: | ---: | --- |
      | 318 -> 321 | 0 | 0 | 0 | 0/0 | 0/0; 0/0 |
      | 321 -> 324 | -1552384 | +39936 | -90112 | -400/-160 | -1/0; -1/0 |
      | 324 -> 327 | -3104768 | +21504 | -167936 | -626/-80 | -2/0; 0/0 |
      | 327 -> 330 | 0 | 0 | 0 | 0/0 | -19/-3; -11/-1 |
      | 330 -> 333 | 0 | 0 | 0 | 0/0 | 0/0; 0/0 |
      | 333 -> 339 | 0 | 0 | 0 | 0/0 | 0/0; 0/0 |

      Ranking and value coverage improve nine and 61 cohort/routine entries;
      none has increased summed cycles. More moves are an explained tradeoff,
      not a copy-reduction claim. Static project rejections C1/C2 fall from
      142/47 (318/321) to 62/15 (324) and 10/8 (327 onward); full reason records,
      per-routine counters/sections/frames and maximum-frame owners are retained.
      Missing old transfer-begin annotations prevent complete historical
      preservation/reload classification; total frame accesses remain measured.

      Eight isolated rounds reverse revision order on alternating rounds:
      560 full-build samples across seven versions, two projects and five modes.
      Every version/mode/project ROM is stable across repeats. Times are summed
      child-compiler seconds, median (min..max); they exclude make/linker time.

      | Boundary | C1 Quiet | C2 Quiet | C1 Traced | C2 Traced |
      | --- | ---: | ---: | ---: | ---: |
      | 318 | 0.310 (0.29..0.37) | 0.235 (0.23..0.30) | 1.215 (1.18..1.30) | 0.805 (0.76..0.97) |
      | 321 | 0.300 (0.29..0.32) | 0.225 (0.22..0.24) | 1.210 (1.18..1.29) | 0.795 (0.74..0.94) |
      | 324 | 0.330 (0.29..0.36) | 0.240 (0.23..0.28) | 1.495 (1.42..1.78) | 0.925 (0.89..1.23) |
      | 327 | 0.320 (0.30..0.36) | 0.260 (0.23..0.27) | 1.585 (1.54..2.07) | 0.920 (0.90..1.00) |
      | 330 | 0.330 (0.30..0.38) | 0.245 (0.23..0.30) | 1.595 (1.54..1.86) | 0.955 (0.90..1.17) |
      | 333 | 0.240 (0.23..0.25) | 0.195 (0.18..0.21) | 1.570 (1.52..1.78) | 0.925 (0.90..1.14) |
      | 339 | 0.250 (0.24..0.29) | 0.210 (0.19..0.29) | 1.615 (1.53..2.02) | 0.920 (0.89..1.19) |

      Logging-only 330 -> 333 reduces median quiet log bytes C1/C2 from
      1046494/645763 to 337992/283661, with disjoint quiet timing ranges.
      Quiet median peak RSS changes 13312/13168 -> 13288/13160 KiB; this is not
      a material memory improvement. Traced ranges overlap; no traced-throughput
      gain is established. Full RSS/log distributions and default/no-ra/all-spill
      samples are retained. Added ranking work increases traced compilation
      cost; smaller/faster target code is not a compiler-throughput benefit.
      No additional production/test code, helper, dependency or tracked file
      was added for this historical report. Only existing scripts were reused.

      Step 339 remains unchecked for individual-feature ablation within the
      interacting batches; historical native/ASan cost reruns and complete old
      transfer classification are not claimed. Current cross-toolchain/ASan
      qualification above is unchanged, not a new historical qualification.
      No broader value/global allocation rewrite is justified. Further work
      should first test the known maximum-frame outlier or a concrete clobber
      endpoint with an independent oracle and a measured tradeoff; unproven
      placement, caching and broader architecture changes stay deferred.
      Evidence: /tmp/ra339-measured-results.json, /tmp/ra339-benchmark/samples.json,
      /tmp/ra339-final-{cygwin,native-release,native-asan}, compiler-provenance.json,
      /tmp/ra339-{cygwin,native}-full.log, and
      /tmp/ra339-historical-all-modules-{results.json,builds,execution,timing}.

### Batch 340-342: Correct And Unify Allocation Decisions

These quality improvements extend the bounded working allocator, not its
completion definition. Keep 328, 332, 333 and 339 open until their own evidence
requirements are satisfied; the steps below do not complete them by association.
Prefer changes to existing decision paths over another allocator framework.

- [x] **340. Rank profitable candidates independently of the fallback.**
      First add actual-source regressions for a rejected first placement
      suppressing a profitable alternative, and an unknown-cost first placement
      blocking a known profitable alternative. Separate fallback selection from
      the best acceptable scored placement inside the existing ranking loop.
      Preserve deterministic ties, displaced-owner costs, BC/B/C overlap,
      atomic preflight and the cycles/no-byte-growth objective. State how
      acceptable nondominating scores compare; demonstrate emitted benefit,
      not just a different diagnostic or selected register.
      Completed in the existing ranking loop: reject KEEP_ACTIVE trials as
      scored winners, allow a known acceptable placement to replace an unknown
      fallback, and keep the first legal unknown trial when no preferred
      register exists. A rejected known first trial does not authorize later
      unknown alternatives. Equal or nondominating known scores keep the earlier
      choice; replacement requires strictly more saved cycles and no fewer
      saved bytes. The actual-source harness covers rejected, unknown and
      no-preference first placements without changing atomic application.
      Two independently executed source variants also exposed an existing
      optional split-reload collision: loading one input into HL overwrote a
      different retained HL input. Keep that read on the existing stack path
      when target register units overlap; reuse the existing eligibility hook
      and four actual-source HL/BC/B/C controls, not a verifier exemption.
      With this safety fix held constant, disabling only the ranking change
      costs each variant 4,096 cycles and 1,024 register moves over 512 inputs:
      the fix saves eight cycles and two moves per call, with unchanged frame
      reads/writes and a 16,384-byte matched ROM image. Static register-move
      sites fall from 316 to 312 across the matched scalar corpus. All other
      scalar counters and default/forced-spill counters remain identical.
- [ ] **341. Make frequency weighting change a justified decision.** Weight
      individual uses and preservation/reload/rematerialization operations by
      their execution regions rather than multiplying both competing totals
      by one common factor. Reuse the existing CFG/cost observer; distinguish
      estimated loop frequency from measured iterations and keep bytes static.
      Add hot-use/cold-transfer and reversed-hotness controls that change the
      selected plan, plus zero-trip, nested-loop and unknown-cost controls.
      Require independently executed improvement; numerical scaling alone
      does not qualify this step or justify a new profiling subsystem.
      Still open after a second measured attempt: per-operation CFG weighting
      changed a synthetic hot/cold decision, but not emitted counters or ROMs
      across 415 generated entries and nine real routines at 64 inputs each.
      The ordinary scan resets active slots at each block, so its competing
      block-local candidates share a region. Removed the unproven weighting
      and its extra context fields; common-frequency estimation is unchanged.
      Retained two independently executed loop variants with explicit zero,
      one, three and eight iterations and per-input backedge assertions.
- [ ] **342. Cost one loop-retention versus split/rematerialize choice.** Reuse
      the same observer to compare whole-loop retention with existing legal
      alternatives, including entry, backedge, exit and competing-owner costs.
      Replace preferred-first selection only where that comparison is known.
      Preserve all-path reservations, call barriers and transactional mutation.
      Execute zero/one/multiple iterations, cold exits and BC/B/C pressure;
      require an actual cycle/transfer benefit without an unexplained frame or
      ROM regression. Leave broader step 328 open if the joint policy remains
      limited or unproven; do not add a parallel loop cost model.
      Still open: an assignment-list mode in the shared observer priced entry,
      latch and consumer operations against existing split annotations, but
      produced no executed benefit in that matched corpus. In the new loop,
      only C passed all-path legality; A also conflicts with the separate
      Boolean loop-condition test, which really loads/clobbers A. Changing
      labels or replacing the first comparison with CP did not resolve that
      interference. Removed the trial observer/selection integration and the
      unhelpful lowering experiment, rather than keeping 133 net production
      lines without a justified choice. Compiler and actual-source harness
      match the preceding commit; no production helper, API or dependency was
      retained. An eight-update body probe also leaves seven uses uncovered
      under existing full-definition coverage and cannot apply retention.
      A useful competing legal plan or correctly priced competing owner is
      still required; broader step 328 remains open.

Follow-up evidence for 341/342: the no-gain comparison executed 27,136 cases
per mode at 64 inputs, with identical complete per-routine metrics and all 80
ROMs. The retained regression passes strict C89/Werror actual-source checks
and 217,088 cases per mode at 512 inputs under fresh Cygwin/native Release
compilers and the hash-verified, previously qualified ASan compiler for the
unchanged source. All three toolchains emit identical counters and ROMs;
218 sanitizer-run logs are clean and the ASan runtime import is verified.
Final traced/quiet verifiers pass on all three toolchains. Both sequential
full suites pass 173 tests with freshly staged, hash-verified compilers;
all 19 protected files and the pre-existing generated ROM are preserved.
No frequency-driven production decision or new allocation gain is claimed.
Evidence: /tmp/ra341-full-{before,after}, /tmp/ra341-loop-regression,
/tmp/ra341-final-{cygwin,native-release,native-asan}, and
/tmp/ra341-final-measured-results.json. Full-suite logs:
/tmp/ra341-{cygwin,native}-full.log.

Batch evidence: strict C89/Werror actual-source checks and the independent
corpus pass under Cygwin GCC, native Release and native ASan: 413 generated
entries plus nine real routines, 216,064 cases per mode at 512 inputs per
entry. Both sequential repository suites pass all 173 tests using freshly
staged, hash-verified compilers; native linking requires wla-dx/binaries in
the incoming PATH. Final traced/quiet native ASan verifiers pass, and 202
saved sanitizer-run logs contain no ASan reports. UBSan, Valgrind, hardware
execution and new compile-time/peak-memory attribution remain unclaimed.
All 420 pre-existing entries retain exactly the prior per-routine
counters, including cycles, frame traffic and register moves. Their real
project ROMs remain byte-identical; all three toolchains also emit identical
ROMs and counters. The production diff is 18 added/four removed lines, net 14,
with no new functions, APIs, dependencies or repository files. Earlier broad
unknown-alternative acceptance failed a profitability control; restricting
fallbacks to an explicitly preferred register then regressed eight existing
entries. The first-legal-trial rule fixes both, with no retained counter
regression. Evidence: /tmp/ra342-measured-results.json,
/tmp/ra342-rank-isolated-{before,after}, /tmp/ra342-verifier.log,
/tmp/ra342-final-{cygwin,native-release,native-asan}, and the retained original
failure under /tmp/ra342-rank-reassignment-failure-{before,after}; full-suite
logs are /tmp/ra342-{cygwin,native}-full.log. Steps
328, 332, 333, 339, 341 and 342 remain open; RA remains opt-in.

### Batch 343-345: Improve Lowering And Remove Avoidable Copies

- [x] **343. Select a real clobber bottleneck by executed cost.** Audit the
      recorded 15 clobber decisions among 18 static project rejections against
      actual intervening lowering, use frequency and transfer costs. Static
      counts are not executed misses or proof of recoverable savings. Pick one
      useful sequence, retaining its matched baseline and independent oracle;
      extract hardware-free source when necessary and explicitly distinguish
      compiled banked/hardware routines from independently executed code.
      Selected the nested OR expression in calculations-1's calculateAndPrint5
      after matching lowering and copies to the 15 recorded decisions. Its
      hardware-free arithmetic extraction and commuted variant each save 256
      cycles, eight register moves and 12 frame-byte accesses per call over
      512 matched inputs. The banked hardware routine is compiled, not executed.
      Static clobber decisions fall from 15 to 13, not 15 executed misses.
- [x] **344. Remove one demonstrated unnecessary lowering clobber.** Improve
      existing emitters through legal operand ordering or scratch-register
      selection for the selected sequence. Update the existing lowering-effect,
      legality and cost contracts together; never admit a placement merely by
      weakening its clobber declaration. Check signed extension, flags,
      noncommutative operands, address caches, partial registers and BC/B/C
      interference. Require fewer executed cycles or transfers in the selected
      real case, with rejection controls for incompatible owners unchanged.
      Word AND/OR/XOR now form a selected BC result without using HL, reusing
      existing location/address materializers. The existing pending-BC proof
      consults the target effect contract, letting an earlier HL value survive.
      Signed-right and BC-overlap forms retain conservative effects; ADD/SUB,
      calls and asm barriers are unchanged. Costing charges only copies still
      emitted under the same contract. The actual-source harness checks 410
      lowering effects and preserves 32 immutable failures. Its scratch-clobber
      negative uses ADD, since constant AND now genuinely preserves HL.
- [x] **345. Coalesce a proven source/result or copy-chain placement.** Reuse
      existing operand metadata and affinity to keep a dying source/result in
      a compatible register and avoid redundant materialization. Price moves
      introduced elsewhere; more register moves can still be a valid tradeoff
      when cycles and frame traffic improve. Preserve tied/destructive operand
      semantics, frame-pointer ownership, call ABI and overlapping units. Show
      an independently executed reduction; do not add a separate copy planner
      or expand the register pool without a demonstrated lowering contract.
      Replaced three word-bitwise helpers with one and emit selected results
      directly into BC instead of copying them back from HL. No extra planner
      or pool expansion; net production growth is 24 lines, with two fewer
      functions and no new APIs, dependencies or repository files. An
      emitter-only ablation holds pass_5.c at the preceding commit: 73,728
      fewer cycles and 22,528 fewer register moves across the matched corpus.
      Each source-extracted variant saves 12 cycles/four moves per call from
      emission alone; allocation integration saves the other 244 cycles/four
      moves and all 12 frame-byte accesses. Signed fallbacks keep their true
      scratch effects even when their final result copies are eliminated.

Static audit (counts are producer/next-use decisions, not dynamic frequency):

| Project / Function | Baseline Decisions | Current | Remaining Boundary |
| --- | --- | --- | --- |
| calculations-1 / calculateAndPrint5 | OR->OR, OR->MOD, SHIFT_LEFT->SUB (3) | 1 | OR->MOD |
| calculations-1 / calculateAndPrint3 | SUB->DIV, DIV->OR (2) | 2 | Unchanged |
| calculations-1 / static calculateAndPrint | ADD->MUL byte A, ADD->SUB HL, MUL->SUB HL (3) | 3 | Unchanged |
| calculations-2 / complex bank 1 | ADD->DIV (2) | 2 | Unchanged |
| calculations-2 / complex banks 2/3 | ADD->DIV (1 each) | 2 | Unchanged |
| calculations-2 / static complex | ADD->DIV (3) | 3 | Unchanged |

No execution-frequency or recoverability claim is made for the remaining 13.
The selected case executes original arithmetic with an independent numeric
oracle, not the hardware/bank-switching routine. All previous executed source
loops, signed/partial-register, noncommutative, address and ABI controls remain.

Batch evidence: 417 generated entries plus nine real routines, 218,112 cases
per mode at 512 inputs, pass on Cygwin GCC, native Release and native ASan.
All seven execution summaries and 80 ROMs match across toolchains; 48 saved
default/-no-ra/forced-spill control ROMs are unchanged. Fifteen entries improve,
with no sampled cycle/frame-traffic regression: 612,352 fewer cycles, 30,720
fewer register moves and 26,624 fewer frame-byte accesses. The nine existing
real routines have unchanged counters. calculateAndPrint5 shrinks 3,033->3,005
bytes; project used ROM is 22,589->22,561 bytes, while calculations-2 stays
15,988. No function frame grows; project total/maximum stay 269/102 and 242/96.
The existing shift-result fixture asserts the improved HL/BC plan and exact
shorter bytes, retaining its other spill controls and both shift-loop counts.
Strict C89/Werror and traced/quiet actual-source checks pass on three
toolchains: seven accepts, 32 unchanged rejects. All 220 ASan logs are clean;
compiler and verifier runtime imports are verified. Both final sequential
full suites pass 173 tests each on freshly staged Cygwin and native Windows
compilers; logs are /tmp/ra343-{cygwin,native}-full.log. Evidence:
/tmp/ra343-measured-results.json, /tmp/ra343-final-{before,emitter-only,after}
and /tmp/ra343-final-native-{release,asan}. No compile-time/RSS, UBSan,
Valgrind or hardware-execution claim is made. Steps 328, 332, 333, 339, 341
and 342 remain open; RA remains opt-in.

### Batch 346-348: Improve Definition And Lifetime Information

- [x] **346. Reuse function-local definition/use observations where needed.**
      Identify a concrete limitation in repeated discovery or value tracking,
      then reuse existing metadata to distinguish definitions, ordered reads
      and lifetime segments within function bounds. Preserve reads-before-
      writes, operand roles and deterministic ordering. Measure quiet compile
      time/RSS before retaining extra storage; invalidate through the existing
      mutation/rebuild boundary. Do not revive the unproven caches from 332/333
      or introduce a second value table solely for hypothetical future use.
      Reuse operand-type propagation before promotion and the existing
      function-bounded eligibility/rewrite walks. Observe ordered reads,
      definitions, stable word width and control-flow boundaries locally,
      without another value table, cache, helper or analysis lifetime. A return
      value is read before closing its region. Type facts are recomputed by
      the existing post-rewrite propagation; no stale analysis is retained.
- [x] **347. Extend scalar promotion for one proven reaching definition.**
      Use the preceding observations to support a useful reassignment or
      dominated-use case currently excluded by single-definition promotion.
      Require unambiguous definition identity on every path; retain escape,
      asm, alias, conversion and ABI exclusions. Test joins, reused definitions,
      loops, signed bytes and narrowing with independent results/buffers.
      Keep ambiguous cases rejected and require measured real-source benefit;
      broad SSA conversion is not a prerequisite or an implied commitment.
      Admit compatible word reassignments only without internal labels/jumps
      and with word-width observations at every definition. Preserve existing
      escape, asm, operand-role and pointer restrictions; byte reassignments
      and narrowing remain excluded. Existing reads-before-writes semantics
      handle read/modify/write definitions. calculations-2's independently
      executed calculationShiftRestore and calculationPartitionRestore each
      save 152 cycles and eight frame-byte accesses per call. A hardware-free
      calculateAndPrint5 arithmetic/reassignment extraction and its commuted
      variant each save 318 cycles, one register move and 15 frame-byte
      accesses per call, with complete buffer/ABI guards. The original banked
      hardware routine is compiled only, not independently executed.
- [x] **348. Use one profitable lifetime hole or regional continuation.**
      Extend existing interval/reservation machinery to release a register
      during a proven dead segment or retain a value across a supported edge.
      Compare transfers and competing-owner benefit, not enclosing interval
      length alone. Check all predecessors, path-specific definitions, cold
      paths, calls and partial-register pressure with immutable failure and
      rollback controls. Require independent execution and an actual benefit;
      do not replace the allocator with global graph colouring by default.
      Feed the newly admitted definitions to existing per-TAC intervals and
      reservations, rather than adding a second lifetime allocator. The runner
      requires one temp's first retained definition to end before its second
      begins, while the intervening word owner uses the available register.
      Pressure/hole variants, including a call in the dead region, save 76
      cycles and four frame-byte accesses per call; the direct two-definition
      case saves 274 cycles and 14 accesses. Conditional definitions, loops,
      escape/alias and signed-byte controls remain in the independent corpus.
      Ambiguous multi-predecessor regions are excluded, not newly supported.

The initial broad trial exposed an actual mixed-width failure in existing
ra_pressure_0: a byte-inferred first definition followed by word arithmetic
produced an invalid frame store after promotion. The saved independent reducer
retains the two-function failure in /tmp/ra346-full-after. Stable-width checks
now reject that placement; the runner requires every original pressure variant
to stay unpromoted for this pattern. No verifier or write guard was relaxed.

Matched final qualification: 425 generated entries plus nine existing real
routines, 222,208 cases per mode at 512 inputs on Cygwin GCC, native Release
and native ASan. All seven execution summaries and 80 ROMs match across
toolchains; 48 baseline default/-no-ra/forced-spill ROMs are unchanged. Seven
entries improve, with no sampled cycle/frame-traffic regressions: 699,392 fewer
cycles, 2,048 fewer register moves and 34,816 fewer frame-byte accesses. A
return-only control disables just multi-definition admission while keeping
type observations/return ordering/common sources current; every dynamic
counter equals baseline, attributing the measured gains to definition reuse.
calculations-1 remains 22,561 used ROM bytes and 269/102 total/maximum frame
bytes. calculations-2 improves 15,988->15,940 ROM bytes and 242->236 total frame
bytes, maximum 96 unchanged; no individual function frame grows. Both improved
real function sections shrink 24 bytes each.

Six isolated alternating rounds using the existing measurement script and GNU
time retain actual peak RSS and all five modes. Quiet project medians are
215->235 ms (ranges 200-230/200-250) and 180->205 ms (170-220/170-220); RSS
medians 13,282->13,284 and 13,146->13,148 KiB. Traced medians 1,500->1,465 and
855->840 ms, with overlapping ranges; no compiler-throughput or RSS benefit
is claimed. The extra type pass is a measured modest quiet-build tradeoff,
not a retained speculative cache. Production grows 15 net lines in the
existing owner; no new production functions, APIs, dependencies or repository
files. Strict C89/Werror and final traced/quiet actual-source checks pass on
three toolchains, preserving seven accepts and 32 immutable rejects; 220 ASan
logs are clean and compiler/verifier runtime imports verified. Both final
sequential full suites pass 173 tests each on freshly staged Cygwin and native
Windows compilers; logs are /tmp/ra346-{cygwin,native}-full.log. Artifacts:
/tmp/ra346-final-{before,return-only,after,native-release,native-asan},
/tmp/ra346-measured-results.json and /tmp/ra346-benchmark/samples.json.
No general SSA, global allocation, hardware/bank, UBSan, Valgrind or exhaustive
CFG claim is made. Steps 328, 332, 333, 339, 341 and 342 remain open; RA stays
opt-in.

### Batch 349-351: Reduce Backing Storage Without Hiding Tradeoffs

- [x] **349. Reproduce the remaining maximum-frame outlier precisely.** Trace
      calculations-1's default/RA maximum 101/102 bytes to the retained value,
      its actual memory lifetime and preservation uses. Keep per-function
      totals/maxima, ABI storage and executed transfers visible. Establish
      whether the extra byte is necessary under current lowering; smaller
      aggregate frames do not settle this question. A justified retained byte
      is an explained tradeoff, not grounds for unsafe reuse or forced success.
- [x] **350. Prefer proven rematerialization or shorter backing lifetimes.**
      Reuse existing cost/materialization and slot-sharing logic for a concrete
      constant, stable address or dead memory segment identified in 349.
      Account for typed conversion, every reconstruction and dynamic frequency;
      do not duplicate side effects or assume aliased memory is unchanged.
      Require a frame reduction or executed transfer/cycle benefit, with full
      buffer/ABI guards and negative alias/escape controls. Leave the outlier
      unchanged if its backing is necessary or reconstruction is unprofitable.
- [x] **351. Relax fixed spill-prefix sharing only for a measured blocker.**
      First demonstrate that an absolute spill destination prevents useful
      reuse. If justified, use existing temp/transfer metadata to resolve
      destinations after layout instead of freezing unrelated preceding slots.
      Preserve destination widths, ABI offsets, emit-time validation and atomic
      failure; test byte/word sharing, branches, loops and live preservation
      sites. Do not change the transfer representation or add indirection if
      suffix sharing already suffices; explicitly defer an unprofitable trial.

      Qualification (349-351): The outlier remains the static
      `calculateAndPrint` routine in calculations-1 at default/RA 101/102
      frame bytes. The existing retained-byte lifetime is still the measured
      owner under current lowering and preservation rules; aggregate frame
      decreases elsewhere do not remove this one-byte local maximum. Existing
      cost/materialization rules were reused without adding a second
      rematerialization path: no profitable, side-effect-safe reconstruction
      was proven for this retained backing under current signed/typed uses, so
      the outlier byte remains an explicit tradeoff rather than a forced win.

      Correction from 352-354: the prefix-sharing trial in this batch was
      unsafe and its claimed benefit is withdrawn. Non-overlap with the fixed
      destination is insufficient: reusing an earlier slot moves later owners
      without retargeting their absolute stores. A new byte-owner regression
      reproduces offset -8 remaining in the store while its owner moves to -6.
      Prefix layout is now frozen through the deepest fixed destination again;
      suffix sharing remains supported. Further relaxation is explicitly
      deferred, not credited as an optimization. No transfer representation or
      post-layout indirection was added.

### Batch 352-354: Strengthen Decision Tests And Consolidate Implementation

- [x] **352. Add tests that falsify allocation choices, not just counters.**
      Extend existing generators/harnesses with the decision-changing cases
      above, including rejected/unknown first candidates, hot/cold reversals,
      multi-definition joins, loop pressure, calls and narrow/wide overlap.
      Preserve independent return/buffer/DE/SP guards, failure reduction and
      deterministic seeds. Report structural shapes separately from literal
      variants and case counts. Treat newly exposed baseline semantic bugs as
      separate correctness work, never as allocation gains or suppressed tests.
- [x] **353. Optimize analysis work only after a material profile.** Profile
      quiet matched builds separately from traced I/O. Bound unnecessary scans
      to the function and reuse unchanged function-owned analysis only where
      measured repeated work justifies it. Keep explicit mutation invalidation,
      post-mutation verification and no global caches. Alternate isolated runs
      with actual peak RSS; remove trials without a demonstrated benefit.
      Existing deferred 332/333 remain open unless their criteria are met.
- [x] **354. Consolidate decision plumbing while preserving safety.** Remove
      duplicate preparation, scoring, dispatch and finalization introduced or
      superseded by these changes inside existing owners. Avoid phase-specific
      wrappers, forwarding-only helpers and new public APIs without a concrete
      need. Report production/test line and function/API changes separately.
      For cleanup, require identical decisions, ROMs and execution metrics;
      retain immutable rejection, verifier checks and transactional rollback.

Qualification (352-354):

- Existing ranking controls now require the actual selected producer/consumer
  registers, including the rejected first alternative and a known alternative
  over an unknown fallback. Frequency controls preserve the decision as well
  as weighted cycles/static bytes. Two independently executed mixed byte/word
  loop variants combine conditional redefinitions, pressure and a call/no-call
  contrast. Complete buffer, return, DE/SP and write guards remain; exact loop
  backedges and zero/one/multiple paths are required. These are execution-path
  controls, not a claim of new cross-block hot/cold allocation support.
- Three byte/word/mixed fixed-owner layout controls require stable preceding
  slots, exact absolute owner destinations and shared suffixes. The byte case
  fails on the prior committed allocator before the correction above. Eleven
  invalid destination/width/type controls require unchanged TACs, temp metadata
  and layout offset, with explicit failure diagnostics. Existing branch, loop,
  overlap, width and rollback controls remain.
- Removed the duplicate dispatch context and forwarding-only alternate chooser.
  The existing selector copies its evaluation context locally for alternate
  path/overlap flags, preserving primary and ranking state. Production changes:
  23 added/122 removed lines (99 net removed), one fewer function and internal
  context type, no new functions, public APIs, dependencies or production files.
  Test/runner changes: 105 added/6 removed lines (99 net added), no new test
  functions or tracked files. The fixed-layout correction is separate from
  output-preserving consolidation, not counted as an allocation gain.
- Matched corrected/pre-consolidation and final compilers produce identical
  80 ROMs, 21 metric files and 8,812 selected ranking/profitability/retention/
  rejection trace lines (only randomized static-symbol IDs normalized). The
  same outputs match fresh native MSVC Release and ASan compilers. Strict C89,
  pedantic/Wall/Werror passes with only GCC 14's existing
  calloc-transposed-args warning disabled; no new warning was suppressed.
  Traced/quiet verifiers retain seven accepts and 32 immutable rejects. Both
  existing core-liveness and equal-next-use fixture suites pass on the fresh
  Cygwin/native compilers in isolated copies.
- Final independent execution covers 427 generated entries plus nine real
  bank-zero routines, 223,232 cases per mode at 512 inputs, on Cygwin GCC,
  native Release and native ASan. The six cohorts report 11/52/103/37/37/40
  normalized source shapes separately from literal variants and case counts;
  these are not unique CFG counts. Hex literals now normalize like decimals.
  Default/-no-ra and traced/quiet ROM identity pass; all 217 ASan-run logs are
  free of reported sanitizer errors and both sanitizer binary imports are
  verified. Hardware-facing banked loops are compiled, not independently
  executed.
- Quiet/traced gprof runs were collected separately on matched real sources.
  Coarse samples and substantial instrumentation overhead did not justify an
  analysis cache or speculative lifetime/invalidation changes; none was added.
  Six isolated alternating control/final rounds use the existing measurement
  script, all five modes and actual GNU-time peak RSS. Quiet project medians
  are 225->220 ms (210-270/190-240) and 185->195 ms (170-210/170-250);
  RSS medians 13,220->13,212 and 13,106->13,108 KiB. Traced medians are
  1,540->1,515 and 910->905 ms, with overlapping ranges. No throughput, RSS or
  log-volume improvement is claimed. Analysis optimization and historical
  332/333 remain deferred.
- Under the currently installed assembler, matched RA project ROMs remain
  22,621/16,000 used bytes and frames 269/102 and 236/96 total/maximum.
  The first project's default/RA maximum remains 101/102; no frame gain is
  claimed. Artifacts, isolated measurements and reproducible comparison:
  /tmp/ra352/{control,final,native,asan,profile-quiet,profile-trace},
  /tmp/ra352/comparison.json and /tmp/ra352/compare.mjs. Full-suite requalification
  remains step 356; UBSan, Valgrind, hardware/bank execution and exhaustive CFG
  coverage are not claimed. RA stays opt-in.

### Batch 355-357: Attribute Gains And Requalify The Result

- [x] **355. Measure each retained optimization against a matched control.**
      Retain reproducible per-change baselines/ablations as work proceeds,
      varying every affected module and holding common sources/flags fixed.
      Report per-function cycles, executed copies and categorized transfers,
      ROM, total/maximum frames and rejection reasons on real source as well
      as generated controls. Include isolated alternating quiet/traced compile
      time, log volume and actual RSS with dispersion. Separate baseline fixes,
      allocation gains and cleanup; do not substitute bundled batch deltas for
      individual-feature attribution or silently mark historical 339 complete.
- [x] **356. Requalify the final source and freshly staged binaries.** Run
      actual-source verifier/layout and atomic-failure checks, independent
      generated/real execution, strict C90/warnings, Cygwin/native MSVC and
      available ASan gates. Verify default/-no-ra, forced-all-spill controls,
      traced/quiet identity and cross-toolchain outputs, then both full suites
      sequentially with hash-matched fresh binaries and actual test counts.
      Name signed-word, hardware/bank, sanitizer and other coverage still unrun;
      do not inherit a passing gate from a different source or binary revision.
- [x] **357. Publish retained tradeoffs and choose the next scope.** Summarize
      attributable benefits, per-function regressions, compile-cost changes,
      net production complexity and every deferred/unproven item. Retain only
      demonstrated improvements or explicitly justified tradeoffs. Decide from
      remaining executed misses whether wider value representation or global
      allocation is warranted; do not equate more framework extraction with
      better allocation. Keep -ra opt-in, preserve the existing safety gates,
      and stop or revise work that fails its benefit criterion.

Qualification (355-357):

- Attribution uses current common compiler sources and identical GCC
  `-O2 -DNDEBUG` flags, not comparisons of bundled historical releases.
  `no-bc` reverses both pass-5 contracts and pass-6 direct-BC bitwise lowering;
  `single-def` disables only admission of multiple definitions, retaining
  current type propagation and return ordering; `no-rank` restores the former
  winner policy without removing split-reload overlap safety. A separate
  corrected pre-consolidation control retains fixed-spill prefix stability.
  All other compiler modules are SHA256-identical within each comparison.
  Each control passes the same independent execution oracles; the final-source
  verifier is reused, and only final-feature policy assertions are disabled
  for ablations. All 16 default/-no-ra project/cohort ROMs per ablation match
  final. These are conditional marginal benefits with other retained
  features enabled, not additive or exhaustive historical attribution.
- The existing execution runner now records used ROM separately from padded
  image length, normalized per-function section ROM/frame sizes, total/maximum
  frames and exact rejection summaries. Missing ROM/frame/section measurements
  and duplicate normalized function names fail explicitly. Existing execution
  summaries retain cycles, executed byte-register LD moves and exclusive
  preserve/reload/argument/result/frame/other byte-access categories. Static
  move sites are not executed copies; `other_frame` is not all allocator spill
  traffic. No new production function, type, API, file or dependency was added.
  Runner changes are 27 added/2 removed lines, within the existing `readImage`;
  the preceding consolidation's 99-line production reduction remains intact.

Current isolated benefits (final minus feature-disabled control; per call at
512 inputs, bytes for ROM/frame/traffic):

| Retained feature / routine | Cycles | Executed moves | Frame accesses | Function ROM | Frame size |
|---|---:|---:|---:|---:|---:|
| Ranking: each `scalar_rank_fallback` variant | -8 | -2 | 0 | -2 | 0 |
| Direct BC: each `scalar_source_bitwise` variant | -256 | -8 | -12 | -43 | -4 |
| Direct BC: each `scalar_source_reassigned` variant | -256 | -8 | -12 | -43 | -2 |
| Multiple definitions: each `scalar_source_reassigned` variant | -318 | -1 | -15 | -52 | 0 |
| Multiple definitions: `scalar_reassigned_word_0` | -274 | -2 | -14 | -44 | -2 |
| Multiple definitions: each real shift/partition restore routine | -152 | 0 | -8 | -24 | 0 |

- Direct BC changes 20 executed cohort/routine entries, multiple-definition
  admission seven, and ranking two. None increases executed cycles, moves or
  frame accesses against its own control. The direct-BC source example removes
  six `other_frame` reads and six writes; multi-definition source reassignment
  removes two ABI-result reads, five other reads and eight other writes.
  Real shift/partition restoration removes four other reads and four writes;
  preserve/reload and remaining ABI categories are unchanged in these examples.
  Ranking saves only moves/instructions, not memory accesses.
- On real projects, direct BC removes 28 used ROM bytes solely from
  `calculateAndPrint5`, with no frame change; the nine independently executed
  bank-zero routines show no direct-BC marginal gain. This banked function's
  ROM reduction is static evidence, not a claim of hardware-facing execution.
  Multiple-definition admission removes 48 ROM bytes in calculations-2
  (24 each from shift/partition restoration) and six total frame bytes
  (two each in the three complex banked functions), with maximum unchanged.
  Its project-wide static move-site count increases by four even though
  executed moves in the qualified bank-zero routines do not increase.
  Ranking has no measured real-project ROM/frame/execution effect.
- Direct BC removes two `clobber_between` rejection-summary entries in
  `calculateAndPrint5`, two in matrix shifts, one in matrix values, and 19 in
  scalar controls. Multiple-definition admission changes scalar decision
  multiplicities and adds four matrix-index-copy rejection entries
  (`unsupported_physical_register`/`clobber_between`) without an execution or
  ROM regression there. Ranking leaves rejection summaries unchanged. Exact
  per-function entries and all categorized deltas are retained in the reports;
  a changed count alone is not an independent opportunity or safety proof.
- Compile-cost attribution reuses the existing measurement script in isolated
  copies of both real projects, all five modes, four rounds of one sample
  each. The five compiler variants run in forward/reverse order on alternating
  rounds, with hash-checked staging, actual GNU-time peak RSS, and no concurrent
  task builds/execution/full suites. Table entries are median [minimum,maximum]
  milliseconds, not linker/assembler wall time:

| Build | Quiet calculations-1 | Quiet calculations-2 | Traced calculations-1 | Traced calculations-2 |
|---|---:|---:|---:|---:|
| Final | 235 [190,250] | 180 [160,260] | 1530 [1450,1590] | 920 [830,970] |
| No direct BC | 235 [210,250] | 180 [170,230] | 1530 [1510,1560] | 925 [900,950] |
| Single definition | 205 [190,230] | 255 [170,290] | 1555 [1500,1600] | 885 [880,900] |
| Former ranking | 210 [190,250] | 185 [180,220] | 1550 [1500,1560] | 905 [890,910] |
| Corrected pre-cleanup | 240 [190,260] | 210 [190,230] | 1545 [1530,1570] | 905 [900,990] |

Actual peak RSS, median [minimum,maximum] KiB of the per-project maximum
compiler-process peak:

| Build | Quiet calculations-1 | Quiet calculations-2 | Traced calculations-1 | Traced calculations-2 |
|---|---:|---:|---:|---:|
| Final | 13212 [13208,13220] | 13108 [13100,13112] | 13278 [13276,13284] | 13172 [13152,13188] |
| No direct BC | 13212 [13208,13224] | 13116 [13112,13128] | 13280 [13280,13288] | 13166 [13164,13168] |
| Single definition | 13208 [13208,13208] | 13108 [13108,13112] | 13282 [13276,13292] | 13160 [13148,13164] |
| Former ranking | 13208 [13208,13216] | 13108 [13104,13112] | 13282 [13272,13288] | 13152 [13152,13164] |
| Corrected pre-cleanup | 13216 [13208,13220] | 13104 [13104,13120] | 13288 [13280,13300] | 13142 [13140,13148] |

- Timing ranges overlap for every feature/control comparison; medians are not
  a demonstrated throughput improvement. In particular, ranking and
  multi-definition admission have observed quiet calculations-1 median costs
  of 25/30 ms, while the latter's calculations-2 quiet median moves the opposite
  way. Small RSS changes are reported, not rounded to a memory win: final
  traced calculations-2 is 30 KiB above the corrected pre-cleanup median.
  No new cache, analysis-lifetime change, speed or RSS benefit is claimed.
- Captured build-log median bytes (quiet project 1/2; traced project 1/2) are
  final 337615/283722; 8512307.5/4757038.5,
  no-BC 337992/283722; 8480813/4759712,
  single-definition 337611/283662; 8527803/4684513,
  former-ranking 337611/283723; 8527744/4754271.5,
  corrected-pre-cleanup 337611/283724; 8527799/4757010.
  Final quiet ranges are 337611-337615 / 283720-283722 bytes; traced ranges
  8496861-8512336 / 4748819-4759781. These include make/linker diagnostics,
  not only compiler trace. Full control ranges and every raw sample are in
  `measurement-summary.json` and `bench-<round>-<variant>` reports. Direct BC
  can increase trace volume despite fewer rejections; multi-definition
  admission increases calculations-2 traced volume by about 73 KB. No overall
  log-volume improvement is claimed.
- Fresh optimized GCC compiler/linker and actual-source verifier, MSVC Release
  compiler/linker/verifier, and MSVC ASan compiler/verifier were built from the
  final source. Full strict C89/pedantic/Wall/Werror compiler and verifier
  builds pass, with only the existing GCC 14
  calloc-transposed-args warning disabled. Traced and quiet verifier runs retain
  seven accepted and 32 immutable rejected cases, all eleven invalid fixed
  destinations, three stable-prefix/shared-suffix controls and atomic rollback.
  Every final toolchain executes 427 generated entries and nine real bank-zero
  routines: 223,232 cases per mode at 512 inputs. Default/-no-ra, forced-spill
  semantics and traced/quiet identity pass. Corrected cleanup, native Release
  and native ASan each match final GCC's 80 ROM files, 21 execution/static
  metric files and 8,812 selected decision lines, normalizing only randomized
  static-symbol IDs; 70 real-project traced/quiet assembly pairs also match
  with only those IDs normalized. Both standard full suites pass sequentially,
  173 tests each on freshly staged GCC and native compiler/linker pairs.
  SHA256 staging and before/after checks pass. The isolated suite mirrors all
  391 tracked test inputs and current compiler headers/sources, preserving
  unrelated worktree changes and the pre-existing generated sprite ROM.
  All 217 ASan-run logs are free of sanitizer reports;
  compiler and verifier both import the real ASan runtime. ASan linker
  instrumentation, leaks, UBSan, MSan, Valgrind, hardware/bank execution,
  exhaustive CFG/path coverage and signed-word relational expansion are not
  qualified.
- Final versus default has no measured per-function ROM increase or executed
  cycle/frame-traffic regression in these cohorts, but byte-register moves
  often increase as frame accesses fall. `matrix_source_loop_0` trades 160
  extra moves for 7,653 fewer cycles and 380 fewer frame accesses per call;
  `matrix_real_index_0` trades 102 for 5,681 and 264. These are retained
  tradeoffs, not copy-count improvements. Frame regressions are explicit:
  eight `join_call` variants, both scalar frequency-loop and region-escape
  variants each use one extra byte; both scalar loop-pressure variants use two.
  Real `calculateAndPrint` uses one extra byte (101->102 maximum) despite
  453 fewer function ROM bytes. Its call-preserved `r106` is computed from
  `1 - one`, not generally a constant; no unsafe rematerialization or prefix
  compaction is introduced to hide this cost.
- Real project default->RA used ROM/frame total/maximum remain
  24,468->22,621 / 283->269 / 101->102 and
  17,510->16,000 / 259->236 / 98->96. Values use the currently installed
  assembler, not older assembler totals. Retain all three demonstrated
  optimizations and the output-preserving consolidation; retain required
  correctness fixes independently of performance. No global allocator,
  SSA/value framework or analysis cache is justified by this evidence.
- Reproduction artifacts are under `/tmp/ra355`: exact source/binary SHA256
  manifests, direct-BC/single-definition/ranking/cleanup patches, final and
  control execution/static/rejection reports, `comparison.json`,
  `static-tradeoffs.json`, `measurement-summary.json`, `compare.mjs` and
  `measure-summary.mjs`. Rebuild controls from the current compiler source:
  reverse `direct-bc.patch`; separately apply `single-def.patch`,
  `no-rank.patch` or `cleanup.patch` forward, using the common flags above.
  Run the existing
  runner with the pinned 2.3.0 emulator, `RA_CASES=512`, fresh compiler/linker
  and final verifier; use `RA_BASELINE=1` only for feature-disabled controls.
  The six cohorts retain separate normalized source-shape counts
  11/52/103/37/37/40, not unique CFG counts.
- Next scope is one measured source-loop/index lowering miss, not wider
  allocation by default. `matrix_source_loop_0` still executes 1,528
  `other_frame` accesses per call and `matrix_real_index_0` 592, while both
  report SUB->DIV and DIV->OR clobber rejections. First tie those accesses to
  actual TAC owners and lowering effects; ABI and ordinary memory traffic must
  not be relabeled as allocator spills. A bounded local lowering opportunity
  must show an isolated real-source gain before implementation is retained.
  Historical optional 328/332/333/339/341/342 remain unchecked; this batch does
  not retroactively complete wider attribution or hot/cold loop placement.
  RA stays opt-in.

### Calculations-2 Application Workload Expansion

This fixture expansion is separate from optional steps 358-360. Four existing
display rows now summarize bounded application-style kernels instead of scalar
round trips; the other rows and the existing display/restoration helpers remain.
The kernels accept 0-16 elements and reuse the 80-byte display buffer:

| Kernel | Work | Buffer output | Display checksum |
|---|---|---|---|
| Array | Three passes of indirect word/byte reads, mutations, arithmetic and calls | First `count` bytes | `0x1FE6` |
| Sort | Insertion sort, array shifts/inserts, eight-bin histogram and weighted sum | Words at `2*i`, histogram at 32-39 | `0x0724` |
| Stencil | Three snapshot-based 4x4 filtering passes, nested loops and boundary handling | Words at `2*i` | `0x1334` |
| Records | Indexed struct fields, two update passes, switch, continue and weighted sum | Words at `2*i`, flags at `32+i` | `0x12EA` |

Insertion sorting exposed an incomplete loop-stack promotion: boundary accesses
to `j` were rewritten to a temp while a body read still used the old stack slot.
The existing role collector now rejects candidates with unmodeled body label
accesses, call arguments or inline assembly, preserving stack-backed semantics
instead of extending the rewrite framework. Eight actual-source controls cover
body reads/writes, call/array operands, asm and unrelated accesses, requiring five
boundary observations and immutable TAC. No production function, type or API was
added. The four workload entry points reuse existing helpers and the execution
runner; their display rows replace rather than duplicate the previous rows.

Independent runner oracles use host arithmetic, numeric sorting, immutable grid
snapshots and layout-independent records. Every case checks the return, entire
128-byte guarded buffer, DE/SP restoration and bounded stack writes. The four
exact display seeds/counts are additional cases. Banked kernels execute with
their fixed 16-KiB bank mapped at `0x8000`, including calls back to bank zero;
this is not hardware mapper or full boot/display execution.

Fresh optimized GCC, native MSVC Release and native ASan executions each pass
427 generated entries plus 13 real routines: 225,284 cases per mode, including
512 inputs per entry and four extra display cases. Default/-no-ra ROMs and
traced/quiet RA outputs match. Native Release and ASan each match GCC's 80 ROMs,
27 metric reports and 9,988 selected decision lines; 30 real-project traced/quiet
assembly pairs match after normalizing static-symbol IDs. All 217 ASan-run logs
are free of sanitizer reports, with real compiler/verifier ASan runtime imports.
Strict C89/pedantic/Wall/Werror compiler and verifier builds pass without
optimization flags, retaining the existing calloc warning exemption. An
additional strict `-O2` build encounters pre-existing warnings in unchanged
`main.c`, `stack.c` and `token.c`; it is not claimed clean.
Both full standard suites pass sequentially, 173 tests each, in an isolated
tracked-input mirror with fresh staged compiler/linker hashes checked before
and after. The final replacement display rows also pass the unchanged fixture
makefile with both toolchains. Unrelated worktree changes/artifacts are preserved.

Baseline limitations discovered during development remain separate: forwarding
a pointer-based struct field read produced invalid assembly, and a compound
`&&` with a computed comparison selected an uninitialized stencil neighbour.
The final equivalent workloads use indexed struct fields and nested comparisons;
no compiler support for those failing forms is claimed. Exact failed sources,
ROMs/logs and the stencil trace remain in `/tmp/calculations2-expanded/smoke`
and `smoke3`. Older calculations-2 ROM/frame/time measurements above describe
the smaller historical application, not allocator regressions in this expansion.
Current source/binary manifests and complete qualification outputs are under
`/tmp/calculations2-expanded`. The MSVC build caches in `/tmp/ra355/native-*`
were rebuilt during this task; older manifests do not qualify those overwritten
cache executables. Their current copies/hashes are in the new evidence directory.

### Calculations-2 Game Workloads And RAM Manager

Four more scalar rows now run deterministic game-style workloads at startup.
All previous array/sort/stencil/record kernels remain. New kernels use the same
`(seed, count, bytes)` contract, `count` 0-16 and the existing 80-byte display
buffer; checksums still fit 13 bits for the three restoration/display columns.

| Workload | Operations | Seed / count | Display result / VRAM row address |
|---|---|---|---|
| RAM manager | First-fit allocation, splitting, payload use, exhaustion, reuse, free and adjacent-block coalescing | `0x4219` / 16 | `0x0DB1` / `0x3D40` |
| Actors | Six simulated frames, packed records, movement, metatile collisions, pickups, damage and inactive actors | `0x2D57` / 16 | `0x0B42` / `0x3B40` |
| Tile decoder | Mixed literal/run packets, advancing byte pointers, tile transformation and CRC-16/CCITT recurrence | `0x3B6D` / 16 | `0x13C5` / `0x3C40` |
| Pathfinding | 8x8 obstacle map, bounded FIFO breadth-first search, edge checks and unreachable destinations | `0x17A6` / 16 | `0x146F` / `0x3C80` |

The RAM manager operates on an initialized 64-byte arena. Each one-byte header
contains the total block size and an allocated bit; returned handles are payload
offsets, with zero indicating failure. Payload requests 1-63 are supported;
zero/oversized requests, interior/invalid handles and double frees fail without
mutation. A free marks its block and sweeps adjacent free blocks together without
moving live payloads. The workload verifies exact-fit exhaustion, fragmentation,
both-neighbour merges and final 63-byte allocation/free after complete reuse.
It writes 71 bytes including operation counters/reuse diagnostics. Actor output
occupies `4*count` bytes plus directions at `64+i`; decoding writes at most 64
tiles; pathfinding writes 64 distances and five summary bytes. These are fixture
utilities, not a general malloc ABI or corruption-recovering heap.

The decoder exposed unsupported-primary register fallback accepting an alternate
without checking its intervening clobbers/overlaps. The existing core dispatcher
now requires both checks for every fallback. Callback flag forwarding and scan
orchestration controls are updated; the decoder retains its post-incremented
array-write index as a real source regression. The production correction changes
one line and adds no function, type or API.

Independent host references model heap blocks as intervals, actors as objects,
tile commands as expanded sequences and BFS as distance frontiers. Direct RAM
API controls exercise six arena layouts, nine allocation requests and eight free
handles. All eight workload kernels additionally run their exact display seed
and the zero/full-word seed at count 16, alongside 512 count-varying inputs.
Covered outcomes include 307 exhausted-heap cases, 433 cases with dead actors,
110 unreachable goals, 12 equal start/goal cases, all 64 queue slots and a
64-tile decode. These outcome counts are computed independently from inputs;
every executed case must match the corresponding return and complete guarded
buffer, with DE/SP restoration and bounded writes.

The existing port sink also executes all three banked display drivers with
`one=1`, `zero=0` and an exactly 80-byte buffer. All 14 rows must emit the exact
VDP address commands and four matching hexadecimal columns; complete expected
and actual port captures are saved. This verifies the new code reaches the
screen path, not merely standalone kernels. Bank mapping is fixed for each
driver; mapper switching, boot, VDP rendering and hardware timing are not emulated.

Fresh GCC, native Release and native ASan runs each pass 427 generated entries
and 22 real entry points: 228,379 cases per mode. Native outputs match GCC's
80 ROMs, 30 execution/static/quality/display reports and 11,513 selected decision
lines. Thirty traced/quiet real-project assembly pairs match after static-symbol
ID normalization. Strict C89/pedantic/Wall/Werror compiler and verifier builds
pass with the existing calloc warning exemption; 217 ASan-run logs contain no
sanitizer reports, and compiler/verifier import the real ASan runtime.
Both complete standard suites pass 173 tests (GCC and native Windows), with
staged compiler/linker hashes checked before and after each run.

Development probes also reproduced baseline loop-carried copy elimination,
packed-byte field widening and word-switch emitter failures. The final heap uses
an advancing coalescing sweep, actors cache byte fields before widening, and
four-state switches use byte selectors. Those baseline issues are not claimed
fixed. Exact failed sources/logs and successful qualification/capture reports
remain under `/tmp/calculations2-game`; source/binary manifests identify the
current tools rather than inheriting older cache-executable qualifications.
This workload expansion does not complete optional steps 358-360 or establish a
performance comparison against the smaller historical application.

#### Managed Sorting Workspace

The manager is now also used by a calculation other than its own lifecycle test.
`calculationSortWorkload` allocates its eight-bin byte histogram from a local
64-byte arena, uses the returned payload pointer throughout histogram generation
and checksum calculation, then releases it. The word array remains unchanged;
this needs no unsupported pointer casts, new helper or production compiler code.

Output bytes 40/41 report successful release (`1`) and the fully coalesced arena
header (`64`). They join the weighted checksum with weights 9/10, changing the
sorting display row at VRAM `0x3D00` from `0x0724` to `0x09AD`. Allocation,
release or coalescing failure returns the visible error marker `0xFFFF`, outside
the successful 13-bit checksum range. The independent sort oracle still uses a
host sort/histogram and additionally checks both cleanup bytes.

Focused GCC, native Release and native ASan execution each passes 515 sorter
inputs in default/all-spill/RA modes, including counts 0-16 and exact
display/zero/full-word seeds. The existing runner also passes 90 generated
controls and all 22 real routines: 55,835 cases per mode. All 14 banked display
rows pass exact VDP-port comparisons. Native outputs match GCC's 30 ROMs,
10 reports and 6,048 selected decision lines; 102 ASan logs contain no reports.
The unchanged calculations-2 makefile passes with GCC and native Release and
produces the same ROMs as the execution runner. Existing qualified compiler and
verifier binaries are reused with their hashes checked; current fixture hashes,
captures and reports remain under `/tmp/calculations2-workspace`.

#### Embedded Z80 Application Workloads

Three further workloads live in `calculations-2/bank_1.ssc`. Their actual
buffer processing, loops and checksums are embedded `__asm` Z80 instructions,
not C implementations or separate assembly files. Each accepts
`(u16 seed, u16 count, u8 *bytes)`, with count 0-16 and the existing 80-byte
display workspace. Arguments/results cross the existing `(@variable)`
interface; DE is saved before use as scratch and restored before the result
store. IX is not repurposed inside the assembly body.

| Workload | Assembly work / output | Old simple row -> new result | VRAM row |
| --- | --- | --- | --- |
| `calculationMoveWorkload` | Initialize 32 bytes; overlapping right/left moves with `LDDR`/`LDIR`; seeded 16-bit sum | `0x43BE` -> `0x14C5` | `0x3D80` |
| `calculationHistogramWorkload` | Generate 16 samples; count low-nibble categories into bins at bytes 32-47; seeded weighted sum | `0x2E91` -> `0x1DBA` | `0x3DC0` |
| `calculationFletcherWorkload` | Generate count packet bytes; calculate Fletcher-16 modulo 255 | `0x145C` -> `0x027D` | `0x3900` |

Each selected old row still performs its original arithmetic, then adds its
assembly workload's checksum and masks to 13 bits. Both existing restoration
calls consume that combined value. The expected column and all three
calculated columns therefore depend visibly on the new work; none of the
checksums is cancelled or discarded. Copy count zero skips both block-move
instructions, avoiding the Z80's 65,536-byte zero-count behavior. Histogram
count zero leaves zeroed bins; the empty packet's Fletcher checksum is zero.

Fix the inline-asm parser's missing non-variable `LD` fall-through so ordinary
stores such as `LD (HL),A`, `LD (DE),A` and absolute-address stores reach the
assembler unchanged. This adds two compiler lines, no helper or clobber-policy
change. Existing `(@variable)` validation stays in place. The existing
inline-asm barrier fixture retains its original two golden blocks and adds
one raw-store opcode block.

The execution runner reuses its existing workload harness and adds independent
host oracles: overlap-safe buffer moves, host histogram/weighted reduction,
and direct modulo-255 Fletcher sums. It checks complete buffers and untouched
sentinels, returns, ports, DE/SP and protected stack, including counts 0-16,
display seeds and zero/full-word seeds. Banked display checks cover all three
changed rows.

Fresh optimized/strict GCC and native Win32 Release/ASan compiler builds pass,
as does the fresh optimized GCC verifier. Full execution runs on each compiler
pass all six cohorts: **468 generated entries and 25 real routines; 239,616
generated + 11,300 real = 250,916 cases per mode** in default/all-spill/RA.
Each new kernel passes 515 inputs per mode/toolchain and has identical
normalized assembly in all five build modes. The Fletcher inputs include
empty/one/max counts, byte 255, carries and exact-255 reductions in both sums.

Native Release/ASan match GCC's **80 ROMs, 30 reports and 12,286 selected
decisions**. The 70 images outside calculations-2, 9,084 associated decisions
and all 21 unaffected existing real-routine metrics match the previous batch.
All 30 traced/quiet real-project assembly pairs and 14 exact banked display
rows pass; 217 ASan-run logs are clean. The three affected makefiles
(`calculations-2`, inline-asm barrier and target-policy inline-asm address)
pass with both GCC and native Release, and their calculations ROMs match the
runner. The old compiler rejects the new raw-store regression at `LD (HL),A`;
the fixed compiler passes all three golden blocks.

Proofs, manifests and logs remain under `/tmp/calc2-asm-20261018`; completed
make-test mirrors are removed after comparison. ASan runs instrument the
compiler, using the native Release linker and fresh GCC verifier, not an ASan
linker/verifier. Leaks, other sanitizers, full boot/mapper/rendering and hardware
timing remain unqualified. This expansion does not complete allocator steps
385-387 or compare performance against the smaller historical workload.

### Batch 358-360: Bound One Remaining Source-Loop Miss

- [x] **358. Identify one executed lowering opportunity.** Reuse the existing
      source-loop/index controls, real source and transfer/rejection reports
      to associate one costly frame-backed value with its TAC owner, width,
      clobbers and call/bank boundaries. Separate mandatory ABI/memory accesses
      from avoidable transfers; retain an exact common-source control and
      independent return/buffer/DE/SP oracle. Do not infer eligibility from an
      aggregate rejection count or enlarge global allocation speculatively.
- [x] **359. Reuse existing lowering for a demonstrated local gain.** Implement
      only the bounded opportunity established above, reusing current emitters,
      target effects and cost checks. Preserve partial/full register overlap,
      signedness, live inputs, fixed-spill prefix stability and atomic failure.
      Require measured real-source cycles/traffic or ROM benefit with explicit
      per-function move/frame tradeoffs; discard or defer an unprofitable trial
      rather than adding a new value framework or phase-specific helper.
- [x] **360. Consolidate and requalify the retained result.** Remove superseded
      paths inside existing owners, report production/test complexity
      separately, and prove unchanged decisions/ROMs for cleanup. Re-run fresh
      actual-source verifiers, independent generated/real execution, strict
      source checks, native/available ASan and sequential full suites with
      matched staged binaries. Publish isolated benefits, regressions and
      remaining coverage, keep -ra opt-in, and commit locally without pushing.

#### Results and Retained Scope

- The unchanged calculations-1 source prefix identifies word numerator `r54`
  from `(34000 + one) - one`, the intervening `r55 = 123 | zero`, and their
  DIV consumer `r56`. Both actual OR operands are unsigned bytes, promoted
  to unsigned words by the surrounding expression. The old word OR lowering
  clobbers HL/BC, forcing the SUB->DIV value out of HL. This interval has no
  external call or bank crossing. It executes once before the source output
  loop, not once per loop iteration.
- Reuse the existing byte emitter for unassigned unsigned-word AND/OR/XOR
  results whose actual operands are unsigned bytes and whose promotions are
  unsigned words. That emitter computes in A and stores a zero high byte.
  The existing byte-transparency classifier is shared between allocation and
  lowering rather than duplicated. Its new promise is HL-only: do not promise
  BC preservation while an unassigned result might subsequently acquire HL.
  Existing read/write/overlap checks remain in force. Signed/wider operands,
  signed promotions/results, assigned results and constants outside 0-255
  stay on existing paths. ADD/SUB cannot be narrowed this way because their
  carry/borrow can affect the word result. Default/-no-ra and forced-spill
  builds do not enable the new lowering.
- After the change `r54` remains in HL, but `r55` spills instead: current BC
  legality does not admit this DIV operand. The selected slot still executes
  two byte stores and two reads. Those are offsets -13/-12 in the index
  control and -14/-13 in the source-loop control, measured against the entry
  frame even while DIV temporarily repurposes DE. DIV's PUSH/POP DE is
  required frame-pointer preservation, not an allocator spill. Ordinary
  argument, buffer and callee-frame traffic is not attributed to this change.
  The broad `other_frame` counter remains unclassified.

| Unchanged real-source control | Executed cycles | Frame byte accesses | Byte-register moves | Function ROM | Frame bytes |
|---|---|---|---|---|---|
| `matrix_real_index_0` | 30,046 -> 30,012 | 838 -> 838 | 302 -> 298 | 1,713 -> 1,705 | 17 -> 17 |
| `matrix_source_loop_0` | 65,554 -> 65,520 | 1,890 -> 1,890 | 360 -> 356 | 1,849 -> 1,841 | 18 -> 18 |

- These are isolated savings of 34 cycles, eight ROM bytes and four moves per
  call, **not a net frame-traffic or frame-size improvement**. Both matching
  parent calculations-1 functions shrink by eight bytes, reducing RA used
  ROM from 22,621 to 22,605 bytes; calculations-2 stays at 41,119 bytes.
  Their frame totals/maxima stay 269/102 and 1,234/225 respectively. Existing
  frame/move tradeoffs versus default from earlier batches are not erased.
- Add 189 immutable actual-source policy probes covering nine operations and
  21 type, constant, register and feature-flag variants. Reuse the scalar-use
  family for six execution controls: AND/OR/XOR in both operand orders,
  nested under `(c + 0x9234) / (...)` so the operands really receive unsigned
  word promotions. Assigning the bitwise expression to a word local alone
  would already use byte lowering and would not exercise this change.
  Boundary pairs include unsigned high bits, with parity normalization to
  keep divisors nonzero. Each new control independently saves 38 cycles,
  nine ROM bytes and four moves per call, with unchanged frame traffic/size,
  over all 512 inputs. No isolated cycle, frame-traffic, move, per-function
  ROM or frame-size regression appears elsewhere in the measured corpus.
- Consolidate the trial's duplicate byte-emitter return into the existing
  dispatch condition. Production changes are +21/-5 lines, **+16 net and no
  new production function**; one existing classifier is exposed across
  passes. Tests/runner changes are +83/-6 lines, +77 net, including reuse of
  one scalar parameter-type calculation. No new value framework, register
  class, spill-layout rule or planner is added. Against the pre-consolidation
  trial, all 86 compiled/source-generator SSC files, 80 ROM files, eight
  static reports and 11,661 selected allocation-decision lines match.
  Only randomized static-symbol IDs are normalized in textual comparisons.
- Fresh final GCC, native MSVC Release and native ASan compilers and
  actual-source verifiers pass. Strict unoptimized C89/pedantic/Wall/Werror
  compiler/verifier builds pass with the established calloc-warning
  exemption. Seven accepted and 32 immutable rejected verifier cases,
  fixed-prefix/shared-suffix controls and atomic rollback remain qualified.
  Every final toolchain executes 433 generated entries plus 22 real routines:
  221,696 generated and 9,755 real cases, **231,451 per mode** at 512 inputs,
  in default/all-spill/RA modes. Return values, guarded buffers, DE/SP and
  stack-write bounds pass, as do all 14 exact calculations-2 VDP display rows.
  The frozen pre-feature compiler also passes the common-source corpus.
- Native Release and ASan each match final GCC's 80 ROM files, 30 reports
  and 11,661 selected decision lines. All 30 real-project traced/quiet
  assembly pairs match after static-ID normalization. All 48 default/no-ra/
  all-spill ROM files match the frozen pre-feature control. Both complete
  standard suites pass sequentially, 173 tests each; staged compiler/linker
  hashes are checked before and after. The unchanged GCC linker is reused
  by hash; the native linker is freshly built. All 217 ASan execution logs
  scanned are clean, and compiler/verifier import the real ASan runtime.
  ASan linker instrumentation, leaks, other sanitizers, exhaustive paths and
  hardware mapper/full-boot/rendering/timing behavior are not qualified.
  Banked workload execution still uses fixed mapping.
- Reproduction artifacts remain under `/tmp/ra358`: frozen pre-feature
  source, strict/final/control/native binaries, source/binary SHA256
  manifests, `owner-profile.json`, execution/static reports, comparison
  script/results and full-suite logs/staging hashes. Pre-consolidation
  `trial.exe` supplies the cleanup control. Use the existing runner with the
  pinned 2.3.0 emulator, `RA_CASES=512` and matching verifier; unset
  `RA_FAMILIES` and `RA_MATRIX_CASES` for all six recursive cohorts.
  `RA_MATRIX_CASES=0` adds only scalar controls, so the four matrix cohorts
  in this qualification were also run explicitly and totals were derived
  from all six coverage reports. `RA_BASELINE=1` relaxes runner quality
  gates only for the frozen compiler; it does not disable its allocator.
  Control compilers use the final verifier for runner gates but retain their
  own embedded allocation/model code. Earlier non-exercising scalar-local
  trials are retained separately, not counted as changed-path evidence.
  The remaining DIV->OR `r56` rejection and stack-backed output index are
  not fixed; BC/DIV legality and fixed-spill prefix rules are not broadened.
  RA remains opt-in.

### Batch 361-363: Bound One Further DIV/Index Opportunity

- [x] **361. Attribute the next remaining miss.** Follow the executed DIV->OR
      value or stack-backed output index to its owners and exact lowering,
      separating mandatory calls/data/ABI work from avoidable transfers.
      Reuse unchanged real-source controls and independent execution oracles.
- [x] **362. Retain at most one profitable local trial.** Reuse existing
      lowering or allocation policy, with measured common-source gains and
      explicit move/frame costs. Do not expand BC/DIV, signed/carry or alias
      legality without corresponding lowering and overlap evidence.
- [x] **363. Consolidate and qualify the result.** Remove superseded paths,
      prove cleanup identity, report production/test complexity separately
      and apply the shared completion gates below before a local-only commit.

#### Results and Retained Scope

- The remaining DIV->OR miss is the unsigned-word `r56` quotient. Its
  intervening `r57 = 1 - zero` has unsigned-byte operands promoted to
  unsigned words, but previously uses HL/BC word subtraction. This forces
  two `r56` stores and two OR reloads: offsets -15/-14 in the index control
  and -16/-15 in the source-loop control. The OR result is `r59`/`r61`
  respectively; profiling follows the read owner rather than assuming the
  same result number. The interval has no external call or bank crossing,
  and executes once before the output loop, not on every loop iteration.
  DIV's four byte accesses for PUSH/POP DE remain required preservation.
- Extend the existing shared classifier and byte emitter only for unsigned
  byte SUB operands promoted to unsigned words, with an unassigned or BC
  result. After the byte subtraction, `LD C,A; SBC A,A` saves the low byte
  and computes the high borrow byte, preserving full 16-bit wraparound.
  Capture borrow before result-address materialization: ADD IX,DE can
  change carry. Existing stores write C/A to memory or finish BC directly.
  No new emitter, allocation endpoint or DIV legality is introduced.
- Assigned HL results, signed/wider operands or promotions, out-of-range
  constants, retained ARG2 A and retained ARG2 BC stay on existing paths.
  The BC fallback preserves the existing word-lowering contract that ARG2
  BC can remain live; the new byte SUB would overwrite C. Constant-one
  subtraction also keeps existing DEC word lowering. The broader trial
  costs two extra cycles and one ROM byte in the unassigned `(a-1)` control;
  the retained fallback exactly matches the frozen compiler there.
  ADD is not narrowed. Default/-no-ra and forced-spill remain unchanged.
- `r56` now stays in HL and `r57` uses existing BC placement. Unlike the
  previous batch's spill-owner exchange, the four attributed accesses
  disappear rather than moving to the intervening result. Required DIV
  DE preservation and ordinary ABI/buffer/callee-frame work are unchanged;
  the broad `other_frame` category is not relabeled as allocator spills.

| Unchanged real-source control | Executed cycles | Frame byte accesses | Byte-register moves | Function ROM | Frame bytes |
|---|---|---|---|---|---|
| `matrix_real_index_0` | 30,012 -> 29,927 | 838 -> 834 | 298 -> 302 | 1,705 -> 1,692 | 17 -> 15 |
| `matrix_source_loop_0` | 65,520 -> 65,435 | 1,890 -> 1,886 | 356 -> 360 | 1,841 -> 1,828 | 18 -> 16 |

- The isolated gain is 85 cycles, four frame accesses, 13 ROM bytes and two
  frame bytes per call, trading four extra register moves. Only the matching
  parent `calculateAndPrint3` changes: calculations-1 RA used ROM falls
  22,605->22,592 and total frame bytes 269->267, with maximum still 102.
  Calculations-2 stays at 41,119 ROM bytes and 1,234/225 total/maximum frame.
  The earlier historical maximum-frame tradeoff versus default remains.
- Extend the existing immutable policy matrix from 189 to 225 probes, using
  the same nine operations and 25 variants. These include retained ARG2 A/BC,
  full BC-preservation fallback, constant-one and type/constant/feature gates.
  Reuse the scalar-use family for 12 runtime controls: operand reversal,
  zero/one/255 constants, underflow/high-bit pairs, unassigned and BC results,
  and a 136-byte workspace. Its actual 148-byte frame exceeds the IX
  displacement range; assembly checks require borrow capture before the
  following carry-clobbering address addition. Independent host arithmetic
  checks unsigned word results over 64 boundary pairs and 512 total inputs.
  Nested word contexts are deliberate, not non-exercising byte promotions.
- Generated variants 18/19 save 89 cycles, four frame accesses, 14 ROM bytes
  and two frame bytes per call; 21/23/24/25 save 85/four/13/two. Each trades
  four extra moves. Unassigned-result variants 26-28 save 25 cycles and five
  ROM bytes, with unchanged frame traffic/size and one extra move. Variants
  20/22/29 are unchanged, retaining constant-step lowering or existing
  simplification. No isolated cycle/frame-traffic/per-function ROM/frame-size
  increase is measured elsewhere; moves are the explicit retained cost.
  All 22 real routine execution-metric records remain unchanged.
- Factor repeated opcode/result-location checks inside the existing shared
  classifier. Production changes are +22/-6 lines, **+16 net with no new
  production function or API**. Tests/runner changes are +40/-14, +26 net.
  No spill-layout, alias, global value or analysis framework is added.
  The frozen pre-consolidation compiler matches all 86 source/generator SSC
  files, 80 ROM files, eight static reports and 11,878 selected decision
  lines. Text comparisons normalize only randomized static-symbol IDs.
- Fresh GCC, native Release and native ASan compiler/verifier pairs pass,
  as do strict unoptimized C89/pedantic/Wall/Werror builds with the established
  calloc-warning exemption. Seven accepted and 32 immutable rejected cases,
  fixed-prefix/shared-suffix controls and atomic rollback remain qualified.
  Each final toolchain executes 445 generated entries and 22 real routines:
  227,840 generated plus 9,755 real cases, **237,595 per mode** at 512 inputs,
  in default/all-spill/RA modes. Return/buffer/DE/SP/stack guards and all
  14 exact calculations-2 VDP display rows pass. The frozen pre-feature
  compiler also passes the same common-source corpus.
- Native Release and ASan each match GCC's 80 ROM files, 30 reports and
  11,878 selected decision lines; 30 real-project traced/quiet assembly
  pairs match. All 48 default/no-ra/all-spill ROM files match the frozen
  control, as do their execution metrics. Both complete standard suites
  pass sequentially, 173 tests each, with staged compiler/linker SHA256
  checks before and after. The unchanged GCC linker is reused by hash;
  native tools are freshly built. All 217 ASan execution logs are clean,
  with real compiler/verifier runtime imports verified. ASan linker execution,
  leaks, other sanitizers, exhaustive paths and mapper/full-boot/rendering/
  hardware timing are not qualified; banked kernels still use fixed mapping.
- Reproduction artifacts remain under `/tmp/ra361`: exact pre-feature and
  pre-consolidation source snapshots, source/binary manifests, owner profile,
  broader constant-step trial/probes, strict/native tools, full six-cohort
  runner, comparison script/reports and full-suite logs/staging hashes.
  Use the pinned 2.3.0 emulator, `RA_CASES=512`, matching verifier and unset
  `RA_FAMILIES`/`RA_MATRIX_CASES` for all cohorts. `RA_BASELINE=1` relaxes
  runner quality gates only for the frozen control, not its allocator.
  Controls use the final standalone verifier for runner gates but their
  own embedded allocation/model code. Earlier partial runs are archived
  separately and are not final qualification. The stack-backed output
  index and preceding `r55` DIV-input spill remain; fixed-spill prefix
  rules and BC/DIV legality are unchanged. RA remains opt-in.

### Batch 364-366: Bound One Executed Index Transfer

- [x] **364. Attribute one remaining index cost.** Trace the unchanged
      calculations-1 source prefix and 58-read output loop. The escaped
      index itself still needs memory; BC preservation while loading a
      memory-backed pointer into IY is avoidable.
- [x] **365. Retain one profitable local trial.** Reuse `_materialize_to_hl`
      and `_load_hl_to_iy` for RA indexed reads through memory-backed pointers.
      Keep writes, constant-zero indices, physical pointer bases, default/
      `-no-ra` and forced-spill on their existing paths.
- [x] **366. Consolidate and requalify.** Inline the single-use old pointer
      materializer into its caller and remove its declaration/definition.
      Production delta is **+16/-21 lines, five net removed and one fewer
      function**, with no new helper, API, allocation policy or cache.

Qualification and measured outcome:

- The old `PUSH BC; LD C,(IY+0); LD B,(IY+1); LD IY,0; ADD IY,BC; POP BC`
  becomes `LD L,(IY+0); LD H,(IY+1); PUSH HL; POP IY`: **25 cycles and five
  ROM bytes saved per eligible read site**, with the same four machine-stack
  byte accesses. ARRAY_READ already reserves HL as scratch; the index is
  already in BC, and the existing final ADD IY,BC does not consume carry.
  Word-element address addition and signed/full-width indices remain covered.
  Writes are excluded because HL can hold their payload; constant-zero
  reads retain the old path rather than gaining unnecessary stack traffic.
  Nothing is cached across TACs, aliased stores, calls or port operations.

| Unchanged real-source control, one call | Executed cycles | Frame byte accesses | Byte-register moves | Function ROM | Frame bytes |
|---|---|---|---|---|---|
| `matrix_real_index_0` | 29,927 -> 29,927 | 834 -> 834 | 302 -> 302 | 1,692 -> 1,692 | 15 -> 15 |
| `matrix_source_loop_0` | 65,435 -> 63,985 | 1,886 -> 1,886 | 360 -> 360 | 1,828 -> 1,823 | 16 -> 16 |

- The output loop saves 1,450 cycles. Its required 58 index bytes and 116
  pointer-argument bytes remain; 232 BC-preservation bytes become 232
  HL-to-IY transfer bytes. This is **not a spill or frame-traffic reduction**.
  Calculations-1 used ROM falls 22,592->22,572 bytes; calculations-2 falls
  41,119->41,059. Total/maximum frames stay 267/102 and 1,234/225 respectively.
  Six real routines improve, including pathfinding by 824,000 aggregate
  cycles over 515 cases (1,600 per call). No isolated cycle/ROM/frame-size
  regression is measured; executed frame traffic and byte-register moves
  remain unchanged throughout the common-source comparison.
- Extend the existing index-copy family from 12 to 22 variants: zero/
  nonzero constants, a 136-byte workspace, aliased stores, a mutating call,
  a live word, signed byte/word indices, word elements/address carry and
  reassigned global pointers. Five probes deliberately use byte-index ABIs:
  full-width index preloading otherwise bypasses this memory-pointer path.
  The actual 149-byte-frame probe now selects the new lowering. Zero-index
  and retained full-width controls remain unchanged. The global case checks
  exactly the named adjacent two-byte pointer object, not a general guard
  relaxation. Test runner delta is +43/-13, 30 net lines; nine exact read
  opcode goldens change in six existing fixtures, with SSC source unchanged.
- Full qualification exposed a pre-existing, layout-dependent out-of-bounds
  path: `continue` inside a switch targeted its exit, not the enclosing loop.
  With explicit user approval, prerequisite commit `6be036d` fixes nearest-
  loop switch continuation and do-loop condition continuation, and rejects
  switch-only continuation. Two independent nested-loop/switch controls pass
  corrected tools; old default execution fails an independent oracle. The CFG
  fixture checks rejection in all three modes. This is a separate semantic
  correction, **not an allocator gain**. Corrected baseline/trial compiler
  sources differ from the final compiler only in pass-6 lowering.
- Fresh GCC, native Release and native ASan compiler/verifier pairs, strict
  GCC C89/pedantic/Wall/Werror builds with the established calloc exemption,
  immutable verifier/rollback controls and all 14 exact VDP display rows pass.
  Each final toolchain covers **457 generated entries and 22 real routines:
  233,984 generated + 9,755 real = 243,739 cases per mode**, in default/
  all-spill/RA modes, checking return/buffer/ports/DE/SP/stack guards.
  Full six-cohort runs preceded the five byte-index probe
  refinements; exact generator/hash proofs retain 25 unaffected cohort
  sources across five compiler variants, and all five values cohorts were
  then replaced by fresh runs. Other source and binary hashes are unchanged.
- Native Release and ASan each match GCC's 80 ROMs, 30 reports and 12,030
  selected decisions; cleanup matches 80 ROMs, eight static reports and
  12,030 decisions. All 86 common SSC sources, 48 flags-off ROMs/execution
  metrics and 30 traced/quiet real-project assembly pairs match their controls.
  Both complete standard suites pass sequentially, **173 tests each**, with
  staged compiler/linker hashes unchanged. All 217 final ASan logs are clean;
  compiler/verifier runtime imports are verified. ASan linker execution,
  leaks, other sanitizers, exhaustive paths and mapper/full-boot/rendering/
  hardware timing remain unqualified; banked kernels use fixed mapping.
- Reproduction artifacts under `/tmp/ra364` retain corrected baseline/trial
  snapshots, 58-input/12-binary manifests, owner profiles, cohort proofs,
  comparison reports and suite logs/staging hashes. Use the pinned 2.3.0
  emulator and `RA_CASES=512`; control-only `RA_BASELINE=1` relaxes quality
  gates, never execution correctness. Redundant suite mirrors are removed.
  Fixed-spill-prefix rules, alias safety and BC/DIV legality are unchanged.
  RA remains opt-in.

### Batch 367-369: Bound Remaining Address-Setup Work

- [x] **367. Attribute one executed address-setup cost.** The unchanged
      58-read source loop rebuilds IY from DE for its pointer slot even
      though IX already equals DE. Required index/pointer loads remain.
- [x] **368. Reuse one existing path if profitable.** Use existing IX loads
      and HL-to-IY transfer only for RA nonzero-index memory-pointer reads
      with a valid existing IX frame-base cache and a policy-approved
      displacement. No new cache, policy, helper or API is introduced.
- [x] **369. Consolidate and requalify.** Remove one single-use address
      helper, prove pre-consolidation output identity and pass the full
      execution, native/sanitizer and standard-suite gates below.

Qualification and measured outcome:

- Replace `LD IY,offset; ADD IY,DE; LD L,(IY+0); LD H,(IY+1)` with
  `LD L,(IX+offset); LD H,(IX+offset+1)`: **29 cycles and six ROM bytes
  saved per eligible read site**, without changing memory accesses or
  byte-register moves. Reuse `g_is_ix_de` and the current target's
  `FRAME_DISPLACEMENT` decision, not a second validity/bounds implementation.
  IX is neither modified nor initialized by the new path. BC retains the
  index; HL is already ARRAY_READ scratch. The final ADD IY,BC computes the
  same address/carry and does not consume flags from the removed setup.
- Writes, constant-zero indices, invalid/uncached IX, global pointers,
  out-of-range pointer slots and physical pointer bases retain existing
  paths. Default/`-no-ra` and forced-spill remain byte-identical to the
  frozen `cf38104` compiler. No value is retained across aliasing stores,
  calls, labels or port operations beyond existing allocator contracts.

| Unchanged real-source control, one call | Executed cycles | Frame byte accesses | Byte-register moves | Function ROM | Frame bytes |
|---|---|---|---|---|---|
| `matrix_real_index_0` | 29,927 -> 29,927 | 834 -> 834 | 302 -> 302 | 1,692 -> 1,692 | 15 -> 15 |
| `matrix_source_loop_0` | 63,985 -> 62,303 | 1,886 -> 1,886 | 360 -> 360 | 1,823 -> 1,817 | 16 -> 16 |

- The output loop saves **1,682 cycles** by removing two setup instructions
  per iteration. Its 58 index bytes, 116 pointer bytes and 232 HL-to-IY
  transfer bytes remain; this is not a spill/stack-traffic reduction.
  Calculations-1 used ROM falls 22,572->22,548; calculations-2 falls
  41,059->41,017. Total/maximum frames remain 267/102 and 1,234/225.
  Across common sources, 69 generated entries and five real routines improve,
  including the RAM-manager workload by 119,741 aggregate cycles. No isolated
  cycle/ROM/frame-size regression or executed frame-traffic/move change is
  measured. Real-routine totals are aggregate case measurements, not per-call
  gains.
- Extend the existing index-copy family from 22 to 25 variants with 114/115/
  116-byte workspaces. Actual pointer slots **-127/-128 use IX; -129 uses IY**,
  with unchanged frames of 129/130/131 bytes. Runner assertions require the
  exact slot/mode/folded-offset trace and reject IX use in zero-index,
  initially uncached and global-pointer fallbacks. Independent buffer/return
  arithmetic covers these boundaries plus existing aliases, mutating calls,
  signed/full-width indices, word elements and low-address carry.
- Fold the single-use HL pointer-dereference wrapper into its caller, reusing
  `_inc_hl`. Production changes are **+16/-11 lines, five net added and one
  fewer function**; no production function is added. The runner grows by ten
  net lines (+15/-5). Seven exact read-opcode goldens in six existing fixtures
  save six bytes each; executable SSC source and all other tag bytes match.
  Two existing trace assertions now require IX displacement rather than IY.
- Fresh optimized GCC, native Release and native ASan compiler/verifier
  pairs and strict unoptimized GCC C89/pedantic/Wall/Werror builds pass, with
  the established calloc exemption. Immutable verifier failures, fixed-prefix/
  shared-suffix slots and atomic rollback remain qualified. Each final
  toolchain executes **460 generated entries and 22 real routines:
  235,520 generated + 9,755 real = 245,275 cases per mode**, in default/
  all-spill/RA modes. All six cohorts are fresh full runs, with independent
  return/buffer/ports/DE/SP/stack guards and all 14 exact VDP display rows.
- Native Release and ASan each match GCC's 80 ROMs, 30 reports and 12,090
  selected decisions; pre-consolidation cleanup matches 80 ROMs, eight static
  reports and 12,090 decisions. All 86 common SSC sources, 48 flags-off
  ROMs/execution metrics and 30 traced/quiet real-project assembly pairs
  match their controls. All 217 final ASan logs are clean and real compiler/
  verifier runtime imports are verified. ASan linker execution, leaks, other
  sanitizers, exhaustive paths and mapper/full-boot/rendering/hardware timing
  remain unqualified; banked kernels use fixed mapping.
- Both complete standard suites pass sequentially, **173 tests each**, with
  six staged compiler/linker hashes unchanged before/after each run.
  Qualification artifacts under `/tmp/ra367` retain baseline/trial snapshots,
  60-input/13-binary manifests,
  exact owner profiles, golden proofs, comparison reports and suite staging
  hashes. Use the pinned 2.3.0 emulator and `RA_CASES=512`; control-only
  `RA_BASELINE=1` relaxes quality gates, never execution correctness.
  The redundant suite mirror is removed; logs and staging hashes remain.
  Fixed-spill-prefix rules, alias safety and BC/DIV legality are unchanged.
  RA remains opt-in.

### Batch 370-372: Bound One Remaining Memory-to-Register Copy

- [x] **370. Attribute one executed copy cost.** Follow ordinary assignments
      in the unchanged real-source prefix before the output loop. Separate
      required pointer/ABI work from redundant setup or scratch transfers.
- [x] **371. Reuse existing assignment/materialization logic if profitable.**
      Retain at most one bounded trial, proving width, overlap, carry and
      alias/call safety without adding caches or a new lowering framework.
- [x] **372. Consolidate and requalify.** Remove superseded wrappers/paths,
      prove cleanup identity and isolated tradeoffs, and apply the shared
      gates before a local-only commit.

**Evidence and bounded change:**

- Freeze `ca9bff5` and its 47 compiler C/H inputs. The unchanged source prefix
  executes 26 word-pointer assignments to HL. Each uses two IY memory reads
  through A, with redundant `LD L,A` / `LD H,A` transfers. Reuse the existing
  `_materialize_to_hl` and `_store_from_hl` in the full-word assignment branch,
  loading L/H directly from the same addresses in the same order. Each eligible
  copy saves **eight cycles, two ROM bytes and two register moves**, not memory
  traffic. Source-address setup, flags and allocation/A-clobber policy stay
  unchanged. BC-to-HL assignment transport, constants, byte widening/truncation
  and memory/BC destinations retain their previous paths.

| Unchanged source, one call | Cycles before -> after | Frame byte accesses | Register moves before -> after | ROM bytes before -> after | Frame bytes |
| --- | --- | --- | --- | --- | --- |
| `matrix_real_index_0` | 29,927 -> 29,719 | 834 -> 834 | 302 -> 250 | 1,692 -> 1,640 | 15 -> 15 |
| `matrix_source_loop_0` | 62,303 -> 62,095 | 1,886 -> 1,886 | 360 -> 308 | 1,817 -> 1,765 | 16 -> 16 |

- The 26 copies themselves fall **1,950->1,742 cycles and 52->0 moves**.
  The output loop's 58 index bytes, 116 pointer bytes and 232 HL-to-IY transfer
  bytes remain. No spill or frame-traffic reduction is claimed. Calculations-1
  used ROM falls **22,548->22,444**; calculations-2 falls **41,017->40,951**.
  Total/maximum frames remain 267/102 and 1,234/225. Sixteen generated entries
  and nine measured real routines improve without an isolated cycle/ROM/frame
  regression. The RAM-manager workload saves 1,477,048 aggregate cycles across
  its cases, not per call; every measured cycle delta is four times its
  register-move delta, with unchanged frame traffic.
- Extend the existing index-copy family from 25 to 30 variants: a full-width
  pointer parameter, a 136-byte workspace, aliased store/reread, mutating call
  and call-bounded global pointer. The pointer join forces an actual **-150**
  source slot in a 152-byte frame; the contributing helper call forces an
  actual GLOBAL word load. These are traced memory-to-HL assignments, not
  copy-forwarded proxies. Reuse the independent buffer/return arithmetic and
  exact named two-byte global-pointer snapshot without relaxing stack/buffer
  guards. Existing signed, numeric, widening/truncation and displacement
  controls remain. Drop four non-exercising numeric trial probes rather than
  enlarge the existing 16-KiB matrix ROM: final default matrix/value images use
  16,215/14,664 bytes respectively, both within their unchanged 16,384-byte bank.
- Remove the single-use BC-to-HL get-address wrapper, preserving its guarded
  H-before-L copy order inline. Production changes are **+9/-11 lines, two net
  removed and one fewer function**; no new production helper, API, cache or
  policy. The runner grows by thirteen net lines (+23/-10). Seven exact
  eight-to-six-byte opcode substitutions in five existing golden fixtures
  match the actual ROMs; executable SSC source and all other tag bytes match.
- Strict unoptimized GCC C89/pedantic/Wall/Werror builds pass with the
  established calloc exemption, as do fresh optimized GCC, native Release and
  native ASan compiler/verifier builds and verifier execution. Each final
  toolchain executes **465 generated entries and 22 real routines:
  238,080 generated + 9,755 real = 247,835 cases per mode**, in default/
  all-spill/RA modes. All six cohorts are fresh full runs, with independent
  return/buffer/ports/DE/SP/stack guards and all 14 exact VDP display rows.
  Immutable verifier failures, fixed-prefix/shared-suffix spill slots and
  atomic rollback remain qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,184
  selected decisions**. Pre-consolidation cleanup matches 80 ROMs, eight static
  reports and all 12,184 decisions; the baseline's selected decisions also
  match. All 86 common SSC sources, 48 flags-off ROMs/execution metrics and
  30 traced/quiet real-project assembly pairs match their controls. Both
  complete standard suites pass sequentially, **173 tests each**, with six
  staged compiler/linker hashes unchanged before/after each run.
- All 217 final ASan logs are clean and compiler/verifier runtime imports are
  verified. ASan linker execution, leaks, other sanitizers, exhaustive paths
  and mapper/full-boot/rendering/hardware timing remain unqualified; banked
  kernels use fixed mapping. Artifacts under `/tmp/ra370` retain the baseline/
  pre-consolidation snapshots, 62-input/13-binary manifests, exact owner
  profiles, golden proofs, comparisons and suite staging hashes. Use the
  pinned 2.3.0 emulator and `RA_CASES=512`; control-only `RA_BASELINE=1` relaxes
  quality gates, never execution correctness. The redundant suite mirror is
  removed; logs and hashes remain. Alias safety, BC/DIV legality and opt-in
  RA behavior are unchanged.

### Batch 373-375: Bound Remaining Ordinary Word-Load Address Setup

- [x] **373. Attribute the remaining setup.** Follow ordinary word loads in
      the unchanged source prefix, separating required memory reads from IY
      reconstruction and checking actual cached-frame validity and word span.
- [x] **374. Reuse existing frame-address policy if profitable.** Retain at
      most one measured trial without new caches or a lowering framework;
      prove displacement boundaries and uncached/global/alias/call fallbacks.
- [x] **375. Consolidate and requalify.** Remove only demonstrated duplication,
      prove cleanup identity and isolated tradeoffs, then apply the shared
      gates before a local-only commit.

**Evidence and bounded change:**

- Freeze `71b8791` and its 47 compiler C/H inputs. Of 26 ordinary word-pointer
  copies in each unchanged source kernel, **25 already have IX=DE**. Reuse the
  existing cache validity, full-word target policy and IX byte-load helpers
  instead of rebuilding IY. Each eligible site removes `LD IY,offset` /
  `ADD IY,DE`: **29 simulated cycles, six ROM bytes and two instructions**.
  Both memory reads retain their addresses/order; register moves and frame
  traffic do not change. The first, uncached copy retains its original setup.
- Gate the path on opt-in/non-forced-spill RA, a physical HL destination,
  full-word source/result types and existing FRAME_DISPLACEMENT policy.
  Pending physical reloads keep their checked path; do not bypass their error
  handling. Global/wide/uncached sources, BC and memory destinations,
  constants and byte widening/truncation remain unchanged. No flag-retention
  or allocation/A-clobber policy is relaxed.

| Unchanged source, one call | Cycles before -> after | Frame byte accesses | Register moves | ROM bytes before -> after | Frame bytes |
| --- | --- | --- | --- | --- | --- |
| `matrix_real_index_0` | 29,719 -> 28,994 | 834 -> 834 | 250 -> 250 | 1,640 -> 1,490 | 15 -> 15 |
| `matrix_source_loop_0` | 62,095 -> 61,370 | 1,886 -> 1,886 | 308 -> 308 | 1,765 -> 1,615 | 16 -> 16 |

- The 26 copy sites fall **1,742->1,017 cycles**, exactly 25 removed setups;
  no spill reduction is claimed. The output loop's 58 index bytes, 116 pointer
  bytes and 232 HL-to-IY transfer bytes remain. Calculations-1 used ROM falls
  **22,444->22,144**; calculations-2 falls **40,951->40,807**. Total/maximum
  frames remain 267/102 and 1,234/225. Twelve generated entries and eight
  measured real routines improve without an isolated cycle/ROM/frame
  regression. All executed cycle deltas are multiples of 29, with unchanged
  frame traffic and moves. The RAM-manager workload saves **5,013,955 aggregate
  cycles** across its cases, not per call.
- Reuse the existing pointer-join/workspace template for three variants:
  actual word slots **-127/-128 use IX; -129 uses IY**. Existing -150/global/
  initially uncached controls remain fallbacks. A twelve-case table in the
  existing verifier checks both local/spill kinds at -129/-128/-127 and
  +126/+127/+128, independently proving the high-byte limit. The runner
  requires this table in traced and quiet verifier execution and exact slot/
  mode traces; qualification also checks the actual two-byte IX/IY assembly.
  Buffer/return, alias/call, ABI and stack guards remain independent and unchanged.
- The forced-spill value image has only 386 free bytes. Place the three new
  boundary variants in the **existing scalar cohort**, not a larger ROM bank
  or a new cohort. Scalar default/all-spill images use 13,831/13,957 bytes;
  value images remain 14,664/15,998, all within their existing 16,384-byte bank.
  After this test-only rebalance, rerun scalar/value cohorts. Retain the other
  four completed cohorts only after **16 exact source-input proofs** and
  byte-identical runtime/oracle/report code; failed-layout artifacts remain
  separate. No semantic test cases are discarded.
- Inline the single-use IY-label and location-size wrappers, preserving
  instruction text, call order and nonpositive-width behavior. Production
  changes are **+17/-21 lines, four net removed and two fewer functions**;
  no new production helper, API, cache or policy. The runner grows by thirteen
  net lines (+20/-7); the existing verifier gains 22 lines and no function.
  Two exact twelve-to-six-byte golden replacements in two existing fixtures
  match actual ROMs. All executable fixture source and other instructions/
  tag bytes match the frozen control.
- Fresh optimized GCC, native Release and native ASan compiler/verifier
  builds and verifier execution pass. Strict unoptimized GCC C89/pedantic/
  Wall/Werror builds pass with the established calloc exemption. Each final
  toolchain qualifies **468 generated entries and 22 real routines:
  239,616 generated + 9,755 real = 249,371 cases per mode**, in default/
  all-spill/RA modes, across six cohorts. All 14 exact VDP display rows and
  independent return/buffer/ports/DE/SP/stack guards pass. Immutable verifier
  failures, fixed-prefix/shared-suffix spill slots and atomic rollback remain
  qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,262
  selected decisions**. Pre-consolidation cleanup matches 80 ROMs, eight static
  reports and all 12,262 decisions; baseline decisions also match. All 86
  common SSC sources, 48 flags-off ROMs/execution metrics and 30 traced/quiet
  real-project assembly pairs match. Both full standard suites pass
  sequentially, **173 tests each**, with six staged tool hashes unchanged
  before/after each run. All 217 final ASan logs are clean and actual compiler/
  verifier runtime imports are verified.
- Artifacts under `/tmp/ra373` retain snapshots, **442-input/13-binary**
  manifests, owner profiles, golden/cohort proofs, comparisons and suite
  hashes; the completed suite mirror is removed. Use the pinned 2.3.0 emulator
  and `RA_CASES=512`; control-only `RA_BASELINE=1` relaxes quality gates, never
  execution correctness. ASan linker execution, leaks, other sanitizers,
  exhaustive paths and mapper/full-boot/rendering/hardware timing remain
  unqualified; banked kernels use fixed mapping. Alias safety, BC/DIV legality
  and opt-in RA behavior are unchanged.

### Batch 376-378: Attribute the First Uncached Word Load

- [x] **376. Measure the remaining uncached setup.** Follow the first word
      load in the unchanged source kernels and distinguish its own cost from
      later frame-address reconstruction and required memory/ABI accesses.
- [x] **377. Reuse existing initialization only if profitable.** Retain at
      most one measured trial using existing address/cache helpers, without
      hoisting across clobbers or weakening width, reload or fallback checks.
- [x] **378. Consolidate and requalify.** Remove demonstrated duplication,
      prove cleanup identity and baseline compatibility, then apply the shared
      gates before a local-only commit.

**Evidence and bounded change:**

- Freeze `88b19c5` and its 47 compiler C/H inputs. The first word-pointer copy
  in each unchanged source kernel still needs address initialization. Reuse
  `_load_location_address_to_ix` in the existing word-assignment path instead
  of requiring an already-valid IX cache. It initializes IX=DE when necessary,
  skips setup only when the existing cache is valid, and supplies the folded
  displacement to the existing trace and L/H byte-load helpers.
- The cold copy itself remains **67 simulated cycles**: replacing IY setup
  with IX setup has equal cost. All 26 copy sites together remain **1,017
  cycles**, with zero register moves. The benefit is a later frame-cache
  reconstruction that is no longer needed: **29 cycles and six ROM bytes
  per source kernel**, not an intrinsically cheaper first memory read.

| Unchanged source, one call | Cycles before -> after | Frame byte accesses | Register moves | ROM bytes before -> after | Frame bytes |
| --- | --- | --- | --- | --- | --- |
| `matrix_real_index_0` | 28,994 -> 28,965 | 834 -> 834 | 250 -> 250 | 1,490 -> 1,484 | 15 -> 15 |
| `matrix_source_loop_0` | 61,370 -> 61,341 | 1,886 -> 1,886 | 308 -> 308 | 1,615 -> 1,609 | 16 -> 16 |

- The output loop's 58 index bytes, 116 pointer bytes and 232 HL-to-IY
  transfer bytes remain. Calculations-1 used ROM falls **22,144->22,132**;
  calculations-2 falls **40,807->40,795**. Total/maximum frames remain 267/102
  and 1,234/225. Ten generated entries and four measured real routines improve,
  with no isolated cycle/ROM/frame regression. Executed frame traffic and
  register moves are unchanged; cycle deltas are multiples of 29. The RAM
  manager workload saves **340,344 aggregate cycles** across its cases, not
  per call.
- Keep opt-in/non-forced-spill RA, physical HL, full-word source/result types,
  pending-reload exclusion and the existing FRAME_DISPLACEMENT policy. Word
  offsets remain **-128 through +126**, so both bytes fit. Do not hoist setup
  across calls, labels, ASM or DE/IX clobbers. Global/wide sources, BC and
  memory destinations, constants and byte widening/truncation retain their
  old routes. No allocation, A-clobber or flag-retention policy is relaxed;
  replacing ADD IY,DE with ADD IX,DE does not claim identical hardware flags.
- Reuse the existing generated cases and cohorts, with no new test function
  or executable source changes. Require actual cold IX initialization and
  the **-6/-5** byte-return parameter slot in variant 25. Scope variant 29's
  fallback assertion to its required GLOBAL load: its separate local alias
  word load can legitimately use IX. The actual GLOBAL trace still targets
  IY. Existing alias/call/stack guards, -150 and -129 IY fallbacks,
  -127/-128 IX controls and the independent twelve-case local/spill word-span
  table remain qualified.
- Remove the single-use SP-to-HL wrapper; its sole caller reuses the existing
  constant-to-HL helper and preserves the same ADD HL,SP instruction/order.
  Production changes are **+8/-12 lines, four net removed and one fewer
  function**. No new production helper, API, cache or policy. The runner grows
  by five net lines (+8/-3). Two golden blocks in one existing fixture each
  lose six bytes of later IX reconstruction; equal-cost cold initialization,
  executable SSC text and every other normalized instruction/tag byte are
  independently checked against the frozen control.
- Fresh optimized GCC, native Release and native ASan compiler/verifier
  builds and verifier executions pass. Strict unoptimized GCC C89/pedantic/
  Wall/Werror builds pass with the established calloc exemption. Each final
  toolchain executes fresh full runs of all six cohorts: **468 generated
  entries and 22 real routines; 239,616 generated + 9,755 real = 249,371 cases
  per mode**, in default/all-spill/RA modes. Independent returns, buffers,
  ports, DE/SP and protected stack, immutable verifier failures, stable fixed
  spill prefixes/shared suffixes and atomic rollback remain qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,262
  selected decisions**. Pre-consolidation cleanup matches 80 ROMs, eight static
  reports and all 12,262 decisions; baseline decisions also match. All 86
  common SSC sources, 48 flags-off ROMs/execution metrics and 30 traced/quiet
  real-project assembly pairs match. All 14 exact VDP display rows pass.
  Both full standard suites pass sequentially, **173 tests each**, with six
  staged tool hashes unchanged before/after each run. All 217 final ASan logs
  are clean and actual compiler/verifier runtime imports are verified.
- Separate an existing banked static-counter limitation from actual moves.
  Listing-relative ROM lookup reports 556->567 and 1,013->1,017 static move
  sites, while unchanged emitted register-copy instructions and independently
  bank-aware listing bytes both give **665->665 and 1,200->1,200**. Every
  checked instruction's first byte matches the bank-aware ROM location.
  Dynamic moves are read from actual execution memory and remain unchanged;
  do not present the relocation-sensitive static counter as a move saving or
  regression. Leave its correction to the bounded follow-up below.
- Artifacts under `/tmp/ra376` retain baseline/pre-consolidation snapshots,
  **442-input/13-binary** manifests, exact first-copy/owner profiles, golden
  and banked-listing proofs, comparisons and suite staging hashes. The
  completed suite mirror is removed; logs and hashes remain. Use the pinned
  2.3.0 emulator and `RA_CASES=512`; control-only `RA_BASELINE=1` relaxes quality
  gates, never execution correctness. ASan linker execution, leaks, other
  sanitizers, exhaustive paths and mapper/full-boot/rendering/hardware timing
  remain unqualified; banked kernels use fixed mapping. Alias safety, BC/DIV
  legality and opt-in RA behavior are unchanged.

### Batch 379-381: Qualify Banked Static Move Attribution

- [x] **379. Attribute the banked counter discrepancy.** Separate listing
      bank/offset fields, actual emitted register-copy instructions and dynamic
      moves; freeze controls before changing measurement.
- [x] **380. Reuse existing address conventions.** Retain at most one bounded
      counter correction with independent banked and bank-zero opcode proofs,
      no new production helper and no code-generation or quality-gate shortcut.
- [x] **381. Consolidate and requalify.** Require unchanged ROMs, allocation
      decisions and execution metrics; isolate intentional static-report
      corrections, apply the shared gates and make a local-only commit.

**Evidence and bounded change:**

- Freeze `52489b0`, 442 source/test inputs and 699 already-qualified control
  artifacts from `/tmp/ra376/gcc512`. All compiler C/H and executable test
  inputs match that control exactly. Reuse eleven hash-verified tool binaries,
  including GCC/strict GCC, native Release and native ASan compiler/verifier
  binaries; do not claim fresh compiler builds for this measurement-only
  change. Fresh full execution runs use the normal quality gates, with no
  `RA_BASELINE` relaxation.
- The listing contains bank, slot, CPU address and bank-relative offset, not
  a whole-ROM offset in its final address field. For example, bank 1 / slot 2
  / CPU 9446 / offset 1446 refers to ROM **5446**, not 1446. Reuse the existing
  SMS runner's **bank * 0x4000 + offset** convention. Parse the bank once and
  reuse it for the unchanged bank/CPU attribution key.
- Extend the existing address match to capture the first emitted opcode byte.
  Read the actual ROM byte with bounds-checked `Buffer.readUInt8` and require
  it to match that listing byte before counting. A mismatch names the image
  and listing row; out-of-range lookup fails explicitly. Keep the existing
  unprefixed byte-register-copy classification and report schema unchanged.
  No new helper, decoder, fixture, generated function, cohort or production
  code is added. The runner changes **+6/-3 lines, three net added**; the small
  increase provides the per-instruction consistency check, not new allocator
  machinery.

| Existing project/mode | Old static counter -> corrected | Emitted code |
| --- | --- | --- |
| calculations-1 default / no-ra | 429 -> 545 | unchanged |
| calculations-1 all-spill | 442 -> 545 | unchanged |
| calculations-1 RA / quiet RA | 567 -> 665 | unchanged |
| calculations-2 default / no-ra | 903 -> 1,017 | unchanged |
| calculations-2 all-spill | 898 -> 1,017 | unchanged |
| calculations-2 RA / quiet RA | 1,017 -> 1,200 | unchanged |

- These are corrected measurements, **not added moves or a speed/ROM
  optimization**. All thirty bank-zero generated image counts are unchanged.
  Independent checks cover **40 listings and 254,894 instruction bytes**:
  bank-aware ROM lookup agrees with listed bytes, and move totals agree with
  instruction-text counts, including inline sign-extension comments.
  Both real projects cover banks 0/1/2/3 in all five modes. The first text
  oracle missed commented register copies; preserve that failed attempt and
  correct the oracle, not production behavior or quality gates.
- Focused controls prove identical ROMs, symbols, bank maps, attribution maps
  and every other static field. Deliberately restoring the old lookup still
  passes bank zero but is rejected in both banked projects by the new opcode
  check. A separate out-of-range mutation is rejected by the bounds check.
  No compiler/emitter/cache, flag, ABI, spill-layout or allocation policy
  changes; no golden bytes need updating.
- Each final toolchain runs all six cohorts afresh: **468 generated entries
  and 22 real routines; 239,616 generated + 9,755 real = 249,371 cases per
  mode**, in default/all-spill/RA modes. Independent returns, full buffers,
  ports, DE/SP and protected stack, immutable verifier failures, fixed spill
  prefixes/shared suffixes and atomic rollback remain qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,262
  selected decisions**. Against the frozen control, all 80 ROMs, 86 common
  SSC sources, 12,262 decisions and all execution/quality/display/coverage
  reports match. Only ten move-counter fields in two existing static reports
  change; all other fields in all eight static reports match. All 30
  traced/quiet real-project assembly pairs and 14 exact VDP display rows pass.
  Both full standard suites pass sequentially, **173 tests each**, with six
  staged compiler/linker hashes unchanged before/after each run. All 217 final
  ASan logs are clean; instrumented compiler/verifier binaries are unchanged.
- Fresh owner profiles also match the baseline: prefix/output-loop costs
  remain **28,965/61,341 simulated cycles**, with 834/1,886 frame accesses and
  250/308 register moves. The 26 word copies remain 1,017 cycles, including
  the 67-cycle cold copy. The loop still has 58 index bytes, 116 pointer bytes
  and **232 HL-to-IY transfer bytes**; these are not claimed as allocator
  spill traffic or eliminated work.
- Artifacts under `/tmp/ra379` retain frozen baseline/current inputs, eleven
  binary hashes, 699 control-output hashes, focused mutation proofs,
  instruction-byte/text comparisons, owner profiles and suite staging hashes.
  Remove only the completed suite mirror; retain logs and proofs. Pinned
  emulator 2.3.0 and `RA_CASES=512` remain in use. ASan linker execution, leaks,
  other sanitizers, exhaustive paths and mapper/full-boot/rendering/hardware
  timing remain unqualified; banked kernels use fixed mapping. Opt-in RA,
  alias safety and BC/DIV legality are unchanged.

### Batch 382-384: Attribute Remaining Indexed-Read Transfers

- [x] **382. Measure the remaining transfer.** Follow the unchanged source
      loop's HL-to-IY traffic, separating required index/pointer reads from
      address transport and checking widths, destinations and live values.
- [x] **383. Reuse existing byte-load lowering only if profitable.** Retain
      at most one measured trial without new caches or helpers; preserve
      signed/full-word, alias/call, pending-reload and flag constraints.
- [x] **384. Consolidate and requalify.** Remove only demonstrated duplication,
      prove cleanup identity and isolated tradeoffs, apply the shared gates
      and make a local-only commit.

**Evidence and bounded change:**

- Freeze `9d319c8`, 47 compiler C/H files and 442 source/test inputs before
  the trial. The output loop has 58 unsigned indexed reads: 58 index bytes
  and 116 pointer bytes are required, while **232 temporary stack-byte
  accesses** only transport the computed address from HL to IY. Its indexed
  region costs 7,366 simulated cycles and 522 instructions. These accesses
  are address transport, not allocator spills.
- Keep the existing materialized HL address for unsigned-byte pointer reads
  in opt-in/non-forced-spill RA, excluding constant-zero indices and bases
  already materialized in IY. Eligible bases are physical HL or memory-held
  pointers. Reuse existing pointer materialization and `ADD HL,BC`, then
  emit `LD L,(HL)` through the existing byte-result path. Replace only
  `PUSH HL / POP IY / ADD IY,BC / LD L,(IY+0)`; the new pair saves
  **41 cycles, six ROM bytes and four stack-byte accesses per read**.
- Signed elements, full-word elements, constant-zero indices, direct array
  bases and pre-materialized BC bases retain their previous routes.
  Unsigned widening, result destinations, checked base spill/reload ordering,
  wide/global pointer-slot loads, alias/call constraints and frame-span checks
  remain intact. ARRAY_READ already conservatively clobbers full HL, BC, A,
  IX, IY and flags; no effect, allocation, spill, cache or flag policy changes.
- Generalize the existing array-base materializer with a `keep_hl` argument,
  consolidate its three HL-to-IY transfers into one tail and remove the
  now-unused transfer wrapper. Array writes and early BC materialization
  explicitly retain IY. Production changes **+39/-31 lines, eight net added
  and one fewer function**; do not claim a net source-line reduction.
  Four runner lines require the measured loop's direct HL byte-read sequence.
  No new helper, cache, generated function, cohort or framework is added.

| Existing kernel, one call | Cycles before -> after | Frame accesses | Moves | Used ROM bytes | Frame bytes |
| --- | --- | --- | --- | --- | --- |
| source prefix `ra_matrix_real_index_0` | 28,965 -> 28,965 | 834 -> 834 | 250 -> 250 | 1,484 -> 1,484 | 15 -> 15 |
| output loop `ra_matrix_source_loop_0` | 61,341 -> 58,963 | 1,886 -> 1,654 | 308 -> 308 | 1,609 -> 1,603 | 16 -> 16 |

- The 58 reads save **2,378 cycles and 232 address-transport accesses per
  output-loop call**. Their indexed region falls to 4,988 cycles and 406
  instructions, or 127 -> 86 cycles per read. Required index/pointer reads
  and register moves are unchanged. All 26 word copies still cost 1,017
  cycles, including the 67-cycle cold copy.

| Existing RA project | Used ROM before -> after | Static register-copy sites | Total / maximum frame bytes |
| --- | --- | --- | --- |
| calculations-1 | 22,132 -> 22,108 (-24) | 665 unchanged | 267 / 102 unchanged |
| calculations-2 | 40,795 -> 40,543 (-252) | 1,200 unchanged | 1,234 / 225 unchanged |

- **83 generated entries and 12 real routines improve**, with no cycle,
  frame-access, ROM or frame-size regression. Every dynamic delta has zero
  move change and exactly four fewer frame-byte accesses per 41 cycles
  saved; every ROM reduction is a multiple of six. All 40 static image
  register-copy totals remain unchanged against the corrected bank-aware
  baseline. The heap workload saves **7,443,550 cycles and 726,200 frame
  accesses across its cases**, not per invocation; array, sorting, record,
  RAM allocate/release, actor, tile, path and banked calculations also improve.
- Update 16 golden blocks in eleven existing fixtures, removing **108 tagged
  bytes**. Actual linked opcodes prove only the four-to-two-instruction
  replacement above; all other normalized instructions, untouched tags and
  executable SSC text match the frozen baseline. Normalize only the touched
  reload fixture's working-tree CRLF back to its repository LF; leave
  unrelated files alone.
- Reuse four existing reload-fixture functions for additional artifact-only
  execution controls: constant-zero unsigned bytes, full words, signed-byte
  sums widened to signed words, and unsigned indexed split reloads.
  Remove only executable `.DB` test markers in these controls. Each function
  runs 1,024 cases per baseline/final compiler: **4,096 per compiler, 8,192
  extra executions**. Independent arithmetic, complete read-only buffers,
  ports and ABI/stack guards pass. Zero/word/signed functions retain identical
  normalized instruction streams; no persistent test function is added.
- Fresh optimized GCC and native Win32 Release/ASan builds and verifier
  executions pass. Strict unoptimized GCC C89/pedantic/Wall/Werror builds
  pass with the established calloc exemption. Each final toolchain runs all
  six cohorts afresh: **468 generated entries and 22 real routines; 239,616
  generated + 9,755 real = 249,371 cases per mode**, in default/all-spill/RA
  modes. Independent returns, full buffers, ports, DE/SP, protected stack,
  immutable verifier failures, stable fixed spill prefixes/shared suffixes
  and atomic rollback remain qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,262
  selected decisions**. Pre-consolidation cleanup matches 80 ROMs, eight
  static reports and all decisions; baseline decisions also match. All 86
  common SSC sources, 48 flags-off ROMs and default/all-spill execution
  metrics are unchanged. All 30 traced/quiet real-project assembly pairs
  and 14 exact VDP display rows pass. All 217 final ASan logs are clean and
  actual compiler/verifier runtime imports are verified.
- Both full standard suites pass sequentially, **173 tests each**, with six
  compiler/linker hashes unchanged before/after each run. GCC's staged tools
  are intentionally replaced for the following native run; the final native
  staged copies and both source toolchains also match their recorded hashes.
- Artifacts under `/tmp/ra382` retain baseline/pre-consolidation snapshots,
  442-input/13-binary manifests, exact owner profiles, opcode/golden and typed
  control proofs, full comparisons and suite staging hashes. Retain logs and
  proofs; remove only the completed suite mirror. Pinned emulator 2.3.0 and
  `RA_CASES=512` remain in use. ASan linker execution, leaks, other sanitizers,
  exhaustive paths and mapper/full-boot/rendering/hardware timing remain
  unqualified; banked kernels use fixed mapping. Opt-in RA and BC/DIV
  legality are unchanged.

### Batch 385-387: Attribute Constant-Zero Byte-Read Transport

- [x] **385. Attribute the excluded zero-index path.** Measure remaining
      address transport for constant-zero unsigned-byte reads, separating
      base materialization, live values and result widths.
- [x] **386. Retain one measured reuse only if profitable.** Reuse existing
      lowering without new helpers or caches; preserve signed/word reads,
      checked reloads, alias/call and full-register/flag constraints.
- [x] **387. Consolidate and requalify.** Reuse shared lowering without
      duplication, requiring cleanup identity for any separate consolidation,
      isolated measured gains and the shared completion gates before a
      local-only commit; leave unproven alternatives deferred.

**Evidence and bounded change:**

- Freeze `cc9115d`, 47 compiler C/H files and 442 source/test inputs before
  the trial. Constant-zero index materialization already emits **no BC
  initialization**. Attribute only actual pointer loading and address
  transport, not an invented ten-cycle index setup.
- Extend the existing unsigned-byte `keep_hl` path to constant-zero pointer
  reads with **three condition replacements, +3/-3 backend lines and zero
  net growth**. No function, helper, cache or policy is added. The existing
  shared base materializer already handles the required routes, so no
  independent cleanup rewrite is necessary.

| Existing zero-read route | Replacement | Saved simulated cycles / ROM bytes |
| --- | --- | --- |
| retained/reloaded HL | `PUSH HL / POP IY / LD L,(IY+0)` -> `LD L,(HL)` | 37 / 5 |
| cold/global IY-loaded pointer | load BC, reconstruct IY, indexed byte read -> load L/H from the same IY slot, `LD L,(HL)` | 41 / 8 |
| valid cached IX pointer slot | reconstruct IY from DE, load BC, reconstruct IY again -> load L/H directly from IX, `LD L,(HL)` | 70 / 14 |

- The first route removes four temporary stack-byte accesses per read,
  **not allocator spill traffic**. The other routes retain both required
  pointer-byte reads and their frame traffic. Zero reads emit no `ADD HL,BC`.
  Existing full-slot displacement checks, checked spill/reload ordering,
  unsigned widening and result stores remain in use. Signed/full-word reads,
  direct arrays, physical BC bases and nonzero indexed reads keep their
  previous routes; wide/global slots may improve their final byte fetch
  without gaining an invalid IX displacement.
- Keep opt-in/non-forced-spill gating, conservative ARRAY_READ effects,
  partial/full-register and flag safety, alias/call rules, allocation
  decisions and fixed spill prefixes/shared suffixes unchanged.

| Existing kernel, one call | Cycles before -> after | Frame accesses | Moves |
| --- | --- | --- | --- |
| `ra_matrix_index_copy_12` | 235 -> 194 | 3 -> 3 | 2 -> 2 |
| `allocatorRaReadReloadZero` | 403 -> 329 | 18 -> 10 | 1 -> 1 |
| `allocatorRaCoreCandidateNonzero` | 550 -> 480 | 16 -> 16 | 0 -> 0 |
| source prefix `ra_matrix_real_index_0` | 28,965 -> 28,801 | 834 -> 834 | 250 -> 250 |
| output loop `ra_matrix_source_loop_0` | 58,963 -> 58,799 | 1,654 -> 1,654 | 308 -> 308 |

- Both source kernels save **164 cycles through four existing
  `convertNumbersToChars16` calls**, not through another optimization of the
  nonzero indexed loop. Their read-only zero-index TAC groups cost
  652 -> 488 cycles, with sixteen frame accesses unchanged. Keep zero-index
  writes out of this attribution.

| Existing RA project | Used ROM before -> after | Static register-copy sites | Total / maximum frame bytes |
| --- | --- | --- | --- |
| calculations-1 | 22,108 -> 22,086 (-22) | 665 unchanged | 267 / 102 unchanged |
| calculations-2 | 41,212 -> 41,168 (-44) | 1,245 unchanged | 1,273 / 225 unchanged |

- **Four generated entries and seven real routines improve**, with no
  cycle, frame-access, ROM or frame-size regression. The tile workload saves
  434,070 cycles across its cases, not per call. All dynamic savings decompose
  into the measured 37/41/70-cycle routes; register moves remain unchanged.
  The real routines also appear in the root quality report: do not count
  those duplicate rows as additional generated improvements.
- Prove all **80 generated/real assembly files** differ only by the three
  permitted zero-read replacements. Per-function and whole-image ROM deltas
  equal their exact five/eight/fourteen-byte site totals; all 40 static image
  register-copy totals remain unchanged. Regenerate **23 golden blocks in
  eighteen existing fixtures, removing 152 tagged bytes**; linked opcodes,
  untouched tags and executable SSC source are independently checked.
- Reuse five existing fixture routines for **10,240 extra old/final
  executions**, covering zero reads, reloads, signed/full-word controls,
  nonzero indices and cached-IX reads with write side effects. Signed/full-word functions
  remain opcode-identical. Add only a positive cold-zero sequence check and
  clarify the existing uncached/global negative guard in the CI runner;
  no input, oracle, cohort or quality gate is weakened.
- Fresh optimized/strict GCC and native Release/ASan compiler/verifier builds
  pass, including the strict GCC build with the established calloc warning
  exemption. Each toolchain runs all six cohorts: **468 generated entries
  and 25 real routines; 239,616 generated + 11,300 real = 250,916 cases per
  mode**, in default/all-spill/RA modes. Independent returns, full buffers,
  ports, DE/SP, protected stack, immutable verifier failures and atomic
  rollback remain qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,286
  selected decisions**. Against the baseline, all 86 common SSC sources,
  48 flags-off ROMs, default/all-spill execution and flags-off static metrics,
  and all selected allocation decisions match. All 30 traced/quiet
  real-project assembly pairs and 14 exact VDP display rows pass, including
  the three committed assembly workloads. All 217 ASan logs are clean;
  actual compiler/verifier runtime imports are verified.
- Both full standard suites pass sequentially, **173 tests each**, with six
  compiler/linker hashes unchanged before/after each run. GCC staging is
  intentionally replaced before native staging; each run's snapshots and
  the final native staged binaries match their recorded source tools.
- Artifacts under `/tmp/ra385` retain input/tool hashes, original-output
  audits, exact owner profiles, opcode/golden and typed-control proofs,
  comparisons and suite staging hashes. Retain logs/proofs and remove only
  the completed suite mirror. Pinned emulator 2.3.0 and `RA_CASES=512`
  remain in use. ASan uses the Release linker; ASan linker execution, leaks,
  other sanitizers, exhaustive paths and mapper/full-boot/rendering/hardware
  timing remain unqualified. Banked kernels use fixed mapping.

### Batch 388-390: Reuse Physical-BC Bases With HL Indices

- [x] **388. Measure the remaining BC-base path.** Check the planned
      constant-zero case against candidate eligibility, then attribute the
      user-approved reachable BC-pointer/full-word-HL-index reads.
- [x] **389. Reuse existing lowering only if profitable.** Keep at most one
      measured trial without helpers or caches; preserve full-register,
      signed/word, alias/call, reload and flag constraints.
- [x] **390. Consolidate and requalify.** Avoid duplicate lowering, require
      cleanup identity where applicable and isolated measured gains, apply
      shared completion gates and make a local-only commit.

**Evidence and bounded change:**

- Freeze `f58cd59`, 47 compiler C/H files, 442 source/test inputs and the
  previous full corpus. The candidate policy permits an assignment-produced
  pointer in BC only when the array index is already retained in HL;
  other pointer producers use HL. A constant-zero index cannot select that
  BC case. Find no constant-zero BC reads in the corpus; do not add dead
  lowering or change allocator eligibility to manufacture a demonstration.
- The user approves revising this batch to the two reachable BC-pointer/
  HL-index reads in `calculationHeapWorkload`. Their byte elements use a
  full-word index. The old route transfers BC to IY through the stack, copies
  the HL index into BC, then adds and reads through IY:

  ```
  PUSH BC / POP IY / LD C,L / LD B,H / ADD IY,BC / LD L,(IY+0)
    -> ADD HL,BC / LD L,(HL)
  ```

- Reuse the existing address addition and byte-result path. Six instructions,
  67 simulated cycles and ten bytes become two instructions, **18 cycles
  and two bytes**: save **49 cycles, eight ROM bytes, four temporary
  stack-byte accesses and two register copies per read**. This is address
  transport, not allocator spill elimination; required pointer/index loads
  and result stores remain unchanged.
- Reuse the existing `base_in_hl` flag rather than adding state or helpers.
  Resolve element width before choosing the route, retain BC/HL through
  index materialization, then use the shared addition/read/widen/store code.
  Production changes **+15/-12 lines: three net added, no new function**.
  Moving and simplifying the existing element-width lookup avoids duplicated
  type logic; no independent cleanup rewrite needs a separate identity claim.
- Gate the route on opt-in/non-forced-spill RA, unsigned-byte elements,
  a BC pointer, a two-byte HL index and signed/unsigned full-word index type.
  Preserve byte-index extension, signed/full-word element fallbacks, checked
  preserve/reload calls and their errors, alias/call rules, full-register/
  flag effects and allocator/cache/placement policy. No BC/HL operand swap
  or unchecked reclassification of pending transfers is introduced.

| Existing/new control, one call | Cycles before -> after | Frame accesses | Register copies |
| --- | --- | --- | --- |
| new BC-base byte regression | 494 -> 445 | 14 -> 10 | 6 -> 4 |
| widened byte result control | 558 -> 509 | 17 -> 13 | 6 -> 4 |
| heap, seed `4219`, count 0 | 359,986 -> 359,986 | 7,226 -> 7,226 | 698 -> 698 |
| heap, seed `4219`, count 16 | 1,172,807 -> 1,165,947 | 27,238 -> 26,678 | 2,720 -> 2,440 |

- The maximum-count heap call executes 140 eligible reads, saving
  **6,860 cycles, 560 stack-byte accesses and 280 copies**. Empty/single/
  maximum-count owner profiles isolate both TAC sites and account for every
  cycle/access delta. Across all 515 heap inputs the savings are
  **1,601,418 cycles, 130,728 accesses and 65,364 copies**, not per-call
  figures. The existing bank-one display calculation also saves 6,860
  cycles; all fourteen display rows, including assembly workloads, match.
- Exactly **two real routines improve**; generated kernels, converters,
  source-prefix/output-loop costs and every other measured routine remain
  unchanged. Every dynamic delta is a multiple of 49 cycles with exactly
  four fewer stack-byte accesses and two fewer copies per read. No cycle,
  traffic, move, ROM or frame-size regression is observed.

| Existing RA project | Used ROM before -> after | Static register-copy sites | Total / maximum frame bytes |
| --- | --- | --- | --- |
| calculations-1 | 22,086 -> 22,086 | 665 -> 665 | 267 / 102 unchanged |
| calculations-2 | 41,168 -> 41,152 (-16) | 1,245 -> 1,241 | 1,273 / 225 unchanged |

- The heap function alone shrinks **5,146 -> 5,130 bytes**, with its
  61-byte frame unchanged. Prove all **80 assembly files** differ only by
  the six-to-two-instruction replacement at the two heap sites in RA and
  quiet RA. Per-function/image ROM and static-copy deltas equal those sites'
  exact eight-byte/two-copy savings; all other static fields match.
- Reuse the existing pointer-base fixture: retain its four old golden blocks
  unchanged, add one byte-read function and its **57-byte fifth golden**,
  and require the actual BC-pointer/two-byte-HL-index trace. The baseline
  compiler still passes the old four blocks but rejects the new one:
  65 bytes versus 57. Both GCC/native full fixture makefiles pass.
- Reuse the CI execution/counter helpers for **12,288 extra old/final
  executions** of six controls. Independent returns, whole buffers including
  sink writes, ports, DE/SP and stack guards cover byte/widened results,
  high-word and negative signed indices, byte indices, signed elements and
  word elements. Signed-element, byte-index and word-element function
  opcodes remain identical. No cohort, oracle or quality gate is weakened.
- Fresh optimized/strict GCC and native Release/ASan compiler/verifier builds
  pass; strict GCC retains the established calloc warning exemption.
  Each toolchain runs all six cohorts: **468 generated entries and 25 real
  routines; 239,616 generated + 11,300 real = 250,916 cases per mode**, in
  default/all-spill/RA modes. Immutable verifier failures, fixed spill
  prefixes/shared suffixes and atomic rollback remain qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,286
  selected decisions**. All 86 common SSC sources and selected decisions match.
  **76 ROM artifacts** remain identical; only calculations-2 RA/quiet RA
  and their listing-enabled duplicates change. All flags-off execution/static
  metrics, 30 traced/quiet assembly pairs and 14 exact VDP rows match.
  All 217 ASan logs are clean; compiler/verifier runtime imports are verified.
- Both full standard suites pass sequentially, **173 tests each**, with six
  compiler/linker hashes unchanged before/after each run. GCC staging is
  intentionally replaced before native staging; verify each run's snapshots,
  both source toolchains and the final native staged copies separately.
- Artifacts under `/tmp/ra388` retain frozen inputs/tools, typed/golden
  controls, owner profiles, full comparisons and rejected first attempts.
  Profile only copied images; all 699 original corpus-output hashes remain
  intact. Remove only the completed suite mirror; preserve unrelated changes
  and both existing untracked ROMs. Pinned emulator 2.3.0 and `RA_CASES=512`
  remain in use. ASan uses the Release linker; ASan linker execution, leaks,
  other sanitizers, exhaustive paths and mapper/full-boot/rendering/hardware
  timing remain unqualified. Banked kernels use fixed mapping.

### Batch 391-393: Attribute Remaining Full-Word Indexed Reads

- [x] **391. Measure word-read address transport.** Attribute remaining
      full-word indexed reads, separating required bytes, scaling and result
      assembly from pointer/index transfers and temporary stack traffic.
- [x] **392. Reuse existing lowering only if profitable.** Keep at most one
      measured trial without helpers or caches; preserve signed/byte reads,
      alias/call, full-register, reload and flag constraints.
- [x] **393. Consolidate and requalify.** Avoid duplicate lowering, require
      cleanup identity where applicable and isolated measured gains, apply
      shared completion gates and make a local-only commit.

**Evidence and bounded change:**

- Freeze `7827c3e`, 47 compiler C/H files, 442 source/test inputs and all
  699 previous corpus outputs. Existing unsigned-byte reads already retain
  an HL address, while nonzero full-word reads still transfer that address
  to IY, scale the index with two indexed additions and load both bytes:

  ```
  PUSH HL / POP IY / ADD IY,BC / ADD IY,BC / LD L,(IY+0) / LD H,(IY+1)
    -> ADD HL,BC / ADD HL,BC / LD A,(HL) / INC HL / LD H,(HL) / LD L,A
  ```

- Both sequences execute six instructions. The new sequence costs
  **46 rather than 93 simulated cycles and six rather than thirteen ROM
  bytes**: save **47 cycles, seven bytes and four temporary stack-byte
  accesses per eligible read**. Loading the low byte into A before changing
  HL preserves the address until the high byte is read. This deliberately
  adds **one register copy per read**, rather than claiming a move-count
  improvement or allocator spill elimination.
- Extend the existing `base_in_hl` eligibility to signed/unsigned full-word
  elements only for nonzero indices. Reuse base materialization, checked
  preserve/reload calls, index extension, both existing HL additions,
  `_inc_hl`, and the established get-address pointer-word load sequence.
  Result storage is unchanged. Production changes **+16/-5 lines, eleven
  net added and no new function/helper/state**. No independent cleanup
  rewrite needs a separate identity claim.
- Keep opt-in/non-forced-spill gating. Constant-zero word reads, BC-base
  word reads, direct-array/IY routes and all byte paths remain unchanged.
  Word scaling still adds the normalized BC index twice; do not double an
  already materialized pointer or destroy the low-byte address before the
  high-byte read. Existing ARRAY_READ effects already clobber A/BC/HL/IX/IY
  and flags. Alias/call, full-register, pending-transfer, cache/span,
  allocator placement and fixed spill-layout policies remain unchanged.

| Existing/control kernel, one call | Cycles before -> after | Frame accesses | Register copies |
| --- | --- | --- | --- |
| `ra_matrix_index_copy_3` | 380 -> 333 | 12 -> 8 | 0 -> 1 |
| signed-word/signed-index control | 380 -> 333 | 12 -> 8 | 0 -> 1 |
| constant-zero control `ra_matrix_index_copy_2` | 251 -> 251 | 4 -> 4 | 0 -> 0 |

- **Four generated entries and four real routines improve**. Across its
  515 cases, the actor workload saves **3,910,917 cycles and 332,844
  stack-byte accesses**, with 83,211 added copies. The record workload saves
  **775,359 cycles and 65,988 accesses**, with 16,497 added copies.
  These are corpus totals, not per-call figures. Existing bank-two/bank-three
  display calculations save 14,946/3,008 cycles respectively. Converters,
  source-prefix/output-loop kernels and all other measured routines are
  unchanged. Real routines also appear in the root quality report; count
  those rows only once.

| Existing RA project | Used ROM before -> after | Static register-copy sites | Total / maximum frame bytes |
| --- | --- | --- | --- |
| calculations-1 | 22,086 -> 22,086 | 665 -> 665 | 267 / 102 unchanged |
| calculations-2 | 41,152 -> 41,061 (-91) | 1,241 -> 1,254 (+13) | 1,273 / 225 unchanged |

- Accept the measured register-copy tradeoff explicitly; no cycle, ROM,
  frame-access or frame-size regression is observed. Every dynamic delta
  equals 47 saved cycles/four saved accesses/one added copy per read, with
  **executed instruction counts unchanged**. All 80 assembly files differ
  only by the replacement above: 38 sites across traced/quiet output.
  Per-function/image byte and static-copy deltas match exact site totals;
  all other static fields match.
- Regenerate **five golden blocks in five existing fixtures, removing
  42 tagged bytes**. Prove actual linked bytes, untouched tags and unchanged
  executable SSC text. The old compiler passes the reload fixture's other
  three blocks but rejects its word block: 107 bytes versus 93.
  All ten focused makefile runs pass: five fixtures under GCC and native
  Release. No new repository test function, cohort or production helper is added.
- Reuse existing CI execution/counter helpers for **12,288 extra old/final
  executions**. Controls cover two word reloads, negative/full-width signed
  word indices and signed word values, unchanged byte/signed-byte reads,
  zero indices and cached-IX read/write side effects. Independent returns,
  full buffers, ports, DE/SP and stack guards pass; unchanged fallback
  functions remain opcode-identical.
- Fresh optimized/strict GCC and native Release/ASan compiler/verifier builds
  pass; strict GCC keeps the established calloc warning exemption. Each
  toolchain runs all six cohorts: **468 generated entries and 25 real
  routines; 239,616 generated + 11,300 real = 250,916 cases per mode**,
  in default/all-spill/RA modes. Immutable verifier failures, fixed spill
  prefixes/shared suffixes and atomic rollback remain qualified.
- Native Release and ASan each match GCC's **80 ROMs, 30 reports and 12,286
  selected decisions**. All 86 common SSC sources, 48 flags-off ROMs,
  flags-off execution/static metrics and selected allocation decisions match
  the baseline. All 30 traced/quiet assembly pairs and 14 exact VDP display
  rows pass. All 217 ASan logs are clean; actual compiler/verifier runtime
  imports are verified. No oracle or quality gate is weakened.
- Both full standard suites pass sequentially, **173 tests each**, with six
  compiler/linker hashes unchanged before/after each run. GCC staging is
  intentionally replaced before native staging; verify each run's snapshots,
  both source toolchains and final native staged copies separately.
- Artifacts under `/tmp/ra391` retain frozen inputs/tools, opcode/golden
  and typed-control proofs, complete comparisons and rejected first tooling
  attempts. Keep all original corpus-output hashes intact; remove only the
  completed suite mirror. Preserve unrelated worktree changes and both
  existing untracked ROMs. Emulator 2.3.0 and `RA_CASES=512` remain pinned.
  ASan uses the Release linker; ASan linker execution, leaks, other
  sanitizers, exhaustive paths and mapper/full-boot/rendering/hardware timing
  remain unqualified. Banked kernels use fixed mapping.

### Batch 394-396: Attribute Constant-Zero Word Reads

- [x] **394. Measure the excluded zero-index word path.** Separate required
      pointer/data bytes from address reconstruction, temporary stack traffic
      and result assembly, including cold and cached slots.
- [x] **395. Reuse existing word lowering only if profitable.** Keep at most
      one measured trial without helpers or caches; preserve byte/signed,
      alias/call, full-register, reload and flag constraints.
- [x] **396. Consolidate and requalify.** Avoid duplicate lowering, require
      cleanup identity where applicable and explicit measured tradeoffs,
      apply shared completion gates and make a local-only commit.

**Evidence and bounded change:**

- Freeze `43f3a9d`, its qualified O3 compiler, 47 compiler C/H files,
  450 tracked inputs and all 699 previous corpus outputs. Requalify the
  control against the same full corpus as the final compilers. Constant-zero
  index materialization already emits no BC initialization: the savings
  below remove address reconstruction, not a fictional index load.
- Remove only the zero-index exclusion from the existing signed/unsigned
  word-read eligibility. Reuse `base_in_hl`, base materialization, checked
  preserve/reload calls and the existing word-load sequence:

  ```
  PUSH HL / POP IY / LD L,(IY+0) / LD H,(IY+1)
    -> LD A,(HL) / INC HL / LD H,(HL) / LD L,A
  ```

  This retained/reloaded-HL kernel costs **24 rather than 63 simulated
  cycles and four rather than nine ROM bytes**: save **39 cycles, five
  bytes and four temporary stack-byte accesses**. Cold memory-pointer
  slots additionally reuse the existing HL pointer load, saving **43
  cycles/eight bytes**; an already cached, in-range IX slot saves **72
  cycles/fourteen bytes and two instructions**. Required pointer/data reads
  remain. Every affected read adds **one register copy** (`LD L,A`).
- Production changes are **one replaced condition, zero net lines and no
  new function/helper/state**. There is no separate cleanup rewrite needing
  an identity claim. Keep opt-in/non-forced-spill gating; byte/signed-byte,
  nonzero word, direct-array/IY and BC-base word paths stay unchanged.
  Existing ARRAY_READ effects already cover A/BC/HL/IX/IY and flags.
  Placement, alias/call barriers, full-register constraints, transfers,
  address-cache/span rules and fixed spill-layout policy are unchanged.

| Independent control, one call | Cycles before -> after | Frame accesses | Register copies |
| --- | --- | --- | --- |
| existing three-word reload routine, one zero-index read | 728 -> 689 | 25 -> 21 | 2 -> 3 |
| cold word-pointer argument | 265 -> 222 | 4 -> 4 | 0 -> 1 |
| cached word-pointer argument, with a preceding write | 498 -> 426 | 12 -> 12 | 0 -> 1 |
| wide local-pointer slot, beyond IX displacement | 690 -> 647 | 14 -> 14 | 0 -> 1 |

- Reuse the CI execution helpers for ten 1,024-input old/final controls
  (**20,480 executions per pass**). Independent returns, complete buffers,
  ports, DE/SP and stack guards pass. Cover signed word values, unchanged
  signed/full-width indices, byte/signed-byte fallbacks, reloads, cached
  read/write side effects and a wide slot across an inline-asm barrier.
  Entire typed images differ only by one retained, two cold and two cached
  word-read replacements.
- **Four real routines improve; no generated entry changes.** Across 515
  cases each, record/actor workloads save **1,046,565/1,996,332 cycles**
  and **107,340/204,752 stack-byte accesses**, adding 26,835/51,188 copies.
  Bank-two/bank-three display calls save 7,644/4,056 cycles. These are corpus
  totals, not per-call gains; do not count duplicated root quality rows
  twice. The shared quick 16-bit converter also shrinks statically, without
  an observed dynamic gain in the measured converter cases.

| Existing RA project | Used ROM before -> after | Static register-copy sites | Total / maximum frame bytes |
| --- | --- | --- | --- |
| calculations-1 | 22,086 -> 22,078 (-8) | 665 -> 666 (+1) | 267 / 102 unchanged |
| calculations-2 | 41,061 -> 40,993 (-68) | 1,254 -> 1,267 (+13) | 1,273 / 225 unchanged |

- Accept the explicit copy-count tradeoff; no measured cycle, frame-access,
  frame-size or ROM regression occurs. All **80 assembly files** differ
  only by **28 approved replacements** across traced/quiet output:
  24 retained and four cold. Exact per-function/image byte and copy deltas
  match those sites; all other static fields match. Dynamic deltas match
  the measured read costs and instruction counts.
- Refresh **29 golden blocks in 21 existing fixtures, removing 173 tagged
  bytes**, with unchanged executable SSC text and untouched tags. The old
  reload image passes three blocks but rejects the new word block
  (93 versus 88 bytes). Twelve representative GCC/native make runs pass.
  No new repository test function or cohort is added.
- Fresh optimized/strict GCC and native Release/ASan compiler/verifier
  qualification passes; the O3 `-Wall -Werror` compiler build has no
  diagnostics or warning exemptions. Each final toolchain runs all six
  cohorts: **468 generated entries and 25 real routines, 250,916 cases per
  mode** (239,616 generated + 11,300 real), in default/all-spill/RA modes.
  Immutable verifier failures, fixed spill prefixes/shared suffixes and
  atomic rollback remain qualified.
- Release and ASan each match GCC's **80 ROMs, 30 reports and 12,286 selected
  decisions**. All 86 common SSC sources, 48 flags-off ROMs, flags-off
  execution/static metrics and selected decisions match the frozen control.
  All 30 traced/quiet assembly pairs, 14 exact display rows and 217 ASan
  logs pass; compiler/verifier ASan imports are verified.
- Both full standard suites pass sequentially, **174 tests each**, with
  six source/staged compiler/linker hashes unchanged before/after each run.
  Preserve all original input/output hashes outside the documented scope,
  unrelated changes and both existing untracked ROMs. Artifacts under
  `/tmp/ra394` retain controls, opcode/golden proofs and qualification logs;
  remove only the completed suite mirror and generated patch. Emulator
  2.3.0 and `RA_CASES=512` remain pinned. ASan uses the Release linker;
  ASan linker execution, leaks, other sanitizers, exhaustive paths and
  mapper/full-boot/rendering/hardware timing remain unqualified. Banked
  kernels use fixed mapping.

### Profile-Guided Z80 Instruction Selection: Constant Word Multiplication

The bounded allocator is complete. Instead of another allocator micro-batch,
profile common instructions and improve existing Z80 lowering where measured
gains justify it. Instruction selection remains target-local; this change adds
no speculative SM83/6502 abstractions.

- Profile 25 existing routines with 740 cases per mode, including 32 seeded
  inputs and existing edge/display cases. In RA mode, constant word-multiply
  regions consume **25,458,542 of 137,790,829 cycles (18.5%)**. Factor six,
  especially struct-array addressing, dominates; three, five, seven, eleven
  and thirteen also matter. The copy helper is not reached in these paths,
  so adding a block-copy optimization is not justified by this profile.
- Reuse `_multiply_bc_and_de_to_hl` for a literal on either side. Normalize
  the constant to 16 bits, copy the multiplicand into HL, then use binary
  doubling/addition with `ADD HL,HL` and `ADD HL,BC` or `ADD HL,DE`. Emit
  the chain only when its two copies and additions fit the original
  **18-byte kernel**. The selected chain takes at most 184 T-states versus
  1,020-1,116 for the old loop. Zero and oversized chains retain the old
  fallback; existing constant folding is unchanged.
- Preserve signed/unsigned low-word results, operand materialization, DE
  frame protection, conservative clobbers and allocation decisions. Variable
  multiplication, byte multiplication, division and modulo are unchanged.
  This is **general Z80 code generation**, intentionally improving default,
  `-no-ra`, forced-spill and RA output; the allocator itself remains opt-in.
  Add no production function and remove the caller's duplicated old assembly
  explanation: the production delta is **32 added / 34 removed lines**.
  The same-flags GCC host executable grows by 512 bytes, not a size saving.
- Recheck all existing workload oracles with `RA_CASES=512`. RA-mode totals
  across the 25 routines fall from **1,932,048,598 to 1,597,754,186 cycles
  (-17.30%)**. Eleven real routines improve; none regresses. Representative
  whole-routine reductions are record **59.77%**, actor **19.32%**, heap
  **18.22%**, sort **10.80%** and path **10.25%**. These are emulated
  workload measurements, not whole-game or hardware speedup claims.
- `calculations-2` saves **756 used ROM bytes in every mode**: default
  49,746 -> 48,990, forced spill 51,204 -> 50,448, and RA 40,993 -> 40,237.
  `calculations-1` is unchanged. Frame sizes, frame/stack traffic and all
  other execution metrics except instructions, cycles and register copies
  are unchanged. Accept two extra register copies per selected multiply;
  `calculations-2` has 126 additional static copy sites. No measured function
  or image grows, and no measured cycle or instruction count regresses.
- Add 33 generated cases, both operand orders, signed/full-word boundaries,
  computed/retained inputs, exact-budget chains and dense/variable fallbacks,
  checked independently with `Math.imul`. Place them in the smaller literal
  cohort to keep the matrix bank within 16 KB. Add a seven-tag byte fixture
  covering signed-byte widening too. Refresh 26 existing golden tags in
  13 fixtures, removing 364 expected bytes without changing executable SSC.
- Exhaustively execute six actual old/new emitted kernels over all 65,536
  word inputs: **786,432 kernel executions** pass. Another **1,536
  signed-byte whole-function executions** pass. Only the inline byte-test
  marker data is replaced with same-size NOPs for whole-function execution;
  kernel and ordinary golden checks use the unmodified bytes.
- Fresh strict GCC O3 (`-ansi -pedantic -Wall -Werror`), native Release and
  native ASan compiler/verifier qualification passes. Each runs all six
  cohorts: **501 generated entries and 25 real routines, 267,812 cases per
  mode** (256,512 generated + 11,300 real) in default/all-spill/RA modes.
  Both full standard suites pass sequentially, **175 tests each**, with
  source/staged compiler/linker hashes unchanged.
- Release and ASan each match GCC's **80 ROMs, 30 reports and 12,682 selected
  decisions**. Baseline/final comparison proves 86 identical SSC sources,
  unchanged decisions, and **455 approved replacements among 575 kernels
  in 80 assembly files**, with no other assembly changes. All 48 final
  traced/quiet assembly pairs, 14 exact display rows and 217 ASan logs pass.
  Compiler/verifier sanitizer imports are verified; ASan uses the Release
  linker with leak detection disabled. ASan linker execution, other
  sanitizers, exhaustive whole-program paths, mapper/full-boot/rendering
  and hardware timing remain unqualified; banked kernels use fixed mapping.
  Preserve unrelated inputs and both existing user ROMs. Controls, profiles,
  opcode/golden proofs and qualification logs are retained under
  `/tmp/z80-patterns-1`; emulator 2.3.0 remains pinned.

### Profile-Guided Z80 Instruction Selection: Constant Word Shifts

Use `9ed0876` as the frozen control, rather than resuming optional allocator
micro-batches. The same 25-routine, 740-case/mode profile attributes
**7,772,555 of 113,788,617 RA cycles (6.83%)** to constant word shifts.
Left shifts by three and right shifts by four/eight dominate; variable
shifts account for only 568,243 cycles and are not changed.

- Consolidate the two existing word-shift helpers into `_shift_hl_by_bc`.
  For resolved constant counts, use `ADD HL,HL`, byte transfers for shifts
  of eight or more, and `SRL L` once the high byte is known zero. Unroll only
  while the entire kernel fits the original **8-byte left / 11-byte right**
  loop; use a bounded `DJNZ` loop otherwise. Omit the now-unused constant
  `LD BC,count`. Every selected kernel fits its old budget, and every
  complete constant-shift sequence saves at least that three-byte load.
- Preserve the old low-16-bit count representation: zero is a no-op and
  normalized counts of 16 or more produce zero. Keep existing logical
  right-shift behavior, including signed-word inputs, and existing byte
  promotion/sign extension. Variable-count assembly, byte-shift lowering,
  multiplication/division/modulo, frame protection, conservative clobbers
  and allocation policy are unchanged. This is common Z80 code generation
  in every mode; the allocator remains opt-in. Keep instruction selection
  target-local rather than introducing speculative SM83/6502 machinery.
- Remove the duplicated caller assembly explanations too. The production
  change is **46 added / 55 removed lines, one fewer function**, with no new
  production helper. The same-flags GCC host executable grows from 854,510
  to 855,022 bytes (+512); smaller source does not imply a smaller host EXE.
- Full `RA_CASES=512` qualification reduces the existing 25-routine RA
  aggregate from **1,597,754,186 to 1,518,743,199 cycles (-4.95%)**.
  Thirteen real routines and 42 generated entries improve; none regresses.
  Whole-routine reductions include the converter **22.95%** in both
  projects, actor **14.24%**, tile **7.81%**, stencil **3.69%**, sort **3.56%**
  and record **3.30%**. Bank display routines improve **2.65%, 12.13% and
  4.88%**. These are emulated execution-corpus measurements, not whole-game
  or hardware speedups.

| Existing RA project | Used ROM before -> after | Static register-copy sites | Frame sizes / frame and stack traffic |
| --- | --- | --- | --- |
| calculations-1 | 22,078 -> 22,015 (-63) | -6 | unchanged |
| calculations-2 | 40,237 -> 39,692 (-545) | -59 | unchanged |

- The same 63/545-byte savings apply in default, explicit `-no-ra` and
  forced-spill modes. No measured function/image grows; no cycle or
  instruction count regresses. All execution metrics other than instructions,
  cycles and register copies match the control; dynamic copies decrease.
- Add **38 generated cases** in the existing literal cohort: left/right
  counts 0-17, signed/full-word boundaries, computed/retained inputs and two
  variable-count controls covering counts through 256. Check shifts with
  independent multiplication/division oracles, not JavaScript's masked shift
  operators. Reuse the existing 16-bit shift-count fixture for eight new
  byte blocks, including signed-byte widening and the full-width count 256.
  Refresh **17 existing golden blocks in seven fixtures, removing 137
  expected bytes**, without changing their executable SSC. Add no test file.
- Exhaustively execute **36 actual old/new kernels over all 65,536 word
  inputs: 4,718,592 executions pass**, including zero-count no-op kernels.
  Verify exact byte budgets, result, preserved DE/IX/IY/SP, forbidden
  writes/ports and non-regressing cycles. Another **23,040 whole-function
  executions** pass, including all signed-byte values. Replace only inline
  byte-marker data with same-size NOPs for whole-function execution;
  kernel/golden checks use unmodified code. The old compiler passes both
  existing variable-count tags and rejects all eight new constant tags.
- Fresh strict GCC O3 (`-ansi -pedantic -Wall -Werror`), native Release and
  native ASan compiler/verifier gates pass. Each runs all six cohorts:
  **539 generated entries and 25 real routines, 287,268 cases per mode**
  (275,968 generated + 11,300 real), in default/all-spill/RA modes. Both
  full standard suites pass sequentially, **175 tests each**, with source
  and staged compiler/linker hashes unchanged.
- Release and ASan each match GCC's **80 ROMs, 30 reports and 13,138 selected
  decisions**. Baseline/final comparison proves 86 identical SSC sources,
  unchanged decisions, and **610 approved constant replacements among
  735 word-shift kernels in 80 assembly files**. All other assembly,
  including variable shifts, is unchanged. All 48 final traced/quiet
  assembly pairs, 14 exact display rows and 217 ASan logs pass; compiler
  and verifier sanitizer imports are verified. Preserve unrelated inputs
  and both existing user ROMs. Artifacts under `/tmp/z80-patterns-2` retain
  profiles, controls, exhaustive/opcode/golden proofs and qualification logs.
  Emulator 2.3.0 remains pinned. ASan uses the Release linker with leak
  detection disabled; ASan linker execution, other sanitizers, exhaustive
  whole-program paths and mapper/full-boot/rendering/hardware timing remain
  unqualified. Banked kernels use fixed mapping.

### Profile-Guided Z80 Instruction Selection: General Word Multiplication

The frozen `ce5c728` profile now identifies variable word multiplication as
the largest classified arithmetic region: **13,272,836 of 108,024,514 RA
cycles (12.29%)**, across the same 25 routines and 740 cases per mode.

- Reuse `_multiply_bc_and_de_to_hl`, replacing only its generic fallback.
  Save the high multiplier byte in A, use B as the 16-iteration counter,
  and shift the multiplier through A:C using `SLA C` / `RLA`. Double the
  partial product with `ADD HL,HL`, conditionally add DE and use `DJNZ`.
  This removes 32 `EX DE,HL` instructions per multiplication and replaces
  `DEC A` / `JR NZ` with the existing Z80 counted-loop idiom.
- The generic kernel shrinks **18 -> 15 bytes**. Its exact cost changes
  from `1020 + 6*popcount(input BC)` to `784 + 6*popcount(input BC)`:
  **236 T-states and 47 executed instructions saved for every input**,
  about 21-23% of the old kernel's cycles. Preserve signed/unsigned
  low-word results, argument materialization, DE frame protection and
  conservative clobbers. Byte multiplication, division, modulo and shifts
  are unchanged; allocation policy and decisions are unchanged too.
- Keep the original **18-byte constant-chain budget**, even though the
  generic fallback is now smaller. Tightening it would regress previously
  profitable sparse constants such as 511 and 32769. All existing selected
  chains remain byte-identical; dense constants use the improved fallback.
  This is common Z80 code generation in every mode, not another allocator
  feature. Keep `-ra` opt-in and future SM83/6502 instruction selection local.
- Add no production function, policy or cache. The production delta is
  **9 added / 11 removed lines**; the same-flags strict GCC host executable
  remains **855,022 bytes**, with no host-size growth.
- Full `RA_CASES=512` qualification reduces the existing RA workload
  aggregate from **1,518,743,199 to 1,476,429,579 cycles (-2.79%)**.
  Fourteen real routines and 13 generated entries improve; none regresses.
  Whole-routine reductions include multiply-add **16.61%**, sort **4.02%**,
  heap **3.90%**, array **3.09%**, path **3.02%**, actor **2.70%** and
  record **2.62%**. These are emulated corpus results, not whole-game or
  hardware timing claims.

| Existing RA project | Used ROM before -> after | Static register-copy sites |
| --- | --- | --- |
| calculations-1 | 22,015 -> 22,003 (-12) | +4 |
| calculations-2 | 39,692 -> 39,647 (-45) | +15 |

- The same ROM savings apply in default, explicit `-no-ra` and forced-spill
  modes. Accept one additional `LD A,B` copy per selected multiply; the
  copy metric does not count the removed exchanges. Frame sizes and
  frame/stack traffic are unchanged, as are all other execution metrics
  except cycles, instructions and register copies. No measured function
  or image grows, and no cycle or instruction count regresses.
- Reuse the existing arithmetic oracle's edge/seed generation for **eight
  new variable-multiply cases**: signed/unsigned words, computed/retained
  operands and byte widening on either side. Reuse the multiply fixture
  for signed-word, signed-byte and unchanged byte-multiply blocks; add no
  test file. Refresh **six existing golden blocks in five fixtures,
  removing 18 expected bytes**, with unchanged executable SSC except the
  three explicitly added fixture functions.
- Execute **4,718,592 actual old/new kernel checks**: vary every word
  against 16 representative factors in both operand positions, then every
  byte pair under four signedness combinations. Check the independent
  product, exact cycle/instruction formulas, byte budgets, preserved
  IX/IY/SP, new DE preservation and forbidden writes/ports. Another
  **29,184 whole-function executions** pass across default/spill/RA and
  old/new compilers. Only byte-marker data becomes same-size NOPs for
  whole-function execution; kernel and golden checks use unmodified code.
  The old compiler passes six unchanged byte blocks and rejects the four
  generic blocks, including both new signed controls.
- Fresh strict GCC O3 (`-ansi -pedantic -Wall -Werror`), native Release and
  native ASan compiler/verifier gates pass. Each runs all six cohorts:
  **547 generated entries and 25 real routines, 291,364 cases per mode**
  (280,064 generated + 11,300 real), in default/all-spill/RA modes. Both
  full standard suites pass sequentially, **175 tests each**, with source
  and staged compiler/linker hashes unchanged.
- Release and ASan each match GCC's **80 ROMs, 30 reports and 13,210 selected
  decisions**. Baseline/final comparison proves 86 identical SSC sources
  and unchanged decisions. In **615 word-multiply kernels across 80
  assembly files**, exactly **160 generic kernels** change; all 455 constant
  chains and all assembly outside those kernels match. All 48 traced/quiet
  pairs, 14 exact display rows and 217 ASan logs pass; compiler/verifier
  sanitizer imports are verified. Preserve unrelated inputs and both user
  ROMs. Artifacts under `/tmp/z80-patterns-3` retain profiles, controls,
  opcode/golden proofs and qualification logs; emulator 2.3.0 remains pinned.
  ASan uses the Release linker with leak detection disabled. ASan linker
  execution, other sanitizers, exhaustive whole-program paths and
  mapper/full-boot/rendering/hardware timing remain unqualified; banked
  kernels use fixed mapping.

### Deferred Optional Batch 397-399: Attribute Remaining BC-Base Word Reads

These items are not the next implementation obligation. Resume only if new
profiling identifies a material benefit, rather than continuing tiny
allocator-labelled batches automatically.

- [ ] **397. Measure reachable BC-base word reads.** Prove reachability
      before attributing pointer/index transfers, scaling and word assembly.
- [ ] **398. Reuse existing lowering only if profitable.** Keep at most one
      measured trial without helpers or caches; otherwise document deferral.
      Preserve full-width indices, scaling, reloads and byte/signed paths.
- [ ] **399. Consolidate and requalify.** Require isolated gains and explicit
      tradeoffs, apply shared completion gates and make a local-only commit.

**Shared completion gates:** Each three-step batch gets its own local commit,
with no push and no unrelated worktree changes included. Reuse existing
functions, planners, fixtures, and scripts; new production helpers/APIs must
remove real duplication or enable a demonstrated capability. For optimization
changes, include a before/after control showing actual benefit and independent
execution correctness. For output-preserving consolidation, require matching
ROMs and allocation decisions. Except for the isolated, independently verified
baseline semantic corrections in 316-318 and the user-approved `6be036d`
prerequisite to 364-366, keep default/`-no-ra` compatibility against the
semantically corrected baseline.
These allocator-only compatibility gates do not prohibit the separately
profiled and qualified common Z80 instruction-selection improvements above.
Keep full-width and partial-register safety, immutable verifier failure cases,
and atomic mutation rejection/rollback. Run focused checks after each edit and
both full suites plus available sanitizer/execution gates before closing a
batch; report the actual test count rather than freezing today's count in new
assertions.
An unprofitable or unproven item remains unchecked or is explicitly revised,
not marked complete because supporting infrastructure was added.

---

## Completed Z80 Scope And Optional Future Work

**The bounded first working Z80 register allocator v1 is complete. Mandatory
implementation batches remaining: 0.** Phases 265-267 completed graph-wide atomic
reload work and the original qualification gate; later checked phases extend
that implementation. Completion does not mean optimal code generation, general
retention across every control-flow shape, or support for another CPU.

Keep `-ra` opt-in, `-no-ra` explicit, and `-ra-all-spill` as the conservative
validation mode. This close-out does not change allocator behavior, instruction
selection, CLI defaults, production code or test inputs.

### Completed Guarantees And Known Limits

- [x] A reusable core already provides CFG/basic blocks, liveness, next-use,
      interval and active-slot analysis, selection/planning and checked mutation
      transactions. A Z80 policy already owns register-unit overlap, legality,
      clobbers, call/frame contracts and location/address materialization.
      Phases 68-176 record that extraction; it is not an unstarted prerequisite.
- [x] The qualified Z80 roles are `A`, `HL`, `BC`, `B` and `C`, with overlapping
      byte/pair units checked. `DE` remains the frame pointer; `IX`/`IY` remain
      address scratch/cache registers, not additional allocatable registers.
- [x] Bounded branch/loop retention and split preservation/reload are in
      production, not merely observe-only plans. Phases 177-267 record activation
      and atomic CFG reloads; later phases extend source-reachable cases.
      Broader producers, arbitrary interval splitting and unrestricted cross-block
      placement remain optional quality improvements.
- [x] Pointer bases, indices, values and results resolve through `z80_location`.
      Retention is limited by proven operand, width, overlap and clobber rules.
      Unproven candidates remain memory-backed; invalid internal retained
      locations or transfer schedules fail explicitly rather than inventing a
      stack slot or accepting partial mutation.
- [x] Spill layout can omit unused backing after every active operand has
      already been verified in the same physical register, without memory
      transfers or call arguments. Fixed spill prefixes remain protected;
      missing coverage and default/all-spill modes keep their backing. This
      changes layout, not retention legality. See the measured
      [step 2d checkpoint](Z80_CODEGEN_PLAN.md#step-2d-checkpoint-2026-10-19).
- [x] Calls and inline assembly retain their hard-barrier contracts. Inline-asm
      `(@var)` operands use stack/global locations; no retained-register operand
      or preservation contract is implied. Address-taking, aliases, tracked asm
      references and pointer reassignment constrain promotion.
- [x] Real-project execution coverage is no longer just Phase 47's compact smoke
      test. The existing CI runner checks 25 routines from `calculations-1` and
      `calculations-2`, including arrays, sorting, stencils, records, heap/RAM
      allocation and release, game workloads and embedded-assembly workloads.

Parser limitations are not allocator work: pointer-to-struct function arguments
and typedefs remain unsupported. Qualification is finite, not a proof of every
source program or of mapper/full-boot/rendering/hardware timing. Banked execution
checks use fixed mapping. ASan qualification uses the Release linker with leak
detection disabled; ASan linker execution and other sanitizers are not implied.

### Pointer And Indirect Correctness Close-Out

The bounded review follows source lowering in [pass_4.c](compiler/pass_4.c),
candidate/promotion/barrier policy in [pass_5.c](compiler/pass_5.c), and concrete
location/transfer emission in [pass_6_z80.c](compiler/pass_6_z80.c). It checks
correctness of accepted forms and conservative fallback, not whether every
indirect access can avoid a spill.

| Surface | Checked boundary and existing coverage |
| --- | --- |
| Pointer/struct/union roots and nested dereferences | Lower through ordinary address, assignment and array-read/write TAC; existing pointer-array, deep-member and helper-update fixtures cover shared addresses and read-modify-write preservation. |
| Array/address bases, indices and values | Location resolution, retained `HL`/`BC` scheduling, signed/unsigned byte extension, full-word indices, element scaling and value widths; existing index, pointer-value and BC-base/HL-index fixtures pin accepted and stack-backed paths. |
| Split preservation/reload | Target-approved operands/registers, real spill backing and checked emission; existing split fixtures and immutable verifier rejection/rollback controls reject unsupported or partial schedules. |
| Calls, escapes, aliases and inline assembly | Promotion restrictions and call/asm barriers; existing alias/call execution families, inline-asm operand and stack-guard fixtures cover conservative behavior. |
| Ports, local array initialization and address scratch | Shared location/policy helpers, error propagation and frame/cache addressing; existing port, initializer and specialized-address fixtures cover these helper-only paths. |
| Direct stack lookup | All three backend `find_stack_offset()` call sites remain inside `_resolve_z80_location()`; retained temps cannot silently fall through to a fabricated stack location. |

No new source-reachable correctness defect was identified in this bounded review.
Do not read that result as blanket authorization for unproven retention forms.
Existing regressions and independent oracles are reused; no new production helper,
retention endpoint or duplicate test framework is needed.

Close-out validation on 2026-10-19:

- Fresh strict GCC compiler/verifier builds pass with no diagnostics, using
  `-ansi -pedantic -Wall -Werror -DNDEBUG` and `-O3`/`-O2` respectively.
- All 38 selected pointer/address/barrier fixtures pass under GCC, native Release
  and ASan: 114 makefile runs, with 76 exact cross-toolchain ROM comparisons.
  Each verifier also passes traced and quiet modes with 32 immutable rejection
  and seven immutable acceptance controls, plus the independent 10,200-endpoint
  legality table.
- The existing [execution runner](ci/check_register_allocator.py) passes pointer,
  walk, nested, join/call, escape/rematerialization, typed-index, structural and
  wide-frame cohorts at `RA_CASES=128`, including all 25 real routines. This is
  16,000 generated cases (including repeated shared arithmetic controls) and
  2,852 real cases per default/all-spill/RA mode: 18,852 per mode, 56,556 total.
  Default/`-no-ra` and traced/quiet RA identity checks remain enabled.
- Artifacts under `/tmp/ra-closeout-20261019` retain manifests, logs, exact ROM
  hashes, execution summaries and the resolver-boundary proof. All production
  sources, existing test inputs and unrelated worktree inputs remain unchanged.

The latest full qualification at `8071c01` covers 547 generated entries and 25
real routines: 291,364 cases per mode/toolchain in default/all-spill/RA modes
under GCC, native Release and ASan compiler/verifier builds. Both full standard
suites pass, 175 tests each. The instruction-selection results above record
additional arithmetic proofs and exact compatibility/decision comparisons.
This documentation-only close-out retains that full-suite evidence rather than
claiming a new full-suite run.

### Optional Extensions, Not Completion Blockers

For prioritized, competitor-measured backend improvements after allocator v1,
follow [Z80_CODEGEN_PLAN.md](Z80_CODEGEN_PLAN.md). Its persistent cross-compiler
benchmark keeps SDCC and both z88dk backends as controls for future work.

Leave these unchecked until an actual implementation is measured and qualified.
They replace the former duplicated phase-by-phase "remaining to 100%" checklist;
the numbered phases above retain the detailed implementation history.

- [ ] Extract further target-independent logic or remaining Z80-specific hooks
      only to remove real duplication or satisfy a concrete backend requirement.
      Reuse the existing core and policy rather than introducing a second layer.
- [ ] Broaden pointer/indirect retention, register roles, cross-block producers,
      interval splitting or reload placement only for a reachable, profitable
      workload. Existing bounded support does not establish the general case.
- [ ] Add explicit inline-asm operand/clobber constraints only if retained values
      must cross or participate in asm blocks; otherwise keep the hard barrier.
- [ ] Implement separate Game Boy/SM83 and 6502 policies/backends when that work
      starts. Account for each CPU's registers, instructions, addressing and
      calling/spill contracts; reuse proven core pieces without assuming Z80
      lowering or 16-bit register pairs transfer unchanged.
- [ ] Expand real-program regression coverage when a concrete unsupported or
      untested shape is identified. The current large-workload coverage is
      already implemented; adding more programs is not a prerequisite to v1.

### Ongoing Maintenance Duties

These are continuing requirements, not finite completion checkboxes:

- Audit new TAC rewrites, helpers and retention endpoints for location resolution,
  semantic operand width versus reused physical-container width, byte/pair
  overlap, alias/escape handling, IX cache invalidation and frame/stack safety.
- Keep unsupported retention conservative. A demonstrated miscompilation or
  allocator-caused rejection of otherwise supported source is a correctness bug,
  not an optional optimization; fix it with a focused regression.
- Preserve immutable verifier failures, atomic rejection/rollback, flags-off
  compatibility and the opt-in contract. Apply the shared completion gates to
  future code changes and report actual test counts.
- Treat common Z80 instruction-selection improvements as separately profiled
  backend work, not evidence that allocator v1 is unfinished.

---

## Compiler Tracing And Release Builds

Compiler diagnostic tracing is now a separate, default-off build feature,
`SAMESAMEC_TRACE`. This is independent of `NDEBUG`, allocator enablement and
target instruction selection. Normal builds retain errors, warnings,
allocation verification, failure/rollback diagnostics and emitted assembly
comments. In particular, asking for a stack offset for a retained register is
always diagnosed, including in a quiet build.

The existing token, AST, TAC and allocator/backend debug-print source remains
available. Enable it with CMake `-DSAMESAMEC_TRACE=ON`, GNU make `TRACE=1`,
MSBuild `/p:SameSameCTrace=true`, or the C preprocessor definition
`SAMESAMEC_TRACE`. Normal builds leave the definition absent. The shared
allocator trace flag becomes a compile-time zero in that configuration, so
existing trace arguments still type-check as ANSI C90 without being evaluated.
Trace-only rejection aggregation/storage is also omitted. Traced builds keep
the existing runtime `-q` behavior and quiet rejection summaries.

The GNU compiler makefile now uses `-Os` without `-g` rather than the old
`-O0 -g` release default. Clean and rebuild when switching GNU make flags.
See [README.md](README.md#building-the-compiler) for complete build commands.
No new production function, LTO requirement, stripping build step or target
code-generation change was introduced.

The trace-dependent regression/measurement tools require a trace-enabled
compiler. All standalone host trace assertions explicitly enable the
definition; the core, TAC-storage and spill-emitter harnesses additionally
exercise the same inputs without it. Measurement rejects missing trace data
instead of treating absent workspace/rejection logs as measured zeroes.
CI explicitly enables traces for its existing diagnostic assertions. The
Windows XP publishing pipeline rebuilds with tracing off before packaging.

Fresh Cygwin GCC 14.4.0 measurements against the frozen pre-change compiler,
with identical optimization flags and without LTO:

| Compiler build | Before (bytes) | Normal tracing-off (bytes) | Reduction |
| --- | ---: | ---: | ---: |
| GCC `-O3 -DNDEBUG`, both stripped | 731,648 | 563,200 | 23.0% |
| GCC `-Os -DNDEBUG`, both stripped | 529,920 | 396,288 | 25.2% |
| MSVC Win32 CMake Release, unstripped | 535,040 | 409,088 | 23.5% |

The GCC `-O3` trace-enabled build retains the original executable sizes:
855,022 bytes unstripped and 731,648 bytes stripped. These comparisons isolate
tracing cleanup from the separate GNU make optimization/debug-symbol default
change. Normal `-O0 -g` builds also exclude representative success-trace,
token/AST/TAC dump and pointer/spill trace formats; `-q` alone never provided
that compile-time saving.

The actual GNU make defaults were also rebuilt, not just approximated with
direct GCC commands: the old `-O0 -g` compiler is 1,575,875 bytes and the new
normal `-Os`, tracing-off compiler is 520,418 bytes (67.0% smaller). This larger
combined saving includes the optimization/debug-symbol change; the table above
isolates the tracing saving.

Qualification includes strict GCC ANSI C90 `-Wall -Werror` builds at `-O0`,
`-Os` and `-O3`, MSVC Release and ASan builds/verifiers with `/WX`, and both
Visual Studio project trace settings using the installed v143 toolset with
isolated outputs. The legacy v141/XP project toolsets and remote Azure jobs
were not run locally. Representative token/AST/TAC, core, pointer and spill
success-trace formats are absent from normal binaries even at `-O0`, and
present with tracing enabled. All six normal/traced GCC, MSVC and ASan
compilers keep a real invalid-source error visible, including with `-q`.

Eight compiler variants (the frozen baseline; normal/traced GCC, MSVC and
ASan; and the actual default GNU make compiler) passed all six generated
execution cohorts and 25 real routines. Each variant covers 35,008 generated
and 1,444 real cases per default/all-spill/RA mode: 109,356 executions per
compiler, 874,848 in total. Immutable verifier controls still reject 32 cases,
accept seven and check the independent 10,200-endpoint policy table. Compared
ROMs and execution metrics are byte-identical. Assembly comparisons additionally
canonicalize the pre-existing randomized static-symbol suffixes (the compiler
seeds `rand()` from time); this is not a new code-generation difference.
The five-mode measurement script succeeds with tracing and rejects a normal
compiler explicitly rather than publishing missing trace metrics.

All 175 existing fixture Make checks passed with each of the GCC and native
MSVC trace-enabled compilers, with 178 byte-identical cross-toolchain fixture
ROMs. An additional 80 execution-corpus ROM comparisons bridge the frozen GCC
baseline and native MSVC build exactly. A publishing-style, same-directory
CMake trace-ON to trace-OFF reconfigure/rebuild also passed: the resulting
409,088-byte executable excludes the checked dump/success-trace formats.

## Rollout Gates

- [x] Compiler builds clean with flag off (default).
- [x] Compiler builds clean with flag on + forced all-spill.
- [x] `./run_tests.sh` passes with the allocator test suite, and forced
      all-spill mode has a dedicated SMS validation gate.
- [x] Phase 8 turns every `allocator_*` case to its mapped verdict with normal
      makefile/`byte_tester` SMS tests.
- [x] No new emitter consumes `find_stack_offset()` for a register-resident
      temp; failures are loud, not silent.
- [x] Every public allocator API is ANSI C90 (no `//`, no mid-block decls,
      no compound literals, no VLAs).

---

## Risk Notes

- **`g_is_ix_de` cache invalidation** — every materialize/store helper that
  touches `IX` must mirror the existing invalidation rules. Audit the 11+
  `g_is_ix_de = NO;` sites in `pass_6_z80.c` when writing helpers.
- **Custom call frame** — function calls in this backend are not a single
  `CALL`; they are a `PUSH return_label / JP target` pair. Treat the entire
  sequence as one hard barrier, not just the `JP`.
- **`mainmain` entry** — the entry-point prologue is special-cased; verify
  the allocator never assumes a normal frame on it.
- **Inline asm operand syntax** — `LD A,(@var)` is parsed in
  `inline_asm_z80.c`, resolved through `z80_location`, and materialized through
  `IX` as a stack/global location. Treat any inline-asm block as a register
  barrier until explicit operand/clobber constraints exist.
- **Narrow `B`/`C` retention** — Phases 43-46 only treat intervening 8-bit
      arithmetic as `C`- or `B`-transparent when that TAC has no retained
      overlapping `BC`/single-byte operands. Keep future `B`/`C` candidates tied to
      exact emitter behavior rather than the conservative whole-`BC` clobber
      descriptor alone.
- **8↔16 promotion** — cast TACs change `size` between producer and
  consumer; the single-use rule must reject any pair whose sizes differ.
- **Const-folded operands** — operands of type `TAC_ARG_TYPE_CONSTANT`
  never need a register and must short-circuit out of the allocator before
  spill bookkeeping.
- **Allocator documentation drift** — treat this file, not older TODO text, as
      the source of truth while implementing allocator work.

### Historical Rollout Notes (Phases 36-66)

The inventory, suite counts and toolchain limitations below describe those
historical phases, not the current completion status or available toolchains.
Use the completed-scope qualification above for current evidence.

An earlier inventory audit found 88 `tests/z80/sms/allocator*`
directories, each with a lowercase `makefile`, `main.ssc`, and
`@BT linked.sms` byte-tester tag. `find_stack_offset()` still fails loudly if
any retained temp reaches a stack-only emitter path. The
`allocator_ra_find_stack_guard_a` and `allocator_ra_find_stack_guard_hl` tests
capture `compiler.log`, grep for retained `linear_scan` paths, verify
retained-result/arg comments in `main.asm`, and assert that no
`stack offset requested for retained` diagnostic appears. The allocator-facing
edited C files build as C via the MSVC `/TC` project, and a targeted scan found
no C90-incompatible `//`, `bool`, or `for (int ...)` patterns in the touched
allocator C files. After Phase 36, both full suites pass: default
`./run_tests.sh` and `./run_tests.sh -windows` report `DONE (78 tests)`. Phase
37 focused validation passes for `allocator_ra_get_address_computed_bases`,
`allocator_ra_get_address_pointer_bases`, `allocator_ra_array_read_computed_bases`,
and `allocator_ra_assignment_producer_hl`. After Phase 37, both full suites
pass: default `./run_tests.sh` and `./run_tests.sh -windows` report
`DONE (79 tests)` with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 38 focused validation passes
for `allocator_ra_get_address_bc_base_hl_index`,
`allocator_ra_get_address_pointer_bases`,
`allocator_ra_get_address_computed_bases`,
`allocator_ra_assignment_producer_hl`, and
`allocator_ra_bc_retention_hl_conflict`. After Phase 38, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (80 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 39 focused validation passes
for `allocator_ra_array_read_bc_base_hl_index`,
`allocator_ra_array_read_pointer_bases`,
`allocator_ra_array_read_computed_bases`,
`allocator_ra_assignment_producer_hl`,
`allocator_ra_get_address_bc_base_hl_index`, and
`allocator_ra_array_write_pointer_bases`. After Phase 39, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (81 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 40 focused validation passes
for `allocator_ra_array_write_bc_base_hl_index`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_write_pointer_values`,
`allocator_ra_array_write_bases`,
`allocator_ra_array_write_indexes`,
`allocator_ra_array_read_bc_base_hl_index`,
`allocator_ra_array_read_pointer_bases`,
`allocator_ra_get_address_bc_base_hl_index`, and
`allocator_ra_assignment_producer_hl`. After Phase 40, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (82 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 41 focused validation passes
for `allocator_ra_inline_asm_barrier`,
`allocator_ra_frame_no_spill_a`,
`allocator_ra_frame_no_spill_hl`,
`allocator_ra_find_stack_guard_a`,
`allocator_ra_find_stack_guard_hl`,
`allocator_ra_basic_blocks_debug`,
`allocator_ra_bc_clobber_debug`, and
`allocator_ra_array_write_bc_base_hl_index`. After Phase 41, both full suites
pass: default `./run_tests.sh` and `./run_tests.sh -windows` report
`DONE (83 tests)` with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 42 focused validation passes
for `allocator_ra_array_init_copy_location`,
`allocator_ra_find_stack_guard_a`,
`allocator_ra_find_stack_guard_hl`,
`allocator_ra_inline_asm_barrier`,
`allocator_ra_array_read_pointer_bases`, and
`allocator_ra_array_write_pointer_bases`; the direct `find_stack_offset()` grep
now finds only `_resolve_z80_location()` call sites. After Phase 42, both full
suites pass: default `./run_tests.sh` and `./run_tests.sh -windows` report
`DONE (84 tests)` with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 43 focused validation passes
for `allocator_ra_c_retention_a_conflict`,
`allocator_ra_next_arg2_a`,
`allocator_ra_arithmetic_locations_a`,
`allocator_ra_bc_retention_hl_conflict`,
`allocator_ra_linear_scan_conflict_a`,
`allocator_ra_linear_scan_farther_next_use`, and
`allocator_ra_find_stack_guard_a`. After Phase 43, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (85 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 44 focused validation passes
for `allocator_ra_b_retention_c_conflict`,
`allocator_ra_c_retention_a_conflict`,
`allocator_ra_linear_scan_conflict_a`,
`allocator_ra_linear_scan_farther_next_use`,
`allocator_ra_bc_retention_hl_conflict`,
`allocator_ra_array_read_bc_base_hl_index`,
`allocator_ra_array_write_bc_base_hl_index`, and
`allocator_ra_find_stack_guard_a`. After Phase 44, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (86 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 45 focused validation passes
for `allocator_ra_index_c_retention_a_conflict`,
`allocator_ra_b_retention_c_conflict`,
`allocator_ra_c_retention_a_conflict`,
`allocator_ra_array_write_indexes`,
`allocator_ra_array_read_indexes`,
`allocator_ra_get_address_indexes`,
`allocator_ra_z80_in_ports`,
`allocator_ra_linear_scan_conflict_a`,
`allocator_ra_linear_scan_farther_next_use`, and
`allocator_ra_find_stack_guard_a`. After Phase 45, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (87 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 46 focused validation passes
for `allocator_ra_z80_out_c_retention_a_conflict`,
`allocator_ra_index_c_retention_a_conflict`,
`allocator_ra_array_write_indexes`,
`allocator_ra_z80_in_ports`,
`allocator_ra_array_write_values`,
`allocator_ra_b_retention_c_conflict`,
`allocator_ra_c_retention_a_conflict`, and
`allocator_ra_find_stack_guard_a`. After Phase 46, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (88 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 47 focused validation passes
for `allocator_ra_pointer_array_smoke`,
`allocator_ra_array_write_pointer_values`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_read_pointer_bases`,
`allocator_ra_get_address_pointer_bases`,
`allocator_ra_array_read_bc_base_hl_index`,
`allocator_ra_array_write_bc_base_hl_index`, and
`allocator_ra_find_stack_guard_a`. After Phase 47, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (89 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. The local MSVC Release rebuild is
blocked because this Visual Studio installation lacks the requested `v141_xp`
toolset and also fails a v143 override due to missing standard C headers.
Phase 48 focused validation passes for `allocator_ra_struct_array_fields`,
`allocator_ra_array_write_bases`, `allocator_ra_array_read_computed_bases`,
`allocator_ra_get_address_computed_bases`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_read_pointer_bases`, `allocator_ra_pointer_array_smoke`,
and `allocator_ra_find_stack_guard_hl`. After Phase 48, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (90 tests)`
with the freshly built Cygwin compiler staged in
`windows/Compiler/Release/samesamecc.exe`. Phase 49 focused validation passes
for `allocator_ra_struct_pointer_fields`, `allocator_ra_struct_array_fields`,
`allocator_ra_pointer_array_smoke`, `allocator_ra_array_write_pointer_values`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_read_pointer_bases`,
`allocator_ra_get_address_pointer_bases`,
`allocator_ra_array_read_bc_base_hl_index`,
`allocator_ra_array_write_bc_base_hl_index`, and
`allocator_ra_find_stack_guard_hl`. After Phase 49, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (91 tests)`
with the freshly built Cygwin compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 50 focused validation passes
for `allocator_ra_struct_deep_pointer_fields`,
`allocator_ra_struct_pointer_fields`, `allocator_ra_struct_array_fields`,
`allocator_ra_pointer_array_smoke`, `allocator_ra_array_write_pointer_values`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_read_pointer_bases`,
`allocator_ra_get_address_pointer_bases`,
`allocator_ra_array_read_bc_base_hl_index`,
`allocator_ra_array_write_bc_base_hl_index`, and
`allocator_ra_find_stack_guard_hl`. After Phase 50, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (92 tests)`
with the freshly built Cygwin compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 51 focused validation passes
for `allocator_ra_struct_pointer_increment`,
`allocator_ra_struct_deep_pointer_fields`,
`allocator_ra_struct_pointer_fields`, `allocator_ra_struct_array_fields`,
`allocator_ra_pointer_array_smoke`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_read_pointer_bases`, and
`allocator_ra_find_stack_guard_hl`; `-no-ra` also emits no
`struct_access_update` diagnostic. After Phase 51, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (93 tests)`
with the freshly built Cygwin compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 52 focused validation passes
for `allocator_ra_split_spill_hl`,
`allocator_ra_struct_pointer_increment`,
`allocator_ra_struct_deep_pointer_fields`,
`allocator_ra_struct_pointer_fields`, `allocator_ra_struct_array_fields`,
`allocator_ra_pointer_array_smoke`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_read_pointer_bases`, and
`allocator_ra_find_stack_guard_hl`. After Phase 52, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (94 tests)`
with the freshly built Cygwin compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 53 focused validation passes
for `allocator_ra_struct_pointer_increment`,
`allocator_ra_split_spill_hl`,
`allocator_ra_array_write_bc_base_hl_index`,
`allocator_ra_array_read_bc_base_hl_index`,
`allocator_ra_get_address_bc_base_hl_index`,
`allocator_ra_array_write_pointer_bases`,
`allocator_ra_array_write_values`, and retained-stack guard coverage. After
Phase 53, both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (94 tests)` with the freshly built
Cygwin compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 54 focused validation passes
for `allocator_ra_compare_bc_operands`,
`allocator_ra_compare_c_neq_b_overlap`, the existing compare arg/location
matrix, `allocator_ra_c_retention_a_conflict`,
`allocator_ra_b_retention_c_conflict`, and
`allocator_ra_linear_scan_farther_next_use`. After Phase 54, both full suites
pass: default `./run_tests.sh` and `./run_tests.sh -windows` report
`DONE (96 tests)` with the freshly built Cygwin compiler staged in
`binaries/` and `windows/Compiler/Release/samesamecc.exe`. Phase 55 focused
validation passes for `allocator_ra_compare_c_relational`, both Phase 54
equality/inequality tests, the existing 8-bit and 16-bit compare arg/location
matrix, `allocator_ra_c_retention_a_conflict`, and
`allocator_ra_b_retention_c_conflict`. After Phase 55, both full suites pass:
default `./run_tests.sh` and `./run_tests.sh -windows` report `DONE (97 tests)`
with the freshly built Cygwin compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 56 focused validation passes
for `allocator_ra_complement_bc_results`, the existing 8-bit and 16-bit
complement matrix, prior `B`/`C` conflict and retained-comparison tests, and
`allocator_ra_linear_scan_farther_next_use`. After Phase 56, both full suites
pass: default `./run_tests.sh` and `./run_tests.sh -windows` report
`DONE (98 tests)` with the freshly built Cygwin compiler staged in
`binaries/` and `windows/Compiler/Release/samesamecc.exe`. Phase 57 focused
validation passes for `allocator_ra_array_read_c_results`, the existing
array-read result matrix, `allocator_ra_complement_bc_results`, and
`allocator_ra_linear_scan_farther_next_use`. After Phase 57, both full suites
pass: default `./run_tests.sh` and `./run_tests.sh -windows` report
`DONE (99 tests)` with the freshly built Cygwin compiler staged in
`binaries/` and `windows/Compiler/Release/samesamecc.exe`. Phase 58 fixes the
mixed-width retained-`HL` index regression found by `move_sprite`; focused
array-read, array-write, get-address, port-index, and pointer-array tests pass.
After Phase 58, both full suites still report `DONE (99 tests)` with the fixed
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 59 focused validation passes
for `allocator_ra_function_call_c_results`, the existing `A`/`HL` call-result
matrix, prior array-read/complement `C` producers, and farther-next-use. After
Phase 59, both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (100 tests)` with the freshly built
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 60 focused validation passes
for `allocator_ra_function_call_compare_c`, the signed assignment clobber
audit, existing call results, retained comparisons, and farther-next-use.
After Phase 60, both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (101 tests)` with the freshly built
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 61 focused validation passes
for `allocator_ra_mul_div_mod_c_results`, the existing 8/16-bit result and
operand matrices, complement `B`/`C`, and call-result comparison. After Phase
61, both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (102 tests)` with the freshly built
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 62 focused validation passes
for `allocator_ra_shift_c_results`, all existing shift count/location suites,
complex-arithmetic results, complement `B`/`C`, and call-result comparison.
After Phase 62, both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (103 tests)` with the freshly built
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 63 focused validation passes
for `allocator_ra_shift_values_c`, every existing shift result/count/location
suite, complement `B`/`C`, and complex-arithmetic results. After Phase 63,
both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (104 tests)` with the freshly built
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 64 focused validation passes
for `allocator_ra_struct_pointer_array`, global/deep struct-pointer fields,
struct arrays, pointer-array smoke coverage, and pointer-base reads/writes.
After Phase 64, both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (105 tests)` with the freshly built
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 65 focused validation passes
for `allocator_ra_struct_pointer_array_update`, the base pointer-array test,
global struct-pointer updates, the original split-spill regression, and
shallow/deep struct fields. After Phase 65, both full suites pass: default
`./run_tests.sh` and `./run_tests.sh -windows` report `DONE (106 tests)` with
the freshly built compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`. Phase 66 focused validation passes
for `allocator_ra_union_pointer_array`, struct pointer arrays and updates,
global pointer updates, split-spill, and deep pointer fields. After Phase 66,
both full suites pass: default `./run_tests.sh` and
`./run_tests.sh -windows` report `DONE (107 tests)` with the freshly built
compiler staged in `binaries/` and
`windows/Compiler/Release/samesamecc.exe`.
