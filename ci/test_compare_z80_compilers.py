import copy
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import check_register_allocator as allocator
import compare_z80_compilers as benchmark
from ra_cpu import Z80Machine, execute, step
from z80_application_oracles import APPLICATION_CASES, application_inputs, navigation, sprite_scene, tracker


class ComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reference = benchmark.baseline()

    def test_random_static_names_do_not_relax_code_identity(self):
        for mutation in (None, "assembly", "rom"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                def fake_run(command, arguments, work, log):
                    if command == "compiler":
                        quiet = work.name == "ra_quiet"
                        suffix = 2 if quiet else 1
                        instruction = "RET" if quiet and mutation == "assembly" else "NOP"
                        (work / "main.asm").write_text(f"leaf__static__main_ssc_{suffix}:\n {instruction}\n")

                def fake_image(work):
                    rom = b"\x01" if mutation == "rom" and work.name == "ra_quiet" else b"\x00"
                    return allocator.Image(rom, {"ra_buffer": 0}, {}, {}, {})

                with patch.object(allocator, "run", fake_run), patch.object(allocator, "read_image", fake_image):
                    if mutation:
                        with self.assertRaisesRegex(RuntimeError, f"Traced/quiet allocator {'assembly' if mutation == 'assembly' else 'ROM'} mismatch"):
                            allocator.compile_generated(Path(directory), "compiler", "linker", [], "")
                    else:
                        images = allocator.compile_generated(Path(directory), "compiler", "linker", [], "")
                        self.assertEqual(len(images), 5)

    def test_reference_is_complete_and_current(self):
        packed = (benchmark.ROOT / "tests" / "z80" / "sms" / "calculations-1" / "tiles.packbits").read_bytes()
        self.assertEqual(benchmark.corpus(benchmark.sources(), packed), self.reference["corpus"])
        original = json.loads(benchmark.BASELINE.read_text())
        self.assertEqual(self.reference["results"][:104], original["results"])
        self.assertEqual(len(original["results"]), 104)
        self.assertEqual(sum(row["cases"] for row in original["results"]), 1856)
        rows = [row for row in self.reference["results"] if row["mode"] in benchmark.DEFAULT_MODES]
        self.assertEqual(len(rows), 126)
        self.assertEqual(len({(row["routine"], row["mode"]) for row in rows}), 126)
        self.assertEqual(sum(row["cases"] for row in rows), 1946)
        self.assertEqual(self.reference["flags"], benchmark.FLAGS)

    def test_application_baseline_rejects_incomplete_or_rewritten_history(self):
        original = json.loads(benchmark.APPLICATION_BASELINE.read_text())
        for mutation in ("schema", "parent", "missing", "duplicate", "corpus", "modes"):
            with self.subTest(mutation=mutation):
                changed = copy.deepcopy(original)
                if mutation == "schema":
                    changed["schema"] = 99
                elif mutation == "parent":
                    changed["parent_sha256"] = "0" * 64
                elif mutation == "missing":
                    changed["results"].pop()
                elif mutation == "duplicate":
                    changed["results"].append(changed["results"][0])
                elif mutation == "corpus":
                    changed["corpus"].pop(next(iter(changed["corpus"])))
                else:
                    changed["modes"].append("ssc-no-ra")
                with patch.object(benchmark, "APPLICATION_BASELINE") as path:
                    path.read_text.return_value = json.dumps(changed)
                    with self.assertRaisesRegex(RuntimeError, "Invalid versioned application baseline"):
                        benchmark.baseline()

    def test_parent_fingerprint_is_independent_of_checkout_line_endings(self):
        original = benchmark.BASELINE.read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / benchmark.BASELINE.name
            for ending in ("\n", "\r\n"):
                with self.subTest(ending=repr(ending)):
                    path.write_bytes(original.replace("\n", ending).encode("utf-8"))
                    with patch.object(benchmark, "BASELINE", path):
                        self.assertEqual(benchmark.baseline(), self.reference)

    def test_comparison_modes_require_ra(self):
        self.assertEqual(len(benchmark.DEFAULT_MODES), 7)
        for mode, (kind, flags) in benchmark.MODES.items():
            if kind == "ssc":
                self.assertIn("-ra", flags, mode)
                self.assertNotIn("-no-ra", flags, mode)
        for mode in ("ssc-no-ra", "ssc-no-ra-shared"):
            with self.subTest(mode=mode), patch("sys.argv", ["compare", "--modes", mode]), \
                    patch("sys.stderr", new=io.StringIO()), patch.object(benchmark, "tool") as tool:
                with self.assertRaises(SystemExit) as error:
                    benchmark.main()
                self.assertEqual(error.exception.code, 2)
                tool.assert_not_called()

    def test_candidate_improvement_is_not_a_regression(self):
        reference = next(row for row in self.reference["results"] if row["mode"] == "ssc-ra")
        result = dict(reference, code_bytes=reference["code_bytes"] - 1)
        self.assertEqual(benchmark.compare_result(result, reference), [])

    def test_candidate_regressions_cannot_be_waived(self):
        reference = next(row for row in self.reference["results"] if row["mode"] == "ssc-ra")
        for metric in benchmark.METRICS:
            with self.subTest(metric=metric):
                result = dict(reference)
                result[metric] += 1
                issues = benchmark.compare_result(result, reference, allow_competitor_drift=True)
                self.assertEqual(len(issues), 1)
                self.assertTrue(issues[0]["fatal"])

    def test_shared_arithmetic_policy_is_separate_and_bounded(self):
        self.assertEqual(set(self.reference["modes"]), set(benchmark.DEFAULT_MODES))
        reference = dict(routine="probe", mode="ssc-ra", code_bytes=100, wrapper_bytes=126, total_kernel_cycles=270,
                         total_wrapper_cycles=729, max_stack_bytes=20, max_stack_access_bytes=20,
                         max_below_sp_bytes=8, max_frame_footprint_bytes=22)
        result = dict(reference, mode="ssc-ra-shared", code_bytes=90, total_kernel_cycles=297,
                      max_stack_bytes=22, max_stack_access_bytes=22, max_frame_footprint_bytes=24)
        self.assertEqual(benchmark.compare_result(result, reference), [])
        for metric, invalid in (("code_bytes", 101), ("total_kernel_cycles", 298),
                                ("total_kernel_cycles", 269), ("wrapper_bytes", 125),
                                ("total_wrapper_cycles", 730), ("max_stack_bytes", 23),
                                ("max_stack_access_bytes", 23), ("max_frame_footprint_bytes", 25)):
            with self.subTest(metric=metric, invalid=invalid):
                issues = benchmark.compare_result(dict(result, **{metric: invalid}), reference, True)
                self.assertEqual(len(issues), 1)
                self.assertTrue(issues[0]["fatal"])

    def test_shared_arithmetic_requires_ordinary_paired_mode(self):
        with patch("sys.argv", ["compare", "--modes", "ssc-ra-shared"]), patch("sys.stderr", new=io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                benchmark.main()
            self.assertEqual(error.exception.code, 2)

    def test_register_call_policy_counts_adapters_and_callers(self):
        reference = dict(routine="probe", mode="ssc-ra", code_bytes=100, wrapper_bytes=100,
                         total_kernel_cycles=400, total_wrapper_cycles=400, max_stack_bytes=20,
                         max_stack_access_bytes=20, max_below_sp_bytes=8, max_frame_footprint_bytes=22)
        for mode in ("ssc-ra-regcalls", "ssc-ra"):
            result = dict(reference, mode=mode, register_calls=True, code_bytes=120, wrapper_bytes=70,
                          total_kernel_cycles=410, total_wrapper_cycles=300)
            self.assertEqual(benchmark.compare_result(result, reference), [])
            for metric, value in (("code_bytes", 131), ("total_kernel_cycles", 501),
                                  ("max_stack_bytes", 21), ("max_stack_access_bytes", 21),
                                  ("max_below_sp_bytes", 9), ("max_frame_footprint_bytes", 23)):
                issues = benchmark.compare_result(dict(result, **{metric: value}), reference, True)
                self.assertEqual(len(issues), 1, (mode, metric))
                self.assertTrue(issues[0]["fatal"])
        with patch("sys.argv", ["compare", "--modes", "ssc-ra-regcalls"]), patch("sys.stderr", new=io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                benchmark.main()
            self.assertEqual(error.exception.code, 2)

    def test_register_entry_timing_uses_hardware_return_pc(self):
        image = bytearray(65536)
        image[0x100:0x10e] = bytes.fromhex("d5 cd 00 02 d1 22 00 c2 21 ff ff 19 f9 c9")
        image[0x200:0x204] = bytes.fromhex("21 07 00 c9")
        for label in ("ssc.regcall.root", "ssc.regcall.v1.w0.root", "ssc.autocall.v1.w0.root"):
            for ordinary in (False, True):
                symbols = dict(bench=0x100, bench_buffer=0xc000, bench_params=0xc100, bench_result=0xc200,
                               mainmain=0x100)
                if ordinary:
                    symbols["root"] = 0x300
                symbols[label] = 0x200
                value, _, counters = benchmark.execute(image, symbols, "root", [], b"", 0)
                self.assertEqual(value, 7)
                self.assertEqual((counters["kernel_cycles"], counters["kernel_instructions"]), (20, 2))
        del symbols[label]
        with self.assertRaisesRegex(RuntimeError, "Missing kernel entry"):
            benchmark.execute(image, symbols, "missing", [], b"", 0)

    def test_competitor_changes_are_not_silent(self):
        reference = next(row for row in self.reference["results"] if row["mode"] == "sdcc-size")
        result = dict(reference, code_bytes=reference["code_bytes"] - 1)
        self.assertTrue(benchmark.compare_result(result, reference)[0]["fatal"])
        self.assertFalse(benchmark.compare_result(result, reference, True)[0]["fatal"])

    def test_oracle_drift_changes_the_fingerprint(self):
        functions = benchmark.sources()
        packed = (benchmark.ROOT / "tests" / "z80" / "sms" / "calculations-1" / "tiles.packbits").read_bytes()
        original = benchmark.inputs
        with patch.object(benchmark, "inputs", side_effect=lambda name, seed: (
            *original(name, seed)[:3], original(name, seed)[3] ^ 1,
        )):
            self.assertNotEqual(benchmark.corpus(functions, packed), self.reference["corpus"])

    def test_source_drift_changes_the_fingerprint(self):
        functions = benchmark.sources()
        functions["fillBytes"] += "\n"
        packed = (benchmark.ROOT / "tests" / "z80" / "sms" / "calculations-1" / "tiles.packbits").read_bytes()
        self.assertNotEqual(benchmark.corpus(functions, packed), self.reference["corpus"])

    def test_application_drift_preserves_original_corpus(self):
        functions = benchmark.sources()
        packed = (benchmark.ROOT / "tests" / "z80" / "sms" / "calculations-1" / "tiles.packbits").read_bytes()
        for name in APPLICATION_CASES:
            with self.subTest(name=name):
                changed = dict(functions)
                changed[name] += "\n"
                measured = benchmark.corpus(changed, packed)
                self.assertNotEqual(measured[name], self.reference["corpus"][name])
                for original_name in self.reference["corpus"].keys() - APPLICATION_CASES.keys():
                    self.assertEqual(measured[original_name], self.reference["corpus"][original_name])
        original_inputs = benchmark.inputs
        for name in APPLICATION_CASES:
            with self.subTest(oracle=name), patch.object(benchmark, "inputs", side_effect=lambda routine, seed: (
                    *original_inputs(routine, seed)[:3],
                    original_inputs(routine, seed)[3] ^ (routine == name))):
                self.assertNotEqual(benchmark.corpus(functions, packed)[name], self.reference["corpus"][name])

    def test_missing_tool_is_an_error(self):
        with patch.object(benchmark.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "Required benchmark tool not found"):
                benchmark.tool("missing-sdcc")

    def test_nonempty_output_is_not_reused_or_modified(self):
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / "keep"
            marker.write_bytes(b"existing output")
            with patch("sys.argv", ["compare", "--output-dir", directory]), patch("sys.stderr", new=io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    benchmark.main()
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(marker.read_bytes(), b"existing output")
            self.assertFalse((Path(directory) / "report.json").exists())

    def test_missing_tools_leave_failed_report_without_compiling(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch("sys.argv", ["compare", "--output-dir", directory]), \
                    patch.object(benchmark, "tool", side_effect=RuntimeError("missing tool")), \
                    patch.object(benchmark, "compile_case") as compile_case:
                with self.assertRaisesRegex(RuntimeError, "missing tool"):
                    benchmark.main()
                compile_case.assert_not_called()
            report = json.loads((Path(directory) / "report.json").read_text())
            self.assertEqual(report["status"], "failed")

    def test_corpus_drift_fails_before_resolving_compilers(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch("sys.argv", ["compare", "--output-dir", directory]), \
                    patch.object(benchmark, "corpus", return_value={}), \
                    patch.object(benchmark, "tool") as tool:
                with self.assertRaisesRegex(RuntimeError, "Benchmark sources, asset or oracle changed"):
                    benchmark.main()
                tool.assert_not_called()
            self.assertEqual(json.loads((Path(directory) / "report.json").read_text())["status"], "failed")

    def test_sccz80_help_status_is_not_a_compile_error(self):
        completed = subprocess.CompletedProcess([], 1, b"\r\nOptions:\r\nSmall-C/Plus - z80 Crosscompiler   Version: 23854")
        with patch.object(benchmark.subprocess, "run", return_value=completed):
            self.assertIn("23854", benchmark.tool_version("sccz80", "sccz80"))
            with self.assertRaisesRegex(RuntimeError, "Cannot identify sdcc"):
                benchmark.tool_version("sdcc", "sdcc")

    def test_unrecognized_version_errors_are_not_ignored(self):
        completed = subprocess.CompletedProcess([], 1, b"Cannot start compiler")
        with patch.object(benchmark.subprocess, "run", return_value=completed):
            with self.assertRaisesRegex(RuntimeError, "Cannot identify sccz80"):
                benchmark.tool_version("sccz80", "sccz80")

    def test_invalid_hex_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.ihx"
            for content in (":0101000000FF\n", ":00000000\n", "not a hex record\n", ":0101000000FE\n"):
                with self.subTest(content=content):
                    path.write_text(content)
                    with self.assertRaises(RuntimeError):
                        benchmark.intel_hex(path)

    def test_valid_hex_requires_end_record(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "valid.ihx"
            path.write_text(":010100002AD4\n:00000001FF\n")
            self.assertEqual(benchmark.intel_hex(path)[0x100], 42)

    def test_native_instruction_prefix_is_one_step(self):
        machine = Z80Machine()
        machine.memory[:4] = bytes([0xdd, 0x21, 0x34, 0x12])
        self.assertEqual(step(machine), 14)
        self.assertEqual(machine.pc, 4)
        self.assertEqual(machine.ix, 0x1234)

    def test_native_clock_wrap_and_pc_guard(self):
        result = execute(bytes([0] * 25001 + [0xc9]), 0, [], [], bytearray(), 0, 0,
                         instruction_limit=26000, entry_sp_offset=-1)
        self.assertEqual((result["instructions"], result["cycles"]), (25002, 100014))
        entry = execute(bytes([0xed, 0x73, 0, 0xc1, 0x21, 0xff, 0xff, 0x19, 0xf9, 0xc9]),
                        0, [], [], bytearray(2), 0xc100, 0)
        self.assertEqual(entry["buffer"], bytearray([0xfd, 0xcf]))
        machine = Z80Machine()
        machine.pc = 0xc000
        with self.assertRaisesRegex(RuntimeError, "PC escape"):
            step(machine)

    def test_original_reference_is_not_mutated(self):
        reference = next(row for row in self.reference["results"] if row["mode"] == "ssc-ra")
        before = copy.deepcopy(reference)
        benchmark.compare_result(dict(reference), reference)
        self.assertEqual(reference, before)

    def test_ssc_wrapper_entry_includes_saved_frame_pointer(self):
        memory = bytearray(65536)
        code = bytes([0xed, 0x73, 0, 0xc0, 0x21, 42, 0, 0x22, 0, 0xc3, 0x31, 0xff, 0xcf, 0xc9])
        memory[0x100:0x100 + len(code)] = code
        for ssc in (False, True):
            with self.subTest(ssc=ssc):
                symbols = {"bench": 0x100, "probe": 0x100, "bench_buffer": 0xc000,
                           "bench_params": 0xc240, "bench_result": 0xc300}
                if ssc:
                    symbols["mainmain"] = 0x200
                value, buffer, _ = benchmark.execute(memory, symbols, "probe", [], b"", 0)
                self.assertEqual(value, 42)
                self.assertEqual(buffer[:2], bytes([0xfd if ssc else 0xff, 0xcf]))

    def test_footprint_includes_storage_below_sp(self):
        for destination, reserve, sp, access, below, footprint in (
            (0xcff0, False, 2, 15, 13, 17),
            (0xc000, False, 2, 2, 0, 4),
            (0xc000, True, 31, 2, 0, 33),
        ):
            with self.subTest(destination=destination, reserve=reserve):
                memory = bytearray(65536)
                memory[0x100:0x107] = bytes([0xcd, 0, 2, 0x22, 0, 0xc3, 0xc9])
                kernel = bytes([
                    0x21, 42, 0, 0x22, destination & 255, destination >> 8, 0xc9,
                ])
                if reserve:
                    kernel = b"\x31\xe0\xcf" + kernel[:-1] + b"\x31\xfd\xcf\xc9"
                memory[0x200:0x200 + len(kernel)] = kernel
                symbols = {"bench": 0x100, "probe": 0x200, "bench_buffer": 0xc000,
                           "bench_params": 0xc240, "bench_result": 0xc300}
                value, _, counters = benchmark.execute(memory, symbols, "probe", [], b"", 0)
                self.assertEqual(value, 42)
                self.assertEqual(counters["stack_high_water"], sp)
                self.assertEqual(counters["stack_access_extent"], access)
                self.assertEqual(counters["below_sp_extent"], below)
                self.assertEqual(counters["frame_footprint"], footprint)


class ApplicationOracleTests(unittest.TestCase):
    def test_application_cases_are_deterministic_and_preserve_inputs(self):
        models = dict(zip(APPLICATION_CASES, (sprite_scene, tracker, navigation)))
        for name, cases in APPLICATION_CASES.items():
            counts = set()
            for case in range(cases):
                with self.subTest(name=name, case=case):
                    parameters, initial, expected, value = application_inputs(name, case)
                    self.assertEqual(len(initial), 512)
                    self.assertEqual(len(expected), 512)
                    self.assertTrue(0 <= value <= 65535)
                    self.assertEqual(application_inputs(name, case), (parameters, initial, expected, value))
                    before = bytes(initial)
                    self.assertEqual(models[name](*parameters[:2], initial), (expected, value))
                    self.assertEqual(initial, before)
                    self.assertEqual(initial[384:], expected[384:])
                    counts.add(parameters[1])
            self.assertIn(0, counts)
            self.assertGreaterEqual(len(counts), 4)
        for name, case in (("unknown", 0), ("calculationTrackerWorkload", -1),
                           ("calculationTrackerWorkload", 12)):
            with self.assertRaisesRegex(ValueError, "Unknown application case"):
                application_inputs(name, case)

    def test_sprite_collision_and_position_observations(self):
        for case in range(12):
            _, _, output, _ = application_inputs("calculationSpriteSceneWorkload", case)
            collisions = int.from_bytes(output[296:298], "little")
            self.assertEqual(sum(output[192:204]), collisions * 2)
            for actor in range(12):
                offset = 224 + actor * 4
                self.assertTrue(256 <= int.from_bytes(output[offset:offset + 2], "little") <= 3584)
                self.assertTrue(256 <= int.from_bytes(output[offset + 2:offset + 4], "little") <= 2560)
                self.assertLess(output[272 + actor], 4)
                self.assertLess(output[284 + actor], 4)
        _, _, output, _ = application_inputs("calculationSpriteSceneWorkload", 0)
        self.assertEqual(output[296:300], bytes((0, 0, 0, 12)))
        self.assertEqual(sum(value >= 16 for value in output[:192]), 12)

    def test_tracker_has_hand_checked_pcm_and_voice_state(self):
        _, _, output, _ = application_inputs("calculationTrackerWorkload", 1)
        self.assertEqual(output[:4], bytes((15, 30, 45, 75)))
        voice = (856).to_bytes(2, "little") + (40).to_bytes(2, "little") + bytes((15, 1, 0, 0))
        self.assertEqual(output[128:160], voice * 4)
        effects = set()
        for case in range(12):
            _, _, output, _ = application_inputs("calculationTrackerWorkload", case)
            effects.update(output[134:160:8])
        self.assertEqual(effects, {0, 1, 2, 3})

    def test_navigation_open_and_unreachable_routes(self):
        for terrain, distance, route in ((1, 7, bytes((7, 0))), (0, 65535, bytes((0, 255)))):
            with self.subTest(terrain=terrain):
                initial = bytearray([terrain] * 512)
                output, _ = navigation(0, 1, initial)
                self.assertEqual(output[288:290], bytes((17, 24)))
                self.assertEqual(int.from_bytes(output[290:292], "little"), distance)
                self.assertEqual(output[192:194], route)
                self.assertEqual(initial, bytearray([terrain] * 512))

    def test_navigation_route_stream_reaches_each_goal(self):
        for case in range(12):
            parameters, _, output, _ = application_inputs("calculationNavigationWorkload", case)
            cursor = 192
            for job in range(parameters[1]):
                offset = 288 + job * 8
                position, goal = output[offset:offset + 2]
                distance = int.from_bytes(output[offset + 2:offset + 4], "little")
                steps = 0
                while cursor < 288 and output[cursor]:
                    command = output[cursor]
                    cursor += 1
                    direction, length = divmod(command, 64)
                    self.assertGreater(length, 0)
                    for _ in range(length):
                        previous = position
                        position += (1, -1, 16, -16)[direction]
                        self.assertTrue(0 <= position < 192)
                        if direction < 2:
                            self.assertEqual(position // 16, previous // 16)
                    steps += length
                self.assertLess(cursor, 288)
                cursor += 1
                self.assertEqual(steps, output[offset + 6])
                if distance != 65535:
                    self.assertEqual((position, steps), (goal, distance))
                else:
                    self.assertEqual(steps, 0)
            self.assertEqual(cursor - 192, output[319])


if __name__ == "__main__":
    unittest.main()
