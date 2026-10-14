import pathlib
import logging
import subprocess
import pydantic_cli
from pydantic import Field
import os

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s: %(message)s",
)
logger = logging.getLogger(__name__)


class OpenOcdCmdLineConfig(pydantic_cli.Cmd):
    openOcdInstallDir: pathlib.Path = Field(
        ...,
        description="OpenOCD 的安装目录",
    )

    projectMainDir: pathlib.Path = Field(
        default_factory=pathlib.Path.cwd,
        description="项目主目录",
    )

    extraHexFileScanDir: pathlib.Path | None = Field(
        default=None,
        description="额外扫描的 hex 文件目录",
    )

    scripts: list[pathlib.Path] | None = None
    cfgFiles: list[pathlib.Path] | None = None
    commands: list[str] | None = None

    @property
    def cmdOcd(self) -> pathlib.Path:
        return self.openOcdInstallDir / "bin" / "openocd.exe"

    @property
    def defaultScriptPath(self) -> pathlib.Path:
        return self.openOcdInstallDir / "scripts"

    def build_args(self):
        logging.info("self: %s", self)

        cfgFiles = self.cfgFiles or [
            self.defaultScriptPath / "interface" / "kitprog3.cfg",
            self.defaultScriptPath / "target" / "infineon" / "pse84xgxs2.cfg",
        ]
        scripts = self.scripts or [
            self.openOcdInstallDir / "scripts",
            self.openOcdInstallDir / "flm" / "cypress" / "cat4",
        ]



        flashTarget = self.projectMainDir / ".flash-target"
        if flashTarget.exists() or flashTarget.is_symlink():
            if not flashTarget.is_file():
                raise ValueError(f"flash target manifest must be a file: {flashTarget}")
            targetLines = flashTarget.read_text(encoding="utf-8").splitlines()
            if len(targetLines) != 1 or not targetLines[0].strip():
                raise ValueError(
                    f"{flashTarget} must contain exactly one relative HEX path"
                )

            targetText = targetLines[0].strip()
            targetPath = pathlib.Path(targetText)
            windowsTarget = pathlib.PureWindowsPath(targetText)
            if (
                targetPath.is_absolute()
                or windowsTarget.root
                or windowsTarget.drive
                # The existing OpenOCD command embeds the path as an unquoted Tcl word.
                or any(char.isspace() or char in '{}[];$`#":\x00' for char in targetText)
            ):
                raise ValueError(f"invalid flash target path: {targetText!r}")

            projectRoot = self.projectMainDir.resolve()
            resolvedTarget = (projectRoot / targetPath).resolve(strict=True)
            try:
                targetRelative = resolvedTarget.relative_to(projectRoot)
            except ValueError as exc:
                raise ValueError(
                    f"flash target must stay inside project root: {targetText!r}"
                ) from exc
            if not resolvedTarget.is_file() or resolvedTarget.suffix.lower() != ".hex":
                raise ValueError(
                    f"flash target must be an existing HEX file: {targetText!r}"
                )
            hexFile = [resolvedTarget]
            hexStr = targetRelative.as_posix()
            # A symlink can resolve to a name different from the manifest's text.
            if any(char.isspace() or char in '{}[];$`#":\\\x00' for char in hexStr):
                raise ValueError(f"invalid resolved flash target path: {hexStr!r}")
        else:
            # Preserve the existing search and its ordering when no manifest exists.
            hexFile = list(self.projectMainDir.rglob("*.hex"))
            if not hexFile:
                raise FileNotFoundError(
                    f"hex file not found in {self.projectMainDir}"
                )
            hexStr = hexFile[0].relative_to(self.projectMainDir).as_posix()
        logging.info("hexFile: %s", hexFile)
        commands = self.commands or [
            "set QSPI_FLASHLOADER ../flm/cypress/cat4/PSE84_SMIF.FLM",
            "transport select swd",
            "set ENABLE_ACQUIRE 0",
            "; ".join(
                [
                    "init",
                    "reset init",
                    f"flash write_image erase {hexStr}",
                    "reset run",
                    "exit",
                ]
            ),
        ]

        args = [str(self.cmdOcd)]

        for script in scripts:
            if not script.exists():
                raise FileNotFoundError(f"script {script} not found")

            args.extend(["-s", str(script.as_posix())])
        args.extend(["-c", "array set SMIF_BANKS {0 {addr 0x60000000 size 0x4000000}}"])
        args.extend(
            ["-c", "set QSPI_FLASHLOADER ../flm/infineon/pse8x6/PSE84_SMIF.FLM"]
        )
        for cfg in cfgFiles:
            args.extend(
                [
                    "-f",
                    cfg.relative_to(self.defaultScriptPath).as_posix(),
                ]
            )

        for command in commands:
            args.extend(["-c", command])

        return args

    def run(self):
        subprocess.run(
            ["scons", f"-j{int(os.cpu_count() * 4.0/5)}"],
            cwd=self.projectMainDir,
            check=True,
        )
        cmd = self.build_args()
        logger.info("cmd: %s", cmd)
        subprocess.run(cmd, check=True, cwd=self.projectMainDir)


if __name__ == "__main__":
    pydantic_cli.run_and_exit(OpenOcdCmdLineConfig)
