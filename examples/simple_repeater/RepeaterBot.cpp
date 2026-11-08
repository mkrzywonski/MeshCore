#include "MyMesh.h"

#ifdef REPEATER_BOT

#include <stdarg.h>
#include <stddef.h>

#define BOT_PREFS_VERSION      2    // v2 added 'format'; v1 files still load (format = short)
#define BOT_ACK_DELAY        200
#define BOT_PATH_WAIT_BASE  2000    // ms to wait for the sender's reciprocal path, plus...
#define BOT_PATH_WAIT_HOP   2500    // ...this much per hop
#define BOT_PATH_WAIT_MAX  30000
#define BOT_ACK_WAIT_BASE   4000    // ms to wait for the ACK of a direct reply, plus...
#define BOT_ACK_WAIT_HOP    2500    // ...this much per hop

static File botOpenWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

// ---- event log: what the bot did with each message, kept in RAM for "bot log" ----

void MyMesh::botLog(const char* fmt, ...) {
  BotLogEntry& e = bot_log[bot_log_next];
  bot_log_next = (bot_log_next + 1) % BOT_LOG_SIZE;
  if (bot_log_count < BOT_LOG_SIZE) bot_log_count++;
  e.time = getRTCClock()->getCurrentTime();
  va_list args;
  va_start(args, fmt);
  vsnprintf(e.msg, sizeof(e.msg), fmt, args);
  va_end(args);
#if MESH_PACKET_LOGGING
  Serial.printf("BOT %02lu:%02lu:%02lu %s\r\n", (unsigned long)(e.time / 3600 % 24),
                (unsigned long)(e.time / 60 % 60), (unsigned long)(e.time % 60), e.msg);
#endif
}

const char* MyMesh::botWho(const BotContact& c, char* buf, size_t len) const {
  snprintf(buf, len, "%.12s/%02x", c.name[0] ? c.name : "?", (uint32_t)c.id.pub_key[0]);
  return buf;
}

// "bot log": over serial, print every entry (oldest first); remotely the reply holds one entry,
// so "bot log [n]" returns the n-th newest (default 1). "bot log clear" empties it.
void MyMesh::botLogCommand(uint32_t sender_timestamp, const char* arg, char* reply) {
  if (strcmp(arg, "clear") == 0) {
    bot_log_next = bot_log_count = 0;
    strcpy(reply, "OK");
    return;
  }
  if (bot_log_count == 0) {
    strcpy(reply, "(bot log is empty)");
    return;
  }
  if (sender_timestamp == 0 && *arg == 0) {   // serial console: dump it all
    for (int i = bot_log_count; i >= 1; i--) {
      const BotLogEntry& e = bot_log[(bot_log_next + BOT_LOG_SIZE - i) % BOT_LOG_SIZE];
      Serial.printf("  %02lu:%02lu:%02lu %s\r\n", (unsigned long)(e.time / 3600 % 24),
                    (unsigned long)(e.time / 60 % 60), (unsigned long)(e.time % 60), e.msg);
    }
    sprintf(reply, "%d entries (times UTC)", bot_log_count);
    return;
  }
  int n = *arg ? atoi(arg) : 1;
  if (n < 1 || n > bot_log_count) {
    sprintf(reply, "Err - 1..%d", bot_log_count);
    return;
  }
  const BotLogEntry& e = bot_log[(bot_log_next + BOT_LOG_SIZE - n) % BOT_LOG_SIZE];
  snprintf(reply, 160, "%d/%d %02lu:%02lu:%02lu %s", n, bot_log_count, (unsigned long)(e.time / 3600 % 24),
           (unsigned long)(e.time / 60 % 60), (unsigned long)(e.time % 60), e.msg);
}

void MyMesh::loadBotPrefs() {
  if (!_fs->exists(BOT_PREFS_FILE)) return;   // keep defaults
#if defined(RP2040_PLATFORM)
  File file = _fs->open(BOT_PREFS_FILE, "r");
#else
  File file = _fs->open(BOT_PREFS_FILE);
#endif
  if (!file) return;
  BotPrefs tmp;
  memset(&tmp, 0, sizeof(tmp));
  size_t n = file.read((uint8_t *)&tmp, sizeof(tmp));
  bool ok = (tmp.version == BOT_PREFS_VERSION && n == sizeof(tmp))
         || (tmp.version == 1 && n >= offsetof(BotPrefs, format));   // v1: no format byte
  if (ok) {
    if (tmp.version == 1) tmp.format = BOT_FORMAT_SHORT;
    if (tmp.format > BOT_FORMAT_LONG) tmp.format = BOT_FORMAT_SHORT;
    tmp.name[sizeof(tmp.name) - 1] = 0;
    bot_prefs = tmp;
  }
  file.close();
}

bool MyMesh::saveBotPrefs() {
  bot_prefs.version = BOT_PREFS_VERSION;
  File file = botOpenWrite(_fs, BOT_PREFS_FILE);
  if (!file) return false;
  bool ok = file.write((const uint8_t *)&bot_prefs, sizeof(bot_prefs)) == sizeof(bot_prefs);
  file.close();
  return ok;
}

const char* MyMesh::getBotName(char* buf, size_t len) const {
  if (bot_prefs.name[0]) {
    snprintf(buf, len, "%s", bot_prefs.name);
  } else {
    snprintf(buf, len, "%s Bot", _prefs.node_name);
  }
  return buf;
}

mesh::Packet* MyMesh::createBotAdvert() {
  char name[32];
  getBotName(name, sizeof(name));

  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  uint8_t app_data_len;
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
    AdvertDataBuilder builder(ADV_TYPE_CHAT, name);
    app_data_len = builder.encodeTo(app_data);
  } else if (_prefs.advert_loc_policy == ADVERT_LOC_SHARE) {
    AdvertDataBuilder builder(ADV_TYPE_CHAT, name, sensors.node_lat, sensors.node_lon);
    app_data_len = builder.encodeTo(app_data);
  } else {
    AdvertDataBuilder builder(ADV_TYPE_CHAT, name, _prefs.node_lat, _prefs.node_lon);
    app_data_len = builder.encodeTo(app_data);
  }
  return createAdvert(bot_id, app_data, app_data_len);
}

void MyMesh::sendBotAdvert(bool flood, uint32_t delay_millis) {
  if (!bot_prefs.enabled) return;
  mesh::Packet* pkt = createBotAdvert();
  if (pkt == NULL) return;
  if (flood) {
    sendFloodScoped(default_scope, pkt, delay_millis, _prefs.path_hash_mode + 1);
  } else {
    sendZeroHop(pkt, delay_millis);
  }
}

// Adds/refreshes a contact in the RAM table. Returns its index, or -1.
int MyMesh::botLearnContact(const mesh::Identity& id, const char* name) {
  if (id.matches(bot_id) || id.matches(self_id)) return -1;

  uint32_t now = getRTCClock()->getCurrentTime();
  int idx = -1;
  for (int i = 0; i < num_bot_contacts; i++) {
    if (bot_contacts[i].id.matches(id)) { idx = i; break; }
  }
  if (idx < 0) {
    if (num_bot_contacts < BOT_MAX_CONTACTS) {
      idx = num_bot_contacts++;
    } else {
      // evict the least recently heard contact that has no reply in flight, preferring ones that
      // are not in the flash cache (those are people who actually use the bot)
      for (int pass = 0; pass < 2 && idx < 0; pass++) {
        uint32_t oldest = 0xFFFFFFFF;
        for (int i = 0; i < num_bot_contacts; i++) {
          if (pass == 0 && bot_contacts[i].cached) continue;
          bool busy = false;
          for (int j = 0; j < BOT_MAX_PENDING; j++) {
            if (bot_pending[j].active && bot_pending[j].contact_idx == i) busy = true;
          }
          if (!busy && bot_contacts[i].last_heard < oldest) { oldest = bot_contacts[i].last_heard; idx = i; }
        }
      }
      if (idx < 0) return -1;
    }
    BotContact& c = bot_contacts[idx];
    memset(&c, 0, sizeof(c));
    c.id = id;
    c.out_path_len = OUT_PATH_UNKNOWN;
  }
  BotContact& c = bot_contacts[idx];
  c.last_heard = now;
  if (name && *name) {
    snprintf(c.name, sizeof(c.name), "%s", name);
  }
  return idx;
}

// ---- flash contact cache ----

#define BOT_CACHE_VERSION 1

void MyMesh::loadBotCache() {
  if (!_fs->exists(BOT_CACHE_FILE)) return;
#if defined(RP2040_PLATFORM)
  File file = _fs->open(BOT_CACHE_FILE, "r");
#else
  File file = _fs->open(BOT_CACHE_FILE);
#endif
  if (!file) return;
  uint8_t hdr[3];   // version, count, next
  if (file.read(hdr, 3) == 3 && hdr[0] == BOT_CACHE_VERSION && hdr[1] <= BOT_CACHE_SIZE && hdr[2] < BOT_CACHE_SIZE) {
    int n = 0;
    while (n < hdr[1] && file.read((uint8_t *)&bot_cache[n], sizeof(BotCacheEntry)) == sizeof(BotCacheEntry)) {
      bot_cache[n].name[BOT_CACHE_NAME_LEN - 1] = 0;
      n++;
    }
    bot_cache_count = n;
    bot_cache_next = n < BOT_CACHE_SIZE ? n : hdr[2];
  }
  file.close();

  for (int i = 0; i < bot_cache_count; i++) {
    int idx = botLearnContact(mesh::Identity(bot_cache[i].pub_key), bot_cache[i].name);
    if (idx >= 0) bot_contacts[idx].cached = true;
  }
}

bool MyMesh::saveBotCache() {
  File file = botOpenWrite(_fs, BOT_CACHE_FILE);
  if (!file) return false;
  uint8_t hdr[3] = { BOT_CACHE_VERSION, bot_cache_count, bot_cache_next };
  bool ok = file.write(hdr, 3) == 3;
  for (int i = 0; ok && i < bot_cache_count; i++) {
    ok = file.write((const uint8_t *)&bot_cache[i], sizeof(BotCacheEntry)) == sizeof(BotCacheEntry);
  }
  file.close();
  return ok;
}

// Save a contact to the flash cache, unless it's already there (no write in that case).
void MyMesh::botCacheContact(BotContact& c) {
  if (c.cached) return;
  for (int i = 0; i < bot_cache_count; i++) {
    if (memcmp(bot_cache[i].pub_key, c.id.pub_key, PUB_KEY_SIZE) == 0) { c.cached = true; return; }
  }
  BotCacheEntry& e = bot_cache[bot_cache_next];   // FIFO: overwrites the oldest once full
  if (bot_cache_count == BOT_CACHE_SIZE) {
    // the contact being replaced is no longer protected from RAM eviction
    for (int i = 0; i < num_bot_contacts; i++) {
      if (memcmp(bot_contacts[i].id.pub_key, e.pub_key, PUB_KEY_SIZE) == 0) bot_contacts[i].cached = false;
    }
  }
  memcpy(e.pub_key, c.id.pub_key, PUB_KEY_SIZE);
  snprintf(e.name, sizeof(e.name), "%s", c.name);
  bot_cache_next = (bot_cache_next + 1) % BOT_CACHE_SIZE;
  if (bot_cache_count < BOT_CACHE_SIZE) bot_cache_count++;
  c.cached = true;
  char who[20];
  bool ok = saveBotCache();
  botLog("saved %s to contact cache (%d/%d)%s", botWho(c, who, sizeof(who)), bot_cache_count, BOT_CACHE_SIZE,
         ok ? "" : " - WRITE FAILED");
}

// Called for every received packet, before the normal Mesh processing.
// returns true if the packet was addressed to the bot and has been consumed.
bool MyMesh::botHandleRecv(mesh::Packet* pkt) {
  if (!bot_prefs.enabled) return false;

  uint8_t type = pkt->getPayloadType();
  if (type != PAYLOAD_TYPE_TXT_MSG && type != PAYLOAD_TYPE_PATH) return false;
  if (pkt->isRouteDirect() && pkt->getPathHashCount() > 0) return false;   // still in transit
  if (pkt->payload_len <= 2 + CIPHER_MAC_SIZE) return false;

  uint8_t dest_hash = pkt->payload[0];
  uint8_t src_hash = pkt->payload[1];
  if (!bot_id.isHashMatch(&dest_hash)) return false;
  if (getTables()->wasSeen(pkt)) return false;   // duplicate, let the normal path drop it

  int candidates = 0;
  for (int i = 0; i < num_bot_contacts; i++) {
    BotContact& c = bot_contacts[i];
    if (!c.id.isHashMatch(&src_hash)) continue;
    candidates++;

    if (!c.has_secret) {
      bot_id.calcSharedSecret(c.secret, c.id);
      c.has_secret = true;
    }
    uint8_t data[MAX_PACKET_PAYLOAD + 1];
    int len = mesh::Utils::MACThenDecrypt(c.secret, data, &pkt->payload[2], pkt->payload_len - 2);
    if (len <= 0) continue;

    getTables()->markSeen(pkt);
    c.last_heard = getRTCClock()->getCurrentTime();
    if (type == PAYLOAD_TYPE_TXT_MSG) {
      botHandleText(pkt, c, data, len);
    } else {
      botHandlePath(pkt, c, data, len);
    }
    pkt->markDoNotRetransmit();   // NOTE: clobbers header, so do this last
    return true;
  }
  // unknown sender (we have not heard their advert): nothing we can decrypt, carry on as normal.
  // (also logged when another node merely shares the bot's 1-byte hash)
  botLog("%s to bot from %02x: %s", type == PAYLOAD_TYPE_TXT_MSG ? "TXT" : "PATH", (uint32_t)src_hash,
         candidates ? "decrypt failed" : "unknown sender, need advert");
  return false;
}

void MyMesh::botFormatReport(mesh::Packet* pkt, const BotContact& from, char* text, size_t len) {
  float snr = pkt->getSNR();
  int rssi = (int)_radio->getLastRSSI();
  int n = 0;

  if (bot_prefs.format == BOT_FORMAT_SHORT) {
    // Path (sender end first) then signal, nothing else:
    //   flood:          "61,36,b4 -3.5dB -110dBm"    the path this message took
    //   direct-routed:  "~61,0d,c0 -5.0dB -114dBm"   the known route (a direct packet's path is
    //                                                 used up on the way, so show our stored route
    //                                                 to them, reversed)
    //   direct, route unknown (e.g. after a reboot): "~? -5.0dB -114dBm"
    //   0 hops:         "13.0dB -38dBm"              heard straight from the sender's radio
    const uint8_t* path = NULL;
    uint8_t hops = 0, hs = 1;
    bool reversed = false;
    if (pkt->isRouteDirect()) {
      if (from.out_path_len == OUT_PATH_UNKNOWN) {
        // routes live in RAM, so after a reboot a direct message can arrive along a route we
        // don't know: say so, rather than look like "heard straight from the sender"
        n = snprintf(text, len, "~?");
      } else {
        hops = from.out_path_len & 63;
        hs = (from.out_path_len >> 6) + 1;
        path = from.out_path;
        reversed = true;
        if (hops) text[n++] = '~';
      }
    } else {
      hops = pkt->getPathHashCount();
      hs = pkt->getPathHashSize();
      path = pkt->path;
    }
    size_t room = len - 24;   // keep space for the signal part
    for (int i = 0; i < hops && (size_t)n < room; i++) {
      int h = reversed ? hops - 1 - i : i;
      if (i > 0) text[n++] = ',';
      for (int b = 0; b < hs; b++) {
        n += snprintf(&text[n], len - n, "%02x", path[h * hs + b]);
      }
    }
    if (hops && (size_t)n >= room) n += snprintf(&text[n], len - n, "..");
    snprintf(&text[n], len - n, "%s%.1fdB %ddBm", n ? " " : "", snr, rssi);
    return;
  }

  if (pkt->isRouteDirect()) {
    n = snprintf(text, len, "Heard you via direct route");
  } else {
    uint8_t hops = pkt->getPathHashCount();
    uint8_t hs = pkt->getPathHashSize();
    if (hops == 0) {
      n = snprintf(text, len, "Heard you direct, 0 hops");
    } else {
      n = snprintf(text, len, "Heard you: %u hop%s via ", hops, hops == 1 ? "" : "s");
      size_t room = len - 40;   // keep space for the signal part
      for (int h = 0; h < hops && n > 0 && (size_t)n < room; h++) {
        if (h > 0) text[n++] = ',';
        for (int b = 0; b < hs; b++) {
          n += snprintf(&text[n], len - n, "%02x", pkt->path[h * hs + b]);
        }
      }
      if (n > 0 && (size_t)n >= room) n += snprintf(&text[n], len - n, "...");
    }
  }
  if (n > 0 && (size_t)n < len) {
    snprintf(&text[n], len - n, " | SNR %.2f dB RSSI %d dBm", snr, rssi);
  }
}

void MyMesh::botSendAck(mesh::Packet* pkt, BotContact& to, const uint8_t* ack_hash, uint8_t ack_len) {
  const char* how;
  bool ok;
  if (pkt->isRouteFlood()) {
    // tell the sender the path TO here (so they can go direct), with the ACK piggy-backed
    mesh::Packet* path = createPathReturn(to.id, to.secret, pkt->path, pkt->path_len,
                                          PAYLOAD_TYPE_ACK, ack_hash, ack_len);
    if ((ok = path != NULL)) {
      bot_id.copyHashTo(&path->payload[1]);   // src hash is the bot, not the repeater
      sendFloodReply(path, BOT_ACK_DELAY, pkt->getPathHashSize());
    }
    how = "path+ack flood";
  } else if (to.out_path_len == OUT_PATH_UNKNOWN) {
    mesh::Packet* ack = createAck(ack_hash, ack_len);
    if ((ok = ack != NULL)) sendFloodScoped(default_scope, ack, BOT_ACK_DELAY, _prefs.path_hash_mode + 1);
    how = "ack flood";
  } else {
    uint32_t d = BOT_ACK_DELAY;
    if (getExtraAckTransmitCount() > 0) {
      mesh::Packet* a1 = createMultiAck(ack_hash, ack_len, 1);
      if (a1) sendDirect(a1, to.out_path, to.out_path_len, d);
      d += 300;
    }
    mesh::Packet* a2 = createAck(ack_hash, ack_len);
    if ((ok = a2 != NULL)) sendDirect(a2, to.out_path, to.out_path_len, d);
    how = "ack direct";
  }
  botLog(" %s %s (txq %d, free %d)", how, ok ? "queued" : "FAILED: no packet buffer",
         _mgr->getOutboundTotal(), _mgr->getFreeCount());
}

void MyMesh::botHandleText(mesh::Packet* pkt, BotContact& from, uint8_t* data, size_t len) {
  if (len <= 5) return;
  uint32_t sender_timestamp;
  memcpy(&sender_timestamp, data, 4);
  checkClockSource(from.id, sender_timestamp, 'B');
  uint8_t flags = data[4] >> 2;
  if (flags != TXT_TYPE_PLAIN) return;   // CLI and signed messages are not for us

  data[len] = 0;
  size_t text_len = strlen((const char *)&data[5]);

  char who[20];
  botWho(from, who, sizeof(who));
  botLog("TXT from %s %s %u hops, try %u, ts %08lx, snr %.1f", who, pkt->isRouteFlood() ? "flood" : "direct",
         (uint32_t)pkt->getPathHashCount(), (uint32_t)(data[4] & 3), (unsigned long)sender_timestamp, pkt->getSNR());
  botCacheContact(from);   // a successful DM: remember them across reboots (no-op if already saved)

  // ACK exactly as a companion does: truncated hash of timestamp + flags + text + sender pub_key
  uint8_t ack_hash[6];
  mesh::Utils::sha256(ack_hash, 4, data, 5 + text_len, from.id.pub_key, PUB_KEY_SIZE);
  ack_hash[4] = (5 + text_len + 1 < len) ? data[5 + text_len + 1] : 0;   // extended attempt byte, if any
  getRNG()->random(&ack_hash[5], 1);
  botSendAck(pkt, from, ack_hash, sizeof(ack_hash));

  if (pkt->isRouteFlood()) {
    // A flood means the sender had no (working) route to us, so any route we stored is stale.
    // Use the reverse of the path this flood took; our PATH+ACK just told the sender to use that
    // same path, so their following direct messages arrive along it. A reciprocal PATH from the
    // sender (if one arrives) replaces this with the route they measured.
    uint8_t hs = pkt->getPathHashSize();
    uint8_t hc = pkt->getPathHashCount();
    for (int h = 0; h < hc; h++) {
      memcpy(&from.out_path[h * hs], &pkt->path[(hc - 1 - h) * hs], hs);
    }
    from.out_path_len = pkt->path_len;   // same hash size and count
  }

  bot_msgs_recv++;
  if (sender_timestamp == from.last_msg_timestamp) {   // a retry of a message we already answered
    botLog(" retry of a message already handled: no new reply");
    return;
  }
  from.last_msg_timestamp = sender_timestamp;

  uint32_t now = getRTCClock()->getCurrentTime();
  if (from.last_reply_secs && now >= from.last_reply_secs && now - from.last_reply_secs < BOT_MIN_REPLY_INTERVAL_SECS) {
    botLog(" no reply: < %d s since last reply to this contact", BOT_MIN_REPLY_INTERVAL_SECS);
    return;
  }
  if (!bot_limiter.allow(now)) {
    botLog(" no reply: global limit (%d/min)", BOT_MAX_REPLIES_PER_MIN);
    return;
  }

  int idx = &from - bot_contacts;
  BotPendingReply* slot = NULL;
  for (int j = 0; j < BOT_MAX_PENDING; j++) {
    if (bot_pending[j].active && bot_pending[j].contact_idx == idx) {   // replace
      slot = &bot_pending[j];
      botLog(" replaces the reply still pending for the previous message%s",
             slot->awaiting_ack ? " (its ack wait is dropped)" : "");
      break;
    }
  }
  for (int j = 0; slot == NULL && j < BOT_MAX_PENDING; j++) {
    if (!bot_pending[j].active) slot = &bot_pending[j];
  }
  if (slot == NULL) {
    botLog(" no reply: all %d reply slots busy", BOT_MAX_PENDING);
    return;
  }

  slot->active = true;
  slot->awaiting_ack = false;
  slot->timestamp = 0;
  slot->contact_idx = idx;
  botFormatReport(pkt, from, slot->text, sizeof(slot->text));
  if (pkt->isRouteFlood()) {
    // the sender answers our PATH+ACK with a reciprocal path; wait for it so the reply can go direct
    uint32_t wait = BOT_PATH_WAIT_BASE + BOT_PATH_WAIT_HOP * pkt->getPathHashCount();
    if (wait > BOT_PATH_WAIT_MAX) wait = BOT_PATH_WAIT_MAX;
    slot->send_at = futureMillis(wait);
    botLog(" reply queued, up to %lu s for their path", (unsigned long)(wait / 1000));
  } else {
    slot->send_at = futureMillis(BOT_ACK_DELAY + 800);
    botLog(" reply queued");
  }
  from.last_reply_secs = now;
}

void MyMesh::botHandlePath(mesh::Packet* pkt, BotContact& from, uint8_t* data, size_t len) {
  uint8_t path_len = data[0];
  if (!mesh::Packet::isValidPathLen(path_len)) return;
  uint8_t hs = (path_len >> 6) + 1;
  uint8_t hc = path_len & 63;
  if (1 + (size_t)hs * hc > len) return;

  from.out_path_len = mesh::Packet::copyPath(from.out_path, &data[1], path_len);

  // a PATH sent in answer to our flood reply carries the sender's ACK for it
  size_t k = 1 + (size_t)hs * hc;
  bool has_ack = k + 5 <= len && (data[k] & 0x0F) == PAYLOAD_TYPE_ACK;
  char who[20];
  botLog("PATH from %s: route to them %u hops%s, came %u hops", botWho(from, who, sizeof(who)),
         (uint32_t)hc, has_ack ? ", with ack" : "", (uint32_t)pkt->getPathHashCount());
  if (has_ack) {
    uint32_t ack_crc;
    memcpy(&ack_crc, &data[k + 1], 4);
    botCheckAck(ack_crc);
  }

  int idx = &from - bot_contacts;
  for (int j = 0; j < BOT_MAX_PENDING; j++) {
    if (bot_pending[j].active && !bot_pending[j].awaiting_ack && bot_pending[j].contact_idx == idx) {
      bot_pending[j].send_at = futureMillis(300);   // path is known now, reply right away
    }
  }
  // NOTE: no reciprocal path (same as the repeater's own peers)
}

// Sends the reply. A direct send keeps the slot active, awaiting the ACK; a flood send frees it.
void MyMesh::botSendReply(BotPendingReply& r, uint8_t attempt) {
  BotContact& to = bot_contacts[r.contact_idx];
  r.active = false;

  uint8_t temp[5 + BOT_REPLY_MAX_LEN + 1];
  if (r.timestamp == 0) r.timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &r.timestamp, 4);
  temp[4] = (TXT_TYPE_PLAIN << 2) | (attempt & 3);
  size_t text_len = strlen(r.text);
  memcpy(&temp[5], r.text, text_len);

  // the ACK the recipient will send: truncated hash of timestamp + flags + text + OUR (the bot's) pub_key
  mesh::Utils::sha256((uint8_t *)&r.expected_ack, 4, temp, 5 + text_len, bot_id.pub_key, PUB_KEY_SIZE);

  char who[20];
  botWho(to, who, sizeof(who));
  mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG, to.id, to.secret, temp, 5 + text_len);
  if (pkt == NULL) {
    botLog("reply to %s FAILED: no packet buffer (txq %d)", who, _mgr->getOutboundTotal());
    return;
  }
  bot_id.copyHashTo(&pkt->payload[1]);   // src hash is the bot, not the repeater

  if (to.out_path_len == OUT_PATH_UNKNOWN) {
    sendFloodScoped(default_scope, pkt, 0, _prefs.path_hash_mode + 1);
    botLog("reply to %s sent by flood%s (txq %d, free %d)", who, attempt ? " (resend)" : "",
           _mgr->getOutboundTotal(), _mgr->getFreeCount());
  } else {
    sendDirect(pkt, to.out_path, to.out_path_len, 0);
    r.active = r.awaiting_ack = true;
    uint32_t wait = BOT_ACK_WAIT_BASE + BOT_ACK_WAIT_HOP * (to.out_path_len & 63);
    r.send_at = futureMillis(wait);
    botLog("reply to %s sent direct, %u hops, ack wait %lu s (txq %d, free %d)", who,
           (uint32_t)(to.out_path_len & 63), (unsigned long)(wait / 1000), _mgr->getOutboundTotal(), _mgr->getFreeCount());
  }
  bot_replies_sent++;
}

bool MyMesh::botCheckAck(uint32_t ack_crc) {
  for (int j = 0; j < BOT_MAX_PENDING; j++) {
    BotPendingReply& r = bot_pending[j];
    if (r.active && r.awaiting_ack && r.expected_ack == ack_crc) {
      r.active = false;   // delivered
      char who[20];
      botLog("reply to %s acked: delivered", botWho(bot_contacts[r.contact_idx], who, sizeof(who)));
      return true;
    }
  }
  return false;
}

void MyMesh::onAckRecv(mesh::Packet* packet, uint32_t ack_crc) {
  if (botCheckAck(ack_crc) && (packet->isRouteFlood() || packet->getPathHashCount() == 0)) {
    packet->markDoNotRetransmit();   // ACK was for the bot, so don't retransmit
  }
}

void MyMesh::botLoop() {
  for (int j = 0; j < BOT_MAX_PENDING; j++) {
    BotPendingReply& r = bot_pending[j];
    if (!r.active || !millisHasNowPassed(r.send_at)) continue;
    if (!bot_prefs.enabled) { r.active = false; continue; }

    if (r.awaiting_ack) {
      // no ACK for the direct reply: the stored route is stale, so forget it and flood once
      bot_contacts[r.contact_idx].out_path_len = OUT_PATH_UNKNOWN;
      r.awaiting_ack = false;
      botLog("no ack for direct reply: route dropped, resending by flood");
      botSendReply(r, 1);
    } else {
      botSendReply(r, 0);
    }
  }
}

bool MyMesh::handleBotCommand(uint32_t sender_timestamp, const char* command, char* reply) {
  if (memcmp(command, "bot", 3) != 0 || (command[3] != 0 && command[3] != ' ')) return false;
  const char* arg = &command[3];
  while (*arg == ' ') arg++;

  char name[32];
  if (*arg == 0) {
    sprintf(reply, "%s: %s, id %02X, contacts %d (%d saved), rx %lu, tx %lu, %s replies", getBotName(name, sizeof(name)),
            bot_prefs.enabled ? "on" : "off", (uint32_t)bot_id.pub_key[0], num_bot_contacts, bot_cache_count,
            (unsigned long)bot_msgs_recv, (unsigned long)bot_replies_sent,
            bot_prefs.format == BOT_FORMAT_LONG ? "long" : "short");
  } else if (strcmp(arg, "on") == 0 || strcmp(arg, "off") == 0) {
    bot_prefs.enabled = strcmp(arg, "on") == 0;
    if (!bot_prefs.enabled) memset(bot_pending, 0, sizeof(bot_pending));
    strcpy(reply, saveBotPrefs() ? "OK" : "Err - save failed");
  } else if (memcmp(arg, "name", 4) == 0 && (arg[4] == 0 || arg[4] == ' ')) {
    const char* n = &arg[4];
    while (*n == ' ') n++;
    if (*n == 0) {
      sprintf(reply, "> %s", getBotName(name, sizeof(name)));
    } else {
      if (strcmp(n, "-") == 0) {
        bot_prefs.name[0] = 0;   // back to "<repeater name> Bot"
      } else {
        snprintf(bot_prefs.name, sizeof(bot_prefs.name), "%s", n);
      }
      strcpy(reply, saveBotPrefs() ? "OK" : "Err - save failed");
    }
  } else if (strcmp(arg, "advert") == 0) {
    if (!bot_prefs.enabled) {
      strcpy(reply, "Err - bot is off");
    } else {
      sendBotAdvert(true, 500);
      strcpy(reply, "OK - bot advert sent");
    }
  } else if (strcmp(arg, "key") == 0) {
    mesh::Utils::toHex(reply, bot_id.pub_key, PUB_KEY_SIZE);
  } else if (memcmp(arg, "learn ", 6) == 0) {
    // bot learn <64 hex pub key> [name]: for senders whose adverts don't reach the bot
    const char* a = &arg[6];
    while (*a == ' ') a++;
    char hex[2 * PUB_KEY_SIZE + 1];
    int h = 0;
    while (h < 2 * PUB_KEY_SIZE && isxdigit((unsigned char)a[h])) { hex[h] = a[h]; h++; }
    hex[h] = 0;
    const char* nm = &a[h];
    while (*nm == ' ') nm++;
    uint8_t key[PUB_KEY_SIZE];
    if (h != 2 * PUB_KEY_SIZE || (a[h] != 0 && a[h] != ' ') || !mesh::Utils::fromHex(key, PUB_KEY_SIZE, hex)) {
      strcpy(reply, "Err - usage: bot learn <64 hex chars public key> [name]");
    } else {
      int idx = botLearnContact(mesh::Identity(key), *nm ? nm : NULL);
      if (idx < 0) {
        strcpy(reply, "Err - can't add that key");
      } else {
        BotContact& c = bot_contacts[idx];
        botCacheContact(c);
        char who[20];
        sprintf(reply, "OK - %s learned and saved", botWho(c, who, sizeof(who)));
      }
    }
  } else if (memcmp(arg, "format", 6) == 0 && (arg[6] == 0 || arg[6] == ' ')) {
    const char* f = &arg[6];
    while (*f == ' ') f++;
    if (*f == 0) {
      sprintf(reply, "> %s", bot_prefs.format == BOT_FORMAT_LONG ? "long" : "short");
    } else if (strcmp(f, "short") == 0 || strcmp(f, "long") == 0) {
      bot_prefs.format = strcmp(f, "long") == 0 ? BOT_FORMAT_LONG : BOT_FORMAT_SHORT;
      strcpy(reply, saveBotPrefs() ? "OK" : "Err - save failed");
    } else {
      strcpy(reply, "Err - usage: bot format [short|long]");
    }
  } else if (strcmp(arg, "cache") == 0) {
    int n = sprintf(reply, "%d/%d saved:", bot_cache_count, BOT_CACHE_SIZE);
    for (int i = 0; i < bot_cache_count && n < 140; i++) {
      n += snprintf(&reply[n], 160 - n, " %.10s/%02x", bot_cache[i].name[0] ? bot_cache[i].name : "?",
                    (uint32_t)bot_cache[i].pub_key[0]);
    }
  } else if (strcmp(arg, "cache clear") == 0) {
    bot_cache_count = bot_cache_next = 0;
    for (int i = 0; i < num_bot_contacts; i++) bot_contacts[i].cached = false;
    strcpy(reply, saveBotCache() ? "OK" : "Err - save failed");
  } else if (memcmp(arg, "log", 3) == 0 && (arg[3] == 0 || arg[3] == ' ')) {
    const char* a = &arg[3];
    while (*a == ' ') a++;
    botLogCommand(sender_timestamp, a, reply);
  } else {
    strcpy(reply, "Err - usage: bot [on|off|name <n>|name -|advert|key|format [short|long]|learn <key> [name]|cache [clear]|log [n|clear]]");
  }
  return true;
}

#endif
