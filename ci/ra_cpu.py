from importlib.metadata import version

from z80 import HL, Z80Machine


if version("z80") != "1.2.0":
    raise RuntimeError("Install the pinned native emulator with python -m pip install -r ci/requirements.txt")

TRANSFER_CAUSES = [
    "allocator_preserve", "allocator_reload", "abi_argument",
    "abi_result", "abi_frame", "other_frame",
]
EXECUTION_METRICS = [
    "instructions", "cycles", "frame_reads", "frame_writes", "register_moves",
    "unmapped_instructions", "entry_backedges", "callee_backedges",
    *[f"{cause}_{access}" for cause in TRANSFER_CAUSES for access in ("reads", "writes")],
]


def step(machine, pc_limit=0xc000):
    cycles = 0
    while True:
        if machine.pc >= pc_limit:
            raise RuntimeError("Instruction bound/PC escape")
        before = machine.frame_tick
        machine.ticks_to_stop = 1
        machine.run()
        # The pinned core wraps every 100,000 T-states and steps DD/FD prefixes separately.
        cycles += (machine.frame_tick - before) % 100000
        if machine.index_rp_kind == HL:
            return cycles


def execute(code, entry, parameters, widths, buffer, buffer_address, return_width,
            transfers=None, ports_allowed=False, instruction_limit=20000, frame_bytes=256,
            entry_sp_offset=-3):
    if transfers is None:
        transfers = {}
    machine = Z80Machine()
    machine.af = 0
    memory = machine.memory
    memory[:] = b"\xa5" * 65536
    memory[:min(len(code), 0xc000)] = code[:0xc000]
    memory[buffer_address:buffer_address + len(buffer)] = buffer
    frame = 0xd000
    sentinel = 0x7fff
    memory[frame - 1] = sentinel & 255
    memory[frame] = sentinel >> 8
    result_offset = frame - 3 - return_width
    offset = result_offset
    for value, width in zip(parameters, widths, strict=True):
        offset -= width
        memory[offset] = value & 255
        if width == 2:
            memory[offset + 1] = (value >> 8) & 255

    frame_reads = 0
    frame_writes = 0
    instruction_metadata = None
    port_writes = []
    traffic = {f"{cause}_{access}": 0 for cause in TRANSFER_CAUSES for access in ("reads", "writes")}

    def read_memory(address):
        nonlocal frame_reads
        frame_reads += 1
        metadata = instruction_metadata if instruction_metadata is not None else {}
        cause = metadata.get("cause") or "other_frame"
        if cause == "other_frame" and any(
            ((machine.de + displacement) & 65535) <= address <
            ((machine.de + displacement) & 65535) + size
            for displacement, size in metadata.get("argumentsInFrame", [])
        ):
            cause = "abi_argument"
        traffic[f"{cause}_reads"] += 1
        return memory[address]

    def write_memory(address, value):
        nonlocal frame_writes
        in_frame = frame - frame_bytes <= address < frame - 1
        if not in_frame and not buffer_address <= address < buffer_address + len(buffer):
            raise RuntimeError(f"Out-of-bounds write {address:x}")
        if in_frame:
            frame_writes += 1
            metadata = instruction_metadata if instruction_metadata is not None else {}
            traffic[f"{metadata.get('cause') or 'other_frame'}_writes"] += 1
        memory[address] = value & 255

    def write_port(address, value):
        if not ports_allowed:
            raise RuntimeError("Unexpected port write")
        if address & 255 not in (0xbe, 0xbf):
            raise RuntimeError("Unsupported captured port")
        port_writes.append([address & 255, value & 255])

    machine.set_read_callback(read_memory)
    machine.set_write_callback(write_memory)
    machine.set_input_callback(lambda address: 0)
    machine.set_output_callback(write_port)
    machine.mark_addrs(frame - frame_bytes, frame_bytes - 1, machine.READ_MARK)
    machine.mark_addrs(0, 65536, machine.WRITE_MARK)
    machine.pc = entry
    machine.de = frame
    # Normal callees enter below saved DE; raw RET probes use offset -1.
    machine.sp = frame + entry_sp_offset
    instructions = cycles = register_moves = unmapped_instructions = 0
    entry_backedges = callee_backedges = 0
    entry_section = (transfers.get(entry) or {}).get("section")
    while machine.pc != sentinel:
        instructions += 1
        if instructions > instruction_limit or machine.pc >= 0xc000:
            raise RuntimeError("Instruction bound/PC escape")
        instruction_metadata = transfers.get(machine.pc)
        if instruction_metadata is None:
            unmapped_instructions += 1
        opcode = memory[machine.pc]
        if opcode & 0xc0 == 0x40 and opcode & 7 != 6 and (opcode >> 3) & 7 != 6:
            register_moves += 1
        previous_pc = machine.pc
        cycles += step(machine)
        if instruction_metadata is not None and instruction_metadata.get("branch") and machine.pc <= previous_pc:
            section = instruction_metadata.get("section")
            if (transfers.get(machine.pc) or {}).get("section") == section:
                if section == entry_section:
                    entry_backedges += 1
                else:
                    callee_backedges += 1

    if machine.de != frame or machine.sp != frame + 1:
        raise RuntimeError("Frame/stack restoration failure")
    if sum(traffic[f"{cause}_reads"] for cause in TRANSFER_CAUSES) != frame_reads or \
            sum(traffic[f"{cause}_writes"] for cause in TRANSFER_CAUSES) != frame_writes:
        raise RuntimeError("Frame traffic attribution is not a partition")
    return {
        "value": memory[result_offset] + (memory[result_offset + 1] * 256 if return_width == 2 else 0),
        "buffer": bytearray(memory[buffer_address:buffer_address + len(buffer)]),
        "instructions": instructions, "cycles": cycles,
        "frame_reads": frame_reads, "frame_writes": frame_writes,
        "register_moves": register_moves, "unmapped_instructions": unmapped_instructions,
        "entry_backedges": entry_backedges, "callee_backedges": callee_backedges,
        "port_writes": port_writes, **traffic,
    }
