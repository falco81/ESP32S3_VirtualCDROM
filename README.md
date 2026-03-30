# ESP32-S3 Virtual CD-ROM Drive + File Manager

Firmware for the ESP32-S3 that emulates a USB CD-ROM drive. Disc images stored on an SD card are presented to the host PC as a standard optical drive — no drivers required. The device also acts as a Wi-Fi file manager and supports audio track playback from CUE images via an I2S DAC module.

---

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Complete Setup from Scratch](#complete-setup-from-scratch)
  - [1. Install Arduino IDE](#1-install-arduino-ide)
  - [2. Install ESP32 Board Package](#2-install-esp32-board-package)
  - [3. Install Libraries](#3-install-libraries)
  - [4. Arduino IDE Settings](#4-arduino-ide-settings)
  - [5. Install WSL with AlmaLinux](#5-install-wsl-with-almalinux)
  - [6. Install Tools in AlmaLinux](#6-install-tools-in-almalinux)
  - [7. Run build_exfat_libs.py](#7-run-build_exfat_libspy)
  - [8. Compile and Upload Firmware](#8-compile-and-upload-firmware)
  - [9. First Boot and Configuration](#9-first-boot-and-configuration)
- [Arduino IDE Settings Reference](#arduino-ide-settings-reference)
- [Web Interface](#web-interface)
- [Serial CLI](#serial-cli)
- [Wi-Fi Configuration](#wi-fi-configuration)
- [802.1x Enterprise Wi-Fi (EAP)](#8021x-enterprise-wi-fi-eap)
- [Web UI Authentication](#web-ui-authentication)
- [Audio CD Playback](#audio-cd-playback)
- [Supported Disc Image Formats](#supported-disc-image-formats)
- [Python Scripts](#python-scripts)
- [API Reference](#api-reference)
- [RGB LED Status Codes](#rgb-led-status-codes)
- [Troubleshooting](#troubleshooting)
- [Building and Flashing Without Arduino IDE](#building-and-flashing-without-arduino-ide)
- [Project Files](#project-files)

---

## Features

- **USB CD-ROM emulation** — USB MSC with CD-ROM SCSI profile; the PC sees an optical drive without any drivers
- **Disc image formats** — ISO 9660 (`.iso`), raw binary (`.bin`), CUE sheets (`.cue`) with multi-track support
- **Audio CD** — playback of audio tracks via GY-PCM5102 I2S DAC; SCSI PLAY AUDIO, READ SUB-CHANNEL, TOC for transparent emulation to games and players
- **Web file manager** — upload, download, delete, create folders, drag-and-drop
- **Wi-Fi** — WPA2-Personal, WPA2-Enterprise PEAP, WPA2-Enterprise EAP-TLS with full certificate management
- **mDNS** — device reachable at `hostname.local`; FQDN support
- **NVS persistence** — all settings survive reboot
- **Serial CLI** — complete configuration and diagnostics at 115 200 baud
- **exFAT SD cards** — supported after applying the `build_exfat_libs.py` patch
- **Web UI authentication** — HTTP Basic Auth, enable/disable, username/password via web or CLI, disabled by default
- **Bidirectional audio sync** — PC controls playback via SCSI, HTML syncs within 400 ms
- **RGB LED** — boot / Wi-Fi / error state indication

---

## Hardware Requirements

| Component | Notes |
|---|---|
| ESP32-S3 DevKitC-1 N16R8 | 16 MB flash, 8 MB OPI PSRAM — tested on this variant |
| MicroSD card + module | 3.3 V logic — **do not use 5 V modules without a level shifter** |
| USB-A to USB-C cable x2 | One for UART/programming, one OTG to the target PC |
| GY-PCM5102 I2S module | Optional — for audio track playback from CUE images |

![ESP32-S3 DevKitC-1 N16R8](doc/esp32s3.png)

---

## Wiring

### SD Card (SD_MMC 1-bit mode)

```
ESP32-S3 GPIO12  ->  CLK  (D5)
ESP32-S3 GPIO11  ->  CMD  (D7 / MOSI)
ESP32-S3 GPIO13  ->  D0   (D6 / MISO)
ESP32-S3 GPIO10  ->  CS   (D4 / D3)
ESP32-S3 3V3     ->  VCC
ESP32-S3 GND     ->  GND
```

### GY-PCM5102 I2S Audio (optional)

![GY-PCM5102](doc/GY-PCM5102.png)

```
ESP32-S3 GPIO14  ->  BCK  (Bit Clock)
ESP32-S3 GPIO15  ->  LCK  (Word Select / WS)
ESP32-S3 GPIO16  ->  DIN  (Data In)
ESP32-S3 3V3     ->  VCC
ESP32-S3 GND     ->  GND
GND              ->  FMT  (I2S Philips format -- connect to GND)
GND              ->  SCK  (no master clock -- connect to GND)
```

> **Important:** The SD module must be powered from **3.3 V**, not 5 V. The ESP32-S3 DevKitC-1 has **two** USB connectors -- the left one is UART (programming and serial monitor), the right one is USB OTG (virtual CD-ROM for the target PC). Never swap them.

---

## Complete Setup from Scratch

### 1. Install Arduino IDE

1. Download **Arduino IDE 2.x** from https://www.arduino.cc/en/software
2. Run the installer with default settings
3. Launch Arduino IDE

---

### 2. Install ESP32 Board Package

1. Open **File -> Preferences**
2. Add to **Additional boards manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Click OK
4. Open **Tools -> Board -> Boards Manager**
5. Search for `esp32`
6. Install **esp32 by Espressif Systems** version **3.3.7**

   > Download takes 5-15 minutes (~500 MB of toolchain).

---

### 3. Install Libraries

Open **Sketch -> Include Library -> Manage Libraries** and install:

- **Adafruit NeoPixel** (search "neopixel", author Adafruit)

All other dependencies are part of the ESP32 Arduino core and do not require separate installation.

---

### 4. Arduino IDE Settings

Copy the project files into one folder:
```
ESP32S3_VirtualCDROM.ino
html_page.h
partitions.csv
```

Open `ESP32S3_VirtualCDROM.ino` in Arduino IDE and configure in the **Tools** menu:

| Setting | Value |
|---|---|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Disabled** |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | **Custom** |
| PSRAM | **OPI PSRAM** |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 921600 |
| USB Mode | **USB-OTG (TinyUSB)** |

![Arduino IDE Settings](doc/arduino_ide_settings.png)

> **Critical:** Board must be `ESP32S3 Dev Module`, **never** `ESP32-S3-USB-OTG` -- that one has 8 MB flash hardcoded and the sketch will not fit. Flash Size must be **16MB (128Mb)** for the Custom partition scheme to work. USB Mode must be **USB-OTG (TinyUSB)** for the virtual CD-ROM.

The `partitions.csv` file from the project folder provides a 6 MB application partition. With `Partition Scheme: Custom` Arduino IDE will find and use it automatically.

---

### 5. Install WSL with AlmaLinux

The `build_exfat_libs.py` script requires a Linux environment to compile ESP-IDF. The recommended distribution is **AlmaLinux 9** (RHEL-compatible, stable, proven with this project).

#### 5.1 Enable WSL on Windows

Open **PowerShell as Administrator** (right-click Start -> Windows PowerShell (Admin)):

```powershell
wsl --install --no-distribution
```

Restart if Windows prompts you to. Verify WSL is working:
```powershell
wsl --status
```

#### 5.2 Install AlmaLinux 9

```powershell
wsl --list --online
wsl --install -d AlmaLinux-9
```

Alternatively install **AlmaLinux 9** directly from the Microsoft Store.

On first launch set a username and password (the WSL account, unrelated to Windows).

#### 5.3 Launch WSL

```powershell
wsl -d AlmaLinux-9
```

or simply `wsl` if AlmaLinux is the default distribution.

#### 5.4 Verify Access to Windows Files

Drive C: is accessible in WSL as `/mnt/c`:

```bash
ls /mnt/c/Users/
ls /mnt/c/Users/Administrator/AppData/Local/Arduino15/
```

---

### 6. Install Tools in AlmaLinux

```bash
# Update the system
sudo dnf update -y

# Core build tools (gcc, make, etc.)
sudo dnf groupinstall -y "Development Tools"

# Packages required for the ESP-IDF build
sudo dnf install -y \
    git python3 python3-pip cmake ninja-build ccache \
    wget flex bison gperf openssl-devel ncurses-devel \
    libffi-devel zlib-devel binutils file

# Python packages
pip3 install --user pyserial

# Verify
git --version      # 2.x
cmake --version    # 3.x
ninja --version    # 1.x
python3 --version  # 3.x
```

> The ESP-IDF toolchain (xtensa-esp-elf compiler, etc.) is downloaded automatically on the first run of `build.sh`. No manual toolchain installation is needed.

#### Troubleshooting AlmaLinux

If `sudo` does not work:
```bash
su -
usermod -aG wheel your_username
exit
# Log out and back in to WSL
```

If `dnf` reports repository errors:
```bash
sudo dnf clean all
sudo dnf makecache
```

---

### 7. Run build_exfat_libs.py

This script does everything automatically. Run it in the **WSL/AlmaLinux terminal**.

#### Preparation -- copy the script to WSL

```bash
cp /mnt/c/Users/Administrator/Downloads/build_exfat_libs.py ~/
cd ~
```

#### First run (full build -- takes 1 to 3 hours)

```bash
python3 build_exfat_libs.py
```

Expected console output:

```
============================================================
  ESP32 Arduino Lib Builder -- exFAT Support
============================================================

>>> Locating Arduino15
    Auto-detected ESP32 Arduino version: 3.3.7
  + Arduino15: /mnt/c/Users/Administrator/AppData/Local/Arduino15

>>> Cloning esp32-arduino-lib-builder...
>>> Running build.sh -t esp32s3 ...   (1-3 hours on first run)
  + build.sh completed

>>> Patching ffconf.h
  + Patched: ffconf.h (FF_FS_EXFAT: 0 -> 1)

>>> Recompiling fatfs target (~10 seconds)
  + fatfs compiled

>>> Installing libfatfs.a to Arduino15
  + Installed: .../esp32s3-libs/3.3.7/lib/libfatfs.a

>>> Patching sdkconfig.h (5 files)
  + Patched: dio_opi, dio_qspi, opi_opi, qio_opi, qio_qspi

>>> Applying USBMSC.cpp CD-ROM + Audio SCSI patch v4
  + USBMSC.cpp patched: CD-ROM + Audio SCSI handlers (v4)

  RESULT: PASS -- all checks passed (18/18)
```

#### Subsequent runs (quick mode -- ~10 seconds)

After the full build has been done once, or after updating the board package in Arduino IDE:

```bash
python3 build_exfat_libs.py --skip-full-build
```

#### Verify without making changes

```bash
python3 build_exfat_libs.py --test
# Output: RESULT: PASS -- all checks passed (18/18)
```

#### Restore original files

```bash
python3 build_exfat_libs.py --restore
```

#### All flags

| Flag | Description |
|---|---|
| *(none)* | Full build + all patches |
| `--skip-full-build` | Skip build.sh, only patch and install (~10s) |
| `--skip-usbmsc` | Skip USBMSC.cpp patch |
| `--arduino-version X.X.X` | Force a specific version instead of auto-detect |
| `--arduino15 /path` | Override auto-detected Arduino15 path |
| `--test` | Verify current state, no changes made |
| `--dry-run` | Show what would happen, no changes made |
| `--restore` | Restore all files from backup copies |

---

### 8. Compile and Upload Firmware

After `build_exfat_libs.py` completes successfully:

#### 8.1 Delete the Arduino build cache

This step is **mandatory** -- without it Arduino IDE will use old compiled objects and the exFAT/USBMSC patch will not take effect.

Open File Explorer and navigate to:
```
%LOCALAPPDATA%\arduino\
```
Delete the **entire `arduino` folder** (or all its contents).

#### 8.2 Restart Arduino IDE

Close completely and reopen.

#### 8.3 Select the port

Connect the ESP32-S3 using the UART USB cable (left connector -- labelled UART or COM).

In Arduino IDE: **Tools -> Port** -> select the port with `CH343`, `CP2102`, or `Silicon Labs`.

> If the port does not appear, install the CH343 driver: https://www.wch-ic.com/products/CH343.html

#### 8.4 Upload

Click the Upload arrow or **Sketch -> Upload**. The first compile takes 3-5 minutes.

#### 8.5 Verify

Open **Tools -> Serial Monitor** at **115200 baud**. A successful boot looks like:

```
[SD]   exFAT support: YES (compiled in)
[WiFi] Connected!  IP: 192.168.x.x
[OK]  HTTP server started at http://192.168.x.x
```

---

### 9. First Boot and Configuration

#### Basic Wi-Fi (WPA2-Personal)

In the Serial Monitor:
```
set ssid YourNetworkName
set pass YourPassword
wifi reconnect
```

#### After connecting

Open a browser at `http://cd.local` or `http://192.168.x.x`.

#### Set a default disc image

```
mount /path/to/image.iso
```

In the web interface click **Set default** -- the image will mount automatically on every boot.

#### Enable the audio module (if you have GY-PCM5102)

Via serial:
```
set audio-module on
reboot
```

Or via web: **Config tab -> Audio Module -> GY-PCM5102 I2S -- GPIO 14/15/16 -> Save -> Reboot**.

---

## Arduino IDE Settings Reference

| Setting | Value | Why |
|---|---|---|
| Board | ESP32S3 Dev Module | Correct config for N16R8; ESP32-S3-USB-OTG has 8 MB flash hardcoded |
| USB CDC On Boot | Disabled | USB OTG must act as CD-ROM, not as CDC serial |
| Flash Size | 16MB (128Mb) | Required for Custom partition; linker rejects the sketch otherwise |
| Partition Scheme | Custom | `partitions.csv` provides 6 MB for the application |
| PSRAM | OPI PSRAM | N16R8 has OPI PSRAM, not QSPI |
| USB Mode | USB-OTG (TinyUSB) | OTG port = virtual CD-ROM |

![Arduino IDE Settings](doc/arduino_ide_settings.png)

---

## Web Interface

Available at `http://cd.local` or via the device IP address.

**CD-ROM** -- browse the SD card, mount/eject disc images, set the default image loaded on boot.

**Audio** -- appears automatically when a CUE image with audio tracks is mounted. Contains: progress bar (clickable seek), play/pause/stop/prev/next controls, volume slider, mute toggle, scrollable track list. Tab is greyed out when no audio CD is mounted.

**File Manager** -- full SD card file manager, drag-and-drop upload, download, delete, create folders.

**Log** -- live output stream (same messages as the serial port).

**Status** -- real-time device status: Wi-Fi, SD card, disc image, EAP certificate details, audio module, system info.

**Config** -- complete configuration without a serial cable. Sections:
- **WiFi** -- SSID, password, network scan
- **Network** -- DHCP/Static, IP/mask/gateway/DNS, Hostname with live mDNS preview
- **802.1x Enterprise WiFi** -- EAP Mode, identity, **Scan SD** for auto-detection of `.pem/.crt/.key` files, CA cert, Client cert/key, passphrase
- **Audio Module** -- PCM5102 I2S enable/disable dropdown
- **Web UI Authentication** -- enable/disable HTTP Basic Auth, username, password (write-only)
- **Actions** -- Save, Reboot, Factory reset, SD unmount/mount

---

## Serial CLI

Connect any serial terminal at **115 200 baud, 8N1**.

| Command | Description |
|---|---|
| `help` | Command list (sections: 802.1x / Web Auth / Commands) |
| `show config` | Full configuration + runtime state |
| `status` | Current Wi-Fi, SD, disc image, audio state |
| `show files [path]` | Recursive SD card listing |
| `mount <file>` | Mount ISO/BIN/CUE |
| `umount` | Eject current disc |
| `sd reinit` | Reinitialize SD card |
| `wifi reconnect` | Disconnect and reconnect Wi-Fi |
| `reboot` | Restart device |
| `clear config` | Factory reset (erases all NVS settings) |

**`set` commands:**

| Key | Description |
|---|---|
| `ssid` | Wi-Fi network name |
| `pass` | WPA2-Personal password |
| `dhcp on/off` | Enable/disable DHCP |
| `ip / mask / gw / dns` | Static network settings |
| `hostname` | Hostname or FQDN |
| `eap-mode 0/1/2` | 0=off, 1=PEAP, 2=EAP-TLS |
| `eap-id / eap-user / eap-pass` | EAP credentials |
| `eap-ca / eap-cert / eap-key` | Paths to certificates on SD card |
| `eap-kpass` | Passphrase for an encrypted private key |
| `audio-module on/off` | PCM5102 I2S audio module |
| `web-auth on/off` | HTTP Basic Auth (default: off) |
| `web-user <name>` | Web UI username (default: admin) |
| `web-pass <password>` | Web UI password (default: admin) |

---

## Wi-Fi Configuration

The firmware supports both DHCP and static IP. The configured hostname is sent to the DHCP server and used for mDNS. A FQDN (e.g. `cd.corp.net`) automatically creates the mDNS alias `cd.local`.

---

## 802.1x Enterprise Wi-Fi (EAP)

### PEAP

```
set eap-mode 1
set eap-id   user@corp.net
set eap-user user
set eap-pass Password123
set eap-ca   /wifi/ca.pem     (optional)
wifi reconnect
```

### EAP-TLS

```
set eap-mode 2
set eap-cert /wifi/client.crt
set eap-key  /wifi/client.key
set eap-ca   /wifi/ca.pem     (optional)
wifi reconnect
```

The EAP identity (`eap-id`) is optional in EAP-TLS mode -- the firmware extracts it from the certificate CN automatically.

### Certificate Requirements

- Format: **PEM** (base64, `-----BEGIN ...-----` header)
- Supported private key formats: PKCS#1, PKCS#8, EC, encrypted (set `eap-kpass`)
- RSA 2048-bit and 4096-bit both work under ESP-IDF 5.5 / mbedTLS 3.x

### FreeRADIUS Setup

```sql
INSERT INTO radcheck (username, attribute, value, op)
VALUES ('identity', 'Auth-Type', 'EAP', ':=');
```

---

## Web UI Authentication

The web interface can be protected with HTTP Basic Auth. Default state: **disabled** (no login required). Default credentials: `admin / admin`.

**Via serial CLI:**
```
set web-auth on          # enable authentication
set web-user myuser      # change username
set web-pass mysecret    # change password
show config              # verify: Web auth: enabled
```

**Via web Config tab:** Web UI Authentication section -> dropdown Enabled -> fill in Username and New password -> Save.

Password is **write-only** -- the API never returns it. Changes take effect immediately, no reboot needed.

---

## Audio CD Playback

### Bidirectional Synchronization

The HTML audio player synchronises with the PC player (and vice versa) within **400 ms**:
- PC sends STOP/PAUSE via SCSI -> firmware updates state within 13 ms -> HTML poll detects the change and redraws the UI
- HTML PLAY/PAUSE/STOP buttons -> firmware -> PC player sees the state immediately via READ SUB-CHANNEL

### Enable the Module

```
set audio-module on
reboot
```

Or via the web Config tab.

### How It Works

The CUE parser extracts audio tracks and their LBA positions. For **separate BIN file** layout (e.g. Tomb Raider -- one `.bin` per track), the parser computes *virtual disc LBAs*: the data track occupies LBA 0 to N-1, audio tracks follow sequentially. This is what gets reported in READ TOC so the SCSI host sees correct track positions and durations. Each BIN file is seeked to `(virtualLba - trackStart + pregap) * 2352` bytes for accurate playback.

A FreeRTOS audio task on core 0 reads raw PCM sectors from BIN files (2352 B/sector, skip 16 B header), applies software gain, and streams the data to I2S DMA -> PCM5102 -> line out. The PC controls playback via SCSI commands (READ TOC, PLAY AUDIO LBA/MSF/TRACK, PAUSE/RESUME, STOP, READ SUB-CHANNEL). READ TOC reports virtual disc LBAs with the standard 150-sector pregap offset so SCSI players show correct track positions and durations.

Games such as Tomb Raider, Quake, and Need for Speed communicate with the firmware transparently as if it were a real optical drive.

### Without the Module

If the audio module is disabled: CUE tracks are still parsed, the audio task tracks position in real time (75 fps), sub-channel data is correct. PC players function correctly (see tracks, get position) but there is no sound output.

### Connecting to a Mixer

The PCM5102 output is line level (~2 Vrms). Connect to **line-in** (AUX), not to a microphone input. Set volume to 100 and regulate on the mixer.

---

## Supported Disc Image Formats

| Format | Extension | Sector size | Header offset |
|---|---|---|---|
| ISO 9660 | `.iso` | 2048 B | 0 |
| Raw MODE1/2048 | `.bin` | 2048 B | 0 |
| Raw MODE1/2352 | `.bin` | 2352 B | 16 |
| Raw MODE2/2352 | `.bin` | 2352 B | 24 |
| Raw MODE2/2336 | `.bin` | 2336 B | 8 |
| CUE sheet (single BIN) | `.cue` | Per CUE | Per CUE |
| CUE sheet (separate BIN per track) | `.cue` | 2352 B | 16 or 24 B |

---

## Python Scripts

### `build_exfat_libs.py` (WSL/Linux)

The main script. Compiles `libfatfs.a` with exFAT support and patches `USBMSC.cpp`. Run in AlmaLinux/WSL.

**Why two files must be patched:**

- `libfatfs.a` -- compiled FATFS library with `FF_FS_EXFAT=1`
- `sdkconfig.h` -- header with `#define CONFIG_FATFS_EXFAT_SUPPORT 1`

The firmware checks `#ifdef CONFIG_FATFS_EXFAT_SUPPORT` at compile time. Without this define the serial output shows `NO` even if `libfatfs.a` is correct. The script patches both.

**Why `build.sh` runs `git reset --hard`:** The Espressif build script automatically reverts any changes at startup. It is therefore impossible to patch `ffconf.h` before running `build.sh`. The solution: run the full build first, patch afterwards, then recompile only the fatfs target.

After updating the board package in Arduino IDE run:

```bash
python3 build_exfat_libs.py --skip-full-build
```

### `patch_usbmsc.py` (Windows)

A standalone backup script for patching `USBMSC.cpp` without the exFAT build. Run from a Windows command prompt:

```cmd
python patch_usbmsc.py
```

---

## API Reference

| Endpoint | Description |
|---|---|
| `GET /api/sysinfo` | Full device status (JSON) |
| `GET /api/status` | Mounted image state |
| `GET /api/mount?file=` | Mount a disc image |
| `GET /api/umount` | Eject disc |
| `GET /api/isos` | List disc images on SD card |
| `GET /api/ls?path=` | Directory listing |
| `GET /api/config/get` | Read configuration (password omitted) |
| `GET /api/config/save?key=val` | Save configuration (includes webAuth/webUser/webPass) |
| `GET /api/wifi/scan` | Scan for Wi-Fi networks |
| `GET /api/audio/status` | Playback state + track list |
| `GET /api/audio/play?track=N` | Play track N |
| `GET /api/audio/play?lba=L&end=E` | Play LBA range |
| `GET /api/audio/stop` | Stop playback |
| `GET /api/audio/pause` | Pause playback |
| `GET /api/audio/resume` | Resume playback |
| `GET /api/audio/volume?v=0-100` | Set volume |
| `GET /api/audio/mute` | Toggle mute |
| `GET /api/audio/seek?track=N&rel=0.0-1.0` | Seek within a track |
| `GET /api/reboot` | Restart device |
| `GET /api/factory` | Factory reset |
| `GET /api/download?path=` | Download file from SD card |
| `POST /api/upload?path=` | Upload file to SD card |
| `GET /api/delete?path=` | Delete file |
| `GET /api/mkdir?path=` | Create folder |
| `GET /api/sd/unmount` | Safely unmount SD card |
| `GET /api/sd/mount` | Remount SD card |

---

## RGB LED Status Codes

| Colour | State |
|---|---|
| Off | Normal operation |
| Blue (pulsing) | Connecting to Wi-Fi |
| Green (solid) | Wi-Fi connected |
| Yellow (solid) | Disc mounted, USB active |
| Red (solid) | Error condition |

---

## Troubleshooting

### SD card not detected

- VCC must be **3.3 V** (not 5 V)
- Check wiring: CLK->GPIO12, CMD->GPIO11, D0->GPIO13, CS->GPIO10
- Try a different SD card
- Verify formatting: FAT32 or exFAT (exFAT requires `build_exfat_libs.py`)

### USB CD-ROM not appearing on the PC

- The **right** USB connector must be plugged in (OTG, not UART)
- Tools -> USB Mode: **USB-OTG (TinyUSB)** (not Hardware CDC)
- Verify `build_exfat_libs.py` completed -- USBMSC patch v4 must be applied
- Delete the build cache and re-upload firmware after patching
- Try `umount` then `mount` from the serial CLI

### `[SD] exFAT support: NO` after compile

1. Run `python3 build_exfat_libs.py --test` -- must show 18/18 PASS
2. Delete the **entire** `%LOCALAPPDATA%\arduino` folder
3. Restart Arduino IDE
4. Recompile

### SCSI player shows wrong track durations or thousands of hours total

This happens when the CUE image uses separate BIN files per track (e.g. Tomb Raider). The firmware assigns virtual disc LBAs after parsing -- check serial output for confirmation:

```
[CUE] Data track sectors from file: 146011
[CUE] Virtual disc LBAs assigned, lead-out LBA=215612
[CUE] Track 02: LBA 146011  len 14593  (~194s)
```

If you see `LBA 0` for all audio tracks, re-flash with the latest firmware which includes the virtual LBA fix.

### Audio tab is always greyed out after page load

- The tab activates within 3 seconds of page load (background poll)
- Verify serial output shows: `[CUE] 16 audio track(s) found`
- Check that the BIN files for audio tracks exist on the SD card

### EAP-TLS: NAK loop or authentication failure

Diagnostics in WSL:
```bash
# Verify key matches certificate (both MD5 hashes must match)
openssl x509 -noout -modulus -in client.crt | md5sum
openssl rsa  -noout -modulus -in client.key | md5sum

# Check signature algorithm (must be sha256WithRSAEncryption)
openssl x509 -in client.crt -noout -text | grep "Signature Algorithm"
```

Common causes:
- EAP identity not present in RADIUS `radcheck` table
- Certificate uses SHA-512 (not supported by ESP32 mbedTLS) -- regenerate with SHA-256
- Encrypted private key without `eap-kpass` configured

### Flash script says "Python not found" (Windows)

Install Python 3 from https://www.python.org/downloads/ — during installation tick **"Add Python to PATH"**.

### Flash script fails with timeout

The device may not be in download mode:
1. Hold the **BOOT** button on the ESP32-S3
2. Press and release **RESET**
3. Release **BOOT**
4. Run the flash script immediately

### Forgotten web UI password

Via serial CLI (does not require web access):
```
set web-auth off         # disable auth -- allow access without login
set web-pass newpassword # or change the password directly
```

Or factory reset: `clear config` -- restores default admin/admin with auth disabled.

### `flash_parts: partition 3 invalid` -- boot loop

Board is configured incorrectly:
- Tools -> **Board: ESP32S3 Dev Module** (not ESP32-S3-USB-OTG)
- Tools -> **Flash Size: 16MB (128Mb)**
- Tools -> **Partition Scheme: Custom**

### `build_exfat_libs.py` fails at cmake/ninja

```bash
sudo dnf install -y python3-devel openssl-devel ncurses-devel libffi-devel
pip3 install --user cryptography future pyparsing pyserial
```

### `build_exfat_libs.py` cannot find Arduino15

```bash
ls /mnt/c/Users/Administrator/AppData/Local/Arduino15/

# Pass the path explicitly
python3 build_exfat_libs.py --arduino15 "/mnt/c/Users/OtherName/AppData/Local/Arduino15"
```

### Web interface shows "Failed to fetch"

- The device and the browser must be on the same network
- Verify `show config` -> Wi-Fi connected = true
- Use the IP address directly if mDNS is blocked by a firewall or VPN

---

## Building and Flashing Without Arduino IDE

Once you have compiled the firmware at least once (with all patches applied), you can export the binary and distribute it so other users can flash the device without installing Arduino IDE or running any build scripts.

---

### Exporting the Compiled Binary from Arduino IDE

After a successful compile, go to **Sketch -> Export Compiled Binary**.

Arduino IDE creates a `build/` subfolder in the sketch directory. The file you need for distribution is:

```
build/esp32.esp32.esp32s3/ESP32S3_VirtualCDROM.ino.merged.bin
```

This is a **single merged image** (16 MB) containing the bootloader, partition table, boot stub and firmware at the correct offsets. Rename it to `firmware_merged.bin` for distribution.

> The `merged.bin` is all you need -- no separate bootloader/partitions/boot_app0 files required.

---

### Flash Address

| File | Address |
|---|---|
| `firmware_merged.bin` | `0x0000` |

The merged image is pre-assembled at the correct internal offsets. Flash it to address `0x0` and you are done.

---

### Flashing with the Included Scripts

Place `firmware_merged.bin` in the same folder as the flash scripts and run:

**Windows** -- double-click `flash_windows.bat`

**Linux / macOS:**
```bash
chmod +x flash_linux.sh
./flash_linux.sh
```

Both scripts automatically install `esptool` via pip if it is not already available.

**Important:** Always use the **LEFT** USB connector (UART/CH343) for flashing. The right connector is OTG and cannot be used for programming.

---

### Flashing with ESP Flash Download Tool (GUI, Windows)

Download from: https://www.espressif.com/en/support/download/other-tools

Settings:
- **Chip**: ESP32-S3
- **SPI Speed**: 80 MHz
- **SPI Mode**: DIO
- **Flash Size**: 16 MB (128 Mb)

Add one file: `firmware_merged.bin` at address `0x0000`, then click **START**.

---

### Troubleshooting Flash

**Port not detected:**
- Install the CH343 driver: https://www.wch-ic.com/products/CH343.html
- On Linux: `sudo chmod a+rw /dev/ttyUSB0`

**Flash fails / timeout:**
1. Hold the **BOOT** button on the ESP32-S3
2. Press and release **RESET**
3. Release **BOOT**
4. Run the flash script immediately -- the device is now in download mode

**After flashing:**
1. Disconnect the UART cable
2. Connect the OTG USB cable (right connector) to your target PC
3. Open `http://espcd.local` in a browser
4. Configure Wi-Fi in the Config tab

---

### GitHub Release Structure

When publishing a release, include:
```
release/
├── firmware_merged.bin   <- complete firmware image (single file, 16 MB)
├── flash_windows.bat     <- Windows flash script (double-click)
└── flash_linux.sh        <- Linux/macOS flash script
```

Upload the folder as a `.zip` file attached to the GitHub Release.

---


## Project Files

| File | Description |
|---|---|
| `ESP32S3_VirtualCDROM.ino` | Main firmware (Arduino C++) |
| `html_page.h` | Web interface as an inline C string |
| `partitions.csv` | Custom partition table (6 MB app) |
| `build_exfat_libs.py` | exFAT + USBMSC patch script -- run in WSL |
| `patch_usbmsc.py` | Standalone USBMSC patch -- run on Windows |
| `flash_windows.bat` | Windows flash script -- double-click to flash |
| `flash_linux.sh` | Linux/macOS flash script |
| `doc/arduino_ide_settings.png` | Arduino IDE settings screenshot |
| `doc/esp32s3.png` | ESP32-S3 DevKitC-1 board photo |
| `doc/GY-PCM5102.png` | GY-PCM5102 I2S module photo |
| `doc/wiring.svg` | Wiring diagram |

All `.ino` and `.h` files must be in the same folder.
