#include "MyMesh.h"

// Trusted clock source: "set clock.source <pubkey>" names one node whose timestamps this repeater trusts.
// Whenever an authenticated packet from that node arrives with a timestamp (its signed adverts, or a
// login/request/CLI message/bot DM it sends us), the clock is stepped forward to it, within sane limits:
//
//  - Forward only. The mesh needs each node's timestamps to keep increasing, so the clock never goes back.
//  - Differences under CLOCK_SRC_MIN_STEP are ignored: they're just the time the packet spent in transit.
//  - Once our clock is set, a step is at most CLOCK_SRC_MAX_STEP, so a source that was set far into the
//    future by mistake can't drag us with it (we couldn't come back).
//  - If our clock is clearly unset (earlier than this firmware's build time, e.g. back at the 2024 default
//    after a reboot), any time from the build time up to CLOCK_SRC_MAX_AHEAD after it is accepted.
//
// Old packets replayed by someone else can't move the clock: their timestamps are behind ours.

#define CLOCK_SRC_FILE      "/clock_src"
#define CLOCK_SRC_MIN_STEP  5                          // secs
#define CLOCK_SRC_MAX_STEP  (7UL * 24 * 3600)          // 7 days
#define CLOCK_SRC_MAX_AHEAD (10UL * 366 * 24 * 3600)   // ~10 years past the build time

static uint32_t buildTime() {
  static uint32_t t = 0;
  if (t == 0) t = DateTime(__DATE__, __TIME__).unixtime();
  return t;
}

static File clockSrcOpenWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

void MyMesh::loadClockSource() {
  has_clock_src = false;
  if (!_fs->exists(CLOCK_SRC_FILE)) return;
#if defined(RP2040_PLATFORM)
  File file = _fs->open(CLOCK_SRC_FILE, "r");
#else
  File file = _fs->open(CLOCK_SRC_FILE);
#endif
  if (!file) return;
  has_clock_src = file.read(clock_src_key, PUB_KEY_SIZE) == PUB_KEY_SIZE;
  file.close();
}

bool MyMesh::saveClockSource() {
  if (!has_clock_src) {
    if (_fs->exists(CLOCK_SRC_FILE)) _fs->remove(CLOCK_SRC_FILE);
    return true;
  }
  File file = clockSrcOpenWrite(_fs, CLOCK_SRC_FILE);
  if (!file) return false;
  bool ok = file.write(clock_src_key, PUB_KEY_SIZE) == PUB_KEY_SIZE;
  file.close();
  return ok;
}

void MyMesh::checkClockSource(const mesh::Identity& id, uint32_t timestamp, char via) {
  if (!has_clock_src || !id.matches(clock_src_key)) return;

  uint32_t now = getRTCClock()->getCurrentTime();
  clock_src_heard = now;
  clock_src_diff = (int32_t)(timestamp - now);
  clock_src_via = via;

  bool ok;
  if (now < buildTime()) {   // our clock is unset
    ok = timestamp >= buildTime() && timestamp - buildTime() <= CLOCK_SRC_MAX_AHEAD;
  } else if (timestamp > now + CLOCK_SRC_MIN_STEP) {
    ok = timestamp - now <= CLOCK_SRC_MAX_STEP;
  } else {
    return;   // in step (or behind, by transit time): nothing to do
  }
  if (!ok) {
    clock_src_rejects++;
    MESH_DEBUG_PRINTLN("clock source: rejected step of %ld s", (long)clock_src_diff);
    return;
  }

  getRTCClock()->setCurrentTime(timestamp);
  clock_src_heard = clock_src_synced = timestamp;
  clock_src_step = clock_src_diff;
  clock_src_diff = 0;
  clock_src_syncs++;
#ifdef REPEATER_BOT
  botLog("clock stepped %+ld s from clock source (%c)", (long)clock_src_step, via);
#endif
}

static const char* clockSrcVia(char via) {
  switch (via) {
    case 'A': return "advert";
    case 'L': return "login";
    case 'R': return "request";
    case 'M': return "message";
    case 'B': return "bot DM";
    default:  return "?";
  }
}

// "get clock.source" / "set clock.source <64 hex pubkey>|off"
bool MyMesh::handleClockSourceCommand(const char* command, char* reply) {
  if (strcmp(command, "get clock.source") == 0) {
    if (!has_clock_src) {
      strcpy(reply, "> off");
      return true;
    }
    int n = sprintf(reply, "> ");
    mesh::Utils::toHex(&reply[n], clock_src_key, 8);
    n += 16;
    if (clock_src_heard == 0) {
      sprintf(&reply[n], "... not heard yet");
    } else {
      DateTime h(clock_src_heard);
      n += sprintf(&reply[n], "... heard %02d:%02d (%s, diff %+lds)", h.hour(), h.minute(),
                   clockSrcVia(clock_src_via), (long)clock_src_diff);
      if (clock_src_syncs > 0) {
        DateTime s(clock_src_synced);
        n += sprintf(&reply[n], ", %lu steps, last %+lds at %02d:%02d", (unsigned long)clock_src_syncs,
                     (long)clock_src_step, s.hour(), s.minute());
      }
      if (clock_src_rejects > 0) sprintf(&reply[n], ", %lu rejected", (unsigned long)clock_src_rejects);
    }
    return true;
  }

  if (memcmp(command, "set clock.source ", 17) != 0) return false;
  const char* arg = &command[17];
  while (*arg == ' ') arg++;

  if (strcmp(arg, "off") == 0 || strcmp(arg, "-") == 0) {
    has_clock_src = false;
    strcpy(reply, saveClockSource() ? "OK - clock source off" : "ERR: could not save");
    return true;
  }
  uint8_t key[PUB_KEY_SIZE];
  if (strlen(arg) != PUB_KEY_SIZE * 2 || !mesh::Utils::fromHex(key, PUB_KEY_SIZE, arg)) {
    strcpy(reply, "ERR: need the full 64 hex char public key, or 'off'");
    return true;
  }
  if (self_id.matches(key)) {
    strcpy(reply, "ERR: that is this repeater");
    return true;
  }
  memcpy(clock_src_key, key, PUB_KEY_SIZE);
  has_clock_src = true;
  clock_src_heard = clock_src_synced = clock_src_syncs = clock_src_rejects = 0;
  clock_src_diff = clock_src_step = 0;
  if (!saveClockSource()) {
    strcpy(reply, "ERR: could not save");
    return true;
  }
  int n = sprintf(reply, "OK - clock source ");
  mesh::Utils::toHex(&reply[n], key, 8);
  strcpy(&reply[n + 16], "...");
  return true;
}
