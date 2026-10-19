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


ROOT = Path(__file__).resolve().parent.parent
BASELINE = Path(__file__).with_name("z80_compiler_baseline.json")
CASES = 17
ROUTINES = {
    "calculationMultiplyAdd": ("calculations-1", "main.ssc"),
    "calculationProductRestore": ("calculations-2", "main.ssc"),
    "calculationShiftRestore": ("calculations-2", "main.ssc"),
    "calculationPartitionRestore": ("calculations-2", "main.ssc"),
    "calculationArrayWorkload": ("calculations-2", "main.ssc"),
    "calculationStencilWorkload": ("calculations-2", "bank_2.ssc"),
    "calculationTileWorkload": ("calculations-2", "bank_3.ssc"),
    "calculationPathWorkload": ("calculations-2", "bank_3.ssc"),
    "unpackPackBits": ("calculations-1", "bank_1.ssc"),
    "calculationPackedCrc": ("calculations-1", "bank_1.ssc"),
    "sumBytes": None,
    "copyBytes": None,
    "fillBytes": None,
}
DEPENDENCIES = {
    "calculationArrayWorkload": ["calculationPartitionRestore", "calculationProductRestore", "calculationShiftRestore"],
    "calculationStencilWorkload": ["calculationShiftRestore", "calculationPartitionRestore"],
    "calculationPathWorkload": ["calculationPartitionRestore"],
    "calculationPackedCrc": ["unpackPackBits"],
}
MODES = {
    "ssc-ra": ("ssc", ["-ra"]),
    "ssc-no-ra": ("ssc", ["-no-ra"]),
    "sdcc-size": ("sdcc", ["--opt-code-size"]),
    "sdcc-speed": ("sdcc", ["--opt-code-speed"]),
    "sccz80-size": ("sccz80", []),
    "sccz80-speed": ("sccz80", ["--opt-code-speed"]),
    "zsdcc-size": ("zsdcc", ["--opt-code-size"]),
    "zsdcc-speed": ("zsdcc", ["--opt-code-speed"]),
}
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
    for name, location in ROUTINES.items():
        if location is None:
            result[name] = f"u16 {name}(u16 seed, u16 count, u8 *bytes) {{ {PORTABLE[name]} }}"
            continue
        path = ROOT / "tests" / "z80" / "sms" / location[0] / location[1]
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
    text = "u8 bench_buffer[512];\nu16 bench_params[4];\nu16 bench_result;\n"
    if name == "calculationPackedCrc":
        text += "const u8 packedTiles[104] = {" + ",".join(str(byte) for byte in packed) + "};\n"
    text += "\n".join(functions[item] for item in names) + "\n"
    text += f"void bench(void) {{ bench_result = {name}({', '.join(arguments)}); }}\n"
    if kind == "ssc":
        text += "void main(void) { bench(); }\n"
    else:
        text = re.sub(r"\b(bytes\[[^\]]+\]) << 8", r"(u16)\1 << 8", text)
        text = "typedef unsigned char u8;\ntypedef unsigned int u16;\n" + text
    return text


def inputs(name, seed):
    if ROUTINES[name] is not None:
        parameters, buffer, expected_buffer, _address, width, expected, ports = real_oracle(
            ROUTINES[name][0], name, seed, CASES,
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
        count = len(PACK_BITS_VECTORS) if name == "unpackPackBits" else CASES + (3 if name.endswith("Workload") else 0)
        result[name] = {
            "ssc_sha256": hashlib.sha256(source_for(name, "ssc", functions, packed).encode()).hexdigest(),
            "c_sha256": hashlib.sha256(source_for(name, "sdcc", functions, packed).encode()).hexdigest(),
            "cases": count,
            "oracle_sha256": hashlib.sha256(json.dumps([
                [parameters, list(buffer), list(expected_buffer), expected]
                for parameters, buffer, expected_buffer, expected in (inputs(name, seed) for seed in range(count))
            ], separators=(",", ":")).encode()).hexdigest(),
        }
    return result


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
        code_bytes = symbols["l__CODE"] - wrapper_bytes - (104 if name == "calculationPackedCrc" else 0)
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
        elapsed += run([tools["linker"], "-lsms", "-ro", "1", "-v", "-o", "linked.sms", "main.asm"],
                       directory, "link.log", commands)
    elif kind == "sdcc":
        elapsed = run([tools["sdcc"], "-mz80", *flags, *FLAGS["sdcc"], "-o", "linked.ihx", "main.c"],
                      directory, "compile.log", commands)
    else:
        clib = "new" if kind == "sccz80" else "sdcc_ix"
        allocation_flags = [] if kind == "sccz80" else FLAGS["zsdcc"]
        elapsed = run([tools["zcc"], "+embedded", f"-clib={clib}", *FLAGS["zcc"],
                       *allocation_flags, *flags, "-m", "--list", "-o", "linked", "main.c"],
                      directory, "compile.log", commands)
    write_json(directory / "build.json", {"seconds": elapsed, "commands": commands})
    return directory, *load_image(name, mode, directory)


def execute(memory_image, symbols, name, parameters, initial_buffer, seed, ticks=None, directory=None):
    machine = Z80Machine()
    machine.af = 0
    memory = machine.memory
    memory[:] = memory_image
    buffer_address = symbols["bench_buffer"]
    memory[buffer_address:buffer_address + 512] = b"\xa5" * 512
    memory[buffer_address:buffer_address + len(initial_buffer)] = initial_buffer
    for index, value in enumerate(parameters):
        address = symbols["bench_params"] + index * 2
        memory[address:address + 2] = (value & 65535).to_bytes(2, "little")
    result_address = symbols["bench_result"]
    memory[result_address:result_address + 2] = b"\xa5\xa5"
    if name == "calculationPackedCrc":
        memory[symbols["packedTiles"] + 103] ^= seed & 255
    frame, sentinel = 0xd000, 0x7fff
    memory[frame - 1:frame + 1] = sentinel.to_bytes(2, "little")
    machine.de, machine.sp, machine.pc = frame, frame - 1, symbols["bench"]
    if ticks is not None:
        snapshot = bytearray(memory)
        boot = bytes([
            0x11, 0, 0xd0, 0x31, 0xff, 0xcf, 0x01, 0, 0, 0xc5, 0xf1,
            0x21, 0, 0, 0xdd, 0x21, 0, 0, 0xfd, 0x21, 0, 0,
            0xc3, symbols["bench"] & 255, symbols["bench"] >> 8,
        ])
        snapshot[0x7f00:0x7f00 + len(boot)] = boot
        (directory / "independent-timing.bin").write_bytes(snapshot)
    counters = {"cycles": 0, "instructions": 0, "kernel_cycles": 0, "kernel_instructions": 0,
                "kernel_stack_reads": 0, "kernel_stack_writes": 0}
    kernel_active = kernel_done = False
    resume = kernel_sp = None
    lowest_sp = frame - 1

    def read_memory(address):
        if kernel_active:
            counters["kernel_stack_reads"] += 1
        return memory[address]

    def write_memory(address, value):
        if 0xc800 <= address < frame - 1:
            if kernel_active:
                counters["kernel_stack_writes"] += 1
        elif not (buffer_address <= address < buffer_address + 512 or result_address <= address < result_address + 2):
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
        if counters["instructions"] >= 2000000:
            raise RuntimeError("Instruction limit exceeded")
        if machine.pc == symbols[name] and not kernel_done and not kernel_active:
            kernel_active = True
            kernel_sp = machine.de - 1 if "mainmain" in symbols else machine.sp
            resume = memory[kernel_sp] + 256 * memory[(kernel_sp + 1) & 65535]
        counters["instructions"] += 1
        if kernel_active:
            counters["kernel_instructions"] += 1
        cycles = step(machine)
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
            "-counter", "100000000", str(directory / "independent-timing.bin"),
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, timeout=60)
        (directory / "independent-timing.log").write_bytes(completed.stdout)
        if int(completed.stdout.strip()) != counters["cycles"]:
            raise RuntimeError(f"Independent emulator cycle mismatch: {name}")
    counters["stack_high_water"] = frame - 1 - lowest_sp
    value = memory[result_address] + 256 * memory[result_address + 1]
    return value, bytes(memory[buffer_address:buffer_address + 512]), counters


def compare_result(result, reference, allow_competitor_drift=False):
    issues = []
    candidate = result["mode"].startswith("ssc-")
    for metric in METRICS:
        current, previous = result[metric], reference[metric]
        if (candidate and current > previous) or (not candidate and current != previous):
            issues.append({"routine": result["routine"], "mode": result["mode"], "metric": metric,
                           "baseline": previous, "current": current,
                           "fatal": candidate or not allow_competitor_drift})
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, help="New or empty artifact directory; default: system temporary directory")
    parser.add_argument("--names", choices=ROUTINES, nargs="+", default=list(ROUTINES))
    parser.add_argument("--modes", choices=MODES, nargs="+", default=list(MODES))
    parser.add_argument("--allow-competitor-drift", action="store_true",
                        help="Record, but do not fail on, changed external tool versions/results; never relax SameSameC gates")
    args = parser.parse_args()
    if len(args.names) != len(set(args.names)) or len(args.modes) != len(set(args.modes)):
        parser.error("Duplicate routine/mode selections are not allowed")
    output = args.output_dir.resolve() if args.output_dir else Path(tempfile.mkdtemp(prefix="ssc-z80-compare-"))
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error(f"Output directory must be empty: {output}")
    report = {"status": "running", "complete": set(args.names) == set(ROUTINES) and set(args.modes) == set(MODES),
              "results": [], "issues": [], "independent_timing_checks": 0}
    print(f"Artifacts: {output}", flush=True)
    try:
        reference = json.loads(BASELINE.read_text())
        if reference["flags"] != FLAGS or reference["modes"] != {mode: [kind, flags] for mode, (kind, flags) in MODES.items()}:
            raise RuntimeError("Compiler settings changed; version the benchmark baseline instead of silently changing flags")
        functions = sources()
        packed = (ROOT / "tests" / "z80" / "sms" / "calculations-1" / "tiles.packbits").read_bytes()
        measured_corpus = corpus(functions, packed)
        if measured_corpus != reference["corpus"]:
            raise RuntimeError("Benchmark sources, asset or oracle changed; preserve/review the corpus before comparing baselines")
        report["corpus"] = measured_corpus
        report["baseline_commit"] = reference["commit"]
        kinds = {MODES[mode][0] for mode in args.modes}
        tools = {}
        if "ssc" in kinds:
            tools.update(ssc=tool(os.environ.get("SAMESAMECC", "samesamecc")),
                         linker=tool(os.environ.get("SAMESAMECL", "samesamecl")),
                         assembler=tool("wla-z80"), wlalink=tool("wlalink"))
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
                    ) else None
                    value, buffer, counters = execute(
                        memory, symbols, name, parameters, initial_buffer, seed, independent, directory,
                    )
                    expected_full = bytearray([0xa5] * 512)
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
                          "cases": len(samples), "total_kernel_cycles": sum(item["kernel_cycles"] for item in samples),
                          "total_wrapper_cycles": sum(item["cycles"] - item["kernel_cycles"] for item in samples),
                          "max_stack_bytes": max(item["stack_high_water"] for item in samples),
                          "mean_kernel_cycles": statistics.mean(item["kernel_cycles"] for item in samples)}
                result["baseline_delta"] = {
                    metric: result[metric] - baseline_rows[name, mode][metric] for metric in METRICS
                }
                write_json(directory / "metrics.json", {"summary": result, "samples": samples})
                report["results"].append(result)
                issues = compare_result(result, baseline_rows[name, mode], args.allow_competitor_drift)
                report["issues"].extend(issues)
                print(json.dumps({**result, "baseline_changes": issues}), flush=True)
        lookup = {(row["routine"], row["mode"]): row for row in report["results"]}
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
