#!/usr/bin/env python3
"""Build RA-only SMS/GG calculation ROMs and verify reset, mapper, RAM and VDP output."""

import argparse
from binascii import crc_hqx
import json
import os
from pathlib import Path
import re
import shutil
import tempfile

from compare_z80_compilers import ROOT, run, tool
from check_register_allocator import write_json
from ra_cpu import Z80Machine, step
from z80_algorithm_oracles import algorithm_inputs
from z80_application_oracles import APPLICATION_CASES, application_inputs


GG_RESULTS = {
    1: (0x0115, 0x369c, 0x4001, 0x5a5a, 0x1c9e, 0x6db6, 0x7f01, 0x9234, 0xcc7d),
    2: (0x1fe6, 0x0b42, 0x13ed, 0x1334, 0x13c5, 0x146f, 0x12ea,
        0x0db1, 0x14c5, 0x1dba, 0x027d, 0x09ad),
}
NEW_LABELS = ("SPRITE SCENE", "TRACKER MIXER", "WAREHOUSE ROUTES",
              "A* MAZE 1 64X64", "A* MAZE 2 64X64", "QUICKSORT 64 BYTES", "QUICKSORT 512 WORDS")


def new_results():
    cases = [application_inputs(name, 5) for name in APPLICATION_CASES]
    cases.extend(algorithm_inputs(name, case) for name in
                 ("calculationAStarWorkload", "calculationQuickSortWorkload") for case in (0, 1))
    lengths = (512, 512, 512, 528, 528, 64, 1024)
    return [(value, crc_hqx(output[:length], 65535))
            for (_, _, output, value), length in zip(cases, lengths, strict=True)]


def build(project, platform, output, compiler, linker):
    source = ROOT / "tests" / "z80" / platform / f"calculations-{project}"
    directory = output / platform / f"calculations-{project}"
    directory.mkdir(parents=True)
    names = ["interrupts", "main", "bank_1", "bank_2"] + (["bank_3"] if project != 3 else [])
    for path in source.iterdir():
        if path.suffix == ".ssc" or path.name == "tiles.packbits":
            shutil.copyfile(path, directory / path.name)
    commands = []
    for name in names:
        bank = ["-b", name[-1], "-s", "2"] if name.startswith("bank_") else []
        run([compiler, "-ra", "-q", "-mz80", "-rs", "3", *bank,
             "-o", f"{name}.asm", f"{name}.ssc"], directory, f"{name}.compiler.log", commands)
    rom_name = "linked.gg" if platform == "gg" else "linked.sms"
    run([linker, "-lsms", "-ro", "4", "-v", "-o", rom_name,
         *(f"{name}.asm" for name in names)], directory, "linker.log", commands)
    write_json(directory / "build.json", {"commands": commands})
    text = (directory / "linked.sym").read_text()
    symbols = {match[2]: int(match[1], 16) for match in re.finditer(
        r"^[\da-f]+:([\da-f]+) (\S+)$", text, re.M | re.I)}
    return directory, (directory / rom_name).read_bytes(), symbols


def execute_rom(rom, symbols, project, platform):
    if len(rom) != 65536 or rom[0x7ff0:0x7ff8] != b"TMR SEGA":
        raise RuntimeError("Invalid 64KB cartridge image/header")
    if platform == "gg" and rom[0x7fff] != 0x6e:
        raise RuntimeError("Missing export Game Gear cartridge region")
    expected_header = 0x6e if platform == "gg" else (0x4e if project == 3 else 0x4c)
    if rom[0x7fff] != expected_header:
        raise RuntimeError("Incorrect cartridge region/size")
    checksum_end = 32768 if rom[0x7fff] & 15 == 12 else 65536
    checksum = (sum(rom[:0x7ff0]) + sum(rom[0x8000:checksum_end])) & 65535
    if int.from_bytes(rom[0x7ffa:0x7ffc], "little") != checksum:
        raise RuntimeError("Incorrect cartridge checksum")
    machine = Z80Machine()
    memory = machine.memory
    memory[:0xc000] = rom[:0xc000]
    ram_end = symbols["RAM_USAGE_SLOT_RAMSlot1_BANK_0_END"]
    if not 0xc000 <= ram_end < 0xd000:
        raise RuntimeError(f"Unexpected global RAM extent: {ram_end:04x}")
    vram, cram = bytearray(16384), bytearray(64)
    registers = bytearray(16)
    latch = None
    vdp_address = command = 0
    lowest_stack = 0xdff0
    palette_writes = []
    mapper_writes = []
    instructions = cycles = 0
    stack_started = False

    def read_memory(address):
        nonlocal lowest_stack
        physical = 0xc000 + (address & 0x1fff)
        if ram_end < physical < 0xdff0:
            lowest_stack = min(lowest_stack, physical)
        return memory[physical]

    def write_memory(address, value):
        nonlocal lowest_stack
        if address < 0xc000:
            raise RuntimeError(f"ROM write {address:04x}, PC={machine.pc:04x}")
        physical = 0xc000 + (address & 0x1fff)
        if ram_end < physical < 0xdff0:
            lowest_stack = min(lowest_stack, physical)
        memory[physical] = value
        if address >= 0xfffc:
            mapper_writes.append((address, value))
            if address == 0xfffc:
                if value != 0:
                    raise RuntimeError("Unexpected cartridge RAM mapping")
            else:
                if value >= len(rom) // 0x4000:
                    raise RuntimeError(f"Invalid ROM bank: {value}")
                slot = address - 0xfffd
                offset = 0x400 if slot == 0 else 0
                start = slot * 0x4000 + offset
                memory[start:(slot + 1) * 0x4000] = rom[value * 0x4000 + offset:(value + 1) * 0x4000]

    def write_port(port, value):
        nonlocal latch, vdp_address, command
        port &= 255
        if port == 0xbf:
            if latch is None:
                latch = value
            else:
                command = value >> 6
                if command == 2:
                    registers[value & 15] = latch
                else:
                    vdp_address = ((value & 63) << 8) | latch
                latch = None
        elif port == 0xbe:
            if command == 3:
                cram[vdp_address % (64 if platform == "gg" else 32)] = value
                palette_writes.append(value)
            elif command == 1:
                vram[vdp_address & 16383] = value
            else:
                raise RuntimeError(f"Unexpected VDP data command: {command}")
            vdp_address = (vdp_address + 1) & 16383
        else:
            raise RuntimeError(f"Unexpected console port write: {port:02x}")

    def read_port(port):
        raise RuntimeError(f"Unexpected console port read: {port:04x}")

    machine.set_read_callback(read_memory)
    machine.set_write_callback(write_memory)
    machine.set_output_callback(write_port)
    machine.set_input_callback(read_port)
    machine.mark_addrs(0xc000, 0x4000, machine.READ_MARK)
    machine.mark_addrs(0, 65536, machine.WRITE_MARK)
    while instructions < 20000000:
        before = machine.pc
        opcode = memory[before]
        cycles += step(machine)
        instructions += 1
        if 0xc000 <= machine.sp <= 0xdff0:
            stack_started = True
            lowest_stack = min(lowest_stack, machine.sp)
        elif stack_started:
            raise RuntimeError(f"Stack escaped physical RAM: {machine.sp:04x}")
        if machine.pc == before and opcode in (0xc3, 0x18):
            break
    else:
        raise RuntimeError("Console instruction budget exceeded")
    if not stack_started:
        raise RuntimeError("Console stack was not initialized")
    if not registers[1] & 0x40 or lowest_stack <= ram_end:
        raise RuntimeError("Display disabled or stack/global RAM collision")
    if (registers[0] & 0x26 != 4 or registers[1] & 0x18
            or (registers[2] << 10) & 0x3800 != 0x3800
            or registers[8] or registers[9] or latch is not None):
        raise RuntimeError("Incorrect mode-4 display geometry")
    expected_palette = [0, 0, 255, 15] if platform == "gg" else [0, 63]
    if palette_writes != expected_palette:
        raise RuntimeError(f"Incorrect {platform} palette: {palette_writes}")
    font_crc = crc_hqx(vram[:0x3800], 65535)
    if font_crc != 0x6986:
        raise RuntimeError(f"Legacy font/blank VRAM changed: {font_crc:04x}")
    rows = []
    for y in range(24):
        cells = [vram[0x3800 + y * 64 + x * 2] for x in range(32)]
        attributes = vram[0x3801 + y * 64:0x3840 + y * 64:2]
        if any(attributes):
            raise RuntimeError("Unexpected name-table tile attributes")
        if platform == "gg":
            if any(cell for x, cell in enumerate(cells) if x < 6 or x >= 26 or y < 3 or y >= 21):
                raise RuntimeError(f"Text outside the 20x18 Game Gear viewport: row {y}")
            if y < 3 or y >= 21:
                continue
            cells = cells[6:26]
        rows.append("".join(chr(cell + 32) for cell in cells).rstrip())
    numeric = [row for row in rows if row.startswith("$")]
    heading = [f"CALCULATIONS {project}", "EXPECTED = ACTUAL"]
    if project == 3:
        expected_rows = heading.copy()
        for label, (primary, full) in zip(NEW_LABELS, new_results(), strict=True):
            expected_rows.extend((label, f"${primary:04X} = ${primary:04X}"))
            if platform == "sms":
                expected_rows.append(f"${full:04X} = ${full:04X}")
        expected_rows.extend([""] * (len(rows) - len(expected_rows)))
        if rows != expected_rows:
            raise RuntimeError(f"Displayed results differ: expected {expected_rows}, got {rows}")
    elif platform == "gg":
        expected_rows = [f"${value:04X} = ${value:04X}" for value in GG_RESULTS[project]]
        if rows[:2] != heading or numeric != expected_rows:
            raise RuntimeError(f"Displayed results differ: expected {expected_rows}, got {numeric}")
    else:
        if (rows[0] != "Hello World!" or len(numeric) != 23
                or any(not re.fullmatch(r"\$([0-9A-F]{4})(?: = \$\1){3}", row) for row in numeric)):
            raise RuntimeError(f"Legacy SMS calculation rows changed: {numeric}")
    return {"instructions": instructions, "cycles": cycles, "rows": rows,
            "numeric_rows": len(numeric), "global_ram_bytes": ram_end + 1 - 0xc000,
            "stack_access_bytes": 0xdff0 - lowest_stack,
            "ram_used_bytes": ram_end + 1 - 0xc000 + 0xe000 - lowest_stack,
            "mapper_writes": mapper_writes, "palette": list(cram[:len(expected_palette)]),
            "font_crc16": font_crc}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--platforms", nargs="+", choices=("sms", "gg"), default=["sms", "gg"])
    parser.add_argument("--projects", nargs="+", type=int, choices=(1, 2, 3), default=[1, 2, 3])
    args = parser.parse_args()
    output = args.output_dir.resolve() if args.output_dir else Path(tempfile.mkdtemp(prefix="ssc-calculations-"))
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("Output directory must be empty")
    if len(args.platforms) != len(set(args.platforms)) or len(args.projects) != len(set(args.projects)):
        parser.error("Duplicate platform/project selections")
    report = {"status": "running", "mode": "ssc-ra", "results": []}
    try:
        compiler = tool(os.environ.get("SAMESAMECC", "samesamecc"))
        linker = tool(os.environ.get("SAMESAMECL", "samesamecl"))
        for platform in args.platforms:
            for project in args.projects:
                directory, rom, symbols = build(project, platform, output, compiler, linker)
                result = {"platform": platform, "project": project,
                          **execute_rom(rom, symbols, project, platform)}
                write_json(directory / "execution.json", result)
                report["results"].append(result)
                print(json.dumps(result), flush=True)
        report["status"] = "passed"
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        write_json(output / "report.json", report)


if __name__ == "__main__":
    main()
