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

#define BOT_LOG_SIZE               32     // event log ring buffer ("bot log"), RAM only
#define BOT_LOG_MSG_LEN            80

// Contacts that have DM'd the bot successfully (or were added with "bot learn") are saved to
// flash, so they survive reboots and don't depend on adverts, which often don't travel far.
// FIFO: a new entry replaces the oldest; a contact already saved costs no write.
#define BOT_CACHE_FILE  "/bot_cache"
#define BOT_CACHE_SIZE             16
#define BOT_CACHE_NAME_LEN         24

struct BotContact {
  mesh::Identity id;
  uint8_t secret[PUB_KEY_SIZE];
  uint8_t out_path[MAX_PATH_SIZE];
  uint8_t out_path_len;          // OUT_PATH_UNKNOWN until the sender tells us a path
  bool has_secret;
  bool cached;                   // saved in the flash contact cache
  uint32_t last_heard;           // RTC secs, for eviction
  uint32_t last_msg_timestamp;   // sender timestamp of last message answered (dedupe retries)
  uint32_t last_reply_secs;
  char name[32];
};

struct BotPendingReply {
  bool active;
  bool awaiting_ack;             // sent direct; resend by flood if no ACK by 'send_at'
  int contact_idx;
  unsigned long send_at;         // queued: send at this time; awaiting_ack: ACK deadline
  uint32_t timestamp;            // reused by the flood resend, so the sender can de-dupe
  uint32_t expected_ack;
  char text[BOT_REPLY_MAX_LEN + 1];
};

struct BotCacheEntry {
  uint8_t pub_key[PUB_KEY_SIZE];
  char name[BOT_CACHE_NAME_LEN];
};

struct BotLogEntry {
  uint32_t time;                 // RTC secs
  char msg[BOT_LOG_MSG_LEN];
};

struct BotPrefs {
  uint8_t version;
  uint8_t enabled;
  char name[BOT_NAME_LEN];       // empty = "<repeater name> Bot"
};

#endif
