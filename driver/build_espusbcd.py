#!/usr/bin/env python3
"""
build_espusbcd.py  -  Build script for ESPUSBCD.SYS DOS CD-ROM driver

Compiles espusbcd.asm into ESPUSBCD.SYS using NASM.
Optionally installs NASM automatically on supported Linux distributions.

Usage:
    python3 build_espusbcd.py                  # build in current directory
    python3 build_espusbcd.py --src /path/to/espusbcd.asm
    python3 build_espusbcd.py --out /path/to/output/ESPUSBCD.SYS
    python3 build_espusbcd.py --install-nasm   # install NASM then build
    python3 build_espusbcd.py --verify         # verify existing SYS without rebuilding
"""

import sys
import os
import subprocess
import shutil
import struct
import argparse

# ─────────────────────────────────────────────
# Constants
# ─────────────────────────────────────────────
DRIVER_NAME     = "ESPUSBCD.SYS"
SOURCE_NAME     = "espusbcd.asm"
MIN_NASM_MAJOR  = 2
MIN_NASM_MINOR  = 14

# Expected values in the compiled device header (offset 0 of .SYS file)
HDR_NEXT        = 0xFFFFFFFF   # no next driver (last in chain)
HDR_ATTR        = 0xC800       # char device | IOCTL | Open/Close
HDR_NAME        = b'ESPUSBCD'  # default device name (8 bytes)
HDR_UNITS       = 1            # number of CD-ROM subunits

# ─────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────

def banner(msg):
    print(f"\n{'='*60}")
    print(f"  {msg}")
    print(f"{'='*60}")

def ok(msg):   print(f"  [OK]  {msg}")
def err(msg):  print(f"  [ERR] {msg}")
def info(msg): print(f"  ...   {msg}")


def run(cmd, **kwargs):
    """Run a shell command, return (returncode, stdout, stderr)."""
    result = subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    return result.returncode, result.stdout.strip(), result.stderr.strip()


def find_nasm():
    """Return path to nasm executable or None."""
    return shutil.which("nasm")


def nasm_version(nasm_path):
    """Return (major, minor) tuple or None on failure."""
    rc, out, _ = run([nasm_path, "--version"])
    if rc != 0:
        return None
    # "NASM version 2.16.01 compiled on ..."
    for token in out.split():
        parts = token.split(".")
        if len(parts) >= 2:
            try:
                return int(parts[0]), int(parts[1])
            except ValueError:
                continue
    return None


def install_nasm():
    """Try to install NASM via dnf or apt."""
    print()
    info("Attempting automatic NASM installation...")
    if shutil.which("dnf"):
        rc, _, _ = run(["sudo", "dnf", "install", "-y", "nasm"])
    elif shutil.which("apt"):
        rc, _, _ = run(["sudo", "apt", "install", "-y", "nasm"])
    elif shutil.which("apt-get"):
        rc, _, _ = run(["sudo", "apt-get", "install", "-y", "nasm"])
    else:
        err("No supported package manager found (dnf / apt).")
        err("Install NASM manually from https://www.nasm.us/ and re-run.")
        return False
    if rc != 0:
        err("Package manager returned an error. Try: sudo dnf install -y nasm")
        return False
    ok("NASM installed.")
    return True


# ─────────────────────────────────────────────
# Verification of compiled binary
# ─────────────────────────────────────────────

def verify_sys(path):
    """
    Verify the compiled .SYS file has a valid DOS device driver header.
    Returns True if all checks pass.
    """
    checks_passed = 0
    checks_total  = 6

    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        err(f"Cannot read {path}: {e}")
        return False

    file_size = len(data)
    info(f"File size: {file_size} bytes")

    if file_size < 22:
        err("File is too small to contain a valid device header (< 22 bytes).")
        return False

    # Unpack fixed header fields
    next_drv, attr, strat_off, intr_off = struct.unpack_from("<IHHH", data, 0)
    dev_name_bytes = data[10:18]
    reserved       = struct.unpack_from("<H", data, 18)[0]
    # byte 20 = first drive letter (set by DOS at runtime, ignore)
    units          = data[21]

    # Check 1: next driver pointer
    if next_drv == HDR_NEXT:
        ok(f"Next driver = 0x{next_drv:08X}  (end of chain ✓)")
        checks_passed += 1
    else:
        err(f"Next driver = 0x{next_drv:08X}  (expected 0x{HDR_NEXT:08X})")

    # Check 2: device attributes
    if attr == HDR_ATTR:
        ok(f"Attributes  = 0x{attr:04X}  (char device | IOCTL | Open/Close ✓)")
        checks_passed += 1
    else:
        err(f"Attributes  = 0x{attr:04X}  (expected 0x{HDR_ATTR:04X})")

    # Check 3: strategy offset is within file
    if 0 < strat_off < file_size:
        ok(f"Strategy offset = 0x{strat_off:04X}  (within file ✓)")
        checks_passed += 1
    else:
        err(f"Strategy offset = 0x{strat_off:04X}  (out of range)")

    # Check 4: interrupt offset is within file
    if 0 < intr_off < file_size:
        ok(f"Interrupt offset = 0x{intr_off:04X}  (within file ✓)")
        checks_passed += 1
    else:
        err(f"Interrupt offset = 0x{intr_off:04X}  (out of range)")

    # Check 5: device name starts with ESPUSBCD
    dev_name_str = dev_name_bytes.decode("ascii", errors="replace")
    if dev_name_bytes == HDR_NAME:
        ok(f"Device name = '{dev_name_str}'  ✓")
        checks_passed += 1
    else:
        err(f"Device name = '{dev_name_str}'  (expected '{HDR_NAME.decode()}')")

    # Check 6: number of subunits
    if units == HDR_UNITS:
        ok(f"Subunits    = {units}  ✓")
        checks_passed += 1
    else:
        err(f"Subunits    = {units}  (expected {HDR_UNITS})")

    print()
    if checks_passed == checks_total:
        ok(f"All {checks_total}/{checks_total} header checks passed.")
        return True
    else:
        err(f"Only {checks_passed}/{checks_total} checks passed.")
        return False


# ─────────────────────────────────────────────
# Main build logic
# ─────────────────────────────────────────────

def build(src_path, out_path, auto_install=False):
    """Compile espusbcd.asm → ESPUSBCD.SYS. Returns True on success."""

    # 1. Check source file
    if not os.path.isfile(src_path):
        err(f"Source file not found: {src_path}")
        err("Make sure espusbcd.asm is in the same directory or use --src.")
        return False
    ok(f"Source: {src_path}")

    # 2. Find / install NASM
    nasm = find_nasm()
    if nasm is None:
        if auto_install:
            if not install_nasm():
                return False
            nasm = find_nasm()
        if nasm is None:
            err("NASM not found in PATH.")
            err("Install with:  sudo dnf install -y nasm   (AlmaLinux/WSL)")
            err("           or: sudo apt install -y nasm   (Ubuntu/Debian)")
            err("           or: download from https://www.nasm.us/")
            return False

    # 3. Check NASM version
    ver = nasm_version(nasm)
    if ver is None:
        err(f"Could not determine NASM version from: {nasm}")
        return False
    major, minor = ver
    if (major, minor) < (MIN_NASM_MAJOR, MIN_NASM_MINOR):
        err(f"NASM {major}.{minor} found but {MIN_NASM_MAJOR}.{MIN_NASM_MINOR}+ required.")
        return False
    ok(f"NASM {major}.{minor} at {nasm}")

    # 4. Compile
    info(f"Compiling: nasm -f bin -o {out_path} {src_path}")
    rc, stdout, stderr = run([nasm, "-f", "bin", "-o", out_path, src_path])
    if rc != 0:
        err("NASM compilation failed:")
        if stdout: print(stdout)
        if stderr: print(stderr)
        return False

    if not os.path.isfile(out_path):
        err("NASM exited cleanly but output file was not created.")
        return False

    size = os.path.getsize(out_path)
    ok(f"Compiled: {out_path}  ({size} bytes)")

    # 5. Verify header
    print()
    info("Verifying device driver header...")
    if not verify_sys(out_path):
        err("Header verification failed. The .SYS file may be corrupted.")
        return False

    return True


# ─────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Build ESPUSBCD.SYS DOS CD-ROM driver from espusbcd.asm",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 build_espusbcd.py
  python3 build_espusbcd.py --install-nasm
  python3 build_espusbcd.py --src ~/project/espusbcd.asm --out ~/output/ESPUSBCD.SYS
  python3 build_espusbcd.py --verify ESPUSBCD.SYS
""")
    parser.add_argument("--src", default=None,
        help=f"Path to source .asm file (default: {SOURCE_NAME} in current directory)")
    parser.add_argument("--out", default=None,
        help=f"Path for output .SYS file (default: {DRIVER_NAME} in current directory)")
    parser.add_argument("--install-nasm", action="store_true",
        help="Automatically install NASM if not found (requires sudo)")
    parser.add_argument("--verify", metavar="SYS_FILE", nargs="?", const="",
        help="Verify an existing .SYS file without rebuilding")
    args = parser.parse_args()

    # Determine paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    src_path   = args.src  if args.src  else os.path.join(script_dir, SOURCE_NAME)
    out_path   = args.out  if args.out  else os.path.join(script_dir, DRIVER_NAME)

    # Verify-only mode
    if args.verify is not None:
        verify_target = args.verify if args.verify else out_path
        banner(f"Verifying {verify_target}")
        success = verify_sys(verify_target)
        sys.exit(0 if success else 1)

    # Full build
    banner(f"Building {DRIVER_NAME}")
    print(f"  Source : {src_path}")
    print(f"  Output : {out_path}")

    success = build(src_path, out_path, auto_install=args.install_nasm)

    print()
    if success:
        banner("BUILD SUCCESSFUL")
        print(f"\n  {DRIVER_NAME} is ready.")
        print(f"\n  Copy to DOS machine and add to CONFIG.SYS:")
        print(f"    DEVICE=ESPUSBCD.SYS /D:ESPUSBCD")
        print(f"\n  Add to AUTOEXEC.BAT:")
        print(f"    LH SHSUCDX.COM /D:ESPUSBCD /Q")
        print()
        sys.exit(0)
    else:
        banner("BUILD FAILED")
        sys.exit(1)


if __name__ == "__main__":
    main()
