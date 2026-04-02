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
- [DOS Compatibility Mode](#dos-compatibility-mode)
- [DOS / Retro PC Compatibility](#dos--retro-pc-compatibility)
  - [DOS Audio CD Player Compatibility](#dos-audio-cd-player-compatibility)
- [ESPUSBCD.SYS — Custom DOS CD-ROM Driver](#espusbcdsys--custom-dos-cd-rom-driver)
  - [Architecture](#architecture)
  - [DOS CD-ROM Driver Interface](#dos-cd-rom-driver-interface)
  - [ASPI Interface](#aspi-interface)
  - [IOCTL Functions Implemented](#ioctl-functions-implemented)
  - [SCSI Commands Used](#scsi-commands-used)
  - [Memory Layout](#memory-layout)
  - [Building from Source](#building-from-source)
- [Web UI Authentication](#web-ui-authentication)
- [HTTPS / TLS](#https--tls)
- [Audio CD Playback](#audio-cd-playback)
- [SCSI Command Reference](#scsi-command-reference)
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
- **Disc image formats** — ISO 9660 (`.iso`), raw binary (`.bin`), CUE sheets (`.cue`) with full multi-track support including all raw sector formats
- **Audio CD** — playback of audio tracks via GY-PCM5102 I2S DAC; full SCSI-2 audio command set per Pioneer OB-U0077C spec (PLAY AUDIO, READ TOC, READ SUB-CHANNEL, PAUSE/RESUME, STOP, TRACK RELATIVE); audio enabled for non-DOS mode (Windows/Linux) and DOS drivers 0, 1, 2; driver 3 (DI1000DD) is data-only by design
- **CUE parser** — supports all sector formats (MODE1/2352, MODE2/2352, MODE2/2336, MODE1/2048, MODE2/2048, CDG/2448), physical pregap via INDEX 00, virtual pregap via PREGAP directive, single-BIN and separate-BIN-per-track layouts
- **Web file manager** — upload, download, delete, create folders, drag-and-drop
- **Wi-Fi** — WPA2-Personal, WPA2-Enterprise PEAP, WPA2-Enterprise EAP-TLS with full certificate management
- **mDNS** — device reachable at `hostname.local`; FQDN support
- **NVS persistence** — all settings survive reboot
- **Serial CLI** — complete configuration and diagnostics at 115 200 baud
- **exFAT SD cards** — supported after applying the `build_exfat_libs.py` patch
- **Web UI authentication** — HTTP Basic Auth, enable/disable, username/password via web or CLI, disabled by default
- **HTTPS / TLS** — optional encrypted HTTPS on port 443; certificate and key loaded from SD card; HTTP auto-redirects to HTTPS; ESP32-S3 hardware AES/SHA/RSA acceleration
- **Bidirectional audio sync** — PC controls playback via SCSI, HTML syncs within 400 ms
- **Audio module runtime toggle** — enable or disable the I2S module without reboot via web UI or CLI
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

![Wiring Diagram](doc/wiring.svg)

### SD Card (SD_MMC 1-bit mode)

<table>
<tr>
<td><img src="doc/sd_front.png" alt="LOLIN MicroSD Card Shield V1.2.0 – front" width="200"/></td>
<td><img src="doc/sd_back.png" alt="LOLIN MicroSD Card Shield V1.2.0 – back (pinout)" width="200"/></td>
</tr>
<tr>
<td align="center">LOLIN MicroSD Shield V1.2.0 – front</td>
<td align="center">LOLIN MicroSD Shield V1.2.0 – back (SPI pinout)</td>
</tr>
</table>

The project uses the **LOLIN/Wemos MicroSD Card Shield** connected to the ESP32-S3 via SD_MMC peripheral in 1-bit mode (not SPI). The SPI labels on the PCB back (SCK, MISO, MOSI) correspond to the D-numbered GPIOs on the D1 Mini — use the GPIO numbers below for ESP32-S3.

| ESP32-S3 | SD Module | Signal |
|---|---|---|
| GPIO 12 | CLK (SCK) | Clock |
| GPIO 11 | CMD (MOSI) | Command |
| GPIO 13 | D0 (MISO) | Data 0 |
| GPIO 10 | D3/CS (TF-CS) | Chip Select |
| 3V3 | 3V3 | Power |
| GND | GND | Ground |

### GY-PCM5102 I2S Audio (optional)

![GY-PCM5102](doc/GY-PCM5102.png)

| ESP32-S3 | PCM5102 | Signal |
|---|---|---|
| GPIO 14 | BCK | Bit Clock |
| GPIO 15 | LCK | Word Select (WS) |
| GPIO 16 | DIN | Data In |
| 3V3 | VIN | Power |
| GND | GND | Ground |
| GND | FMT | I2S Philips format |
| GND | SCK | No master clock |

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

>>> Applying USBMSC.cpp CD-ROM + Audio SCSI patch v14
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
```

Or via web: **Config tab -> Audio Module -> GY-PCM5102 I2S -- GPIO 14/15/16 -> Save**.

The I2S module is initialised immediately at runtime — no reboot required. Disabling the module also takes effect immediately and stops any active playback.

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

**Config** -- complete configuration without a serial cable. Sections scroll freely; Save/Reboot/Factory Reset remain fixed at the bottom of the tab at all times. Sections:
- **WiFi** -- SSID, password, network scan
- **Network** -- DHCP/Static, IP/mask/gateway/DNS, Hostname with live mDNS preview
- **802.1x Enterprise WiFi** -- EAP Mode, identity, **Scan SD** for auto-detection of `.pem/.crt/.key` files, CA cert, Client cert/key, passphrase
- **Audio Module** -- PCM5102 I2S enable/disable dropdown; takes effect immediately without reboot
- **Web UI Authentication** -- enable/disable HTTP Basic Auth, username, password (write-only)
- **DOS Compatibility Mode** -- UNIT ATTENTION instead of USB re-enum on mount/eject; expands to show DOS driver selector when enabled
- **HTTPS / TLS** — optional HTTPS on port 443 with certificate from SD card; HTTP auto-redirects to HTTPS; hardware AES/SHA/RSA acceleration
- **Actions** (fixed at bottom) -- Save & apply, Reboot, Factory reset, SD unmount/mount

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
| `audio-module on/off` | PCM5102 I2S audio module (takes effect immediately, no reboot required) |
| `dos-compat on/off` | DOS compatibility mode — no USB re-enum on mount; automatically set when a DOS driver is selected; disabling resets driver to 0 (default: off) |
| `dos-driver 0/1/2/3` | DOS CD-ROM driver identity (requires dos-compat on; see DOS Compatibility Mode) |
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

## DOS Compatibility Mode

When **DOS Compatibility Mode** is enabled, the drive never disconnects from USB when you switch disc images. Instead:

1. Host sends READ(10) — `mscReadCb` is disabled immediately at the start of the mount operation
2. Firmware parses and opens the new image file
3. `msc.mediaPresent(false)` is signalled briefly — MSCDEX detects no media
4. After a short wait, `msc.mediaPresent(true)` + SCSI UNIT ATTENTION (`06h/28h`) is signalled
5. UNIT ATTENTION persists across 3 consecutive TUR poll cycles to ensure the OS catches the disc-change event
6. MSCDEX re-reads the TOC — new disc appears without any USB re-enumeration

Enable via CLI: `set dos-compat on`, or via the web Config tab.

### DOS Driver Modes

DOS driver mode is only available when DOS Compatibility Mode is enabled. Selecting a driver mode automatically enables DOS compat; disabling DOS compat resets the driver to Generic.

| Driver | Value | INQUIRY Identity | Audio | Notes |
|---|---|---|---|---|
| Generic | `0` | ESP32-S3 / Virtual CD-ROM | ✅ Full | Works with any ASPI-compatible driver + MSCDEX |
| USBCD2/TEAC | `1` | TEAC / CD-56E (SCSI-2) | ✅ Handled | Driver communication broken — USBCD2 uses INT 13h AH=0x50, standard USBASPI does not provide this hook |
| ESPUSBCD/Panasonic | `2` | MATSHITA / CD-ROM CR-572 (SCSI-2) | ✅ Full | Requires `usbaspi1.sys` or `usbaspi2.sys` + `ESPUSBCD.SYS` (custom driver) |
| DI1000DD | `3` | ESP32-S3 / Virtual CD-ROM (SCSI-2) | ❌ Data-only | Requires `usbaspi1.sys` (Panasonic v2.20); no MSCDEX audio commands |

Via CLI: `set dos-driver 0` (Generic), `set dos-driver 2` (Panasonic/ESPUSBCD), etc.

The web UI respects DOS compat mode: the **Mount** button automatically ejects (`/api/umount`), waits 2.5 s, then mounts the new image (`/api/mount`) — all without a USB disconnect.

---

## DOS / Retro PC Compatibility

### Tested Hardware

The firmware has been tested on a real **DOS / Windows 98SE retro PC** — [DOSRetroPC by falco81](https://github.com/falco81/DOSRetroPC):

| Component | Detail |
|---|---|
| Motherboard | Octek Aristo Rhino 15 (Baby AT) |
| CPU | Intel Pentium MMX 200 MHz (Socket 7, 66 MHz FSB) |
| Chipset | Intel 430TX (North Bridge: 82439TX, South Bridge: 82371AB PIIX4) |
| **USB** | **Intel 82371AB PIIX4 — onboard UHCI (USB 1.1)** |
| OS | MS-DOS 7.1 / Windows 98SE |
| CD-ROM stack | Panasonic USBASPI.EXE + USBCD1.SYS |
| CD-ROM manager | SHSUCDX.COM (replaces MSCDEX) |

### Why This Works Where USBODE Does Not

[USBODE](https://github.com/danifunker/usbode) (Pi Zero 2W USB optical drive emulator) has known issues with Intel PIIX4 UHCI and the USBASPI/USBCD1 stack: the drive frequently fails to enumerate, disconnects during disc swaps, or does not respond to UNIT ATTENTION correctly.

| Issue | USBODE | ESP32-S3 Virtual CD-ROM |
|---|---|---|
| USB enumeration on PIIX4 UHCI | Unreliable | Stable |
| UNIT ATTENTION on disc swap | Inconsistent | Correct (SCSI 06h/28h) |
| DOS compat — no USB re-enum | Not implemented | Implemented |
| Disc swap without reboot | Requires reboot | Works (MSCDEX re-reads TOC) |
| Audio CD (PLAY/TOC/SUB-CH) | Partial | Full OB-U0077C spec |

### DOS Driver Setup

> **Note:** Sample `CONFIG.SYS` and `AUTOEXEC.BAT` files with correct DOS CRLF line endings are included in the project as ready-to-use templates. All files below assume drivers are stored in `C:\DRIVERS\`.

#### ESPUSBCD/Panasonic (Driver 2) — recommended, full audio

**CONFIG.SYS:**
```dos
DEVICE=C:\DRIVERS\USBASPI1.SYS /w /v
DEVICE=C:\DRIVERS\ESPUSBCD.SYS /D:ESPUSBCD
```

**AUTOEXEC.BAT:**
```dos
@ECHO OFF
LH C:\DRIVERS\SHSUCDX.COM /D:ESPUSBCD /Q
```

Set on ESP32: `set dos-compat on` + `set dos-driver 2`

This is the recommended configuration. ESPUSBCD.SYS is a custom DOS CD-ROM driver included in the project that provides complete MSCDEX audio and data support — see [ESPUSBCD.SYS — Custom DOS CD-ROM Driver](#espusbcdsys--custom-dos-cd-rom-driver) for full technical details.

#### Generic (Driver 0) — alternative identity

**CONFIG.SYS:**
```dos
DEVICE=C:\DRIVERS\USBASPI1.SYS /w /v
DEVICE=C:\DRIVERS\ESPUSBCD.SYS /D:ESPUSBCD
```

**AUTOEXEC.BAT:**
```dos
@ECHO OFF
LH C:\DRIVERS\SHSUCDX.COM /D:ESPUSBCD /Q
```

Set on ESP32: `set dos-compat on` + `set dos-driver 0`

Uses the same ESPUSBCD.SYS driver as mode 2, but the ESP32 presents a generic INQUIRY identity (`ESP32-S3 / Virtual CD-ROM`) instead of the Panasonic identity. Use this if mode 2 causes compatibility issues with specific ASPI managers.

#### DI1000DD (Driver 3) — data-only, no MSCDEX needed

**CONFIG.SYS:**
```dos
DEVICE=C:\DRIVERS\USBASPI1.SYS /w /v
DEVICE=C:\DRIVERS\DI1000DD.SYS
```

Set on ESP32: `set dos-compat on` + `set dos-driver 3`

DI1000DD accepts USB device type 0x05 (CD-ROM) natively and presents the disc as a DOS drive letter without MSCDEX. Data access only — no audio SCSI commands supported.

> **Important:** The ESP32-S3 must be powered and connected **before** the PC boots — USBASPI only scans the USB bus at driver init time during POST.

### DOS Audio CD Player Compatibility

The firmware has been tested with **CD Player for DOS 2.25e** (by Ben Lunt / Forever Young Software):

**Required for correct operation:**
- MODE SENSE page `0x0E` (Audio Control Parameters) — read by CD Player before playback
- MODE SELECT `15h`/`55h` — sent by CD Player to save volume settings
- UNIT ATTENTION (`06h/28h`) — for disc detection after swap

Without `MODE SENSE page 0x0E`, CD Player returns **"return error number: 3"** (MSCDEX error 3 = "Unknown command"). This has been fixed since patch v8 and is present in all current builds (v14).

### Disc Swap in DOS Without Rebooting

With DOS Compat enabled:

1. Open the ESP32-S3 web UI from another machine on the same WiFi
2. **CD-ROM tab** → select image → **Mount**
3. Firmware: eject → 2.5 s wait → mount → UNIT ATTENTION signalled
4. MSCDEX/SHSUCDX detects the new disc automatically — no reboot, no driver reload

Alternatively control via script from DOS (requires a network-capable DOS TCP stack or another PC):

```bat
REM ESPCD.BAT - switch virtual disc
@ECHO OFF
WGET http://192.168.40.110/api/umount -q -O NUL
PING -n 4 127.0.0.1 > NUL
WGET "http://192.168.40.110/api/mount?file=%1" -q -O NUL
ECHO Disc: %1
```

---

## ESPUSBCD.SYS — Custom DOS CD-ROM Driver

`ESPUSBCD.SYS` is a custom DOS CD-ROM character device driver written from scratch in x86 16-bit assembly (NASM) specifically for the ESP32-S3 Virtual CD-ROM project. It replaces the older closed-source `ESPUSB.SYS` and provides complete MSCDEX-compatible audio and data CD-ROM support through the standard DOS ASPI interface.

### Architecture

The DOS CD-ROM software stack with ESPUSBCD.SYS:

```
DOS application (game, cdplayer.exe, etc.)
        │  INT 2Fh (MSCDEX API)
        ▼
  SHSUCDX.COM / MSCDEX.EXE     ← CD-ROM redirector, file system layer
        │  DOS IOCTL / device commands (INT 21h)
        ▼
  ESPUSBCD.SYS                  ← our driver (character device "ESPUSBCD")
        │  INT 4Bh (ASPI SendASPICommand, SRB in DS:SI)
        ▼
  USBASPI1.SYS / USBASPI2.SYS  ← USB ASPI manager (unchanged, Panasonic)
        │  USB Bulk-Only Transport (BOT)
        ▼
  ESP32-S3 Virtual CD-ROM       ← USB FS device, presents as CD-ROM (device type 05h)
```

ESPUSBCD.SYS sits between the MSCDEX redirector and the USBASPI transport layer. It translates every MSCDEX IOCTL request into the appropriate SCSI command, sends it via ASPI, and returns the result in the format MSCDEX expects. It does not contain any USB code — that is entirely handled by USBASPI.

### DOS CD-ROM Driver Interface

DOS CD-ROM drivers are character device drivers with a special interface documented in the Microsoft *Hardware-Independent Device Driver Specification for CD-ROM*. The driver presents itself to DOS via a 22-byte device header at offset 0 of the `.SYS` file:

```
Offset  Size  Content
0       4     Next driver pointer (FFFF:FFFF = last in chain)
4       2     Device attributes: 0xC800
                bit 15 = 1 (character device)
                bit 14 = 1 (IOCTL supported)
                bit 11 = 1 (Open/Close/RM supported)
6       2     Strategy routine offset
8       2     Interrupt routine offset
10      8     Device name 'ESPUSBCD' (overridden by /D: switch)
18      2     Reserved (0)
20      1     First drive letter (set by DOS)
21      1     Number of subunits = 1
```

The device attributes `0xC800` mark the driver as a character device that supports IOCTL and open/close operations. MSCDEX locates the driver by scanning the DOS device chain for a character device whose 8-byte name matches the `/D:` parameter given to MSCDEX/SHSUCDX.

**Request dispatch:** DOS calls the strategy routine (with ES:BX = request header address), then the interrupt routine. The interrupt routine dispatches by command code at offset +2 of the request header:

| Command code | Function |
|---|---|
| 0 | INIT — parse switches, probe device, set end-of-resident pointer |
| 3 | IOCTL INPUT — query drive state, TOC, audio info |
| 12 | IOCTL OUTPUT — eject, lock, volume control |
| 13 / 14 | OPEN / CLOSE — acknowledged, no action |
| 128 / 130 | READ LONG / READ LONG PREFETCH — read data sectors |
| 131 | SEEK — no-op (seek is instantaneous on virtual drive) |
| 132 | PLAY AUDIO — play audio tracks by LBA or MSF address |
| 133 | STOP AUDIO — stop playback |
| 136 | RESUME AUDIO — resume from pause |

All other command codes return `STAT_DONE | STAT_ERR | ERR_NOCMD` (0x8003).

### ASPI Interface

The driver communicates with the USB device through the DOS ASPI (Advanced SCSI Programming Interface) provided by USBASPI. Every SCSI command goes through a single `do_aspi` subroutine:

```asm
; Service Request Block layout (Adaptec ASPI for DOS, 74 bytes):
; Offset  Size  Field
;  0       1    SRB_Cmd    = 2 (Execute SCSI Command)
;  1       1    SRB_Status (0=pending, 1=done, 4=error)
;  2       1    SRB_HaId   (host adapter number, default 0)
;  3       1    SRB_Flags  (08h=data-in, 10h=data-out, 18h=no data)
;  4       2    Reserved   (0)
;  6       1    SRB_Target (SCSI target ID, default 0)
;  7       1    SRB_Lun    (LUN, default 0)
;  8       4    SRB_BufLen (data buffer length in bytes)
; 12       2    SRB_BufPtr offset (far pointer to data buffer)
; 14       2    SRB_BufPtr segment
; 16       1    SRB_SenseLen = 14
; 17       1    SRB_CDBLen   (6 or 10)
; 18       1    SRB_HaStat   (host adapter status, output)
; 19       1    SRB_TargStat (target SCSI status, output)
; 20       4    SRB_PostProc = 0 (synchronous poll mode)
; 24      20    Reserved
; 44      16    SRB_CDB      (SCSI Command Descriptor Block)
; 60      14    SRB_SenseArea (auto request sense data)
```

Calling convention:

```asm
; Set DS:SI = address of SRB, then:
int 4Bh          ; ASPI entry point installed by USBASPI

; Poll SRB_Status with timeout (~500 000 iterations ≈ 5 seconds):
.poll:
    mov al, cs:[srb.sts]
    test al, al
    jnz .done
    dec ecx
    jnz .poll
    ; timeout → return CF=1
```

The driver uses **synchronous polling** (`SRB_PostProc = 0`) rather than a callback, which is safe in the single-threaded DOS environment. All ASPI calls return CF=0 on success or CF=1 on error (timeout, host adapter error, or non-zero SCSI target status).

The ASPI target ID and host adapter number default to 0 and can be overridden at load time with `/T:id` and `/H:ha` on the `DEVICE=` line in `CONFIG.SYS`.

### IOCTL Functions Implemented

IOCTL INPUT (command 3) and IOCTL OUTPUT (command 12) carry a data buffer whose first byte is a subfunction code. The driver implements the following subfunctions:

**IOCTL INPUT:**

| SF | Name | Action | SCSI command |
|---|---|---|---|
| 0 | Return Audio Channel Info | Returns port select/volume for 4 audio output ports | None (local state) |
| 1 | Return Drive Head Location | Returns current playback position in LBA or MSF | READ SUB-CHANNEL (42h) |
| 4 | Audio Status | Queries live playback state (playing/paused/stopped) and audio flags | READ SUB-CHANNEL (42h) |
| 5 | Return Drive Bytes | Returns 128-byte SCSI INQUIRY response (vendor, product, revision) | INQUIRY (12h) |
| 6 | Return Device Status | Reports door state, disc presence, audio capability; invalidates TOC and capacity cache on no-disc | TEST UNIT READY (00h) |
| 9 | Return Volume Size | Returns disc size in 2048-byte logical blocks; result cached after first call | READ CAPACITY (25h) |
| 10 | Return Audio Disk Info | Returns first track, last track, and lead-out MSF from cached TOC | READ TOC (43h) |
| 11 | Return Audio Track Info | Returns start address MSF and control byte for a specific track number | READ TOC cache lookup |
| 12/13 | Return Audio Q-Channel Info | Returns ADR/Control, track, index, absolute M/S/F, relative M/S/F | READ SUB-CHANNEL (42h) |

**IOCTL OUTPUT:**

| SF | Name | Action | SCSI command |
|---|---|---|---|
| 0 | Eject | Ejects disc; invalidates TOC and capacity caches | START/STOP UNIT (1Bh) LoEj=1 Start=0 |
| 1 | Lock/Unlock Door | Prevents or allows medium removal | PREVENT/ALLOW MEDIUM REMOVAL (1Eh) |
| 2 | Reset Drive | No-op probe | TEST UNIT READY (00h) |
| 3 | Audio Channel Control | Stores per-port channel routing and volume (reported via SF0) | None (local state only) |
| 5 | Close Tray | Loads disc | START/STOP UNIT (1Bh) LoEj=1 Start=1 |

### SCSI Commands Used

| Opcode | Command | Use in driver |
|---|---|---|
| 00h | TEST UNIT READY (6) | Disc presence check (SF6, INIT probe) |
| 12h | INQUIRY (6) | Vendor/product strings for SF5 |
| 1Bh | START/STOP UNIT (6) | Eject (SF0) and load tray (SF5) |
| 1Eh | PREVENT/ALLOW MEDIUM REMOVAL (6) | Door lock/unlock (IOCTL OUT SF1) |
| 25h | READ CAPACITY (10) | Disc sector count for SF9 |
| 28h | READ (10) | Data sector reads — LBA and transfer count in CDB bytes 2–5 and 7–8 in big-endian; buffer is the caller's DOS buffer passed directly (no intermediate copy) |
| 42h | READ SUB-CHANNEL (10) | Current position (SF1, SF4, SF12); requests MSF format, SubQ=1, format=01h (Current Position); 16-byte response |
| 43h | READ TOC (10) | Full TOC into 804-byte resident buffer; MSF=1, alloc=804; response parsed for SF10/SF11/find_leadout/find_track |
| 47h | PLAY AUDIO MSF (10) | Audio playback — CDB bytes 3–5=start M/S/F, bytes 6–8=end M/S/F; used for both PLAY AUDIO (cmd 132) and PLAY AUDIO TRACK INDEX (cmd 134) |
| 4Bh | PAUSE/RESUME (10) | CDB byte 8 bit 0: 0=pause, 1=resume |
| 4Eh | STOP PLAY/SCAN (10) | Stop audio (cmd 133) |

**TOC caching:** The 804-byte TOC buffer is read once on first access and reused for all subsequent SF10, SF11, find_leadout, and find_track calls. The cache is invalidated (flag `toc_ok` cleared, `vol_sectors` zeroed) whenever SF6 (device status) detects no disc or when the tray is ejected via IOCTL OUT SF0. This ensures that after a disc swap the new TOC is always read fresh.

**READ LONG addressing:** The driver supports both HSG (High Sierra Group, 0-based LBA) and Red Book (MSF) addressing modes. Red Book sector addresses in the request header follow the MSCDEX specification: the DWORD at offset +20 is stored as `{M, S, F, 00h}` in memory (M at lowest address). LBA is computed as `(M×60+S)×75+F−150`. The resulting LBA is stored big-endian in CDB bytes 2–5 using the x86 `BSWAP` instruction (requires 486+).

**PLAY AUDIO addressing:** The starting sector DWORD for PLAY AUDIO (command 132) in Red Book mode is `{00h, M, S, F}` per the MSCDEX spec (00h at lowest address, M at +1). The end address is computed as start LBA + sector count, then both are converted to MSF for the SCSI PLAY AUDIO MSF command using:

```
abs = LBA + 150        ; add 2-second pregap offset
F = abs mod 75
S = (abs / 75) mod 60
M = abs / 75 / 60
```

#### Minimum CPU requirement

The driver uses the `BSWAP` instruction (introduced on the 80486) to convert 32-bit LBA values to big-endian for SCSI CDB construction. Any PC capable of running USBASPI-based USB drivers is at minimum a 486, so this is not a practical restriction.

### Memory Layout

After DOS loads the driver and INIT completes, only the resident section remains in memory. The INIT code, command line parser, and message strings are in a separate area past `end_resident` which DOS reclaims:

```
Offset 0x0000   Device header (22 bytes)
Offset 0x0016   Resident variables (disc state, audio state, channel volumes, ASPI params)
Offset 0x0050   ASPI SRB — 74 bytes (single shared SRB, safe because DOS is single-threaded)
Offset 0x009A   HA-Inquiry SRB (INIT only, but in resident area for alignment)
Offset 0x00B4   data_buf — 2048 bytes (SCSI response buffer: INQUIRY, RDCAP, SUB-CHANNEL)
Offset 0x08B4   toc_buf  — 804 bytes  (READ TOC cache, max 99 tracks + lead-out + header)
Offset 0x0BD8   end_resident ← DOS reclaims everything above this
─────────────────────────────────────────────────────────────────
Resident footprint: 3032 bytes (~3 KB) in conventional memory

Offset 0x0BD8   _strategy, _interrupt, all command handlers, SCSI helpers, TOC/MSF utils
Offset ~0x16xx  INIT (cmd_init, parse_cmdline, print_msg, strings)
─────────────────────────────────────────────────────────────────
Total .SYS file: ~5700 bytes
```

The data_buf and toc_buf are the largest consumers. The 2048-byte data_buf is used for all SCSI responses except READ LONG, where the application's own buffer is passed directly to ASPI as the DMA target (avoiding a second copy entirely).

### Building from Source

The pre-built `ESPUSBCD.SYS` is included in the project release and is ready to use without building. Building from source is only necessary if you modify `espusbcd.asm`.

#### Requirements

NASM 2.14 or newer. Install in the same AlmaLinux/WSL environment used for `build_exfat_libs.py`:

```bash
sudo dnf install -y nasm    # AlmaLinux / RHEL / Fedora
sudo apt install -y nasm    # Ubuntu / Debian
```

Verify:
```bash
nasm --version
# NASM version 2.16.x
```

#### Build command

One command, no Makefile needed:

```bash
nasm -f bin -o ESPUSBCD.SYS espusbcd.asm
```

| Flag | Meaning |
|---|---|
| `-f bin` | Raw binary output — no ELF or COFF header, exactly what DOS expects for a `.SYS` device driver |
| `-o ESPUSBCD.SYS` | Output filename |
| `espusbcd.asm` | Source file |

Expected result:

```
(no output = success)
$ ls -l ESPUSBCD.SYS
-rw-r--r-- 1 user user 5737 ... ESPUSBCD.SYS
```

#### Copy to DOS machine

Place `ESPUSBCD.SYS` on the DOS machine alongside `USBASPI1.SYS` or `USBASPI2.SYS`, for example in `C:\DRIVERS\USBCD\`.

#### Build script

The project includes `build_espusbcd.py` which automates the build and verifies the output:

```bash
python3 build_espusbcd.py                          # build in current directory
python3 build_espusbcd.py --install-nasm           # install NASM automatically then build
python3 build_espusbcd.py --src /path/espusbcd.asm --out /path/ESPUSBCD.SYS
python3 build_espusbcd.py --verify ESPUSBCD.SYS    # verify existing .SYS without rebuilding
```

The script:
1. Locates NASM in PATH (optionally installs it via `dnf` or `apt` with `--install-nasm`)
2. Checks NASM version is ≥ 2.14
3. Runs `nasm -f bin`
4. Verifies the compiled binary by parsing the DOS device driver header and checking all six fields (next-driver pointer, device attributes `0xC800`, strategy/interrupt offsets within file bounds, device name `ESPUSBCD`, subunit count = 1)
5. Prints the exact `CONFIG.SYS` and `AUTOEXEC.BAT` lines needed

Example output on success:

```
============================================================
  Building ESPUSBCD.SYS
============================================================
  [OK]  Source: espusbcd.asm
  [OK]  NASM 2.16 at /usr/bin/nasm
  ...   Compiling: nasm -f bin -o ESPUSBCD.SYS espusbcd.asm
  [OK]  Compiled: ESPUSBCD.SYS  (5737 bytes)

  ...   Verifying device driver header...
  [OK]  Next driver = 0xFFFFFFFF  (end of chain ✓)
  [OK]  Attributes  = 0xC800  (char device | IOCTL | Open/Close ✓)
  [OK]  Strategy offset = 0x0BD8  (within file ✓)
  [OK]  Interrupt offset = 0x0BE3  (within file ✓)
  [OK]  Device name = 'ESPUSBCD'  ✓
  [OK]  Subunits    = 1  ✓
  [OK]  All 6/6 header checks passed.

============================================================
  BUILD SUCCESSFUL
============================================================

  ESPUSBCD.SYS is ready.

  Copy to DOS machine and add to CONFIG.SYS:
    DEVICE=ESPUSBCD.SYS /D:ESPUSBCD

  Add to AUTOEXEC.BAT:
    LH SHSUCDX.COM /D:ESPUSBCD /Q
```

The driver uses the `BSWAP` instruction (introduced on the 80486) to convert 32-bit LBA values to big-endian for SCSI CDB construction. Any PC capable of running USBASPI-based USB drivers is at minimum a 486, so this is not a practical restriction.

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

## HTTPS / TLS

The web interface can be served over HTTPS (port 443) using a certificate and private key stored on the SD card.

### Enable HTTPS

**Via web Config tab → HTTPS / TLS:**
1. Set **HTTPS on port 443** → Enabled
2. Click **Scan SD** to find `.pem` / `.crt` / `.key` files on the SD card
3. Select **Server certificate** and **Server private key**
4. Click **Save** → reboot

**Via serial CLI:**
```
set https-enable on
set https-cert /certs/server.crt
set https-key  /certs/server.key
reboot
```

### Generate a self-signed certificate

Run in WSL / Linux:
```bash
openssl req -x509 -newkey rsa:2048 \
  -keyout /tmp/server.key -out /tmp/server.crt \
  -days 3650 -nodes \
  -subj "/CN=192.168.1.100" \
  -addext "subjectAltName=IP:192.168.1.100,DNS:cd.local"
```

> **Use your actual ESP32 IP address** in `-subj` and `-addext`. Modern browsers require `subjectAltName` to include the IP address or hostname used to connect.

> **RSA 2048-bit only.** RSA 4096-bit causes TLS handshake failure on ESP32 due to memory constraints.

Copy the files to the SD card (e.g. `/certs/`) and configure the paths in the web interface or via CLI.

### Behaviour

- When HTTPS is enabled, HTTP (port 80) automatically redirects all requests to `https://`.
- Browsers will show a certificate warning for self-signed certificates — click **Advanced → Proceed** to continue.
- Upload and download use direct TLS streaming (no proxy overhead).
- All other API requests go through an internal loopback proxy.

### Performance note

HTTPS reduces transfer speed **2–4× compared to HTTP** due to TLS encryption and internal proxy overhead (ESP32-S3 hardware AES/SHA acceleration is used, but loopback TCP still adds latency):

| Protocol | Upload / Download speed |
|---|---|
| HTTP  | ~10 Mbps (SD write limited) |
| HTTPS | ~2–4 Mbps |

For large file transfers (ISO images, BIN tracks) use HTTP or copy the SD card directly to a PC.

### Hardware acceleration

The ESP32-S3 includes hardware accelerators for AES, SHA and RSA — verified at startup:
```
[HTTPS] HW crypto: AES=ON SHA=ON RSA=ON
```

## Audio CD Playback

The firmware implements a full Red Book-compatible audio CD subsystem: SCSI commands are handled per the **Pioneer OB-U0077C CD-ROM SCSI-2 Command Set v3.1** specification.

### Hardware

Connect a **GY-PCM5102 I2S DAC** module to the ESP32-S3:

| ESP32-S3 GPIO | PCM5102 Pin | Signal |
|---|---|---|
| GPIO 14 | BCK | Bit clock |
| GPIO 15 | LCK/WS | Word select |
| GPIO 16 | DIN | Data |
| 3V3 | VIN | Power |
| GND | GND | Ground |
| GND | FMT | I2S Philips format |
| GND | SCK | No master clock |

Enable via CLI: `set audio-module on`, or via the web Config tab. The module is initialised immediately — no reboot required. Disabling also takes effect immediately.

### Audio Data Format

Red Book audio sectors are **raw 16-bit signed stereo PCM, little-endian, 44100 Hz**:
- 2352 bytes per sector = 588 stereo samples × 4 bytes (16-bit L + 16-bit R)
- **No header** — all 2352 bytes are usable PCM data
- I2S configured: 44100 Hz, 16-bit, stereo, Philips format

### Audio Support by Mode

| Mode | Audio |
|---|---|
| Non-DOS (Windows, Windows 98, Linux) | ✅ Full |
| DOS compat, driver 0 Generic | ✅ Full |
| DOS compat, driver 1 USBCD2/TEAC | ✅ SCSI handled (driver communication broken via INT 13h) |
| DOS compat, driver 2 ESPUSBCD/Panasonic | ✅ Full |
| DOS compat, driver 3 DI1000DD | ❌ Data-only by design |

### Virtual Disc Layout (CUE/BIN)

For **separate BIN files** (one `.bin` per track — e.g. Tomb Raider):
```
LBA 0          … dataTrackSectors-1   → Track 01 (data)
LBA dataEnd    … dataEnd+len02-1      → Track 02 (audio, pregap stripped)
LBA …          … …                    → Track 03 … N
LBA leadOut                           → Lead-out (reported in READ TOC as 0xAA)
```

For **single BIN file** (all tracks in one file):
- Track LBAs come directly from CUE `INDEX 01` positions (absolute BIN sector)
- Audio seek: `fileSector = fileSectorBase + (lba - trackStartLba)` where `fileSectorBase = trackStartLba` for single-BIN
- `mscReadCb` uses `dataTrackStartSect = 0` for single-BIN (DOS navigates via Track 01 LBA from TOC)

**Pregap handling:**
- Physical pregap (INDEX 00 in CUE): `fileSectorBase = INDEX01_position`; length excludes the pregap
- Virtual pregap (PREGAP directive): `fileSectorBase = 0`; pregap LBA space reserved in virtual layout but no file seek needed

### Bidirectional Synchronisation

HTML player and PC player are kept in sync via shared firmware state:

| Direction | Path | Latency |
|---|---|---|
| PC → HTML | SCSI command → `audioState` → `/api/audio/status` poll | ≤ 400 ms |
| HTML → PC | `/api/audio/*` → `audioState` → SCSI READ SUB-CHANNEL | < 1 ms |

The HTML player polls `/api/audio/status` every **400 ms** when the Audio tab is open, every **2 s** in the background. Sub-channel MSF position updates every sector (every **13 ms**) in the audio task.

### Without the I2S Module

CUE tracks are still parsed and all SCSI commands work correctly. The audio task tracks position in real time at 75 frames/sec using `vTaskDelay`. PC games see a correctly working CD player (TOC, sub-channel, play/stop/pause) — just no analogue audio output.

### Connecting to a Mixer

PCM5102 output is line level (~2 Vrms). Connect to **line-in** (AUX input), not microphone. Set firmware volume to 100 and regulate gain at the mixer.

---

## SCSI Command Reference

The firmware implements the USB MSC CD-ROM profile with SCSI-2 commands per **Pioneer OB-U0077C v3.1**. Commands are handled in a patched `tud_msc_scsi_cb` via `build_exfat_libs.py --skip-full-build`.

### General Commands

| Code | Command | Implementation |
|---|---|---|
| `03h` | REQUEST SENSE | Returns extended sense data (12 bytes) |
| `12h` | INQUIRY | Returns device type=05 (CD-ROM), vendor/product strings |
| `15h` | MODE SELECT (6) | Accepted, ignored — volume and settings stored by host |
| `1Ah` | MODE SENSE (6) | Page `0x0E` (Audio Control): port 0=left 0xFF, port 1=right 0xFF; page `0x3F`=same; other pages: 4-byte header |
| `1Bh` | START/STOP UNIT | `LoEj=1,Start=0` → eject + audio stop; `Start=0` → spin down + audio stop; `Start=1` → no-op |
| `1Eh` | PREVENT/ALLOW MEDIUM REMOVAL | Always returns success (no mechanical lock) |
| `25h` | READ CAPACITY | Returns sector count × block size (2048 B) |
| `28h` | READ (10) | Reads data sectors from mounted BIN with header offset applied |
| `2Bh` | SEEK (10) | No-op — virtual drive seek is instantaneous |
| `46h` | GET CONFIGURATION | Returns minimal 8-byte feature header |
| `51h` | READ DISC INFORMATION | Returns 34-byte disc info (disc type=0x20, erasable=0, sessions=1) |
| `55h` | MODE SELECT (10) | Accepted, ignored |
| `5Ah` | MODE SENSE (10) | Page `0x0E` (Audio Control): 24-byte response with port routing and volume; other pages: 8-byte header |
| `BBh` | SET CD-ROM SPEED (2) | No-op — accepted without error |
| `BDh` | MECHANISM STATUS | Returns 8-byte all-zeros response |
| `DAh` | SET CD-ROM SPEED (1) | No-op — accepted without error |

### Audio Commands

| Code | Command | Implementation |
|---|---|---|
| `42h` | READ SUB-CHANNEL | Returns 16-byte current position block (format 01h). ADR/Control=`0x10` (audio, ADR=1). Audio status: `11h`=playing, `12h`=paused, `13h`=completed (returned once), `15h`=no status. SubQ=0 returns 4-byte header only. |
| `43h` | READ TOC | Returns track descriptors with correct LBA/MSF. Data track control=`0x14`, audio=`0x10`. Lead-out entry = `0xAA`. Supports LBA and MSF format (bit 1 of byte 1). |
| `44h` | READ HEADER | Returns CD-ROM Data Mode + absolute address. Returns CHECK CONDITION if LBA is within an audio track. |
| `45h` | PLAY AUDIO (10) | Play from LBA, length in sectors. Length=0 → play to disc end. |
| `47h` | PLAY AUDIO MSF | Play from MSF start to MSF end. MSF → LBA conversion: `(M×60+S)×75+F−150`. |
| `48h` | PLAY AUDIO TRACK INDEX | Play from starting track number to ending track number (inclusive). |
| `49h` | PLAY AUDIO TRACK RELATIVE (10) | TRLBA = signed 32-bit offset from track start (index 1). Negative = pre-gap area. Length=0 → no-op. |
| `4Bh` | PAUSE/RESUME | Byte 8 bit 0: `1`=resume, `0`=pause. |
| `4Eh` | STOP PLAY/SCAN | Stops audio playback. |
| `A5h` | PLAY AUDIO (12) | Same as `45h` but 32-bit transfer length in bytes 6–9. |

### Sense Key Reference

| Situation | Sense Key | ASC | ASCQ |
|---|---|---|---|
| Unknown command | `05h` ILLEGAL REQUEST | `20h` | `00h` |
| LBA out of range / audio track for READ HEADER | `05h` ILLEGAL REQUEST | `21h` | `00h` |
| Track not found (PLAY TRACK RELATIVE) | `05h` ILLEGAL REQUEST | `21h` | `00h` |
| Media changed (DOS compat mode) | `06h` UNIT ATTENTION | `28h` | `00h` |

### MODE SENSE Page 0x0E — Audio Control Parameters

Returned for page code `0x0E` or `0x3F` (all pages) per OB-U0077C §2.9.6:

```
MODE SENSE(6) response — 20 bytes:
  Byte 0:    Mode data length = 19
  Byte 1:    Medium type = 0 (CD-ROM)
  Byte 2:    Device specific = 0
  Byte 3:    Block descriptor length = 0

  Page 0x0E (16 bytes):
  Byte 4:    Page code = 0x0E
  Byte 5:    Page length = 0x0E (14)
  Byte 6:    Immed=1, SOTC=0
  Bytes 7–11: Reserved
  Byte 12:   Output Port 0 channel select = 0x01 (left)
  Byte 13:   Output Port 0 volume = 0xFF (max)
  Byte 14:   Output Port 1 channel select = 0x02 (right)
  Byte 15:   Output Port 1 volume = 0xFF (max)
  Bytes 16–19: Ports 2–3 = 0 (unused)
```

> DOS audio CD players (e.g. CD Player for DOS 2.25e) read this page before playback.
> Returning a stub response caused **MSCDEX error 3** ("Unknown command") in those applications.

### READ TOC Response Layout

```
Byte 0–1:  Data Length (MSB first, excludes first 2 bytes)
Byte 2:    First Track Number = 1
Byte 3:    Last Track Number  = 1 + audio track count

Per track (8 bytes each):
  Byte 0:  Reserved (0)
  Byte 1:  ADR|Control  — 0x14 data track, 0x10 audio track
  Byte 2:  Track Number
  Byte 3:  Reserved (0)
  Byte 4–7: LBA (MSB first) or MSF (byte 4=0, 5=M, 6=S, 7=F)

Lead-out entry: Track Number = 0xAA, address = lead-out LBA / MSF
```

### READ SUB-CHANNEL Response Layout

```
Byte 0:    Reserved (0)
Byte 1:    Audio Status  — 11h playing / 12h paused / 13h completed / 15h none
Byte 2–3:  Sub-channel data length = 12 (MSB first)

Current Position Data Block (12 bytes, format 01h):
  Byte 4:  Format Code = 01h
  Byte 5:  ADR|Control = 0x10  (ADR=1 current position, Control=0 audio)
  Byte 6:  Track Number
  Byte 7:  Index Number
  Byte 8:  Absolute Address MSB (0)
  Byte 9–11: Absolute M/S/F  (LBA+150 converted to MSF)
  Byte 12: Relative Address MSB (0)
  Byte 13–15: Relative M/S/F  (sectors from track start)
```

---

## Supported Disc Image Formats

| Format | Extension | Sector size | Header offset | Notes |
|---|---|---|---|---|
| ISO 9660 | `.iso` | 2048 B | 0 | |
| Raw MODE1/2048 | `.bin` | 2048 B | 0 | Cooked/stripped |
| Raw MODE1/2352 | `.bin` / `.cue` | 2352 B | 16 | Full raw sector with sync and ECC |
| Raw MODE2/2352 | `.bin` / `.cue` | 2352 B | 24 | XA with 8-byte sub-header |
| Raw MODE2/2336 | `.bin` / `.cue` | 2336 B | 8 | Sub-header only, no sync |
| Raw MODE2/2048 | `.cue` | 2048 B | 0 | Cooked MODE2 |
| CD+G | `.cue` | 2448 B | 0 | Raw 2352 B data + 96 B subcode; served as-is |
| CUE sheet (single BIN) | `.cue` | Per CUE | Per CUE | All tracks in one file; LBAs from INDEX 01 |
| CUE sheet (separate BIN per track) | `.cue` | Per CUE | Per CUE | One file per track; virtual LBAs assigned after parse |

### CUE Pregap Handling

Two pregap formats are fully supported:

- **Physical pregap (INDEX 00):** pregap data is stored in the BIN file before INDEX 01. The parser reads the INDEX 00 position, subtracts it from INDEX 01 to determine pregap length, and the audio task seeks past it using `fileSectorBase`.
- **Virtual pregap (PREGAP directive):** pregap silence is not in the BIN file. The parser reserves the pregap LBA space in the virtual disc layout and the audio task reads from byte 0 of the BIN file directly.

### Multi-sector Read Correctness

For all raw sector formats (sector size > 2048 B), `mscReadCb` reads one sector at a time, seeking to `lba × rawSectorSize + headerOffset` for each sector. This prevents the sync/ECC bytes between sectors from appearing as data when the OS requests multiple sectors in a single read command.

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
# First restore original USBMSC.cpp, then re-apply all patches
python3 build_exfat_libs.py --restore
python3 build_exfat_libs.py --skip-full-build
```

**Current patch version: v14.** The patch modifies `tud_msc_scsi_cb` in `USBMSC.cpp` to inject:
- Full CD-ROM SCSI command set (READ TOC, PLAY AUDIO, READ SUB-CHANNEL, START/STOP, …)
- Audio and TOC SCSI commands active for: non-DOS mode (Windows/Linux) and DOS drivers 0, 1, 2; driver 3 (DI1000DD) receives universal handlers only (data-only)
- READ TOC response fills tracks sequentially from Track 1; for small-buffer DOS drivers (alloc < 28 bytes) the data track is skipped so audio tracks are prioritised — `Data Length` always reflects the full TOC size so the OS can request all tracks with a follow-up call
- MODE SENSE pages `0x0D`, `0x0E`, `0x2A` (6-byte and 10-byte variants) — required by DOS audio players and Windows
- MODE SELECT `15h`/`55h` — accepted without error
- UNIT ATTENTION (`06h/28h`) via a 3-cycle counter — persists across multiple OS poll cycles to ensure reliable disc-change detection
- `setSense()` helper, `_audioPlayedOnce` sticky audio status flag
- `tud_msc_test_unit_ready_cb` patched for UNIT ATTENTION counter support

Always run `--restore` before `--skip-full-build` to avoid double-patching.

### `patch_usbmsc.py` (Windows)

A standalone backup script included for reference. Not used in the standard workflow — `build_exfat_libs.py` handles the USBMSC patch as part of the exFAT build.

### `build_espusbcd.py` (WSL/Linux or any Python 3)

Build script for the ESPUSBCD.SYS DOS CD-ROM driver. Locates or installs NASM, compiles `espusbcd.asm`, and verifies the resulting binary. See [Building from Source](#building-from-source) for full usage.

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
- Verify `build_exfat_libs.py` completed — USBMSC patch v14 must be applied
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
[CUE] Data track sectors from file: 146011 (total=146011 pregap=0)
[CUE] Virtual disc LBAs assigned, lead-out LBA=215612
[CUE] Track 02: LBA 146011  len 14593  (~194s)
```

If you see `LBA 0` for all audio tracks, re-flash with the latest firmware.

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

### Disc swap in DOS causes device to disappear

Enable DOS Compatibility Mode:
```
set dos-compat on
```

This replaces the USB re-enumeration with a UNIT ATTENTION signal so MSCDEX/USBASPI can reload the TOC without losing the device.

### DOS CD player shows only one audio track

This is caused by an older USBMSC patch (v13 or earlier). The READ TOC handler in older versions did not correctly handle the small allocation length used by DOS drivers such as USBCD1 — MSCDEX received only one audio track on the first TOC request. Since v14 the patch skips the data track when the allocation length is too small (< 28 bytes) so audio tracks are prioritised, and `Data Length` always reflects the full TOC. With ESPUSBCD.SYS this issue does not occur because the driver uses a full 804-byte TOC buffer.

Re-apply the patch:
```bash
python3 build_exfat_libs.py --restore
python3 build_exfat_libs.py --skip-full-build
```

### After several remounts the drive appears empty or disappears

This is caused by the OS not catching the disc-change signal in time. The firmware signals UNIT ATTENTION across 3 consecutive TUR poll cycles and sequences the `msc.mediaPresent(false)` / `msc.mediaPresent(true)` transition carefully to minimise the window during which no media is visible. In non-DOS mode, USB callbacks are re-registered before every `msc.begin()` call to prevent callback loss across remounts.

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
| `build_exfat_libs.py` | exFAT + USBMSC patch script — run in WSL (current patch: v14) |
| `patch_usbmsc.py` | Standalone USBMSC patch — run on Windows (backup/reference only) |
| `espusbcd.asm` | ESPUSBCD.SYS source — x86 16-bit NASM assembly |
| `build_espusbcd.py` | Build script for ESPUSBCD.SYS — compiles and verifies the driver |
| `ESPUSBCD.SYS` | Pre-built DOS CD-ROM driver binary (ready to use, 5737 bytes) |
| `CONFIG.SYS` | Sample DOS CONFIG.SYS with correct CRLF line endings |
| `AUTOEXEC.BAT` | Sample DOS AUTOEXEC.BAT with correct CRLF line endings |
| `flash_windows.bat` | Windows flash script — double-click to flash |
| `flash_linux.sh` | Linux/macOS flash script |
| `doc/arduino_ide_settings.png` | Arduino IDE settings screenshot |
| `doc/esp32s3.png` | ESP32-S3 DevKitC-1 board photo |
| `doc/GY-PCM5102.png` | GY-PCM5102 I2S module photo |
| `doc/wiring.svg` | Wiring diagram |

All `.ino` and `.h` files must be in the same folder.
