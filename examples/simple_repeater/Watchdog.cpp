#include "MyMesh.h"

// Two ways for a repeater to recover by itself instead of staying dead until someone power-cycles it:
//
//  - Hardware watchdog ("set watchdog <secs>"): the main loop feeds it on every pass. If the firmware
//    hangs (stuck loop, deadlock, crash), the chip resets after the timeout. Default 300 s. On nRF52 a
//    running watchdog can't be stopped or changed, and it survives a soft reboot, so a new value takes
//    effect after the next power-on or watchdog reset. Boards without one report "unsupported".
//
//  - Receive watchdog ("set rx.watchdog <mins>"): if no valid packet has been received for that many
//    minutes, reboot. Catches a radio that has gone deaf while the CPU keeps running. Off by default:
//    only useful on a site that always hears traffic (a quiet site would reboot for nothing).

#define WATCHDOG_FILE          "/watchdog"
#define WATCHDOG_FILE_VERSION  1
#define WATCHDOG_DEFAULT_SECS  300
#define WATCHDOG_MIN_SECS      30
#define WATCHDOG_MAX_SECS      3600
#define RX_WATCHDOG_MIN_MINS   5
#define RX_WATCHDOG_MAX_MINS   1440

struct WatchdogFile {
  uint8_t  version;
  uint8_t  reserved;
  uint16_t hw_secs;       // 0 = off
  uint16_t rx_mins;       // 0 = off
  uint16_t rx_reboots;    // times the receive watchdog has rebooted this node
};

static File watchdogOpenWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

void MyMesh::loadWatchdog() {
  wdt_secs = WATCHDOG_DEFAULT_SECS;
  rxwdt_mins = 0;
  rxwdt_reboots = 0;
  if (_fs->exists(WATCHDOG_FILE)) {
#if defined(RP2040_PLATFORM)
    File file = _fs->open(WATCHDOG_FILE, "r");
#else
    File file = _fs->open(WATCHDOG_FILE);
#endif
    if (file) {
      WatchdogFile f;
      if (file.read((uint8_t *)&f, sizeof(f)) == sizeof(f) && f.version == WATCHDOG_FILE_VERSION) {
        wdt_secs = f.hw_secs;
        rxwdt_mins = f.rx_mins;
        rxwdt_reboots = f.rx_reboots;
      }
      file.close();
    }
  }
  rxwdt_last_count = getNumRecvFlood() + getNumRecvDirect();
  rxwdt_last_rx = _ms->getMillis();
}

bool MyMesh::saveWatchdog() {
  WatchdogFile f;
  memset(&f, 0, sizeof(f));
  f.version = WATCHDOG_FILE_VERSION;
  f.hw_secs = wdt_secs;
  f.rx_mins = rxwdt_mins;
  f.rx_reboots = rxwdt_reboots;
  File file = watchdogOpenWrite(_fs, WATCHDOG_FILE);
  if (!file) return false;
  bool ok = file.write((const uint8_t *)&f, sizeof(f)) == sizeof(f);
  file.close();
  return ok;
}

void MyMesh::startWatchdog() {
  board.startWatchdog(wdt_secs);   // no-op if unsupported, or if still running from before a soft reboot
}

void MyMesh::watchdogLoop() {
  board.feedWatchdog();

  uint32_t count = getNumRecvFlood() + getNumRecvDirect();
  unsigned long now = _ms->getMillis();
  if (count != rxwdt_last_count) {
    rxwdt_last_count = count;
    rxwdt_last_rx = now;
  } else if (rxwdt_mins > 0 && now - rxwdt_last_rx >= (unsigned long)rxwdt_mins * 60000UL) {
    MESH_DEBUG_PRINTLN("rx.watchdog: nothing received for %u min, rebooting", (uint32_t)rxwdt_mins);
    rxwdt_reboots++;
    saveWatchdog();
    board.reboot();
  }
}

static void formatAgo(char* dest, unsigned long ms) {
  unsigned long s = ms / 1000;
  if (s < 120) sprintf(dest, "%lus", s);
  else if (s < 7200) sprintf(dest, "%lum", s / 60);
  else sprintf(dest, "%luh", s / 3600);
}

bool MyMesh::handleWatchdogCommand(const char* command, char* reply) {
  if (strcmp(command, "get watchdog") == 0) {
    uint32_t running = board.getWatchdogSecs();
    int n;
    if (wdt_secs == 0) n = sprintf(reply, "> off");
    else n = sprintf(reply, "> %us", (uint32_t)wdt_secs);
    if (running == 0 && wdt_secs != 0) {
      n += sprintf(&reply[n], " (not running: unsupported on this board)");
    } else if (running != 0 && running != wdt_secs) {
      n += sprintf(&reply[n], " (running %lus until power-on/watchdog reset)", (unsigned long)running);
    }
    sprintf(&reply[n], ", last reset: %s", board.getResetReasonString(board.getResetReason()));
    return true;
  }
  if (memcmp(command, "set watchdog ", 13) == 0) {
    const char* arg = &command[13];
    while (*arg == ' ') arg++;
    long secs = strcmp(arg, "off") == 0 ? 0 : atol(arg);
    if (!(secs == 0 && (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0))
        && (secs < WATCHDOG_MIN_SECS || secs > WATCHDOG_MAX_SECS)) {
      sprintf(reply, "ERR: %d-%d secs, or 0/off", WATCHDOG_MIN_SECS, WATCHDOG_MAX_SECS);
      return true;
    }
    wdt_secs = (uint16_t)secs;
    if (!saveWatchdog()) {
      strcpy(reply, "ERR: could not save");
      return true;
    }
    uint32_t running = board.getWatchdogSecs();
    if (running == 0 && wdt_secs > 0) {
      strcpy(reply, board.startWatchdog(wdt_secs) ? "OK - started" : "OK - saved (unsupported on this board)");
    } else if (running != 0 && running != wdt_secs) {
      strcpy(reply, "OK - applies after the next power-on or watchdog reset");
    } else {
      strcpy(reply, "OK");
    }
    return true;
  }
  if (strcmp(command, "get rx.watchdog") == 0) {
    char ago[16];
    formatAgo(ago, _ms->getMillis() - rxwdt_last_rx);
    if (rxwdt_mins == 0) {
      sprintf(reply, "> off, last rx %s ago", ago);
    } else {
      sprintf(reply, "> %umin, last rx %s ago, %u reboots", (uint32_t)rxwdt_mins, ago, (uint32_t)rxwdt_reboots);
    }
    return true;
  }
  if (memcmp(command, "set rx.watchdog ", 16) == 0) {
    const char* arg = &command[16];
    while (*arg == ' ') arg++;
    long mins = strcmp(arg, "off") == 0 ? 0 : atol(arg);
    if (!(mins == 0 && (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0))
        && (mins < RX_WATCHDOG_MIN_MINS || mins > RX_WATCHDOG_MAX_MINS)) {
      sprintf(reply, "ERR: %d-%d mins, or 0/off", RX_WATCHDOG_MIN_MINS, RX_WATCHDOG_MAX_MINS);
      return true;
    }
    rxwdt_mins = (uint16_t)mins;
    rxwdt_last_rx = _ms->getMillis();    // start the count from now
    strcpy(reply, saveWatchdog() ? "OK" : "ERR: could not save");
    return true;
  }
  return false;
}
