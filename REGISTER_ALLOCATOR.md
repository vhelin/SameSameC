# Register Allocator Implementation Plan

This document is the actionable, checkbox-driven plan for adding a real,
target-aware Z80 register allocator to SameSameC. Treat the checked boxes in
this document as the current implementation record on top of the existing
virtual-temp pipeline.

---

## 0. Ground Truth: What Exists Today

Read-only context. Not tasks. Do not check these boxes.

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
      Phase 41 routes those operands through `z80_location` as stack/global
      locations materialized through IX. It is still a hard register barrier.
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

## Remaining Work To 100%

The current completion target is the bounded first working Z80 allocator, not
every possible optimization or future target. The allocator must keep `-ra` as
the opt-in feature flag. Do not remove, rename, or make the register allocator
unconditional while finishing these items. A 6502 backend and optional quality
improvements are explicitly postponed and are not part of this completion
target. Phases 265-267 complete the mandatory graph-wide atomic reload work and
final qualification. No mandatory implementation batches remain for this
bounded scope (estimated remaining batches: 0).

The unchecked items below are optional architecture extraction, optimization
quality, broader backend coverage, and future-target work. They are not defects
in the completed bounded Z80 allocator.

- [ ] Extract target-independent basic-block construction, liveness, next-use,
      live-interval, linear-scan, spill/split, and join-reconciliation logic
      from the current Z80-specific implementation into a reusable allocator
      core without changing `-ra` behavior. Phase 97 extracts generic
      liveness-set indexing and backward fixed-point solving. Phase 98
      extracts normalized basic-block partitioning and CFG construction.
      Phase 99 extracts callback-driven next-use traversal. Phase 100 extracts
      aggregate live-interval metadata construction. Phase 101 extracts
      active-slot conflict arbitration. Phase 102 extracts active-slot state,
      reset, expiration, and assignment. Phase 103 moves physical-register to
      active-slot mapping and slot cardinality behind the target policy. Phase
      104 extracts callback-driven per-definition candidate discovery and
      read-multiplicity classification. Phase 105 extracts temp retention and
      displacement state transitions. Phase 106 extracts split-preservation
      action planning. Phase 107 extracts callback-driven first-read reload-site
      discovery. Phase 108 extracts post-target-selection reload-action
      planning. Phase 109 extracts canonical reload TAC-mutation planning;
      Phase 110 extracts canonical spill TAC-mutation planning. Concrete plan
      application becomes callback-driven in Phase 111. Phase 112 extracts
      bounded reload-chain discovery while current integration remains
      capacity one. Phase 113 applies complete block-local chains with per-site
      target approval and callback mutation. Phase 114 extracts canonical
      reload-emission metadata and validation while concrete target instruction
      selection remains local. Phase 115 extracts matching spill-emission
      metadata and validation. Phase 116 extracts and validates the reusable
      target-policy contract. Phase 117 extracts opaque physical-register unit
      overlap decisions. Phase 118 allocates active-slot storage from the policy
      cardinality. Phase 119 resolves the current five Z80 scheduling roles
      through policy-owned indexes instead of fixed positions. Phase 120 moves
      ordered primary/alternate candidate register sets behind target callbacks.
      Phase 121 extracts ordered candidate arbitration while policy fact
      collection and fallback triggers remain local. Phase 122 extracts lazy,
      callback-driven candidate fact collection while fallback triggers remain
      local. Phase 123 extracts fallback-trigger precedence while target
      capabilities remain local. Phase 124 moves fallback capabilities behind
      the target policy and suppresses unavailable alternates in core. Phase
      125 extracts primary legality, path, and active fact evaluation. Phase
      126 moves equal-next-use target preference behind policy while keeping
      generic linear-scan arbitration in core. Phases 127-129 extract interval
      boundary analysis, callback-driven transparent-range scanning, and full
      candidate-path arbitration. Phases 130-132 consolidate primary fallback
      decisions, active-slot transition planning, and ordered callback-driven
      transition application. Phases 133-135 extract candidate range analysis,
      ordered qualification, and composed competing-candidate probing. Broader
      Phase 136 composes ordered alternate evaluation and arbitration into one
      transaction. Phase 137 extracts primary-plan dispatch while concrete
      alternate sets remain target-owned. Phase 138 composes equal-next-use
      preference, linear-scan arbitration, and active-slot transition planning
      after active-temp replaceability is known. Phase 139 extracts that
      replaceability decision behind an opaque metadata callback. Phase 140
      composes slot-transition application and split-spill mutation application
      behind opaque callbacks. Phase 141 composes primary decision, fallback
      dispatch, and split-action planning. Phase 142 composes spill-boundary
      gating and lazy split-preservation capability resolution. Phase 143
      composes preparation and finalization into one ordered selection
      transaction. Phase 144 composes policy-owned active-slot resolution and
      candidate transition planning. Phase 145 composes that preparation with
      callback-driven transition and split-spill mutation application while
      preserving target diagnostics through a preparation observer. Phase 146
      composes reload-action planning, canonical mutation preparation, and
      callback application. Phase 147 composes bounded reload discovery,
      per-site target selection, and mutation execution. Phase 148 extracts
      checked parallel-buffer storage planning for those reload chains. Phase
      149 extracts checked basic-block and CFG workspace planning. Phase 150
      adds checked instruction-descriptor sizing to that shared control-flow
      plan. Phase 151 extracts checked four-buffer liveness workspace planning.
      Phase 152 makes the generic liveness solver validate and consume that
      storage descriptor rather than recomputing its layout. Broader operand/
      register support and join reconciliation still remain. Phase 153 extracts
      callback-driven per-block liveness use/def construction while keeping
      target TAC operand interpretation in pass 5. Phase 154 extracts
      stack-only join discovery, live-in traversal, and fail-closed retained
      state rejection while keeping target temp-state classification local.
      Phase 155 replaces public unchecked liveness indexing with
      descriptor-validated block/temp offset resolution. Phase 156 moves join
      stack-versus-retained classification onto the generic temp-state
      invariant while preserving stack-only policy enforcement. Phase 157
      extracts the resulting join action decision while retaining rejection of
      planned predecessor spills until cross-block mutation is implemented.
      Phase 158 extracts deterministic, validated join predecessor collection;
      Phase 159 maps those predecessors to validated terminal anchors with
      explicit before/after placement. Phase 160 adds atomic target approval for
      every structural-join site. Phase 161 prepares canonical descriptor-only
      mutations for approved sites without applying them. Phase 162 adds
      callback-driven validation and application, currently exercising empty
      plans without target mutation. Phase 163 composes approved sites with
      validated target spill offset/width metadata. Phase 164 validates and
      dispatches that complete metadata through a callback, while current
      production plans exercise canonical empty skips. Phase 165 atomically
      orders approved sites by descending TAC anchor so future insertion cannot
      invalidate pending anchors. Phase 166 adds ownership-safe indexed TAC
      insertion with canonical empty slots and capacity growth. Phase 167 adds
      validated register-spill marker population and callback-driven pass-5
      insertion while current production remains empty. Phase 168 adds
      fail-closed concrete Z80 emission for `A`, `B`, `C`, `HL`, and `BC`.
      Phase 169 validates and stably orders the complete function-wide join,
      temp, and predecessor-site emission batch, including `after`-before-
      `before` precedence at equal anchors. Phase 170 validates the applied TAC
      count, discards stale analysis storage, reconstructs CFG/basic blocks,
      and recomputes liveness before later consumers. Phase 171 adds a
      fail-closed all-path eligibility transaction and read-only Z80 preflight.
      Phase 172 resolves definitions through validated CFG predecessors and
      carries all-path transparency to the join. Phase 173 atomically plans the
      distinct producer and consumer TAC assignments and rejects incompatible
      existing assignments. Phase 174 plans a conservative cross-block active-
      slot reservation against target-supplied overlap facts and current
      retained intervals. Phase 175 converts that span into the deterministic
      union of block-local reservations on valid producer-to-consumer CFG
      routes. Phase 176 validates and dispatches that plan through ordered
      callbacks with exact partial-failure reporting; production currently
      applies it only to isolated observation state. Atomic allocator-state
      application, block-exit spill coordination, and schedule application
      remain prerequisites for narrow retained-live-in enablement.
- [ ] Define a target-policy interface for physical registers and overlapping
      units, candidate legality, instruction clobbers, calling conventions,
      spill/reload constraints, and target location materialization. Phase 68
      introduces the interface and migrates physical-register unit/overlap
      lookup, Phase 69 migrates candidate legality, and Phase 70 migrates the
      per-TAC clobber descriptor lookup. Phase 71 migrates per-TAC physical
      register transparency. Phase 72 migrates the primary physical register
      for call results, and Phase 73 migrates call/return hard-boundary spill
      classification. Phase 74 migrates the first narrow split spill/reload
      constraints. Phase 75 migrates stack-return slot reservation, and Phase
      76 migrates contiguous stack-argument placement. Phase 77 migrates call
      argument transport classification while preserving concrete Z80
      emission. Phase 78 migrates concrete return-slot byte offsets for both
      callee stores and caller loads. Phase 79 migrates the fixed call-frame
      prefix shared by layout, caller setup/restoration, and callee return.
      Phase 80 migrates logical source/destination byte offsets and access
      widths for concrete call arguments while preserving Z80 instruction
      selection. Phase 81 migrates semantic-source to target-location-kind
      classification for constants, globals, stack storage, and all current
      physical registers. Phase 82 migrates address strategy selection for
      the shared IX/IY/HL helpers. Phase 83 migrates instruction-equivalent
      `GET_ADDRESS_ARRAY` index and 16-bit `MUL`/`DIV`/`MOD` arg2 IX paths.
      Phase 84 migrates instruction-distinct 8-bit arithmetic arg2 paths with
      an explicit uncached IX target. Phase 85 migrates function-call
      old-frame IY cache strategy. Phase 86 migrates memory-backed get-address
      base HL construction. Phase 87 migrates function-call result-store IY
      construction. Phase 88 migrates global function-call argument-source IY
      construction while preserving old-frame cache invalidation. Phase 89
      closes the array-initializer target HL boundary with contextual policy
      diagnostics and fail-closed helper propagation. Phase 90 adds contextual
      IY policy ownership for memory-backed array read/write bases while
      preserving retained-base bypasses. Phase 91 adds contextual IX policy
      ownership for inline-asm variable reads/writes while preserving the hard
      barrier. Phase 92 adds contextual IX policy ownership for memory-backed
      Z80 input/output port operands while preserving constant and retained
      bypasses. Phase 93 adds contextual IX policy ownership for 8-bit and
      16-bit complement memory sources/results while preserving all retained
      result bypasses. Phase 94 adds contextual IX/IY policy ownership for
      assignment memory results/sources while preserving constant and retained
      bypasses. Phase 95 adds contextual IX policy ownership for memory-backed
      8-bit and 16-bit shift results while preserving retained physical-result
      bypasses. Phase 96 adds contextual IX policy ownership for memory-backed
      callee return-value sources and permits terminal `RETURN_VALUE` operands
      to use target-approved physical registers. Phase 103 adds active-slot
      count, index, and display-name ownership for all five current Z80
      registers. Phase 105 extracts the target-independent temp storage-state
      transitions for register-only retention, spill-backed retention, and
      displacement. Phase 106 extracts the target-independent split action
      decision after final register selection; reload-site discovery, concrete
      spill/reload insertion, and join reconciliation remain. Phase 107 extracts
      callback-driven first-read reload-site discovery while target reload
      selection remains local. Phase 108 extracts the resulting target-independent
      reload-action decision while TAC marking and concrete emission remain
      local. Phase 109 extracts the reload TAC-mutation plan while concrete TAC
      application and backend emission remain local. Phase 110 extracts the
      matching spill TAC-mutation plan; concrete TAC application and backend
      emission remain local. Phase 111 moves concrete TAC writes behind generic
      core dispatch and TAC-specific callbacks; backend emission remains local.
      Phase 112 extracts bounded multi-read reload discovery. Phase 113 applies
      complete block-local chains while preserving per-site Z80 target approval
      and concrete emission. Phase 114 extracts the canonical reload-emission
      plan while preserving Z80 address materialization and instruction
      selection. Phase 115 extracts the symmetric canonical spill-emission
      plan while preserving Z80 address materialization and instruction
      selection. Phase 116 moves the complete policy contract into the core
      header and validates every hook. Phases 117-120 extract physical-unit
      overlap, policy-sized active storage, policy-owned slot-role mapping, and
      ordered candidate-register sets. Phase 124 adds policy-owned candidate
      fallback capabilities. Phase 126 adds policy-owned equal-next-use
      preference. Other specialized address sequences, concrete instruction
      selection, and other hooks remain.
- [ ] Move the current `A`/`HL`/`BC`/`B`/`C` rules behind a Z80 policy and keep
      all existing exact-ROM and debug-trace tests unchanged as the extraction
      compatibility gate.
- [ ] Postponed beyond the current Z80 completion target: add a separate 6502
      policy/backend using `A`/`X`/`Y`, 6502 addressing modes and clobbers,
      memory-backed or target-defined 16-bit values, and a 6502-specific
      call/spill contract. Reuse the allocator core rather than cloning the
      Z80 allocator when that future work begins.
- [x] Add native SMS coverage for retained computed `GET_ADDRESS_ARRAY arg1`
      base temps, or document the parser/lowering limitation if SameSameC
      cannot express that shape. Phase 37 covers constant-index pointer-base
      temps retained in `HL`; Phase 38 covers computed-index pointer bases by
      retaining the base in `BC` while the computed index remains in `HL`.
- [ ] Audit remaining pointer/indirect-address paths beyond the migrated
      `ARRAY_WRITE`, `ARRAY_READ`, `GET_ADDRESS_ARRAY`, and `__z80_in`/`__z80_out`
      port cases so pointer bases, dereferences, and pointer writes can consume
      retained values safely wherever the backend can schedule them. Phase 39
      covers the known 16-bit computed-index `ARRAY_READ` pointer-base gap, and
      Phase 40 covers the matching `ARRAY_WRITE` pointer-base gap; less common
      indirect forms still need review. Phase 41 routes inline-asm `(@var)`
      variable operands through `z80_location`, and Phase 42 routes local array
      initializer bulk-copy targets through `z80_location`; Phase 47 adds a
      combined pointer-to-pointer and pointer-array smoke regression; Phase 48
      adds debug and coverage for struct-array field helper addresses; Phase
      49 adds debug and coverage for global struct-pointer roots plus nested
      pointer-member dereferences; Phase 50 extends that to a two-hop global
      struct-pointer chain with depth/member indirect debug; Phase 51 covers
      struct-pointer field increment/decrement helpers and pins their safe
      multi-read address spill; Phase 52 adds the first targeted split-spill
      insertion for retained `HL` array-read bases; Phase 53 explicitly reloads
      the later zero-index write base into `BC`; Phase 64 covers computed-index
      reads and writes through arrays of struct pointers; Phase 65 covers byte
      increment and 16-bit decrement helpers through those indexed pointers,
      including `HL` split preservation and `BC` write-base reload; Phase 66
      covers matching union-pointer arrays across ordinary reads/writes and
      helper updates. Phase 92 closes contextual policy ownership for
      memory-backed `__z80_in` and `__z80_out` port operands while retaining
      the existing physical-register paths.
      Pointer-to-struct function arguments are not accepted by the parser and
      typedefs are not supported. These phases do not yet prove every other
      supported complex indexed dereference, pointer-write, or helper-only
      address path can safely consume retained registers.
- [ ] Replace stack-only join reconciliation with cross-block physical
      register retention, including loop back-edges and branch joins. Phase 171
      establishes the generic all-path eligibility contract and an observe-only
      Z80 fact preflight. Phase 172 adds reaching-definition discovery through
      dominating blocks with ambiguity and path-clobber handling. Phase 173
      plans and conflict-checks the required producer/consumer assignments
      without applying them. Phase 174 plans a conservative active-slot
      reservation across the producer-to-consumer span. Phase 175 converts it
      into block-local reservations along the exact contributing CFG paths.
      Phase 176 dispatches those reservations through validated callbacks in
      observe-only production mode. Phases 177-180 add atomic reservation-state
      commit, function-scan ownership, candidate arbitration, and narrowly
      scoped block-exit spill exemptions. Phase 181 atomically applies complete
      producer/consumer assignment schedules. Phase 182 enables the first
      source-reachable retained branch join in `C`. Phases 183-194 establish
      loop topology, candidate, definition, fact, schedule, cyclic reservation,
      composite plan, and atomic state-plus-assignment application contracts,
      then wire the complete path into production behind fail-closed gates.
      Phases 195-200 classify the concrete representation gap, discover and
      select a complete stack-backed loop value, and handle structured loop
      exits conservatively. Phases 201-202 plan a collision-free promoted temp
      and its complete ordered operand rewrite schedule. Phase 203 adds the
      atomic promotion application transaction boundary. Phase 204 makes the
      post-mutation rebuild decision aware of inserted TAC, rewritten operands,
      and added temp metadata. Phase 205 adds atomic publication and rollback
      for normalized promoted-temp and operand storage. Phases 206-219 add the
      production target adapter, atomic operand mutation, CFG/liveness
      reconstruction, loop-retention activation, exit-path reservation, and
      spill coordination. Phases 220-225 classify loop flow and structured
      exits and make post-mutation fixed-point control checked,
      target-independent, and production-visible. Phases 226-228 select a
      unique defining latch, collect roles across every back edge, and retain
      values through transparent continue paths. Phases 229-231 collect all
      defining latches, generalize retention to multiple producers, and enable
      multiple updating latches in production. Phases 232-234 validate complete
      assignment register facts, select a common target-policy candidate, and
      activate that arbitration in production. Phases 235-237 evaluate all
      candidate registers across the complete loop reservation ranges, compose
      those path facts with assignment compatibility, and activate fail-closed
      path-aware arbitration in production. Phases 238-240 add checked
      candidate-exclusion state, exclusion-aware common-register selection,
      and bounded production retry after candidate-specific reservation
      conflicts. Phases 241-243 enable assignment-produced 8-bit loop values in
      `B`, close physical-`B` assignment materialization, and pin the complete
      source-to-ROM path. Phases 250-252 retain single shift-produced 8-bit loop
      values in `C` while proving multi-value shift conflicts remain stack-
      backed. Broader producer forms and general cross-block split/reload
      scheduling remain.
- [ ] Add spill/reload insertion for longer live ranges instead of relying
      only on single-use block-local retention. Phase 52 handles the first
      narrow case by preserving a retained `HL` array-read base into its
      existing spill slot for later reads, and Phase 53 explicitly reloads a
      later zero-index `ARRAY_WRITE result` base into `BC`. Phase 67 covers a
      source-reachable non-zero-index read-modify-write conflict by preferring
      the final value in `A` and spilling its equally distant index. General
      interval splitting, reload placement for shared non-zero-index bases and
      other operand positions/registers, and cross-block ranges remain. Phase
      74 moves the existing Phase 52/53 preservation and reload constraints
      behind fail-closed target-policy hooks without broadening those cases.
      Phase 112 discovers complete bounded reload chains, and Phase 113 applies
      every block-local site approved by those existing narrow target hooks.
- [ ] Broaden the physical register model beyond the currently migrated
      `A`/`HL` cases, narrow `BC` roles, and Phase 43-46's first
      clobber-aware 8-bit `C`/`B` arithmetic and index paths. Phase 54 adds
      conditional-comparison `arg1` retention in `C`, and Phase 55 completes
      its coverage across all six comparison operators. Comparison `arg2` in
      `C` lacks a source-reachable lowering case, while `B` comparison operands
      and broader `B`/`C` emitter roles remain.
      Phase 56 adds unary `COMPLEMENT` as the first non-binary-arithmetic
      producer for `B`/`C`; Phase 57 adds source-reachable 8-bit normal and
      `__z80_in` array-read results in `C`; Phase 59 adds source-reachable
      8-bit function-call results in `C`; Phase 60 carries those results through
      spilled constant boolean initialization into equality comparison; Phase
      61 adds source-reachable 8-bit `MUL`, `DIV`, and `MOD` results in `C`;
      Phase 62 adds source-reachable 8-bit left- and right-shift results in `C`;
      Phase 63 adds computed left- and right-shift values in `C` across later
      count computation. Phase 96 adds terminal 8-bit `A` and 16-bit `HL`
      producer-to-return forwarding. Phases 241-243 add assignment-produced
      8-bit loop retention and complete assignment destinations in `B`.
      Phases 244-246 add source-reachable 8-bit array-read results in `B` and
      close their concrete backend emission. Phases 247-249 add source-reachable
      8-bit shift results in `B` and close their concrete no-store backend path.
      Phases 250-252 add cross-block left/right shift loop retention in `C` and
      pin unsafe multi-value shift fallback. Call and complex-arithmetic results
      in `B`, broader nonconstant assignment transparency, other loads, and
      other producer classes remain.
      Phase 67 adds a same-consumer tie-break that retains an 8-bit
      `ARRAY_WRITE arg1` value in `A` instead of its equally distant index.
- [x] Decide the current inline-asm contract: keep it as a hard clobber barrier
      and force `(@var)` operands through stack/global `z80_location` lookups.
- [ ] Add explicit inline-asm operand/clobber constraints if inline asm should
      ever consume retained registers or preserve values across asm blocks.
- [x] Audit every backend `find_stack_offset()` use so retained temps either
      resolve through `z80_location` or fail loudly. The only remaining direct
      calls in `compiler/pass_6_z80.c` are now inside `_resolve_z80_location()`.
- [ ] Keep auditing TAC rewrites and new backend helper paths so any future
      retained temp either resolves through `z80_location` or fails loudly.
      Phase 58 also requires these audits to distinguish semantic TAC operand
      width from a conservatively widened reused physical container.
- [ ] Expand SMS regression coverage for larger real pointer-heavy and
      array-heavy programs, then keep both `./run_tests.sh` and
      `./run_tests.sh -windows` green with `-ra` still opt-in. Phase 47 adds
      a compact pointer-heavy smoke test; broader app-style coverage remains.

---

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

Current rollout note: a current inventory audit found 88 `tests/z80/sms/allocator*`
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
