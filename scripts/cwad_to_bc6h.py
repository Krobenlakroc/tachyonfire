#!/usr/bin/env python3
"""
CWAD to BC6H Converter
Extracts cubemap data from .cwad files and compresses color data to BC6H using Compressonator.

This version uses EXR as an intermediary format for HDR color data.

Requirements:
    - numpy
    - OpenEXR (pip install openexr)
    - Compressonator CLI (https://github.com/GPUOpen-Tools/compressonator)

Usage:
    python cwad_to_bc6h.py <input.cwad> <resolution> <layers> [options]
"""

import argparse
import struct
import numpy as np
import subprocess
import os
import sys
import shutil
from pathlib import Path

COMPRESSONATOR_CLI = "/home/wyearp/src/compresstool/compressonatorcli-4.5.52-Linux/compressonatorcli"

def read_cwad(filepath, resolution, layers):
    """
    Read a .cwad file and extract all data.
    
    Format:
        - minmaxdata: 6 floats per layer (6 * layers floats)
        - wad_data: distance field (6 * resolution * resolution * layers floats)
        - color_data: 5 mip levels, each (6 * mip_res * mip_res * layers * 3 floats)
    
    Returns:
        minmaxdata: (layers, 6) array
        wad_data: (layers, 6, resolution, resolution) array
        color_data: list of 5 arrays, each (layers, 6, mip_res, mip_res, 3)
    """
    with open(filepath, 'rb') as f:
        # Read minmaxdata
        minmax_count = 6 * layers
        minmax_bytes = f.read(minmax_count * 4)
        minmaxdata = np.frombuffer(minmax_bytes, dtype=np.float32).reshape(layers, 6)
        
        # Read wad_data (distance field)
        wad_count = 6 * resolution * resolution * layers
        wad_bytes = f.read(wad_count * 4)
        wad_data = np.frombuffer(wad_bytes, dtype=np.float32).reshape(layers, 6, resolution, resolution)
        
        # Read color_data for 5 mip levels
        color_data = []
        for mip_level in range(5):
            mip_res = resolution // (2 ** mip_level)
            color_count = 6 * mip_res * mip_res * layers * 3
            color_bytes = f.read(color_count * 4)
            color_array = np.frombuffer(color_bytes, dtype=np.float32).reshape(
                layers, 6, mip_res, mip_res, 3
            )

            color_array = np.nan_to_num(color_array, copy=True, nan=0.0)

            color_data.append(color_array)
        
        return minmaxdata, wad_data, color_data


def write_distance_file(filepath, minmaxdata, wad_data):
    """Write distance field data to binary file."""
    with open(filepath, 'wb') as f:
        f.write(minmaxdata.astype(np.float32).tobytes())
        f.write(wad_data.astype(np.float32).tobytes())
    
    file_size_mb = os.path.getsize(filepath) / (1024 * 1024)
    print(f"✓ Distance field written: {filepath} ({file_size_mb:.2f} MB)")


def save_face_as_exr(face_data, filepath):
    """
    Save a single face (height, width, 3) as an EXR file.
    Uses OpenEXR library for proper HDR support.
    """
    try:
        import OpenEXR
        import Imath
    except ImportError:
        print("Error: OpenEXR module not found.")
        print("Install with: pip install openexr")
        sys.exit(1)
    
    height, width, channels = face_data.shape
    assert channels == 3, "Expected RGB data"
    
    # OpenEXR expects separate channel buffers
    r = face_data[:, :, 0].astype(np.float32).tobytes()
    g = face_data[:, :, 1].astype(np.float32).tobytes()
    b = face_data[:, :, 2].astype(np.float32).tobytes()
    
    # Create header
    header = OpenEXR.Header(width, height)
    header['channels'] = {
        'R': Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT)),
        'G': Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT)),
        'B': Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT))
    }
    
    # Write file
    exr = OpenEXR.OutputFile(filepath, header)
    exr.writePixels({'R': r, 'G': g, 'B': b})
    exr.close()


def export_color_data_to_exr(color_data, output_dir):
    """
    Export all color data as EXR files.
    
    Directory structure:
        output_dir/
            mip0/
                layer0_face0.exr
                layer0_face1.exr
                ...
                layer0_face5.exr
                layer1_face0.exr
                ...
            mip1/
            ...
    
    Returns list of directories for each mip level.
    """
    os.makedirs(output_dir, exist_ok=True)
    mip_dirs = []
    
    for mip_level, mip_data in enumerate(color_data):
        mip_dir = os.path.join(output_dir, f"mip{mip_level}")
        os.makedirs(mip_dir, exist_ok=True)
        
        layers_count, faces, height, width, channels = mip_data.shape
        
        print(f"  Exporting mip {mip_level} ({width}x{height}): ", end="", flush=True)
        
        for layer_idx in range(layers_count):
            for face_idx in range(faces):
                face_data = mip_data[layer_idx, face_idx, :, :, :]
                filename = f"layer{layer_idx:02d}_face{face_idx}.exr"
                filepath = os.path.join(mip_dir, filename)
                save_face_as_exr(face_data, filepath)
        
        print(f"{layers_count * faces} faces")
        mip_dirs.append(mip_dir)
    
    return mip_dirs


def find_compressonator():
    """Find Compressonator CLI executable."""
    return "/home/wyearp/src/compresstool/compressonatorcli-4.5.52-Linux/compressonatorcli"

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

def compress_exr_to_bc6h(exr_path, output_path, compressonator_cli):
    """
    Compress a single EXR file to BC6H format.
    Returns True on success, False on failure.
    """
#     cmd = [
#         compressonator_cli,
#         "-fd", "BC6H",  # Format: BC6H unsigned
#         exr_path,
#         output_path
#     ]
#
#     try:
#         result = subprocess.run(
#             cmd,
#             capture_output=True,
#             text=True,
#             check=False
#         )
#         return result.returncode == 0
#     except Exception as e:
#         print(f"Error compressing {exr_path}: {e}")
#         return False

    cmd = f'"{COMPRESSONATOR_CLI}" -fd BC6H -Quality 0.4 "{exr_path}" "{output_path}"'
    run_command(cmd)
    return True


def assemble_ktx_cubemap_array(mip_dirs, output_ktx, layers, resolution, compressonator_cli):
    """
    Compress all EXR files to BC6H and assemble into a KTX v1 cubemap array.
    
    This is the complex part - we need to:
    1. Compress each EXR face to BC6H
    2. Assemble all faces, layers, and mips into a single KTX file
    """
    print("\nCompressing to BC6H format...")
    
    compressed_dir = "temp_bc6h_compressed"
    os.makedirs(compressed_dir, exist_ok=True)
    
    # Compress each mip level
    for mip_level, mip_dir in enumerate(mip_dirs):
        mip_res = resolution // (2 ** mip_level)
        mip_compressed_dir = os.path.join(compressed_dir, f"mip{mip_level}")
        os.makedirs(mip_compressed_dir, exist_ok=True)
        
        print(f"  Compressing mip {mip_level} ({mip_res}x{mip_res})...", end="", flush=True)
        
        # Get all EXR files in this mip level
        exr_files = sorted(Path(mip_dir).glob("*.exr"))
        
        compressed_count = 0
        for exr_file in exr_files:
            output_file = os.path.join(mip_compressed_dir, exr_file.stem + ".ktx")
            if compress_exr_to_bc6h(str(exr_file), output_file, compressonator_cli):
                compressed_count += 1
        
        print(f" {compressed_count}/{len(exr_files)} files compressed")
    
    print(f"\n✓ BC6H compressed files in: {compressed_dir}/")
    print(f"\nNote: Final KTX v1 cubemap array assembly requires additional tooling.")
    print(f"Options:")
    print(f"  1. Use PVRTexTool to assemble DDS files into KTX cubemap array")
    print(f"  2. Use custom KTX assembly script (ktx_assembler.py)")
    print(f"  3. Use DirectXTex's texconv or texassemble")
    
    return compressed_dir


def create_assembly_script(compressed_dir, output_ktx, layers, resolution):
    """
    Create a helper script showing how to assemble the final KTX v1 file.
    """
    script_path = "assemble_ktx.sh"
    
    with open(script_path, 'w') as f:
        f.write("#!/bin/bash\n")
        f.write("# KTX v1 Cubemap Array Assembly Script\n")
        f.write("# This script shows how to assemble compressed BC6H faces into a KTX v1 file\n\n")
        f.write(f"COMPRESSED_DIR=\"{compressed_dir}\"\n")
        f.write(f"OUTPUT_KTX=\"{output_ktx}\"\n")
        f.write(f"LAYERS={layers}\n")
        f.write(f"RESOLUTION={resolution}\n\n")
        f.write("# Use the ktx_assembler.py script:\n")
        f.write("python ktx_assembler.py $COMPRESSED_DIR $OUTPUT_KTX $LAYERS $RESOLUTION\n\n")
        f.write("# Alternative: Use PVRTexTool (if available):\n")
        f.write("# PVRTexToolCLI -i $COMPRESSED_DIR/mip0/*.dds -array -cube -flip y -o $OUTPUT_KTX\n\n")
        f.write("echo \"KTX v1 assembly complete!\"\n")
    
    os.chmod(script_path, 0o755)
    print(f"✓ Assembly helper script created: {script_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Convert CWAD cubemap files to BC6H compressed KTX',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python cwad_to_bc6h.py input.cwad 512 16
  python cwad_to_bc6h.py input.cwad 512 16 --output-prefix mydata --keep-exr

The script will:
  1. Extract distance field data to a separate .bin file
  2. Export color data as EXR files (HDR intermediate format)
  3. Compress EXR files to BC6H using Compressonator
  4. Provide instructions for final KTX v1 assembly
        """
    )
    
    parser.add_argument('input_cwad', help='Input .cwad file path')
    parser.add_argument('resolution', type=int, help='Base resolution (e.g., 512)')
    parser.add_argument('layers', type=int, help='Number of layers')
    parser.add_argument('--output-prefix', default='output', help='Output file prefix')
    parser.add_argument('--keep-exr', action='store_true', help='Keep intermediate EXR files')
    parser.add_argument('--compressonator', help='Path to Compressonator CLI')
    
    args = parser.parse_args()
    
    # Validate input
    if not os.path.exists(args.input_cwad):
        print(f"Error: Input file '{args.input_cwad}' not found.")
        sys.exit(1)
    
    # Find Compressonator
    if args.compressonator:
        compressonator_cli = args.compressonator
    else:
        compressonator_cli = find_compressonator()
    
    if not compressonator_cli:
        print("Error: Compressonator CLI not found.")
        print("Please install from: https://github.com/GPUOpen-Tools/compressonator")
        print("Or specify path with --compressonator option")
        sys.exit(1)
    
    print(f"Using Compressonator: {compressonator_cli}")
    
    # Print configuration
    print("\n" + "="*60)
    print("CWAD to BC6H Converter")
    print("="*60)
    print(f"Input:      {args.input_cwad}")
    print(f"Resolution: {args.resolution}")
    print(f"Layers:     {args.layers}")
    print(f"Output:     {args.output_prefix}_*")
    print("="*60 + "\n")
    
    # Step 1: Read CWAD
    print("[1/4] Reading CWAD file...")
    try:
        minmaxdata, wad_data, color_data = read_cwad(
            args.input_cwad, 
            args.resolution, 
            args.layers
        )
        print(f"✓ CWAD file loaded successfully")
        print(f"  - Distance field: {wad_data.shape}")
        print(f"  - Color mip levels: {len(color_data)}")
    except Exception as e:
        print(f"✗ Error reading CWAD: {e}")
        sys.exit(1)
    
    # Step 2: Write distance file
    print("\n[2/4] Writing distance field...")
    distance_output = f"{args.output_prefix}_distance.bin"
    write_distance_file(distance_output, minmaxdata, wad_data)
    
    # Step 3: Export color to EXR
    print("\n[3/4] Exporting color data to EXR...")
    exr_dir = f"{args.output_prefix}_exr_temp"
    mip_dirs = export_color_data_to_exr(color_data, exr_dir)
    print(f"✓ EXR files exported to: {exr_dir}/")
    
    # Step 4: Compress to BC6H
    print("\n[4/4] Compressing to BC6H...")
    compressed_dir = assemble_ktx_cubemap_array(
        mip_dirs,
        f"{args.output_prefix}_color.ktx",
        args.layers,
        args.resolution,
        compressonator_cli
    )
    
    # # Create assembly helper
    # create_assembly_script(
    #     compressed_dir,
    #     f"{args.output_prefix}_color.ktx",
    #     args.layers,
    #     args.resolution
    # )
    
    # Cleanup
    if not args.keep_exr:
        print(f"\nCleaning up intermediate EXR files...")
        shutil.rmtree(exr_dir)
        print(f"✓ Removed: {exr_dir}/")
    
    # Summary
    print("\n" + "="*60)
    print("PROCESSING COMPLETE")
    print("="*60)
    print(f"✓ Distance field:    {distance_output}")
    print(f"✓ BC6H compressed:   {compressed_dir}/")
    print(f"✓ Assembly script:   assemble_ktx.sh")
    if args.keep_exr:
        print(f"✓ EXR files kept:    {exr_dir}/")
    print("="*60)
    print("\nNext: Run the assembly script to create final KTX v1 file")


if __name__ == "__main__":
    main()
