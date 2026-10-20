"""Execute shared branch lowering using the allocator's existing build/CPU helpers."""

import os
from pathlib import Path
import re
import subprocess
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
                for form in range(8 if left == right and left in ("u16", "s16") else 4):
                    name = f"condition_{left}_{right}_{operator_index}_{form}"
                    expression = f"a {operator} b"
                    if form == 1:
                        expression = f"!({expression})"
                    elif form == 2:
                        expression = f"!(!({expression}))"
                    body = f"return {expression};" if form == 3 else f"if ({expression}) {{ return 1; }} return 0;"
                    if form >= 4:
                        expression = f"other {operator} saved" if form == 5 else f"saved {operator} {'other' if form == 7 else 'b'}"
                        prefix = (
                            "u8 workspace[136]; workspace[135] = a;" if form == 6 else
                            "u16 live = a+b;" if form == 7 else ""
                        )
                        extra = "workspace[135]&254" if form == 6 else "live&254" if form == 7 else "0"
                        saved = f"{left} saved = ~a;" + (f" {right} other = ~b;" if form in (5, 7) else "")
                        body = (
                            f"{prefix} {saved} if ({expression}) {{ return ({extra})|1; }}"
                            f" return {extra};"
                        )
                    programs.append(Program(
                        name, "condition", form, operator_index,
                        [int(left[1:]) // 8, int(right[1:]) // 8], 1, [],
                        f"u8 {name}({left} a, {right} b) {{ {body} }}",
                    ))
            source = "u8 ra_buffer[128];\n" + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
            compiled = compile_generated(cls.output / f"{left}_{right}", compiler, linker, programs, source)
            cls.groups[left, right] = (programs, compiled)
        nop = '__asm("NOP");'
        programs = [
            Program("condition_join", "condition_join", 0, 0, [2, 2], 2, [],
                    "u16 condition_join(u16 a, u16 b) { u16 saved = a; if (b > 0) { if (a < b) { saved = a+1; } } return saved; }"),
            Program("condition_loops", "condition_loops", 0, 0, [2, 2], 2, [],
                    "u16 condition_loops(u16 a, u16 b) { u16 i; u16 total = 0; for (i = 0; i < a; i++) { if (i == 2) { continue; } if (i == 7) { break; } total += i; } i = a; while (i > 0) { i--; if (i == 1) { continue; } total++; } i = 0; do { i++; if (i == 1) { continue; } total += i; } while (i < b); return total; }"),
            Program("condition_calls", "condition_calls", 0, 0, [2, 2], 2, [],
                    "u16 condition_calls(u16 a, u16 b) { u16 result = 0; ra_buffer[0] = 0; if (condition_tick(a) < condition_tick(b)) { result = 1; } if (!condition_tick(a)) { result += 2; } if (!(!condition_tick(b))) { result += 4; } if (condition_tick(a)) { result += 8; } return result; }"),
            Program("condition_array_index", "condition_array_index", 0, 0, [2, 2], 2, [],
                    "u16 condition_array_index(u16 a, u16 b) { if (ra_buffer[a < b] == 47) { return 1; } return 0; }"),
            Program("condition_compound_loops", "condition_compound_loops", 0, 0, [2, 2], 2, [],
                    "u16 condition_compound_loops(u16 a, u16 b) { u16 i; u16 total = 0; ra_buffer[0] = 0; for (i = 0; i < a && condition_tick(i < b); i++) { if (i == 2) { continue; } if (i == 7) { break; } total += i; } i = 0; while (i < b || condition_tick(i < a)) { i++; if (i == 2) { continue; } if (i == 9) { break; } total += i; } i = 0; do { i++; if (i == 2) { continue; } total += i; } while (i < b && condition_tick(i < a)); return total; }"),
            Program("condition_compound_increment", "condition_compound_increment", 0, 0, [2, 2], 2, [],
                    "u16 condition_compound_increment(u16 a, u16 b) { u16 i = a; u16 result = 0; ra_buffer[0] = 0; if (i++ && ++i) { result += 1; } if (i++ || ++i) { result += 2; } if (!(i++ && condition_tick(i))) { result += 4; } if (!(!(++i || condition_tick(i)))) { result += 8; } return result+i; }"),
            *[
                Program(f"condition_forward_{padding}", "branch_range", 0, padding, [2, 2], 1, [],
                        f"u8 condition_forward_{padding}(u16 a, u16 b) {{ u16 saved = ~a; if (saved < b) {{ {nop * padding} return 1; }} return 0; }}")
                for padding in (111, 112)
            ],
            *[
                Program(f"condition_backward_{padding}", "branch_range", 1, padding, [2, 2], 1, [],
                        f"u8 condition_backward_{padding}(u16 a, u16 b) {{ u16 saved; u8 i = 0; do {{ {nop * padding} i++; if (i == 2) {{ return i; }} saved = ~a; }} while (saved < b); return i; }}")
                for padding in (44, 45)
            ],
        ]
        source = (
            "u8 ra_buffer[128];\n"
            "u16 condition_tick(u16 value) { ra_buffer[0] = ra_buffer[0]+1; return value; }\n"
            + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
        )
        cls.other = (programs, compile_generated(cls.output / "control", compiler, linker, programs, source))
        cls.logical_cases = [
            ("condition_logic_tick(a,1) && condition_logic_tick(b,2)",
             lambda a, b, tick: bool(tick(a, 1)) and bool(tick(b, 2)), False, True, False),
            ("condition_logic_tick(a,1) || condition_logic_tick(b,2)",
             lambda a, b, tick: bool(tick(a, 1)) or bool(tick(b, 2)), False, True, False),
            ("condition_logic_tick(a,1) || condition_logic_tick(b,2) && condition_logic_tick(a^b,3)",
             lambda a, b, tick: bool(tick(a, 1)) or (bool(tick(b, 2)) and bool(tick(a ^ b, 3))), False, True, False),
            ("(condition_logic_tick(a,1) && condition_logic_tick(b,2)) || condition_logic_tick(a^b,3)",
             lambda a, b, tick: (bool(tick(a, 1)) and bool(tick(b, 2))) or bool(tick(a ^ b, 3)), False, True, False),
            ("condition_logic_tick(a,1) && (condition_logic_tick(b,2) || condition_logic_tick(a^b,3))",
             lambda a, b, tick: bool(tick(a, 1)) and (bool(tick(b, 2)) or bool(tick(a ^ b, 3))), False, True, False),
            ("(condition_logic_tick(a,1) || condition_logic_tick(b,2)) && (condition_logic_tick(a,3) || condition_logic_tick(b,4))",
             lambda a, b, tick: (bool(tick(a, 1)) or bool(tick(b, 2))) and (bool(tick(a, 3)) or bool(tick(b, 4))), False, True, False),
            ("a != 0 && condition_logic_tick(b/a,1) > 1",
             lambda a, b, tick: a != 0 and tick(b // a, 1) > 1, False, True, False),
            ("a < 0 && condition_logic_tick(b,1)",
             lambda a, b, tick: a < 0 and bool(tick(b, 1)), True, True, False),
            ("a < 0 || condition_logic_tick(b,1)",
             lambda a, b, tick: a < 0 or bool(tick(b, 1)), True, True, False),
            ("(condition_logic_tick(a,1) && condition_logic_tick(b,2)) + condition_logic_tick(a|b,3)",
             lambda a, b, tick: int(bool(tick(a, 1)) and bool(tick(b, 2))) + tick(a | b, 3), False, False, True),
            ("!(condition_logic_tick(a,1) && condition_logic_tick(b,2)) + condition_logic_tick(a|b,3)",
             lambda a, b, tick: int(not (bool(tick(a, 1)) and bool(tick(b, 2)))) + tick(a | b, 3), False, False, True),
            ("(condition_logic_tick(a,1) && condition_logic_tick(b,2)) == (condition_logic_tick(a,3) || condition_logic_tick(b,4))",
             lambda a, b, tick: (bool(tick(a, 1)) and bool(tick(b, 2))) == (bool(tick(a, 3)) or bool(tick(b, 4))), False, False, True),
            ("condition_logic_tick(a,1) & condition_logic_tick(b,2)",
             lambda a, b, tick: tick(a, 1) & tick(b, 2), False, False, False),
            ("condition_logic_tick(a,1) | condition_logic_tick(b,2)",
             lambda a, b, tick: tick(a, 1) | tick(b, 2), False, False, False),
            ("!((condition_logic_tick(a,1) && condition_logic_tick(b,2)) + condition_logic_tick(a|b,3))",
             lambda a, b, tick: not ((int(bool(tick(a, 1)) and bool(tick(b, 2))) + tick(a | b, 3)) & 65535), False, True, True),
            ("!(!((condition_logic_tick(a,1) && condition_logic_tick(b,2)) + condition_logic_tick(a|b,3)))",
             lambda a, b, tick: bool((int(bool(tick(a, 1)) and bool(tick(b, 2))) + tick(a | b, 3)) & 65535), False, True, True),
            ("ra_buffer[(condition_logic_tick(a,1) && condition_logic_tick(b,2)) + 8]",
             lambda a, b, tick: 8 + int(bool(tick(a, 1)) and bool(tick(b, 2))), False, False, True),
            ("condition_logic_tick(condition_logic_tick(a,1) && condition_logic_tick(b,2),3)",
             lambda a, b, tick: tick(int(bool(tick(a, 1)) and bool(tick(b, 2))), 3), False, False, True),
            ("a < 128 && condition_logic_tick(ra_buffer[a],1)",
             lambda a, b, tick: a < 128 and bool(tick(a, 1)), False, True, False),
            ("0 && condition_logic_tick(a,1)",
             lambda a, b, tick: False, False, None, False),
            ("1 || condition_logic_tick(a,1)",
             lambda a, b, tick: True, False, None, False),
        ]
        cls.logical = []
        for start in range(0, len(cls.logical_cases), 3):
            programs = []
            for index in range(start, min(start + 3, len(cls.logical_cases))):
                expression, _, signed, _, _ = cls.logical_cases[index]
                for form in range(4):
                    name = f"condition_logical_{index}_{form}"
                    operand = f"!({expression})" if form == 1 else f"!(!({expression}))" if form == 2 else expression
                    body = f"return {operand};" if form == 3 else f"if ({operand}) {{ return 1; }} return 0;"
                    programs.append(Program(
                        name, "logical", form, index, [2, 2], 2, [],
                        f"u16 {name}({'s16' if signed else 'u16'} a, u16 b) {{ ra_buffer[0] = 0; {body} }}",
                    ))
            source = (
                "u8 ra_buffer[128];\n"
                "u16 condition_logic_tick(u16 value, u8 tag) { ra_buffer[0] = ra_buffer[0]+1; ra_buffer[ra_buffer[0]] = tag; return value; }\n"
                + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
            )
            cls.logical.append((programs, compile_generated(cls.output / f"logical_{start}", compiler, linker, programs, source)))

        cls.masked = []
        masks = (0, 1, 64, 128, 255, 256, 16384, 32768, 65280, 257, 32769, 65535)
        for kind in ("u8", "s8", "u16", "s16"):
            for start in range(0, len(masks), 3):
                programs = []
                for mask in masks[start:start + 3]:
                    expression = f"(a&{mask})"
                    bodies = [
                        f"if ({expression}) {{ return 1; }} return 0;",
                        f"if ({expression} == 0) {{ return 1; }} return 0;",
                        f"if ({expression} != 0) {{ return 1; }} return 0;",
                        f"if (!{expression}) {{ return 1; }} return 0;",
                        f"if (0 == {expression}) {{ return 1; }} return 0;",
                        f"if ({mask}&a) {{ return 1; }} return 0;",
                        f"return {expression};",
                        f"u16 saved = {expression}; if (saved) {{ ra_buffer[1] = saved; return saved; }} return 0;",
                        f"u8 saved = {expression}; if (saved) {{ return 1; }} return 0;",
                        f"if ({expression} && condition_mask_tick(b)) {{ return 1; }} return 0;",
                        f"if ({expression} || condition_mask_tick(b)) {{ return 1; }} return 0;",
                        f"u16 saved = {expression}; a = b; if (saved) {{ return a; }} return 0;",
                        f"u16 value = a+b; if (value&{mask}) {{ return 1; }} return 0;",
                        f"if ({expression} == {mask}) {{ return 1; }} return 0;",
                        "if (a&b) { return 1; } return 0;",
                        f"u8 workspace[136]; u16 value = a+b; workspace[135] = b; if (value&{mask}) {{ return workspace[135]; }} return 0;",
                        f"if (condition_mask_tick(a)&{mask}) {{ return 1; }} return 0;",
                    ]
                    for form, body in enumerate(bodies):
                        if form == 14 and mask:
                            continue
                        name = f"condition_mask_{kind}_{mask}_{form}"
                        programs.append(Program(
                            name, "masked", form, mask, [int(kind[1:]) // 8, 2], 2, [],
                            f"u16 {name}({kind} a, u16 b) {{ ra_buffer[0] = 0; {body} }}",
                        ))
                source = (
                    "u8 ra_buffer[128];\n"
                    "u16 condition_mask_tick(u16 value) { ra_buffer[0] = ra_buffer[0]+1; return value; }\n"
                    + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
                )
                cls.masked.append((kind, start, programs, compile_generated(
                    cls.output / f"masked_{kind}_{start}", compiler, linker, programs, source,
                )))

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
                        if program.variant >= 4:
                            left = (first ^ 65535) & 65535
                            if left_type == "s16" and left >= 32768:
                                left -= 65536
                            if program.variant in (5, 7):
                                right = ~right if right_type == "s16" else right ^ 65535
                            if program.variant == 5:
                                left, right = right, left
                        expected = int((left == right, left != right, left < right, left > right, left <= right, left >= right)[program.constant])
                        if program.variant == 1:
                            expected ^= 1
                        elif program.variant == 6:
                            expected |= first & 254
                        elif program.variant == 7:
                            expected |= (first + second) & 254
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
                    borrow = "unsigned word comparison using byte borrow; carry only" in body
                    if left == right == "u16" and program.variant in (4, 5):
                        required = program.constant >= 2 and (program.variant == 4 or program.constant in (2, 5))
                        self.assertEqual(borrow, mode == "ra" and required, (program.name, mode))
                    if left != "u16" or right != "u16" or program.constant < 2:
                        self.assertFalse(borrow, (program.name, mode))
                    if borrow:
                        comparison = body.split("; unsigned word comparison using byte borrow; carry only")[1]
                        comparison = comparison.split("; -----------------------------------------------------------------")[0]
                        self.assertEqual(len(re.findall(r"\bJP\s+(?:N?C),", comparison)), 1, (program.name, mode))
                        self.assertNotRegex(comparison, r"\b(?:JP\s+(?:N?Z),|SBC HL,)")
                        if program.variant == 5:
                            self.assertRegex(comparison, r"LD\s+A,[LC]\s+SUB A,[LC]\s+LD\s+A,[HB]\s+SBC A,[HB]")
                    self.assertNotRegex(body, r"\bJR\s+(?:(?:N?C|N?Z),)?_label")
        _, compiled = self.other
        displacements = {}
        section = ""
        for line in (self.output / "control" / "ra" / "linked.sms.combined.lst").read_text().splitlines():
            text = line[60:].strip()
            if match := re.match(r'\.SECTION "([^"]+)"', text):
                section = match[1]
            if not section.startswith(("condition_forward_", "condition_backward_")):
                continue
            self.assertNotRegex(text, r"^JR\s+(?:(?:N?C|N?Z),)?_label")
            if match := re.fullmatch(r"JP\s+(?:(?:N?C|N?Z),)?(_label_\d+)", text):
                pc = int(line[:60].split()[3], 16)
                target = compiled["ra"].symbols[match[1]]
                # A forward target moves back one byte if this JP becomes JR.
                displacement = target - pc - (3 if target > pc else 2)
                if section.startswith("condition_forward_") or target < pc:
                    displacements[section] = displacement
        self.assertEqual(displacements, {
            "condition_forward_111": 127, "condition_forward_112": 128,
            "condition_backward_44": -128, "condition_backward_45": -129,
        })
        for index, literal in enumerate(("65536", "(0-32769)")):
            for reverse in (False, True):
                directory = self.output / f"literal_out_of_range_{index}_{int(reverse)}"
                directory.mkdir()
                expression = f"{literal} < saved" if reverse else f"saved < {literal}"
                (directory / "main.ssc").write_text(
                    f"u8 check(u16 a) {{ u16 saved = ~a; if ({expression}) {{ return 1; }} return 0; }}\n"
                    "void main(void) {}\n"
                )
                compiled = subprocess.run(
                    [os.environ.get("SAMESAMECC") or "samesamecc", "-mz80", "-rs", "3", "-ra",
                     "-o", "main.asm", "main.ssc"], cwd=directory,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                )
                (directory / "compiler.log").write_text(compiled.stdout)
                self.assertEqual(compiled.returncode, 0, compiled.stdout)
                linked = subprocess.run(
                    [os.environ.get("SAMESAMECL") or "samesamecl", "-lsms", "-v",
                     "-o", "linked.sms", "main.asm"], cwd=directory,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                )
                (directory / "linker.log").write_text(linked.stdout)
                self.assertNotEqual(linked.returncode, 0, (literal, reverse))
                self.assertIn("Out of 16-bit range.", linked.stdout)

    def test_control_flow_calls_and_nested_value_contexts(self):
        programs, compiled = self.other
        for program in programs:
            first_values = (0, 1, 2, 7, 8, 255, 256)
            if program.name == "condition_compound_increment" or program.family == "branch_range":
                first_values += (32767, 32768, 65534, 65535)
            for first in first_values:
                for second in ((0, 1, 2, 7, 8, 255, 256, 32767, 32768, 65535)
                               if program.family == "branch_range" else (0, 1, 2, 7, 8)):
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
                    elif program.name == "condition_compound_loops":
                        i = total = ticks = 0
                        while i < first:
                            ticks += 1
                            if i >= second or i == 7:
                                break
                            if i != 2:
                                total += i
                            i += 1
                        i = 0
                        while True:
                            if i >= second:
                                ticks += 1
                                if i >= first:
                                    break
                            i += 1
                            if i == 9:
                                break
                            if i != 2:
                                total += i
                        i = 0
                        while True:
                            i += 1
                            if i != 2:
                                total += i
                            if i >= second:
                                break
                            ticks += 1
                            if i >= first:
                                break
                        expected = total
                        expected_buffer[0] = ticks & 255
                    elif program.name == "condition_compound_increment":
                        i = first
                        old = i
                        i = (i + 1) & 65535
                        if old:
                            i = (i + 1) & 65535
                        expected = int(bool(old) and bool(i))
                        old = i
                        i = (i + 1) & 65535
                        if not old:
                            i = (i + 1) & 65535
                        expected += 2 * int(bool(old) or bool(i))
                        old = i
                        i = (i + 1) & 65535
                        ticks = int(bool(old))
                        expected += 4 * int(not (bool(old) and bool(i)))
                        i = (i + 1) & 65535
                        ticks += int(i == 0)
                        expected += 8 * int(i != 0)
                        expected = (expected + i) & 65535
                        expected_buffer[0] = ticks
                    elif program.family == "branch_range":
                        expected = int((first ^ 65535) < second) + int(program.variant == 1)
                    else:
                        expected = int(first < second)
                    for mode in ("default", "all_spill", "ra"):
                        image = compiled[mode]
                        result = execute(
                            image.rom, image.symbols[program.name], [first, second], program.widths,
                            buffer, image.symbols["ra_buffer"], program.returnWidth, image.transfers,
                            instruction_limit=100000,
                        )
                        self.assertEqual(result["value"], expected, (program.name, mode, first, second))
                        self.assertEqual(result["buffer"], expected_buffer)
                        self.assertEqual(result["unmapped_instructions"], 0)

    def test_compound_short_circuit_values_and_effects(self):
        calls = 0
        for programs, compiled in self.logical:
            for program in programs:
                _, oracle, signed, _, _ = self.logical_cases[program.constant]
                for first in (0, 1, 2, 127, 128, 255, 256, 32767, 32768, 65535):
                    for second in (0, 1, 2, 255, 256, 32768, 65535):
                        expected_buffer = bytearray(range(128))
                        expected_buffer[0] = 0

                        def tick(value, tag):
                            expected_buffer[0] += 1
                            expected_buffer[expected_buffer[0]] = tag
                            return value & 65535

                        left = first - 65536 if signed and first >= 32768 else first
                        expected = int(oracle(left, second, tick)) & 65535
                        if program.variant != 3:
                            expected = int(bool(expected)) ^ int(program.variant == 1)
                        for mode in ("default", "all_spill", "ra"):
                            image = compiled[mode]
                            result = execute(
                                image.rom, image.symbols[program.name], [first, second], program.widths,
                                bytes(range(128)), image.symbols["ra_buffer"], 2, image.transfers,
                            )
                            self.assertEqual(result["value"], expected, (program.name, mode, first, second))
                            self.assertEqual(result["buffer"], expected_buffer, (program.name, mode, first, second))
                            self.assertEqual(result["unmapped_instructions"], 0)
                            calls += 1
        print(f"Compound logical/value/side-effect executions: {calls}")

    def test_compound_branches_only_materialize_required_values(self):
        for programs, _ in self.logical:
            for mode in ("default", "all_spill", "ra"):
                assembly = (self.output / f"logical_{programs[0].constant}" / mode / "main.asm").read_text()
                for program in programs:
                    _, _, _, logical_root, nested_value = self.logical_cases[program.constant]
                    if logical_root is None:
                        continue
                    body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                    assignments = re.findall(r"; TAC: (r\d+)\.[^\n]* := ([01])\.", body)
                    zero = {register for register, value in assignments if value == "0"}
                    one = {register for register, value in assignments if value == "1"}
                    self.assertEqual(bool(zero & one), nested_value or (logical_root and program.variant == 3), (program.name, mode))

    def test_masked_values_branches_widths_and_effects(self):
        calls = 0
        for kind, _, programs, compiled in self.masked:
            bits = int(kind[1:])
            for program in programs:
                mask, form = program.constant, program.variant
                for first in (0, 1, 2, 63, 64, 127, 128, 129, 255, 256, 257, 32767, 32768, 65535):
                    value = first & ((1 << bits) - 1)
                    if kind.startswith("s") and value >= 1 << (bits - 1):
                        value -= 1 << bits
                    masked = value & mask
                    for second in (0, 32769):
                        expected_buffer = bytearray(range(128))
                        expected_buffer[0] = 0
                        expected = int(bool(masked))
                        if form in (1, 3, 4):
                            expected ^= 1
                        elif form in (6, 7):
                            expected = masked
                            if form == 7 and masked:
                                expected_buffer[1] = masked & 255
                        elif form == 8:
                            expected = int(bool(masked & 255))
                        elif form in (9, 10):
                            expected_buffer[0] = int(bool(masked) if form == 9 else not masked)
                            expected = int(bool(masked) and bool(second) if form == 9 else bool(masked) or bool(second))
                        elif form == 11:
                            changed = second & ((1 << bits) - 1)
                            if kind.startswith("s") and changed >= 1 << (bits - 1):
                                changed -= 1 << bits
                            expected = changed if masked else 0
                        elif form in (12, 15):
                            expected = int(bool(((value + second) & 65535) & mask))
                            if form == 15:
                                expected *= second & 255
                        elif form == 13:
                            expected = int(masked == mask)
                        elif form == 14:
                            expected = int(bool(value & second))
                        elif form == 16:
                            expected_buffer[0] = 1
                        for mode in ("default", "all_spill", "ra"):
                            image = compiled[mode]
                            result = execute(
                                image.rom, image.symbols[program.name], [first, second], program.widths,
                                bytes(range(128)), image.symbols["ra_buffer"], 2, image.transfers,
                            )
                            self.assertEqual(result["value"], expected & 65535, (program.name, mode, first, second))
                            self.assertEqual(result["buffer"], expected_buffer, (program.name, mode, first, second))
                            self.assertEqual(result["unmapped_instructions"], 0)
                            calls += 1
        print(f"Masked value/branch/width/side-effect executions: {calls}")

    def test_masked_branch_instruction_selection(self):
        for kind, start, programs, _ in self.masked:
            for mode in ("default", "all_spill", "ra"):
                assembly = (self.output / f"masked_{kind}_{start}" / mode / "main.asm").read_text()
                for program in programs:
                    mask, form = program.constant, program.variant
                    body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                    folded = "folded single-use mask/zero branch" in body
                    one_byte = bool(mask) and (mask < 256 or not (mask & 255))
                    required = one_byte and (
                        (form < 6 and (int(kind[1:]) == 16 or mask < 256))
                        or (mode == "ra" and form in (12, 15))
                    )
                    if required:
                        self.assertTrue(folded, (program.name, mode))
                        selection = body.split("; folded single-use mask/zero branch")[1].split("JP ")[0]
                        if not (mask & (mask - 1)):
                            self.assertRegex(selection, rf"\bBIT {(mask.bit_length() - 1) & 7},")
                        else:
                            self.assertRegex(selection, rf"\bAND A,{mask if mask < 256 else mask >> 8}\b")
                    if not one_byte or form in (6, 7, 13, 14) or (form == 8 and mask >= 256):
                        self.assertFalse(folded, (program.name, mode))


if __name__ == "__main__":
    unittest.main()
