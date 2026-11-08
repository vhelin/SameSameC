#!/usr/bin/env python3
"""Compare actual Z80 kernel code and execution with pinned SDCC/z88dk controls."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import tempfile
import time

from check_register_allocator import PACK_BITS_VECTORS, calibrate_counters, real_oracle, write_json
from ra_cpu import Z80Machine, step
from z80_application_oracles import APPLICATION_CASES, application_inputs
from z80_algorithm_oracles import ALGORITHM_CASES, BUFFER_SIZES, algorithm_inputs


ROOT = Path(__file__).resolve().parent.parent
BASELINE = Path(__file__).with_name("z80_compiler_baseline.json")
APPLICATION_BASELINE = Path(__file__).with_name("z80_application_baseline_v2.json")
ALGORITHM_BASELINE = Path(__file__).with_name("z80_algorithm_baseline_v3.json")
INSTRUCTION_LIMITS = {"calculationAStarWorkload": 20000000, "calculationQuickSortWorkload": 5000000}
CASES = 17
ROUTINES = {
    "calculationMultiplyAdd": ("tests", "z80", "sms", "calculations-1", "main.ssc"),
    "calculationProductRestore": ("tests", "z80", "sms", "calculations-2", "main.ssc"),
    "calculationShiftRestore": ("tests", "z80", "sms", "calculations-2", "main.ssc"),
    "calculationPartitionRestore": ("tests", "z80", "sms", "calculations-2", "main.ssc"),
    "calculationArrayWorkload": ("tests", "z80", "sms", "calculations-2", "main.ssc"),
    "calculationStencilWorkload": ("tests", "z80", "sms", "calculations-2", "bank_2.ssc"),
    "calculationTileWorkload": ("tests", "z80", "sms", "calculations-2", "bank_3.ssc"),
    "calculationPathWorkload": ("tests", "z80", "sms", "calculations-2", "bank_3.ssc"),
    "unpackPackBits": ("tests", "z80", "sms", "calculations-1", "bank_1.ssc"),
    "calculationPackedCrc": ("tests", "z80", "sms", "calculations-1", "bank_1.ssc"),
    "sumBytes": None,
    "copyBytes": None,
    "fillBytes": None,
    **{name: ("ci", "z80_application_workloads.ssc") for name in APPLICATION_CASES},
    **{name: ("ci", "z80_algorithm_workloads.ssc") for name in ALGORITHM_CASES},
}
DEPENDENCIES = {
    "calculationArrayWorkload": ["calculationPartitionRestore", "calculationProductRestore", "calculationShiftRestore"],
    "calculationStencilWorkload": ["calculationShiftRestore", "calculationPartitionRestore"],
    "calculationPathWorkload": ["calculationPartitionRestore"],
    "calculationPackedCrc": ["unpackPackBits"],
    "calculationTrackerWorkload": ["calculationMultiplyAdd"],
    **{name: ["calculationCrc16"] for name in ALGORITHM_CASES},
}
MODES = {
    "ssc-ra": ("ssc", ["-ra"]),
    "sdcc-size": ("sdcc", ["--opt-code-size"]),
    "sdcc-speed": ("sdcc", ["--opt-code-speed"]),
    "sccz80-size": ("sccz80", []),
    "sccz80-speed": ("sccz80", ["--opt-code-speed"]),
    "zsdcc-size": ("zsdcc", ["--opt-code-size"]),
    "zsdcc-speed": ("zsdcc", ["--opt-code-speed"]),
}
DEFAULT_MODES = tuple(MODES)
MODES["ssc-ra-shared"] = MODES["ssc-ra"]
MODES["ssc-ra-regcalls"] = ("ssc", ["-ra", "-z80-regcalls"])
MODES["ssc-ra-regcalls-shared"] = MODES["ssc-ra-regcalls"]
FLAGS = {
    "ssc": ["-q", "-mz80", "-rs", "3"],
    "sdcc": ["--max-allocs-per-node", "200000", "--no-std-crt0", "--code-loc", "0x100", "--data-loc", "0xc000"],
    "zcc": ["--no-crt", "-O3", "-SO3"],
    "zsdcc": ["--max-allocs-per-node200000"],
}
PORTABLE = {
    "sumBytes": "u16 sum = seed; u16 i; for (i = 0; i < count; i++) sum += bytes[i]; return sum;",
    "copyBytes": "u16 i; for (i = 0; i < count; i++) bytes[256 + i] = bytes[i]; return count;",
    "fillBytes": "u16 i; for (i = 0; i < count; i++) bytes[i] = seed; return count;",
}
METRICS = ("code_bytes", "wrapper_bytes", "total_kernel_cycles", "total_wrapper_cycles", "max_stack_bytes")


def sources():
    result = {}
    for name, location in {
        **ROUTINES, "calculationCrc16": ("ci", "z80_algorithm_workloads.ssc"),
    }.items():
        if location is None:
            result[name] = f"u16 {name}(u16 seed, u16 count, u8 *bytes) {{ {PORTABLE[name]} }}"
            continue
        path = ROOT.joinpath(*location)
        text = path.read_text()
        match = re.search(rf"^u16 {name}\([^;\n]*\) \{{", text, re.M)
        if match is None:
            raise RuntimeError(f"Cannot extract benchmark function {name} from {path}")
        depth, end = 1, match.end()
        while depth and end < len(text):
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        if depth:
            raise RuntimeError(f"Unbalanced benchmark function {name} in {path}")
        result[name] = text[match.start():end]
    result["astarMaps"] = (ROOT / "ci" / "z80_maze_maps.ssc").read_text()
    return result


def source_for(name, kind, functions, packed):
    names = [*DEPENDENCIES.get(name, []), name]
    signature = functions[name].split("{", 1)[0]
    parameters = re.search(r"\((.*)\)", signature)[1].split(",")
    arguments = []
    for index, parameter in enumerate(parameters):
        if "*" not in parameter:
            arguments.append(f"bench_params[{index}]")
        elif name == "unpackPackBits":
            arguments.append("bench_buffer" if index == 0 else "bench_buffer + 160")
        else:
            arguments.append("bench_buffer + 1" if name == "calculationPackedCrc" else "bench_buffer")
    text = f"u8 bench_buffer[{BUFFER_SIZES.get(name, 512)}];\nu16 bench_params[4];\nu16 bench_result;\n"
    if name == "calculationPackedCrc":
        text += "const u8 packedTiles[104] = {" + ",".join(str(byte) for byte in packed) + "};\n"
    if name == "calculationAStarWorkload":
        text += functions["astarMaps"] + "\n"
    text += "\n".join(functions[item] for item in names) + "\n"
    text += f"void bench(void) {{ bench_result = {name}({', '.join(arguments)}); }}\n"
    if kind == "ssc":
        text += "void main(void) { bench(); }\n"
    else:
        text = re.sub(r"\b(bytes\[[^\]]+\]) << 8", r"(u16)\1 << 8", text)
        text = "typedef unsigned char u8;\ntypedef unsigned int u16;\n" + text
        if name == "calculationQuickSortWorkload":
            text = "typedef signed int s16;\n" + text
    return text


def inputs(name, seed):
    if name in ALGORITHM_CASES:
        return algorithm_inputs(name, seed)
    if name in APPLICATION_CASES:
        return application_inputs(name, seed)
    if ROUTINES[name] is not None:
        parameters, buffer, expected_buffer, _address, width, expected, ports = real_oracle(
            ROUTINES[name][-2], name, seed, CASES,
        )
        if width != 2 or ports:
            raise RuntimeError(f"Unexpected oracle contract for {name}")
        return parameters, buffer, expected_buffer, expected
    count = [0, 1, 2, 7, 16, 31, 63, 127, 128, 255, 256][seed % 11]
    value = (seed * 7919 + 32767) & 65535
    buffer = bytearray((index * 47 + seed * 11) & 255 for index in range(512))
    expected_buffer = buffer.copy()
    expected = count
    if name == "sumBytes":
        expected = (value + sum(buffer[:count])) & 65535
    elif name == "copyBytes":
        expected_buffer[256:256 + count] = buffer[:count]
    else:
        expected_buffer[:count] = bytes([value & 255]) * count
    return [value, count, 0xc100], buffer, expected_buffer, expected


def corpus(functions, packed):
    result = {}
    for name in ROUTINES:
        count = {**APPLICATION_CASES, **ALGORITHM_CASES}.get(
            name, len(PACK_BITS_VECTORS) if name == "unpackPackBits" else CASES + (3 if name.endswith("Workload") else 0))
        result[name] = {
            "ssc_sha256": hashlib.sha256(source_for(name, "ssc", functions, packed).encode()).hexdigest(),
            "c_sha256": hashlib.sha256(source_for(name, "sdcc", functions, packed).encode()).hexdigest(),
            "cases": count,
            "oracle_sha256": hashlib.sha256(json.dumps([
                [parameters, list(buffer), list(expected_buffer), expected]
                for parameters, buffer, expected_buffer, expected in (inputs(name, seed) for seed in range(count))
            ], separators=(",", ":")).encode()).hexdigest(),
        }
        if name in ALGORITHM_CASES:
            result[name].update(buffer_bytes=BUFFER_SIZES[name], frame_top=0xdff0,
                                instruction_limit=INSTRUCTION_LIMITS[name],
                                bss_origin=0xc000,
                                rom_data_bytes=1024 if name == "calculationAStarWorkload" else 0)
    return result


def baseline():
    original = BASELINE.read_text(encoding="utf-8")
    reference = json.loads(original)
    parent, parent_path = original, BASELINE
    for path, schema, cases, label in (
        (APPLICATION_BASELINE, 2, APPLICATION_CASES, "application"),
        (ALGORITHM_BASELINE, 3, ALGORITHM_CASES, "algorithm"),
    ):
        text = path.read_text(encoding="utf-8")
        extension = json.loads(text)
        expected = {(name, mode) for name in cases for mode in DEFAULT_MODES}
        pairs = [(row["routine"], row["mode"]) for row in extension["results"]]
        if (extension["schema"] != schema
                or extension["parent_baseline"] != parent_path.name
                or extension["parent_sha256"] != hashlib.sha256(parent.encode("utf-8")).hexdigest()
                or set(extension["corpus"]) != set(cases)
                or set(extension["modes"]) != set(DEFAULT_MODES)
                or set(pairs) != expected or len(pairs) != len(expected)
                or set(reference["corpus"]) & set(extension["corpus"])):
            raise RuntimeError(f"Invalid versioned {label} baseline or historical parent fingerprint")
        reference["corpus"].update(extension["corpus"])
        reference["results"].extend(extension["results"])
        reference[f"{label}_commit"] = extension["compiler_commit"]
        parent, parent_path = text, path
    reference["modes"] = {mode: reference["modes"][mode] for mode in DEFAULT_MODES}
    return reference


def tool(command):
    executable = shutil.which(command)
    if executable is None:
        raise RuntimeError(f"Required benchmark tool not found: {command}; see Z80_CODEGEN_PLAN.md")
    return str(Path(executable).resolve())


def tool_version(name, command):
    arguments = ["-h"] if name == "sccz80" else ["--version"]
    completed = subprocess.run([command, *arguments], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = completed.stdout.decode(errors="replace")
    help_status = name == "sccz80" and completed.returncode == 1 and re.search(
        r"^Small-C/Plus - z80 Crosscompiler\s+Version:", text, re.M,
    )
    if completed.returncode and not help_status:
        raise RuntimeError(f"Cannot identify {name}: exit {completed.returncode}: {text}")
    return text


def run(command, directory, log, commands):
    commands.append(command)
    start = time.perf_counter()
    completed = subprocess.run(command, cwd=directory, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (directory / log).write_bytes(completed.stdout)
    if completed.returncode:
        raise RuntimeError(f"Command failed ({completed.returncode}): {command}; inspect {directory / log}")
    return time.perf_counter() - start


def intel_hex(path):
    memory = bytearray(65536)
    ended = False
    for line in path.read_text().splitlines():
        if not line.startswith(":"):
            raise RuntimeError(f"Malformed Intel HEX record in {path}")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) & 255:
            raise RuntimeError(f"Bad Intel HEX length/checksum in {path}")
        length, kind = record[0], record[3]
        address = int.from_bytes(record[1:3], "big")
        if kind == 0:
            if address + length > len(memory):
                raise RuntimeError(f"Intel HEX address overflow in {path}")
            memory[address:address + length] = record[4:4 + length]
        elif kind == 1 and length == 0:
            ended = True
            break
        else:
            raise RuntimeError(f"Unexpected Intel HEX record {kind} in {path}")
    if not ended:
        raise RuntimeError(f"Missing Intel HEX end record in {path}")
    return memory


def load_image(name, mode, directory):
    kind = MODES[mode][0]
    symbols = {}
    if kind == "ssc":
        text = (directory / "linked.sym").read_text()
        for match in re.finditer(r"^00:([\da-f]+) (\S+)$", text, re.M | re.I):
            symbols[match[2]] = int(match[1], 16)
        sections = [
            (match[2], int(match[1], 16)) for match in re.finditer(
                r"^[\da-f]+ 00:[\da-f]+ [\da-f]+ ([\da-f]+) (\S+)$", text, re.M | re.I,
            )
        ]
        excluded = {"bench", "main", "mainmain", "global_variables_main_init", "global_variables_main_rom"}
        code_bytes = sum(size for section, size in sections if section not in excluded)
        wrapper_bytes = dict(sections)["bench"]
        memory = bytearray(65536)
        rom = (directory / "linked.sms").read_bytes()
        memory[:len(rom)] = rom
    elif kind == "sdcc":
        text = (directory / "linked.map").read_text()
        for match in re.finditer(r"^\s+([\da-f]{8})\s+([A-Za-z_]\w+)\s", text, re.M | re.I):
            symbols[match[2]] = int(match[1], 16)
        relocatable = (directory / "linked.rel").read_text()
        module_bytes = int(re.search(r"^A _CODE size ([\da-f]+) ", relocatable, re.M | re.I)[1], 16)
        wrapper_bytes = module_bytes - (symbols["_bench"] - symbols["s__CODE"])
        data_bytes = 104 if name == "calculationPackedCrc" else 1024 if name == "calculationAStarWorkload" else 0
        code_bytes = symbols["l__CODE"] - wrapper_bytes - data_bytes
        memory = intel_hex(directory / "linked.ihx")
        symbols = {key.removeprefix("_"): value for key, value in symbols.items()}
    else:
        text = (directory / "linked.map").read_text()
        for match in re.finditer(r"^(\w+)\s+= \$([\da-f]+)", text, re.M | re.I):
            symbols[match[1]] = int(match[2], 16)
        total_code = sum(value for key, value in symbols.items() if key.startswith("__code_") and key.endswith("_size"))
        wrapper_bytes = symbols["__code_compiler_tail"] - symbols["_bench"]
        code_bytes = total_code - wrapper_bytes
        binary = (directory / "linked").read_bytes()
        memory = bytearray(65536)
        memory[symbols["__head"]:symbols["__head"] + len(binary)] = binary
        symbols = {key.removeprefix("_"): value for key, value in symbols.items()}
    if len(memory) != 65536 or not 0 < code_bytes < 16000 or not 0 < wrapper_bytes < 2000:
        raise RuntimeError(f"Invalid image/size accounting for {name}/{mode}")
    return memory, symbols, code_bytes, wrapper_bytes


def compile_case(name, mode, output, tools, functions, packed):
    directory = output / name / mode
    directory.mkdir(parents=True)
    kind, flags = MODES[mode]
    (directory / ("main.ssc" if kind == "ssc" else "main.c")).write_bytes(
        source_for(name, kind, functions, packed).encode(),
    )
    commands = []
    if kind == "ssc":
        elapsed = run([tools["ssc"], *flags, *FLAGS["ssc"], "-o", "main.asm", "main.ssc"],
                      directory, "compile.log", commands)
        elapsed += run([tools["linker"], "-lsms", "-ro", "1", "-v",
                        *(["-share-arithmetic"] if mode.endswith("-shared") else []),
                        "-o", "linked.sms", "main.asm"],
                       directory, "link.log", commands)
    elif kind == "sdcc":
        elapsed = run([tools["sdcc"], "-mz80", *flags, *FLAGS["sdcc"], "-o", "linked.ihx", "main.c"],
                      directory, "compile.log", commands)
    else:
        clib = "new" if kind == "sccz80" else "sdcc_ix"
        allocation_flags = [] if kind == "sccz80" else FLAGS["zsdcc"]
        ram_source = []
        if name in ALGORITHM_CASES:
            (directory / "ram.asm").write_text(
                'defc __crt_org_far = 0\ndefc __crt_org_graphics = 0\n'
                'INCLUDE "crt/classic/crt_section_code.inc"\n'
                'SECTION rodata_compiler\nSECTION bss_compiler\nORG $c000\n')
            ram_source = ["ram.asm"]
        elapsed = run([tools["zcc"], "+embedded", f"-clib={clib}", *FLAGS["zcc"],
                       *allocation_flags, *flags, "-m", "--list", "-o", "linked", *ram_source, "main.c"],
                      directory, "compile.log", commands)
    write_json(directory / "build.json", {"seconds": elapsed, "commands": commands})
    return directory, *load_image(name, mode, directory)


def calibrate_independent_clock(ticks, directory):
    # The pinned ticks build charges ADC A,H as a memory operand (7, not 4).
    # Calibrate explicitly rather than accepting an arbitrary cycle mismatch.
    probe = bytearray(65536)
    probe[0x100:0x108] = bytes([0x26, 0, 0x3e, 0, 0x8c, 0xc3, 0xff, 0x7f])
    path = directory / "independent-clock-probe.bin"
    path.write_bytes(probe)
    completed = subprocess.run([ticks, "-mz80", "-pc", "100", "-start", "100",
                                "-end", "7fff", "-counter", "1000", str(path)],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, timeout=10)
    measured = int(completed.stdout.strip())
    if measured not in (28, 31):
        raise RuntimeError(f"Independent ADC A,H clock calibration failed: expected 28/31, got {measured}")
    calibration = {"expected_probe_cycles": 28, "raw_probe_cycles": measured,
                   "adc_h_extra_cycles": measured - 28}
    write_json(directory / "independent-clock-calibration.json", calibration)
    if measured != 28:
        print("WARNING: independent ticks ADC A,H timing is 7 instead of 4; applying the calibrated "
              "3-cycle correction per executed unprefixed opcode 8C.", flush=True)
    return calibration


def execute(memory_image, symbols, name, parameters, initial_buffer, seed, ticks=None, directory=None,
            ticks_adc_h_extra_cycles=0):
    ordinary_entry = symbols.get(name)
    register_address = symbols.get(f"ssc.autocall.v1.w{len(parameters)}.{name}",
                                   symbols.get(f"ssc.regcall.v1.w{len(parameters)}.{name}",
                                               symbols.get(f"ssc.regcall.{name}")))
    if ordinary_entry is None and register_address is None:
        raise RuntimeError(f"Missing kernel entry: {name}")
    machine = Z80Machine()
    machine.af = 0
    memory = machine.memory
    memory[:] = memory_image
    buffer_address = symbols["bench_buffer"]
    buffer_size = BUFFER_SIZES.get(name, 512)
    if (len(initial_buffer) > buffer_size or not 0 <= buffer_address < buffer_address + buffer_size <= 0xc800
            or (name in ALGORITHM_CASES and buffer_address < 0xc000)):
        raise RuntimeError(f"Invalid benchmark buffer extent: {name}")
    memory[buffer_address:buffer_address + buffer_size] = b"\xa5" * buffer_size
    memory[buffer_address:buffer_address + len(initial_buffer)] = initial_buffer
    for index, value in enumerate(parameters):
        address = symbols["bench_params"] + index * 2
        memory[address:address + 2] = (value & 65535).to_bytes(2, "little")
    result_address = symbols["bench_result"]
    memory[result_address:result_address + 2] = b"\xa5\xa5"
    if name == "calculationPackedCrc":
        memory[symbols["packedTiles"] + 103] ^= seed & 255
    frame, sentinel = 0xdff0 if name in ALGORITHM_CASES else 0xd000, 0x7fff
    memory[frame - 1:frame + 1] = sentinel.to_bytes(2, "little")
    entry_sp = frame - (3 if "mainmain" in symbols else 1)
    machine.de, machine.sp, machine.pc = frame, entry_sp, symbols["bench"]
    if ticks is not None:
        snapshot = bytearray(memory)
        boot = bytes([
            0x11, frame & 255, frame >> 8, 0x31, entry_sp & 255, entry_sp >> 8, 0x01, 0, 0, 0xc5, 0xf1,
            0x21, 0, 0, 0xdd, 0x21, 0, 0, 0xfd, 0x21, 0, 0,
            0xc3, symbols["bench"] & 255, symbols["bench"] >> 8,
        ])
        snapshot[0x7f00:0x7f00 + len(boot)] = boot
        (directory / "independent-timing.bin").write_bytes(snapshot)
    counters = {"cycles": 0, "instructions": 0, "kernel_cycles": 0, "kernel_instructions": 0,
                "kernel_stack_reads": 0, "kernel_stack_writes": 0}
    adc_h_instructions = 0
    kernel_active = kernel_done = False
    resume = kernel_sp = None
    lowest_sp = frame - 1
    lowest_access = frame - 1
    instruction_access = frame - 1
    below_sp = 0

    def read_memory(address):
        nonlocal lowest_access, instruction_access
        lowest_access = min(lowest_access, address)
        instruction_access = min(instruction_access, address)
        if kernel_active:
            counters["kernel_stack_reads"] += 1
        return memory[address]

    def write_memory(address, value):
        nonlocal lowest_access, instruction_access
        if 0xc800 <= address < frame - 1:
            lowest_access = min(lowest_access, address)
            instruction_access = min(instruction_access, address)
            if kernel_active:
                counters["kernel_stack_writes"] += 1
        elif not (buffer_address <= address < buffer_address + buffer_size or result_address <= address < result_address + 2):
            raise RuntimeError(f"Out-of-bounds write {address:04x}, PC={machine.pc:04x}")
        memory[address] = value

    def unexpected_port(*arguments):
        raise RuntimeError(f"Unexpected I/O {arguments}")

    machine.set_read_callback(read_memory)
    machine.set_write_callback(write_memory)
    machine.set_input_callback(unexpected_port)
    machine.set_output_callback(unexpected_port)
    machine.mark_addrs(0xc800, frame - 0xc800 - 1, machine.READ_MARK)
    machine.mark_addrs(0, 65536, machine.WRITE_MARK)
    while machine.pc != sentinel:
        if counters["instructions"] >= INSTRUCTION_LIMITS.get(name, 2000000):
            raise RuntimeError("Instruction limit exceeded")
        register_entry = machine.pc == register_address
        if (machine.pc == ordinary_entry or register_entry) and not kernel_done and not kernel_active:
            kernel_active = True
            kernel_sp = machine.de - 1 if "mainmain" in symbols and not register_entry else machine.sp
            resume = memory[kernel_sp] + 256 * memory[(kernel_sp + 1) & 65535]
        counters["instructions"] += 1
        adc_h_instructions += memory[machine.pc] == 0x8c
        if kernel_active:
            counters["kernel_instructions"] += 1
        instruction_access = frame - 1
        before_sp = machine.sp
        cycles = step(machine)
        # PUSH/POP callbacks can precede the core's SP update.
        below_sp = max(below_sp, min(before_sp, machine.sp) - instruction_access)
        counters["cycles"] += cycles
        if kernel_active:
            counters["kernel_cycles"] += cycles
        lowest_sp = min(lowest_sp, machine.sp)
        if kernel_active and machine.pc == resume and machine.sp >= kernel_sp + 2:
            kernel_active = False
            kernel_done = True
    if not kernel_done or machine.sp != frame + 1:
        raise RuntimeError(f"Kernel return/stack restoration failure: {name}")
    if "mainmain" in symbols and machine.de != frame:
        raise RuntimeError(f"SameSameC frame restoration failure: {name}")
    if ticks is not None:
        completed = subprocess.run([
            ticks, "-mz80", "-pc", "7f00", "-start", f"{symbols['bench']:x}", "-end", "7fff",
            "-counter", "500000000" if name == "calculationAStarWorkload" else "100000000",
            str(directory / "independent-timing.bin"),
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True,
            timeout=120 if name == "calculationAStarWorkload" else 60)
        (directory / "independent-timing.log").write_bytes(completed.stdout)
        counters["independent_raw_cycles"] = int(completed.stdout.strip())
        counters["independent_adc_h_correction"] = adc_h_instructions * ticks_adc_h_extra_cycles
        if counters["independent_raw_cycles"] - counters["independent_adc_h_correction"] != counters["cycles"]:
            raise RuntimeError(f"Independent emulator cycle mismatch: {name}")
    counters["stack_high_water"] = frame - 1 - lowest_sp
    counters["stack_access_extent"] = frame - 1 - lowest_access
    counters["below_sp_extent"] = below_sp
    # Include the entry return address, not only displacement below entry SP.
    counters["frame_footprint"] = frame + 1 - min(lowest_sp, lowest_access)
    value = memory[result_address] + 256 * memory[result_address + 1]
    return value, bytes(memory[buffer_address:buffer_address + buffer_size]), counters


def compare_result(result, reference, allow_competitor_drift=False):
    issues = []
    candidate = result["mode"].startswith("ssc-")
    shared = result["mode"].endswith("-shared")
    metrics = (*METRICS, "max_stack_access_bytes", "max_below_sp_bytes", "max_frame_footprint_bytes") if shared else METRICS
    if candidate and not shared and (result.get("register_calls") or result["mode"] == "ssc-ra-regcalls"):
        metrics = ("complete_code_bytes", "complete_cycles", "max_stack_bytes")
        # Paired reports additionally measure frame-access extent; the pinned baseline predates it.
        metrics += tuple(metric for metric in (
            "max_stack_access_bytes", "max_below_sp_bytes", "max_frame_footprint_bytes",
        ) if metric in reference)
    for metric in metrics:
        if metric == "complete_code_bytes":
            current, previous = (row["code_bytes"] + row["wrapper_bytes"] for row in (result, reference))
        elif metric == "complete_cycles":
            current, previous = (row["total_kernel_cycles"] + row["total_wrapper_cycles"] for row in (result, reference))
        else:
            current, previous = result[metric], reference[metric]
        if shared:
            invalid = (
                current < previous or (current - previous) % 27 != 0 if metric == "total_kernel_cycles" else
                current > previous + 2 if metric.startswith("max_") else
                current > previous if metric == "code_bytes" else current != previous
            )
        else:
            invalid = (candidate and current > previous) or (not candidate and current != previous)
        if invalid:
            issues.append({"routine": result["routine"], "mode": result["mode"], "metric": metric,
                           "baseline": previous, "current": current,
                           "fatal": candidate or not allow_competitor_drift})
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, help="New or empty artifact directory; default: system temporary directory")
    parser.add_argument("--names", choices=ROUTINES, nargs="+", default=list(ROUTINES))
    parser.add_argument("--modes", choices=MODES, nargs="+", default=list(DEFAULT_MODES),
                        help="Seven RA-only/competitor modes by default; shared and register-call modes require their ordinary paired modes")
    parser.add_argument("--allow-competitor-drift", action="store_true",
                        help="Record, but do not fail on, changed external tool versions/results; never relax SameSameC gates")
    args = parser.parse_args()
    if len(args.names) != len(set(args.names)) or len(args.modes) != len(set(args.modes)):
        parser.error("Duplicate routine/mode selections are not allowed")
    shared_requested = any(mode.endswith("-shared") for mode in args.modes)
    if any(mode.endswith("-shared") and mode.removesuffix("-shared") not in args.modes for mode in args.modes):
        parser.error("Each shared arithmetic mode requires its ordinary paired mode")
    if "ssc-ra-regcalls" in args.modes and "ssc-ra" not in args.modes:
        parser.error("Register calls require the ordinary ssc-ra paired mode")
    output = args.output_dir.resolve() if args.output_dir else Path(tempfile.mkdtemp(prefix="ssc-z80-compare-"))
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error(f"Output directory must be empty: {output}")
    report = {"status": "running", "complete": set(args.names) == set(ROUTINES) and set(DEFAULT_MODES).issubset(args.modes),
              "results": [], "issues": [], "independent_timing_checks": 0}
    print(f"Artifacts: {output}", flush=True)
    try:
        reference = baseline()
        if reference["flags"] != FLAGS or reference["modes"] != {
            mode: [MODES[mode][0], MODES[mode][1]] for mode in DEFAULT_MODES
        }:
            raise RuntimeError("Compiler settings changed; version the benchmark baseline instead of silently changing flags")
        functions = sources()
        packed = (ROOT / "tests" / "z80" / "sms" / "calculations-1" / "tiles.packbits").read_bytes()
        measured_corpus = corpus(functions, packed)
        if measured_corpus != reference["corpus"]:
            raise RuntimeError("Benchmark sources, asset or oracle changed; preserve/review the corpus before comparing baselines")
        report["corpus"] = measured_corpus
        report["corpus_version"] = 3
        report["baseline_commit"] = reference["commit"]
        report["application_baseline_commit"] = reference["application_commit"]
        report["algorithm_baseline_commit"] = reference["algorithm_commit"]
        kinds = {MODES[mode][0] for mode in args.modes}
        tools = {}
        if "ssc" in kinds:
            tools.update(ssc=tool(os.environ.get("SAMESAMECC", "samesamecc")),
                         linker=tool(os.environ.get("SAMESAMECL", "samesamecl")),
                         assembler=tool("wla-z80"), wlalink=tool("wlalink"))
            if shared_requested:
                tools["ticks"] = tool("z88dk-ticks")
        if "sdcc" in kinds:
            tools["sdcc"] = tool(os.environ.get("SDCC", "sdcc"))
        if kinds & {"sccz80", "zsdcc"}:
            tools["zcc"] = tool(os.environ.get("ZCC", "zcc"))
            tools["ticks"] = tool("z88dk-ticks")
            os.environ["PATH"] = str(Path(tools["zcc"]).parent) + os.pathsep + os.environ["PATH"]
        report["tools"] = {name: {"path": path, "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest()}
                           for name, path in tools.items()}
        for name in kinds & {"sdcc", "sccz80", "zsdcc"}:
            command = tools["sdcc"] if name == "sdcc" else tool("z88dk-" + name)
            version = tool_version(name, command)
            report["tools"][name] = {"path": command, "sha256": hashlib.sha256(Path(command).read_bytes()).hexdigest(),
                                     "version": version}
            if reference["versions"][name] not in version:
                issue = {"tool": name, "expected_version": reference["versions"][name],
                         "actual_version": version, "fatal": not args.allow_competitor_drift}
                report["issues"].append(issue)
                if issue["fatal"]:
                    raise RuntimeError(f"Unpinned {name} version; see --allow-competitor-drift")
        calibrate_counters()
        clock_calibration = calibrate_independent_clock(tools["ticks"], output) if "ticks" in tools else None
        if clock_calibration is not None:
            report["independent_clock_calibration"] = clock_calibration
        baseline_rows = {(row["routine"], row["mode"]): row for row in reference["results"]}
        for name in args.names:
            for mode in args.modes:
                directory, memory, symbols, code_bytes, wrapper_bytes = compile_case(
                    name, mode, output, tools, functions, packed,
                )
                samples = []
                for seed in range(measured_corpus[name]["cases"]):
                    parameters, initial_buffer, expected_buffer, expected = inputs(name, seed)
                    independent = tools.get("ticks") if (
                        (name == "fillBytes" and seed == 10)
                        or (seed == 0 and name in ("calculationMultiplyAdd", "calculationPackedCrc"))
                        or (seed == 0 and name in APPLICATION_CASES)
                        or (name in ALGORITHM_CASES)
                        or (seed == 0 and shared_requested)
                    ) else None
                    value, buffer, counters = execute(
                        memory, symbols, name, parameters, initial_buffer, seed, independent, directory,
                        clock_calibration["adc_h_extra_cycles"] if clock_calibration else 0,
                    )
                    expected_full = bytearray([0xa5] * BUFFER_SIZES.get(name, 512))
                    expected_full[:len(expected_buffer)] = expected_buffer
                    if value != expected or buffer != expected_full:
                        write_json(directory / "failure.json", {
                            "routine": name, "mode": mode, "seed": seed, "parameters": parameters,
                            "expected": expected, "actual": value, "buffer_match": buffer == expected_full,
                        })
                        raise RuntimeError(f"Incorrect result: {name}/{mode} seed={seed}")
                    report["independent_timing_checks"] += independent is not None
                    samples.append({"seed": seed, **counters})
                result = {"routine": name, "mode": mode, "code_bytes": code_bytes, "wrapper_bytes": wrapper_bytes,
                          "register_calls": any(symbol.startswith(("ssc.regcall.", "ssc.autocall.")) for symbol in symbols),
                          "cases": len(samples), "total_kernel_cycles": sum(item["kernel_cycles"] for item in samples),
                          "total_wrapper_cycles": sum(item["cycles"] - item["kernel_cycles"] for item in samples),
                          "max_stack_bytes": max(item["stack_high_water"] for item in samples),
                          "max_stack_access_bytes": max(item["stack_access_extent"] for item in samples),
                          "max_below_sp_bytes": max(item["below_sp_extent"] for item in samples),
                          "max_frame_footprint_bytes": max(item["frame_footprint"] for item in samples),
                          "mean_kernel_cycles": statistics.mean(item["kernel_cycles"] for item in samples)}
                if name in ALGORITHM_CASES:
                    result["buffer_bytes"] = BUFFER_SIZES[name]
                    result["rom_data_bytes"] = measured_corpus[name]["rom_data_bytes"]
                result["baseline_delta"] = {
                    metric: result[metric] - baseline_rows[
                        name, mode.removesuffix("-shared").removesuffix("-regcalls")
                    ][metric] for metric in METRICS
                }
                if mode.endswith("-shared"):
                    result["arithmetic_helpers"] = {
                        match[2]: int(match[1], 16) for match in re.finditer(
                            r"^[\da-f]+ 00:[\da-f]+ [\da-f]+ ([\da-f]+) (ssc\.arithmetic_\w+)$",
                            (directory / "linked.sym").read_text(), re.M | re.I,
                        )
                    }
                    result["arithmetic_call_sites"] = len(re.findall(
                        r"^\s+CALL ssc\.arithmetic_", (directory / "linked.sms.combined.asm").read_text(), re.M,
                    ))
                write_json(directory / "metrics.json", {"summary": result, "samples": samples})
                report["results"].append(result)
                issues = [] if mode.endswith("-shared") or mode == "ssc-ra-regcalls" else compare_result(
                    result, baseline_rows[name, mode], args.allow_competitor_drift,
                )
                report["issues"].extend(issues)
                print(json.dumps({**result, "baseline_changes": issues}), flush=True)
        lookup = {(row["routine"], row["mode"]): row for row in report["results"]}
        for row in report["results"]:
            if row["mode"].endswith("-shared"):
                paired = lookup[row["routine"], row["mode"].removesuffix("-shared")]
                row["paired_delta"] = {metric: row[metric] - paired[metric] for metric in (
                    *METRICS, "max_stack_access_bytes", "max_below_sp_bytes", "max_frame_footprint_bytes",
                )}
                row["dynamic_helper_calls"] = row["paired_delta"]["total_kernel_cycles"] // 27
                report["issues"].extend(compare_result(row, paired))
            elif row["mode"] == "ssc-ra-regcalls":
                paired = lookup[row["routine"], "ssc-ra"]
                row["paired_delta"] = {metric: row[metric] - paired[metric] for metric in (
                    *METRICS, "max_stack_access_bytes", "max_below_sp_bytes", "max_frame_footprint_bytes",
                )}
                report["issues"].extend(compare_result(row, paired))
        if "ssc-ra" in args.modes:
            report["same_samec_ratios"] = {
                mode: {
                    metric: statistics.geometric_mean(
                        lookup[name, "ssc-ra"][metric] / lookup[name, mode][metric] for name in args.names
                    ) for metric in ("code_bytes", "total_kernel_cycles")
                } for mode in args.modes if mode != "ssc-ra"
            }
        if any(issue["fatal"] for issue in report["issues"]):
            raise RuntimeError("Baseline regression or pinned competitor drift; inspect report.json")
        report["status"] = "competitor_drift" if report["issues"] else "passed" if report["complete"] else "partial"
        print(f"{report['status'].upper()}: {len(report['results'])} builds, "
              f"{sum(row['cases'] for row in report['results'])} calls; artifacts={output}")
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        write_json(output / "report.json", report)


if __name__ == "__main__":
    main()
