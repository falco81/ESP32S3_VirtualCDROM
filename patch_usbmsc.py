#!/usr/bin/env python3
"""
Patches USBMSC.cpp to handle CD-ROM SCSI commands.
v3 - places CD handlers OUTSIDE the media_present check
"""
import os, sys, shutil
from pathlib import Path

def find_usbmsc():
    base = Path(os.environ.get("LOCALAPPDATA","")) / "Arduino15"
    for p in (base / "packages/esp32/hardware/esp32").rglob("USBMSC.cpp"):
        return p
    return None

def restore_backup(path):
    bak = str(path) + ".bak"
    if os.path.exists(bak):
        shutil.copy2(bak, str(path))
        print(f"  Restored from backup")
        return True
    return False

def patch(path):
    with open(path, 'r', encoding='utf-8') as f:
        src = f.read()

    # Always restore from backup first to get clean version
    bak = str(path) + ".bak"
    if os.path.exists(bak):
        shutil.copy2(bak, str(path))
        with open(path, 'r', encoding='utf-8') as f:
            src = f.read()
        print("  Restored clean backup")
    else:
        # Create backup of current (possibly clean) version
        shutil.copy2(str(path), bak)
        print(f"  Backup created: {path.name}.bak")

    if 'CD-ROM SCSI patched v3' in src:
        print("  Already patched v3")
        return True

    # Find tud_msc_scsi_cb and its entire body
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
    print(f"  Found tud_msc_scsi_cb ({len(func_body)} bytes)")

    # Find the opening brace of the function body
    body_start = func_body.find('{')

    # Insert our CD handlers RIGHT AFTER the opening brace of the function
    # This way they run BEFORE any media_present checks
    injection = '''{ /* CD-ROM SCSI patched v3 - handles CD commands regardless of media state */
    extern uint32_t cdromBlockCount;
    switch (scsi_cmd[0]) {
      case 0x43: { /* READ TOC */
        if (bufsize >= 20) {
          uint32_t bc = cdromBlockCount;
          uint8_t toc[20] = {
            0x00,0x12,0x01,0x01,
            0x00,0x14,0x01,0x00,0x00,0x00,0x00,0x00,
            0x00,0x14,0xAA,0x00,
            (uint8_t)(bc>>24),(uint8_t)(bc>>16),(uint8_t)(bc>>8),(uint8_t)bc
          };
          uint16_t len = bufsize < 20 ? (uint16_t)bufsize : 20;
          memcpy(buffer, toc, len);
          return (int32_t)len;
        }
        break;
      }
      case 0x46: { /* GET CONFIGURATION */
        if (bufsize >= 8) {
          memset(buffer,0,8);
          ((uint8_t*)buffer)[3]=4; ((uint8_t*)buffer)[7]=8;
          return 8;
        }
        break;
      }
      case 0x51: { /* READ DISC INFO */
        if (bufsize >= 34) {
          memset(buffer,0,34);
          ((uint8_t*)buffer)[1]=0x20; ((uint8_t*)buffer)[2]=0x0E;
          ((uint8_t*)buffer)[3]=((uint8_t*)buffer)[4]=
          ((uint8_t*)buffer)[5]=((uint8_t*)buffer)[6]=1;
          return 34;
        }
        break;
      }
      case 0x1A: { /* MODE SENSE 6 */
        if (bufsize >= 4) { memset(buffer,0,4); ((uint8_t*)buffer)[0]=3; return 4; }
        break;
      }
      case 0x5A: { /* MODE SENSE 10 */
        if (bufsize >= 8) { memset(buffer,0,8); ((uint8_t*)buffer)[1]=6; return 8; }
        break;
      }
      case 0x1E: return 0; /* PREVENT REMOVAL */
      case 0xBD: { /* MECH STATUS */
        uint16_t len = bufsize < 8 ? (uint16_t)bufsize : 8;
        memset(buffer,0,len); return (int32_t)len;
      }
      default: break;
    }
  }
  /* end CD-ROM SCSI patched v3 */
  '''

    new_func_body = func_body[:body_start+1] + '\n  ' + injection + func_body[body_start+1:]
    new_src = src[:func_start] + new_func_body + src[func_end:]

    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_src)
    print(f"  Patched v3: CD handlers now run BEFORE media_present check")
    return True

def main():
    print("=" * 56)
    print("  USBMSC.cpp CD-ROM SCSI Patcher v3")
    print("  (fixes patch placement - was inside wrong if block)")
    print("=" * 56)

    path = find_usbmsc()
    if not path:
        print("ERROR: USBMSC.cpp not found"); sys.exit(1)

    print(f"\nFile: {path}\n")
    if patch(path):
        print(f"\n[SUCCESS]")
        print("  1. Delete build cache:")
        print("     %LOCALAPPDATA%\\arduino\\sketches\\  (delete all subfolders)")
        print("  2. Re-upload ESP32S3_VirtualCDROM.ino")
        print("  3. Mount ISO - should now show DATA disc")
    print("=" * 56)

if __name__ == "__main__":
    main()
