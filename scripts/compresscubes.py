import argparse
import struct
import numpy as np
import subprocess
import os
import sys
import shutil
from pathlib import Path

from dataclasses import dataclass
from typing import List, Optional


@dataclass
class SceneManifest:
    """Scene manifest data structure"""
    filenames: List[str]
    num_objects: int
    num_noncolliding: int
    noncolliding: List[int]
    num_cubemapmesh: int
    cubemapmesh: List[int]
    num_colliders: int
    colliders: List[int]
    num_concave: int
    concave: List[int]


def get_scene_manifest(folder: str) -> Optional[SceneManifest]:
    """
    Load scene manifest from a folder's manifest.txt file

    Args:
        folder: Path to the folder containing manifest.txt

    Returns:
        SceneManifest object if successful, None if file not found
    """
    # Construct manifest file path
    manifest_path = Path(folder) / "manifest.txt"

    try:
        with open(manifest_path, 'r') as fptr:
            # Read number of objects
            num_objects = int(fptr.readline().strip())

            # Read filenames
            filenames = []
            for _ in range(num_objects):
                filename = fptr.readline().rstrip('\n')
                filenames.append(filename)

            # Read noncolliding indices
            num_noncolliding = int(fptr.readline().strip())
            noncolliding = []
            for _ in range(num_noncolliding):
                noncolliding.append(int(fptr.readline().strip()))

            # Read cubemapmesh indices
            num_cubemapmesh = int(fptr.readline().strip())
            cubemapmesh = []
            for _ in range(num_cubemapmesh):
                cubemapmesh.append(int(fptr.readline().strip()))

            # Read colliders indices
            num_colliders = int(fptr.readline().strip())
            colliders = []
            for _ in range(num_colliders):
                colliders.append(int(fptr.readline().strip()))

            # Read concave indices
            num_concave = int(fptr.readline().strip())
            concave = []
            for _ in range(num_concave):
                concave.append(int(fptr.readline().strip()))

            return SceneManifest(
                filenames=filenames,
                num_objects=num_objects,
                num_noncolliding=num_noncolliding,
                noncolliding=noncolliding,
                num_cubemapmesh=num_cubemapmesh,
                cubemapmesh=cubemapmesh,
                num_colliders=num_colliders,
                colliders=colliders,
                num_concave=num_concave,
                concave=concave
            )

    except FileNotFoundError:
        return None
    except (ValueError, IndexError) as e:
        print(f"Error parsing manifest file: {e}")
        return None


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


# /home/wyearp/venv/bin/python3.13 cwad_to_bc6h.py 0.cwad 512 28
# /home/wyearp/venv/bin/python3.13 ktx_asm3.py packedcubes.ktx 28 512

PYTHON_EXEC = "/home/wyearp/venv/bin/python3.13"

def run_converters(name_of_dir):
    #get number of cubes
    manifest_path = "th1/models/" + name_of_dir
    wad_path = "th1/wad/" + name_of_dir

    cube_output = "th1/wad/" + name_of_dir + "/cubes.ktx"

    manifest = get_scene_manifest(manifest_path)
    n_cubes = manifest.num_cubemapmesh
    res = 512

    wad_fullpath = str(Path(wad_path) / "0.cwad")

    cmd1 = f'"{PYTHON_EXEC}" scripts/cwad_to_bc6h.py "{wad_fullpath}" {res} {n_cubes}'
    run_command(cmd1)
    #print(cmd1)

    cmd2 = f'"{PYTHON_EXEC}" scripts/ktx_asm3.py "{cube_output}" {n_cubes} {res}'
    run_command(cmd2)
    #print(cmd2)

    shutil.rmtree("temp_bc6h_compressed")

    shutil.move("output_distance.bin", "th1/wad/" + name_of_dir + "/distance.bin")

    os.remove("th1/wad/" + name_of_dir + "/0.cwad")




# Example usage
if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description='Compress the cubemaps',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('wadname', help='name of map')

    args = parser.parse_args()

    run_converters(args.wadname)
