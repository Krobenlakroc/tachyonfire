#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path
import argparse

# Configuration
COMPRESSONATOR_CLI = "/home/wyearp/src/compresstool/compressonatorcli-4.5.52-Linux/compressonatorcli"

def run_command(cmd, cwd=None):
    """Run a shell command and handle errors."""
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            check=True,
            cwd=cwd,
            capture_output=True,
            text=True
        )
        print(f"✓ {cmd}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"✗ Error running: {cmd}")
        print(f"  {e.stderr}")
        return False

def process_standard_format(subdir, output_subdir):
    """Process standard format: albedo.png, metalness.png, roughness.png, normal.png, displacement.png"""
    print(f"Format: Standard (albedo.png, metalness.png, etc.)")

    # Define file paths
    albedo = subdir / "albedo.png"
    metalness = subdir / "metalness.png"
    roughness = subdir / "roughness.png"
    normal = subdir / "normal.png"
    displacement = subdir / "displacement.png"

    # Check required files
    required_files = [albedo, metalness, roughness, normal]
    missing_files = [f.name for f in required_files if not f.exists()]

    if missing_files:
        print(f"⚠ Missing files: {', '.join(missing_files)}")
        return False

    # Step 1: Create albedometal.png using ImageMagick convert
    albedometal_temp = subdir / "albedometal.png"
    cmd = f'convert "{albedo}" "{metalness}" -alpha off -compose copy-opacity -composite "{albedometal_temp}"'
    if not run_command(cmd):
        return False

    # Step 2: Compress albedometal to BC7
    albedometal_output = output_subdir / "albedometal.ktx"
    cmd = f'"{COMPRESSONATOR_CLI}" -fd BC7 -Quality 0.4 -mipsize 1 "{albedometal_temp}" "{albedometal_output}"'
    run_command(cmd)

    # Clean up temporary albedometal.png
    if albedometal_temp.exists():
        albedometal_temp.unlink()

    # Step 3: Compress roughness to BC4
    roughness_output = output_subdir / "roughness.ktx"
    cmd = f'"{COMPRESSONATOR_CLI}" -fd BC4 -Quality 0.4 -mipsize 1 "{roughness}" "{roughness_output}"'
    run_command(cmd)

    # Step 4: Compress normal to BC5
    normal_output = output_subdir / "normal.ktx"
    cmd = f'"{COMPRESSONATOR_CLI}" -fd BC5 -Quality 0.4 -mipsize 1 "{normal}" "{normal_output}"'
    run_command(cmd)

    # Step 5: Compress displacement to BC4 (only if it exists)
    if displacement.exists():
        displacement_output = output_subdir / "displacement.ktx"
        cmd = f'"{COMPRESSONATOR_CLI}" -fd BC4 -Quality 0.4 -mipsize 1 "{displacement}" "{displacement_output}"'
        run_command(cmd)
        print(f"✓ Processed displacement map")
    else:
        print(f"ℹ No displacement map found (optional)")

    return True

def process_numbered_format(subdir, output_subdir):
    """Process numbered format: 0.png, 1.png, etc. with metalness.png, roughness.png, normal.png"""
    print(f"Format: Numbered (0.png, 1.png, etc.)")

    # Check for shared maps
    metalness = subdir / "metalness.png"
    roughness = subdir / "roughness.png"
    normal = subdir / "normal.png"

    required_shared = [metalness, roughness, normal]
    missing_shared = [f.name for f in required_shared if not f.exists()]

    if missing_shared:
        print(f"⚠ Missing shared files: {', '.join(missing_shared)}")
        return False

    # Find all numbered albedo files (0.png, 1.png, etc.)
    numbered_files = sorted([f for f in subdir.glob("*.png")
                            if f.stem.isdigit() and not f.stem.endswith("_d")])

    if not numbered_files:
        print(f"⚠ No numbered PNG files found (0.png, 1.png, etc.)")
        return False

    print(f"Found {len(numbered_files)} numbered albedo textures")

    # Process roughness (once)
    roughness_output = output_subdir / "roughness.ktx"
    cmd = f'"{COMPRESSONATOR_CLI}" -fd BC4 -Quality 0.4 -mipsize 1 "{roughness}" "{roughness_output}"'
    run_command(cmd)

    # Process normal (once)
    normal_output = output_subdir / "normal.ktx"
    cmd = f'"{COMPRESSONATOR_CLI}" -fd BC5 -Quality 0.4 -mipsize 1 "{normal}" "{normal_output}"'
    run_command(cmd)

    # Process each numbered albedo + metalness
    for albedo_file in numbered_files:
        number = albedo_file.stem
        print(f"\n  Processing texture {number}...")

        # Create albedometal for this number
        albedometal_temp = subdir / f"albedometal_{number}.png"
        cmd = f'convert "{albedo_file}" "{metalness}" -alpha off -compose copy-opacity -composite "{albedometal_temp}"'
        if not run_command(cmd):
            continue

        # Compress albedometal
        albedometal_output = output_subdir / f"{number}.ktx"
        cmd = f'"{COMPRESSONATOR_CLI}" -fd BC7 -Quality 0.4 -mipsize 1 "{albedometal_temp}" "{albedometal_output}"'
        run_command(cmd)

        # Clean up temporary file
        if albedometal_temp.exists():
            albedometal_temp.unlink()

        # Check for corresponding displacement file (n_d.png)
        displacement_file = subdir / f"{number}_d.png"
        if displacement_file.exists():
            displacement_output = output_subdir / f"{number}_d.ktx"
            cmd = f'"{COMPRESSONATOR_CLI}" -fd BC4 -Quality 0.4 -mipsize 1 "{displacement_file}" "{displacement_output}"'
            run_command(cmd)
            print(f"  ✓ Processed displacement_{number}")
        else:
            print(f"  ℹ No displacement_{number} found (optional)")

    return True

def detect_and_process_directory(subdir, output_subdir):
    """Detect format and process accordingly."""
    # Check if this is standard format (has albedo.png)
    if (subdir / "albedo.png").exists():
        #return True
        return process_standard_format(subdir, output_subdir)

    # Check if this is numbered format (has metalness.png + numbered files)
    elif (subdir / "metalness.png").exists():
        return process_numbered_format(subdir, output_subdir)

    else:
        print(f"⚠ Unknown format - no albedo.png or numbered format detected")
        return False

def process_texture_directory(textures_dir, compressed_dir,oneshot):
    """Process all texture subdirectories."""
    textures_path = Path(textures_dir)
    compressed_path = Path(compressed_dir)

    # Create compressed directory if it doesn't exist
    compressed_path.mkdir(exist_ok=True)

    # Iterate over all subdirectories in textures/
    for subdir in textures_path.iterdir():
        if oneshot:
            if subdir.name != oneshot:
                continue

        if not subdir.is_dir():
            continue

        print(f"\n{'='*60}")
        print(f"Processing: {subdir.name}")
        print(f"{'='*60}")

        # Create corresponding subdirectory in compressed/
        output_subdir = compressed_path / subdir.name
        output_subdir.mkdir(exist_ok=True)

        # Detect format and process
        success = detect_and_process_directory(subdir, output_subdir)

        if success:
            print(f"\n✓ Completed: {subdir.name}")
        else:
            print(f"\n✗ Failed or skipped: {subdir.name}")

def main():
    # Verify compressonatorcli exists
    if not Path(COMPRESSONATOR_CLI).exists():
        print(f"Error: Compressonator CLI not found at: {COMPRESSONATOR_CLI}")
        print("Please update the COMPRESSONATOR_CLI variable at the top of the script.")
        return


    parser = argparse.ArgumentParser(
        description='Compress the cubemaps',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('--oneshot',default = None, help='folder to use')

    args = parser.parse_args()

    # Get the script's directory as the base
    script_dir = Path(__file__).resolve().parents[1]

    textures_dir = script_dir / "textures"
    compressed_dir = script_dir / "compressed"

    # Verify textures directory exists
    if not textures_dir.exists():
        print(f"Error: '{textures_dir}' directory not found!")
        print(f"Please create it at: {textures_dir}")
        return

    print(f"Compressonator CLI: {COMPRESSONATOR_CLI}")
    print(f"Textures directory: {textures_dir}")
    print(f"Compressed output: {compressed_dir}")

    process_texture_directory(textures_dir, compressed_dir,args.oneshot)

    print(f"\n{'='*60}")
    print("Processing complete!")
    print(f"{'='*60}")

if __name__ == "__main__":
    main()

