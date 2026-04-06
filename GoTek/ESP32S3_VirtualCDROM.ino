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
#include <mbedtls/x509_crt.h>
#include <mbedtls/base64.h>  // for reading CN from client cert
#include <esp_log.h>             // for WPA debug logging
#include <WebServer.h>
#include <esp_https_server.h>   // ESP-IDF HTTPS server
#include <SPI.h>
#include "FS.h"
#include "SD_MMC.h"
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBMSC.h"
#include <driver/i2s_std.h>       // PCM5102 audio output
extern "C" { bool tud_disconnect(void); bool tud_connect(void); void msc_set_unit_attention(void); }

// Forward declarations for Arduino prototype generator
struct AudioTrack;

// ── Crash log in RTC memory ───────────────────────────────────────────────────
// Survives soft reboot (panic/watchdog). Displayed in HTML log after WiFi connects.
// Cleared on power-on reset.
RTC_DATA_ATTR static char rtcCrashMsg[128] = {0};
RTC_DATA_ATTR static uint32_t rtcCrashMagic = 0;
#define RTC_CRASH_MAGIC 0xDEADC0DE



// =============================================================================
//  PIN DEFINITIONS
// =============================================================================
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_SCK_PIN   12
#define SD_MISO_PIN  13
#define RGB_LED_PIN  48

// I2S pins for PCM5102 module
#define I2S_BCK_PIN    8    // Bit clock  (PCM5102 BCK)
#define I2S_WS_PIN    15    // Word select (PCM5102 LCK)
#define I2S_DATA_PIN  16    // Data out   (PCM5102 DIN)

// =============================================================================
//  AUDIO TRACK TABLE
//  Populated by parseCue() when a CUE sheet is mounted.
//  Audio sectors are raw 16-bit signed stereo LE at 44100 Hz, 2352 B/sector.
//  The first 16 bytes of each sector are the sync/address header (ignored).
// =============================================================================
#define MAX_AUDIO_TRACKS 99   // CD standard: max 99 tracks; Tomb Raider has 56 audio tracks

struct AudioTrack {
  uint8_t  number;          // Track number (1-based)
  uint32_t startLba;        // Virtual disc LBA (absolute, reported in TOC)
  uint32_t lengthSectors;   // Track length in sectors (usable audio, after pregap)
  uint32_t pregapSectors;   // Pregap offset within BIN file (separate-BIN layout only)
  uint32_t fileSectorBase;  // First sector to seek in the BIN file for this track:
                            //   single-BIN  → absolute BIN sector (= startLba before virtual remapping)
                            //   separate-BIN → pregap sectors within individual track file
  String   binFile;         // Full SD path to the .bin file for this track
  bool     valid;
};

// Global audio track table — filled by parseCue, cleared on umount
static AudioTrack audioTracks[MAX_AUDIO_TRACKS];
static uint8_t    audioTrackCount   = 0;
static volatile uint32_t   dataTrackEndLba   = 0;  // LBA where data track ends
static volatile uint32_t   dataTrackStartSect = 0; // Sector index in BIN file where data track begins (for mscReadCb offset)
static volatile uint32_t   dataTrackSectors_g = 0; // Usable data sector count (for cdromBlockCount)

// ── Playback state (accessed from SCSI callbacks and audio task) ──
// All writes from a single core; reads from SCSI callback (core 0) are
// safe because volatile + atomic uint32 reads are single-cycle on Xtensa.
enum AudioPlayState { AUDIO_STOP = 0, AUDIO_PLAY = 1, AUDIO_PAUSE = 2 };
volatile AudioPlayState audioState     = AUDIO_STOP;
volatile uint32_t       audioCurrentLba = 0;   // current playback position
volatile uint32_t       audioEndLba     = 0;   // stop when reaching this LBA
// Sequence counter: incremented by audioPlay() on every new play command.
// The audio task captures mySeq at the start of each batch; if audioPlaySeq
// changes mid-batch the task aborts the current write loop immediately and
// re-reads audioCurrentLba (which points to the new position).  This replaces
// the old vTaskDelay(5ms) hack which was far too short to interrupt a 49 ms SD read.
volatile uint32_t       audioPlaySeq    = 0;
static  TaskHandle_t      audioTaskHandle = nullptr;
static volatile i2s_chan_handle_t i2sTx   = nullptr;  // volatile: written from core 1, read from core 0
static volatile int             audioVolume = 80;      // volatile: written from core 1, read from core 0
// Win98 Stop/Pause detection via READ_SUB_CHANNEL polling absence
static volatile uint32_t subPollLastTime = 0;  // millis() of last READ_SUB_CHANNEL
static volatile bool     subPollActive   = false; // true after first poll since play started
// Win98 Stop/Pause timeout configured via cfg.win98StopMs (0 = disabled).
static volatile bool            audioMuted  = false;   // volatile: written from core 1, read from core 0
static volatile bool            audioCloseFile = false; // set by doUmount → task closes BIN handle

// ── Sub-channel data (updated by audio task, read by SCSI READ SUB-CHANNEL) ──
// MSF = Minutes:Seconds:Frames  (1 frame = 1 sector = 1/75 s)
struct SubChannel {
  uint8_t trackNum;
  uint8_t indexNum;
  uint8_t absM, absS, absF;   // absolute position on disc
  uint8_t relM, relS, relF;   // position within current track
};
static volatile SubChannel subChannel = {};

// ══════════════════════════════════════════════════════════════════
//  GLOBAL OBJECTS
// =============================================================================
Preferences        prefs;
WebServer          httpServer(80);  // port updated at setupWebServer()
httpd_handle_t     httpsHandle  = nullptr;  // ESP-IDF HTTPS server handle
static SemaphoreHandle_t s_proxyMutex  = nullptr; // serialise loopback
static SemaphoreHandle_t sdMutex        = nullptr; // serialise SD card access (audio task + SCSI DAE)
static WiFiClient        s_proxyClient;            // persistent loopback
Adafruit_NeoPixel  rgbLed(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// =============================================================================
//  GOTEK / FAT12 FORWARD DECLARATIONS
//  Must be before loadConfig/saveConfig/clearConfig which reference gotekImgDir.
// =============================================================================
// GoTek image directory on SD card (persisted in NVS key "gotekDir")
static String gotekImgDir = "/gotek";

// FAT12 BPB (BIOS Parameter Block) — parsed from .img file boot sector
struct ImgBPB {
  uint16_t bps;       // bytes per sector
  uint8_t  spc;       // sectors per cluster
  uint16_t rsv;       // reserved sectors (including boot sector)
  uint8_t  nfat;      // number of FAT copies
  uint16_t rde;       // root directory entry count
  uint16_t fatsz;     // sectors per FAT
  uint32_t fat1;      // LBA of FAT1
  uint32_t rootLBA;   // LBA of root directory
  uint32_t rootSec;   // sectors occupied by root directory
  uint32_t dataLBA;   // LBA of first data cluster (cluster 2)
  uint32_t totSec;    // total sectors on volume
  uint32_t dataClu;   // total data clusters
};

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
  bool audioModule;        // true = PCM5102 I2S module connected and enabled
  bool webAuth;            // true = HTTP Basic Auth enabled
  char webUser[32];        // Web UI username (default: admin)
  char webPass[64];        // Web UI password (default: admin)
  bool dosCompat;          // true = skip USB re-enum on mount (DOS compatibility)
  uint8_t dosDriver;       // 0=Generic 1=USBCD2/TEAC 2=ESPUSBCD/Panasonic 3=DI1000DD/USBASPI
  bool     httpsEnabled;       // true = enable HTTPS
  char     httpsCertPath[64];  // SD path to TLS server certificate
  char     httpsKeyPath[64];   // SD path to TLS server private key
  char     httpsKeyPass[64];   // Passphrase for encrypted key
  uint16_t httpPort;           // HTTP server port (default 80)
  uint16_t httpsPort;          // HTTPS server port (default 443)
  uint8_t  tlsMinVer;          // 0=TLS1.2  1=TLS1.2+1.3
  uint8_t  tlsCiphers;         // 0=Auto 1=Strong(GCM) 2=Medium(CBC) 3=All
  // WiFi TX power — units are 0.25 dBm (ESP-IDF convention).
  // Lower power = less RF interference into PCM5102 analog section.
  // 8=2dBm  20=5dBm  40=10dBm  60=15dBm  80=20dBm
  uint8_t  wifiTxPower;        // default 40 = 10 dBm
  bool     debugMode;          // true = verbose SCSI/API logs (default: off)
  uint16_t win98StopMs;        // Win98 Stop/Pause detect timeout ms (0=off, default:1200)
  // Device mode — determines which USB profile is presented at boot.
  // 0 = CD-ROM (default, all existing functionality)
  // 1 = GoTek floppy emulator (presents as USB MSC flash drive, GoTek reads .img files)
  // Changing this requires reboot to take effect (USB descriptor set at init time).
  uint8_t  deviceMode;
  // GoTek USB presentation mode:
  // 0 = Raw LBA — slot N at LBA N × GK_SLOT_STRIDE (for stock GoTek firmware)
  // 1 = FAT virtual — virtual FAT16 volume with DSKA0000.IMG... mapped to real files
  //                   (for FlashFloppy: use nav-mode=indexed in FF.CFG)
  uint8_t  gotekUsbMode;
  // Write-protect the FAT32 virtual volume (prevents Windows format/delete).
  // Default ON — GoTek hardware ignores write-protect for image writes.
  // Disable only if your GoTek firmware requires write access to the container.
  uint8_t  gotekFatWP;    // 1 = write-protected (default), 0 = writable
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
  cfg.audioModule = prefs.getBool("audioMod", false);
  cfg.webAuth = prefs.getBool("webAuth", false);
  strlcpy(cfg.webUser, prefs.getString("webUser", "admin").c_str(), sizeof(cfg.webUser));
  strlcpy(cfg.webPass, prefs.getString("webPass", "admin").c_str(), sizeof(cfg.webPass));
  cfg.dosCompat = prefs.getBool("dosCompat", false);
  cfg.dosDriver = (uint8_t)prefs.getUChar("dosDriver", 0);
  cfg.httpsEnabled = prefs.getBool("httpsEnabled", false);
  strlcpy(cfg.httpsCertPath, prefs.getString("httpsCert", "").c_str(), sizeof(cfg.httpsCertPath));
  strlcpy(cfg.httpsKeyPath,  prefs.getString("httpsKey",  "").c_str(), sizeof(cfg.httpsKeyPath));
  strlcpy(cfg.httpsKeyPass,  prefs.getString("httpsKPass","").c_str(), sizeof(cfg.httpsKeyPass));
  cfg.httpPort  = (uint16_t)prefs.getUInt("httpPort",  80);
  cfg.httpsPort = (uint16_t)prefs.getUInt("httpsPort", 443);
  cfg.tlsMinVer = (uint8_t) prefs.getUInt("tlsMinVer", 0);
  cfg.tlsCiphers= (uint8_t) prefs.getUInt("tlsCiphers",0);
  cfg.wifiTxPower = (uint8_t)constrain((int)prefs.getUChar("wifiTxPow", 40), 8, 80);
  cfg.debugMode   = prefs.getBool("debugMode", false);
  cfg.win98StopMs = (uint16_t)constrain((int)prefs.getUShort("win98StopMs", 1200), 0, 9999);
  cfg.httpsEnabled = prefs.getBool("httpsEnabled", false);
  strlcpy(cfg.httpsCertPath, prefs.getString("httpsCert", "").c_str(), sizeof(cfg.httpsCertPath));
  strlcpy(cfg.httpsKeyPath,  prefs.getString("httpsKey",  "").c_str(), sizeof(cfg.httpsKeyPath));
  cfg.deviceMode    = (uint8_t)constrain((int)prefs.getUChar("deviceMode", 0), 0, 1);
  cfg.gotekUsbMode  = (uint8_t)constrain((int)prefs.getUChar("gotekUsbMode", 0), 0, 1);
  cfg.gotekFatWP    = (uint8_t)constrain((int)prefs.getUChar("gotekFatWP",   1), 0, 1);
  gotekImgDir     = prefs.getString("gotekDir", "/gotek");
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
  prefs.putBool("audioMod", cfg.audioModule);
  prefs.putBool("webAuth", cfg.webAuth);
  prefs.putString("webUser", cfg.webUser);
  prefs.putString("webPass", cfg.webPass);
  prefs.putBool("dosCompat", cfg.dosCompat);
  prefs.putUChar("dosDriver", cfg.dosDriver);
  prefs.putBool("httpsEnabled", cfg.httpsEnabled);
  prefs.putString("httpsCert", cfg.httpsCertPath);
  prefs.putString("httpsKey",  cfg.httpsKeyPath);
  prefs.putString("httpsKPass",cfg.httpsKeyPass);
  prefs.putUInt("httpPort",   cfg.httpPort  ? cfg.httpPort  : 80);
  prefs.putUInt("httpsPort",  cfg.httpsPort ? cfg.httpsPort : 443);
  prefs.putUInt("tlsMinVer",  cfg.tlsMinVer);
  prefs.putUInt("tlsCiphers", cfg.tlsCiphers);
  prefs.putUChar("wifiTxPow", cfg.wifiTxPower);
  prefs.putBool("debugMode",   cfg.debugMode);
  prefs.putUShort("win98StopMs", cfg.win98StopMs);
  prefs.putUChar("deviceMode",   cfg.deviceMode);
  prefs.putUChar("gotekUsbMode", cfg.gotekUsbMode);
  prefs.putUChar("gotekFatWP",   cfg.gotekFatWP);
  prefs.putString("gotekDir", gotekImgDir);
  prefs.putBool("httpsEnabled", cfg.httpsEnabled);
  prefs.putString("httpsCert", cfg.httpsCertPath);
  prefs.putString("httpsKey",  cfg.httpsKeyPath);
  prefs.end();
}

// ── Web log ring buffer ──────────────────────────────────────────────────────
// Captures Serial output so the HTML Log tab can display it live
#define WEB_LOG_SIZE   8192   // ring buffer size in bytes
#define WEB_LOG_LINES  200    // max lines kept
static char     webLogBuf[WEB_LOG_SIZE];
static uint32_t webLogHead = 0;   // write position
static uint32_t webLogSeq  = 0;   // monotonic sequence number (increments per line)

// Call this instead of (or in addition to) Serial.println/printf
void webLog(const char* msg) {
  Serial.println(msg);
  // Append to ring buffer with timestamp
  char tmp[256];
  uint32_t ms = millis();
  snprintf(tmp, sizeof(tmp), "[%lu.%03lu] %s\n", ms/1000, ms%1000, msg);
  size_t len = strlen(tmp);
  for (size_t i = 0; i < len; i++) {
    webLogBuf[webLogHead % WEB_LOG_SIZE] = tmp[i];
    webLogHead++;
  }
  webLogSeq++;
}

// Append formatted string to web log (mirrors Serial.printf)
void webLogf(const char* fmt, ...) {
  char tmp[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, args);
  va_end(args);
  // strip trailing newline if present (webLog adds one)
  size_t l = strlen(tmp);
  if (l > 0 && tmp[l-1] == '\n') tmp[l-1] = '\0';
  webLog(tmp);
}

// Custom Print class that mirrors output to both Serial and web log buffer
class DualPrint : public Print {
public:
  // Buffer incoming chars until newline then flush to webLog
  size_t write(uint8_t c) override {
    Serial.write(c);
    if (c == '\n') {
      _line[_pos] = '\0';
      // Append line to ring buffer with timestamp
      char tmp[280];
      uint32_t ms = millis();
      snprintf(tmp, sizeof(tmp), "[%lu.%03lu] %s\n", ms/1000, ms%1000, _line);
      size_t len = strlen(tmp);
      for (size_t i = 0; i < len; i++) {
        webLogBuf[webLogHead % WEB_LOG_SIZE] = tmp[i];
        webLogHead++;
      }
      webLogSeq++;
      _pos = 0;
    } else {
      if (_pos < (int)sizeof(_line)-1) _line[_pos++] = (char)c;
    }
    return 1;
  }
  size_t write(const uint8_t *buf, size_t size) override {
    for (size_t i = 0; i < size; i++) write(buf[i]);
    return size;
  }
private:
  char _line[256] = {};
  int  _pos = 0;
};
DualPrint dualPrint; 
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
  cfg.audioModule = false;
  cfg.webAuth = false;
  strlcpy(cfg.webUser, "admin", sizeof(cfg.webUser));
  strlcpy(cfg.webPass, "admin", sizeof(cfg.webPass));
  cfg.dosCompat = false;
  cfg.dosDriver = 0;
  cfg.httpsEnabled = false;
  cfg.httpsCertPath[0] = 0;
  cfg.httpsKeyPath[0]  = 0;
  cfg.httpsKeyPass[0]  = 0;
  cfg.httpPort   = 80;
  cfg.httpsPort  = 443;
  cfg.tlsMinVer  = 0;
  cfg.tlsCiphers = 0;
  cfg.wifiTxPower = 40;  // 10 dBm default — reduces RF→PCM5102 interference
  cfg.debugMode   = false;
  cfg.win98StopMs = 1200;  // default: 1.2s
  cfg.deviceMode  = 0;     // default: CD-ROM
  gotekImgDir     = "/gotek";
  defaultMount = "";
  dualPrint.println(F("[OK]  All NVS configuration cleared."));
}

// =============================================================================
//  SD CARD
// =============================================================================
bool      sdReady        = false;
String    mountedFile    = "";
uint32_t  mountedBlocks  = 0;
uint16_t  mountedBlkSize = 2048;
// isoOpen and mscMediaPresent guard mscReadCb (Core 0) from accessing isoFileHandle
// while doMount/fullReset (Core 1) is swapping files.  MUST be volatile so Core 0
// always reads the current value written by Core 1, not a stale register copy.
File               isoFileHandle;
volatile bool      isoOpen        = false;

USBMSC msc;  // registers MSC USB interface descriptor

// MSC state - accessed from TinyUSB callbacks (Core 0) and doMount (Core 1)
volatile bool     mscMediaPresent = false;
uint32_t mscBlockCount   = 0;
uint16_t mscBlockSize    = 2048;

// Block count shared with USBMSC.cpp patch (plain C++ — both TUs are C++)
uint32_t cdromBlockCount = 0;  // data track sectors (READ CAPACITY, I/O)

 // use this instead of Serial for log messages

uint32_t discLeadOutLba  = 0;  // lead-out LBA for READ TOC (includes audio tracks)
volatile bool unitAttentionPending = false;  // set on disc swap, cleared by TEST UNIT READY

bool initSD() {
  dualPrint.println(F("[SD]   Initializing SD_MMC (SDMMC hardware, 1-bit mode)"));
  dualPrint.printf("[SD]   Pins: CLK=%d CMD=%d D0=%d D3(CS)=%d\n",
                SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_CS_PIN);
#ifdef CONFIG_FATFS_EXFAT_SUPPORT
  dualPrint.println(F("[SD]   exFAT support: YES (compiled in)"));
#else
  dualPrint.println(F("[SD]   exFAT support: NO - recompile with CONFIG_FATFS_EXFAT_SUPPORT=y needed"));
#endif
  SD_MMC.setPins(SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN);
  pinMode(SD_CS_PIN, INPUT_PULLUP);
  if (SD_MMC.begin("/sdcard", true, false, 40000000, 20)) {  // 40 MHz, maxOpenFiles=20
    uint8_t t = SD_MMC.cardType();
    const char* tn = t==CARD_MMC?"MMC":t==CARD_SD?"SD":t==CARD_SDHC?"SDHC/SDXC":"UNKNOWN";
    uint64_t mb = SD_MMC.cardSize() / (1024*1024);
    uint64_t freeMb = (SD_MMC.totalBytes()-SD_MMC.usedBytes()) / (1024*1024);
    uint64_t totalMb = SD_MMC.totalBytes() / (1024*1024);
    dualPrint.printf("[SD]   OK  Type: %-10s  Size: %llu MB  CLK: 40 MHz\n", tn, mb);
    dualPrint.printf("[SD]   Used: %llu MB  Free: %llu MB  Total: %llu MB\n",
                  totalMb - freeMb, freeMb, totalMb);
    return true;
  }
  dualPrint.println(F("[ERR] SD_MMC.begin() failed!"));
  dualPrint.println(F("[ERR] Check wiring: D5->GPIO12 D7->GPIO11 D6->GPIO13 D4->GPIO10"));
  dualPrint.println(F("[ERR] VCC must be 3V3, not 5V! Card inserted?"));
  return false;
}

void closeIso() {
  if (isoOpen) {
    isoOpen = false;          // disable reads on core 0 FIRST
    delay(15);                // wait for any in-flight mscReadCb sector loop to see isoOpen=false and exit
    isoFileHandle.close();
    isoFileHandle = File();   // explicit null — prevents stale internal FatFS pointer
  }
}

// =============================================================================
//  CUE PARSER
//  Raw sector layout:
//    MODE1/2048 : [DATA 2048]                                   offset=0
//    MODE1/2352 : [sync 12][addr 3][mode 1][DATA 2048][ECC 288] offset=16
//    MODE2/2352 : [sync 12][addr 3][mode 1][subhdr 8][DATA 2048][ECC 280] offset=24
//    MODE2/2336 : [subhdr 8][DATA 2048][ECC 280]                offset=8
// =============================================================================
// Sector geometry — written by Core 1 (doMount/fullReset), read by Core 0 (mscReadCb/cacheFill).
// MUST be volatile: without it, Core 0 may read stale register/cache values after Core 1 updates them,
// causing cacheFillWindow_locked to use old binRawSectorSize (e.g. 2352 from a previous CUE mount)
// and take the raw-sector path for a cooked ISO, producing garbage data (wrong seek position + wrong
// byte extraction).  This only manifests after audio playback because Core 0 hot-caches the geometry
// during intensive audio-era reads; on a first/fresh mount Core 0 has no stale cached value.
static volatile uint8_t  binHeaderOffset  = 16;
static volatile uint16_t binRawSectorSize = 2352;
static volatile uint16_t binUserDataSize  = 2048;

// Convert MSF string "MM:SS:FF" to LBA (sectors from start of disc)
static uint32_t msfToLba(const String &msf) {
  int m = msf.substring(0, 2).toInt();
  int s = msf.substring(3, 5).toInt();
  int f = msf.substring(6, 8).toInt();
  return (uint32_t)(m * 60 * 75 + s * 75 + f);
}

// Convert absolute LBA to MSF components
static void lbaToMsf(uint32_t lba, uint8_t &m, uint8_t &s, uint8_t &f) {
  f = lba % 75; lba /= 75;
  s = lba % 60; lba /= 60;
  m = (uint8_t)lba;
}

String parseCue(const String &cuePath) {
  File cue = SD_MMC.open(cuePath.c_str(), FILE_READ);
  if (!cue) { dualPrint.printf("[CUE] Cannot open: %s\n", cuePath.c_str()); return ""; }

  String dir = cuePath;
  int slash = dir.lastIndexOf('/');
  dir = (slash >= 0) ? dir.substring(0, slash) : "/";

  // Clear audio track table and all disc-level state
  audioTrackCount   = 0;
  dataTrackEndLba   = 0;
  dataTrackStartSect = 0;
  dataTrackSectors_g = 0;
  discLeadOutLba    = 0;  // reset — must not carry stale value from previous image
  for (int i = 0; i < MAX_AUDIO_TRACKS; i++) {
    audioTracks[i].valid = false;
    audioTracks[i].pregapSectors = 0;
    audioTracks[i].fileSectorBase = 0;
    audioTracks[i].startLba = 0;
    audioTracks[i].lengthSectors = 0;
  }

  String dataTrackBin = "";
  String currentFile  = "";
  bool   foundData    = false;
  binHeaderOffset = 16;

  // Track parsing state
  int     currentTrackNum  = 0;
  bool    currentIsAudio   = false;
  bool    currentIsData    = false;
  String  pendingIndex01   = "";
  uint32_t currentStartLba = 0;
  uint32_t dataTrackLba    = 0;
  uint32_t dataTrackSectors = 0;

  // We need two passes: first to collect all INDEX 01 positions,
  // then to compute lengths. Store intermediate info:
  //   pregapPhysical: true  = pregap is stored in BIN file (came from INDEX 00)
  //                   false = pregap is virtual/silent (came from PREGAP directive)
  struct RawTrack {
    int num; bool audio; String file; uint32_t startLba;
    uint32_t pregapLba;      // size of pregap in sectors (0 if none)
    bool     pregapPhysical; // true=in BIN file (INDEX 00), false=virtual (PREGAP directive)
  };
  static RawTrack raw[MAX_AUDIO_TRACKS + 2];
  int rawCount = 0;

  // Per-track parsing temporaries reset on each TRACK line
  uint32_t currentIndex00Lba  = 0;
  bool     currentHasIndex00  = false;
  uint32_t currentPregapSects = 0;   // from PREGAP directive
  bool     currentHasPregap   = false;

  while (cue.available()) {
    String line = cue.readStringUntil('\n');
    line.trim();
    String up = line; up.toUpperCase();

    if (up.startsWith("FILE")) {
      int q1 = line.indexOf('"'), q2 = line.lastIndexOf('"');
      if (q1 >= 0 && q2 > q1) currentFile = line.substring(q1+1, q2);
    }
    else if (up.startsWith("TRACK") && currentFile.length()) {
      // Reset per-track pregap state on each new TRACK
      currentIndex00Lba  = 0;
      currentHasIndex00  = false;
      currentPregapSects = 0;
      currentHasPregap   = false;

      currentTrackNum = line.substring(6, 8).toInt();
      currentIsAudio  = (up.indexOf("AUDIO") >= 0);
      currentIsData   = (up.indexOf("MODE") >= 0);
      if (currentIsData && !foundData) {
        dataTrackBin = (dir == "/") ? "/" + currentFile : dir + "/" + currentFile;
        if      (up.indexOf("MODE1/2352") >= 0) { binRawSectorSize=2352; binHeaderOffset=16; binUserDataSize=2048; dualPrint.println(F("[CUE] MODE1/2352 -> offset=16")); }
        else if (up.indexOf("MODE2/2352") >= 0) { binRawSectorSize=2352; binHeaderOffset=24; binUserDataSize=2048; dualPrint.println(F("[CUE] MODE2/2352 -> offset=24")); }
        else if (up.indexOf("MODE2/2336") >= 0) { binRawSectorSize=2336; binHeaderOffset=8;  binUserDataSize=2048; dualPrint.println(F("[CUE] MODE2/2336 -> offset=8")); }
        else if (up.indexOf("MODE1/2048") >= 0) { binRawSectorSize=2048; binHeaderOffset=0;  binUserDataSize=2048; dualPrint.println(F("[CUE] MODE1/2048 -> offset=0")); }
        else if (up.indexOf("MODE2/2048") >= 0) { binRawSectorSize=2048; binHeaderOffset=0;  binUserDataSize=2048; dualPrint.println(F("[CUE] MODE2/2048 -> offset=0")); }
        else if (up.indexOf("CDG")       >= 0) { binRawSectorSize=2448; binHeaderOffset=0;  binUserDataSize=2352; dualPrint.println(F("[CUE] CDG/2448 -> raw passthrough")); }
        else { binRawSectorSize=2352; binHeaderOffset=16; binUserDataSize=2048; dualPrint.println(F("[CUE] Unknown mode -> default MODE1/2352")); }
        foundData = true;
        dualPrint.printf("[CUE] Data track %d: %s\n", currentTrackNum, dataTrackBin.c_str());
      }
    }
    else if (up.startsWith("PREGAP") && currentTrackNum > 0) {
      // PREGAP directive = silent/virtual pregap NOT stored in BIN file
      String msfStr = line.substring(7); msfStr.trim();
      currentPregapSects = msfToLba(msfStr);
      currentHasPregap   = true;
      dualPrint.printf("[CUE] Track %02d: PREGAP directive %lu sectors (virtual)\n",
                    currentTrackNum, currentPregapSects);
    }
    else if (up.startsWith("INDEX 00") && currentTrackNum > 0) {
      // INDEX 00 = physical pregap stored in BIN file
      String msfStr = line.substring(9); msfStr.trim();
      currentIndex00Lba = msfToLba(msfStr);
      currentHasIndex00 = true;
    }
    else if (up.startsWith("INDEX 01") && currentTrackNum > 0 && rawCount < MAX_AUDIO_TRACKS + 1) {
      String msfStr = line.substring(9); msfStr.trim();
      uint32_t lba = msfToLba(msfStr);
      String fp = (dir == "/") ? "/" + currentFile : dir + "/" + currentFile;

      // Compute pregap size and type:
      //   INDEX 00 present → physical pregap (in file), size = INDEX01 - INDEX00
      //   PREGAP directive → virtual pregap (not in file), size from directive
      //   Neither          → no pregap
      uint32_t pgSects = 0;
      bool     pgPhys  = false;
      if (currentHasIndex00) {
        pgSects = (lba > currentIndex00Lba) ? (lba - currentIndex00Lba) : 0;
        pgPhys  = true;
      } else if (currentHasPregap) {
        pgSects = currentPregapSects;
        pgPhys  = false;
      }

      raw[rawCount++] = { currentTrackNum, currentIsAudio, fp, lba, pgSects, pgPhys };
    }
  }
  cue.close();

  // Detect CUE layout type:
  // Type A — single BIN file, all tracks share it, INDEX 01 = absolute disc LBA
  // Type B — separate BIN per track, INDEX 01 is relative to that file (always ~LBA 150)
  //          → length must come from individual file sizes, not LBA differences
  bool separateFiles = false;
  if (rawCount >= 2) {
    // If two consecutive tracks have the same or near-equal startLba, they are separate files
    for (int i = 0; i < rawCount - 1; i++) {
      if (raw[i].startLba == raw[i+1].startLba || raw[i].file != raw[i+1].file) {
        // Different files — Type B
        if (raw[i].file != raw[i+1].file) { separateFiles = true; break; }
      }
    }
  }

  for (int i = 0; i < rawCount; i++) {
    if (raw[i].audio) {
      if (audioTrackCount < MAX_AUDIO_TRACKS) {
        AudioTrack &at = audioTracks[audioTrackCount++];
        at.number   = (uint8_t)raw[i].num;
        at.binFile  = raw[i].file;
        at.valid    = true;

        if (separateFiles) {
          // Each track in its own file.
          // Physical pregap (INDEX 00): stored in BIN — fileSectorBase = INDEX01 pos in file, strip from length
          // Virtual pregap (PREGAP directive): not in BIN — fileSectorBase = 0, not subtracted from file size
          if (raw[i].pregapPhysical && raw[i].pregapLba > 0) {
            // INDEX 00 present: pregap physically in file; INDEX 01 LBA = offset into file
            uint32_t pregap = raw[i].pregapLba; // sectors before audio data
            File af = SD_MMC.open(at.binFile.c_str());
            if (af) {
              uint64_t sz = af.size(); af.close();
              uint32_t totalSectors = (uint32_t)(sz / 2352);
              at.pregapSectors  = pregap;
              at.fileSectorBase = raw[i].startLba; // INDEX 01 position in file = seek offset
              at.lengthSectors  = (totalSectors > raw[i].startLba) ? (totalSectors - raw[i].startLba) : 0;
            } else {
              at.pregapSectors  = pregap;
              at.fileSectorBase = raw[i].startLba;
              at.lengthSectors  = 0;
            }
            dualPrint.printf("[CUE] Track %02d: physical pregap %lu sects (INDEX 00)\n",
                          at.number, pregap);
          } else {
            // PREGAP directive or no pregap: audio data starts at byte 0 of BIN file
            File af = SD_MMC.open(at.binFile.c_str());
            if (af) {
              uint64_t sz = af.size(); af.close();
              uint32_t totalSectors = (uint32_t)(sz / 2352);
              at.pregapSectors  = raw[i].pregapLba;  // virtual pregap (not in file)
              at.fileSectorBase = 0;                 // file starts with audio data directly
              at.lengthSectors  = totalSectors;
            } else {
              at.pregapSectors  = raw[i].pregapLba;
              at.fileSectorBase = 0;
              at.lengthSectors  = 0;
            }
            if (raw[i].pregapLba > 0)
              dualPrint.printf("[CUE] Track %02d: virtual pregap %lu sects (PREGAP directive)\n",
                            at.number, raw[i].pregapLba);
          }
          // virtualStart assigned after all tracks parsed (needs dataTrackSectors)
        } else {
          // Single BIN — INDEX 01 is absolute BIN LBA.
          // startLba stays as BIN-absolute so PLAY AUDIO commands map directly to file seeks.
          // fileSectorBase = startLba so audio task can compute: fileSector = fileSectorBase + (lba - startLba)
          at.startLba      = raw[i].startLba;
          at.fileSectorBase = raw[i].startLba;  // absolute BIN sector = startLba for single-BIN
          uint32_t nextLba = (i + 1 < rawCount) ? raw[i+1].startLba : 0xFFFFFFFF;
          at.lengthSectors = (nextLba != 0xFFFFFFFF) ? (nextLba - at.startLba) : 0;
        }

        // startLba will be updated to virtual disc LBA after all tracks parsed
        dualPrint.printf("[CUE] Audio track %02d: file len %lu sectors  (%s)\n",
                      at.number, at.lengthSectors, at.binFile.c_str());
      }
    } else {
      dataTrackLba = raw[i].startLba;
      if (!separateFiles) {
        // Single-BIN: entire BIN file is presented as-is.
        // DOS uses Track01.startLba from TOC to find data — no mscReadCb offset needed.
        dataTrackStartSect = 0;
        uint32_t nextLba = (i + 1 < rawCount) ? raw[i+1].startLba : 0xFFFFFFFF;
        if (nextLba != 0xFFFFFFFF) {
          // Next track's INDEX 01 LBA marks end of data track
          dataTrackSectors = nextLba - raw[i].startLba;
          dataTrackEndLba  = raw[i].startLba + dataTrackSectors;
          dataTrackSectors_g = dataTrackSectors;
        } else {
          // Data track is the last (or only) track — derive length from file size
          File dtf2 = SD_MMC.open(raw[i].file.c_str());
          if (dtf2) {
            uint64_t sz2 = dtf2.size(); dtf2.close();
            uint32_t totalSect2 = (uint32_t)(sz2 / binRawSectorSize);
            dataTrackSectors = (totalSect2 > dataTrackLba) ? (totalSect2 - dataTrackLba) : totalSect2;
            dataTrackEndLba  = dataTrackLba + dataTrackSectors;
            dataTrackSectors_g = dataTrackSectors;
            dualPrint.printf("[CUE] Data track sectors (last track, from file): %lu\n", dataTrackSectors);
          }
        }
      } else {
        // Separate BIN files: data track length from Track 01 file size, minus any pregap.
        // dataTrackStartSect: offset within the data BIN file where actual data begins
        // (= INDEX 01 position; strips any pregap so host LBA 0 = first real data sector).
        dataTrackStartSect = dataTrackLba;
        // Use binRawSectorSize (not hardcoded 2352) to support all sector formats
        File dtf = SD_MMC.open(raw[i].file.c_str());
        if (dtf) {
          uint64_t sz = dtf.size(); dtf.close();
          uint32_t totalSectors = (uint32_t)(sz / binRawSectorSize);
          // Subtract pregap (= dataTrackLba offset within file) to get usable data sectors
          dataTrackSectors = (totalSectors > dataTrackLba) ? (totalSectors - dataTrackLba) : totalSectors;
          dataTrackEndLba  = dataTrackLba + dataTrackSectors;
          dataTrackSectors_g = dataTrackSectors;
          dualPrint.printf("[CUE] Data track sectors from file: %lu (total=%lu pregap=%lu)\n",
                        dataTrackSectors, totalSectors, dataTrackLba);
        }
      }
    }
  }

  // Last track in single-BIN: length from file size
  // Use binRawSectorSize — correct for all modes (MODE2/2336 = 2336, MODE1/2048 = 2048, etc.)
  // Audio tracks are always 2352 B/sector, but in a single-BIN all sectors share binRawSectorSize.
  if (!separateFiles && audioTrackCount > 0) {
    AudioTrack &last = audioTracks[audioTrackCount - 1];
    if (last.lengthSectors == 0) {
      File af = SD_MMC.open(last.binFile.c_str());
      if (af) {
        uint64_t sz = af.size(); af.close();
        uint32_t totalSects = (uint32_t)(sz / binRawSectorSize);
        last.lengthSectors = (totalSects > last.startLba) ? (totalSects - last.startLba) : 0;
      }
    }
  }

  // For separate BIN layout: assign virtual disc LBAs
  // data track occupies [0 .. dataTrackSectors-1]
  // audio tracks follow sequentially. For each track:
  //   Physical pregap (INDEX 00): already stripped — fileSectorBase skips it, not in virtual LBA space
  //   Virtual pregap (PREGAP directive): must be added to virtual LBA space so TOC gap is correct,
  //     but NOT seeked in the BIN file (fileSectorBase = 0, audio data starts at byte 0 of file)
  if (separateFiles && audioTrackCount > 0) {
    uint32_t virtualCursor = (dataTrackSectors > 0) ? dataTrackSectors : mountedBlocks;
    for (int i = 0; i < audioTrackCount; i++) {
      // Virtual pregap (PREGAP directive, fileSectorBase==0, pregapSectors>0):
      // reserve LBA space so the gap appears in TOC, but it's silence not in the BIN file.
      // Physical pregap (INDEX 00, fileSectorBase>0): already stripped, not in virtual LBA space.
      if (audioTracks[i].fileSectorBase == 0 && audioTracks[i].pregapSectors > 0) {
        virtualCursor += audioTracks[i].pregapSectors;
      }
      audioTracks[i].startLba = virtualCursor;
      virtualCursor += audioTracks[i].lengthSectors;
    }
    // Lead-out LBA = virtualCursor (used by READ TOC 0xAA entry)
    discLeadOutLba = virtualCursor;
    dualPrint.printf("[CUE] Virtual disc LBAs assigned, lead-out LBA=%lu\n", virtualCursor);
  } else if (!separateFiles && audioTrackCount > 0) {
    // Single-BIN: lead-out LBA = last audio track end (in BIN-absolute LBA space)
    // This gives the correct TOC lead-out to the host without waiting for doMount()
    // to fall back to mountedBlocks (which requires fileSize, not yet known here).
    AudioTrack &last = audioTracks[audioTrackCount - 1];
    if (last.startLba > 0 && last.lengthSectors > 0) {
      discLeadOutLba = last.startLba + last.lengthSectors;
      dualPrint.printf("[CUE] Single-BIN lead-out LBA=%lu (from last audio track)\n", discLeadOutLba);
    }
    // If lengthSectors still 0 (last track length computed later from file size),
    // discLeadOutLba remains 0 and doMount() will set it from mountedBlocks — acceptable fallback.
  }

  // Log final track table with correct virtual LBAs
  for (int i = 0; i < audioTrackCount; i++) {
    dualPrint.printf("[CUE] Track %02d: LBA %6lu  len %5lu  (~%lus)\n",
                  audioTracks[i].number,
                  audioTracks[i].startLba,
                  audioTracks[i].lengthSectors,
                  audioTracks[i].lengthSectors / 75);
  }
  if (audioTrackCount > 0)
    dualPrint.printf("[CUE] %d audio track(s) found  (layout: %s)\n",
                  audioTrackCount, separateFiles ? "separate BIN files" : "single BIN");

  if (!foundData) dualPrint.println(F("[CUE] No data track found!"));
  return dataTrackBin;
}

// =============================================================================
//  FULL DISC STATE RESET
//  Forward declarations for functions defined later in file
void audioStop();
bool checkAuth();
static void urlDecode(char* dst, const char* src, size_t dstSize);
//  Brings the device to an identical state as a fresh boot — minus USB
//  connection, Wi-Fi and NVS settings.  Called at the start of every
//  doMount() and doUmount() so every mount/eject begins from a clean slate.
//
//  Order matters:
//    1. mscMediaPresent=false / isoOpen=false  — stop Core 0 reads first
//    2. cacheInvalidate()                      — while no reads in flight
//    3. audioStop() + wait for file close      — audio task owns its File
//    4. delay(15) + closeIso()                 — let in-flight mscReadCb exit
//    5. zero ALL disc geometry and track state — deterministic fresh slate
// =============================================================================
static void fullReset() {
  // ── 1. Block all reads immediately ───────────────────────────────────────
  mscMediaPresent = false;
  isoOpen         = false;   // belt-and-suspenders: mscReadCb checks both

  // ── 2. Flush PSRAM cache ──────────────────────────────────────────────────
  cacheInvalidate();

  // ── 3. Stop audio and close its BIN file handle ───────────────────────────
  audioStop();
  audioCloseFile = true;
  for (int _w = 0; _w < 60 && audioCloseFile; _w++) delay(10);
  if (audioCloseFile) {
    dualPrint.println(F("[WARN] fullReset: audioCloseFile wait timed out — forcing clear"));
    audioCloseFile = false;
  }

  // ── 4. Close ISO file handle ──────────────────────────────────────────────
  delay(15);   // let any in-flight mscReadCb sector loop see isoOpen=false
  if (isoFileHandle) { isoFileHandle.close(); isoFileHandle = File(); }

  // ── 5. Reset disc geometry ────────────────────────────────────────────────
  mountedFile        = "";
  mountedBlocks      = 0;
  mountedBlkSize     = 2048;
  mscBlockCount      = 0;
  mscBlockSize       = 2048;
  binRawSectorSize   = 2048;
  binHeaderOffset    = 0;
  binUserDataSize    = 2048;
  cdromBlockCount    = 0;
  discLeadOutLba     = 0;
  dataTrackStartSect = 0;
  dataTrackSectors_g = 0;
  dataTrackEndLba    = 0;

  // ── 6. Reset audio track table ────────────────────────────────────────────
  audioTrackCount = 0;
  audioCurrentLba = 0;
  audioEndLba     = 0;
  audioPlaySeq++;   // invalidate any in-flight audio batch
  for (int i = 0; i < MAX_AUDIO_TRACKS; i++) {
    audioTracks[i].valid          = false;
    audioTracks[i].pregapSectors  = 0;
    audioTracks[i].fileSectorBase = 0;
    audioTracks[i].startLba       = 0;
    audioTracks[i].lengthSectors  = 0;
  }

  // ── 7. Reset SCSI polling state ───────────────────────────────────────────
  subPollActive        = false;
  unitAttentionPending = false;
}

// =============================================================================
//  MOUNT / UMOUNT
// =============================================================================
bool doMount(const String &filename) {
  if (!sdReady) { dualPrint.println(F("[ERR] SD not available.")); return false; }

  bool hadDisc = isoOpen;  // save before fullReset zeroes it

  // ── Full clean slate (audio, cache, file handles, all state) ─────────────
  fullReset();

  String fl = filename; fl.toLowerCase();
  String actualFile = filename;

  if (fl.endsWith(".cue")) {
    dualPrint.printf("[CUE] Parsing: %s\n", filename.c_str());
    actualFile = parseCue(filename);
    if (!actualFile.length()) { dualPrint.println(F("[ERR] CUE: no data track found.")); return false; }
    // parseCue() sets binRawSectorSize / binHeaderOffset / binUserDataSize / audioTracks[]
  } else {
    // ISO / plain BIN defaults (parseCue not called)
    if (fl.endsWith(".iso")) {
      binRawSectorSize = 2048; binHeaderOffset = 0; binUserDataSize = 2048;
    } else {
      binRawSectorSize = 2352; binHeaderOffset = 16; binUserDataSize = 2048;
    }
  }

  isoFileHandle = SD_MMC.open(actualFile.c_str(), FILE_READ);
  if (!isoFileHandle) { dualPrint.printf("[ERR] Cannot open: %s\n", actualFile.c_str()); return false; }

  uint64_t fileSize = isoFileHandle.size();
  if (!fileSize) { isoFileHandle.close(); dualPrint.println(F("[ERR] File is empty.")); return false; }

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
  cdromBlockCount = mountedBlocks;  // default; overridden below after discLeadOutLba is set
  // For non-CUE or single-BIN: lead-out = total sectors
  // For CUE separate files: already set by parseCue() to virtualCursor (data + audio)
  if (discLeadOutLba == 0) discLeadOutLba = mountedBlocks;
  // cdromBlockCount is used by the SCSI patch as the lead-out LBA in READ TOC.
  // It must be the FULL virtual disc end (including audio tracks), not just the data track.
  // discLeadOutLba is correct for both separate-BIN (from parseCue) and single-BIN/ISO.
  cdromBlockCount = discLeadOutLba;
  // Signal new media to host
  if (cfg.dosCompat) {
    // DOS compat: msc.begin() is NEVER called after initUSBMSC.
    //
    // CRITICAL LESSON: do NOT signal msc.mediaPresent(false) + delay() before
    // the swap.  When Windows sees NOT_READY for ~3+ poll cycles it fully
    // unmounts the CDFS driver from the drive.  When media returns the driver
    // fails to re-attach → "CD doesn't exist" error even though the AutoPlay
    // popup appeared (UA was delivered).
    //
    // The correct approach for BOTH Windows and DOS (MSCDEX):
    //   1. msc_set_unit_attention() while mscMediaPresent=false → UA queued,
    //      TUR still returns NOT READY so it cannot be consumed prematurely.
    //   2. mscMediaPresent=true  → reads unblocked.
    //   3. msc.mediaPresent(true) → TinyUSB internal flag.
    //   First TUR after step 3 sees the UA → UNIT ATTENTION / MEDIUM CHANGED
    //   → Windows keeps CDFS mounted and re-reads TOC + PVD → label appears.
    //   MSCDEX also handles UNIT ATTENTION natively; no NOT_READY trick needed.
    //
    msc_set_unit_attention();   // UA queued while reads are still blocked
    mscMediaPresent = true;     // unblock reads; next TUR delivers UA
    msc.mediaPresent(true);     // TinyUSB internal flag
    dualPrint.printf("[USB]  DOS compat: media %s, UA signalled\n",
                     hadDisc ? "swapped" : "inserted (first)");
  } else {
    // Normal (Windows/Linux): full USB re-enumeration with correct block count
    mscMediaPresent = false;
    msc.mediaPresent(false);
    tud_disconnect();
    delay(800);  // give OS time to fully process USB removal
    // Re-register callbacks before msc.begin() — begin() may clear them internally
    msc.onRead(mscReadCb);
    msc.onWrite(mscWriteCb);
    msc.isWritable(false);
    msc.begin(mountedBlocks, 2048);
    mscMediaPresent = true;
    msc.mediaPresent(true);
    tud_connect();
    delay(500);  // give OS time to enumerate before UA
    msc_set_unit_attention();  // signal UNIT ATTENTION to host
  }

  // Benchmark: read first sector to estimate SD speed
  {
    uint8_t buf[2048];
    isoFileHandle.seek(0);
    unsigned long t0 = micros();
    isoFileHandle.read(buf, 2048);
    unsigned long dt = micros() - t0;
    float sdSpeedMBs = (dt > 0) ? (2048.0f / dt) : 0;
    dualPrint.printf("[SD]   Read speed: %.2f MB/s  (USB max: ~1.0 MB/s)\n", sdSpeedMBs);
    isoFileHandle.seek(0);  // rewind after benchmark
  }
  dualPrint.printf("[OK]  Mounted : %s\n", actualFile.c_str());
  dualPrint.printf("      Image   : %s\n", filename.c_str());
  dualPrint.printf("      Sectors : %lu x 2048 B  (raw: %u B/sector, header: +%u B)\n",
                mountedBlocks, binRawSectorSize, binHeaderOffset);
  dualPrint.printf("      Size    : %.1f MB\n", (float)fileSize / (1024.0f * 1024.0f));
  return true;
}

void doUmount() {
  fullReset();   // clean slate — identical to boot state

  if (cfg.dosCompat) {
    // DOS compat: drive stays connected — just eject the disc
    msc.mediaPresent(false);   // signal: no media, drive stays enumerated
    dualPrint.println(F("[USB]  DOS compat: disc ejected (drive stays connected)"));
  } else {
    // Re-register callbacks before msc.begin() — begin() may clear them internally
    msc.onRead(mscReadCb);
    msc.onWrite(mscWriteCb);
    msc.isWritable(false);
    msc.begin(1, 2048);
    msc.mediaPresent(false);
    tud_disconnect(); delay(800); tud_connect();
  }
  dualPrint.println(F("[OK]  CD-ROM ejected."));
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
//  PSRAM SECTOR CACHE — 2-window LRU read-ahead cache for USB MSC data-track sectors
//
//  Problem: mscReadCb was called once per 2048-byte sector with a seek()+read()
//  each time.  For a 700 MB disc that means ~350 000 individual SD transactions,
//  each paying seek overhead (FatFS cluster-chain traversal + SD command latency).
//
//  Single-window solution: on a miss at LBA X, read CACHE_SECTOR_COUNT sectors
//  sequentially into PSRAM.  Subsequent requests served by memcpy (≈1 µs).
//
//  Problem with single window: if the host alternates between two non-overlapping
//  LBA regions (e.g. directory at LBA 106 and data at LBA 833), the single window
//  is continuously evicted → 100% miss rate → every read costs ~200 ms → PC hangs.
//
//  2-window LRU solution: two independent cache windows share the same PSRAM pool.
//  Each holds 128 sectors (256 KB).  LRU eviction replaces the least-recently-used
//  window on a miss.  Two hot regions are both retained simultaneously, eliminating
//  the ping-pong thrash while keeping total PSRAM at 512 KB.
//
//  Format support:
//    ISO / MODE1-2048  → raw sector == user data → single large read()
//    MODE1/2352        → read raw, extract bytes [16..2063]
//    MODE2/2352        → read raw, extract bytes [24..2071]
//    MODE2/2336        → read raw, extract bytes [8..2055]
//    CDG/2448          → read raw, extract bytes [0..2351] (binUserDataSize=2352)
//
//  Safety:
//    • Cache invalidated on every doMount() / doUmount() — no stale data.
//    • g_cacheMtx serialises fill / invalidate (cross-core safe).
//    • isoFileHandle touched only from mscReadCb (core 0) or doMount/doUmount
//      (core 1 with mscMediaPresent=false guard).
//    • Audio task uses its own File handle + sdMutex — fully independent.
// =============================================================================
#define CACHE_WINDOWS       2u               // number of LRU windows
#define CACHE_SECTOR_COUNT  128u             // sectors per window (128 × 2048 = 256 KB)
#define CACHE_WINDOW_BYTES  (CACHE_SECTOR_COUNT * 2048u)
#define CACHE_BYTES         (CACHE_WINDOWS * CACHE_WINDOW_BYTES)  // 512 KB total

static uint8_t*          g_cacheBuf          = nullptr;   // PSRAM: all windows contiguous
static uint8_t*          g_rawBuf            = nullptr;   // PSRAM: temp raw-sector (≤2448 B)
static uint32_t          g_cacheBase[CACHE_WINDOWS];      // LBA base (0xFFFFFFFF = invalid)
static uint32_t          g_cacheCnt [CACHE_WINDOWS];      // valid sector count per window
static uint32_t          g_cacheTick[CACHE_WINDOWS];      // LRU timestamps
static uint32_t          g_cacheNow          = 0;         // global LRU clock
static SemaphoreHandle_t g_cacheMtx          = nullptr;

// Return pointer to data area of window w.
static inline uint8_t* cacheWinBuf(int w) {
  return g_cacheBuf + (uint32_t)w * CACHE_WINDOW_BYTES;
}

// Allocate cache buffers in PSRAM.  Call once from setup().
static void cacheAlloc() {
  g_cacheMtx = xSemaphoreCreateMutex();
  for (int i = 0; i < (int)CACHE_WINDOWS; i++) {
    g_cacheBase[i] = 0xFFFFFFFFu;
    g_cacheCnt [i] = 0;
    g_cacheTick[i] = 0;
  }
  g_cacheNow = 0;
  g_cacheBuf = (uint8_t*)heap_caps_malloc(CACHE_BYTES,
                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_rawBuf   = (uint8_t*)heap_caps_malloc(2448u,
                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (g_cacheBuf && g_rawBuf) {
    dualPrint.printf("[CACHE] %u KB read-ahead cache in PSRAM (%u windows × %u sectors)\n",
                     CACHE_BYTES / 1024u, CACHE_WINDOWS, CACHE_SECTOR_COUNT);
  } else {
    dualPrint.println(F("[CACHE] PSRAM unavailable — sector cache disabled"));
    heap_caps_free(g_cacheBuf); g_cacheBuf = nullptr;
    heap_caps_free(g_rawBuf);   g_rawBuf   = nullptr;
  }
}

// Invalidate all windows.  Call whenever the mounted image changes.
static void cacheInvalidate() {
  if (!g_cacheMtx) return;
  if (xSemaphoreTake(g_cacheMtx, pdMS_TO_TICKS(500)) == pdTRUE) {
    for (int i = 0; i < (int)CACHE_WINDOWS; i++) {
      g_cacheBase[i] = 0xFFFFFFFFu;
      g_cacheCnt [i] = 0;
      g_cacheTick[i] = 0;
    }
    g_cacheNow = 0;
    xSemaphoreGive(g_cacheMtx);
  }
}

// Fill window `win` starting at disc-relative LBA `lba`.
// Must be called with g_cacheMtx already held.
static void cacheFillWindow_locked(int win, uint32_t lba) {
  if (!g_cacheBuf || !g_rawBuf || !isoOpen || !mscMediaPresent) return;

  uint32_t maxLba = (dataTrackSectors_g > 0) ? dataTrackSectors_g : cdromBlockCount;
  if (lba >= maxLba) return;
  uint32_t avail = maxLba - lba;
  if (avail > CACHE_SECTOR_COUNT) avail = CACHE_SECTOR_COUNT;

  g_cacheBase[win] = lba;
  g_cacheCnt [win] = 0;
  uint8_t* winBuf  = cacheWinBuf(win);

  if (binRawSectorSize <= 2048) {
    // Cooked / ISO / MODE1-2048: single large sequential read.
    uint64_t pos = (uint64_t)(lba + dataTrackStartSect) * 2048u;
    if (!isoFileHandle.seek(pos)) { g_cacheBase[win] = 0xFFFFFFFFu; return; }
    int32_t got = (int32_t)isoFileHandle.read(winBuf, avail * 2048u);
    if (got > 0) g_cacheCnt[win] = (uint32_t)got / 2048u;
  } else {
    // Raw format: seek once, read sector-by-sector, extract user data.
    uint64_t pos = (uint64_t)(lba + dataTrackStartSect) * binRawSectorSize;
    if (!isoFileHandle.seek(pos)) { g_cacheBase[win] = 0xFFFFFFFFu; return; }
    for (uint32_t i = 0; i < avail; i++) {
      if (!isoOpen || !mscMediaPresent) break;
      int32_t got = (int32_t)isoFileHandle.read(g_rawBuf, binRawSectorSize);
      if (got < (int32_t)binRawSectorSize) break;
      memcpy(winBuf + i * 2048u, g_rawBuf + binHeaderOffset, binUserDataSize);
      g_cacheCnt[win]++;
    }
  }

  if (g_cacheCnt[win] == 0) g_cacheBase[win] = 0xFFFFFFFFu;

  if (cfg.debugMode)
    dualPrint.printf("[CACHE] fill LBA %lu+%lu ok (%s) win%d\n",
                     (unsigned long)lba, (unsigned long)g_cacheCnt[win],
                     binRawSectorSize <= 2048 ? "cooked" : "raw", win);
}

// =============================================================================
//  USB MSC - READ CALLBACK
// =============================================================================
static int32_t mscReadCb(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (!isoOpen || !mscMediaPresent) return -1;

  // ── CACHE PATH ────────────────────────────────────────────────────────────
  // Serve whole-sector reads from the 2-window LRU PSRAM cache.
  // On a hit in any window: memcpy (≈1 µs).
  // On a miss: evict the least-recently-used window, fill from SD (~100 ms
  // for 128-sector / 256 KB window), then serve.
  // Both hot regions (e.g. directory LBA and data LBA) stay cached simultaneously,
  // eliminating ping-pong thrash that would occur with a single window.
  if (g_cacheBuf && g_cacheMtx && offset == 0) {
    uint32_t sectorCount = bufsize / 2048u;
    if (sectorCount > 0 && (bufsize % 2048u) == 0 &&
        sectorCount <= CACHE_SECTOR_COUNT) {

      if (xSemaphoreTake(g_cacheMtx, pdMS_TO_TICKS(200)) == pdTRUE) {

        // Search all windows for a hit.
        int hitWin = -1;
        for (int w = 0; w < (int)CACHE_WINDOWS; w++) {
          if (g_cacheBase[w] != 0xFFFFFFFFu &&
              lba >= g_cacheBase[w] &&
              (lba - g_cacheBase[w]) + sectorCount <= g_cacheCnt[w]) {
            hitWin = w;
            break;
          }
        }

        if (hitWin < 0) {
          // Miss: evict the LRU window and fill it.
          int evict = 0;
          for (int w = 1; w < (int)CACHE_WINDOWS; w++)
            if (g_cacheTick[w] < g_cacheTick[evict]) evict = w;
          g_cacheTick[evict] = ++g_cacheNow;
          cacheFillWindow_locked(evict, lba);

          // Re-check after fill.
          for (int w = 0; w < (int)CACHE_WINDOWS; w++) {
            if (g_cacheBase[w] != 0xFFFFFFFFu &&
                lba >= g_cacheBase[w] &&
                (lba - g_cacheBase[w]) + sectorCount <= g_cacheCnt[w]) {
              hitWin = w;
              break;
            }
          }
        }

        if (hitWin >= 0) {
          g_cacheTick[hitWin] = ++g_cacheNow;  // refresh LRU on hit
          uint8_t* src = cacheWinBuf(hitWin) + (lba - g_cacheBase[hitWin]) * 2048u;
          memcpy(buffer, src, bufsize);
          // Diagnostic: log first 6 bytes for LBA 16 (ISO PVD) — must be 01 43 44 30 30 31
          if (cfg.debugMode && lba == 16 && bufsize >= 6) {
            char _dg[80];
            snprintf(_dg, sizeof(_dg),
              "[DBG]  LBA16 PVD bytes: %02X %02X %02X %02X %02X %02X (expect 01 43 44 30 30 31)\n",
              src[0], src[1], src[2], src[3], src[4], src[5]);
            dualPrint.print(_dg);
          }
          xSemaphoreGive(g_cacheMtx);
          return (int32_t)bufsize;
        }
        xSemaphoreGive(g_cacheMtx);
        // Fill didn't cover all sectors (near EOT) → fall through to direct read.
      }
    }
  }

  // ── DIRECT PATH (cache disabled, sub-sector read, or near-EOT fallback) ──
  if (binRawSectorSize <= 2048) {
    // Cooked format (ISO / MODE1/2048): sectors contiguous in file.
    uint64_t pos = (uint64_t)(lba + dataTrackStartSect) * 2048 + offset;
    if (!isoFileHandle.seek(pos)) return -1;
    int32_t got = (int32_t)isoFileHandle.read((uint8_t*)buffer, bufsize);
    // Diagnostic: log first 6 bytes for LBA 16 (ISO PVD)
    if (cfg.debugMode && lba == 16 && got >= 6) {
      uint8_t* b = (uint8_t*)buffer;
      char _dg[80];
      snprintf(_dg, sizeof(_dg),
        "[DBG]  LBA16 PVD direct bytes: %02X %02X %02X %02X %02X %02X (expect 01 43 44 30 30 31)\n",
        b[0], b[1], b[2], b[3], b[4], b[5]);
      dualPrint.print(_dg);
    }
    return got;
  }

  // Raw sector format (MODE1/2352, MODE2/2352, MODE2/2336, CDG/2448):
  // Skip sync/header/ECC per sector.  Re-check isoOpen each iteration —
  // doMount() on the other core may close isoFileHandle while we're mid-loop.
  uint8_t* dst    = (uint8_t*)buffer;
  int32_t  total  = 0;
  uint32_t rem    = bufsize;
  uint32_t curLba = lba + dataTrackStartSect;
  uint32_t inSect = offset;

  while (rem > 0) {
    if (!isoOpen || !mscMediaPresent) return total > 0 ? total : -1;

    uint32_t chunk = binUserDataSize - inSect;
    if (chunk > rem) chunk = rem;

    uint64_t pos = (uint64_t)curLba * binRawSectorSize + binHeaderOffset + inSect;
    if (!isoFileHandle.seek(pos)) return total > 0 ? total : -1;
    int32_t got = (int32_t)isoFileHandle.read(dst, chunk);
    if (got <= 0) break;

    dst    += got;
    total  += got;
    rem    -= (uint32_t)got;
    inSect  = 0;
    curLba++;
  }
  return total > 0 ? total : -1;
}

static int32_t mscWriteCb(uint32_t lba, uint32_t offset, uint8_t *buf, uint32_t bufsize) {
  (void)lba; (void)offset; (void)buf; (void)bufsize;
  return -1;  // read-only
}

static void mscFlushCb(void) {}

void initUSBMSC() {
  // Unique serial number ensures Windows separates CD-ROM mode from GoTek mode
  uint64_t _mac = ESP.getEfuseMac();
  char sn[24]; snprintf(sn, sizeof(sn), "CD-%06llX", (unsigned long long)(_mac & 0xFFFFFF));
  USB.serialNumber(sn);
  USB.productName("VirtualCDROM");
  msc.vendorID("ESP32-S3");
  msc.productID("VirtualCDROM");
  msc.productRevision("1.00");
  msc.onRead(mscReadCb);
  msc.onWrite(mscWriteCb);
  msc.isWritable(false);
  msc.begin(400000, 2048);
  msc.mediaPresent(false);
  USB.begin();
  dualPrint.printf("[OK]  USB MSC CD-ROM initialized. Serial: %s\n", sn);
}

// =============================================================================
//  GOTEK FLOPPY EMULATOR MODE
//  In GoTek mode the ESP32 presents itself as a USB MSC flash drive (512-byte
//  sectors, FAT32) that FlashFloppy reads exactly like a real USB stick.
//  The virtual FAT32 volume contains DSKA0000.img, DSKA0001.img, ... files
//  that are served directly from .img files stored on the SD card under the
//  configured GoTek image directory (default: /gotek/).
//
//  Phase 1 (this implementation): USB MSC device is registered, sector read/
//  write callbacks are connected.  The virtual FAT32 generator (Phase 2) will
//  replace the stub implementations below.
//
//  CD-ROM code (doMount, mscReadCb, cacheFill, audioTask, parseCue, etc.) is
//  completely untouched — GoTek mode branches before any of it is initialised.
// =============================================================================

// GoTek virtual disk geometry — 64 MB virtual FAT32 volume.
// GoTek only needs to read the FAT32 directory and the .img file data;
// it does not care about the total size as long as it is large enough.
// =============================================================================
//  GOTEK FLOPPY EMULATOR MODE — Raw sector passthrough
//
//  Original GoTek firmware reads USB flash as RAW sectors — NO FAT, NO MBR.
//  Layout on virtual USB disk:
//    LBA 0 .. (img0.sectors-1)       = Slot 0  (first  .img file)
//    LBA img0.sectors .. x           = Slot 1  (second .img file)
//    ...
//
//  GoTek's front-panel buttons change the current slot number.
//  GoTek reads from LBA (slot × sectors_per_slot) transparently.
//  ESP32 maps every LBA read/write to the corresponding .img file on SD.
//
//  Compatible with: original GoTek firmware + USB_Floppy_Manager v1.40i
// =============================================================================

#define GK_SECTOR_SIZE   512u
#define GK_MAX_FILES     100          // max slots
#define GK_MAX_SECTORS   (1000u * GK_SLOT_STRIDE)  // fixed USB disk size GoTek sees
// GoTek (FlashFloppy) slot stride — number of 512-byte sectors per image slot.
// This MUST match GoTek's configured drive type:
//   2880 = PC 1.44 MB  (80 tracks × 2 sides × 18 sectors)
//   3072 = PC ~1.5 MB  (observed on some FlashFloppy builds — use if slot 1+ fail)
//   3360 = PC 1.68 MB  (XDF / DMF format)
//   1760 = Amiga DD    (80 tracks × 2 sides × 11 sectors)
//   3520 = Amiga HD
// Check the Log tab after boot — if slot 1 boots correctly change is not needed.
#define GK_SLOT_STRIDE   3072u

// Forward declarations for MSC callbacks
static int32_t gotekReadCb(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);

static int32_t gotekWriteCb(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);
static bool    gotekStartStopCb(uint8_t, bool, bool);

// ── Slot file table ───────────────────────────────────────────────────────────
struct GotekFile {
  char     path[192];   // full SD path e.g. /gotek/DSKA0000.img
  uint32_t sz;          // file size in bytes
  uint32_t sectors;     // number of 512-byte sectors (sz rounded up)
  uint32_t startLba;    // this slot's first LBA on virtual USB disk
  uint32_t slotNum;     // logical slot number (parsed from filename, e.g. DSKA0002→2)
};

static GotekFile        gkF[GK_MAX_FILES];
static int              gkFC       = 0;
static bool             gkRdy      = false;
static volatile int     gkCurSlot  = -1;
static volatile int     gkSelectedSlot = -1;
// Runtime USB mode — follows cfg.gotekUsbMode but forced to 0 (raw) in single-slot mode.
// Kept separate so single-slot mode doesn't permanently corrupt cfg.gotekUsbMode.
static uint8_t          gkRuntimeUsbMode = 0;
static volatile uint32_t gkLastLba = 0;
static uint32_t          gkReadCnt[GK_MAX_FILES]  = {};
static uint32_t          gkWriteCnt[GK_MAX_FILES] = {};

// ── LBA access ring buffer (debug) ───────────────────────────────────────────
#define GK_LOG_SIZE 64
struct GkLogEntry {
  uint32_t lba;
  int      slot;    // -1 = unmapped
  uint8_t  op;      // 'R' read, 'W' write
  uint8_t  b0, b1, b2, b3;  // first 4 bytes of sector
};
static GkLogEntry  gkLog[GK_LOG_SIZE];
static uint32_t    gkLogHead = 0;   // next write index (ring)
static uint32_t    gkLogSeq  = 0;   // monotonic counter for polling

static void gkLogPush(uint32_t lba, int slot, uint8_t op, const uint8_t* data) {
  GkLogEntry& e = gkLog[gkLogHead % GK_LOG_SIZE];
  e.lba  = lba; e.slot = slot; e.op = op;
  if (data) { e.b0=data[0];e.b1=data[1];e.b2=data[2];e.b3=data[3]; }
  else       { e.b0=e.b1=e.b2=e.b3=0; }
  gkLogHead++; gkLogSeq++;
}

// ── LRU read handle cache — avoids opening/closing file on every sector read ──
// Keeps up to GK_CACHE_HANDLES files open. GoTek typically reads one slot
// at a time sequentially, so 2 entries handles all practical cases.
// With 999 possible slots we cannot keep one handle per slot open.
#define GK_CACHE_HANDLES 4
static struct {
  int      fi;    // array index into gkF[], -1 = empty
  File     fh;   // open file handle
} gkRHCache[GK_CACHE_HANDLES];
static int gkRHLRU[GK_CACHE_HANDLES]; // LRU order: gkRHLRU[0] = most recently used idx

static void gkCacheInit() {
  for (int i = 0; i < GK_CACHE_HANDLES; i++) {
    if (gkRHCache[i].fh) gkRHCache[i].fh.close();
    gkRHCache[i].fi = -1;
    gkRHLRU[i] = i;
  }
}

// Returns open File& for gkF[fi], opening if not cached (evicting LRU entry)
static File* gkCacheGet(int fi) {
  // Check cache
  for (int i = 0; i < GK_CACHE_HANDLES; i++) {
    if (gkRHCache[i].fi == fi) {
      // Move to front of LRU
      for (int j = 0; j < GK_CACHE_HANDLES; j++) if (gkRHLRU[j] == i) {
        memmove(&gkRHLRU[1], &gkRHLRU[0], j * sizeof(int));
        gkRHLRU[0] = i; break;
      }
      return &gkRHCache[i].fh;
    }
  }
  // Cache miss — evict LRU (last entry)
  int evict = gkRHLRU[GK_CACHE_HANDLES - 1];
  if (gkRHCache[evict].fh) gkRHCache[evict].fh.close();
  gkRHCache[evict].fi = -1;
  gkRHCache[evict].fh = SD_MMC.open(gkF[fi].path, FILE_READ);
  if (!gkRHCache[evict].fh) {
    dualPrint.printf("[GK] ERROR: cannot open slot %d: %s\n", fi, gkF[fi].path);
    return nullptr;
  }
  gkRHCache[evict].fi = fi;
  // Promote to front of LRU
  memmove(&gkRHLRU[1], &gkRHLRU[0], (GK_CACHE_HANDLES - 1) * sizeof(int));
  gkRHLRU[0] = evict;
  return &gkRHCache[evict].fh;
}

static void gkOpenHandles() {
  gkCacheInit();
  dualPrint.println(F("[GOTEK] Opening read handle cache:"));
  // Pre-warm first GK_CACHE_HANDLES slots for fast initial access
  for (int i = 0; i < gkFC && i < GK_CACHE_HANDLES; i++) {
    File* fh = gkCacheGet(i);
    if (!fh) continue;
    uint8_t bs[64]; fh->seek(0); int got = fh->read(bs, sizeof(bs));
    bool ok = got >= 22 && (bs[0]==0xEB||bs[0]==0xE9) && bs[21]==0xF0;
    dualPrint.printf("[GOTEK] Slot %03u: %s  sz=%lu B  boot=%02X FAT12=%s totSec=%u\n",
      gkF[i].slotNum, strrchr(gkF[i].path,'/')+1, (unsigned long)gkF[i].sz,
      bs[0], ok?"OK":"WARN", (uint16_t)bs[19]|((uint16_t)bs[20]<<8));
  }
  dualPrint.printf("[GOTEK] Cache ready (%d/%d slots pre-warmed, %d FDs used)\n",
    min(gkFC, GK_CACHE_HANDLES), gkFC, GK_CACHE_HANDLES);
}

static void gkCloseHandles() {
  gkCacheInit();  // closes all cached handles and resets LRU
}
// == GoTek PSRAM sector cache ================================================
// One sliding window per slot — GoTek reads sequentially, so a single
// read-ahead window per slot achieves near-100% hit rate.
// Window size: GK_CACHE_WIN_SEC sectors × 512 B.
// Total PSRAM used: GK_CACHE_WIN_SEC × 512 bytes (default 256 = 128 KB).
// Falls back gracefully when PSRAM is not available.

#define GK_CACHE_WIN_SEC  256u          // sectors per window (256 × 512 = 128 KB)
#define GK_CACHE_WIN_BYTES (GK_CACHE_WIN_SEC * GK_SECTOR_SIZE)

static uint8_t*  gkCacheBuf  = nullptr; // PSRAM window buffer
static int       gkCacheSlot = -1;      // which slot is cached (-1 = invalid)
static uint32_t  gkCacheBase = 0xFFFFFFFFu; // first sector in window
static uint32_t  gkCacheCnt  = 0;           // valid sectors in window

static void gkCacheAlloc() {
  gkCacheBuf = (uint8_t*)heap_caps_malloc(GK_CACHE_WIN_BYTES,
                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (gkCacheBuf) {
    dualPrint.printf("[GK CACHE] %u KB read-ahead in PSRAM (%u sectors/window)\n",
      GK_CACHE_WIN_BYTES / 1024u, GK_CACHE_WIN_SEC);
  } else {
    dualPrint.println(F("[GK CACHE] PSRAM not available — no sector cache"));
  }
}

static void gkCacheInvalidate() {
  gkCacheSlot = -1;
  gkCacheBase = 0xFFFFFFFFu;
  gkCacheCnt  = 0;
}

// Fill cache window for file fi starting at sector sectorInFile.
// Must be called with sdMutex held.
static void gkCacheFill(int fi, uint32_t sectorInFile) {
  if (!gkCacheBuf || fi < 0 || fi >= gkFC) return;
  uint32_t avail = gkF[fi].sectors > sectorInFile
                   ? gkF[fi].sectors - sectorInFile : 0;
  if (!avail) return;
  if (avail > GK_CACHE_WIN_SEC) avail = GK_CACHE_WIN_SEC;
  File* fh = gkCacheGet(fi);
  if (!fh) return;
  uint64_t pos = (uint64_t)sectorInFile * GK_SECTOR_SIZE;
  if ((uint64_t)fh->position() != pos) fh->seek(pos);
  int32_t got = (int32_t)fh->read(gkCacheBuf, avail * GK_SECTOR_SIZE);
  gkCacheSlot = fi;
  gkCacheBase = sectorInFile;
  gkCacheCnt  = (got > 0) ? (uint32_t)got / GK_SECTOR_SIZE : 0;
  if (cfg.debugMode)
    dualPrint.printf("[GK CACHE] fill fi=%d sec=%u+%u\n", fi, sectorInFile, gkCacheCnt);
}

// Try to serve one sector from cache. Returns true on hit.
static bool gkCacheRead(int fi, uint32_t sectorInFile, uint8_t* buf) {
  if (!gkCacheBuf || gkCacheSlot != fi || gkCacheBase == 0xFFFFFFFFu) return false;
  if (sectorInFile < gkCacheBase || sectorInFile >= gkCacheBase + gkCacheCnt) return false;
  memcpy(buf, gkCacheBuf + (sectorInFile - gkCacheBase) * GK_SECTOR_SIZE, GK_SECTOR_SIZE);
  return true;
}

// == Virtual FAT32 filesystem (GoTek FAT USB mode) ===========================
// FlashFloppy requires FAT32 on the USB source device (ExFAT/FAT16 not supported).
// Root directory contains DSKA0000.IMG, DSKA0001.IMG... mapped to actual image
// files in slot order. No files renamed on SD card.
// Use this mode with FlashFloppy in indexed navigation (nav-mode = indexed in FF.CFG)
// or native navigation mode. Raw LBA mode is for stock GoTek firmware.

#define GK_FAT_SPC     8u          // sectors per cluster (4 KB)
#define GK_FAT_NFATS   2u
#define GK_FAT_RESV    32u         // reserved sectors (FAT32 standard)
#define GK_FAT_ROOTCLU 2u          // root directory cluster (FAT32: in data area)

// Free space added to FAT32 volume when WP=OFF so Windows can delete files
// (needs space for $RECYCLE.BIN etc.). Writes to these clusters are silently ignored.
#define GK_FAT_FREE_CLUS  1280u   // 1280 × 8 × 512 = 5 MB free space

static struct {
  bool     valid;
  uint32_t fatSz;
  uint32_t fat1Start;
  uint32_t fat2Start;
  uint32_t dataStart;
  uint32_t totalSectors;
  uint32_t totalClusters;   // file clusters (3..3+totalClusters-1)
  uint32_t freeClusters;    // free clusters appended after file clusters (WP=OFF only)
  uint32_t firstFreeCluster;// cluster number of first free cluster
  uint32_t startCluster[GK_MAX_FILES];
  uint32_t clusterCount[GK_MAX_FILES];
} gkFat;

// Shadow root directory for FAT32 writable mode (WP=OFF).
// Mirrors the virtual root dir in RAM so Windows can delete/modify entries
// without touching real SD files. Allocated lazily, reset on gkBuildFS().
// Root dir = 1 cluster = GK_FAT_SPC sectors = 4096 bytes = 128 dir entries.
static uint8_t* gkShadowRoot    = nullptr;
static bool     gkShadowRootOk  = false;

// Populate shadow root dir from current gkF[] slot table
static void gkShadowRootInit() {
  if (!gkShadowRoot)
    gkShadowRoot = (uint8_t*)malloc(GK_FAT_SPC * GK_SECTOR_SIZE);
  if (!gkShadowRoot) { gkShadowRootOk = false; return; }
  memset(gkShadowRoot, 0, GK_FAT_SPC * GK_SECTOR_SIZE);
  for (int fi = 0; fi < gkFC && fi < 128; fi++) {
    uint8_t* de = gkShadowRoot + fi * 32;
    char stem[9]; snprintf(stem, sizeof(stem), "DSKA%04u", (unsigned)gkF[fi].slotNum);
    memcpy(de, stem, 8); memcpy(de+8, "IMG", 3);
    de[11] = 0x20;
    *(uint16_t*)&de[20] = (uint16_t)((gkFat.startCluster[fi] >> 16) & 0xFFFF);
    *(uint16_t*)&de[22] = 0;
    *(uint16_t*)&de[24] = (uint16_t)(((2024-1980)<<9)|(1<<5)|1);
    *(uint16_t*)&de[26] = (uint16_t)(gkFat.startCluster[fi] & 0xFFFF);
    *(uint32_t*)&de[28] = gkF[fi].sz;
  }
  gkShadowRootOk = true;
  dualPrint.printf("[GOTEK FAT] Shadow root init: %d entries\n", gkFC);
}

static void gkFatBuild() {
  memset(&gkFat, 0, sizeof(gkFat));
  if (!gkFC) return;

  // Cluster 2 = root directory (1 cluster = 8 sectors = 4 KB = 128 dir entries)
  // File clusters start at 3
  uint32_t nextCluster = 3;
  for (int i = 0; i < gkFC; i++) {
    uint32_t cc = (gkF[i].sectors + GK_FAT_SPC - 1) / GK_FAT_SPC;
    if (!cc) cc = 1;
    gkFat.startCluster[i] = nextCluster;
    gkFat.clusterCount[i] = cc;
    nextCluster += cc;
  }
  // +1 for root dir cluster (cluster 2), rest are file clusters
  gkFat.totalClusters = nextCluster - 2;

  // FAT32: 4 bytes per entry
  // Add free clusters when WP=OFF so Windows can use the volume (Recycle Bin, etc.)
  // Writes to free clusters are silently ignored — no real files are created.
  gkFat.freeClusters    = (cfg.gotekUsbMode == 1 && cfg.gotekFatWP == 0) ? GK_FAT_FREE_CLUS : 0;
  gkFat.firstFreeCluster = nextCluster;  // free clusters start right after file clusters

  gkFat.fatSz      = ((nextCluster + gkFat.freeClusters) * 4u + 511u) / 512u;
  gkFat.fat1Start  = GK_FAT_RESV;
  gkFat.fat2Start  = GK_FAT_RESV + gkFat.fatSz;
  gkFat.dataStart  = GK_FAT_RESV + 2 * gkFat.fatSz;
  gkFat.totalSectors = gkFat.dataStart +
    (gkFat.totalClusters + 1 + gkFat.freeClusters) * GK_FAT_SPC; // +1 for root cluster
  gkFat.valid = true;

  dualPrint.printf("[GOTEK FAT32] fatSz=%u data=%u total=%u clusters=%u\n",
    gkFat.fatSz, gkFat.dataStart, gkFat.totalSectors, gkFat.totalClusters);
  for (int i = 0; i < gkFC; i++)
    dualPrint.printf("[GOTEK FAT32] DSKA%04u.IMG -> %s  clu=%u cc=%u\n",
      gkF[i].slotNum, strrchr(gkF[i].path,'/')+1,
      gkFat.startCluster[i], gkFat.clusterCount[i]);
  // Initialize shadow root dir (used when WP=OFF to allow Windows delete operations)
  gkShadowRootOk = false;
  if (cfg.gotekUsbMode == 1 && cfg.gotekFatWP == 0)
    gkShadowRootInit();
}

static void gkFatReadSector(uint32_t lba, uint8_t* buf) {
  memset(buf, 0, 512);
  if (!gkFat.valid) return;

  // LBA 0: FAT32 Boot Sector
  if (lba == 0) {
    buf[0]=0xEB; buf[1]=0x58; buf[2]=0x90;
    memcpy(&buf[3], "ESP32GK ", 8);
    *(uint16_t*)&buf[11] = 512;              // BytesPerSector
    buf[13]              = (uint8_t)GK_FAT_SPC; // SectorsPerCluster
    *(uint16_t*)&buf[14] = (uint16_t)GK_FAT_RESV; // ReservedSectors
    buf[16]              = (uint8_t)GK_FAT_NFATS;  // NumFATs
    *(uint16_t*)&buf[17] = 0;               // RootEntCnt = 0 (FAT32)
    *(uint16_t*)&buf[19] = 0;               // TotSec16 = 0 (FAT32)
    buf[21]              = 0xF8;             // MediaType
    *(uint16_t*)&buf[22] = 0;               // FATSz16 = 0 (FAT32)
    *(uint16_t*)&buf[24] = 63;              // SectorsPerTrack
    *(uint16_t*)&buf[26] = 255;             // NumHeads
    *(uint32_t*)&buf[28] = 0;               // HiddenSectors
    *(uint32_t*)&buf[32] = gkFat.totalSectors; // TotSec32
    // FAT32 extended BPB (offset 36+)
    *(uint32_t*)&buf[36] = gkFat.fatSz;    // FATSz32
    *(uint16_t*)&buf[40] = 0;               // ExtFlags
    *(uint16_t*)&buf[42] = 0;               // FSVer = 0
    *(uint32_t*)&buf[44] = GK_FAT_ROOTCLU; // RootClus = 2
    *(uint16_t*)&buf[48] = 1;               // FSInfo at sector 1
    *(uint16_t*)&buf[50] = 6;               // BkBootSec at sector 6
    // bytes 52-63: reserved (zeros)
    buf[64] = 0x80;                          // DrvNum
    buf[66] = 0x29;                          // BootSig
    *(uint32_t*)&buf[67] = 0xE5321ABC;      // VolID
    memcpy(&buf[71], "GOTEK IMGS ", 11);    // VolLab (11 bytes)
    memcpy(&buf[82], "FAT32   ", 8);        // FilSysType
    buf[510]=0x55; buf[511]=0xAA;
    return;
  }

  // LBA 1: FSInfo sector
  if (lba == 1) {
    *(uint32_t*)&buf[0]   = 0x41615252;
    *(uint32_t*)&buf[484] = 0x61417272;
    *(uint32_t*)&buf[488] = gkFat.freeClusters;            // FreeCount
    *(uint32_t*)&buf[492] = gkFat.freeClusters > 0         // NextFree
                            ? gkFat.firstFreeCluster : 0xFFFFFFFF;
    buf[508]=0x00; buf[509]=0x00; buf[510]=0x55; buf[511]=0xAA;
    return;
  }

  // FAT1 / FAT2 (4 bytes per entry, 128 entries per sector)
  if (lba >= gkFat.fat1Start && lba < gkFat.dataStart) {
    bool isFat2 = (lba >= gkFat.fat2Start);
    uint32_t base = (lba - (isFat2 ? gkFat.fat2Start : gkFat.fat1Start)) * 128;
    uint32_t* fat = (uint32_t*)buf;
    for (int e = 0; e < 128; e++) {
      uint32_t cl = base + (uint32_t)e;
      uint32_t val = 0x00000000;
      if      (cl == 0) val = 0x0FFFFFF8;
      else if (cl == 1) val = 0x0FFFFFFF;
      else if (cl == GK_FAT_ROOTCLU) val = 0x0FFFFFFF;
      else if (cl >= gkFat.firstFreeCluster &&
               cl <  gkFat.firstFreeCluster + gkFat.freeClusters) val = 0x00000000; // free
      else {
        for (int i = 0; i < gkFC; i++) {
          uint32_t cs = gkFat.startCluster[i], ce = cs + gkFat.clusterCount[i];
          if (cl >= cs && cl < ce) {
            val = (cl+1 < ce) ? (cl+1) : 0x0FFFFFFF;
            break;
          }
        }
      }
      fat[e] = val;
    }
    return;
  }

  // Data area (cluster 2 = root dir, cluster 3+ = file data)
  if (lba >= gkFat.dataStart) {
    uint32_t ds = lba - gkFat.dataStart;
    uint32_t cl = (ds / GK_FAT_SPC) + 2;
    uint32_t sc = ds % GK_FAT_SPC;

    // Cluster 2 = root directory entries
    if (cl == GK_FAT_ROOTCLU) {
      if (cfg.gotekFatWP == 0 && gkShadowRootOk && gkShadowRoot) {
        // Shadow mode (WP=OFF): serve from shadow root dir so Windows sees deletions
        memcpy(buf, gkShadowRoot + sc * GK_SECTOR_SIZE, GK_SECTOR_SIZE);
      } else {
        // WP=ON or no shadow: virtual generation
        uint32_t first = sc * 16;
        for (int e = 0; e < 16; e++) {
          uint32_t fi = first + (uint32_t)e;
          if (fi >= (uint32_t)gkFC) break;
          uint8_t* de = &buf[e * 32];
          char stem[9]; snprintf(stem, sizeof(stem), "DSKA%04u", (unsigned)gkF[fi].slotNum);
          memcpy(de, stem, 8); memcpy(de+8, "IMG", 3);
          de[11] = 0x20;
          *(uint16_t*)&de[20] = (uint16_t)((gkFat.startCluster[fi] >> 16) & 0xFFFF);
          *(uint16_t*)&de[22] = 0;
          *(uint16_t*)&de[24] = (uint16_t)(((2024-1980)<<9)|(1<<5)|1);
          *(uint16_t*)&de[26] = (uint16_t)(gkFat.startCluster[fi] & 0xFFFF);
          *(uint32_t*)&de[28] = gkF[fi].sz;
        }
      }
      return;
    }

    // Free clusters (WP=OFF): return zeros, writes silently ignored
    if (gkFat.freeClusters > 0 &&
        cl >= gkFat.firstFreeCluster &&
        cl <  gkFat.firstFreeCluster + gkFat.freeClusters) {
      return;  // buf already zeroed
    }

    // File data clusters (cluster 3+)
    for (int i = 0; i < gkFC; i++) {
      uint32_t cs = gkFat.startCluster[i], ce = cs + gkFat.clusterCount[i];
      if (cl < cs || cl >= ce) continue;
      uint32_t sf = (cl - cs) * GK_FAT_SPC + sc;
      if (sf >= gkF[i].sectors) return;
      // Try PSRAM cache first
      if (gkCacheRead(i, sf, buf)) return;
      gkCacheFill(i, sf);
      if (gkCacheRead(i, sf, buf)) return;
      // Fallback: direct SD read
      uint64_t fOff = (uint64_t)sf * GK_SECTOR_SIZE;
      File* fh = gkCacheGet(i); if (!fh) return;
      if ((uint64_t)fh->position() != fOff) fh->seek(fOff);
      fh->read(buf, GK_SECTOR_SIZE);
      return;
    }
  }
}

static void gkFatWriteSector(uint32_t lba, const uint8_t* data) {
  if (!gkFat.valid || lba < gkFat.dataStart) return;
  uint32_t ds = lba - gkFat.dataStart;
  uint32_t cl = (ds / GK_FAT_SPC) + 2, sc = ds % GK_FAT_SPC;

  // Root directory cluster: update shadow (WP=OFF only)
  if (cl == GK_FAT_ROOTCLU) {
    if (cfg.gotekFatWP == 0 && gkShadowRootOk && gkShadowRoot) {
      memcpy(gkShadowRoot + sc * GK_SECTOR_SIZE, data, GK_SECTOR_SIZE);
      // Log any deletions for debug
      const uint8_t* s = data;
      for (int e = 0; e < 16; e++) {
        if (s[e*32] == 0xE5 && s[e*32+11] != 0x0F)
          dualPrint.printf("[GOTEK FAT] Shadow: slot deleted (entry %d)\n", (int)(sc*16+e));
      }
    }
    return;
  }

  // Data clusters: find which image file this maps to
  for (int i = 0; i < gkFC; i++) {
    uint32_t cs = gkFat.startCluster[i], ce = cs + gkFat.clusterCount[i];
    if (cl < cs || cl >= ce) continue;
    uint32_t sf = (cl - cs) * GK_FAT_SPC + sc;
    if (sf >= gkF[i].sectors) return;
    // Note: no zero-detection here — GoTek legitimately writes zero sectors
    // (empty floppy sectors). Full Format with WP=OFF will corrupt image data
    // (user must explicitly choose Full Format — Quick Format is safe).
    if (gkCacheSlot == i) gkCacheInvalidate();
    uint64_t fOff = (uint64_t)sf * GK_SECTOR_SIZE;
    File wfh = SD_MMC.open(gkF[i].path, "r+"); if (!wfh) return;
    wfh.seek(fOff); wfh.write(data, GK_SECTOR_SIZE); wfh.flush(); wfh.close();
    return;
  }
}


// ── Build slot table from SD card directory ───────────────────────────────────
static void gkBuildFS() {
  gkFC = 0; gkRdy = false; gkCurSlot = -1;
  gkCloseHandles();
  gkCacheInvalidate();
  gkShadowRootOk = false;  // shadow will be rebuilt after gkFatBuild()

  if (!sdReady) {
    dualPrint.println(F("[GOTEK] SD not ready"));
    gkRdy = true; return;
  }

  if (!SD_MMC.exists(gotekImgDir.c_str()))
    SD_MMC.mkdir(gotekImgDir.c_str());

  File dir = SD_MMC.open(gotekImgDir.c_str());
  if (!dir || !dir.isDirectory()) { gkRdy = true; return; }

  // Collect supported image filenames
  String names[GK_MAX_FILES]; int nc = 0;
  File f = dir.openNextFile();
  while (f && nc < GK_MAX_FILES) {
    if (!f.isDirectory()) {
      String n = String(f.name()), nl = n; nl.toLowerCase();
      if (nl.endsWith(".img")||nl.endsWith(".ima")||nl.endsWith(".adf")||
          nl.endsWith(".hfe")||nl.endsWith(".dsk")||nl.endsWith(".st") ||
          nl.endsWith(".d81")||nl.endsWith(".d64"))
        names[nc++] = n;
    }
    f = dir.openNextFile();
  }
  dir.close();

  // Sort: try to load order.json first; fall back to alphabetical
  // order.json format: ["file1.img","file2.img",...] — defines slot assignment
  String orderPath = gotekImgDir + "/order.json";
  String orderJson = "";
  {
    File of = SD_MMC.open(orderPath.c_str(), FILE_READ);
    if (of) { orderJson = of.readString(); of.close(); }
  }

  if (orderJson.length() > 2) {
    // Parse JSON array of filenames and reorder names[] accordingly
    String ordered[GK_MAX_FILES]; int oc = 0;
    bool used[GK_MAX_FILES] = {};
    // Extract each quoted string from the JSON array
    int pos = 0;
    while (pos < (int)orderJson.length() && oc < GK_MAX_FILES) {
      int q1 = orderJson.indexOf('"', pos); if (q1 < 0) break;
      int q2 = orderJson.indexOf('"', q1+1); if (q2 < 0) break;
      String fn = orderJson.substring(q1+1, q2);
      pos = q2+1;
      // Find this filename in names[]
      for (int i = 0; i < nc; i++) {
        if (!used[i] && names[i].equalsIgnoreCase(fn)) {
          ordered[oc++] = names[i]; used[i] = true; break;
        }
      }
    }
    // Append any files not in order.json (new files added after last save)
    for (int i = 0; i < nc; i++)
      if (!used[i] && oc < GK_MAX_FILES) ordered[oc++] = names[i];
    for (int i = 0; i < oc; i++) names[i] = ordered[i];
    nc = oc;
    dualPrint.println(F("[GOTEK] Using custom slot order from order.json"));
  } else {
    // Alphabetical fallback
    for (int i=0;i<nc-1;i++)
      for (int j=0;j<nc-1-i;j++)
        if(names[j]>names[j+1]){String t=names[j];names[j]=names[j+1];names[j+1]=t;}
    dualPrint.println(F("[GOTEK] No order.json — using alphabetical order"));
  }

  // Assign slot numbers positionally — slot 0 = first file in order, slot 1 = second, etc.
  // Order comes from order.json (if exists) or alphabetical fallback.
  // Custom filenames (Test.img, Work.img) are fully supported.
  for (int i = 0; i < nc; i++) {
    String fp = gotekImgDir + "/" + names[i];
    File fi = SD_MMC.open(fp.c_str(), FILE_READ);
    if (!fi) { Serial.printf("[GOTEK] WARN: cannot open %s — skipping\n", names[i].c_str()); continue; }
    uint32_t sz = (uint32_t)fi.size(); fi.close();
    if (!sz) { Serial.printf("[GOTEK] WARN: %s is empty — skipping\n", names[i].c_str()); continue; }

    GotekFile& gf = gkF[gkFC];
    strlcpy(gf.path, fp.c_str(), sizeof(gf.path));
    gf.sz       = sz;
    gf.sectors  = (sz + GK_SECTOR_SIZE - 1) / GK_SECTOR_SIZE;
    gf.slotNum  = (uint32_t)gkFC;               // slot = position in list
    gf.startLba = (uint32_t)gkFC * GK_SLOT_STRIDE;

    dualPrint.printf("[GOTEK] Slot %03u: %-24s  %lu B  LBA %lu\n",
      gkFC, names[i].c_str(), (unsigned long)sz, (unsigned long)gf.startLba);

    if (gf.sectors > GK_SLOT_STRIDE)
      dualPrint.printf("[GOTEK] WARN: Slot %03u image (%lu sectors) > GK_SLOT_STRIDE (%u)!\n",
        gkFC, (unsigned long)gf.sectors, (unsigned)GK_SLOT_STRIDE);
    gkFC++;
  }
  gkRdy = true;
  dualPrint.printf("[GOTEK] %d slot(s) ready. Max LBA: %u\n", gkFC, gkFC * GK_SLOT_STRIDE);
  // Build FAT virtual filesystem layout (used only when gotekUsbMode == 1)
  gkFatBuild();
  // Open persistent per-slot file handles AFTER slot table is built
  gkOpenHandles();
}

// ── USB MSC init — called AFTER gkBuildFS so block count is known ────────────
// Returns correct USB MSC block count for current GoTek USB mode
static uint32_t gkBlockCount() {
  if (gkRuntimeUsbMode == 1 && gkFat.valid) return gkFat.totalSectors;
  return GK_MAX_SECTORS;
}

void initUSBGoTek() {
  // ── Unique USB serial number per device ────────────────────────────────────
  // Windows caches USB device driver by VID+PID+SerialNumber.
  // Using a MAC-based serial ensures Windows has NEVER seen this combination as
  // CD-ROM, so it enumerates fresh, reads block_size=512 → installs Disk driver.
  uint64_t _mac = ESP.getEfuseMac();
  char sn[24]; snprintf(sn, sizeof(sn), "GK-%06llX", (unsigned long long)(_mac & 0xFFFFFF));
  USB.serialNumber(sn);
  USB.productName("VirtualFloppy");
  USB.manufacturerName("ESP32-S3");
  USB.PID(0x8020);
  msc.vendorID("ESP32-S3");
  msc.productID("VirtualFlop");
  msc.productRevision("1.00");
  msc.onRead(gotekReadCb);
  msc.onWrite(gotekWriteCb);
  msc.onStartStop(gotekStartStopCb);
  gkRuntimeUsbMode = cfg.gotekUsbMode;  // sync runtime from config
  // Write-protect only in FAT32 virtual multi-slot mode with WP enabled.
  // Single-slot (raw) mode is always writable regardless of gotekFatWP.
  msc.isWritable(gkRuntimeUsbMode != 1 || cfg.gotekFatWP == 0);
  msc.begin(gkBlockCount(), GK_SECTOR_SIZE);
  msc.mediaPresent(true);
  USB.begin();
  dualPrint.printf("[GOTEK] USB serial: %s  PID: 0x8020  block_size: %u\n", sn, GK_SECTOR_SIZE);
  dualPrint.printf("[GOTEK] Virtual disk: %u sectors = %.1f MB\n",
    GK_MAX_SECTORS, (float)GK_MAX_SECTORS*GK_SECTOR_SIZE/1048576.0f);
  dualPrint.printf("[GOTEK] Image dir: %s  Slots: %d\n", gotekImgDir.c_str(), gkFC);
}

// ── MSC callbacks ─────────────────────────────────────────────────────────────

static bool gotekStartStopCb(uint8_t, bool, bool) { return true; }

// ── LBA resolution helper ─────────────────────────────────────────────────────
// In single-slot mode (gkSelectedSlot >= 0): LBA maps directly to that image.
// In raw multi-slot mode (gkSelectedSlot < 0): LBA maps across concatenated images.
static inline int gkResolveLba(uint32_t lba, uint64_t& fOff) {
  int sel = gkSelectedSlot;  // slotNum value, not array index
  if (sel >= 0) {
    // Single-slot mode (Windows direct access): find array entry by slotNum
    for (int fi = 0; fi < gkFC; fi++) {
      if ((int)gkF[fi].slotNum == sel) {
        fOff = (uint64_t)lba * GK_SECTOR_SIZE;
        if (fOff >= (uint64_t)gkF[fi].sz) return -1;
        return fi;
      }
    }
    return -1;  // slot not found
  }
  // Multi-slot raw layout: slot N starts at LBA N×GK_SLOT_STRIDE
  for (int fi = 0; fi < gkFC; fi++) {
    uint32_t slotEnd = gkF[fi].startLba + GK_SLOT_STRIDE;
    if (lba < gkF[fi].startLba || lba >= slotEnd) continue;
    uint32_t relSec = lba - gkF[fi].startLba;
    if (relSec >= gkF[fi].sectors) return -1;
    fOff = (uint64_t)relSec * GK_SECTOR_SIZE;
    return fi;
  }
  return -1;
}


// ── MSC read callback ────────────────────────────────────────────────────────
static int32_t gotekReadCb(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  (void)offset;
  uint8_t* dst = (uint8_t*)buffer;
  uint32_t sec = bufsize / GK_SECTOR_SIZE;
  bool haveMtx = sdMutex && (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE);

  // FAT virtual filesystem mode (use gkRuntimeUsbMode — may differ from cfg in single-slot)
  if (gkRuntimeUsbMode == 1) {
    for (uint32_t s = 0; s < sec; s++) {
      gkFatReadSector(lba + s, dst + s * GK_SECTOR_SIZE);
      gkLastLba = lba + s;
    }
    if (haveMtx) xSemaphoreGive(sdMutex);
    return (int32_t)bufsize;
  }

  for (uint32_t s = 0; s < sec; s++) {
    uint8_t* sb = dst + s * GK_SECTOR_SIZE;
    if (!gkRdy) { memset(sb, 0, GK_SECTOR_SIZE); continue; }
    uint64_t fOff = 0;
    int fi = gkResolveLba(lba + s, fOff);

    if (fi < 0) {
      memset(sb, 0, GK_SECTOR_SIZE);
      gkLogPush(lba + s, -1, 'R', nullptr);
      if (cfg.debugMode)
        dualPrint.printf("[GK R] LBA %u UNMAPPED — GoTek reads outside all slots!\n", lba + s);
      continue;
    }

    gkCurSlot = fi; gkLastLba = lba + s;
    if (fi < GK_MAX_FILES) gkReadCnt[fi]++;

    uint32_t secInFile = (uint32_t)(fOff / GK_SECTOR_SIZE);

    // Try PSRAM cache first
    if (gkCacheRead(fi, secInFile, sb)) {
      gkLogPush(lba + s, fi, 'R', sb);
    } else {
      // Cache miss — fill window then retry
      gkCacheFill(fi, secInFile);
      if (!gkCacheRead(fi, secInFile, sb)) {
        // PSRAM unavailable — direct SD read fallback
        File* fhp = gkCacheGet(fi);
        if (!fhp) { memset(sb, 0, GK_SECTOR_SIZE); gkLogPush(lba + s, fi, 'R', sb); continue; }
        if ((uint64_t)fhp->position() != fOff) fhp->seek(fOff);
        uint32_t toRead = min((uint32_t)GK_SECTOR_SIZE,
                              (uint32_t)(gkF[fi].sz > fOff ? gkF[fi].sz - fOff : 0));
        int32_t got = toRead ? fhp->read(sb, toRead) : 0;
        if (got < (int32_t)GK_SECTOR_SIZE) memset(sb + got, 0, GK_SECTOR_SIZE - got);
      }
      gkLogPush(lba + s, fi, 'R', sb);
    }

    // Log critical sectors once per session (boot sector, FAT, root dir)
    // Runs regardless of cache hit/miss — sector data is in sb at this point
    static uint8_t _loggedSec[GK_MAX_FILES][3] = {};
    uint32_t relSec = secInFile;  // sector within image file
    int sidx = (relSec==0)?0:(relSec==1)?1:(relSec==19)?2:-1;
    if (sidx >= 0 && fi < GK_MAX_FILES && !_loggedSec[fi][sidx]) {
      _loggedSec[fi][sidx] = 1;
      if (relSec == 0)
        dualPrint.printf("[GK] Slot %d boot sector OK: OEM=%.8s TotSec=%u Media=%02X\n",
          fi, (char*)&sb[3], (uint16_t)sb[19]|((uint16_t)sb[20]<<8), sb[21]);
      else if (relSec == 1)
        dualPrint.printf("[GK] Slot %d FAT1[0-2]: %02X %02X %02X  %s\n",
          fi, sb[0],sb[1],sb[2], (sb[0]==0xF0&&sb[1]==0xFF&&sb[2]==0xFF)?"OK":"WARN: invalid FAT!");
      else {
        bool hasEntry = (sb[0]!=0x00 && sb[0]!=0xE5);
        dualPrint.printf("[GK] Slot %d root dir: %s\n",
          fi, hasEntry ? "has files" : "EMPTY — host may ask to format");
        if (hasEntry) {
          char name[12]={}; memcpy(name, sb, 11);
          dualPrint.printf("[GK] Slot %d first entry: '%.11s' size=%lu\n",
            fi, name, (uint32_t)sb[28]|((uint32_t)sb[29]<<8)|((uint32_t)sb[30]<<16)|((uint32_t)sb[31]<<24));
        }
      }
    }
    if (cfg.debugMode && relSec == 0)
      dualPrint.printf("[GK R] Slot %d boot (LBA %u): %02X%02X%02X\n", fi, lba+s, sb[0],sb[1],sb[2]);
  }
  if (haveMtx) xSemaphoreGive(sdMutex);
  return (int32_t)bufsize;
}

static int32_t gotekWriteCb(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  (void)offset;
  if (!gkRdy) return (int32_t)bufsize;
  // Wait up to 2s for SD mutex — image editor operations can take ~500ms
  // If timeout, report error (return -1) so host retries rather than silently dropping
  if (!sdMutex || xSemaphoreTake(sdMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    dualPrint.printf("[GK W] WARN: mutex timeout — write LBA %u dropped!\n", lba);
    return -1;  // signal error to host so it retries
  }

  // FAT virtual filesystem mode — route writes to real files via FAT mapping
  if (gkRuntimeUsbMode == 1) {
    uint32_t sec = bufsize / GK_SECTOR_SIZE;
    for (uint32_t s = 0; s < sec; s++)
      gkFatWriteSector(lba + s, buffer + s * GK_SECTOR_SIZE);
    xSemaphoreGive(sdMutex);
    return (int32_t)bufsize;
  }

  uint32_t sec = bufsize / GK_SECTOR_SIZE;
  bool didWrite = false;

  for (uint32_t s = 0; s < sec; s++) {
    uint64_t fOff = 0;
    int fi = gkResolveLba(lba + s, fOff);
    if (fi < 0) {
      gkLogPush(lba + s, -1, 'W', nullptr);
      if (cfg.debugMode) dualPrint.printf("[GK W] LBA %u UNMAPPED write!\n", lba + s);
      continue;
    }
    if (fi >= 0 && fi < GK_MAX_FILES) gkWriteCnt[fi]++;
    gkLogPush(lba + s, fi, 'W', buffer + s * GK_SECTOR_SIZE);
    if (cfg.debugMode)
      dualPrint.printf("[GK W] LBA %u  slot %d  fOff %llu  b0-3: %02X%02X%02X%02X\n",
        lba+s, fi, (unsigned long long)fOff,
        buffer[s*GK_SECTOR_SIZE], buffer[s*GK_SECTOR_SIZE+1],
        buffer[s*GK_SECTOR_SIZE+2], buffer[s*GK_SECTOR_SIZE+3]);
    if (fOff >= (uint64_t)gkF[fi].sz) continue;
    // Invalidate PSRAM cache for this slot on write
    if (gkCacheSlot == fi) gkCacheInvalidate();
    // Open write handle on demand, write, close immediately — avoids FD exhaustion
    File wfh = SD_MMC.open(gkF[fi].path, "r+");
    if (!wfh) {
      dualPrint.printf("[GK W] ERROR: cannot open write handle slot %d\n", fi);
      continue;
    }
    wfh.seek(fOff);
    uint32_t toWrite = min((uint32_t)GK_SECTOR_SIZE,
                           (uint32_t)((uint64_t)gkF[fi].sz > fOff ? (uint64_t)gkF[fi].sz - fOff : 0));
    if (toWrite) { wfh.write(buffer + s * GK_SECTOR_SIZE, toWrite); didWrite = true; }
    wfh.flush(); wfh.close();
  }
  if (didWrite) { /* individual handles flushed and closed above */ }
  xSemaphoreGive(sdMutex);
  return (int32_t)bufsize;
}

static void gkRefresh() {
  bool haveMtx = sdMutex && (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) == pdTRUE);
  gkBuildFS();
  if (haveMtx) xSemaphoreGive(sdMutex);
  dualPrint.println(F("[GOTEK] Slot table refreshed."));
}


// =============================================================================
//  FAT12 IMAGE EDITOR — session-based, in-memory FAT cache
//  Reading/writing individual FAT entries via SD seek() is ~100x slower than
//  keeping the FAT table (≤4608 bytes for 1.44 MB) in RAM.
//  All operations open a session: load FAT → operate in RAM → write FAT back once.
// =============================================================================

// SD mutex helpers for GoTek web handlers (forward-declared here, defined after upload state)
static inline bool gkMtxTake();
static void gkInvalidateForPath(const String& imgPath);  // forward decl
static inline void gkMtxGive();

// ── Session globals ───────────────────────────────────────────────────────────
static File     g_sFile;                // currently open .img file
static ImgBPB   g_sBPB;                // parsed BPB for current session
static uint8_t* g_fatBuf  = nullptr;   // heap-allocated in-RAM FAT copy
static uint32_t g_fatBytes = 0;
static bool     g_fatDirty = false;
static bool     g_sOpen    = false;
static String   g_sImgPath = "";       // path of currently open image (for cache invalidation)
static uint8_t  g_secBuf[512];         // shared sector read/write buffer

static bool imgParseBPB(File& f, ImgBPB& b) {
  uint8_t s[512];
  f.seek(0);
  if (f.read(s, 512) != 512) return false;
  if (s[510] != 0x55 || s[511] != 0xAA) return false;
  b.bps   = (uint16_t)s[11] | ((uint16_t)s[12]<<8);
  b.spc   = s[13];
  b.rsv   = (uint16_t)s[14] | ((uint16_t)s[15]<<8);
  b.nfat  = s[16];
  b.rde   = (uint16_t)s[17] | ((uint16_t)s[18]<<8);
  b.fatsz = (uint16_t)s[22] | ((uint16_t)s[23]<<8);
  uint32_t t16 = (uint16_t)s[19]|((uint16_t)s[20]<<8);
  uint32_t t32 = (uint32_t)s[32]|((uint32_t)s[33]<<8)|((uint32_t)s[34]<<16)|((uint32_t)s[35]<<24);
  b.totSec  = t16 ? t16 : t32;
  if (!b.bps || !b.spc || !b.nfat || !b.fatsz) return false;
  b.fat1    = b.rsv;
  b.rootLBA = b.fat1 + (uint32_t)b.nfat * b.fatsz;
  b.rootSec = ((uint32_t)b.rde * 32 + b.bps - 1) / b.bps;
  b.dataLBA = b.rootLBA + b.rootSec;
  b.dataClu = (b.totSec > b.dataLBA) ? (b.totSec - b.dataLBA) / b.spc : 0;
  return true;
}

static inline uint32_t imgCluLBA(const ImgBPB& b, uint16_t clu) {
  return b.dataLBA + (uint32_t)(clu - 2) * b.spc;
}

// Open session: load entire FAT into RAM
static bool imgOpenSession(const String& imgPath) {
  if (g_sOpen) { // close previous
    if (g_fatDirty && g_fatBuf)
      for (int fi=0; fi<g_sBPB.nfat; fi++) {
        g_sFile.seek((uint64_t)(g_sBPB.fat1+(uint32_t)fi*g_sBPB.fatsz)*g_sBPB.bps);
        g_sFile.write(g_fatBuf, g_fatBytes);
      }
    if (g_fatBuf) { free(g_fatBuf); g_fatBuf=nullptr; }
    g_sFile.close(); g_sOpen=false;
  }
  g_sFile = SD_MMC.open(imgPath.c_str(), "r+");
  if (!g_sFile) return false;
  g_sImgPath = imgPath;
  if (!imgParseBPB(g_sFile, g_sBPB)) { g_sFile.close(); return false; }
  g_fatBytes = (uint32_t)g_sBPB.fatsz * g_sBPB.bps;
  g_fatBuf = (uint8_t*)malloc(g_fatBytes);
  if (!g_fatBuf) { g_sFile.close(); return false; }
  g_sFile.seek((uint64_t)g_sBPB.fat1 * g_sBPB.bps);
  g_sFile.read(g_fatBuf, g_fatBytes);
  g_fatDirty = false; g_sOpen = true;
  return true;
}

// Close session: flush FAT if dirty
static void imgCloseSession() {
  if (!g_sOpen) return;
  if (g_fatDirty && g_fatBuf)
    for (int fi=0; fi<g_sBPB.nfat; fi++) {
      g_sFile.seek((uint64_t)(g_sBPB.fat1+(uint32_t)fi*g_sBPB.fatsz)*g_sBPB.bps);
      g_sFile.write(g_fatBuf, g_fatBytes);
    }
  if (g_fatBuf) { free(g_fatBuf); g_fatBuf=nullptr; }
  g_sFile.close(); g_sOpen=false; g_fatDirty=false;
}

// In-RAM FAT12 access — O(1) per operation
static uint16_t imgFat12GetM(uint16_t clu) {
  uint32_t off=(uint32_t)clu*3/2;
  if (off+1>=g_fatBytes) return 0;
  uint16_t v=(uint16_t)g_fatBuf[off]|((uint16_t)g_fatBuf[off+1]<<8);
  return (clu&1)?(v>>4):(v&0x0FFF);
}
static void imgFat12SetM(uint16_t clu, uint16_t val) {
  uint32_t off=(uint32_t)clu*3/2;
  if (off+1>=g_fatBytes) return;
  if (clu&1){g_fatBuf[off]=(g_fatBuf[off]&0x0F)|((uint8_t)(val&0x0F)<<4);g_fatBuf[off+1]=(uint8_t)(val>>4);}
  else      {g_fatBuf[off]=(uint8_t)(val&0xFF);g_fatBuf[off+1]=(g_fatBuf[off+1]&0xF0)|((uint8_t)(val>>8)&0x0F);}
  g_fatDirty=true;
}
static uint16_t imgFindFreeM() {
  for (uint16_t c=2; c<(uint16_t)(g_sBPB.dataClu+2)&&c<0xFF8; c++)
    if (imgFat12GetM(c)==0) return c;
  return 0;
}
static void imgFreeChainM(uint16_t clu) {
  while (clu>=2&&clu<0xFF8){uint16_t n=imgFat12GetM(clu);imgFat12SetM(clu,0);clu=n;}
}
static uint16_t imgFreeClustersM() {
  uint16_t cnt=0;
  for (uint16_t c=2;c<(uint16_t)(g_sBPB.dataClu+2)&&c<0xFF8;c++) if(imgFat12GetM(c)==0) cnt++;
  return cnt;
}

// Build 8.3 name from dir entry bytes
static void imgEntry83Name(const uint8_t* de, char* out) {
  bool isDir=(de[11]&0x10)!=0; int ni=0;
  for (int k=0;k<8&&de[k]!=' ';k++){char c=de[k];if(c==0x05)c=0xE5;out[ni++]=(char)tolower((unsigned char)c);}
  if (!isDir&&de[8]!=' '){out[ni++]='.';for(int k=8;k<11&&de[k]!=' ';k++)out[ni++]=(char)tolower((unsigned char)de[k]);}
  out[ni]=0;
}

// Scan directory at given LBA (sector-at-a-time for speed)
static String imgScanDirAt(uint32_t lba, uint16_t count) {
  String j="["; bool first=true;
  uint16_t ePerSec=g_sBPB.bps/32;
  uint16_t sectors=(count+ePerSec-1)/ePerSec;
  for (uint16_t s=0; s<sectors; s++) {
    g_sFile.seek((uint64_t)(lba+s)*g_sBPB.bps);
    g_sFile.read(g_secBuf, g_sBPB.bps);
    for (uint16_t e=0; e<ePerSec; e++) {
      uint16_t idx=s*ePerSec+e; if(idx>=count) return j+"]";
      uint8_t* de=g_secBuf+e*32;
      if (de[0]==0x00) return j+"]";
      // Skip: deleted, LFN, volume-label, dot-entries, and UTF-16LE garbage
      // (UTF-16LE stores null bytes in name positions 1-7; valid 8.3 names never have 0x00 there)
      if (de[0]==0xE5 || de[11]==0x0F || (de[11]&0x08) || de[0]=='.') continue;
      if (de[1]==0x00 || de[3]==0x00 || de[5]==0x00) continue;  // UTF-16LE guard
      bool isDir=(de[11]&0x10)!=0;
      char name[13]; imgEntry83Name(de,name);
      uint32_t sz=(uint32_t)de[28]|((uint32_t)de[29]<<8)|((uint32_t)de[30]<<16)|((uint32_t)de[31]<<24);
      uint16_t fc=(uint16_t)de[26]|((uint16_t)de[27]<<8);
      if(!first)j+=","; first=false;
      j+="{\"n\":\""+String(name)+"\",\"d\":"+(isDir?"true":"false")+",\"s\":"+String(sz)+",\"c\":"+String(fc)+"}";
    }
  }
  return j+"]";
}

// Scan dir by cluster chain (0=root)
static String imgScanDir(uint16_t clu) {
  if (clu<2) return imgScanDirAt(g_sBPB.rootLBA, g_sBPB.rde);
  String j="["; bool first=true;
  while (clu>=2&&clu<0xFF8) {
    String part=imgScanDirAt(imgCluLBA(g_sBPB,clu),(uint16_t)(g_sBPB.spc*g_sBPB.bps/32));
    String inner=part.length()>2?part.substring(1,part.length()-1):"";
    if(inner.length()){if(!first)j+=",";j+=inner;first=false;}
    clu=imgFat12GetM(clu);
  }
  return j+"]";
}

// Navigate path string → cluster number (0=root, 0xFFFF=not found)
static uint16_t imgNavPath(const String& path) {
  if (path=="/"||path=="") return 0;
  String p=path; if(p[0]=='/')p=p.substring(1);
  uint16_t curClu=0;
  while (p.length()) {
    int sl=p.indexOf('/'); String part=(sl<0)?p:p.substring(0,sl); p=(sl<0)?"":p.substring(sl+1);
    part.toUpperCase();
    bool found=false;
    uint16_t scanClu=curClu;
    do {
      uint32_t scanLBA=(scanClu<2)?g_sBPB.rootLBA:imgCluLBA(g_sBPB,scanClu);
      uint16_t scanCnt=(scanClu<2)?g_sBPB.rde:(uint16_t)(g_sBPB.spc*g_sBPB.bps/32);
      uint16_t ePerSec=g_sBPB.bps/32;
      for (uint16_t s=0; s<(scanCnt+ePerSec-1)/ePerSec&&!found; s++) {
        g_sFile.seek((uint64_t)(scanLBA+s)*g_sBPB.bps); g_sFile.read(g_secBuf,g_sBPB.bps);
        for (uint16_t e=0;e<ePerSec;e++){
          uint16_t idx=s*ePerSec+e; if(idx>=scanCnt||g_secBuf[e*32]==0) goto nextClu;
          uint8_t*de=g_secBuf+e*32;
          if(de[0]==0xE5||de[11]==0x0F||(de[11]&0x08)||!(de[11]&0x10)) continue;
          if(de[1]==0x00||de[3]==0x00||de[5]==0x00) continue; // UTF-16LE guard
          char nm[13]; imgEntry83Name(de,nm); String nmU=String(nm);nmU.toUpperCase();
          if(nmU==part){curClu=(uint16_t)de[26]|((uint16_t)de[27]<<8);found=true;break;}
        }
      }
      nextClu:
      if(found||scanClu<2) break;
      scanClu=imgFat12GetM(scanClu);
    } while(scanClu>=2&&scanClu<0xFF8);
    if(!found) return 0xFFFF;
  }
  return curClu;
}

// Find dir entry by name → byte offset in image, or -1
static int64_t imgFindEntry(uint16_t dirClu, const String& name83) {
  String target=name83; target.toUpperCase();
  uint16_t clu=dirClu;
  do {
    uint32_t lba=(clu<2)?g_sBPB.rootLBA:imgCluLBA(g_sBPB,clu);
    uint16_t cnt=(clu<2)?g_sBPB.rde:(uint16_t)(g_sBPB.spc*g_sBPB.bps/32);
    uint16_t ePerSec=g_sBPB.bps/32;
    for (uint16_t s=0;s<(cnt+ePerSec-1)/ePerSec;s++) {
      g_sFile.seek((uint64_t)(lba+s)*g_sBPB.bps); g_sFile.read(g_secBuf,g_sBPB.bps);
      for (uint16_t e=0;e<ePerSec;e++){
        uint16_t idx=s*ePerSec+e; if(idx>=cnt) return -1;
        uint8_t*de=g_secBuf+e*32;
        if(de[0]==0x00) return -1;
        if(de[0]==0xE5||de[11]==0x0F||(de[11]&0x08)) continue;
        if(de[1]==0x00||de[3]==0x00||de[5]==0x00) continue; // UTF-16LE guard
        char nm[13]; imgEntry83Name(de,nm); String nmU=String(nm);nmU.toUpperCase();
        if(nmU==target) return (int64_t)(lba+s)*g_sBPB.bps+(int64_t)e*32;
      }
    }
    if(clu<2) break;
    clu=imgFat12GetM(clu);
  } while(clu>=2&&clu<0xFF8);
  return -1;
}

// Find free dir slot → byte offset or -1
static int64_t imgFindFreeSlot(uint16_t dirClu) {
  uint16_t clu=dirClu;
  do {
    uint32_t lba=(clu<2)?g_sBPB.rootLBA:imgCluLBA(g_sBPB,clu);
    uint16_t cnt=(clu<2)?g_sBPB.rde:(uint16_t)(g_sBPB.spc*g_sBPB.bps/32);
    uint16_t ePerSec=g_sBPB.bps/32;
    for (uint16_t s=0;s<(cnt+ePerSec-1)/ePerSec;s++){
      g_sFile.seek((uint64_t)(lba+s)*g_sBPB.bps); g_sFile.read(g_secBuf,g_sBPB.bps);
      for (uint16_t e=0;e<ePerSec;e++){
        uint16_t idx=s*ePerSec+e; if(idx>=cnt) return -1;
        uint8_t f=g_secBuf[e*32];
        if(f==0x00||f==0xE5) return (int64_t)(lba+s)*g_sBPB.bps+(int64_t)e*32;
      }
    }
    if(clu<2) break;
    clu=imgFat12GetM(clu);
  } while(clu>=2&&clu<0xFF8);
  return -1;
}

// Build 8.3 dir entry (32 bytes)
static void imgMakeDirEntry(uint8_t* de, const char* name83, uint32_t size, uint16_t firstClu, bool isDir) {
  memset(de,0,32);
  const char* dot=strchr(name83,'.');
  int nlen=dot?(int)(dot-name83):(int)strlen(name83);
  for(int i=0;i<8;i++) de[i]=' '; for(int i=0;i<3;i++) de[8+i]=' ';
  for(int i=0;i<nlen&&i<8;i++) de[i]=(uint8_t)toupper((unsigned char)name83[i]);
  if(dot){dot++;for(int i=0;i<3&&dot[i];i++) de[8+i]=(uint8_t)toupper((unsigned char)dot[i]);}
  de[11]=isDir?0x10:0x20;
  de[22]=0x00;de[23]=0x00;de[24]=0x21;de[25]=0x5A; // write date 2025-01-01
  de[26]=(uint8_t)firstClu;de[27]=(uint8_t)(firstClu>>8);
  de[28]=(uint8_t)size;de[29]=(uint8_t)(size>>8);de[30]=(uint8_t)(size>>16);de[31]=(uint8_t)(size>>24);
}

// ── FAT12 API handlers ────────────────────────────────────────────────────────

// Create directory inside a .img file
// GET /api/img/mkdir?img=&dir=&name=
void handleImgMkdir() {
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(503,"application/json","{\"error\":\"SD not ready\"}"); return; }
  String imgPath = httpServer.hasArg("img")  ? httpServer.arg("img")  : "";
  String dirPath = httpServer.hasArg("dir")  ? httpServer.arg("dir")  : "/";
  String name    = httpServer.hasArg("name") ? httpServer.arg("name") : "";
  name.toUpperCase();
  if (!imgPath.length()||!name.length()||name.length()>8) {
    httpServer.send(400,"application/json","{\"error\":\"Missing/invalid params\"}"); return;
  }
  if (!gkMtxTake()) { httpServer.send(503,"application/json","{\"error\":\"SD busy\"}"); return; }
  if (!imgOpenSession(imgPath)) {
    gkMtxGive(); httpServer.send(400,"application/json","{\"error\":\"Bad image\"}"); return;
  }
  uint16_t dirClu = imgNavPath(dirPath);
  if (dirClu==0xFFFF) {
    imgCloseSession(); gkMtxGive();
    httpServer.send(404,"application/json","{\"error\":\"Parent dir not found\"}"); return;
  }
  if (imgFindEntry(dirClu,name)>=0) {
    imgCloseSession(); gkMtxGive();
    httpServer.send(409,"application/json","{\"error\":\"Already exists\"}"); return;
  }
  // Allocate one cluster for new directory
  uint16_t nc = imgFindFreeM();
  if (!nc) {
    imgCloseSession(); gkMtxGive();
    httpServer.send(507,"application/json","{\"error\":\"Disk full\"}"); return;
  }
  imgFat12SetM(nc, 0x0FFF);
  // Zero the cluster and write . and .. entries
  uint32_t lba = imgCluLBA(g_sBPB, nc);
  memset(g_secBuf, 0, g_sBPB.bps);
  for (uint8_t s=0; s<g_sBPB.spc; s++) {
    g_sFile.seek((uint64_t)(lba+s)*g_sBPB.bps);
    g_sFile.write(g_secBuf, g_sBPB.bps);
  }
  // Write . entry
  g_sFile.seek((uint64_t)lba*g_sBPB.bps);
  uint8_t de[32]; memset(de,0x20,11); de[0]='.';
  memset(de+11,0,21); de[11]=0x10;
  de[24]=0x21; de[25]=0x5A;  // date 2025-01-01
  de[26]=(uint8_t)nc; de[27]=(uint8_t)(nc>>8);
  g_sFile.write(de,32);
  // Write .. entry
  memset(de,0x20,11); de[0]='.'; de[1]='.';
  de[11]=0x10; de[24]=0x21; de[25]=0x5A;
  uint16_t parentClu=(dirClu<2)?0:dirClu;
  de[26]=(uint8_t)parentClu; de[27]=(uint8_t)(parentClu>>8);
  g_sFile.write(de,32);
  // Create dir entry in parent
  int64_t slot=imgFindFreeSlot(dirClu);
  if (slot<0) {
    imgFat12SetM(nc,0);
    imgCloseSession(); gkMtxGive();
    httpServer.send(507,"application/json","{\"error\":\"Parent dir full\"}"); return;
  }
  uint8_t dirEntry[32];
  imgMakeDirEntry(dirEntry, name.c_str(), 0, nc, true);
  g_sFile.seek((uint64_t)slot); g_sFile.write(dirEntry,32);
  imgCloseSession();
  gkMtxGive();
  gkInvalidateForPath(imgPath);
  httpServer.send(200,"application/json","{\"ok\":true}");
}

void handleImgLs() {
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(503,"application/json","{\"error\":\"SD not ready\"}"); return; }
  String imgPath=httpServer.hasArg("img")?httpServer.arg("img"):"";
  String dir=httpServer.hasArg("dir")?httpServer.arg("dir"):"/";
  if (!imgPath.length()){httpServer.send(400,"application/json","{\"error\":\"Missing img\"}");return;}
  if (!gkMtxTake()){httpServer.send(503,"application/json","{\"error\":\"SD busy\"}");return;}
  if (!imgOpenSession(imgPath)){gkMtxGive();httpServer.send(400,"application/json","{\"error\":\"Bad FAT12 image\"}");return;}
  uint16_t clu=imgNavPath(dir);
  String entries=(clu==0xFFFF)?"[]":imgScanDir(clu);
  uint16_t freeClu=imgFreeClustersM();
  ImgBPB snap=g_sBPB;  // snapshot before close
  imgCloseSession();
  gkMtxGive();
  httpServer.send(200,"application/json",
    "{\"ok\":true,\"dir\":\""+dir+"\",\"bps\":"+String(snap.bps)
    +",\"spc\":"+String(snap.spc)+",\"total\":"+String(snap.totSec)
    +",\"free_clusters\":"+String(freeClu)+",\"files\":"+entries+"}");
}

void handleImgRm() {
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(503,"application/json","{\"error\":\"SD not ready\"}"); return; }
  String imgPath=httpServer.hasArg("img")?httpServer.arg("img"):"";
  String fpath=httpServer.hasArg("file")?httpServer.arg("file"):"";
  if (!imgPath.length()||!fpath.length()){httpServer.send(400,"application/json","{\"error\":\"Missing params\"}");return;}
  if (!gkMtxTake()){httpServer.send(503,"application/json","{\"error\":\"SD busy\"}");return;}
  if (!imgOpenSession(imgPath)){gkMtxGive();httpServer.send(400,"application/json","{\"error\":\"Bad image\"}");return;}
  String parent="/",name83=fpath;
  int sl=fpath.lastIndexOf('/');
  if(sl>0){parent=fpath.substring(0,sl);name83=fpath.substring(sl+1);}
  else if(sl==0) name83=fpath.substring(1);
  uint16_t dirClu=imgNavPath(parent);
  if(dirClu==0xFFFF){imgCloseSession();gkMtxGive();httpServer.send(404,"application/json","{\"error\":\"Dir not found\"}");return;}
  int64_t entOff=imgFindEntry(dirClu,name83);
  if(entOff<0){imgCloseSession();gkMtxGive();httpServer.send(404,"application/json","{\"error\":\"File not found\"}");return;}
  g_sFile.seek((uint64_t)entOff);
  uint8_t de[32]; g_sFile.read(de,32);
  uint16_t fc=(uint16_t)de[26]|((uint16_t)de[27]<<8);
  uint8_t del=0xE5; g_sFile.seek((uint64_t)entOff); g_sFile.write(&del,1);
  if(fc>=2) imgFreeChainM(fc);
  imgCloseSession();
  gkMtxGive();
  gkInvalidateForPath(imgPath);
  httpServer.send(200,"application/json","{\"ok\":true}");
}

// Download a file from inside a .img image
void handleImgGet() {
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(503,"application/json","{\"error\":\"SD not ready\"}"); return; }
  String imgPath=httpServer.hasArg("img")?httpServer.arg("img"):"";
  String fpath=httpServer.hasArg("file")?httpServer.arg("file"):"";
  if (!imgPath.length()||!fpath.length()){httpServer.send(400,"text/plain","Missing params");return;}
  if (!gkMtxTake()){httpServer.send(503,"text/plain","SD busy");return;}
  if (!imgOpenSession(imgPath)){gkMtxGive();httpServer.send(400,"text/plain","Bad image");return;}
  String parent="/",name83=fpath;
  int sl=fpath.lastIndexOf('/');
  if(sl>0){parent=fpath.substring(0,sl);name83=fpath.substring(sl+1);}
  else if(sl==0) name83=fpath.substring(1);
  uint16_t dirClu=imgNavPath(parent);
  if(dirClu==0xFFFF){imgCloseSession();gkMtxGive();httpServer.send(404,"text/plain","Dir not found");return;}
  int64_t entOff=imgFindEntry(dirClu,name83);
  if(entOff<0){imgCloseSession();gkMtxGive();httpServer.send(404,"text/plain","File not found");return;}
  g_sFile.seek((uint64_t)entOff);
  uint8_t de[32]; g_sFile.read(de,32);
  uint32_t fileSize=(uint32_t)de[28]|((uint32_t)de[29]<<8)|((uint32_t)de[30]<<16)|((uint32_t)de[31]<<24);
  uint16_t firstClu=(uint16_t)de[26]|((uint16_t)de[27]<<8);
  httpServer.sendHeader("Content-Disposition","attachment; filename=\""+name83+"\"");
  httpServer.setContentLength(fileSize);
  httpServer.send(200,"application/octet-stream","");
  WiFiClient client=httpServer.client();
  uint16_t clu=firstClu;
  uint32_t remaining=fileSize;
  while (clu>=2&&clu<0xFF8&&remaining>0) {
    uint32_t lba=imgCluLBA(g_sBPB,clu);
    for (uint8_t s=0;s<g_sBPB.spc&&remaining>0;s++) {
      g_sFile.seek((uint64_t)(lba+s)*g_sBPB.bps);
      uint32_t toSend=min(remaining,(uint32_t)g_sBPB.bps);
      g_sFile.read(g_secBuf,toSend);
      client.write(g_secBuf,toSend);
      remaining-=toSend;
    }
    clu=imgFat12GetM(clu);
  }
  imgCloseSession();
  gkMtxGive();
}

// ── Upload state (uses session FAT cache) ─────────────────────────────────────
static uint16_t _iuDir   = 0;
static uint16_t _iuFirst = 0;
static uint16_t _iuPrev  = 0;
static uint32_t _iuWrit  = 0;
static uint8_t  _iuSec[512];
static uint32_t _iuSecOff= 0;
static uint8_t  _iuSIC   = 0;
static uint32_t _iuCluLBA= 0;
static String   _iuFile  = "";
static String   _iuImgPath = "";  // image path for cache invalidation on completion
static bool     _iuOk    = false;
static bool     _iuDone  = false;

static bool _iuFlushSector() {
  if (!_iuOk||!g_sOpen) return false;
  if (_iuSIC==0) {
    uint16_t fc=imgFindFreeM(); if(!fc){_iuOk=false;return false;}
    imgFat12SetM(fc,0x0FFF);
    if (_iuPrev) imgFat12SetM(_iuPrev,fc);
    if (!_iuFirst) _iuFirst=fc;
    _iuPrev=fc; _iuCluLBA=imgCluLBA(g_sBPB,fc);
  }
  g_sFile.seek((uint64_t)(_iuCluLBA+_iuSIC)*g_sBPB.bps);
  g_sFile.write(_iuSec,g_sBPB.bps);
  _iuSIC++; if(_iuSIC>=g_sBPB.spc) _iuSIC=0;
  _iuSecOff=0; return true;
}

void handleImgPut() {
  if (!checkAuth()) return;
  HTTPUpload& up=httpServer.upload();
  if (up.status==UPLOAD_FILE_START) {
    _iuOk=false;_iuDone=false;_iuFirst=0;_iuPrev=0;_iuWrit=0;_iuSecOff=0;_iuSIC=0;_iuCluLBA=0;
    String imgPath=httpServer.hasArg("img")?httpServer.arg("img"):"";
    _iuFile=httpServer.hasArg("file")?httpServer.arg("file"):"";
    _iuImgPath=imgPath;
    if(!imgPath.length()||!_iuFile.length()) return;
    if(!imgOpenSession(imgPath)){gkMtxGive();dualPrint.println(F("[IMG] Cannot open session"));return;}
    // ── Space pre-check: if Content-Length header present, reject early ────────
    if(httpServer.hasHeader("Content-Length")){
      uint32_t fileSize=(uint32_t)httpServer.header("Content-Length").toInt();
      uint16_t freeClu=imgFreeClustersM();
      uint32_t freeBytes=(uint32_t)freeClu*g_sBPB.spc*g_sBPB.bps;
      if(fileSize>freeBytes){
        imgCloseSession();gkMtxGive();
        dualPrint.printf("[IMG] Upload rejected: file %lu B > free %lu B\n",
          (unsigned long)fileSize,(unsigned long)freeBytes);
        return;  // handleImgPutDone will return error via _iuDone=false
      }
    }
    String parent="/"; int sl=_iuFile.lastIndexOf('/');
    if(sl>0){parent=_iuFile.substring(0,sl);_iuFile=_iuFile.substring(sl+1);}
    else if(sl==0) _iuFile=_iuFile.substring(1);
    _iuDir=imgNavPath(parent);
    if(_iuDir==0xFFFF){imgCloseSession();gkMtxGive();return;}
    int64_t ex=imgFindEntry(_iuDir,_iuFile);
    if(ex>=0){
      g_sFile.seek((uint64_t)ex);uint8_t de[32];g_sFile.read(de,32);
      uint16_t fc=(uint16_t)de[26]|((uint16_t)de[27]<<8);
      uint8_t del=0xE5;g_sFile.seek((uint64_t)ex);g_sFile.write(&del,1);
      if(fc>=2) imgFreeChainM(fc);
    }
    memset(_iuSec,0,sizeof(_iuSec)); _iuOk=true;
  } else if (up.status==UPLOAD_FILE_WRITE) {
    if(!_iuOk||!g_sOpen) return;
    uint32_t rem=up.currentSize; uint8_t* src=up.buf;
    while(rem>0){
      uint32_t sp=g_sBPB.bps-_iuSecOff,cp=min(rem,sp);
      memcpy(_iuSec+_iuSecOff,src,cp);
      _iuSecOff+=cp;src+=cp;rem-=cp;_iuWrit+=cp;
      if(_iuSecOff==g_sBPB.bps){if(!_iuFlushSector()) return;}
    }
  } else if (up.status==UPLOAD_FILE_END) {
    if(!_iuOk||!g_sOpen){imgCloseSession();return;}
    if(_iuSecOff>0){memset(_iuSec+_iuSecOff,0,g_sBPB.bps-_iuSecOff);_iuFlushSector();}
    if(_iuFirst){
      int64_t slotOff=imgFindFreeSlot(_iuDir);
      if(slotOff>=0){
        uint8_t de[32]; imgMakeDirEntry(de,_iuFile.c_str(),_iuWrit,_iuFirst,false);
        g_sFile.seek((uint64_t)slotOff);g_sFile.write(de,32);
        _iuDone=true;
      }
    }
    imgCloseSession(); _iuOk=false; _iuDone=true;
    dualPrint.printf("[IMG] Upload done: %lu B\n",(unsigned long)_iuWrit);
    gkMtxGive();
    gkInvalidateForPath(_iuImgPath);
  } else if (up.status==UPLOAD_FILE_ABORTED) {
    if(_iuOk&&_iuFirst) imgFreeChainM(_iuFirst);
    imgCloseSession(); _iuOk=false;
    gkMtxGive();
  }
}

void handleImgPutDone() {
  if (!checkAuth()) return;
  httpServer.send(200,"application/json",
    _iuDone?"{\"ok\":true,\"bytes\":"+String(_iuWrit)+"}":"{\"error\":\"upload failed\"}");
}

// Helper: take sdMutex for web handlers (GoTek mode — Core 0 TinyUSB also accesses SD)
// Also closes GoTek file handles so web handlers don't conflict with open handles.
static inline bool gkMtxTake() {
  if (!sdMutex) return true;
  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) != pdTRUE) return false;
  return true;
}
static inline void gkMtxGive() { if (sdMutex) xSemaphoreGive(sdMutex); }

// Invalidate PSRAM sector cache and LRU file handle for a specific image path.
// Call after any image editor write (rm, mkdir, put) so GoTek/Windows sees fresh data.
static void gkInvalidateForPath(const String& imgPath) {
  if (!imgPath.length()) return;
  for (int i = 0; i < gkFC; i++) {
    if (imgPath.equalsIgnoreCase(String(gkF[i].path))) {
      // Invalidate PSRAM sector cache
      if (gkCacheSlot == i) gkCacheInvalidate();
      // Evict LRU read handle so next USB read reopens a fresh handle
      for (int c = 0; c < GK_CACHE_HANDLES; c++) {
        if (gkRHCache[c].fi == i) {
          if (gkRHCache[c].fh) gkRHCache[c].fh.close();
          gkRHCache[c].fi = -1;
          for (int r = 0; r < GK_CACHE_HANDLES; r++)
            if (gkRHLRU[r] == c) { gkRHLRU[r] = GK_CACHE_HANDLES - 1; break; }
        }
      }
      // If this image is currently single-slot mounted, signal UNIT ATTENTION
      // so Windows re-reads FAT/directory automatically — no manual remount needed.
      if ((int)gkF[i].slotNum == gkSelectedSlot && gkSelectedSlot >= 0) {
        msc_set_unit_attention();
        dualPrint.printf("[GK] UA → Windows will refresh disk view: %s\n", imgPath.c_str());
      } else {
        dualPrint.printf("[GK] Cache invalidated: %s\n", imgPath.c_str());
      }
      break;
    }
  }
}

// Create a blank FAT12 floppy image on SD card
// GET /api/gotek/create?name=DSKA0000.img&type=144  (144=1.44MB, 720=720KB, 360=360KB)
void handleGotekCreate() {
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(503,"application/json","{\"error\":\"SD not ready\"}"); return; }

  String name = httpServer.hasArg("name") ? httpServer.arg("name") : "";
  int    type = httpServer.hasArg("type") ? httpServer.arg("type").toInt() : 144;
  if (!name.length()) { httpServer.send(400,"application/json","{\"error\":\"Missing name\"}"); return; }
  if (!name.endsWith(".img") && !name.endsWith(".ima")) name += ".img";

  uint16_t totSec, rde, fatSz;
  uint8_t  spc, media, spt;
  if (type == 720) {
    totSec=1440; rde=112; fatSz=3; spc=2; media=0xF9; spt=9;
  } else if (type == 360) {
    totSec=720;  rde=112; fatSz=2; spc=2; media=0xFD; spt=9;
  } else {  // 1.44 MB default
    totSec=2880; rde=224; fatSz=9; spc=1; media=0xF0; spt=18;
  }
  uint32_t rootSec = ((uint32_t)rde * 32 + 511) / 512;
  uint32_t dataLBA = 1 + 2*fatSz + rootSec;

  String path = gotekImgDir + "/" + name;

  if (!gkMtxTake()) { httpServer.send(503,"application/json","{\"error\":\"SD busy\"}"); return; }

  if (SD_MMC.exists(path.c_str())) {
    gkMtxGive(); httpServer.send(409,"application/json","{\"error\":\"File already exists\"}"); return;
  }
  File f = SD_MMC.open(path.c_str(), FILE_WRITE);
  if (!f) { gkMtxGive(); httpServer.send(500,"application/json","{\"error\":\"Cannot create file\"}"); return; }

  uint8_t buf[512]; memset(buf, 0, 512);

  // Boot sector
  buf[0]=0xEB; buf[1]=0x3C; buf[2]=0x90;
  memcpy(buf+3, "MSDOS5.0", 8);
  buf[0x0B]=0x00; buf[0x0C]=0x02;
  buf[0x0D]=(uint8_t)spc;
  buf[0x0E]=1;    buf[0x0F]=0;
  buf[0x10]=2;
  buf[0x11]=(uint8_t)rde; buf[0x12]=(uint8_t)(rde>>8);
  buf[0x13]=(uint8_t)totSec; buf[0x14]=(uint8_t)(totSec>>8);
  buf[0x15]=media;
  buf[0x16]=(uint8_t)fatSz; buf[0x17]=0;
  buf[0x18]=(uint8_t)spt;   buf[0x19]=0;
  buf[0x1A]=2;              buf[0x1B]=0;
  buf[0x24]=0x00; buf[0x26]=0x29;
  buf[0x27]=0x42; buf[0x28]=0x42; buf[0x29]=0x42; buf[0x2A]=0x42;
  memcpy(buf+0x2B, "NO NAME    ", 11);
  memcpy(buf+0x36, "FAT12   ",   8);
  buf[0x1FE]=0x55; buf[0x1FF]=0xAA;
  f.write(buf, 512);

  // FAT1 + FAT2
  memset(buf, 0, 512);
  for (int fi = 0; fi < 2; fi++) {
    for (int s = 0; s < fatSz; s++) {
      if (s == 0) { buf[0]=media; buf[1]=0xFF; buf[2]=0xFF; }
      else        { buf[0]=0; buf[1]=0; buf[2]=0; }
      f.write(buf, 512);
    }
  }

  // Root dir + data area — write zeroed sectors
  memset(buf, 0, 512);
  uint32_t remaining = totSec - 1 - 2*fatSz;
  for (uint32_t s = 0; s < remaining; s++) f.write(buf, 512);

  f.close();
  gkMtxGive();

  uint32_t sizeKB = (uint32_t)totSec / 2;
  dualPrint.printf("[GOTEK] Created: %s  %lu KB\n", path.c_str(), (unsigned long)sizeKB);
  gkRefresh();

  httpServer.send(200,"application/json",
    "{\"ok\":true,\"name\":\""+name+"\",\"kb\":"+String(sizeKB)+"}");
}
// 44100 Hz, 16-bit signed, stereo, I2S Philips standard
void initI2S() {
  if (!cfg.audioModule) {
    dualPrint.println(F("[I2S]  Audio module disabled — I2S not initialized."));
    return;
  }
  // PCM5102A SCK (system clock) pin fix:
  // In 3-wire I2S mode (no MCLK), PCM5102 requires SCK to be held LOW for
  // ≥16 LRCK periods so its internal PLL can start.  A floating SCK pin is
  // picked up as noise, causing PCM5102 to periodically detect/lose an
  // "external clock" signal → PLL instability → soft-mute → audible crackling.
  //
  // Hardware fix (preferred): solder the SCK bridge on the GY-PCM5102 module,
  // or connect the SCK pin to GND with a wire.
  //
  // Software fix (backup): drive the MCLK GPIO LOW as an output so the SCK
  // line is actively held at GND even if the hardware bridge is missing.
  // GPIO 7 is the MCLK pin defined in our I2S config.
#ifndef I2S_MCLK_PIN
#define I2S_MCLK_PIN 7   // connected to PCM5102 SCK — hold LOW for 3-wire PLL mode
#endif
  gpio_config_t sck_cfg = {};
  sck_cfg.pin_bit_mask = (1ULL << I2S_MCLK_PIN);
  sck_cfg.mode         = GPIO_MODE_OUTPUT;
  sck_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
  sck_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
  sck_cfg.intr_type    = GPIO_INTR_DISABLE;
  gpio_config(&sck_cfg);
  gpio_set_level((gpio_num_t)I2S_MCLK_PIN, 0);  // SCK = LOW → internal PLL mode
  dualPrint.printf("[I2S]  SCK (GPIO%d) held LOW — PCM5102 internal PLL mode\n", I2S_MCLK_PIN);
  dualPrint.println(F("[I2S]  Hardware fix: solder SCK bridge on module or wire SCK pin to GND"));
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;   // output silence when DMA buffer drains (pause/stop)
  // ── DMA descriptor alignment fix ─────────────────────────────────────────
  // CD audio sector = 2352 bytes = 588 stereo 16-bit frames.
  // dma_frame_num=588 (13 ms/descriptor): too small — Core 1 WiFi IPC preemption
  //   causes audio task to miss DMA events → machine-gun crackling at 75 Hz.
  // dma_frame_num=2352 (53 ms/descriptor): 4 writes fill exactly one descriptor,
  //   perfect CD-sector alignment, ISR fires only ~18 Hz → immune to IPC delays.
  //   Core 1 (audio) + Core 0 (WiFi/USB) → no WiFi preemption between cores.
  // ─────────────────────────────────────────────────────────────────────────
  chan_cfg.dma_frame_num = 2352;   // 2352 frames × 4 B = 9408 B = 4 CD sectors exactly
  chan_cfg.dma_desc_num  = 14;     // 14 × 9408 B = 131712 B ≈ 747 ms
  i2s_chan_handle_t txTmp = nullptr;
  if (i2s_new_channel(&chan_cfg, &txTmp, nullptr) != ESP_OK) {
    chan_cfg.dma_desc_num = 7;     // fallback: 7 × 9408 B ≈ 373 ms
    if (i2s_new_channel(&chan_cfg, &txTmp, nullptr) != ESP_OK) {
      chan_cfg.dma_frame_num = 4096; chan_cfg.dma_desc_num = 8;  // proven fallback
      if (i2s_new_channel(&chan_cfg, &txTmp, nullptr) != ESP_OK) {
        dualPrint.println(F("[I2S]  Failed to create channel!")); return;
      }
    }
  }
  uint32_t dmaBufMs = (chan_cfg.dma_desc_num * chan_cfg.dma_frame_num * 4 * 1000UL) / 176400UL;
  dualPrint.printf("[I2S]  DMA buffer: %u KB = %u ms  (desc=%u frame=%u — aligned to CD sector)\n",
                   chan_cfg.dma_desc_num * chan_cfg.dma_frame_num * 4 / 1024, dmaBufMs,
                   chan_cfg.dma_desc_num, chan_cfg.dma_frame_num);
  i2s_std_config_t std_cfg = {
    // ESP32-S3 has no APLL — use default PLL_160M clock source.
    // The driver uses fractional dividers to approximate 44100 Hz with < 0.1% error.
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCK_PIN,
      .ws   = (gpio_num_t)I2S_WS_PIN,
      .dout = (gpio_num_t)I2S_DATA_PIN,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = { .mclk_inv=false, .bclk_inv=false, .ws_inv=false }
    }
  };
  if (i2s_channel_init_std_mode(txTmp, &std_cfg) != ESP_OK) {
    i2s_del_channel(txTmp);
    dualPrint.println(F("[I2S]  Failed to init STD mode!")); return;
  }
  i2s_channel_enable(txTmp);
  i2sTx = txTmp;  // assign to volatile global only after full init
  dualPrint.printf("[I2S]  Ready — BCK=%d WS=%d DATA=%d\n",
                I2S_BCK_PIN, I2S_WS_PIN, I2S_DATA_PIN);
  dualPrint.println(F("[I2S]  XSMT must be HIGH (3V3) — if no audio, check XSMT pin or H3L bridge on module."));
#ifdef CONFIG_I2S_ISR_IRAM_SAFE
  dualPrint.println(F("[I2S]  CONFIG_I2S_ISR_IRAM_SAFE=y — ISR in IRAM, patch applied correctly."));
#else
  dualPrint.println(F("[I2S]  CONFIG_I2S_ISR_IRAM_SAFE=n — ISR in flash cache (run build_exfat_libs.py to fix)."));
#endif
}

// ── Audio playback FreeRTOS task ────────────────────────────────
// Runs on core 0. Reads 2352-byte raw audio sectors from SD card BIN file
// and writes all 2352 bytes to the I2S DMA buffer.
// Red Book audio sectors contain raw 16-bit stereo PCM with NO header —
// all 2352 bytes are usable audio data (588 stereo samples × 4 bytes each).
// AUDIO_BUF_SECTORS = 32 = LCM(2352, 512) / 2352 = 75264 / 2352.
// FatFs has an internal 512-byte sector buffer that tracks how many bytes of the
// current SD sector have been consumed.  After reading N × 2352 bytes, the buffer
// state is (N × 2352) mod 512.  This state repeats with period 32 audio sectors
// (= 147 SD sectors = 75264 bytes = LCM(2352,512)).
// With AUDIO_BUF_SECTORS < 32, the internal buffer state changes on each batch,
// and at every 32nd sector the state transitions from non-zero to zero — causing
// a brief FatFs timing irregularity that manifests as a periodic phase-jump crackle
// audible roughly every 430 ms.
// Reading exactly 32 sectors per batch guarantees the buffer state is always 0
// (SD-sector aligned) after every read, eliminating the periodic irregularity.
// 32 × 2352 = 75264 bytes ≈ 426 ms of audio.
//
// DMA bus contention fix:
// audioBuf is allocated in PSRAM (if available).  When the SDMMC driver sees a
// buffer that is not DMA-capable (PSRAM is not), it automatically uses an internal
// 512-byte SRAM bounce buffer and copies sector-by-sector.  This means each
// SDMMC DMA transaction is only 512 bytes (~0.05 ms at 10 MB/s) instead of
// 75 264 bytes (~7.5 ms).  The short transactions leave the AHB bus available
// for I2S DMA refills every 1.45 ms, eliminating the bus-contention crackle.
// If PSRAM is not present the buffer falls back to internal SRAM.
// SDMMC CLK (GPIO12, 40 MHz) couples into I2S BCK (GPIO8) during SD reads.
// Burst = N×2352 / 1.46 MB/s:  4 sectors = ~6 ms (barely perceptible)
//                               32 sectors = ~51 ms (clearly audible click)
// Keep small (≤4) for best audio. Full fix: route I2S to GPIO17+ or use 4-bit SDMMC.
#define AUDIO_BUF_SECTORS 1   // 1 sector = 2352 bytes = 1/4 DMA descriptor → immediate write, no ISR wait
// With dma_frame_num=2352 (131KB buffer), writing 1 sector always finds space → no blocking.
// Writing 4 sectors fills one full descriptor → must wait 53ms for DMA drain → WiFi ISR adds 10ms → SLOW BATCH.
// Use 'set audio-sectors N' (1-4) at runtime to experiment. 1 = cleanest, 4 = more SD bursts.
static uint8_t* audioBuf = nullptr;   // allocated in PSRAM in audioTask (see below)

// Runtime-adjustable batch size (1–32). Change via "set audio-sectors <n>".
// Diagnostic: if crackling disappears at n=1, the issue is in batch/FatFs boundary handling.
static volatile int audioReadSectors = AUDIO_BUF_SECTORS;

// Static volume-scaling buffer in SRAM (DMA requires SRAM, not PSRAM)
static int16_t audioScaledBatch[AUDIO_BUF_SECTORS * 2352 / 2];

static void audioTask(void* arg) {
  if (!audioBuf) {
    audioBuf = (uint8_t*)heap_caps_malloc(2352 * AUDIO_BUF_SECTORS,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (audioBuf)
      dualPrint.println(F("[AUDIO] audioBuf → PSRAM (SDMMC bounce-buffer mode, min AHB contention)"));
    else {
      audioBuf = (uint8_t*)heap_caps_malloc(2352 * AUDIO_BUF_SECTORS,
                                            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      dualPrint.println(audioBuf
        ? F("[AUDIO] audioBuf → SRAM (PSRAM not available)")
        : F("[AUDIO] ERROR: audioBuf alloc failed — no audio!"));
    }
  }

  File audioFile;
  String openedFile;

  while (true) {
    if (!audioBuf) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    if (audioState != AUDIO_PLAY) {
      // Always close the file handle when not playing — idempotent (File::close on already-closed File is a no-op).
      // Clearing audioCloseFile unconditionally ensures doMount()'s wait loop always exits within one 20ms tick
      // instead of timing out when audioFile happens to be invalid (which left the old && condition false).
      // On resume the PLAY branch reopens and seeks to the correct offset anyway.
      if (audioFile) { audioFile.close(); openedFile = ""; }
      audioCloseFile = false;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    uint32_t lba = audioCurrentLba;
    uint32_t mySeq = audioPlaySeq;

    AudioTrack* trk = nullptr;
    for (int i = 0; i < audioTrackCount; i++) {
      if (audioTracks[i].valid &&
          lba >= audioTracks[i].startLba &&
          lba <  audioTracks[i].startLba + audioTracks[i].lengthSectors) {
        trk = &audioTracks[i];
        break;
      }
    }

    if (!trk) {
      audioState = AUDIO_STOP;
      if (audioFile) { audioFile.close(); openedFile = ""; }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (!i2sTx) {
      if (audioState != AUDIO_PLAY) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
      audioCurrentLba++;
      uint8_t am,as_,af,rm,rs,rf;
      lbaToMsf(audioCurrentLba+150,am,as_,af);
      uint32_t rel=audioCurrentLba-trk->startLba;
      lbaToMsf(rel,rm,rs,rf);
      subChannel.trackNum=trk->number;subChannel.indexNum=1;
      subChannel.absM=am;subChannel.absS=as_;subChannel.absF=af;
      subChannel.relM=rm;subChannel.relS=rs;subChannel.relF=rf;
      if(audioEndLba>0&&audioCurrentLba>=audioEndLba) audioState=AUDIO_STOP;
      vTaskDelay(pdMS_TO_TICKS(13));
      continue;
    }

    // ── SD access — guarded by sdMutex (shared with SCSI DAE reader) ────────────
    if (sdMutex && xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
      // SD busy (SCSI DAE read in progress) — skip this batch, retry next tick
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    if (!audioFile || openedFile != trk->binFile) {
      if (audioFile) audioFile.close();
      audioFile  = SD_MMC.open(trk->binFile.c_str(), FILE_READ);
      openedFile = trk->binFile;
      if (!audioFile) {
        if (sdMutex) xSemaphoreGive(sdMutex);
        dualPrint.printf("[AUDIO] Cannot open: %s\n", trk->binFile.c_str());
        audioState = AUDIO_STOP;
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }
    }

    uint64_t fileSector = (uint64_t)trk->fileSectorBase + (lba - trk->startLba);
    uint64_t fileOffset = fileSector * 2352;
    if ((uint64_t)audioFile.position() != fileOffset) {
      if (!audioFile.seek(fileOffset)) {
        if (sdMutex) xSemaphoreGive(sdMutex);
        audioState = AUDIO_STOP;
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }
    }

    int sectorsToRead = audioReadSectors;
    uint32_t remaining = (audioEndLba > lba) ? (audioEndLba - lba) : 0;
    if (remaining == 0) { if (sdMutex) xSemaphoreGive(sdMutex); audioState = AUDIO_STOP; continue; }
    if ((uint32_t)sectorsToRead > remaining) sectorsToRead = (int)remaining;
    uint32_t trackRemaining = (trk->startLba + trk->lengthSectors) - lba;
    if ((uint32_t)sectorsToRead > trackRemaining) sectorsToRead = (int)trackRemaining;
    if (sectorsToRead <= 0) { if (sdMutex) xSemaphoreGive(sdMutex); audioCurrentLba++; continue; }

    int bytesRead = audioFile.read(audioBuf, sectorsToRead * 2352);
    if (sdMutex) xSemaphoreGive(sdMutex);  // release BEFORE I2S write
    if (bytesRead <= 0) { audioState = AUDIO_STOP; continue; }

    int sectorsGot = bytesRead / 2352;

    i2s_chan_handle_t i2sHandle = i2sTx;
    if (!i2sHandle) { audioState = AUDIO_STOP; continue; }

    const uint8_t* writeSrc;
    if (audioMuted || audioVolume == 0) {
      static const uint8_t silenceBatch[AUDIO_BUF_SECTORS * 2352] = {};
      writeSrc = silenceBatch;
    } else if (audioVolume < 100) {
      const int16_t* src16 = (const int16_t*)audioBuf;
      int nSamples = sectorsGot * 2352 / 2;
      for (int i = 0; i < nSamples; i++)
        audioScaledBatch[i] = (int16_t)((int32_t)src16[i] * audioVolume / 100);
      writeSrc = (const uint8_t*)audioScaledBatch;
    } else {
      memcpy(audioScaledBatch, audioBuf, sectorsGot * 2352);
      writeSrc = (const uint8_t*)audioScaledBatch;
    }

    size_t totalBytes = (size_t)sectorsGot * 2352;
    size_t written = 0;
    uint32_t t0 = micros();
    i2s_channel_write(i2sHandle, writeSrc, totalBytes, &written, pdMS_TO_TICKS(500));
    uint32_t dtUs = micros() - t0;

    if (written < totalBytes)
      dualPrint.printf("[AUDIO] DMA underwrite: %u/%u bytes at LBA %lu\n",
                       (unsigned)written, (unsigned)totalBytes, (unsigned long)lba);

    uint32_t expUs = (uint32_t)sectorsGot * 13333;
    if (dtUs > expUs + 10000)
      if (cfg.debugMode) dualPrint.printf("[AUDIO] SLOW BATCH: %lu us (exp~%lu) lba=%lu\n",
                       (unsigned long)dtUs, (unsigned long)expUs, (unsigned long)lba);

    if (audioPlaySeq == mySeq) {
      lba += sectorsGot;
      audioCurrentLba = lba;
      uint8_t am, as_, af;
      lbaToMsf(lba + 150, am, as_, af);
      uint32_t relLba = (lba > trk->startLba) ? lba - trk->startLba : 0;
      uint8_t rm, rs, rf;
      lbaToMsf(relLba, rm, rs, rf);
      subChannel.trackNum = trk->number;
      subChannel.indexNum = 1;
      subChannel.absM = am; subChannel.absS = as_; subChannel.absF = af;
      subChannel.relM = rm; subChannel.relS = rs; subChannel.relF = rf;
    }

    if (audioPlaySeq == mySeq && audioEndLba > 0 && audioCurrentLba >= audioEndLba)
      audioState = AUDIO_STOP;

    vTaskDelay(1);
  }
  vTaskDelete(nullptr);
}

void startAudioTask() {
  if (audioTaskHandle) return;
  // Core 1: isolated from WiFi ISRs which run on Core 0.
  // dma_frame_num=2352 (53ms/descriptor) makes the task immune to Core 1 IPC delays.
  // FatFS access (audio reads) is protected by sdMutex shared with SCSI DAE reader.
  int audioCore = 1;
  int audioPrio = 24;
  xTaskCreatePinnedToCore(audioTask, "audio", 16384, nullptr, audioPrio, &audioTaskHandle, audioCore);
  dualPrint.printf("[AUDIO] Task started on core %d prio %d.\n", audioCore, audioPrio);
}

// ── Playback control helpers ─────────────────────────────────────
// Find audio track by 1-based track number
static AudioTrack* findAudioTrack(uint8_t num) {
  for (int i = 0; i < audioTrackCount; i++)
    if (audioTracks[i].valid && audioTracks[i].number == num)
      return &audioTracks[i];
  return nullptr;
}

void audioPlay(uint32_t startLba, uint32_t endLba) {
  subPollActive = false;  // reset — wait for first poll before enabling stop detection
  // USBCD1 truncates M:S:F to M:S when storing track positions (drops F field).
  // Result: startLba can be up to 74 sectors BEFORE the actual audio track start.
  // Snap forward to the nearest audio track start if within 150 sectors (2 seconds).
  for (int i = 0; i < audioTrackCount; i++) {
    uint32_t ts = audioTracks[i].startLba;
    if (startLba < ts && ts - startLba <= 150u) {
      dualPrint.printf("[AUDIO] Snap LBA %lu→%lu (F-trunc fix, +%lu sectors)\n",
                       startLba, ts, ts - startLba);
      startLba = ts;
      break;
    }
    if (startLba >= ts && startLba < ts + audioTracks[i].lengthSectors)
      break;  // already in a valid track, no snap needed
  }
  // Increment sequence counter FIRST so the audio task's write loop immediately
  // aborts its current batch (the loop checks audioPlaySeq == mySeq).  The task
  // will NOT update audioCurrentLba after the batch (seq mismatch guard), so our
  // new value below is preserved.  No vTaskDelay needed — the task self-corrects
  // on the very next sector boundary (≤ 13 ms).
  audioPlaySeq++;
  audioState      = AUDIO_STOP;
  audioCurrentLba = startLba;
  audioEndLba     = endLba;
  audioState      = AUDIO_PLAY;
  // Resolve to audio track for debug
  AudioTrack* trk = nullptr;
  for (int i = 0; i < audioTrackCount; i++)
    if (audioTracks[i].valid && startLba >= audioTracks[i].startLba &&
        startLba < audioTracks[i].startLba + audioTracks[i].lengthSectors)
      { trk = &audioTracks[i]; break; }
  if (trk) dualPrint.printf("[AUDIO] Play LBA %lu→%lu  Track %d  File:%s\n",
           startLba, endLba, trk->number,
           trk->binFile.substring(trk->binFile.lastIndexOf("/")+1).c_str());
  else     dualPrint.printf("[AUDIO] Play LBA %lu→%lu  (no audio track at start LBA!)\n",
           startLba, endLba);
}

void audioStop() {
  subPollActive = false;  // reset poll tracking on explicit stop
  audioState = AUDIO_STOP;
  // Do NOT call i2s_channel_write here — the audio task may be inside
  // i2s_channel_write concurrently; two simultaneous writers corrupt the
  // I2S DMA ring buffer, causing "channel not enabled" errors.
  // auto_clear=true outputs silence automatically when the DMA drains.
  dualPrint.println(F("[AUDIO] Stop."));
}

void audioPause() {
  audioState = AUDIO_PAUSE;
  // Same reason as audioStop — no concurrent i2s_channel_write here.
  // The audio task stops writing at the next sector boundary and auto_clear
  // fills the remaining DMA frames with silence.
  dualPrint.println(F("[AUDIO] Pause."));
}
void audioResume(){ audioState = AUDIO_PLAY;  dualPrint.println(F("[AUDIO] Resume.")); }

// ── Test tone generator ───────────────────────────────────────────────────────
// Plays a sine wave directly from RAM (no SD, no file) to isolate hardware.
// If crackling occurs here too → pure hardware issue (power/PCM5102).
// If this plays clean → issue is in SD→I2S data pipeline.
static volatile bool audioTestActive = false;

void audioTestTone(int freqHz, int durationSec) {
  if (!i2sTx) { dualPrint.println(F("[AUDIO] Test tone: I2S not initialized.")); return; }
  audioStop();
  vTaskDelay(pdMS_TO_TICKS(50));
  audioTestActive = true;

  const int   SR      = 44100;
  const int   BUFSIZE = 512;          // stereo frames per chunk
  int16_t     buf[BUFSIZE * 2];       // L+R interleaved
  const float scale   = 16000.0f;    // amplitude (~50% full scale)
  float       phase   = 0.0f;
  const float inc     = 2.0f * 3.14159265f * freqHz / SR;
  int32_t     totalFrames = (int32_t)SR * durationSec;

  dualPrint.printf("[AUDIO] Test tone: %d Hz, %d s — listen for crackling\n", freqHz, durationSec);
  dualPrint.println(F("[AUDIO]   Clean tone = hardware OK, crackle = hardware problem"));

  i2s_chan_handle_t h = i2sTx;
  while (totalFrames > 0 && audioTestActive && h) {
    int frames = (totalFrames > BUFSIZE) ? BUFSIZE : (int)totalFrames;
    for (int i = 0; i < frames; i++) {
      int16_t s = (int16_t)(sinf(phase) * scale);
      buf[i*2]   = s;   // L
      buf[i*2+1] = s;   // R
      phase += inc;
      if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;
    }
    size_t written = 0;
    i2s_channel_write(h, buf, frames * 4, &written, pdMS_TO_TICKS(200));
    totalFrames -= frames;
    h = i2sTx;  // re-check in case module was disabled
  }
  audioTestActive = false;
  dualPrint.println(F("[AUDIO] Test tone done."));
}

// =============================================================================
//  SCSI DEBUG BRIDGE — volána z USBMSC.cpp patche, výstup jde do dualPrint (Serial + HTML)
// =============================================================================
void scsi_log(const char* msg) {
  if (cfg.debugMode) dualPrint.print(msg);
}

void scsi_log_play_msf(uint8_t sm, uint8_t ss, uint8_t sf,
                       uint8_t em, uint8_t es, uint8_t ef,
                       uint32_t s, uint32_t e) {
  if (!cfg.debugMode) return;
  dualPrint.printf("[SCSI] PLAY_MSF %02X:%02X:%02X→%02X:%02X:%02X  LBA %lu→%lu\n",
                   sm, ss, sf, em, es, ef, s, e);
}

// =============================================================================
//  BRIDGE FUNCTIONS FOR USBMSC.cpp SCSI PATCH
//  Plain C++ functions — USBMSC.cpp and this file are both C++,
//  so C++ name mangling matches automatically. No extern "C" needed.
// =============================================================================

// ── Patch sentinel ────────────────────────────────────────────────────────────
// Called by patched USBMSC.cpp from tud_msc_inquiry2_cb on first INQUIRY.
// If this is never called, the patch was not applied (old build or no rebuild).
static bool _scsiPatchDaeOk = false;
void scsi_patch_v14_dae_ok() {
  if (!_scsiPatchDaeOk) {
    _scsiPatchDaeOk = true;
    dualPrint.println(F("[SCSI] USBMSC patch v14+DAE confirmed — READ CD (0xBE) bridge active."));
  }
}
bool scsi_patch_dae_active() { return _scsiPatchDaeOk; }

void scsi_audio_play(uint32_t startLba, uint32_t endLba) {
  audioPlay(startLba, endLba);
}
void scsi_audio_stop()   { audioStop(); }

// ── DAE (Digital Audio Extraction) mirror ────────────────────────────────────
// Called from USBMSC.cpp SCSI patch every time Windows reads audio sectors via
// READ CD (0xBE).  Windows plays them through the PC sound card; we mirror the
// same position to PCM5102 so audio comes out of the hardware DAC as well.
//
// Detection logic:
//  • If LBA is inside an audio track and the position is new (seek or start),
//    call audioPlay() to start PCM5102 from that position.
//  • If LBA is sequential (Windows advancing through the track), let the audio
//    task continue — it self-corrects via audioCurrentLba tracking.
//  • If no DAE read arrives for >3 s, stop PCM5102 (Windows stopped).
//
// To enable: in patch_usbmsc.py, after the READ CD handler successfully fills
// the response buffer with audio data, add ONE call:
//   scsi_dae_read(lba, transfer_length);
// ─────────────────────────────────────────────────────────────────────────────
static volatile uint32_t daeLastLba   = 0;
static volatile uint32_t daeLastTime  = 0;   // millis() of last DAE read
static volatile bool     daeActive    = false;



// ── Windows 10 DAE: return real audio data from BIN file ─────────────────────
// Called from case 0xBE (READ CD) SCSI handler in TinyUSB task context.
// Takes sdMutex to prevent concurrent SD access with the audio task.
// Returns bytes written to buf, or 0 on error (caller should return silence).
extern "C" int32_t scsi_dae_read_data(uint32_t lba, uint32_t cnt,
                                       void* buf, uint32_t bufsize) {
  if (!buf || cnt == 0 || !sdMutex) return 0;

  // Find audio track containing this LBA
  AudioTrack* trk = nullptr;
  for (int i = 0; i < audioTrackCount; i++) {
    if (audioTracks[i].valid &&
        lba >= audioTracks[i].startLba &&
        lba <  audioTracks[i].startLba + audioTracks[i].lengthSectors) {
      trk = &audioTracks[i]; break;
    }
  }
  if (!trk) return 0;  // LBA not in any audio track

  // Clamp to track boundary
  uint32_t trackEnd = trk->startLba + trk->lengthSectors;
  if (lba + cnt > trackEnd) cnt = trackEnd - lba;
  if (cnt == 0) return 0;

  uint32_t needed = cnt * 2352;
  if (bufsize < needed) { cnt = bufsize / 2352; needed = cnt * 2352; }
  if (needed == 0) return 0;

  uint64_t fileOffset = ((uint64_t)trk->fileSectorBase + (lba - trk->startLba)) * 2352;

  // Take SD mutex — audio task holds it for ~3 ms per batch; SCSI holds ~25 ms max
  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    // SD busy — return silence so Windows gets valid response without USB reset
    memset(buf, 0, needed);
    return (int32_t)needed;
  }

  int32_t result = 0;
  File f = SD_MMC.open(trk->binFile.c_str(), FILE_READ);
  if (f) {
    if (f.seek(fileOffset)) {
      int n = f.read((uint8_t*)buf, needed);
      if (n > 0) result = (int32_t)n;
    }
    f.close();
  }
  xSemaphoreGive(sdMutex);

  if (result == 0) { memset(buf, 0, needed); result = (int32_t)needed; }
  return result;
}

void scsi_dae_read(uint32_t lba, uint32_t count) {
  if (count == 0) return;

  // Is this LBA inside an audio track?
  AudioTrack* trk = nullptr;
  for (int i = 0; i < audioTrackCount; i++) {
    if (audioTracks[i].valid &&
        lba >= audioTracks[i].startLba &&
        lba <  audioTracks[i].startLba + audioTracks[i].lengthSectors) {
      trk = &audioTracks[i]; break;
    }
  }
  if (!trk) return;  // data sector, ignore

  uint32_t now = millis();
  // Allowed drift: Windows reads in bursts; tolerate gaps up to 500 ms
  bool sequential = daeActive &&
                    (lba >= daeLastLba) &&
                    (lba <= daeLastLba + 150) &&   // ≤ 150 secs ahead = sequential
                    (now - daeLastTime < 500);

  daeLastLba  = lba + count;
  daeLastTime = now;
  daeActive   = true;

  if (!sequential || audioState != AUDIO_PLAY) {
    // New position or audio task was stopped — (re)start from current LBA
    uint32_t endLba = trk->startLba + trk->lengthSectors;
    dualPrint.printf("[DAE] Windows DAE → LBA %lu  Track %d — mirroring to PCM5102\n",
                     lba, trk->number);
    audioPlay(lba, endLba);
  }
  // If sequential and already playing, audio task tracks position itself — no action needed.
}

// Call from a periodic task or main loop to detect DAE stop:
// if daeActive && millis()-daeLastTime > 3000 → audioStop(), daeActive=false
void scsi_dae_check_timeout() {
  // Win98 Stop/Pause detection: if polling ceased while audio plays → stop
  if (cfg.win98StopMs > 0 && audioState == AUDIO_PLAY && subPollActive &&
      (millis() - subPollLastTime > (uint32_t)cfg.win98StopMs)) {
    subPollActive = false;
    dualPrint.println(F("[AUDIO] Win98 Stop detected (no SUB_CH poll) → stopping"));
    audioStop();
  }
  if (daeActive && (millis() - daeLastTime > 3000)) {
    daeActive = false;
    if (audioState == AUDIO_PLAY) {
      dualPrint.println(F("[DAE] Windows DAE stopped — stopping PCM5102"));
      audioStop();
    }
  }
}
void scsi_audio_pause()  { audioPause(); }
void scsi_audio_resume() { audioResume(); }

int scsi_audio_state() { return (int)audioState; }

void scsi_audio_subchannel(uint8_t* buf) {
  buf[0] = subChannel.trackNum;
  buf[1] = subChannel.indexNum;
  buf[2] = subChannel.absM; buf[3] = subChannel.absS; buf[4] = subChannel.absF;
  buf[5] = subChannel.relM; buf[6] = subChannel.relS; buf[7] = subChannel.relF;
}

int scsi_audio_track_count() { return (int)audioTrackCount; }

int scsi_audio_track_info(int n, uint32_t* startLba, uint32_t* lenSectors) {
  // Note: track 1 IS a valid audio track on pure audio CDs — don't skip it
  for (int i = 0; i < audioTrackCount; i++) {
    if (audioTracks[i].number == (uint8_t)n) {
      if (startLba)   *startLba   = audioTracks[i].startLba;
      if (lenSectors) *lenSectors = audioTracks[i].lengthSectors;
      return 1;
    }
  }
  return 0;
}

// Returns track number of first audio track (1 for pure audio CD, 2 for mixed)
uint8_t scsi_first_audio_track() {
  if (audioTrackCount > 0) return audioTracks[0].number;
  return 0;
}

// Returns true if disc has a data track (cdromBlockCount > 0)
bool scsi_has_data_track() { return cdromBlockCount > 0; }

uint32_t cdromBlockCount_get() {
  // For READ TOC lead-out: return full virtual disc LBA (includes audio)
  return discLeadOutLba > 0 ? discLeadOutLba : cdromBlockCount;
}

// Returns current DOS driver mode (used by SCSI patch for PLAY_MSF decode)
uint8_t scsi_get_dos_driver() { return cfg.dosDriver; }
bool    scsi_get_dos_compat()  { return cfg.dosCompat; }
bool    scsi_get_debug()       { return cfg.debugMode; }
bool    scsi_get_gotek_mode()  { return cfg.deviceMode == 1; }

// ── tud_msc_inquiry2_cb — definitive SCSI INQUIRY override ────────────────────
// TinyUSB (in newer Arduino ESP32 versions) calls this callback to let the
// application fill the ENTIRE 36-byte SCSI INQUIRY response structure,
// including peripheral_device_type (byte[0]).
// This is the CORRECT way to control device classification:
//   0x00 = Direct-access block device → Windows: Disk drive  (GoTek mode)
//   0x05 = CD-ROM                     → Windows: CdRom drive (CD-ROM mode)
// The function supersedes tud_msc_inquiry_cb for device_type control.
extern "C" uint32_t tud_msc_inquiry2_cb(uint8_t lun, void *inquiry_resp_v, uint32_t bufsize) {
  (void)lun;
  if (bufsize < 36) return 0;
  uint8_t *r = (uint8_t*)inquiry_resp_v;
  memset(r, 0, 36);
  r[1] = 0x80;  // removable medium bit
  r[2] = 0x02;  // ANSI SCSI-2
  r[3] = 0x02;  // response data format
  r[4] = 0x1F;  // additional length = 31
  if (scsi_get_gotek_mode()) {
    r[0] = 0x00;  // Direct-access block device → Windows installs Disk driver
    memcpy(&r[8],  "ESP32-S3",         8);
    memcpy(&r[16], "VirtualFloppy   ", 16);
    memcpy(&r[32], "1.00",              4);
  } else {
    r[0] = 0x05;  // CD-ROM peripheral device type → Windows installs CdRom driver
    // Copy from msc_luns via the existing inquiry_cb strings (already set by initUSBMSC)
    memcpy(&r[8],  "ESP32-S3",     8);
    memcpy(&r[16], "VirtualCDROM", 12);
    memcpy(&r[32], "1.00",          4);
  }
  return 36;
}

// Called from READ_SUB_CHANNEL SCSI handler to record poll timestamp.
void scsi_subchannel_polled() {
  subPollLastTime = millis();
  subPollActive   = true;
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
    if (label) dualPrint.printf("[EAP]  %s path not set\n", label);
    return nullptr;
  }
  File f = SD_MMC.open(path, "r");
  if (!f) { dualPrint.printf("[EAP]  Cannot open: %s\n", path); return nullptr; }
  size_t sz = f.size();
  if (sz == 0 || sz > 16000) { f.close(); dualPrint.printf("[EAP]  File too large or empty: %s (%u B)\n", path, sz); return nullptr; }
  // +1 for null terminator required by esp_eap_client API
  // 4096-bit RSA key PEM ~3.2KB, cert ~2KB, CA ~2KB — 16KB limit covers all
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < sz * 3 + 8192) {
    f.close();
    dualPrint.printf("[EAP]  Low heap (%lu B free) — cannot safely load %s\n", freeHeap, path);
    return nullptr;
  }
  char* buf = (char*)malloc(sz + 1);
  if (!buf) { f.close(); dualPrint.printf("[EAP]  malloc(%u) failed (heap: %lu B)\n", sz+1, freeHeap); return nullptr; }
  f.readBytes(buf, sz);
  buf[sz] = 0;
  f.close();
  dualPrint.printf("[EAP]  Loaded %s (%u bytes)\n", path, sz);
  return buf;
}

void startWiFi() {
  if (!strlen(cfg.ssid)) { dualPrint.println(F("[WARN] SSID not configured.")); return; }
  freeEapBuffers();  // Free any previous EAP cert buffers
  dualPrint.printf("[WiFi] Connecting to: %s\n", cfg.ssid);
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
    dualPrint.printf("[EAP]  Mode: %s\n", modeStr);
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
          dualPrint.printf("[EAP]  Auto identity from cert CN: %s\n", effectiveIdentity);
        }
        free(tmpCert);
      }
    }
    if (strlen(effectiveIdentity)) {
      esp_eap_client_set_identity((uint8_t*)effectiveIdentity, strlen(effectiveIdentity));
      dualPrint.printf("[EAP]  Identity: %s\n", effectiveIdentity);
    }

    // CA cert (optional) — kept in global buffer for supplicant task lifetime
    s_eapCaBuf = loadPemFromSD(cfg.eapCaPath, nullptr);
    if (s_eapCaBuf) {
      esp_eap_client_set_ca_cert((uint8_t*)s_eapCaBuf, strlen(s_eapCaBuf) + 1);
      dualPrint.println(F("[EAP]  CA cert loaded"));
    } else {
      dualPrint.println(F("[EAP]  No CA cert — server cert not verified"));
    }

    if (cfg.eapMode == EAP_PEAP) {
      // PEAP: username + password
      if (strlen(cfg.eapUsername))
        esp_eap_client_set_username((uint8_t*)cfg.eapUsername, strlen(cfg.eapUsername));
      if (strlen(cfg.eapPassword))
        esp_eap_client_set_password((uint8_t*)cfg.eapPassword, strlen(cfg.eapPassword));
      dualPrint.printf("[EAP]  PEAP username: %s\n", cfg.eapUsername);
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
        dualPrint.printf("[EAP]  ERROR: cert='%s' key='%s'\n",
          strlen(cfg.eapCertPath) ? cfg.eapCertPath : "(not set)",
          strlen(cfg.eapKeyPath)  ? cfg.eapKeyPath  : "(not set)");
        freeEapBuffers();
        dualPrint.println(F("[EAP]  Aborting."));
        return;
      }
      // Key format info — PKCS#8 warning only, not blocked (may work on some builds)
      dualPrint.println(F("[EAP]  ── Format Check ──"));
      bool keyOk = true;
      if (strstr(s_eapKeyBuf, "BEGIN RSA PRIVATE KEY"))
        dualPrint.println(F("[EAP]  Key : PKCS#1 RSA  ✓"));
      else if (strstr(s_eapKeyBuf, "BEGIN EC PRIVATE KEY"))
        dualPrint.println(F("[EAP]  Key : EC  ✓"));
      else if (strstr(s_eapKeyBuf, "BEGIN PRIVATE KEY"))
        dualPrint.println(F("[EAP]  Key : PKCS#8  ✓  (supported by ESP-IDF 5.x mbedTLS)"));
      else if (strstr(s_eapKeyBuf, "BEGIN ENCRYPTED PRIVATE KEY")) {
        if (strlen(cfg.eapKeyPass)) {
          dualPrint.println(F("[EAP]  Key : Encrypted  ✓  (passphrase provided)"));
        } else {
          dualPrint.println(F("[EAP]  Key : Encrypted  ✗  Set passphrase: set eap-kpass <passphrase>"));
          keyOk = false;
        }
      } else {
        dualPrint.println(F("[EAP]  Key : Unknown format — attempting anyway"));
      }
      if (strstr(s_eapCertBuf, "BEGIN CERTIFICATE")) dualPrint.println(F("[EAP]  Cert: X.509 PEM  ✓"));
      else { dualPrint.println(F("[EAP]  Cert: Unknown  ✗")); keyOk = false; }
      if (!keyOk) { freeEapBuffers(); dualPrint.println(F("[EAP]  Aborting.")); return; }

      dualPrint.printf("[EAP]  Client cert + key OK  (heap: %lu B)\n", ESP.getFreeHeap());
      // Key passphrase (for encrypted private keys)
      const uint8_t* keyPass   = strlen(cfg.eapKeyPass) ? (uint8_t*)cfg.eapKeyPass : nullptr;
      int            keyPassLen = strlen(cfg.eapKeyPass);
      if (keyPass) dualPrint.println(F("[EAP]  Key passphrase: provided"));
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
  dualPrint.print(F("[WiFi] "));
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis()-t0 > 15000) break;
    delay(500); dualPrint.print(".");
  }
  dualPrint.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    // Disable WiFi modem sleep — prevents DTIM-interval wakeups (~100ms or ~1s with DTIM=10)
    // from briefly blocking DMA and causing periodic I2S audio crackling.
    esp_wifi_set_ps(WIFI_PS_NONE);
    // TX power (0.25 dBm units). Lower = less RF interference into PCM5102 analog section.
    uint8_t txp = constrain(cfg.wifiTxPower, 8, 80);
    esp_wifi_set_max_tx_power(txp);
    dualPrint.printf("[WiFi] Connected!  IP: %s  RSSI: %d dBm  Ch: %d  TXpow: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel(), txp/4);
    // Show previous crash info from RTC memory — now visible in HTML log
    if (rtcCrashMagic == RTC_CRASH_MAGIC && rtcCrashMsg[0]) {
      dualPrint.printf("[BOOT] *** PREVIOUS CRASH WAS: %s *** (check serial for backtrace)\n",
                       rtcCrashMsg);
      rtcCrashMagic = 0;  // clear after showing
    }

    // Start mDNS so device is reachable as hostname.local
    // mDNS uses only the first label of an FQDN (before first dot)
    char mdnsLabel[32];
    strlcpy(mdnsLabel, cfg.hostname, sizeof(mdnsLabel));
    if (char* dot = strchr(mdnsLabel, '.')) *dot = 0;  // truncate at first dot
    if (MDNS.begin(mdnsLabel)) {
      if (strchr(cfg.hostname, '.')) {
        dualPrint.printf("[mDNS] Responder started: http://%s.local  (FQDN: %s)\n", mdnsLabel, cfg.hostname);
      } else {
        dualPrint.printf("[mDNS] Responder started: http://%s.local\n", mdnsLabel);
      }
    } else {
      dualPrint.println(F("[mDNS] Failed to start responder"));
    }
  } else {
    int st = WiFi.status();
    dualPrint.printf("[WiFi] Failed (status=%d)\n", st);
    if (cfg.eapMode) {
      dualPrint.println(F("[EAP]  Connection failed. Common causes:"));
      dualPrint.println(F("[EAP]   1. User not in RADIUS DB — add via SQL or users file"));
      dualPrint.printf("[EAP]      INSERT INTO radcheck (username,attribute,value,op)\n");
      dualPrint.printf("[EAP]        VALUES ('%s','Auth-Type','EAP',':=');\n",
                    strlen(cfg.eapIdentity) ? cfg.eapIdentity : "username");
      dualPrint.println(F("[EAP]   2. RADIUS rejected cert — check raddb/certs CA chain"));
      dualPrint.println(F("[EAP]   3. Identity mismatch — cert CN must match eap-id"));
      dualPrint.println(F("[EAP]   4. Cipher suite unsupported by mbedTLS"));
      dualPrint.println(F("[EAP]   5. Cert not PEM / key has passphrase (not supported)"));
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
  if (!checkAuth()) return;
  HTTPUpload &upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String dir = httpServer.hasArg("path") ? normPath(httpServer.arg("path")) : "/";
    uploadFilePath = (dir == "/" ? "" : dir) + "/" + String(upload.filename);
    dualPrint.printf("[UPLOAD] Start: %s\n", uploadFilePath.c_str());
    uploadOk     = (bool)(uploadFile = SD_MMC.open(uploadFilePath.c_str(), FILE_WRITE));
    uploadActive = true;
    if (!uploadOk) dualPrint.printf("[ERR] Cannot open for write: %s\n", uploadFilePath.c_str());
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadOk && uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      dualPrint.println(F("[ERR] Write error!")); uploadOk = false;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadOk) {
      uploadFile.close();
      dualPrint.printf("[UPLOAD] OK: %s  (%zu B)\n", uploadFilePath.c_str(), upload.totalSize);
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
// Check HTTP Basic Auth — returns false and sends 401 if auth required and not provided
bool checkAuth() {
  // HTTP→HTTPS redirect — runs for every request via checkAuth()
  if (cfg.httpsEnabled && httpsHandle != nullptr) {
    bool fromProxy = (httpServer.hasHeader("X-Internal-Proxy") &&
                      httpServer.header("X-Internal-Proxy") == "1");
    if (!fromProxy) {
      String host = httpServer.hostHeader();
      if (!host.length()) host = WiFi.localIP().toString();
      int col = host.lastIndexOf(":");
      if (col > 0) host = host.substring(0, col);
      String url = "https://" + host + httpServer.uri();
      if (httpServer.args()) {
        url += "?";
        for (int i = 0; i < httpServer.args(); i++) {
          if (i) url += "&";
          url += httpServer.argName(i) + "=" + httpServer.arg(i);
        }
      }
      httpServer.sendHeader("Location", url, true);
      httpServer.send(302, "text/plain", "Redirecting to HTTPS");
      return false;
    }
  }
  if (!cfg.webAuth) return true;  // auth disabled — always allow
  if (httpServer.authenticate(cfg.webUser, cfg.webPass)) return true;
  httpServer.requestAuthentication(BASIC_AUTH, "ESP32-S3 CD-ROM", "Login required");
  return false;
}

void handleRoot() {
  if (!checkAuth()) return;
  httpServer.send_P(200, "text/html; charset=utf-8", HTML_PAGE);
}

void handleApiStatus() {
  if (!checkAuth()) return;
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

void handleApiLog() {
  if (!checkAuth()) return;
  // Optional: client sends ?seq=N to get only new lines since seq N
  uint32_t clientSeq = 0;
  if (httpServer.hasArg("seq")) clientSeq = (uint32_t)httpServer.arg("seq").toInt();
  
  // Build response: seq number + content since last request
  String resp = "{";
  resp += "\"seq\":" + String(webLogSeq) + ",";
  // Extract readable portion of ring buffer
  resp += "\"log\":";
  // Determine how much of the buffer to return
  uint32_t total = webLogHead;
  uint32_t start = (total > WEB_LOG_SIZE) ? (total - WEB_LOG_SIZE) : 0;
  resp += "\"";  // JSON string open
  // Escape and output
  for (uint32_t i = start; i < total; i++) {
    char ch = webLogBuf[i % WEB_LOG_SIZE];
    if      (ch == '"')  resp += "\\\""; 
    else if (ch == '\\') resp += "\\\\";
    else if (ch == '\n') resp += "\\n";
    else if (ch == '\r') continue;
    else                  resp += ch;
  }
  resp += "\"}";
  httpServer.send(200, "application/json", resp);
}

void handleApiSysinfo() {
  if (!checkAuth()) return;
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
    j += ",\"wifi_tx_power\":"  + String(cfg.wifiTxPower / 4);  // dBm
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
  j += ",\"dos_compat\":" + String(cfg.dosCompat ? "true" : "false");
  j += ",\"dos_driver\":" + String(cfg.dosDriver);
  j += ",\"debug_mode\":" + String(cfg.debugMode ? "true" : "false");
  j += ",\"win98_stop_ms\":" + String(cfg.win98StopMs);
  j += ",\"device_mode\":" + String(cfg.deviceMode);
  j += ",\"gotek_dir\":\"" + jsonEsc(gotekImgDir.c_str()) + "\"";
  j += ",\"gotek_usb_mode\":" + String(cfg.gotekUsbMode);
  j += ",\"gotek_fat_wp\":"  + String(cfg.gotekFatWP);
  j += ",\"gotek_files\":" + String(gkFC);
  j += ",\"gotek_ready\":" + String(gkRdy ? "true" : "false");
  j += ",\"gotek_cur_slot\":" + String(gkCurSlot);
  // Also expose the slotNum (display ID) of the currently active slot
  {
    int sn = -1;
    if (gkCurSlot >= 0 && gkCurSlot < gkFC) sn = (int)gkF[gkCurSlot].slotNum;
    j += ",\"gotek_cur_slotnum\":" + String(sn);
  }
  j += ",\"gotek_selected_slot\":" + String(gkSelectedSlot);
  j += ",\"httpsEnabled\":" + String(cfg.httpsEnabled ? "true" : "false");
  j += ",\"httpsCert\":\"" + jsonEsc(cfg.httpsCertPath) + "\"";
  j += ",\"httpsCert\":\"" + jsonEsc(cfg.httpsCertPath) + "\"";
  j += ",\"httpsKey\":\"" + jsonEsc(cfg.httpsKeyPath) + "\"";
  j += ",\"httpPort\":" + String(cfg.httpPort ? cfg.httpPort : 80);
  j += ",\"httpsPort\":" + String(cfg.httpsPort ? cfg.httpsPort : 443);
  j += ",\"tlsMinVer\":" + String(cfg.tlsMinVer);
  j += ",\"tlsCiphers\":" + String(cfg.tlsCiphers);
  j += ",\"wifiTxPower\":" + String(cfg.wifiTxPower);
  j += ",\"httpsKPassSet\":" + String(strlen(cfg.httpsKeyPass) ? "true" : "false");
  j += ",\"audio_module\":" + String(cfg.audioModule ? "true" : "false");
  j += ",\"i2s_ready\":" + String(i2sTx ? "true" : "false");
  j += ",\"audio_state\":" + String((int)audioState);
  j += ",\"audio_volume\":" + String(audioVolume);
  j += ",\"audio_muted\":" + String(audioMuted ? "true" : "false");
  j += ",\"audio_tracks\":" + String(audioTrackCount);

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
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(200, "application/json", "[]"); return; }
  File root = SD_MMC.open("/");
  if (!root) { httpServer.send(200, "application/json", "[]"); return; }
  String json = "["; bool first = true;
  collectIsos(root, "/", json, first);
  root.close(); json += "]";
  httpServer.send(200, "application/json", json);
}

void handleApiLs() {
  if (!checkAuth()) return;
  String path = httpServer.hasArg("path") ? normPath(httpServer.arg("path")) : "/";
  if (cfg.debugMode) dualPrint.printf("[API]  GET /api/ls path=%s\n", path.c_str());
  if (!sdReady) { httpServer.send(200, "application/json", "[]"); return; }
  File dir = SD_MMC.open(path.c_str());
  if (!dir) { httpServer.send(404, "application/json", "[]"); return; }
  String json = "["; bool first = true; int count = 0;
  File f;
  while ((f = dir.openNextFile())) {
    String n = String(f.name()); bool isD = f.isDirectory();
    if (cfg.debugMode) dualPrint.printf("[LS]     '%s' dir=%d size=%lu\n", f.name(), (int)isD, (uint32_t)f.size());
    if (!first) json += ",";
    json += "{\"name\":\"" + jsonEsc(f.name()) + "\",\"dir\":" + (isD?"true":"false")
          + ",\"size\":" + String((uint32_t)f.size()) + "}";
    first = false; count++; f.close();
  }
  dir.close(); json += "]";
  if (cfg.debugMode) dualPrint.printf("[API]  ls: %d items\n", count);
  httpServer.send(200, "application/json", json);
}

void handleApiMount() {
  if (!checkAuth()) return;
  if (!httpServer.hasArg("file")) { httpServer.send(400, "text/plain", "Missing 'file'"); return; }
  String f = httpServer.arg("file");
  if (doMount(f)) httpServer.send(200, "text/plain", "OK: Mounted: " + f);
  else            httpServer.send(500, "text/plain", "ERROR: Cannot mount: " + f);
}

void handleApiUmount() {
  if (!checkAuth()) return;
  doUmount();
  httpServer.send(200, "text/plain", "OK: Ejected.");
}


// Safe SD card unmount -- ejects disc image, then deinits SD_MMC so the card
// can be physically removed without filesystem corruption.
void handleApiSdUnmount() {
  if (!checkAuth()) return;
  // Refuse if a file transfer is actively in progress
  if (uploadActive) {
    dualPrint.println(F("[SD]   Unmount blocked: upload in progress."));
    httpServer.send(409, "text/plain", "ERROR: Upload in progress - wait for it to finish first.");
    return;
  }
  if (downloadActive) {
    dualPrint.println(F("[SD]   Unmount blocked: download in progress."));
    httpServer.send(409, "text/plain", "ERROR: Download in progress - wait for it to finish first.");
    return;
  }

  // Step 1: eject disc image from USB (tud_disconnect + reconnect without media)
  if (isoOpen || mscMediaPresent) {
    dualPrint.println(F("[SD]   Ejecting disc image before SD unmount..."));
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
    dualPrint.println(F("[SD]   Safely unmounted. Card can be removed."));
    httpServer.send(200, "text/plain", "OK: SD card safely unmounted. You can now remove the card.");
  } else {
    httpServer.send(200, "text/plain", "OK: SD was already unmounted.");
  }
}

// Remount SD card after physical insertion.
void handleApiSdMount() {
  if (!checkAuth()) return;
  if (sdReady) {
    SD_MMC.end(); delay(100);
  }
  sdReady = initSD();
  if (sdReady) {
    dualPrint.println(F("[SD]   Remounted OK."));
    httpServer.send(200, "text/plain", "OK: SD card mounted.");
  } else {
    httpServer.send(500, "text/plain", "ERROR: SD card not found. Is the card inserted?");
  }
}

void handleApiSetDefault() {
  if (!checkAuth()) return;
  String target = httpServer.hasArg("file") ? httpServer.arg("file") : mountedFile;
  if (!target.length()) { httpServer.send(400, "text/plain", "Nothing mounted"); return; }
  defaultMount = target;
  prefs.begin("cfg", false);
  prefs.putString("defmount", defaultMount);
  prefs.end();
  dualPrint.printf("[OK]  Default mount set: %s\n", defaultMount.c_str());
  httpServer.send(200, "text/plain", "OK: Default set: " + defaultMount);
}

void handleApiClearDefault() {
  if (!checkAuth()) return;
  defaultMount = "";
  prefs.begin("cfg", false);
  prefs.putString("defmount", "");
  prefs.end();
  dualPrint.println(F("[OK]  Default mount cleared."));
  httpServer.send(200, "text/plain", "OK: Default cleared.");
}


void handleApiWifiScan() {
  if (!checkAuth()) return;
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
  if (!checkAuth()) return;
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
  j += ",\"audioModule\":" + String(cfg.audioModule ? "true" : "false");
  j += ",\"webAuth\":" + String(cfg.webAuth ? "true" : "false");
  j += ",\"webUser\":\"" + jsonEsc(cfg.webUser) + "\"";
  j += ",\"dosCompat\":" + String(cfg.dosCompat ? "true" : "false");
  j += ",\"dosDriver\":" + String(cfg.dosDriver);
  j += ",\"debugMode\":" + String(cfg.debugMode ? "true" : "false");
  j += ",\"win98StopMs\":" + String(cfg.win98StopMs);
  j += ",\"gotekUsbMode\":" + String(cfg.gotekUsbMode);
  j += ",\"gotekFatWP\":"  + String(cfg.gotekFatWP);
  j += ",\"deviceMode\":" + String(cfg.deviceMode);
  j += ",\"gotekDir\":\"" + jsonEsc(gotekImgDir.c_str()) + "\"";
  j += ",\"httpsEnabled\":" + String(cfg.httpsEnabled ? "true" : "false");
  j += ",\"httpsCert\":\"" + jsonEsc(cfg.httpsCertPath) + "\"";
  j += ",\"httpsKey\":\"" + jsonEsc(cfg.httpsKeyPath) + "\"";
  j += ",\"httpPort\":" + String(cfg.httpPort ? cfg.httpPort : 80);
  j += ",\"httpsPort\":" + String(cfg.httpsPort ? cfg.httpsPort : 443);
  j += ",\"tlsMinVer\":" + String(cfg.tlsMinVer);
  j += ",\"tlsCiphers\":" + String(cfg.tlsCiphers);
  j += ",\"wifiTxPower\":" + String(cfg.wifiTxPower);
  // webPass intentionally omitted from GET response
  j += "}";
  httpServer.send(200, "application/json", j);
}

void handleApiSaveConfig() {
  if (!checkAuth()) return;
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
  if (httpServer.hasArg("audioModule")) {
    String v=httpServer.arg("audioModule");
    bool newVal=(v=="1"||v=="true"||v=="on");
    if (newVal != cfg.audioModule) {
      cfg.audioModule = newVal;
      if (cfg.audioModule) {
        // Enable: init I2S immediately if not already running
        if (!i2sTx) initI2S();
        dualPrint.println(F("[I2S]  Audio module enabled at runtime."));
      } else {
        // Disable: safe cross-core teardown sequence
        // 1. Signal audio task to stop (volatile write, visible to core 0)
        audioStop();
        if (i2sTx) {
          // 2. Null out handle FIRST — audio task sees nullptr, skips i2s_channel_write
          i2s_chan_handle_t h = i2sTx;
          i2sTx = nullptr;
          // 3. Wait for audio task to exit any in-flight i2s_channel_write (max one call ~30ms)
          delay(50);
          // 4. Safe to disable and delete now — no concurrent access possible
          i2s_channel_disable(h);
          i2s_del_channel(h);
          dualPrint.println(F("[I2S]  Audio module disabled at runtime."));
        }
      }
    }
    changed=true;
  }
  if (httpServer.hasArg("webAuth"))    { String v=httpServer.arg("webAuth");    cfg.webAuth=(v=="1"||v=="true"||v=="on"); changed=true; }
  if (httpServer.hasArg("deviceMode")) {
    String v=httpServer.arg("deviceMode");
    cfg.deviceMode=(uint8_t)constrain(v.toInt(),0,1); changed=true;
  }
  if (httpServer.hasArg("gotekUsbMode")) {
    String v=httpServer.arg("gotekUsbMode");
    cfg.gotekUsbMode=(uint8_t)constrain(v.toInt(),0,1); changed=true;
  }
  if (httpServer.hasArg("gotekFatWP")) {
    String v=httpServer.arg("gotekFatWP");
    uint8_t newWP=(uint8_t)constrain(v.toInt(),0,1);
    if (newWP != cfg.gotekFatWP) {
      cfg.gotekFatWP=newWP; changed=true;
      // Re-enumerate USB immediately so Windows sees new write-protect state
      if (cfg.deviceMode==1 && cfg.gotekUsbMode==1) {
        tud_disconnect(); delay(400);
        msc.isWritable(cfg.gotekFatWP==0);
        msc.begin(gkBlockCount(), GK_SECTOR_SIZE);
        msc.mediaPresent(true);
        tud_connect();
        dualPrint.printf("[GOTEK] FAT32 WP changed to %s — USB re-enumerated\n",
          cfg.gotekFatWP ? "ON" : "OFF");
      }
    }
  }
  if (httpServer.hasArg("gotekDir")) {
    String v=httpServer.arg("gotekDir"); v.trim();
    if (v.length()) { if (v[0]!='/') v="/"+v; gotekImgDir=v; }
    changed=true;
  }
  if (httpServer.hasArg("dosCompat"))  {
    String v=httpServer.arg("dosCompat");
    cfg.dosCompat=(v=="1"||v=="true"||v=="on");
    if (!cfg.dosCompat) cfg.dosDriver=0;  // driver modes only meaningful with dosCompat ON
    changed=true;
  }
  if (httpServer.hasArg("dosDriver"))  {
    cfg.dosDriver=(uint8_t)constrain(httpServer.arg("dosDriver").toInt(),0,3); changed=true;
    if (cfg.dosDriver > 0) cfg.dosCompat=true;  // any specific driver mode requires dosCompat ON
  }
  if (httpServer.hasArg("win98StopMs")) {
    cfg.win98StopMs=(uint16_t)constrain(httpServer.arg("win98StopMs").toInt(),0,9999); changed=true;
  }
  if (httpServer.hasArg("httpsEnabled")) { String v=httpServer.arg("httpsEnabled"); cfg.httpsEnabled=(v=="1"||v=="true"||v=="on"); changed=true; }
  if (httpServer.hasArg("httpsCert")) { strlcpy(cfg.httpsCertPath, httpServer.arg("httpsCert").c_str(), sizeof(cfg.httpsCertPath)); changed=true; }
  if (httpServer.hasArg("httpsKey"))  { strlcpy(cfg.httpsKeyPath,  httpServer.arg("httpsKey").c_str(),  sizeof(cfg.httpsKeyPath));  changed=true; }
  if (httpServer.hasArg("httpPort"))   { int v=httpServer.arg("httpPort").toInt(); if(v>0&&v<65536) cfg.httpPort=(uint16_t)v; changed=true; }
  if (httpServer.hasArg("httpsPort"))  { int v=httpServer.arg("httpsPort").toInt(); if(v>0&&v<65536) cfg.httpsPort=(uint16_t)v; changed=true; }
  if (httpServer.hasArg("tlsMinVer"))  { cfg.tlsMinVer=(uint8_t)constrain(httpServer.arg("tlsMinVer").toInt(),0,1); changed=true; }
  if (httpServer.hasArg("tlsCiphers")) { cfg.tlsCiphers=(uint8_t)constrain(httpServer.arg("tlsCiphers").toInt(),0,3); changed=true; }
  if (httpServer.hasArg("wifiTxPower")) {
    cfg.wifiTxPower=(uint8_t)constrain(httpServer.arg("wifiTxPower").toInt(),8,80); changed=true;
    if (wifiConnected) esp_wifi_set_max_tx_power(cfg.wifiTxPower); // apply live without reboot
  }
  if (httpServer.hasArg("httpsKPass")){ strlcpy(cfg.httpsKeyPass, httpServer.arg("httpsKPass").c_str(), sizeof(cfg.httpsKeyPass)); changed=true; }
  if (httpServer.hasArg("webUser") && httpServer.arg("webUser").length()) { strlcpy(cfg.webUser, httpServer.arg("webUser").c_str(), sizeof(cfg.webUser)); changed=true; }
  if (httpServer.hasArg("webPass") && httpServer.arg("webPass").length()) { strlcpy(cfg.webPass, httpServer.arg("webPass").c_str(), sizeof(cfg.webPass)); changed=true; }
  if (changed) {
    saveConfig();
    dualPrint.println(F("[OK]  Config saved via web."));
    httpServer.send(200, "text/plain", "OK: Configuration saved. Reboot to apply WiFi changes.");
  } else {
    httpServer.send(400, "text/plain", "No parameters provided.");
  }
}

void handleApiReboot() {
  if (!checkAuth()) return;
  httpServer.send(200, "text/plain", "OK: Rebooting...");
  delay(500);
  ESP.restart();
}

void handleApiFactoryReset() {
  if (!checkAuth()) return;
  clearConfig();
  httpServer.send(200, "text/plain", "OK: Factory reset done. Rebooting...");
  delay(500);
  ESP.restart();
}

void handleApiDownload() {
  if (!checkAuth()) return;
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
  dualPrint.printf("[DOWNLOAD] Start: %s  (%zu B)\n", path.c_str(), fileSize);

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
      dualPrint.println("[DOWNLOAD] Client disconnected - aborting.");
      break;
    }
    size_t toRead = min((size_t)sizeof(buf), fileSize - sent);
    int rd = f.read(buf, toRead);
    if (rd <= 0) break;
    size_t wr = client.write(buf, (size_t)rd);
    if (wr > 0) { sent += wr; lastOk = millis(); }
    if (millis() - lastOk > 5000) {
      dualPrint.println("[DOWNLOAD] Timeout - client stopped reading.");
      break;
    }
    yield();
  }

  downloadActive = false;
  f.close();
  client.stop();
  dualPrint.printf("[DOWNLOAD] Done: %zu / %zu B sent.\n", sent, fileSize);
}

void handleApiMkdir() {
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(503, "text/plain", "SD not available."); return; }
  if (!httpServer.hasArg("path")) { httpServer.send(400, "text/plain", "Missing 'path'"); return; }
  String path = normPath(httpServer.arg("path"));
  if (SD_MMC.mkdir(path.c_str())) httpServer.send(200, "text/plain", "OK: Created: " + path);
  else                             httpServer.send(500, "text/plain", "ERROR: mkdir failed: " + path);
}

void handleApiDelete() {
  if (!checkAuth()) return;
  if (!sdReady) { httpServer.send(503, "text/plain", "SD not available."); return; }
  if (!httpServer.hasArg("path")) { httpServer.send(400, "text/plain", "Missing 'path'"); return; }
  String path = normPath(httpServer.arg("path"));
  if (path == mountedFile) doUmount();
  if (deleteRecursive(path)) httpServer.send(200, "text/plain", "OK: Deleted: " + path);
  else                        httpServer.send(500, "text/plain", "ERROR: delete failed: " + path);
}

void handleApiUploadDone() {
  if (!checkAuth()) return;
  if (uploadOk) httpServer.send(200, "text/plain", "OK");
  else          httpServer.send(500, "text/plain", "ERROR: upload failed");
}

// =============================================================================
//  AUDIO API HANDLERS
// =============================================================================
void handleApiAudioStatus() {
  if (!checkAuth()) return;
  String j = "{";
  j += "\"state\":" + String((int)audioState);
  j += ",\"lba\":"  + String(audioCurrentLba);
  j += ",\"end_lba\":" + String(audioEndLba);
  j += ",\"volume\":" + String(audioVolume);
  j += ",\"muted\":"  + String(audioMuted ? "true" : "false");
  j += ",\"track_count\":" + String(audioTrackCount);
  j += ",\"sub\":{";
  j += "\"track\":"  + String(subChannel.trackNum);
  j += ",\"absM\":"  + String(subChannel.absM);
  j += ",\"absS\":"  + String(subChannel.absS);
  j += ",\"absF\":"  + String(subChannel.absF);
  j += ",\"relM\":"  + String(subChannel.relM);
  j += ",\"relS\":"  + String(subChannel.relS);
  j += ",\"relF\":"  + String(subChannel.relF);
  j += "}";
  j += ",\"tracks\":[";
  for (int i = 0; i < audioTrackCount; i++) {
    if (i) j += ",";
    j += "{\"num\":" + String(audioTracks[i].number);
    j += ",\"lba\":"  + String(audioTracks[i].startLba);
    j += ",\"len\":"  + String(audioTracks[i].lengthSectors);
    String fname = audioTracks[i].binFile;
    int sl = fname.lastIndexOf('/'); if (sl >= 0) fname = fname.substring(sl+1);
    int dot = fname.lastIndexOf('.'); if (dot > 0) fname = fname.substring(0, dot);
    j += ",\"title\":\"" + jsonEsc(fname.c_str()) + "\"";
    j += "}";
  }
  j += "]}";
  httpServer.send(200, "application/json", j);
}

void handleApiAudioPlay() {
  if (!checkAuth()) return;
  if (httpServer.hasArg("track")) {
    int tn = httpServer.arg("track").toInt();
    for (int i = 0; i < audioTrackCount; i++) {
      if (audioTracks[i].number == tn) {
        uint32_t end = audioTracks[i].startLba + audioTracks[i].lengthSectors;
        audioPlay(audioTracks[i].startLba, end);
        httpServer.send(200,"application/json",
          "{\"ok\":true,\"track\":" + String(tn) + "}");
        return;
      }
    }
    httpServer.send(404,"application/json","{\"ok\":false,\"err\":\"track not found\"}");
  } else if (httpServer.hasArg("lba")) {
    uint32_t lba = httpServer.arg("lba").toInt();
    uint32_t end = httpServer.hasArg("end") ? httpServer.arg("end").toInt() : lba+75*3;
    audioPlay(lba, end);
    httpServer.send(200,"application/json","{\"ok\":true}");
  } else {
    httpServer.send(400,"application/json","{\"ok\":false}");
  }
}

void handleApiAudioStop()   {
  if (!checkAuth()) return; audioStop();   httpServer.send(200,"application/json","{\"ok\":true}"); }
void handleApiAudioPause()  {
  if (!checkAuth()) return; audioPause();  httpServer.send(200,"application/json","{\"ok\":true}"); }
void handleApiAudioResume() {
  if (!checkAuth()) return; audioResume(); httpServer.send(200,"application/json","{\"ok\":true}"); }

void handleApiAudioVolume() {
  if (!checkAuth()) return;
  if (httpServer.hasArg("v")) {
    int v = httpServer.arg("v").toInt();
    if (v < 0) v = 0; if (v > 100) v = 100;
    audioVolume = v; audioMuted = false;
  }
  httpServer.send(200,"application/json",
    "{\"ok\":true,\"volume\":" + String(audioVolume) + "}");
}

void handleApiAudioMute() {
  if (!checkAuth()) return;
  if (httpServer.hasArg("m")) audioMuted = (httpServer.arg("m") == "1");
  else audioMuted = !audioMuted;
  httpServer.send(200,"application/json",
    "{\"ok\":true,\"muted\":" + String(audioMuted?"true":"false") + "}");
}

void handleApiAudioSeek() {
  if (!checkAuth()) return;
  if (!httpServer.hasArg("track") || !httpServer.hasArg("rel"))
    { httpServer.send(400,"application/json","{\"ok\":false}"); return; }
  int tn = httpServer.arg("track").toInt();
  float rel = httpServer.arg("rel").toFloat();
  if (rel < 0) rel = 0; if (rel > 1) rel = 1;
  for (int i = 0; i < audioTrackCount; i++) {
    if (audioTracks[i].number == tn) {
      uint32_t newLba = audioTracks[i].startLba +
                        (uint32_t)(rel * audioTracks[i].lengthSectors);
      uint32_t endLba = audioTracks[i].startLba + audioTracks[i].lengthSectors;
      // Use audioPlay() not just audioCurrentLba — this works from any state
      // (stopped, paused, playing) and takes effect within one batch cycle.
      audioPlay(newLba, endLba);
      httpServer.send(200,"application/json","{\"ok\":true}"); return;
    }
  }
  httpServer.send(404,"application/json","{\"ok\":false}");
}

// =============================================================================
//  HTTPS SERVER (ESP-IDF httpd_ssl + loopback proxy to WebServer on port 80)
//  Architecture:
//    Port 443 → esp_https_server → proxy to 127.0.0.1:80 with X-Internal-Proxy header
//    Port 80  → WebServer → if X-Internal-Proxy absent + httpsEnabled → 301 redirect
//    Port 80  → WebServer → if X-Internal-Proxy present → serve normally
// =============================================================================

static char*       s_tlsCertBuf = nullptr;
static char*       s_tlsKeyBuf  = nullptr;
bool loadTlsCerts() {
  if (!sdReady) return false;
  if (strlen(cfg.httpsCertPath)) {
    File f = SD_MMC.open(cfg.httpsCertPath, FILE_READ);
    if (f) {
      size_t sz = f.size();
      free(s_tlsCertBuf); s_tlsCertBuf = (char*)malloc(sz + 1);
      if (s_tlsCertBuf) { f.read((uint8_t*)s_tlsCertBuf, sz); s_tlsCertBuf[sz] = 0;
        // Strip Windows CRLF → LF (mbedTLS requires LF only)
        char *r = s_tlsCertBuf, *w = s_tlsCertBuf;
        while (*r) { if (!(*r == '\r' && *(r+1) == '\n')) *w++ = *r; r++; }
        *w = 0;
      }
      f.close();
      dualPrint.printf("[HTTPS] Cert loaded: %s (%u B)\n", cfg.httpsCertPath, sz);
    if (strstr(s_tlsCertBuf, "-----BEGIN") == nullptr)
      dualPrint.println(F("[HTTPS] WARNING: cert does not look like PEM!"));
    // Parse cert with mbedTLS to get key size info
    {
      mbedtls_x509_crt crt; mbedtls_x509_crt_init(&crt);
      int ret = mbedtls_x509_crt_parse(&crt,
        (const unsigned char*)s_tlsCertBuf, strlen(s_tlsCertBuf)+1);
      if (ret == 0) {
        size_t bits = mbedtls_pk_get_bitlen(&crt.pk);
        const char* pktype = mbedtls_pk_get_name(&crt.pk);
        dualPrint.printf("[HTTPS] Cert: %s %u-bit", pktype, (unsigned)bits);
        if (bits > 2048) dualPrint.print(F(" WARNING: >2048-bit may fail on ESP32!"));
        dualPrint.println();
      } else {
        dualPrint.printf("[HTTPS] mbedTLS parse error: -0x%04X\n", (unsigned)(-ret));
      }
      mbedtls_x509_crt_free(&crt);
    }
    } else { dualPrint.printf("[HTTPS] Cannot open cert: %s\n", cfg.httpsCertPath); return false; }
  } else { dualPrint.println(F("[HTTPS] No cert path configured")); return false; }
  if (strlen(cfg.httpsKeyPath)) {
    File f = SD_MMC.open(cfg.httpsKeyPath, FILE_READ);
    if (f) {
      size_t sz = f.size();
      free(s_tlsKeyBuf); s_tlsKeyBuf = (char*)malloc(sz + 1);
      if (s_tlsKeyBuf) { f.read((uint8_t*)s_tlsKeyBuf, sz); s_tlsKeyBuf[sz] = 0;
        char *r = s_tlsKeyBuf, *w = s_tlsKeyBuf;
        while (*r) { if (!(*r == '\r' && *(r+1) == '\n')) *w++ = *r; r++; }
        *w = 0;
      }
      f.close();
      dualPrint.printf("[HTTPS] Key  loaded: %s (%u B)\n", cfg.httpsKeyPath, sz);
    if (strstr(s_tlsKeyBuf, "-----BEGIN") == nullptr)
      dualPrint.println(F("[HTTPS] WARNING: key does not look like PEM format!"));
    if (strstr(s_tlsKeyBuf, "ENCRYPTED") != nullptr && !strlen(cfg.httpsKeyPass))
      dualPrint.println(F("[HTTPS] WARNING: key is encrypted but no passphrase set!"));
    } else { dualPrint.printf("[HTTPS] Cannot open key: %s\n", cfg.httpsKeyPath); return false; }
  } else { dualPrint.println(F("[HTTPS] No key path configured")); return false; }
  return true;
}

// Wildcard HTTPS handler: decrypt TLS then proxy to WebServer on port 80
static esp_err_t httpsProxyHandler(httpd_req_t *req) {
  // Serialise: one loopback request at a time, max wait 8s
  if (!s_proxyMutex || xSemaphoreTake(s_proxyMutex, pdMS_TO_TICKS(8000)) != pdTRUE) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Proxy busy");
    return ESP_FAIL;
  }
  // Reuse persistent connection or reconnect
  if (!s_proxyClient.connected()) {
    s_proxyClient.stop();
    if (!s_proxyClient.connect("127.0.0.1", 80)) {
      xSemaphoreGive(s_proxyMutex);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Loopback failed");
      return ESP_FAIL;
    }
  }
  WiFiClient& client = s_proxyClient;
  // Build HTTP/1.0 request with internal marker header
  const char* method = (req->method == HTTP_GET)  ? "GET"  :
                       (req->method == HTTP_POST)  ? "POST" :
                       (req->method == HTTP_PUT)   ? "PUT"  : "DELETE";
  String req_str = String(method) + " " + String(req->uri) + " HTTP/1.0\r\n";
  req_str += "Host: localhost\r\n";
  req_str += "X-Internal-Proxy: 1\r\n";  // skip redirect check
  // Forward key request headers
  char authBuf[128] = {};
  if (httpd_req_get_hdr_value_str(req, "Authorization", authBuf, sizeof(authBuf)) == ESP_OK)
    req_str += "Authorization: " + String(authBuf) + "\r\n";
  char cookieBuf[256] = {};
  if (httpd_req_get_hdr_value_str(req, "Cookie", cookieBuf, sizeof(cookieBuf)) == ESP_OK)
    req_str += "Cookie: " + String(cookieBuf) + "\r\n";
  // Forward Content-Type for POST
  if (req->content_len > 0) {
    char ctBuf[256] = "application/octet-stream";  // must fit multipart boundary
    httpd_req_get_hdr_value_str(req, "Content-Type", ctBuf, sizeof(ctBuf));
    req_str += "Content-Type: " + String(ctBuf) + "\r\n";
    req_str += "Content-Length: " + String(req->content_len) + "\r\n";
  }
  req_str += "\r\n";
  client.print(req_str);
  // Forward body
  if (req->content_len > 0) {
    char bodyBuf[4096]; size_t rem = req->content_len;  // larger = fewer iterations
    while (rem > 0) {
      int got = httpd_req_recv(req, bodyBuf, min((size_t)sizeof(bodyBuf), rem));
      if (got <= 0) break;
      client.write((uint8_t*)bodyBuf, got); rem -= got;
    }
  }
  // Wait for response
  unsigned long t0 = millis();
  while (!client.available() && millis() - t0 < 30000) delay(1);  // 30s for uploads
  // Parse status line
  String statusLine = client.readStringUntil('\n'); statusLine.trim();
  int code = 200;
  if (statusLine.startsWith("HTTP/")) {
    int s1 = statusLine.indexOf(' '), s2 = statusLine.indexOf(' ', s1+1);
    if (s1 > 0) code = statusLine.substring(s1+1, s2 > s1 ? s2 : s1+4).toInt();
  }
  // Parse response headers
  String contentType = "text/plain";
  String location = "";
  String wwwAuth = "";    // persistent buffer for WWW-Authenticate
  String setCookie = "";  // persistent buffer for Set-Cookie
  while (client.available()) {
    String hdr = client.readStringUntil('\n'); hdr.trim();
    if (!hdr.length()) break;
    String lhdr = hdr; lhdr.toLowerCase();
    if (lhdr.startsWith("content-type:"))    { contentType = hdr.substring(13); contentType.trim(); }
    if (lhdr.startsWith("location:"))         { location = hdr.substring(9); location.trim(); }
    if (lhdr.startsWith("www-authenticate:")) { wwwAuth = hdr.substring(17); wwwAuth.trim(); }
    if (lhdr.startsWith("set-cookie:"))       { setCookie = hdr.substring(11); setCookie.trim(); }
  }
  // Send response — set all headers before status
  char statusStr[16]; snprintf(statusStr, sizeof(statusStr), "%d %s", code,
    code==200?"OK":code==401?"Unauthorized":code==404?"Not Found":"Error");
  httpd_resp_set_status(req, statusStr);
  httpd_resp_set_type(req, contentType.c_str());
  if (location.length())  httpd_resp_set_hdr(req, "Location", location.c_str());
  if (wwwAuth.length())   httpd_resp_set_hdr(req, "WWW-Authenticate", wwwAuth.c_str());
  if (setCookie.length()) httpd_resp_set_hdr(req, "Set-Cookie", setCookie.c_str());
  // Stream body
  char rbuf[4096];
  while (client.connected() || client.available()) {
    int n = client.read((uint8_t*)rbuf, sizeof(rbuf));
    if (n > 0) httpd_resp_send_chunk(req, rbuf, n);
    else if (!client.connected()) break;
    else delay(1);
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  // Keep connection alive; WebServer closes on HTTP/1.0 — reconnect next time
  xSemaphoreGive(s_proxyMutex);
  return ESP_OK;
}


// =============================================================================
//  DIRECT HTTPS UPLOAD HANDLER (bypasses loopback proxy for max speed)
// =============================================================================
static esp_err_t httpsUploadHandler(httpd_req_t *req) {
  // Get target path from query string
  char pathBuf[128] = "/";
  if (httpd_req_get_url_query_len(req) > 0) {
    char qbuf[256] = {};
    httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf));
    char rawPath[128] = {};
    httpd_query_key_value(qbuf, "path", rawPath, sizeof(rawPath));
    urlDecode(pathBuf, rawPath, sizeof(pathBuf));
  }
  // Check auth via X-Internal-Proxy trick: use the real checkAuth logic
  // by checking Authorization header manually
  char authBuf[128] = {};
  if (cfg.webAuth) {
    httpd_req_get_hdr_value_str(req, "Authorization", authBuf, sizeof(authBuf));
    // Decode Basic auth: "Basic base64(user:pass)"
    bool authOk = false;
    if (strncmp(authBuf, "Basic ", 6) == 0) {
      // Decode base64
      const char* b64 = authBuf + 6;
      size_t b64len = strlen(b64);
      size_t outlen = 0;
      unsigned char decoded[128] = {};
      if (mbedtls_base64_decode(decoded, sizeof(decoded)-1, &outlen,
                                 (const unsigned char*)b64, b64len) == 0) {
        char expected[128];
        snprintf(expected, sizeof(expected), "%s:%s", cfg.webUser, cfg.webPass);
        authOk = (strcmp((char*)decoded, expected) == 0);
      }
    }
    if (!authOk) {
      httpd_resp_set_status(req, "401 Unauthorized");
      httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32-S3 CD-ROM\"");
      httpd_resp_send(req, "Unauthorized", 12);
      return ESP_OK;
    }
  }
  // Parse multipart boundary from Content-Type header
  char ctBuf[256] = {};
  httpd_req_get_hdr_value_str(req, "Content-Type", ctBuf, sizeof(ctBuf));
  char* bndPos = strstr(ctBuf, "boundary=");
  if (!bndPos) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No boundary");
    return ESP_FAIL;
  }
  String boundary = "--"; boundary += (bndPos + 9);
  boundary.trim();
  // Read multipart body, parse headers, write file to SD
  File outFile;
  String fileName = "";
  bool inBody = false;
  bool writeError = false;
  size_t written = 0;
  static char buf[4096];
  String carry = "";  // leftover data between chunks
  size_t remaining = req->content_len;
  while (remaining > 0 || carry.length() > 0) {
    int toRead = min((size_t)sizeof(buf)-1, remaining);
    int got = toRead > 0 ? httpd_req_recv(req, buf, toRead) : 0;
    if (got < 0) break;
    remaining -= got;
    buf[got] = 0;
    carry += String(buf).substring(0, got);
    // Process line by line
    while (true) {
      int nl = carry.indexOf("\r\n");
      if (nl < 0 && remaining > 0) break;  // need more data
      String line = (nl >= 0) ? carry.substring(0, nl) : carry;
      carry = (nl >= 0) ? carry.substring(nl + 2) : "";
      if (line.startsWith(boundary)) {
        if (outFile) { outFile.close(); outFile = File(); }
        inBody = false;
        // Don't clear fileName — needed for done log
      } else if (!inBody && line.startsWith("Content-Disposition:")) {
        // Extract filename
        int fi = line.indexOf("filename=\"");
        if (fi >= 0) {
          int fe = line.indexOf("\"", fi + 10);
          fileName = line.substring(fi + 10, fe);
        }
      } else if (!inBody && line.length() == 0 && fileName.length() > 0) {
        // Blank line = body starts
        String dir = String(pathBuf);
        String fpath = (dir == "/" ? "" : dir) + "/" + fileName;
        dualPrint.printf("[UPLOAD] Start: %s\n", fpath.c_str());
        outFile = SD_MMC.open(fpath.c_str(), FILE_WRITE);
        writeError = !outFile;
        written = 0; inBody = true;
      } else if (inBody) {
        // Check if this is the last line before boundary (strip trailing \r\n)
        bool isLast = carry.startsWith(boundary);
        String data = isLast ? line : line + "\r\n";
        if (outFile && data.length() > 0) {
          if (outFile.write((uint8_t*)data.c_str(), data.length()) != data.length())
            writeError = true;
          written += data.length();
        }
      }
      if (nl < 0) break;
    }
  }
  if (outFile) outFile.close();
  if (writeError) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
    return ESP_FAIL;
  }
  dualPrint.printf("[UPLOAD] OK: %s  (%u B)\n", fileName.c_str(), written);
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}



// URL-decode a string in-place (e.g. %2F → /)
static void urlDecode(char* dst, const char* src, size_t dstSize) {
  size_t di = 0;
  for (size_t i = 0; src[i] && di < dstSize - 1; i++) {
    if (src[i] == '%' && src[i+1] && src[i+2]) {
      char hex[3] = {src[i+1], src[i+2], 0};
      dst[di++] = (char)strtol(hex, nullptr, 16);
      i += 2;
    } else if (src[i] == '+') {
      dst[di++] = ' ';
    } else {
      dst[di++] = src[i];
    }
  }
  dst[di] = 0;
}
// Direct HTTPS download handler — reads SD and streams directly via TLS, no proxy
static esp_err_t httpsDownloadHandler(httpd_req_t *req) {
  // Auth check
  if (cfg.webAuth) {
    char authBuf[128] = {};
    httpd_req_get_hdr_value_str(req, "Authorization", authBuf, sizeof(authBuf));
    bool authOk = false;
    if (strncmp(authBuf, "Basic ", 6) == 0) {
      size_t outlen = 0;
      unsigned char decoded[128] = {};
      if (mbedtls_base64_decode(decoded, sizeof(decoded)-1, &outlen,
          (const unsigned char*)(authBuf+6), strlen(authBuf+6)) == 0) {
        char expected[128];
        snprintf(expected, sizeof(expected), "%s:%s", cfg.webUser, cfg.webPass);
        authOk = (strcmp((char*)decoded, expected) == 0);
      }
    }
    if (!authOk) {
      httpd_resp_set_status(req, "401 Unauthorized");
      httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32-S3 CD-ROM\"");
      httpd_resp_sendstr(req, "Unauthorized");
      return ESP_OK;
    }
  }
  // Get path from query string
  char qbuf[256] = {}; char pathBuf[128] = {};
  if (httpd_req_get_url_query_len(req) > 0) {
    httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf));
    char rawPath[128] = {};
    httpd_query_key_value(qbuf, "path", rawPath, sizeof(rawPath));
    urlDecode(pathBuf, rawPath, sizeof(pathBuf));
  }
  if (!pathBuf[0]) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path"); return ESP_FAIL; }
  if (!sdReady)    { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD unavailable"); return ESP_FAIL; }
  File f = SD_MMC.open(pathBuf, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found"); return ESP_FAIL;
  }
  // Extract filename for Content-Disposition
  String path = String(pathBuf);
  String fname = path.substring(path.lastIndexOf('/') + 1);
  size_t fileSize = f.size();
  dualPrint.printf("[DOWNLOAD] Start: %s  (%zu B)\n", pathBuf, fileSize);
  // Send headers
  char cdBuf[192];
  snprintf(cdBuf, sizeof(cdBuf), "attachment; filename=\"%s\"", fname.c_str());
  char szBuf[24];
  snprintf(szBuf, sizeof(szBuf), "%zu", fileSize);
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "Content-Disposition", cdBuf);
  httpd_resp_set_hdr(req, "Content-Length", szBuf);
  // Stream file directly via TLS
  downloadActive = true;
  static uint8_t dlBuf[8192];
  size_t sent = 0;
  while (sent < fileSize) {
    size_t toRead = min((size_t)sizeof(dlBuf), fileSize - sent);
    int rd = f.read(dlBuf, toRead);
    if (rd <= 0) break;
    if (httpd_resp_send_chunk(req, (char*)dlBuf, rd) != ESP_OK) break;
    sent += rd;
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  downloadActive = false;
  f.close();
  dualPrint.printf("[DOWNLOAD] Done: %zu / %zu B sent.\n", sent, fileSize);
  return ESP_OK;
}

static const httpd_uri_t httpsDownloadUri = {
  "/api/download", HTTP_GET, httpsDownloadHandler, nullptr
};

static const httpd_uri_t httpsUploadUri = {
  "/api/upload", HTTP_POST, httpsUploadHandler, nullptr
};

static const httpd_uri_t _httpsGet  = { "/*", HTTP_GET,    httpsProxyHandler, nullptr };
static const httpd_uri_t _httpsPost = { "/*", HTTP_POST,   httpsProxyHandler, nullptr };
static const httpd_uri_t _httpsPut  = { "/*", HTTP_PUT,    httpsProxyHandler, nullptr };


// TLS cipher suite presets for esp_https_server
// Note: the int* arrays must be static (NULL-terminated, valid for server lifetime)
static const int _ciphers_strong[] = {        // ECDHE + AES-GCM only (best security)
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
  MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
  MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
  0
};
static const int _ciphers_medium[] = {        // ECDHE + AES-GCM + AES-CBC
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
  0
};
static const int _ciphers_all[] = {           // All supported incl. legacy RSA
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
  MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,
  MBEDTLS_TLS_RSA_WITH_AES_256_GCM_SHA384,
  MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
  MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA256,
  0
};

// TLS handshake callback: apply cipher suite and version settings
static esp_err_t tlsPreConfigCb(void *ssl_params, void *user_ctx) {
  mbedtls_ssl_config *sslcfg = (mbedtls_ssl_config *)ssl_params;
  if (!sslcfg) return ESP_FAIL;
  // Apply cipher suite preset
  switch (cfg.tlsCiphers) {
    case 1: mbedtls_ssl_conf_ciphersuites(sslcfg, _ciphers_strong); break;
    case 2: mbedtls_ssl_conf_ciphersuites(sslcfg, _ciphers_medium); break;
    case 3: mbedtls_ssl_conf_ciphersuites(sslcfg, _ciphers_all);    break;
    default: break;  // 0=Auto: let mbedTLS decide
  }
  // Apply TLS minimum version
  if (cfg.tlsMinVer == 0) {
    // TLS 1.2 only
    mbedtls_ssl_conf_min_tls_version(sslcfg, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(sslcfg, MBEDTLS_SSL_VERSION_TLS1_2);
  }
  // tlsMinVer==1: leave default (TLS 1.2 + 1.3 if compiled in)
  return ESP_OK;
}

bool startHttpsServer() {
  if (httpsHandle) return true;
  if (!loadTlsCerts()) return false;
  if (!s_proxyMutex) s_proxyMutex = xSemaphoreCreateMutex();
  // Log HW crypto status
#ifdef CONFIG_MBEDTLS_HARDWARE_AES
  #define _HW_AES "AES=ON"
#else
  #define _HW_AES "AES=OFF"
#endif
#ifdef CONFIG_MBEDTLS_HARDWARE_SHA
  #define _HW_SHA " SHA=ON"
#else
  #define _HW_SHA " SHA=OFF"
#endif
#ifdef CONFIG_MBEDTLS_HARDWARE_MPI
  #define _HW_RSA " RSA=ON"
#else
  #define _HW_RSA " RSA=OFF"
#endif
  dualPrint.println(F("[HTTPS] HW crypto: " _HW_AES _HW_SHA _HW_RSA));
  // TLS logging: WARN level (DEBUG floods serial with handshake details)
  esp_log_level_set("esp-tls-mbedtls", ESP_LOG_WARN);
  esp_log_level_set("esp_https_server", ESP_LOG_WARN);
  httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
  // Apply configured ports
  conf.httpd.server_port = cfg.httpsPort ? cfg.httpsPort : 443;
  conf.httpd.uri_match_fn      = httpd_uri_match_wildcard;
  conf.httpd.max_uri_handlers  = 4;
  conf.httpd.stack_size        = 10240;
  conf.httpd.lru_purge_enable  = true;
  conf.httpd.max_open_sockets  = 2;    // max 2 concurrent TLS sessions (~64KB heap)
  conf.httpd.backlog_conn      = 1;    // TCP backlog=1: reject excess connections early
  conf.httpd.recv_wait_timeout = 10;
  conf.httpd.send_wait_timeout = 10;
  conf.session_tickets         = false;
  // Log free heap before TLS start
  dualPrint.printf("[HTTPS] Heap before TLS: %lu B\n", ESP.getFreeHeap());
  // PEM: length MUST include null terminator for mbedTLS
  // Apply TLS handshake callback for cipher/version configuration
#ifdef ESP_TLS_SERVER_HANDSHAKE_CB
  conf.tls_handshake_cb = tlsPreConfigCb;
#endif
  conf.servercert           = (const uint8_t*)s_tlsCertBuf;
  conf.servercert_len       = strlen(s_tlsCertBuf) + 1;
  conf.prvtkey_pem          = (const uint8_t*)s_tlsKeyBuf;
  conf.prvtkey_len          = strlen(s_tlsKeyBuf) + 1;
  // Note: encrypted private key passphrase requires manual decryption
  // ESP-IDF esp_https_server does not support encrypted keys directly
  // Use unencrypted key or decrypt with openssl before placing on SD
  esp_err_t ret = httpd_ssl_start(&httpsHandle, &conf);
  if (ret != ESP_OK) {
    dualPrint.printf("[HTTPS] Failed: %s\n", esp_err_to_name(ret));
    httpsHandle = nullptr; return false;
  }
  httpd_register_uri_handler(httpsHandle, &httpsDownloadUri); // direct, no proxy
  httpd_register_uri_handler(httpsHandle, &httpsUploadUri); // direct, no proxy
  httpd_register_uri_handler(httpsHandle, &_httpsGet);
  httpd_register_uri_handler(httpsHandle, &_httpsPost);
  httpd_register_uri_handler(httpsHandle, &_httpsPut);
  dualPrint.println(F("[HTTPS] Started on port 443"));
  return true;
}

void stopHttpsServer() {
  if (!httpsHandle) return;
  httpd_ssl_stop(httpsHandle);
  httpsHandle = nullptr;
  dualPrint.println(F("[HTTPS] Stopped"));
}

// HTTP→HTTPS redirect (called by onNotFound when httpsEnabled and no proxy header)
void handleHttpRedirect() {
  // Pass-through for internal proxy requests
  // If request has X-Internal-Proxy header, serve normally via 404
  if (httpServer.hasHeader("X-Internal-Proxy") &&
      httpServer.header("X-Internal-Proxy") == "1") { 
    httpServer.send(404, "text/plain", "Not found");
    return;
  }
  String host = httpServer.hostHeader();
  if (!host.length()) host = WiFi.localIP().toString();
  // Remove port from host if present
  int col = host.lastIndexOf(':');
  if (col > 0 && col > host.lastIndexOf(']')) host = host.substring(0, col);
  String url = "https://" + host + httpServer.uri();
  if (httpServer.args()) {
    url += "?";
    for (int i = 0; i < httpServer.args(); i++) {
      if (i) url += "&";
      url += httpServer.argName(i) + "=" + httpServer.arg(i);
    }
  }
  httpServer.sendHeader("Location", url, true);
  httpServer.send(301, "text/plain", "Redirecting to HTTPS");
}

void setupWebServer() {
  // Recreate WebServer with configured port
  uint16_t hport = cfg.httpPort ? cfg.httpPort : 80;
  httpServer.~WebServer();
  new (&httpServer) WebServer(hport);
  httpServer.on("/",                 HTTP_GET,  handleRoot);
  httpServer.on("/api/status",       HTTP_GET,  handleApiStatus);
  httpServer.on("/api/log",          HTTP_GET,  handleApiLog);
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
  httpServer.on("/api/img/ls",    HTTP_GET,  handleImgLs);
  httpServer.on("/api/img/rm",    HTTP_GET,  handleImgRm);
  httpServer.on("/api/img/get",   HTTP_GET,  handleImgGet);
  httpServer.on("/api/img/mkdir", HTTP_GET,  handleImgMkdir);
  httpServer.on("/api/img/put",   HTTP_POST, handleImgPutDone, handleImgPut);
  // GET /api/img/stat?img= — returns free/used/total bytes for a FAT12 image
  httpServer.on("/api/img/stat", HTTP_GET, [](){
    if (!checkAuth()) return;
    String imgPath=httpServer.hasArg("img")?httpServer.arg("img"):"";
    if (!imgPath.length()){httpServer.send(400,"application/json","{\"error\":\"Missing img\"}");return;}
    if (!gkMtxTake()){httpServer.send(503,"application/json","{\"error\":\"SD busy\"}");return;}
    if (!imgOpenSession(imgPath)){gkMtxGive();httpServer.send(400,"application/json","{\"error\":\"Bad FAT12 image\"}");return;}
    uint16_t freeClu=imgFreeClustersM();
    ImgBPB snap=g_sBPB;
    imgCloseSession(); gkMtxGive();
    uint32_t clusterBytes=(uint32_t)snap.spc*snap.bps;
    uint32_t freeBytes=freeClu*clusterBytes;
    uint32_t totalBytes=snap.dataClu*clusterBytes;
    uint32_t usedBytes=totalBytes>freeBytes?totalBytes-freeBytes:0;
    httpServer.send(200,"application/json",
      "{\"ok\":true,\"free_bytes\":"+String(freeBytes)
      +",\"used_bytes\":"+String(usedBytes)
      +",\"total_bytes\":"+String(totalBytes)
      +",\"free_clusters\":"+String(freeClu)
      +",\"cluster_bytes\":"+String(clusterBytes)+"}");
  });
  // Synchronous mode bootstrap — loaded as <script src> in <head> before any other JS.
  // Sets window.DEVICE_MODE so tabs and Status cards have correct initial state
  // without waiting for the async /api/sysinfo fetch.
  httpServer.on("/api/mode.js", HTTP_GET, [](){
    if (!checkAuth()) return;
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "application/javascript",
      "window.DEVICE_MODE=" + String(cfg.deviceMode) + ";"
      "window.GOTEK_DIR=\"" + gotekImgDir + "\";");
  });
  // Diagnostic endpoint — shows current USB state for debugging CD-ROM vs Disk issue
  httpServer.on("/api/gotek/usbdebug", HTTP_GET, [](){
    if (!checkAuth()) return;
    String j = "{";
    j += "\"gotek_mode_fn\":"    + String(scsi_get_gotek_mode() ? "true" : "false");
    j += ",\"cfg_deviceMode\":"  + String(cfg.deviceMode);
    j += ",\"gk_selected_slot\":"+ String(gkSelectedSlot);
    j += ",\"gk_cur_slot\":"     + String(gkCurSlot);
    j += ",\"gk_last_lba\":"     + String(gkLastLba);
    j += ",\"gk_fc\":"           + String(gkFC);
    j += ",\"gk_ready\":"        + String(gkRdy ? "true" : "false");
    j += ",\"pid_hex\":\"0x8020\"";
    j += ",\"slots\":[";
    for (int i = 0; i < gkFC; i++) {
      if (i) j += ",";
      uint32_t stride = (gkF[i].sectors <= 2880) ? 2880 : ((gkF[i].sectors + 2879) / 2880) * 2880;
      j += "{\"slot\":"      + String(i);
      j += ",\"path\":\""    + String(gkF[i].path) + "\"";
      j += ",\"sz\":"        + String(gkF[i].sz);
      j += ",\"sectors\":"   + String(gkF[i].sectors);
      j += ",\"startLba\":"  + String(gkF[i].startLba);
      j += ",\"endLba\":"    + String(gkF[i].startLba + stride - 1);
      j += ",\"reads\":"     + String(gkReadCnt[i]);
      j += ",\"writes\":"    + String(gkWriteCnt[i]);
      j += ",\"sz_ok\":"     + String(gkF[i].sectors == 2880 ? "true" : "false");
      j += "}";
    }
    j += "]";
    j += "}";
    httpServer.send(200, "application/json", j);
  });
  httpServer.on("/api/gotek/create", HTTP_GET, handleGotekCreate);

  // GET /api/gotek/lbalog — last 64 LBA accesses (ring buffer)
  httpServer.on("/api/gotek/lbalog", HTTP_GET, [](){
    if (!checkAuth()) return;
    String j = "{\"seq\":"+String(gkLogSeq)+",\"fc\":"+String(gkFC)+",\"entries\":[";
    uint32_t total = min(gkLogHead, (uint32_t)GK_LOG_SIZE);
    uint32_t start = (gkLogHead > GK_LOG_SIZE) ? gkLogHead - GK_LOG_SIZE : 0;
    bool first = true;
    for (uint32_t i = start; i < gkLogHead; i++) {
      GkLogEntry& e = gkLog[i % GK_LOG_SIZE];
      if (!first) j += ","; first = false;
      char hex[12]; snprintf(hex, sizeof(hex), "%02X%02X%02X%02X", e.b0, e.b1, e.b2, e.b3);
      j += "{\"lba\":"+String(e.lba)
          +",\"slot\":"+String(e.slot)
          +",\"op\":\""+String((char)e.op)+"\""
          +",\"hex\":\""+String(hex)+"\"";
      // Flag LBAs that would be slot boundaries
      uint32_t slotN = (gkFC > 0) ? (e.lba / 2880) : 0;
      j += ",\"slotN\":"+String(slotN);
      j += "}";
    }
    j += "]}";
    httpServer.send(200, "application/json", j);
  });

  // GET /api/gotek/sector?lba=N — dump raw bytes of one sector (for troubleshooting)
  httpServer.on("/api/gotek/sector", HTTP_GET, [](){
    if (!checkAuth()) return;
    uint32_t lba = httpServer.hasArg("lba") ? (uint32_t)httpServer.arg("lba").toInt() : 0;
    uint8_t buf[512]; memset(buf, 0, 512);
    uint64_t fOff = 0;
    int fi = gkResolveLba(lba, fOff);
    String j = "{\"lba\":"+String(lba)+",\"slot\":"+String(fi);
    if (fi >= 0) {
      j += ",\"path\":\""+String(gkF[fi].path)+"\"";
      j += ",\"fOff\":"+String((uint32_t)fOff);
      if (gkMtxTake()) {
        File f = SD_MMC.open(gkF[fi].path, FILE_READ);
        if (f) { f.seek(fOff); f.read(buf, 512); f.close(); }
        gkMtxGive();
      }
      // Return hex of first 64 bytes
      String hex = "";
      for (int i = 0; i < 64; i++) {
        char h[3]; snprintf(h, sizeof(h), "%02X", buf[i]);
        hex += h;
        if (i % 16 == 15) hex += " ";
      }
      j += ",\"hex64\":\""+hex+"\"";
      // Check if it looks like a FAT12 boot sector
      bool fat12 = (buf[0]==0xEB || buf[0]==0xE9) && buf[21]==0xF0;
      j += ",\"looks_fat12\":"+String(fat12?"true":"false");
      j += ",\"b0\":"+String(buf[0]);
      j += ",\"b510\":"+String(buf[510]);
      j += ",\"b511\":"+String(buf[511]);
    }
    j += "}";
    httpServer.send(200, "application/json", j);
  });
  // SELECT: switch single-slot mode (Windows-friendly — exposes one image as 1.44MB USB disk)
  httpServer.on("/api/gotek/select", HTTP_GET, []() {
    if (!checkAuth()) return;
    int slot = httpServer.hasArg("slot") ? httpServer.arg("slot").toInt() : -1;
    gkSelectedSlot = slot;  // stores slotNum (-1 = raw multi-slot)
    gkCacheInvalidate();    // flush PSRAM cache — new slot has different data

    // Single-slot mode always uses raw LBA presentation regardless of gotekUsbMode.
    // Use gkRuntimeUsbMode so cfg.gotekUsbMode is never corrupted.
    gkRuntimeUsbMode = (slot >= 0) ? 0 : cfg.gotekUsbMode;

    uint32_t newCount = gkBlockCount();
    if (slot >= 0) {
      for (int i = 0; i < gkFC; i++) {
        if ((int)gkF[i].slotNum == slot) { newCount = gkF[i].sectors; break; }
      }
    }
    tud_disconnect();
    delay(600);
    msc.onRead(gotekReadCb);
    msc.onWrite(gotekWriteCb);
    msc.onStartStop(gotekStartStopCb);
    msc.isWritable(gkRuntimeUsbMode != 1 || cfg.gotekFatWP == 0);
    msc.begin(newCount, GK_SECTOR_SIZE);
    msc.mediaPresent(true);
    tud_connect();
    dualPrint.printf("[GOTEK] Selected slot: %d  sectors: %u  usbMode: %s  writable: %s  (re-enumerated)\n",
      slot, newCount,
      gkRuntimeUsbMode ? "FAT32" : "Raw",
      (gkRuntimeUsbMode != 1 || cfg.gotekFatWP == 0) ? "yes" : "no (WP)");
    httpServer.send(200,"application/json",
      "{\"ok\":true,\"slot\":"+String(slot)+",\"sectors\":"+String(newCount)+"}");
  });
  // POST /api/gotek/order — rename files to DSKA0000..N to match new drag-drop order
  // Body: JSON array of filenames in new order, e.g. ["DSKA0002.IMG","DSKA0000.IMG"]
  // POST /api/gotek/order — saves desired slot order to order.json (no renaming!)
  // Body: JSON array of filenames in desired slot order
  httpServer.on("/api/gotek/order", HTTP_POST, []() {
    if (!checkAuth()) return;
    String body = httpServer.arg("plain");
    if (!body.length() || body[0] != '[') {
      httpServer.send(400,"application/json","{\"error\":\"invalid\"}"); return;
    }
    String orderPath = gotekImgDir + "/order.json";
    if (!gkMtxTake()) { httpServer.send(503,"application/json","{\"error\":\"busy\"}"); return; }
    File f = SD_MMC.open(orderPath.c_str(), FILE_WRITE);
    if (!f) { gkMtxGive(); httpServer.send(500,"application/json","{\"error\":\"write failed\"}"); return; }
    f.print(body); f.close();
    dualPrint.printf("[GOTEK] order.json saved (%d bytes)\n", body.length());
    gkBuildFS(); gkMtxGive();
    gkRuntimeUsbMode = cfg.gotekUsbMode;  // re-sync (multi-slot, no single-slot override)
    tud_disconnect(); delay(600);
    msc.onRead(gotekReadCb); msc.onWrite(gotekWriteCb); msc.onStartStop(gotekStartStopCb);
    msc.isWritable(gkRuntimeUsbMode != 1 || cfg.gotekFatWP == 0); msc.begin(gkBlockCount(), GK_SECTOR_SIZE); msc.mediaPresent(true);
    tud_connect();
    httpServer.send(200,"application/json","{\"ok\":true}");
  });

  httpServer.on("/api/gotek/refresh", HTTP_GET, []() {
    if (!checkAuth()) return;
    if (cfg.deviceMode != 1) { httpServer.send(400, "text/plain", "Not in GoTek mode."); return; }
    gkRefresh();
    httpServer.send(200, "application/json",
      "{\"ok\":true,\"files\":" + String(gkFC) + "}");
  });
  httpServer.on("/api/gotek/ls", HTTP_GET, []() {
    if (!checkAuth()) return;
    if (cfg.deviceMode != 1) { httpServer.send(400, "text/plain", "Not in GoTek mode."); return; }
    String j = "{\"dir\":\"" + gotekImgDir + "\",\"files\":[";
    for (int i = 0; i < gkFC; i++) {
      if (i) j += ",";
      String p = String(gkF[i].path);
      int sl = p.lastIndexOf('/');
      String fn = (sl >= 0) ? p.substring(sl+1) : p;
      j += "{\"name\":\"" + fn + "\",\"size\":" + String(gkF[i].sz) +
           ",\"slot\":" + String(gkF[i].slotNum) + "}";
    }
    j += "]}";
    httpServer.send(200, "application/json", j);
  });
  httpServer.on("/api/gotek/labels", HTTP_GET, []() {
    if (!checkAuth()) return;
    String labelsPath = gotekImgDir + "/labels.json";
    if (!gkMtxTake()) { httpServer.send(503,"application/json","{\"error\":\"busy\"}"); return; }
    File f = SD_MMC.open(labelsPath.c_str(), FILE_READ);
    if (!f) { gkMtxGive(); httpServer.send(200,"application/json","{}"); return; }
    String content = f.readString(); f.close();
    gkMtxGive();
    httpServer.send(200, "application/json", content.length() ? content : "{}");
  });

  // POST /api/gotek/labels — saves JSON body to SD
  httpServer.on("/api/gotek/labels", HTTP_POST, []() {
    if (!checkAuth()) return;
    String body = httpServer.arg("plain");
    if (!body.length()) { httpServer.send(400,"application/json","{\"error\":\"empty\"}"); return; }
    String labelsPath = gotekImgDir + "/labels.json";
    if (!gkMtxTake()) { httpServer.send(503,"application/json","{\"error\":\"busy\"}"); return; }
    File f = SD_MMC.open(labelsPath.c_str(), FILE_WRITE);
    if (!f) { gkMtxGive(); httpServer.send(500,"application/json","{\"error\":\"write failed\"}"); return; }
    f.print(body); f.close();
    gkMtxGive();
    httpServer.send(200,"application/json","{\"ok\":true}");
  });
  httpServer.on("/api/download",      HTTP_GET,  handleApiDownload);
  httpServer.on("/api/mkdir",        HTTP_GET,  handleApiMkdir);
  httpServer.on("/api/delete",       HTTP_GET,  handleApiDelete);
  httpServer.on("/api/upload",       HTTP_POST, handleApiUploadDone, handleFileUpload);
  httpServer.on("/api/audio/status", HTTP_GET,  handleApiAudioStatus);
  httpServer.on("/api/audio/play",   HTTP_GET,  handleApiAudioPlay);
  httpServer.on("/api/audio/stop",   HTTP_GET,  handleApiAudioStop);
  httpServer.on("/api/audio/pause",  HTTP_GET,  handleApiAudioPause);
  httpServer.on("/api/audio/resume", HTTP_GET,  handleApiAudioResume);
  httpServer.on("/api/audio/volume", HTTP_GET,  handleApiAudioVolume);
  httpServer.on("/api/audio/mute",   HTTP_GET,  handleApiAudioMute);
  httpServer.on("/api/audio/seek",   HTTP_GET,  handleApiAudioSeek);
  static const char* hdrs[] = {"X-Internal-Proxy", "Authorization", "Content-Type", "Host"};
  httpServer.collectHeaders(hdrs, 4);
  httpServer.begin();
  if (cfg.httpsEnabled) {
    httpServer.onNotFound(handleHttpRedirect);
    startHttpsServer();
    dualPrint.printf("[OK]  HTTPS started at https://%s\n", WiFi.localIP().toString().c_str());
  }
  dualPrint.printf("[OK]  HTTP server started at http://%s\n", WiFi.localIP().toString().c_str());
}
// =============================================================================
//  UART CLI
// =============================================================================
void printSep() { dualPrint.println(F("================================================")); }

void printConfig(bool showRuntime = true) {
  printSep();
  dualPrint.println(F("  CONFIGURATION (NVS)"));
  printSep();
  dualPrint.printf("  WiFi SSID     : %s\n",  strlen(cfg.ssid)     ? cfg.ssid    : "(not set)");
  dualPrint.printf("  WiFi Password : %s\n",  strlen(cfg.password) ? "********"  : "(not set)");
  dualPrint.printf("  DHCP          : %s\n",  cfg.dhcp ? "enabled" : "disabled");
  dualPrint.printf("  Static IP     : %s\n",  cfg.ip);
  dualPrint.printf("  Subnet mask   : %s\n",  cfg.mask);
  dualPrint.printf("  Gateway       : %s\n",  cfg.gw);
  dualPrint.printf("  DNS           : %s\n",  cfg.dns);
  dualPrint.printf("  Hostname/FQDN : %s\n",  cfg.hostname);
  {
    char lbl[32]; strlcpy(lbl, cfg.hostname, sizeof(lbl));
    if (char* d = strchr(lbl, '.')) *d = 0;
    dualPrint.printf("  mDNS address  : %s.local\n", lbl);
  }
  dualPrint.printf("  WiFi TX power : %d dBm\n", cfg.wifiTxPower / 4);
  const char* eapLabel = cfg.eapMode==1?"PEAP":cfg.eapMode==2?"EAP-TLS":"disabled";
  dualPrint.printf("  802.1x EAP    : %s\n", eapLabel);
  if (cfg.eapMode) {
    dualPrint.printf("  EAP Identity  : %s\n", strlen(cfg.eapIdentity) ? cfg.eapIdentity : "(not set)");
    if (cfg.eapMode == 1) {
      dualPrint.printf("  EAP Username  : %s\n", strlen(cfg.eapUsername) ? cfg.eapUsername : "(not set)");
      dualPrint.printf("  EAP Password  : %s\n", strlen(cfg.eapPassword) ? "********" : "(not set)");
    }
    dualPrint.printf("  CA cert path  : %s\n", strlen(cfg.eapCaPath)   ? cfg.eapCaPath   : "(none)");
    if (cfg.eapMode == 2) {
      dualPrint.printf("  Client cert   : %s\n", strlen(cfg.eapCertPath) ? cfg.eapCertPath : "(not set)");
      dualPrint.printf("  Client key    : %s\n", strlen(cfg.eapKeyPath)  ? cfg.eapKeyPath  : "(not set)");
      dualPrint.printf("  Key passphrase: %s\n", strlen(cfg.eapKeyPass) ? "set (********)" : "(none)");
    }
  }
  dualPrint.printf("  Default image : %s\n",  defaultMount.length() ? defaultMount.c_str() : "(none)");
  dualPrint.printf("  Device mode   : %s\n",  cfg.deviceMode == 1 ? "GoTek floppy emulator" : "CD-ROM (default)");
  if (cfg.deviceMode == 1) {
    dualPrint.println(F("\n  ┌─ GoTek Floppy Emulator ────────────────────────┐"));
    dualPrint.printf("  GoTek USB mode: %s\n",
      cfg.gotekUsbMode == 1
        ? "FAT32 virtual (FlashFloppy indexed/native)"
        : "Raw LBA      (stock GoTek firmware)");
    if (cfg.gotekUsbMode == 1)
      dualPrint.printf("  FAT32 WP      : %s\n",
        cfg.gotekFatWP ? "ON  (Windows cannot format/write container)" : "OFF (writable)");
    dualPrint.printf("  GoTek img dir : %s\n", gotekImgDir.c_str());
    dualPrint.printf("  Slots loaded  : %d  (ready: %s)\n", gkFC, gkRdy ? "yes" : "no");
    for (int i = 0; i < gkFC; i++) {
      const char* fn = strrchr(gkF[i].path, '/');
      fn = fn ? fn+1 : gkF[i].path;
      dualPrint.printf("  Slot %03u       : %-24s  %lu KB  LBA %lu\n",
        gkF[i].slotNum, fn, (unsigned long)(gkF[i].sz/1024), (unsigned long)gkF[i].startLba);
    }
    dualPrint.printf("  Selected slot : %d  (%s)\n",
      gkSelectedSlot, gkSelectedSlot < 0 ? "RAW multi-slot" : "single-slot mode");
    dualPrint.printf("  Current slot  : %d  (last GoTek access)\n", gkCurSlot);
    dualPrint.printf("  USB block cnt : %u  (%.2f MB)\n",
      gkBlockCount(), (float)gkBlockCount()*GK_SECTOR_SIZE/1048576.0f);
    dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  }
  // DOS compat + driver (driver jen kdyz compat=ON)
  dualPrint.printf("  DOS compat    : %s\n",  cfg.dosCompat ? "ON  (UNIT ATTENTION, no USB re-enum)" : "OFF");
  if (cfg.dosCompat) {
    const char* dosDriverName[] = {
      "0=Generic (no special identity)",
      "1=USBCD2/TEAC  [INQUIRY: TEAC CD-56E]",
      "2=ESPUSBCD/Panasonic  [INQUIRY: MATSHITA CR-572]",
      "3=DI1000DD/USBASPI 2.20  [data only, no audio]"
    };
    dualPrint.printf("  DOS driver    : %s\n",  dosDriverName[cfg.dosDriver < 4 ? cfg.dosDriver : 0]);
  }
  dualPrint.printf("  Audio module  : %s\n",  cfg.audioModule ? "PCM5102 enabled" : "disabled");
  if (cfg.win98StopMs > 0)
    dualPrint.printf("  Win98 Stop    : %u ms\n", cfg.win98StopMs);
  else
    dualPrint.println(F("  Win98 Stop    : disabled"));
  dualPrint.printf("  Debug logging : %s\n",  cfg.debugMode ? "ON" : "OFF");
  dualPrint.printf("  Web auth      : %s\n",  cfg.webAuth ? "enabled" : "disabled (no login required)");
  if (cfg.webAuth) dualPrint.printf("  Web user      : %s\n", cfg.webUser);
  dualPrint.printf("  HTTPS         : %s\n", cfg.httpsEnabled ? "enabled (port 443)" : "disabled");
  if (cfg.httpsEnabled) {
    dualPrint.printf("  HTTPS cert    : %s\n", strlen(cfg.httpsCertPath) ? cfg.httpsCertPath : "(not set)");
    dualPrint.printf("  HTTPS key     : %s\n", strlen(cfg.httpsKeyPath)  ? cfg.httpsKeyPath  : "(not set)");
  dualPrint.printf("  HTTP port     : %u\n", cfg.httpPort  ? cfg.httpPort  : 80);
  dualPrint.printf("  HTTPS port    : %u\n", cfg.httpsPort ? cfg.httpsPort : 443);
  dualPrint.printf("  TLS version   : %s\n", cfg.tlsMinVer==0?"TLS 1.2 only":"TLS 1.2 + 1.3");
  dualPrint.printf("  TLS ciphers   : %s\n", cfg.tlsCiphers==1?"Strong/GCM":cfg.tlsCiphers==2?"Medium/CBC":cfg.tlsCiphers==3?"All/Legacy":"Auto");
  }
  dualPrint.println(F(""));
  printSep();
  if (!showRuntime) return;
  dualPrint.println(F("  RUNTIME STATE"));
  printSep();
  dualPrint.printf("  SD card       : %s\n",  sdReady ? "OK" : "NOT FOUND");
  if (sdReady) {
    uint64_t totalMb = SD_MMC.totalBytes() / (1024*1024);
    uint64_t freeMb  = (SD_MMC.totalBytes()-SD_MMC.usedBytes()) / (1024*1024);
    dualPrint.printf("  SD size       : %llu MB total, %llu MB free\n", totalMb, freeMb);
  }
  dualPrint.printf("  WiFi          : %s\n",  wifiConnected ? "connected" : "not connected");
  if (wifiConnected) {
    dualPrint.printf("  IP address    : %s\n",  WiFi.localIP().toString().c_str());
    dualPrint.printf("  WiFi RSSI     : %d dBm\n", WiFi.RSSI());
    dualPrint.printf("  WiFi channel  : %d\n",  WiFi.channel());
    dualPrint.printf("  WiFi TX power : %d dBm  (set wifi-txpower to adjust)\n", cfg.wifiTxPower/4);
    dualPrint.printf("  BSSID         : %s\n",  WiFi.BSSIDstr().c_str());
    dualPrint.printf("  Web interface : http://%s\n", WiFi.localIP().toString().c_str());
    // Show hostname/FQDN and mDNS
    char lbl[32]; strlcpy(lbl, cfg.hostname, sizeof(lbl));
    if (char* d = strchr(lbl, '.')) *d = 0;
    if (strchr(cfg.hostname, '.')) {
      dualPrint.printf("  FQDN (DHCP)   : %s\n", cfg.hostname);
      dualPrint.printf("  mDNS address  : http://%s.local\n", lbl);
    } else {
      dualPrint.printf("  Hostname/mDNS : http://%s.local\n", lbl);
    }
  }
  dualPrint.printf("  Mounted image : %s\n",  mountedFile.length() ? mountedFile.c_str() : "(none)");
  if (isoOpen) {
    dualPrint.printf("  Sectors       : %lu x 2048 B\n", mountedBlocks);
    dualPrint.printf("  Raw sector    : %u B  header offset: +%u B\n", binRawSectorSize, binHeaderOffset);
    dualPrint.printf("  Image size    : %.1f MB\n", (float)mountedBlocks * binRawSectorSize / (1024.0f*1024.0f));
  }
  dualPrint.printf("  Media present : %s\n",  mscMediaPresent ? "yes" : "no");
  printSep();
}

void printStatus() {
  printSep();
  dualPrint.println(F("  STATUS"));
  printSep();

  // ── WiFi ─────────────────────────────────────────────────────────────────
  dualPrint.println(F("  ┌─ WiFi ─────────────────────────────────────────┐"));
  if (wifiConnected) {
    int rssi = WiFi.RSSI();
    const char* quality = rssi > -50 ? "Excellent" : rssi > -65 ? "Good" : rssi > -75 ? "Fair" : "Weak";
    dualPrint.printf("  Status        : Connected  (%s, %d dBm)\n", quality, rssi);
    dualPrint.printf("  TX power      : %d dBm\n", cfg.wifiTxPower / 4);
    dualPrint.printf("  SSID          : %s\n", WiFi.SSID().c_str());
    dualPrint.printf("  BSSID         : %s\n", WiFi.BSSIDstr().c_str());
    dualPrint.printf("  Band/Channel  : %s / Ch %d\n", WiFi.channel()<=13?"2.4 GHz":"5 GHz", WiFi.channel());
    dualPrint.printf("  IP            : %s\n", WiFi.localIP().toString().c_str());
    dualPrint.printf("  Mask          : %s\n", WiFi.subnetMask().toString().c_str());
    dualPrint.printf("  Gateway       : %s\n", WiFi.gatewayIP().toString().c_str());
    dualPrint.printf("  DNS           : %s\n", WiFi.dnsIP().toString().c_str());
    const char* authStr = cfg.eapMode==1?"WPA2-Enterprise (PEAP)":cfg.eapMode==2?"WPA2-Enterprise (EAP-TLS)":"WPA2-Personal";
    dualPrint.printf("  Auth          : %s\n", authStr);
    if (cfg.eapMode) {
      dualPrint.printf("  EAP Identity  : %s\n", strlen(cfg.eapIdentity) ? cfg.eapIdentity : "(auto from cert CN)");
      if (cfg.eapMode == 2) {
        dualPrint.printf("  Client cert   : %s\n", strlen(cfg.eapCertPath) ? cfg.eapCertPath : "(not set)");
        dualPrint.printf("  Client key    : %s\n", strlen(cfg.eapKeyPath)  ? cfg.eapKeyPath  : "(not set)");
        dualPrint.printf("  Key passphrase: %s\n", strlen(cfg.eapKeyPass)  ? "set" : "(none)");
        if (s_eapCertBuf) {
          mbedtls_x509_crt crt; mbedtls_x509_crt_init(&crt);
          if (mbedtls_x509_crt_parse(&crt,(const unsigned char*)s_eapCertBuf,strlen(s_eapCertBuf)+1)==0) {
            char cnBuf[64]={};
            mbedtls_x509_name* nm=&crt.subject;
            while(nm){if(nm->oid.len==3&&memcmp(nm->oid.p,"\x55\x04\x03",3)==0){memcpy(cnBuf,nm->val.p,min((int)nm->val.len,63));break;}nm=nm->next;}
            dualPrint.printf("  Cert CN       : %s\n", strlen(cnBuf)?cnBuf:"(unknown)");
            char notBefore[20]={},notAfter[20]={};
            snprintf(notBefore,sizeof(notBefore),"%04d-%02d-%02d",crt.valid_from.year,crt.valid_from.mon,crt.valid_from.day);
            snprintf(notAfter, sizeof(notAfter), "%04d-%02d-%02d",crt.valid_to.year,  crt.valid_to.mon,  crt.valid_to.day);
            dualPrint.printf("  Cert validity : %s → %s\n", notBefore, notAfter);
            dualPrint.printf("  Cert bits     : %u bit\n", (unsigned)mbedtls_pk_get_bitlen(&crt.pk));
          }
          mbedtls_x509_crt_free(&crt);
        }
        dualPrint.printf("  CA cert       : %s\n", strlen(cfg.eapCaPath) ? cfg.eapCaPath : "(none)");
        if (s_eapKeyBuf) {
          const char* kfmt = strstr(s_eapKeyBuf,"BEGIN RSA PRIVATE KEY")?"PKCS#1 RSA":
                             strstr(s_eapKeyBuf,"BEGIN EC PRIVATE KEY") ?"EC":
                             strstr(s_eapKeyBuf,"BEGIN PRIVATE KEY")    ?"PKCS#8":
                             strstr(s_eapKeyBuf,"BEGIN ENCRYPTED PRIVATE KEY")?"Encrypted":"Unknown";
          dualPrint.printf("  Key format    : %s\n", kfmt);
        }
      } else {
        dualPrint.printf("  PEAP username : %s\n", strlen(cfg.eapUsername) ? cfg.eapUsername : "(not set)");
        dualPrint.printf("  CA cert       : %s\n", strlen(cfg.eapCaPath) ? cfg.eapCaPath : "(none)");
      }
    }
  } else {
    dualPrint.println(F("  Status        : Not connected"));
  }
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));

  // ── SD Card ──────────────────────────────────────────────────────────────
  dualPrint.println(F("  ┌─ SD Card ──────────────────────────────────────┐"));
  dualPrint.printf("  SD card       : %s\n", sdReady ? "OK" : "NOT FOUND");
  if (sdReady) {
    uint64_t totalMb = SD_MMC.totalBytes() / (1024*1024);
    uint64_t usedMb  = SD_MMC.usedBytes()  / (1024*1024);
    uint64_t freeMb  = totalMb - usedMb;
    dualPrint.printf("  Storage       : %llu MB used / %llu MB total (%llu MB free)\n",
                  usedMb, totalMb, freeMb);
  }
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));

  // ── Mode-specific sections ────────────────────────────────────────────────
  if (cfg.deviceMode == 1) {
    // GoTek mode
    dualPrint.println(F("  ┌─ GoTek Floppy Emulator ────────────────────────┐"));
    dualPrint.printf("  USB mode      : %s\n",
      cfg.gotekUsbMode == 1
        ? "FAT32 virtual (FlashFloppy indexed/native)"
        : "Raw LBA      (stock GoTek firmware)");
    if (cfg.gotekUsbMode == 1)
      dualPrint.printf("  FAT32 WP      : %s\n",
        cfg.gotekFatWP ? "ON  (Windows cannot format container)" : "OFF (writable)");
    dualPrint.printf("  Image dir     : %s\n", gotekImgDir.c_str());
    dualPrint.printf("  Slots loaded  : %d  (FS ready: %s)\n", gkFC, gkRdy ? "yes" : "no");
    for (int i = 0; i < gkFC; i++) {
      const char* fn = strrchr(gkF[i].path, '/');
      fn = fn ? fn+1 : gkF[i].path;
      bool isActive = (gkCurSlot == i);
      bool isSel    = ((int)gkF[i].slotNum == gkSelectedSlot && gkSelectedSlot >= 0);
      dualPrint.printf("  Slot %03u       : %-24s  %4lu KB%s%s\n",
        gkF[i].slotNum, fn, (unsigned long)(gkF[i].sz/1024),
        isActive ? "  \xe2\x86\x90 ACTIVE"  : "",
        isSel    ? "  \xe2\x86\x90 MOUNTED" : "");
    }
    dualPrint.printf("  Mode          : %s\n",
      gkSelectedSlot >= 0 ? ("Single-slot (slot " + String(gkSelectedSlot) + ")").c_str()
                          : "GoTek RAW (all slots)");
    dualPrint.printf("  USB sectors   : %u  (%.2f MB)\n",
      gkBlockCount(), (float)gkBlockCount()*GK_SECTOR_SIZE/1048576.0f);
    dualPrint.println(F("  └────────────────────────────────────────────────┘"));

  } else {
    // CD-ROM mode
    dualPrint.println(F("  ┌─ CD-ROM ───────────────────────────────────────┐"));
    if (isoOpen) {
      dualPrint.printf("  Mounted image : %s\n", mountedFile.c_str());
      dualPrint.printf("  Sectors       : %lu x 2048 B  (%.1f MB)\n",
                    mountedBlocks, (float)mountedBlocks * 2048.0f / (1024.0f*1024.0f));
      dualPrint.printf("  Raw sector    : %u B  header: +%u B\n", binRawSectorSize, binHeaderOffset);
      dualPrint.printf("  USB media     : %s\n", mscMediaPresent ? "present" : "not present");
    } else {
      dualPrint.println(F("  Mounted image : (none)"));
    }
    dualPrint.printf("  Default image : %s\n", defaultMount.length() ? defaultMount.c_str() : "(none)");
    dualPrint.println(F("  └────────────────────────────────────────────────┘"));
    dualPrint.println(F(""));

    dualPrint.println(F("  ┌─ Audio ────────────────────────────────────────┐"));
    dualPrint.printf("  Audio module  : %s\n", cfg.audioModule ? "PCM5102 I2S — enabled" : "disabled");
    dualPrint.printf("  Win98 Stop    : %s\n", cfg.win98StopMs > 0 ?
      (String(cfg.win98StopMs) + " ms — Stop/Pause detection active").c_str() : "disabled");
    if (cfg.audioModule) {
      dualPrint.printf("  I2S pins      : BCK=%d  WS=%d  DATA=%d\n", I2S_BCK_PIN, I2S_WS_PIN, I2S_DATA_PIN);
      dualPrint.printf("  I2S state     : %s\n", i2sTx ? "initialized" : "init failed");
      const char* astStr = audioState==1?"PLAYING":audioState==2?"PAUSED":"STOPPED";
      dualPrint.printf("  Playback      : %s\n", astStr);
      if (audioState > 0) {
        uint8_t am,as_,af; lbaToMsf(audioCurrentLba,am,as_,af);
        dualPrint.printf("  Position      : %02d:%02d:%02d  (LBA %lu)\n",am,as_,af,audioCurrentLba);
        dualPrint.printf("  Track         : %d\n", subChannel.trackNum);
      }
      dualPrint.printf("  Volume        : %d%%%s\n", audioVolume, audioMuted?" (MUTED)":"");
      dualPrint.printf("  Audio tracks  : %d\n", audioTrackCount);
    }
    dualPrint.println(F("  └────────────────────────────────────────────────┘"));
    dualPrint.println(F(""));

    dualPrint.println(F("  ┌─ DOS Compatibility ─────────────────────────────┐"));
    dualPrint.printf("  Debug logging : %s\n", cfg.debugMode ? "ON" : "OFF");
    dualPrint.printf("  DOS compat    : %s\n", cfg.dosCompat ? "ON  (UNIT ATTENTION)" : "OFF");
    if (cfg.dosCompat) {
      const char* drvNames[] = {
        "0=Generic", "1=USBCD2/TEAC", "2=ESPUSBCD/Panasonic", "3=DI1000DD"
      };
      dualPrint.printf("  DOS driver    : %s\n", drvNames[cfg.dosDriver < 4 ? cfg.dosDriver : 0]);
    }
    dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  }
  dualPrint.println(F(""));

  // ── Web & HTTPS (common) ──────────────────────────────────────────────────
  dualPrint.println(F("  ┌─ Web & HTTPS ──────────────────────────────────┐"));
  dualPrint.printf("  Web auth      : %s\n", cfg.webAuth ? "enabled" : "disabled");
  dualPrint.printf("  HTTPS         : %s\n", cfg.httpsEnabled ?
                   (httpsHandle ? "enabled + running (port 443)" : "enabled (start failed)") : "disabled");
  if (cfg.httpsEnabled) {
    dualPrint.printf("  TLS cert      : %s\n", strlen(cfg.httpsCertPath) ? cfg.httpsCertPath : "(not set)");
    dualPrint.printf("  TLS key       : %s\n", strlen(cfg.httpsKeyPath)  ? cfg.httpsKeyPath  : "(not set)");
  }
  dualPrint.printf("  HTTP port     : %u\n", cfg.httpPort  ? cfg.httpPort  : 80);
  dualPrint.printf("  HTTPS port    : %u\n", cfg.httpsPort ? cfg.httpsPort : 443);
  dualPrint.printf("  TLS version   : %s\n", cfg.tlsMinVer==0?"TLS 1.2 only":"TLS 1.2 + 1.3");
  dualPrint.printf("  TLS ciphers   : %s\n", cfg.tlsCiphers==1?"Strong/GCM":cfg.tlsCiphers==2?"Medium/CBC":cfg.tlsCiphers==3?"All/Legacy":"Auto");
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));

  // ── System (common) ───────────────────────────────────────────────────────
  dualPrint.println(F("  ┌─ System ───────────────────────────────────────┐"));
  dualPrint.printf("  Free heap     : %lu B\n", ESP.getFreeHeap());
  dualPrint.printf("  Uptime        : %lu s\n", millis()/1000);
  dualPrint.printf("  CPU freq      : %u MHz\n", getCpuFrequencyMhz());
  dualPrint.printf("  Flash size    : %u MB\n", ESP.getFlashChipSize()/(1024*1024));
  dualPrint.printf("  SDK version   : %s\n", ESP.getSdkVersion());
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  printSep();
}

void printHelp() {
  printSep();
  dualPrint.println(F("  COMMANDS"));
  printSep();
  dualPrint.println(F("  set ssid <value>      Set WiFi SSID"));
  dualPrint.println(F("  set pass <value>      Set WiFi password"));
  dualPrint.println(F("  set dhcp on|off       Enable/disable DHCP"));
  dualPrint.println(F("  set ip <addr>         Set static IP address"));
  dualPrint.println(F("  set mask <mask>       Set subnet mask"));
  dualPrint.println(F("  set gw <addr>         Set default gateway"));
  dualPrint.println(F("  set dns <addr>        Set DNS server"));
  dualPrint.println(F("  set hostname <n>      Set hostname or FQDN (e.g. espcd or espcd.falco81.net)"));
  dualPrint.println(F("  set wifi-txpower <dBm>  2/5/10/15/20 dBm TX power (lower = less RF noise)"));
  dualPrint.println(F(""));
  dualPrint.println(F("  ┌─ 802.1x Enterprise WiFi ───────────────────────┐"));
  dualPrint.println(F("  set eap-mode <0|1|2>  0=off 1=PEAP 2=EAP-TLS"));
  dualPrint.println(F("  set eap-id <val>      EAP outer identity"));
  dualPrint.println(F("  set eap-user <val>    EAP inner username (PEAP)"));
  dualPrint.println(F("  set eap-pass <val>    EAP inner password (PEAP)"));
  dualPrint.println(F("  set eap-ca <path>     SD path to CA cert e.g. /wifi/ca.pem"));
  dualPrint.println(F("  set eap-cert <path>   SD path to client cert (EAP-TLS)"));
  dualPrint.println(F("  set eap-key <path>    SD path to client key (EAP-TLS)"));
  dualPrint.println(F("  set eap-kpass <val>   Passphrase for encrypted private key"));
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));
  dualPrint.println(F("  ┌─ Audio ─────────────────────────────────────────┐"));
  dualPrint.println(F("  set audio-module on|off  Enable/disable PCM5102 I2S audio module"));
  dualPrint.println(F("  set win98-stop <ms>      Win98 Stop/Pause detect ms (0=off, def:1200)"));
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));
  dualPrint.println(F("  ┌─ Web UI Authentication ────────────────────────┐"));
  dualPrint.println(F("  set web-auth on|off       Enable/disable HTTP Basic Auth (default: off)"));
  dualPrint.println(F("  set web-user <name>       Web UI username (default: admin)"));
  dualPrint.println(F("  set web-pass <password>   Web UI password (default: admin)"));
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));
  dualPrint.println(F("  ┌─ DOS Compatibility ─────────────────────────────┐"));
  dualPrint.println(F("  set dos-compat on|off     ON = UNIT ATTENTION only, no USB re-enum"));
  dualPrint.println(F("  set dos-driver <0-3>      Set USB CD-ROM driver identity:"));
  dualPrint.println(F("    0 Generic     no special identity, universal"));
  dualPrint.println(F("    1 USBCD2/TEAC INQUIRY: TEAC CD-56E  [BROKEN: needs INT 13h]"));
  dualPrint.println(F("    2 ESPUSBCD/Pan  INQUIRY: MATSHITA CR-572  [recommended for audio]"));
  dualPrint.println(F("    3 DI1000DD    INQUIRY: generic  [data only, no audio]"));
  dualPrint.println(F("  DOS driver 2 (ESPUSBCD) notes:"));
  dualPrint.println(F("    USBASPI: usbaspi2.sys (Novac) or usbaspi1.sys (Panasonic v2.20)"));
  dualPrint.println(F("    ESPUSBCD communicates via SCSIMGR$ DOS device (INT 21h IOCTL)"));
  dualPrint.println(F("    ESPUSBCD scans ASPI target IDs: 0x18, 0x08, 0x10"));
  dualPrint.println(F("    CDPlayer.exe works with ESPUSBCD.SYS (sf3 IOCTL OUT fix included)"));
  dualPrint.println(F("    Use CDP.COM if cdplayer.exe still fails"));
  dualPrint.println(F("  DOS driver 3 (DI1000DD) notes:"));
  dualPrint.println(F("    USBASPI: usbaspi1.sys (Panasonic v2.20)  assigns ID 0"));
  dualPrint.println(F("    Data access only — no MSCDEX audio IOCTL support"));
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));
  dualPrint.println(F("  ┌─ Device Mode ───────────────────────────────────┐"));
  dualPrint.println(F("  set device-mode cdrom|gotek  Switch USB profile (reboot required)"));
  dualPrint.println(F("    cdrom  USB CD-ROM drive (default) — all CD/audio features active"));
  dualPrint.println(F("    gotek  USB flash drive for GoTek/FlashFloppy floppy emulator"));
  dualPrint.println(F("  set gotek-dir <path>      SD folder with .img files (default: /gotek)"));
  dualPrint.println(F("  set gotek-usb-mode raw|fat      raw=LBA stride (stock GoTek FW)  fat=FAT32 virtual (FlashFloppy indexed/native)"));
  dualPrint.println(F("  set gotek-fat-wp on|off         write-protect FAT32 virtual volume (default: on)"));
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));
  dualPrint.println(F("  ┌─ HTTPS / TLS ───────────────────────────────────┐"));
  dualPrint.println(F("  set https-enable on|off   Enable HTTPS on port 443"));
  dualPrint.println(F("  set https-cert <path>     SD path to TLS server certificate (.pem)"));
  dualPrint.println(F("  set https-key  <path>     SD path to TLS server private key (.pem)"));
  dualPrint.println(F("  set http-port <port>      HTTP server port (default: 80, reboot required)"));
  dualPrint.println(F("  set https-port <port>     HTTPS server port (default: 443)"));
  dualPrint.println(F("  set tls-ver <0|1>         0=TLS1.2 only  1=TLS1.2+1.3"));
  dualPrint.println(F("  set tls-ciphers <0-3>     0=Auto 1=Strong/GCM 2=Medium/CBC 3=All/Legacy"));
  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  dualPrint.println(F(""));
  dualPrint.println(F("  ┌─ Commands ─────────────────────────────────────┐"));
  dualPrint.println(F("  show config           Show full configuration + runtime state"));
  dualPrint.println(F("  show files [path]     List SD files recursively (default: /)"));
  dualPrint.println(F("  status                Show current status (WiFi, SD, image)"));
  dualPrint.println(F("  mount <file>          Mount ISO/BIN/CUE as CD-ROM"));
  dualPrint.println(F("  umount                Eject current CD-ROM"));
  dualPrint.println(F("  sd reinit             Reinitialize SD card"));
  dualPrint.println(F("  wifi reconnect        Disconnect and reconnect WiFi"));
  dualPrint.println(F("  audio test [Hz] [s]   Play sine wave from RAM (no SD) — isolates HW crackling"));
  dualPrint.println(F("  audio stop            Stop test tone or playback"));
  dualPrint.println(F("  wifi off              Disable WiFi (test if PLL shared clock causes crackling)"));
  dualPrint.println(F("  set audio-sectors <n> SD read batch: 1..32 sectors (diagnostic, not saved)"));
  dualPrint.println(F("  clear config          Erase ALL NVS settings (factory reset)"));
  dualPrint.println(F("  reboot                Restart ESP32"));
  dualPrint.println(F("  help                  Show this help"));
  dualPrint.println(F("  set debug on|off      Verbose SCSI/API logging (default: off, saved)"));

  dualPrint.println(F("  └────────────────────────────────────────────────┘"));
  printSep();
}

// Recursive directory listing helper
static uint32_t listDirRecursive(File dir, int depth, uint32_t &totalFiles, uint64_t &totalBytes) {
  uint32_t count = 0;
  File f;
  while ((f = dir.openNextFile())) {
    // indent by depth
    for (int i = 0; i < depth; i++) dualPrint.print("  ");
    if (f.isDirectory()) {
      dualPrint.printf("[DIR]  %s/\n", f.name());
      listDirRecursive(f, depth + 1, totalFiles, totalBytes);
    } else {
      uint32_t sz = f.size();
      totalBytes += sz;
      totalFiles++;
      // Format size human-readable
      if (sz >= 1024*1024) {
        dualPrint.printf("%-38s %8.1f MB\n", f.name(), sz / (1024.0f*1024.0f));
      } else if (sz >= 1024) {
        dualPrint.printf("%-38s %8.1f KB\n", f.name(), sz / 1024.0f);
      } else {
        dualPrint.printf("%-38s %8lu B\n",  f.name(), sz);
      }
    }
    count++;
    f.close();
  }
  return count;
}

void listSDFiles(const char* path = "/") {
  if (!sdReady) { dualPrint.println(F("[ERR] SD not available.")); return; }
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) {
    dualPrint.printf("[ERR] Cannot open directory: %s\n", path);
    if (dir) dir.close();
    return;
  }
  printSep();
  dualPrint.printf("  SD CARD - %s\n", path);
  printSep();
  uint32_t files = 0; uint64_t bytes = 0;
  uint32_t entries = listDirRecursive(dir, 0, files, bytes);
  dir.close();
  if (entries == 0) dualPrint.println(F("  (empty)"));
  printSep();
  if (files > 0) {
    if (bytes >= 1024ULL*1024*1024)
      dualPrint.printf("  %lu files  /  %.2f GB total\n", files, bytes/(1024.0*1024.0*1024.0));
    else if (bytes >= 1024*1024)
      dualPrint.printf("  %lu files  /  %.1f MB total\n", files, bytes/(1024.0*1024.0));
    else
      dualPrint.printf("  %lu files  /  %llu B total\n",  files, bytes);
    printSep();
  }
}

void reinitSD() {
  dualPrint.println(F("[SD]   Reinitializing..."));
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
  else if (line.startsWith("set debug") || line.startsWith("SET DEBUG")) {
    String v = line.substring(9); v.trim(); v.toLowerCase();
    cfg.debugMode = (v == "on" || v == "1" || v == "yes");
    saveConfig();
    dualPrint.printf("[OK]  Debug logging: %s\n", cfg.debugMode ? "ON" : "OFF");
  }
  else if (line.startsWith("set win98-stop") || line.startsWith("SET WIN98-STOP")) {
    String v = line.substring(14); v.trim();
    int ms = v.toInt();
    cfg.win98StopMs = (uint16_t)constrain(ms, 0, 9999);
    saveConfig();
    if (cfg.win98StopMs == 0)
      dualPrint.println(F("[OK]  Win98 Stop detection: DISABLED"));
    else
      dualPrint.printf("[OK]  Win98 Stop timeout: %u ms\n", cfg.win98StopMs);
  }
  else if (line.startsWith("audio test") || line.startsWith("AUDIO TEST")) {
    // audio test [freq_hz] [duration_sec]
    // Plays a pure sine wave from RAM — no SD, no WiFi needed.
    // Clean tone = hardware OK.  Crackling = hardware problem (power/PCM5102).
    String rest = line.substring(10); rest.trim();
    int freq = 1000, dur = 10;
    int sp = rest.indexOf(' ');
    if (sp >= 0) { freq = rest.substring(0,sp).toInt(); dur = rest.substring(sp+1).toInt(); }
    else if (rest.length()) { freq = rest.toInt(); }
    if (freq < 20 || freq > 20000) freq = 1000;
    if (dur  < 1  || dur  > 120)  dur  = 10;
    audioTestTone(freq, dur);
  }
  else if (line.equalsIgnoreCase("audio stop")) {
    audioTestActive = false;
    audioStop();
    dualPrint.println(F("[AUDIO] Stopped."));
  }
  else if (line.equalsIgnoreCase("clear config")) {
    clearConfig();
    dualPrint.println(F("[INFO] Reboot to apply factory defaults."));
  }
  else if (line.equalsIgnoreCase("wifi reconnect")) {
    WiFi.disconnect(true); delay(400); startWiFi();
    if (wifiConnected) setupWebServer();
  }
  else if (line.equalsIgnoreCase("wifi off")) {
    // Disable WiFi completely — diagnostic test for PLL_160M interference with I2S clock.
    // If audio crackling disappears → WiFi is disturbing the shared PLL → hardware shielding needed.
    // Restore with: wifi reconnect
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    dualPrint.println(F("[WiFi] Disabled. Play audio and check for crackling."));
    dualPrint.println(F("[WiFi] Restore with: wifi reconnect"));
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
    else if (key=="audio-sectors") {
      // Diagnostic: change SD read batch size at runtime without recompile.
      // Test: set audio-sectors 1  → if crackling stops, batch/FatFs boundary is the cause.
      //       set audio-sectors 32 → restore default (LCM-aligned, best quality).
      int n = constrain(val.toInt(), 1, AUDIO_BUF_SECTORS);
      audioReadSectors = n;
      dualPrint.printf("[AUDIO] Batch size: %d sectors = %d ms per read\n",
                       n, n * 2352 * 1000 / 176400);
      ok = false; // don't save to NVS — diagnostic only
    }
    else if (key=="audio-module") {
      String v=val; v.toLowerCase();
      bool newVal=(v=="on"||v=="1"||v=="yes");
      if (newVal != cfg.audioModule) {
        cfg.audioModule = newVal;
        if (cfg.audioModule) {
          if (!i2sTx) initI2S();
          dualPrint.println(F("[I2S]  Audio module enabled at runtime."));
        } else {
          audioStop();
          if (i2sTx) {
            i2s_chan_handle_t h = i2sTx;
            i2sTx = nullptr;       // core 0 sees nullptr → skips i2s_channel_write
            delay(50);             // wait for any in-flight write to finish
            i2s_channel_disable(h);
            i2s_del_channel(h);
            dualPrint.println(F("[I2S]  Audio module disabled at runtime."));
          }
        }
      }
    }
    else if (key=="web-auth")  { String v=val; v.toLowerCase(); cfg.webAuth=(v=="on"||v=="1"||v=="yes"); }
    else if (key=="dos-compat")   {
      String v=val; v.toLowerCase(); cfg.dosCompat=(v=="on"||v=="1"||v=="yes");
      if (!cfg.dosCompat) cfg.dosDriver=0;  // driver modes only meaningful with dosCompat ON
    }
    else if (key=="dos-driver")   { int d=constrain(val.toInt(),0,3); cfg.dosDriver=(uint8_t)d;
      if (d > 0) cfg.dosCompat=true;  // any specific driver mode requires dosCompat ON
    }
    else if (key=="device-mode") {
      String v=val; v.toLowerCase();
      if (v=="gotek" || v=="1") {
        cfg.deviceMode=1;
        dualPrint.println(F("[OK]  Device mode: GoTek floppy emulator — reboot to apply."));
      } else {
        cfg.deviceMode=0;
        dualPrint.println(F("[OK]  Device mode: CD-ROM — reboot to apply."));
      }
    }
    else if (key=="gotek-dir") {
      if (val.length() && val[0] != '/') val = "/" + val;
      gotekImgDir = val;
      dualPrint.printf("[GOTEK] Image directory set to: %s\n", gotekImgDir.c_str());
    }
    else if (key=="gotek-usb-mode") {
      val.toLowerCase();
      if (val=="fat" || val=="1") {
        cfg.gotekUsbMode = 1;
        dualPrint.println(F("[GOTEK] USB mode: FAT32 virtual (FlashFloppy: nav-mode=indexed in FF.CFG)"));
      } else {
        cfg.gotekUsbMode = 0;
        dualPrint.println(F("[GOTEK] USB mode: Raw LBA (stock GoTek firmware)"));
      }
    }
    else if (key=="gotek-fat-wp") {
      val.toLowerCase();
      uint8_t newWP = (val=="on"||val=="1"||val=="yes") ? 1 : 0;
      cfg.gotekFatWP = newWP;
      dualPrint.printf("[GOTEK] FAT32 write-protect: %s\n",
        cfg.gotekFatWP ? "ON (Windows cannot format)" : "OFF (writable)");
      if (cfg.deviceMode==1 && cfg.gotekUsbMode==1) {
        tud_disconnect(); delay(400);
        msc.isWritable(cfg.gotekFatWP==0);
        msc.begin(gkBlockCount(), GK_SECTOR_SIZE);
        msc.mediaPresent(true);
        tud_connect();
        dualPrint.println(F("[GOTEK] USB re-enumerated with new WP state"));
      }
    }
    else if (key=="https-enable")  { String v=val; v.toLowerCase(); cfg.httpsEnabled=(v=="on"||v=="1"||v=="yes"); }
    else if (key=="https-cert")    { strlcpy(cfg.httpsCertPath, val.c_str(), sizeof(cfg.httpsCertPath)); }
    else if (key=="https-key")     { strlcpy(cfg.httpsKeyPath,  val.c_str(), sizeof(cfg.httpsKeyPath)); }
    else if (key=="https-kpass")   { strlcpy(cfg.httpsKeyPass,  val.c_str(), sizeof(cfg.httpsKeyPass)); }
    else if (key=="http-port")    { cfg.httpPort  = (uint16_t)constrain(val.toInt(),1,65535); }
    else if (key=="https-port")   { cfg.httpsPort = (uint16_t)constrain(val.toInt(),1,65535); }
    else if (key=="tls-ver")      { cfg.tlsMinVer = (uint8_t)constrain(val.toInt(),0,1); }
    else if (key=="tls-ciphers")  { cfg.tlsCiphers= (uint8_t)constrain(val.toInt(),0,3); }
    else if (key=="wifi-txpower") {
      // Accept dBm value 2–20, converted to ESP-IDF units (×4).
      // ESP32-S3 supports 2–20 dBm. Default is 10 dBm (40 units).
      int dbm = constrain(val.toInt(), 2, 20);
      cfg.wifiTxPower = (uint8_t)(dbm * 4);
      if (wifiConnected) esp_wifi_set_max_tx_power(cfg.wifiTxPower);
      dualPrint.printf("[WiFi] TX power set to %d dBm (units=%d)%s\n",
                       dbm, cfg.wifiTxPower, wifiConnected ? " — applied live" : " — applied on next connect");
    }
    else if (key=="web-user")  { strlcpy(cfg.webUser, val.c_str(), sizeof(cfg.webUser)); }
    else if (key=="web-pass")  { strlcpy(cfg.webPass, val.c_str(), sizeof(cfg.webPass)); }
    else { dualPrint.printf("[ERR] Unknown key: '%s'\n", key.c_str()); ok=false; }
    if (ok) {
      saveConfig();
      dualPrint.printf("[OK]  %s = %s\n", key.c_str(), val.c_str());
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
            dualPrint.printf("[NVS]  Verified: %s cleared\n", nvsk.c_str());
          } else if (stored.length() || key=="eap-mode") {
            dualPrint.printf("[NVS]  Verified: %s = '%s'\n", nvsk.c_str(), stored.c_str());
          } else {
            dualPrint.printf("[NVS]  WARNING: %s not saved correctly!\n", nvsk.c_str());
          }
        }
        vfy.end();
      }
    }
  }
  else dualPrint.printf("[ERR] Unknown command: '%s'  (type help)\n", line.c_str());
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

  dualPrint.println();
  printSep();
  dualPrint.println(F("  ESP32-S3 Virtual CD-ROM + File Manager  v3.1"));
  printSep();

  // ── Reset reason diagnostic ───────────────────────────────────────────────
  // Reads PREVIOUS reset reason and shows it. If it was a crash, displays it
  // prominently. Also saves to RTC memory so HTML log can show it after WiFi.
  {
    esp_reset_reason_t rr = esp_reset_reason();
    const char* rrName = "UNKNOWN";
    bool wasCrash = false;
    switch(rr) {
      case ESP_RST_POWERON:   rrName = "POWER_ON";    rtcCrashMagic=0; break; // clear on power-on
      case ESP_RST_SW:        rrName = "SOFTWARE";    break;
      case ESP_RST_PANIC:     rrName = "PANIC/CRASH"; wasCrash=true; break;
      case ESP_RST_INT_WDT:   rrName = "INT_WDT";     wasCrash=true; break;
      case ESP_RST_TASK_WDT:  rrName = "TASK_WDT";    wasCrash=true; break;
      case ESP_RST_WDT:       rrName = "WDT";         wasCrash=true; break;
      case ESP_RST_DEEPSLEEP: rrName = "DEEP_SLEEP";  rtcCrashMagic=0; break;
      case ESP_RST_BROWNOUT:  rrName = "BROWNOUT";    wasCrash=true; break;
      default: break;
    }
    // Show previous crash from RTC (survives reboot, visible in serial NOW)
    if (rtcCrashMagic == RTC_CRASH_MAGIC && rtcCrashMsg[0]) {
      Serial.printf("[BOOT] *** PREVIOUS CRASH: %s ***\n", rtcCrashMsg);
    }
    if (wasCrash) {
      snprintf(rtcCrashMsg, sizeof(rtcCrashMsg), "%s (will show in HTML after WiFi)", rrName);
      rtcCrashMagic = RTC_CRASH_MAGIC;
      dualPrint.printf("[BOOT] *** LAST RESET: %s *** (backtrace on serial)\n", rrName);
    } else {
      dualPrint.printf("[BOOT] Reset reason: %s\n", rrName);
    }
  }
  // ─────────────────────────────────────────────────────────────────────────

  // Load config and print everything stored in NVS
  loadConfig();
  printConfig(false);  // NVS settings only (runtime not yet available)
  printHelp();

  if (cfg.deviceMode == 1) {
    // ── GoTek Floppy Emulator Mode ─────────────────────────────────────────
    // Only SD, WiFi, web server and GoTek USB MSC are initialised.
    // Audio task, PSRAM cache, CD-ROM SCSI and doMount are NOT started —
    // GoTek mode has no audio and no CD-ROM disc handling.
    dualPrint.println(F("[MODE] GoTek floppy emulator mode"));
    dualPrint.printf("[DIAG] cfg.deviceMode=%d  scsi_get_gotek_mode()=%d\n",
                     cfg.deviceMode, (int)scsi_get_gotek_mode());
    sdMutex = xSemaphoreCreateMutex();  // protects SD from concurrent TinyUSB + web access
    sdReady = initSD();                 // must be before gkBuildFS
    if (sdReady) gkBuildFS();          // scan slots before USB init (sets correct slot count)
    initUSBGoTek();                     // uses gkFC for slot info in log
    startWiFi();
    if (wifiConnected) setupWebServer();
    dualPrint.println(F("\n[READY] GoTek mode active. Type 'help' for commands.\n"));
    dualPrint.printf("[GOTEK] Place .img files in %s on the SD card.\n", gotekImgDir.c_str());
  } else {
    // ── CD-ROM Mode (default) ──────────────────────────────────────────────
    initI2S();
    sdMutex = xSemaphoreCreateMutex();  // shared between audio task and SCSI DAE reader
    startAudioTask();
    cacheAlloc();    // CD-ROM PSRAM read-ahead cache (512 KB, 2×128 sectors LRU)
    gkCacheAlloc();  // GoTek PSRAM sector cache (128 KB, 256 sectors)
    initUSBMSC();
    sdReady = initSD();
    startWiFi();
    if (wifiConnected) setupWebServer();

    dualPrint.println(F("\n[READY] Type 'help' for available commands\n"));

    // Auto-mount default image
    if (sdReady && defaultMount.length()) {
      dualPrint.printf("[AUTO] Default image: %s\n", defaultMount.c_str());
      File _f = SD_MMC.open(defaultMount.c_str());
      if (_f) { _f.close(); doMount(defaultMount); }
      else dualPrint.println(F("[AUTO] File not found on SD card."));
    }
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
  if (cfg.deviceMode == 0) scsi_dae_check_timeout();  // CD-ROM mode only
  delay(1);
}
