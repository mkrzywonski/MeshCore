#include "MyMesh.h"

#ifdef REPEATER_BOT

#define BOT_PREFS_VERSION      1
#define BOT_ACK_DELAY        200
#define BOT_PATH_WAIT_BASE  2000    // ms to wait for the sender's reciprocal path, plus...
#define BOT_PATH_WAIT_HOP   2500    // ...this much per hop
#define BOT_PATH_WAIT_MAX  30000

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
  if (file.read((uint8_t *)&tmp, sizeof(tmp)) == sizeof(tmp) && tmp.version == BOT_PREFS_VERSION) {
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

void MyMesh::botLearnContact(const mesh::Identity& id, const char* name) {
  if (id.matches(bot_id) || id.matches(self_id)) return;

  uint32_t now = getRTCClock()->getCurrentTime();
  int idx = -1;
  for (int i = 0; i < num_bot_contacts; i++) {
    if (bot_contacts[i].id.matches(id)) { idx = i; break; }
  }
  if (idx < 0) {
    if (num_bot_contacts < BOT_MAX_CONTACTS) {
      idx = num_bot_contacts++;
    } else {   // evict the least recently heard contact that has no reply in flight
      uint32_t oldest = 0xFFFFFFFF;
      for (int i = 0; i < num_bot_contacts; i++) {
        bool busy = false;
        for (int j = 0; j < BOT_MAX_PENDING; j++) {
          if (bot_pending[j].active && bot_pending[j].contact_idx == i) busy = true;
        }
        if (!busy && bot_contacts[i].last_heard < oldest) { oldest = bot_contacts[i].last_heard; idx = i; }
      }
      if (idx < 0) return;
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

  for (int i = 0; i < num_bot_contacts; i++) {
    BotContact& c = bot_contacts[i];
    if (!c.id.isHashMatch(&src_hash)) continue;

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
  // unknown sender (we have not heard their advert): nothing we can decrypt, carry on as normal
  return false;
}

void MyMesh::botFormatReport(mesh::Packet* pkt, char* text, size_t len) {
  float snr = pkt->getSNR();
  int rssi = (int)_radio->getLastRSSI();
  int n = 0;

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
  if (pkt->isRouteFlood()) {
    // tell the sender the path TO here (so they can go direct), with the ACK piggy-backed
    mesh::Packet* path = createPathReturn(to.id, to.secret, pkt->path, pkt->path_len,
                                          PAYLOAD_TYPE_ACK, ack_hash, ack_len);
    if (path) {
      bot_id.copyHashTo(&path->payload[1]);   // src hash is the bot, not the repeater
      sendFloodReply(path, BOT_ACK_DELAY, pkt->getPathHashSize());
    }
  } else if (to.out_path_len == OUT_PATH_UNKNOWN) {
    mesh::Packet* ack = createAck(ack_hash, ack_len);
    if (ack) sendFloodScoped(default_scope, ack, BOT_ACK_DELAY, _prefs.path_hash_mode + 1);
  } else {
    uint32_t d = BOT_ACK_DELAY;
    if (getExtraAckTransmitCount() > 0) {
      mesh::Packet* a1 = createMultiAck(ack_hash, ack_len, 1);
      if (a1) sendDirect(a1, to.out_path, to.out_path_len, d);
      d += 300;
    }
    mesh::Packet* a2 = createAck(ack_hash, ack_len);
    if (a2) sendDirect(a2, to.out_path, to.out_path_len, d);
  }
}

void MyMesh::botHandleText(mesh::Packet* pkt, BotContact& from, uint8_t* data, size_t len) {
  if (len <= 5) return;
  uint32_t sender_timestamp;
  memcpy(&sender_timestamp, data, 4);
  uint8_t flags = data[4] >> 2;
  if (flags != TXT_TYPE_PLAIN) return;   // CLI and signed messages are not for us

  data[len] = 0;
  size_t text_len = strlen((const char *)&data[5]);

  // ACK exactly as a companion does: truncated hash of timestamp + flags + text + sender pub_key
  uint8_t ack_hash[6];
  mesh::Utils::sha256(ack_hash, 4, data, 5 + text_len, from.id.pub_key, PUB_KEY_SIZE);
  ack_hash[4] = (5 + text_len + 1 < len) ? data[5 + text_len + 1] : 0;   // extended attempt byte, if any
  getRNG()->random(&ack_hash[5], 1);
  botSendAck(pkt, from, ack_hash, sizeof(ack_hash));

  bot_msgs_recv++;
  if (sender_timestamp == from.last_msg_timestamp) return;   // a retry of a message we already answered
  from.last_msg_timestamp = sender_timestamp;

  uint32_t now = getRTCClock()->getCurrentTime();
  if (from.last_reply_secs && now >= from.last_reply_secs && now - from.last_reply_secs < BOT_MIN_REPLY_INTERVAL_SECS) return;
  if (!bot_limiter.allow(now)) return;

  int idx = &from - bot_contacts;
  BotPendingReply* slot = NULL;
  for (int j = 0; j < BOT_MAX_PENDING; j++) {
    if (bot_pending[j].active && bot_pending[j].contact_idx == idx) { slot = &bot_pending[j]; break; }   // replace
  }
  for (int j = 0; slot == NULL && j < BOT_MAX_PENDING; j++) {
    if (!bot_pending[j].active) slot = &bot_pending[j];
  }
  if (slot == NULL) return;   // busy

  slot->active = true;
  slot->contact_idx = idx;
  botFormatReport(pkt, slot->text, sizeof(slot->text));
  if (pkt->isRouteFlood()) {
    // the sender answers our PATH+ACK with a reciprocal path; wait for it so the reply can go direct
    uint32_t wait = BOT_PATH_WAIT_BASE + BOT_PATH_WAIT_HOP * pkt->getPathHashCount();
    slot->send_at = futureMillis(wait > BOT_PATH_WAIT_MAX ? BOT_PATH_WAIT_MAX : wait);
  } else {
    slot->send_at = futureMillis(BOT_ACK_DELAY + 800);
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

  int idx = &from - bot_contacts;
  for (int j = 0; j < BOT_MAX_PENDING; j++) {
    if (bot_pending[j].active && bot_pending[j].contact_idx == idx) {
      bot_pending[j].send_at = futureMillis(300);   // path is known now, reply right away
    }
  }
  // NOTE: no reciprocal path (same as the repeater's own peers)
}

void MyMesh::botSendReply(BotPendingReply& r) {
  BotContact& to = bot_contacts[r.contact_idx];

  uint8_t temp[5 + BOT_REPLY_MAX_LEN + 1];
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &timestamp, 4);
  temp[4] = (TXT_TYPE_PLAIN << 2);   // attempt 0
  size_t text_len = strlen(r.text);
  memcpy(&temp[5], r.text, text_len);

  mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG, to.id, to.secret, temp, 5 + text_len);
  if (pkt == NULL) return;
  bot_id.copyHashTo(&pkt->payload[1]);   // src hash is the bot, not the repeater

  if (to.out_path_len == OUT_PATH_UNKNOWN) {
    sendFloodScoped(default_scope, pkt, 0, _prefs.path_hash_mode + 1);
  } else {
    sendDirect(pkt, to.out_path, to.out_path_len, 0);
  }
  bot_replies_sent++;
}

void MyMesh::botLoop() {
  for (int j = 0; j < BOT_MAX_PENDING; j++) {
    BotPendingReply& r = bot_pending[j];
    if (r.active && millisHasNowPassed(r.send_at)) {
      r.active = false;
      if (bot_prefs.enabled) botSendReply(r);
    }
  }
}

bool MyMesh::handleBotCommand(const char* command, char* reply) {
  if (memcmp(command, "bot", 3) != 0 || (command[3] != 0 && command[3] != ' ')) return false;
  const char* arg = &command[3];
  while (*arg == ' ') arg++;

  char name[32];
  if (*arg == 0) {
    sprintf(reply, "%s: %s, id %02X, contacts %d, rx %lu, tx %lu", getBotName(name, sizeof(name)),
            bot_prefs.enabled ? "on" : "off", (uint32_t)bot_id.pub_key[0], num_bot_contacts,
            (unsigned long)bot_msgs_recv, (unsigned long)bot_replies_sent);
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
  } else {
    strcpy(reply, "Err - usage: bot [on|off|name <name>|name -|advert|key]");
  }
  return true;
}

#endif
