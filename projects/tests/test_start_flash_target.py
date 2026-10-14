import pathlib
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import call, patch

from start import OpenOcdCmdLineConfig


class FlashTargetTests(unittest.TestCase):
    def build_args(self, root):
        config = SimpleNamespace(
            projectMainDir=root,
            cfgFiles=[root],
            scripts=[root],
            defaultScriptPath=root,
            openOcdInstallDir=root,
            cmdOcd=root / "openocd.exe",
            commands=None,
        )
        return OpenOcdCmdLineConfig.build_args(config)

    def flash_command(self, args):
        return next(arg for arg in args if "flash write_image erase" in arg)

    def test_manifest_target_precedes_other_hex_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "fallback.hex").write_text(":00000001FF\n", encoding="ascii")
            (root / "build").mkdir()
            (root / "build" / "rtthread.hex").write_text(
                ":00000001FF\n", encoding="ascii"
            )
            (root / ".flash-target").write_text(
                "build/rtthread.hex\n", encoding="utf-8"
            )

            args = self.build_args(root)

            self.assertIn("flash write_image erase build/rtthread.hex", self.flash_command(args))

    def test_absent_manifest_uses_original_all_hex_search(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "rtthread.hex").write_text(":00000001FF\n", encoding="ascii")
            (root / "application.hex").write_text(":00000001FF\n", encoding="ascii")

            args = self.build_args(root)

            self.assertIn("flash write_image erase", self.flash_command(args))
            # The legacy search includes arbitrary .hex names, not only rt*thread.hex.
            self.assertEqual(
                list(root.rglob("*.hex"))[0].relative_to(root).as_posix(),
                self.flash_command(args).split("flash write_image erase ", 1)[1].split(";", 1)[0],
            )

    def test_invalid_manifest_targets_fail_without_fallback(self):
        invalid_targets = [
            "",
            " \n",
            "fallback.hex\n\n",
            "../outside.hex",
            "C:/outside.hex",
            "C:outside.hex",
            "/outside.hex",
            "\\outside.hex",
            "//server/share/outside.hex",
            "build/image.bin",
            "missing.hex",
            "build/image.hex; reset run",
            '"fallback.hex"',
            "build/image.hex:stream.hex",
            "build/image name.hex",
            "build/$image.hex",
            "build/[exit].hex",
            "build/{image}.hex",
            "build/image\x00.hex",
            "build/image.hex\nbuild/other.hex",
        ]
        for target in invalid_targets:
            with self.subTest(target=target), tempfile.TemporaryDirectory() as temp:
                root = pathlib.Path(temp)
                (root / "fallback.hex").write_text(":00000001FF\n", encoding="ascii")
                (root / ".flash-target").write_text(target, encoding="utf-8")

                expected_error = (
                    FileNotFoundError
                    if target in ("../outside.hex", "build/image.bin", "missing.hex")
                    else ValueError
                )
                with self.assertRaises(expected_error):
                    self.build_args(root)

    def test_manifest_target_must_be_a_file(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "build.hex").mkdir()
            (root / ".flash-target").write_text("build.hex\n", encoding="utf-8")

            with self.assertRaises(ValueError):
                self.build_args(root)

    def test_existing_non_hex_target_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "image.bin").write_bytes(b"image")
            (root / ".flash-target").write_text("image.bin", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "existing HEX file"):
                self.build_args(root)

    def test_existing_target_outside_project_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            parent = pathlib.Path(temp)
            root = parent / "project"
            root.mkdir()
            (parent / "outside.hex").touch()
            (root / "fallback.hex").touch()
            (root / ".flash-target").write_text("../outside.hex", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "inside project root"):
                self.build_args(root)

    def test_manifest_must_be_a_regular_file(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "fallback.hex").touch()
            (root / ".flash-target").mkdir()
            with self.assertRaisesRegex(ValueError, "manifest must be a file"):
                self.build_args(root)

    def test_single_line_with_optional_line_ending(self):
        for ending in ("", "\n", "\r\n"):
            with self.subTest(ending=ending), tempfile.TemporaryDirectory() as temp:
                root = pathlib.Path(temp)
                (root / "image.HEX").touch()
                (root / ".flash-target").write_bytes(("image.HEX" + ending).encode())
                with patch.object(pathlib.Path, "rglob", side_effect=AssertionError("must not scan")):
                    args = self.build_args(root)
                self.assertIn("flash write_image erase image.HEX;", self.flash_command(args))

    def test_absent_manifest_preserves_missing_hex_error(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaisesRegex(FileNotFoundError, "hex file not found"):
                self.build_args(pathlib.Path(temp))

    def test_absent_manifest_preserves_search_order(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            candidates = [root / "nested" / "z.hex", root / "a.hex"]
            with patch.object(pathlib.Path, "rglob", return_value=iter(candidates)) as scan:
                args = self.build_args(root)
            scan.assert_called_once_with("*.hex")
            self.assertIn("flash write_image erase nested/z.hex;", self.flash_command(args))

    def make_config(self, root, **overrides):
        install = root / "openocd"
        (install / "scripts").mkdir(parents=True, exist_ok=True)
        (install / "flm" / "cypress" / "cat4").mkdir(parents=True, exist_ok=True)
        return OpenOcdCmdLineConfig(
            openOcdInstallDir=install, projectMainDir=root, **overrides
        )

    def test_complete_default_args_are_unchanged_with_or_without_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "firmware.hex").touch()
            config = self.make_config(root)
            install = config.openOcdInstallDir
            expected = [
                str(install / "bin" / "openocd.exe"),
                "-s", (install / "scripts").as_posix(),
                "-s", (install / "flm" / "cypress" / "cat4").as_posix(),
                "-c", "array set SMIF_BANKS {0 {addr 0x60000000 size 0x4000000}}",
                "-c", "set QSPI_FLASHLOADER ../flm/infineon/pse8x6/PSE84_SMIF.FLM",
                "-f", "interface/kitprog3.cfg",
                "-f", "target/infineon/pse84xgxs2.cfg",
                "-c", "set QSPI_FLASHLOADER ../flm/cypress/cat4/PSE84_SMIF.FLM",
                "-c", "transport select swd",
                "-c", "set ENABLE_ACQUIRE 0",
                "-c", "init; reset init; flash write_image erase firmware.hex; reset run; exit",
            ]
            self.assertEqual(config.build_args(), expected)
            (root / ".flash-target").write_text("firmware.hex\n", encoding="utf-8")
            self.assertEqual(config.build_args(), expected)

    def test_custom_options_are_unchanged(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "firmware.hex").touch()
            script = root / "custom scripts"
            script.mkdir()
            config = self.make_config(
                root, scripts=[script],
                cfgFiles=[root / "openocd" / "scripts" / "custom.cfg"],
                commands=["init; exit"], extraHexFileScanDir=root / "unused",
            )
            expected = [
                str(config.cmdOcd), "-s", script.as_posix(),
                "-c", "array set SMIF_BANKS {0 {addr 0x60000000 size 0x4000000}}",
                "-c", "set QSPI_FLASHLOADER ../flm/infineon/pse8x6/PSE84_SMIF.FLM",
                "-f", "custom.cfg", "-c", "init; exit",
            ]
            self.assertEqual(config.build_args(), expected)
            (root / ".flash-target").write_text("firmware.hex", encoding="utf-8")
            self.assertEqual(config.build_args(), expected)

    def test_run_builds_before_resolving_and_flashing_target(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            config = self.make_config(root)
            (root / ".flash-target").write_text("final.hex", encoding="utf-8")

            def simulate_build(args, **kwargs):
                if args[0] == "scons":
                    (root / "final.hex").touch()

            with patch("start.subprocess.run", side_effect=simulate_build) as run, \
                    patch("start.os.cpu_count", return_value=8):
                config.run()
            self.assertEqual(run.call_args_list, [
                call(["scons", "-j6"], cwd=root, check=True),
                call(config.build_args(), check=True, cwd=root),
            ])

    def test_failed_build_does_not_flash(self):
        with tempfile.TemporaryDirectory() as temp:
            config = self.make_config(pathlib.Path(temp))
            with patch("start.subprocess.run", side_effect=RuntimeError("build failed")) as run:
                with self.assertRaisesRegex(RuntimeError, "build failed"):
                    config.run()
            run.assert_called_once()

    def test_symlink_targets_cannot_escape_project(self):
        with tempfile.TemporaryDirectory() as temp:
            parent = pathlib.Path(temp)
            root = parent / "project"
            root.mkdir()
            (parent / "outside.hex").touch()
            try:
                (root / "link.hex").symlink_to(parent / "outside.hex")
            except OSError as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")
            (root / ".flash-target").write_text("link.hex", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "inside project root"):
                self.build_args(root)

    def test_dangling_manifest_does_not_fall_back(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "fallback.hex").touch()
            try:
                (root / ".flash-target").symlink_to(root / "missing-manifest")
            except OSError as exc:
                self.skipTest(f"symlink creation unavailable: {exc}")
            with self.assertRaisesRegex(ValueError, "manifest must be a file"):
                self.build_args(root)


if __name__ == "__main__":
    unittest.main()
