"""Execute shared branch lowering using the allocator's existing build/CPU helpers."""

import os
from pathlib import Path
import re
import tempfile
import unittest

from check_register_allocator import Program, compile_generated
from ra_cpu import execute


class BranchConditionsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.output = Path(os.environ.get("SSC_CONDITIONS_DIR") or tempfile.mkdtemp(prefix="ssc-conditions-"))
        cls.output.mkdir(parents=True, exist_ok=True)
        if any(cls.output.iterdir()):
            raise RuntimeError(f"Condition test output must be empty: {cls.output}")
        print(f"Condition test artifacts: {cls.output}")
        cls.groups = {}
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        for left, right in (("u8", "u8"), ("s8", "s8"), ("u16", "u16"), ("s16", "s16"), ("s8", "u16"), ("u8", "s16")):
            programs = []
            for operator_index, operator in enumerate(("==", "!=", "<", ">", "<=", ">=")):
                for form in range(4):
                    name = f"condition_{left}_{right}_{operator_index}_{form}"
                    expression = f"a {operator} b"
                    if form == 1:
                        expression = f"!({expression})"
                    elif form == 2:
                        expression = f"!(!({expression}))"
                    body = f"return {expression};" if form == 3 else f"if ({expression}) {{ return 1; }} return 0;"
                    programs.append(Program(
                        name, "condition", form, operator_index,
                        [int(left[1:]) // 8, int(right[1:]) // 8], 1, [],
                        f"u8 {name}({left} a, {right} b) {{ {body} }}",
                    ))
            source = "u8 ra_buffer[128];\n" + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
            compiled = compile_generated(cls.output / f"{left}_{right}", compiler, linker, programs, source)
            cls.groups[left, right] = (programs, compiled)
        programs = [
            Program("condition_join", "condition_join", 0, 0, [2, 2], 2, [],
                    "u16 condition_join(u16 a, u16 b) { u16 saved = a; if (b > 0) { if (a < b) { saved = a+1; } } return saved; }"),
            Program("condition_loops", "condition_loops", 0, 0, [2, 2], 2, [],
                    "u16 condition_loops(u16 a, u16 b) { u16 i; u16 total = 0; for (i = 0; i < a; i++) { if (i == 2) { continue; } if (i == 7) { break; } total += i; } i = a; while (i > 0) { i--; if (i == 1) { continue; } total++; } i = 0; do { i++; if (i == 1) { continue; } total += i; } while (i < b); return total; }"),
            Program("condition_calls", "condition_calls", 0, 0, [2, 2], 2, [],
                    "u16 condition_calls(u16 a, u16 b) { u16 result = 0; ra_buffer[0] = 0; if (condition_tick(a) < condition_tick(b)) { result = 1; } if (!condition_tick(a)) { result += 2; } if (!(!condition_tick(b))) { result += 4; } if (condition_tick(a)) { result += 8; } return result; }"),
            Program("condition_array_index", "condition_array_index", 0, 0, [2, 2], 2, [],
                    "u16 condition_array_index(u16 a, u16 b) { if (ra_buffer[a < b] == 47) { return 1; } return 0; }"),
        ]
        source = (
            "u8 ra_buffer[128];\n"
            "u16 condition_tick(u16 value) { ra_buffer[0] = ra_buffer[0]+1; return value; }\n"
            + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
        )
        cls.other = (programs, compile_generated(cls.output / "control", compiler, linker, programs, source))

    def test_relational_boundaries_and_boolean_values(self):
        calls = 0
        for (left_type, right_type), (programs, compiled) in self.groups.items():
            for program in programs:
                for first in (0, 1, 2, 127, 128, 255, 256, 32767, 32768, 65535):
                    for second in (0, 1, 2, 127, 128, 255, 256, 32767, 32768, 65535):
                        values = []
                        for value, type_name in ((first, left_type), (second, right_type)):
                            bits = int(type_name[1:])
                            value &= (1 << bits) - 1
                            if type_name.startswith("s") and value >= 1 << (bits - 1):
                                value -= 1 << bits
                            values.append(value)
                        if (left_type, right_type) == ("s8", "u16"):
                            values[0] &= 65535
                        left, right = values
                        expected = int((left == right, left != right, left < right, left > right, left <= right, left >= right)[program.constant])
                        if program.variant == 1:
                            expected ^= 1
                        for mode in ("default", "all_spill", "ra"):
                            image = compiled[mode]
                            result = execute(
                                image.rom, image.symbols[program.name], [first, second], program.widths,
                                bytes(range(128)), image.symbols["ra_buffer"], 1, image.transfers,
                            )
                            self.assertEqual(result["value"], expected, (program.name, mode, first, second))
                            self.assertEqual(result["buffer"], bytes(range(128)))
                            self.assertEqual(result["unmapped_instructions"], 0)
                            calls += 1
        print(f"Relational/Boolean boundary executions: {calls}")

    def test_only_value_contexts_materialize_booleans(self):
        for (left, right), (programs, _) in self.groups.items():
            for mode in ("default", "all_spill", "ra"):
                assembly = (self.output / f"{left}_{right}" / mode / "main.asm").read_text()
                for program in programs:
                    body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                    materialized = re.search(r"; TAC: r\d+\.[^\n]* := [01]\.", body) is not None
                    self.assertEqual(materialized, program.variant == 3, (program.name, mode))

    def test_control_flow_calls_and_nested_value_contexts(self):
        programs, compiled = self.other
        for first in (0, 1, 2, 7, 8, 255, 256):
            for second in (0, 1, 2, 7, 8):
                for program in programs:
                    buffer = bytearray((index * 47) & 255 for index in range(128))
                    expected_buffer = buffer.copy()
                    if program.name == "condition_join":
                        expected = first + int(second > 0 and first < second)
                    elif program.name == "condition_loops":
                        expected = sum(i for i in range(min(first, 7)) if i != 2)
                        expected += first - int(first > 1)
                        expected += sum(range(2, max(1, second) + 1))
                    elif program.name == "condition_calls":
                        expected = int(first < second) + 2 * int(first == 0) + 4 * int(second != 0) + 8 * int(first != 0)
                        expected_buffer[0] = 5
                    else:
                        expected = int(first < second)
                    for mode in ("default", "all_spill", "ra"):
                        image = compiled[mode]
                        result = execute(
                            image.rom, image.symbols[program.name], [first, second], program.widths,
                            buffer, image.symbols["ra_buffer"], 2, image.transfers,
                            instruction_limit=100000,
                        )
                        self.assertEqual(result["value"], expected, (program.name, mode, first, second))
                        self.assertEqual(result["buffer"], expected_buffer)
                        self.assertEqual(result["unmapped_instructions"], 0)


if __name__ == "__main__":
    unittest.main()
