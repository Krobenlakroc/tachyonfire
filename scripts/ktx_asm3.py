import os
from pathlib import Path
import argparse
import struct

# def iterate_ktx_files(base_dir, num_mips, num_layers):
#     """
#     Iterate over KTX files in order: mip -> layer -> face
#
#     Args:
#         base_dir: Base directory containing mip folders (e.g., 'temp_bc6h_compressed')
#         num_mips: Number of mip levels (0 to num_mips-1)
#         num_layers: Number of layers (0 to num_layers-1)
#     """
#     base_path = Path(base_dir)
#
#     for mip in range(num_mips):
#         mip_dir = base_path / f"mip{mip}"
#
#         if not mip_dir.exists():
#             print(f"Warning: {mip_dir} does not exist")
#             continue
#
#         for layer in range(num_layers):
#             for face in range(6):  # Always 0-5 for cube faces
#                 filename = f"layer{layer:02d}_face{face}.ktx"
#                 filepath = mip_dir / filename
#
#                 if filepath.exists():
#                     print(f"Processing: {filepath}")
#                     # Do your processing here
#                     # For example: process_file(filepath)
#                 else:
#                     print(f"Warning: {filepath} does not exist")



class KTX1File:
    KTX_IDENTIFIER = b'\xABKTX 11\xBB\r\n\x1A\n'

    def __init__(self):
        self.header = None
        self.key_value_data = b''
        self.mipmap_data = []

    def read(self, filepath):
        """Read a KTX1 file"""
        with open(filepath, 'rb') as f:
            # Read identifier
            identifier = f.read(12)
            if identifier != self.KTX_IDENTIFIER:
                raise ValueError(f"Not a valid KTX file: {filepath}")

            # Read header (64 bytes total, 12 already read)
            header_data = f.read(52)
            self.header = struct.unpack('<IIIIIIIIIIIII', header_data)

            # Unpack header fields
            (self.endianness, self.gl_type, self.gl_type_size, self.gl_format,
             self.gl_internal_format, self.gl_base_internal_format,
             self.pixel_width, self.pixel_height, self.pixel_depth,
             self.array_elements, self.faces, self.mipmap_levels,
             self.bytes_key_value_data) = self.header

            # Read key-value data
            self.key_value_data = f.read(self.bytes_key_value_data)

            # Read mipmap data
            self.mipmap_data = []
            for mip in range(max(1, self.mipmap_levels)):
                image_size = struct.unpack('<I', f.read(4))[0]
                image_data = f.read(image_size)

                # Read padding (mip levels are padded to 4-byte alignment)
                padding = (4 - (image_size % 4)) % 4
                if padding:
                    f.read(padding)

                self.mipmap_data.append(image_data)

            return self

    def write(self, filepath):
        """Write a KTX1 file"""
        with open(filepath, 'wb') as f:
            # Write identifier
            f.write(self.KTX_IDENTIFIER)

            # Write header
            header_data = struct.pack('<IIIIIIIIIIIII',
                self.endianness, self.gl_type, self.gl_type_size, self.gl_format,
                self.gl_internal_format, self.gl_base_internal_format,
                self.pixel_width, self.pixel_height, self.pixel_depth,
                self.array_elements, self.faces, self.mipmap_levels,
                self.bytes_key_value_data)
            f.write(header_data)

            # Write key-value data
            f.write(self.key_value_data)

            # Write mipmap data
            for image_data in self.mipmap_data:
                image_size = len(image_data)
                f.write(struct.pack('<I', image_size))
                f.write(image_data)

                # Write padding
                padding = (4 - (image_size % 4)) % 4
                if padding:
                    f.write(b'\x00' * padding)


def combine_ktx_files(base_dir, num_mips, num_layers, output_file):
    """
    Combine individual KTX face files into one master KTX file

    Args:
        base_dir: Base directory containing mip folders
        num_mips: Number of mip levels
        num_layers: Number of array layers
        output_file: Output KTX file path
    """
    base_path = Path(base_dir)

    # Read the first file to get header information
    first_file = base_path / "mip0" / "layer00_face0.ktx"
    first_ktx = KTX1File().read(first_file)

    # Create master KTX file
    master = KTX1File()
    master.endianness = first_ktx.endianness
    master.gl_type = first_ktx.gl_type
    master.gl_type_size = first_ktx.gl_type_size
    master.gl_format = first_ktx.gl_format
    master.gl_internal_format = first_ktx.gl_internal_format
    master.gl_base_internal_format = first_ktx.gl_base_internal_format
    master.pixel_width = first_ktx.pixel_width
    master.pixel_height = first_ktx.pixel_height
    master.pixel_depth = first_ktx.pixel_depth
    master.array_elements = num_layers if num_layers > 1 else 0
    master.faces = 6  # Cube map
    master.mipmap_levels = num_mips
    master.key_value_data = first_ktx.key_value_data
    master.bytes_key_value_data = len(master.key_value_data)

    # Collect all face data for each mip level
    master.mipmap_data = []
    master.sizes = []

    for mip in range(num_mips):
        mip_dir = base_path / f"mip{mip}"
        combined_mip_data = b''
        sz = 0
        for layer in range(num_layers):
            for face in range(6):
                filename = f"layer{layer:02d}_face{face}.ktx"
                filepath = mip_dir / filename

                if not filepath.exists():
                    raise FileNotFoundError(f"Missing file: {filepath}")

                # Read the face file
                face_ktx = KTX1File().read(filepath)

                # Append the image data (should be just one mip level per file)
                combined_mip_data += face_ktx.mipmap_data[0]
                #print(len(face_ktx.mipmap_data[0]))

                print(f"Added: {filepath}")
        #print(len(combined_mip_data))
        master.mipmap_data.append(combined_mip_data)


    # Write the combined file
    master.write(output_file)
    print(f"\nSuccessfully created: {output_file}")
    print(f"  Mip levels: {num_mips}")
    print(f"  Array layers: {num_layers}")
    print(f"  Faces: 6 (cube map)")

# Example usage
if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description='Assemble BC6H DDS files into KTX v1 cubemap array',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('output_ktx', help='Output KTX v1 file path')
    parser.add_argument('layers', type=int, help='Number of layers in the cubemap array')
    parser.add_argument('base_resolution', type=int, help='Base resolution (e.g., 512)')

    args = parser.parse_args()

    base_directory = "temp_bc6h_compressed"
    num_mips = 5      # Set your number of mip levels
    num_layers = args.layers    # Set your number of layers

    # iterate_ktx_files(base_directory, num_mips, num_layers)

    combine_ktx_files(base_directory,num_mips,num_layers,args.output_ktx)
