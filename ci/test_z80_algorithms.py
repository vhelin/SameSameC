from binascii import crc_hqx
from collections import Counter
import copy
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import compare_z80_compilers as benchmark
from ra_cpu import Z80Machine, step
from z80_algorithm_oracles import (
    ALGORITHM_CASES, BUFFER_SIZES, algorithm_inputs, astar, maze_cells, neighbours,
    random_table, shortest_distance,
)


class AlgorithmOracleTests(unittest.TestCase):
    def test_maze_geometry_reachability_and_shortest_routes(self):
        for map_number, (open_count, distance, total, crc, peak) in enumerate(
                ((2049, 642, 41165, 0xc8ed, 6), (2113, 198, 12193, 0xe6cd, 31))):
            with self.subTest(map_number=map_number):
                cells = maze_cells(map_number)
                output, result, path = astar(map_number)
                self.assertEqual(len(cells), open_count)
                self.assertEqual((path[0], path[-1]), (0, 4095))
                self.assertEqual(len(path), len(set(path)))
                self.assertTrue(set(path) <= cells)
                self.assertTrue(all(b in neighbours(a) for a, b in zip(path, path[1:])))
                self.assertEqual(shortest_distance(cells), distance)
                self.assertEqual(int.from_bytes(output[512:514], "little"), distance)
                self.assertEqual(int.from_bytes(output[514:516], "little"), total)
                self.assertEqual(int.from_bytes(output[518:520], "little"), peak)
                self.assertLess(peak, 512)
                self.assertEqual(sum(p % 64 + p // 64 for p in path) & 65535, total)
                self.assertEqual(result, crc_hqx(total.to_bytes(2, "little"), 65535))
                self.assertEqual(result, crc)
                marked = {p for p in range(4096) if output[p // 8] & (1 << (p % 8))}
                self.assertEqual(marked, set(path))
                self.assertEqual(output[520:], bytes([0, map_number, 0, 0, 0, 0, 0, 0]))
                edges = sum(len(set(neighbours(p)) & cells) for p in cells) // 2
                self.assertEqual(edges - len(cells) + 1, 0 if map_number == 0 else 64)

    def test_sort_sizes_unsigned_order_multisets_crc_and_untouched_tail(self):
        for case in range(8):
            with self.subTest(case=case):
                params, initial, output, crc = algorithm_inputs("calculationQuickSortWorkload", case)
                seed, mode = params[:2]
                width, count = mode + 1, 512 if mode else 64
                source = random_table(seed, mode)
                payload = output[:width * count]
                values = [int.from_bytes(payload[i:i + width], "little") for i in range(0, len(payload), width)]
                self.assertEqual(Counter(source), Counter(values))
                self.assertEqual(values, sorted(source))
                self.assertEqual(output[len(payload):], initial[len(payload):])
                self.assertEqual(crc, crc_hqx(payload, 65535))
                if seed:
                    self.assertTrue(any(value >= (32768 if mode else 128) for value in values))
                else:
                    self.assertEqual(values, [0] * count)
                self.assertEqual(algorithm_inputs("calculationQuickSortWorkload", case),
                                 (params, initial, output, crc))
        self.assertEqual(algorithm_inputs("calculationQuickSortWorkload", 0)[3], 0xf478)
        self.assertEqual(algorithm_inputs("calculationQuickSortWorkload", 1)[3], 0x32d9)
        self.assertEqual(crc_hqx(b"123456789", 65535), 0x29b1)

    def test_invalid_model_inputs_fail_explicitly(self):
        for name, case in (("unknown", 0), ("calculationAStarWorkload", -1),
                           ("calculationAStarWorkload", 2), ("calculationQuickSortWorkload", 8)):
            with self.assertRaisesRegex(ValueError, "Unknown algorithm case"):
                algorithm_inputs(name, case)
        with self.assertRaisesRegex(ValueError, "Unknown maze"):
            maze_cells(2)
        with self.assertRaisesRegex(ValueError, "Unknown table"):
            random_table(1, 2)
        with self.assertRaisesRegex(ValueError, "no corner-to-corner"):
            shortest_distance({0, 4095})

    def test_packed_map_shape_is_not_silently_truncated(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.ssc"
            path.write_text("const u8 astarMaps[1024] = { 0x00 };")
            with patch("z80_algorithm_oracles.MAZE_SOURCE", path):
                with self.assertRaisesRegex(ValueError, "two packed"):
                    maze_cells(0)


class AlgorithmComparisonTests(unittest.TestCase):
    def test_algorithm_extension_rejects_missing_duplicate_and_changed_history(self):
        original = json.loads(benchmark.ALGORITHM_BASELINE.read_text())
        for mutation in ("schema", "parent", "parent_name", "missing", "duplicate", "corpus", "modes"):
            changed = copy.deepcopy(original)
            if mutation == "schema":
                changed["schema"] = 99
            elif mutation == "parent":
                changed["parent_sha256"] = "0" * 64
            elif mutation == "parent_name":
                changed["parent_baseline"] = "wrong.json"
            elif mutation == "missing":
                changed["results"].pop()
            elif mutation == "duplicate":
                changed["results"].append(changed["results"][0])
            elif mutation == "corpus":
                changed["corpus"].pop(next(iter(changed["corpus"])))
            else:
                changed["modes"].append("ssc-no-ra")
            with self.subTest(mutation=mutation), patch.object(benchmark, "ALGORITHM_BASELINE") as path:
                path.read_text.return_value = json.dumps(changed)
                with self.assertRaisesRegex(RuntimeError, "Invalid versioned algorithm baseline"):
                    benchmark.baseline()

    def test_extension_parent_uses_normalized_line_endings(self):
        reference = benchmark.baseline()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / benchmark.APPLICATION_BASELINE.name
            text = benchmark.APPLICATION_BASELINE.read_text(encoding="utf-8")
            for ending in ("\n", "\r\n"):
                path.write_bytes(text.replace("\n", ending).encode())
                with patch.object(benchmark, "APPLICATION_BASELINE", path):
                    self.assertEqual(benchmark.baseline(), reference)

    def test_map_and_crc_drift_only_change_the_new_corpus(self):
        functions = benchmark.sources()
        packed = (benchmark.ROOT / "tests" / "z80" / "sms" / "calculations-1" / "tiles.packbits").read_bytes()
        original = benchmark.corpus(functions, packed)
        for symbol, affected in (("astarMaps", {"calculationAStarWorkload"}),
                                 ("calculationCrc16", set(ALGORITHM_CASES))):
            changed = dict(functions)
            changed[symbol] += "\n"
            measured = benchmark.corpus(changed, packed)
            self.assertEqual({name for name in measured if measured[name] != original[name]}, affected)

    def test_large_buffers_are_guarded_at_their_exact_boundaries(self):
        for name, size in BUFFER_SIZES.items():
            for offset in (size - 1, size):
                memory = bytearray(65536)
                memory[0x100:0x107] = bytes([0xcd, 0, 2, 0x22, 8, 0xc6, 0xc9])
                destination = 0xc000 + offset
                memory[0x200:0x209] = bytes([0x3e, 42, 0x32, destination & 255,
                                           destination >> 8, 0x21, 42, 0, 0xc9])
                symbols = {"bench": 0x100, name: 0x200, "bench_buffer": 0xc000,
                           "bench_params": 0xc600, "bench_result": 0xc608}
                with self.subTest(name=name, offset=offset):
                    if offset == size:
                        with self.assertRaisesRegex(RuntimeError, "Out-of-bounds write"):
                            benchmark.execute(memory, symbols, name, [], b"", 0)
                    else:
                        value, output, _ = benchmark.execute(memory, symbols, name, [], b"", 0)
                        self.assertEqual((value, len(output), output[-1]), (42, size, 42))
                with self.assertRaisesRegex(RuntimeError, "Invalid benchmark buffer"):
                    benchmark.execute(memory, symbols, name, [], bytes(size + 1), 0)

    def test_adc_h_hardware_clock_and_explicit_independent_calibration(self):
        machine = Z80Machine()
        machine.memory[0] = 0x8c
        self.assertEqual(step(machine), 4)
        for reported in (28, 31, 32):
            with self.subTest(reported=reported), tempfile.TemporaryDirectory() as directory:
                completed = subprocess.CompletedProcess([], 0, str(reported).encode())
                with patch.object(benchmark.subprocess, "run", return_value=completed), \
                        patch("sys.stdout", new=io.StringIO()):
                    if reported == 32:
                        with self.assertRaisesRegex(RuntimeError, "clock calibration failed"):
                            benchmark.calibrate_independent_clock("ticks", Path(directory))
                    else:
                        calibration = benchmark.calibrate_independent_clock("ticks", Path(directory))
                        self.assertEqual(calibration["adc_h_extra_cycles"], reported - 28)
                        self.assertEqual(json.loads((Path(directory) / "independent-clock-calibration.json").read_text()),
                                         calibration)

    def test_calibration_does_not_waive_unrelated_cycle_mismatches(self):
        memory = bytearray(65536)
        memory[0x100:0x107] = bytes([0xcd, 0, 2, 0x22, 0, 0xc3, 0xc9])
        memory[0x200:0x205] = bytes([0x8c, 0x21, 42, 0, 0xc9])
        symbols = {"bench": 0x100, "probe": 0x200, "bench_buffer": 0xc000,
                   "bench_params": 0xc240, "bench_result": 0xc300}
        _, _, native = benchmark.execute(memory, symbols, "probe", [], b"", 0)
        for extra, correction, passes in ((0, 0, True), (3, 3, True), (3, 0, False), (4, 3, False)):
            with self.subTest(extra=extra, correction=correction), tempfile.TemporaryDirectory() as directory:
                completed = subprocess.CompletedProcess([], 0, str(native["cycles"] + extra).encode())
                with patch.object(benchmark.subprocess, "run", return_value=completed):
                    arguments = (memory, symbols, "probe", [], b"", 0, "ticks", Path(directory), correction)
                    if passes:
                        self.assertEqual(benchmark.execute(*arguments)[0], 42)
                    else:
                        with self.assertRaisesRegex(RuntimeError, "Independent emulator cycle mismatch"):
                            benchmark.execute(*arguments)


if __name__ == "__main__":
    unittest.main()
