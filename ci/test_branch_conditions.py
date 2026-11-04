"""Execute shared branch lowering using the allocator's existing build/CPU helpers."""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from check_register_allocator import (
    BYTE_SHIFT_COUNTS, Program, buffer_symbol, compile_generated, generate_programs, generated_oracle, mapped_routine_image, read_image,
    run,
)
from ra_cpu import execute


class BranchConditionsTests(unittest.TestCase):
    def test_allocated_regional_snapshots(self):
        forms = {
            "straight": "other[index]=total; result=data[index];",
            "join": "if(n&1) { other[0]+=1; } else { other[1]+=2; } result=data[index];",
            "nested": "if(n&1) { if(n&2) { other[0]+=1; } else { other[1]+=2; } } result=data[index];",
            "guarded": "if(n&1) { result=data[index]; } else { result=b; }",
            "zero": "result=0; for(i=0;i<(n&3);i++) { result+=data[index]; }",
            "nested_loop": "result=0; for(i=0;i<(n&3);i++) { for(j=0;j<(a&3);j++) { result+=data[index]; } }",
            "latches": "result=0; for(i=0;i<(n&3);i++) { if(i&1) { result+=data[index]; continue; } result^=data[index]; }",
            "changed_index": "index=(index+1)&3; result=data[index];",
            "changed_store": "data[index]=b; result=data[index];",
            "changed_join": "if(n&1) { data[index]=b; } result=data[index];",
            "changed_loop": "for(i=0;i<(n&3);i++) { data[index]+=i+1; } result=data[index];",
            "pressure": "u16 v0=a*b; u16 v1=(a+1)*(b+3); total+=v0+v1; other[index]=v0^v1; result=data[index];",
            "call": "total=regional_snapshot_touch(total); result=data[index];",
            "asm": '__asm("LD HL,4660"); __asm("LD BC,22136"); __asm("XOR A"); result=data[index];',
            "port": "__z80_out[0xBE]=total; result=data[index];",
            "escape": "KIND *pointer=data; pointer[index]=b; result=data[index];",
            "shifts_straight": "other[index]=total; result=data[index];",
            "shifts_join": "if(n&1) { other[0]+=1; } else { other[1]+=2; } result=data[index];",
            "shifts_exit": "for(i=0;i<(n&3);i++) { other[i]+=1; } result=data[index];",
            "shifts_guarded": "if(n&1) { result=data[index]; } else { result=b; }",
            "equal": "other[index]=total; result=data[index];",
            "unequal": "other[index]=total; result=data[index];",
        }
        calls = selections = 0
        for kind in ("u8", "s8", "u16", "s16"):
            width = 1 if kind.endswith("8") else 2
            mask = (1 << (width*8))-1
            signed = kind.startswith("s")

            def scalar(value):
                value &= mask
                return value-mask-1 if signed and value & (mask+1)//2 else value

            programs = []
            for family, form in forms.items():
                name = f"snapshot_{kind}_{family}"
                initialize = " ".join(
                    f"data[{i}]=a+{i*257}; other[{i}]=b-{i*131};" for i in range(4))
                output = " ".join(f"ra_buffer[{base+i*2}]={array}[{i}]; ra_buffer[{base+i*2+1}]="
                                  + (f"{array}[{i}]>>8;" if width == 2 else "0;")
                                  for array, base in (("data", 0), ("other", 16)) for i in range(4))
                snapshot = (f" {'s16' if signed else 'u16'} saved;"
                            + (" if(n&1) { saved=data[index]; } else { saved="
                               + ("a;" if family == "unequal" else "data[index];") + " } "
                               if family in ("equal", "unequal") else " saved=data[index]; "))
                source = (f"u16 {name}(u16 a,u16 b,u16 n) {{ {kind} data[4]; {kind} other[4]; "
                          f"u16 index=b&3; u16 i; u16 j; u16 total; {'s16' if signed else 'u16'} result; "
                          + initialize + snapshot
                          + ("total=saved>>3; total+=saved>>5; " if family.startswith("shifts_") else
                             "total=saved+other[index]; total^=saved; ")
                          + form.replace("KIND", kind) + output + " return total^result; }")
                programs.append(Program(name, family, width, int(signed), [2, 2, 2], 2, [], source))
            for policy in ("inline", "shared"):
                for offset in range(0, len(programs), 4):
                    group = programs[offset:offset+4]
                    directory = self.output / "allocated-snapshots" / kind / policy / f"batch-{offset}"
                    source = ("u8 ra_buffer[128];\n"
                              "u16 regional_snapshot_touch(u16 x) { ra_buffer[127]^=x; return x+7; }\n"
                              + "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
                    images = compile_generated(
                        directory, os.environ.get("SAMESAMECC") or "samesamecc",
                        os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                        linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                    baseline_reads = {}
                    for mode, image in images.items():
                        assembly = (directory / mode / "main.asm").read_text()
                        for program in group:
                            body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                            reads = len(re.findall(r"; TAC: .* := _data_\d+\..*\[",
                                                   body.split("; TAC: ra_buffer", 1)[0]))
                            if mode == "default":
                                baseline_reads[program.name] = reads
                            elif mode in ("ra", "ra_quiet") and os.environ.get("RA_BASELINE") != "1":
                                if program.family.startswith("shifts_") or kind == "u8" and program.family in (
                                        "straight", "join", "nested", "guarded"):
                                    self.assertLess(reads, baseline_reads[program.name], (program.name, policy, mode))
                                    selections += 1
                                elif program.family in ("changed_index", "changed_store", "changed_join",
                                                        "changed_loop", "pressure", "call", "asm", "port", "escape",
                                                        "equal", "unequal"):
                                    self.assertEqual(reads, baseline_reads[program.name], (program.name, policy, mode))
                            for a in (0, 127, 128, 255, 256, 32768, 65535):
                                b = (a*137+0x8001)&65535
                                for n in (0, 1, 2, 3):
                                    data = [scalar(a+i*257) for i in range(4)]
                                    other = [scalar(b-i*131) for i in range(4)]
                                    index = b&3
                                    saved = a if program.family == "unequal" and not n&1 else data[index]
                                    shifted = program.family.startswith("shifts_")
                                    total = ((((saved&65535)>>3)+((saved&65535)>>5)) if shifted else
                                             ((saved+other[index])^saved))&65535
                                    initial = bytes((i*37+a+n)&255 for i in range(128))
                                    expected = bytearray(initial)
                                    family = program.family.removeprefix("shifts_")
                                    ports = []
                                    if family in ("straight", "equal", "unequal"):
                                        other[index] = scalar(total)
                                    elif family == "join":
                                        lane, add = (0, 1) if n&1 else (1, 2)
                                        other[lane] = scalar(other[lane]+add)
                                    elif family == "nested" and n&1:
                                        lane, add = (0, 1) if n&2 else (1, 2)
                                        other[lane] = scalar(other[lane]+add)
                                    elif family == "changed_index":
                                        index = (index+1)&3
                                    elif family in ("changed_store", "escape") or family == "changed_join" and n&1:
                                        data[index] = scalar(b)
                                    elif family == "changed_loop":
                                        for i in range(n&3):
                                            data[index] = scalar(data[index]+i+1)
                                    elif family == "exit":
                                        for i in range(n&3):
                                            other[i] = scalar(other[i]+1)
                                    elif family == "pressure":
                                        v0, v1 = (a*b)&65535, ((a+1)*(b+3))&65535
                                        total = (total+v0+v1)&65535
                                        other[index] = scalar(v0^v1)
                                    elif family == "call":
                                        expected[127] ^= total&255
                                        total = (total+7)&65535
                                    elif family == "port":
                                        ports.append([0xBE, total&255])
                                    result = (b if family == "guarded" and not n&1 else data[index])&65535
                                    if family in ("zero", "nested_loop", "latches"):
                                        result = 0
                                        for i in range(n&3):
                                            for _ in range((a&3) if family == "nested_loop" else 1):
                                                result = ((result^data[index]) if family == "latches" and not i&1
                                                          else (result+data[index]))&65535
                                    for array, base in ((data, 0), (other, 16)):
                                        for i, value in enumerate(array):
                                            expected[base+i*2] = value&255
                                            expected[base+i*2+1] = (value>>8)&255 if width == 2 else 0
                                    with self.subTest(program=program.name, policy=policy, mode=mode, a=a, n=n):
                                        actual = execute(image.rom, image.symbols[program.name], [a, b, n],
                                                         program.widths, initial, image.symbols["ra_buffer"], 2,
                                                         image.transfers, ports_allowed=True, frame_bytes=1024)
                                        self.assertEqual((actual["value"], actual["buffer"], actual["port_writes"]),
                                                         ((total^result)&65535, expected, ports))
                                        self.assertEqual(actual["unmapped_instructions"], 0)
                                    calls += 1
        self.assertEqual(calls, len(forms)*4*2*5*7*4)
        if os.environ.get("RA_BASELINE") != "1":
            self.assertEqual(selections, 80)
        print(f"Allocated snapshot oracle: {len(forms)*4} shapes, {calls} executions", flush=True)

    def test_maintained_induction_addresses(self):
        read = "total+=left[at]; total^=right[at]; total+=third[at];"
        forms = {
            "reads": "for(i=0;i<8;i++) { u16 at=i; READ }",
            "stores": "for(i=0;i<8;i++) { left[i]=a+i; right[i]=b-i; third[i]=left[i]^right[i]; narrow[i]=(a+i)*(b-i); }",
            "global": "for(i=0;i<8;i++) { total+=ra_buffer[i]; total^=ra_buffer[i]; total+=ra_buffer[i]; }",
            "pointer": "for(i=0;i<8;i++) { total+=p[i]; total^=p[i]; total+=p[i]; }",
            "overlap": "for(i=0;i<7;i++) { p[i+1]=p[i]; total+=p[i+1]; }",
            "zero": "for(i=0;i<(n&7);i++) { u16 at=i; READ }",
            "signed": "for(i=7;i>=0;i--) { u16 at=i; READ }",
            "down": "for(i=8;i>0;i--) { u16 at=i-1; READ }",
            "stride": "i=0; while(i<8) { u16 at=i; READ i+=2; }",
            "latches": "for(i=0;i<8;i++) { if(i&1) { total+=left[i]; continue; } u16 at=i; READ }",
            "nested": "for(j=0;j<2;j++) { for(i=0;i<8;i++) { u16 at=i; READ } }",
            "skipped": "i=0; if(n&1) { for(i=0;i<8;i++) { u16 at=i; READ } }",
            "call": "for(i=0;i<8;i++) { total=induction_touch(total); u16 at=i; READ }",
            "asm": 'for(i=0;i<8;i++) { __asm("LD IY,4660"); u16 at=i; READ }',
            "ports": "for(i=0;i<8;i++) { __z80_out[0xBE]=a+i; u16 at=i; READ }",
            "pressure": "for(i=0;i<8;i++) { total+=v0+v1+v2+v3+v4+v5; u16 at=i; READ }",
            "bigframe": "for(i=0;i<8;i++) { u16 at=i; READ }",
            "wrap": "for(i=65534;i!=1;i++) { u16 at=i&7; READ }",
        }
        positive = {"reads", "global", "nested", "skipped", "ports", "bigframe"}
        fallback = {"pointer", "overlap", "zero", "signed", "down", "stride", "latches", "call", "asm", "wrap"}
        calls = selections = 0
        for kind in ("u8", "s8", "u16", "s16"):
            width = 1 if kind.endswith("8") else 2
            mask = (1 << (width*8))-1
            signed = kind.startswith("s")

            def scalar(value):
                value &= mask
                return value-mask-1 if signed and value & (mask+1)//2 else value

            programs = []
            for family, form in forms.items():
                name = f"induction_{kind}_{family}"
                initialize = " ".join(
                    f"left[{index}]=a+{index*257}; right[{index}]=b-{index*131}; third[{index}]=a^b^{index};"
                    for index in range(8))
                output = " ".join(f"ra_buffer[{base+index}]={array}[{index}];"
                                  for array, base in (("left", 0), ("right", 16), ("third", 32))
                                  for index in range(8))
                if family == "stores":
                    output += " ".join(f"ra_buffer[{48+index}]=narrow[{index}];" for index in range(8))
                pressure = ("u16 v0=a+1; u16 v1=b+2; u16 v2=a+3; u16 v3=b+4; u16 v4=a+5; u16 v5=b+6; "
                            if family == "pressure" else "")
                padding = "u8 padding[300]; padding[299]=b; " if family == "bigframe" else ""
                source = (f"u16 {name}(u16 a,u16 b,u16 n) {{ {kind} left[8]; {kind} right[8]; {kind} third[8]; "
                          f"{kind} *p=&ra_buffer[40]; {'s16' if family == 'signed' else 'u16'} i; "
                          f"u16 j; u16 total=a; {'u8 narrow[8];' if family == 'stores' else ''} {pressure}{padding}{initialize}"
                          + form.replace("READ", read)
                          + ("total^=padding[299]; " if family == "bigframe" else "")
                          + output + " ra_buffer[60]=total; ra_buffer[61]=i; return total^i; }")
                programs.append(Program(name, family, width, int(signed), [2, 2, 2], 2, [], source))
            for policy in ("inline", "shared"):
                for offset in range(0, len(programs), 6):
                    group = programs[offset:offset+6]
                    directory = self.output / "induction-addresses" / kind / policy / f"batch-{offset}"
                    source = (f"{kind} ra_buffer[{128//width}];\n"
                              "u16 induction_touch(u16 value) { ra_buffer[63]+=1; return value+7; }\n"
                              + "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
                    images = compile_generated(
                        directory, os.environ.get("SAMESAMECC") or "samesamecc",
                        os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                        linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                    for mode, image in images.items():
                        assembly = (directory / mode / "main.asm").read_text()
                        for program in group:
                            body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                            selected = "; maintain loop induction address;" in body
                            if os.environ.get("RA_BASELINE") != "1":
                                if mode not in ("ra", "ra_quiet") or program.family in fallback:
                                    self.assertFalse(selected, (program.name, policy, mode))
                                elif program.family in positive:
                                    self.assertTrue(selected, (program.name, policy, mode))
                                    selections += 1
                                if mode in ("ra", "ra_quiet") and program.family == "stores" and width == 2:
                                    self.assertTrue(selected, (program.name, policy, mode))
                                    narrow_store = next(tac for tac in body.split("; TAC:")
                                                        if re.match(r" _narrow_\d+\.u8 .*\[", tac))
                                    self.assertIn("LD  A,L", narrow_store)
                            for a in (0, 1, 127, 128, 255, 256, 32768, 65535):
                                b = (a*137+0x8001)&65535
                                for n in (0, 1, 2, 65535):
                                    initial = bytes((index*37+a+b+n)&255 for index in range(128))
                                    expected = bytearray(initial)

                                    def item(index):
                                        return scalar(int.from_bytes(expected[index*width:(index+1)*width], "little"))

                                    def store(index, value):
                                        expected[index*width:(index+1)*width] = (value&mask).to_bytes(width, "little")

                                    left = [scalar(a+index*257) for index in range(8)]
                                    right = [scalar(b-index*131) for index in range(8)]
                                    third = [scalar(a^b^index) for index in range(8)]
                                    family = program.family
                                    indices = (list(range(7, -1, -1)) if family in ("signed", "down") else
                                               list(range(0, 8, 2)) if family == "stride" else
                                               list(range(n&7)) if family == "zero" else
                                               list(range(8))*2 if family == "nested" else
                                               [] if family == "skipped" and not n&1 else
                                               [6, 7, 0] if family == "wrap" else
                                               list(range(7)) if family == "overlap" else list(range(8)))
                                    total, ports = a, []
                                    for index in indices:
                                        if family == "stores":
                                            left[index], right[index] = scalar(a+index), scalar(b-index)
                                            third[index] = scalar(left[index]^right[index])
                                            store(48+index, ((a+index)*(b-index))&255)
                                            continue
                                        if family == "global" or family == "pointer":
                                            value = item(index+(40 if family == "pointer" else 0))
                                            total = ((total+value)^value)+value
                                        elif family == "overlap":
                                            store(41+index, item(40+index))
                                            total += item(41+index)
                                        elif family == "latches" and index&1:
                                            total += left[index]
                                        else:
                                            if family == "call":
                                                total += 7
                                                store(63, item(63)+1)
                                            if family == "ports":
                                                ports.append([0xBE, (a+index)&255])
                                            if family == "pressure":
                                                total += sum((value+add)&65535 for value, add in
                                                             ((a, 1), (b, 2), (a, 3), (b, 4), (a, 5), (b, 6)))
                                            total = ((total+left[index])^right[index])+third[index]
                                        total &= 65535
                                    final = (65535 if family == "signed" else 0 if family == "down" else
                                             1 if family == "wrap" else n&7 if family == "zero" else
                                             0 if family == "skipped" and not n&1 else 7 if family == "overlap" else 8)
                                    if family == "bigframe":
                                        total ^= b&255
                                    for values, base in ((left, 0), (right, 16), (third, 32)):
                                        for index, value in enumerate(values):
                                            store(base+index, value)
                                    store(60, total)
                                    store(61, final)
                                    with self.subTest(program=program.name, policy=policy, mode=mode, a=a, n=n):
                                        result = execute(image.rom, image.symbols[program.name], [a, b, n],
                                                         program.widths, initial, image.symbols["ra_buffer"], 2,
                                                         image.transfers, ports_allowed=True, frame_bytes=1024)
                                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                                         ((total^final)&65535, expected, ports))
                                        self.assertEqual(result["unmapped_instructions"], 0)
                                    calls += 1
        self.assertEqual(calls, len(forms)*4*2*5*8*4)
        if os.environ.get("RA_BASELINE") != "1":
            self.assertEqual(selections, len(positive)*4*2*2)
        print(f"Maintained induction oracle: {len(forms)*4} shapes, {calls} executions", flush=True)

    def test_regional_local_array_reuse(self):
        forms = {
            "straight": "other[index]=b; result=data[index];",
            "join": "if(b&4) { other[0]+=1; } else { other[1]+=2; } result=data[index];",
            "nested": "if(b&4) { if(b&2) { other[0]+=1; } else { other[1]+=2; } } result=data[index];",
            "constant": "data[index]=0x92FF; other[index]=b; result=data[index];",
            "stored": "KIND saved=a; data[index]=saved; other[index]=b; result=data[index];",
            "stored_join": "KIND saved=a; data[index]=saved; if(b&4) { other[0]+=1; } result=data[index]; result+=saved;",
            "changed_index": "index=(index+1)&3; other[index]=b; result=data[index];",
            "changed_source": "first+=1; other[index]=b; result=data[index];",
            "same_array": "data[(index+1)&3]=b; result=data[index];",
            "changed_join": "if(b&4) { data[index]=b; } result=data[index];",
            "unequal": "if(b&4) { first=data[index]; } else { first=a; } result=data[index];",
            "equal_stores": "if(b&4) { data[index]=b; } else { data[index]=b; } result=data[index];",
            "escape": "pointer=data; other[index]=b; result=data[index]; result+=pointer[(index+1)&3];",
            "partial": "bytes=data; bytes[index*WIDTH]=b; result=data[index]; result+=bytes[index*WIDTH];",
            "pointer_store": "pointer=other; pointer[index]=b; result=data[index]; result+=pointer[index];",
            "call": "b=regional_touch(b); result=data[index];",
            "asm": '__asm("NOP"); result=data[index];',
            "output": "__z80_out[0xBE]=a; result=data[index];",
            "input": "result=__z80_in[0xBE]; result+=data[index];",
            "zero_loop": "result=0; for(i=0;i<(b&3);i++) { other[i]=b+i; result+=data[index]; }",
            "nested_loop": "u16 j; result=0; for(i=0;i<(b&3);i++) { for(j=0;j<(a&3);j++) { other[j]=b+i+j; result+=data[index]; } }",
            "changed_loop": "result=0; for(i=0;i<(b&3);i++) { data[index]+=i+1; result+=data[index]; }",
            "skipped": "if(b&4) { first=data[index]; other[index]=b; result=data[index]; } else { result=b; }",
            "byte_index": "u8 small=b&3; first=data[small]; other[small]=b; result=data[small];",
            "unsigned": "other[index]=b; u16 widened=data[index]; result=widened;",
        }
        selected = {"straight", "join", "nested", "constant", "stored", "stored_join"}
        fallback = {"changed_index", "changed_source", "same_array", "changed_join", "unequal",
                    "equal_stores", "escape", "partial", "pointer_store", "call", "asm", "output", "input"}
        programs = []
        for kind in ("u8", "s8", "u16", "s16"):
            width = 1 if kind.endswith("8") else 2
            for name, body in forms.items():
                source = (
                    f"u16 regional_{kind}_{name}(u16 a,u16 b) {{ "
                    f"{kind} data[4]; {kind} other[4]; {kind} first; {kind} *pointer; u8 *bytes; "
                    f"u16 index=b&3; u16 i; {'s16' if kind == 's8' else 'u16'} result; "
                    "for(i=0;i<4;i++) { data[i]=a+i*257; other[i]=b+i*13; } "
                    + ("first=a; " if name in ("unequal", "skipped", "byte_index") else "first=data[index]; ")
                    + body.replace("KIND", kind).replace("WIDTH", str(width))
                    + f" for(i=0;i<4;i++) {{ ra_buffer[i*{width}]=data[i]; "
                    + (f"ra_buffer[i*{width}+1]=data[i]>>8; " if width == 2 else "")
                    + f"ra_buffer[16+i*{width}]=other[i]; "
                    + (f"ra_buffer[17+i*{width}]=other[i]>>8; " if width == 2 else "")
                    + "} return result+first; }"
                )
                programs.append(Program(f"regional_{kind}_{name}", name, width, int(kind.startswith("s")),
                                        [2, 2], 2, [], source))

        calls = selections = 0
        for policy in ("inline", "shared"):
            for offset in range(0, len(programs), 12):
                group = programs[offset:offset+12]
                directory = self.output / "regional-reuse" / policy / f"batch-{offset}"
                source = ("u8 ra_buffer[128];\n"
                          "u16 regional_touch(u16 value) { ra_buffer[127]^=value; return value+1; }\n"
                          + "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
                images = compile_generated(
                    directory, os.environ.get("SAMESAMECC") or "samesamecc",
                    os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                    linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                baseline_reads = {}
                for mode, image in images.items():
                    assembly = (directory / mode / "main.asm").read_text()
                    for program in group:
                        body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        output_loop = list(re.finditer(r"; TAC: _i_\d+\..* := 0\.", body))[-1].start()
                        body = body[:output_loop]
                        reads = len(re.findall(r"; TAC: .* := _data_\d+\..*\[", body))
                        if mode == "default":
                            baseline_reads[program.name] = reads
                        elif mode in ("ra", "ra_quiet"):
                            if program.family in selected:
                                self.assertLess(reads, baseline_reads[program.name], (policy, mode, program.name))
                                selections += 1
                            elif program.family in fallback or program.family == "unsigned" and program.constant and program.variant == 1:
                                self.assertEqual(reads, baseline_reads[program.name], (policy, mode, program.name))
                        for a in (0, 127, 128, 255, 256, 32768, 65535):
                            for b in (0, 1, 2, 3, 4, 7, 65535):
                                width = program.variant
                                mask = (1 << (width*8))-1
                                data = [(a+i*257)&mask for i in range(4)]
                                other = [(b+i*13)&mask for i in range(4)]
                                index = b&3
                                first = data[index]
                                result = None
                                name = program.family

                                def extend(value):
                                    return value-(mask+1) if program.constant and value&(mask+1)//2 else value

                                if name in ("straight", "constant", "stored", "changed_index", "changed_source",
                                            "escape", "pointer_store", "skipped", "byte_index", "unsigned"):
                                    if name == "constant":
                                        data[index] = 0x92FF&mask
                                    if name == "changed_index":
                                        index = (index+1)&3
                                    if name == "changed_source":
                                        first = (first+1)&mask
                                    if name == "skipped":
                                        first = data[index] if b&4 else a&mask
                                    if name != "skipped" or b&4:
                                        other[index] = b&mask
                                if name == "join":
                                    other[0 if b&4 else 1] = (other[0 if b&4 else 1]+(1 if b&4 else 2))&mask
                                if name == "nested" and b&4:
                                    other[0 if b&2 else 1] = (other[0 if b&2 else 1]+(1 if b&2 else 2))&mask
                                if name == "stored_join" and b&4:
                                    other[0] = (other[0]+1)&mask
                                if name in ("stored", "stored_join"):
                                    data[index] = a&mask
                                if name == "same_array":
                                    data[(index+1)&3] = b&mask
                                if name == "partial":
                                    data[index] = (data[index]&0xff00 if width == 2 else 0)|(b&255)
                                if name == "equal_stores" or name == "changed_join" and b&4:
                                    data[index] = b&mask
                                if name == "unequal" and not b&4:
                                    first = a&mask
                                if name in ("zero_loop", "nested_loop", "changed_loop"):
                                    result = 0
                                    for i in range(b&3):
                                        if name == "nested_loop":
                                            for j in range(a&3):
                                                other[j] = (b+i+j)&mask
                                                result += extend(data[index])
                                        else:
                                            if name == "changed_loop":
                                                data[index] = (data[index]+i+1)&mask
                                            else:
                                                other[i] = (b+i)&mask
                                            result += extend(data[index])
                                if result is None:
                                    result = extend(data[index])
                                if name == "stored_join":
                                    result += extend(a&mask)
                                if name == "escape":
                                    result += extend(data[(index+1)&3])
                                if name == "partial":
                                    result += b&255
                                if name == "pointer_store":
                                    result += extend(other[index])
                                if name == "skipped" and not b&4:
                                    result = b
                                initial = bytes((i*37+a+b)&255 for i in range(128))
                                buffer = bytearray(initial)
                                for i in range(4):
                                    buffer[i*width:(i+1)*width] = data[i].to_bytes(width, "little")
                                    buffer[16+i*width:16+(i+1)*width] = other[i].to_bytes(width, "little")
                                if name == "call":
                                    buffer[127] ^= b&255
                                ports = [[0xBE, a&255]] if name == "output" else []
                                actual = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                                 initial, image.symbols["ra_buffer"], 2, image.transfers,
                                                 frame_bytes=2048, instruction_limit=100000,
                                                 ports_allowed=name == "output")
                                self.assertEqual((actual["value"], actual["buffer"], actual["port_writes"],
                                                  actual["unmapped_instructions"]),
                                                 ((result+extend(first))&65535, buffer, ports, 0),
                                                 (policy, mode, program.name, a, b))
                                calls += 1
        self.assertEqual((calls, selections), (49000, 96))
        print(f"Regional array reuse: 100 shapes, {calls} guarded calls, {selections} selections", flush=True)

    def test_general_array_address_forms(self):
        indices = {
            "u8": (0, 1, 3, 7, 127, 128, 255),
            "s8": (0, 1, 3, -4, -1, -128, 127),
            "u16": (0, 1, 3, 7, 128, 32768, 65535, 0x6010, 0x6011, 0xC021),
            "s16": (0, 1, 3, -4, -1, -128, -32768, 32767, -8191),
        }
        pointer_forms = {
            "ptr_read": "out=p[idx];",
            "ptr_write": "p[idx]=a; out=a;",
            "ptr_read_write": "p[idx]=a; out=p[idx];",
            "ptr_alias": "out=p[idx]; p[idx]=a;",
            "ptr_loop": "out=0; for(i=0;i<(a&7);i++) { out+=p[idx+i]; }",
            "ptr_pressure": "u16 held=a+17; out=p[idx]; out+=held+(a^1234);",
            "ptr_live_base": "INDEX next=idx; next+=1; p[idx]=a; out=p[next]; out+=p[idx];",
        }
        local_forms = {
            "local_const": "data[3]=0x9234; out=data[3];",
            "local_zero": "data[3]=0; out=data[3];",
            "local_next": "data[3]=0x9234; out=data[4];",
            "local_previous": "data[3]=0; out=data[2];",
            "local_write_read": "data[j]=a; out=data[j];",
            "local_live_index": "data[j]=a; out=data[j]; out+=j;",
            "local_far": "data[3]=0; out=data[j]; out+=padding[259];",
        }
        global_forms = {
            "global_read": "out=ra_buffer[j+16];",
            "global_write_read": "ra_buffer[j+16]=a; out=ra_buffer[j+16];",
            "global_zero": "ra_buffer[3]=0; out=ra_buffer[3];",
            "global_pressure": "u16 held=a+17; out=ra_buffer[j+16]; out+=held+(a^idx);",
        }
        calls = selections = expected_cases = 0
        for kind in ("u8", "s8", "u16", "s16"):
            width = 1 if kind.endswith("8") else 2
            programs = []
            for index_kind, cases in indices.items():
                forms = dict(pointer_forms)
                if index_kind == "u16":
                    forms.update(local_forms)
                    forms["partial"] = "u8 *bytes=ra_buffer; bytes[32]=a; out=p[idx]; out+=bytes[32];"
                if index_kind == "s16":
                    forms.update(global_forms)
                for family, body in forms.items():
                    name = f"address_{kind}_{index_kind}_{family}"
                    prefix = "u16 out; u16 i; u16 j=idx&7; "
                    suffix = ""
                    if family.startswith("local"):
                        prefix += f"{kind} data[8]; "
                        if family == "local_far":
                            prefix += "u8 padding[260]; padding[259]=a; "
                        prefix += "for(i=0;i<8;i++) { data[i]=a+i*257; } "
                        suffix = "for(i=0;i<8;i++) { ra_buffer[i]=data[i]; } "
                    source = (f"u16 {name}({kind} *p,{index_kind} idx,u16 a) {{ "
                              + prefix + body.replace("INDEX", "s16" if index_kind.startswith("s") else "u16")
                              + suffix + " return out; }")
                    programs.append(Program(name, family, width, int(kind.startswith("s")),
                                            [2, 1 if index_kind.endswith("8") else 2, 2], 2, [], source))
                    expected_cases += len(cases)*8
            for policy in ("inline", "shared"):
                for offset in range(0, len(programs), 12):
                    group = programs[offset:offset+12]
                    directory = self.output / "address-forms" / kind / policy / f"batch-{offset}"
                    source = (f"{kind} ra_buffer[{128//width}];\n" +
                              "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
                    images = compile_generated(
                        directory, os.environ.get("SAMESAMECC") or "samesamecc",
                        os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                        linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                    for mode, image in images.items():
                        assembly = (directory / mode / "main.asm").read_text()
                        for program in group:
                            index_kind = program.name.split("_")[2]
                            body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                            if mode in ("ra", "ra_quiet"):
                                if program.family in ("local_const", "local_zero", "local_next", "local_previous") and not program.constant:
                                    self.assertIn("reuse preceding local-array store address in HL", body)
                                    if program.family == "local_next":
                                        self.assertIn("INC HL", body)
                                    elif width == 2 or program.family == "local_previous":
                                        self.assertIn("DEC HL", body)
                                    selections += 1
                                elif program.family == "ptr_read":
                                    self.assertRegex(body, r"LD  [ACL],\(HL\)")
                                    selections += 1
                            for index in indices[index_kind]:
                                for a in (0, 1, 7, 127, 128, 255, 32768, 65535):
                                    initial = bytes((i*37+a+index)&255 for i in range(128))
                                    buffer = bytearray(initial)
                                    mask = (1 << (width*8))-1

                                    def extend(value):
                                        return value-(mask+1) if program.constant and value&(mask+1)//2 else value

                                    def cell(byte):
                                        return extend(int.from_bytes(buffer[byte:byte+width], "little"))

                                    j = index&7
                                    expected = cell(32)
                                    if program.family in ("ptr_write", "ptr_read_write", "ptr_alias", "ptr_live_base"):
                                        buffer[32:32+width] = (a&mask).to_bytes(width, "little")
                                        if program.family == "ptr_write":
                                            expected = a
                                        elif program.family == "ptr_read_write":
                                            expected = cell(32)
                                        elif program.family == "ptr_live_base":
                                            expected = cell(32+width)+cell(32)
                                    elif program.family == "ptr_loop":
                                        expected = sum(cell(32+i*width) for i in range(a&7))
                                    elif program.family == "ptr_pressure":
                                        expected += ((a+17)&65535)+(a^1234)
                                    elif program.family == "partial":
                                        buffer[32] = a&255
                                        expected = cell(32)+(a&255)
                                    elif program.family.startswith("local"):
                                        data = [(a+i*257)&mask for i in range(8)]
                                        if program.family in ("local_const", "local_next"):
                                            data[3] = 0x9234&mask
                                        elif program.family in ("local_zero", "local_far", "local_previous"):
                                            data[3] = 0
                                        else:
                                            data[j] = a&mask
                                        expected = extend(data[3] if program.family in ("local_const", "local_zero") else data[j])
                                        if program.family == "local_next":
                                            expected = extend(data[4])
                                        elif program.family == "local_previous":
                                            expected = extend(data[2])
                                        if program.family == "local_live_index":
                                            expected += j
                                        elif program.family == "local_far":
                                            expected += a&255
                                        for i, value in enumerate(data):
                                            buffer[i*width:(i+1)*width] = value.to_bytes(width, "little")
                                    elif program.family.startswith("global"):
                                        byte = (j+16)*width
                                        if program.family == "global_zero":
                                            byte = 3*width
                                            buffer[byte:byte+width] = bytes(width)
                                        elif program.family == "global_write_read":
                                            buffer[byte:byte+width] = (a&mask).to_bytes(width, "little")
                                        expected = cell(byte)
                                        if program.family == "global_pressure":
                                            expected += ((a+17)&65535)+((a^index)&65535)
                                    pointer = (image.symbols["ra_buffer"]+32-width*index)&65535
                                    result = execute(
                                        image.rom, image.symbols[program.name], [pointer, index, a], program.widths,
                                        initial, image.symbols["ra_buffer"], 2, image.transfers,
                                        frame_bytes=2048, instruction_limit=100000)
                                    self.assertEqual(
                                        (result["value"], result["buffer"], result["port_writes"],
                                         result["unmapped_instructions"]),
                                        (expected&65535, buffer, [], 0),
                                        (policy, mode, program.name, index, a))
                                    calls += 1
        self.assertEqual(calls, expected_cases*10)
        self.assertEqual(selections, 96)
        print(f"General address forms: {calls} guarded calls; {selections} native/reused selections", flush=True)

    def test_lowering_aware_word_array_pairs(self):
        operations = {
            "add": ("+", lambda x, y: x+y),
            "sub": ("-", lambda x, y: x-y),
            "and": ("&", lambda x, y: x&y),
            "or": ("|", lambda x, y: x|y),
            "xor": ("^", lambda x, y: x^y),
        }
        forms = {
            **{name: f"out=left[p]{op}right[q];" for name, (op, _) in operations.items()},
            "pressure": "u16 extra=a+b; out=left[p]+right[q]+extra+(a^b);",
            "multi_read": "u16 held=left[p]; out=held+right[q]+held;",
            "live_indices": "out=left[p]+right[q]+p+q;",
            "biased": "lp=&left[4]; rp=&right[4]; out=lp[p-4]+rp[q-4];",
            "signed_index": "s16 index=p; out=left[index]+right[q];",
            "absolute": "out=left[p]-ra_buffer[3]; out+=ra_buffer[6]+right[q];",
            "overlap": "u16 held=left[p]; left[q]=b; out=held+left[q];",
            "branch": "out=left[p]+right[q]; if(a&256) { out=left[q]^right[p]; }",
            "nested_call": "out=left[p]+pair_wrap(right[q]);",
            "assembly": 'u16 held=left[p]; __asm("LD HL,0"); out=held+right[q];',
            "multiple_latches": (
                "out=0; for(i=0;i<(b&7);i++) { if(i&1) { "
                "out+=left[i]+right[(i+1)&7]; continue; } "
                "out+=left[(i+1)&7]-right[i]; }"),
        }
        programs = []
        for kind in ("u16", "s16", "u8", "s8"):
            for family, body in forms.items():
                if kind.endswith("8") and family not in operations:
                    continue
                name = f"word_pair_{kind}_{family}"
                if kind.endswith("8"):
                    body = ("u16 wide_left=left[p]; u16 wide_right=right[q]; "
                            f"out=wide_left{operations[family][0]}wide_right;")
                source = (f"u16 {name}(u16 a,u16 b) {{ {kind} left[8]; {kind} right[8]; "
                          f"{kind} *lp; {kind} *rp; u16 i; u16 p=a&7; u16 q=b&7; u16 out; "
                          "for(i=0;i<8;i++) { left[i]=a+i*257; "
                          + ("right[i]=b^(i*4660); " if family != "overlap" else "") + "} "
                          + body + " return out; }")
                programs.append(Program(name, family, 1 if kind.endswith("8") else 2,
                                        int(kind.startswith("s")), [2, 2], 2, [], source))

        calls = selected = 0
        for policy in ("inline", "shared"):
            for offset in range(0, len(programs), 12):
                group = programs[offset:offset+12]
                directory = self.output / "word-array-pairs" / policy / f"batch-{offset}"
                source = ("u16 ra_buffer[64];\n"
                          "u16 pair_touch(u16 value) { ra_buffer[63]^=value; return value+1; }\n"
                          "u16 pair_wrap(u16 value) { return pair_touch(value)+1; }\n" +
                          "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
                images = compile_generated(
                    directory, os.environ.get("SAMESAMECC") or "samesamecc",
                    os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                    linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                for mode, image in images.items():
                    assembly = (directory / mode / "main.asm").read_text()
                    for program in group:
                        body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        if mode in ("ra", "ra_quiet") and program.variant == 2 and (
                                program.family in operations or program.family == "multi_read"):
                            reads = [part for part in body.split("; TAC:")[1:]
                                     if ":=" in part.splitlines()[0] and
                                     "[" in part.splitlines()[0].split(":=", 1)[1]]
                            left = next(part for part in reads if "_left_" in part.splitlines()[0])
                            right = next(part for part in reads if "_right_" in part.splitlines()[0])
                            self.assertIn("; retained result in HL", left, (policy, mode, program.name))
                            self.assertNotRegex(left, r"LD\s+\(IX[+-]\d+\),[LH]")
                            self.assertIn("LD  C,(IY+0)", right, (policy, mode, program.name))
                            self.assertIn("LD  B,(IY+1)", right)
                            self.assertIn("; retained result in BC", right)
                            self.assertNotIn("LD  L,(IY+0)", right)
                            selected += 1
                        if mode in ("ra", "ra_quiet") and program.family == "absolute":
                            self.assertRegex(body, r"LD  BC,\(ra_buffer\s*\+\s*(?:6|12)&65535\)")
                        for a in (0, 1, 7, 127, 128, 255, 256, 32767, 32768, 65535):
                            for b in (0, 1, 7, 255, 256, 32768, 0x9234, 65535):
                                initial = bytes((i*37+a+b)&255 for i in range(128))
                                mask = (1 << (program.variant*8))-1
                                left = [(a+i*257)&mask for i in range(8)]
                                right = [(b^(i*4660))&mask for i in range(8)]

                                def extend(value):
                                    return value-(mask+1) if program.constant and value&(mask+1)//2 else value

                                p, q = a&7, b&7
                                x, y = extend(left[p]), extend(right[q])
                                expected = x+y
                                if program.family in operations:
                                    expected = operations[program.family][1](x, y)
                                elif program.family == "pressure":
                                    expected += ((a+b)&65535)+(a^b)
                                elif program.family == "multi_read":
                                    expected += x
                                elif program.family == "live_indices":
                                    expected += p+q
                                elif program.family == "absolute":
                                    expected -= int.from_bytes(initial[6:8], "little")
                                    expected += int.from_bytes(initial[12:14], "little")
                                elif program.family == "overlap":
                                    expected = x+extend(b&mask)
                                elif program.family == "branch" and a&256:
                                    expected = extend(left[q])^extend(right[p])
                                elif program.family == "nested_call":
                                    expected += 2
                                elif program.family == "multiple_latches":
                                    expected = sum(
                                        extend(left[i])+extend(right[(i+1)&7]) if i&1 else
                                        extend(left[(i+1)&7])-extend(right[i])
                                        for i in range(b&7))
                                buffer = bytearray(initial)
                                if program.family == "nested_call":
                                    buffer[126] ^= y&255
                                    buffer[127] ^= (y>>8)&255
                                result = execute(
                                    image.rom, image.symbols[program.name], [a, b], [2, 2],
                                    initial, image.symbols["ra_buffer"], 2, image.transfers,
                                    frame_bytes=2048, instruction_limit=100000)
                                self.assertEqual(
                                    (result["value"], result["buffer"], result["port_writes"],
                                     result["unmapped_instructions"]),
                                    (expected&65535, buffer, [], 0),
                                    (policy, mode, program.name, a, b))
                                calls += 1
        self.assertEqual(selected, 48)
        self.assertEqual(calls, len(programs)*80*10)
        print(f"Word-array pairs: {len(programs)} shapes, {calls} guarded calls, "
              f"{selected} simultaneous HL/BC selections", flush=True)

    def test_nested_copy_fill_loops(self):
        forms = {
            "copy": "dst[i]=src[i];",
            "fill": "dst[i]=value;",
            "global_copy": "ra_buffer[i+128]=ra_buffer[i+512];",
            "global_fill": "ra_buffer[i+128]=value;",
            "global_commuted_copy": "ra_buffer[128+i]=ra_buffer[512+i];",
            "global_commuted_fill": "ra_buffer[128+i]=value;",
            "local_source": "dst[i]=local_src[i];",
            "local_dest": "local_dst[i]=src[i];",
            "local_both": "local_dst[i]=local_src[i];",
            "source_wrap": "dst[i]=src[i+65535];",
            "dest_wrap": "dst[i+65535]=src[i];",
            "counter_fill": "dst[i]=i;",
            "partial_owner": "dst[i]=src[i];",
            "call": "dst[i]=nested_touch(src[i]);",
            "assembly": '__asm("NOP"); dst[i]=src[i];',
            "escaped_counter": "dst[i]=src[i];",
            "escaped_bound": "dst[i]=src[i];",
            "counter_write": "if(i==3) { i++; } dst[i]=src[i];",
            "break": "if(i==3) { break; } dst[i]=src[i];",
            "continue": "if(i&1) { continue; } dst[i]=src[i];",
        }
        programs = []
        variants = [(family, -1) for family in forms]
        variants += [(family, count) for family in ("copy", "fill") for count in (0, 1, 2, 3, 255, 256, 257)]
        for family, constant in variants:
            name = f"nested_{family}_{'word' if constant < 0 else constant}"
            filled = family.endswith("fill")
            local_source = family in ("local_source", "local_both")
            local_dest = family in ("local_dest", "local_both")
            prefix = "u16 i=0; u16 k; u16 total=n^rounds; "
            if local_source:
                prefix += ("u8 local_src[272]; u16 j; "
                           "for(j=0;j<272;j++) { local_src[j]=ra_buffer[512+j]; } ")
            if local_dest:
                prefix += "u8 local_dst[272]; "
            if family == "partial_owner":
                prefix += "u16 held=n+rounds; total^=held; "
            elif family in ("escaped_counter", "escaped_bound"):
                prefix += f"nested_escape(&{'i' if family == 'escaped_counter' else 'n'}); "
            inner = f"for(i=0;i<{'n' if constant < 0 else constant};i++) {{ {forms[family]} }} "
            loop = f"for(k=0;k<(rounds&3);k++) {{ total+=k; {inner} }} "
            if local_dest:
                loop += "for(k=0;k<i;k++) { ra_buffer[128+k]=local_dst[k]; } "
            tail = ("total+=dst[0]; " + ("total+=value; " if filled else "total+=src[0]; ") +
                    ("total+=held; " if family == "partial_owner" else "") + "return total+i+k;")
            programs.append(Program(name, family, 0, constant, [2, 2, 2, 2], 2, [],
                                    f"u16 {name}(u16 n,u16 rounds,u8 *dst,{'u16 value' if filled else 'u8 *src'}) "
                                    f"{{ {prefix}{loop}{tail} }}"))
        source = ("u8 ra_buffer[1024];\n"
                  "u16 nested_touch(u16 value) { ra_buffer[1023]+=1; return value^7; }\n"
                  "void nested_escape(u16 *value) { value[0]^=1; ra_buffer[1022]^=1; }\n" +
                  "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        directory = self.output / "nested-copy-fill"
        calls = selected = 0
        for policy in ("inline", "shared"):
            images = compile_generated(directory / policy, os.environ.get("SAMESAMECC") or "samesamecc",
                                       os.environ.get("SAMESAMECL") or "samesamecl", programs, source,
                                       linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for mode, image in images.items():
                assembly = (directory / policy / mode / "main.asm").read_text()
                for program in programs:
                    body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                    supported = program.family not in ("global_commuted_copy", "global_commuted_fill", "counter_fill",
                                                       "partial_owner", "call", "assembly", "escaped_counter",
                                                       "escaped_bound", "counter_write", "break", "continue")
                    chosen = "; advancing byte " in body
                    if os.environ.get("RA_BASELINE") != "1":
                        self.assertEqual(chosen, supported and program.constant != 0 and mode in ("ra", "ra_quiet"),
                                         (policy, mode, program.name))
                        if chosen:
                            self.assertNotRegex(body, r"TAC: if _i_\d+\.u16 .*>= ",
                                                (policy, mode, program.name, "nested counter still scalar"))
                    selected += int(chosen)
                    vectors = [(n, rounds, 64, 512) for n in (0, 1, 2, 3, 4, 255, 256, 257)
                               for rounds in (0, 1, 2)]
                    if program.family in ("copy", "source_wrap", "dest_wrap"):
                        vectors += [(7, 3, dst, src) for dst, src in
                                    ((32, 32), (33, 32), (32, 33), (64, 32), (32, 64))]
                    if program.family.startswith("local_"):
                        vectors = [(n, rounds, 64, 512) for n in (0, 1, 16, 257) for rounds in (0, 2)]
                    if program.constant >= 0:
                        vectors = [(0, 0, 64, 512), (65535, 2, 33, 32), (256, 1, 32, 33)]
                    for n, rounds, dst, src in vectors:
                        initial = bytes((index*37+n+rounds)&255 for index in range(1024))
                        buffer = bytearray(initial)
                        local = initial[512:784]
                        count = program.constant if program.constant >= 0 else n
                        total = n^rounds
                        index = 0
                        if program.family == "escaped_counter":
                            index = 1
                            buffer[1022] ^= 1
                        elif program.family == "escaped_bound":
                            count ^= 1
                            buffer[1022] ^= 1
                        elif program.family == "partial_owner":
                            total ^= (n+rounds)&65535
                        for outer in range(rounds&3):
                            total = (total+outer)&65535
                            index = 0
                            while index < count:
                                if program.family == "break" and index == 3:
                                    break
                                if program.family == "counter_write" and index == 3:
                                    index += 1
                                if program.family != "continue" or not index&1:
                                    value = (index if program.family == "counter_fill" else
                                             (n*257+rounds)&65535 if program.family.endswith("fill") else
                                             local[index] if program.family in ("local_source", "local_both") else
                                             buffer[(512 if program.family.startswith("global_") else src)+index-
                                                    (program.family == "source_wrap")])
                                    if program.family == "call":
                                        value ^= 7
                                        buffer[1023] = (buffer[1023]+1)&255
                                    if program.family not in ("local_dest", "local_both"):
                                        buffer[(128 if program.family.startswith("global_") else dst)+index-
                                               (program.family == "dest_wrap")] = value&255
                                index += 1
                        outer = rounds&3
                        if program.family in ("local_dest", "local_both"):
                            buffer[128:128+index] = (local[:index] if program.family == "local_both" else
                                                     buffer[src:src+index])
                            outer = index
                        total += buffer[dst]
                        total += ((n*257+rounds)&65535 if program.family.endswith("fill") else buffer[src])
                        if program.family == "partial_owner":
                            total += (n+rounds)&65535
                        expected = (total+index+outer)&65535
                        address = image.symbols["ra_buffer"]
                        parameters = [n, rounds, address+dst,
                                      (n*257+rounds)&65535 if program.family.endswith("fill") else address+src]
                        result = execute(image.rom, image.symbols[program.name], parameters, program.widths,
                                         initial, address, 2, image.transfers, instruction_limit=200000,
                                         frame_bytes=1024)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"],
                                          result["unmapped_instructions"]), (expected, buffer, [], 0),
                                         (policy, mode, program.name, n, rounds, dst, src))
                        calls += 1
        self.assertGreater(selected, 0)
        print(f"Nested copy/fill oracle executions: {calls}; selected shape/modes: {selected}", flush=True)

    def test_loop_header_control_boundaries(self):
        source = """
u8 ra_buffer[128];
u16 loop_control_label(u16 a,u16 b,u16 c) {
  u8 count=b&15;
  u16 total=c;
  u16 i;
  for(i=0;i<count;i++) {
body:
    if(a&1) {
      a^=1;
      if(c&1) { goto again; }
again:
      goto body;
    }
    total+=a+i;
  }
  ra_buffer[0]=i; ra_buffer[1]=i>>8;
  return total+i;
}
void main(void) {}
"""
        program = Program("loop_control_label", "control", 0, 0, [2, 2, 2], 2, [], source)
        calls = 0
        for policy in ("inline", "shared"):
            images = compile_generated(self.output / "loop-control-label" / policy,
                                       os.environ.get("SAMESAMECC") or "samesamecc",
                                       os.environ.get("SAMESAMECL") or "samesamecl", [program], source,
                                       linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for a in (0, 1, 127, 128, 255, 256, 32768, 65535):
                for b in (0, 1, 2, 3, 7, 15, 255, 65535):
                    count = b&15
                    c = (a*37+b)&65535
                    expected = (c+count*((a&~1) if count else a)+count*(count-1)//2+count)&65535
                    initial = bytes((i*37+a+b+c)&255 for i in range(128))
                    buffer = bytearray(initial)
                    buffer[:2] = count.to_bytes(2, "little")
                    for mode, image in images.items():
                        result = execute(image.rom, image.symbols[program.name], [a, b, c], [2, 2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"],
                                          result["unmapped_instructions"]), (expected, buffer, [], 0),
                                         (policy, mode, a, b, c))
                        calls += 1
        self.assertEqual(calls, 640)
        print(f"Noncanonical loop-control oracle executions: {calls}", flush=True)

    def test_proven_loop_header_frame_addresses(self):
        forms = {
            "plain": "total+=a+i;",
            "global": "total+=a+i; ra_buffer[32+(i&31)]=total;",
            "diamond": "if(i&1) { total+=a+i; } else { total^=c; }",
            "continue": "if(i&1) { continue; } total+=a+i;",
            "break": "if(i==3) { break; } total+=a+i;",
            "return": "if(i==2) { return total+i+workspace[LAST]; } total+=a+i;",
            "nested": "for(j=0;j<3;j++) { total+=a+i+j; }",
            "pressure": "total+=a+i; total+=x+y+z;",
            "pair": "total+=saved*3; total^=i+c;",
            "call": "total=loop_cache_touch(total+a+i);",
            "asm": '__asm("LD IX,1234"); total+=a+i;',
            "escape": "total+=a+i;",
            "overwrite": "if(i==2) { i+=2; } total+=a+i;",
            "entry": "",
            "byte_counter": "total+=a+i;",
            "signed_counter": "total+=a+i;",
            "word_bound": "total+=a+i;",
        }
        programs = []
        for size in (8, 104, 136, 260):
            for family, body in forms.items():
                name = f"loop_frame_{size}_{family}"
                kind = "u8" if family == "byte_counter" else "s16" if family == "signed_counter" else "u16"
                prefix = (f"u8 workspace[{size}]; {'u16' if family == 'word_bound' else 'u8'} "
                          f"count=b&{511 if family == 'word_bound' else 15}; "
                          f"u16 total=c; {kind} i; u16 j; workspace[{size-1}]=a; ")
                if family == "pressure":
                    prefix += "u16 x=a+c; u16 y=a^c; u16 z=a*257; "
                elif family == "pair":
                    prefix += "u16 saved=a+c; "
                elif family == "escape":
                    prefix += "u16 *pointer=&i; "
                loop = "for(i=0;i<count;i++) { " + body.replace("LAST", str(size-1)) + " } "
                if family == "entry":
                    loop = (f"i=0; if(a&1) {{ goto {name}_step; }} {name}_head: "
                            f"if(i>=count) {{ goto {name}_done; }} total+=a+i; "
                            f"{name}_step: i++; goto {name}_head; {name}_done: ")
                tail = ("total+=pointer[0]; " if family == "escape" else "")
                tail += (f"ra_buffer[0]=i; ra_buffer[1]=i>>8; ra_buffer[2]=total; ra_buffer[3]=total>>8; "
                         f"return total+i+workspace[{size-1}];")
                programs.append(Program(name, family, size, 0, [2, 2, 2], 2, [],
                                        f"u16 {name}(u16 a,u16 b,u16 c) {{ {prefix}{loop}{tail} }}"))

        def oracle(program, a, b, c, initial):
            count = b & (511 if program.family == "word_bound" else 15)
            total = c
            index = int(bool(a&1)) if program.family == "entry" else 0
            buffer = bytearray(initial)
            while index < count:
                if program.family == "break" and index == 3:
                    break
                if program.family == "return" and index == 2:
                    return (total+index+(a&255))&65535, buffer
                if program.family == "overwrite" and index == 2:
                    index += 2
                if program.family == "continue" and index&1:
                    index += 1
                    continue
                if program.family == "diamond":
                    total = total+a+index if index&1 else total^c
                elif program.family == "nested":
                    total += sum(a+index+j for j in range(3))
                elif program.family == "pressure":
                    total += a+index+((a+c)&65535)+(a^c)+((a*257)&65535)
                elif program.family == "pair":
                    total = ((total+((a+c)&65535)*3)&65535)^((index+c)&65535)
                elif program.family == "call":
                    value = (total+a+index)&65535
                    buffer[127] ^= value&255
                    total = value+1
                else:
                    total += a+index
                total &= 65535
                if program.family == "global":
                    buffer[32+(index&31)] = total&255
                index += 1
            if program.family == "escape":
                total = (total+index)&65535
            buffer[:4] = index.to_bytes(2, "little")+total.to_bytes(2, "little")
            return (total+index+(a&255))&65535, buffer

        calls = selected = 0
        for policy in ("inline", "shared"):
            for offset in range(0, len(programs), 8):
                group = programs[offset:offset+8]
                directory = self.output / "loop-header-frames" / policy / f"batch-{offset}"
                source = ("u8 ra_buffer[128];\nu16 loop_cache_touch(u16 value) { "
                          "ra_buffer[127]^=value; return value+1; }\n"
                          + "\n".join(p.source for p in group) + "\nvoid main(void) {}\n")
                images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                           os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                                           linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                for mode, image in images.items():
                    assembly = (directory / mode / "main.asm").read_text()
                    for program in group:
                        body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        reused = "; proven loop-header frame address IX = DE" in body
                        frame = int(re.search(r"allocate space for the stack frame bytes=(\d+)", body)[1])
                        if not mode.startswith("ra") or program.family in (
                                "call", "asm", "escape", "overwrite", "entry", "byte_counter", "signed_counter"):
                            self.assertFalse(reused, (policy, mode, program.name, frame))
                        elif program.family != "word_bound":
                            self.assertTrue(reused, (policy, mode, program.name, frame))
                        selected += reused
                        for a in (0, 1, 127, 128, 255, 256, 32768, 65535):
                            for b in (0, 1, 2, 3, 7, 15, 255, 256, 511, 512, 65535):
                                c = (a*37+b)&65535
                                initial = bytes((i*37+a+b+c)&255 for i in range(128))
                                value, buffer = oracle(program, a, b, c, initial)
                                result = execute(image.rom, image.symbols[program.name], [a, b, c], [2, 2, 2],
                                                 initial, image.symbols["ra_buffer"], 2, image.transfers,
                                                 instruction_limit=100000 if program.family == "word_bound" else 20000,
                                                 frame_bytes=1024)
                                self.assertEqual((result["value"], result["buffer"], result["port_writes"],
                                                  result["unmapped_instructions"]), (value, buffer, [], 0),
                                                 (policy, mode, program.name, a, b, c))
                                calls += 1
        self.assertGreater(selected, 0)
        self.assertEqual(calls, 59840)
        print(f"Loop-header frame oracle executions: {calls}; selected shapes: {selected}", flush=True)

    def test_large_frame_loop_windows(self):
        forms = {
            "stable": "total+=a+i;",
            "diamond": "if(i&1) { total+=a+i; } else { total^=c; }",
            "nested": "for(j=0;j<3;j++) { total+=a+i+j; }",
            "continue": "if(i&1) { continue; } total+=a+i;",
            "call": "total=large_window_touch(total+a+i,0);",
            "recursive": "total=large_window_touch(total+a+i,2);",
            "asm": '__asm("LD IX,4660"); total+=a+i;',
            "escape": "total+=a+i;",
            "overwrite": "if(i==2) { i+=2; } total+=a+i;",
            "far": "far+=a+i; total+=far;",
            "cache": "total+=a+i;",
            "deep": "",
            "late_entry": "",
            "latches": "",
            "ports": "total+=a+i; __z80_out[0xBE]=total;",
        }
        programs = []
        for size in (116, 128, 136, 224, 240, 256, 512, 1024):
            for family, body in forms.items():
                name = f"large_window_{size}_{family}"
                prefix = "u16 total=c; u8 count=b&7; u16 i,j; "
                if family == "deep":
                    prefix += "u16 k0,k1,k2,k3,k4,k5,k6,k7,k8,k9; "
                prefix += f"u8 workspace[{size}]; workspace[{size-1}]=a; "
                if family == "escape":
                    prefix += "u16 *pointer=&i; "
                elif family == "far":
                    prefix += "u16 far=a; large_window_escape(&far); "
                elif family == "cache":
                    prefix += " ".join(
                        f"if(a&{1<<bit}) {{ ra_buffer[{32+bit}]=a+{bit}; }} else {{ ra_buffer[{32+bit}]=c+{bit}; }}"
                        for bit in range(10))
                elif family == "late_entry":
                    prefix += "ra_buffer[120]=a&1; "
                loop = "for(i=0;i<count;i++) { " + body + " } "
                if family == "deep":
                    loop = (" ".join(f"for(k{level}=0;k{level}<{2 if level in (0, 9) else 1};k{level}++) {{"
                                     for level in range(10))
                            + " ".join(f"if(a&{1<<bit}) {{ ra_buffer[{42+bit}]=a; }} "
                                       f"else {{ ra_buffer[{42+bit}]=c; }}" for bit in range(10))
                            + " total+=a; " + "} " * 10 + "i=0; ")
                elif family in ("late_entry", "latches"):
                    loop = (f"i=0; {name}_head: if(i>=count) {{ goto {name}_done; }} "
                            + (f"if(i&1) {{ total^=c; i++; goto {name}_head; }} " if family == "latches" else "")
                            + f"total+=a+i; i++; goto {name}_head; {name}_done: "
                            + (f"if(ra_buffer[120]!=0) {{ ra_buffer[120]=0; goto {name}_head; }} "
                               if family == "late_entry" else ""))
                tail = ("total+=pointer[0]; " if family == "escape" else "")
                tail += (f"ra_buffer[0]=i; ra_buffer[1]=i>>8; ra_buffer[2]=total; ra_buffer[3]=total>>8; "
                         f"return total+i+workspace[{size-1}];")
                programs.append(Program(name, family, size, 0, [2, 2, 2], 2, [],
                                        f"u16 {name}(u16 a,u16 b,u16 c) {{ {prefix}{loop}{tail} }}"))

        calls = selected = restored = 0
        for policy in ("inline", "shared"):
            for offset in range(0, len(programs), 8):
                group = programs[offset:offset+8]
                directory = self.output / "large-frame-loops" / policy / f"batch-{offset}"
                source = ("u8 ra_buffer[128];\nvoid large_window_escape(u16 *p) { p[0]^=3; }\n"
                          "u16 large_window_touch(u16 value,u8 depth) { ra_buffer[127]^=value; "
                          "if(depth!=0) { return large_window_touch(value+1,depth-1)+1; } return value+1; }\n"
                          + "\n".join(p.source for p in group) + "\nvoid main(void) {}\n")
                images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                           os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                                           linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                for mode, image in images.items():
                    assembly = (directory / mode / "main.asm").read_text()
                    for program in group:
                        section = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        frame = int(re.search(r"allocate space for the stack frame bytes=(\d+)", section)[1])
                        reused = section.count("; proven loop-header frame address IX = DE")
                        restored += section.count("; restore proven loop-latch frame address IX = DE")
                        if not mode.startswith("ra") or program.family in (
                                "call", "recursive", "asm", "escape", "overwrite", "late_entry", "latches"
                        ):
                            self.assertEqual(reused, 0, (policy, mode, program.name))
                        elif program.family in ("stable", "diamond", "nested", "continue", "ports", "cache"):
                            self.assertGreater(reused, 0, (policy, mode, program.name))
                        elif program.family == "deep" and frame > 128:
                            self.assertEqual(reused, 8, (policy, mode, program.name))
                        selected += reused
                        for a in (0, 1, 128, 255, 256, 32768, 65535):
                            for b in (0, 1, 2, 3, 7, 8, 65535):
                                c = (a*37+b)&65535
                                total, index, far = c, 0, a^3
                                expected = bytearray((p*37+a+b+c)&255 for p in range(128))
                                initial = bytes(expected)
                                ports = []
                                if program.family == "cache":
                                    for bit in range(10):
                                        expected[32+bit] = ((a if a&(1<<bit) else c)+bit)&255
                                elif program.family == "late_entry":
                                    expected[120] = 0
                                if program.family == "deep":
                                    total = (total+a*4)&65535
                                    for bit in range(10):
                                        expected[42+bit] = (a if a&(1<<bit) else c)&255
                                else:
                                    while index < (b&7):
                                        if program.family == "overwrite" and index == 2:
                                            index += 2
                                        if program.family == "continue" and index&1:
                                            index += 1
                                            continue
                                        if ((program.family == "diamond" and not index&1) or
                                                (program.family == "latches" and index&1)):
                                            total ^= c
                                        elif program.family == "nested":
                                            total += sum(a+index+j for j in range(3))
                                        elif program.family in ("call", "recursive"):
                                            value = (total+a+index)&65535
                                            depth = 2 if program.family == "recursive" else 0
                                            for level in range(depth+1):
                                                expected[127] ^= (value+level)&255
                                            total = value+depth*2+1
                                        elif program.family == "far":
                                            far = (far+a+index)&65535
                                            total += far
                                        else:
                                            total += a+index
                                        total &= 65535
                                        if program.family == "ports":
                                            ports.append([0xBE, total&255])
                                        index += 1
                                if program.family == "escape":
                                    total = (total+index)&65535
                                expected[:4] = index.to_bytes(2, "little")+total.to_bytes(2, "little")
                                result = execute(image.rom, image.symbols[program.name], [a, b, c], program.widths,
                                                 initial, image.symbols["ra_buffer"], 2, image.transfers,
                                                 ports_allowed=program.family == "ports",
                                                 frame_bytes=2048)
                                self.assertEqual((result["value"], result["buffer"], result["port_writes"],
                                                  result["unmapped_instructions"]),
                                                 ((total+index+(a&255))&65535, expected, ports, 0),
                                                 (policy, mode, program.name, a, b, c))
                                calls += 1
        self.assertGreater(selected, 0)
        self.assertGreater(restored, 0)
        self.assertEqual(calls, 58800)
        print(f"Large-frame loop executions: {calls}; headers: {selected}; latch restores: {restored}", flush=True)

    def test_exact_local_array_forwarding(self):
        forms = {
            "direct": "data[index]=a+c; result=data[index];",
            "constant": "data[3]=a+c; result=data[3];",
            "repeat": "data[index]=a; data[index]=a+c; result=data[index];",
            "wide": "data[index]=a*257+c; result=data[index];",
            "changed_index": "data[index]=a+c; index=(index+1)&3; result=data[index];",
            "changed_value": "data[index]=a+c; a^=b; result=data[index]+a;",
            "other": "data[index]=a+c; other[index]=b; result=data[index]; result+=other[index];",
            "call": "data[index]=a+c; b=array_forward_touch(b); result=data[index]+b;",
            "asm": 'data[index]=a+c; __asm("NOP"); result=data[index];',
            "escape": "pointer=data; data[index]=a+c; result=data[index]; result+=pointer[(index+1)&3];",
            "partial": "byte_pointer=data; data[index]=a+c; byte_pointer[index*WIDTH]=b; result=data[index]; result+=byte_pointer[index*WIDTH];",
            "branch": "data[index]=a+c; if(b&4) { data[index]=a; } result=data[index];",
            "loop": "result=0; for(i=0;i<(b&3);i++) { data[index]=a+i+c; result+=data[index]; }",
            "pressure": "u16 left=a+b; u16 right=b+c; u16 extra=a^c; data[index]=a+c; result=data[index]+left+right+extra;",
            "far": "workspace[259]=a; data[index]=a+c; result=data[index]; result+=workspace[259];",
            "byte_index": "u8 small=b&3; data[small]=a+c; result=data[small];",
            "live_index": "data[index]=a+c; result=data[index]+index;",
            "temporary_index": "data[b&3]=a+c; result=data[b&3];",
            "global": "ra_buffer[index]=a; result=ra_buffer[index];",
        }
        programs = []
        for kind in ("u8", "u16", "s8", "s16"):
            width = 1 if kind.endswith("8") else 2
            for name, body in forms.items():
                source = (f"u16 array_forward_{kind}_{name}(u16 a,u16 b,u16 c) {{ "
                          + ("u8 workspace[260]; " if name == "far" else "") +
                          f"{kind} data[4]; {kind} other[4]; {kind} *pointer; u8 *byte_pointer; "
                          "u16 index; u16 i; u16 result; "
                          "data[0]=10; data[1]=20; data[2]=30; data[3]=40; " +
                          ("index=b&3; " if name not in ("constant", "byte_index", "temporary_index") else "") +
                          body.replace("WIDTH", str(width)) +
                          f" for(i=0;i<4;i++) {{ ra_buffer[i*{width}]=data[i]; " +
                          (f"ra_buffer[i*{width}+1]=data[i]>>8;" if width == 2 else "") +
                          " } return result; }")
                programs.append(Program(f"array_forward_{kind}_{name}", name, width, int(kind.startswith("s")),
                                        [2, 2, 2], 2, [], source))

        def oracle(program, a, b, c, initial):
            data = [10, 20, 30, 40]
            index = b&3
            mask = 255 if program.variant == 1 else 65535
            value = (a*257+c if program.family == "wide" else a+c)&mask
            if program.family == "constant":
                index = 3
            if program.family not in ("loop", "global"):
                data[index] = value
            if program.family == "changed_index":
                index = (index+1)&3
            if program.family == "partial":
                data[index] = (data[index]&0xff00 if program.variant == 2 else 0)|(b&255)
            if program.family == "branch" and b&4:
                data[index] = a&mask

            def extend(value):
                return (value-256 if program.constant and program.variant == 1 and value&128 else value)&65535

            result = extend(data[index])
            if program.family == "loop":
                result = 0
                for i in range(b&3):
                    data[index] = (a+i+c)&mask
                    result += extend(data[index])
            if program.family == "global":
                result = a&255
            if program.family == "changed_value":
                result += a^b
            if program.family == "other":
                result += extend(b&mask)
            if program.family == "escape":
                result += extend(data[(index+1)&3])
            if program.family == "partial":
                result += b&255
            if program.family == "far":
                result += a&255
            if program.family == "live_index":
                result += index
            if program.family == "call":
                result += (b+1)&65535
            if program.family == "pressure":
                result += ((a+b)&65535)+((b+c)&65535)+(a^c)
            expected_buffer = bytearray(initial)
            for i, value in enumerate(data):
                expected_buffer[i*program.variant:(i+1)*program.variant] = value.to_bytes(program.variant, "little")
            if program.family == "call":
                expected_buffer[127] ^= b&255
            return result&65535, expected_buffer

        calls = 0
        for policy in ("inline", "shared"):
            for offset in range(0, len(programs), 15):
                group = programs[offset:offset+15]
                directory = self.output / "array-forwarding" / policy / f"batch-{offset}"
                source = ("u8 ra_buffer[128];\nu16 array_forward_touch(u16 value) { ra_buffer[127]^=value; return value+1; }\n" +
                          "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
                images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                           os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                                           linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                for mode, image in images.items():
                    assembly = (directory / mode / "main.asm").read_text()
                    for program in group:
                        body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        selected = "preceding local-array" in body
                        if program.constant or not mode.startswith("ra") or program.family in (
                                "changed_index", "changed_value", "other", "call", "asm", "escape", "partial", "branch", "global"):
                            self.assertFalse(selected, (policy, mode, program.name))
                        elif program.family in ("direct", "constant", "repeat", "wide"):
                            self.assertTrue(selected, (policy, mode, program.name))
                        for a in (0, 1, 127, 128, 255, 256, 32767, 32768, 65535):
                            for b in (0, 1, 2, 3, 4, 7, 255, 65535):
                                c = (a*37+b)&65535
                                initial = bytes((i*37+a+b+c)&255 for i in range(128))
                                expected, buffer = oracle(program, a, b, c, initial)
                                result = execute(image.rom, image.symbols[program.name], [a, b, c], [2, 2, 2],
                                                 initial, image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                                self.assertEqual((result["value"], result["buffer"], result["port_writes"],
                                                  result["unmapped_instructions"]),
                                                 (expected, buffer, [], 0), (policy, mode, program.name, a, b, c))
                                calls += 1
        print(f"Exact local-array forwarding oracle executions: {calls}", flush=True)

    def test_complementary_constant_word_shifts(self):
        from random import Random
        from z80 import Z80Machine
        from ra_cpu import step

        counts = list(range(18)) + [n for n in BYTE_SHIFT_COUNTS if n not in range(18)] + [257, 65535]
        cases = {}
        for kind, operator in (("u16", ">>"), ("u16", "<<"), ("s16", ">>")):
            for count in counts:
                name = f"{kind}_{'right' if operator == '>>' else 'left'}_{count}".replace("-", "minus")
                effective = count & 65535
                cases[name] = (kind, f"return a{operator}({count});",
                               lambda a, b, c, n=effective, op=operator:
                               0 if n >= 16 else a >> n if op == ">>" else a << n,
                               kind == "u16" and operator == ">>" and count in (6, 7))
        for count in (6, 7):
            cases.update({
                f"byte_{count}": ("u8", f"return a>>{count};", lambda a, b, c, n=count: (a&255)>>n, False),
                f"signed_byte_{count}": ("s8", f"return a>>{count};",
                                        lambda a, b, c, n=count: ((a&255)-(256 if a&128 else 0)&65535)>>n, False),
                f"narrow_{count}": ("u16", f"u8 value=a>>{count}; return value;",
                                   lambda a, b, c, n=count: (a>>n)&255, True),
                f"pressure_{count}": ("u16", f"u16 live=a+b; u8 byte=b^c; u16 value=a>>{count}; "
                                     "return value+live+byte+c;",
                                     lambda a, b, c, n=count: (a>>n)+((a+b)&65535)+((b^c)&255)+c, True),
                f"branch_{count}": ("u16", f"if(b&1) {{ return (a>>{count})+c; }} return a+c;",
                                   lambda a, b, c, n=count: (a>>n if b&1 else a)+c, True),
                f"loop_{count}": ("u16", f"u16 i,result=c; for(i=0;i<(b&3);i++) {{ result+=(a+i)>>{count}; }} "
                                 "return result;",
                                 lambda a, b, c, n=count: c+sum(((a+i)&65535)>>n for i in range(b&3)), True),
                f"alias_{count}": ("u16", f"u16 *p=&a; p[0]^=b; return (a>>{count})+c;",
                                  lambda a, b, c, n=count: ((a^b)>>n)+c, True),
                f"call_{count}": ("u16", f"u16 value=a>>{count}; return shift_touch(b)+value+c;",
                                 lambda a, b, c, n=count: (a>>n)+b+c, True),
            })
        programs = [Program(f"constant_shift_caller_{name}", name, 0, 0, [2, 2, 2], 2, [],
                            f"u16 constant_shift_caller_{name}(u16 a,u16 b,u16 c) {{ u16 saved=a^b^c; "
                            f"return constant_shift_leaf_{name}(a,b,c)^saved; }}")
                    for name in cases]
        random = Random(0x2A0)
        inputs = [(a, b, (a*37+b)&65535) for a, b in (
            (0, 0), (1, 1), (127, 255), (128, 256), (255, 257), (256, 32768),
            (32767, 65535), (32768, 32767), (65535, 3))]
        inputs += [tuple(random.randrange(65536) for _ in range(3)) for _ in range(12)]
        calls = 0
        for policy in ("inline", "shared"):
            for offset in range(0, len(programs), 24):
                group = programs[offset:offset+24]
                directory = self.output / "constant-word-shifts" / policy / f"batch-{offset}"
                source = ("u8 ra_buffer[128];\nu16 shift_touch(u16 a) { ra_buffer[0]^=a; return a; }\n" +
                          "\n".join(f"u16 constant_shift_leaf_{p.family}({cases[p.family][0]} a,u16 b,u16 c) "
                                    f"{{ {cases[p.family][1]} }}" for p in group) +
                          "\n" + "\n".join(p.source for p in group) + "\nvoid main(void) {}\n")
                images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                           os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                                           linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                for mode, image in images.items():
                    assembly = (directory / mode / "main.asm").read_text()
                    for program in group:
                        body = assembly.split(f'.SECTION "constant_shift_leaf_{program.family}" FREE')[1].split(".ENDS")[0]
                        self.assertEqual("complementary constant word right shift" in body, cases[program.family][3],
                                         (policy, mode, program.family))
                        for a, b, c in inputs:
                            initial = bytes((index*37+a+b+c)&255 for index in range(128))
                            expected_buffer = bytearray(initial)
                            if program.family.startswith("call_"):
                                expected_buffer[0] ^= b&255
                            expected = (cases[program.family][2](a, b, c)&65535)^a^b^c
                            result = execute(image.rom, image.symbols[program.name], [a, b, c], [2, 2, 2],
                                             initial, image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"],
                                              result["unmapped_instructions"]),
                                             (expected, expected_buffer, [], 0), (policy, mode, program.family, a, b, c))
                            calls += 1
            native = [Program(f"shift_native_caller_{n}", "native", n, 0, [2], 2, [],
                              f"u16 shift_native_caller_{n}(u16 a) {{ return shift_native_leaf_{n}(a); }}")
                      for n in (6, 7)]
            source = ("u8 ra_buffer[128];\n" +
                      "\n".join(f"u16 shift_native_leaf_{n}(u16 a) {{ return a>>{n}; }}" for n in (6, 7)) +
                      "\n" + "\n".join(p.source for p in native) + "\nvoid main(void) {}\n")
            images = compile_generated(self.output / "constant-word-shifts" / policy / "native",
                                       os.environ.get("SAMESAMECC") or "samesamecc",
                                       os.environ.get("SAMESAMECL") or "samesamecl", native, source,
                                       linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for program in native:
                count = program.variant
                kernel = bytes([0x69, 0x60, 0xaf] + [0x29, 0x17]*(8-count) + [0x6c, 0x67, 0xc9])
                for mode, image in images.items():
                    entry = image.symbols[f"ssc.regleaf.shift_native_leaf_{count}"]
                    self.assertEqual(image.rom[entry:entry+len(kernel)], kernel, (policy, mode, count))
                    self.assertEqual(image.staticMetrics["functions"][f"shift_native_leaf_{count}"]["rom_bytes"],
                                     len(kernel))
                image = images["ra"]
                machine = Z80Machine()
                machine.memory[:len(image.rom)] = image.rom
                machine.memory[0xdf00:0xdf02] = b"\xff\x7f"
                for value in range(65536):
                    machine.bc, machine.hl, machine.af = value, value ^ 0xffff, (value*37)&65535
                    machine.de, machine.ix, machine.iy, machine.sp = 0x1234, 0x9876, 0x5432, 0xdf00
                    machine.pc = image.symbols[f"ssc.regleaf.shift_native_leaf_{count}"]
                    cycles = 0
                    for _ in range(4 + 2*(8-count) + 2):
                        cycles += step(machine)
                    self.assertEqual((machine.hl, machine.bc, machine.de, machine.ix, machine.iy, machine.sp, machine.pc),
                                     (value>>count, value, 0x1234, 0x9876, 0x5432, 0xdf02, 0x7fff),
                                     (policy, count, value))
                    self.assertEqual(cycles, 60 if count == 6 else 45)
        print(f"Constant word shifts: {calls} guarded calls and 262144 exhaustive native leaf executions", flush=True)

    def test_frameless_multiply_leaves(self):
        from random import Random

        cases = {}
        names = ("a", "b", "c")
        for first in range(3):
            for second in range(3):
                expression = f"{names[first]}*{names[second]}"
                cases[f"product_{first}_{second}"] = (
                    "u16 a,u16 b,u16 c", f"return {expression};",
                    lambda values, initial, f=first, s=second: values[f]*values[s], True)
                for third in range(3):
                    for reverse in (False, True):
                        body = f"{names[third]}+({expression})" if reverse else f"({expression})+{names[third]}"
                        cases[f"sum_{first}_{second}_{third}_{int(reverse)}"] = (
                            "u16 a,u16 b,u16 c", f"return {body};",
                            lambda values, initial, f=first, s=second, t=third: values[f]*values[s]+values[t], True)
        for factor in (3, 5, 7, 17, 255, 257, 0x1234, 65535):
            for first in range(3):
                for reverse in (False, True):
                    product = f"{factor}*{names[first]}" if reverse else f"{names[first]}*{factor}"
                    cases[f"constant_{factor}_{first}_{int(reverse)}"] = (
                        "u16 a,u16 b,u16 c", f"return ({product})+{names[(first+1)%3]};",
                        lambda values, initial, k=factor, f=first: values[f]*k+values[(f+1)%3], True)
        cases.update({
            "constant_sum": ("u16 a,u16 b,u16 c", "return a*b+0x9234;",
                             lambda v, initial: v[0]*v[1]+0x9234, True),
            "signed": ("s16 a,u16 b,u16 c", "return a*b+c;", lambda v, initial: v[0]*v[1]+v[2], False),
            "byte": ("u8 a,u16 b,u16 c", "return a*b+c;", lambda v, initial: (v[0]&255)*v[1]+v[2], False),
            "signed_byte": ("s8 a,u16 b,u16 c", "return a*b+c;",
                            lambda v, initial: ((v[0]&255)-(256 if v[0]&128 else 0))*v[1]+v[2], False),
            "narrow": ("u16 a,u16 b,u16 c", "u8 value=a*b; return value+c;",
                       lambda v, initial: (v[0]*v[1]&255)+v[2], False),
            "pressure": ("u16 a,u16 b,u16 c", "return a*b+b*c;", lambda v, initial: v[0]*v[1]+v[1]*v[2], False),
            "alias": ("u16 a,u16 b,u16 c", "u16 *p=&a; p[0]^=c; return a*b+c;",
                      lambda v, initial: (v[0]^v[2])*v[1]+v[2], False),
            "memory": ("u16 a,u16 b,u16 c", "return a*ra_buffer[3]+c;",
                       lambda v, initial: v[0]*initial[3]+v[2], False),
            "branch": ("u16 a,u16 b,u16 c", "if(a&1) { return a*b+c; } return b*c+a;",
                       lambda v, initial: v[0]*v[1]+v[2] if v[0]&1 else v[1]*v[2]+v[0], False),
        })
        programs = [Program(f"multiply_caller_{name}", name, 0, 0, [2, 2, 2], 2, [],
                            f"u16 multiply_caller_{name}(u16 a,u16 b,u16 c) {{ u16 saved=a^b^c; "
                            f"return multiply_leaf_{name}(a,b,c)^saved; }}")
                    for name in cases]
        random = Random(0x2A0)
        inputs = [(a, b, c) for a, b, c in (
            (0, 0, 0), (0, 65535, 0x8000), (1, 1, 1), (1, 65535, 65535),
            (127, 128, 255), (255, 256, 257), (0x8000, 1, 65535),
            (65535, 0x8000, 0x8000), (65535, 65535, 65535))]
        inputs += [tuple(random.randrange(65536) for _ in range(3)) for _ in range(12)]
        extra = [tuple(random.randrange(65536) for _ in range(3)) for _ in range(793)]
        directory = self.output / "multiply-leaves"
        for policy, offset in ((policy, offset) for policy in ("inline", "shared")
                               for offset in range(0, len(programs), 24)):
            group = programs[offset:offset+24]
            helpers = "u8 ra_buffer[128];\n"+"\n".join(
                f"u16 multiply_leaf_{p.family}({cases[p.family][0]}) {{ {cases[p.family][1]} }}" for p in group)
            images = compile_generated(
                directory / policy / f"batch-{offset}", os.environ.get("SAMESAMECC") or "samesamecc",
                os.environ.get("SAMESAMECL") or "samesamecl", group,
                helpers+"\n"+"\n".join(p.source for p in group)+"\nvoid main(void) {}\n",
                linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for mode, image in images.items():
                for program in group:
                    eligible = cases[program.family][3]
                    self.assertEqual(f"ssc.regleaf.multiply_leaf_{program.family}" in image.symbols,
                                     eligible, (policy, mode, program.family))
                    if eligible:
                        self.assertEqual(image.staticMetrics["functions"][f"multiply_leaf_{program.family}"]["frame_bytes"], 0)
                    for values in inputs+(extra if program.family == "sum_0_1_2_0" else []):
                        initial = bytes((index*37+sum(values))&255 for index in range(128))
                        expected = (cases[program.family][2](values, initial)&65535)^values[0]^values[1]^values[2]
                        result = execute(image.rom, image.symbols[program.name], values, [2, 2, 2], initial,
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected, initial, []), (policy, mode, program.family, values))
                        self.assertEqual(result["unmapped_instructions"], 0)

    def test_frameless_multiply_objects(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        for contract in ("native", "banked", "old", "escape", "mixed", "opaque", "explicit", "explicit_opaque"):
            for mode, flags in (("default", ()), ("ra", ("-ra",)), ("forced", ("-ra-all-spill",))):
                work = self.output / f"multiply-objects-{contract}-{mode}"
                work.mkdir()
                annotation = " __z80_regcall" if contract.startswith("explicit") else ""
                barrier = '__asm("LD BC,4660");' if contract in ("opaque", "explicit_opaque") else ""
                escape = "u16 address=&multiply_object;" if contract in ("escape", "mixed") else ""
                value = ("multiply_object(a,b,c)^multiply_object(c,a,b)" if contract == "mixed" else
                         "multiply_object(a,b,c)")
                adjustment = "+(address!=0)" if escape else ""
                sources = {
                    "callee": f"u16 multiply_object(u16 a,u16 b,u16 c){annotation} {{ return a*b+c; }}\n",
                    "caller": ("u8 ra_buffer[128];\n"
                               f"u16 multiply_object(u16 a,u16 b,u16 c){annotation};\n"
                               f"u16 multiply_boundary(u16 a,u16 b,u16 c) {{ {escape} u16 saved=a^b^c; "
                               f"{barrier} return (({value})^saved){adjustment}; }}\nvoid main(void) {{}}\n"),
                }
                for unit, source in sources.items():
                    (work / f"{unit}.ssc").write_text(source)
                    bank = ("-b", "3", "-s", "2") if contract == "banked" and unit == "callee" else ()
                    run(compiler, ["-mz80", "-rs", "3", *bank, *flags, "-o", f"{unit}.asm", f"{unit}.ssc"],
                        work, f"{unit}.compiler.log")
                if contract == "old":
                    path = work / "callee.asm"
                    path.write_text(re.sub(r"^\s*; ssc_regcalls[^\n]*\n", "", path.read_text(), flags=re.M))
                run(linker, ["-lsms", "-ro", "4", "-share-arithmetic", "-v", "-o", "linked.sms",
                             "caller.asm", "callee.asm"], work, "linker.log")
                image = read_image(work)
                selected = contract in ("native", "banked", "explicit", "explicit_opaque")
                self.assertEqual("ssc.regleaf.multiply_object" in image.symbols, selected, (contract, mode))
                self.assertFalse(any(name.startswith("ssc.arithmetic_") for name in image.symbols))
                if contract == "banked":
                    image = mapped_routine_image("multiply", "ssc.autocall.v1.w3.multiply_object", mode, image)
                for a, b, c in ((0, 0, 0), (1, 65535, 0x8000), (127, 128, 255), (255, 256, 257),
                                (0x8000, 0x8000, 1), (65535, 65535, 65535)):
                    expected = (a*b+c)&65535
                    if contract == "mixed":
                        expected ^= (c*a+b)&65535
                    expected ^= a^b^c
                    expected = (expected+bool(escape))&65535
                    initial = bytes((i*37+a+b+c)&255 for i in range(128))
                    result = execute(image.rom, image.symbols["multiply_boundary"], [a, b, c], [2, 2, 2],
                                     initial, image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                    self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                     (expected, initial, []), (contract, mode, a, b, c))
                    self.assertEqual(result["unmapped_instructions"], 0)

    def test_costed_loop_invariant_hoisting(self):
        cases = {
            "bias": ("4", "(a>>pass)&31", "", "", True),
            "scaled": ("4", "((a>>pass)&31)<<3", "", "", True),
            "constant": ("4", "(a>>8)&31", "", "", True),
            "high": ("4", "a>>8", "", "", True),
            "counter_scaled": ("4", "((pass>>2)&15)<<3", "", "", True),
            "calls": ("4", "(a>>pass)&31", "", "value=lh_read(value);", True),
            "early": ("4", "(a>>pass)&31", "", "if(i==2 && (b&1)) { return total; }", False),
            "signed": ("4", "(input>>pass)&31", "s16 input=a;", "", False),
            "changing": ("4", "(a>>pass)&31", "", "a^=i+pass;", False),
            "escaped": ("4", "(a>>pass)&31", "", "lh_change(&a,a^(i+pass));", False),
            "unknown_shift": ("4", "(a>>b)&31", "", "", False),
            "effectful": ("4", "(lh_read(a)>>pass)&31", "", "", False),
            "wide": ("4", "(a>>pass)&511", "", "", False),
            "leaf": ("4", "(a>>pass)&31", "", "", False),
            "explicit": ("4", "(a>>pass)&31", "", "", False),
            "one": ("1", "(a>>pass)&31", "", "", False),
            "zero": ("0", "(a>>pass)&31", "", "", False),
            "dynamic_bound": ("b", "(a>>pass)&31", "", "", False),
            "outer_bound": ("4", "(a>>pass)&31", "", "", False),
            "latches": ("4", "(a>>pass)&31", "", "if(i==1) { i++; }", False),
            "load": ("4", "(ra_buffer[249]>>pass)&31", "", "", False),
            "short_circuit": ("4", "(a>>pass)&31", "", "", True),
            "entries": ("4", "(a>>pass)&31", "", "", False),
            "pressure": ("4", "(a>>pass)&31",
                         "u16 reserve[80]; for(i=0;i<80;i++) { reserve[i]=(a+i*13)^b; }", "", False),
        }
        helpers = """u8 ra_buffer[256];
u16 lh_read(u16 value) { ra_buffer[250]+=1; return value; }
u16 lh_change(u16 *p,u16 value) { p[0]=value; return value; }
"""
        programs = []
        for name, (bound, bias, setup, update, _) in cases.items():
            loop_entry = "i=0; if(a&1) { goto lh_entry; }" if name == "entries" else ""
            label = "lh_entry:" if name == "entries" else ""
            value = "reserve[pass*4+i]" if name == "pressure" else "ra_buffer[pass*4+i]+257"
            tail = "total" if name in ("leaf", "explicit") else "lh_read(total)"
            target = f"hoist_{name}_body" if name in ("leaf", "explicit") else f"hoist_{name}"
            annotation = " __z80_regcall" if name == "explicit" else ""
            passes = "b" if name == "outer_bound" else "12" if name == "counter_scaled" else "3"
            guard = "i>=b || lh_read(i)==2" if name == "short_circuit" else "i>=b"
            source = (
                f"u16 {target}(u16 a,u16 b){annotation} {{ "
                f"u16 pass; u16 i; u16 value; u16 total=a; {setup} "
                f"for(pass=0;pass<{passes};pass++) {{ {loop_entry} for(i=0;i<{bound};i++) {{ "
                f"{label} if({guard}) {{ continue; }} value={value}; {update} "
                f"total+=(value/6)+({bias}); }} }} return {tail}; }}"
            )
            if name in ("leaf", "explicit"):
                source += f"\nu16 hoist_{name}(u16 a,u16 b) {{ return {target}(a,b); }}"
            programs.append(Program(f"hoist_{name}", name, 0, 0, [2, 2], 2, [], source))
        directory = self.output / "loop-invariants"
        for policy in ("inline", "shared"):
            images = compile_generated(
                directory / policy, os.environ.get("SAMESAMECC") or "samesamecc",
                os.environ.get("SAMESAMECL") or "samesamecl", programs,
                helpers+"\n".join(program.source for program in programs)+"\nvoid main(void) {}\n",
                linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for mode, image in images.items():
                assembly = (directory / policy / mode / "linked.sms.combined.asm").read_text()
                for program in programs:
                    name = program.family
                    body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                    self.assertEqual("; loop invariant " in body,
                                     cases[name][4] and mode in ("ra", "ra_quiet"), (name, policy, mode))
                    if name in ("leaf", "explicit"):
                        target = assembly.split(f'.SECTION "{program.name}_body" FREE')[1].split(".ENDS")[0]
                        self.assertNotIn("; loop invariant ", target)
                        if mode in ("ra", "ra_quiet"):
                            convention = "regcall" if name == "explicit" else "autocall"
                            self.assertIn(f"ssc.{convention}.v1.w2.{program.name}_body", image.symbols)
                    for a in (0, 1, 127, 255, 256, 0x8000, 0xFFFF):
                        for b in (0, 1, 2, 3, 4, 16):
                            initial = bytes((index*37+a+b)&255 for index in range(256))
                            expected = bytearray(initial)
                            total, current, stopped = a, a, False
                            bound = b if name == "dynamic_bound" else 1 if name == "one" else 0 if name == "zero" else 4
                            passes = b if name == "outer_bound" else 12 if name == "counter_scaled" else 3
                            for outer in range(passes):
                                for inner in ((0, 1, 3) if name == "latches" else range(bound)):
                                    if inner >= b:
                                        continue
                                    if name == "short_circuit":
                                        expected[250] = (expected[250]+1)&255
                                        if inner == 2:
                                            continue
                                    if name == "early" and inner == 2 and b&1:
                                        stopped = True
                                        break
                                    if name in ("changing", "escaped"):
                                        current ^= inner+outer
                                    value = (((a+(outer*4+inner)*13)&65535)^b if name == "pressure" else
                                             initial[outer*4+inner]+257)
                                    shift = b if name == "unknown_shift" else 8 if name in ("constant", "high") else outer
                                    bias = (current >> shift)&(511 if name == "wide" else 31)
                                    if name == "high":
                                        bias = current>>8
                                    if name == "load":
                                        bias = (initial[249]>>outer)&31
                                    if name == "counter_scaled":
                                        bias = ((outer>>2)&15)<<3
                                    if name == "scaled":
                                        bias <<= 3
                                    total = (total+value//6+bias)&65535
                                    if name in ("calls", "effectful"):
                                        expected[250] = (expected[250]+1)&255
                                if stopped:
                                    break
                            if not stopped and name not in ("leaf", "explicit"):
                                expected[250] = (expected[250]+1)&255
                            result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], initial,
                                             image.symbols["ra_buffer"], 2, image.transfers,
                                             frame_bytes=2048, instruction_limit=100000)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                             (total, expected, []), (name, policy, mode, a, b))
                            self.assertEqual(result["unmapped_instructions"], 0)

    def test_loop_invariant_object_boundaries(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        callee = """extern u8 ra_buffer[256];
u16 lh_object_finish(u16 value) { ra_buffer[250]+=1; return value; }
u16 lh_object(u16 a,u16 b) {
  u16 i; u16 value; u16 total=a;
  for(i=0;i<4;i++) {
    if(i>=b) { continue; }
    value=ra_buffer[i]+257;
    total+=(value/6)+((a>>2)&31);
  }
  return lh_object_finish(total);
}
"""
        for contract in ("separate", "banked", "legacy", "caller_asm", "callee_asm"):
            for mode, flags in (("ra", ("-ra",)), ("shared", ("-ra",)), ("forced", ("-ra-all-spill",))):
                work = self.output / f"loop-objects-{contract}-{mode}"
                work.mkdir()
                barrier = '__asm("LD BC,4660");' if contract == "caller_asm" else ""
                caller = ("u8 ra_buffer[256]; u16 lh_object(u16 a,u16 b);\n"
                          f"u16 hoist_object(u16 a,u16 b) {{ {barrier} return lh_object(a,b); }}\n"
                          "void main(void) {}\n")
                body = callee.replace("u16 total=a;", 'u16 total=a; __asm("NOP");') if contract == "callee_asm" else callee
                for unit, source in (("caller", caller), ("callee", body)):
                    (work / f"{unit}.ssc").write_text(source)
                    bank = ("-b", "3", "-s", "2") if contract == "banked" and unit == "callee" else ()
                    run(compiler, ["-mz80", "-rs", "3", *bank, *flags, "-o", f"{unit}.asm", f"{unit}.ssc"],
                        work, f"{unit}.compiler.log")
                if contract == "legacy":
                    path = work / "callee.asm"
                    path.write_text(re.sub(r"^\s*; ssc_regcalls[^\n]*\n", "", path.read_text(), flags=re.M))
                run(linker, ["-lsms", "-ro", "4", "-v", *(["-share-arithmetic"] if mode == "shared" else []),
                             "-o", "linked.sms", "caller.asm", "callee.asm"], work, "linker.log")
                assembly = (work / "callee.asm").read_text()
                self.assertEqual("; loop invariant " in assembly,
                                 mode != "forced" and contract != "callee_asm", (contract, mode))
                image = read_image(work)
                self.assertNotIn("ssc.autocall.v1.w2.lh_object", image.symbols)
                if contract == "banked":
                    image = mapped_routine_image("loop", "lh_object", mode, image)
                for a in (0, 1, 127, 255, 256, 0x8000, 65535):
                    for b in (0, 1, 2, 3, 4, 16):
                        initial = bytes((index*37+a+b)&255 for index in range(256))
                        expected = bytearray(initial)
                        expected[250] = (expected[250]+1)&255
                        value = (a+sum((initial[i]+257)//6+((a>>2)&31) for i in range(min(b, 4))))&65535
                        result = execute(image.rom, image.symbols["hoist_object"], [a, b], [2, 2], initial,
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (value, expected, []), (contract, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)

    def test_byte_fact_comparisons_and_dynamic_counters(self):
        comparisons = {
            "mask": ("", "a&127", "b&255", lambda a, b: (a&127, b&255), True),
            "constant": ("", "a&255", "127", lambda a, b: (a&255, 127), True),
            "reverse": ("", "127", "a&255", lambda a, b: (127, a&255), True),
            "high": ("", "a>>8", "b&255", lambda a, b: (a>>8, b&255), True),
            "scalar": ("u16 left=a&127; u16 right=b&255;", "left", "right",
                       lambda a, b: (a&127, b&255), True),
            "add": ("", "(a&7)+1", "b&255", lambda a, b: ((a&7)+1, b&255), True),
            "signed": ("s16 left=a&127; s16 right=b&255;", "left", "right",
                       lambda a, b: (a&127, b&255), True),
            "byte": ("u8 left=a;", "left", "b&255", lambda a, b: (a&255, b&255), True),
            "negative": ("s8 left=a;", "left", "b&255",
                         lambda a, b: (((a&255)-(256 if a&128 else 0))&65535, b&255), False),
            "wide": ("", "a&511", "b&255", lambda a, b: (a&511, b&255), False),
            "carry": ("", "(a&255)+1", "b&255", lambda a, b: ((a&255)+1, b&255), False),
            "call": ("u16 left=a&127; fact_tick(b);", "left", "b&255",
                     lambda a, b: (a&127, b&255), False),
            "branch": ("u16 left=a&127; if(b&256) { left=256; }", "left", "b&255",
                       lambda a, b: (256 if b&256 else a&127, b&255), False),
            "alias": ("u16 left=a&127; fact_change(&left);", "left", "b&255",
                      lambda a, b: ((a&127)^256, b&255), False),
            "owner": ("u8 *p=ra_buffer+(a&15);", "a&255", "b&255",
                      lambda a, b: (a&255, b&255), True),
        }
        loops = {
            "mask": ("u16 n=a&127;", "", "", True),
            "add": ("u16 n=(a&7)+1;", "", "", True),
            "byte": ("u8 n=a;", "", "", True),
            "byte_mutate": ("u8 n=a;", "", "n=255;", True),
            "wide": ("u16 n=a&511;", "", "", False),
            "carry": ("u16 n=(a&255)+1;", "", "", False),
            "unknown": ("u16 n=a;", "", "", False),
            "mutate": ("u16 n=a&127;", "", "n=257;", False),
            "bound_alias": ("u16 n=a&127; u16 *p=&n;", "", "p[0]|=256;", False),
            "counter_alias": ("u16 n=a&127;", "u16 *p=&i;", "p[0]+=256;", False),
            "branch": ("u16 n=a&127; if(b&256) { n=257; }", "", "", False),
            "call": ("u16 n=a&127;", "", "fact_tick(b);", False),
            "assembly": ("u16 n=a&127;", "", '__asm("NOP");', False),
            "signed": ("s16 n=a&127;", "", "", False),
            "constant255": ("", "", "", True),
            "constant256": ("", "", "", False),
            "pressure": ("u16 saved=a*b; u16 n=a&127;", "", "", True),
            "dead": ("u16 n=a&127;", "", "", True),
        }
        programs = []
        for family, (setup, left, right, _, _) in comparisons.items():
            for op, token in enumerate(("==", "!=", "<", ">", "<=", ">=")):
                value = "p[0]" if family == "owner" else "1"
                programs.append(Program(
                    f"fact_compare_{family}_{op}", family, 0, op, [2, 2], 2, [],
                    f"u16 fact_compare_{family}_{op}(u16 a,u16 b) {{ {setup} "
                    f"if(({left}){token}({right})) {{ return {value}; }} return 0; }}"))
        for family, (setup, extra, body, _) in loops.items():
            bound = family.removeprefix("constant") if family.startswith("constant") else "n"
            counter_type = "s16" if family == "signed" else "u16"
            returned = "sum" if family == "dead" else "sum+i*257"
            if family == "pressure":
                returned += "+saved"
            if family in ("mutate", "byte_mutate"):
                returned += "+n"
            programs.append(Program(
                f"fact_loop_{family}", family, 1, 0, [2, 2], 2, [],
                f"u16 fact_loop_{family}(u16 a,u16 b) {{ {setup} {counter_type} i; u16 sum=0; {extra} "
                f"for(i=0;i<{bound};i++) {{ sum+=i^b; {body} }} return {returned}; }}"))
        source = ("u8 ra_buffer[128];\n"
                  "u16 fact_tick(u16 value) { ra_buffer[0]^=value; return value; }\n"
                  "void fact_change(u16 *value) { value[0]^=256; }\n" +
                  "\n".join(program.source for program in programs)+"\nvoid main(void) {}\n")
        directory = self.output / "byte-facts"
        profiles = {policy: compile_generated(
            directory / policy, os.environ.get("SAMESAMECC") or "samesamecc",
            os.environ.get("SAMESAMECL") or "samesamecl", programs, source,
            linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                    for policy in ("inline", "shared")}
        calls = 0
        for program in programs:
            firsts = ((0, 1, 2, 127, 128, 255, 256, 257, 511, 0xff00, 65535)
                      if program.variant == 0 else (0, 1, 2, 127, 128, 255, 256, 257, 511))
            for a in firsts:
                for b in ((0, 1, 127, 128, 255, 256, 257, 65535) if program.variant == 0 else (0, 257)):
                    initial = bytes((index*37+a+b)&255 for index in range(128))
                    expected_buffer = bytearray(initial)
                    if program.variant == 0:
                        left, right = comparisons[program.family][3](a, b)
                        condition = (left == right, left != right, left < right, left > right,
                                     left <= right, left >= right)[program.constant]
                        expected = (initial[a&15] if program.family == "owner" else 1) if condition else 0
                        if program.family == "call":
                            expected_buffer[0] ^= b&255
                    else:
                        family = program.family
                        n = (a&255 if family in ("byte", "byte_mutate") else
                             (a&7)+1 if family == "add" else (a&255)+1 if family == "carry" else
                             a if family in ("unknown", "wide") else
                             int(family.removeprefix("constant")) if family.startswith("constant") else
                             257 if family == "branch" and b&256 else a&127)
                        i = total = 0
                        while i < n:
                            total = (total+(i^b))&65535
                            if family in ("mutate", "byte_mutate"):
                                n = 255 if family == "byte_mutate" else 257
                            if family == "bound_alias":
                                n |= 256
                            if family == "counter_alias":
                                i += 256
                            if family == "call":
                                expected_buffer[0] ^= b&255
                            i += 1
                        expected = (total+(0 if family == "dead" else i*257)+
                                    (a*b if family == "pressure" else 0)+
                                    (n if family in ("mutate", "byte_mutate") else 0))&65535
                    for policy, images in profiles.items():
                        for mode, image in images.items():
                            with self.subTest(program=program.name, policy=policy, mode=mode, a=a, b=b):
                                result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                                 initial, image.symbols["ra_buffer"], 2, image.transfers,
                                                 instruction_limit=200000, frame_bytes=2048)
                                self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                                 (expected, expected_buffer, []))
                                self.assertEqual(result["unmapped_instructions"], 0)
                                calls += 1
        if os.environ.get("RA_BASELINE") != "1":
            for policy, images in profiles.items():
                for mode in ("ra", "ra_quiet"):
                    assembly = (directory / policy / mode / "main.asm").read_text()
                    for program in programs:
                        section = assembly.split(f'.SECTION "{program.name}"')[1].split(".ENDS")[0]
                        if program.variant == 0:
                            self.assertEqual("bounded word comparison using byte facts" in section,
                                             comparisons[program.family][4], (mode, program.name))
                        else:
                            self.assertEqual("bounded word counter increment; high byte preserved" in section,
                                             loops[program.family][3], (mode, program.name))
        self.assertEqual(calls, (90*88+18*18)*10)

    def test_dead_argument_homes_and_shift_leaves(self):
        cases = {
            "left": (3, "return a<<b;", lambda a, b, c: a<<b if b < 16 else 0, True),
            "right": (3, "return a>>b;", lambda a, b, c: a>>b, True),
            "restore": (3, "return (a<<b)>>b;",
                        lambda a, b, c: ((a<<b)&65535)>>b if b < 16 else 0, True),
            "reverse": (3, "return (a>>b)<<b;", lambda a, b, c: (a>>b)<<b, True),
            "constant": (3, "return (a<<9)>>9;", lambda a, b, c: a&127, True),
            "first": (3, "return a;", lambda a, b, c: a, True),
            "second": (3, "return b;", lambda a, b, c: b, True),
            "zero": (3, "return 0x9234;", lambda a, b, c: 0x9234, True),
            "add": (3, "return a+b;", lambda a, b, c: a+b, True),
            "sub": (3, "return a-b;", lambda a, b, c: a-b, True),
            "effects": (3, "return a;", lambda a, b, c: a, True),
            "one_shift": (1, "return a<<7;", lambda a, b, c: a<<7, True),
            "two_shift": (2, "return a>>b;", lambda a, b, c: a>>b, True),
            "mul": (3, "return a*b;", lambda a, b, c: a*b, True),
            "mul_two": (2, "return a*a;", lambda a, b, c: a*a, True),
            "memory_one": (1, "return ra_buffer[5];", lambda a, b, c: (185+a+b+c)&255, False),
            "memory_two": (2, "return ra_buffer[5];", lambda a, b, c: (185+a+b+c)&255, False),
            "memory_three": (3, "return ra_buffer[5];", lambda a, b, c: (185+a+b+c)&255, False),
            "gap": (3, "return a*c;", lambda a, b, c: a*c, True),
            "dead_first": (3, "return b*c;", lambda a, b, c: b*c, True),
            "assigned": (3, "c=a; return c*b;", lambda a, b, c: a*b, True),
            "escape": (3, "u16 *p=&c; p[0]^=a; return c;", lambda a, b, c: a^c, False),
            "third": (3, "return c;", lambda a, b, c: c, False),
            "different": (3, "return (a<<b)>>c;",
                          lambda a, b, c: ((a<<b)&65535)>>c if b < 16 else 0, False),
            "reverse_sub": (3, "return b-a;", lambda a, b, c: b-a, False),
            "call": (3, "return home_inner(a)+b;", lambda a, b, c: a+1+b, False),
            "recursive": (3, "if((a&3)==0) { return b; } return home_recursive(a-1,b,c)+1;",
                          lambda a, b, c: b+(a&3), False),
        }
        programs, helpers = [], []
        for name, (count, body, _, _) in cases.items():
            parameters = ",".join(f"u16 {value}" for value in ("a", "b", "c")[:count])
            arguments = ",".join(("a", "b", "home_touch(c)" if name == "effects" else "c")[:count])
            helpers.append(f"u16 home_{name}({parameters}) {{ {body} }}")
            helpers.append(f"u16 home_witness_{name}(u16 a,u16 b,u16 c) {{ return home_{name}({arguments}); }}")
            programs.append(Program(f"home_caller_{name}", name, 0, count, [2, 2, 2], 2, [],
                                    f"u16 home_caller_{name}(u16 a,u16 b,u16 c) {{ return home_{name}({arguments}); }}"))
        source = ("u8 ra_buffer[128];\nu16 home_inner(u16 a) { return a+1; }\n"
                  "u16 home_touch(u16 value) { ra_buffer[0]^=value; return value; }\n" +
                  "\n".join(helpers+[program.source for program in programs])+"\nvoid main(void) {}\n")
        directory = self.output / "argument-homes"
        profiles = {policy: compile_generated(
            directory / policy, os.environ.get("SAMESAMECC") or "samesamecc",
            os.environ.get("SAMESAMECL") or "samesamecl", programs,
            source+('void home_noise(void) { __asm("NOP"); }\n' if policy == "opaque" else ""),
            linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                    for policy in ("inline", "shared", "opaque")}
        calls = 0
        for program in programs:
            for a in (0, 1, 32768, 65535):
                for b in (0, 1, 7, 8, 15, 16, 17, 31, 255, 256, 257):
                    c = b+1 if program.family == "different" else (a*37+b)&65535
                    initial = bytes((index*37+a+b+c)&255 for index in range(128))
                    expected = cases[program.family][2](a, b, c)&65535
                    for policy, images in profiles.items():
                        for mode, image in images.items():
                            expected_buffer = bytearray(initial)
                            if program.family == "effects":
                                expected_buffer[0] ^= c&255
                            for legacy in (False, True) if policy == "opaque" else (False,):
                                name = f"home_{program.family}" if legacy else program.name
                                parameters = [a, b, c][:program.constant] if legacy else [a, b, c]
                                with self.subTest(program=name, policy=policy, mode=mode, a=a, b=b, c=c):
                                    result = execute(image.rom, image.symbols[name], parameters, [2]*len(parameters),
                                                     initial, image.symbols["ra_buffer"], 2, image.transfers,
                                                     instruction_limit=200000, frame_bytes=2048)
                                    self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                                     (expected, initial if legacy else expected_buffer, []))
                                    self.assertEqual(result["unmapped_instructions"], 0)
                                    calls += 1
        if os.environ.get("RA_BASELINE") != "1":
            for policy, images in profiles.items():
                for mode, image in images.items():
                    for program in programs:
                        selected = cases[program.family][3] and policy != "opaque"
                        self.assertEqual(f"ssc.regleaf.home_{program.family}" in image.symbols,
                                         selected, (policy, mode, program.name))
                        if selected:
                            self.assertEqual(image.staticMetrics["functions"][f"home_{program.family}"]["frame_bytes"], 0)
        self.assertEqual(calls, len(programs)*44*5*4)

    def test_postincrement_copy_fill_loops(self):
        forms = {
            "copy": ("", "dst[written++]=src[i];"),
            "copy_offset": ("", "dst[written++]=src[i+3];"),
            "fill": ("", "dst[written++]=value;"),
            "dead": ("", "dst[written++]=value;"),
            "counter": ("", "dst[written++]=i;"),
            "repeat": ("", "dst[written++]=value;"),
            "pre": ("", "dst[++written]=value;"),
            "double": ("", "dst[written++]=value; dst[written++]=value;"),
            "stride": ("", "dst[written]=value; written+=2;"),
            "dependent": ("", "dst[written++]=written;"),
            "bound": ("", "dst[written++]=value;"),
            "escape_index": ("post_escape(&written);", "dst[written++]=value;"),
            "escape_bound": ("post_escape(&n);", "dst[written++]=value;"),
            "call": ("", "dst[written++]=post_touch(value);"),
            "assembly": ("", '__asm("NOP"); dst[written++]=value;'),
            "pressure": ("u16 saved=n*start;", "dst[written++]=value;"),
            "pointer": ("", "dst[written++]=src[0]; src++;"),
            "byte": ("", "dst[written++]=value;"),
            "signed": ("", "dst[written++]=value;"),
        }
        programs = []
        for family, constant in ([(kind, -1) for kind in forms] +
                                 [("fill", count) for count in (0, 1, 2, 3, 127, 128, 255, 256, 257)] +
                                 [("copy", count) for count in (1, 2, 256)]):
            prefix, body = forms[family]
            name = f"post_loop_{family}_{'word' if constant < 0 else constant}"
            copied = family in ("copy", "copy_offset", "pointer")
            bound = "written" if family == "bound" else "n" if constant < 0 else str(constant)
            loop = f"for(i=0;i<{bound};i++) {{ {body} }}"
            source = (f"u16 {name}({'u8' if family == 'byte' else 'u16'} n,u16 start,u8 *dst,"
                      f"{'u8 *src' if copied else 'u16 value'}) {{ "
                      f"{'s16' if family == 'signed' else 'u16'} written=start; u16 i; {prefix} "
                      f"{loop} {loop if family == 'repeat' else ''} "
                      f"return written{'' if family == 'dead' else '^i'}"
                      f"{'^saved' if family == 'pressure' else ''}; }}")
            programs.append(Program(name, family, 0, constant, [1 if family == "byte" else 2, 2, 2, 2],
                                    2, [], source))
        directory = self.output / "postincrement-loops"
        source = ("u8 ra_buffer[1024];\nvoid post_escape(u16 *p) { p[0]^=3; }\n"
                  "u16 post_touch(u16 value) { ra_buffer[1023]+=1; return value^7; }\n" +
                  "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        profiles = {policy: compile_generated(directory / policy, os.environ.get("SAMESAMECC") or "samesamecc",
                                             os.environ.get("SAMESAMECL") or "samesamecl", programs, source,
                                             linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                    for policy in ("inline", "shared")}
        vectors = ([(count, 0, 64, 32, count*37+5) for count in (0, 1, 2, 7, 127, 128, 255, 256, 257)] +
                   [(3, start, 64, 32, 65535) for start in (0, 1, 127, 128, 255, 256, 65534, 65535)] +
                   [(7, 65535, dst, src, 255) for dst, src in ((32, 32), (33, 32), (32, 33), (64, 32), (32, 64))] +
                   [(1, 256, 64, 32, value) for value in (0, 1, 127, 128, 255, 256, 65535)])
        calls = 0
        for program in programs:
            cases = vectors if program.constant < 0 else (
                (0, 0, 64, 32, 0), (1, 65535, 33, 32, 255),
                (1, 128, 32, 33, 128), (1, 65534, 32, 32, 65535))
            if program.family == "bound":
                cases = ((0, 0, 64, 32, 255), (1, 65535, 64, 32, 255))
            for n, start, dst, src, value in cases:
                initial = bytes((index*37+n+start+value)&255 for index in range(1024))
                expected_buffer = bytearray(initial)
                written = start^3 if program.family == "escape_index" else start
                base = written
                count = (program.constant if program.constant >= 0 else n^3 if program.family == "escape_bound"
                         else n&255 if program.family == "byte" else 1 if program.family == "bound" and start
                         else 0 if program.family == "bound" else n)
                for index in range(count*(2 if program.family in ("double", "repeat") else 1)):
                    position = dst+index*(2 if program.family == "stride" else 1)+(program.family == "pre")
                    byte = (expected_buffer[src+index+(3 if program.family == "copy_offset" else 0)]
                            if program.family in ("copy", "copy_offset", "pointer") else
                            index if program.family == "counter" else
                            written+1 if program.family == "dependent" else
                            value^7 if program.family == "call" else value)
                    expected_buffer[position] = byte&255
                    written = (written+(2 if program.family == "stride" else 1))&65535
                    if program.family == "call":
                        expected_buffer[1023] = (expected_buffer[1023]+1)&255
                expected = written^(0 if program.family == "dead" else count)^(
                    (n*start)&65535 if program.family == "pressure" else 0)
                for policy, images in profiles.items():
                    for mode, image in images.items():
                        address = image.symbols["ra_buffer"]
                        parameters = [n, start, (address+dst-base)&65535,
                                      address+src if program.family in ("copy", "copy_offset", "pointer") else value]
                        with self.subTest(program=program.name, policy=policy, mode=mode,
                                          n=n, start=start, dst=dst, src=src):
                            result = execute(image.rom, image.symbols[program.name], parameters, program.widths,
                                             initial, address, 2, image.transfers, instruction_limit=200000,
                                             frame_bytes=512)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                             (expected, expected_buffer, []))
                            self.assertEqual(result["unmapped_instructions"], 0)
                            calls += 1
        if os.environ.get("RA_BASELINE") != "1":
            for policy, images in profiles.items():
                for mode in images:
                    assembly = (directory / policy / mode / "main.asm").read_text()
                    for program in programs:
                        body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        selected = "; advancing byte " in body
                        supported = program.family in ("copy", "copy_offset", "fill", "repeat", "pressure")
                        if program.family in ("repeat", "pressure", "dead") and mode in ("ra", "ra_quiet"):
                            continue
                        self.assertEqual(selected, supported and program.constant != 0 and mode in ("ra", "ra_quiet"),
                                         (program.name, policy, mode))
        self.assertEqual(calls, 5720)

    def test_biased_frame_windows(self):
        sizes = (8, 112, 116, 120, 128, 136, 176, 208, 220, 228, 236, 252, 260, 512)
        programs = []
        for family in ("plain", "guarded", "alias", "assembly", "edge"):
            for size in (range(220, 240) if family == "edge" else sizes):
                before = f"bytes=&workspace[{size-1}];" if family == "alias" else ""
                body = ('if(i&1) { sum=window_touch(sum); } else { ra_buffer[126]=sum; }'
                        if family == "guarded" else '__asm("LD IX,4660");' if family == "assembly" else "")
                loop = (f"u16 i; for(i=0;i<(b&7);i++) {{ {body} "
                        f"sum=(sum+bytes[{'0' if family == 'alias' else 'i'}]*(i+1))&8191; }}")
                source = (f"u16 biased_{family}_{size}(u16 a,u16 b,u8 *bytes) {{ u8 workspace[{size}]; "
                          f"u16 sum=a; window_escape(&sum); workspace[{size-1}]=a; {before} {loop} "
                          f"return sum+workspace[{size-1}]+b; }}")
                programs.append(Program(f"biased_{family}_{size}", family, size, 0, [2, 2, 2], 2, [], source))
        directory = self.output / "biased-frame-windows"
        profiles = {}
        for family in ("plain", "guarded", "alias", "assembly", "edge"):
            group = [program for program in programs if program.family == family]
            source = ("u8 ra_buffer[128];\nvoid window_escape(u16 *p) { p[0]^=3; }\n"
                      "u16 window_touch(u16 value) { ra_buffer[127]+=1; return value^7; }\n" +
                      "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
            for policy in ("inline", "shared"):
                profiles[family, policy] = compile_generated(
                    directory / family / policy, os.environ.get("SAMESAMECC") or "samesamecc",
                    os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                    linker_flags=("-share-arithmetic",) if policy == "shared" else ())
        calls = 0
        for program in programs:
            for a in (0, 1, 127, 128, 255, 256, 32768, 65535):
                for b in (0, 1, 2, 7, 8, 15, 65535):
                    initial = bytes((index*37+a+b)&255 for index in range(128))
                    expected_buffer = bytearray(initial)
                    value = a^3
                    for index in range(b&7):
                        if program.family == "guarded":
                            if index&1:
                                value ^= 7
                                expected_buffer[127] = (expected_buffer[127]+1)&255
                            else:
                                expected_buffer[126] = value&255
                        byte = a&255 if program.family == "alias" else initial[32+index]
                        value = (value+byte*(index+1))&8191
                    expected = (value+(a&255)+b)&65535
                    for policy in ("inline", "shared"):
                        for mode, image in profiles[program.family, policy].items():
                            address = image.symbols["ra_buffer"]
                            result = execute(image.rom, image.symbols[program.name], [a, b, address+32], [2, 2, 2],
                                             initial, address, 2, image.transfers, frame_bytes=1024)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                             (expected, expected_buffer, []), (program.name, policy, mode, a, b))
                            self.assertEqual(result["unmapped_instructions"], 0)
                            calls += 1
        if os.environ.get("RA_BASELINE") != "1":
            boundary_frames = set()
            compact_windows = 0
            biased_windows = 0
            for (family, policy), images in profiles.items():
                for mode in images:
                    assembly = (directory / family / policy / mode / "main.asm").read_text()
                    for program in (item for item in programs if item.family == family):
                        body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        frame = int(re.search(r"allocate space for the stack frame bytes=(\d+)", body)[1])
                        selected = "      LD  IX,-126\n      ADD IX,DE\n" in body
                        if family == "edge" and mode == "ra":
                            boundary_frames.add(frame)
                        if mode not in ("ra", "ra_quiet") or family == "assembly" or frame > 253:
                            self.assertFalse(selected, (program.name, policy, mode, frame))
                        elif family != "alias" and program.variant >= 128:
                            if selected:
                                biased_windows += 1
                            else:
                                self.assertEqual(set(re.findall(r"^      LD  IX,(-?\d+)$", body, re.M)), {"0"},
                                                 (program.name, policy, mode, frame))
                                scalar_offsets = re.findall(
                                    r'; TAC: variable "[^"]+" size [12] offset (-?\d+) type local', body)
                                self.assertTrue(scalar_offsets)
                                self.assertTrue(all(-128 <= int(offset) <= 126 for offset in scalar_offsets))
                                compact_windows += 1
                        for displacement in re.findall(r"\(IX([+-]\d+)\)", body):
                            self.assertLessEqual(-128, int(displacement))
                            self.assertLessEqual(int(displacement), 127)
            self.assertTrue({253, 254}.issubset(boundary_frames), boundary_frames)
            self.assertGreater(compact_windows, 0)
            self.assertGreater(biased_windows, 0)
        self.assertEqual(calls, 42560)

    def test_biased_frame_join_fallback(self):
        source = """
u16 ra_buffer[64];
void perturb(u16 *p) { p[0]^=3; }
void biased_join(u16 a,u16 b) {
  u16 near; u8 padding[119]; u16 x,y,z; u8 far_padding[17]; u16 far;
  ra_buffer[0]=&x; ra_buffer[1]=&y; ra_buffer[2]=&z; ra_buffer[3]=&far;
  ra_buffer[4]=padding; ra_buffer[5]=far_padding; ra_buffer[6]=&near;
  x=a; y=a; z=a; far=a;
  perturb(&x); perturb(&y); perturb(&z); perturb(&far);
  if(b&1) { near=b+far; near+=a; near+=x; near+=y; near+=z; }
  else { near=b+a; near+=x; near+=y; near+=z; }
  ra_buffer[16]=near;
}
void main(void) {}
"""
        program = Program("biased_join", "join", 0, 0, [2, 2], 0, [], source)
        directory = self.output / "biased-frame-windows" / "join"
        for policy in ("inline", "shared"):
            images = compile_generated(directory / policy, os.environ.get("SAMESAMECC") or "samesamecc",
                                       os.environ.get("SAMESAMECL") or "samesamecl", [program], source,
                                       linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for mode, image in images.items():
                self.assertNotIn("      LD  IX,-126\n", (directory / policy / mode / "main.asm").read_text())
                for a in (0, 1, 127, 128, 255, 256, 32768, 65535):
                    for b in (0, 1, 2, 7, 8, 15, 65535):
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], bytes(128),
                                         image.symbols["ra_buffer"], 0, image.transfers, frame_bytes=512)
                        expected = ((a^3)*(4 if b&1 else 3)+a+b)&65535
                        self.assertEqual(int.from_bytes(result["buffer"][32:34], "little"), expected)
                        self.assertEqual(result["buffer"][14:32]+result["buffer"][34:], bytes(112))
                        self.assertEqual((result["port_writes"], result["unmapped_instructions"]), ([], 0))

    def test_byte_factors_word_products(self):
        cases = {
            "byte_left": ("u8 factor=a; return factor*b;", lambda a, b, v: (a&255)*b, True),
            "byte_right": ("u8 factor=a; return b*factor;", lambda a, b, v: (a&255)*b, True),
            "mask_left": ("return (a&255)*b;", lambda a, b, v: (a&255)*b, True),
            "mask_right": ("return b*(a&255);", lambda a, b, v: (a&255)*b, True),
            "pair": ("return (a&255)*(b&255);", lambda a, b, v: (a&255)*(b&255), True),
            "typed_pair": ("u8 first=a; u8 second=b; return first*b+a*second;",
                           lambda a, b, v: (a&255)*b+a*(b&255), True),
            "shift_left": ("return (a>>8)*b;", lambda a, b, v: (a>>8)*b, True),
            "shift_right": ("return b*(a>>8);", lambda a, b, v: (a>>8)*b, True),
            "load_left": ("return bytes[0]*b;", lambda a, b, v: v*b, True),
            "load_right": ("return b*bytes[0];", lambda a, b, v: v*b, True),
            "snapshot": ("u16 factor=bytes[0]; return factor*b;", lambda a, b, v: v*b, True),
            "or": ("return ((a&127)|128)*b;", lambda a, b, v: ((a&127)|128)*b, True),
            "xor": ("return ((a&255)^129)*b;", lambda a, b, v: ((a&255)^129)*b, True),
            "signed_left": ("s8 factor=a; return factor*b;",
                            lambda a, b, v: ((a&255)-(256 if a&128 else 0))*b, False),
            "signed_right": ("s8 factor=a; return b*factor;",
                             lambda a, b, v: ((a&255)-(256 if a&128 else 0))*b, False),
            "signed_load": ("return bytes[0]*b;", lambda a, b, v: (v-(256 if v&128 else 0))*b, False),
            "signed_other": ("s16 word=b; return (a&255)*word;", lambda a, b, v: (a&255)*b, True),
            "wide": ("return (a&511)*b;", lambda a, b, v: (a&511)*b, False),
            "unknown": ("return a*b;", lambda a, b, v: a*b, False),
            "carry": ("return ((a&255)+1)*b;", lambda a, b, v: ((a&255)+1)*b, False),
            "call_before": ("return (product_touch(a)&255)*b;", lambda a, b, v: (a&255)*b, True),
            "call_after": ("u16 factor=a&255; product_touch(a); return factor*b;",
                           lambda a, b, v: (a&255)*b, False),
            "write_after": ("u16 factor=bytes[0]; bytes[0]=a; return factor*b;", lambda a, b, v: v*b, False),
            "alias": ("u16 factor=a&255; product_escape(&factor); return factor*b;",
                      lambda a, b, v: ((a&255)^256)*b, False),
            "branch": ("u16 factor=a&255; if(a&256) { factor|=256; } return factor*b;",
                       lambda a, b, v: (a&511)*b, False),
            "assembly": ('u16 factor=a&255; __asm("NOP"); return factor*b;',
                         lambda a, b, v: (a&255)*b, False),
            "pressure": ("u16 held=a^b; u16 result=(a&255)*b; return result+held;",
                         lambda a, b, v: (a&255)*b+(a^b), True),
            "far": ("u8 workspace[260]; workspace[259]=a; return (a&255)*b+workspace[259];",
                    lambda a, b, v: (a&255)*b+(a&255), True),
            "constant": ("return (a&255)*9;", lambda a, b, v: (a&255)*9, False),
            "store": ("u16 result=(a&255)*b; bytes[2]=result>>8; return result;",
                      lambda a, b, v: (a&255)*b, True),
            "narrow": ("u8 result=(a&255)*b; return result;", lambda a, b, v: ((a&255)*b)&255, True),
            "both_byte": ("u8 first=a; u8 second=b; return first*second;",
                          lambda a, b, v: ((a&255)*(b&255))&255, False),
            "read_twice": ("u16 first=bytes[0]; return first*bytes[1];",
                           lambda a, b, v: v*((v+37)&255), True),
            "word_load": ("return product_words[a&7]*b;",
                          lambda a, b, v: (0, 1, 255, 256, 32767, 32768, 65535, 0x9234)[a&7]*b, False),
        }
        programs = [
            Program(f"byte_product_{name}", name, 0, 0, [2, 2, 2], 2, [],
                    f"u16 byte_product_{name}(u16 a,u16 b,{'s8' if name == 'signed_load' else 'u8'} *bytes) {{ {body} }}")
            for name, (body, _, _) in cases.items()
        ]
        source = ("u8 ra_buffer[128];\n"
                  "const u16 product_words[8]={0,1,255,256,32767,32768,65535,0x9234};\n"
                  "u16 product_touch(u16 value) { ra_buffer[0]=ra_buffer[0]+1; return value; }\n"
                  "void product_escape(u16 *value) { value[0]^=256; }\n" +
                  "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        directory = self.output / "byte-factors-word-products"
        profiles = {
            policy: compile_generated(directory / policy, compiler, linker, programs, source,
                                      linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for policy in ("inline", "shared")
        }
        for program in programs:
            name = program.family
            values = range(256) if name.startswith("byte_") else (0, 1, 127, 128, 255, 256, 257, 32767, 32768, 65535)
            for a in values:
                for b in (0, 1, 255, 256, 32768, 65535):
                    initial = bytes((index*37+a+b)&255 for index in range(128))
                    expected = cases[name][1](a, b, initial[0]) & 65535
                    expected_buffer = bytearray(initial)
                    if name.startswith("call_"):
                        expected_buffer[0] = (initial[0]+1)&255
                    elif name == "write_after":
                        expected_buffer[0] = a&255
                    elif name == "store":
                        expected_buffer[2] = expected>>8
                    for policy, images in profiles.items():
                        for mode, image in images.items():
                            address = image.symbols["ra_buffer"]
                            result = execute(image.rom, image.symbols[program.name], [a, b, address], [2, 2, 2],
                                             initial, address, 2, image.transfers, frame_bytes=2048)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                             (expected, expected_buffer, []), (name, policy, mode, a, b))
                            self.assertEqual(result["unmapped_instructions"], 0)
        if os.environ.get("RA_BASELINE") != "1":
            for policy, images in profiles.items():
                for mode, image in images.items():
                    if policy == "shared":
                        self.assertEqual(sum(name.startswith("ssc.arithmetic_") for name in image.symbols), 3)
                    if mode not in ("ra", "ra_quiet"):
                        continue
                    assembly = (directory / policy / mode / "main.asm").read_text()
                    for program in programs:
                        section = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        selected = any(f"      LD  {register},8 ; number of bits to process" in section
                                       for register in ("B", "D"))
                        self.assertEqual(selected, cases[program.family][2],
                                         (policy, mode, program.family))

    def test_scaled_word_byte_reductions(self):
        cases = {
            "dynamic": True, "one": True, "two": True, "sixteen": True,
            "zero": False, "max_byte": True, "word_bound": True, "wide": True,
            "dynamic_wide": True, "alias": True, "live_counter": True,
            "low_mask": True, "high_mask": True, "wrap_mask": True,
            "identity": True, "signed_word": False, "signed_byte": False,
            "signed_sum": False, "signed_counter": False, "byte_counter": False,
            "pointer_word": False, "reverse": False, "stride": False, "row": False,
            "write": False, "call": False, "branch": False, "continue": False,
            "break": False, "escaped": False, "assembly": False, "pressure": True,
            "far": False, "changing_bound": False, "full_word": False,
        }
        fixed_counts = {"zero": 0, "one": 1, "two": 2, "sixteen": 16,
                        "max_byte": 255, "word_bound": 256, "wide": 257, "full_word": 65535}
        global_cases = {"max_byte", "word_bound", "wide", "dynamic_wide", "alias", "pointer_word"}
        masks = {"low_mask": 15, "high_mask": 0xff00, "wrap_mask": 0x8001,
                 "identity": 65535, "changing_bound": 1}
        words = [(index*0x1234 ^ 65535) & 65535 for index in range(512)]
        word_bytes = b"".join(value.to_bytes(2, "little") for value in words)
        programs = []
        for name in cases:
            size = 32 if name == "stride" else 16
            prefix = ("u16 *values=scaled_words;" if name == "pointer_word" else "" if name in global_cases else
                      f"u16 values[{size}]; for(k=0;k<{size};k++) {{ values[k]=(a+k*0x1234)^65535; }}")
            array = "scaled_words" if name in global_cases and name != "pointer_word" else "values"
            index = "i*2" if name == "stride" else "(i&3)*4+(i>>2)" if name == "row" else "i"
            sources = f"bytes[i]+{array}[{index}]" if name == "reverse" else f"{array}[{index}]+bytes[i]"
            body = f"sum=(sum+{sources})&{masks.get(name, 0x1fff)};"
            before = {
                "write": "bytes[i]=bytes[i]+1;", "call": "sum=scaled_touch(sum);",
                "branch": "if(i&1) { sum^=7; }", "continue": "if(i==3) { continue; }",
                "break": "if(i==3) { break; }",
            }.get(name, "")
            after = "if(i==15) { break; }" if name == "full_word" else ""
            extra = {"escaped": "scaled_escape(&sum);", "assembly": '__asm("NOP");',
                     "pressure": "u16 held=a^b;", "far": "workspace[259]=a;"}.get(name, "")
            suffix = ("+i" if name in ("live_counter", "break", "full_word") else
                      "+held" if name == "pressure" else "+workspace[259]" if name == "far" else "")
            bound = "sum" if name == "changing_bound" else str(fixed_counts.get(name, "b"))
            source = (f"u16 scaled_{name}(u16 a,u16 b,{'s8' if name == 'signed_byte' else 'u8'} *bytes) {{ "
                      f"{'u8 workspace[260];' if name == 'far' else ''} u16 k; {prefix} "
                      f"{'s16' if name == 'signed_sum' else 'u16'} sum=a&{15 if name == 'changing_bound' else 8191}; "
                      f"{'s16' if name == 'signed_counter' else 'u8' if name == 'byte_counter' else 'u16'} i; "
                      f"{extra} for(i=0;i<{bound};i++) {{ {before} {body} {after} }} return sum{suffix}; }}")
            if name == "signed_word":
                source = source.replace("u16 values[", "s16 values[")
            programs.append(Program(f"scaled_{name}", name, 0, 0, [2, 2, 2], 2, [], source))
        source = ("u8 ra_buffer[1024];\nconst u16 scaled_words[512]={" +
                  ",".join(map(str, words)) + "};\n"
                  "u16 scaled_touch(u16 value) { return value^7; }\n"
                  "void scaled_escape(u16 *value) { value[0]^=0x1234; }\n" +
                  "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n")
        directory = self.output / "scaled-word-byte-reductions"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        for program in programs:
            name = program.family
            counts = (0, 1, 2, 16, 255, 256, 257, 512) if name == "dynamic_wide" else (0, 1, 2, 7, 15, 16)
            for a in (0, 1, 0x7fff, 0x8000, 0xffff, 0xa5a5):
                for b in counts:
                    initial = bytes((index*29+a+b) & 255 for index in range(1024))
                    expected_buffer = bytearray(initial)
                    values = words if name in global_cases else [
                        ((a+index*0x1234) ^ 65535) & 65535 for index in range(32)]
                    sum_value = a & (15 if name == "changing_bound" else 8191)
                    if name == "escaped":
                        sum_value ^= 0x1234
                    count = fixed_counts.get(name, b)
                    if name == "changing_bound":
                        count = int(sum_value != 0)
                    counter = 0
                    for index in range(min(count, 16) if name == "full_word" else count):
                        counter = index
                        if name == "break" and index == 3:
                            break
                        if name == "continue" and index == 3:
                            continue
                        if name == "write":
                            expected_buffer[index] = (expected_buffer[index]+1) & 255
                        if name == "call" or name == "branch" and index & 1:
                            sum_value ^= 7
                        word_index = index*2 if name == "stride" else (index & 3)*4+(index >> 2) if name == "row" else index
                        byte_value = word_bytes[index+1] if name == "alias" else expected_buffer[index]
                        if name == "signed_byte" and byte_value & 128:
                            byte_value -= 256
                        sum_value = (sum_value+values[word_index]+byte_value) & masks.get(name, 0x1fff)
                    else:
                        counter = count
                    if name in ("live_counter", "break", "full_word"):
                        sum_value += 15 if name == "full_word" else counter
                    elif name == "pressure":
                        sum_value += a ^ b
                    elif name == "far":
                        sum_value += a & 255
                    for mode, image in images.items():
                        address = image.symbols["ra_buffer"]
                        pointer = image.symbols["scaled_words"]+1 if name == "alias" else address
                        result = execute(image.rom, image.symbols[program.name], [a, b, pointer], [2, 2, 2],
                                         initial, address, 2, image.transfers,
                                         instruction_limit=100000, frame_bytes=1024)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (sum_value & 65535, expected_buffer, []), (name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0, (name, mode))
        if os.environ.get("RA_BASELINE") != "1":
            for mode in images:
                assembly = (directory / mode / "main.asm").read_text()
                for program in programs:
                    body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                    self.assertEqual("; advancing scaled word-byte sum loop" in body,
                                     mode in ("ra", "ra_quiet") and cases[program.family], (mode, program.family))

    def test_word_quotient_carriers(self):
        def divide(a, b, mask=65535):
            return divmod(a, b) if b else (mask, a)

        cases = {
            "pair": ("u16", "u16 q=a/b; u16 r=a%b; return (q*b)^r;",
                     lambda a, b, q, r: (q*b)^r, True),
            "direct": ("u16", "u16 q=a/b; return q*b;",
                       lambda a, b, q, r: q*b, True),
            "constant": ("u16", "u16 q=a/b; u16 r=a%b; return (q*3)^r;",
                         lambda a, b, q, r: (q*3)^r, True),
            "weighted": ("u16", "u16 q=a/b; u16 r=a%b; return q*(b^0x1234)+r;",
                         lambda a, b, q, r: q*(b^0x1234)+r, True),
            "narrow_remainder": ("u16", "u16 q=a/b; u8 r=a%b; return (q*b)^r;",
                                 lambda a, b, q, r: (q*b)^(r&255), True),
            "signed_remainder": ("u16", "u16 q=a/b; s8 r=a%b; return (q*b)^r;",
                                 lambda a, b, q, r: (q*b)^((r&255)-(256 if r&128 else 0)), True),
            "pressure": ("u16", "u16 held=~a; u16 q=a/b; u16 r=a%b; return ((q*b)^r)+held;",
                         lambda a, b, q, r: ((q*b)^r)+(a^65535), None),
            "arg2": ("u16", "u16 q=a/b; u16 r=a%b; return (b*q)^r;",
                     lambda a, b, q, r: (q*b)^r, False),
            "multiple": ("u16", "u16 q=a/b; u16 r=a%b; return ((q*b)^r)+q;",
                         lambda a, b, q, r: ((q*b)^r)+q, False),
            "byte": ("u8", "u16 q=a/b; u16 r=a%b; return (q*b)^r;",
                     lambda a, b, q, r: (q*b)^r, False),
            "narrow_dividend": ("u16", "u8 first=a; u16 q=first/b; u16 r=first%b; return (q*b)^r;",
                                lambda a, b, q, r: (q*b)^r, False),
            "narrow_divisor": ("u16", "u8 second=b; u16 q=a/second; u16 r=a%second; return (q*second)^r;",
                               lambda a, b, q, r: (q*(b&255))^r, False),
            "signed_dividend": ("u16", "s8 first=a; u16 q=first/b; u16 r=first%b; return (q*b)^r;",
                                lambda a, b, q, r: (q*b)^r, False),
            "signed_word": ("s16", "s16 q=a/b; s16 r=a%b; return (q*b)^r;",
                            lambda a, b, q, r: (q*b)^r, False),
            "overwrite": ("u16", "a=a/b; u16 r=a%b; return (a*b)^r;",
                          lambda a, b, q, r: (q*b)^divide(q, b)[1], False),
            "changed": ("u16", "u16 q=a/b; a^=0x1234; u16 r=a%b; return (q*b)^r;",
                        lambda a, b, q, r: (q*b)^divide(a^0x1234, b)[1], False),
            "call": ("u16", "u16 q=a/b; quotient_touch(a); u16 r=a%b; return (q*b)^r;",
                     lambda a, b, q, r: (q*b)^r, False),
            "alias": ("u16", "u16 q=a/b; quotient_escape(&a); u16 r=a%b; return (q*b)^r;",
                      lambda a, b, q, r: (q*b)^divide(a^0x1234, b)[1], False),
            "asm": ("u16", 'u16 q=a/b; __asm("NOP"); u16 r=a%b; return (q*b)^r;',
                    lambda a, b, q, r: (q*b)^r, False),
            "global_input": ("u16", "u16 q=quotient_global/b; "
                             "u16 r=quotient_global%b; return (q*b)^r;",
                             lambda a, b, q, r: (q*b)^r, False),
            "global_result": ("u16", "quotient_global=a/b; u16 r=a%b; return (quotient_global*b)^r;",
                              lambda a, b, q, r: (q*b)^r, False),
            "width_change": ("u16", "u16 q=a/b; u8 first=a; u8 second=b; "
                             "u16 r=first%second; return (q*b)^r;",
                             lambda a, b, q, r: (q*b)^divide(a&255, b&255, 255)[1], False),
            "branch": ("u16", "u16 q=a/b; if (a&1) { b^=1; } u16 r=a%b; return (q*b)^r;",
                       lambda a, b, q, r: (q*(b^(a&1)))^divide(a, b^(a&1))[1], False),
        }
        programs = [Program(f"quotient_{name}", name, 0, 0, [1, 1] if kind == "u8" else [2, 2],
                            2, [], f"u16 quotient_{name}({kind} a,{kind} b) {{ {body} }}")
                    for name, (kind, body, _, _) in cases.items()]
        source = ("u8 ra_buffer[128]; u16 quotient_global;\n"
                  "void quotient_touch(u16 value) { ra_buffer[0]=value; }\n"
                  "void quotient_escape(u16 *value) { value[0]^=0x1234; }\n")
        source += "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
        directory = self.output / "word-quotient-carriers"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        buffer_bytes = max(128, *(image.symbols["quotient_global"]-image.symbols["ra_buffer"]+2
                                  for image in images.values()))
        edges = (0, 1, 127, 128, 255, 256, 32768, 65535)
        arguments = [(edges[seed%8], edges[(seed//8)*2+seed%2]) for seed in range(32)]
        arguments += [((seed*15373+421)&65535, (seed*7919+12345)&65535) for seed in range(16)]
        calls = 0
        for program in programs:
            kind, _, oracle, _ = cases[program.family]
            for left, right in arguments:
                a, b = (left&255, right&255) if kind == "u8" else (left, right)
                numerator = a&255 if program.family == "narrow_dividend" else (
                    ((a&255)-(256 if a&128 else 0))&65535 if program.family == "signed_dividend" else a)
                divisor = b&255 if program.family == "narrow_divisor" else b
                q, r = divide(numerator, divisor, 255 if kind == "u8" else 65535)
                expected = oracle(a, b, q, r)&65535
                initial = bytes((index*37+a+b)&255 for index in range(buffer_bytes))
                for mode, image in images.items():
                    if program.family == "global_input":
                        initial = bytearray(initial)
                        offset = image.symbols["quotient_global"]-image.symbols["ra_buffer"]
                        initial[offset:offset+2] = a.to_bytes(2, "little")
                    expected_buffer = bytearray(initial)
                    if program.family == "call":
                        expected_buffer[0] = a&255
                    if program.family in ("global_input", "global_result"):
                        offset = image.symbols["quotient_global"]-image.symbols["ra_buffer"]
                        value = a if program.family == "global_input" else q
                        expected_buffer[offset:offset+2] = value.to_bytes(2, "little")
                    result = execute(image.rom, image.symbols[program.name], [a, b], program.widths,
                                     initial, image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                    self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                     (expected, expected_buffer, []), (program.family, mode, a, b))
                    self.assertEqual(result["unmapped_instructions"], 0)
                    calls += 1
        for mode in images:
            assembly = (directory / mode / "main.asm").read_text()
            for program in programs:
                expected_selection = cases[program.family][3]
                if expected_selection is None:
                    continue
                body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                selected = bool(re.search(r"; TAC: [^\n]* / [^\n]*\n(?:(?!; TAC:).)*"
                                          r"; retained result in BC", body, re.DOTALL))
                self.assertEqual(selected, mode in ("ra", "ra_quiet") and expected_selection,
                                 (program.family, mode))
        self.assertEqual(calls, len(programs)*48*len(images))

    def test_constant_word_lanes(self):
        programs, oracles = [], {}
        for operation, symbol in (("and", "&"), ("or", "|"), ("xor", "^")):
            for mask in (0, 1, 0x80, 0xff, 0xff00, 0xffff, 0x1234, 0x8000):
                for reverse in (False, True):
                    expression = f"{mask}{symbol}value" if reverse else f"value{symbol}{mask}"
                    for captured in (False, True):
                        name = f"lane_{operation}_{mask}_{int(reverse)}_{int(captured)}"
                        snapshot = ('__asm("PUSH AF","POP BC","LD HL,ra_buffer",'
                                    '"LD (HL),C","INC HL","LD (HL),B");') if captured else ""
                        programs.append(Program(name, operation, int(captured), mask, [2, 2], 2, [],
                                                f"u16 {name}(u16 a,u16 b) {{ u16 value=a+b; "
                                                f"u16 result={expression}; {snapshot} "
                                                f"return result{'' if captured else '^b'}; }}"))
                        oracles[name] = lambda a, b, mask=mask, symbol=symbol, captured=captured: (
                            {"&": lambda x: x&mask, "|": lambda x: x|mask, "^": lambda x: x^mask}[symbol](
                                (a+b)&65535)^(0 if captured else b))&65535
        source = "u8 ra_buffer[128];\n"+"\n".join(program.source for program in programs)+"\nvoid main(void) {}\n"
        images = compile_generated(self.output / "constant-word-lanes",
                                   os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        for mode, image in images.items():
            assembly = (self.output / "constant-word-lanes" / mode / "main.asm").read_text()
            self.assertNotRegex(assembly, r"LD  A,0\n      AND A,[HB]\n      LD  [HB],A")
            self.assertNotRegex(assembly, r"LD  A,255\n      XOR A,[HB]\n      LD  [HB],A")
            self.assertIn("      CPL\n", assembly)
            for program in programs:
                for a in (0, 1, 127, 128, 255, 256, 0x7fff, 0x8000, 0xff00, 65535):
                    for b in (0, 1, 0x9234, 65535):
                        initial = bytes((index*37+a+b)&255 for index in range(128))
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], initial,
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        expected = oracles[program.name](a, b)
                        self.assertEqual((result["value"], result["unmapped_instructions"]),
                                         (expected, 0), (program.name, mode, a, b))
                        if program.variant:
                            low = expected&255
                            flags = (low&128) | (64 if low == 0 else 0) | (4 if low.bit_count()%2 == 0 else 0)
                            self.assertEqual((result["buffer"][0]&0xc4, result["buffer"][1], result["buffer"][2:]),
                                             (flags, low, initial[2:]), (program.name, mode, a, b))
                        else:
                            self.assertEqual(result["buffer"], initial, (program.name, mode, a, b))

    def test_bounded_pure_expression_reuse(self):
        cases = {
            "masked": ("u16 first=1+((a>>(b&7))&3); return first+((a>>(b&7))&1);",
                       lambda a, b: 1+((a>>(b&7))&3)+((a>>(b&7))&1), True),
            "left": ("u16 first=1+((a<<(b&7))&3); return first+((a<<(b&7))&1);",
                     lambda a, b: 1+((a<<(b&7))&3)+((a<<(b&7))&1), True),
            "direct": ("u16 count=b&7; u16 first=1+((a>>count)&3); return first+((a>>count)&1);",
                       lambda a, b: 1+((a>>(b&7))&3)+((a>>(b&7))&1), True),
            "changed_value": ("u16 first=1+((a>>(b&7))&3); a++; return first+((a>>(b&7))&1);",
                              lambda a, b: 1+((a>>(b&7))&3)+((((a+1)&65535)>>(b&7))&1), False),
            "changed_count": ("u16 first=1+((a>>(b&7))&3); b++; return first+((a>>(b&7))&1);",
                              lambda a, b: 1+((a>>(b&7))&3)+((a>>((b+1)&7))&1), False),
            "call": ("u16 first=1+((a>>(b&7))&3); a=reuse_touch(a); return first+((a>>(b&7))&1);",
                     lambda a, b: 1+((a>>(b&7))&3)+((((a+1)&65535)>>(b&7))&1), False),
            "assembly": ('u16 first=1+((a>>(b&7))&3); __asm("NOP"); return first+((a>>(b&7))&1);',
                         lambda a, b: 1+((a>>(b&7))&3)+((a>>(b&7))&1), False),
            "alias": ("u16 *pointer=&a; u16 first=1+((a>>(b&7))&3); pointer[0]=a+1; "
                      "return first+((pointer[0]>>(b&7))&1);",
                      lambda a, b: 1+((a>>(b&7))&3)+((((a+1)&65535)>>(b&7))&1), False),
            "store": ("u16 first=1+((a>>(b&7))&3); ra_buffer[1]=a; return first+((a>>(b&7))&1);",
                      lambda a, b: 1+((a>>(b&7))&3)+((a>>(b&7))&1), False),
            "branch": ("u16 first=1+((a>>(b&7))&3); if(b&1) { a++; } return first+((a>>(b&7))&1);",
                       lambda a, b: 1+((a>>(b&7))&3)+((((a+(b&1))&65535)>>(b&7))&1), False),
            "narrow": ("u8 first=a>>(b&7); return first+((a>>(b&7))&1);",
                       lambda a, b: ((a>>(b&7))&255)+((a>>(b&7))&1), False),
            "pressure": ("u16 first=1+((a>>(b&7))&3); u16 extra=a+b; "
                         "return first+((a>>(b&7))&1)+extra;",
                         lambda a, b: 1+((a>>(b&7))&3)+((a>>(b&7))&1)+a+b, False),
            "temp_pressure": ("return (1+((a>>(b&7))&3))+((a>>(b&7))&1);",
                              lambda a, b: 1+((a>>(b&7))&3)+((a>>(b&7))&1), False),
        }
        programs = [Program(f"reuse_{name}", name, 0, 0, [2, 2], 2, [],
                            f"u16 reuse_{name}(u16 a,u16 b) {{ {body} }}")
                    for name, (body, _, _) in cases.items()]
        source = ("u8 ra_buffer[128]; u16 reuse_touch(u16 value) { ra_buffer[0]=ra_buffer[0]+1; return value+1; }\n"+
                  "\n".join(program.source for program in programs)+"\nvoid main(void) {}\n")
        images = compile_generated(self.output / "bounded-expression-reuse",
                                   os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        for mode, image in images.items():
            assembly = (self.output / "bounded-expression-reuse" / mode / "main.asm").read_text()
            for program in programs:
                body = assembly.split(f'.SECTION "{program.name}" FREE', 1)[1].split(".ENDS", 1)[0]
                shifts = len(re.findall(r"; TAC:.* (?:>>|<<) ", body))
                self.assertEqual(shifts, 1 if mode.startswith("ra") and cases[program.family][2] else 2,
                                 (program.name, mode))
                if mode.startswith("ra") and cases[program.family][2]:
                    self.assertIn("; reload arg1 from spill slot into BC", body)
                for a in (0, 1, 127, 128, 255, 256, 0x7fff, 0x8000, 65535):
                    for b in (0, 1, 3, 7, 8, 15, 255, 65535):
                        initial = bytes((index*37+a+b)&255 for index in range(128))
                        expected_buffer = bytearray(initial)
                        if program.family == "call":
                            expected_buffer[0] = (initial[0]+1)&255
                        if program.family == "store":
                            expected_buffer[1] = a&255
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], initial,
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        self.assertEqual((result["value"], result["buffer"], result["unmapped_instructions"]),
                                         (cases[program.family][1](a, b)&65535, expected_buffer, 0),
                                         (program.name, mode, a, b))

    def test_bounded_known_bits(self):
        cases = {
            "mask7": ("return a>>(b&7);", lambda a, b: a>>(b&7), True),
            "mask31": ("return a<<(b&31);", lambda a, b: a<<(b&31), True),
            "mask255": ("return a>>(b&255);", lambda a, b: a>>(b&255), True),
            "reverse": ("return a>>(255&b);", lambda a, b: a>>(b&255), True),
            "high": ("return a>>(b>>8);", lambda a, b: a>>(b>>8), True),
            "or": ("return a<<((b&31)|1);", lambda a, b: a<<((b&31)|1), True),
            "xor": ("return a>>((b&31)^16);", lambda a, b: a>>((b&31)^16), True),
            "and": ("return a>>((b&31)&15);", lambda a, b: a>>((b&31)&15), True),
            "byte": ("u8 count=b; return a>>count;", lambda a, b: a>>(b&255), True),
            "signed": ("s16 count=b&127; return a>>count;", lambda a, b: a>>(b&127), True),
            "wide": ("return a>>(b&511);", lambda a, b: a>>(b&511), False),
            "unknown": ("return a>>b;", lambda a, b: a>>b, False),
            "carry": ("return a>>((b&255)+1);", lambda a, b: a>>((b&255)+1), False),
            "call_before": ("return a>>(known_tick(b)&31);", lambda a, b: a>>(b&31), True),
            "call_after": ("u16 count=b&31; known_tick(b); return a>>count;",
                           lambda a, b: a>>(b&31), False),
            "alias": ("u16 count=b&31; known_change(&count); return a>>count;", lambda a, b: 0, False),
            "branch": ("u16 count=b&31; if (b&256) { count=256; } return a>>count;",
                       lambda a, b: 0 if b&256 else a>>(b&31), False),
            "asm": ('u16 count=b&31; __asm("NOP"); return a>>count;', lambda a, b: a>>(b&31), False),
            "index": ("return a+ra_buffer[b&63];", lambda a, b: a+((b&63)*37+a+b)%256, None),
            "index_high": ("return a+ra_buffer[b>>3];", lambda a, b: a+((b>>3)*37+a+b)%256, None),
            "word_index": ("u16 words[8]={3,257,511,1023,32767,32768,0x9234,65535}; return a+words[b&7];",
                           lambda a, b: a+(3,257,511,1023,32767,32768,0x9234,65535)[b&7], None),
            "word_value": ("return (a&255)-(b>>8);", lambda a, b: (a&255)-(b>>8), None),
            "sign_extend": ("s8 small=a; s16 value=small; return b-value;",
                            lambda a, b: b-((a&255)-(256 if a&128 else 0)), None),
            "legacy": ("u16 count=b&7; " + " ".join(
                f"a=(a+{i+1})^{0x1234+i};" for i in range(8)) + " return a>>count;", None, True),
        }
        programs = [Program(f"known_bits_{name}", name, 0, 0, [2, 2], 2, [],
                            f"u16 known_bits_{name}(u16 a,u16 b) {{ {body} }}")
                    for name, (body, _, _) in cases.items()]
        source = ("u8 ra_buffer[128];\n"
                  "u16 known_tick(u16 value) { ra_buffer[0]=ra_buffer[0]+1; return value; }\n"
                  "void known_change(u16 *value) { value[0]^=256; }\n" +
                  "\n".join(program.source for program in programs)+"\nvoid main(void) {}\n")
        directory = self.output / "bounded-known-bits"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs, source)
        for program in programs:
            for a in (0, 1, 127, 128, 255, 256, 0x9234, 65535):
                counts = (0, 1, 7, 15, 16, 31, 127, 128, 255, 256, 257, 511)
                if program.family not in ("unknown", "index_high"):
                    counts += (0xff00, 65535)
                for b in counts:
                    initial = bytes((index*37+a+b)&255 for index in range(128))
                    expected_buffer = bytearray(initial)
                    if program.family.startswith("call_"):
                        expected_buffer[0] = (expected_buffer[0]+1)&255
                    oracle = cases[program.family][1]
                    if oracle is None:
                        expected = a
                        for i in range(8):
                            expected = ((expected+i+1)^(0x1234+i))&65535
                        expected >>= b&7
                    else:
                        expected = oracle(a, b)&65535
                    for mode, image in images.items():
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers,
                                         instruction_limit=30000, frame_bytes=2048)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected, expected_buffer, []), (mode, program.family, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
        for mode, image in images.items():
            if mode not in ("ra", "ra_quiet"):
                continue
            assembly = (directory / mode / "main.asm").read_text()
            for program in programs:
                selected = cases[program.family][2]
                if selected is not None:
                    section = assembly.split(f'.SECTION "{program.name}"')[1].split(".ENDS")[0]
                    self.assertEqual("bounded variable word shift" in section, selected, (mode, program.family))

    def test_frameless_register_leaves(self):
        cases = {
            "zero": ("void", "return 0x9234;", "", lambda a, b: 0x9234, True),
            "first": ("u16 a,u16 b", "return a;", "a,b", lambda a, b: a, True),
            "second": ("u16 a,u16 b", "return b;", "a,b", lambda a, b: b, True),
            "add": ("u16 a,u16 b", "return a+b;", "a,b", lambda a, b: a+b, True),
            "reverse_add": ("u16 a,u16 b", "return b+a;", "a,b", lambda a, b: a+b, True),
            "reverse_sub": ("u16 a,u16 b", "return b-a;", "a,b", lambda a, b: b-a, False),
            "sub_constant": ("u16 a", "return a-0x1234;", "a", lambda a, b: a-0x1234, True),
            "constant_sub": ("u16 a", "return 0x1234-a;", "a", lambda a, b: 0x1234-a, True),
            "and": ("u16 a,u16 b", "return a&b;", "a,b", lambda a, b: a&b, True),
            "or": ("u16 a,u16 b", "return a|b;", "a,b", lambda a, b: a|b, True),
            "xor": ("u16 a,u16 b", "return a^b;", "a,b", lambda a, b: a^b, True),
            "complement": ("u16 a", "return ~a;", "a", lambda a, b: a^65535, True),
            "same_add": ("u16 a", "return a+a;", "a", lambda a, b: a+a, True),
            "mask": ("u16 a", "return a&0xff00;", "a", lambda a, b: a&0xff00, True),
            "signed": ("s16 a,s16 b", "return a^b;", "a,b", lambda a, b: a^b, True),
            "sub": ("u16 a,u16 b", "return a-b;", "a,b", lambda a, b: a-b, True),
            "multiply": ("u16 a,u16 b", "return a*b;", "a,b", lambda a, b: a*b, True),
            "divide": ("u16 a,u16 b", "return a/b;", "a,b",
                       lambda a, b: a//b if b else 65535, False),
            "modulo": ("u16 a,u16 b", "return a%b;", "a,b",
                       lambda a, b: a%b if b else a, False),
            "pressure": ("u16 a,u16 b", "return (a+b)*(a^b);", "a,b",
                         lambda a, b: (a+b)*(a^b), False),
            "three": ("u16 a,u16 b,u16 c", "return c;", "a,b,a^b", lambda a, b: a^b, False),
            "byte": ("u8 a,u8 b", "return a^b;", "a,b", lambda a, b: (a^b)&255, False),
            "narrow": ("u16 a", "u8 small=a; return small;", "a", lambda a, b: a&255, False),
            "branch": ("u16 a,u16 b", "if (a<b) { return a; } return b;", "a,b", min, False),
            "call": ("u16 a", "return leaf_first(a,7);", "a", lambda a, b: a, False),
            "alias": ("u16 a", "leaf_escape(&a); return a;", "a", lambda a, b: a^0x1234, False),
            "copy": ("u16 a,u16 b", "u8 data[8]={3,5,7,11,13,17,19,23}; return a^data[b&7];",
                     "a,b", lambda a, b: a^(3,5,7,11,13,17,19,23)[b&7], False),
        }
        helpers = ("u8 ra_buffer[128];\n"
                   "void leaf_escape(u16 *value) { value[0]^=0x1234; }\n")
        helpers += "\n".join(f"u16 leaf_{name}({parameters}) {{ {body} }}"
                             for name, (parameters, body, _, _, _) in cases.items()) + "\n"
        programs = [Program(f"microleaf_{name}", name, 0, 0, [2, 2], 2, [],
                            f"u16 microleaf_{name}(u16 a,u16 b) {{ return leaf_{name}({arguments}); }}")
                    for name, (_, _, arguments, _, _) in cases.items()]
        source = helpers + "\n".join(program.source for program in programs) + "\nvoid main(void) {}\n"
        expected_code = {"zero": bytes.fromhex("213492c9"), "first": bytes.fromhex("c9"),
                         "second": bytes.fromhex("6960c9"), "add": bytes.fromhex("09c9"),
                         "reverse_add": bytes.fromhex("09c9"), "sub": bytes.fromhex("a7ed42c9")}
        calls, cohorts = 0, []
        for cohort, flags, suffix in (("ordinary", (), ""), ("shared", ("-share-arithmetic",), ""),
                                      ("opaque", (), 'void leaf_noise(void) { __asm("NOP"); }\n')):
            directory = self.output / f"frameless-leaves-{cohort}"
            images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                       os.environ.get("SAMESAMECL") or "samesamecl", programs,
                                       source+suffix, linker_flags=flags)
            cohorts.append((cohort, images))
            for program in programs:
                for a in (0, 1, 127, 128, 255, 256, 32768, 65535):
                    for b in (0, 1, 255, 0x9234):
                        initial = bytes((index*37+a+b)&255 for index in range(128))
                        expected = cases[program.family][3](a, b)&65535
                        for mode, image in images.items():
                            result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                             initial, image.symbols["ra_buffer"], 2, image.transfers,
                                             frame_bytes=2048)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                             (expected, initial, []), (cohort, mode, program.name, a, b))
                            self.assertEqual(result["unmapped_instructions"], 0)
                            calls += 1
        for cohort, images in cohorts:
            for mode, image in images.items():
                for name, (_, _, arguments, _, eligible) in cases.items():
                    selected = eligible and cohort != "opaque"
                    marker = f"ssc.regleaf.leaf_{name}"
                    self.assertEqual(marker in image.symbols, selected, (cohort, mode, name))
                    if selected:
                        count = len(arguments.split(",")) if arguments else 0
                        entry = image.symbols[f"ssc.autocall.v1.w{count}.leaf_{name}"]
                        self.assertEqual(entry, image.symbols[marker])
                        self.assertEqual(image.staticMetrics["functions"][f"leaf_{name}"]["frame_bytes"], 0)
                        if name in expected_code:
                            code = expected_code[name]
                            self.assertEqual(image.rom[entry:entry+len(code)], code, (cohort, mode, name))
                    else:
                        self.assertGreater(image.staticMetrics["functions"][f"leaf_{name}"]["frame_bytes"], 0)
                        if cohort == "opaque" and eligible:
                            self.assertIn(f"leaf_{name}", image.symbols)
        self.assertEqual(calls, len(programs)*32*5*3)

    def test_direct_call_argument_placement(self):
        signed = lambda value: (value ^ 0x8000)-0x8000
        cases = {
            "one": ("dc_one", 1, "return dc_one(direct_params[0]);", lambda a, b: a^0x9234, True),
            "two": ("dc_two", 2, "return dc_two(direct_params[0],direct_params[1]);", lambda a, b: a+3*b, True),
            "three": ("dc_three", 3, "return dc_three(direct_params[0],direct_params[1],direct_params[2]);",
                      lambda a, b: a+3*b+5*(a^b), True),
            "reverse": ("dc_three", 3, "return dc_three(direct_params[2],direct_params[1],direct_params[0]);",
                        lambda a, b: (a^b)+3*b+5*a, True),
            "constant_first": ("dc_three", 3, "return dc_three(32768,direct_params[0],direct_params[1]);",
                               lambda a, b: 32768+3*a+5*b, True),
            "constant_middle": ("dc_three", 3, "return dc_three(direct_params[0],32768,direct_params[1]);",
                                lambda a, b: a+3*32768+5*b, True),
            "constant_last": ("dc_three", 3, "return dc_three(direct_params[0],direct_params[1],32768);",
                              lambda a, b: a+3*b+5*32768, True),
            "address": ("dc_pointer", 2, "return dc_pointer(direct_params[0],&direct_params[1]);",
                        lambda a, b: a+b, True),
            "address_first": ("dc_pointer_first", 2, "return dc_pointer_first(&direct_params[2],direct_params[1]);",
                              lambda a, b: (a^b)+b, True),
            "signed": ("dc_signed", 3, "return dc_signed(direct_params[0],direct_params[1],direct_params[2]);",
                       lambda a, b: min(signed(a), signed(b))+(a^b), True),
            "dead": ("dc_dead", 3, "return dc_dead(direct_params[0],direct_params[1],direct_params[2]);",
                     lambda a, b: a, True),
            "snapshot": ("dc_three", 3, "u16 early=direct_params[0]; direct_params[0]=b; "
                         "return dc_three(early,direct_params[1],direct_params[2]);",
                         lambda a, b: a+3*b+5*(a^b), False),
            "nested": ("dc_three", 3, "return dc_three(direct_params[0],dc_tick(direct_params[1]),direct_params[2]);",
                       lambda a, b: a+3*(b^0x31)+5*(a^b), False),
            "alias": ("dc_three", 3, "return dc_three(direct_params[0],dc_mutate(&direct_params[0],b),direct_params[2]);",
                      lambda a, b: a+3*(b^0x55)+5*(a^b), False),
            "byte": ("dc_three", 3, "u8 first=a; return dc_three(first,direct_params[1],direct_params[2]);",
                     lambda a, b: (a&255)+3*b+5*(a^b), False),
            "signed_byte": ("dc_three", 3, "s8 first=a; return dc_three(first,direct_params[1],direct_params[2]);",
                            lambda a, b: (a&255)-(256 if a&128 else 0)+3*b+5*(a^b), False),
            "mixed": ("dc_mixed", 2, "return dc_mixed(direct_params[0],direct_params[1]);",
                      lambda a, b: (a&255)+3*b, False),
            "nonleaf": ("dc_nonleaf", 3, "return dc_nonleaf(direct_params[0],direct_params[1],direct_params[2]);",
                        lambda a, b: (a^0x31)+3*b+5*(a^b), False),
            "nonleaf_constant_first": ("dc_nonleaf", 3, "return dc_nonleaf(32768,direct_params[0],direct_params[1]);",
                                       lambda a, b: (32768^0x31)+3*a+5*b, False),
            "nonleaf_constant_middle": ("dc_nonleaf", 3, "return dc_nonleaf(direct_params[0],32768,direct_params[1]);",
                                        lambda a, b: (a^0x31)+3*32768+5*b, False),
            "nonleaf_constant_last": ("dc_nonleaf", 3, "return dc_nonleaf(direct_params[0],direct_params[1],32768);",
                                      lambda a, b: (a^0x31)+3*b+5*32768, False),
            "ignored": ("dc_three", 3, "dc_three(direct_params[0],direct_params[1],direct_params[2]); return a^b;",
                        lambda a, b: a^b, True),
            "four": ("dc_four", 4, "return dc_four(direct_params[0],direct_params[1],direct_params[2],direct_params[3]);",
                     lambda a, b: a+3*b+5*(a^b)+7*(a+b), False),
            "dynamic": ("dc_three", 3, "return dc_three(direct_params[a&3],direct_params[1],direct_params[2]);",
                        lambda a, b: (a, b, a^b, (a+b)&65535)[a&3]+3*b+5*(a^b), False),
            "reuse": ("dc_three", 3, "u16 first=direct_params[0]; return dc_three(first,first,direct_params[2]);",
                      lambda a, b: 4*a+5*(a^b), False),
            "local_address": ("dc_pointer", 2, "u16 pair[2]; pair[0]=a; pair[1]=b; "
                              "return dc_pointer(direct_params[0],pair);", lambda a, b: 2*a, False),
            "effects_dead": ("dc_dead", 3, "return dc_dead(direct_params[0],dc_tick(direct_params[1]),direct_params[2]);",
                             lambda a, b: a, False),
        }
        helpers = """u8 ra_buffer[128];
u16 direct_params[4];
u16 dc_one(u16 a) { return a^0x9234; }
u16 dc_two(u16 a,u16 b) { return a+3*b; }
u16 dc_three(u16 a,u16 b,u16 c) { return a+3*b+5*c; }
u16 dc_pointer(u16 a,u16 *p) { return a+p[0]; }
u16 dc_pointer_first(u16 *p,u16 b) { return p[0]+b; }
u16 dc_signed(s16 a,s16 b,u16 c) { if(a<b) { return a+c; } return b+c; }
u16 dc_dead(u16 a,u16 b,u16 c) { return a; }
u16 dc_tick(u16 value) { ra_buffer[0]^=value; return value^0x31; }
u16 dc_mutate(u16 *p,u16 value) { p[0]=value; return value^0x55; }
u16 dc_mixed(u8 a,u16 b) { return a+3*b; }
u16 dc_nonleaf(u16 a,u16 b,u16 c) { return dc_tick(a)+3*b+5*c; }
u16 dc_four(u16 a,u16 b,u16 c,u16 d) { return a+3*b+5*c+7*d; }
"""
        setup = "direct_params[0]=a; direct_params[1]=b; direct_params[2]=a^b; direct_params[3]=a+b;"
        programs = [Program(f"direct_{name}", name, 0, count, [2, 2], 2, [],
                            f"u16 direct_{name}(u16 a,u16 b) {{ {setup} {body} }}")
                    for name, (_, count, body, _, _) in cases.items()]
        directory = self.output / "direct-call-arguments"
        for policy in ("inline", "shared"):
            images = compile_generated(
                directory / policy, os.environ.get("SAMESAMECC") or "samesamecc",
                os.environ.get("SAMESAMECL") or "samesamecl", programs,
                helpers+"\n".join(program.source for program in programs)+"\nvoid main(void) {}\n",
                linker_flags=("-share-arithmetic",) if policy == "shared" else ())
            for mode, image in images.items():
                self.assertEqual(image.symbols["direct_params"], image.symbols["ra_buffer"]+128)
                assembly = (directory / policy / mode / "linked.sms.combined.asm").read_text()
                for program in programs:
                    callee, count, _, oracle, eligible = cases[program.family]
                    body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                    selected = (bool(re.search(r"; direct argument \d+ to "+callee+r"\b", body)) and
                                f"ssc.autocall.v1.w{count}.{callee}" in image.symbols)
                    self.assertEqual(selected, eligible and mode in ("ra", "ra_quiet"), (program.name, policy, mode))
                    legacy = (bool(re.search(r"; direct legacy argument \d+ to "+callee+r"\b", body)) and
                              f"ssc.autocall.v1.w{count}.{callee}" not in image.symbols)
                    self.assertEqual(legacy, program.family.startswith("nonleaf") and mode in ("ra", "ra_quiet"),
                                     (program.name, policy, mode))
                    for a in (0, 1, 127, 128, 255, 256, 0x8000, 65535):
                        for b in (0, 7, 255, 0x9234, 65535):
                            initial = bytes((index*37+a+b)&255 for index in range(136))
                            expected = bytearray(initial)
                            values = [b if program.family in ("snapshot", "alias") else a, b, a^b, (a+b)&65535]
                            expected[128:] = b"".join(value.to_bytes(2, "little") for value in values)
                            if program.family.startswith("nonleaf"):
                                expected[0] ^= (32768 if program.family == "nonleaf_constant_first" else a)&255
                            elif program.family in ("nested", "effects_dead"):
                                expected[0] ^= b&255
                            result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], initial,
                                             image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                            self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                             (oracle(a, b)&65535, expected, []), (program.name, policy, mode, a, b))
                            self.assertEqual(result["unmapped_instructions"], 0)

    def test_direct_call_argument_objects(self):
        compiler = os.environ.get("SAMESAMECC") or "samesamecc"
        linker = os.environ.get("SAMESAMECL") or "samesamecl"
        for contract in ("leaf", "banked", "assembly", "caller_asm", "nonleaf", "legacy", "explicit", "external_data"):
            for mode, flags in (("ra", ("-ra",)), ("forced", ("-ra-all-spill",))):
                work = self.output / f"direct-objects-{contract}-{mode}"
                work.mkdir()
                annotation = " __z80_regcall" if contract == "explicit" else ""
                barrier = '__asm("LD BC,4660");' if contract == "caller_asm" else ""
                storage = "extern " if contract == "external_data" else ""
                caller = (f"u8 ra_buffer[128]; {storage}u16 direct_params[3];\n"
                          f"u16 dc_external(u16 a,u16 b,u16 c){annotation};\n"
                          "u16 direct_object(u16 a,u16 b) { direct_params[0]=a; direct_params[1]=b; "
                          f"direct_params[2]=a^b; {barrier} return dc_external(direct_params[0],direct_params[1],direct_params[2]); }}\n"
                          "void main(void) {}\n")
                body = ('__asm("NOP"); return a+3*b+5*c;' if contract == "assembly" else
                        "return dc_inner(a)+3*b+5*c;" if contract == "nonleaf" else "return a+3*b+5*c;")
                callee = ("u16 dc_inner(u16 a) { return a+1; }\n" if contract == "nonleaf" else "")
                if contract == "external_data":
                    callee += "u16 direct_params[3];\n"
                callee += f"u16 dc_external(u16 a,u16 b,u16 c){annotation} {{ {body} }}\n"
                for unit, source in (("caller", caller), ("callee", callee)):
                    (work / f"{unit}.ssc").write_text(source)
                    bank = ("-b", "3", "-s", "2") if contract == "banked" and unit == "callee" else ()
                    run(compiler, ["-mz80", "-rs", "3", *bank, *flags, "-o", f"{unit}.asm", f"{unit}.ssc"],
                        work, f"{unit}.compiler.log")
                if contract == "legacy":
                    path = work / "callee.asm"
                    path.write_text(re.sub(r"^\s*; ssc_regcalls[^\n]*\n", "", path.read_text(), flags=re.M))
                run(linker, ["-lsms", "-ro", "4", "-v", "-o", "linked.sms", "caller.asm", "callee.asm"],
                    work, "linker.log")
                image = read_image(work)
                fast = contract in ("leaf", "banked", "explicit", "external_data")
                entry = f"ssc.{'regcall' if contract == 'explicit' else 'autocall'}.v1.w3.dc_external"
                if contract != "caller_asm":
                    self.assertEqual(entry in image.symbols, fast, (contract, mode))
                assembly = (work / "caller.asm").read_text()
                candidate = "; direct argument" in assembly
                self.assertEqual(candidate, mode == "ra" and contract not in ("caller_asm", "external_data"),
                                 (contract, mode))
                legacy = "; direct legacy argument" in assembly and entry not in image.symbols
                self.assertEqual(legacy, mode == "ra" and contract in ("assembly", "nonleaf", "legacy"),
                                 (contract, mode))
                self.assertEqual(image.symbols["direct_params"], image.symbols["ra_buffer"]+128)
                if contract == "banked":
                    image = mapped_routine_image("direct", entry, mode, image)
                for a in (0, 1, 127, 128, 255, 256, 0x8000, 65535):
                    for b in (0, 7, 255, 0x9234, 65535):
                        initial = bytes((index*37+a+b)&255 for index in range(134))
                        expected = bytearray(initial)
                        expected[128:] = b"".join(value.to_bytes(2, "little") for value in (a, b, a^b))
                        result = execute(image.rom, image.symbols["direct_object"], [a, b], [2, 2], initial,
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        value = (a+3*b+5*(a^b)+(contract == "nonleaf"))&65535
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (value, expected, []), (contract, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)

    def test_automatic_register_calls(self):
        cases = {
            "zero": ("return er_zero()+a+b;", lambda a, b: 0x9234+a+b),
            "one": ("return er_one(a)+b;", lambda a, b: (a^0x1234)+b),
            "two": ("return er_two(a,b);", lambda a, b: a+3*b),
            "three": ("return er_three(a,b,7);", lambda a, b: a+3*b+35),
            "extend": ("s8 left=a; u8 right=b; return er_three(left,right,0);",
                       lambda a, b: (a&255)-(256 if a&128 else 0)+3*(b&255)),
            "nested": ("return er_three(er_one(a),er_two(b,7),0);", lambda a, b: (a^0x1234)+3*(b+21)),
            "signed": ("return er_signed(a,b);", lambda a, b: min((a^0x8000)-0x8000, (b^0x8000)-0x8000)),
            "pointer": ("u16 data[2]; data[0]=a; data[1]=b; return er_pointer(data);", lambda a, b: a^b),
            "math": ("return er_math(a,b)^er_math_plus(b,a);",
                     lambda a, b: ((a*b+a//(b|1))&65535)^((a*b+b//(a|1)+1)&65535)),
            "ignored": ("er_three(a,b,7); return a^b;", lambda a, b: a^b),
        }
        helpers = """u8 ra_buffer[128];
u16 er_zero(void) { return 0x9234; }
u16 er_one(u16 a) { return a^0x1234; }
u16 er_two(u16 a,u16 b) { return a+3*b; }
u16 er_three(u16 a,u16 b,u16 c) { return a+3*b+5*c; }
s16 er_signed(s16 a,s16 b) { if(a<b) { return a; } return b; }
u16 er_pointer(u16 *data) { return data[0]^data[1]; }
u16 er_math(u16 a,u16 b) { return a*b+a/(b|1); }
u16 er_math_plus(u16 a,u16 b) { return a*b+a/(b|1)+1; }
"""
        programs = [Program(f"explicit_call_{name}", name, 0, 0, [2, 2], 2, [],
                            f"u16 explicit_call_{name}(u16 a,u16 b) {{ {body} }}")
                    for name, (body, _) in cases.items()]
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        directory = self.output / "automatic-register-calls"
        images = compile_generated(directory, compiler, linker, programs,
                                   helpers+"\n".join(p.source for p in programs)+"\nvoid main(void) {}\n",
                                   linker_flags=("-share-arithmetic",))
        for mode, image in images.items():
            assembly = (directory / mode / "main.asm").read_text()
            for name, count in (("zero", 0), ("one", 1), ("two", 2), ("three", 3),
                                ("signed", 2), ("pointer", 1), ("math", 2), ("math_plus", 2)):
                self.assertIn(f"ssc.autocall.v1.w{count}.er_{name}", image.symbols)
                self.assertNotIn(f"er_{name}", image.symbols)
                self.assertNotIn(f"ssc.regbody.er_{name}", image.symbols)
            self.assertIn("CALL ssc.arithmetic_", (directory / mode / "linked.sms.combined.asm").read_text())
            self.assertNotIn(".DEFINE ssc.legacy.", (directory / mode / "linked.sms.combined.asm").read_text())
            for program in programs:
                for a in (0, 1, 127, 128, 255, 0x7fff, 0x8000, 65535):
                    for b in (0, 7, 255, 0x9234):
                        initial = bytes((index*37+a+b)&255 for index in range(128))
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], initial,
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (cases[program.family][1](a, b)&65535, initial, []),
                                         (program.name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)

    def test_private_register_call_adapters(self):
        helpers = """u8 ra_buffer[128];
static u16 pr_zero(void) { return 0x9234; }
static u16 pr_one(u16 a) { return a^0x1234; }
static u16 pr_two(u16 a,u16 b) { return a+3*b; }
static u16 pr_three(u16 a,u16 b,u16 c) { return a+3*b+5*c; }
static u16 pr_discard(u16 a,u16 b,u16 c) { ra_buffer[0]=a; return a+3*b+5*c; }
"""
        cases = [
            ("zero", "return pr_zero()+a+b;", lambda a, b: 0x9234+a+b),
            ("one", "return pr_one(a)+b;", lambda a, b: (a^0x1234)+b),
            ("two", "return pr_two(a,b);", lambda a, b: a+3*b),
            ("three", "return pr_three(a,b,7);", lambda a, b: a+3*b+35),
            ("ignored", "pr_discard(a,b,7); return a^b;", lambda a, b: a^b),
        ]
        programs = [Program(f"private_call_{name}", name, 0, 0, [2, 2], 2, [],
                            f"u16 private_call_{name}(u16 a,u16 b) {{ {body} }}")
                    for name, body, _ in cases]
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        directory = self.output / "private-register-calls"
        images = compile_generated(directory, compiler, linker, programs,
                                   helpers+"\n".join(p.source for p in programs)+"\nvoid main(void) {}\n")
        for mode, image in images.items():
            assembly = (directory / mode / "main.asm").read_text()
            for name in ("zero", "one", "two", "three", "discard"):
                symbol = re.search(rf'\.SECTION "(pr_{name}__static__[^"]+)"', assembly)[1]
                count = {"zero": 0, "one": 1, "two": 2, "three": 3, "discard": 3}[name]
                self.assertIn(f"ssc.autocall.v1.w{count}.{symbol}", image.symbols)
                self.assertNotIn(symbol, image.symbols)
                self.assertNotIn(f"ssc.regbody.{symbol}", image.symbols)
            for program, (_, _, oracle) in zip(programs, cases, strict=True):
                for a in (0, 1, 0x8000, 65535):
                    for b in (0, 255, 0x9234):
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], bytes(128),
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        self.assertEqual(result["value"], oracle(a, b)&65535, (mode, program.name, a, b))
                        self.assertEqual(result["buffer"], bytes([a&255])+bytes(127) if program.family == "ignored" else bytes(128))
                        self.assertEqual(result["port_writes"], [])
                        self.assertEqual(result["unmapped_instructions"], 0)
        for boundary, prefix, body in (
            ("asm", 'void pr_noise(void) { __asm("NOP"); }\n', "return pr_leaf(a)+pr_leaf(b);"),
            ("address", "", "u16 address=&pr_leaf; return pr_leaf(a)+pr_leaf(b)+(address!=0);"),
            ("argument", "u16 pr_keep(u16 value) { return value!=0; }\n",
             "return pr_leaf(a)+pr_leaf(b)+pr_keep(&pr_leaf);"),
        ):
            source = ("u8 ra_buffer[128]; static u16 pr_leaf(u16 a) { return a^0x1234; }\n"+prefix+
                      f"u16 private_boundary(u16 a,u16 b) {{ {body} }}\nvoid main(void) {{}}\n")
            program = Program("private_boundary", boundary, 0, 0, [2, 2], 2, [], "")
            work = self.output / f"private-boundary-{boundary}"
            guarded = compile_generated(work, compiler, linker, [program], source)
            for mode, image in guarded.items():
                assembly = (work / mode / "main.asm").read_text()
                symbol = re.search(r'\.SECTION "(pr_leaf__static__[^"]+)"', assembly)[1]
                self.assertIn(symbol, image.symbols, (boundary, mode))
                self.assertIn(f"ssc.regbody.{symbol}", image.symbols)
                result = execute(image.rom, image.symbols[program.name], [65535, 0x9234], [2, 2], bytes(128),
                                 image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                self.assertEqual(result["value"], ((65535^0x1234)+(0x9234^0x1234)+(boundary!="asm"))&65535)

    def test_automatic_register_call_objects(self):
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        flags = ((), ("-no-ra",), ("-ra-all-spill",), ("-ra",), ("-ra", "-z80-regcalls"))
        for caller_index, caller_flags in enumerate(flags):
            for callee_index, callee_flags in enumerate(flags):
                for kind, count, parameters, body, arguments, oracle in (
                        ("full", 3, "u16 a,u16 b,u16 c", "a+3*b+5*c", "a,b,7", lambda a, b: a+3*b+35),
                        ("sub", 2, "u16 a,u16 b", "a-b", "a,b", lambda a, b: a-b),
                        ("dead", 3, "u16 a,u16 b,u16 c", "a*b", "a,b,7", lambda a, b: a*b),
                        ("right6", 2, "u16 a,u16 b", "a>>6", "a,b", lambda a, b: a>>6),
                        ("right7", 2, "u16 a,u16 b", "a>>7", "a,b", lambda a, b: a>>7),
                        ("shift", 3, "u16 a,u16 b,u16 c", "(a<<b)>>b", "a,b&15,7",
                         lambda a, b: ((a<<(b&15))&65535)>>(b&15))):
                    work = self.output / f"automatic-objects-{caller_index}-{callee_index}-{kind}"
                    work.mkdir()
                    (work / "callee.ssc").write_text(f"u16 cross_leaf({parameters}) {{ return {body}; }}\n")
                    (work / "caller.ssc").write_text(
                        f"u8 ra_buffer[128]; u16 cross_leaf({parameters});\n"
                        f"u16 cross_caller(u16 a,u16 b) {{ return cross_leaf({arguments}); }}\nvoid main(void) {{}}\n")
                    for unit, options in (("caller", caller_flags), ("callee", callee_flags)):
                        run(compiler, ["-mz80", "-rs", "3", *options, "-o", f"{unit}.asm", f"{unit}.ssc"],
                            work, f"{unit}.compiler.log")
                    run(linker, ["-lsms", "-v", "-o", "linked.sms", "caller.asm", "callee.asm"], work, "linker.log")
                    image = read_image(work)
                    self.assertNotIn("cross_leaf", image.symbols)
                    selected = kind in ("sub", "right6", "right7") or (
                        kind in ("shift", "dead") and os.environ.get("RA_BASELINE") != "1")
                    self.assertEqual("ssc.regleaf.cross_leaf" in image.symbols, selected)
                    if selected:
                        self.assertEqual(image.staticMetrics["functions"]["cross_leaf"]["frame_bytes"], 0)
                    for a, b in ((0, 7), (0x8000, 65535), (65535, 0x9234)):
                        result = execute(image.rom, image.symbols["cross_caller"], [a, b], [2, 2], bytes(128),
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        self.assertEqual(result["value"], oracle(a, b)&65535, (caller_index, callee_index, kind, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)

    def test_register_call_cross_object_boundaries(self):
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        cases = (
            ("address", "u16 cross_leaf(u16 a,u16 b,u16 c) { return a+3*b+5*c; }",
             "u16 address=&cross_leaf; return cross_leaf(a,b,7)+cross_leaf(b,a,9)+(address!=0);",
             lambda a, b: 4*a+4*b+81, True, True),
            ("ignored", "u16 saved; u16 cross_leaf(u16 a,u16 b,u16 c) { saved=a; return a+3*b+5*c; }\n"
             "u16 cross_read(void) { return saved; }",
             "cross_leaf(a,b,7); return cross_read();", lambda a, b: a, False, True),
            ("nonleaf", "u16 inner(u16 a) { return a+1; }\n"
             "u16 cross_leaf(u16 a,u16 b,u16 c) { return inner(a)+3*b+5*c; }",
             "return cross_leaf(a,b,7);", lambda a, b: a+3*b+36, True, False),
            ("asm", 'u16 cross_leaf(u16 a,u16 b,u16 c) { __asm("NOP"); return a+3*b+5*c; }',
             "return cross_leaf(a,b,7);", lambda a, b: a+3*b+35, True, False),
        )
        for name, definition, body, oracle, legacy, fast in cases:
            for mode, flags in (("default", ()), ("ra", ("-ra",))):
                work = self.output / f"register-call-cross-{name}-{mode}"
                work.mkdir()
                (work / "callee.ssc").write_text(definition+"\n")
                (work / "caller.ssc").write_text(
                    "u8 ra_buffer[128]; u16 cross_leaf(u16 a,u16 b,u16 c); u16 cross_read(void);\n"
                    f"u16 boundary(u16 a,u16 b) {{ {body} }}\nvoid main(void) {{}}\n")
                for unit in ("caller", "callee"):
                    run(compiler, ["-mz80", "-rs", "3", *flags, "-o", f"{unit}.asm", f"{unit}.ssc"],
                        work, f"{unit}.compiler.log")
                run(linker, ["-lsms", "-v", "-o", "linked.sms", "caller.asm", "callee.asm"], work, "linker.log")
                image = read_image(work)
                self.assertEqual("cross_leaf" in image.symbols, legacy, (name, mode))
                self.assertEqual("ssc.autocall.v1.w3.cross_leaf" in image.symbols, fast, (name, mode))
                for a, b in ((0, 7), (0x8000, 65535), (65535, 0x9234)):
                    expected_buffer = bytes(128)
                    if name == "ignored":
                        self.assertEqual(image.symbols["saved"], image.symbols["ra_buffer"]+128)
                        expected_buffer += a.to_bytes(2, "little")
                    result = execute(image.rom, image.symbols["boundary"], [a, b], [2, 2], bytes(len(expected_buffer)),
                                     image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                    self.assertEqual(result["value"], oracle(a, b)&65535, (name, mode, a, b))
                    self.assertEqual((result["buffer"], result["port_writes"], result["unmapped_instructions"]),
                                     (expected_buffer, [], 0))

    def test_register_call_metadata_validation(self):
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        work = self.output / "register-call-metadata"
        work.mkdir()
        (work / "main.ssc").write_text(
            "u16 leaf(u16 a) { return a+1; } u16 use(u16 a) { return leaf(a); } void main(void) {}\n")
        run(compiler, ["-mz80", "-rs", "3", "-o", "main.asm", "main.ssc"], work, "compiler.log")
        original = (work / "main.asm").read_text()
        header = "      ; ssc_regcalls 1 0 0 0 -\n"
        self.assertIn(header, original)
        for index, replacement in enumerate((
            "      ; ssc_regcalls 2 0 0 0 -\n",
            "      ; ssc_regcalls 4294967297 0 0 0 -\n",
            "      ; ssc_regcalls 1 4294967296 0 0 -\n",
            "      ; ssc_regcalls 1 0 4294967296 0 -\n",
            "      ; ssc_regcalls 1 0 0 4294967296 -\n",
            "      ; ssc_regcalls 1 5 0 0 -\n",
            "      ; ssc_regcalls 1 0 4 0 -\n",
            "      ; ssc_regcalls 1 0 0 2 -\n",
            "      ; ssc_regcalls 1 0 0 0 - trailing\n",
            "      ; ssc_regcalls 1 0 0\n",
            header+header,
            "",
            header+"      ; ssc_regcalls 1 1 1 0 invalid.name\n",
            header+"      ; ssc_regcalls 1 2 2 1 leaf\n",
            header+"      ; ssc_regcalls 1 1 1 0 leaf\n",
        )):
            (work / "main.asm").write_text(original.replace(header, replacement, 1))
            completed = subprocess.run([linker, "-lsms", "-o", "linked.sms", "main.asm"],
                                       cwd=work, capture_output=True, timeout=30)
            self.assertNotEqual(completed.returncode, 0, index)
            self.assertIn(b"register-call", completed.stdout+completed.stderr, index)

    def test_explicit_register_call_extra_optimization(self):
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        signature = "u16 forced_leaf(u16 a,u16 b,u16 c)"
        source = ("u8 ra_buffer[128]; void noise(void) { __asm(\"NOP\"); }\n"+signature+
                  " { return a+3*b+5*c; }\n"
                  "u16 forced_caller(u16 a,u16 b) { return forced_leaf(a,b,7); }\nvoid main(void) {}\n")
        program = Program("forced_caller", "forced", 0, 0, [2, 2], 2, [], "")
        variants = {}
        for forced in (False, True):
            variants[forced] = compile_generated(
                self.output / f"forced-register-calls-{int(forced)}", compiler, linker, [program],
                source.replace(signature, signature+" __z80_regcall") if forced else source,
            )
        for mode, image in variants[True].items():
            self.assertLess(image.staticMetrics["rom_used_bytes"], variants[False][mode].staticMetrics["rom_used_bytes"])
            self.assertNotIn("forced_leaf", image.symbols)
            self.assertIn("ssc.regcall.v1.w3.forced_leaf", image.symbols)
            self.assertIn("forced_leaf", variants[False][mode].symbols)
            self.assertNotIn("ssc.autocall.v1.w3.forced_leaf", variants[False][mode].symbols)
            for a, b in ((0, 7), (0x8000, 65535), (65535, 0x9234)):
                result = execute(image.rom, image.symbols["forced_caller"], [a, b], [2, 2], bytes(128),
                                 image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                self.assertEqual((result["value"], result["buffer"], result["unmapped_instructions"]),
                                 ((a+3*b+35)&65535, bytes(128), 0), (mode, a, b))
        for index, (declaration, definition, valid) in enumerate((
            ("u16 forced_leaf(u16 a) __z80_regcall;", "u16 forced_leaf(u16 a) __z80_regcall { return a+1; }", True),
            ("u16 forced_leaf(u16 a);", "u16 forced_leaf(u16 a) __z80_regcall { return a+1; }", False),
            ("u16 forced_leaf(u16 a) __z80_regcall;", "u16 forced_leaf(u16 a) { return a+1; }", False),
            ("u16 forced_leaf(u16 a) __z80_regcall;", "u16 forced_leaf(u16 a,u16 b) __z80_regcall { return a+b; }", False),
        )):
            work = self.output / f"forced-register-objects-{index}"
            work.mkdir()
            (work / "callee.ssc").write_text(definition+"\n")
            (work / "caller.ssc").write_text(
                "u8 ra_buffer[128]; "+declaration+
                "\nu16 forced_caller(u16 a,u16 b) { return forced_leaf(a); }\nvoid main(void) {}\n")
            for unit in ("caller", "callee"):
                run(compiler, ["-mz80", "-rs", "3", "-o", f"{unit}.asm", f"{unit}.ssc"],
                    work, f"{unit}.compiler.log")
            completed = subprocess.run([linker, "-lsms", "-v", "-o", "linked.sms", "caller.asm", "callee.asm"],
                                       cwd=work, capture_output=True, timeout=30)
            self.assertEqual(completed.returncode == 0, valid, index)
            if valid:
                (work / "linker.log").write_bytes(completed.stdout+completed.stderr)
                image = read_image(work)
                self.assertNotIn("forced_leaf", image.symbols)
                result = execute(image.rom, image.symbols["forced_caller"], [65535, 0], [2, 2], bytes(128),
                                 image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                self.assertEqual(result["value"], 0)
            else:
                self.assertIn(b"forced_leaf", completed.stdout+completed.stderr)
        for index, definition in enumerate((
            "u8 invalid(u16 a) __z80_regcall { return a; }",
            "u16 invalid(u8 a) __z80_regcall { return a; }",
            "u16 invalid(u16 a,u16 b,u16 c,u16 d) __z80_regcall { return a; }",
            'u16 invalid(u16 a) __z80_regcall { __asm("NOP"); return a; }',
            "u16 invalid(u16 a) __z80_regcall { return invalid(a); }",
            "u16 other(u16 a) { return a; } u16 invalid(u16 a) __z80_regcall { return other(a); }",
            "u16 invalid(u16 a) __z80_regcall __org 4096 { return a; }",
            "u16 invalid(u16 a) __z80_regcall __orga 4096 { return a; }",
            "void invalid(void) __z80_regcall __pureasm {}",
            "u16 invalid(u16 a) __z80_regcall { return a; } u16 escape(void) { return &invalid; }",
            "u16 invalid(u16 a) __z80_regcall; u16 escape(void) { return &invalid; }",
            "u16 invalid(u8 a) __z80_regcall;",
        )):
            work = self.output / f"invalid-forced-register-{index}"
            work.mkdir()
            (work / "main.ssc").write_text(definition+"\nvoid main(void) {}\n")
            completed = subprocess.run([compiler, "-mz80", "-rs", "3", "-o", "main.asm", "main.ssc"],
                                       cwd=work, capture_output=True, timeout=30)
            self.assertNotEqual(completed.returncode, 0, index)
            self.assertIn(b"__z80_regcall", completed.stdout+completed.stderr, index)

    def test_register_call_assembly_boundaries(self):
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        for mode, flags in (("default", ()), ("no_ra", ("-no-ra",)), ("ra", ("-ra",)),
                            ("all_spill", ("-ra-all-spill",))):
            work = self.output / f"register-call-assembly-{mode}"
            work.mkdir()
            (work / "main.ssc").write_text(
                "u8 ra_buffer[128]; u16 old_tail(u16 a); u16 old_leaf(u16 a);\n"
                "u16 new_leaf(u16 a) { return a^0x1234; }\n"
                "u16 new_peer(u16 a) { return new_leaf(a)+new_leaf(a+1); }\n"
                "u16 boundary(u16 a,u16 b) { return old_tail(a)+new_peer(b)+old_leaf(b); }\n"
                "void main(void) {}\n")
            (work / "old.asm").write_text(
                '  .BANK 0 SLOT 0\n  .ORG 0\n  .SECTION "old_tail" FREE\n'
                "old_tail:\n      JP new_leaf\n  .ENDS\n"
                '  .SECTION "old_leaf" FREE\nold_leaf:\n'
                "      LD HL,-7\n      ADD HL,DE\n      LD C,(HL)\n      INC HL\n      LD B,(HL)\n"
                "      INC BC\n      LD HL,-5\n      ADD HL,DE\n      LD (HL),C\n      INC HL\n      LD (HL),B\n"
                "      LD HL,-1\n      ADD HL,DE\n      LD SP,HL\n      RET\n  .ENDS\n")
            run(compiler, ["-mz80", "-rs", "3", *flags, "-o", "main.asm", "main.ssc"], work, "compiler.log")
            run(linker, ["-lsms", "-v", "-o", "linked.sms", "main.asm", "old.asm"], work, "linker.log")
            image = read_image(work)
            self.assertIn("new_leaf", image.symbols)
            self.assertIn("ssc.autocall.v1.w1.new_leaf", image.symbols)
            for a, b in ((0, 7), (0x8000, 65535), (65535, 0x9234)):
                result = execute(image.rom, image.symbols["boundary"], [a, b], [2, 2], bytes(128),
                                 image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                expected = (a^0x1234)+(b^0x1234)+(((b+1)&65535)^0x1234)+((b+1)&65535)
                self.assertEqual(result["value"], expected&65535, (mode, a, b))
                self.assertEqual((result["buffer"], result["port_writes"], result["unmapped_instructions"]),
                                 (bytes(128), [], 0))

    def test_compatible_internal_register_calls(self):
        cases = {
            "zero": ("return rc_zero()+a+b;", lambda a, b: 0x9234+a+b),
            "zero_twice": ("return rc_zero()+rc_zero();", lambda a, b: 2*0x9234),
            "one": ("return rc_one(a)+b;", lambda a, b: (a^0x1234)+b),
            "one_nested": ("return rc_one(rc_one(a))+b;", lambda a, b: a+b),
            "two": ("return rc_two(a,b);", lambda a, b: a+3*b),
            "two_nested": ("return rc_two(rc_one(a),rc_two(b,7));", lambda a, b: (a^0x1234)+3*(b+21)),
            "three": ("return rc_three(a,b,7);", lambda a, b: a+3*b+35),
            "constant": ("return rc_three(0,65535,32768);", lambda a, b: 3*65535+5*32768),
            "extend": ("s8 left=a; u8 right=b; return rc_three(left,right,7);",
                       lambda a, b: (a&255)-(256 if a&128 else 0)+3*(b&255)+35),
            "alias": ("rc_global=a; return rc_three(rc_global,rc_change(b),rc_global);", lambda a, b: a+8*b),
            "live": ("return rc_one(a)+rc_three(a,b,7);", lambda a, b: (a^0x1234)+a+3*b+35),
            "far": ("u8 work[260]; work[259]=b; u16 local=a; rc_seed(&local,a); "
                    "return rc_three(local,b,7)+work[259];", lambda a, b: a+3*b+35+(b&255)),
            "loop": ("u16 total=a; u16 i; for(i=0;i<3;i++) { total=rc_three(total,b,7); } return total;",
                     lambda a, b: a+3*(3*b+35)),
            "global": ("rc_word=rc_three(a,b,7); return rc_word^a;", lambda a, b: ((a+3*b+35)&65535)^a),
            "pointer": ("u16 data[2]; data[0]=a; data[1]=b; return rc_pointer(data);", lambda a, b: a^b),
            "pointer_twice": ("u16 data[2]; data[0]=a; data[1]=b; return rc_pointer(data)+rc_pointer(data);",
                              lambda a, b: 2*(a^b)),
            "signed": ("return rc_signed(a,b)+rc_signed(b,a);",
                       lambda a, b: 2*min((a^0x8000)-0x8000, (b^0x8000)-0x8000)),
            "arithmetic": ("return rc_math(a,b)^rc_math(b,a);",
                           lambda a, b: ((a*b+a//(b|1))&65535)^((a*b+b//(a|1))&65535)),
            "byte": ("u16 value=rc_byte(a); value=value+rc_byte(b); return value;",
                     lambda a, b: ((a^0x45)&255)+((b^0x45)&255)),
            "mixed": ("return rc_mixed(a,b)+rc_mixed(b,a);", lambda a, b: (a&255)+b+(b&255)+a),
            "recursive": ("return rc_recursive(a&3)+b;", lambda a, b: 7+(a&3)*((a&3)+1)//2+b),
            "asm": ('return rc_asm(a)+rc_asm(b);', lambda a, b: a+b+2),
            "four": ("return rc_four(a,b,7,9)+rc_four(b,a,9,7);", lambda a, b: 2*(a+b+16)),
            "ignored": ("rc_three(a,b,7); return a^b;", lambda a, b: a^b),
        }
        helpers = """u8 ra_buffer[128]; u16 rc_global; u16 rc_word;
u16 rc_zero(void) { return 0x9234; }
u16 rc_one(u16 a) { return a^0x1234; }
u16 rc_two(u16 a,u16 b) { return a+3*b; }
u16 rc_three(u16 a,u16 b,u16 c) { return a+3*b+5*c; }
u16 rc_pointer(u16 *data) { return data[0]^data[1]; }
s16 rc_signed(s16 a,s16 b) { if(a<b) { return a; } return b; }
u16 rc_math(u16 a,u16 b) { return a*b+a/(b|1); }
u16 rc_change(u16 value) { rc_global=value; return value; }
void rc_seed(u16 *target,u16 value) { target[0]=value; }
u8 rc_byte(u16 a) { return a^0x45; }
u16 rc_mixed(u8 a,u16 b) { return a+b; }
u16 rc_recursive(u16 a) { if(a==0) { return 7; } return rc_recursive(a-1)+a; }
u16 rc_asm(u16 a) { __asm("NOP"); return a+1; }
u16 rc_four(u16 a,u16 b,u16 c,u16 d) { return a+b+c+d; }
"""
        programs = [Program(f"register_call_{name}", name, 0, 0, [2, 2], 2, [],
                            f"u16 register_call_{name}(u16 a,u16 b) {{ {body} }}")
                    for name, (body, _) in cases.items()]
        compiler, linker = os.environ.get("SAMESAMECC") or "samesamecc", os.environ.get("SAMESAMECL") or "samesamecl"
        directory = self.output / "compatible-register-calls"
        images = compile_generated(directory, compiler, linker, programs,
                                   helpers + "\n".join(p.source for p in programs) + "\nvoid main(void) {}\n")
        calls = 0
        for program in programs:
            for a in (0, 1, 127, 128, 255, 0x7fff, 0x8000, 65535):
                for b in (0, 7, 255, 0x9234):
                    initial = bytes((index*37+a+b)&255 for index in range(132))
                    expected = bytearray(initial)
                    if program.family == "alias":
                        expected[128:130] = b.to_bytes(2, "little")
                    if program.family == "global":
                        expected[130:132] = ((a+3*b+35)&65535).to_bytes(2, "little")
                    for mode, image in images.items():
                        self.assertEqual((image.symbols["rc_global"], image.symbols["rc_word"]),
                                         (image.symbols["ra_buffer"]+128, image.symbols["ra_buffer"]+130))
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2], initial,
                                         image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (cases[program.family][1](a, b)&65535, expected, []),
                                         (program.name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        for mode, image in images.items():
            assembly = (directory / mode / "main.asm").read_text()
            for name, count in (("rc_zero", 0), ("rc_one", 1), ("rc_two", 2), ("rc_three", 3),
                                ("rc_pointer", 1), ("rc_signed", 2), ("rc_math", 2)):
                self.assertIn(f"ssc.autocall.v1.w{count}.{name}", image.symbols, (name, mode))
            for name in ("rc_change", "rc_byte", "rc_mixed", "rc_recursive", "rc_asm", "rc_four"):
                self.assertNotIn(f"ssc.regbody.{name}", image.symbols, (name, mode))
            for name, args, oracle in (("rc_zero", [], lambda: 0x9234),
                                      ("rc_one", [0x8001], lambda a: a^0x1234),
                                      ("rc_two", [0xffff, 0x9234], lambda a, b: a+3*b),
                                      ("rc_three", [0xffff, 0x9234, 0x8000], lambda a, b, c: a+3*b+5*c)):
                result = execute(image.rom, image.symbols[name], args, [2]*len(args), bytes(132),
                                 image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                self.assertEqual(result["value"], oracle(*args)&65535, (name, mode))
        self.assertEqual(calls, len(programs)*32*len(images))
        for caller_new in (False, True):
            for callee_new in (False, True):
                work = self.output / f"register-call-objects-{int(caller_new)}-{int(callee_new)}"
                work.mkdir()
                (work / "callee.ssc").write_text(
                    "u16 object_leaf(u16 a,u16 b,u16 c) { return a+3*b+5*c; }\n"
                    "u16 object_peer(u16 a,u16 b) { return object_leaf(a,b,7)+object_leaf(b,a,9); }\n")
                (work / "caller.ssc").write_text(
                    "u8 ra_buffer[128]; u16 object_leaf(u16 a,u16 b,u16 c); u16 object_peer(u16 a,u16 b);\n"
                    "u16 object_caller(u16 a,u16 b) { return object_leaf(a,b,7)^object_peer(a,b); }\nvoid main(void) {}\n")
                for unit, enabled in (("caller", caller_new), ("callee", callee_new)):
                    run(compiler, ["-mz80", "-rs", "3", "-ra", *(["-z80-regcalls"] if enabled else []),
                                   "-o", f"{unit}.asm", f"{unit}.ssc"], work, f"{unit}.compiler.log")
                run(linker, ["-lsms", "-v", "-o", "linked.sms", "caller.asm", "callee.asm"], work, "linker.log")
                image = read_image(work)
                for a, b in ((0, 7), (0x8000, 65535), (65535, 0x9234)):
                    result = execute(image.rom, image.symbols["object_caller"], [a, b], [2, 2], bytes(128),
                                     image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=2048)
                    self.assertEqual(result["value"], ((a+3*b+35)&65535)^((4*a+4*b+80)&65535),
                                     (caller_new, callee_new, a, b))
                self.assertIn("ssc.autocall.v1.w3.object_leaf", image.symbols)

    def test_constant_array_address_rematerialization(self):
        cases = {}
        programs = []
        for kind in ("global", "local", "far", "pointer", "variable"):
            for item_type in ("u8", "u16"):
                name = f"address_{kind}_{item_type}"
                array = "ra_buffer" if item_type == "u8" else "address_words"
                prefix = ""
                if kind in ("local", "far", "variable"):
                    prefix = f"{item_type} data[16]; "
                    array = "data"
                if kind == "far":
                    prefix = "u8 workspace[260]; workspace[259]=a; " + prefix
                if kind == "pointer":
                    prefix = f"{item_type} *source={array}; "
                    array = "source"
                offset = "(a&3)" if kind == "variable" else "3"
                body = (f"{prefix} u8 first=a&7; u8 second=b&7; "
                        f"{array}[first+{offset}]=a; {array}[second+{offset}]=b; "
                        f"{item_type} *base=&{array}[{offset}]; "
                        f"{item_type} out=base[first]; out=out+base[second]; "
                        f"{'u16 total=out; return total+workspace[259];' if kind == 'far' else 'return out;'}")
                programs.append(Program(name, kind, 0, 0, [2, 2], 2, [],
                                        f"u16 {name}(u16 a,u16 b) {{ {body} }}"))
                cases[name] = (kind, item_type)
        directory = self.output / "constant-array-address"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs,
                                   "u8 ra_buffer[128]; u16 address_words[16];\n"
                                   + "\n".join(p.source for p in programs) + "\nvoid main(void) {}\n")
        calls = 0
        for program in programs:
            kind, item_type = cases[program.name]
            item_size = 1 if item_type == "u8" else 2
            mask = (1 << (item_size * 8)) - 1
            for a in (0, 1, 7, 128, 255, 0x8000, 0xa55a, 65535):
                for b in (0, 7, 255, 0x9234):
                    first, second = a & 7, b & 7
                    expected_value = (((b if first == second else a) + b) & mask)
                    if kind == "far":
                        expected_value = (expected_value + (a & 255)) & 65535
                    initial = bytes((index*37+a+b) & 255 for index in range(160))
                    for mode, image in images.items():
                        expected_buffer = bytearray(initial)
                        if kind in ("global", "pointer"):
                            offset = 0 if item_size == 1 else image.symbols["address_words"] - image.symbols["ra_buffer"]
                            self.assertGreaterEqual(offset, 0)
                            self.assertLessEqual(offset + 32, len(initial))
                            for index, value in ((first + 3, a), (second + 3, b)):
                                start = offset + index*item_size
                                expected_buffer[start:start+item_size] = (value & mask).to_bytes(item_size, "little")
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=1024)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected_value, expected_buffer, []), (program.name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        for mode in images:
            assembly = (directory / mode / "main.asm").read_text()
            for program in programs:
                kind, item_type = cases[program.name]
                body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                selected = mode in ("ra", "ra_quiet") and kind in ("global", "local", "far")
                self.assertEqual(f"; folded constant array address offset={3 if item_type == 'u8' else 6}" in body,
                                 selected, (program.name, mode))
                if selected:
                    self.assertIn(f"from stable {'global' if kind == 'global' else 'frame'} address into HL", body)
                elif kind in ("pointer", "variable"):
                    self.assertNotIn("; rematerialize", body)
        self.assertEqual(calls, len(programs) * 32 * len(images))

    def test_bounded_word_crc_loops(self):
        cases = {
            "word": (8, 0x1021, True), "one": (1, 0x8005, True),
            "sixteen": (16, 0x1021, True), "byte_max": (255, 0x1021, True),
            "word_bound": (256, 0x8005, True), "high_poly": (7, 0xa500, True),
            "low_poly": (8, 0x00a5, True), "live_counter": (256, 0x1021, True),
            "argument": (8, 0x1021, True), "zero": (0, 0x1021, False),
            "wide": (257, 0x1021, False), "signed": (8, 0x1021, False),
            "signed_counter": (8, 0x1021, False), "escaped": (8, 0x1021, False),
            "call": (8, 0x1021, False), "assembly": (8, 0x1021, False),
            "mask": (8, 0x1021, False), "shift": (8, 0x1021, False),
            "reverse": (8, 0xa001, False), "global": (8, 0x1021, False),
            "far": (8, 0x1021, False), "nested": (8, 0x1021, True),
            "break": (8, 0x1021, False), "continue": (8, 0x1021, False),
            "counter_read": (8, 0x1021, False), "variable": (8, 0x1021, False),
            "outer_word": (8, 0x1021, True), "outer_word_bound": (256, 0x8005, True),
            "outer_live_counter": (8, 0x1021, True), "outer_call": (8, 0x1021, True),
            "outer_escaped": (8, 0x1021, False), "outer_assembly": (8, 0x1021, False),
            "outer_break": (8, 0x1021, False), "outer_variable": (8, 0x1021, False),
            "partial_owner": (8, 0x1021, False),
        }
        programs = []
        for name, (count, polynomial, _) in cases.items():
            outer = name.startswith("outer_")
            kind = name[6:] if outer else name
            loop = (f"for(i=0;i<{count};i++) {{ "
                    f"if((crc&0x8000)!=0) {{ crc=(crc<<1)^{polynomial}; }} else {{ crc<<=1; }} }}")
            prefix, suffix = "", "return crc;"
            if kind == "live_counter":
                suffix = "return crc+i;"
            elif kind == "escaped":
                prefix = "crc_escape(&crc);"
            elif kind == "call" and not outer:
                loop = loop.replace("if(", "crc=crc_call(crc); if(")
            elif kind == "assembly":
                prefix = '__asm("NOP");'
            elif name == "mask":
                loop = loop.replace("0x8000", "0x4000")
            elif name == "shift":
                loop = loop.replace("<<1", "<<2").replace("<<=1", "<<=2")
            elif name == "reverse":
                loop = loop.replace("0x8000", "1").replace("<<", ">>")
            elif name == "far":
                prefix, suffix = "workspace[259]=b;", "return crc+workspace[259];"
            elif name == "nested":
                loop = "for(k=0;k<2;k++) { " + loop + " }"
            elif kind in ("break", "continue"):
                loop = loop.replace("if(", f"if(i==3) {{ {kind}; }} if(", 1)
            elif name == "counter_read":
                loop = loop[:-1] + " crc^=i; }"
            elif kind == "variable":
                loop = loop.replace(f"i<{count}", "i<(b&15)")
            elif name == "partial_owner":
                prefix, suffix = "u16 held=a+b; crc^=held;", "return crc+held;"
            if outer:
                call = "crc=crc_call(crc);" if kind == "call" else ""
                loop = f"for(k=0;k<((b&3)+1);k++) {{ crc^=k; {call} {loop} }}"
                suffix = "return crc+k+i;" if kind == "live_counter" else "return crc+k;"
            body = f"u16 crc=a; u16 i; {prefix} {loop} {suffix}"
            if name == "signed":
                body = body.replace("u16 crc", "s16 crc")
            elif name == "signed_counter":
                body = body.replace("u16 i", "s16 i")
            elif name == "argument":
                body = re.sub(r"\bcrc\b", "a", body.replace("u16 crc=a;", ""))
            elif name == "global":
                body = re.sub(r"\bcrc\b", "crc_global", body.replace("u16 crc=a;", "crc=a;"))
            elif name == "far":
                body = "u8 workspace[260]; " + body
            elif name == "nested" or outer:
                body = "u16 k; " + body
            programs.append(Program(f"crc_loop_{name}", name, 0, 0, [2, 2], 2, [],
                                    f"u16 crc_loop_{name}(u16 a,u16 b) {{ {body} }}"))
        directory = self.output / "bounded-word-crc"
        helpers = ("u8 ra_buffer[128]; u16 crc_global;\n"
                   "void crc_escape(u16 *p) { p[0]^=0x1234; }\n"
                   "u16 crc_call(u16 value) { return value^7; }\n")
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs,
                                   helpers + "\n".join(p.source for p in programs) + "\nvoid main(void) {}\n")
        calls = 0
        for program in programs:
            name = program.family
            outer = name.startswith("outer_")
            kind = name[6:] if outer else name
            count, polynomial, _ = cases[name]
            for a in (0, 1, 0x7fff, 0x8000, 0x8001, 0xffff, 0x1234, 0xa5a5):
                for b in (0, 1, 15, 255):
                    crc = a ^ (0x1234 if kind == "escaped" else
                               (a+b) & 65535 if name == "partial_owner" else 0)
                    iterations = b & 15 if kind == "variable" else count
                    if kind in ("break", "continue"):
                        iterations = 3 if kind == "break" else count - 1
                    rounds = (b & 3)+1 if outer else 2 if name == "nested" else 1
                    for outer_index in range(rounds):
                        if outer:
                            crc ^= outer_index
                            if kind == "call":
                                crc ^= 7
                        for index in range(iterations):
                            if kind == "call" and not outer:
                                crc ^= 7
                            high = crc & (1 if name == "reverse" else 0x4000 if name == "mask" else 0x8000)
                            crc = ((crc >> 1 if name == "reverse" else crc << (2 if name == "shift" else 1))
                                   ^ (polynomial if high else 0)) & 65535
                            if name == "counter_read":
                                crc ^= index
                    expected_value = (crc + (count if kind == "live_counter" else
                                             b if name == "far" else
                                             (a+b) & 65535 if name == "partial_owner" else 0)
                                      + (rounds if outer else 0)) & 65535
                    initial = bytes((index*37+a+b) & 255 for index in range(130))
                    expected_buffer = bytearray(initial)
                    if name == "global":
                        expected_buffer[128:130] = crc.to_bytes(2, "little")
                    for mode, image in images.items():
                        self.assertEqual(image.symbols["crc_global"], image.symbols["ra_buffer"] + 128)
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers,
                                         instruction_limit=100000, frame_bytes=1024)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected_value, expected_buffer, []), (name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        for mode in images:
            assembly = (directory / mode / "main.asm").read_text()
            for program in programs:
                body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                selected = mode in ("ra", "ra_quiet") and cases[program.family][2]
                self.assertEqual("; bounded word CRC loop" in body, selected, (program.family, mode))
                if selected:
                    self.assertRegex(body, r"ADD HL,HL\s+JR\s+NC,\+")
                    self.assertRegex(body, r"DJNZ _byte_loop_\d+")
        self.assertEqual(calls, len(programs) * 32 * len(images))

    def test_word_argument_push_transport(self):
        cases = {
            "word": ("return transport_word(a,b,7);", lambda a, b: a + 3*b + 35),
            "constant": ("return transport_word(65535,128,32768);", lambda a, b: 65535 + 384 + 5*32768),
            "extend": ("s8 left=a; u8 right=b; return transport_word(left,right,7);",
                       lambda a, b: ((a & 255) - (256 if a & 128 else 0)) + 3*(b & 255) + 35),
            "global": ("transport_global=a; return transport_word(b,transport_global,7);",
                       lambda a, b: b + 3*a + 35),
            "alias": ("transport_global=a; return transport_word(transport_global,transport_change(b),transport_global);",
                      lambda a, b: a + 8*b),
            "far": ("u8 workspace[260]; u16 local=a; transport_seed(&local,a); "
                    "workspace[259]=b; return transport_word(local,b,workspace[259]);",
                    lambda a, b: a + 3*b + 5*(b & 255)),
            "nested": ("return transport_word(transport_id(a),transport_id(b),transport_id(7));",
                       lambda a, b: a + 3*b + 35),
            "recursive": ("return transport_recursive(3,a);", lambda a, b: a + 6),
            "byte_return": ("return transport_byte(a);", lambda a, b: a & 255),
            "signed_return": ("return transport_signed(a);",
                              lambda a, b: (a & 255) | (0xff00 if a & 128 else 0)),
            "void": ("transport_void(a); return b;", lambda a, b: b),
            "mixed": ("return transport_mixed(a,b);", lambda a, b: (a & 255) + b),
            "narrow": ("return transport_narrow(a);", lambda a, b: a & 255),
            "zero": ("return transport_zero();", lambda a, b: 321),
            "pure": ("transport_pure(); return a;", lambda a, b: a),
        }
        programs = [Program(f"transport_case_{name}", name, 0, 0, [2, 2], 2, [],
                            f"u16 transport_case_{name}(u16 a,u16 b) {{ {body} }}")
                    for name, (body, _) in cases.items()]
        helpers = (
            "u8 ra_buffer[128]; u16 transport_global;\n"
            "u16 transport_word(u16 a,u16 b,u16 c) { return a+3*b+5*c; }\n"
            "u16 transport_id(u16 a) { return a; }\n"
            "u16 transport_change(u16 a) { transport_global=a; return a; }\n"
            "void transport_seed(u16 *p,u16 a) { p[0]=a; }\n"
            "u16 transport_recursive(u16 depth,u16 a) { "
            "if(depth) { return transport_recursive(depth-1,a)+depth; } return a; }\n"
            "u8 transport_byte(u16 a) { return a; }\n"
            "s8 transport_signed(u16 a) { return a; }\n"
            "void transport_void(u16 a) { ra_buffer[2]=a; }\n"
            "u16 transport_mixed(u8 a,u16 b) { return a+b; }\n"
            "u16 transport_narrow(u8 a) { return a; }\n"
            "u16 transport_zero(void) { return 321; }\n"
            'void transport_pure(void) __pureasm { __asm("RET"); }\n'
        )
        directory = self.output / "word-argument-transport"
        images = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                   os.environ.get("SAMESAMECL") or "samesamecl", programs,
                                   helpers + "\n".join(p.source for p in programs) + "\nvoid main(void) {}\n")
        calls = 0
        for program in programs:
            for a in (0, 1, 127, 128, 255, 256, 32767, 32768, 65535):
                for b in (0, 1, 128, 255, 32768, 65535):
                    initial = bytes((i*37+a+b) & 255 for i in range(130))
                    expected = bytearray(initial)
                    if program.family in ("global", "alias"):
                        expected[128:130] = (a if program.family == "global" else b).to_bytes(2, "little")
                    elif program.family == "void":
                        expected[2] = a & 255
                    for mode, image in images.items():
                        self.assertEqual(image.symbols["transport_global"], image.symbols["ra_buffer"] + 128)
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=1024)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (cases[program.family][1](a, b) & 65535, expected, []),
                                         (program.name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        for mode in images:
            assembly = (directory / mode / "main.asm").read_text()
            for program in programs:
                body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                selected = mode in ("ra", "ra_quiet") and program.family not in ("mixed", "narrow", "zero", "pure")
                self.assertEqual(bool(re.search(r"LD\s+SP,HL\s+DEC SP\s+DEC SP\s+DEC SP\s+; new stack frame -> DE",
                                                body)), selected, (program.name, mode))
                if program.family in ("mixed", "narrow") or not selected and program.family not in ("zero", "pure"):
                    self.assertRegex(body, r"LD\s+IX,3\s+ADD IX,SP")
                if selected:
                    self.assertNotRegex(body, r"LD\s+IX,3\s+ADD IX,SP")
                if program.family == "pure":
                    self.assertRegex(body, r"CALL transport_pure")
        print(f"Word-argument transport oracle executions: {calls}", flush=True)

    def test_shared_window_memory_copies(self):
        kinds = ("u8", "s8", "u16", "s16")
        shapes = [(left, right, size, "copy") for left in kinds for right in kinds
                  for size in (8, 136, 260) if size == 8 or left.endswith("8") == right.endswith("8")]
        shapes += [("u16", "u16", size, family) for size in (114, 115, 116, 117, 123, 124, 125, 136, 260)
                   for family in ("source_span", "target_span")]
        shapes += [(kind, kind, 8, family) for kind in ("u8", "u16") for family in ("alias", "loop")]
        shapes += [("u16", "u16", 8, family) for family in ("global_source", "global_target")]
        programs = []
        for left, right, size, family in shapes:
            name = f"memory_{left}_{right}_{size}_{family}"
            source = "copy_global_source" if family == "global_source" else "source"
            target = "copy_global_target" if family == "global_target" else "target"
            declarations = [f"u8 workspace[{size}];",
                            f"{'' if family == 'global_source' else left + ' '}{source}=a;",
                            f"{'' if family == 'global_target' else right + ' '}{target}=b;"]
            if family == "source_span":
                declarations = [declarations[2], declarations[0], declarations[1]]
            elif family == "target_span":
                declarations = [declarations[1], declarations[0], declarations[2]]
            prefix = (f"copy_seed_{left}(&{source},a); copy_seed_{right}(&{target},b); workspace[{size-1}]=b; ")
            if family == "alias":
                prefix += (f"if(b&1) {{ copy_alias_{left}(&source,&source,b); }} "
                           f"else {{ copy_alias_{left}(&source,&target,b); }} ")
            copy = f'__asm("LD IX,4660"); {target}={source}; '
            if family == "loop":
                copy = f"u16 i; for(i=0;i<(b&3);i++) {{ " + copy + f"copy_seed_{left}(&source,b+i); }} "
            programs.append(Program(name, family, size, 0, [2, 2], 2, [],
                                    f"u16 {name}(u16 a,u16 b) {{ " + " ".join(declarations) + " " + prefix + copy
                                    + f"ra_buffer[1]=workspace[{size-1}]; return copy_read_{right}(&{target}); }}"))
        helpers = "u8 ra_buffer[128]; u16 copy_global_source; u16 copy_global_target;\n"
        for kind in kinds:
            helpers += (f"void copy_seed_{kind}({kind} *p,{kind} value) {{ p[0]=value; ra_buffer[127]+=1; }}\n"
                        f"u16 copy_read_{kind}({kind} *p) {{ ra_buffer[127]+=1; return p[0]; }}\n")
        for kind in ("u8", "u16"):
            helpers += (f"void copy_alias_{kind}({kind} *p,{kind} *q,u16 b) {{ "
                        "p[0]=q[0]+b; q[0]=0; ra_buffer[127]+=1; }\n")
        images, directories = {}, {}
        for group in (8, 136, 260, "boundaries"):
            selected = [program for program in programs if (
                "boundaries" if program.family.endswith("_span") else program.variant) == group]
            directory = self.output / "memory-copies" / str(group)
            directories[group] = directory
            source = helpers + "\n".join(program.source for program in selected) + "\nvoid main(void) {}\n"
            images[group] = compile_generated(directory, os.environ.get("SAMESAMECC") or "samesamecc",
                                              os.environ.get("SAMESAMECL") or "samesamecl", selected, source)
        calls = 0
        for program in programs:
            left, right = program.name.split("_")[1:3]
            source_mask = 255 if left.endswith("8") else 65535
            target_mask = 255 if right.endswith("8") else 65535
            group = "boundaries" if program.family.endswith("_span") else program.variant
            for a in (0, 1, 127, 128, 255, 256, 32767, 32768, 65535):
                for b in (0, 1, 127, 128, 65535):
                    copied, seeds = a & source_mask, 2
                    if program.family == "alias":
                        copied, seeds = (0 if b & 1 else 2 * b) & source_mask, 3
                    elif program.family == "loop":
                        count = b & 3
                        copied = b if count == 0 else a if count == 1 else b + count - 2
                        copied &= source_mask
                        seeds += count
                    # The existing assignment path sign-extends s8 only for an s16 destination.
                    if left == "s8" and right == "s16" and copied & 128:
                        copied |= 0xff00
                    copied &= target_mask
                    expected = copied | (0xff00 if right == "s8" and copied & 128 else 0)
                    initial = bytes((offset * 37 + a + b) & 255 for offset in range(132))
                    expected_buffer = bytearray(initial)
                    expected_buffer[1] = b & 255
                    expected_buffer[127] = (initial[127] + seeds + 1) & 255
                    if program.family == "global_source":
                        expected_buffer[128:130] = (a & 65535).to_bytes(2, "little")
                    elif program.family == "global_target":
                        expected_buffer[130:132] = copied.to_bytes(2, "little")
                    for mode, image in images[group].items():
                        self.assertEqual(image.symbols["copy_global_source"], image.symbols["ra_buffer"] + 128)
                        self.assertEqual(image.symbols["copy_global_target"], image.symbols["ra_buffer"] + 130)
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                         initial, image.symbols["ra_buffer"], 2, image.transfers, frame_bytes=512)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected, expected_buffer, []), (program.name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        print(f"Shared-window memory-copy oracle executions: {calls}", flush=True)
        for mode in images[8]:
            assemblies = {group: (directory / mode / "main.asm").read_text()
                          for group, directory in directories.items()}
            boundaries = set()
            for program in programs:
                left, right = program.name.split("_")[1:3]
                group = "boundaries" if program.family.endswith("_span") else program.variant
                body = assemblies[group].split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                source = "copy_global_source" if program.family == "global_source" else r"_source_\d+"
                target = "copy_global_target" if program.family == "global_target" else r"_target_\d+"
                copy = re.search(r"; TAC: " + target + r"\.\w+ \(\w+\) := " + source
                                 + r"\.\w+ \(\w+\)\n\s*; -+\n([\s\S]*?)(?=\s*; -+)", body)
                self.assertIsNotNone(copy, (program.name, mode))
                selected = False
                if not program.family.startswith("global_") and left.endswith("8") == right.endswith("8"):
                    source_offset = int(re.search(r'; TAC: variable "' + source + r'" size \d+ offset (-?\d+)', body)[1])
                    target_offset = int(re.search(r'; TAC: variable "' + target + r'" size \d+ offset (-?\d+)', body)[1])
                    base = 0 if -128 <= target_offset <= 126 else target_offset
                    displacement = source_offset - base
                    selected = mode in ("ra", "ra_quiet") and -128 <= displacement <= 126
                    if program.family.endswith("_span"):
                        boundaries.add(displacement)
                if selected:
                    self.assertNotIn("IY", copy[1], (program.name, mode))
                    width = 1 if right.endswith("8") else 2
                    transfers = [" ".join(line.split()) for line in copy[1].splitlines() if line.strip().startswith("LD ")]
                    expected = [instruction for byte in range(width) for instruction in (
                        f"LD A,(IX{displacement+byte:+d})", f"LD (IX{target_offset-base+byte:+d}),A")]
                    self.assertEqual(transfers[-2*width:], expected, (program.name, mode))
                else:
                    self.assertIn("IY", copy[1], (program.name, mode))
            self.assertTrue({-129, -128, 126, 127}.issubset(boundaries), (mode, boundaries))

    def test_retained_word_frame_copies(self):
        programs = []
        for kind in ("u16", "s16"):
            for size in (0, 8, 112, 113, 114, 115, 116, 117, 136, 260):
                for primed in (False, True):
                    name = f"word_copy_{kind}_{size}" + ("_primed" if primed else "")
                    last = max(size, 8) - 1
                    local = f"{kind} index=a; word_copy_step_{kind}(&index); " if size else ""
                    programs.append(Program(name, "primed" if primed else "plain", size, 0, [2, 2, 2], 2, [],
                                            f"u16 {name}({kind} {'a' if size else 'index'}, u16 b, u8 *p) {{ "
                                            f"u8 workspace[{last+1}]; {local}workspace[{last}]=b; "
                                            + ("if(index&1) { index^=2; } " if primed else "")
                                            + f"u16 value=p[index++]; return value+index+workspace[{last}]; }}"))
        programs.extend(
            Program(f"word_copy_arguments_{count}", "arguments", count, 0, [2] * (count + 3), 2, [],
                    f"u16 word_copy_arguments_{count}(u16 a,u16 b,"
                    + ",".join(f"u16 padding{i}" for i in range(count))
                    + ",u8 *p) { u16 value=p[a&31]; return value+b; }")
            for count in (64, 128)
        )
        directories, images = {}, {}
        for family in ("plain", "primed", "arguments"):
            group = [program for program in programs if program.family == family]
            source = ("u8 ra_buffer[128];\n"
                      "void word_copy_step_u16(u16 *p) { p[0]+=1; }\n"
                      "void word_copy_step_s16(s16 *p) { p[0]+=1; }\n"
                      + "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
            directories[family] = self.output / ("word-frame-copies" + (f"-{family}" if family != "plain" else ""))
            images[family] = compile_generated(directories[family], os.environ.get("SAMESAMECC") or "samesamecc",
                                               os.environ.get("SAMESAMECL") or "samesamecl", group, source)
        calls = 0
        for program in programs:
            indices = ((0, 1, 127, 128, 255, 256, 32767, 32768, 65535) if program.family == "arguments"
                       else (-3, -1, 0, 1, 7, 23, 63))
            for index in indices:
                a = (index - bool(program.variant)) & 65535
                access_index = index ^ 2 if program.family == "primed" and index & 1 else index
                for b in (0, 1, 127, 128, 255, 256, 32767, 32768, 65535):
                    initial = bytes((offset * 37 + b) & 255 for offset in range(128))
                    if program.family == "arguments":
                        expected = (initial[64 + (index & 31)] + b) & 65535
                    else:
                        expected = (initial[64 + access_index] + access_index + 1 + (b & 255)) & 65535
                    for mode, image in images[program.family].items():
                        parameters = [a, b, image.symbols["ra_buffer"] + 64]
                        if program.family == "arguments":
                            parameters = [index, b, *([0] * program.variant), image.symbols["ra_buffer"] + 64]
                        result = execute(image.rom, image.symbols[program.name],
                                         parameters, program.widths,
                                         initial, image.symbols["ra_buffer"], 2, image.transfers,
                                         frame_bytes=512)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected, initial, []), (program.name, mode, index, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        if program.family == "arguments" and mode in ("ra", "ra_quiet"):
                            self.assertEqual(result["cycles"], 325, (program.name, mode, index, b))
                        calls += 1
        print(f"Retained word frame-copy oracle executions: {calls}", flush=True)
        for mode in images["plain"]:
            assemblies = {family: (directory / mode / "main.asm").read_text()
                          for family, directory in directories.items()}
            offsets = set()
            for program in programs:
                body = assemblies[program.family].split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                variable_name = "p" if program.family == "arguments" else r"index|_index_\d+"
                variable = re.search(r'; TAC: variable "(' + variable_name + r')" size 2 offset (-?\d+)', body)
                self.assertIsNotNone(variable, (program.name, mode))
                offset = int(variable[2])
                offsets.add(offset)
                if program.family == "arguments" and mode not in ("ra", "ra_quiet"):
                    self.assertIn(f"LD  IY,{offset}", body, (program.name, mode))
                    continue
                copy = re.search(r"; TAC: r(\d+)\.(?:u16|s16) \((?:u16|s16)\) := "
                                 + re.escape(variable[1]) + r"\.(?:u16|s16) \((?:u16|s16)\)\n"
                                 r"\s*; -+\n([\s\S]*?)(?=\s*; -+)", body)
                self.assertIsNotNone(copy, (program.name, mode))
                if mode in ("ra", "ra_quiet"):
                    physical = "HL" if program.family == "arguments" else "BC"
                    self.assertIn(f"; register {copy[1]} size 2 spill no phy {physical}", body, (program.name, mode))
                    if -128 <= offset <= 126 or program.family == "primed":
                        self.assertNotIn("IY", copy[2], (program.name, mode))
                        self.assertRegex(copy[2], r"LD  C,\(IX[+-]\d+\)\n\s*LD  B,\(IX[+-]\d+\)")
                        if program.family == "primed" and offset < -128:
                            self.assertNotRegex(copy[2], r"\b(?:LD|INC|DEC)\s+IX\b", (program.name, mode))
                    else:
                        self.assertIn(f"LD  IY,{offset}", copy[2], (program.name, mode))
                    if program.family == "arguments":
                        self.assertEqual(images[program.family][mode].staticMetrics["functions"][program.name]["rom_bytes"], 57)
                else:
                    self.assertIn(f"LD  IY,{offset}", copy[2], (program.name, mode))
            self.assertTrue(set(range(-130, -124)).issubset(offsets), (mode, offsets))

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
            "or": "if((b&1)||(b&2)||(b&4)) {{ workspace[{last}]=a+3; }} "
                  "else {{ workspace[{last}]=a+7; }}",
            "and": "if((b&1)&&(b&2)&&(b&4)) {{ workspace[{last}]=a+3; }} "
                   "else {{ workspace[{last}]=a+7; }}",
            "call_or": "if((b&1)||(frame_sink(b&2)>3)||(b&4)) {{ workspace[{last}]=a+3; }} "
                       "else {{ workspace[{last}]=a+7; }}",
            "different": "workspace[0]=a; if(b&1) {{ workspace[0]=a+3; }} "
                         "else {{ workspace[{last}]=a+7; }} return workspace[0]+workspace[{last}]+b;",
            "pressure": "if(b&1) {{ goto done; }} "
                        + "if(b&2) {{ workspace[{last}]+=1; }} " * 17 + "done:",
        }
        for limit in (8, 9):
            body = " ".join(
                "if(b&" + str(1 << index) + ") {{ goto done; }} workspace[{last}]+=1;"
                for index in range(limit)
            )
            bodies[f"evict{limit}"] = body + " done:"
            bodies[f"terminal{limit}"] = body + " return workspace[{last}]+b; done: workspace[{last}]+=3;"
        programs = [
            Program(f"frame_{size}_{family}", family, size, 0, [2, 2], 2, [],
                    f"u16 frame_{size}_{family}(u16 a, u16 b) {{ u8 workspace[{size}]; "
                    f"workspace[{size-1}]=a; " + body.format(last=size-1).replace("done", f"done_{size}_{family}")
                    + f" return workspace[{size-1}]+b; }}")
            for size in (8, 136, 260) for family, body in bodies.items()
        ]
        directory = self.output / "forward-frames"
        images = {}
        for size in (8, 136, 260):
            group = [program for program in programs if program.variant == size]
            source = ("u8 ra_buffer[128];\nu16 frame_sink(u16 a) { ra_buffer[127]+=1; return a+3; }\n"
                      "void frame_alias(u8 *p, u16 index, u16 a, u16 b) { "
                      "if(b&1) { p[index]=a+3; return; } p[index]=a+7; }\n"
                      + "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
            images[size] = compile_generated(directory / str(size), os.environ.get("SAMESAMECC") or "samesamecc",
                                            os.environ.get("SAMESAMECL") or "samesamecl", group, source)
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
                    elif program.family in ("or", "call_or", "and"):
                        condition = (b & 7) == 7 if program.family == "and" else bool(b & 7)
                        value = a + (3 if condition else 7)
                    elif program.family == "pressure":
                        value = a + (0 if b & 1 else 17 * bool(b & 2))
                    elif program.family.startswith(("evict", "terminal")):
                        limit = int(program.family[-1])
                        first = next((i for i in range(limit) if b & (1 << i)), limit)
                        value = a + first + (3 if program.family.startswith("terminal") and first < limit else 0)
                    if program.family == "call" and b & 1:
                        expected_buffer[127] = 128
                    elif program.family == "call_or" and not b & 1:
                        expected_buffer[127] = 128
                    elif program.family == "global" and b & 1:
                        expected_buffer[1] = a & 255
                    expected = (value & 255) + b
                    if program.family == "different":
                        expected = (a & 255) + ((a + (3 if b & 1 else 7)) & 255) + b
                    for mode, image in images[program.variant].items():
                        result = execute(image.rom, image.symbols[program.name], [a, b], [2, 2],
                                         bytes(range(128)), image.symbols["ra_buffer"], 2, image.transfers,
                                         frame_bytes=512)
                        self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                         (expected & 65535, expected_buffer, []),
                                         (program.name, mode, a, b))
                        self.assertEqual(result["unmapped_instructions"], 0)
                        calls += 1
        print(f"Forward-join frame-address oracle executions: {calls}", flush=True)
        for mode in images[8]:
            # Count assembled instructions, not unselected ABI alternatives.
            assemblies = {
                size: "\n".join(line[60:] for line in
                               (directory / str(size) / mode / "linked.sms.combined.lst").read_text().splitlines()
                               if re.match(r"\s*\d+\s+[0-9a-f]{4}\s", line, re.I) or
                               re.match(r"^\s*\.(?:SECTION|ENDS)\b", line[60:]))
                for size in images
            }
            for program in programs:
                body = assemblies[program.variant].split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                if program.family == "diamond":
                    if mode in ("ra", "ra_quiet"):
                        self.assertEqual(body.count("LD  IX,"), 1, (program.name, mode))
                    else:
                        self.assertGreaterEqual(body.count("LD  IX,"), 3, (program.name, mode))
                elif mode in ("ra", "ra_quiet") and program.family in ("call", "asm"):
                    self.assertEqual(body.count("LD  IX,"), 2 if program.family == "call" else 4,
                                     (program.name, mode))
                elif mode in ("ra", "ra_quiet") and program.family in (
                    "or", "and", "pressure", "evict8", "evict9", "terminal8", "terminal9", "return"
                ):
                    self.assertEqual(body.count("LD  IX,"), 2 if program.family in ("evict9", "terminal9") else 1,
                                     (program.name, mode))

    def test_cfg_mutable_scalar_webs(self):
        forms = {
            "up": "for(k=0;k<n;k++) { state++; total+=state; }",
            "down": "for(k=0;k<n;k++) { state--; total+=state; }",
            "latches": "k=0; while(k<n) { k++; if(k&1) { state++; total+=state; continue; } state--; total+=state; }",
            "reset": "for(k=0;k<n;k++) { state++; total+=state; } state=b; for(k=0;k<n;k++) { state--; total+=state; }",
            "join": "if(n&1) { state=b; } for(k=0;k<n;k++) { state++; total+=state; }",
            "conditional": "if(n&1) { state=a; } else { state=b; } for(k=0;k<n;k++) { state--; total+=state; }",
            "nested": "for(k=0;k<n;k++) { for(j=0;j<(n&3);j++) { state++; total+=state; } }",
            "word_stores": "u16 data[8]; for(k=0;k<8;k++) { data[k]=0; } for(k=0;k<n;k++) { data[k]=b; state++; total+=state; } for(j=0;j<n;j++) { total+=data[j]; }",
            "escape": "scalar_web_escape_KIND(&state,b); for(k=0;k<n;k++) { state++; total+=state; }",
            "call": "for(k=0;k<n;k++) { state++; total+=scalar_web_touch(state); }",
            "asm": 'for(k=0;k<n;k++) { __asm("NOP"); state++; total+=state; }',
            "narrow": "for(k=0;k<n;k++) { state=small; total+=state; }",
            "arithmetic": "for(k=0;k<n;k++) { state+=b; total+=state; }",
            "missing": "if(n) { state=a; } else if(a) { state=b; } if(n||a) { return state^257; } return 0;",
        }
        selected = set(forms) - {"escape", "asm", "narrow", "arithmetic", "missing"}
        programs = []
        for kind in ("u8", "s8", "u16", "s16"):
            width = 1 if kind.endswith("8") else 2
            for family, body in forms.items():
                if width == 1 and family == "narrow":
                    continue
                name = f"scalar_web_{kind}_{family}"
                initializer = "" if family in ("conditional", "missing") else "=a"
                tail = ("" if family == "missing" else
                        "u16 final=state; ra_buffer[0]=final; ra_buffer[1]=final>>8; "
                        "ra_buffer[2]=total; ra_buffer[3]=total>>8; return final^total;")
                source = (f"u16 {name}({kind} a,{kind} b,u16 n) {{ {kind} state{initializer}; "
                          f"u16 k; u16 j; {'s16' if kind.startswith('s') else 'u16'} total=0; u8 small=b; n&=7; "
                          + body.replace("KIND", kind) + tail + " }")
                programs.append(Program(name, family, width, int(kind.startswith("s")),
                                        [width, width, 2], 2, [], source))
        helpers = "\n".join(f"void scalar_web_escape_{kind}({kind} *p,{kind} value) {{ p[0]=value; }}"
                            for kind in ("u8", "s8", "u16", "s16"))
        pairs = [(value, (value*137+0x8001)&65535)
                 for value in (0, 1, 127, 128, 255, 256, 32767, 32768, 65534, 65535)]
        pairs += [(0, 0), (65535, 0), (32768, 65535)]
        calls = selections = 0
        for policy in ("inline", "shared"):
            for offset in range(0, len(programs), len(forms)):
                group = programs[offset:offset+len(forms)]
                directory = self.output / "scalar-webs" / policy / f"batch-{offset}"
                source = ("u8 ra_buffer[128];\n" + helpers +
                          "\nu16 scalar_web_touch(u16 value) { ra_buffer[127]+=1; return value+7; }\n" +
                          "\n".join(program.source for program in group) + "\nvoid main(void) {}\n")
                images = compile_generated(
                    directory, os.environ.get("SAMESAMECC") or "samesamecc",
                    os.environ.get("SAMESAMECL") or "samesamecl", group, source,
                    linker_flags=("-share-arithmetic",) if policy == "shared" else ())
                for mode, image in images.items():
                    assembly = (directory / mode / "main.asm").read_text()
                    for program in group:
                        body = assembly.split(f'.SECTION "{program.name}" FREE')[1].split(".ENDS")[0]
                        if os.environ.get("RA_BASELINE") != "1":
                            promoted = (mode in ("ra", "ra_quiet") and program.variant == 2
                                        and program.family in selected)
                            self.assertEqual('; TAC: variable "_state_' not in body, promoted,
                                             (program.name, policy, mode))
                            selections += int(promoted)
                        mask = (1 << (8*program.variant))-1

                        def scalar(value):
                            value &= mask
                            return value-(mask+1) if program.constant and value&(mask+1)//2 else value

                        for a, b in pairs:
                            for count in (0, 1, 2, 3, 7, 65535):
                                n, total, state = count&7, 0, scalar(a)
                                family = program.family
                                initial = bytes((index*37+a+b+count)&255 for index in range(128))
                                expected_buffer = bytearray(initial)
                                if family == "missing":
                                    expected = ((scalar(a) if n else scalar(b))^257) if n or scalar(a) else 0
                                else:
                                    if family == "escape" or family == "join" and n&1:
                                        state = scalar(b)
                                    elif family == "conditional":
                                        state = scalar(a if n&1 else b)
                                    for phase in range(2 if family == "reset" else 1):
                                        if phase:
                                            state = scalar(b)
                                        iterations = n*(n&3) if family == "nested" else n
                                        for index in range(iterations):
                                            step = (-1 if family in ("down", "conditional") or phase else
                                                    1 if family != "latches" or (index+1)&1 else -1)
                                            state = scalar(b&255 if family == "narrow" else
                                                           state+scalar(b) if family == "arithmetic" else state+step)
                                            total = (total+state+(7 if family == "call" else 0))&65535
                                            if family == "call":
                                                expected_buffer[127] = (expected_buffer[127]+1)&255
                                    final = state&65535
                                    if family == "word_stores":
                                        total = (total+n*scalar(b))&65535
                                    expected_buffer[:4] = final.to_bytes(2, "little")+total.to_bytes(2, "little")
                                    expected = final^total
                                with self.subTest(program=program.name, policy=policy, mode=mode, a=a, b=b, count=count):
                                    result = execute(image.rom, image.symbols[program.name], [a&mask, b&mask, count],
                                                     program.widths, initial, image.symbols["ra_buffer"], 2,
                                                     image.transfers, frame_bytes=512)
                                    self.assertEqual((result["value"], result["buffer"], result["port_writes"]),
                                                     (expected&65535, expected_buffer, []))
                                    self.assertEqual(result["unmapped_instructions"], 0)
                                calls += 1
        self.assertEqual(calls, len(programs)*len(pairs)*6*5*2)
        if os.environ.get("RA_BASELINE") != "1":
            self.assertEqual(selections, len(selected)*2*2*2)
        print(f"CFG scalar web oracle: {len(programs)} shapes, {calls} executions", flush=True)

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
        profiles = (
            ("no-ra", ("-no-ra",) * 4), ("ra-all-spill", ("-ra-all-spill",) * 4), ("ra", ("-ra",) * 4),
            ("ra-callers", ("-ra", "-no-ra", "-no-ra", "-ra")),
            ("ra-callees", ("-no-ra", "-ra", "-ra", "-no-ra")),
        )
        for profile, flags in profiles:
            directory = self.output / f"arithmetic-banks-{profile}"
            directory.mkdir()
            for index, (bank, slot, source) in enumerate(sources):
                (directory / f"unit{index}.ssc").write_text(source)
                command = [compiler, "-mz80", "-rs", "3", "-b", str(bank), "-s", str(slot), flags[index],
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
        self.assertEqual(len(re.findall("ssc_arithmetic_begin", assembly)), 5)
        for index, (prefix, opening, closing) in enumerate((
            (".DEFINE user_guard 1\n", ".IFDEF user_guard", ".ENDIF"),
            ("", ".IFNDEF user_guard", ".ENDIF"),
            ("", ".if 1", ".endif"),
            ("", ".IF 0\n.ELSE", ".ENDIF"),
            ("", ".IF 0\n.ELIF 1", ".ENDIF"),
            (".DEFINE user_guard 1\n", ".IFDEF user_guard\n.IFNDEF other_guard", ".ENDIF\n.ENDIF"),
        )):
            work = directory / f"unknown-{index}"
            work.mkdir()
            guarded = assembly.replace("      ; ssc_arithmetic_begin", opening+"\n      ; ssc_arithmetic_begin")
            guarded = prefix+guarded.replace("      ; ssc_arithmetic_end\n", "      ; ssc_arithmetic_end\n"+closing+"\n")
            (work / "main.asm").write_text(guarded)
            run(linker, ["-lsms", "-share-arithmetic", "-v", "-o", "linked.sms", "main.asm"], work, "linker.log")
            image = read_image(work)
            self.assertEqual(image.rom, inline["ra"].rom)
            self.assertFalse(any(name.startswith("ssc.arithmetic_") for name in image.symbols))
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
