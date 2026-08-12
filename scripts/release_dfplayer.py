#!/usr/bin/env python3
"""
scripts/release_dfplayer.py

Builds the project with ESP-IDF and packages the generated .bin files into a zip in release/.
Optionally flashes the device using `idf.py flash` when --flash and --port are provided.

Usage:
  python scripts/release_dfplayer.py [--port /dev/ttyUSB0] [--baud 2000000] [--target esp32s3] [--no-set-target] [--out release] [--flash]

This script assumes `idf.py` is available in PATH and that you've sourced the ESP-IDF environment (export PATH and IDF_PATH) or run the IDF export script beforehand.
"""

import argparse
import subprocess
import sys
import os
import glob
import zipfile
import datetime
import shutil


def run(cmd, cwd=None, check=True):
    print(f">>> {' '.join(cmd)}")
    try:
        subprocess.run(cmd, cwd=cwd, check=check)
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {e}")
        if check:
            sys.exit(e.returncode)


def find_bins(build_dir):
    bins = []
    # common locations
    patterns = [
        os.path.join(build_dir, "*.bin"),
        os.path.join(build_dir, "bootloader", "*.bin"),
        os.path.join(build_dir, "partition_table", "*.bin"),
    ]
    for p in patterns:
        bins.extend(glob.glob(p))
    # also include any nested bins
    for root, _, files in os.walk(build_dir):
        for f in files:
            if f.endswith('.bin'):
                path = os.path.join(root, f)
                if path not in bins:
                    bins.append(path)
    return sorted(set(bins))


def package_bins(bins, out_dir, target):
    os.makedirs(out_dir, exist_ok=True)
    ts = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    name = f"xiaozhi-esp32v1-{target}-{ts}.zip"
    out_path = os.path.join(out_dir, name)
    with zipfile.ZipFile(out_path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        for b in bins:
            arcname = os.path.relpath(b)
            print(f"Adding {b} as {arcname}")
            zf.write(b, arcname)
    print(f"Created package: {out_path}")
    return out_path


def main():
    parser = argparse.ArgumentParser(description='Build and package XiaoZhi firmware (DFPlayer support)')
    parser.add_argument('--port', '-p', help='Serial port for flashing (e.g. /dev/ttyUSB0 or COM3)')
    parser.add_argument('--baud', '-b', default='2000000', help='Baud rate for flashing (default: 2000000)')
    parser.add_argument('--target', default='esp32s3', help='IDF target (default esp32s3)')
    parser.add_argument('--no-set-target', action='store_true', help="Don't run 'idf.py set-target' first")
    parser.add_argument('--out', default='release', help='Output directory for packaged zip')
    parser.add_argument('--flash', action='store_true', help='Run idf.py flash after build (requires --port)')
    parser.add_argument('--build-only', action='store_true', help='Only build, do not package or flash')
    parser.add_argument('--clean', action='store_true', help='Do a clean build (idf.py fullclean)')
    args = parser.parse_args()

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_dir = os.path.join(repo_root, 'build')

    # Quick checks
    if shutil.which('idf.py') is None:
        print("Error: idf.py not found in PATH. Please source the ESP-IDF export script before running this.")
        sys.exit(1)

    if not args.no_set_target:
        run(['idf.py', 'set-target', args.target], cwd=repo_root)

    if args.clean:
        run(['idf.py', 'fullclean'], cwd=repo_root)

    # Build
    run(['idf.py', 'build'], cwd=repo_root)

    if args.build_only:
        print('Build finished. Exiting due to --build-only.')
        return

    # Find bins
    bins = find_bins(build_dir)
    if not bins:
        print('No .bin files found in build/. Aborting packaging.')
        sys.exit(1)

    # Package bins
    package = package_bins(bins, args.out, args.target)

    # Flash if requested
    if args.flash:
        if not args.port:
            print('Error: --flash requires --port to be specified')
            sys.exit(1)
        # Use idf.py flash which handles partition/offsets automatically
        run(['idf.py', '-p', args.port, '-b', str(args.baud), 'flash'], cwd=repo_root)
        print('Flashing finished. You can use idf.py monitor to view logs.')

if __name__ == '__main__':
    main()
