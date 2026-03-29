// =============================================================================
//  ESP32-S3 Virtual CD-ROM Drive  +  File Manager
//  Version: 3.1
// =============================================================================
//
//  ARDUINO IDE SETTINGS (Tools menu):
//    Board:              ESP32S3 Dev Module
//    USB Mode:           USB-OTG (TinyUSB)
//    USB CDC On Boot:    Disabled
//    Flash Size:         16MB (N16R8)
//    Partition Scheme:   Huge APP (3MB No OTA / 1MB SPIFFS)
//    PSRAM:              OPI PSRAM
//    Upload Mode:        UART0 / Hardware CDC
//    Upload Speed:       921600
//
//  REQUIRED LIBRARIES (Library Manager):
//    - Adafruit NeoPixel
//
//  WIRING (Wemos MicroSD shield -> ESP32-S3):
//    Shield D5 (CLK)  -> GPIO12
//    Shield D7 (MOSI) -> GPIO11
//    Shield D6 (MISO) -> GPIO13
//    Shield D4 (CS)   -> GPIO10
//    VCC              -> 3V3  (NOT 5V!)
//    GND              -> GND
//
//  USB:
//    UART (CH343) -> PC Serial Monitor 115200 baud  (configuration + CLI)
//    USB OTG      -> target PC  (Virtual CD-ROM drive)
//
// =============================================================================

#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_eap_client.h>
#include <esp_wifi.h>       // for esp_wifi_set_config / esp_wifi_start
#include <mbedtls/x509_crt.h>  // for reading CN from client cert
#include <esp_log.h>             // for WPA debug logging
#include <WebServer.h>
#include <SPI.h>
#include "FS.h"
#include "SD_MMC.h"
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "soc/usb_serial_jtag_reg.h"  // disable USB-Serial/JTAG peripheral
#include "USBMSC.h"
extern "C" { bool tud_disconnect(void); bool tud_connect(void); }

// =============================================================================
//  PIN DEFINITIONS
// =============================================================================
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_SCK_PIN   12
#define SD_MISO_PIN  13
#define RGB_LED_PIN  48

// =============================================================================
//  GLOBAL OBJECTS
// =============================================================================
Preferences        prefs;
WebServer          httpServer(80);
Adafruit_NeoPixel  rgbLed(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// =============================================================================
//  CONFIGURATION (NVS / Preferences)
// =============================================================================
// EAP authentication mode
enum EapMode { EAP_NONE = 0, EAP_PEAP = 1, EAP_TLS = 2 };

struct Config {
  char ssid[64];
  char password[64];       // WPA2-Personal password (unused in EAP mode)
  bool dhcp;
  char ip[16];
  char mask[16];
  char gw[16];
  char dns[16];
  char hostname[32];       // DHCP hostname + mDNS name (hostname.local)
  // 802.1x / WPA2-Enterprise
  uint8_t eapMode;         // EapMode enum: 0=none 1=PEAP 2=TLS
  char eapIdentity[64];    // Outer identity (often same as username)
  char eapUsername[64];    // Inner username (PEAP/TTLS)
  char eapPassword[64];    // Inner password (PEAP/TTLS)
  char eapCaPath[64];      // SD path to CA cert PEM  e.g. /wifi/ca.pem
  char eapCertPath[64];    // SD path to client cert  e.g. /wifi/client.crt
  char eapKeyPath[64];     // SD path to client key   e.g. /wifi/client.key
  char eapKeyPass[64];     // Passphrase for encrypted private key (optional)
};
Config cfg;

String defaultMount = "";   // auto-mount image path on boot

void loadConfig() {
  prefs.begin("cfg", true);
  defaultMount = prefs.getString("defmount", "");
  strlcpy(cfg.ssid,     prefs.getString("ssid", "").c_str(),          sizeof(cfg.ssid));
  strlcpy(cfg.password, prefs.getString("pass", "").c_str(),          sizeof(cfg.password));
  cfg.dhcp = prefs.getBool("dhcp", true);
  strlcpy(cfg.ip,   prefs.getString("ip",   "192.168.1.100").c_str(), sizeof(cfg.ip));
  strlcpy(cfg.mask, prefs.getString("mask", "255.255.255.0").c_str(), sizeof(cfg.mask));
  strlcpy(cfg.gw,   prefs.getString("gw",   "192.168.1.1").c_str(),   sizeof(cfg.gw));
  strlcpy(cfg.dns,      prefs.getString("dns",      "8.8.8.8").c_str(),        sizeof(cfg.dns));
  strlcpy(cfg.hostname,  prefs.getString("hostname", "esp32cdrom").c_str(),  sizeof(cfg.hostname));
  cfg.eapMode = (uint8_t)prefs.getUChar("eapMode", 0);
  strlcpy(cfg.eapIdentity, prefs.getString("eapIdent", "").c_str(), sizeof(cfg.eapIdentity));
  strlcpy(cfg.eapUsername, prefs.getString("eapUser",  "").c_str(), sizeof(cfg.eapUsername));
  strlcpy(cfg.eapPassword, prefs.getString("eapPass",  "").c_str(), sizeof(cfg.eapPassword));
  strlcpy(cfg.eapCaPath,   prefs.getString("eapCa",    "").c_str(), sizeof(cfg.eapCaPath));
  strlcpy(cfg.eapCertPath, prefs.getString("eapCert",  "").c_str(), sizeof(cfg.eapCertPath));
  strlcpy(cfg.eapKeyPath,  prefs.getString("eapKey",   "").c_str(), sizeof(cfg.eapKeyPath));
  strlcpy(cfg.eapKeyPass,  prefs.getString("eapKPass", "").c_str(), sizeof(cfg.eapKeyPass));
  prefs.end();
}

void saveConfig() {
  prefs.begin("cfg", false);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.password);
  prefs.putBool("dhcp",   cfg.dhcp);
  prefs.putString("ip",   cfg.ip);
  prefs.putString("mask", cfg.mask);
  prefs.putString("gw",   cfg.gw);
  prefs.putString("dns",      cfg.dns);
  prefs.putString("hostname",  cfg.hostname);
  prefs.putUChar("eapMode",  cfg.eapMode);
  // For string fields: remove key when empty so getString returns "" correctly
  #define PPUT(key, val) do { if (strlen(val)) prefs.putString(key, val); else prefs.remove(key); } while(0)
  PPUT("eapIdent", cfg.eapIdentity);
  PPUT("eapUser",  cfg.eapUsername);
  PPUT("eapPass",  cfg.eapPassword);
  PPUT("eapCa",    cfg.eapCaPath);
  PPUT("eapCert",  cfg.eapCertPath);
  PPUT("eapKey",   cfg.eapKeyPath);
  PPUT("eapKPass", cfg.eapKeyPass);
  #undef PPUT
  prefs.end();
}

void clearConfig() {
  prefs.begin("cfg", false);
  prefs.clear();
  prefs.end();
  // Reset runtime values to defaults
  memset(cfg.ssid,     0, sizeof(cfg.ssid));
  memset(cfg.password, 0, sizeof(cfg.password));
  cfg.dhcp = true;
  strlcpy(cfg.ip,   "192.168.1.100", sizeof(cfg.ip));
  strlcpy(cfg.mask, "255.255.255.0", sizeof(cfg.mask));
  strlcpy(cfg.gw,   "192.168.1.1",  sizeof(cfg.gw));
  strlcpy(cfg.dns,      "8.8.8.8",      sizeof(cfg.dns));
  strlcpy(cfg.hostname, "esp32cdrom",   sizeof(cfg.hostname));
  cfg.eapMode = 0;
  memset(cfg.eapIdentity, 0, sizeof(cfg.eapIdentity));
  memset(cfg.eapUsername,  0, sizeof(cfg.eapUsername));
  memset(cfg.eapPassword,  0, sizeof(cfg.eapPassword));
  memset(cfg.eapCaPath,    0, sizeof(cfg.eapCaPath));
  memset(cfg.eapCertPath,  0, sizeof(cfg.eapCertPath));
  memset(cfg.eapKeyPath,   0, sizeof(cfg.eapKeyPath));
  memset(cfg.eapKeyPass,   0, sizeof(cfg.eapKeyPass));
  defaultMount = "";
  Serial.println(F("[OK]  All NVS configuration cleared."));
}

// =============================================================================
//  SD CARD
// =============================================================================
bool      sdReady        = false;
String    mountedFile    = "";
uint32_t  mountedBlocks  = 0;
uint16_t  mountedBlkSize = 2048;
File      isoFileHandle;
bool      isoOpen        = false;

USBMSC msc;  // registers MSC USB interface descriptor

// MSC state - accessed from TinyUSB callbacks
bool     mscMediaPresent = false;
uint32_t mscBlockCount   = 0;
uint16_t mscBlockSize    = 2048;

// C-linkage block count for USBMSC.cpp READ TOC patch
extern "C" {
  uint32_t cdromBlockCount = 0;
}

bool initSD() {
  Serial.println(F("[SD]   Initializing SD_MMC (SDMMC hardware, 1-bit mode)"));
  Serial.printf("[SD]   Pins: CLK=%d CMD=%d D0=%d D3(CS)=%d\n",
                SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_CS_PIN);
#ifdef CONFIG_FATFS_EXFAT_SUPPORT
  Serial.println(F("[SD]   exFAT support: YES (compiled in)"));
#else
  Serial.println(F("[SD]   exFAT support: NO - recompile with CONFIG_FATFS_EXFAT_SUPPORT=y needed"));
#endif
  SD_MMC.setPins(SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN);
  pinMode(SD_CS_PIN, INPUT_PULLUP);
  if (SD_MMC.begin("/sdcard", true, false, 40000000)) {  // 40 MHz clock
    uint8_t t = SD_MMC.cardType();
    const char* tn = t==CARD_MMC?"MMC":t==CARD_SD?"SD":t==CARD_SDHC?"SDHC/SDXC":"UNKNOWN";
    uint64_t mb = SD_MMC.cardSize() / (1024*1024);
    uint64_t freeMb = (SD_MMC.totalBytes()-SD_MMC.usedBytes()) / (1024*1024);
    uint64_t totalMb = SD_MMC.totalBytes() / (1024*1024);
    Serial.printf("[SD]   OK  Type: %-10s  Size: %llu MB\n", tn, mb);
    Serial.printf("[SD]   Used: %llu MB  Free: %llu MB  Total: %llu MB\n",
                  totalMb - freeMb, freeMb, totalMb);
    return true;
  }
  Serial.println(F("[ERR] SD_MMC.begin() failed!"));
  Serial.println(F("[ERR] Check wiring: D5->GPIO12 D7->GPIO11 D6->GPIO13 D4->GPIO10"));
  Serial.println(F("[ERR] VCC must be 3V3, not 5V! Card inserted?"));
  return false;
}

void closeIso() {
  if (isoOpen) { isoFileHandle.close(); isoOpen = false; }
}

// =============================================================================
//  CUE PARSER
//  Raw sector layout:
//    MODE1/2048 : [DATA 2048]                                   offset=0
//    MODE1/2352 : [sync 12][addr 3][mode 1][DATA 2048][ECC 288] offset=16
//    MODE2/2352 : [sync 12][addr 3][mode 1][subhdr 8][DATA 2048][ECC 280] offset=24
//    MODE2/2336 : [subhdr 8][DATA 2048][ECC 280]                offset=8
// =============================================================================
static uint8_t  binHeaderOffset  = 16;
static uint16_t binRawSectorSize = 2352;
static uint16_t binUserDataSize  = 2048;

String parseCue(const String &cuePath) {
  File cue = SD_MMC.open(cuePath.c_str(), FILE_READ);
  if (!cue) { Serial.printf("[CUE] Cannot open: %s\n", cuePath.c_str()); return ""; }

  String dir = cuePath;
  int slash = dir.lastIndexOf('/');
  dir = (slash >= 0) ? dir.substring(0, slash) : "/";

  String dataTrackBin = "";
  String currentFile  = "";
  bool   foundData    = false;
  binHeaderOffset = 16;  // default

  while (cue.available()) {
    String line = cue.readStringUntil('\n');
    line.trim();

    if (line.startsWith("FILE") || line.startsWith("file")) {
      int q1 = line.indexOf('"'), q2 = line.lastIndexOf('"');
      if (q1 >= 0 && q2 > q1) currentFile = line.substring(q1+1, q2);
    }

    if ((line.startsWith("TRACK") || line.startsWith("track")) && currentFile.length()) {
      String up = line; up.toUpperCase();
      if ((up.indexOf("MODE1") >= 0 || up.indexOf("MODE2") >= 0) && !foundData) {
        dataTrackBin = (dir == "/") ? "/" + currentFile : dir + "/" + currentFile;
        if      (up.indexOf("MODE1/2352") >= 0) { binRawSectorSize=2352; binHeaderOffset=16; binUserDataSize=2048; Serial.println(F("[CUE] MODE1/2352 -> header offset=16")); }
        else if (up.indexOf("MODE2/2352") >= 0) { binRawSectorSize=2352; binHeaderOffset=24; binUserDataSize=2048; Serial.println(F("[CUE] MODE2/2352 -> header offset=24")); }
        else if (up.indexOf("MODE2/2336") >= 0) { binRawSectorSize=2336; binHeaderOffset=8;  binUserDataSize=2048; Serial.println(F("[CUE] MODE2/2336 -> header offset=8")); }
        else if (up.indexOf("MODE1/2048") >= 0) { binRawSectorSize=2048; binHeaderOffset=0;  binUserDataSize=2048; Serial.println(F("[CUE] MODE1/2048 -> header offset=0")); }
        else { binRawSectorSize=2352; binHeaderOffset=16; binUserDataSize=2048; Serial.printf("[CUE] Unknown mode in '%s' -> assuming MODE1/2352\n", line.c_str()); }
        foundData = true;
        Serial.printf("[CUE] Data track: %s\n", dataTrackBin.c_str());
      }
    }
  }
  cue.close();
  if (!foundData) Serial.println(F("[CUE] No data track found!"));
  return dataTrackBin;
}

// =============================================================================
//  MOUNT / UMOUNT
// =============================================================================
bool doMount(const String &filename) {
  if (!sdReady) { Serial.println(F("[ERR] SD not available.")); return false; }

  String fl = filename; fl.toLowerCase();
  String actualFile = filename;

  if (fl.endsWith(".cue")) {
    Serial.printf("[CUE] Parsing: %s\n", filename.c_str());
    actualFile = parseCue(filename);
    if (!actualFile.length()) { Serial.println(F("[ERR] CUE: no data track found.")); return false; }
  }

  closeIso();
  mscMediaPresent = false;
  delay(200);

  isoFileHandle = SD_MMC.open(actualFile.c_str(), FILE_READ);
  if (!isoFileHandle) { Serial.printf("[ERR] Cannot open: %s\n", actualFile.c_str()); return false; }

  uint64_t fileSize = isoFileHandle.size();
  if (!fileSize) { isoFileHandle.close(); Serial.println(F("[ERR] File is empty.")); return false; }

  // Set sector geometry (CUE already set binRawSectorSize/binHeaderOffset via parseCue)
  String orig = filename; orig.toLowerCase();
  if (orig.endsWith(".iso")) {
    mountedBlkSize=2048; binRawSectorSize=2048; binHeaderOffset=0; binUserDataSize=2048;
  } else if (orig.endsWith(".bin")) {
    mountedBlkSize=2352; binRawSectorSize=2352; binHeaderOffset=16; binUserDataSize=2048;
  }
  // .cue: parseCue() already configured everything - don't overwrite

  mountedFile   = filename;
  mountedBlocks = (uint32_t)(fileSize / binRawSectorSize);
  isoOpen       = true;
  mscBlockCount   = mountedBlocks;
  mscBlockSize    = 2048;
  cdromBlockCount = mountedBlocks;
  mscMediaPresent = true;

  msc.begin(mountedBlocks, 2048);
  msc.mediaPresent(true);
  tud_disconnect();
  delay(500);
  tud_connect();

  // Benchmark: read first sector to estimate SD speed
  {
    uint8_t buf[2048];
    isoFileHandle.seek(0);
    unsigned long t0 = micros();
    isoFileHandle.read(buf, 2048);
    unsigned long dt = micros() - t0;
    float sdSpeedMBs = (dt > 0) ? (2048.0f / dt) : 0;
    Serial.printf("[SD]   Read speed: %.2f MB/s  (USB max: ~1.0 MB/s)\n", sdSpeedMBs);
    isoFileHandle.seek(0);  // rewind after benchmark
  }
  Serial.printf("[OK]  Mounted : %s\n", actualFile.c_str());
  Serial.printf("      Image   : %s\n", filename.c_str());
  Serial.printf("      Sectors : %lu x 2048 B  (raw: %u B/sector, header: +%u B)\n",
                mountedBlocks, binRawSectorSize, binHeaderOffset);
  Serial.printf("      Size    : %.1f MB\n", (float)fileSize / (1024.0f * 1024.0f));
  return true;
}

void doUmount() {
  mscMediaPresent = false;
  msc.mediaPresent(false);
  closeIso();
  mountedFile = ""; mountedBlocks = 0;
  mscBlockCount = 0; cdromBlockCount = 0;
  msc.begin(1, 2048);
  tud_disconnect(); delay(300); tud_connect();
  Serial.println(F("[OK]  CD-ROM ejected."));
}

// =============================================================================
//  SD HELPERS
// =============================================================================
String normPath(String p) {
  p.trim();
  if (!p.length() || p[0] != '/') p = "/" + p;
  while (p.length() > 1 && p.endsWith("/")) p.remove(p.length()-1);
  return p;
}

String jsonEsc(const char *s) {
  String r;
  while (*s) {
    if      (*s == '"')  r += "\\\"";
    else if (*s == '\\') r += "\\\\";
    else if (*s == '\n') r += "\\n";
    else if (*s == '\r') r += "\\r";
    else                 r += *s;
    s++;
  }
  return r;
}

bool deleteRecursive(const String &path) {
  File entry = SD_MMC.open(path.c_str());
  if (!entry) return false;
  if (entry.isDirectory()) {
    File child;
    entry.rewindDirectory();
    while ((child = entry.openNextFile())) {
      String cp = (path == "/" ? "" : path) + "/" + String(child.name());
      child.close();
      deleteRecursive(cp);
    }
    entry.close();
    return SD_MMC.rmdir(path.c_str());
  }
  entry.close();
  return SD_MMC.remove(path.c_str());
}

// =============================================================================
//  USB MSC - INQUIRY (sets device type = 0x05 CD-ROM)
// =============================================================================
extern "C" uint32_t tud_msc_inquiry2_cb(uint8_t lun, uint8_t *r, uint32_t bufsize) {
  (void)lun;
  if (bufsize < 36) return 0;
  memset(r, 0, 36);
  r[0] = 0x05;  // peripheral device type: CD-ROM
  r[1] = 0x80;  // removable medium
  r[2] = 0x05;  // version: SPC-3
  r[3] = 0x02;  // response data format
  r[4] = 0x1F;  // additional length
  memcpy(&r[8],  "ESP32-S3",         8);
  memcpy(&r[16], "Virtual CD-ROM  ", 16);
  memcpy(&r[32], "1.00",              4);
  return 36;
}

// =============================================================================
//  USB MSC - READ CALLBACK
// =============================================================================
static int32_t mscReadCb(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (!isoOpen || !mscMediaPresent) return -1;
  uint64_t pos = (binRawSectorSize > 2048)
    ? (uint64_t)lba * binRawSectorSize + binHeaderOffset + offset
    : (uint64_t)lba * 2048 + offset;
  if (!isoFileHandle.seek(pos)) return -1;
  return (int32_t)isoFileHandle.read((uint8_t*)buffer, bufsize);
}

static int32_t mscWriteCb(uint32_t lba, uint32_t offset, uint8_t *buf, uint32_t bufsize) {
  (void)lba; (void)offset; (void)buf; (void)bufsize;
  return -1;  // read-only
}

static void mscFlushCb(void) {}

// Disable the USB Serial/JTAG peripheral so it does not enumerate on the host.
// On ESP32-S3 both the JTAG controller and the USB OTG (TinyUSB) share one
// physical USB port. The JTAG controller is active from power-on by default.
// Clearing USB_PAD_ENABLE disconnects it from the D+/D- pads immediately,
// preventing the host from ever seeing the JTAG device.
// This must run as early as possible — called from setup() before USB.begin().
static void disableUsbJtag() {
  // Clear USB_SERIAL_JTAG_USB_PAD_ENABLE bit — detaches JTAG from USB pads
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
  Serial.println(F("[USB]  JTAG peripheral disabled (USB pads released to OTG)."));
}

void initUSBMSC() {
  msc.vendorID("ESP32-S3");
  msc.productID("VirtualCDROM");
  msc.productRevision("1.00");
  msc.onRead(mscReadCb);
  msc.onWrite(mscWriteCb);
  msc.isWritable(false);
  msc.begin(1, 2048);
  msc.mediaPresent(false);
  disableUsbJtag();  // Detach JTAG from USB pads before USB.begin()
  USB.begin();
  // Immediately disconnect — keep D+ low until a disc image is ready.
  // Without this the host (especially Windows 9x) sees an empty MSC device,
  // then a disconnect/reconnect once the ISO mounts, causing re-enumeration chaos.
  delay(50);
  tud_disconnect();
  Serial.println(F("[OK]  USB MSC initialized (disconnected until disc ready)."));
}

// Call once after a disc image has been mounted to present the drive to the host.
void usbConnect() {
  tud_disconnect(); delay(200);
  tud_connect();
  Serial.println(F("[USB]  Connected to host."));
}

// =============================================================================
//  WIFI
// =============================================================================
bool wifiConnected = false;

// EAP cert buffers — must stay alive while WPA supplicant task runs.
// Freed on next startWiFi() call or when EAP mode disabled.
static char* s_eapCertBuf = nullptr;
static char* s_eapKeyBuf  = nullptr;
static char* s_eapCaBuf   = nullptr;

static void freeEapBuffers() {
  free(s_eapCertBuf); s_eapCertBuf = nullptr;
  free(s_eapKeyBuf);  s_eapKeyBuf  = nullptr;
  free(s_eapCaBuf);   s_eapCaBuf   = nullptr;
}

// Extract CN from a PEM certificate buffer using mbedTLS.
// Writes into out (max outLen bytes). Returns true on success.
static bool extractCertCN(const char* pemBuf, char* out, size_t outLen) {
  mbedtls_x509_crt crt;
  mbedtls_x509_crt_init(&crt);
  int ret = mbedtls_x509_crt_parse(&crt, (const unsigned char*)pemBuf,
                                   strlen(pemBuf) + 1);
  if (ret != 0) {
    mbedtls_x509_crt_free(&crt);
    return false;
  }
  // Walk subject name looking for CN
  mbedtls_x509_name* name = &crt.subject;
  bool found = false;
  while (name) {
    // OID for Common Name: 2.5.4.3
    if (name->oid.len == 3 &&
        memcmp(name->oid.p, "U", 3) == 0) {
      size_t cpLen = name->val.len < outLen-1 ? name->val.len : outLen-1;
      memcpy(out, name->val.p, cpLen);
      out[cpLen] = 0;
      found = true;
      break;
    }
    name = name->next;
  }
  mbedtls_x509_crt_free(&crt);
  return found;
}

// Load a PEM file from SD card into a heap-allocated buffer.
// Caller must free() the returned pointer. Returns nullptr on failure.
static char* loadPemFromSD(const char* path, const char* label = nullptr) {
  if (!sdReady || !path || !path[0]) {
    if (label) Serial.printf("[EAP]  %s path not set\n", label);
    return nullptr;
  }
  File f = SD_MMC.open(path, "r");
  if (!f) { Serial.printf("[EAP]  Cannot open: %s\n", path); return nullptr; }
  size_t sz = f.size();
  if (sz == 0 || sz > 16000) { f.close(); Serial.printf("[EAP]  File too large or empty: %s (%u B)\n", path, sz); return nullptr; }
  // +1 for null terminator required by esp_eap_client API
  // 4096-bit RSA key PEM ~3.2KB, cert ~2KB, CA ~2KB — 16KB limit covers all
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < sz * 3 + 8192) {
    f.close();
    Serial.printf("[EAP]  Low heap (%lu B free) — cannot safely load %s\n", freeHeap, path);
    return nullptr;
  }
  char* buf = (char*)malloc(sz + 1);
  if (!buf) { f.close(); Serial.printf("[EAP]  malloc(%u) failed (heap: %lu B)\n", sz+1, freeHeap); return nullptr; }
  f.readBytes(buf, sz);
  buf[sz] = 0;
  f.close();
  Serial.printf("[EAP]  Loaded %s (%u bytes)\n", path, sz);
  return buf;
}

void startWiFi() {
  if (!strlen(cfg.ssid)) { Serial.println(F("[WARN] SSID not configured.")); return; }
  freeEapBuffers();  // Free any previous EAP cert buffers
  Serial.printf("[WiFi] Connecting to: %s\n", cfg.ssid);
  WiFi.mode(WIFI_STA); WiFi.disconnect(false); delay(100);
  // Hostname must be set before WiFi.begin()
  WiFi.setHostname(cfg.hostname);
  if (!cfg.dhcp) {
    IPAddress ip, mask, gw;
    IPAddress dns1;
    ip.fromString(cfg.ip); mask.fromString(cfg.mask); gw.fromString(cfg.gw);
    dns1.fromString(strlen(cfg.dns) ? cfg.dns : "8.8.8.8");
    WiFi.config(ip, gw, mask, dns1);
  }

  if (cfg.eapMode == EAP_PEAP || cfg.eapMode == EAP_TLS) {
    // ── WPA2-Enterprise (802.1x) ──────────────────────────────────────────
    const char* modeStr = (cfg.eapMode == EAP_PEAP) ? "PEAP" : "EAP-TLS";
    Serial.printf("[EAP]  Mode: %s\n", modeStr);
    esp_eap_client_set_disable_time_check(true);

    // Clear previous EAP config
    esp_eap_client_clear_identity();
    esp_eap_client_clear_username();
    esp_eap_client_clear_password();
    esp_eap_client_clear_ca_cert();
    esp_eap_client_clear_certificate_and_key();

    // Identity: configured or auto-read CN from cert
    char effectiveIdentity[64];
    strlcpy(effectiveIdentity, cfg.eapIdentity, sizeof(effectiveIdentity));
    if (cfg.eapMode == EAP_TLS && !strlen(effectiveIdentity)) {
      char* tmpCert = loadPemFromSD(cfg.eapCertPath, nullptr);
      if (tmpCert) {
        char cnBuf[64] = {};
        if (extractCertCN(tmpCert, cnBuf, sizeof(cnBuf))) {
          strlcpy(effectiveIdentity, cnBuf, sizeof(effectiveIdentity));
          Serial.printf("[EAP]  Auto identity from cert CN: %s\n", effectiveIdentity);
        }
        free(tmpCert);
      }
    }
    if (strlen(effectiveIdentity)) {
      esp_eap_client_set_identity((uint8_t*)effectiveIdentity, strlen(effectiveIdentity));
      Serial.printf("[EAP]  Identity: %s\n", effectiveIdentity);
    }

    // CA cert (optional) — kept in global buffer for supplicant task lifetime
    s_eapCaBuf = loadPemFromSD(cfg.eapCaPath, nullptr);
    if (s_eapCaBuf) {
      esp_eap_client_set_ca_cert((uint8_t*)s_eapCaBuf, strlen(s_eapCaBuf) + 1);
      Serial.println(F("[EAP]  CA cert loaded"));
    } else {
      Serial.println(F("[EAP]  No CA cert — server cert not verified"));
    }

    if (cfg.eapMode == EAP_PEAP) {
      // PEAP: username + password
      if (strlen(cfg.eapUsername))
        esp_eap_client_set_username((uint8_t*)cfg.eapUsername, strlen(cfg.eapUsername));
      if (strlen(cfg.eapPassword))
        esp_eap_client_set_password((uint8_t*)cfg.eapPassword, strlen(cfg.eapPassword));
      Serial.printf("[EAP]  PEAP username: %s\n", cfg.eapUsername);
      esp_wifi_sta_enterprise_enable();
      WiFi.begin(cfg.ssid, WPA2_AUTH_PEAP,
                 effectiveIdentity, cfg.eapUsername, cfg.eapPassword,
                 nullptr, nullptr, nullptr);

    } else {
      // EAP-TLS: client cert + key
      // Buffers kept in globals — supplicant task needs them during handshake!
      s_eapCertBuf = loadPemFromSD(cfg.eapCertPath, "Client cert");
      s_eapKeyBuf  = loadPemFromSD(cfg.eapKeyPath,  "Client key");
      if (!s_eapCertBuf || !s_eapKeyBuf) {
        Serial.printf("[EAP]  ERROR: cert='%s' key='%s'\n",
          strlen(cfg.eapCertPath) ? cfg.eapCertPath : "(not set)",
          strlen(cfg.eapKeyPath)  ? cfg.eapKeyPath  : "(not set)");
        freeEapBuffers();
        Serial.println(F("[EAP]  Aborting."));
        return;
      }
      // Key format info — PKCS#8 warning only, not blocked (may work on some builds)
      Serial.println(F("[EAP]  ── Format Check ──"));
      bool keyOk = true;
      if (strstr(s_eapKeyBuf, "BEGIN RSA PRIVATE KEY"))
        Serial.println(F("[EAP]  Key : PKCS#1 RSA  ✓"));
      else if (strstr(s_eapKeyBuf, "BEGIN EC PRIVATE KEY"))
        Serial.println(F("[EAP]  Key : EC  ✓"));
      else if (strstr(s_eapKeyBuf, "BEGIN PRIVATE KEY"))
        Serial.println(F("[EAP]  Key : PKCS#8  ✓  (supported by ESP-IDF 5.x mbedTLS)"));
      else if (strstr(s_eapKeyBuf, "BEGIN ENCRYPTED PRIVATE KEY")) {
        if (strlen(cfg.eapKeyPass)) {
          Serial.println(F("[EAP]  Key : Encrypted  ✓  (passphrase provided)"));
        } else {
          Serial.println(F("[EAP]  Key : Encrypted  ✗  Set passphrase: set eap-kpass <passphrase>"));
          keyOk = false;
        }
      } else {
        Serial.println(F("[EAP]  Key : Unknown format — attempting anyway"));
      }
      if (strstr(s_eapCertBuf, "BEGIN CERTIFICATE")) Serial.println(F("[EAP]  Cert: X.509 PEM  ✓"));
      else { Serial.println(F("[EAP]  Cert: Unknown  ✗")); keyOk = false; }
      if (!keyOk) { freeEapBuffers(); Serial.println(F("[EAP]  Aborting.")); return; }

      Serial.printf("[EAP]  Client cert + key OK  (heap: %lu B)\n", ESP.getFreeHeap());
      // Key passphrase (for encrypted private keys)
      const uint8_t* keyPass   = strlen(cfg.eapKeyPass) ? (uint8_t*)cfg.eapKeyPass : nullptr;
      int            keyPassLen = strlen(cfg.eapKeyPass);
      if (keyPass) Serial.println(F("[EAP]  Key passphrase: provided"));
      esp_eap_client_set_certificate_and_key(
        (uint8_t*)s_eapCertBuf, strlen(s_eapCertBuf) + 1,
        (uint8_t*)s_eapKeyBuf,  strlen(s_eapKeyBuf)  + 1,
        keyPass, keyPassLen);
      esp_wifi_sta_enterprise_enable();
      // WPA2_AUTH_TLS tells AP this is enterprise — needed for 802.11 association.
      // Certs already set via API above; nullptr args = don't override.
      WiFi.begin(cfg.ssid, WPA2_AUTH_TLS,
                 effectiveIdentity, nullptr, nullptr,
                 nullptr, nullptr, nullptr);
    }

  } else {
    // ── WPA2-Personal (standard) ───────────────────────────────────────────
    WiFi.begin(cfg.ssid, cfg.password);
  }
  Serial.print(F("[WiFi] "));
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis()-t0 > 15000) break;
    delay(500); Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("[WiFi] Connected!  IP: %s  RSSI: %d dBm  Ch: %d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel());
    // Start mDNS so device is reachable as hostname.local
    // mDNS uses only the first label of an FQDN (before first dot)
    char mdnsLabel[32];
    strlcpy(mdnsLabel, cfg.hostname, sizeof(mdnsLabel));
    if (char* dot = strchr(mdnsLabel, '.')) *dot = 0;  // truncate at first dot
    if (MDNS.begin(mdnsLabel)) {
      if (strchr(cfg.hostname, '.')) {
        Serial.printf("[mDNS] Responder started: http://%s.local  (FQDN: %s)\n", mdnsLabel, cfg.hostname);
      } else {
        Serial.printf("[mDNS] Responder started: http://%s.local\n", mdnsLabel);
      }
    } else {
      Serial.println(F("[mDNS] Failed to start responder"));
    }
  } else {
    int st = WiFi.status();
    Serial.printf("[WiFi] Failed (status=%d)\n", st);
    if (cfg.eapMode) {
      Serial.println(F("[EAP]  Connection failed. Common causes:"));
      Serial.println(F("[EAP]   1. User not in RADIUS DB — add via SQL or users file"));
      Serial.printf("[EAP]      INSERT INTO radcheck (username,attribute,value,op)\n");
      Serial.printf("[EAP]        VALUES ('%s','Auth-Type','EAP',':=');\n",
                    strlen(cfg.eapIdentity) ? cfg.eapIdentity : "username");
      Serial.println(F("[EAP]   2. RADIUS rejected cert — check raddb/certs CA chain"));
      Serial.println(F("[EAP]   3. Identity mismatch — cert CN must match eap-id"));
      Serial.println(F("[EAP]   4. Cipher suite unsupported by mbedTLS"));
      Serial.println(F("[EAP]   5. Cert not PEM / key has passphrase (not supported)"));
    }
  }
}

// =============================================================================
//  HTML PAGE (separate file to avoid Arduino IDE preprocessor bugs)
// =============================================================================
#include "html_page.h"

// =============================================================================
//  FILE UPLOAD HANDLER
// =============================================================================
File   uploadFile;
String uploadFilePath = "";
bool   uploadActive   = false;
bool   uploadOk       = false;
bool   downloadActive = false;

void handleFileUpload() {
  HTTPUpload &upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String dir = httpServer.hasArg("path") ? normPath(httpServer.arg("path")) : "/";
    uploadFilePath = (dir == "/" ? "" : dir) + "/" + String(upload.filename);
    Serial.printf("[UPLOAD] Start: %s\n", uploadFilePath.c_str());
    uploadOk     = (bool)(uploadFile = SD_MMC.open(uploadFilePath.c_str(), FILE_WRITE));
    uploadActive = true;
    if (!uploadOk) Serial.printf("[ERR] Cannot open for write: %s\n", uploadFilePath.c_str());
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadOk && uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Serial.println(F("[ERR] Write error!")); uploadOk = false;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadOk) {
      uploadFile.close();
      Serial.printf("[UPLOAD] OK: %s  (%zu B)\n", uploadFilePath.c_str(), upload.totalSize);
    } else {
      if (uploadFile) uploadFile.close();
      SD_MMC.remove(uploadFilePath.c_str());
    }
    uploadActive = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    if (uploadActive) SD_MMC.remove(uploadFilePath.c_str());
    uploadActive = false; uploadOk = false;
  }
}

// =============================================================================
//  API HANDLERS
// =============================================================================
void handleRoot() {
  httpServer.send_P(200, "text/html; charset=utf-8", HTML_PAGE);
}

void handleApiStatus() {
  String j = "{\"mounted\":";
  j += (isoOpen && mountedFile.length()) ? "true" : "false";
  j += ",\"file\":\"" + jsonEsc(mountedFile.c_str()) + "\"";
  j += ",\"sectors\":"  + String(mountedBlocks);
  j += ",\"blksize\":"  + String(mscBlockSize);
  j += ",\"rawsector\":" + String(binRawSectorSize);
  j += ",\"hdroffset\":" + String(binHeaderOffset);
  j += ",\"default\":\"" + jsonEsc(defaultMount.c_str()) + "\"}";
  httpServer.send(200, "application/json", j);
}

void handleApiSysinfo() {
  String j = "{";

  // WiFi
  bool wc = (WiFi.status() == WL_CONNECTED);
  j += "\"wifi_connected\":" + String(wc ? "true" : "false");
  if (wc) {
    int rssi = WiFi.RSSI();
    const char* q = rssi > -50 ? "Excellent" : rssi > -65 ? "Good" : rssi > -75 ? "Fair" : "Weak";
    int wch = WiFi.channel();
    const char* wband = (wch >= 36) ? "5 GHz" : "2.4 GHz";
    j += ",\"wifi_ssid\":\""    + jsonEsc(WiFi.SSID().c_str())            + "\"";
    j += ",\"wifi_rssi\":"      + String(rssi);
    j += ",\"wifi_quality\":\"" + String(q)                               + "\"";
    j += ",\"wifi_band\":\""    + String(wband)                           + "\"";
    j += ",\"wifi_ip\":\""      + WiFi.localIP().toString()               + "\"";
    j += ",\"wifi_mask\":\""    + WiFi.subnetMask().toString()            + "\"";
    j += ",\"wifi_gw\":\""      + WiFi.gatewayIP().toString()             + "\"";
    j += ",\"wifi_dns\":\""     + WiFi.dnsIP().toString()                 + "\"";
    j += ",\"wifi_channel\":"   + String(wch);
    j += ",\"wifi_bssid\":\""   + jsonEsc(WiFi.BSSIDstr().c_str())        + "\"";
    j += ",\"wifi_hostname\":\"" + jsonEsc(cfg.hostname) + "\""; 
    // mDNS label = first part of FQDN
    String mdnsLbl = String(cfg.hostname);
    int dotPos = mdnsLbl.indexOf('.');
    if (dotPos > 0) mdnsLbl = mdnsLbl.substring(0, dotPos);
    j += ",\"wifi_mdns\":\"" + mdnsLbl + ".local\"";
    const char* eapStr = cfg.eapMode==1?"PEAP":cfg.eapMode==2?"EAP-TLS":"WPA2-Personal";
    j += ",\"wifi_auth\":\"" + String(eapStr) + "\"";
    // EAP security details for status display
    if (cfg.eapMode) {
      j += ",\"eap_mode\":" + String(cfg.eapMode);
      j += ",\"eap_identity\":\"" + jsonEsc(strlen(cfg.eapIdentity) ? cfg.eapIdentity : "") + "\"";
      j += ",\"eap_ca\":\"" + jsonEsc(strlen(cfg.eapCaPath) ? cfg.eapCaPath : "") + "\"";
      if (cfg.eapMode == 2) {
        j += ",\"eap_cert\":\"" + jsonEsc(strlen(cfg.eapCertPath) ? cfg.eapCertPath : "") + "\"";
        j += ",\"eap_key\":\"" + jsonEsc(strlen(cfg.eapKeyPath) ? cfg.eapKeyPath : "") + "\"";
        j += ",\"eap_kpass\":" + String(strlen(cfg.eapKeyPass) ? "true" : "false");
        // Runtime cert info from loaded buffers
        if (s_eapCertBuf) {
          j += ",\"cert_format\":\"";
          if (strstr(s_eapCertBuf,"BEGIN CERTIFICATE")) j += "X.509 PEM";
          else j += "unknown";
          j += "\"";
        }
        if (s_eapKeyBuf) {
          j += ",\"key_format\":\"";
          if      (strstr(s_eapKeyBuf,"BEGIN RSA PRIVATE KEY")) j += "PKCS#1 RSA";
          else if (strstr(s_eapKeyBuf,"BEGIN EC PRIVATE KEY"))  j += "EC";
          else if (strstr(s_eapKeyBuf,"BEGIN PRIVATE KEY"))     j += "PKCS#8";
          else if (strstr(s_eapKeyBuf,"BEGIN ENCRYPTED PRIVATE KEY")) j += "Encrypted";
          else j += "unknown";
          j += "\"";
        }
      }
    }
  }

  // SD card
  j += ",\"sd_ready\":"  + String(sdReady ? "true" : "false");
  if (sdReady) {
    uint64_t tot  = SD_MMC.totalBytes() / (1024*1024);
    uint64_t used = SD_MMC.usedBytes()  / (1024*1024);
    j += ",\"sd_total_mb\":"  + String((uint32_t)tot);
    j += ",\"sd_used_mb\":"   + String((uint32_t)used);
    j += ",\"sd_free_mb\":"   + String((uint32_t)(tot - used));
  }

  // Mounted image
  bool mounted = isoOpen && mountedFile.length();
  j += ",\"img_mounted\":"   + String(mounted ? "true" : "false");
  j += ",\"img_file\":\""    + jsonEsc(mountedFile.c_str())        + "\"";
  j += ",\"img_default\":\"" + jsonEsc(defaultMount.c_str())       + "\"";
  if (mounted) {
    float sizeMb = (float)mountedBlocks * 2048.0f / (1024.0f * 1024.0f);
    j += ",\"img_sectors\":"    + String(mountedBlocks);
    j += ",\"img_size_mb\":"    + String(sizeMb, 1);
    j += ",\"img_rawsector\":"  + String(binRawSectorSize);
    j += ",\"img_hdroffset\":"  + String(binHeaderOffset);
    j += ",\"img_usbpresent\":" + String(mscMediaPresent ? "true" : "false");
  }

  // System
  j += ",\"sys_heap\":"    + String(ESP.getFreeHeap());
  j += ",\"sys_uptime\":"  + String(millis() / 1000);
  j += ",\"sys_cpu_mhz\":" + String(getCpuFrequencyMhz());
  j += ",\"sys_flash_mb\":" + String(ESP.getFlashChipSize() / (1024*1024));
  j += ",\"sys_sdk\":\""   + jsonEsc(ESP.getSdkVersion())           + "\"";

  j += "}";
  httpServer.send(200, "application/json", j);
}

void collectIsos(File &dir, const String &base, String &json, bool &first) {
  File f;
  while ((f = dir.openNextFile())) {
    String n = String(f.name()), nl = n; nl.toLowerCase();
    bool isD = f.isDirectory();
    if (isD) {
      String sub = (base == "/" ? "" : base) + "/" + n;
      collectIsos(f, sub, json, first);
    } else if (nl.endsWith(".iso") || nl.endsWith(".bin") || nl.endsWith(".cue")) {
      String fp = (base == "/" ? "" : base) + "/" + n;
      if (!first) json += ",";
      json += "{\"path\":\"" + jsonEsc(fp.c_str()) + "\",\"size\":" + String((uint32_t)f.size()) + "}";
      first = false;
    }
    f.close();
  }
}

void handleApiIsos() {
  if (!sdReady) { httpServer.send(200, "application/json", "[]"); return; }
  File root = SD_MMC.open("/");
  if (!root) { httpServer.send(200, "application/json", "[]"); return; }
  String json = "["; bool first = true;
  collectIsos(root, "/", json, first);
  root.close(); json += "]";
  httpServer.send(200, "application/json", json);
}

void handleApiLs() {
  String path = httpServer.hasArg("path") ? normPath(httpServer.arg("path")) : "/";
  Serial.printf("[API]  GET /api/ls path=%s\n", path.c_str());
  if (!sdReady) { httpServer.send(200, "application/json", "[]"); return; }
  File dir = SD_MMC.open(path.c_str());
  if (!dir) { httpServer.send(404, "application/json", "[]"); return; }
  String json = "["; bool first = true; int count = 0;
  File f;
  while ((f = dir.openNextFile())) {
    String n = String(f.name()); bool isD = f.isDirectory();
    Serial.printf("[LS]     '%s' dir=%d size=%lu\n", f.name(), (int)isD, (uint32_t)f.size());
    if (!first) json += ",";
    json += "{\"name\":\"" + jsonEsc(f.name()) + "\",\"dir\":" + (isD?"true":"false")
          + ",\"size\":" + String((uint32_t)f.size()) + "}";
    first = false; count++; f.close();
  }
  dir.close(); json += "]";
  Serial.printf("[API]  ls: %d items\n", count);
  httpServer.send(200, "application/json", json);
}

void handleApiMount() {
  if (!httpServer.hasArg("file")) { httpServer.send(400, "text/plain", "Missing 'file'"); return; }
  String f = httpServer.arg("file");
  if (doMount(f)) httpServer.send(200, "text/plain", "OK: Mounted: " + f);
  else            httpServer.send(500, "text/plain", "ERROR: Cannot mount: " + f);
}

void handleApiUmount() {
  doUmount();
  httpServer.send(200, "text/plain", "OK: Ejected.");
}


// Safe SD card unmount -- ejects disc image, then deinits SD_MMC so the card
// can be physically removed without filesystem corruption.
void handleApiSdUnmount() {
  // Refuse if a file transfer is actively in progress
  if (uploadActive) {
    Serial.println(F("[SD]   Unmount blocked: upload in progress."));
    httpServer.send(409, "text/plain", "ERROR: Upload in progress - wait for it to finish first.");
    return;
  }
  if (downloadActive) {
    Serial.println(F("[SD]   Unmount blocked: download in progress."));
    httpServer.send(409, "text/plain", "ERROR: Download in progress - wait for it to finish first.");
    return;
  }

  // Step 1: eject disc image from USB (tud_disconnect + reconnect without media)
  if (isoOpen || mscMediaPresent) {
    Serial.println(F("[SD]   Ejecting disc image before SD unmount..."));
    doUmount();
    delay(300);  // give USB host time to process eject
  }

  // Step 2: close any leftover open file handle (safety net)
  if (uploadFile) { uploadFile.close(); }
  closeIso();

  // Step 3: deinit SD_MMC - card is now safe to remove
  if (sdReady) {
    SD_MMC.end();
    sdReady = false;
    Serial.println(F("[SD]   Safely unmounted. Card can be removed."));
    httpServer.send(200, "text/plain", "OK: SD card safely unmounted. You can now remove the card.");
  } else {
    httpServer.send(200, "text/plain", "OK: SD was already unmounted.");
  }
}

// Remount SD card after physical insertion.
void handleApiSdMount() {
  if (sdReady) {
    SD_MMC.end(); delay(100);
  }
  sdReady = initSD();
  if (sdReady) {
    Serial.println(F("[SD]   Remounted OK."));
    httpServer.send(200, "text/plain", "OK: SD card mounted.");
  } else {
    httpServer.send(500, "text/plain", "ERROR: SD card not found. Is the card inserted?");
  }
}

void handleApiSetDefault() {
  String target = httpServer.hasArg("file") ? httpServer.arg("file") : mountedFile;
  if (!target.length()) { httpServer.send(400, "text/plain", "Nothing mounted"); return; }
  defaultMount = target;
  prefs.begin("cfg", false);
  prefs.putString("defmount", defaultMount);
  prefs.end();
  Serial.printf("[OK]  Default mount set: %s\n", defaultMount.c_str());
  httpServer.send(200, "text/plain", "OK: Default set: " + defaultMount);
}

void handleApiClearDefault() {
  defaultMount = "";
  prefs.begin("cfg", false);
  prefs.putString("defmount", "");
  prefs.end();
  Serial.println(F("[OK]  Default mount cleared."));
  httpServer.send(200, "text/plain", "OK: Default cleared.");
}


void handleApiWifiScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    int ch = WiFi.channel(i);
    const char* band = (ch >= 36) ? "5G" : "2.4G";
    j += "{\"ssid\":\""  + jsonEsc(WiFi.SSID(i).c_str()) + "\"";
    j += ",\"bssid\":\"" + jsonEsc(WiFi.BSSIDstr(i).c_str()) + "\"";
    j += ",\"rssi\":"    + String(WiFi.RSSI(i));
    j += ",\"ch\":"      + String(ch);
    j += ",\"band\":\""  + String(band) + "\"";
    j += ",\"enc\":"     + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0) + "}";
  }
  WiFi.scanDelete();
  j += "]";
  httpServer.send(200, "application/json", j);
}

void handleApiGetConfig() {
  String j = "{";
  j += "\"ssid\":\""  + jsonEsc(cfg.ssid)  + "\"";
  j += ",\"dhcp\":"   + String(cfg.dhcp ? "true" : "false");
  j += ",\"ip\":\""   + String(cfg.ip)     + "\"";
  j += ",\"mask\":\"" + String(cfg.mask)   + "\"";
  j += ",\"gw\":\""   + String(cfg.gw)    + "\"";
  j += ",\"dns\":\""  + String(cfg.dns)   + "\"";
  j += ",\"hostname\":\"" + jsonEsc(cfg.hostname) + "\"";
  j += ",\"eapMode\":" + String(cfg.eapMode);
  j += ",\"eapIdentity\":\"" + jsonEsc(cfg.eapIdentity) + "\"";
  j += ",\"eapUsername\":\"" + jsonEsc(cfg.eapUsername) + "\"";
  j += ",\"eapCaPath\":\"" + jsonEsc(cfg.eapCaPath) + "\"";
  j += ",\"eapCertPath\":\"" + jsonEsc(cfg.eapCertPath) + "\"";
  j += ",\"eapKeyPath\":\"" + jsonEsc(cfg.eapKeyPath) + "\"";
  j += ",\"eapKeyPass\":\"" + String(strlen(cfg.eapKeyPass) ? "set" : "") + "\"";
  j += "}";
  httpServer.send(200, "application/json", j);
}

void handleApiSaveConfig() {
  bool changed = false;
  if (httpServer.hasArg("ssid")) {
    strlcpy(cfg.ssid, httpServer.arg("ssid").c_str(), sizeof(cfg.ssid)); changed = true;
  }
  if (httpServer.hasArg("pass") && httpServer.arg("pass").length()) {
    strlcpy(cfg.password, httpServer.arg("pass").c_str(), sizeof(cfg.password)); changed = true;
  }
  if (httpServer.hasArg("dhcp")) {
    String v = httpServer.arg("dhcp"); v.toLowerCase();
    cfg.dhcp = (v == "true" || v == "1" || v == "on"); changed = true;
  }
  if (httpServer.hasArg("ip"))   { strlcpy(cfg.ip,   httpServer.arg("ip").c_str(),   sizeof(cfg.ip));   changed = true; }
  if (httpServer.hasArg("mask")) { strlcpy(cfg.mask, httpServer.arg("mask").c_str(), sizeof(cfg.mask)); changed = true; }
  if (httpServer.hasArg("gw"))   { strlcpy(cfg.gw,   httpServer.arg("gw").c_str(),   sizeof(cfg.gw));   changed = true; }
  if (httpServer.hasArg("dns"))      { strlcpy(cfg.dns,      httpServer.arg("dns").c_str(),      sizeof(cfg.dns));      changed = true; }
  if (httpServer.hasArg("hostname")) { strlcpy(cfg.hostname, httpServer.arg("hostname").c_str(), sizeof(cfg.hostname)); changed = true; }
  if (httpServer.hasArg("eapMode"))    { cfg.eapMode = (uint8_t)httpServer.arg("eapMode").toInt(); changed = true; }
  if (httpServer.hasArg("eapIdentity")){ strlcpy(cfg.eapIdentity, httpServer.arg("eapIdentity").c_str(), sizeof(cfg.eapIdentity)); changed = true; }
  if (httpServer.hasArg("eapUsername")){ strlcpy(cfg.eapUsername, httpServer.arg("eapUsername").c_str(), sizeof(cfg.eapUsername)); changed = true; }
  if (httpServer.hasArg("eapPassword")){ strlcpy(cfg.eapPassword, httpServer.arg("eapPassword").c_str(), sizeof(cfg.eapPassword)); changed = true; }
  if (httpServer.hasArg("eapCaPath"))  { strlcpy(cfg.eapCaPath,   httpServer.arg("eapCaPath").c_str(),   sizeof(cfg.eapCaPath));   changed = true; }
  if (httpServer.hasArg("eapCertPath")){ strlcpy(cfg.eapCertPath, httpServer.arg("eapCertPath").c_str(), sizeof(cfg.eapCertPath)); changed = true; }
  if (httpServer.hasArg("eapKeyPath")) { strlcpy(cfg.eapKeyPath,  httpServer.arg("eapKeyPath").c_str(),  sizeof(cfg.eapKeyPath));  changed = true; }
  if (httpServer.hasArg("eapKeyPass") && httpServer.arg("eapKeyPass").length()) { strlcpy(cfg.eapKeyPass, httpServer.arg("eapKeyPass").c_str(), sizeof(cfg.eapKeyPass)); changed = true; }
  if (httpServer.hasArg("eapKeyPass") && httpServer.arg("eapKeyPass").length()) { strlcpy(cfg.eapKeyPass, httpServer.arg("eapKeyPass").c_str(), sizeof(cfg.eapKeyPass)); changed = true; }
  if (changed) {
    saveConfig();
    Serial.println(F("[OK]  Config saved via web."));
    httpServer.send(200, "text/plain", "OK: Configuration saved. Reboot to apply WiFi changes.");
  } else {
    httpServer.send(400, "text/plain", "No parameters provided.");
  }
}

void handleApiReboot() {
  httpServer.send(200, "text/plain", "OK: Rebooting...");
  delay(500);
  ESP.restart();
}

void handleApiFactoryReset() {
  clearConfig();
  httpServer.send(200, "text/plain", "OK: Factory reset done. Rebooting...");
  delay(500);
  ESP.restart();
}

void handleApiDownload() {
  if (!sdReady) { httpServer.send(503, "text/plain", "SD not available."); return; }
  if (!httpServer.hasArg("path")) { httpServer.send(400, "text/plain", "Missing 'path'"); return; }
  String path = normPath(httpServer.arg("path"));
  File f = SD_MMC.open(path.c_str(), FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    httpServer.send(404, "text/plain", "Not found: " + path);
    return;
  }
  String fname = path;
  int sl = fname.lastIndexOf('/');
  if (sl >= 0) fname = fname.substring(sl + 1);
  size_t fileSize = f.size();
  Serial.printf("[DOWNLOAD] Start: %s  (%zu B)\n", path.c_str(), fileSize);

  // FIX: correct ASCII quotes in filename (curly quotes "" were wrong)
  httpServer.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
  httpServer.sendHeader("Connection", "close");
  httpServer.setContentLength(fileSize);
  httpServer.send(200, "application/octet-stream", "");

  // Chunked manual streaming:
  //   - downloadActive flag  -> blocks SD unmount while transfer is in progress
  //   - client.connected()   -> abort immediately when browser cancels download
  //   - 5 s timeout          -> safety net against stalled client
  //   - 8 KB buffer          -> ~5-8x faster than default streamFile()
  downloadActive = true;
  WiFiClient client = httpServer.client();
  static uint8_t buf[8192];
  size_t sent = 0;
  unsigned long lastOk = millis();

  while (sent < fileSize) {
    if (!client.connected()) {
      Serial.println("[DOWNLOAD] Client disconnected - aborting.");
      break;
    }
    size_t toRead = min((size_t)sizeof(buf), fileSize - sent);
    int rd = f.read(buf, toRead);
    if (rd <= 0) break;
    size_t wr = client.write(buf, (size_t)rd);
    if (wr > 0) { sent += wr; lastOk = millis(); }
    if (millis() - lastOk > 5000) {
      Serial.println("[DOWNLOAD] Timeout - client stopped reading.");
      break;
    }
    yield();
  }

  downloadActive = false;
  f.close();
  client.stop();
  Serial.printf("[DOWNLOAD] Done: %zu / %zu B sent.\n", sent, fileSize);
}

void handleApiMkdir() {
  if (!sdReady) { httpServer.send(503, "text/plain", "SD not available."); return; }
  if (!httpServer.hasArg("path")) { httpServer.send(400, "text/plain", "Missing 'path'"); return; }
  String path = normPath(httpServer.arg("path"));
  if (SD_MMC.mkdir(path.c_str())) httpServer.send(200, "text/plain", "OK: Created: " + path);
  else                             httpServer.send(500, "text/plain", "ERROR: mkdir failed: " + path);
}

void handleApiDelete() {
  if (!sdReady) { httpServer.send(503, "text/plain", "SD not available."); return; }
  if (!httpServer.hasArg("path")) { httpServer.send(400, "text/plain", "Missing 'path'"); return; }
  String path = normPath(httpServer.arg("path"));
  if (path == mountedFile) doUmount();
  if (deleteRecursive(path)) httpServer.send(200, "text/plain", "OK: Deleted: " + path);
  else                        httpServer.send(500, "text/plain", "ERROR: delete failed: " + path);
}

void handleApiUploadDone() {
  if (uploadOk) httpServer.send(200, "text/plain", "OK");
  else          httpServer.send(500, "text/plain", "ERROR: upload failed");
}

void setupWebServer() {
  httpServer.on("/",                 HTTP_GET,  handleRoot);
  httpServer.on("/api/status",       HTTP_GET,  handleApiStatus);
  httpServer.on("/api/sysinfo",      HTTP_GET,  handleApiSysinfo);
  httpServer.on("/api/isos",         HTTP_GET,  handleApiIsos);
  httpServer.on("/api/ls",           HTTP_GET,  handleApiLs);
  httpServer.on("/api/mount",        HTTP_GET,  handleApiMount);
  httpServer.on("/api/umount",       HTTP_GET,  handleApiUmount);
  httpServer.on("/api/sd/unmount",   HTTP_GET,  handleApiSdUnmount);
  httpServer.on("/api/sd/mount",     HTTP_GET,  handleApiSdMount);
  httpServer.on("/api/setdefault",   HTTP_GET,  handleApiSetDefault);
  httpServer.on("/api/cleardefault", HTTP_GET,  handleApiClearDefault);
  httpServer.on("/api/wifi/scan",    HTTP_GET,  handleApiWifiScan);
  httpServer.on("/api/config/get",   HTTP_GET,  handleApiGetConfig);
  httpServer.on("/api/config/save",  HTTP_GET,  handleApiSaveConfig);
  httpServer.on("/api/reboot",       HTTP_GET,  handleApiReboot);
  httpServer.on("/api/factory",      HTTP_GET,  handleApiFactoryReset);
  httpServer.on("/api/download",      HTTP_GET,  handleApiDownload);
  httpServer.on("/api/mkdir",        HTTP_GET,  handleApiMkdir);
  httpServer.on("/api/delete",       HTTP_GET,  handleApiDelete);
  httpServer.on("/api/upload",       HTTP_POST, handleApiUploadDone, handleFileUpload);
  httpServer.begin();
  Serial.printf("[OK]  HTTP server started at http://%s\n", WiFi.localIP().toString().c_str());
}

// =============================================================================
//  UART CLI
// =============================================================================
void printSep() { Serial.println(F("================================================")); }

void printConfig(bool showRuntime = true) {
  printSep();
  Serial.println(F("  CONFIGURATION (NVS)"));
  printSep();
  Serial.printf("  WiFi SSID     : %s\n",  strlen(cfg.ssid)     ? cfg.ssid    : "(not set)");
  Serial.printf("  WiFi Password : %s\n",  strlen(cfg.password) ? "********"  : "(not set)");
  Serial.printf("  DHCP          : %s\n",  cfg.dhcp ? "enabled" : "disabled");
  Serial.printf("  Static IP     : %s\n",  cfg.ip);
  Serial.printf("  Subnet mask   : %s\n",  cfg.mask);
  Serial.printf("  Gateway       : %s\n",  cfg.gw);
  Serial.printf("  DNS           : %s\n",  cfg.dns);
  Serial.printf("  Hostname/FQDN : %s\n",  cfg.hostname);
  {
    char lbl[32]; strlcpy(lbl, cfg.hostname, sizeof(lbl));
    if (char* d = strchr(lbl, '.')) *d = 0;
    Serial.printf("  mDNS address  : %s.local\n", lbl);
  }
  // EAP / 802.1x
  const char* eapLabel = cfg.eapMode==1?"PEAP":cfg.eapMode==2?"EAP-TLS":"disabled";
  Serial.printf("  802.1x EAP    : %s\n", eapLabel);
  if (cfg.eapMode) {
    Serial.printf("  EAP Identity  : %s\n", strlen(cfg.eapIdentity) ? cfg.eapIdentity : "(not set)");
    if (cfg.eapMode == 1) {
      Serial.printf("  EAP Username  : %s\n", strlen(cfg.eapUsername) ? cfg.eapUsername : "(not set)");
      Serial.printf("  EAP Password  : %s\n", strlen(cfg.eapPassword) ? "********" : "(not set)");
    }
    Serial.printf("  CA cert path  : %s\n", strlen(cfg.eapCaPath)   ? cfg.eapCaPath   : "(none)");
    if (cfg.eapMode == 2) {
      Serial.printf("  Client cert   : %s\n", strlen(cfg.eapCertPath) ? cfg.eapCertPath : "(not set)");
      Serial.printf("  Client key    : %s\n", strlen(cfg.eapKeyPath)  ? cfg.eapKeyPath  : "(not set)");
      Serial.printf("  Key passphrase: %s\n", strlen(cfg.eapKeyPass) ? "set (********)" : "(none)");
    }
  }
  Serial.printf("  Default image : %s\n",  defaultMount.length() ? defaultMount.c_str() : "(none)");
  printSep();
  if (!showRuntime) return;
  Serial.println(F("  RUNTIME STATE"));
  printSep();
  Serial.printf("  SD card       : %s\n",  sdReady ? "OK" : "NOT FOUND");
  if (sdReady) {
    uint64_t totalMb = SD_MMC.totalBytes() / (1024*1024);
    uint64_t freeMb  = (SD_MMC.totalBytes()-SD_MMC.usedBytes()) / (1024*1024);
    Serial.printf("  SD size       : %llu MB total, %llu MB free\n", totalMb, freeMb);
  }
  Serial.printf("  WiFi          : %s\n",  wifiConnected ? "connected" : "not connected");
  if (wifiConnected) {
    Serial.printf("  IP address    : %s\n",  WiFi.localIP().toString().c_str());
    Serial.printf("  WiFi RSSI     : %d dBm\n", WiFi.RSSI());
    Serial.printf("  WiFi channel  : %d\n",  WiFi.channel());
    Serial.printf("  BSSID         : %s\n",  WiFi.BSSIDstr().c_str());
    Serial.printf("  Web interface : http://%s\n", WiFi.localIP().toString().c_str());
    // Show hostname/FQDN and mDNS
    char lbl[32]; strlcpy(lbl, cfg.hostname, sizeof(lbl));
    if (char* d = strchr(lbl, '.')) *d = 0;
    if (strchr(cfg.hostname, '.')) {
      Serial.printf("  FQDN (DHCP)   : %s\n", cfg.hostname);
      Serial.printf("  mDNS address  : http://%s.local\n", lbl);
    } else {
      Serial.printf("  Hostname/mDNS : http://%s.local\n", lbl);
    }
  }
  Serial.printf("  Mounted image : %s\n",  mountedFile.length() ? mountedFile.c_str() : "(none)");
  if (isoOpen) {
    Serial.printf("  Sectors       : %lu x 2048 B\n", mountedBlocks);
    Serial.printf("  Raw sector    : %u B  header offset: +%u B\n", binRawSectorSize, binHeaderOffset);
    Serial.printf("  Image size    : %.1f MB\n", (float)mountedBlocks * binRawSectorSize / (1024.0f*1024.0f));
  }
  Serial.printf("  Media present : %s\n",  mscMediaPresent ? "yes" : "no");
  printSep();
}

void printStatus() {
  printSep();
  Serial.println(F("  STATUS"));
  printSep();
  // WiFi
  if (wifiConnected) {
    int rssi = WiFi.RSSI();
    const char* quality = rssi > -50 ? "Excellent" : rssi > -65 ? "Good" : rssi > -75 ? "Fair" : "Weak";
    Serial.printf("  WiFi          : Connected  (%s, %d dBm)\n", quality, rssi);
    Serial.printf("  IP            : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  Mask          : %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("  Gateway       : %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("  DNS           : %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("  SSID          : %s\n", WiFi.SSID().c_str());
    Serial.printf("  Band/Channel  : %s / Ch %d\n", WiFi.channel()<=13?"2.4 GHz":"5 GHz", WiFi.channel());
    Serial.printf("  BSSID         : %s\n", WiFi.BSSIDstr().c_str());
    // Security
    Serial.println(F("  ── Security ──────────────────────────────"));
    const char* authStr = cfg.eapMode==1?"WPA2-Enterprise (PEAP)":cfg.eapMode==2?"WPA2-Enterprise (EAP-TLS)":"WPA2-Personal";
    Serial.printf("  Auth method   : %s\n", authStr);
    if (cfg.eapMode) {
      Serial.printf("  EAP Identity  : %s\n", strlen(cfg.eapIdentity) ? cfg.eapIdentity : "(auto from cert CN)");
      if (cfg.eapMode == 2) {
        Serial.printf("  Client cert   : %s\n", strlen(cfg.eapCertPath) ? cfg.eapCertPath : "(not set)");
        Serial.printf("  Client key    : %s\n", strlen(cfg.eapKeyPath)  ? cfg.eapKeyPath  : "(not set)");
        Serial.printf("  Key passphrase: %s\n", strlen(cfg.eapKeyPass)  ? "set" : "(none)");
        if (s_eapCertBuf) {
          // Show cert details using mbedTLS
          mbedtls_x509_crt crt; mbedtls_x509_crt_init(&crt);
          if (mbedtls_x509_crt_parse(&crt,(const unsigned char*)s_eapCertBuf,strlen(s_eapCertBuf)+1)==0) {
            char cnBuf[64]={};
            mbedtls_x509_name* nm=&crt.subject;
            while(nm){if(nm->oid.len==3&&memcmp(nm->oid.p,"\x55\x04\x03",3)==0){memcpy(cnBuf,nm->val.p,min((int)nm->val.len,63));break;}nm=nm->next;}
            Serial.printf("  Cert CN       : %s\n", strlen(cnBuf)?cnBuf:"(unknown)");
            char notBefore[20]={},notAfter[20]={};
            snprintf(notBefore,sizeof(notBefore),"%04d-%02d-%02d",
              crt.valid_from.year,crt.valid_from.mon,crt.valid_from.day);
            snprintf(notAfter,sizeof(notAfter),"%04d-%02d-%02d",
              crt.valid_to.year,crt.valid_to.mon,crt.valid_to.day);
            Serial.printf("  Cert validity : %s → %s\n", notBefore, notAfter);
            Serial.printf("  Cert bits     : %u bit\n", (unsigned)mbedtls_pk_get_bitlen(&crt.pk));
          }
          mbedtls_x509_crt_free(&crt);
        }
        Serial.printf("  CA cert       : %s\n", strlen(cfg.eapCaPath) ? cfg.eapCaPath : "(none — server not verified)");
        if (s_eapKeyBuf) {
          const char* kfmt = strstr(s_eapKeyBuf,"BEGIN RSA PRIVATE KEY")?"PKCS#1 RSA":
                             strstr(s_eapKeyBuf,"BEGIN EC PRIVATE KEY") ?"EC":
                             strstr(s_eapKeyBuf,"BEGIN PRIVATE KEY")    ?"PKCS#8":
                             strstr(s_eapKeyBuf,"BEGIN ENCRYPTED PRIVATE KEY")?"Encrypted":"Unknown";
          Serial.printf("  Key format    : %s\n", kfmt);
        }
      } else {
        Serial.printf("  PEAP username : %s\n", strlen(cfg.eapUsername) ? cfg.eapUsername : "(not set)");
        Serial.printf("  CA cert       : %s\n", strlen(cfg.eapCaPath) ? cfg.eapCaPath : "(none)");
      }
    }
    Serial.println(F("  ─────────────────────────────────────────"));
  } else {
    Serial.println(F("  WiFi          : Not connected"));
  }
  // SD
  Serial.printf("  SD card       : %s\n", sdReady ? "OK" : "NOT FOUND");
  if (sdReady) {
    uint64_t totalMb = SD_MMC.totalBytes() / (1024*1024);
    uint64_t usedMb  = SD_MMC.usedBytes()  / (1024*1024);
    uint64_t freeMb  = totalMb - usedMb;
    Serial.printf("  SD storage    : %llu MB used / %llu MB total (%llu MB free)\n",
                  usedMb, totalMb, freeMb);
  }
  // Image
  if (isoOpen) {
    Serial.printf("  Mounted image : %s\n", mountedFile.c_str());
    Serial.printf("  Sectors       : %lu x 2048 B  (%.1f MB)\n",
                  mountedBlocks, (float)mountedBlocks * 2048.0f / (1024.0f*1024.0f));
    Serial.printf("  Raw sector    : %u B  header: +%u B\n", binRawSectorSize, binHeaderOffset);
    Serial.printf("  USB media     : %s\n", mscMediaPresent ? "present" : "not present");
  } else {
    Serial.println(F("  Mounted image : (none)"));
  }
  // Default
  Serial.printf("  Default image : %s\n", defaultMount.length() ? defaultMount.c_str() : "(none)");
  // System
  Serial.println(F("  ------------------------------------------------"));
  Serial.println(F("  TRANSFER SPEED NOTE"));
  Serial.println(F("  USB Full Speed (ESP32-S3) = 12 Mbit/s max"));
  Serial.println(F("  Real copy speed: ~600-900 KB/s  (hardware limit)"));
  Serial.println(F("  SD card speed is NOT the bottleneck."));
  Serial.println(F("  ------------------------------------------------"));
  Serial.printf("  Free heap     : %lu B\n", ESP.getFreeHeap());
  Serial.printf("  Uptime        : %lu s\n", millis()/1000);
  Serial.printf("  CPU freq      : %u MHz\n", getCpuFrequencyMhz());
  Serial.printf("  Flash size    : %u MB\n", ESP.getFlashChipSize()/(1024*1024));
  Serial.printf("  SDK version   : %s\n", ESP.getSdkVersion());
  printSep();
}

void printHelp() {
  printSep();
  Serial.println(F("  COMMANDS"));
  printSep();
  Serial.println(F("  set ssid <value>      Set WiFi SSID"));
  Serial.println(F("  set pass <value>      Set WiFi password"));
  Serial.println(F("  set dhcp on|off       Enable/disable DHCP"));
  Serial.println(F("  set ip <addr>         Set static IP address"));
  Serial.println(F("  set mask <mask>       Set subnet mask"));
  Serial.println(F("  set gw <addr>         Set default gateway"));
  Serial.println(F("  set dns <addr>        Set DNS server"));
  Serial.println(F("  set hostname <n>   Set hostname or FQDN (e.g. espcd or espcd.falco81.net)"));
  Serial.println(F("  -- 802.1x Enterprise WiFi --"));
  Serial.println(F("  set eap-mode <0|1|2>  0=off 1=PEAP 2=EAP-TLS"));
  Serial.println(F("  set eap-id <val>      EAP outer identity"));
  Serial.println(F("  set eap-user <val>    EAP inner username (PEAP)"));
  Serial.println(F("  set eap-pass <val>    EAP inner password (PEAP)"));
  Serial.println(F("  set eap-ca <path>     SD path to CA cert e.g. /wifi/ca.pem"));
  Serial.println(F("  set eap-cert <path>   SD path to client cert (EAP-TLS)"));
  Serial.println(F("  set eap-key <path>    SD path to client key (EAP-TLS)"));
  Serial.println(F("  set eap-kpass <val>   Passphrase for encrypted private key"));
  Serial.println(F("  show config           Show full configuration + runtime state"));
  Serial.println(F("  show files [path]     List SD files recursively (default: /)"));
  Serial.println(F("  status                Show current status (WiFi, SD, image)"));
  Serial.println(F("  mount <file>          Mount ISO/BIN/CUE as CD-ROM"));
  Serial.println(F("  umount                Eject current CD-ROM"));
  Serial.println(F("  sd reinit             Reinitialize SD card"));
  Serial.println(F("  wifi reconnect        Disconnect and reconnect WiFi"));
  Serial.println(F("  clear config          Erase ALL NVS settings (factory reset)"));
  Serial.println(F("  reboot                Restart ESP32"));
  Serial.println(F("  help                  Show this help"));
  printSep();
}

// Recursive directory listing helper
static uint32_t listDirRecursive(File dir, int depth, uint32_t &totalFiles, uint64_t &totalBytes) {
  uint32_t count = 0;
  File f;
  while ((f = dir.openNextFile())) {
    // indent by depth
    for (int i = 0; i < depth; i++) Serial.print("  ");
    if (f.isDirectory()) {
      Serial.printf("[DIR]  %s/\n", f.name());
      listDirRecursive(f, depth + 1, totalFiles, totalBytes);
    } else {
      uint32_t sz = f.size();
      totalBytes += sz;
      totalFiles++;
      // Format size human-readable
      if (sz >= 1024*1024) {
        Serial.printf("%-38s %8.1f MB\n", f.name(), sz / (1024.0f*1024.0f));
      } else if (sz >= 1024) {
        Serial.printf("%-38s %8.1f KB\n", f.name(), sz / 1024.0f);
      } else {
        Serial.printf("%-38s %8lu B\n",  f.name(), sz);
      }
    }
    count++;
    f.close();
  }
  return count;
}

void listSDFiles(const char* path = "/") {
  if (!sdReady) { Serial.println(F("[ERR] SD not available.")); return; }
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[ERR] Cannot open directory: %s\n", path);
    if (dir) dir.close();
    return;
  }
  printSep();
  Serial.printf("  SD CARD - %s\n", path);
  printSep();
  uint32_t files = 0; uint64_t bytes = 0;
  uint32_t entries = listDirRecursive(dir, 0, files, bytes);
  dir.close();
  if (entries == 0) Serial.println(F("  (empty)"));
  printSep();
  if (files > 0) {
    if (bytes >= 1024ULL*1024*1024)
      Serial.printf("  %lu files  /  %.2f GB total\n", files, bytes/(1024.0*1024.0*1024.0));
    else if (bytes >= 1024*1024)
      Serial.printf("  %lu files  /  %.1f MB total\n", files, bytes/(1024.0*1024.0));
    else
      Serial.printf("  %lu files  /  %llu B total\n",  files, bytes);
    printSep();
  }
}

void reinitSD() {
  Serial.println(F("[SD]   Reinitializing..."));
  closeIso(); SD_MMC.end(); delay(100);
  sdReady = initSD();
}

void processCommand(String &line) {
  line.trim();
  if (!line.length()) return;
  if      (line.equalsIgnoreCase("help"))           printHelp();
  else if (line.equalsIgnoreCase("show config"))    printConfig();
  else if (line.length() >= 10 &&
           line.substring(0,10).equalsIgnoreCase("show files")) {
    String arg = line.substring(10); arg.trim();
    if (arg.length() == 0 || arg == "/") listSDFiles("/");
    else listSDFiles(arg.c_str());
  }
  else if (line.equalsIgnoreCase("status"))         printStatus();
  else if (line.equalsIgnoreCase("umount"))         doUmount();
  else if (line.equalsIgnoreCase("sd reinit"))      reinitSD();
  else if (line.equalsIgnoreCase("reboot"))         { delay(300); ESP.restart(); }
  else if (line.equalsIgnoreCase("clear config")) {
    clearConfig();
    Serial.println(F("[INFO] Reboot to apply factory defaults."));
  }
  else if (line.equalsIgnoreCase("wifi reconnect")) {
    WiFi.disconnect(true); delay(400); startWiFi();
    if (wifiConnected) setupWebServer();
  }
  else if (line.startsWith("mount ") || line.startsWith("MOUNT ")) {
    String f = line.substring(6); f.trim();
    if (f.length()) doMount(f);
  }
  else if (line.startsWith("set ") || line.startsWith("SET ")) {
    String rest = line.substring(4); rest.trim();
    int sp = rest.indexOf(' ');
    String key, val;
    if (sp < 0) {
      // No value — allowed for clearing optional fields (e.g. set eap-id)
      key = rest; key.trim(); key.toLowerCase();
      val = "";
    } else {
      key = rest.substring(0, sp); key.trim(); key.toLowerCase();
      val = rest.substring(sp+1); val.trim();
    }
    bool ok = true;
    if      (key=="ssid") strlcpy(cfg.ssid,     val.c_str(), sizeof(cfg.ssid));
    else if (key=="pass") strlcpy(cfg.password,  val.c_str(), sizeof(cfg.password));
    else if (key=="dhcp") { String v=val; v.toLowerCase(); cfg.dhcp=(v=="on"||v=="1"||v=="yes"); }
    else if (key=="ip")   strlcpy(cfg.ip,   val.c_str(), sizeof(cfg.ip));
    else if (key=="mask") strlcpy(cfg.mask, val.c_str(), sizeof(cfg.mask));
    else if (key=="gw")  strlcpy(cfg.gw,  val.c_str(), sizeof(cfg.gw));
    else if (key=="dns")      strlcpy(cfg.dns,      val.c_str(), sizeof(cfg.dns));
    else if (key=="hostname")  strlcpy(cfg.hostname,  val.c_str(), sizeof(cfg.hostname));
    else if (key=="eap-mode")  { cfg.eapMode = (uint8_t)val.toInt(); }
    else if (key=="eap-id")    strlcpy(cfg.eapIdentity, val.c_str(), sizeof(cfg.eapIdentity));
    else if (key=="eap-user")  strlcpy(cfg.eapUsername,  val.c_str(), sizeof(cfg.eapUsername));
    else if (key=="eap-pass")  strlcpy(cfg.eapPassword,  val.c_str(), sizeof(cfg.eapPassword));
    else if (key=="eap-ca")    strlcpy(cfg.eapCaPath,    val.c_str(), sizeof(cfg.eapCaPath));
    else if (key=="eap-cert")  strlcpy(cfg.eapCertPath,  val.c_str(), sizeof(cfg.eapCertPath));
    else if (key=="eap-key")   strlcpy(cfg.eapKeyPath,   val.c_str(), sizeof(cfg.eapKeyPath));
    else if (key=="eap-kpass") strlcpy(cfg.eapKeyPass,  val.c_str(), sizeof(cfg.eapKeyPass));
    else { Serial.printf("[ERR] Unknown key: '%s'\n", key.c_str()); ok=false; }
    if (ok) {
      saveConfig();
      Serial.printf("[OK]  %s = %s\n", key.c_str(), val.c_str());
      // Verify EAP fields are readable back from NVS
      if (key.startsWith("eap")) {
        Preferences vfy; vfy.begin("cfg", true);
        String nvsk = key=="eap-mode" ? "eapMode" :
                      key=="eap-id"   ? "eapIdent" :
                      key=="eap-user" ? "eapUser"  :
                      key=="eap-pass" ? "eapPass"  :
                      key=="eap-ca"   ? "eapCa"    :
                      key=="eap-cert" ? "eapCert"  :
                      key=="eap-key"  ? "eapKey"   : "";
        if (nvsk.length()) {
          String stored = vfy.getString(nvsk.c_str(), "");
          if (val.length() == 0) {
            // Empty value = field cleared (key removed from NVS) — OK
            Serial.printf("[NVS]  Verified: %s cleared\n", nvsk.c_str());
          } else if (stored.length() || key=="eap-mode") {
            Serial.printf("[NVS]  Verified: %s = '%s'\n", nvsk.c_str(), stored.c_str());
          } else {
            Serial.printf("[NVS]  WARNING: %s not saved correctly!\n", nvsk.c_str());
          }
        }
        vfy.end();
      }
    }
  }
  else Serial.printf("[ERR] Unknown command: '%s'  (type help)\n", line.c_str());
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(800);

  rgbLed.begin();
  rgbLed.setBrightness(0);
  rgbLed.setPixelColor(0, 0, 0, 0);
  rgbLed.show();

  Serial.println();
  printSep();
  Serial.println(F("  ESP32-S3 Virtual CD-ROM + File Manager  v3.1"));
  printSep();

  // Load config and print everything stored in NVS
  loadConfig();
  printConfig(false);  // NVS settings only (runtime not yet available)
  printHelp();

  initUSBMSC();
  sdReady = initSD();
  startWiFi();
  if (wifiConnected) setupWebServer();

  Serial.println(F("\n[READY] Type 'help' for available commands\n"));

  // Auto-mount default image, then connect USB.
  // If no default image, connect USB anyway (host will see empty drive).
  if (sdReady && defaultMount.length()) {
    Serial.printf("[AUTO] Default image: %s\n", defaultMount.c_str());
    File _f = SD_MMC.open(defaultMount.c_str());
    if (_f) { _f.close(); doMount(defaultMount); }
    else {
      Serial.println(F("[AUTO] File not found on SD card."));
      usbConnect();  // connect with no disc rather than staying invisible
    }
  } else {
    usbConnect();  // no default image configured — connect empty drive
  }
}

// =============================================================================
//  LOOP
// =============================================================================
String serialBuf = "";
void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { processCommand(serialBuf); serialBuf = ""; }
    else if (c == '\b' || c == 127) { if (serialBuf.length()) serialBuf.remove(serialBuf.length()-1); }
    else serialBuf += c;
  }
  if (wifiConnected) httpServer.handleClient();
  delay(1);
}
