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


class ComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reference = json.loads(benchmark.BASELINE.read_text())

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
        rows = self.reference["results"]
        self.assertEqual(len(rows), 104)
        self.assertEqual(len({(row["routine"], row["mode"]) for row in rows}), 104)
        self.assertEqual(sum(row["cases"] for row in rows), 1856)
        self.assertEqual(self.reference["flags"], benchmark.FLAGS)

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
        result = dict(reference, mode="ssc-ra-regcalls", code_bytes=120, wrapper_bytes=70,
                      total_kernel_cycles=410, total_wrapper_cycles=300)
        self.assertEqual(benchmark.compare_result(result, reference), [])
        for metric, value in (("code_bytes", 131), ("total_kernel_cycles", 501),
                              ("max_stack_bytes", 21), ("max_stack_access_bytes", 21),
                              ("max_below_sp_bytes", 9), ("max_frame_footprint_bytes", 23)):
            issues = benchmark.compare_result(dict(result, **{metric: value}), reference, True)
            self.assertEqual(len(issues), 1, metric)
            self.assertTrue(issues[0]["fatal"])
        with patch("sys.argv", ["compare", "--modes", "ssc-ra-regcalls"]), patch("sys.stderr", new=io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                benchmark.main()
            self.assertEqual(error.exception.code, 2)

    def test_register_entry_timing_uses_hardware_return_pc(self):
        image = bytearray(65536)
        image[0x100:0x10e] = bytes.fromhex("d5 cd 00 02 d1 22 00 c2 21 ff ff 19 f9 c9")
        image[0x200:0x204] = bytes.fromhex("21 07 00 c9")
        symbols = dict(bench=0x100, bench_buffer=0xc000, bench_params=0xc100, bench_result=0xc200,
                       mainmain=0x100, root=0x300)
        symbols["ssc.regcall.root"] = 0x200
        value, _, counters = benchmark.execute(image, symbols, "root", [], b"", 0)
        self.assertEqual(value, 7)
        self.assertEqual((counters["kernel_cycles"], counters["kernel_instructions"]), (20, 2))

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


if __name__ == "__main__":
    unittest.main()
