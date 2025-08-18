from PIL import Image
import struct
import sys
import os
import glob

def convert_png_to_bmp_rgb565(input_path, output_path):
    try:
        # Open the PNG image
        img = Image.open(input_path)
        
        # Handle transparency or non-RGB modes
        if img.mode == 'RGBA':
            background = Image.new('RGB', img.size, (255, 255, 255))  # White background
            background.paste(img, mask=img.split()[3])
            img = background
        elif img.mode != 'RGB':
            img = img.convert('RGB')
        
        # Resize to 170x320 for portrait mode
        img = img.resize((170, 320))
        
        # Get image dimensions
        width, height = img.size
        
        # Calculate sizes
        pixel_data_size = width * height * 2
        file_size = 14 + 40 + 12 + pixel_data_size
        
        # BMP file header (14 bytes)
        bmp_header = bytearray()
        bmp_header.extend(b'BM')
        bmp_header.extend(struct.pack('<I', file_size))
        bmp_header.extend(struct.pack('<H', 0))
        bmp_header.extend(struct.pack('<H', 0))
        bmp_header.extend(struct.pack('<I', 54 + 12))
        
        # DIB header (40 + 12 bytes)
        bmp_header.extend(struct.pack('<I', 40))
        bmp_header.extend(struct.pack('<i', width))
        bmp_header.extend(struct.pack('<i', -height))  # Top-down
        bmp_header.extend(struct.pack('<H', 1))
        bmp_header.extend(struct.pack('<H', 16))
        bmp_header.extend(struct.pack('<I', 3))  # BI_BITFIELDS
        bmp_header.extend(struct.pack('<I', pixel_data_size))
        bmp_header.extend(struct.pack('<i', 2835))  # 72 DPI
        bmp_header.extend(struct.pack('<i', 2835))
        bmp_header.extend(struct.pack('<I', 0))
        bmp_header.extend(struct.pack('<I', 0))
        
        # RGB565 bitfield masks
        bmp_header.extend(struct.pack('<I', 0xF800))  # Red
        bmp_header.extend(struct.pack('<I', 0x07E0))  # Green
        bmp_header.extend(struct.pack('<I', 0x001F))  # Blue
        
        # Convert pixels to RGB565
        pixel_data = bytearray()
        pixels = img.load()
        for y in range(height):
            for x in range(width):
                r, g, b = pixels[x, y]
                r = (r * 31 // 255) & 0x1F
                g = (g * 63 // 255) & 0x3F
                b = (b * 31 // 255) & 0x1F
                pixel = (r << 11) | (g << 5) | b
                pixel_data.extend(struct.pack('>H', pixel))  # Big-endian
        
        # Write to file
        with open(output_path, 'wb') as f:
            f.write(bmp_header)
            f.write(pixel_data)
        
        print(f"Converted {input_path} to {output_path}")
        
    except Exception as e:
        print(f"Error converting {input_path}: {str(e)}")

def batch_convert_pngs(input_dir, output_dir):
    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)
    
    # Find all PNG files in input directory
    png_files = glob.glob(os.path.join(input_dir, '*.png'))
    
    if not png_files:
        print(f"No PNG files found in {input_dir}")
        sys.exit(1)
    
    print(f"Found {len(png_files)} PNG files to convert")
    
    # Convert each PNG
    for input_path in png_files:
        # Generate output path (same filename, .bmp extension)
        filename = os.path.basename(input_path)
        output_path = os.path.join(output_dir, os.path.splitext(filename)[0] + '.bmp')
        convert_png_to_bmp_rgb565(input_path, output_path)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py input_directory output_directory")
        sys.exit(1)
    
    input_dir = sys.argv[1]
    output_dir = sys.argv[2]
    batch_convert_pngs(input_dir, output_dir)