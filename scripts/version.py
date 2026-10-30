# PlatformIO pre-скрипт: версия прошивки из git (тег vX.Y.Z), попадает в код как FW_VERSION.
# На коммите с тегом — "v1.2.0"; после него — "v1.2.0-3-gabc1234"; без тегов — хэш; без git — "dev".
import subprocess

Import("env")  # noqa: F821 — подставляет PlatformIO

try:
    version = subprocess.check_output(
        ["git", "describe", "--tags", "--always", "--dirty"],
        cwd=env["PROJECT_DIR"], stderr=subprocess.DEVNULL).decode().strip()
except Exception:
    version = "dev"

env.Append(CPPDEFINES=[("FW_VERSION", '\\"%s\\"' % version)])
print("Firmware version:", version)
