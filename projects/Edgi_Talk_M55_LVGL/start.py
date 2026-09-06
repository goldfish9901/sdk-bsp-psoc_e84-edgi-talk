import pathlib
import sys
import logging
import subprocess

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)


class OpenOcdCmdLineConfig:

    def __init__(
        self,
        openOcdInstallDir: pathlib.Path,
        scripts: list[pathlib.Path] = None,
        cfgFiles: list[pathlib.Path] = None,
        commands: list[str] = None,
    ):
        self.openOcdInstallDir = openOcdInstallDir
        self.cmdOcd = openOcdInstallDir / "bin" / "openocd.exe"
        self.defaultScriptPath = openOcdInstallDir / "scripts"
        self.scripts: list[pathlib.Path] = scripts or [
            openOcdInstallDir / "scripts",
            openOcdInstallDir / "flm" / "cypress" / "cat4",
        ]
        self.cfgFiles: list[pathlib.Path] = cfgFiles or [
            self.defaultScriptPath / "interface" / "kitprog3.cfg",
            self.defaultScriptPath / "target" / "infineon" / "pse84xgxs2.cfg",
        ]
        hex = pathlib.Path(__file__).parent / "build" / "rtthread.hex"
        hex_str = str(hex.relative_to(pathlib.Path(__file__).parent).as_posix())
        self.commands: list[str] = commands or [
            "set QSPI_FLASHLOADER ../flm/cypress/cat4/PSE84_SMIF.FLM",
            "transport select swd",
            "set ENABLE_ACQUIRE 0",
            "; ".join(
                [
                    "init",
                    "reset init",
                    f"flash write_image erase {hex_str}",
                    "reset run",
                    "exit",
                ]
            ),
        ]

    def build_args(self):
        args = [str(self.cmdOcd)]
        for script in self.scripts:
            args.append("-s")
            if not script.exists():
                raise FileNotFoundError(f"script {script} not found")
            args.append(str(script.as_posix()))
        for cfg in self.cfgFiles:
            args.append("-f")
            args.append(str(cfg.relative_to(self.defaultScriptPath).as_posix()))
        for cmd in self.commands:
            args.append("-c")
            args.append(cmd)
        return args


def main():
    cfg = OpenOcdCmdLineConfig(
        pathlib.Path("E:\\sdk\\openocd-5.19.0.4782-windows\\openocd")
    )
    cmd = cfg.build_args()
    logging.info(f"cmd: {cmd}")
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()
