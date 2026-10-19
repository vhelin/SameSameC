#!/usr/bin/env python3
"""Compile and execute the allocator's generated and real-project regressions."""

import base64
from dataclasses import asdict, dataclass
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

from ra_cpu import execute, TRANSFER_CAUSES, EXECUTION_METRICS


ROOT = Path(__file__).resolve().parent.parent
MODES = [
    ("default", []),
    ("no_ra", ["-no-ra"]),
    ("all_spill", ["-ra-all-spill"]),
    ("ra", ["-ra"]),
    ("ra_quiet", ["-ra", "-q"]),
]
MULTIPLY_CONSTANTS = [
    3, 5, 6, 7, 11, 13, 73, 97, 131, 257, 511, 1023, 32769, 65535, -32767, -32768,
]


@dataclass
class Program:
    name: str
    family: str
    variant: int
    constant: int
    widths: list
    returnWidth: int
    pointerParameters: list
    source: str


@dataclass
class Image:
    rom: bytes
    symbols: dict
    banks: dict
    transfers: dict
    staticMetrics: dict
    compileSeconds: float = 0


class ByteBuffer(bytearray):
    """Match Uint8Array stores, including slice/set/fill byte truncation."""

    def __setitem__(self, key, value):
        if isinstance(key, slice):
            super().__setitem__(key, bytes(item & 255 for item in value))
        else:
            super().__setitem__(key, value & 255)

    def copy(self):
        return ByteBuffer(self)

    def set(self, values, offset=0):
        values = bytes(item & 255 for item in values)
        if offset < 0 or offset + len(values) > len(self):
            raise ValueError("Byte-buffer set exceeds bounds")
        self[offset:offset + len(values)] = values

    def fill(self, value, start=0, end=None):
        if end is None:
            end = len(self)
        self[start:end] = [value] * (end - start)


def byte_buffer(values):
    return ByteBuffer(item & 255 for item in values)


def js_int32(value):
    value = int(value) & 0xffffffff
    return value - 0x100000000 if value & 0x80000000 else value


def js_left_shift(value, count):
    return js_int32(js_int32(value) << (count & 31))


def js_unsigned_shift(value, count):
    return (int(value) & 0xffffffff) >> (count & 31)


def js_floor_divide(numerator, denominator):
    # The scalar-use oracle eagerly computes all six candidate divisors, some
    # of which are zero even though the selected source's divisor is nonzero.
    if denominator == 0:
        return math.nan if numerator == 0 else math.copysign(math.inf, numerator)
    return numerator // denominator


def json_default(value):
    if isinstance(value, Program):
        return asdict(value)
    if isinstance(value, (bytes, bytearray)):
        return {str(index): byte for index, byte in enumerate(value)}
    raise TypeError(f"Cannot encode {type(value).__name__}")


def compact_json(value):
    return json.dumps(value, separators=(",", ":"), default=json_default)


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, default=json_default), encoding="utf-8")


def read_text(path):
    # SSC templates and assembly must not undergo universal-newline rewriting.
    with Path(path).open(encoding="utf-8", newline="") as stream:
        return stream.read()


def read_output(path):
    # Native Windows tools emit CRLF. Normalize their textual output before
    # applying multiline anchors, without changing any frozen SSC source bytes.
    return read_text(path).replace("\r\n", "\n").replace("\r", "\n")


def write_text(path, text):
    with Path(path).open("w", encoding="utf-8", newline="") as stream:
        stream.write(text)


def run(command, args, cwd, log):
    path = Path(cwd) / log
    try:
        with path.open("wb") as stream:
            result = subprocess.run(
                [str(command), *(str(argument).replace("\\", "/") for argument in args)],
                cwd=cwd, stdin=subprocess.DEVNULL, stdout=stream, stderr=stream,
                check=False,
            )
    except OSError as error:
        raise RuntimeError(f"{command} failed; see {path}: {error}") from error
    if result.returncode:
        raise RuntimeError(f"{command} failed; see {path}: {result.returncode}")


def verify_allocator(output):
    fixture = ROOT / "tests" / "z80" / "sms" / "allocator_ra_core_liveness_extraction"
    sources = [
        path for path in (ROOT / "compiler").iterdir()
        if path.suffix == ".c" and path.name not in ("main.c", "pass_5.c")
    ]
    harness = os.environ.get("RA_VERIFIER") or str(
        output / ("allocation_verify_test.exe" if sys.platform == "win32" else "allocation_verify_test")
    )
    if not os.environ.get("RA_VERIFIER"):
        run(os.environ.get("CC", "gcc"), [
            "-ansi", "-pedantic", "-Wall", "-DNDEBUG", "-DSAMESAMEC_TRACE", "-o",
            harness, fixture / "allocation_verify_test.c", *sources, "-lm",
        ], output, "verifier-build.log")
    required = [
        "allocation_mutation_test: invalid_tail unchanged=yes status=complete",
        "allocation_mutation_test: displaced_arg2 cleared=yes status=complete",
        "allocation_policy_test: register_pairs=25 independent_units=yes status=complete",
        "allocation_policy_test: frame_word_span=-128..126 high_byte_bounded=yes status=complete",
        "spill_slot_test: cases=10 overlap_width_boundary_redefinition_controls=yes tacs_unchanged=yes status=complete",
        "spill_slot_test: fixed_prefix=yes shared_suffix=yes invalid_fixed=11 immutable=yes status=complete",
        "spill_slot_test: fixed_owners=3 byte_word_mixed=yes prefix_stable=yes suffix_shared=yes tacs_unchanged=yes status=complete",
        "allocation_cost_test: nearer_rejected=yes farther_reused=yes status=complete",
        "allocation_cost_test: weighted_cycles=yes static_bytes=yes instruction_bound=256 fallback=next_use immutable=yes status=complete",
        "allocation_cost_test: split_reload_overlap_controls=4 status=complete",
        "allocation_cost_test: rejected_first_alternative=yes known_alternative_over_unknown_fallback=yes status=complete",
        "allocation_loop_stack_test: body_controls=8 boundary_rewrites_complete=yes immutable=yes status=complete",
        "candidate_definition_test: read_before_write=yes previous_definition_excluded=yes status=complete",
    ]
    patterns = [
        r"allocation_policy_test: lowering_effects=\d+ direct_bc_complement=yes scratch_hl_rejected=yes full_barriers=yes status=complete",
        r"allocation_policy_test: direct_bc_bitwise=yes unsigned_widening=yes signed_right_fallback=yes bc_overlap_rejected=yes status=complete",
        r"allocation_policy_test: byte_word_bitwise=\d+ unsigned_only=yes constants_bounded=yes flags_gated=yes unchanged=yes borrow_word_sub=yes arg2_a_rejected=yes arg2_bc_fallback=yes unit_step_fallback=yes status=complete",
        r"allocation_policy_test: endpoints=10200 supported=\d+ rejected=\d+ independent_table=yes status=complete",
    ]
    for flags in ([], ["-q"]):
        log = "verifier-quiet.log" if flags else "verifier.log"
        run(harness, flags, output, log)
        text = read_output(output / log)
        if "status=missing_or_clobbered_definition" not in text:
            raise RuntimeError("Verifier failure diagnostics missing")
        failures = text.count("result=rejected unchanged=yes status=complete")
        if (failures != 32 or text.count("result=accepted unchanged=yes status=complete") != 7
                or any(value not in text for value in required)
                or any(not re.search(pattern, text) for pattern in patterns)):
            raise RuntimeError("Verifier coverage count mismatch")
    return harness, failures


def generate_programs(comparison_forms):
    real_loop_body = read_text(ROOT / "tests/z80/sms/calculations-1/bank_2.ssc").split(
        "void calculateAndPrint3(u8 *chars, u8 one, u8 zero) {"
    )[1].split("\n  extra1 =")[0]
    real_index_body = real_loop_body.split("__z80_out")[0]
    real_while_body = re.search(
        r"^  while \(start < chars\) \{[\s\S]*?^  \}",
        read_text(ROOT / "tests/z80/sms/calculations-1/bank_1.ssc"), re.M,
    )[0]
    programs = []
    for variant in range(38):
        constant = 1 + variant * 7
        operator = ["+", "-", "&", "|", "^"][variant % 5]
        comparison = ["==", "!=", "<", ">", "<=", ">="][variant % 6]
        byte_comparison = "128" if comparison_forms else "c"
        word_comparison = "32768" if comparison_forms else "c"
        signed_comparison = "(0-1)" if comparison_forms else "c"
        signed_word = variant in (4, 5) or variant >= 8
        expression = f"saved{operator}c" if variant & 1 else f"c{operator}saved"
        inner = "column" if variant & 4 else "0"
        call_argument = "previous" if variant & 1 else f"alias[start+row+{inner}]"
        call = f"u8 called = ra_signed_helper({call_argument},selector,row);"
        scalar_type = "u8" if variant >= 12 or variant in (4, 7, 9, 11) else "s8" if variant in (6, 8) else "u16"
        pointer_index = variant == 9 or variant >= 12
        byte_index = variant in (14, 15, 16, 20, 21, 22, 23, 24)
        # Out-of-range rows are constructed, then excluded by the same family
        # predicates as the original runner.
        multiply_operand = "c" if variant == 32 else (
            f"({MULTIPLY_CONSTANTS[variant >> 1]})" if variant >> 1 < len(MULTIPLY_CONSTANTS) else "(undefined)"
        )
        index_bodies = [
            "u16 index = a; return ra_buffer[index];",
            "s16 index = a; u8 *target = &ra_buffer[32]; return target[index];",
            "u16 index = a; return ra_words[index];",
            "s16 index = a; u16 *target = &ra_words[16]; return target[index];",
            "u8 index = a; return ra_buffer[index];",
            "s8 index = a; u8 *target = &ra_buffer[32]; return target[index];",
            "u16 index = a; ra_buffer[index] = b; return ra_buffer[index];",
            "u16 index = a; u16 copied = index; return ra_buffer[copied];",
            "u16 index = 7; return ra_buffer[index];",
            "u16 index = a; target[index] = b; return alias[index];",
            "u16 index = a; ra_buffer[index++] = b; return index;",
            "u16 index = a; ra_words[index++] = b; return index;",
            "return target[0];",
            "return target[7];",
            "u8 workspace[136]; workspace[a&127] = b; u16 loaded = target[a]; return loaded+workspace[a&127];",
            "u16 first = target[a]; alias[a] = b; return first+target[a];",
            "u16 first = target[a]; ra_pointer_helper(&target[a],b); return first+target[a];",
            "u16 live = a+b; return target[a]+live;",
            "s8 index = a; return target[index];",
            "s16 index = a; return target[index];",
            "return target[a];",
            "ra_index_pointer = target; u16 first = ra_index_pointer[a]; ra_index_pointer = alias; return first+ra_index_pointer[a];",
            *[
                f"u8 workspace[{size}]; u8 *local = target; workspace[a&63] = b; u16 loaded = local[a]; return loaded+workspace[a&63];"
                for size in (114, 115, 116)
            ],
            "return target[a];",
            "u8 workspace[136]; u8 *local; if (b&2) { local = target; } else { local = alias; } workspace[a&127] = b; u16 loaded = local[a]; return loaded+workspace[a&127];",
            "u16 first = target[a]; alias[a] = b; return first+target[a];",
            "u16 first = target[a]; ra_pointer_helper(&target[a],b); return first+target[a];",
            "ra_index_pointer = target; u8 called = ra_signed_helper(b,a,0); u16 first = ra_index_pointer[a]; ra_index_pointer = alias; return first+ra_index_pointer[a]+called;",
        ]
        scalar_bodies = [
            "u16 saved = a^b; if (saved < c) { return a; } return b;",
            "u16 saved = a^b; return saved << (c&15);",
            "u16 saved = a&15; return b << saved;",
            "u16 saved = a&127; return ra_buffer[saved];",
            "u8 saved = a^b; return ra_signed_helper(saved,b,c);",
            "u16 saved = a^b; return calculationAdd3(saved,b,c);",
            "s8 saved = a^b; if (saved < c) { return 1; } return 0;",
            "u8 saved = a^b; return saved >> (c&7);",
            "s8 saved = a; u8 *target = &ra_buffer[32]; return target[saved];",
            "u8 saved = a^b; u8 called = ra_signed_helper(saved,b,c); return called^saved;",
            "u16 saved; if (b&1) { saved = a; } else { saved = c; } return (saved+b)^saved;",
            "u8 saved = a; ra_pointer_helper(&saved,b); return saved;",
            *[
                f"return (c+0x9234)/({'a' if index < 3 else 'b'}{['&', '|', '^'][index % 3]}{'b' if index < 3 else 'a'});"
                for index in range(6)
            ],
            *[
                f"return ((c+0x9234)/3)|({difference});"
                for difference in ("a-b", "b-a", "a-1", "1-a", "a-0", "0-a", "a-255", "255-a")
            ],
            "return (a-b)|((c+0x9234)/3);",
            "return (b-a)|((c+0x9234)/3);",
            "u8 workspace[136]; workspace[a&127] = b; return ((a-b)|((c+0x9234)/3))+workspace[a&127];",
            "return (a-1)|((c+0x9234)/3);",
        ]
        sentinel_bodies = [
            "u16 live = a+b; u16 saved = ~c; return (live+saved)^live;",
            "u16 live = a+b; u16 saved = ~c; return (live^saved)+live;",
            "s8 saved = a^b; s16 wide = saved; u16 live = c+c; return wide+live;",
            "s8 saved = a^b; s16 wide = saved; u16 live = c+c; return live+wide;",
            "u16 live = a+b; u8 low = c; return (live+low)^live;",
            "u16 live = a+b; u8 low = c; return (low+live)^live;",
            "u16 live = a+b; return (live << (c&15))^live;",
            "u16 live = a+b; return (live >> (c&15))^live;",
            "u8 *target = &ra_buffer[32]; u8 live = b^c; target[a] = live; u16 loaded = target[a]; return loaded+live;",
            "u8 *target = &ra_buffer[32]; u8 live = b^c; s8 index = a^1; target[index] = live; u16 loaded = target[index]; return loaded+live;",
        ]
        functions = [
            ("matrix_byte", "u8 a, u8 b, u8 c", f"u8 saved = a^b; return {expression};", [1, 1, 1], 1),
            ("matrix_word", "u16 a, u16 b, u16 c", f"u16 saved = ~b; return {expression};", [2, 2, 2], 2),
            ("matrix_affinity", "u16 a, u16 b, u16 c", f"u16 live = a+b; return {'(~c)' if variant < 4 else 'live'} {['+', '&', '|', '^'][variant % 4]} {'live' if variant < 4 else '(~c)'};", [2, 2, 2], 2),
            ("matrix_rank", "u16 a, u16 b, u16 c", f"u16 live = ~a; u16 saved = ~c; return live {['+', '&', '|', '^'][variant % 4]} saved;", [2, 2, 2], 2),
            ("matrix_constant_multiply", "s16 a, s16 b, u16 c" if variant >= 28 else "u16 a, u16 b, u16 c",
             f"return ({multiply_operand + '*(a^b)' if variant & 1 else '(a^b)*' + multiply_operand})+c;", [2, 2, 2], 2),
            ("matrix_variable_multiply", [
                "u16 a, u16 b, u16 c", "u16 a, u16 b, u16 c",
                "s16 a, s16 b, u16 c", "s16 a, s16 b, u16 c", "u8 a, u16 b, u16 c",
                "u16 a, u8 b, u16 c", "s8 a, u16 b, u16 c", "u16 a, s8 b, u16 c",
            ][variant] if variant < 8 else None,
             f"return ({'(a^c)' if variant == 1 else 'a'}*{'(b^c)' if variant == 3 else 'b'})+c;",
             [1, 2, 2] if variant in (4, 6) else [2, 1, 2] if variant in (5, 7) else [2, 2, 2], 2),
            ("matrix_constant_shift", "s16 a, s16 b, u16 c" if variant & 4 else "u16 a, u16 b, u16 c",
             f"return ((a^b){'>>' if variant & 1 else '<<'}{'(' + str(variant >> 1) + ')' if variant < 36 else 'c'})+c;", [2, 2, 2], 2),
            ("matrix_index_affinity", "u16 a",
             f"{'u8 *target = &ra_buffer[0]; ' if variant else ''}u16 index = ~a; return {'target' if variant else 'ra_buffer'}[index];", [2], 1),
            ("matrix_index_copy",
             f"{'u16' if variant == 20 else 'u8'} *target, u8 *alias, {'u8' if byte_index else 'u16'} a, u8 b" if pointer_index else f"{'s8' if variant == 5 else 'u8' if variant == 4 else 'u16'} a, u8 b",
             index_bodies[26 if variant >= 30 else variant].replace("workspace[136]", f"workspace[{variant + 83 if variant >= 30 else 136}]", 1),
             [2, 2, 1 if byte_index else 2, 1] if pointer_index else [1 if variant in (4, 5) else 2, 1],
             2 if variant >= 30 or variant in (2, 3, 10, 11, 14, 15, 16, 17, 20, 21, 22, 23, 24, 26, 27, 28, 29) else 1),
            ("matrix_bc_alu", "u16 a, u16 b, u16 c",
             f"u16 saved = a{operator}b; return c{'-' if variant < 5 else '+'}saved;" if variant < 10 else
             "u16 live = a+c; u16 saved = b+c; return (live-saved)^live;" if variant == 10 else
             "u8 small = a; u16 saved = b+c; return (saved+small)^small;", [2, 2, 2], 2),
            ("matrix_scalar_uses", f"{scalar_type} a, {scalar_type} b, {scalar_type} c",
             scalar_bodies[variant] if variant < len(scalar_bodies) else None, [2 if scalar_type == "u16" else 1] * 3, 2),
            ("matrix_real_index", "u8 *chars, u8 one, u8 zero", f"{real_index_body} return index;", [2, 1, 1], 2),
            ("matrix_source_loop", "u8 *start, u8 *chars" if variant else "u8 *chars, u8 one, u8 zero",
             real_while_body if variant else f"{real_loop_body} return index;", [2, 2] if variant else [2, 1, 1], 0 if variant else 2),
            ("matrix_compare_byte", "u8 a, u8 b, u8 c",
             f"u8 saved = {'a' if comparison_forms == 2 else 'a^b'}; if ({'saved' if variant < 6 else byte_comparison} {comparison} {byte_comparison if variant < 6 else 'saved'}) {{ return a; }} return b;", [1, 1, 1], 1),
            ("matrix_compare_word", "u16 a, u16 b, u16 c",
             f"u16 saved = {'a' if comparison_forms == 2 else '~b'}; if ({'saved' if variant < 6 else word_comparison} {comparison} {word_comparison if variant < 6 else 'saved'}) {{ return a; }} return b;", [2, 2, 2], 2),
            ("matrix_compare_signed", "s8 a, s8 b, s8 c",
             f"s8 saved = {'a' if comparison_forms == 2 else 'a^b'}; if ({'saved' if variant < 6 else signed_comparison} {comparison} {signed_comparison if variant < 6 else 'saved'}) {{ return a; }} return b;", [1, 1, 1], 1),
            ("matrix_extend", "s8 a, s8 b, u16 c", f"s8 saved = a^b; s16 extended = saved; return {'c+extended' if variant & 1 else 'extended+c'};", [1, 1, 2], 2),
            ("matrix_low_byte", f"{'s16' if signed_word else 'u16'} a, u16 b, u16 c",
             f"{'s16' if signed_word else 'u16'} saved = ~a; {'s8' if variant >= 6 else 'u8'} narrowed = {('b+saved' if variant & 1 else 'saved+b') if variant < 2 else 'saved'}; {'s16 extended = narrowed;' if variant >= 6 else ''} return {'c+narrowed' if variant < 2 else (('extended' if variant >= 6 else 'narrowed') + '+(b+c)') if variant & 1 else '(b+c)+' + ('extended' if variant >= 6 else 'narrowed')};", [2, 2, 2], 2),
            ("matrix_shift", "u16 a, u16 b, u8 c", f"u16 saved = ~a; return saved {'>>' if variant & 1 else '<<'} (c&15);", [2, 2, 1], 2),
            ("matrix_sentinel", "s8 a, u8 b, u8 c" if variant >= 8 else "s8 a, s8 b, u16 c" if variant in (2, 3) else "u16 a, u16 b, u16 c",
             sentinel_bodies[variant] if variant < len(sentinel_bodies) else None, [1, 1, 1] if variant >= 8 else [1, 1, 2] if variant in (2, 3) else [2, 2, 2], 2),
            ("matrix_structural", "u8 *target, u8 *alias, u8 start, u8 count, u8 selector, u8 value",
             f"u16 total = value; u8 saved = selector^value; u8 row = 0; while (row < count) {{ {'u8 column = 0; while (column < 2) {' if variant & 4 else ''} u8 previous = target[start+row+{inner}]; {call if variant & 1 else ''} if ((previous^selector)&1) {{ alias[start+row+{inner}] = previous+saved; }} else {{ target[start+row+{inner}] = previous^saved; }} {'' if variant & 1 else call} total = total+target[start+row+{inner}]+called; {('u8 narrowed = total; if (row&1) { total = ' + ('narrowed+total' if variant & 2 else 'total+narrowed') + '; } else { total = total+(selector&7); }') if variant >= 8 else ''} {'column = column+1; }' if variant & 4 else ''} row = row+1; }} return total;", [2, 2, 1, 1, 1, 1], 2),
            ("matrix_byte_store", "u8 *target, u8 *alias, u16 a, u16 b", [
                "target[a] = b; return target[a];",
                "target[a] = 173; return target[a];",
                "u8 value = b^a; target[a] = value; return target[a];",
                "s8 index = a; s8 value = b; target[index] = value; return target[index];",
                "target[0] = b; return target[0];",
                "ra_index_pointer = alias; ra_index_pointer[a] = b; return target[a];",
                "u8 workspace[136]; workspace[a] = b; target[a] = workspace[a]; u16 first = workspace[a]; return target[a]+first;",
                "if (b&1) { target[a] = b; } else { alias[a] = b; } u16 first = target[a]; return first+alias[a];",
                "u8 value = b; u8 *escaped = &value; target[a] = escaped[0]; return target[a];",
                "u8 called = ra_signed_helper(b,a,0); target[a] = called; return target[a];",
                "u16 value = b^0x9234; target[a] = value; return target[a]+value;",
                "u16 i; for (i = 0; i < a; i++) { target[i] = alias[i]; } return target[0];",
                "u16 *words = target; words[a] = b; return words[a];",
                "u16 first = a+b; u16 second = a^b; target[a] = first; return second+target[a]+first;",
                "s8 index = a; u8 value = b^a; target[index] = value; u16 first = target[index]; return first+value;",
                "target[a] = b; alias[a] = target[a]+1; u16 first = target[a]; return first+alias[a];",
            ][variant] if variant < 16 else None, [2, 2, 2, 2], 2),
            ("scalar_byte", "u8 a, u8 b, u8 c", "u8 saved = a^b; return (c+c)+saved;", [1, 1, 1], 1),
            ("scalar_signed_byte", "s8 a, s8 b, s8 c", "s8 saved = a^b; return (c|7)^saved;", [1, 1, 1], 1),
            ("scalar_signed_word", "s16 a, s16 b, s16 c", "s16 saved = a^b; return (c|7)^saved;", [2, 2, 2], 2),
            ("scalar_widen", "u8 a, u8 b, u8 c", "u16 saved = a+b; return (c+c)+saved;", [1, 1, 1], 2),
            ("scalar_word", "u16 a, u16 b, u16 c", "u16 saved = a^b; return (c+c)+saved;", [2, 2, 2], 2),
            ("scalar_reassigned_word", "u16 a, u16 b, u16 c", f"u16 saved = a^b; u16 first = c+saved; {'if (a&1) {' if variant else ''} saved = b^c; {'}' if variant else ''} return (first+saved)^saved;", [2, 2, 2], 2),
            ("scalar_reassigned_byte", f"{'s8' if variant else 'u8'} a, {'s8' if variant else 'u8'} b, {'s8' if variant else 'u8'} c",
             f"{'s8' if variant else 'u8'} saved = a^b; {'s8' if variant else 'u8'} first = c+saved; saved = a^c; return (first+saved)^saved;", [1, 1, 1], 1),
            ("scalar_lifetime_hole", "u16 a, u16 b, u16 c", f"u16 saved = a^b; u16 first = c+saved; {'u8 called = ra_signed_helper(a,b,c);' if variant else 'u8 called = 0;'} u16 middle = b+c+called; saved = a+c; return (first^middle)+(saved^middle);", [2, 2, 2], 2),
            ("scalar_switch_continue", "u8 a, u8 b, u8 c", [
                "u16 total = b; u8 i; for (i = 0; i < (a&7); i++) { switch (i&3) { case 0: switch (b&1) { case 0: continue; default: continue; } default: total = total+c+i; break; } total = total+1; } i = 0; do { i++; switch (i&3) { case 0: continue; default: total = total+c+i; break; } total = total+1; } while (i < (a&7)); return total;",
                "u16 total = b; u8 i = 0; u8 j; while (i < (a&7)) { i++; switch (i&1) { case 0: for (j = 0; j < 3; j++) { switch (j) { case 1: continue; default: total = total+c; break; } total = total+1; } break; default: continue; } total = total+i; } return total;",
            ][variant] if variant < 2 else None, [1, 1, 1], 2),
            ("scalar_source_reassigned", "u16 a, u8 b, u8 c", f"s16 result = 0x7521; u8 low = result; ra_buffer[0] = low; result = (((a|b)%0x1001)<<4)-10000; result ^= 0x4321; u8 next = result; ra_buffer[1] = next; result = (((({'(c|a)|(b|c)' if variant else '(a|c)|(c|b)'})%0x1001)<<4)-(c|10000)); result ^= (0x4321|c); return result;", [2, 1, 1], 2),
            ("scalar_source_bitwise", "u16 a, u8 b, u8 c", f"return (((({'(c|a)|(b|c)' if variant else '(a|c)|(c|b)'}) % 0x1001) << 4) - (c|10000)) ^ (0x4321|c);", [2, 1, 1], 2),
            ("scalar_copy", "u16 a, u16 b, u16 c", "u16 saved = a; return (b+c)+saved;", [2, 2, 2], 2),
            ("scalar_complement_sub", "u16 a, u16 b, u16 c", "return (a+b)-(~c);", [2, 2, 2], 2),
            ("scalar_multi_word", "u16 a, u16 b, u16 c", "u16 left = a+b; u16 saved = ~c; return ((left+saved)^saved)-saved;", [2, 2, 2], 2),
            ("scalar_multi_rmw", "u16 a, u16 b, u16 c", "u16 saved = ~c; return ((a+saved)^saved)+b;", [2, 2, 2], 2),
            ("scalar_multi_byte", "u8 a, u8 b, u8 c", "u8 saved = a^b; return ((c+saved)^saved)+saved;", [1, 1, 1], 1),
            ("scalar_rank_fallback", f"u16 a, u16 b, {'u8' if variant else 'u16'} c", f"u16 saved = ~a; u16 first = saved+b; u16 second = saved+first; if (c&{1 << variant}) {{ {'return second+saved;' if variant else 'second = second+saved;'} }} return second;", [2, 2, 1 if variant else 2], 2),
            ("scalar_frequency_loop", "u8 a, u8 b, u8 c", f"u8 value = a^{constant}; while (value < b) {{ value = value+1; }} return value;", [1, 1, 1], 1),
            ("scalar_split_byte", "u8 a, u8 b, u8 c", "u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; return ((((first+loaded)^saved)+saved)^saved)+saved;", [1, 1, 1], 1),
            ("scalar_region", "u8 a, u8 b, u8 c", f"u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; u8 out = ((((first+loaded)^saved)+saved)^saved)+saved; if (a < b) {{ {'ra_buffer[2] = out;' if variant == 1 else ''} out = out+1; }} return ((out^saved)+saved)^saved;", [1, 1, 1], 1),
            ("scalar_region_escape", "u8 a, u8 b, u8 c", "u8 saved = a^b; if (a < b) { ra_pointer_helper(&saved,c); } return saved+b;", [1, 1, 1], 1),
            ("scalar_call_region", "u8 a, u8 b, u8 c", f"u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; u8 out = ((((first+loaded)^saved)+saved)^saved)+saved; u8 called = ra_pointer_helper(ra_buffer,ra_signed_helper({'ra_recursive_helper(2,a)' if variant == 1 else 'a'},b,c)); return ((((out^saved)+saved)^saved)+saved)^called;", [1, 1, 1], 1),
            ("scalar_loop_pressure", "u8 a, u8 b, u8 c", "u8 left = a; u8 right = b; while (left < right) { left = left+1; right = right-1; } return left^right;", [1, 1, 1], 1),
            ("scalar_loop_call", "u8 a, u8 b, u8 c", "u8 saved = a^b; u8 value = b; while (c) { value = ra_pointer_helper(ra_buffer,value)+saved; c = c-1; } return value;", [1, 1, 1], 1),
            ("scalar_word_counter", "u16 a, u16 b, u16 c", [
                "u16 i = a; while (i != c) { i++; } return i+1;",
                "u16 i = a; while (i != c) { if (i&1) { i++; continue; } i++; } return i+b;",
                "u16 i = a; while (i != c) { if (i == b) { break; } i++; } return i+b;",
                "u16 i = a; while (i != c) { u16 j; for (j = 0; j < 2; j++) {} ra_buffer[0] = j; i++; } return i+b;",
                "u16 i = a; while (i != c) { ra_pointer_helper(ra_buffer,b); i++; } return i+b;",
                "u16 i = a; u16 *escaped = &i; while (i != c) { i++; } return escaped[0]+b;",
                "u16 i = a; while (i != c) { if (i == b) { return i+b; } i++; } return i+b;",
                "u16 i = a; u16 crc = b; while (i != c) { if (crc&1) { crc = (crc>>1)^0xA001; } else { crc = crc>>1; } i++; } return crc+i;",
            ][variant] if variant < 8 else None, [2, 2, 2], 2),
            ("scalar_word_prefix", "u16 a, u16 b, u16 c",
             f"u16 i = a; u16 first = i+b; while (i != c) {{ i++; }} {'i = a; while (i != c) { i++; }' if variant else ''} return first+i;", [2, 2, 2], 2),
            ("scalar_word_step", "u16 a, u16 b, u16 c", [
                "a++; return a;",
                "u16 value = a+1; return value^a;",
                "u16 value = a; u16 *escaped = &value; value++; return escaped[0];",
                "u8 value = a; u16 widened = value; widened++; return widened;",
                "s8 value = a; s16 widened = value; widened++; return widened;",
                "return a+256;",
                "return 256+a;",
                "return a-256;",
                "a = a+256; return a;",
                "u8 *next = &ra_buffer[a&15]; next = next+256; next = next-256; next++; return next[0];",
                "u16 i; u16 count = b&31; for (i = 0; i < count; i++) { ra_buffer[i+1] = ra_buffer[i]; } return ra_buffer[count];",
                "u16 i; u16 count = b&31; for (i = 0; i < count; i++) { ra_buffer[i] = ra_buffer[i+1]; } return ra_buffer[0];",
                "u8 workspace[136]; u16 value = a; u16 *escaped = &value; workspace[b&127] = c; value++; return escaped[0]+workspace[b&127];",
                "return a+255;",
                "return a+(0-256);",
            ][variant] if variant < 15 else None, [2, 2, 2], 2),
            ("scalar_decision_loop", "u16 a, u8 count, u8 value", f"u16 saved = a; u16 out = a^0x4321; u8 left = value; u8 right = count+value; while (count) {{ if (count&1) {{ saved = out^count; }} else {{ saved = saved+left; }} left = left+1; right = right-1; {'left = left+ra_pointer_helper(ra_buffer,right);' if variant else ''} u8 combined = left+right; out = (out+saved)^combined; ra_buffer[count] = left^right; count = count-1; }} return out+saved+left+right;", [2, 1, 1], 2),
            ("scalar_cost_reject_byte", "u8 a, u8 b, u8 c", "u8 saved = a^b; u8 first = c+saved; u8 loaded = ra_buffer[1]; return ((first+loaded)^saved)+saved;", [1, 1, 1], 1),
            ("scalar_split_word", "u16 a, u16 b, u16 c", "u16 saved = ~c; u16 first = a+saved; u16 loaded = ra_buffer[1]; return ((first+loaded)^saved)+b+saved;", [2, 2, 2], 2),
            ("scalar_word_index", "u16 a, u16 b, u8 c", f"u16 out = a+b; ra_buffer[{constant}] = c; return out+ra_buffer[{constant}];", [2, 2, 1], 2),
            ("scalar_narrow", "u16 a, u16 b, u16 c", "u8 saved = a+b; return (c+c)+saved;", [2, 2, 2], 2),
            ("scalar_before_call", "u8 a, u8 b, u8 c", f"u8 called = ra_arithmetic_{variant}(a,b,c); u8 saved = a^b; return (called+called)+saved;", [1, 1, 1], 1),
            ("scalar_after_call", "u8 a, u8 b, u8 c", f"u8 saved = a^b; u8 out = (c+c)+saved; ra_buffer[0] = ra_arithmetic_{variant}(a,b,c); return out;", [1, 1, 1], 1),
            ("scalar_call_argument", "u8 a, u8 b, u8 c", f"u8 saved = a^b; return ra_arithmetic_{variant}(saved,b,c);", [1, 1, 1], 1),
            ("scalar_across_call", "u8 a, u8 b, u8 c", f"u8 saved = a^b; u8 called = ra_arithmetic_{variant}(a,b,c); return (called+c)+saved;", [1, 1, 1], 1),
            ("scalar_signed_call", "u8 a, u8 b, u8 c", "u16 first = a; return first+ra_signed_helper(a,b,c);", [1, 1, 1], 2),
            ("scalar_slots", "u8 a, u8 b, u8 c", "ra_buffer[0] = ra_buffer[a]^ra_buffer[b]^(a+b); return ra_buffer[c]^ra_buffer[b+1]^(b+c);", [1, 1, 1], 1),
            ("scalar_remat_byte", "u8 first, u8 second", "u8 *base = &ra_buffer[3]; u8 out = base[first]; out = out+base[second]; return out;", [1, 1], 1),
            ("scalar_remat_word", "u8 first, u8 second", "u16 *base = &ra_words[3]; u16 out = base[first]; out = out+base[second]; return out;", [1, 1], 2),
            ("scalar_remat_pointer", "u8 *source, u8 first, u8 second", "u8 *base = &source[3]; u8 out = base[first]; out = out+base[second]; return out;", [2, 1, 1], 1),
            ("word_bc", "u16 a, u16 b, u16 c", f"return (c+c)+(a{operator}b);", [2, 2, 2], 2),
            ("arithmetic", "u8 a, u8 b, u8 c", f"return (a+b)+((c+{constant})+c);", [1, 1, 1], 1),
            ("word", "u16 a, u16 b, u16 c", f"return (a+b)+((c+{constant})+c);", [2, 2, 2], 2),
            ("branch", "u8 a, u8 b, u8 c", f"u8 out; if (a < b) {{ out = (a+c)^{constant}; }} else {{ out = (b+c)^{constant}; }} return out;", [1, 1, 1], 1),
            ("loop", "u8 count, u8 value", f"while (count) {{ value = (value+{constant})^count; count = count-1; }} return value;", [1, 1], 1),
            ("array", "u8 first, u8 second, u8 a, u8 b", "return ra_buffer[first]^ra_buffer[second]^(a+b);", [1, 1, 1, 1], 1),
            ("pointer", "u8 *target, u8 left, u8 right, u8 value", f"target[left+right] = target[left+right]+value+{constant}; return target[left+right];", [2, 1, 1, 1], 1),
            ("call", "u8 a, u8 b, u8 c", f"return ra_arithmetic_{variant}(a,b,c)^{constant};", [1, 1, 1], 1),
            ("pressure", "u8 left, u8 right, u8 value", f"u16 first = left+right; u16 second = right+value; u16 third = left+value; u16 fourth = first^{constant}; first = first+ra_arithmetic_{variant}(left,right,value); return (first+second)^(third+fourth);", [1, 1, 1], 2),
            ("walk", "u8 *target, u8 start, u8 count, u8 value", f"u8 index = start; u8 total = value; while (count) {{ u8 previous = target[index]; target[index] = previous+total+{constant}; total = total^previous; index = index+{1 + variant % 3}; count = count-1; }} return total;", [2, 1, 1, 1], 1),
            ("nested", "u8 *target, u8 start, u8 rows, u8 columns", f"u8 row = 0; u16 total = {constant}; while (row < rows) {{ u8 column = 0; while (column < columns) {{ u8 index = start+row*8+column; u8 previous = target[index]; total = total+previous+row; target[index] = previous^column^{constant}; column = column+1; }} row = row+1; }} return total;", [2, 1, 1, 1], 2),
            ("join_call", "u8 *target, u8 index, u8 selector, u8 value", f"u16 retained = target[index]+{constant}; u8 called = ra_arithmetic_{variant}(value,selector,target[index]); if (selector&{1 << (variant % 4)}) {{ target[index] = called+value; }} else {{ target[index+1] = called^value; retained = retained+selector; }} return retained+target[index]+target[index+1];", [2, 1, 1, 1], 2),
        ]
        limits = {
            "matrix_byte": 10, "matrix_word": 10, "matrix_compare_byte": 12,
            "matrix_compare_word": 12, "matrix_compare_signed": 12, "matrix_low_byte": 10,
            "matrix_structural": 12, "matrix_sentinel": 10, "matrix_affinity": 8,
            "matrix_rank": 4, "matrix_index_copy": 33, "matrix_bc_alu": 12,
            "matrix_scalar_uses": 30, "matrix_real_index": 1, "matrix_constant_multiply": 33,
            "matrix_constant_shift": 38, "matrix_variable_multiply": 8,
            "matrix_byte_store": 16,
        }
        for family, parameters, body, widths, return_width in functions:
            word_boundary = family == "matrix_index_copy" and variant >= 30
            arithmetic_family = family in (
                "matrix_constant_multiply", "matrix_constant_shift", "matrix_variable_multiply",
            )
            # Dedicated cohorts keep the all-spill images within one ROM bank.
            if family in ("scalar_word_step", "matrix_byte_store") and family not in os.environ.get("RA_FAMILIES", "").split(","):
                continue
            if variant >= (15 if family == "scalar_word_step" else 8) and not family.startswith("matrix_"):
                continue
            if family.startswith("matrix_") and variant >= limits.get(family, 2):
                continue
            if arithmetic_family and comparison_forms != 1 and not os.environ.get("RA_FAMILIES"):
                continue
            if comparison_forms and family != "arithmetic" and not family.startswith("matrix_compare_") and not arithmetic_family:
                continue
            if os.environ.get("RA_FAMILIES") and family != "arithmetic" and family not in os.environ["RA_FAMILIES"].split(","):
                continue
            value_family = family in (
                "matrix_index_copy", "matrix_bc_alu", "matrix_scalar_uses",
                "matrix_real_index", "matrix_source_loop",
                "matrix_byte_store",
            )
            if not word_boundary and family.startswith("matrix_") and value_family != (os.environ.get("RA_VALUE_CASES") == "1"):
                continue
            scalar_family = family.startswith("scalar_") or family == "word_bc"
            if family != "arithmetic":
                if word_boundary:
                    excluded = os.environ.get("RA_SCALAR_CASES") != "1"
                elif os.environ.get("RA_MATRIX_CASES") == "1":
                    excluded = not family.startswith("matrix_")
                else:
                    excluded = family.startswith("matrix_") or scalar_family != (os.environ.get("RA_SCALAR_CASES") == "1")
                if excluded:
                    continue
            if scalar_family and family not in ("word_bc", "scalar_word_counter", "scalar_word_step") and variant > 1:
                continue
            name = f"ra_{family}_{variant}"
            if family == "matrix_source_loop":
                pointer_parameters = [0, 1] if variant else [0]
            elif family == "matrix_real_index":
                pointer_parameters = [0]
            elif family in ("matrix_structural", "matrix_byte_store") or (family == "matrix_index_copy" and pointer_index):
                pointer_parameters = [0, 1]
            elif family in ("pointer", "walk", "nested", "join_call", "scalar_remat_pointer"):
                pointer_parameters = [0]
            else:
                pointer_parameters = []
            return_type = (
                f"{'s' if family.startswith('scalar_signed') else 'u'}{return_width * 8}"
                if return_width else "void"
            )
            programs.append(Program(
                name, family, variant, constant, widths, return_width, pointer_parameters,
                f"{return_type} {name}({parameters}) {{ {body} }}",
            ))
    preamble = (
        "u8 ra_buffer[128];\nu8 *ra_index_pointer;\nu16 ra_words[64];\n"
        "s8 ra_signed_helper(u8 a, u8 b, u8 c) { return a+b+c; }\n"
        "u8 ra_pointer_helper(u8 *target, u8 value) { target[0] = value; return target[1]; }\n"
        "u8 ra_recursive_helper(u8 depth, u8 value) { if (depth) { u8 saved = value^depth; u8 nested = ra_recursive_helper(depth-1,value+1); return nested^saved; } return value; }\n"
    )
    if any(program.family in ("matrix_real_index", "matrix_scalar_uses", "matrix_source_loop") for program in programs):
        main = read_text(ROOT / "tests/z80/sms/calculations-1/main.ssc")
        preamble += re.search(r"^void convertNumbersToChars16\([^;]+\) \{[\s\S]*?^\}", main, re.M)[0] + "\n"
        preamble += re.search(r"^u16 calculationAdd3\([^;]+\) \{[\s\S]*?^\}", main, re.M)[0] + "\n"
    source = preamble + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
    return programs, preamble, source


def read_image(directory):
    symbols, banks, section_sizes = {}, {}, {}
    static_suffix = r"(__static__\w+_ssc_)\d+"
    for line in read_text(directory / "linked.sym").splitlines():
        match = re.search(r"^([0-9a-f]+):([0-9a-f]+)\s+(\S+)$", line, re.I)
        if match:
            symbols[match[3]] = int(match[2], 16)
            banks[match[3]] = int(match[1], 16)
        section = re.search(
            r"^[0-9a-f]{8}\s+[0-9a-f]+:[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)\s+(\S+)$", line, re.I,
        )
        if section:
            section_sizes[section[2]] = int(section[1], 16)
    rom = (directory / "linked.sms").read_bytes()
    run("wla-z80", ["-i", "-o", "counters.o", "linked.sms.combined.asm"], directory, "counter-assembler.log")
    write_text(directory / "counters.link", "[objects]\ncounters.o\n")
    run("wlalink", ["-i", "counters.link", "counters.sms"], directory, "counter-linker.log")
    if rom != (directory / "counters.sms").read_bytes():
        raise RuntimeError(f"Listing-enabled ROM mismatch in {directory}")
    transfers = {}
    usage = re.search(r"^ROM: (\d+) bytes .* free of total (\d+)\.", read_output(directory / "linker.log"), re.M)
    if not usage:
        raise RuntimeError(f"Missing used-ROM summary in {directory}")
    static_metrics = {
        "rom_image_bytes": len(rom), "rom_used_bytes": int(usage[2]) - int(usage[1]),
        "register_move_sites": 0, "frame_total_bytes": 0, "frame_max_bytes": 0,
        "functions": {}, "rejections": [],
    }
    for path in sorted(directory.iterdir(), key=lambda path: path.name):
        if re.search(r"^(?:compiler|.+\.compiler)\.log$", path.name):
            static_metrics["rejections"].extend(
                re.sub(static_suffix, r"\1<id>", line)
                for line in re.findall(r"^register_allocator: rejection_summary .+$", read_output(path), re.M)
            )
    frame_pending = False
    cause = tac_cause = "other_frame"
    arguments_in_frame = []
    transfer_open = False
    section = ""
    for line in read_text(directory / "linked.sms.combined.lst").splitlines():
        source = line[60:].strip()
        if re.search(r"^\.SECTION\b", source):
            if transfer_open:
                raise RuntimeError("Unterminated allocator transfer")
            if frame_pending:
                raise RuntimeError(f"Missing frame allocation in {section}")
            cause = tac_cause = "other_frame"
            arguments_in_frame = []
            section = re.search(r'^\.SECTION "([^"]+)"', source)[1]
            frame_pending = False
        if "allocate space for the stack frame" in source:
            frame_pending = True
        frame = re.search(r"^LD\s+HL,-(\d+)$", source) if frame_pending else None
        if frame:
            name, size = re.sub(static_suffix, r"\1<id>", section), int(frame[1])
            if section not in section_sizes or name in static_metrics["functions"]:
                raise RuntimeError(f"Invalid function-frame section {section} in {directory}")
            static_metrics["functions"][name] = {"rom_bytes": section_sizes[section], "frame_bytes": size}
            static_metrics["frame_total_bytes"] += size
            static_metrics["frame_max_bytes"] = max(static_metrics["frame_max_bytes"], size)
            frame_pending = False
        if re.search(r"^; TAC:", source):
            if transfer_open:
                raise RuntimeError("Allocator transfer crossed TAC boundary")
            cause = tac_cause = (
                "allocator_preserve" if re.search(r"^; TAC: spill ", source) else
                "abi_result" if re.search(r"\breturn\b", source) else
                "abi_frame" if re.search(r"\b[A-Za-z_]\w*\(", source) else "other_frame"
            )
        argument = re.search(r"^; TAC: variable .* size (\d+) offset (-?\d+) type argument$", source)
        if argument:
            arguments_in_frame.append([int(argument[2]), int(argument[1])])
        begin = re.search(r"^; allocator transfer begin kind=(reload|preserve) bytes=([12])$", source)
        if begin:
            if transfer_open:
                raise RuntimeError("Nested allocator transfer")
            transfer_open = True
            cause = f"allocator_{begin[1]}"
        if re.search(r"^; (?:reload .* from spill slot into|preserve retained .* in spill slot)", source):
            if not transfer_open and os.environ.get("RA_BASELINE") != "1":
                raise RuntimeError("Allocator transfer end without begin")
            transfer_open = False
            cause = tac_cause
        if not transfer_open:
            if re.search(r"^; copy argument \d+", source):
                cause = "abi_argument"
            if re.search(r"^; (?:return value ->|copy return value to)", source):
                cause = "abi_result"
            if re.search(r"^; old stack frame address ->", source):
                cause = "abi_frame"
        address = re.search(
            r"^\s*\d+\s+([0-9a-f]{4})\s+[0-9a-f]{4}\s+([0-9a-f]{4})\s+([0-9a-f]{4,8})\s+([0-9a-f]{2})\s+",
            line, re.I,
        )
        if not address or not re.search(r"^[A-Z][A-Z0-9]*(?:\s|$)", source):
            continue
        bank = int(address[1], 16)
        opcode = rom[bank * 0x4000 + int(address[3], 16)]
        if opcode != int(address[4], 16):
            raise RuntimeError(f"Listing opcode mismatch in {directory}: {line.strip()}")
        if opcode & 0xc0 == 0x40 and opcode & 7 != 6 and (opcode >> 3) & 7 != 6:
            static_metrics["register_move_sites"] += 1
        transfers[(bank << 16) | int(address[2], 16)] = {
            "cause": cause, "argumentsInFrame": arguments_in_frame, "section": section,
            "branch": tac_cause == "other_frame" and bool(
                re.search(r"^(?:JP|JR)\s+(?:[A-Z]+,\s*)?_label_", source)
            ),
        }
    if frame_pending or not static_metrics["functions"]:
        raise RuntimeError(f"Incomplete function-frame measurements in {directory}")
    if transfer_open or not transfers:
        raise RuntimeError("Incomplete execution attribution map")
    return Image(rom, symbols, banks, transfers, static_metrics)


def compile_generated(output, compiler, linker, programs, source):
    compiled = {}
    for mode, flags in MODES:
        directory = output / mode
        directory.mkdir(parents=True, exist_ok=True)
        write_text(directory / "main.ssc", source)
        start = time.perf_counter()
        run(compiler, ["-mz80", "-rs", "3", *flags, "-o", "main.asm", "main.ssc"], directory, "compiler.log")
        compile_seconds = time.perf_counter() - start
        run(linker, ["-lsms", "-v", "-o", "linked.sms", "main.asm"], directory, "linker.log")
        image = read_image(directory)
        for name in ["ra_buffer", *(program.name for program in programs)]:
            if name not in image.symbols:
                raise RuntimeError(f"Missing symbol {name} in {mode}")
        image.compileSeconds = compile_seconds
        compiled[mode] = image
    if compiled["default"].rom != compiled["no_ra"].rom:
        raise RuntimeError("Default/-no-ra ROM mismatch")
    if compiled["ra"].rom != compiled["ra_quiet"].rom:
        raise RuntimeError("Traced/quiet allocator ROM mismatch")
    if (output / "ra/main.asm").read_bytes() != (output / "ra_quiet/main.asm").read_bytes():
        raise RuntimeError("Traced/quiet allocator assembly mismatch")
    write_json(output / "static-summary.json", {mode: image.staticMetrics for mode, image in compiled.items()})
    return compiled


def check_analytical_assertions(output, programs):
    if os.environ.get("RA_BASELINE") == "1":
        return
    allocator_log = read_output(output / "ra/compiler.log")
    if (not re.search(r"^register_allocator_core: next_use .+status=found$", allocator_log, re.M)
            or not re.search(r"^register_allocator_core: liveness_solve .+status=converged$", allocator_log, re.M)):
        raise RuntimeError(
            "Allocator lifecycle trace coverage missing; rebuild the compiler with CMake -DSAMESAMEC_TRACE=ON or make TRACE=1"
        )
    if re.search(r"scalar_copy_promotion function=ra_pressure_\d+ variable=first .+scope=linear_definitions", allocator_log):
        raise RuntimeError("Mixed-width reassignment was promoted")
    if any(program.family == "scalar_reassigned_word" for program in programs):
        for name in ("reassigned_word_0", "lifetime_hole_0", "lifetime_hole_1", "source_reassigned_0", "source_reassigned_1"):
            if (any(program.name == f"ra_scalar_{name}" for program in programs)
                    and not re.search(
                        rf"scalar_copy_promotion function=ra_scalar_{name} variable=(?:saved|result) .+scope=linear_definitions",
                        allocator_log,
                    )):
                raise RuntimeError(f"Missing proven definition promotion: {name}")
        if re.search(r"scalar_copy_promotion function=ra_scalar_(?:reassigned_word_1|reassigned_byte_[01]) variable=saved ", allocator_log):
            raise RuntimeError("Ambiguous or byte-width reassignment was promoted")
        ranges = list(re.finditer(
            r"linear_scan retain_interval function=ra_scalar_reassigned_word_0 block=\d+ r(\d+) 16-bit in (HL|BC) producer_tac=(\d+)\([^\n]+?consumer_tac=(\d+)\(",
            allocator_log,
        ))
        if len(ranges) != 2 or ranges[0][1] != ranges[1][1] or int(ranges[0][4]) >= int(ranges[1][3]):
            raise RuntimeError("Reassigned definitions did not release their disjoint retained lifetimes")
    quiet_lines = [
        line for line in read_output(output / "ra_quiet/compiler.log").splitlines()
        if re.search(
            r"^register_allocator_core: |^register_allocator: (?:rejection_summary|loop_retention_coverage|target_active_slots_release|join_path_state_release|post_promotion_reconciliation) ",
            line,
        )
    ]
    if "\n".join(quiet_lines) != "\n".join(re.findall(r"^register_allocator: rejection_summary .+$", allocator_log, re.M)):
        raise RuntimeError("Quiet allocator lifecycle output or rejection summary mismatch")
    assembly = read_text(output / "ra/main.asm")
    for program in programs:
        family, variant, name = program.family, program.variant, program.name
        lines = [line for line in allocator_log.splitlines() if f"function={name} " in line]

        def has(pattern):
            return any(re.search(pattern, line) for line in lines)

        def section_assembly():
            return assembly.split(f'.SECTION "{name}" FREE')[1].split(".ENDS")[0]

        if family == "matrix_source_loop" and variant == 0 and not re.search(
            r"LD\s+L,\(IX-7\)\s+LD\s+H,\(IX-6\)\s+ADD HL,BC\s+LD\s+L,\(HL\)", section_assembly(),
        ):
            raise RuntimeError(f"Unsigned pointer read did not reuse its HL address: {name}")
        if family == "matrix_byte_store":
            body = re.sub(r";[^\n]*", "", section_assembly())
            if variant == 0 and not re.search(r"ADD HL,BC\s+LD\s+A,\(IX[+-]\d+\)\s+LD\s+\(HL\),A", body):
                raise RuntimeError(f"Byte store did not reuse its HL address and low source byte: {name}")
            if variant == 12 and not re.search(r"LD\s+\(IY\+1\),[A-Z0-9]+", body):
                raise RuntimeError(f"Word store did not preserve its IY fallback: {name}")
        if family == "matrix_bc_alu" and variant < 5 and not has(
            r"linear_scan retain .*16-bit in BC .*consumer_tac=\d+\(SUB arg2\)"
        ):
            raise RuntimeError(f"BC ALU right operand not exercised: {name}")
        if (family == "matrix_scalar_uses" and variant != 8
                and (variant in (0, 1, 2, 3, 4, 5, 6, 7, 9)) != has(r"scalar_copy_promotion .*variable=saved ")):
            raise RuntimeError(f"Scalar use promotion mismatch: {name}")
        if family == "matrix_index_affinity" and variant == 0 and (
            not has(r"profitability .*phy=HL known=yes ") or not has(r"profitability .*phy=BC known=yes ")
            or not has(r"placement_rank .*original=HL selected=BC known=yes .*mutation=no$")
        ):
            raise RuntimeError(f"Cost-ranked word-index affinity missing: {name}")
        if family == "matrix_index_copy" and 22 <= variant <= 24:
            offset, use_ix = -105 - variant, variant < 24
            address = (
                f"offset={offset} mode={'frame_displacement' if use_ix else 'frame_address'} "
                f"folded_offset={offset if use_ix else 0} address_target={'IX' if use_ix else 'IY'} target=z80"
            )
            if not any(
                "emitter=array_read_base_address operand=arg1 kind=STACK_LOCAL " in line and line.endswith(address)
                for line in lines
            ):
                raise RuntimeError(f"Pointer-slot displacement boundary not exercised: {name}")
        if family == "matrix_index_copy" and variant in (12, 13, 21) and any(
            "emitter=array_read_base_address " in line and "address_target=IX " in line for line in lines
        ):
            raise RuntimeError(f"Uncached or global pointer-slot fallback changed: {name}")
        if family == "matrix_index_copy" and variant == 12 and not re.search(
            r"LD\s+L,\(IY\+0\)\s+LD\s+H,\(IY\+1\)\s+LD\s+L,\(HL\)", section_assembly(),
        ):
            raise RuntimeError(f"Zero-index unsigned pointer read did not reuse HL: {name}")
        if family == "matrix_index_copy" and variant >= 25 and not any(
            f"arg1_kind={'GLOBAL' if variant == 29 else 'STACK_LOCAL'} arg1_phy=NONE arg1_size=2 result_kind=PHY_HL " in line
            for line in lines
        ):
            raise RuntimeError(f"Word-memory assignment to HL not exercised: {name}")
        if family == "matrix_index_copy" and variant == 26 and not any(
            int(match[1]) < -128 for line in lines
            if (match := re.search(r"emitter=assignment_arg1_address operand=arg1 kind=STACK_LOCAL offset=(-?\d+) ", line))
        ):
            raise RuntimeError(f"Wide pointer-slot assignment not exercised: {name}")
        if family == "matrix_index_copy" and variant >= 30:
            offset, use_ix = -97 - variant, variant < 32
            address = (
                f"offset={offset} mode={'frame_displacement' if use_ix else 'frame_address'} "
                f"folded_offset={offset if use_ix else 0} address_target={'IX' if use_ix else 'IY'} target=z80"
            )
            if not any(
                "emitter=assignment_arg1_address operand=arg1 kind=STACK_LOCAL " in line and line.endswith(address)
                for line in lines
            ):
                raise RuntimeError(f"Word assignment displacement boundary not exercised: {name}")
        if family == "matrix_index_copy" and variant == 25 and not re.search(
            r"LD\s+IX,0\s+ADD IX,DE\s+LD\s+L,\(IX-6\)\s+LD\s+H,\(IX-5\)", section_assembly(),
        ):
            raise RuntimeError(f"Initial word load did not initialize the existing frame cache: {name}")
        if family == "matrix_index_copy" and variant in (26, 29) and any(
            f"emitter=assignment_arg1_address operand=arg1 kind={'GLOBAL' if variant == 29 else 'STACK_LOCAL'} " in line
            and "address_target=IX " in line for line in lines
        ):
            raise RuntimeError(f"Wide or global word assignment fallback changed: {name}")
        if family == "scalar_split_byte" and (
            sum("scalar_split_transfer" in line and line.endswith("kind=store") for line in lines) != 1
            or sum("scalar_split_transfer" in line and line.endswith("kind=reload") for line in lines) != 1
            or sum("reload_reuse" in line for line in lines) != 3
        ):
            raise RuntimeError(f"Split transfer counts changed: {name}")
        if family == "scalar_cost_reject_byte":
            unprofitable = any(
                int(score[1]) <= 0 or int(score[2]) < 0
                for line in lines
                if (score := re.search(r"profitability .*known=yes saved_cycles=(-?\d+) saved_bytes=(-?\d+) .*decision=3 ", line))
            )
            if any("scalar_split_transfer" in line for line in lines) or not unprofitable:
                raise RuntimeError(f"Unprofitable split accepted: {name}")
        if family in ("scalar_region", "scalar_call_region") and (
            sum(bool(re.search(r"scalar_split_transfer.*kind=store$", line)) for line in lines) != 1
            or sum(bool(re.search(r"scalar_split_transfer.*kind=reload$", line)) for line in lines) != 2
            or sum("reload_reuse" in line for line in lines) != (5 if family == "scalar_region" else 6)
        ):
            raise RuntimeError(f"Function-region transfers changed: {name}")
        if family == "scalar_loop_pressure" and any(
            not any(
                "loop_retention_apply_preflight" in line and f"selected={register} " in line and "application=applied" in line
                for line in lines
            ) for register in ("B", "C")
        ):
            raise RuntimeError(f"Two-value loop retention missing: {name}")
        if family in ("scalar_region_escape", "scalar_narrow") and has(r"scalar_copy_promotion .*variable=saved "):
            raise RuntimeError(f"Unsafe scalar promoted: {name}")
        if family == "scalar_loop_call" and any(
            "loop_retention_apply_preflight" in line and "application=applied" in line for line in lines
        ):
            raise RuntimeError(f"Call-clobbered loop retained: {name}")
        if family == "scalar_decision_loop" and has(r"scalar_copy_promotion .*variable=saved "):
            raise RuntimeError(f"Ambiguous loop definitions promoted: {name}")
        if family == "scalar_word_counter":
            if variant == 0 and not has(r"loop_retention_apply_preflight .*selected=BC .*application=applied"):
                raise RuntimeError(f"Bounded unsigned word counter was not retained: {name}")
            if variant == 0 and not re.search(r"\bINC BC\b", section_assembly()):
                raise RuntimeError(f"Retained word counter was not incremented directly: {name}")
            if variant == 5 and has(r"scalar_copy_promotion .*variable=i "):
                raise RuntimeError(f"Escaped word counter was promoted: {name}")
            if variant == 4 and has(r"loop_retention_apply_preflight .*application=applied"):
                raise RuntimeError(f"Call-clobbered word counter was retained: {name}")
        if family == "scalar_word_prefix" and (
            has(r"loop_retention_apply_preflight .*application=applied")
            or (variant == 1 and has(r"scalar_copy_promotion .*variable=i "))
        ):
            raise RuntimeError(f"Unsupported prefix or reused counter escaped conservative fallback: {name}")
        if family == "scalar_word_step":
            body = section_assembly()
            if variant in (0, 2, 12) and not re.search(r"INC \(IX[+-]\d+\)\s+JR\s+NZ,\+\s+INC \(IX[+-]\d+\)", body):
                raise RuntimeError(f"In-place word increment was not selected: {name}")
            if variant in (5, 6, 8) and not re.search(r"\bINC H\b", body):
                raise RuntimeError(f"Page address increment was not selected: {name}")
            if variant == 7 and not re.search(r"\bDEC H\b", body):
                raise RuntimeError(f"Page address decrement was not selected: {name}")
        if family.startswith("scalar_remat_"):
            body = assembly.split(f"{name}:")[1].split(".ENDS")[0]
            if ("; rematerialize " in body) != (family != "scalar_remat_pointer"):
                raise RuntimeError(f"Constant-address rematerialization mismatch: {name}")


def calibrate_counters():
    reset = execute(bytes([0x3e, 0x1a, 0x27, 0x21, 0, 0xc1, 0x77, 0xc9]),
                    0, [], [], bytearray(1), 0xc100, 0)
    if reset["buffer"] != bytearray([0x20]):
        raise RuntimeError("Native reset-flags calibration failed")
    probe = execute(bytes([0x21, 0, 0xc1, 0x06, 42, 0x48, 0x79, 0x77, 0x7e, 0xc9]),
                    0, [], [], bytearray(1), 0xc100, 0)
    if probe["register_moves"] != 2 or probe["instructions"] != 7 or probe["cycles"] != 49 or probe["buffer"][0] != 42:
        raise RuntimeError("Executed register-move counter calibration failed")
    traffic = execute(bytes([
        0xdd, 0x21, 0xf8, 0xcf, 0x06, 42, 0x78, 0xdd, 0x70, 0, 0xdd, 0x4e, 0,
        0xdd, 0x71, 1, 0xdd, 0x7e, 1, 0xdd, 0x77, 2, 0xdd, 0x6e, 2, 0xc9,
    ]), 0, [], [], bytearray(), 0, 0, {
        address: {"cause": cause, "argumentsInFrame": []}
        for address, cause in (
            (7, "allocator_preserve"), (10, "allocator_reload"), (13, "abi_argument"),
            (16, "abi_argument"), (19, "abi_result"), (22, "abi_result"),
        )
    })
    if (traffic["cycles"] != 149 or traffic["instructions"] != 10 or traffic["register_moves"] != 1
            or traffic["frame_reads"] != 3 or traffic["frame_writes"] != 3
            or any(traffic[metric] != 1 for metric in (
                "allocator_preserve_writes", "allocator_reload_reads", "abi_argument_reads",
                "abi_argument_writes", "abi_result_reads", "abi_result_writes",
            ))):
        raise RuntimeError("Frame transfer-cause counter calibration failed")
    stack = execute(bytes([0xc5, 0xc1, 0xc9]), 0, [], [], bytearray(), 0, 0, {
        address: {"cause": "abi_frame", "argumentsInFrame": []} for address in (0, 1)
    })
    if (stack["instructions"] != 3 or stack["cycles"] != 31 or stack["register_moves"] != 0
            or stack["abi_frame_reads"] != 2 or stack["abi_frame_writes"] != 2):
        raise RuntimeError("ABI stack counter calibration failed")
    branch = execute(bytes([0x06, 2, 0x10, 0xfe, 0xc9]), 0, [], [], bytearray(), 0, 0, {
        address: {"cause": "other_frame", "argumentsInFrame": [], "section": "probe", "branch": address == 2}
        for address in (0, 2, 4)
    })
    if (branch["instructions"] != 4 or branch["cycles"] != 38
            or branch["entry_backedges"] != 1 or branch["callee_backedges"] != 0):
        raise RuntimeError("Source-loop backedge counter calibration failed")
    port_code = bytes([0x3e, 42, 0xd3, 0xbe, 0xc9])
    if execute(port_code, 0, [], [], bytearray(), 0, 0, {}, True)["port_writes"] != [[190, 42]]:
        raise RuntimeError("Software port-sink calibration failed")
    rejected_port = False
    try:
        execute(port_code, 0, [], [], bytearray(), 0, 0)
    except Exception as error:
        rejected_port = "Unexpected port write" in str(error)
    if not rejected_port:
        raise RuntimeError("Unexpected port write was not rejected")
    clock = execute(bytes([0] * 25001 + [0xc9]), 0, [], [], bytearray(), 0, 0, {}, False, 26000)
    if clock["instructions"] != 25002 or clock["cycles"] != 100014:
        raise RuntimeError("Native clock-wrap counter calibration failed")
    print("PASS:counter_calibration=register_moves,indexed_transfers,abi_stack,backedges,port_sink frame_partition=exclusive")


def word_parameters(seed, variant=0):
    return [
        (seed * 7919 + variant * 1009) & 65535,
        (seed * 5113 + 65535) & 65535,
        (seed * 3571 + 32767) & 65535,
    ]


def edge_parameters(seed, edges):
    return [edges[seed // 8], edges[seed % 8], edges[(seed + 3) % 8]]


def generated_oracle(program, seed, comparison_forms, decision_loop_paths):
    family, variant, constant = program.family, program.variant, program.constant
    byte = (seed * 73 + variant * 19) & 255
    other = (seed * 29 + 17) & 255
    third = (seed * 47 + 3) & 255
    global_pointer = (family == "matrix_index_copy" and variant in (21, 29)) or (family == "matrix_byte_store" and variant == 5)
    buffer = byte_buffer((index * 47 + seed * 11 + variant) for index in range(130 if global_pointer else 128))
    expected_buffer = buffer.copy()
    parameters = [byte, other, third]
    expected_ports = []
    match family:
        case "matrix_byte_store":
            signed = variant in (3, 14)
            index = seed % 63 - 31 if signed else seed % 32
            parameters = [32 if signed else seed & 1, 32 if signed else (seed + 1) & 1,
                          index & 65535, [0, 1, 127, 128, 255, 256, 32768, 65535][seed % 8]]
            target, alias, _, value = parameters
            destination = target + index
            stored = value & 255
            if variant == 11:
                for offset in range(index):
                    expected_buffer[target + offset] = expected_buffer[alias + offset]
                expected = expected_buffer[target]
            elif variant == 12:
                expected_buffer[target + index * 2] = stored
                expected_buffer[target + index * 2 + 1] = value >> 8
                expected = value
            else:
                if variant == 1:
                    stored = 173
                elif variant in (2, 14):
                    stored = (value ^ parameters[2]) & 255
                elif variant == 9:
                    stored = (value + index) & 255
                elif variant == 10:
                    stored = (value ^ 0x9234) & 255
                elif variant == 13:
                    stored = (index + value) & 255
                if variant == 4:
                    destination = target
                elif variant == 5 or (variant == 7 and not value & 1):
                    destination = alias + index
                expected_buffer[destination] = stored
                expected = stored
                if variant == 5:
                    expected = expected_buffer[target + index]
                elif variant in (6, 14):
                    expected = stored * 2
                elif variant == 7:
                    expected = expected_buffer[target + index] + expected_buffer[alias + index]
                elif variant == 10:
                    expected = (stored + (value ^ 0x9234)) & 65535
                elif variant == 13:
                    expected = ((index ^ value) + stored + ((index + value) & 65535)) & 65535
                elif variant == 15:
                    expected_buffer[alias + index] = (stored + 1) & 255
                    expected = expected_buffer[target + index] + expected_buffer[alias + index]
        case "matrix_constant_shift":
            edges = [0, 1, 255, 256, 32767, 32768, 65534, 65535]
            parameters = edge_parameters(seed, edges) if seed < 64 else word_parameters(seed)
            if variant >= 36:
                parameters[2] = [0, 1, 2, 7, 8, 15, 16, 17, 31, 32, 255, 256][seed % 12]
            first, second, last = parameters
            count = variant >> 1 if variant < 36 else last
            shifted = 0 if count >= 16 else (first ^ second) // 2 ** count if variant & 1 else (first ^ second) * 2 ** count
            expected = (shifted + last) & 65535
        case "matrix_variable_multiply" | "matrix_constant_multiply":
            edges = [0, 1, 255, 256, 32767, 32768, 65534, 65535]
            parameters = edge_parameters(seed, edges) if seed < 64 else word_parameters(seed)
            first, second, last = parameters
            if family == "matrix_constant_multiply":
                multiplier = last if variant == 32 else MULTIPLY_CONSTANTS[variant >> 1]
                expected = ((first ^ second) * multiplier + last) & 65535
            else:
                left, right = first, second
                if variant >= 4:
                    index = variant & 1
                    parameters[index] &= 255
                    byte_value = parameters[index]
                    widened = byte_value - 256 if variant >= 6 and byte_value >= 128 else byte_value
                    if index:
                        right = widened
                    else:
                        left = widened
                if variant == 1:
                    left ^= last
                if variant == 3:
                    right ^= last
                expected = (left * right + last) & 65535
        case "matrix_byte" | "matrix_word" | "matrix_compare_byte" | "matrix_compare_word" | "matrix_compare_signed":
            word = family.endswith("_word")
            mask = 65535 if word else 255
            if word:
                parameters = [value & mask for value in word_parameters(seed, variant)]
            if family.startswith("matrix_compare_") and seed < 65:
                edges = [0, 1, 255, 256, 32767, 32768, 65534, 65535] if word else [0, 1, 2, 127, 128, 129, 254, 255]
                saved = edges[max(0, seed - 1) // 8]
                parameters = (
                    [byte, mask ^ saved, edges[max(0, seed - 1) % 8]] if word else
                    [byte, byte ^ saved, edges[max(0, seed - 1) % 8]]
                )
                if seed == 0:
                    parameters = (
                        [0, 65535 ^ 68, 68] if word else
                        [184, 75, 97] if family == "matrix_compare_signed" else [0, 68, 68]
                    )
                if comparison_forms == 2:
                    parameters[0] = saved
            first, second, last = parameters
            saved = first if comparison_forms == 2 else second ^ mask if word else first ^ second
            if family == "matrix_compare_signed":
                saved = saved - 256 if saved >= 128 else saved
            if family.startswith("matrix_compare_"):
                last_value = (
                    (-1 if family == "matrix_compare_signed" else 32768 if word else 128)
                    if comparison_forms else
                    last - 256 if family == "matrix_compare_signed" and last >= 128 else last
                )
                left, right = (last_value, saved) if variant >= 6 else (saved, last_value)
                condition = [left == right, left != right, left < right, left > right, left <= right, left >= right][variant % 6]
                expected = first if condition else second
            else:
                left, right = (saved, last) if variant & 1 else (last, saved)
                expected = [left + right, left - right, left & right, left | right, left ^ right][variant % 5] & mask
        case "matrix_extend":
            parameters[2] = (seed * 3571 + 32767) & 65535
            saved = byte ^ other
            expected = (parameters[2] + (saved - 256 if saved >= 128 else saved)) & 65535
        case "matrix_low_byte":
            parameters = word_parameters(seed, variant % 2)
            if 0 < seed <= 64:
                edges = [0, 1, 127, 128, 255, 256, 32768, 65535]
                parameters = [edges[(seed - 1) // 8], edges[(seed - 1) % 8], edges[(seed + 2) % 8]]
            narrowed = (parameters[0] ^ 65535) & 255
            expected = (parameters[2] + (
                ((parameters[0] ^ 65535) + parameters[1]) & 255 if variant < 2 else
                parameters[1] + (narrowed - 256 if variant >= 6 and narrowed >= 128 else narrowed)
            )) & 65535
        case "matrix_index_affinity":
            parameters = [65535 - seed % len(buffer)]
            expected = buffer[seed % len(buffer)]
        case "matrix_index_copy":
            word = variant in (2, 3, 11, 20)
            signed = variant in (1, 3, 5, 18, 19)
            offset = (16 if word else 32) if signed else 0
            index = (
                7 if variant == 8 else seed % (offset * 2 - 1) - offset + 1 if signed else
                seed % (64 if word or variant >= 30 or variant in (15, 16, 21, 26, 27, 28, 29) else 128)
            )
            parameters = [index & (255 if variant == 5 else 65535), other]
            if variant >= 12:
                parameters = [offset, offset if seed & 1 else 16, index & 65535, other]
                previous = (
                    buffer[(offset + index) * 2] + buffer[(offset + index) * 2 + 1] * 256
                    if word else buffer[offset + index]
                )
                if variant == 12:
                    expected = buffer[offset]
                elif variant == 13:
                    expected = buffer[offset + 7]
                elif variant in (14, 22, 23, 24):
                    expected = previous + other
                elif variant == 26 or variant >= 30:
                    expected = (previous if other & 2 else buffer[parameters[1] + index]) + other
                elif variant in (15, 16, 27, 28):
                    expected_buffer[(parameters[1] if variant in (15, 27) else offset) + index] = other
                    expected = previous + expected_buffer[offset + index]
                elif variant == 17:
                    expected = previous + index + other
                elif variant in (21, 29):
                    expected = previous + buffer[parameters[1] + index] + (((other + index) & 255) if variant == 29 else 0)
                else:
                    expected = previous
            elif variant == 9:
                parameters = [0, 0 if seed & 1 else 16, seed % 64, other]
                expected_buffer[parameters[2]] = other
                expected = expected_buffer[parameters[1] + parameters[2]]
            else:
                if variant == 6:
                    expected_buffer[index] = other
                if variant >= 10:
                    expected_buffer[index * (2 if word else 1)] = other
                    if word:
                        expected_buffer[index * 2 + 1] = 0
                    expected = index + 1
                else:
                    expected = (
                        buffer[(offset + index) * 2] + buffer[(offset + index) * 2 + 1] * 256
                        if word else expected_buffer[offset + index]
                    )
        case "matrix_bc_alu":
            edges = [0, 1, 127, 128, 255, 256, 32768, 65535]
            parameters = edge_parameters(seed, edges) if seed < 64 else word_parameters(seed)
            first, second, last = parameters
            saved = [first + second, first - second, first & second, first | second, first ^ second][variant % 5] & 65535
            expected = (
                ((last - saved if variant < 5 else last + saved) & 65535) if variant < 10 else
                ((((first + last) & 65535) - ((second + last) & 65535)) & 65535) ^ ((first + last) & 65535) if variant == 10 else
                ((second + last + (first & 255)) & 65535) ^ (first & 255)
            )
        case "matrix_scalar_uses":
            if program.widths[0] == 2:
                parameters = word_parameters(seed)
            if variant == 8:
                parameters[0] = ((seed % 63) - 31) & 255
            if variant >= 12:
                edges = [0, 1, 127, 128, 255, 126, 129, 254]
                if seed < 64:
                    parameters = [edges[seed // 8], edges[seed % 8], parameters[2]]
                if variant < 18:
                    parameters[1] |= 1
                    parameters[0] = (
                        parameters[0] | 1 if variant % 3 == 0 else parameters[0] & 254 if variant % 3 == 2 else parameters[0]
                    )
            first, second, last = parameters
            saved = first ^ second
            called = (saved + second + last) & 255
            quotient = (last + 0x9234) // 3
            # Compute the selected alternative lazily: invalid unselected
            # buffer indices are undefined in JavaScript, not Python indices.
            if variant == 0:
                expected = first if saved < last else second
            elif variant == 1:
                expected = js_left_shift(saved, last & 15) & 65535
            elif variant == 2:
                expected = js_left_shift(second, first & 15) & 65535
            elif variant == 3:
                expected = buffer[first & 127]
            elif variant == 4:
                expected = (called - 256 if called >= 128 else called) & 65535
            elif variant == 5:
                expected = (saved + second + last) & 65535
            elif variant == 6:
                expected = int((saved - 256 if saved >= 128 else saved) < (last - 256 if last >= 128 else last))
            elif variant == 7:
                expected = js_unsigned_shift(saved, last & 7)
            elif variant == 8:
                expected = buffer[32 + (first - 256 if first >= 128 else first)]
            elif variant == 9:
                expected = called ^ saved
            elif variant == 10:
                chosen = first if second & 1 else last
                expected = ((chosen + second) & 65535) ^ chosen
            elif variant == 11:
                expected = second
            elif variant < 18:
                expected = js_floor_divide(last + 0x9234, [first & second, first | second, first ^ second][(variant - 12) % 3])
            elif variant < 28:
                difference = [
                    first - second, second - first, first - 1, 1 - first, first, -first,
                    first - 255, 255 - first, first - second, second - first,
                ][variant - 18]
                expected = quotient | (difference & 65535)
            elif variant == 28:
                expected = ((quotient | ((first - second) & 65535)) + second) & 65535
            else:
                expected = quotient | ((first - 1) & 65535)
        case "matrix_real_index" | "matrix_source_loop":
            if family == "matrix_source_loop" and variant:
                start, count = 32, seed % 64
                parameters = [start, start + count]
                expected = 0
                expected_ports = [[0xbe, value] for value in buffer[start:start + count]]
            else:
                parameters = [0, 1, 0]
                text = "$0115 = $0115 = $0115 = $0115"
                for index, character in enumerate(text):
                    expected_buffer[index * 2] = (
                        4 if character == "$" else 0x1d if character == "=" else 0 if character == " " else int(character, 16) + 0x10
                    )
                    expected_buffer[index * 2 + 1] = 0
                expected = len(text) * 2
                if family == "matrix_source_loop":
                    expected_ports = [[0xbf, 0], [0xbf, 0x7b], *[[0xbe, value] for value in expected_buffer[:expected]]]
        case "matrix_rank" | "matrix_affinity":
            edges = [0, 1, 127, 128, 255, 256, 32768, 65535]
            parameters = edge_parameters(seed, edges) if seed < 64 else word_parameters(seed)
            live = parameters[0] ^ 65535 if family == "matrix_rank" else (parameters[0] + parameters[1]) & 65535
            saved = parameters[2] ^ 65535
            expected = [live + saved, live & saved, live | saved, live ^ saved][variant % 4] & 65535
        case "matrix_sentinel":
            edges = [0, 1, 127, 128, 255, 256, 32768, 65535]
            parameters = edge_parameters(seed, edges) if seed < 64 else word_parameters(seed)
            first, second, last = parameters
            live = (first + second) & 65535
            saved = last ^ 65535
            if variant < 2:
                expected = ((live ^ saved) + live) & 65535 if variant else ((live + saved) & 65535) ^ live
            elif variant < 4:
                parameters[0] &= 255
                parameters[1] &= 255
                byte_value = parameters[0] ^ parameters[1]
                expected = ((last + last) + (byte_value - 256 if byte_value >= 128 else byte_value)) & 65535
            elif variant < 6:
                expected = ((live + (last & 255)) & 65535) ^ live
            elif variant < 8:
                expected = ((js_unsigned_shift(live, last & 15) if variant & 1 else js_left_shift(live, last & 15)) & 65535) ^ live
            else:
                parameters = [((seed % 63) - 31) & 255, other, third]
                index = parameters[0] ^ (variant & 1)
                expected_buffer[32 + (index - 256 if index >= 128 else index)] = other ^ third
                expected = (other ^ third) * 2
        case "matrix_shift":
            parameters[0] = (seed * 7919 + variant * 1009) & 65535
            parameters[1] = (seed * 5113 + 65535) & 65535
            saved = parameters[0] ^ 65535
            expected = (js_unsigned_shift(saved, third & 15) if variant & 1 else js_left_shift(saved, third & 15)) & 65535
        case "matrix_structural":
            parameters = [0, 0 if variant & 2 else 8, seed & 15, seed % 8 if variant >= 8 else 1 + seed % 4, other, byte]
            expected = byte
            for row in range(parameters[3]):
                for column in range(2 if variant & 4 else 1):
                    index = parameters[2] + row + column
                    alias = parameters[1] + index
                    previous = expected_buffer[index]
                    called = (previous + other + row) & 255
                    if (previous ^ other) & 1:
                        expected_buffer[alias] = (previous + (other ^ byte)) & 255
                    else:
                        expected_buffer[index] = previous ^ other ^ byte
                    if not variant & 1:
                        called = (expected_buffer[alias] + other + row) & 255
                    expected = (expected + expected_buffer[index] + called) & 65535
                    if variant >= 8:
                        expected = (expected + (expected & 255 if row & 1 else other & 7)) & 65535
        case "scalar_region" | "scalar_call_region":
            saved = byte ^ other
            first = (third + saved) & 255
            value = (((first + buffer[1]) & 255) ^ saved) + saved
            value = (((value & 255) ^ saved) + saved) & 255
            if family == "scalar_region":
                if variant == 1 and byte < other:
                    expected_buffer[2] = value
                value = (value + (1 if byte < other else 0)) & 255
                expected = (((value ^ saved) + saved) & 255) ^ saved
            else:
                called_byte = ((byte + 2) & 255) ^ ((byte + 1) & 255) ^ byte ^ 3 if variant == 1 else byte
                expected_buffer[0] = (called_byte + other + third) & 255
                value = (((value ^ saved) + saved) & 255) ^ saved
                expected = ((value + saved) & 255) ^ buffer[1]
        case "scalar_region_escape":
            expected = ((third if byte < other else byte ^ other) + other) & 255
        case "scalar_loop_pressure":
            iterations = max(0, math.ceil((other - byte) / 2))
            expected = (byte + iterations) ^ (other - iterations)
        case "scalar_loop_call":
            parameters[2] = seed & 7
            value = other
            for _ in range(parameters[2]):
                expected_buffer[0] = value
                value = (buffer[1] + (byte ^ other)) & 255
            expected = value
        case "scalar_word_step":
            parameters = edge_parameters(seed, [0, 1, 254, 255, 256, 32767, 32768, 65535]) if seed < 64 else word_parameters(seed)
            first, second, last = parameters
            expected = (first + 1) & 65535
            if variant == 1:
                expected ^= first
            elif variant in (3, 4):
                narrowed = first & 255
                expected = (narrowed + 1 - (256 if variant == 4 and narrowed >= 128 else 0)) & 65535
            elif variant in (5, 6, 8, 13):
                expected = (first + (255 if variant == 13 else 256)) & 65535
            elif variant in (7, 14):
                expected = (first - 256) & 65535
            elif variant == 9:
                expected = buffer[(first & 15) + 1]
            elif variant in (10, 11):
                count = second & 31
                for index in range(count):
                    expected_buffer[index + (variant == 10)] = expected_buffer[index + (variant == 11)]
                expected = expected_buffer[count if variant == 10 else 0]
            elif variant == 12:
                expected = (first + 1 + (last & 255)) & 65535
        case "scalar_word_counter" | "scalar_word_prefix":
            start = (0, 255, 256, 65534, 65535)[seed % 5]
            count = (0, 1, 255, 256)[seed % 4]
            value = (start + 2) & 65535 if variant in (2, 6) else word_parameters(seed, variant)[1]
            parameters = [start, value, (start + count) & 65535]
            iterations = min(count, 2) if variant in (2, 6) else count
            expected = ((start + iterations) + value) & 65535
            if family == "scalar_word_prefix":
                expected = (start + value + parameters[2]) & 65535
            elif variant == 0:
                expected = (parameters[2] + 1) & 65535
            if family == "scalar_word_counter" and count and variant in (3, 4):
                expected_buffer[0] = 2 if variant == 3 else value & 255
            if family == "scalar_word_counter" and variant == 7:
                crc = value
                for _ in range(count):
                    crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
                expected = (crc + parameters[2]) & 65535
        case "scalar_decision_loop":
            parameters = [(seed * 7919 + 32767) & 65535, seed % 8, third]
            first, count, value = parameters
            saved, out, left, right = first, first ^ 0x4321, value, (count + value) & 255
            for remaining in range(count, 0, -1):
                saved = out ^ remaining if remaining & 1 else (saved + left) & 65535
                left = (left + 1) & 255
                right = (right - 1) & 255
                if variant:
                    expected_buffer[0] = right
                    left = (left + expected_buffer[1]) & 255
                out = ((out + saved) ^ ((left + right) & 255)) & 65535
                expected_buffer[remaining] = left ^ right
            expected = (out + saved + left + right) & 65535
            paths = decision_loop_paths.setdefault(program.name, set())
            paths.add("zero" if count == 0 else "one" if count == 1 else "multiple")
        case "scalar_signed_call":
            called = (byte + other + third) & 255
            expected = (byte + (called - 256 if called >= 128 else called)) & 65535
        case "scalar_slots":
            parameters = [1 + (seed & 15), 1 + ((seed * 3) & 15), 17 + (seed & 15)]
            expected_buffer[0] = buffer[parameters[0]] ^ buffer[parameters[1]] ^ ((parameters[0] + parameters[1]) & 255)
            expected = buffer[parameters[2]] ^ buffer[parameters[1] + 1] ^ ((parameters[1] + parameters[2]) & 255)
        case "scalar_remat_byte" | "scalar_remat_word" | "scalar_remat_pointer":
            parameters = [seed & 15, (seed * 7) & 15]
            width = 2 if family == "scalar_remat_word" else 1
            first, second = (3 + parameters[0]) * width, (3 + parameters[1]) * width
            expected = (
                ((buffer[first] | buffer[first + 1] << 8) + (buffer[second] | buffer[second + 1] << 8)) & 65535
                if width == 2 else (buffer[first] + buffer[second]) & 255
            )
            if family == "scalar_remat_pointer":
                parameters.insert(0, 0)
        case "scalar_word_index":
            parameters = [(seed * 7919 + variant * 1009) & 65535, (seed * 5113 + 65535) & 65535, third]
            expected_buffer[constant] = third
            expected = (parameters[0] + parameters[1] + third) & 65535
        case ("scalar_word" | "scalar_signed_word" | "scalar_copy" | "scalar_complement_sub"
              | "scalar_multi_word" | "scalar_multi_rmw" | "scalar_split_word" | "scalar_rank_fallback"
              | "scalar_reassigned_word" | "scalar_lifetime_hole" | "scalar_narrow" | "word_bc"):
            parameters = word_parameters(seed, variant)
            first, second, third_word = parameters
            if family == "scalar_lifetime_hole":
                called = ((first & 255) + (second & 255) + (third_word & 255)) & 255 if variant else 0
                middle = (second + third_word + called) & 65535
                expected = ((((third_word + (first ^ second)) & 65535) ^ middle) + (((first + third_word) & 65535) ^ middle)) & 65535
            elif family == "scalar_reassigned_word":
                initial = first ^ second
                saved = second ^ third_word if not variant or first & 1 else initial
                expected = (((third_word + initial + saved) & 65535) ^ saved) & 65535
            elif family == "scalar_rank_fallback":
                saved = first ^ 65535
                expected = (saved + ((saved + second) & 65535)) & 65535
                if third_word & (1 << variant):
                    expected = (expected + saved) & 65535
            elif family == "scalar_split_word":
                saved = third_word ^ 65535
                expected = (((((first + saved + buffer[1]) & 65535) ^ saved) + second) + saved) & 65535
            elif family.startswith("scalar_multi_"):
                saved = third_word ^ 65535
                expected = (
                    ((((first + second + saved) & 65535) ^ saved) - saved) & 65535 if family == "scalar_multi_word" else
                    ((((first + saved) & 65535) ^ saved) + second) & 65535
                )
            elif family == "scalar_signed_word":
                expected = (third_word | 7) ^ first ^ second
            elif family == "scalar_complement_sub":
                expected = (first + second - (third_word ^ 65535)) & 65535
            else:
                saved = (
                    (first + second) & 255 if family == "scalar_narrow" else first if family == "scalar_copy" else
                    first ^ second if family == "scalar_word" else
                    [first + second, first - second, first & second, first | second, first ^ second][variant % 5]
                )
                expected = ((second + third_word if family == "scalar_copy" else 2 * third_word) + saved) & 65535
        case "scalar_byte":
            expected = (2 * third + (byte ^ other)) & 255
        case "scalar_reassigned_byte":
            expected = (((third + (byte ^ other) + (byte ^ third)) & 255) ^ byte ^ third) & 255
        case "scalar_source_reassigned" | "scalar_source_bitwise":
            parameters = [(seed * 7919 + 32767) & 65535, other, third]
            if seed < 4:
                parameters = [[0, 65535, 0x6543, 32768][seed], [0, 255, 0x95, 128][seed], [0, 255, 0, 1][seed]]
            first, second, last = parameters
            if family == "scalar_source_reassigned":
                expected_buffer[0] = 0x21
                expected_buffer[1] = (((((first | second) % 0x1001) << 4) - 10000) ^ 0x4321) & 255
            expected = ((((((first | last) | (last | second)) % 0x1001) << 4) - (last | 10000)) ^ (0x4321 | last)) & 65535
        case "scalar_frequency_loop":
            if seed < 4:
                parameters = [[255, 0, 252, 247][seed] ^ constant, [0, 1, 255, 255][seed], third]
            expected = max(parameters[0] ^ constant, parameters[1])
        case "scalar_multi_byte":
            saved = byte ^ other
            expected = ((((third + saved) & 255) ^ saved) + saved) & 255
        case "scalar_split_byte" | "scalar_cost_reject_byte":
            saved = byte ^ other
            expected = ((((third + saved + buffer[1]) & 255) ^ saved) + saved) & 255
            if family == "scalar_split_byte":
                expected = ((expected ^ saved) + saved) & 255
        case "scalar_switch_continue":
            expected = other
            limit = byte & 7
            if variant:
                for index in range(1, limit + 1):
                    if not index & 1:
                        expected += 2 * (third + 1) + index
            else:
                for index in range(limit):
                    if index & 3:
                        expected += third + index + 1
                for index in range(1, max(1, limit) + 1):
                    if index & 3:
                        expected += third + index + 1
        case "scalar_signed_byte":
            expected = ((third | 7) ^ byte ^ other) & 255
        case "scalar_widen":
            expected = 2 * third + ((byte + other) & 255)
        case "scalar_before_call":
            expected = (2 * ((byte + other + 2 * third + constant) & 255) + (byte ^ other)) & 255
        case "scalar_after_call":
            expected_buffer[0] = (byte + other + 2 * third + constant) & 255
            expected = (2 * third + (byte ^ other)) & 255
        case "scalar_call_argument":
            expected = ((byte ^ other) + other + 2 * third + constant) & 255
        case "scalar_across_call":
            expected = (byte + other + 3 * third + constant + (byte ^ other)) & 255
        case "word":
            parameters = word_parameters(seed, variant)
            expected = (parameters[0] + parameters[1] + 2 * parameters[2] + constant) & 65535
        case "arithmetic":
            expected = (byte + other + 2 * third + constant) & 255
        case "call":
            expected = ((byte + other + 2 * third + constant) & 255) ^ constant
        case "branch":
            expected = (((byte if byte < other else other) + third) & 255) ^ constant
        case "loop":
            parameters = [seed & 7, byte]
            expected = byte
            for count in range(parameters[0], 0, -1):
                expected = ((expected + constant) & 255) ^ count
        case "array":
            parameters = [seed & 31, (seed * 13 + variant) & 31, byte, other]
            expected = buffer[parameters[0]] ^ buffer[parameters[1]] ^ ((byte + other) & 255)
        case "pointer":
            parameters = [0, seed & 7, (seed >> 3) & 7, byte]
            index = parameters[1] + parameters[2]
            expected = (buffer[index] + byte + constant) & 255
            expected_buffer[index] = expected
        case "pressure":
            first = (byte + other) & 255
            second = (other + third) & 255
            combined = (byte + other + 2 * third + constant) & 255
            expected = ((first + combined + second) & 65535) ^ ((((byte + third) & 255) + (first ^ constant)) & 65535)
        case "walk":
            parameters = [0, seed & 31, 1 + seed % 12, byte]
            expected = byte
            for iteration in range(parameters[2]):
                index = parameters[1] + iteration * (1 + variant % 3)
                previous = expected_buffer[index]
                expected_buffer[index] = (previous + expected + constant) & 255
                expected ^= previous
        case "nested":
            parameters = [0, seed & 63, 1 + seed % 4, 1 + (seed >> 2) % 4]
            expected = constant
            for row in range(parameters[2]):
                for column in range(parameters[3]):
                    index = parameters[1] + row * 8 + column
                    previous = expected_buffer[index]
                    expected = (expected + previous + row) & 65535
                    expected_buffer[index] = previous ^ column ^ constant
        case "join_call":
            parameters = [0, seed & 63, other, byte]
            index = parameters[1]
            called = (byte + other + 2 * buffer[index] + constant) & 255
            retained = (buffer[index] + constant) & 255
            if other & (1 << (variant % 4)):
                expected_buffer[index] = (called + byte) & 255
            else:
                expected_buffer[index + 1] = called ^ byte
                retained += other
            expected = (retained + expected_buffer[index] + expected_buffer[index + 1]) & 65535
        case _:
            raise RuntimeError(f"Unknown family: {family}")
    return parameters, buffer, expected_buffer, expected, expected_ports, global_pointer


def new_counters():
    return dict.fromkeys(["cases", *EXECUTION_METRICS], 0)


def accumulate(counters, result):
    counters["cases"] += 1
    for metric in EXECUTION_METRICS:
        counters[metric] += result[metric]


def buffer_symbol(program):
    return "ra_words" if program.family == "scalar_remat_word" or (
        program.family == "matrix_index_copy" and program.variant in (2, 3, 11, 20)
    ) else "ra_buffer"


def reduce_failure(output, compiler, linker, programs, preamble, program, mode, seed,
                   parameters, args, buffer, expected_buffer, expected, expected_ports, error):
    retained = [program]
    retained_names = {program.name}
    # Iterate newly discovered callees too, retaining insertion order.
    for caller in retained:
        for candidate in programs:
            if f"{candidate.name}(" in caller.source and candidate.name not in retained_names:
                retained.append(candidate)
                retained_names.add(candidate.name)
    reduced = preamble + "\n".join(entry.source for entry in retained) + "\nvoid main(void) {}\n"
    reduction = {
        "original_functions": len(programs), "retained_functions": len(retained),
        "seed": seed, "parameters": parameters, "expected": expected, "results": {}, "reproduced": False,
    }
    write_text(output / "failure-reduced.ssc", reduced)
    for replay_mode in dict.fromkeys(["default", mode]):
        directory = output / "failure-reduced" / replay_mode
        directory.mkdir(parents=True, exist_ok=True)
        write_text(directory / "main.ssc", reduced)
        try:
            flags = dict(MODES)[replay_mode]
            run(compiler, ["-mz80", "-rs", "3", *flags, "-o", "main.asm", "main.ssc"], directory, "compiler.log")
            run(linker, ["-lsms", "-v", "-o", "linked.sms", "main.asm"], directory, "linker.log")
            replay = read_image(directory)
            address = replay.symbols[buffer_symbol(program)]
            replay_args = parameters.copy()
            for index in program.pointerParameters:
                replay_args[index] += address
            result = execute(
                replay.rom, replay.symbols[program.name], replay_args, program.widths, buffer,
                address, program.returnWidth, replay.transfers, program.family == "matrix_source_loop",
            )
            reduction["results"][replay_mode] = {
                "value": result["value"],
                "correct": (
                    (not program.returnWidth or result["value"] == expected)
                    and result["buffer"] == expected_buffer and result["port_writes"] == expected_ports
                ),
            }
        except Exception as replay_error:
            reduction["results"][replay_mode] = {"correct": False, "error": str(replay_error)}
    reduction["reproduced"] = (
        not reduction["results"][mode]["correct"] and
        (mode == "default" or reduction["results"]["default"]["correct"])
    )
    write_json(output / "failure-reduction.json", reduction)
    write_json(output / "failure.json", {
        "mode": mode, "seed": seed, "program": program, "parameters": args, "expected": expected,
        "expected_ports": expected_ports, "buffer": list(buffer), "error": str(error), "reduction": reduction,
    })


def execute_generated(output, compiler, linker, programs, preamble, compiled, cases, comparison_forms, verifier_failures):
    totals = {mode: new_counters() for mode in ("default", "all_spill", "ra")}
    program_totals = {}
    shapes = {
        program.name: re.sub(
            r"\b(?:0x[0-9a-f]+|\d+)\b", "#",
            re.sub(r"ra_arithmetic_\d+", "ra_arithmetic", program.source.replace(program.name, "generated", 1)),
            flags=re.I,
        ) for program in programs
    }
    unique_inputs = set()
    decision_loop_paths = {}
    for program in programs:
        program_totals[program.name] = {mode: new_counters() for mode in totals}
        for seed in range(cases):
            parameters, buffer, expected_buffer, expected, expected_ports, global_pointer = generated_oracle(
                program, seed, comparison_forms, decision_loop_paths,
            )
            unique_inputs.add(compact_json([
                shapes[program.name], parameters, base64.b64encode(buffer).decode("ascii"),
            ]))
            for mode in totals:
                image = compiled[mode]
                buffer_address = image.symbols[buffer_symbol(program)]
                args = parameters.copy()
                for index in program.pointerParameters:
                    args[index] += buffer_address
                if global_pointer:
                    if image.symbols["ra_index_pointer"] != buffer_address + 128:
                        raise RuntimeError("Global pointer snapshot layout mismatch")
                    expected_buffer[128] = args[1] & 255
                    expected_buffer[129] = args[1] >> 8
                try:
                    result = execute(
                        image.rom, image.symbols[program.name], args, program.widths,
                        buffer, buffer_address, program.returnWidth, image.transfers, program.family == "matrix_source_loop",
                    )
                    if result["unmapped_instructions"]:
                        raise RuntimeError(f"{program.name}: unmapped executed instructions")
                    if program.family == "matrix_source_loop":
                        backedges = seed % 64 if program.variant else expected
                        if result["entry_backedges"] != backedges:
                            raise RuntimeError(f"{program.name}: source-loop backedges={result['entry_backedges']}, expected={backedges}")
                    if program.family == "scalar_frequency_loop":
                        backedges = expected - (parameters[0] ^ program.constant)
                        if result["entry_backedges"] != backedges:
                            raise RuntimeError(f"{program.name}: frequency-loop backedges={result['entry_backedges']}, expected={backedges}")
                    if program.family == "scalar_decision_loop" and result["entry_backedges"] != parameters[1]:
                        raise RuntimeError(f"{program.name}: decision-loop backedges={result['entry_backedges']}, expected={parameters[1]}")
                    if program.family in ("scalar_word_counter", "scalar_word_prefix"):
                        count = (parameters[2] - parameters[0]) & 65535
                        if program.family == "scalar_word_prefix":
                            backedges = count * (2 if program.variant else 1)
                        else:
                            backedges = min(count, 2) if program.variant in (2, 6) else count * (3 if program.variant == 3 else 1)
                        if result["entry_backedges"] != backedges:
                            raise RuntimeError(f"{program.name}: word-counter backedges={result['entry_backedges']}, expected={backedges}")
                    if mode == "ra" and os.environ.get("RA_BASELINE") != "1" and program.family in (
                        "scalar_split_byte", "scalar_split_word", "scalar_call_region",
                    ):
                        sizes = (2, 4) if program.family == "scalar_split_word" else (1, 2) if program.family == "scalar_call_region" else (1, 1)
                        if result["allocator_preserve_writes"] != sizes[0] or result["allocator_reload_reads"] != sizes[1]:
                            raise RuntimeError(f"{program.name}: explicit transfer byte counts differ")
                    if ((program.returnWidth and result["value"] != expected)
                            or result["buffer"] != expected_buffer or result["port_writes"] != expected_ports):
                        equal = str(result["buffer"] == expected_buffer).lower()
                        raise RuntimeError(f"Expected {expected}, got {result['value']}; buffer_equal={equal}")
                    for counters in (totals[mode], program_totals[program.name][mode]):
                        accumulate(counters, result)
                except Exception as error:
                    reduce_failure(
                        output, compiler, linker, programs, preamble, program, mode, seed,
                        parameters, args, buffer, expected_buffer, expected, expected_ports, error,
                    )
                    raise RuntimeError(f"Failure {program.name} seed={seed} mode={mode}: {error}; artifacts={output}") from error
    for mode, counters in totals.items():
        print(compact_json({"mode": mode, **counters, "compile_seconds": compiled[mode].compileSeconds}))
    write_json(output / "execution-summary.json", totals)
    write_json(output / "program-execution-summary.json", program_totals)
    if cases >= 4 and os.environ.get("RA_BASELINE") != "1" and "ra_scalar_word_counter_0" in program_totals:
        counter = program_totals["ra_scalar_word_counter_0"]
        for metric in ("frame_reads", "frame_writes"):
            if counter["ra"][metric] * 10 > counter["default"][metric] * 7:
                raise RuntimeError(f"Word-counter {metric} did not decrease by at least 30%")
    if ("matrix_source_loop" in os.environ.get("RA_FAMILIES", "").split(",")
            and sum(program.family == "matrix_source_loop" for program in programs) != 2):
        raise RuntimeError("Both source-loop cases must be selected")
    for name, paths in decision_loop_paths.items():
        if cases >= 3 and len(paths) != 3:
            raise RuntimeError(f"Missing zero/one/multiple loop coverage: {name}")
    unique_shapes = len(set(shapes.values()))
    coverage = {
        "functions": len(programs), "unique_shapes": unique_shapes,
        "shape_definition": "normalized_source_not_unique_cfg",
        "literal_variant_extras": len(programs) - unique_shapes,
        "decision_loop_paths": {name: sorted(paths) for name, paths in decision_loop_paths.items()},
        "source_loops": [
            {"name": program.name, "source": "calculations-1/" + (
                "bank_1.ssc:calculateAndPrint5:while" if program.variant else "bank_2.ssc:calculateAndPrint3:for"
            )} for program in programs if program.family == "matrix_source_loop"
        ],
        "unique_shape_input_tuples": len(unique_inputs), "cases_per_mode": totals["ra"]["cases"],
        "retained_registers": sorted(set(re.findall(
            r"; retained (?:arg[12]|result) in (HL|BC|A|B|C)\b", read_text(output / "ra/main.asm"),
        ))),
        "python": sys.version.split()[0], "emulator": "1.2.0",
    }
    write_json(output / "coverage-summary.json", coverage)
    print(compact_json({"coverage": coverage}))
    print(
        f"PASS:verifier_failures={verifier_failures} immutable=yes differential_cases={totals['ra']['cases']} "
        f"families={len({program.family for program in programs})} default_no_ra=identical artifacts={output}"
    )
    return program_totals


REAL_PROJECTS = [
    ("calculations-1", [
        "calculationAdd3", "calculationMultiplyAdd", "calculationXorRoundTrip", "calculationQuotientRoundTrip",
        "convertNumbersToChars16", "unpackPackBits", "calculationPackedCrc", "calculateAndPrint5",
    ]),
    ("calculations-2", [
        "calculationProductRestore", "calculationShiftRestore", "calculationPartitionRestore", "convertNumbersToChars16",
        "calculationArrayWorkload", "calculationSortWorkload", "calculationStencilWorkload", "calculationRecordWorkload",
        "gameRamAllocate", "gameRamRelease", "calculationHeapWorkload", "calculationActorWorkload",
        "calculationTileWorkload", "calculationPathWorkload", "calculationMoveWorkload",
        "calculationHistogramWorkload", "calculationFletcherWorkload",
        "calculateAndPrintComplexBank1", "calculateAndPrintComplexBank2", "calculateAndPrintComplexBank3",
    ]),
]
DISPLAY_SEEDS = {
    "calculationArrayWorkload": 0x47c1, "calculationSortWorkload": 0x253a,
    "calculationStencilWorkload": 0x36b9, "calculationRecordWorkload": 0x5a13,
    "calculationHeapWorkload": 0x4219, "calculationActorWorkload": 0x2d57,
    "calculationTileWorkload": 0x3b6d, "calculationPathWorkload": 0x17a6,
    "calculationMoveWorkload": 0x43be, "calculationHistogramWorkload": 0x2e91,
    "calculationFletcherWorkload": 0x145c,
}
PACK_BITS_VECTORS = [
    {"data": [], "capacity": 0, "bytes": []},
    {"data": [128], "capacity": 0, "bytes": []},
    {"data": [0, 128], "capacity": 1, "bytes": [128]},
    {"data": [127, *range(128)], "capacity": 128, "bytes": list(range(128))},
    {"data": [129, 85], "capacity": 128, "bytes": [85] * 128},
    {"data": [255, 170], "capacity": 2, "bytes": [170, 170]},
    {"data": [128, 1, 0, 255, 128, 255, 7, 128], "capacity": 4, "bytes": [0, 255, 7, 7]},
    {"data": [0], "capacity": 1, "bytes": [], "invalid": True},
    {"data": [127, *([3] * 127)], "capacity": 128, "bytes": [], "invalid": True},
    {"data": [2, 1, 2, 3], "capacity": 2, "bytes": [], "invalid": True},
    {"data": [129], "capacity": 128, "bytes": [], "invalid": True},
    {"data": [255, 3], "capacity": 1, "bytes": [], "invalid": True},
    {"data": [0, 7, 1, 9], "capacity": 4, "bytes": [7], "invalid": True},
    {"data": [0, 7, 129, 9], "capacity": 128, "bytes": [7], "invalid": True},
    {"data": [0, 9, 128], "capacity": 1, "bytes": [9]},
    {"data": [0, 9, 0, 5], "capacity": 1, "bytes": [9], "invalid": True},
]


def heap_oracle(name, seed, value, count, buffer_address, buffer, expected_buffer, expected):
    # Intervals are independent of the SSC header-walking implementation.
    blocks = [{"offset": 0, "length": 64, "used": False}]

    def allocate(requested):
        if requested < 1 or requested > 63:
            return 0
        index = next((index for index, block in enumerate(blocks) if not block["used"] and block["length"] > requested), -1)
        if index < 0:
            return 0
        block = blocks[index]
        remaining = block["length"] - requested - 1
        if remaining >= 2:
            blocks.insert(index + 1, {
                "offset": block["offset"] + requested + 1, "length": remaining, "used": False,
            })
            block["length"] = requested + 1
            expected_buffer[block["offset"] + block["length"]] = remaining
        block["used"] = True
        expected_buffer[block["offset"]] = 128 | block["length"]
        return block["offset"] + 1

    def release(handle):
        index = next((index for index, block in enumerate(blocks) if block["offset"] + 1 == handle and block["used"]), -1)
        if index < 0:
            return 0
        block = blocks[index]
        block["used"] = False
        expected_buffer[block["offset"]] = block["length"]
        if index + 1 < len(blocks) and not blocks[index + 1]["used"]:
            block["length"] += blocks[index + 1]["length"]
            del blocks[index + 1]
        if index > 0 and not blocks[index - 1]["used"]:
            previous = blocks[index - 1]
            previous["length"] += block["length"]
            expected_buffer[previous["offset"]] = previous["length"]
            del blocks[index]
        else:
            expected_buffer[block["offset"]] = block["length"]
        return 1

    for index in range(64):
        expected_buffer[index] = value + index * 13
    if name.startswith("gameRam"):
        shapes = [[64], [-64], [8, -8, 16, -32], [-8, 8, -16, 32], [32, -32], [16, -16, 32]]
        offset = 0
        blocks = []
        for length in shapes[seed % len(shapes)]:
            block = {"offset": offset, "length": abs(length), "used": length < 0}
            expected_buffer[offset] = block["length"] | (128 if block["used"] else 0)
            offset += block["length"]
            blocks.append(block)
        buffer = expected_buffer.copy()
        arguments_to_test = [0, 1, 6, 7, 15, 31, 63, 64, 65535] if name == "gameRamAllocate" else [0, 1, 2, 9, 17, 33, 64, 65535]
        argument = arguments_to_test[(seed // len(shapes)) % len(arguments_to_test)]
        parameters = [buffer_address, argument]
        expected = allocate(argument) if name == "gameRamAllocate" else release(argument)
    else:
        parameters = [value, count, buffer_address]
        expected_buffer.fill(0, 64, 71)
        expected_buffer[0] = 64
        handle = allocate(63)
        expected += handle
        if handle:
            expected_buffer[handle] = value
            expected_buffer[handle + 62] = value >> 8
        expected += allocate(1) * 3
        expected += release(handle) * 5
        expected += release(handle) * 7
        expected += allocate(0) * 11
        expected += allocate(64) * 13
        expected += release(2) * 17
        first, second, third = allocate(7), allocate(7), allocate(7)
        expected += first + second + third
        expected += release(first) * 19
        expected += release(third) * 23
        handle = allocate(15)
        expected += handle * 29
        expected += release(second) * 31
        expected += release(handle) * 37
        handles, sizes = [0] * 8, [0] * 8
        for round_number in range(count):
            slot = round_number % 8
            if handles[slot]:
                expected_buffer[66] += release(handles[slot])
            sizes[slot] = 2 + (((value >> (round_number % 8)) + round_number * 3) % 16)
            handle = allocate(sizes[slot])
            handles[slot] = handle
            if not handle:
                expected_buffer[65] += 1
            else:
                expected_buffer[64] += 1
                for index in range(sizes[slot]):
                    expected_buffer[handle + index] = value + round_number * 7 + index
            for index, item in enumerate(handles):
                if item:
                    expected = (expected + expected_buffer[item] * (index + 1) + expected_buffer[item + sizes[index] - 1]) & 8191
            if round_number % 2:
                slot = (slot + 3) % 8
                if handles[slot]:
                    expected_buffer[66] += release(handles[slot])
                    handles[slot] = 0
        for item in handles:
            if item:
                expected_buffer[66] += release(item)
        expected_buffer[67] = release(0)
        handle = allocate(63)
        expected_buffer[68] = handle
        expected_buffer[69] = release(handle)
        expected_buffer[70] = expected_buffer[0]
        if (len(blocks) != 1 or blocks[0]["used"] or blocks[0]["length"] != 64
                or expected_buffer[68] != 1 or expected_buffer[69] != 1):
            raise RuntimeError("Heap reference did not coalesce and reuse the complete arena")
        for index in range(71):
            expected = (expected + expected_buffer[index] * (index + 1)) & 8191
    return parameters, buffer, expected_buffer, expected


def real_oracle(project, name, seed, cases):
    packed_display = name == "calculateAndPrint5"
    extra_values = [DISPLAY_SEEDS.get(name), 0, 65535]
    value = (
        extra_values[seed - cases] if seed - cases < len(extra_values) else None
    ) if seed >= cases else (seed * 7919 + 32767) & 65535
    other = (seed * 5113 + 17) & 65535
    third = (seed * 3571 + 3) & 65535
    parameters = None
    expected = None
    expected_ports = None
    buffer = ByteBuffer()
    expected_buffer = buffer
    buffer_address, return_width = 0, 2
    match name:
        case "unpackPackBits":
            vector = PACK_BITS_VECTORS[seed]
            buffer_address = 0xc100
            buffer = ByteBuffer([0xa5] * 512)
            buffer.set(vector["data"])
            expected_buffer = buffer.copy()
            expected_buffer.set(vector["bytes"], 160)
            parameters = [buffer_address, len(vector["data"]), buffer_address + 160, vector["capacity"]]
            expected = 65535 if vector.get("invalid") else len(vector["bytes"])
        case "calculationPackedCrc":
            buffer_address = 0xc100
            buffer = byte_buffer(index * 47 + seed * 11 for index in range(258))
            expected_buffer = buffer.copy()
            expected_buffer.set([
                *([0] * 128), *[(index * 37) & 255 for index in range(64)],
                *([255] * 16), *([170] * 16), *[(index * 13 + 7) & 255 for index in range(32)],
            ], 1)
            expected_buffer[256] ^= seed & 255
            parameters = [buffer_address + 1]
            expected = 65535
            for byte in expected_buffer[1:257]:
                for bit in range(7, -1, -1):
                    feedback = (expected >> 15) ^ ((byte >> bit) & 1)
                    expected = ((expected << 1) & 65535) ^ (0x1021 if feedback else 0)
            if seed == 0 and expected != 0xcc7d:
                raise RuntimeError("Packed-tile CRC known-answer mismatch")
        case ("calculationArrayWorkload" | "calculationSortWorkload" | "calculationStencilWorkload"
              | "calculationRecordWorkload" | "calculationHeapWorkload" | "calculationActorWorkload"
              | "calculationTileWorkload" | "calculationPathWorkload" | "calculationMoveWorkload"
              | "calculationHistogramWorkload" | "calculationFletcherWorkload" | "gameRamAllocate" | "gameRamRelease"):
            count = 16 if seed >= cases else seed % 17
            buffer_address = 0xc100
            buffer = byte_buffer(index * 47 + seed * 11 for index in range(128))
            expected_buffer = buffer.copy()
            parameters = [value, count, buffer_address]
            expected = value & 8191
            if name == "calculationArrayWorkload":
                values = [(value + index * 73) & 8191 for index in range(count)]
                for index in range(count):
                    expected_buffer[index] = (value >> 3) + index * 11
                for pass_number in range(3):
                    for index in range(count):
                        other_index = (index + pass_number + 1) % count
                        mixed = values[other_index] + expected_buffer[index]
                        values[index] = ((mixed ^ value) + index * 7 if mixed & 1 else (mixed >> 1) + values[index]) & 8191
                        expected_buffer[index] = values[index] ^ (values[index] >> 8) ^ expected_buffer[other_index]
                expected = (expected + sum(item + expected_buffer[index] for index, item in enumerate(values))) & 8191
            elif name == "calculationSortWorkload":
                values = [((value ^ (index * 131)) + count * 19) & 8191 for index in range(count)]
                histogram = [0] * 8
                for item in values:
                    histogram[(item >> 3) & 7] += 1
                values.sort()
                for index, item in enumerate(values):
                    expected_buffer[index * 2] = item
                    expected_buffer[index * 2 + 1] = item >> 8
                    expected = (expected + item * (index + 1)) & 8191
                for index, item in enumerate(histogram):
                    expected_buffer[32 + index] = item
                    expected = (expected + item * (index + 1)) & 8191
                expected_buffer[40] = 1
                expected_buffer[41] = 64
                expected = (expected + 9 + 64 * 10) & 8191
            elif name == "calculationStencilWorkload":
                grid = [(value + index * index + index * 17) & 8191 for index in range(count)]
                for pass_number in range(3):
                    following = []
                    for index, item in enumerate(grid):
                        neighbours = [
                            index - 1 if index % 4 else index,
                            index + 1 if index % 4 < 3 and index + 1 < count else index,
                            index - 4 if index >= 4 else index, index + 4 if index + 4 < count else index,
                        ]
                        following.append(((item * 2 + sum(grid[neighbour] for neighbour in neighbours)) // 6 + ((value >> pass_number) & 31)) & 8191)
                    grid = following
                for index, item in enumerate(grid):
                    expected_buffer[index * 2] = item
                    expected_buffer[index * 2 + 1] = item >> 8
                    expected = (expected + item) & 8191
            elif name == "calculationRecordWorkload":
                readings = [
                    {"value": (value + index * 97) & 8191, "weight": 1 + index % 7,
                     "category": (value + index) & 3, "enabled": (value + index * 3) & 7}
                    for index in range(count)
                ]
                for pass_number in range(2):
                    for reading in (reading for reading in readings if reading["enabled"]):
                        if reading["category"] == 0:
                            reading["value"] = (reading["value"] + reading["weight"] * (pass_number + 1)) & 8191
                        elif reading["category"] == 1:
                            reading["value"] = (reading["value"] ^ (value >> pass_number)) & 8191
                        elif reading["category"] == 2:
                            reading["value"] = (reading["value"] * 3 + reading["weight"]) // 4
                        reading["weight"] = 1 + ((reading["weight"] + reading["value"]) & 7)
                for index, reading in enumerate(readings):
                    expected_buffer[index * 2] = reading["value"]
                    expected_buffer[index * 2 + 1] = reading["value"] >> 8
                    expected_buffer[32 + index] = reading["category"] | (16 if reading["enabled"] else 0)
                    expected = (expected + reading["value"] * reading["weight"]) & 8191
            elif name == "calculationHeapWorkload" or name.startswith("gameRam"):
                parameters, buffer, expected_buffer, expected = heap_oracle(
                    name, seed, value, count, buffer_address, buffer, expected_buffer, expected,
                )
            elif name == "calculationActorWorkload":
                tiles = [(value + index * 5 + (index // 8) * 3) & 7 for index in range(64)]
                actors = [
                    {"x": (value + index * 13) % 128, "y": ((value >> 3) + index * 19) % 128,
                     "score": 0, "direction": (value >> index) & 3, "health": 3 + index % 4}
                    for index in range(count)
                ]
                for frame in range(6):
                    for index, actor in enumerate(actors):
                        if not actor["health"]:
                            continue
                        direction, speed = (actor["direction"] + frame) % 4, 1 + index % 4
                        x = (actor["x"] + [speed, 128 - speed, 0, 0][direction]) % 128
                        y = (actor["y"] + [0, 0, speed, 128 - speed][direction]) % 128
                        if tiles[(y // 16) * 8 + x // 16] & 1:
                            actor["direction"] = (direction + 1) % 4
                        else:
                            actor["x"], actor["y"] = x, y
                        position = (actor["y"] // 16) * 8 + actor["x"] // 16
                        if tiles[position] & 2:
                            actor["health"] -= 1
                        if tiles[position] & 4:
                            actor["score"] += (index + 1) * 4
                            tiles[position] &= 3
                for index, actor in enumerate(actors):
                    expected_buffer.set([actor["x"], actor["y"], actor["health"], actor["score"]], index * 4)
                    expected_buffer[64 + index] = actor["direction"]
                    expected = (expected + actor["x"] * 3 + actor["y"] * 5 + actor["score"] + actor["health"] * 17 + actor["direction"]) & 8191
                expected = (expected + sum(tile * (index + 1) for index, tile in enumerate(tiles))) & 8191
            elif name == "calculationTileWorkload":
                # Expand commands directly, independently of the stream decoder.
                decoded = []
                encoded_bytes = 0
                for index in range(count):
                    length, run_length = 1 + ((value >> (index % 8)) & 3), (value >> (index % 8)) & 1
                    encoded_bytes += 1 + (1 if run_length else length)
                    for item in range(length):
                        decoded.append((value + index * 11) & 63 if run_length else (value + index * 7 + item * 3) & 63)
                crc = value
                for index, tile in enumerate(decoded):
                    tile = (tile + (value & 7) + index // 8) & 63
                    expected_buffer[index] = tile
                    crc ^= tile << 8
                    for _ in range(8):
                        crc = ((crc << 1) ^ (0x1021 if crc & 32768 else 0)) & 65535
                expected = (crc + encoded_bytes + len(decoded) * 17) & 8191
            elif name == "calculationMoveWorkload":
                for index in range(32):
                    expected_buffer[index] = (value ^ (value >> 8)) + index * 13
                expected_buffer[3:3 + count] = expected_buffer[:count]
                expected_buffer[:count] = expected_buffer[5:5 + count]
                expected = (value + sum(expected_buffer[:32])) & 65535
            elif name == "calculationHistogramWorkload":
                sample = (value ^ (value >> 8)) & 255
                for index in range(16):
                    expected_buffer[index] = sample
                    sample = (((sample << 1) | (sample >> 7)) + 7) & 255
                expected_buffer.fill(0, 32, 48)
                for index in range(count):
                    expected_buffer[32 + (expected_buffer[index] & 15)] += 1
                expected = (value + sum(bin_count * (index + 1) for index, bin_count in enumerate(expected_buffer[32:48]))) & 65535
            elif name == "calculationFletcherWorkload":
                packet = byte_buffer((value ^ (value >> 8)) + index * 29 for index in range(count))
                expected_buffer.set(packet)
                first = sum(packet) % 255
                second = sum(byte * (count - index) for index, byte in enumerate(packet)) % 255
                expected = first | (second << 8)
            elif name == "calculationPathWorkload":
                start, goal = value & 63, (value >> 6) & 63
                walls = [((value + index * 11) & 31) < count and index != start and index != goal for index in range(64)]
                distances = [255] * 64
                frontier, distance, visited = [start], 0, 1
                distances[start] = 0
                while frontier:
                    distance += 1
                    following = []
                    for position in frontier:
                        neighbours = [
                            position - 1 if position % 8 else -1, position + 1 if position % 8 < 7 else -1,
                            position - 8 if position >= 8 else -1, position + 8 if position < 56 else -1,
                        ]
                        # Like Array.filter, choose the whole neighbour list
                        # before assigning any distances for this position.
                        for next_position in [
                            item for item in neighbours if item >= 0 and not walls[item] and distances[item] == 255
                        ]:
                            distances[next_position] = distance
                            following.append(next_position)
                            visited += 1
                    frontier = following
                expected_buffer.set([*distances, visited, distances[goal], sum(walls), start, goal])
                for index in range(69):
                    expected = (expected + expected_buffer[index] * (index + 1)) & 8191
        case "calculateAndPrint5" | "calculateAndPrintComplexBank1" | "calculateAndPrintComplexBank2" | "calculateAndPrintComplexBank3":
            buffer_address = 0xc100
            buffer = ByteBuffer(80)
            parameters = [buffer_address, 1, 0]
            return_width = 0
            bank = "1" if packed_display else name[-1]
            source = read_text(ROOT / "tests/z80/sms" / project / f"bank_{bank}.ssc")
            rows = [
                match.groups() for match in re.finditer(
                    r"printCalculationLine\(chars,\s*(0x[\da-f]+),\s*(0x[\da-f]+),\s*(0x[\da-f]+),", source, re.I,
                )
            ]
            if not rows or len(rows) != len(re.findall(r"printCalculationLine\(chars,", source)):
                raise RuntimeError(f"Incomplete display oracle for {name}")
            if packed_display:
                rows.insert(0, ("0x00", "0x7d", "0x7521"))
            expected_ports = []
            for row in rows:
                expected_ports.extend([[0xbf, int(row[0], 16)], [0xbf, int(row[1], 16)]])
                for column in range(4):
                    if column:
                        expected_ports.extend([[0xbe, byte] for byte in (0, 0, 0x1d, 0, 0, 0)])
                    expected_ports.extend([[0xbe, 4], [0xbe, 0]])
                    # The final payload bit changes the CRC by its polynomial.
                    expected_value = int(row[2], 16) ^ (
                        0x1021 if packed_display and seed and int(row[2], 16) == 0xa5c3 and column == 3 else 0
                    )
                    for digit in f"{expected_value:04x}":
                        digit_value = int(digit, 16)
                        expected_ports.extend([[0xbe, digit_value + (0x10 if digit_value < 10 else 0x17)], [0xbe, 0]])
        case "convertNumbersToChars16":
            buffer_address = 0xc100
            buffer = byte_buffer(index * 47 + seed * 11 for index in range(128))
            start = seed & 31
            buffer[120] = start
            expected_buffer = buffer.copy()
            parameters = [value, buffer_address, buffer_address + 120]
            return_width = 0
            for index, digit in enumerate(f"{value:04x}"):
                digit_value = int(digit, 16)
                expected_buffer[start + 2 * index] = digit_value + (0x10 if digit_value < 10 else 0x17)
                expected_buffer[start + 2 * index + 1] = 0
            expected_buffer[120] = start + 8
        case "calculationAdd3":
            parameters = [value, other, third]
            expected = (value + other + third) & 65535
        case "calculationMultiplyAdd":
            parameters = [value, other, third]
            expected = (value * other + third) & 65535
        case "calculationXorRoundTrip":
            parameters, expected = [value, other], value
        case "calculationQuotientRoundTrip":
            parameters, expected = [value, 1 + other % 65535], value
        case "calculationProductRestore":
            multiplier = 1 + seed % 31
            parameters = [value & 1023, multiplier, seed % multiplier]
            expected = parameters[0]
        case "calculationShiftRestore":
            shift = seed % 8
            parameters = [value >> shift, shift, other]
            expected = parameters[0]
        case "calculationPartitionRestore":
            parameters, expected = [value, 1 + other % 65535, third], value
        case _:
            raise RuntimeError(f"Unknown real routine: {name}")
    return parameters, buffer, expected_buffer, buffer_address, return_width, expected, expected_ports


def mapped_routine_image(project, name, mode, image):
    bank, address = image.banks[name], image.symbols[name]
    if bank == 0:
        return image
    if bank < 1 or bank > 3 or address < 0x8000 or address >= 0xc000:
        raise RuntimeError(f"Unsupported mapped routine {project}/{name}/{mode}")
    rom = bytearray(image.rom)
    rom[0x8000:0xc000] = image.rom[bank * 0x4000:(bank + 1) * 0x4000]
    transfers = {
        key & 65535: metadata for key, metadata in image.transfers.items() if key < 65536 or key >> 16 == bank
    }
    return Image(rom, image.symbols, image.banks, transfers, image.staticMetrics, image.compileSeconds)


def execute_real_projects(output, compiler, linker, cases, quality_totals):
    real_totals = {}
    for project, routines in REAL_PROJECTS:
        images = {}
        packed_tiles = (ROOT / "tests/z80/sms" / project / "tiles.packbits").read_bytes() if project == "calculations-1" else None
        for mode, flags in MODES:
            directory = output / project / mode
            directory.mkdir(parents=True, exist_ok=True)
            if packed_tiles:
                (directory / "tiles.packbits").write_bytes(packed_tiles)
            sources = ["interrupts", "main", "bank_1", "bank_2", "bank_3"]
            for name in sources:
                (directory / f"{name}.ssc").write_bytes((ROOT / "tests/z80/sms" / project / f"{name}.ssc").read_bytes())
                bank = ["-b", name[-1], "-s", "2"] if name.startswith("bank_") else []
                run(compiler, [*bank, "-mz80", "-rs", "3", *flags, "-o", f"{name}.asm", f"{name}.ssc"], directory, f"{name}.compiler.log")
            run(linker, ["-lsms", "-ro", "4", "-v", "-o", "linked.sms", *[f"{name}.asm" for name in sources]], directory, "linker.log")
            image = read_image(directory)
            for name in routines:
                if name not in image.symbols:
                    raise RuntimeError(f"Missing routine {project}/{name}/{mode}")
            images[mode] = image
        if images["default"].rom != images["no_ra"].rom:
            raise RuntimeError(f"{project}: default/-no-ra ROM mismatch")
        if images["ra"].rom != images["ra_quiet"].rom:
            raise RuntimeError(f"{project}: traced/quiet allocator ROM mismatch")
        write_json(output / project / "static-summary.json", {mode: image.staticMetrics for mode, image in images.items()})
        real_totals[project] = {}
        for name in routines:
            routine_totals = {mode: new_counters() for mode in ("default", "all_spill", "ra")}
            real_totals[project][name] = routine_totals
            quality_totals[f"{project}/{name}"] = routine_totals
            workload = name.endswith("Workload")
            packed_crc = name == "calculationPackedCrc"
            pack_bits = name == "unpackPackBits"
            packed_display = name == "calculateAndPrint5"
            display = packed_display or name.startswith("calculateAndPrintComplexBank")
            routine_images = {mode: mapped_routine_image(project, name, mode, image) for mode, image in images.items()}
            seed_count = len(PACK_BITS_VECTORS) if pack_bits else 2 if packed_display else 1 if display else cases + (3 if workload else 0)
            for seed in range(seed_count):
                parameters, buffer, expected_buffer, buffer_address, return_width, expected, expected_ports = real_oracle(
                    project, name, seed, cases,
                )
                display_ports = {}
                for mode in routine_totals:
                    image = routine_images[mode]
                    rom = image.rom
                    if (packed_crc and seed & 255) or (packed_display and seed):
                        address = image.symbols.get("packedTiles")
                        if (not isinstance(address, int) or image.banks.get("packedTiles") != image.banks[name]
                                or address < 0x8000 or address + len(packed_tiles) > 0xc000):
                            raise RuntimeError("Packed asset is not in the mapped routine bank")
                        rom = bytearray(rom)
                        rom[address + len(packed_tiles) - 1] ^= seed & 255 if packed_crc else 1
                    result = None
                    try:
                        result = execute(
                            rom, image.symbols[name], parameters, [2, 1, 1] if display else [2] * len(parameters),
                            buffer, buffer_address, return_width, image.transfers, display,
                            1000000 if display else 250000 if workload or packed_crc else 20000,
                            512 if workload or display else 256,
                        )
                        if ((return_width and result["value"] != expected) or (
                            result["port_writes"] != expected_ports if display else result["buffer"] != expected_buffer
                        )):
                            raise RuntimeError(f"Return/buffer/VDP mismatch: expected {expected}, got {result['value']}")
                        accumulate(routine_totals[mode], result)
                        if display:
                            display_ports[mode] = result["port_writes"]
                    except Exception as error:
                        failure = {
                            "project": project, "name": name, "mode": mode, "seed": seed, "parameters": parameters,
                            "expected_buffer": list(expected_buffer), "error": str(error),
                        }
                        if expected is not None:
                            failure["expected"] = expected
                        if expected_ports is not None:
                            failure["expected_ports"] = expected_ports
                        if result is not None:
                            failure["actual"] = result
                        write_json(output / "failure.json", failure)
                        raise RuntimeError(f"Failure {project}/{name} seed={seed} mode={mode}: {error}; artifacts={output}") from error
                if display:
                    suffix = f"-{seed}" if packed_display else ""
                    write_json(output / project / f"{name}{suffix}-display.json", {"expected": expected_ports, "actual": display_ports})
            print(compact_json({"project": project, "routine": name, **routine_totals}))
    write_json(output / "real-execution-summary.json", real_totals)
    routines = [routine for project in real_totals.values() for routine in project.values()]
    print(
        f"PASS:real_routines={len(routines)} cases_per_mode={sum(modes['ra']['cases'] for modes in routines)} "
        f"randomized_inputs={cases} default_no_ra=identical artifacts={output}"
    )


def write_quality_summary(output, quality_totals):
    rows = [{
        "routine": routine,
        "cycles_delta": modes["ra"]["cycles"] - modes["default"]["cycles"],
        "register_moves_delta": modes["ra"]["register_moves"] - modes["default"]["register_moves"],
        "frame_traffic_delta": (
            modes["ra"]["frame_reads"] + modes["ra"]["frame_writes"]
            - modes["default"]["frame_reads"] - modes["default"]["frame_writes"]
        ),
        "allocator_preserve_bytes": modes["ra"]["allocator_preserve_writes"],
        "allocator_reload_bytes": modes["ra"]["allocator_reload_reads"],
    } for routine, modes in quality_totals.items()]
    outliers = {
        metric: [
            row["routine"] for row in sorted(
                (row for row in rows if row[f"{metric}_delta"] > 0),
                key=lambda row: -row[f"{metric}_delta"],
            )
        ] for metric in ("cycles", "register_moves", "frame_traffic")
    }
    write_json(output / "quality-summary.json", {
        "baseline": "default",
        "register_moves": "executed unprefixed byte-register LD instructions; stack/exchange traffic is not a register LD",
        "frame_traffic": "exclusive byte-access categories; other_frame remains unclassified",
        "per_routine": rows, "increases": outliers,
    })
    print(compact_json({"quality_outliers": {metric: len(routines) for metric, routines in outliers.items()}}))


def env_integer(name, default, minimum, maximum, message):
    try:
        text = os.environ.get(name) or str(default)
        # Accept the numeric forms accepted by Number(), while refusing
        # fractions and non-finite values rather than silently truncating them.
        stripped = text.strip()
        number = int(stripped, 0) if stripped.lower().startswith(("0x", "0o", "0b")) else float(stripped or "0")
        if not math.isfinite(number) or int(number) != number or not minimum <= number <= maximum:
            raise ValueError(message)
        return int(number)
    except (ValueError, OverflowError) as error:
        raise RuntimeError(message) from error


def main():
    output = Path(os.environ.get("RA_EXECUTION_DIR") or ROOT / f"_ra_execution_{uuid.uuid4().hex}").resolve()
    compiler = os.environ.get("SAMESAMECC") or "samesamecc"
    linker = os.environ.get("SAMESAMECL") or "samesamecl"
    cases = env_integer("RA_CASES", 64, 1, 4096, "RA_CASES must be 1..4096")
    comparison_forms = env_integer("RA_COMPARISON_FORMS", 0, 0, 2, "RA_COMPARISON_FORMS must be 0..2")
    output.mkdir(parents=True, exist_ok=True)
    harness, verifier_failures = verify_allocator(output)
    programs, preamble, source = generate_programs(comparison_forms)
    write_text(output / "generated.ssc", source)
    compiled = compile_generated(output, compiler, linker, programs, source)
    check_analytical_assertions(output, programs)
    calibrate_counters()
    quality_totals = execute_generated(
        output, compiler, linker, programs, preamble, compiled, cases, comparison_forms, verifier_failures,
    ).copy()
    if os.environ.get("RA_REAL_PROJECTS") != "0":
        execute_real_projects(output, compiler, linker, cases, quality_totals)
    write_quality_summary(output, quality_totals)
    if os.environ.get("RA_SCALAR_CASES") != "1" and os.environ.get("RA_MATRIX_CASES") != "1":
        family_filter = os.environ.get("RA_FAMILIES", "")
        cohorts = ["scalar"] if os.environ.get("RA_MATRIX_CASES") == "0" else [
            "scalar", "matrix", "matrix_literals", "matrix_memory", "matrix_values",
        ]
        if not family_filter:
            cohorts.append("word_steps")
            if os.environ.get("RA_MATRIX_CASES") != "0":
                cohorts.append("byte_stores")
        for cohort in cohorts:
            scalar_cohort = cohort in ("scalar", "word_steps")
            os.environ.update({
                "RA_SCALAR_CASES": "1" if scalar_cohort else "0",
                "RA_MATRIX_CASES": "0" if scalar_cohort else "1", "RA_REAL_PROJECTS": "0",
                "RA_VALUE_CASES": "1" if cohort in ("matrix_values", "byte_stores") else "0",
                "RA_FAMILIES": "scalar_word_step" if cohort == "word_steps" else "matrix_byte_store" if cohort == "byte_stores" else family_filter,
                "RA_COMPARISON_FORMS": "1" if cohort == "matrix_literals" else "2" if cohort == "matrix_memory" else "0",
                "RA_EXECUTION_DIR": str(output / cohort), "RA_VERIFIER": harness,
            })
            run(sys.executable, [Path(__file__).resolve()], output, f"{cohort}-execution.log")
            sys.stdout.flush()
            sys.stdout.buffer.write((output / f"{cohort}-execution.log").read_bytes())
            sys.stdout.buffer.flush()


if __name__ == "__main__":
    main()
