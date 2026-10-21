"""Execute shared branch lowering using the allocator's existing build/CPU helpers."""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from check_register_allocator import (
    Program, buffer_symbol, compile_generated, generate_programs, generated_oracle, mapped_routine_image, read_image,
)
from ra_cpu import execute


class BranchConditionsTests(unittest.TestCase):
    def test_forward_join_frame_addresses(self):
        bodies = {
            "diamond": "if(b&1) {{ workspace[{last}]=a+3; }} else {{ workspace[{last}]=a+7; }}",
            "nested": "if(b&1) {{ if(a&128) {{ workspace[{last}]=a+3; }} else {{ workspace[{last}]=a+7; }} }} "
                      "else {{ workspace[{last}]=a+11; }}",
            "multiple": "if(b&1) {{ goto done; }} workspace[{last}]=a+3; if(b&2) {{ goto done; }} "
                        "workspace[{last}]=a+7; done:",
            "loop": "u16 i; for(i=0;i<(b&3);i++) {{ if(i&1) {{ workspace[{last}]+=3; }} "
                    "else {{ workspace[{last}]+=7; }} }}",
            "call": "if(b&1) {{ workspace[{last}]=frame_sink(a); }} else {{ workspace[{last}]=a+7; }}",
            "asm": 'if(b&1) {{ __asm("LD IX,4660"); workspace[{last}]=a+3; }} '
                   "else {{ workspace[{last}]=a+7; }}",
            "global": "if(b&1) {{ ra_buffer[1]=a; workspace[{last}]=ra_buffer[1]+3; }} "
                      "else {{ workspace[{last}]=a+7; }}",
            "alias": "frame_alias(workspace,{last},a,b);",
            "return": "if(b&1) {{ return workspace[{last}]+b; }} workspace[{last}]+=7;",
            "continue": "u16 i; for(i=0;i<(b&3);i++) {{ if(i==1) {{ continue; }} "
                        "if(a&128) {{ break; }} workspace[{last}]+=7; }}",
        }
        programs = [
            Program(f"frame_{size}_{family}", family, size, 0, [2, 2], 2, [],
                    f"u16 frame_{size}_{family}(u16 a, u16 b) {{ u8 workspace[{size}]; "
                    f"workspace[{size-1}]=a; " + body.format(last=size-1).replace("done", f"done_{size}")
                    + f" return workspace[{size-1}]+b; }}")
            for size in (8, 136, 260) for family, body in bodies.items()
        ]
        source = ("u8 ra_buffer[128];\nu16 frame_sink(u16 a) { ra_buffer[127]+=1; return a+3; }\n"
                  "void frame_alias(u8 *p, u16 index, u16 a, u16 b) { "
                  "if(b&1) { p[index]=a+3; } else { p[index]=a+7; } }\n"
                  + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        directory = self.output / "forward-frames"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        calls = 0
        for program in programs:
            for a in (0, 1, 127, 128, 255, 256, 32767, 32768, 65535):
                for b in (0, 1, 2, 3, 4, 127, 128, 65535):
                    expected_buffer = bytearray(range(128))
                    value = a + (3 if b & 1 else 7)
                    if program.family == "nested":
                        value = a + (3 if a & 128 else 7) if b & 1 else a + 11
                    elif program.family == "multiple":
                        value = a if b & 1 else a + (3 if b & 2 else 7)
                    elif program.family == "loop":
                        value = a + sum(3 if i & 1 else 7 for i in range(b & 3))
                    elif program.family == "continue":
                        value = a + (0 if a & 128 else 7 * sum(i != 1 for i in range(b & 3)))
                    elif program.family == "return":
                        value = a if b & 1 else a + 7
                    if program.family == "call" and b & 1:
                        expected_buffer[127] = 128
                    elif program.family == "global" and b & 1:
                        expected_buffer[1] = a & 255
                    for mode, image in images.items():
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                         bytes(range(128)), image.symbols["ra_buffer"], 2, image.transfers,
                                         frame_bytes=512)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (((value & 255) + b) & 65535, expected_buffer, []),
                                         (program.name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        print(f"Forward-join frame-address oracle executions: {calls}", flush=True)
        for mode in images:
            assembly = (directory / mode / "main.asm").read_text()
            for program in programs:
                body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                if program.family == "diamond":
                    if mode in ("ra", "ra_quiet"):
                        self.assertEqual(body.count("LD  IX,"), 1, (program.name, mode))
                    else:
                        self.assertGreaterEqual(body.count("LD  IX,"), 3, (program.name, mode))
                elif mode in ("ra", "ra_quiet") and program.family in ("call", "asm"):
                    self.assertEqual(body.count("LD  IX,"), 5 if program.family == "call" else 4,
                                     (program.name, mode))

    def test_scalar_load_promotion(self):
        bodies = {
            "linear": "return (saved*257)^value;",
            "carry": "index++; u16 low=saved&127; u16 high=saved&128; return low+high+index+value;",
            "replace": "index=value+1; return (saved*257)^index;",
            "global": "scalar_counter=value; scalar_counter++; return (saved*257)^scalar_counter;",
            "call": "return scalar_load_sink(saved, value);",
            "alias": "data[index]=value; return (saved*257)^value;",
            "sign": "u16 negative=0; if(saved<0) negative=1; return negative+saved*257;",
            "loop": "u16 i; u16 total=0; for(i=0;i<3;i++) total+=saved; return total^value;",
            "escape": "scalar_load_escape_{kind}(&saved, value); return (saved*257)^value;",
            "redefine": "if(value&1) saved=saved+1; return (saved*257)^value;",
            "narrow": "return (saved*257)^value;",
        }
        programs = [
            Program(f"load_{element}_{kind}_{family}", family, 0, 0, [2, 2, 2], 2, [],
                    f"u16 load_{element}_{kind}_{family}({element} *data, s16 index, u16 value) {{ "
                    f"{'u8' if family == 'narrow' else kind} saved=data[index]; "
                    + body.format(kind=kind) + " }")
            for element in ("u8", "s8", "u16", "s16") for kind in ("u16", "s16")
            for family, body in bodies.items()
        ]
        source = ("u8 ra_buffer[128];\nu16 scalar_counter;\nu16 scalar_load_sink(u16 a, u16 b) { "
                  "return (a>>8)+(a&255)+b; }\n"
                  "void scalar_load_escape_u16(u16 *p, u16 value) { p[0]=value; }\n"
                  "void scalar_load_escape_s16(s16 *p, u16 value) { p[0]=value; }\n"
                  + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        directory = self.output / "scalar-loads"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        for image in images.values():
            self.assertEqual(image.symbols["scalar_counter"], image.symbols["ra_buffer"] + 128)
        calls = 0
        for program in programs:
            _, element, kind, _ = program.name.split("_")
            width = 1 if element.endswith("8") else 2
            mask = (1 << (8 * width)) - 1
            for index in (-3, 0, 1, 7, 23):
                address = 64 + width * index
                for loaded in (0, 1, 127, 128, 255, 256, 32767, 32768, 65535):
                    value = (loaded * 137 + 0x8001) & 65535
                    initial = bytearray((offset * 37 + value) & 255 for offset in range(130))
                    initial[address:address + width] = (loaded & mask).to_bytes(width, "little")
                    raw = loaded & mask
                    if element.startswith("s") and raw & (1 << (8 * width - 1)):
                        raw = (raw - (1 << (8 * width))) & 65535
                    expected_buffer = initial.copy()
                    if program.family == "alias":
                        expected_buffer[address:address + width] = (value & mask).to_bytes(width, "little")
                    if program.family == "escape":
                        raw = value
                    elif program.family == "redefine":
                        raw = (raw + (value & 1)) & 65535
                    elif program.family == "narrow":
                        raw &= 255
                    expected = (raw * 257) ^ value
                    if program.family == "carry":
                        expected = (raw & 255) + index + 1 + value
                    elif program.family == "call":
                        expected = (raw >> 8) + (raw & 255) + value
                    elif program.family == "sign":
                        expected = raw * 257 + (kind == "s16" and bool(raw & 32768))
                    elif program.family == "loop":
                        expected = (raw * 3) ^ value
                    elif program.family in ("replace", "global"):
                        expected = (raw * 257) ^ ((value + 1) & 65535)
                        if program.family == "global":
                            expected_buffer[128:130] = ((value + 1) & 65535).to_bytes(2, "little")
                    for mode, image in images.items():
                        result = execute(image.rom, image.symbols[program.name],
                                         [image.symbols["ra_buffer"] + 64, index & 65535, value], [2, 2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected & 65535, expected_buffer, []), (program.name, index, loaded, mode))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        print(f"Scalar-load promotion oracle executions: {calls}", flush=True)
        for mode in images:
            assembly = (directory / mode / "main.asm").read_text()
            for program in programs:
                body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                promoted = mode in ("ra", "ra_quiet") and program.family not in ("escape", "redefine", "narrow")
                self.assertEqual('; TAC: variable "_saved_' not in body, promoted, (program.name, mode))
                if promoted and program.name == "load_u8_u16_carry":
                    load = re.search(r"; TAC: r(\d+)\.u16 \(u16\) := [^\n]*\[", body)
                    if load is None:
                        self.fail(f"Missing scalar-load TAC ({mode})")
                    self.assertIn(f"; register {load[1]} size 2 spill no phy BC", body, mode)

    def test_spilled_pointer_copy_folding(self):
        programs = [
            Program(f"pointer_{kind}_{operation}", operation, 0, 0, [2, 2, 2], 2, [],
                    f"u16 pointer_{kind}_{operation}({kind} *p, s16 index, u16 value) {{ "
                    + ("p[index]=value+index; return p[index];" if operation == "store" else
                       "return value*7+p[index];") + " }")
            for kind in ("u8", "s8", "u16", "s16") for operation in ("store", "read")
        ]
        for name, body in (
            ("pointer_alias", "u8 *q=p; p[index]=value+index; q[index+1]=p[index]+1; "
             "u16 first=q[index]; return first+p[index+1];"),
            ("pointer_call", "u16 result=pointer_side(value); p[index]=result+index; return p[index];"),
            ("pointer_post", "s16 before=index++; p[before]=value; return index+p[before];"),
        ):
            programs.append(Program(name, name, 0, 0, [2, 2, 2], 2, [],
                                    f"u16 {name}(u8 *p, s16 index, u16 value) {{ {body} }}"))
        for name, body in (
            ("pointer_local", "u8 *q=p; q[index]=value+index; return q[index];"),
            ("pointer_global", "ra_pointer=p; ra_pointer[index]=value+index; return ra_pointer[index];"),
        ):
            programs.append(Program(name, "store", 0, 0, [2, 2, 2], 2, [],
                                    f"u16 {name}(u8 *p, s16 index, u16 value) {{ {body} }}"))
        source = ("u8 ra_buffer[128];\nu8 *ra_pointer;\nu16 pointer_side(u16 value) { "
                  "ra_buffer[127]=ra_buffer[127]+1; return value; }\n"
                  + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        directory = self.output / "pointer-copies"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        for image in images.values():
            self.assertEqual(image.symbols["ra_pointer"], image.symbols["ra_buffer"] + 128)
        calls = 0
        for program in programs:
            width = 2 if program.name.startswith(("pointer_u16_", "pointer_s16_")) else 1
            signed = program.name.startswith(("pointer_s8_", "pointer_s16_"))
            for index in (-3, 0, 1, 7, 23):
                for value in (0, 1, 127, 128, 255, 256, 32767, 32768, 65535):
                    initial = bytearray((offset * 37 + value) & 255 for offset in range(130))
                    expected_buffer = initial.copy()
                    address = 64 + width * index
                    if program.family == "store":
                        stored = (value + index) & ((1 << (8 * width)) - 1)
                        expected_buffer[address:address + width] = stored.to_bytes(width, "little")
                    else:
                        stored = int.from_bytes(initial[address:address + width], "little")
                    if signed and stored & (1 << (8 * width - 1)):
                        stored -= 1 << (8 * width)
                    expected = stored if program.family == "store" else value * 7 + stored
                    if program.family == "pointer_alias":
                        expected_buffer[address] = (value + index) & 255
                        expected_buffer[address + 1] = (expected_buffer[address] + 1) & 255
                        expected = expected_buffer[address] + expected_buffer[address + 1]
                    elif program.family in ("pointer_call", "pointer_post"):
                        expected_buffer[address] = (value + index if program.family == "pointer_call" else value) & 255
                        expected = expected_buffer[address] + (index + 1 if program.family == "pointer_post" else 0)
                        if program.family == "pointer_call":
                            expected_buffer[127] = (expected_buffer[127] + 1) & 255
                    for mode, image in images.items():
                        expected_memory = expected_buffer.copy()
                        if program.name == "pointer_global":
                            expected_memory[128:130] = (image.symbols["ra_buffer"] + 64).to_bytes(2, "little")
                        result = execute(image.rom, image.symbols[program.name],
                                         [image.symbols["ra_buffer"] + 64, index & 65535, value], [2, 2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected & 65535, expected_memory, []), (program.name, index, value, mode))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        print(f"Pointer-copy arithmetic oracle executions: {calls}", flush=True)
        for mode in images:
            assembly = (directory / mode / "main.asm").read_text()
            if mode not in ("ra", "ra_quiet"):
                self.assertNotIn("; folded spilled pointer base", assembly, mode)
                continue
            for program in programs[:8]:
                body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                if program.family == "store":
                    self.assertIn("; folded spilled pointer base", body, (program.name, mode))
                else:
                    self.assertNotIn("; folded spilled pointer base", body, (program.name, mode))
                    self.assertIn("; retained arg1 in HL", body, (program.name, mode))

    def test_unsigned_power_of_two_arithmetic(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        programs = []
        for kind, parameter, result in (("byte", "u8", "u8"), ("word", "u16", "u16"),
                                         ("widen", "u8", "u16"), ("truncate", "u16", "u8")):
            for shift in range(16):
                for operation, symbol in (("div", "/"), ("mod", "%")):
                    name = f"power_{kind}_{operation}_{shift}"
                    prefix = "u16 wide = a; " if kind == "widen" else ""
                    operand = "wide" if kind == "widen" else "a"
                    programs.append(Program(name, operation, shift, 1 << shift,
                                            [1 if parameter == "u8" else 2], 1 if result == "u8" else 2, [],
                                            f"{result} {name}({parameter} a) {{ "
                                            f"{prefix}return {operand}{symbol}{1 << shift}; }}"))
        for name, body in (
            ("power_pair", "u16 q=a/256; u16 r=a%256; return (q<<8)|r;"),
            ("power_post", "u16 before=a++ /256; return (before<<8)|(a&255);"),
            ("power_call", "return power_side(a)%256;"),
            ("power_call_zero", "return power_side(a)%1;"),
            ("power_call_identity", "return power_side(a)/1;"),
            ("power_post_zero", "u16 before=a++%1; return before+a;"),
        ):
            programs.append(Program(name, name, 0, 1 if name.endswith(("zero", "identity")) else 256,
                                    [2], 2, [], f"u16 {name}(u16 a) {{ {body} }}"))
        source = ("u8 ra_buffer[128];\nu16 power_side(u16 a) { ra_buffer[0]=ra_buffer[0]+1; return a; }\n"
                  + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        directory = self.output / "power-of-two"
        images = compile_generated(directory, compiler, linker, programs, source)
        for mode in images:
            sections = dict(re.findall(r'\.SECTION "([^"]+)" FREE\n(.*?)\.ENDS',
                                       (directory / mode / "main.asm").read_text(), re.S))
            for program in programs:
                if not (program.name.startswith("power_byte_") and 8 <= program.variant < 15):
                    self.assertNotIn("ssc_arithmetic_begin", sections[program.name], (program.name, mode))
        calls = 0
        initial = bytearray((index * 37 + 11) & 255 for index in range(128))
        for program in programs:
            mask = (1 << (8 * program.widths[0])) - 1
            values = range(256) if mask == 255 else sorted({
                *(value & mask for value in (0, 1, 2, 3, 7, 15, 16, 127, 128, 255, 256, 257,
                                             32767, 32768, 65535, program.constant - 1,
                                             program.constant, program.constant + 1)),
                *((index * 811 ^ 0xa55a) & mask for index in range(64)),
            })
            for value in values:
                expected_buffer = bytearray(initial)
                if program.family == "power_pair":
                    expected = value
                elif program.family == "power_post":
                    expected = ((value // 256) << 8) | ((value + 1) & 255)
                elif program.family == "power_post_zero":
                    expected = value + 1
                elif program.family.startswith("power_call"):
                    expected = value if program.family.endswith("identity") else value % program.constant
                    expected_buffer[0] = (expected_buffer[0] + 1) & 255
                else:
                    expected = value // program.constant if program.family == "div" else value % program.constant
                expected &= (1 << (8 * program.returnWidth)) - 1
                for mode, image in images.items():
                    result = execute(image.rom, image.symbols[program.name], [value], program.widths,
                                     initial, image.symbols["ra_buffer"], program.returnWidth, image.transfers)
                    self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                     (expected, expected_buffer, []), (program.name, value, mode))
                    self.assertEqual(result["unmapped_instructions"], 0)
                    calls += 1
        print(f"Power-of-two arithmetic oracle executions: {calls}", flush=True)

    def test_power_of_two_fallbacks_and_invalid_literals(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        programs = [
            Program(f"power_fallback_{index}", operation, index, int(float(divisor)), [2], 2, [],
                    f"{kind} power_fallback_{index}({kind} a) {{ return a{operation}({divisor}); }}")
            for index, (kind, operation, divisor) in enumerate((
                ("s16", "/", 256), ("s16", "%", 16), ("s16", "%", 256),
                ("u16", "/", -256), ("u16", "%", -256), ("u16", "/", 257), ("u16", "%", 257),
                ("u16", "/", "2.5"), ("u16", "%", "2.5"), ("u16", "/", "256.5"), ("u16", "%", "256.5"),
            ))
        ]
        source = "u8 ra_buffer[128];\n" + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
        directory = self.output / "power-fallbacks"
        images = compile_generated(directory, compiler, linker, programs, source)
        for mode, image in images.items():
            assembly = (directory / mode / "main.asm").read_text()
            self.assertEqual(assembly.count("ssc_arithmetic_begin 2 "), len(programs))
            for program in programs:
                for value in (0, 1, 15, 16, 127, 128, 255, 256, 257, 32767, 32768, 65535):
                    divisor = program.constant & 65535
                    expected = value // divisor if program.family == "/" else value % divisor
                    result = execute(image.rom, image.symbols[program.name], [value], [2], bytearray(128),
                                     image.symbols["ra_buffer"], 2, image.transfers)
                    self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                     (expected, bytearray(128), []), (program.name, value, mode))
                    self.assertEqual(result["unmapped_instructions"], 0)
        for index, (operation, literal) in enumerate(
            (operation, literal) for operation in ("/", "%") for literal in ("0", "65536", "-65536")
        ):
            output = self.output / f"power-invalid-{index}"
            output.mkdir()
            (output / "main.ssc").write_text(f"u16 invalid(u16 a) {{ return a{operation}({literal}); }}\nvoid main(void) {{}}\n")
            completed = subprocess.run([compiler, "-mz80", "-rs", "3", "-o", "main.asm", "main.ssc"],
                                       cwd=output, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (output / "compiler.log").write_bytes(completed.stdout)
            if literal == "0":
                self.assertNotEqual(completed.returncode, 0, operation)
                self.assertIn(b"zero", completed.stdout)
            else:
                self.assertEqual(completed.returncode, 0, (operation, literal))
                self.assertIn("ssc_arithmetic_begin 2 ", (output / "main.asm").read_text())
                completed = subprocess.run([linker, "-lsms", "-o", "linked.sms", "main.asm"],
                                           cwd=output, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                (output / "linker.log").write_bytes(completed.stdout)
                self.assertNotEqual(completed.returncode, 0, (operation, literal))

    def test_opt_in_arithmetic_sharing(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        calls = 0
        for family, matrix in (("matrix_divmod,matrix_variable_multiply", True), ("scalar_byte_multiply", False)):
            with patch.dict(os.environ, RA_FAMILIES=family, RA_SCALAR_CASES="0" if matrix else "1",
                            RA_MATRIX_CASES="1" if matrix else "0", RA_VALUE_CASES="0"):
                programs, _, source = generate_programs(0)
            directory = self.output / f"arithmetic-{matrix}"
            inline = compile_generated(directory / "inline", compiler, linker, programs, source)
            shared = compile_generated(directory / "shared", compiler, linker, programs, source,
                                       linker_flags=("-share-arithmetic",))
            self.assertTrue(programs)
            for mode, image in shared.items():
                self.assertTrue(any(name.startswith("ssc.arithmetic_") for name in image.symbols))
                self.assertLess(image.staticMetrics["rom_used_bytes"], inline[mode].staticMetrics["rom_used_bytes"])
            for program in programs:
                for seed in range(64):
                    arguments, buffer, expected_buffer, expected, ports, pointer = generated_oracle(program, seed, 0, {})
                    self.assertFalse(pointer)
                    self.assertFalse(program.pointerParameters)
                    for mode in shared:
                        results = []
                        for image in (inline[mode], shared[mode]):
                            result = execute(image.rom, image.symbols[program.name], arguments, program.widths,
                                             buffer, image.symbols[buffer_symbol(program)], program.returnWidth,
                                             image.transfers)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                             (expected, expected_buffer, ports), (program.name, seed, mode))
                            self.assertEqual(result["unmapped_instructions"], 0)
                            calls += 1
                            results.append(result)
                        extra = results[1]["cycles"] - results[0]["cycles"]
                        self.assertGreaterEqual(extra, 0)
                        self.assertEqual(extra % 27, 0)
                        for traffic in ("frame_reads", "frame_writes"):
                            self.assertEqual(results[1][traffic] - results[0][traffic], 2 * (extra // 27))
        print(f"Shared arithmetic oracle executions: {calls}", flush=True)

    def test_shared_arithmetic_banks_and_recursive_calls(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        sources = [
            (0, 0, "u8 ra_buffer[128];\n"
             "u16 arithmetic_other(u16 a, u16 b);\n"
             "u16 arithmetic_root(u16 a, u16 b) { return a*b+arithmetic_other(a,b); }\n"
             "u16 arithmetic_recursive(u8 depth, u16 a, u16 b) { "
             "if (depth) { return arithmetic_recursive(depth-1,a,b)+a*b; } return a*b; }\n"
             "void main(void) {}\n"),
            (0, 0, "u16 arithmetic_other(u16 a, u16 b) { return a*b; }\n"),
            (1, 1, "u16 arithmetic_one(u16 a, u16 b) { return a*b; }\n"
             "u16 arithmetic_nested(u16 a, u16 b) { return a*b+arithmetic_one(a,b); }\n"),
            (3, 2, "u16 arithmetic_nested(u16 a, u16 b);\n"
             "u16 arithmetic_root(u16 a, u16 b);\n"
             "u16 arithmetic_three(u16 a, u16 b) { return a*b; }\n"
             "u16 arithmetic_banked(u16 a, u16 b) { "
             "return a*b+arithmetic_three(a,b)+arithmetic_nested(a,b)+arithmetic_root(a,b); }\n"),
        ]
        for flags in (("-no-ra",), ("-ra-all-spill",), ("-ra",)):
            directory = self.output / f"arithmetic-banks-{flags[0][1:]}"
            directory.mkdir()
            for index, (bank, slot, source) in enumerate(sources):
                (directory / f"unit{index}.ssc").write_text(source)
                command = [compiler, "-mz80", "-rs", "3", "-b", str(bank), "-s", str(slot), *flags,
                           "-o", f"unit{index}.asm", f"unit{index}.ssc"]
                completed = subprocess.run(command, cwd=directory, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                (directory / f"unit{index}.compiler.log").write_bytes(completed.stdout)
                self.assertEqual(completed.returncode, 0, completed.stdout.decode(errors="replace"))
            images = []
            for profile in ("inline", "shared"):
                output = directory / profile
                output.mkdir()
                completed = subprocess.run(
                    [linker, "-lsms", "-ro", "4", "-v", *(["-share-arithmetic"] if profile == "shared" else []),
                     "-o", "linked.sms", *(str(directory / f"unit{index}.asm") for index in range(len(sources)))],
                    cwd=output, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                )
                (output / "linker.log").write_bytes(completed.stdout)
                self.assertEqual(completed.returncode, 0, completed.stdout.decode(errors="replace"))
                image = read_image(output)
                helpers = {name: bank for name, bank in image.banks.items() if name.startswith("ssc.arithmetic_")}
                if profile == "shared":
                    self.assertEqual(sorted(helpers.values()), [0, 1, 3])
                    self.assertIn("bank_003_slot_2", " ".join(helpers))
                else:
                    self.assertFalse(helpers)
                mapped = mapped_routine_image("arithmetic", "arithmetic_banked", flags[0], image)
                mapped.transfers.update({key & 65535: value for key, value in image.transfers.items() if key >> 16 == 1})
                images.append(mapped)
            self.assertLess(images[1].staticMetrics["rom_used_bytes"], images[0].staticMetrics["rom_used_bytes"])
            for a in (0, 1, 127, 128, 255, 256, 32768, 65535):
                for b in (0, 1, 255, 256, 32767, 32768, 65535):
                    for name, arguments, widths, expected in (
                        ("arithmetic_banked", [a, b], [2, 2], 6 * a * b),
                        ("arithmetic_recursive", [3, a, b], [1, 2, 2], 4 * a * b),
                    ):
                        results = [
                            execute(image.rom, image.symbols[name], arguments, widths, bytearray(128),
                                    image.symbols["ra_buffer"], 2, image.transfers)
                            for image in images
                        ]
                        for result in results:
                            self.assertEqual(result["value"], expected & 65535, (name, arguments, flags))
                            self.assertEqual(result["buffer"], bytearray(128))
                            self.assertEqual(result["unmapped_instructions"], 0)
                        extra = results[1]["cycles"] - results[0]["cycles"]
                        self.assertGreater(extra, 0)
                        self.assertEqual(extra % 27, 0)

    def test_shared_arithmetic_single_uses_and_invalid_markers(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        programs = [
            Program(f"arithmetic_single_{index}", "arithmetic", index, 0, [width, width], 2, [],
                    f"u16 arithmetic_single_{index}({kind} a, {kind} b) {{ return a{op}b; }}")
            for index, (kind, width, op) in enumerate((("u8", 1, "*"), ("u16", 2, "*"),
                                                      ("u16", 2, "/"), ("u8", 1, "/")))
        ]
        source = "u8 ra_buffer[128];\n" + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
        directory = self.output / "arithmetic-single"
        inline = compile_generated(directory / "inline", compiler, linker, programs, source)
        shared = compile_generated(directory / "shared", compiler, linker, programs, source,
                                   linker_flags=("-share-arithmetic",))
        for mode, image in shared.items():
            self.assertEqual(image.rom, inline[mode].rom)
            self.assertFalse(any(name.startswith("ssc.arithmetic_") for name in image.symbols))
        assembly = (directory / "shared" / "ra" / "main.asm").read_text()
        self.assertEqual(len(re.findall("ssc_arithmetic_begin", assembly)), 4)
        for index, malformed in enumerate((
            assembly.replace("ssc_arithmetic_begin 0 0 0", "ssc_arithmetic_begin x 0 0", 1),
            assembly.replace("ssc_arithmetic_begin 0 0 0", "ssc_arithmetic_begin 4 0 0", 1),
            assembly.replace("ssc_arithmetic_begin 0 0 0", "ssc_arithmetic_begin 0 256 0", 1),
            assembly.replace("ssc_arithmetic_begin 0 0 0", "ssc_arithmetic_begin 0 0 4", 1),
            assembly.replace("ssc_arithmetic_end", "ssc_arithmetic_end trailing", 1),
            assembly.replace("      ; ssc_arithmetic_end\n", "", 1),
        )):
            output = directory / f"invalid-{index}"
            output.mkdir()
            self.assertNotEqual(malformed, assembly)
            (output / "main.asm").write_text(malformed)
            completed = subprocess.run([linker, "-lsms", "-share-arithmetic", "-o", "linked.sms", "main.asm"],
                                       cwd=output, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (output / "linker.log").write_bytes(completed.stdout)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn(b"Invalid arithmetic body", completed.stdout)

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
