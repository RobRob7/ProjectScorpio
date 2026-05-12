import os
import subprocess
import sys

GLSLC = "glslc"
GLSLANG = "glslangValidator"

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

GLSLC_STAGE_MAP = {
    ".vert": "vertex",
    ".frag": "fragment",
    ".comp": "compute",
    ".geom": "geometry",
    ".tesc": "tesscontrol",
    ".tese": "tesseval",
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
            "-g",
            "-V",
            "--target-env", "vulkan1.2",
            filepath,
            "-o", output_path,
        ]

    cmd = [GLSLC, "-g"]

    if ext in GLSLC_STAGE_MAP:
        cmd.append(f"-fshader-stage={GLSLC_STAGE_MAP[ext]}")

    cmd += [filepath, "-o", output_path]
    return cmd


def compile_shader(filepath: str, is_debug: bool) -> bool:
    shader_dir = os.path.dirname(filepath)
    shader_file = os.path.basename(filepath)

    cmd = build_command(shader_file)

    if is_debug:
        print(f"Compiling: {filepath}")
        print("CWD:", shader_dir)
        print("Command:", " ".join(cmd))

    result = subprocess.run(
        cmd,
        cwd=shader_dir,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(f"\nFAILED TO COMPILE: {filepath}")
        print(result.stdout)
        print(result.stderr)
        return False

    if is_debug:
        print(f"Generated {filepath}.spv")

    return True


def main(root_dir: str, is_debug: bool):
    for root, _, files in os.walk(root_dir):
        for file in files:
            if is_vk_shader(file):
                full_path = os.path.join(root, file)

                success = compile_shader(full_path, is_debug)

                if not success:
                    print("\nBuild stopped due to shader errors.")
                    sys.exit(1)


if __name__ == "__main__":
    directory = sys.argv[1] if len(sys.argv) > 1 else "."
    debug_on = len(sys.argv) > 2 and sys.argv[2].lower() == "debug"

    main(directory, debug_on)
