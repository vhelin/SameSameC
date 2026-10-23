import io
import json
import os
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

import check_z80_calculations as console
import check_register_allocator as allocator


def cartridge(palette=(0, 0, 255, 15), column=6, wrong_result=False, rom_write=False,
              bad_stack=False, bad_vdp=False):
    rom = bytearray(65536)
    rom[:3] = bytes([0xc3, 0, 1])
    code = bytearray([0xf3, 0x31, 0xf0, 0xdf])

    def port(number, value):
        code.extend((0x3e, value, 0xd3, number))

    def address(value, command):
        port(0xbf, value & 255)
        port(0xbf, (value >> 8) | command)

    if rom_write:
        code.extend((0x3e, 1, 0x32, 0, 2))
    address(0, 0xc0)
    for value in palette:
        port(0xbe, value)
    port(0xbf, 4)
    port(0xbf, 0x80)
    port(0xbf, 0x40)
    port(0xbf, 0x81)
    port(0xbf, 0 if bad_vdp else 0xff)
    port(0xbf, 0x82)
    text = (console.ROOT / "tests" / "z80" / "common" / "calculation_display.ssc").read_text()
    font_text = text.split("const u8 fontData[] = {", 1)[1].split("};", 1)[0]
    address(0, 0x40)
    for value in re.findall(r"0x([0-9a-fA-F]+)", font_text):
        port(0xbe, int(value, 16))
    rows = ["CALCULATIONS 1", "EXPECTED = ACTUAL"]
    rows.extend(f"${value:04X} = ${(value + int(wrong_result and index == 0)):04X}"
                for index, value in enumerate(console.GG_RESULTS[1]))
    for row, label in enumerate(rows):
        address(0x3800 + (row + 3) * 64 + column * 2, 0x40)
        for character in label:
            port(0xbe, ord(character) - 32)
            port(0xbe, 0)
    if bad_stack:
        code.extend((0x31, 0x10, 0xe0))
    end = 0x100 + len(code)
    code.extend((0xc3, end & 255, end >> 8))
    if len(code) >= 0x3f00:
        raise RuntimeError("Synthetic cartridge code overlaps its mapped bank")
    rom[0x100:0x100 + len(code)] = code
    rom[0x7ff0:0x7ff8] = b"TMR SEGA"
    rom[0x7fff] = 0x6e
    checksum = (sum(rom[:0x7ff0]) + sum(rom[0x8000:])) & 65535
    rom[0x7ffa:0x7ffc] = checksum.to_bytes(2, "little")
    return rom


class ConsoleTests(unittest.TestCase):
    def test_allocator_cohorts_still_extract_the_shared_original_helpers(self):
        for flags, symbol in (
            ({"RA_MATRIX_CASES": "1", "RA_VALUE_CASES": "1", "RA_FAMILIES": "matrix_real_index"},
             "void convertNumbersToChars16("),
            ({"RA_SCALAR_CASES": "1", "RA_FAMILIES": "scalar_call_transport"}, "u16 calculationAdd3("),
        ):
            with self.subTest(flags=flags), patch.dict(os.environ, flags, clear=True):
                programs, preamble, _ = allocator.generate_programs(0)
                self.assertTrue(any(program.family == flags["RA_FAMILIES"] for program in programs))
                self.assertIn(symbol, preamble)

    def test_display_oracles_bind_all_seven_new_tests(self):
        self.assertEqual(console.new_results(), [
            (0xa9e7, 0x18e0), (0x84a0, 0xeb88), (0xcb02, 0x934a),
            (0xc8ed, 0xfa5a), (0xe6cd, 0xa776), (0xf478, 0xf478), (0x32d9, 0x32d9),
        ])
        self.assertEqual(len(console.NEW_LABELS), 7)
        self.assertTrue(all(len(label) <= 20 for label in console.NEW_LABELS))

    def test_complete_game_gear_viewport_font_palette_and_output(self):
        result = console.execute_rom(cartridge(), {"RAM_USAGE_SLOT_RAMSlot1_BANK_0_END": 0xc000}, 1, "gg")
        self.assertEqual(result["numeric_rows"], 9)
        self.assertEqual(len(result["rows"]), 18)
        self.assertEqual(result["font_crc16"], 0x6986)
        self.assertEqual(result["palette"], [0, 0, 255, 15])
        self.assertLessEqual(result["ram_used_bytes"], 8192)

    def test_console_failures_are_not_success_shaped(self):
        for arguments, error in (
            ({"palette": (0, 63)}, "Incorrect gg palette"),
            ({"column": 5}, "outside the 20x18"),
            ({"wrong_result": True}, "Displayed results differ"),
            ({"rom_write": True}, "ROM write"),
            ({"bad_stack": True}, "Stack escaped physical RAM"),
            ({"bad_vdp": True}, "mode-4 display geometry"),
        ):
            with self.subTest(arguments=arguments), self.assertRaisesRegex(RuntimeError, error):
                console.execute_rom(cartridge(**arguments), {"RAM_USAGE_SLOT_RAMSlot1_BANK_0_END": 0xc000}, 1, "gg")

    def test_cartridge_tag_region_checksum_and_ram_extent_are_checked(self):
        for mutation, error in (("tag", "header"), ("region", "Game Gear"),
                                ("checksum", "checksum"), ("ram", "global RAM")):
            rom = cartridge()
            symbols = {"RAM_USAGE_SLOT_RAMSlot1_BANK_0_END": 0xc000}
            if mutation == "tag":
                rom[0x7ff0] = 0
            elif mutation == "region":
                rom[0x7fff] = 0x4e
            elif mutation == "checksum":
                rom[0x7ffa] ^= 1
            else:
                symbols["RAM_USAGE_SLOT_RAMSlot1_BANK_0_END"] = 0xd000
            with self.subTest(mutation=mutation), self.assertRaisesRegex(RuntimeError, error):
                console.execute_rom(rom, symbols, 1, "gg")

    def test_all_six_builds_require_ra_and_the_correct_bank_flags(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            for platform in ("sms", "gg"):
                for project in (1, 2, 3):
                    commands = []

                    def fake_run(command, directory, log, recorded):
                        commands.append(command)
                        if command[0] == "linker":
                            (directory / ("linked.gg" if platform == "gg" else "linked.sms")).write_bytes(bytes(65536))
                            (directory / "linked.sym").write_text("00:c000 RAM_USAGE_SLOT_RAMSlot1_BANK_0_END\n")

                    with patch.object(console, "run", side_effect=fake_run):
                        _, _, symbols = console.build(project, platform, output, "compiler", "linker")
                    compiled = [command for command in commands if command[0] == "compiler"]
                    self.assertEqual(len(compiled), 4 if project == 3 else 5)
                    for command in compiled:
                        self.assertIn("-ra", command)
                        self.assertNotIn("-no-ra", command)
                        name = command[-1]
                        if name.startswith("bank_"):
                            self.assertEqual(command[command.index("-b") + 1], name[5])
                            self.assertEqual(command[command.index("-s") + 1], "2")
                    self.assertEqual(symbols["RAM_USAGE_SLOT_RAMSlot1_BANK_0_END"], 0xc000)

    def test_nonempty_output_and_duplicate_selections_fail_before_tools(self):
        for duplicate in (False, True):
            with tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary)
                arguments = ["console", "--output-dir", temporary]
                if duplicate:
                    arguments.extend(("--platforms", "gg", "gg"))
                else:
                    (output / "keep").write_bytes(b"existing")
                with patch("sys.argv", arguments), patch("sys.stderr", new=io.StringIO()), \
                        patch.object(console, "tool") as tool:
                    with self.assertRaises(SystemExit) as error:
                        console.main()
                    self.assertEqual(error.exception.code, 2)
                    tool.assert_not_called()

    def test_missing_tool_leaves_an_explicit_failed_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            with patch("sys.argv", ["console", "--output-dir", temporary]), \
                    patch.object(console, "tool", side_effect=RuntimeError("missing compiler")):
                with self.assertRaisesRegex(RuntimeError, "missing compiler"):
                    console.main()
            report = json.loads((Path(temporary) / "report.json").read_text())
            self.assertEqual((report["status"], report["mode"]), ("failed", "ssc-ra"))


if __name__ == "__main__":
    unittest.main()
