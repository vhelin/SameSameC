"""Independent heap-based A*, BFS and built-in-sort checks for the large kernels."""

from binascii import crc_hqx
from collections import deque
import heapq
from pathlib import Path
import re


ALGORITHM_CASES = {"calculationAStarWorkload": 2, "calculationQuickSortWorkload": 8}
BUFFER_SIZES = {"calculationAStarWorkload": 528, "calculationQuickSortWorkload": 1024}
MAZE_SOURCE = Path(__file__).with_name("z80_maze_maps.ssc")


def maze_cells(map_number):
    if map_number not in (0, 1):
        raise ValueError(f"Unknown maze: {map_number}")
    text = MAZE_SOURCE.read_text().split("{", 1)[1].split("}", 1)[0]
    packed = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", text))
    if len(packed) != 1024:
        raise ValueError("Expected two packed 64x64 mazes")
    return frozenset(cell for cell in range(4096)
                     if not packed[map_number * 512 + cell // 8] & (1 << (cell % 8)))


def neighbours(cell):
    x, y = cell % 64, cell // 64
    return tuple(ny * 64 + nx for nx, ny in
                 ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1))
                 if 0 <= nx < 64 and 0 <= ny < 64)


def shortest_distance(cells):
    pending = deque([0])
    distances = {0: 0}
    while pending:
        cell = pending.popleft()
        if cell == 4095:
            return distances[cell]
        for other in neighbours(cell):
            if other in cells and other not in distances:
                distances[other] = distances[cell] + 1
                pending.append(other)
    raise ValueError("Maze has no corner-to-corner route")


def astar(map_number):
    cells = maze_cells(map_number)
    scores = {0: 0}
    parents = {}
    closed = set()
    frontier = {0}
    heap = [(126, 0, 0)]
    peak = 1
    while heap:
        _, negative_g, cell = heapq.heappop(heap)
        cost = -negative_g
        if cell in closed or cost != scores[cell]:
            continue
        frontier.remove(cell)
        closed.add(cell)
        if cell == 4095:
            break
        for other in neighbours(cell):
            if other not in cells or other in closed or cost + 1 >= scores.get(other, 65535):
                continue
            scores[other] = cost + 1
            parents[other] = cell
            frontier.add(other)
            heapq.heappush(heap, (cost + 1 + 126 - other % 64 - other // 64, -cost - 1, other))
        peak = max(peak, len(frontier))
    if 4095 not in closed:
        raise ValueError("A* failed to reach the goal")
    path = [4095]
    while path[-1]:
        path.append(parents[path[-1]])
    path.reverse()
    if len(path) != len(set(path)) or len(path) - 1 != shortest_distance(cells):
        raise ValueError("A* route is not a simple shortest path")
    if any(other not in neighbours(cell) for cell, other in zip(path, path[1:])):
        raise ValueError("A* route has a non-adjacent step")
    if peak > 512:
        raise ValueError("Predefined maze exceeds the console frontier capacity")
    output = bytearray(528)
    for cell in path:
        output[cell // 8] |= 1 << (cell % 8)
    total = sum(cell % 64 + cell // 64 for cell in path) & 65535
    for offset, value in ((512, len(path) - 1), (514, total),
                          (516, len(closed)), (518, peak)):
        output[offset:offset + 2] = value.to_bytes(2, "little")
    output[521] = map_number
    return output, crc_hqx(output[514:516], 65535), path


def random_table(seed, mode):
    if mode not in (0, 1):
        raise ValueError(f"Unknown table mode: {mode}")
    values = []
    for _ in range(64 if mode == 0 else 512):
        seed ^= (seed << 7) & 65535
        seed ^= seed >> 9
        seed ^= (seed << 8) & 65535
        values.append(seed >> 8 if mode == 0 else seed)
    return values


def algorithm_inputs(name, case):
    if name not in ALGORITHM_CASES or not 0 <= case < ALGORITHM_CASES[name]:
        raise ValueError(f"Unknown algorithm case: {name}/{case}")
    initial = bytearray((index * 37 + case * 53) & 255 for index in range(BUFFER_SIZES[name]))
    if name == "calculationAStarWorkload":
        expected, result, _ = astar(case)
        return [case, 0xc100], initial, expected, result
    seed = (0x1234, 0xbeef, 0, 0, 1, 65535, 32768, 32767)[case]
    mode = case % 2
    width = mode + 1
    payload = b"".join(value.to_bytes(width, "little") for value in sorted(random_table(seed, mode)))
    expected = initial.copy()
    expected[:len(payload)] = payload
    return [seed, mode, 0xc100], initial, expected, crc_hqx(payload, 65535)
