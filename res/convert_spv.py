import os
import subprocess
import sys

GLSLC = "glslc"
GLSLANG = "glslangValidator"

# valid Vulkan shader extensions
VALID_EXTENSIONS = {
    ".vert",
    ".frag",
    ".comp",
    ".geom",
    ".tesc",
    ".tese",
    ".rchit",
    ".rmiss",
    ".rgen",
    ".rahit",
    ".rint",
    ".rcall",
}

RAY_TRACING_EXTENSIONS = {
    ".rgen",
    ".rmiss",
    ".rchit",
    ".rahit",
    ".rint",
    ".rcall",
}

# only non-raytracing stages for glslc
GLSLC_STAGE_MAP = {
    ".vert": "vert",
    ".frag": "frag",
    ".comp": "comp",
    ".geom": "geom",
    ".tesc": "tesc",
    ".tese": "tese",
}


def is_vk_shader(filename: str) -> bool:
    _, ext = os.path.splitext(filename)
    return ext in VALID_EXTENSIONS


def build_command(filepath: str) -> list[str]:
    _, ext = os.path.splitext(filepath)
    output_path = filepath + ".spv"

    if ext in RAY_TRACING_EXTENSIONS:
        return [
            GLSLANG,
            "-V",
            "--target-env", "vulkan1.2",
            filepath,
            "-o", output_path,
        ]

    cmd = [GLSLC, filepath, "-o", output_path]

    if ext in GLSLC_STAGE_MAP:
        cmd.insert(1, f"-fshader-stage={GLSLC_STAGE_MAP[ext]}")

    return cmd


def compile_shader(filepath: str, is_debug: bool):
    output_path = filepath + ".spv"
    cmd = build_command(filepath)

    if is_debug:
        print(f"Compiling: {filepath}")
        print("Command:", " ".join(cmd))

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(f"Failed to compile {filepath}")
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr)
    else:
        print(f"Generated {output_path}")
        if is_debug:
            print()


def main(root_dir: str, is_debug: bool):
    for root, _, files in os.walk(root_dir):
        for file in files:
            if is_vk_shader(file):
                full_path = os.path.join(root, file)
                compile_shader(full_path, is_debug)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        directory = sys.argv[1]
    else:
        directory = "."

    debug_on = len(sys.argv) > 2 and sys.argv[2].lower() == "debug"

    main(directory, debug_on)
