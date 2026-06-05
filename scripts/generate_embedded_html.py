from pathlib import Path
import subprocess
import sys

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
platform = env.PioPlatform()

source_html = project_dir / "web" / "index.html"
target_asm = project_dir / "src" / "index.html.S"

if target_asm.exists() and target_asm.stat().st_mtime >= source_html.stat().st_mtime:
    print("Embedded HTML is up to date")
else:
    framework_dir = Path(platform.get_package_dir("framework-espidf"))
    tool_cmake_dir = Path(platform.get_package_dir("tool-cmake"))

    cmake_exe = tool_cmake_dir / "bin" / ("cmake.exe" if sys.platform.startswith("win") else "cmake")
    embed_script = framework_dir / "tools" / "cmake" / "scripts" / "data_file_embed_asm.cmake"

    subprocess.run(
        [
            str(cmake_exe),
            "-D",
            f"DATA_FILE={source_html.as_posix()}",
            "-D",
            f"SOURCE_FILE={target_asm.as_posix()}",
            "-D",
            "FILE_TYPE=TEXT",
            "-P",
            str(embed_script),
        ],
        check=True,
    )

    print("Generated src/index.html.S from web/index.html")
