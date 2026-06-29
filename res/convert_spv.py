import os
import subprocess
import sys

GLSLC = "glslc"
GLSLANG = "glslangValidator"
DXC = "dxc"

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

HLSL_STAGE_MAP = {
    ".vert.hlsl": ("vs_6_8", "VSMain"),
    ".frag.hlsl": ("ps_6_8", "PSMain"),
    ".comp.hlsl": ("cs_6_8", "CSMain"),
}


def is_vk_shader(filename: str) -> bool:
    _, ext = os.path.splitext(filename)
    return ext in VALID_EXTENSIONS


def get_hlsl_stage(filename: str):
    lower = filename.lower()

    for suffix, stage_info in HLSL_STAGE_MAP.items():
        if lower.endswith(suffix):
            return stage_info

    return None


def is_hlsl_shader(filename: str) -> bool:
    return get_hlsl_stage(filename) is not None


def build_vk_command(filepath: str) -> list[str]:
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


def build_hlsl_command(filepath: str, is_debug: bool) -> list[str]:
    stage_info = get_hlsl_stage(filepath)

    if stage_info is None:
        raise RuntimeError(f"Could not infer HLSL shader stage from file name: {filepath}")

    target_profile, entry_point = stage_info

    # cubemap.vert.hlsl -> cubemap.vert.cso
    output_path = filepath[:-len(".hlsl")] + ".cso"

    cmd = [
        DXC,
        "-T", target_profile,
        "-E", entry_point,
        "-Fo", output_path,
        filepath,
    ]

    if is_debug:
        cmd += [
            "-Zi",
            "-Od",
            "-Qembed_debug"
        ]
    else:
        cmd += [
            "-O3"
        ]

    return cmd


def get_output_path(filepath: str) -> str:
    if is_hlsl_shader(filepath):
        return filepath[:-len(".hlsl")] + ".cso"

    if is_vk_shader(filepath):
        return filepath + ".spv"

    return filepath


def compile_shader(filepath: str, is_debug: bool) -> bool:
    shader_dir = os.path.dirname(filepath)
    shader_file = os.path.basename(filepath)

    if is_vk_shader(shader_file):
        cmd = build_vk_command(shader_file)
        output_kind = "SPIR-V"
    elif is_hlsl_shader(shader_file):
        cmd = build_hlsl_command(shader_file, is_debug)
        output_kind = "DX12 CSO"
    else:
        return True

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
        print(f"Generated {output_kind}: {get_output_path(filepath)}")

    return True


def main(root_dir: str, is_debug: bool):
    for root, _, files in os.walk(root_dir):
        for file in files:
            if is_vk_shader(file) or is_hlsl_shader(file):
                full_path = os.path.join(root, file)

                success = compile_shader(full_path, is_debug)

                if not success:
                    print("\nBuild stopped due to shader errors.")
                    sys.exit(1)


if __name__ == "__main__":
    directory = sys.argv[1] if len(sys.argv) > 1 else "."
    debug_on = len(sys.argv) > 2 and sys.argv[2].lower() == "debug"

    main(directory, debug_on)
