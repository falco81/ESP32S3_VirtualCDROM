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

    if 'CD-ROM SCSI patched v4' in src or 'scsi_audio_play' in src:
        print("  Already patched v4")
        return True

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
        "extern int      scsi_audio_state();\n"
        "extern void     scsi_audio_subchannel(uint8_t*);\n"
        "extern int      scsi_audio_track_count();\n"
        "extern int      scsi_audio_track_info(int, uint32_t*, uint32_t*);\n"
        "extern uint32_t cdromBlockCount_get();\n"
        "extern uint32_t cdromBlockCount;\n"
        "/* end audio bridge */\n\n"
    )

    # Switch block inserted inside the function (no extern declarations here)
    inner = (
        "{ /* CD-ROM SCSI patched v4 */\n"
        "    auto msfToLba = [](uint8_t m,uint8_t s,uint8_t f)->uint32_t{\n"
        "      return (uint32_t)m*60*75+(uint32_t)s*75+f; };\n"
        "    switch (scsi_cmd[0]) {\n"
        "      case 0x43: { /* READ TOC */\n"
        "        uint8_t msf=scsi_cmd[1]&0x02;\n"
        "        int na=scsi_audio_track_count(), tot=1+na;\n"
        "        int resp=4+(tot+1)*8; if((uint32_t)resp>bufsize)resp=(int)bufsize;\n"
        "        memset(buffer,0,resp); uint8_t*b=(uint8_t*)buffer;\n"
        "        uint16_t dl=(uint16_t)(resp-2);\n"
        "        b[0]=dl>>8;b[1]=dl&0xFF;b[2]=1;b[3]=(uint8_t)tot;\n"
        "        int off=4; b[off+1]=0x14;b[off+2]=1;off+=8;\n"
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
        "          uint32_t bc=cdromBlockCount;\n"
        "          b[off+1]=0x14;b[off+2]=0xAA;\n"
        "          b[off+4]=(uint8_t)(bc>>24);b[off+5]=(uint8_t)(bc>>16);\n"
        "          b[off+6]=(uint8_t)(bc>>8);b[off+7]=(uint8_t)(bc);\n"
        "        }\n"
        "        return resp;\n"
        "      }\n"
        "      case 0x42: { /* READ SUB-CHANNEL */\n"
        "        if(bufsize<16)break;\n"
        "        memset(buffer,0,16); uint8_t*b=(uint8_t*)buffer;\n"
        "        int st=scsi_audio_state();\n"
        "        b[1]=(st==1)?0x11:(st==2)?0x12:0x13;\n"
        "        b[3]=12;b[4]=0x01;b[5]=b[1];\n"
        "        uint8_t sub[8]={}; scsi_audio_subchannel(sub);\n"
        "        b[6]=sub[0];b[7]=sub[1];\n"
        "        b[9]=sub[2];b[10]=sub[3];b[11]=sub[4];\n"
        "        b[13]=sub[5];b[14]=sub[6];b[15]=sub[7];\n"
        "        return 16;\n"
        "      }\n"
        "      case 0x45: { /* PLAY AUDIO LBA */\n"
        "        uint32_t lba=((uint32_t)scsi_cmd[2]<<24)|((uint32_t)scsi_cmd[3]<<16)|\n"
        "                     ((uint32_t)scsi_cmd[4]<<8)|scsi_cmd[5];\n"
        "        uint16_t len=((uint16_t)scsi_cmd[7]<<8)|scsi_cmd[8];\n"
        "        scsi_audio_play(lba,lba+len); return 0;\n"
        "      }\n"
        "      case 0x47: { /* PLAY AUDIO MSF */\n"
        "        uint32_t s=msfToLba(scsi_cmd[3],scsi_cmd[4],scsi_cmd[5]);\n"
        "        uint32_t e=msfToLba(scsi_cmd[6],scsi_cmd[7],scsi_cmd[8]);\n"
        "        if(s>150)s-=150; if(e>150)e-=150;\n"
        "        scsi_audio_play(s,e); return 0;\n"
        "      }\n"
        "      case 0x48: { /* PLAY AUDIO TRACK INDEX */\n"
        "        uint8_t st=scsi_cmd[4],et=scsi_cmd[6];\n"
        "        uint32_t sl=0,el=0,tmp=0;\n"
        "        scsi_audio_track_info(st,&sl,&tmp);\n"
        "        if(scsi_audio_track_info(et,&el,&tmp))el+=tmp;\n"
        "        else el=cdromBlockCount;\n"
        "        scsi_audio_play(sl,el); return 0;\n"
        "      }\n"
        "      case 0x4B:{if(scsi_cmd[8]&1)scsi_audio_resume();else scsi_audio_pause();return 0;}\n"
        "      case 0x4E:{scsi_audio_stop();return 0;}\n"
        "      case 0x46:{if(bufsize>=8){memset(buffer,0,8);((uint8_t*)buffer)[3]=4;((uint8_t*)buffer)[7]=8;return 8;}break;}\n"
        "      case 0x51:{if(bufsize>=34){memset(buffer,0,34);\n"
        "        ((uint8_t*)buffer)[1]=0x20;((uint8_t*)buffer)[2]=0x0E;\n"
        "        ((uint8_t*)buffer)[3]=((uint8_t*)buffer)[4]=\n"
        "        ((uint8_t*)buffer)[5]=((uint8_t*)buffer)[6]=1;return 34;}break;}\n"
        "      case 0x1A:{if(bufsize>=4){memset(buffer,0,4);((uint8_t*)buffer)[0]=3;return 4;}break;}\n"
        "      case 0x5A:{if(bufsize>=8){memset(buffer,0,8);((uint8_t*)buffer)[1]=6;return 8;}break;}\n"
        "      case 0x1E:return 0;\n"
        "      case 0xBD:{uint16_t l=bufsize<8?(uint16_t)bufsize:8;memset(buffer,0,l);return(int32_t)l;}\n"
        "      default:break;\n"
        "    }\n"
        "  }\n"
        "  /* end CD-ROM SCSI patched v4 */\n"
        "  "
    )

    new_func_body = func_body[:body_start+1] + '\n  ' + inner + func_body[body_start+1:]
    new_src = src[:func_start] + file_scope + new_func_body + src[func_end:]

    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_src)
    print("  Patched v4: CD+Audio handlers (C bridge, no extern issues)")
    return True


def main():
    print("=" * 56)
    print("  USBMSC.cpp CD-ROM SCSI Patcher v4 — with audio SCSI commands")
    print("  (READ TOC multi-track, PLAY AUDIO, PAUSE/RESUME, READ SUB-CHANNEL)")
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
