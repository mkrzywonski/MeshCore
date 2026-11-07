#pragma once

// Optional "virtual companion" bot hosted inside the repeater firmware (build with -D REPEATER_BOT=1).
//
// The bot has its own identity and advertises as a separate chat node (default name: "<repeater name> Bot").
// It answers each direct message with how the repeater heard it: route type, hop count, the relay path
// and the SNR/RSSI of the last hop. It never touches channel traffic, and forwarding is unaffected.

#ifdef REPEATER_BOT

#include <Mesh.h>
#include <MeshCore.h>

#ifndef BOT_MAX_CONTACTS
  #define BOT_MAX_CONTACTS          40
#endif
#define BOT_MAX_PENDING             4
#define BOT_NAME_LEN               24     // advert name space is limited when a location is included
#define BOT_REPLY_MAX_LEN         140
#define BOT_PREFS_FILE   "/bot_prefs"
#define BOT_IDENTITY_NAME      "_bot"

#define BOT_MIN_REPLY_INTERVAL_SECS 3     // per contact
#define BOT_MAX_REPLIES_PER_MIN    12     // all contacts

struct BotContact {
  mesh::Identity id;
  uint8_t secret[PUB_KEY_SIZE];
  uint8_t out_path[MAX_PATH_SIZE];
  uint8_t out_path_len;          // OUT_PATH_UNKNOWN until the sender tells us a path
  bool has_secret;
  uint32_t last_heard;           // RTC secs, for eviction
  uint32_t last_msg_timestamp;   // sender timestamp of last message answered (dedupe retries)
  uint32_t last_reply_secs;
  char name[32];
};

struct BotPendingReply {
  bool active;
  int contact_idx;
  unsigned long send_at;         // wait for the sender's reciprocal path until here, then send anyway
  char text[BOT_REPLY_MAX_LEN + 1];
};

struct BotPrefs {
  uint8_t version;
  uint8_t enabled;
  char name[BOT_NAME_LEN];       // empty = "<repeater name> Bot"
};

#endif
