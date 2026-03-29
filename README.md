# ESP32-S3 Virtual CD-ROM Drive + File Manager

A firmware project for the ESP32-S3 microcontroller that emulates a USB CD-ROM drive while serving a full-featured web-based file manager over Wi-Fi. Disc images stored on an SD card are presented to the host PC as a standard read-only optical drive — no drivers required.

---

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring Diagram](#wiring-diagram)
- [Arduino IDE Settings](#arduino-ide-settings)
- [Required Libraries](#required-libraries)
- [First-Time Setup](#first-time-setup)
- [Serial CLI Reference](#serial-cli-reference)
- [Web Interface](#web-interface)
- [Wi-Fi Configuration](#wi-fi-configuration)
- [802.1x Enterprise Wi-Fi (EAP)](#8021x-enterprise-wi-fi-eap)
- [Supported Disc Image Formats](#supported-disc-image-formats)
- [Python Utility Scripts](#python-utility-scripts)
  - [build_exfat_libs.py — exFAT Support](#build_exfat_libspy--exfat-support)
  - [patch_usbmsc.py — CD-ROM SCSI Commands](#patch_usbmscpy--cd-rom-scsi-commands)
- [API Reference](#api-reference)
- [RGB LED Status Codes](#rgb-led-status-codes)
- [Troubleshooting](#troubleshooting)

---

## Features

- **USB CD-ROM emulation** — presents as a USB Mass Storage Class device with CD-ROM SCSI profile; the host PC sees a read-only optical drive
- **Disc image formats** — ISO 9660 (`.iso`), raw binary (`.bin`) with all standard sector layouts, and CUE sheet parsing (`.cue`) for multi-track images
- **Web file manager** — upload, download, delete, create directories, and drag-and-drop via a single-page browser application served directly from the ESP32
- **Wi-Fi** — WPA2-Personal, WPA2-Enterprise PEAP, and WPA2-Enterprise EAP-TLS with full certificate management
- **mDNS** — device reachable at `hostname.local` on the local network; supports full FQDN for DHCP clients
- **NVS configuration persistence** — all settings survive reboot; stored in the ESP32 non-volatile storage partition
- **Serial CLI** — complete configuration and diagnostic interface at 115 200 baud via the USB-UART bridge
- **exFAT SD cards** — supported after applying the `build_exfat_libs.py` patch to the Arduino ESP32 toolchain
- **RGB LED** — boot, connected, and error state indication via the onboard WS2812B LED

---

## Hardware Requirements

| Component | Notes |
|---|---|
| ESP32-S3 development board | Any variant with OTG USB and 16 MB flash recommended; tested on ESP32-S3 DevKitC-1 N16R8 |
| MicroSD card module (SPI/SD_MMC) | 3.3 V logic — **do not use 5 V modules without a level shifter** |
| MicroSD card | FAT32 or exFAT (exFAT requires the `build_exfat_libs.py` script) |
| USB-A to USB-C cable (×2) | One for UART programming/serial, one OTG to the target host PC |
| USB-C to USB-A OTG adapter | For connecting the OTG port to a standard PC port |

The MicroSD module is connected to the ESP32-S3 using SD_MMC in **1-bit mode** to avoid pin conflicts with the USB OTG peripheral. The four required signals are: CLK, CMD (MOSI), D0 (MISO), and D3/CS.

> **Important:** The SD module's VCC pin must be connected to **3.3 V**, not 5 V. Most bare SD card breakout boards accept 3.3 V logic and power directly. Modules with onboard 5 V regulators may need the 3.3 V rail bypassed.

---

## Wiring Diagram

```
ESP32-S3 DevKitC-1
───────────────────────────────────────────────────────
                    ┌──────────────────────────────┐
                    │         ESP32-S3             │
                    │                              │
   SD_MMC CLK  ─── │ GPIO12 ─────────────────── CLK │ ──► SD Module D5
   SD_MMC CMD  ─── │ GPIO11 ─────────────────── CMD │ ──► SD Module D7 (MOSI)
   SD_MMC D0   ─── │ GPIO13 ─────────────────── D0  │ ──► SD Module D6 (MISO)
   SD_MMC D3   ─── │ GPIO10 ─────────────────── CS  │ ──► SD Module D4 (CS)
                    │                              │
   RGB LED     ─── │ GPIO48 ──────────────── WS2812B │ (onboard)
                    │                              │
   UART CH343  ─── │ USB (left connector)         │ ──► PC  serial monitor / programming
   USB OTG     ─── │ USB (right connector)        │ ──► Target PC  (virtual CD-ROM)
                    └──────────────────────────────┘

   SD Module
   ─────────────────────────
   VCC ──► 3V3 rail on ESP32-S3
   GND ──► GND
   D4  ──► GPIO10  (CS / D3)
   D5  ──► GPIO12  (CLK)
   D6  ──► GPIO13  (MISO / D0)
   D7  ──► GPIO11  (MOSI / CMD)
```

![Wiring Diagram](doc/wiring.svg)


---

## Arduino IDE Settings

Open **Tools** menu and set all of the following before compiling:

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB Mode | USB-OTG (TinyUSB) |
| USB CDC On Boot | Disabled |
| Flash Size | 16MB (N16R8) |
| Partition Scheme | Huge APP (3MB No OTA / 1MB SPIFFS) |
| PSRAM | OPI PSRAM |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 921600 |

> **USB Mode** must be **USB-OTG (TinyUSB)**, not "Hardware CDC". The OTG port is used for the virtual CD-ROM; the UART bridge (CH343 or CP2102) handles programming and the serial monitor.

---

## Required Libraries

Install via the Arduino Library Manager (**Sketch → Include Library → Manage Libraries**):

- **Adafruit NeoPixel** — RGB LED control

All other dependencies (`WiFi`, `ESPmDNS`, `WebServer`, `SD_MMC`, `USB`, `USBMSC`, `Preferences`, `mbedtls`) are part of the ESP32 Arduino core and do not require separate installation.

---

## First-Time Setup

1. Wire the hardware as described above.
2. Apply the two Python patches (see [Python Utility Scripts](#python-utility-scripts)).
3. Open `ESP32S3_VirtualCDROM.ino` in the Arduino IDE. The file `html_page.h` must be in the same folder.
4. Select the board and settings listed above.
5. Upload the sketch.
6. Open the Serial Monitor at **115 200 baud**.
7. At the `>` prompt, configure Wi-Fi:
   ```
   set ssid YourNetworkName
   set pass YourPassword
   wifi reconnect
   ```
8. The device prints its IP address and mDNS hostname on successful connection. Open `http://esp32cdrom.local` (or the IP address) in a browser to reach the web interface.
9. Place disc images (`.iso`, `.bin`/`.cue`) on the SD card. Use the web File Manager or the `mount` command to mount an image.

---

## Serial CLI Reference

Connect any serial terminal to the UART bridge at **115 200 baud, 8N1**. Commands are case-insensitive. Pressing Enter sends a command.

### General Commands

| Command | Description |
|---|---|
| `help` | Print the command list |
| `show config` | Print the full configuration stored in NVS plus runtime state |
| `status` | Print current Wi-Fi, SD, disc image, and security status |
| `show files [path]` | Recursively list SD card contents; default path is `/` |
| `reboot` | Restart the ESP32 |
| `clear config` | Erase all NVS settings and restore defaults |

### Disc Image Commands

| Command | Description |
|---|---|
| `mount <path>` | Mount an ISO/BIN/CUE file as the CD-ROM drive |
| `umount` | Eject the current disc |

### SD Card Commands

| Command | Description |
|---|---|
| `sd reinit` | Reinitialize the SD card driver |

### Wi-Fi Commands

| Command | Description |
|---|---|
| `wifi reconnect` | Disconnect and reconnect using current settings |

### Configuration — `set <key> [value]`

Omitting the value clears that field. All changes are written to NVS immediately.

| Key | Description | Example |
|---|---|---|
| `ssid` | Wi-Fi network name | `set ssid MyNetwork` |
| `pass` | WPA2-Personal password | `set pass s3cr3t` |
| `dhcp` | DHCP on/off | `set dhcp on` |
| `ip` | Static IP address | `set ip 192.168.1.50` |
| `mask` | Subnet mask | `set mask 255.255.255.0` |
| `gw` | Default gateway | `set gw 192.168.1.1` |
| `dns` | DNS server | `set dns 8.8.8.8` |
| `hostname` | Hostname or FQDN | `set hostname espcd` or `set hostname espcd.corp.net` |
| `eap-mode` | 802.1x mode: `0`=off, `1`=PEAP, `2`=EAP-TLS | `set eap-mode 2` |
| `eap-id` | EAP outer identity | `set eap-id device@corp.net` |
| `eap-user` | PEAP inner username | `set eap-user john` |
| `eap-pass` | PEAP inner password | `set eap-pass hunter2` |
| `eap-ca` | SD path to CA certificate PEM | `set eap-ca /wifi/ca.pem` |
| `eap-cert` | SD path to client certificate | `set eap-cert /wifi/client.crt` |
| `eap-key` | SD path to private key | `set eap-key /wifi/client.key` |
| `eap-kpass` | Passphrase for encrypted private key | `set eap-kpass myKeyPass` |

---

## Web Interface

When connected to Wi-Fi the device serves a single-page application at `http://<ip-address>/` or `http://<hostname>.local/`. The interface consists of five tabs:

### CD-ROM Tab
Browse the SD card for disc images (`.iso`, `.bin`, `.cue`). Click a file to mount it; the entry updates in real time. A separate button ejects the current disc. The default auto-mount image (loaded automatically on every boot) is set here.

### File Manager Tab
Full file manager for the SD card — navigate directories, upload files via drag-and-drop or the file picker, download any file, delete files and directories, and create new folders. Upload progress is shown per file.

### Log Tab
Live streaming log output — shows the same messages that appear on the serial port, visible without a serial cable.

### Status Tab
Real-time device status in a structured panel:

- **Wi-Fi** — SSID, band (2.4/5 GHz), channel, signal quality bar, IP/mask/gateway/DNS, BSSID, hostname, mDNS link, auth method
- **802.1x security detail** (when enterprise mode is active) — EAP identity, client cert path, key path, passphrase status, CA cert, runtime cert format, runtime key format
- **SD card** — total / used / free capacity
- **Disc image** — path, sector count, raw sector size, header offset, USB media-present flag
- **System** — free heap, uptime, CPU frequency, flash size, SDK version

### Config Tab
All configuration settings are editable through the web UI without the serial port:

- Wi-Fi SSID and password
- DHCP toggle — when switched off the current IP/mask/gateway/DNS from the running system is pre-filled automatically
- Static IP/mask/gateway/DNS fields
- Hostname field with live mDNS preview (`.local` link updates as you type)
- **802.1x section** — mode dropdown, SD card scan button (finds all `.pem`, `.crt`, `.key` files anywhere on the card and populates dropdowns), CA cert dropdown (includes "none" option to disable server verification), client cert dropdown, client key dropdown, key passphrase field
- Save, Reboot, and Factory Reset actions
- SD card unmount / remount

---

## Wi-Fi Configuration

The device supports both DHCP and static IP addressing. The hostname configured via `set hostname` is sent to the DHCP server as the client identifier and is also used for mDNS. When a fully-qualified domain name is given (e.g. `device.corp.net`) the first label becomes the mDNS name (`device.local`) while the full FQDN is sent to the DHCP server.

Wi-Fi credentials and all settings are stored in the ESP32 NVS flash partition and survive power cycling.

---

## 802.1x Enterprise Wi-Fi (EAP)

The firmware supports connecting to WPA2-Enterprise networks using either PEAP (username + password) or EAP-TLS (client certificate + private key). Both modes use the `esp_eap_client` API from ESP-IDF 5.x.

### PEAP (mode 1)

```
set eap-mode 1
set eap-id   username@corp.net
set eap-user username
set eap-pass Password123
set eap-ca   /wifi/ca.pem        (optional)
wifi reconnect
```

### EAP-TLS (mode 2)

```
set eap-mode 2
set eap-cert /wifi/client.crt
set eap-key  /wifi/client.key
set eap-ca   /wifi/ca.pem        (optional — omit to skip server cert verification)
wifi reconnect
```

The EAP identity (`eap-id`) is optional in EAP-TLS mode. When not set, the firmware extracts the Common Name (CN) from the client certificate using mbedTLS at connect time and uses it automatically.

### Certificate Requirements

- Certificates and keys must be in **PEM format** (base64, with `-----BEGIN ...-----` headers)
- Supported private key formats:
  - `BEGIN RSA PRIVATE KEY` — PKCS#1 (traditional OpenSSL format)
  - `BEGIN PRIVATE KEY` — PKCS#8 (default in OpenSSL 3.x and later)
  - `BEGIN EC PRIVATE KEY` — EC private key
  - `BEGIN ENCRYPTED PRIVATE KEY` — passphrase-protected; set `eap-kpass` before connecting
- Both RSA 2048-bit and RSA 4096-bit certificates are supported under ESP-IDF 5.5 / mbedTLS 3.x
- The client certificate should include the `extendedKeyUsage = clientAuth` (OID `1.3.6.1.5.5.7.3.2`) extension
- Maximum file size per PEM file: 16 KB; the firmware checks available heap before loading

### Converting a PKCS#8 Key to PKCS#1

Modern OpenSSL (3.x) generates PKCS#8 by default. While PKCS#8 is supported by ESP-IDF 5.5, if you need PKCS#1 for compatibility with older builds:

```bash
openssl pkey -traditional -in client.key -out client_rsa.key
# Output starts with: -----BEGIN RSA PRIVATE KEY-----
```

### Key Format Diagnostics

Every connection attempt logs the detected key and certificate format:

```
[EAP]  ── Format Check ──
[EAP]  Key : PKCS#8  ✓  (supported by ESP-IDF 5.x mbedTLS)
[EAP]  Cert: X.509 PEM  ✓
[EAP]  Client cert + key OK  (heap: 218460 B)
```

### Certificate Buffers and the Supplicant Task

A critical implementation detail: the ESP-IDF WPA supplicant runs as a background FreeRTOS task and continues performing the TLS handshake asynchronously after `WiFi.begin()` returns. The certificate and key buffers passed to `esp_eap_client_set_certificate_and_key()` **must remain allocated in memory** for the entire duration of the handshake. The firmware keeps these buffers in static global variables (`s_eapCertBuf`, `s_eapKeyBuf`, `s_eapCaBuf`) that are freed only at the start of the next `startWiFi()` call.

### RADIUS Server Setup (EAP-TLS)

For a FreeRADIUS server to accept the ESP32 certificate, the username used in the SQL `radcheck` table must match the EAP identity sent by the device:

```sql
INSERT INTO radcheck (username, attribute, value, op)
VALUES ('device@corp.net', 'Auth-Type', 'EAP', ':=');
```

---

## Supported Disc Image Formats

| Format | Extension | Sector size | Header offset |
|---|---|---|---|
| ISO 9660 | `.iso` | 2048 B | 0 |
| Raw MODE1/2048 | `.bin` | 2048 B | 0 |
| Raw MODE1/2352 | `.bin` | 2352 B | 16 |
| Raw MODE2/2352 | `.bin` | 2352 B | 24 |
| Raw MODE2/2336 | `.bin` | 2336 B | 8 |
| CUE sheet | `.cue` | Determined by CUE | Determined by CUE |

The CUE parser reads the `FILE` and `TRACK` directives to locate the binary data track and determine its sector layout. Only the first data track is mounted; audio tracks are ignored. The parser handles both `MODE1/2352` and `MODE2/2352` CUE track types.

---

## Python Utility Scripts

Both scripts must be run **once** before compiling the sketch. They modify the Arduino ESP32 toolchain installation in place and create `.bak` backups of every file they change. They do not modify the sketch source files.

---

### `build_exfat_libs.py` — exFAT Support

By default the Arduino ESP32 core's FATFS library is compiled without exFAT support. This script rebuilds `libfatfs.a` with `FF_FS_EXFAT = 1` and installs it into the Arduino15 package directory.

#### Background

The Espressif Arduino lib-builder uses `esp32-arduino-lib-builder`, a CMake/ninja build system layered on top of ESP-IDF. The relevant FATFS configuration header is `ffconf.h`, located inside the lib-builder's ESP-IDF submodule. The key challenge is that `build.sh` calls `git reset --hard` at startup, which silently reverts any changes made to `ffconf.h` before the build starts. The script works around this by:

1. Running `build.sh` first so that git reset can complete and all ESP-IDF components are built
2. Patching `ffconf.h` **after** `build.sh` exits
3. Deleting only the `fatfs` CMake cache directory
4. Invoking `ninja` to recompile only the `esp-idf/fatfs/libfatfs.a` target (~10 seconds instead of the full 1–3 hour build)
5. Copying the resulting `libfatfs.a` into the Arduino15 package tree

The same `ffconf.h` patch is also applied to the TinyUSB fatfs copy inside the Arduino hardware directory.

#### Usage

Run from a WSL terminal (the script auto-detects the Arduino15 path by scanning `/mnt/c/Users/*/AppData/Local/Arduino15`):

```bash
# Full build + patch (first run, takes 1-3 hours):
python3 build_exfat_libs.py

# Skip build.sh if it was already run recently (~10 seconds):
python3 build_exfat_libs.py --skip-full-build

# Specify a different Arduino ESP32 version:
python3 build_exfat_libs.py --arduino-version 3.3.7

# Override the Arduino15 path:
python3 build_exfat_libs.py --arduino15 /mnt/c/Users/Admin/AppData/Local/Arduino15

# Verify current installation without making changes:
python3 build_exfat_libs.py --test

# Preview actions without modifying any file:
python3 build_exfat_libs.py --dry-run

# Restore all original files from backups:
python3 build_exfat_libs.py --restore
```

#### What the Script Does — Step by Step

**Step 1 — Locate Arduino15**

The script searches `/mnt/c/Users/*/AppData/Local/Arduino15` and `/mnt/d/...` for the Arduino15 directory. It then derives the paths to the ESP32 hardware directory and `esp32s3-libs` tools directory from the version string. Two possible locations for `libfatfs.a` are checked (the path format changed between lib-builder versions).

**Step 2 — Clone the lib-builder (if absent)**

If `esp32-arduino-lib-builder` is not already present at the hardcoded path `/root/esp32_exfat_build/esp32-arduino-lib-builder`, it is cloned from the official Espressif GitHub repository.

**Step 3 — Run `build.sh`**

```bash
./build.sh -t esp32s3 -b build -c '<arduino15_hardware_path>'
```

This is a full ESP-IDF build targeting the ESP32-S3. On the first run it takes 1–3 hours. The script warns that `git reset --hard` is expected and intentional. Skipped with `--skip-full-build` if the build directory already exists.

**Step 4 — Patch `ffconf.h`**

After `build.sh` exits, the script uses a compiled regular expression to find and replace `#define FF_FS_EXFAT 0` with `#define FF_FS_EXFAT 1` in every `ffconf.h` found under the lib-builder directory tree. A `.bak` copy of the original is created before any modification. The function `patch_ffconf()` is idempotent — it skips files that already have the value set to `1`.

```python
FFCONF_PATTERN = re.compile(r'(#define\s+FF_FS_EXFAT\s+)0')
new, count = FFCONF_PATTERN.subn(r'\g<1>1', txt)
```

**Step 5 — Delete fatfs CMake cache and recompile**

Deleting the `build/esp-idf/fatfs/` directory forces ninja to recompile the fatfs target from scratch with the patched header:

```bash
source esp-idf/export.sh && cd build && ninja esp-idf/fatfs/libfatfs.a
```

**Step 6 — Verify exFAT symbols**

`strings` and `nm` are run on the freshly built `libfatfs.a` to confirm exFAT symbol names are present. The script also checks file size (without exFAT ~40 KB, with exFAT ~80+ KB).

**Step 7 — Install and patch installed files**

`shutil.copy2()` is used instead of the shell `cp` command to avoid the interactive alias `-i` present on many Linux systems. The built library is copied to:

```
<arduino15>/packages/esp32/tools/esp32s3-libs/<version>/lib/libfatfs.a
```

The installed `ffconf.h` headers in the Arduino15 tree are also patched in the same way.

**Step 8 — Run `--test` as a final verification pass**

The script calls itself with `--test` to produce a structured pass/fail report covering all modified files.

#### Test Mode Output

```
>>> 1. Checking installed ffconf.h
  ✓ FF_FS_EXFAT = 1 in installed ffconf.h

>>> 3. Checking exFAT symbols in installed libfatfs.a
  ✓ exFAT strings/symbols found in libfatfs.a
  ✓ libfatfs.a size 87 KB (expected >50 KB)

==============================
  RESULT: PASS - all checks passed (6/6)
```

#### After the Script

1. Delete the Arduino build cache: `%LOCALAPPDATA%\arduino\sketches\` — delete all subdirectory contents
2. Restart the Arduino IDE completely
3. Compile and upload the sketch
4. The serial monitor will show:
   ```
   [SD]   exFAT support: YES (compiled in)
   ```

---

### `patch_usbmsc.py` — CD-ROM SCSI Commands

The Arduino ESP32 core's `USBMSC.cpp` implements only basic USB Mass Storage read/write operations. A standard CD-ROM drive must also respond to several additional SCSI commands that hosts use to discover the disc type, read the Table of Contents, and query drive capabilities. Without these handlers, Windows and Linux mount the device as a raw drive but cannot auto-run CDs, show the correct disc type, or enumerate audio tracks.

#### What the Patch Does

The script locates `USBMSC.cpp` in the Arduino15 hardware directory, creates a `.bak` backup, and inserts a `switch` block inside the `tud_msc_scsi_cb()` callback function. The handlers are injected at the very start of the function body — before any `media_present` check — so that the host always gets a valid response to these commands regardless of whether a disc is currently mounted.

| SCSI Command | Opcode | Purpose |
|---|---|---|
| READ TOC | `0x43` | Returns the disc's Table of Contents; the firmware reports a single data track using the current block count |
| GET CONFIGURATION | `0x46` | Reports the drive profile as a standard CD/DVD-ROM |
| READ DISC INFO | `0x51` | Returns disc state (finalised, single session) |
| MODE SENSE 6 | `0x1A` | Returns minimal mode page data |
| MODE SENSE 10 | `0x5A` | Extended mode sense (8-byte response) |
| PREVENT MEDIUM REMOVAL | `0x1E` | Acknowledged silently (returns 0 = success) |
| MECHANISM STATUS | `0xBD` | Returns 8 zero bytes (tray in, no error) |

The block count for the READ TOC response is read from the C-linkage global variable `extern uint32_t cdromBlockCount`, which the firmware updates whenever a disc image is mounted.

#### Patch Marker

The injected block begins with the comment `/* CD-ROM SCSI patched v3 */`. The script checks for this marker before patching so it is safe to run multiple times. Re-running always restores from the `.bak` backup first to ensure the patch is applied cleanly to an unmodified source.

#### Usage

Run from a Windows command prompt or PowerShell (Python 3 required):

```cmd
python patch_usbmsc.py
```

The script auto-detects `USBMSC.cpp` via `%LOCALAPPDATA%\Arduino15`. Output:

```
========================================================
  USBMSC.cpp CD-ROM SCSI Patcher v3
========================================================

File: C:\Users\...\Arduino15\packages\esp32\hardware\esp32\3.3.7\cores\esp32\USBMSC.cpp

  Backup created: USBMSC.cpp.bak
  Found tud_msc_scsi_cb (2184 bytes)
  Patched v3: CD handlers now run BEFORE media_present check

[SUCCESS]
  1. Delete build cache: %LOCALAPPDATA%\arduino\sketches\
  2. Re-upload ESP32S3_VirtualCDROM.ino
  3. Mount ISO - should now show DATA disc
```

#### After the Script

Delete the Arduino build cache and recompile. The patch only affects the compiled binary — the `.ino` source files do not change.

---

## API Reference

The web server exposes a REST-style JSON API consumed by the browser UI.

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | GET | Mounted image state (path, sectors, default image) |
| `/api/sysinfo` | GET | Full device status (WiFi, SD, image, EAP details, system) |
| `/api/isos` | GET | Recursive list of `.iso`/`.bin`/`.cue` files on the SD card |
| `/api/ls?path=` | GET | Directory listing for the given SD path |
| `/api/mount?file=` | POST | Mount the specified disc image |
| `/api/umount` | POST | Eject the current disc |
| `/api/config/get` | GET | Retrieve all configuration (passwords redacted) |
| `/api/config/save` | POST | Save configuration (form-encoded body) |
| `/api/wifi/scan` | GET | Scan for nearby access points |
| `/api/sd/unmount` | POST | Safely unmount the SD card |
| `/api/sd/mount` | POST | Re-mount the SD card |
| `/api/reboot` | POST | Restart the ESP32 |
| `/api/factory` | POST | Factory reset (clears NVS) and reboot |
| `/upload?path=` | POST | Upload a file to the SD card (multipart/form-data) |
| `/api/delete?path=` | DELETE | Delete a file |
| `/api/mkdir?path=` | POST | Create a directory |
| `/api/download?path=` | GET | Download a file from the SD card |

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

- Confirm VCC is connected to **3.3 V** (not 5 V)
- Check the four data wires: CLK→GPIO12, CMD→GPIO11, D0→GPIO13, CS/D3→GPIO10
- Try a different SD card; some cards require a lower SPI frequency (edit `SD_MMC.begin()` clock parameter)
- Confirm the card is formatted FAT32 or exFAT (exFAT requires `build_exfat_libs.py`)

### USB CD-ROM not appearing on host PC

- Confirm the **right** USB port is connected (the OTG port, not the UART bridge)
- Check that `patch_usbmsc.py` was applied and the sketch was recompiled after deleting the build cache
- Arduino IDE must be set to **USB-OTG (TinyUSB)**, not "Hardware CDC"
- Try `umount` then `mount` again from the serial CLI
- Some hosts require a reboot after first enumerating the device

### Wi-Fi fails to connect

- Verify SSID and password with `show config`
- Check that the access point is visible with a scan (web Config tab → Scan button)
- For enterprise networks, ensure the RADIUS SQL table has a row for the EAP identity

### EAP-TLS: connection attempt reaches RADIUS but authentication fails

Check RADIUS server logs. Common causes and remedies:

| Symptom in RADIUS log | Cause | Fix |
|---|---|---|
| `User not found in radcheck table` | EAP identity not in RADIUS database | Add SQL row for the identity shown in `[EAP] Identity:` |
| `Server certificate chain validation failed` | CA cert missing or wrong | Set `eap-ca` to the server's CA, or omit it to skip server verification |
| `TLS handshake failure` | Cipher suite mismatch | Generate a new cert with `sha256WithRSAEncryption` and 2048-bit RSA |
| `Method private structure allocated failure` | Internal mbedTLS error | Usually caused by an encrypted key without a passphrase; set `eap-kpass` |

### EAP-TLS: nothing appears in RADIUS log

The device is not reaching the access point at the 802.11 association level. This typically means Wi-Fi mode or auth type is misconfigured. Verify with `show config` that `eap-mode` is `2` and that the SSID matches the enterprise network name exactly (case-sensitive).

### Private key format error

```
[EAP]  Key : Encrypted  ✗  Set passphrase: set eap-kpass <passphrase>
```

The key is password-protected. Either set the passphrase with `set eap-kpass` or remove the passphrase:

```bash
openssl rsa -in client.key -out client_plain.key
```

### `build_exfat_libs.py` fails at ninja step

If the ESP-IDF environment is not sourced, the `export.sh` step fails. Ensure the lib-builder's ESP-IDF submodule is fully initialised. Run with `--dry-run` first to inspect the commands, then run without the flag.

### Web interface shows "Failed to fetch"

The device's web server and the browser must be on the same network. Check that `wifiConnected` is true in `show config`. If using a VPN or network policy that blocks mDNS, use the IP address directly.
