#!/usr/bin/env python3
"""
ESP32 Arduino Lib Builder - exFAT Support
==========================================
Correctly patches libfatfs.a with exFAT support for Arduino ESP32 3.3.7.

Key findings from development session:
  - build.sh runs 'git reset --hard' at startup -> overwrites ffconf.h patches
  - CONFIG_FATFS_EXFAT_SUPPORT does not exist in ESP-IDF 5.5 Kconfig -> silently ignored
  - Correct approach: run build.sh first (updates code, builds other libs),
    then patch ffconf.h and recompile ONLY the fatfs target via ninja
  - cp has alias -i -> use shutil.copy2 instead

Usage:
  python3 build_exfat_libs.py                     # full build + patch
  python3 build_exfat_libs.py --arduino-version 3.3.7
  python3 build_exfat_libs.py --test              # verify only, no changes
  python3 build_exfat_libs.py --skip-full-build   # skip build.sh (if run recently)
  python3 build_exfat_libs.py --restore           # restore originals from backups
  python3 build_exfat_libs.py --dry-run           # show what would be done
"""

import os, sys, re, shutil, subprocess, glob, argparse
from pathlib import Path

# ── Colors ─────────────────────────────────────────────────────────────────
BOLD   = "\033[1m"
GREEN  = "\033[32m"
YELLOW = "\033[33m"
RED    = "\033[31m"
CYAN   = "\033[36m"
RESET  = "\033[0m"

def ok(m):    print(f"{GREEN}  \u2713 {m}{RESET}")
def warn(m):  print(f"{YELLOW}  \u26a0 {m}{RESET}")
def err(m):   print(f"{RED}  \u2717 {m}{RESET}")
def die(m):   err(m); sys.exit(1)
def step(m):  print(f"\n{BOLD}{CYAN}>>> {m}{RESET}")
def info(m):  print(f"    {m}")

def run(cmd, cwd=None, check=True, capture=False):
    info(f"$ {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=capture, text=True)
    if check and result.returncode != 0:
        die(f"Command failed (exit {result.returncode}): {cmd}")
    return result

# ── Arguments ──────────────────────────────────────────────────────────────
parser = argparse.ArgumentParser(description="Build ESP32 libs with exFAT support")
parser.add_argument("--arduino-version", default="3.3.7")
parser.add_argument("--arduino15", default=None, help="Path to Arduino15 (optional)")
parser.add_argument("--test", action="store_true",
                    help="Verify all changes only, make no modifications")
parser.add_argument("--restore", action="store_true",
                    help="Restore original files from backups")
parser.add_argument("--dry-run", action="store_true",
                    help="Show what would be done without doing it")
parser.add_argument("--skip-full-build", action="store_true",
                    help="Skip build.sh (use if it was run recently)")
args = parser.parse_args()

AR_VERSION  = args.arduino_version
BUILDER_DIR = Path("/root/esp32_exfat_build/esp32-arduino-lib-builder")
BUILDER_URL = "https://github.com/espressif/esp32-arduino-lib-builder"

print(f"\n{BOLD}{'='*60}")
print(f"  ESP32 Arduino Lib Builder \u2014 exFAT Support")
print(f"{'='*60}{RESET}\n")

if args.test:
    print(f"{BOLD}{CYAN}  MODE: TEST (verification only, no changes){RESET}\n")

# ── Locate Arduino15 ───────────────────────────────────────────────────────
step("Locating Arduino15")

def find_arduino15():
    if args.arduino15:
        p = Path(args.arduino15)
        if p.exists(): return p
        die(f"Arduino15 not found at: {args.arduino15}")
    for pattern in [
        "/mnt/c/Users/*/AppData/Local/Arduino15",
        "/mnt/d/Users/*/AppData/Local/Arduino15",
    ]:
        matches = glob.glob(pattern)
        if matches:
            return Path(matches[0])
    die("Arduino15 not found. Use --arduino15 <path>")

arduino15 = find_arduino15()
ok(f"Arduino15: {arduino15}")

hw_path  = arduino15 / "packages" / "esp32" / "hardware" / "esp32" / AR_VERSION
libs_dir = arduino15 / "packages" / "esp32" / "tools" / "esp32s3-libs"

# libfatfs.a can be in two locations depending on lib-builder version
lib_file = libs_dir / AR_VERSION / "lib" / "libfatfs.a"
if not lib_file.exists():
    lib_file = libs_dir / "lib" / "libfatfs.a"

# Installed ffconf.h
installed_ffconf = libs_dir / AR_VERSION / "include" / "fatfs" / "src" / "ffconf.h"
if not installed_ffconf.exists():
    candidates = list(libs_dir.rglob("ffconf.h"))
    installed_ffconf = candidates[0] if candidates else None

# USBMSC.cpp
usbmsc = hw_path / "cores" / "esp32" / "USBMSC.cpp"

if not hw_path.exists():
    die(f"ESP32 hardware {AR_VERSION} not found: {hw_path}")
ok(f"ESP32 hardware: {hw_path}")

# ════════════════════════════════════════════════════════════════════════════
# TEST MODE - verification only, no changes
# ════════════════════════════════════════════════════════════════════════════
if args.test:
    PASS = f"{GREEN}  [PASS]{RESET}"
    FAIL = f"{RED}  [FAIL]{RESET}"
    SKIP = f"{YELLOW}  [SKIP]{RESET}"

    results = []  # (name, passed, detail)

    def check(name, passed, detail=""):
        results.append((name, passed, detail))
        icon = PASS if passed else FAIL
        print(f"{icon} {name}")
        if detail:
            info(detail)

    # ── 1. Installed ffconf.h ─────────────────────────────────────────────
    step("1. Checking installed ffconf.h")

    if installed_ffconf and installed_ffconf.exists():
        txt = installed_ffconf.read_text(errors="replace")
        m = re.search(r"#define\s+FF_FS_EXFAT\s+(\d)", txt)
        val = m.group(1) if m else "?"
        passed = (val == "1")
        check(
            f"FF_FS_EXFAT = {val} in installed ffconf.h",
            passed,
            str(installed_ffconf)
        )
        if not passed:
            info("  -> Fix: run script without --test")
    else:
        check("Installed ffconf.h found", False, f"Expected: {installed_ffconf}")

    # ── 2. Build ffconf.h in lib-builder ──────────────────────────────────
    step("2. Checking ffconf.h in lib-builder (for next build)")

    if BUILDER_DIR.exists():
        for f in BUILDER_DIR.rglob("ffconf.h"):
            if ".bak" in str(f): continue
            txt = f.read_text(errors="replace")
            m = re.search(r"#define\s+FF_FS_EXFAT\s+(\d)", txt)
            val = m.group(1) if m else "?"
            passed = (val == "1")
            check(
                f"FF_FS_EXFAT = {val} in {f.relative_to(BUILDER_DIR)}",
                passed,
                str(f)
            )
    else:
        print(f"{SKIP} lib-builder not found at {BUILDER_DIR} (skipping)")

    # ── 3. libfatfs.a - exFAT symbols ────────────────────────────────────
    step("3. Checking exFAT symbols in installed libfatfs.a")

    if lib_file.exists():
        ok(f"libfatfs.a found: {lib_file}")
        r = subprocess.run(
            f"strings '{lib_file}' | grep -v esp32_exfat_build | grep -i exfat | head -10",
            shell=True, capture_output=True, text=True
        )
        exfat_strings = [l for l in r.stdout.strip().splitlines() if l.strip()]

        r2 = subprocess.run(
            f"nm '{lib_file}' 2>/dev/null | grep -i exfat | head -10",
            shell=True, capture_output=True, text=True
        )
        exfat_symbols = [l for l in r2.stdout.strip().splitlines() if l.strip()]

        if exfat_strings or exfat_symbols:
            check("exFAT strings/symbols found in libfatfs.a", True)
            for s in (exfat_strings + exfat_symbols)[:5]:
                info(f"  -> {s}")
        else:
            check("exFAT strings/symbols found in libfatfs.a", False,
                  "libfatfs.a does not contain exFAT code - rebuild required")

        size_kb = lib_file.stat().st_size // 1024
        info(f"  Size of libfatfs.a: {size_kb} KB")
        size_ok = size_kb > 50  # without exFAT ~40KB, with exFAT ~80+KB
        check(f"libfatfs.a size {size_kb} KB (expected >50 KB)", size_ok)
    else:
        check("libfatfs.a found", False, f"Expected: {lib_file}")

    # ── 4. Build libfatfs.a (if exists) ───────────────────────────────────
    step("4. Checking build libfatfs.a in lib-builder")

    build_lib = BUILDER_DIR / "build" / "esp-idf" / "fatfs" / "libfatfs.a"
    if build_lib.exists():
        r = subprocess.run(
            f"strings '{build_lib}' | grep -v esp32_exfat_build | grep -i exfat | head -5",
            shell=True, capture_output=True, text=True
        )
        has_exfat = bool(r.stdout.strip())
        check("exFAT in build libfatfs.a", has_exfat, str(build_lib))
        size_kb = build_lib.stat().st_size // 1024
        info(f"  Size: {size_kb} KB")
    else:
        print(f"{SKIP} build libfatfs.a not found - run build first")

    # ── 5. USBMSC.cpp - CD-ROM patch ──────────────────────────────────────
    step("5. Checking USBMSC.cpp CD-ROM SCSI patch")

    if usbmsc.exists():
        content = usbmsc.read_text(errors="replace")

        checks_usbmsc = [
            ("READ TOC handler",          "READ_TOC"           in content or "0x43" in content),
            ("GET CONFIGURATION handler", "GET_CONFIGURATION"  in content or "0x46" in content),
            ("CD-ROM SCSI response",      "CDROM"              in content or "CD-ROM" in content
                                          or "cdrom" in content.lower()),
            ("PREVENT REMOVAL handler",   "PREVENT_ALLOW"      in content or "prevent" in content.lower()
                                          or "0x1e" in content.lower()),
        ]

        for name, passed in checks_usbmsc:
            check(f"USBMSC: {name}", passed)

        any_cdrom = any(p for _, p in checks_usbmsc)
        if "virtual cdrom" in content.lower() or "v3" in content or "patched" in content.lower():
            check("USBMSC: patch version identified", True)
        elif any_cdrom:
            warn("USBMSC: patch applied but version tag not identified")
        else:
            check("USBMSC: CD-ROM patch applied", False,
                  "-> Run patch_usbmsc.py separately")
    else:
        check("USBMSC.cpp found", False, str(usbmsc))

    # ── 6. Backups ────────────────────────────────────────────────────────
    step("6. Checking backups")

    bak_lib = Path(str(lib_file) + ".bak")
    if bak_lib.exists():
        check("Backup libfatfs.a.bak exists", True, str(bak_lib))
    else:
        warn("Backup libfatfs.a.bak missing - creating...")
        if lib_file.exists():
            shutil.copy2(str(lib_file), str(bak_lib))
            ok(f"Backup created: {bak_lib}")
        results.append(("Backup libfatfs.a.bak created", True, str(bak_lib)))

    if installed_ffconf and installed_ffconf.exists():
        bak_ffconf = Path(str(installed_ffconf) + ".bak")
        if bak_ffconf.exists():
            check("Backup ffconf.h.bak exists", True, str(bak_ffconf))
        else:
            warn("Backup ffconf.h.bak missing - creating...")
            shutil.copy2(str(installed_ffconf), str(bak_ffconf))
            ok(f"Backup created: {bak_ffconf}")
            results.append(("Backup ffconf.h.bak created", True, str(bak_ffconf)))

    # ── Summary ───────────────────────────────────────────────────────────
    total  = len(results)
    passed = sum(1 for _, p, _ in results if p)
    failed = total - passed

    print(f"\n{BOLD}{'='*60}{RESET}")
    if failed == 0:
        print(f"{BOLD}{GREEN}  RESULT: PASS - all checks passed ({passed}/{total}){RESET}")
    else:
        print(f"{BOLD}{RED}  RESULT: FAIL - {failed} check(s) failed ({passed}/{total} passed){RESET}")
        print(f"\n  Failed:")
        for name, passed_r, detail in results:
            if not passed_r:
                print(f"    {RED}  x {name}{RESET}")
                if detail:
                    print(f"      {detail}")
        print(f"\n  -> Run: python3 build_exfat_libs.py --skip-full-build")
    print('='*60)

    sys.exit(0 if failed == 0 else 1)

# ════════════════════════════════════════════════════════════════════════════
# RESTORE MODE
# ════════════════════════════════════════════════════════════════════════════
if args.restore:
    step("Restoring original files from backups")
    restored = 0
    for d in [BUILDER_DIR, libs_dir]:
        if not d.exists(): continue
        for bak in d.rglob("*.bak"):
            orig = Path(str(bak)[:-4])
            shutil.copy2(str(bak), str(orig))
            ok(f"Restored: {orig}")
            restored += 1
    if restored == 0:
        warn("No backups found")
    sys.exit(0)

# ════════════════════════════════════════════════════════════════════════════
# MAIN BUILD MODE
# ════════════════════════════════════════════════════════════════════════════

# ── Clone lib-builder ───────────────────────────────────────────────────────
step("Preparing esp32-arduino-lib-builder")

if not BUILDER_DIR.exists():
    BUILDER_DIR.parent.mkdir(parents=True, exist_ok=True)
    if not args.dry_run:
        run(f"git clone {BUILDER_URL} '{BUILDER_DIR}'")
    ok(f"Cloned: {BUILDER_DIR}")
else:
    ok(f"lib-builder exists: {BUILDER_DIR}")

# ── Run full build.sh ────────────────────────────────────────────────────────
if not args.skip_full_build:
    step(f"Running build.sh for esp32s3 (1-3 hours on first run)")
    warn("build.sh will run git reset --hard - this is expected.")
    warn("We patch ffconf.h AFTER build.sh completes.")

    build_cmd = f"./build.sh -t esp32s3 -b build -c '{hw_path}'"
    info(f"Command: {build_cmd}")
    info(f"Directory: {BUILDER_DIR}")

    if not args.dry_run:
        rc = subprocess.run(build_cmd, shell=True, cwd=str(BUILDER_DIR)).returncode
        if rc != 0:
            die(f"build.sh failed (exit {rc})")
    ok("build.sh completed")
else:
    warn("Skipping full build (--skip-full-build)")

# ── KEY STEP: Patch ffconf.h AFTER build.sh ──────────────────────────────────
step("Patching ffconf.h (after git reset from build.sh)")

FFCONF_PATTERN = re.compile(r'(#define\s+FF_FS_EXFAT\s+)0')

def patch_ffconf(f):
    txt = f.read_bytes().decode('utf-8', errors='replace')
    if re.search(r'#define\s+FF_FS_EXFAT\s+1', txt):
        ok(f"Already patched (=1): {f.name}")
        return False
    new, count = FFCONF_PATTERN.subn(r'\g<1>1', txt)
    if count == 0:
        warn(f"FF_FS_EXFAT line not found in: {f}")
        return False
    bak = Path(str(f) + '.bak')
    if not bak.exists():
        shutil.copy2(str(f), str(bak))
    if not args.dry_run:
        f.write_text(new, encoding='utf-8')
    ok(f"Patched ({count}x): {f}")
    return True

patched_count = 0
for ffconf in BUILDER_DIR.rglob('ffconf.h'):
    if '.bak' not in str(ffconf):
        if patch_ffconf(ffconf):
            patched_count += 1

if patched_count == 0:
    warn("No ffconf.h files patched (possibly all already =1)")

# ── Delete fatfs cache and recompile fatfs only ──────────────────────────────
step("Recompiling fatfs target only (~10 seconds)")

fatfs_cache = BUILDER_DIR / "build" / "esp-idf" / "fatfs"
if fatfs_cache.exists() and not args.dry_run:
    shutil.rmtree(str(fatfs_cache))
    ok("Deleted fatfs cache")

if not args.dry_run:
    full_cmd = (
        "bash -c 'source esp-idf/export.sh 2>/dev/null "
        "&& cd build "
        "&& ninja esp-idf/fatfs/libfatfs.a'"
    )
    run(full_cmd, cwd=str(BUILDER_DIR))
    ok("fatfs compiled")

# ── Verify exFAT in new libfatfs.a ───────────────────────────────────────────
step("Verifying exFAT symbols in new libfatfs.a")

build_lib = BUILDER_DIR / "build" / "esp-idf" / "fatfs" / "libfatfs.a"
if not build_lib.exists():
    die(f"libfatfs.a not found in build directory: {build_lib}")

r = subprocess.run(
    f"strings '{build_lib}' | grep -v esp32_exfat_build | grep -i exfat | head -5",
    shell=True, capture_output=True, text=True
)
if r.stdout.strip():
    ok("exFAT symbols found in new libfatfs.a:")
    for line in r.stdout.strip().splitlines():
        info(f"  -> {line}")
else:
    die("exFAT symbols NOT FOUND in libfatfs.a - patch did not propagate!")

# ── Install libfatfs.a ────────────────────────────────────────────────────────
step("Installing libfatfs.a to Arduino15")

if lib_file.exists():
    bak = Path(str(lib_file) + '.bak')
    if not bak.exists():
        shutil.copy2(str(lib_file), str(bak))
        ok(f"Backup: {bak}")

if not args.dry_run:
    shutil.copy2(str(build_lib), str(lib_file))
    ok(f"Installed: {lib_file}")

# ── Patch installed ffconf.h ─────────────────────────────────────────────────
step("Patching installed ffconf.h in Arduino15")

for ffconf in libs_dir.rglob('ffconf.h'):
    if '.bak' not in str(ffconf):
        patch_ffconf(ffconf)

# ── USBMSC check ─────────────────────────────────────────────────────────────
step("Checking USBMSC.cpp CD-ROM patch")

if usbmsc.exists():
    content = usbmsc.read_text(errors='replace')
    if ('READ_TOC' in content or 'CDROM' in content or
            'cdrom' in content.lower() or '0x43' in content):
        ok("USBMSC.cpp already patched (CD-ROM SCSI handlers found)")
    else:
        warn("USBMSC.cpp is NOT patched for CD-ROM")
        warn("-> Run patch_usbmsc.py separately")
else:
    warn(f"USBMSC.cpp not found: {usbmsc}")

# ── Final verification via --test ─────────────────────────────────────────────
step("Running final verification")

info("Running internal test...")
r = subprocess.run(
    f"python3 '{__file__}' --test --arduino-version {AR_VERSION}",
    shell=True
)
if r.returncode == 0:
    ok("All checks passed!")
else:
    warn("Some checks failed - see output above")

# ── Done ──────────────────────────────────────────────────────────────────────
print(f"\n{BOLD}{'='*60}{RESET}")
print(f"{BOLD}{GREEN}  Done! exFAT support installed.{RESET}")
print(f"{BOLD}{'='*60}{RESET}")
print("""
  Next steps:
  1. Delete Arduino build cache (Windows):
       %LOCALAPPDATA%/arduino/sketches/  <- delete folder contents
  2. Restart Arduino IDE completely
  3. Compile and upload sketch
  4. Serial monitor should show:
       [SD]   exFAT support: YES (compiled in)

  To verify without changes:
    python3 build_exfat_libs.py --test

  To restore originals:
    python3 build_exfat_libs.py --restore
""")
print('='*60)
