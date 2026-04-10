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
  python3 build_exfat_libs.py                     # full build + patch (auto-detects version)
  python3 build_exfat_libs.py --arduino-version 3.3.7  # force specific version
  python3 build_exfat_libs.py --test              # verify only, no changes
  python3 build_exfat_libs.py --skip-full-build   # skip build.sh (if run recently)
  python3 build_exfat_libs.py --restore           # restore originals from backups
  python3 build_exfat_libs.py --dry-run           # show what would be done
  python3 build_exfat_libs.py --skip-usbmsc       # skip USBMSC patch

Note: USBMSC.cpp patch (CD-ROM + Audio SCSI v14) is now integrated.
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
parser.add_argument("--arduino-version", default=None,
                    help="ESP32 Arduino version e.g. 3.3.7 (default: auto-detect latest)")
parser.add_argument("--arduino15", default=None, help="Path to Arduino15 (optional)")
parser.add_argument("--test", action="store_true",
                    help="Verify all changes only, make no modifications")
parser.add_argument("--restore", action="store_true",
                    help="Restore original files from backups")
parser.add_argument("--dry-run", action="store_true",
                    help="Show what would be done without doing it")
parser.add_argument("--skip-usbmsc", action="store_true",
                    help="Skip USBMSC.cpp CD-ROM+Audio patch")
parser.add_argument("--skip-full-build", action="store_true",
                    help="Skip build.sh (use if it was run recently)")
args = parser.parse_args()

BUILDER_DIR = Path("/root/esp32_exfat_build/esp32-arduino-lib-builder")
BUILDER_URL = "https://github.com/espressif/esp32-arduino-lib-builder"

def get_builder_tag(version):
    """Map Arduino ESP32 version to lib-builder git tag.
    The lib-builder tag matches the Arduino package version exactly."""
    # e.g. 3.3.7 -> tags/3.3.7, 3.4.0 -> tags/3.4.0
    return f"tags/{version}"


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

def detect_ar_version(ar15):
    """Auto-detect latest installed ESP32 Arduino version."""
    hw = ar15 / "packages" / "esp32" / "hardware" / "esp32"
    if not hw.exists():
        return None
    versions = sorted([v for v in os.listdir(str(hw))
                       if (hw / v).is_dir() and v[0].isdigit()], reverse=True)
    return versions[0] if versions else None

# Auto-detect version if not specified
_ar15_tmp = find_arduino15()  # called again below properly, just for detection
AR_VERSION = args.arduino_version
if AR_VERSION is None:
    AR_VERSION = detect_ar_version(_ar15_tmp) if _ar15_tmp else "3.3.7"
    if AR_VERSION:
        print(f"    Auto-detected ESP32 Arduino version: {AR_VERSION}")
    else:
        AR_VERSION = "3.3.7"
        warn(f"Could not auto-detect version, using default: {AR_VERSION}")
arduino15 = find_arduino15()
ok(f"Arduino15: {arduino15}")

hw_path  = arduino15 / "packages" / "esp32" / "hardware" / "esp32" / AR_VERSION
libs_dir = arduino15 / "packages" / "esp32" / "tools" / "esp32s3-libs"


# Auto-detect libfatfs.a — path varies across ESP32 Arduino versions
# 2.x: esp32-arduino-libs/<ver>/esp32s3/lib/libfatfs.a
# 3.x: esp32-arduino-libs/<ver>/esp32s3/lib/libfatfs.a  (same but version changes)
def find_libfatfs(libs_dir, version, target="esp32s3"):
    """Find libfatfs.a for given version and target chip."""
    # Try common paths in order of likelihood
    candidates = [
        libs_dir / version / target / "lib" / "libfatfs.a",
        libs_dir / version / "lib" / "libfatfs.a",
    ]
    # Also glob for any version if exact not found
    for c in candidates:
        if c.exists():
            return c
    # Fallback: find any libfatfs.a under libs_dir
    found = list(libs_dir.rglob("libfatfs.a"))
    # Prefer esp32s3 variant
    s3 = [f for f in found if target in str(f)]
    return s3[0] if s3 else (found[0] if found else candidates[0])

lib_file = find_libfatfs(libs_dir, AR_VERSION)

# Auto-detect ffconf.h
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
    step("0. Checking sdkconfig.h exFAT define")
    sdk_files = list(libs_dir.rglob("sdkconfig.h"))
    tools_dir_t = libs_dir.parent.parent
    for chip in ["esp32s3", "esp32"]:
        for p in tools_dir_t.glob(f"*{chip}*/**/sdkconfig.h"):
            if p not in sdk_files:
                sdk_files.append(p)
    if not sdk_files:
        check("sdkconfig.h found", False, "-> File not found in Arduino15 tools")
    else:
        for sdk_h in sdk_files:
            try:
                txt = sdk_h.read_text(encoding="utf-8", errors="ignore")
                has_define = "#define CONFIG_FATFS_EXFAT_SUPPORT 1" in txt
                check(f"CONFIG_FATFS_EXFAT_SUPPORT=1 in {sdk_h.name}", has_define,
                      str(sdk_h) if has_define else f"MISSING define in {sdk_h}")
            except Exception as e:
                check(f"sdkconfig.h readable", False, str(e))

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
        is_v10 = "patched v14" in content.lower() or "patched v10" in content.lower() or "scsi_audio_play" in content
        if is_v10:
            check("USBMSC: CD-ROM + Audio SCSI patch v14", True)
        elif any_cdrom:
            check("USBMSC: CD-ROM patch applied (older version)", True,
                  "-> Re-run build_exfat_libs.py --restore then --skip-full-build to upgrade to v14")
        else:
            check("USBMSC: CD-ROM patch applied", False,
                  "-> Run build_exfat_libs.py to apply patch")
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

def patch_usbmsc(path):
    with open(path, 'r', encoding='utf-8') as f:
        src = f.read()

    # Always restore from backup first
    bak = str(path) + ".bak"
    if os.path.exists(bak):
        shutil.copy2(bak, str(path))
        with open(path, 'r', encoding='utf-8') as f:
            src = f.read()
        print("  Restored clean backup")
    else:
        shutil.copy2(str(path), bak)
        print(f"  Backup created: {path.name}.bak")

    # Always patch tud_msc_test_unit_ready_cb for UNIT ATTENTION support
    tur_start = src.find('bool tud_msc_test_unit_ready_cb(')
    if tur_start >= 0:
        brace = src.find('{', tur_start)
        depth = 0; tur_end = -1
        for i in range(brace, len(src)):
            if src[i] == '{': depth += 1
            elif src[i] == '}': depth -= 1
            if depth == 0: tur_end = i + 1; break
        if tur_end > 0 and 'msc_set_unit_attention' not in src[:tur_start]:
            new_tur = (
                '// UNIT ATTENTION counter — persists for multiple TUR polling cycles so the OS\n'
                '// has several opportunities to react to disc change (single-shot flag is unreliable).\n'
                '// First TUR returns ASC=29h (POWER ON/RESET) which forces every SCSI host to perform\n'
                '// a complete re-initialisation including INQUIRY and PVD re-read.  This is required\n'
                '// for DOS audio players (NC, CDPlayer) that keep a cached PVD across disc swaps and\n'
                '// do not re-read the filesystem on ASC=28h (MEDIUM CHANGED) alone.\n'
                '// Subsequent TURs return ASC=28h (MEDIUM CHANGED) so MSCDEX/SHSUCDX also sees the\n'
                '// standard disc-change notification and re-reads the TOC.\n'
                'static volatile uint8_t _ua_counter = 0;\n'
                '// _audioPlayedOnce: per spec 0x13 returned once after play ends, then 0x15.\n'
                '// Defined here (before msc_set_unit_attention) so it can be reset on disc swap.\n'
                'static volatile bool _audioPlayedOnce = false;\n'
                'extern "C" void msc_set_unit_attention(void) { _ua_counter = 4; _audioPlayedOnce = false; }\n'
                'bool tud_msc_test_unit_ready_cb(uint8_t lun) {\n'
                '  if (_ua_counter > 0) {\n'
                '    uint8_t asc = (_ua_counter == 4) ? 0x29 : 0x28;\n'
                '    _ua_counter--;\n'
                '    tud_msc_set_sense(lun, 0x06, asc, 0x00);\n'
                '    return false;\n'
                '  }\n'
                '  return msc_luns[lun].media_present;\n'
                '}\n'
            )
            src = src[:tur_start] + new_tur + src[tur_end:]
            print('  Patched tud_msc_test_unit_ready_cb for UNIT ATTENTION (ASC=29 first + ASC=28 x3)')

    if 'CD-ROM SCSI patched v14' in src:
        print("  Already patched v14")
        return True
    # v10/v11 had audio gated on mode-2 only or non-DOS — v12 enables all modes except driver 3.
    # After restore-from-backup above, src is always clean so this only triggers
    # on first-ever run when no .bak exists yet and USBMSC.cpp is already v10-patched.
    if 'CD-ROM SCSI patched v13' in src or 'CD-ROM SCSI patched v12' in src or 'CD-ROM SCSI patched v11' in src or 'CD-ROM SCSI patched v10' in src or 'scsi_audio_play' in src:
        print("  Found older patch (v11/v10) — run --restore first, then --skip-full-build to upgrade to v14")
        return False  # cannot safely re-patch without clean original

    func_start = src.find('int32_t tud_msc_scsi_cb(')
    if func_start < 0:
        func_start = src.find('tud_msc_scsi_cb(')
    if func_start < 0:
        print("ERROR: tud_msc_scsi_cb not found"); return False

    brace_start = src.find('{', func_start)
    depth = 0
    func_end = -1
    for i in range(brace_start, len(src)):
        if src[i] == '{': depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                func_end = i + 1
                break

    func_body = src[func_start:func_end]
    body_start = func_body.find('{')
    print(f"  Found tud_msc_scsi_cb ({len(func_body)} bytes)")

    # File-scope declarations inserted BEFORE the function (no extern "C" needed
    # since both translation units are C++ and name mangling matches)
    file_scope = (
        "\n/* scsi_audio bridge — forward declarations */\n"
        "extern void     scsi_audio_play(uint32_t, uint32_t);\n"
        "extern void     scsi_audio_stop();\n"
        "extern void     scsi_audio_pause();\n"
        "extern void     scsi_audio_resume();\n"
        "// setSense: set SCSI sense and return -1 (error)\n"
        "static inline int32_t setSense(uint8_t sk,uint8_t asc,uint8_t ascq){\n"
        "  tud_msc_set_sense(0,sk,asc,ascq); return -1;}\n"
        "// _audioPlayedOnce declared near _ua_counter (before msc_set_unit_attention)\n"
        "extern int      scsi_audio_state();\n"
        "extern void     scsi_audio_subchannel(uint8_t*);\n"
        "extern int      scsi_audio_track_count();\n"
        "extern int      scsi_audio_track_info(int, uint32_t*, uint32_t*);\n"
        "extern uint32_t cdromBlockCount_get();\n"
        "extern uint32_t cdromBlockCount;\n"
        "extern void scsi_log(const char* msg);\n"
        "extern void scsi_log_play_msf(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint32_t,uint32_t);\n"
        "extern uint8_t  scsi_get_dos_driver();\n"
        "extern bool     scsi_get_dos_compat();\n"
        "extern bool     scsi_get_debug();\n"
        "extern bool     scsi_get_gotek_mode();\n"
        "extern void     scsi_subchannel_polled();\n"
        "extern uint8_t  scsi_first_audio_track();\n"
        "extern bool     scsi_has_data_track();\n"
        "extern void     scsi_dae_read(uint32_t lba, uint32_t count);\n"
        "extern void     scsi_patch_v14_dae_ok(); /* sentinel: confirms patch is loaded */\n"
        "extern \"C\" int32_t scsi_dae_read_data(uint32_t lba,uint32_t cnt,void* buf,uint32_t bufsz);\n"
        "/* end audio bridge */\n\n"
    )


    # Switch block inserted inside the function (no extern declarations here)
    inner = (
        "{ /* CD-ROM SCSI patched v14 */\n"
        "  /* ── INQUIRY (0x12) — handle explicitly to control device_type byte ──────\n"
        "     TinyUSB calls tud_msc_scsi_cb for INQUIRY. If scsi_cb returns -1,\n"
        "     TinyUSB falls back to its internal handler which returns device_type=0x05\n"
        "     (CD-ROM) regardless of mode. We must return 36 bytes here to prevent\n"
        "     TinyUSB from using the fallback 0x05 in GoTek mode.\n"
        "     GoTek mode: r[0]=0x00 (Direct-access) → Windows installs Disk driver.\n"
        "     CD-ROM mode: r[0]=0x05 (CD-ROM)       → Windows installs CdRom driver. */\n"
        "  if (scsi_cmd[0] == 0x12) {\n"
        "    if ((uint32_t)bufsize >= 36u) {\n"
        "      uint8_t *r = (uint8_t*)buffer;\n"
        "      memset(r, 0, 36);\n"
        "      if (scsi_get_gotek_mode()) {\n"
        "        r[0] = 0x00;  /* Direct-access block device (USB flash/floppy) */\n"
        "        r[1] = 0x80;  r[2] = 0x02;  r[3] = 0x02;  r[4] = 0x1F;\n"
        "        memcpy(&r[8],  \"ESP32-S3\",         8);\n"
        "        memcpy(&r[16], \"VirtualFloppy   \", 16);\n"
        "        memcpy(&r[32], \"1.00\",              4);\n"
        "      } else {\n"
        "        r[0] = 0x05;  /* CD-ROM peripheral device type */\n"
        "        r[1] = 0x80;  r[2] = 0x02;  r[3] = 0x02;  r[4] = 0x1F;\n"
        "        cplstr(&r[8],  msc_luns[lun].vendor_id,   8);\n"
        "        cplstr(&r[16], msc_luns[lun].product_id,  16);\n"
        "        cplstr(&r[32], msc_luns[lun].product_rev, 4);\n"
        "      }\n"
        "    }\n"
        "    return (int32_t)((uint32_t)bufsize >= 36u ? 36 : 0);\n"
        "  }\n"
        "  /* In GoTek mode skip ALL CD-ROM SCSI — fall through to original handler */\n"
        "  if (scsi_get_gotek_mode()) goto cdrom_scsi_end;\n"
        "  /* Sentinel and CD-ROM SCSI — only active in CD-ROM mode */\n"
        "  scsi_patch_v14_dae_ok();\n"
        "  { /* CD-ROM SCSI commands block */\n"
        "  switch(scsi_cmd[0]) {\n"
"      case 0x51:{if(bufsize>=34){memset(buffer,0,34);\n"
        "        ((uint8_t*)buffer)[1]=0x20;((uint8_t*)buffer)[2]=0x0E;\n"
        "        ((uint8_t*)buffer)[3]=((uint8_t*)buffer)[4]=\n"
        "        ((uint8_t*)buffer)[5]=((uint8_t*)buffer)[6]=1;return 34;}break;}\n"
        "      case 0x1A:{ /* MODE SENSE(6) — pages 0x0D, 0x0E, 0x2A */\n"
        "        uint8_t pc=scsi_cmd[2]&0x3F;\n"
        "        if((pc==0x2A||pc==0x3F)&&bufsize>=28){\n"
        "          /* Page 0x2A: CD-ROM Capabilities per MMC-1 */\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,28);\n"
        "          b[0]=27;                   /* mode data length */\n"
        "          b[3]=0;                    /* block desc length */\n"
        "          b[4]=0x2A;                 /* page code */\n"
        "          b[5]=0x12;                 /* page length = 18 */\n"
        "          b[6]=0x00;                 /* read caps */\n"
        "          b[7]=0x00;                 /* write caps */\n"
        "          b[8]=0x01;                 /* bit0=CD-DA commands supported (audio play) */\n"
        "          b[9]=0x00;\n"
        "          b[10]=0; b[11]=0;           /* obsolete */\n"
        "          b[12]=0x00; b[13]=0x58;     /* max speed: 1x = 88KB/s = 0x0058 */\n"
        "          b[14]=0x00; b[15]=0xFF;     /* volume levels: 256 */\n"
        "          b[16]=0x00; b[17]=0x00;     /* buffer size */\n"
        "          b[18]=0x00; b[19]=0x58;     /* current speed: 1x */\n"
        "          return 28;\n"
        "        }\n"
        "        if((pc==0x0D||pc==0x3F)&&bufsize>=12){\n"
        "          /* Page 0x0D: CD-ROM device parameters per SCSI-2 §14.3.3.2 */\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,12);\n"
        "          b[0]=11;                   /* mode data length */\n"
        "          b[4]=0x0D;                 /* page code */\n"
        "          b[5]=0x06;                 /* page length = 6 */\n"
        "          b[6]=0x00;                 /* reserved */\n"
        "          b[7]=0x0D;                 /* inactivity timer = 8s */\n"
        "          b[8]=0x00; b[9]=60;         /* S units per M = 60 */\n"
        "          b[10]=0x00; b[11]=75;       /* F units per S = 75 */\n"
        "          return 12;\n"
        "        }\n"
        "        if((pc==0x0E||pc==0x3F)&&bufsize>=20){\n"
        "          /* page 0x0E: CD-ROM Audio Control Parameters per OB-U0077C §2.9.6 */\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,20);\n"
        "          b[0]=19;                 /* mode data length (20-1) */\n"
        "          b[3]=0;                  /* block descriptor length = 0 */\n"
        "          b[4]=0x0E;               /* page code */\n"
        "          b[5]=0x0E;               /* page length = 14 */\n"
        "          b[6]=0x04;               /* Immed=1, SOTC=0 */\n"
        "          b[12]=0x01; b[13]=0xFF;  /* port 0: left channel, volume max */\n"
        "          b[14]=0x02; b[15]=0xFF;  /* port 1: right channel, volume max */\n"
        "          return 20;\n"
        "        }\n"
        "        if(bufsize>=4){memset(buffer,0,4);((uint8_t*)buffer)[0]=3;return 4;}break;}\n"
        "      case 0x5A:{ /* MODE SENSE(10) — pages 0x0D, 0x0E, 0x2A */\n"
        "        uint8_t pc=scsi_cmd[2]&0x3F;\n"
        "        if((pc==0x2A||pc==0x3F)&&bufsize>=32){\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,32);\n"
        "          b[1]=30;                   /* mode data length (32-2) */\n"
        "          b[8]=0x2A; b[9]=0x12;\n"
        "          b[12]=0x01;                /* audio play supported */\n"
        "          b[16]=0x00; b[17]=0x58;    /* max speed 1x */\n"
        "          b[18]=0x00; b[19]=0xFF;    /* volume levels */\n"
        "          b[22]=0x00; b[23]=0x58;    /* current speed */\n"
        "          return 32;\n"
        "        }\n"
        "        if((pc==0x0D||pc==0x3F)&&bufsize>=16){\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,16);\n"
        "          b[1]=14;                   /* mode data length */\n"
        "          b[8]=0x0D; b[9]=0x06;\n"
        "          b[11]=0x0D;\n"
        "          b[12]=0x00; b[13]=60;\n"
        "          b[14]=0x00; b[15]=75;\n"
        "          return 16;\n"
        "        }\n"
        "        if((pc==0x0E||pc==0x3F)&&bufsize>=24){\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,24);\n"
        "          b[1]=22;                 /* mode data length (24-2), MSB=0 */\n"
        "          b[7]=0;                  /* block descriptor length = 0 */\n"
        "          b[8]=0x0E;               /* page code */\n"
        "          b[9]=0x0E;               /* page length = 14 */\n"
        "          b[10]=0x04;              /* Immed=1, SOTC=0 */\n"
        "          b[16]=0x01; b[17]=0xFF;  /* port 0: left channel, volume max */\n"
        "          b[18]=0x02; b[19]=0xFF;  /* port 1: right channel, volume max */\n"
        "          return 24;\n"
        "        }\n"
        "        if(bufsize>=8){memset(buffer,0,8);((uint8_t*)buffer)[1]=6;return 8;}break;}\n"
        "      case 0x1B:{/* START/STOP UNIT per spec OB-U0077C §2.39 */\n"
        "        {uint8_t loej=scsi_cmd[4]&2, start=scsi_cmd[4]&1;\n"
        "        if(loej&&!start){ /* eject */ scsi_audio_stop(); }\n"
        "        else if(!start){ /* spin down = stop audio */ scsi_audio_stop(); }\n"
        "        /* start=1: spin up, no-op for virtual drive */\n"
        "        } return 0;}\n"
        "      case 0x2B:return 0; /* SEEK(10): no-op for virtual drive */\n"
        "      case 0xDA:return 0; /* SET CD-ROM SPEED(1): no-op */\n"
        "      case 0xBB:return 0; /* SET CD-ROM SPEED(2): no-op */\n"
        "      case 0x15:return 0; /* MODE SELECT(6): accept, ignore — volume stored by host */\n"
        "      case 0x55:return 0; /* MODE SELECT(10): accept, ignore */\n"
        "      case 0x1E:return 0;\n"
        "      case 0xBD:{uint16_t l=bufsize<8?(uint16_t)bufsize:8;memset(buffer,0,l);return(int32_t)l;}\n"
                "    default:break;\n"
        "  }\n"
        
        "  /* Audio/TOC block: active for non-DOS mode (Windows/Win98/Linux) OR any DOS driver except 3.  */\n"
        "  /* DOS driver 3 (DI1000DD/USBASPI 2.20): data-only by design, no audio SCSI commands.        */\n"
        "  if(!scsi_get_dos_compat()||scsi_get_dos_driver()!=3){\n"
        "    auto msfToLba = [](uint8_t m,uint8_t s,uint8_t f)->uint32_t{\n"
        "      return (uint32_t)m*60*75+(uint32_t)s*75+f; };\n"
        "    // Helper: set CHECK CONDITION sense data in buffer\n"
        "    // Static sense buffer: filled on error, returned on REQUEST SENSE\n"
        "    static uint8_t senseData[18]={0x70,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,0,0};\n"
        "    static bool senseValid=false;\n"
        "    auto setSense=[&](uint8_t sk,uint8_t asc,uint8_t ascq)->int32_t{\n"
        "      memset(senseData,0,18);\n"
        "      senseData[0]=0x70; senseData[2]=sk; senseData[7]=10;\n"
        "      senseData[12]=asc; senseData[13]=ascq;\n"
        "      senseValid=true; return -1; };\n"
        "    // REQUEST SENSE (0x03) -- always handle first\n"
        "    if(scsi_cmd[0]==0x03){\n"
        "      uint8_t len=(uint8_t)(bufsize<18?bufsize:18);\n"
        "      memcpy(buffer,senseValid?senseData:(uint8_t[]){0x70,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,0,0},len);\n"
        "      senseValid=false; return len;\n"
        "    }\n"
        "    switch (scsi_cmd[0]) {\n"
        "      case 0x43: { /* READ TOC — ANSI X3.304-1997 Table 67/70 */\n"
        "        uint8_t msf=scsi_cmd[1]&0x02;\n"
        "        uint8_t req_track=scsi_cmd[6]; /* Track/Session Number */\n"
        "        int na=scsi_audio_track_count();\n"
        "        bool hasData=scsi_has_data_track();\n"
        "        uint8_t fat=scsi_first_audio_track();\n"
        "        int tot = hasData ? (1+na) : na;\n"
        "        if(req_track==0xAA){\n"
        "          if((uint32_t)12>bufsize) return 0;\n"
        "          memset(buffer,0,12); uint8_t*b=(uint8_t*)buffer;\n"
        "          uint16_t dl2=10;\n"
        "          b[0]=dl2>>8;b[1]=dl2&0xFF;\n"
        "          b[2]=hasData?1:(uint8_t)fat;\n"
        "          b[3]=(uint8_t)tot;\n"
        "          uint32_t bc=cdromBlockCount_get();\n"
        "          b[4+1]=0x14;b[4+2]=0xAA;\n"
        "          if(msf){uint32_t l=bc+150;b[4+5]=(uint8_t)(l/(60*75));b[4+6]=(uint8_t)((l/75)%60);b[4+7]=(uint8_t)(l%75);}\n"
        "          else{b[4+4]=(uint8_t)(bc>>24);b[4+5]=(uint8_t)(bc>>16);b[4+6]=(uint8_t)(bc>>8);b[4+7]=(uint8_t)(bc);}\n"
        "          return 12;\n"
        "        }\n"
        "        /* Full response size (per spec: data_length always reflects full TOC,    */\n"
        "        /* even when alloc_len is too small — host issues 2nd request with full size) */\n"
        "        int full_resp=4+(tot+1)*8; /* header + all tracks + lead-out */\n"
        "        int actual_resp=(int)bufsize<full_resp?(int)bufsize:full_resp;\n"
        "        /* Safety: if alloc too small for even the header, return safe zeros.     */\n"
        "        /* USBCD1 reads past returned bytes into garbage — zeroing prevents       */\n"
        "        /* it from corrupting its track table with random LBA addresses.          */\n"
        "        if(actual_resp<4){ memset(buffer,0,actual_resp); return actual_resp; }\n"
        "        memset(buffer,0,actual_resp); uint8_t*b=(uint8_t*)buffer;\n"
        "        uint16_t dl=(uint16_t)(full_resp-2);  /* ALWAYS full length per spec */\n"
        "        bool clipped=(actual_resp<full_resp);\n"
        "        /* b[2] always reflects the true first track number (1 if data track present,\n"
        "         * first audio track number for pure-audio discs).                           \n"
        "         * Drivers that probe with a small alloc (e.g. alloc=10) rely on b[2] to    \n"
        "         * determine whether a data track exists before issuing a full-size request. \n"
        "         * Returning first_track=2 (skip data) caused those drivers to conclude the  \n"
        "         * disc is audio-only, preventing filesystem access entirely.                */\n"
        "        b[0]=dl>>8;b[1]=dl&0xFF;\n"
        "        b[2]=hasData?1:(uint8_t)fat;  /* first track: 1 if data track present, else first audio */\n"
        "        b[3]=(uint8_t)tot;               /* last track number always full */\n"
        "        int off=4;\n"
        "        /* Data track 1 — only in full (non-clipped) response; clipped probe skips it to maximize audio tracks */\n"
        "        if(hasData&&req_track<=1&&off+8<=actual_resp){\n"
"          b[off+1]=0x14;b[off+2]=1;\n"
"          if(msf){b[off+5]=0;b[off+6]=2;b[off+7]=0;}\n"
"          else{b[off+7]=0;}\n"
"          off+=8;\n"
        "        }\n"
        "        /* Audio tracks — fill sequentially until buffer full.                   */\n"
        "        /* Always reserve 8B for lead-out so MSCDEX can find disc end.           */\n"
        "        int loop_max=clipped?(actual_resp-8):actual_resp;\n"
        "        for(int t=0;t<na&&off+8<=loop_max;t++){\n"
        "          uint8_t tnum=(fat>0)?(uint8_t)(fat+t):(uint8_t)(t+2);\n"
        "          if(tnum<req_track) continue;\n"
        "          uint32_t lba=0,len=0; scsi_audio_track_info(tnum,&lba,&len);\n"
        "          b[off+1]=0x10;b[off+2]=tnum;\n"
        "          if(msf){uint32_t l=lba+150;\n"
        "            b[off+5]=(uint8_t)(l/(60*75));b[off+6]=(uint8_t)((l/75)%60);b[off+7]=(uint8_t)(l%75);\n"
        "          }else{b[off+4]=(uint8_t)(lba>>24);b[off+5]=(uint8_t)(lba>>16);\n"
        "                b[off+6]=(uint8_t)(lba>>8);b[off+7]=(uint8_t)(lba);}\n"
        "          off+=8;\n"
        "        }\n"
        "        /* Lead-out track AAh */\n"
        "        if(off+8<=actual_resp){\n"
        "          uint32_t bc=cdromBlockCount_get();\n"
        "          b[off+1]=0x14;b[off+2]=0xAA;\n"
        "          if(msf){uint32_t l=bc+150;\n"
        "            b[off+5]=(uint8_t)(l/(60*75));\n"
        "            b[off+6]=(uint8_t)((l/75)%60);\n"
        "            b[off+7]=(uint8_t)(l%75);\n"
        "          }else{\n"
        "            b[off+4]=(uint8_t)(bc>>24);b[off+5]=(uint8_t)(bc>>16);\n"
        "            b[off+6]=(uint8_t)(bc>>8);b[off+7]=(uint8_t)(bc);\n"
        "          }\n"
        "        }\n"
        "        /* Debug log */\n"
        "        if(na>0){\n"
        "          uint32_t lba1=0,len1=0; scsi_audio_track_info(fat>0?fat:2,&lba1,&len1);\n"
        "          char _tb[96];\n"
        "          if(scsi_get_debug()){\n"
"          snprintf(_tb,sizeof(_tb),\"[SCSI] READ_TOC %s %s trk%d=%lu req=%02X alloc=%u full=%u\\n\",\n"
        "            msf?\"MSF\":\"LBA\", hasData?\"mixed\":\"audio-only\",\n"
        "            (int)(fat>0?fat:2),(unsigned long)lba1,\n"
        "            (unsigned)req_track,(unsigned)bufsize,(unsigned)full_resp);\n"
        "          scsi_log(_tb);\n"
        "          }\n"
        "        }\n"
        "        return actual_resp;\n"
        "      }\n"
        "      case 0x42: { /* READ SUB-CHANNEL per OB-U0077C §2.27 */\n"
        "        if(bufsize<4)break;\n"
        "        bool subq=(scsi_cmd[2]&0x40)!=0;\n"
        "        memset(buffer,0,subq?16:4); uint8_t*b=(uint8_t*)buffer;\n"
        "        /* Record poll timestamp for Win98 Stop/Pause detection */\n"
        "        scsi_subchannel_polled();\n"
        "        if(scsi_get_debug()) scsi_log(\"[SCSI] READ_SUB_CH\\n\");\n"
        "        /* Audio status per TABLE 2-27C */\n"
        "        int st=scsi_audio_state();\n"
        "        if(st==1){ b[1]=0x11; _audioPlayedOnce=true; }\n"
        "        else if(st==2){ b[1]=0x12; }\n"
        "        else{ b[1]=_audioPlayedOnce?0x13:0x15; _audioPlayedOnce=false; }\n"
        "        if(!subq){ b[2]=0;b[3]=0; return 4; } /* SubQ=0: header only */\n"
        "        /* Sub-channel data block (12 bytes) per TABLE 2-27F */\n"
        "        b[2]=0; b[3]=12; /* sub-channel data length */\n"
        "        uint8_t fmt=scsi_cmd[3]; /* sub-channel data format from CDB */\n"
        "        b[4]=(fmt<=0x03)?fmt:0x01; /* return requested format code */\n"
        "        /* b[5] = ADR|Control: ADR=1 (current pos), Control=0 (audio, no pre-emph, no copy) */\n"
        "        b[5]=0x10; /* ADR=1<<4 | Control=0 */\n"
        "        uint8_t sub[8]={}; scsi_audio_subchannel(sub);\n"
        "        b[6]=sub[0]; /* track number */\n"
        "        b[7]=sub[1]; /* index number */\n"
        "        /* Absolute address bytes 8-11 */\n"
        "        b[8]=0; b[9]=sub[2]; b[10]=sub[3]; b[11]=sub[4];\n"
        "        /* Track-relative address bytes 12-15 */\n"
        "        b[12]=0; b[13]=sub[5]; b[14]=sub[6]; b[15]=sub[7];\n"
        "        return 16;\n"
        "      }\n"
        "      case 0x45: { /* PLAY AUDIO LBA per OB-U0077C §2.10 */\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint16_t len=((uint16_t)scsi_cmd[7]<<8)|scsi_cmd[8];\n"
        "        uint32_t end=len?lba+len:cdromBlockCount_get();\n"
        "        _audioPlayedOnce=false; scsi_audio_play(lba,end); return 0;\n"
        "      }\n"
        "      case 0x47: { /* PLAY AUDIO MSF */\n"
        "        /* ANSI X3.304-1997 Table 28: CDB bytes 3-8 are real M,S,F fields.    */\n"
        "        /* Standard decode: abs=(M*60+S)*75+F, LBA=abs-150.                   */\n"
        "        auto msf2lba=[](uint8_t m,uint8_t s,uint8_t f)->uint32_t{\n"
"          uint32_t l=(uint32_t)(m*60+s)*75+f; return l>=150?l-150:0;};\n"
        "        uint8_t sm=scsi_cmd[3],ss=scsi_cmd[4],sf=scsi_cmd[5];\n"
        "        uint8_t em=scsi_cmd[6],es=scsi_cmd[7],ef=scsi_cmd[8];\n"
        "        uint32_t s=msf2lba(sm,ss,sf), e=msf2lba(em,es,ef);\n"
        "        scsi_log_play_msf(sm,ss,sf,em,es,ef,s,e);\n"
        "        /* Snap: resolve s to valid audio range BEFORE any s==e / s>e checks.    */\n"
        "        /* USBCD1 may corrupt its track table from alloc=10 re-queries and then  */\n"
        "        /* send PLAY_MSF with s=lead-out (past all audio) or s=0 (data area).    */\n"
        "        if(scsi_has_data_track()&&scsi_audio_track_count()>0){\n"
        "          uint8_t fat2=scsi_first_audio_track();\n"
        "          uint32_t fstlba=0,fstlen=0;\n"
        "          scsi_audio_track_info((int)fat2,&fstlba,&fstlen);\n"
        "          int lasttrk=(int)fat2+scsi_audio_track_count()-1;\n"
        "          uint32_t lstlba=0,lstlen=0;\n"
        "          scsi_audio_track_info(lasttrk,&lstlba,&lstlen);\n"
        "          uint32_t audio_end=lstlba+lstlen;\n"
        "          if(fstlba>0&&(s<fstlba||s>=audio_end)){\n"
        "            s=fstlba;\n"
        "            scsi_log(\"[SCSI] PLAY_MSF snap: s outside audio -> first audio\\n\");\n"
        "          }\n"
        "        }\n"
        "        /* SCSI-2 §14.2.4: start==end → no-op; start>end → error */\n"
        "        if(s==e){\n"
        "          /* USBCD1 sends s==e when its track table is corrupt/empty.          */\n"
        "          /* On mixed disc: interpret as 'play all audio' from first track.    */\n"
        "          if(scsi_has_data_track()&&scsi_audio_track_count()>0){\n"
        "            uint8_t fat2=scsi_first_audio_track();\n"
        "            uint32_t fstlba=0,fstlen=0;\n"
        "            scsi_audio_track_info((int)fat2,&fstlba,&fstlen);\n"
        "            uint32_t discend=cdromBlockCount_get();\n"
        "            if(fstlba>0&&discend>fstlba){\n"
        "              scsi_log(\"[SCSI] PLAY_MSF s==e: play all audio\\n\");\n"
        "              _audioPlayedOnce=false; scsi_audio_play(fstlba,discend); return 0;\n"
        "            }\n"
        "          }\n"
        "          scsi_log(\"[SCSI] PLAY_MSF s==e\\n\"); return 0;\n"
        "        }\n"
        "        if(s>e){ scsi_log(\"[SCSI] PLAY_MSF s>e\\n\"); return setSense(0x05,0x24,0x00);}\n"
        "        if(e==0) e=cdromBlockCount_get();\n"
        "        _audioPlayedOnce=false; scsi_audio_play(s,e); return 0;\n"
        "      }\n"
        "      case 0x48: { /* PLAY AUDIO TRACK INDEX per SCSI-2 Table 243 */\n"
        "        /* byte4=startTrack, byte5=startIdx, byte7=endTrack, byte8=endIdx */\n"
        "        uint8_t st=scsi_cmd[4],et=scsi_cmd[7];\n"
        "        uint32_t sl=0,el=0,tmp=0;\n"
        "        scsi_audio_track_info(st,&sl,&tmp);\n"
        "        if(scsi_audio_track_info(et,&el,&tmp))el+=tmp;\n"
        "        else el=cdromBlockCount_get();\n"
        "        _audioPlayedOnce=false; scsi_audio_play(sl,el); return 0;\n"
        "      }\n"
        "      case 0x4B:{if(scsi_cmd[8]&1){if(scsi_get_debug()) scsi_log(\"[SCSI] RESUME\\n\");scsi_audio_resume();}else{if(scsi_get_debug()) scsi_log(\"[SCSI] PAUSE\\n\");scsi_audio_pause();}return 0;}\n"
        "      case 0x4E:{if(scsi_get_debug()) scsi_log(\"[SCSI] STOP\\n\");scsi_audio_stop();return 0;}\n"
        "      case 0xBE:{ /* READ CD (MMC-3) — Digital Audio Extraction (DAE)            */\n"
        "        /* Windows Vista+: uses READ_CD to get PCM data → plays via PC speakers  */\n"
        "        /* DOS/Win98: uses PLAY AUDIO MSF/LBA — READ_CD never arrives from them  */\n"
        "        /* dosCompat: return ILLEGAL REQUEST (prevents I2S DMA NULL-handle crash) */\n"
        "        if(scsi_get_dos_compat()) return setSense(0x05,0x20,0x00);\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint32_t cnt=((uint32_t)scsi_cmd[6]<<16)|((uint32_t)scsi_cmd[7]<<8)|scsi_cmd[8];\n"
        "        if(cnt==0) return 0;\n"
        "        { char _dc[72];\n"
        "          if(scsi_get_debug()){\n"
"          snprintf(_dc,sizeof(_dc),\"[SCSI] READ_CD lba=%lu cnt=%lu buf=%u\\n\",\n"
        "            (unsigned long)lba,(unsigned long)cnt,(unsigned)bufsize);\n"
        "          scsi_log(_dc); }\n"
        "          }\n"
        "        /* Read real PCM data from BIN file — Windows decodes & plays via soundcard */\n"
        "        int32_t n=scsi_dae_read_data(lba,cnt,buffer,(uint32_t)bufsize);\n"
        "        return (n>0)?n:setSense(0x05,0x21,0x00);}\n"
        "      case 0x46:{ /* GET CONFIGURATION — Feature 0x001E (CD Read/DAE) for Windows  */\n"
        "        /* MMC-3: response includes ONLY features with code >= Starting Feature Number */\n"
        "        /* DAP bit (Feature 0x001E byte4 bit2) = 1: signals digital audio play support */\n"
        "        uint8_t rt=(scsi_cmd[1]&3);\n"
        "        uint16_t sf=(uint16_t)(((uint16_t)scsi_cmd[2]<<8)|scsi_cmd[3]);\n"
        "        if(scsi_get_debug()){ char _gc[80]; snprintf(_gc,sizeof(_gc),\"[SCSI] GET_CONFIG rt=%u feat=0x%04X alloc=%u\\n\",(unsigned)rt,(unsigned)sf,(unsigned)bufsize); scsi_log(_gc); }\n"
        "        /* Build response: header(8) + Core(12) + CDRead(8) = 28 bytes */\n"
        "        /* Only include features with code >= sf */\n"
        "        uint8_t resp[28]; memset(resp,0,sizeof(resp));\n"
        "        uint8_t *b=resp;\n"
        "        /* Bytes 4-7: Current Profile = 0x0008 (CD-ROM) */\n"
        "        b[6]=0x00; b[7]=0x08;\n"
        "        uint16_t off=8;\n"
        "        /* Feature 0x0001: Core (always included if sf <= 1) */\n"
        "        if(sf<=0x0001){ b[off]=0x00;b[off+1]=0x01;b[off+2]=0x0B;b[off+3]=0x08;\n"
        "          b[off+4]=0x00;b[off+5]=0x00;b[off+6]=0x00;b[off+7]=0x08;\n"
        "          b[off+8]=0x03;b[off+9]=0x00;b[off+10]=0x00;b[off+11]=0x00; off+=12; }\n"
        "        /* Feature 0x001E: CD Read — DAP=1 (bit2) signals digital audio play */\n"
        "        if(sf<=0x001E){ b[off]=0x00;b[off+1]=0x1E;b[off+2]=0x0B;b[off+3]=0x04;\n"
        "          b[off+4]=0x04; off+=8; } /* 0x04 = DAP bit set */\n"
        "        /* Set Data Length = total response size - 4 */\n"
        "        uint16_t dlen=(uint16_t)(off-4);\n"
        "        b[0]=(uint8_t)(dlen>>8); b[1]=(uint8_t)(dlen&0xFF);\n"
        "        if((uint32_t)bufsize<(uint32_t)off) off=(uint16_t)bufsize;\n"
        "        memcpy(buffer,resp,off); return(int32_t)off;}\n"
        "      case 0xA9:{ /* PLAY AUDIO TRACK RELATIVE(12) per SCSI-2 Table 245 */\n"
        "        /* bytes 2-5: TRLBA (signed), bytes 6-9: transferLen, byte 10: startTrack */\n"
        "        int32_t trlba=(int32_t)(((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                                ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5]);\n"
        "        uint32_t tlen=((uint32_t)scsi_cmd[6]<<24)|((uint32_t)scsi_cmd[7]<<16)|\n"
        "                      ((uint32_t)scsi_cmd[8]<<8)|scsi_cmd[9];\n"
        "        uint8_t trk=scsi_cmd[10];\n"
        "        uint32_t tstart=0,tlenBlocks=0;\n"
        "        if(!scsi_audio_track_info(trk,&tstart,&tlenBlocks)){return setSense(0x05,0x21,0x00);}\n"
        "        uint32_t abs_start=(uint32_t)((int32_t)tstart+trlba);\n"
        "        if(tlen==0) return 0;\n"
        "        _audioPlayedOnce=false; scsi_audio_play(abs_start,abs_start+tlen); return 0;}\n"
        "      case 0xA5:{ /* PLAY AUDIO(12) - same as 45h but 32-bit len */\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint32_t len=((uint32_t)scsi_cmd[6]<<24)|((uint32_t)scsi_cmd[7]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[8]<<8)|scsi_cmd[9];\n"
        "        uint32_t end=len?lba+len:cdromBlockCount_get();\n"
        "        scsi_audio_play(lba,end); return 0;}\n"
        "      case 0x49:{ /* PLAY AUDIO TRACK RELATIVE(10) per §2.15 */\n"
        "        /* TRLBA: signed 32-bit offset from track start (index 1) */\n"
        "        int32_t trlba=(int32_t)(((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                                ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5]);\n"
        "        uint8_t trk=scsi_cmd[6];\n"
        "        uint16_t tlen=((uint16_t)scsi_cmd[7]<<8)|scsi_cmd[8];\n"
        "        uint32_t tstart=0,tlenBlocks=0;\n"
        "        if(!scsi_audio_track_info(trk,&tstart,&tlenBlocks)){return setSense(0x05,0x21,0x00);}\n"
        "        uint32_t abs_start=(uint32_t)((int32_t)tstart+trlba);\n"
        "        if(tlen==0) return 0; /* len=0: no-op per spec */\n"
        "        uint32_t abs_end=abs_start+tlen;\n"
        "        scsi_audio_play(abs_start,abs_end); return 0;}\n"
        "      case 0x44:{ /* READ HEADER per §2.26: returns data mode + address */\n"
        "        if(bufsize<8) break;\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint8_t msf=scsi_cmd[1]&2;\n"
        "        /* If LBA is on audio track return CHECK CONDITION per spec */\n"
        "        int na=scsi_audio_track_count();\n"
        "        for(int t=0;t<na;t++){uint32_t ts=0,tl=0;\n"
        "          scsi_audio_track_info(t+2,&ts,&tl);\n"
        "          if(lba>=ts&&lba<ts+tl){return setSense(0x05,0x21,0x00);}}\n"
        "        uint8_t*b=(uint8_t*)buffer; memset(b,0,8);\n"
        "        b[0]=1; /* CD-ROM Data Mode 1 */\n"
        "        if(msf){uint32_t l=lba+150;\n"
        "          b[4]=(uint8_t)(l/(60*75));b[5]=(uint8_t)((l/75)%60);b[6]=(uint8_t)(l%75);\n"
        "        }else{b[4]=(uint8_t)(lba>>24);b[5]=(uint8_t)(lba>>16);\n"
        "               b[6]=(uint8_t)(lba>>8);b[7]=(uint8_t)(lba);}\n"
        "        return 8;}\n"
        "      default:break;\n"
        "    }\n"
        "  }else{ /* dosCompat=OFF or dosDriver!=2: v8 behavior */\n"
        "    auto msfToLba = [](uint8_t m,uint8_t s,uint8_t f)->uint32_t{\n"
        "      return (uint32_t)m*60*75+(uint32_t)s*75+f; };\n"
        "    // Helper: set CHECK CONDITION sense data in buffer\n"
        "    // Static sense buffer: filled on error, returned on REQUEST SENSE\n"
        "    static uint8_t senseData[18]={0x70,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,0,0};\n"
        "    static bool senseValid=false;\n"
        "    auto setSense=[&](uint8_t sk,uint8_t asc,uint8_t ascq)->int32_t{\n"
        "      memset(senseData,0,18);\n"
        "      senseData[0]=0x70; senseData[2]=sk; senseData[7]=10;\n"
        "      senseData[12]=asc; senseData[13]=ascq;\n"
        "      senseValid=true; return -1; };\n"
        "    // REQUEST SENSE (0x03) -- always handle first\n"
        "    if(scsi_cmd[0]==0x03){\n"
        "      uint8_t len=(uint8_t)(bufsize<18?bufsize:18);\n"
        "      memcpy(buffer,senseValid?senseData:(uint8_t[]){0x70,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,0,0},len);\n"
        "      senseValid=false; return len;\n"
        "    }\n"
        "    switch (scsi_cmd[0]) {\n"
        "      case 0x43: { /* READ TOC */\n"
        "        uint8_t msf=scsi_cmd[1]&0x02;\n"
        "        int na=scsi_audio_track_count(), tot=1+na;\n"
        "        int resp=4+(tot+1)*8; if((uint32_t)resp>bufsize)resp=(int)bufsize;\n"
        "        memset(buffer,0,resp); uint8_t*b=(uint8_t*)buffer;\n"
        "        uint16_t dl=(uint16_t)(resp-2);\n"
        "        b[0]=dl>>8;b[1]=dl&0xFF;b[2]=1;b[3]=(uint8_t)tot;\n"
        "        int off=4;\n"
"        b[off+1]=0x14;b[off+2]=1;\n"
"        if(msf){b[off+5]=0;b[off+6]=2;b[off+7]=0;}\n"
"        off+=8;\n"
        "        for(int t=0;t<na&&off+8<=resp;t++){\n"
        "          uint32_t lba=0,len=0; scsi_audio_track_info(t+2,&lba,&len);\n"
        "          b[off+1]=0x10;b[off+2]=(uint8_t)(t+2);\n"
        "          if(msf){uint32_t l=lba+150;\n"
        "            b[off+5]=(uint8_t)(l/(60*75));b[off+6]=(uint8_t)((l/75)%60);b[off+7]=(uint8_t)(l%75);\n"
        "          }else{b[off+4]=(uint8_t)(lba>>24);b[off+5]=(uint8_t)(lba>>16);\n"
        "                b[off+6]=(uint8_t)(lba>>8);b[off+7]=(uint8_t)(lba);}\n"
        "          off+=8;\n"
        "        }\n"
        "        if(off+8<=resp){\n"
        "          uint32_t bc=cdromBlockCount_get();\n"
        "          b[off+1]=0x14;b[off+2]=0xAA;\n"
        "          if(msf){uint32_t l=bc+150;\n"
        "            b[off+5]=(uint8_t)(l/(60*75));\n"
        "            b[off+6]=(uint8_t)((l/75)%60);\n"
        "            b[off+7]=(uint8_t)(l%75);\n"
        "          }else{\n"
        "            b[off+4]=(uint8_t)(bc>>24);b[off+5]=(uint8_t)(bc>>16);\n"
        "            b[off+6]=(uint8_t)(bc>>8);b[off+7]=(uint8_t)(bc);\n"
        "          }\n"
        "        }\n"
        "        return resp;\n"
        "      }\n"
        "      case 0x42: { /* READ SUB-CHANNEL per OB-U0077C §2.27 */\n"
        "        if(bufsize<4)break;\n"
        "        bool subq=(scsi_cmd[2]&0x40)!=0;\n"
        "        memset(buffer,0,subq?16:4); uint8_t*b=(uint8_t*)buffer;\n"
        "        /* Audio status per TABLE 2-27C */\n"
        "        int st=scsi_audio_state();\n"
        "        if(st==1){ b[1]=0x11; _audioPlayedOnce=true; }\n"
        "        else if(st==2){ b[1]=0x12; }\n"
        "        else{ b[1]=_audioPlayedOnce?0x13:0x15; _audioPlayedOnce=false; }\n"
        "        if(!subq){ b[2]=0;b[3]=0; return 4; } /* SubQ=0: header only */\n"
        "        /* Sub-channel data block (12 bytes) per TABLE 2-27F */\n"
        "        b[2]=0; b[3]=12; /* sub-channel data length */\n"
        "        uint8_t fmt=scsi_cmd[3]; /* sub-channel data format from CDB */\n"
        "        b[4]=(fmt<=0x03)?fmt:0x01; /* return requested format code */\n"
        "        /* b[5] = ADR|Control: ADR=1 (current pos), Control=0 (audio, no pre-emph, no copy) */\n"
        "        b[5]=0x10; /* ADR=1<<4 | Control=0 */\n"
        "        uint8_t sub[8]={}; scsi_audio_subchannel(sub);\n"
        "        b[6]=sub[0]; /* track number */\n"
        "        b[7]=sub[1]; /* index number */\n"
        "        /* Absolute address bytes 8-11 */\n"
        "        b[8]=0; b[9]=sub[2]; b[10]=sub[3]; b[11]=sub[4];\n"
        "        /* Track-relative address bytes 12-15 */\n"
        "        b[12]=0; b[13]=sub[5]; b[14]=sub[6]; b[15]=sub[7];\n"
        "        return 16;\n"
        "      }\n"
        "      case 0x45: { /* PLAY AUDIO LBA per OB-U0077C §2.10 */\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint16_t len=((uint16_t)scsi_cmd[7]<<8)|scsi_cmd[8];\n"
        "        uint32_t end=len?lba+len:cdromBlockCount_get();\n"
        "        _audioPlayedOnce=false; scsi_audio_play(lba,end); return 0;\n"
        "      }\n"
        "      case 0x47: { /* PLAY AUDIO MSF */\n"
        "        auto msf2lba=[](uint8_t m,uint8_t s,uint8_t f)->uint32_t{\n"
"          uint32_t l=(uint32_t)(m*60+s)*75+f; return l>=150?l-150:0;};\n"
        "        uint32_t s=msf2lba(scsi_cmd[3],scsi_cmd[4],scsi_cmd[5]);\n"
        "        uint32_t e=msf2lba(scsi_cmd[6],scsi_cmd[7],scsi_cmd[8]);\n"
        "        /* SCSI-2 §14.2.4: start==end → no-op; start>end → error */\n"
        "        if(s==e) return 0;\n"
        "        if(s>e) return setSense(0x05,0x24,0x00);\n"
        "        if(e==0) e=cdromBlockCount_get();\n"
        "        _audioPlayedOnce=false; scsi_audio_play(s,e); return 0;\n"
        "      }\n"
        "      case 0x48: { /* PLAY AUDIO TRACK INDEX per SCSI-2 Table 243 */\n"
        "        /* byte4=startTrack, byte5=startIdx, byte7=endTrack, byte8=endIdx */\n"
        "        uint8_t st=scsi_cmd[4],et=scsi_cmd[7];\n"
        "        uint32_t sl=0,el=0,tmp=0;\n"
        "        scsi_audio_track_info(st,&sl,&tmp);\n"
        "        if(scsi_audio_track_info(et,&el,&tmp))el+=tmp;\n"
        "        else el=cdromBlockCount_get();\n"
        "        _audioPlayedOnce=false; scsi_audio_play(sl,el); return 0;\n"
        "      }\n"
        "      case 0x4B:{if(scsi_cmd[8]&1)scsi_audio_resume();else scsi_audio_pause();return 0;}\n"
        "      case 0x4E:{scsi_audio_stop();return 0;}\n"
        "      case 0x46:{if(bufsize>=8){memset(buffer,0,8);((uint8_t*)buffer)[3]=4;((uint8_t*)buffer)[7]=8;return 8;}break;}\n"
        "      case 0x51:{if(bufsize>=34){memset(buffer,0,34);\n"
        "        ((uint8_t*)buffer)[1]=0x20;((uint8_t*)buffer)[2]=0x0E;\n"
        "        ((uint8_t*)buffer)[3]=((uint8_t*)buffer)[4]=\n"
        "        ((uint8_t*)buffer)[5]=((uint8_t*)buffer)[6]=1;return 34;}break;}\n"
        "      case 0x1A:{ /* MODE SENSE(6) — support page 0x0E Audio Control */\n"
        "        uint8_t pc=scsi_cmd[2]&0x3F; /* page code */\n"
        "        if((pc==0x0E||pc==0x3F)&&bufsize>=20){\n"
        "          /* page 0x0E: CD-ROM Audio Control Parameters per OB-U0077C §2.9.6 */\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,20);\n"
        "          b[0]=19;                 /* mode data length (20-1) */\n"
        "          b[3]=0;                  /* block descriptor length = 0 */\n"
        "          b[4]=0x0E;               /* page code */\n"
        "          b[5]=0x0E;               /* page length = 14 */\n"
        "          b[6]=0x04;               /* Immed=1, SOTC=0 */\n"
        "          b[12]=0x01; b[13]=0xFF;  /* port 0: left channel, volume max */\n"
        "          b[14]=0x02; b[15]=0xFF;  /* port 1: right channel, volume max */\n"
        "          return 20;\n"
        "        }\n"
        "        if(bufsize>=4){memset(buffer,0,4);((uint8_t*)buffer)[0]=3;return 4;}break;}\n"
        "      case 0x5A:{ /* MODE SENSE(10) — support page 0x0E Audio Control */\n"
        "        uint8_t pc=scsi_cmd[2]&0x3F;\n"
        "        if((pc==0x0E||pc==0x3F)&&bufsize>=24){\n"
        "          uint8_t*b=(uint8_t*)buffer; memset(b,0,24);\n"
        "          b[1]=22;                 /* mode data length (24-2), MSB=0 */\n"
        "          b[7]=0;                  /* block descriptor length = 0 */\n"
        "          b[8]=0x0E;               /* page code */\n"
        "          b[9]=0x0E;               /* page length = 14 */\n"
        "          b[10]=0x04;              /* Immed=1, SOTC=0 */\n"
        "          b[16]=0x01; b[17]=0xFF;  /* port 0: left channel, volume max */\n"
        "          b[18]=0x02; b[19]=0xFF;  /* port 1: right channel, volume max */\n"
        "          return 24;\n"
        "        }\n"
        "        if(bufsize>=8){memset(buffer,0,8);((uint8_t*)buffer)[1]=6;return 8;}break;}\n"
        "      case 0x1B:{/* START/STOP UNIT per spec OB-U0077C §2.39 */\n"
        "        {uint8_t loej=scsi_cmd[4]&2, start=scsi_cmd[4]&1;\n"
        "        if(loej&&!start){ /* eject */ scsi_audio_stop(); }\n"
        "        else if(!start){ /* spin down = stop audio */ scsi_audio_stop(); }\n"
        "        /* start=1: spin up, no-op for virtual drive */\n"
        "        } return 0;}\n"
        "      case 0xA9:{ /* PLAY AUDIO TRACK RELATIVE(12) per SCSI-2 Table 245 */\n"
        "        /* bytes 2-5: TRLBA (signed), bytes 6-9: transferLen, byte 10: startTrack */\n"
        "        int32_t trlba=(int32_t)(((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                                ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5]);\n"
        "        uint32_t tlen=((uint32_t)scsi_cmd[6]<<24)|((uint32_t)scsi_cmd[7]<<16)|\n"
        "                      ((uint32_t)scsi_cmd[8]<<8)|scsi_cmd[9];\n"
        "        uint8_t trk=scsi_cmd[10];\n"
        "        uint32_t tstart=0,tlenBlocks=0;\n"
        "        if(!scsi_audio_track_info(trk,&tstart,&tlenBlocks)){return setSense(0x05,0x21,0x00);}\n"
        "        uint32_t abs_start=(uint32_t)((int32_t)tstart+trlba);\n"
        "        if(tlen==0) return 0;\n"
        "        _audioPlayedOnce=false; scsi_audio_play(abs_start,abs_start+tlen); return 0;}\n"
        "      case 0xA5:{ /* PLAY AUDIO(12) - same as 45h but 32-bit len */\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint32_t len=((uint32_t)scsi_cmd[6]<<24)|((uint32_t)scsi_cmd[7]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[8]<<8)|scsi_cmd[9];\n"
        "        uint32_t end=len?lba+len:cdromBlockCount_get();\n"
        "        scsi_audio_play(lba,end); return 0;}\n"
        "      case 0x49:{ /* PLAY AUDIO TRACK RELATIVE(10) per §2.15 */\n"
        "        /* TRLBA: signed 32-bit offset from track start (index 1) */\n"
        "        int32_t trlba=(int32_t)(((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                                ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5]);\n"
        "        uint8_t trk=scsi_cmd[6];\n"
        "        uint16_t tlen=((uint16_t)scsi_cmd[7]<<8)|scsi_cmd[8];\n"
        "        uint32_t tstart=0,tlenBlocks=0;\n"
        "        if(!scsi_audio_track_info(trk,&tstart,&tlenBlocks)){return setSense(0x05,0x21,0x00);}\n"
        "        uint32_t abs_start=(uint32_t)((int32_t)tstart+trlba);\n"
        "        if(tlen==0) return 0; /* len=0: no-op per spec */\n"
        "        uint32_t abs_end=abs_start+tlen;\n"
        "        scsi_audio_play(abs_start,abs_end); return 0;}\n"
        "      case 0x44:{ /* READ HEADER per §2.26: returns data mode + address */\n"
        "        if(bufsize<8) break;\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint8_t msf=scsi_cmd[1]&2;\n"
        "        /* If LBA is on audio track return CHECK CONDITION per spec */\n"
        "        int na=scsi_audio_track_count();\n"
        "        for(int t=0;t<na;t++){uint32_t ts=0,tl=0;\n"
        "          scsi_audio_track_info(t+2,&ts,&tl);\n"
        "          if(lba>=ts&&lba<ts+tl){return setSense(0x05,0x21,0x00);}}\n"
        "        uint8_t*b=(uint8_t*)buffer; memset(b,0,8);\n"
        "        b[0]=1; /* CD-ROM Data Mode 1 */\n"
        "        if(msf){uint32_t l=lba+150;\n"
        "          b[4]=(uint8_t)(l/(60*75));b[5]=(uint8_t)((l/75)%60);b[6]=(uint8_t)(l%75);\n"
        "        }else{b[4]=(uint8_t)(lba>>24);b[5]=(uint8_t)(lba>>16);\n"
        "               b[6]=(uint8_t)(lba>>8);b[7]=(uint8_t)(lba);}\n"
        "        return 8;}\n"
        "      case 0x2B:return 0; /* SEEK(10): no-op for virtual drive */\n"
        "      case 0xDA:return 0; /* SET CD-ROM SPEED(1): no-op */\n"
        "      case 0xBB:return 0; /* SET CD-ROM SPEED(2): no-op */\n"
        "      case 0x15:return 0; /* MODE SELECT(6): accept, ignore — volume stored by host */\n"
        "      case 0x55:return 0; /* MODE SELECT(10): accept, ignore */\n"
        "      case 0x1E:return 0;\n"
        "      case 0xBD:{uint16_t l=bufsize<8?(uint16_t)bufsize:8;memset(buffer,0,l);return(int32_t)l;}\n"
        "      case 0xBE:{ /* READ CD — Windows DAE in data-only/DI1000DD mode */\n"
        "        /* Block 2 = DI1000DD data-only driver. No audio support.         */\n"
        "        /* Return ILLEGAL REQUEST so Windows knows DAE is not available.   */\n"
        "        /* Without this case, the command falls through to the original    */\n"
        "        /* USBMSC handler which has no CD-ROM support → crash/watchdog.    */\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint32_t cnt=((uint32_t)scsi_cmd[6]<<16)|((uint32_t)scsi_cmd[7]<<8)|scsi_cmd[8];\n"
        "        { char _dc[72];\n"
        "          snprintf(_dc,sizeof(_dc),\"[SCSI] READ_CD(DI1000DD) lba=%lu cnt=%lu — ILLEGAL REQ\\n\",\n"
        "            (unsigned long)lba,(unsigned long)cnt);\n"
        "          scsi_log(_dc); }\n"
        "        return setSense(0x05,0x20,0x00); /* ILLEGAL REQUEST: cmd not supported */}\n"
        "      default:break;\n"
        "    }\n"   # closes second dosCompat switch
        "  }\n"     # closes if(!dosCompat||driver!=3) block
        "  } /* end CD-ROM SCSI commands block */\n"
        "  cdrom_scsi_end:;\n"   # goto label — GoTek mode jumps here, skipping all CD-ROM SCSI
        "  }\n"     # closes outer { /* CD-ROM SCSI patched v14 */ }
"  /* end CD-ROM SCSI patched v14 */\n"
        "  "        "  "
    )

    new_func_body = func_body[:body_start+1] + '\n  ' + inner + func_body[body_start+1:]
    new_src = src[:func_start] + file_scope + new_func_body + src[func_end:]

    # ── Replace tud_msc_inquiry_cb with GoTek-aware version ──────────────────
    # This is the ONLY way to change what Windows sees as hardware ID.
    # Windows builds USBSTOR\<type><vendor><product> from the INQUIRY response.
    # <type> is CdRom (device_type=0x05) or Disk (0x00) — set by TinyUSB internally.
    # <vendor> and <product> come from tud_msc_inquiry_cb which we control.
    # By returning "VirtualFloppy" in GoTek mode, Windows gets a different HW ID
    # and installs the correct "Disk" driver (not the cached "CdRom" driver).
    INQ_FUNC = 'void tud_msc_inquiry_cb('
    inq_start = new_src.find(INQ_FUNC)
    if inq_start >= 0 and 'scsi_get_gotek_mode' not in new_src[inq_start:inq_start+500]:
        inq_brace = new_src.find('{', inq_start)
        inq_depth = 0; inq_end = -1
        for ci in range(inq_brace, len(new_src)):
            if new_src[ci] == '{': inq_depth += 1
            elif new_src[ci] == '}': inq_depth -= 1
            if inq_depth == 0: inq_end = ci + 1; break
        if inq_end > 0:
            new_inq = (
                "/* Forward declaration — scsi_get_gotek_mode is defined in the .ino file.\n"
                "   This declaration must appear before tud_msc_inquiry_cb because inquiry_cb\n"
                "   is called before tud_msc_scsi_cb in the USB stack. */\n"
                "static bool scsi_get_gotek_mode_early();\n"
                "void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {\n"
                "  if (scsi_get_gotek_mode_early()) {\n"
                "    /* GoTek mode: present as USB flash drive — different USBSTOR HW ID */\n"
                "    memcpy(vendor_id,   \"ESP32-S3\",         8);\n"
                "    memcpy(product_id,  \"VirtualFloppy   \", 16);\n"
                "    memcpy(product_rev, \"1.00\",              4);\n"
                "  } else {\n"
                "    cplstr(vendor_id,   msc_luns[lun].vendor_id,   8);\n"
                "    cplstr(product_id,  msc_luns[lun].product_id,  16);\n"
                "    cplstr(product_rev, msc_luns[lun].product_rev, 4);\n"
                "  }\n"
                "}\n"
                "/* Thunk: scsi_get_gotek_mode is defined in the .ino TU — forward-declare\n"
                "   it here with C++ linkage so the linker can resolve it at link time. */\n"
                "extern bool scsi_get_gotek_mode();\n"
                "static bool scsi_get_gotek_mode_early() { return scsi_get_gotek_mode(); }\n"
            )
            new_src = new_src[:inq_start] + new_inq + new_src[inq_end:]
            print("  Replaced tud_msc_inquiry_cb: GoTek=VirtualFloppy, CD-ROM=VirtualCDROM")

    # ── tud_msc_is_writable_cb: not needed — msc.isWritable() sets the flag ──

    # ── Scan for any OTHER tud_msc_* callbacks with CD-ROM specific content ──────
    # Catches version-specific functions (e.g. inquiry2_cb) that set 0x05 or 2048.
    import re as _re
    for _m in _re.finditer(r'(tud_msc_\w+)\s*\(', new_src):
        _fname = _m.group(1)
        if _fname in ('tud_msc_inquiry_cb','tud_msc_inquiry2_cb','tud_msc_scsi_cb',
                      'tud_msc_set_sense','tud_msc_is_writable_cb',
                      'tud_msc_read10_cb','tud_msc_write10_cb',
                      'tud_msc_capacity_cb','tud_msc_start_stop_cb',
                      'tud_msc_test_unit_ready_cb'):
            continue
        _fb = new_src.find('{', _m.start())
        if _fb < 0 or _fb > _m.start()+200: continue
        _d=0; _fe=-1
        for _i in range(_fb, min(len(new_src), _fb+5000)):
            if new_src[_i]=='{': _d+=1
            elif new_src[_i]=='}': _d-=1
            if _d==0: _fe=_i+1; break
        if _fe < 0: continue
        _body = new_src[_fb:_fe]
        if any(x in _body for x in ('0x05','2048','CD-ROM','cdrom','audio_track')):
            print(f"  WARNING: {_fname} has CD-ROM content — review manually")

    # ── Patch tud_msc_inquiry2_cb — ROOT CAUSE of Windows CD-ROM classification ─
    # This function builds the full 36-byte SCSI INQUIRY response and always sets
    # r[0]=0x05 (CD-ROM peripheral device type) regardless of mode.
    # Windows reads byte[0] to decide driver: 0x05=CdRom, 0x00=Disk.
    # We inject a GoTek check at the very start of the function body so that
    # in GoTek mode it returns 0x00 (Direct-access block device) and exits early.
    inq2_pos = new_src.find('tud_msc_inquiry2_cb(')
    if inq2_pos >= 0 and 'scsi_get_gotek_mode_early' not in new_src[inq2_pos:inq2_pos+1500]:
        inq2_brace = new_src.find('{', inq2_pos)
        if inq2_brace > 0:
            gotek_guard = (
                "\n"
                "  /* ── GoTek mode: Direct-access block device, NOT CD-ROM ─────────────────\n"
                "     r[0]=0x00 makes Windows install Disk driver instead of CdRom driver.\n"
                "     This is the ROOT FIX for the USBSTOR\\CdRom classification problem. */\n"
                "  if (scsi_get_gotek_mode_early()) {\n"
                "    if (bufsize < 36) return 0;\n"
                "    memset(r, 0, 36);\n"
                "    r[0] = 0x00;  /* Direct-access block device (USB flash disk) */\n"
                "    r[1] = 0x80;  /* removable medium */\n"
                "    r[2] = 0x02;  /* ANSI SCSI-2 */\n"
                "    r[3] = 0x02;  /* response data format */\n"
                "    r[4] = 0x1F;  /* additional length = 31 */\n"
                "    memcpy(&r[8],  \"ESP32-S3\",         8);\n"
                "    memcpy(&r[16], \"VirtualFloppy   \", 16);\n"
                "    memcpy(&r[32], \"1.00\",              4);\n"
                "    return 36;\n"
                "  }\n"
                "  /* CD-ROM mode: original implementation follows unchanged */\n"
            )
            new_src = new_src[:inq2_brace + 1] + gotek_guard + new_src[inq2_brace + 1:]
            print("  Patched tud_msc_inquiry2_cb: GoTek=0x00 Direct-access, CD-ROM=0x05")
    elif inq2_pos < 0:
        print("  WARNING: tud_msc_inquiry2_cb not found — may not exist in this Arduino version")
    else:
        print("  tud_msc_inquiry2_cb already has GoTek guard")

    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_src)
    print("  Patched v14: READ TOC sequential fill, all tracks visible in DOS")
    return True



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

# ── PATCH defconfig: enable CONFIG_I2S_ISR_IRAM_SAFE ─────────────────────────
# By default the I2S interrupt handler lives in flash cache.
# When SDMMC DMA reads audio data, it can momentarily disable the flash cache,
# deferring the I2S EOF interrupt → DMA descriptor not updated in time → audio
# glitch / crackling.  Setting CONFIG_I2S_ISR_IRAM_SAFE=y moves the I2S ISR to
# IRAM so it always runs regardless of flash cache state — same fix Espressif
# recommends in their I2S docs for real-time audio applications.
defconfig_paths = list(BUILDER_DIR.rglob("defconfig.common")) + \
                  list(BUILDER_DIR.rglob("defconfig.esp32s3"))
for dc in defconfig_paths:
    if ".bak" in str(dc): continue
    txt = dc.read_text(errors="replace")
    if "CONFIG_I2S_ISR_IRAM_SAFE" in txt:
        ok(f"CONFIG_I2S_ISR_IRAM_SAFE already in {dc.name}")
    else:
        bak = Path(str(dc) + ".bak")
        if not bak.exists(): shutil.copy2(str(dc), str(bak))
        if not args.dry_run:
            dc.write_text(txt + "\nCONFIG_I2S_ISR_IRAM_SAFE=y\n", encoding="utf-8")
        ok(f"Patched {dc.name}: added CONFIG_I2S_ISR_IRAM_SAFE=y (I2S ISR in IRAM — fixes SD crackling)")

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

# ── Recompile I2S driver (for CONFIG_I2S_ISR_IRAM_SAFE) ──────────────────────
step("Recompiling I2S driver (CONFIG_I2S_ISR_IRAM_SAFE — I2S ISR in IRAM)")

i2s_targets = [
    "esp-idf/esp_driver_i2s/libesp_driver_i2s.a",  # ESP-IDF 5.x
    "esp-idf/driver/libdriver.a",                    # ESP-IDF 4.x fallback
]
if not args.dry_run:
    for tgt in i2s_targets:
        # Delete cache so ninja forces recompile
        tgt_dir = BUILDER_DIR / "build" / Path(tgt).parent
        if tgt_dir.exists():
            shutil.rmtree(str(tgt_dir))
            ok(f"Deleted I2S cache: {tgt_dir.name}")
        i2s_cmd = (
            f"bash -c 'source esp-idf/export.sh 2>/dev/null "
            f"&& cd build "
            f"&& ninja {tgt}'"
        )
        r = subprocess.run(i2s_cmd, shell=True, cwd=str(BUILDER_DIR))
        if r.returncode == 0:
            ok(f"I2S driver compiled: {Path(tgt).name}")
            break
        else:
            warn(f"Target not found: {tgt} — trying next")

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

# ── Install I2S driver library to Arduino15 ───────────────────────────────────
step("Installing I2S driver library to Arduino15 (CONFIG_I2S_ISR_IRAM_SAFE)")

i2s_build_candidates = [
    BUILDER_DIR / "build" / "esp-idf" / "esp_driver_i2s" / "libesp_driver_i2s.a",
    BUILDER_DIR / "build" / "esp-idf" / "driver" / "libdriver.a",
]
i2s_installed = False
for i2s_build_lib in i2s_build_candidates:
    if not i2s_build_lib.exists():
        continue
    # Find corresponding installed library in Arduino15
    i2s_name = i2s_build_lib.name
    installed_matches = list(libs_dir.rglob(i2s_name))
    if not installed_matches:
        warn(f"No installed {i2s_name} found in Arduino15 — skipping")
        continue
    for dst in installed_matches:
        if ".bak" in str(dst): continue
        bak = Path(str(dst) + ".bak")
        if not bak.exists():
            shutil.copy2(str(dst), str(bak))
            ok(f"Backup: {bak.name}")
        if not args.dry_run:
            shutil.copy2(str(i2s_build_lib), str(dst))
            ok(f"Installed I2S driver: {dst}")
        i2s_installed = True
    break

if not i2s_installed:
    warn("I2S driver library not found in build output — CONFIG_I2S_ISR_IRAM_SAFE will apply on next full build.sh run")

# ── Patch installed ffconf.h ─────────────────────────────────────────────────
step("Patching installed ffconf.h in Arduino15")

for ffconf in libs_dir.rglob('ffconf.h'):
    if '.bak' not in str(ffconf):
        patch_ffconf(ffconf)


# -- Patch sdkconfig.h (compiler define for exFAT) --
step("Patching sdkconfig.h - compiler define for exFAT")

import re as _re
# sdkconfig.h may be in libs_dir OR directly in the tools dir
# e.g. .../tools/esp32s3-libs/3.3.7/include/sdkconfig.h
sdkconfig_h_files = list(libs_dir.rglob("sdkconfig.h"))
# Also search one level up (tools/<chip>-libs/<ver>/)
tools_dir = libs_dir.parent.parent  # packages/esp32/tools
for chip in ["esp32s3", "esp32"]:
    for p in tools_dir.glob(f"*{chip}*/**/sdkconfig.h"):
        if p not in sdkconfig_h_files:
            sdkconfig_h_files.append(p)
if sdkconfig_h_files:
    info(f"Found {len(sdkconfig_h_files)} sdkconfig.h file(s)")
    for p in sdkconfig_h_files:
        info(f"  {p}")
patched_sdk = 0
for sdk_h in sdkconfig_h_files:
    if ".bak" in str(sdk_h): continue
    try:
        txt = sdk_h.read_text(encoding="utf-8", errors="ignore")
    except: continue
    needs_patch = False
    if "#define CONFIG_FATFS_EXFAT_SUPPORT 1" not in txt:
        needs_patch = True
    if "#define CONFIG_I2S_ISR_IRAM_SAFE 1" not in txt:
        needs_patch = True
    if not needs_patch:
        ok(f"Already has exFAT + I2S_ISR_IRAM_SAFE defines: {sdk_h.name}"); continue
    bak = Path(str(sdk_h) + ".bak")
    if not bak.exists(): shutil.copy2(str(sdk_h), str(bak))
    new_txt = _re.sub(r"#define\s+CONFIG_FATFS_EXFAT_SUPPORT\s+\S+",
                      "#define CONFIG_FATFS_EXFAT_SUPPORT 1", txt)
    if new_txt == txt:
        idx = txt.rfind("#endif")
        ins = "\n#define CONFIG_FATFS_EXFAT_SUPPORT 1\n\n"
        new_txt = (txt[:idx] + ins + txt[idx:]) if idx >= 0 else txt + ins
    # Add I2S ISR IRAM-safe define (fixes I2S interrupt deferred when cache disabled)
    if "#define CONFIG_I2S_ISR_IRAM_SAFE" not in new_txt:
        idx2 = new_txt.rfind("#endif")
        ins2 = "\n/* I2S ISR in IRAM: interrupt runs even when flash cache is disabled (SD/PSRAM access) */\n#define CONFIG_I2S_ISR_IRAM_SAFE 1\n\n"
        new_txt = (new_txt[:idx2] + ins2 + new_txt[idx2:]) if idx2 >= 0 else new_txt + ins2
    if not args.dry_run:
        sdk_h.write_text(new_txt, encoding="utf-8")
    ok(f"Patched sdkconfig.h: {sdk_h} (exFAT + I2S_ISR_IRAM_SAFE)")
    patched_sdk += 1
if not sdkconfig_h_files:
    warn("sdkconfig.h not found in libs_dir")

# ── USBMSC patch (integrated from patch_usbmsc.py) ──────────────────────────
if not args.skip_usbmsc:
    step("Applying USBMSC.cpp CD-ROM + Audio SCSI patch v14")
    if usbmsc.exists():
        if not args.dry_run:
            result = patch_usbmsc(usbmsc)
            if result:
                ok("USBMSC.cpp patched: CD-ROM + Audio SCSI handlers (v14 — non-DOS audio enabled)")
            else:
                warn("USBMSC.cpp: patch failed or needs --restore first (v10 found without clean backup)")
        else:
            ok(f"[dry-run] Would patch: {usbmsc}")
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
print(f"{BOLD}{GREEN}  Done! exFAT + USBMSC patch installed.{RESET}")
print(f"{BOLD}{'='*60}{RESET}")
print("""
  Next steps:
  1. Delete Arduino build cache (Windows):
       %LOCALAPPDATA%\\arduino\\sketches\\  <- delete folder contents
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
