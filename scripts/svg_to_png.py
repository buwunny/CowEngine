#!/usr/bin/env python3
"""
Convert SVG files to PNG format.
"""

import os
import sys
from pathlib import Path
from cairosvg import svg2png


def convert_svg_to_png(svg_file, output_dir=None, dpi=96):
    """
    Convert a single SVG file to PNG.
    
    Args:
        svg_file: Path to the SVG file
        output_dir: Directory to save PNG (defaults to same directory as SVG)
        dpi: DPI for output PNG (default 96)
    """
    svg_path = Path(svg_file)
    
    if not svg_path.exists():
        print(f"Error: {svg_file} not found")
        return False
    
    if not svg_path.suffix.lower() == '.svg':
        print(f"Error: {svg_file} is not an SVG file")
        return False
    
    # Set output directory
    if output_dir is None:
        output_dir = svg_path.parent
    else:
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
    
    # Create output filename
    png_file = output_dir / f"{svg_path.stem}.png"
    
    try:
        svg2png(url=str(svg_path), write_to=str(png_file), dpi=dpi)
        print(f"✓ Converted: {svg_path.name} → {png_file.name}")
        return True
    except Exception as e:
        print(f"✗ Error converting {svg_path.name}: {e}")
        return False


def batch_convert(input_dir, output_dir=None, dpi=96):
    """
    Convert all SVG files in a directory to PNG.
    
    Args:
        input_dir: Directory containing SVG files
        output_dir: Directory to save PNG files (defaults to input_dir)
        dpi: DPI for output PNG (default 96)
    """
    input_path = Path(input_dir)
    
    if not input_path.is_dir():
        print(f"Error: {input_dir} is not a directory")
        return
    
    svg_files = list(input_path.glob('*.svg'))
    
    if not svg_files:
        print(f"No SVG files found in {input_dir}")
        return
    
    if output_dir is None:
        output_dir = input_path
    
    print(f"Converting {len(svg_files)} SVG file(s) from {input_dir}...")
    
    for svg_file in svg_files:
        convert_svg_to_png(svg_file, output_dir, dpi)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage:")
        print("  Single file:  python svg_to_png.py <svg_file> [output_dir] [dpi]")
        print("  Batch:        python svg_to_png.py --batch <input_dir> [output_dir] [dpi]")
        sys.exit(1)
    
    if sys.argv[1] == '--batch':
        input_dir = sys.argv[2] if len(sys.argv) > 2 else '.'
        output_dir = sys.argv[3] if len(sys.argv) > 3 else None
        dpi = int(sys.argv[4]) if len(sys.argv) > 4 else 96
        batch_convert(input_dir, output_dir, dpi)
    else:
        svg_file = sys.argv[1]
        output_dir = sys.argv[2] if len(sys.argv) > 2 else None
        dpi = int(sys.argv[3]) if len(sys.argv) > 3 else 96
        convert_svg_to_png(svg_file, output_dir, dpi)
