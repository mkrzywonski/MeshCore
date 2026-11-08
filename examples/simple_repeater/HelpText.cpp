#include "HelpText.h"
#include <stdio.h>
#include <string.h>

// One row per command. The topic decides where it's listed; name is what "help <command>" matches
// (without "get"/"set"); syntax and desc make up the usage line.
enum {
  T_INFO, T_NODE, T_TIME, T_RADIO, T_ROUTING, T_ADVERTS, T_ACCESS, T_SYSTEM, T_REGIONS,
  T_GPS, T_SENSORS, T_BRIDGE, T_BOT, T_COUNT
};

static const char* const topic_names[T_COUNT] = {
  "info", "node", "time", "radio", "routing", "adverts", "access", "system", "regions",
  "gps", "sensors", "bridge", "bot"
};

#define H_USB     1   // only works over USB serial
#define H_HIDDEN  2   // alias: matched by "help <name>" but not listed

struct HelpEntry {
  unsigned char topic;
  unsigned char flags;
  const char* name;
  const char* syntax;
  const char* desc;
};

static const HelpEntry entries[] = {
  // info
  { T_INFO, 0, "help", "help [topic|command]", "list topics, a topic's commands, or a command's usage" },
  { T_INFO, H_HIDDEN, "?", "?", "same as help" },
  { T_INFO, 0, "ver", "ver", "firmware version and build date" },
  { T_INFO, 0, "board", "board", "hardware name" },
  { T_INFO, 0, "power", "power", "battery mV, and whether USB, solar or a charger is supplying power" },
  { T_INFO, 0, "neighbors", "neighbors", "repeaters heard directly, newest first, as id:seconds ago:SNR x4 (47 = 11.75 dB)" },
  { T_INFO, 0, "role", "get role", "firmware role (repeater)" },
  { T_INFO, 0, "bootloader.ver", "get bootloader.ver", "bootloader version (nRF52 boards)" },
  { T_INFO, 0, "pwrmgt", "get pwrmgt.support|source|bootreason|bootmv", "power management: supported?, power source, last reset/shutdown reason, boot voltage" },
  { T_INFO, H_USB, "stats-core", "stats-core", "battery, uptime, queue, errors (JSON). In the app: Status screen" },
  { T_INFO, H_USB, "stats-radio", "stats-radio", "noise floor, last RSSI/SNR, airtime (JSON). In the app: Status screen" },
  { T_INFO, H_USB, "stats-packets", "stats-packets", "packets received/sent, flood/direct (JSON). In the app: Status screen" },
  { T_INFO, 0, "clear stats", "clear stats", "reset the packet and airtime counters" },

  // node: identity and what others see
  { T_NODE, 0, "name", "get|set name <name>", "node name shown in apps and on maps" },
  { T_NODE, 0, "lat", "get|set lat <degrees>", "latitude shown on maps, decimal (30.0711)" },
  { T_NODE, 0, "lon", "get|set lon <degrees>", "longitude shown on maps, decimal (-97.8206)" },
  { T_NODE, 0, "owner.info", "get|set owner.info <text>", "contact info others can read; | starts a new line" },
  { T_NODE, 0, "public.key", "get public.key", "this node's public key (64 hex)" },
  { T_NODE, 0, "prv.key", "get|set prv.key <128 hex>", "identity private key. get: USB serial only. set: reboot to apply" },

  // time
  { T_TIME, 0, "clock", "clock", "show the time (UTC)" },
  { T_TIME, 0, "clock sync", "clock sync", "set the clock to the sender's time (your phone, in the app). Forward only" },
  { T_TIME, 0, "time", "time <epoch secs>", "set the clock to a Unix time. Forward only" },
  { T_TIME, 0, "clkreboot", "clkreboot", "reset the clock to 2024 and reboot (undoes a clock set in the future)" },

  // radio
  { T_RADIO, 0, "radio", "get|set radio <freq>,<bw>,<sf>,<cr>", "LoRa settings, e.g. 910.525,62.5,7,5. Reboot to apply" },
  { T_RADIO, 0, "freq", "get|set freq <MHz>", "frequency. set: USB serial only, reboot to apply" },
  { T_RADIO, 0, "tx", "get|set tx <dBm>", "transmit power (SX1262: up to 22)" },
  { T_RADIO, 0, "tempradio", "tempradio <freq>,<bw>,<sf>,<cr>,<mins>", "use other LoRa settings for <mins>, then go back" },
  { T_RADIO, 0, "af", "get|set af <0-9>", "airtime factor: stay quiet af x airtime after sending; duty cycle 1/(1+af)" },
  { T_RADIO, 0, "dutycycle", "get|set dutycycle <1-100>", "max % of time transmitting (sets af)" },
  { T_RADIO, 0, "cad", "get|set cad on|off", "channel activity detection: listen before transmitting" },
  { T_RADIO, 0, "int.thresh", "get|set int.thresh <n>", "local interference threshold, 0 = off" },
  { T_RADIO, 0, "radio.rxgain", "get|set radio.rxgain on|off", "boosted receive gain" },
  { T_RADIO, 0, "agc.reset.interval", "get|set agc.reset.interval <secs>", "reset receiver gain every n secs (multiple of 4, 0 = off). Austin Mesh: 4" },
#if defined(USE_LR2021)
  { T_RADIO, 0, "extra.sf", "get|set extra.sf <sf>[,<sf>,<sf>]", "also listen on up to 3 more spreading factors" },
#endif

  // routing: how this repeater forwards
  { T_ROUTING, 0, "repeat", "get|set repeat on|off", "forward other nodes' packets. off: still answers logins and the bot" },
  { T_ROUTING, 0, "txdelay", "get|set txdelay <0-2>", "flood retransmit delay factor; higher with more neighbors (Austin Mesh table)" },
  { T_ROUTING, 0, "direct.txdelay", "get|set direct.txdelay <0-2>", "retransmit delay factor for direct (routed) packets" },
  { T_ROUTING, 0, "rxdelay", "get|set rxdelay <0-20>", "wait longer on weaker copies of a flood so better paths win. Austin Mesh: 3" },
  { T_ROUTING, 0, "multi.acks", "get|set multi.acks 0|1", "send an extra ack for direct messages. Austin Mesh: 1" },
  { T_ROUTING, 0, "path.hash.mode", "get|set path.hash.mode 0|1|2", "size of this node's id in paths: 1, 2 or 3 bytes" },
  { T_ROUTING, 0, "loop.detect", "get|set loop.detect off|minimal|moderate|strict", "drop floods that already passed through here" },
  { T_ROUTING, 0, "flood.max", "get|set flood.max <0-64>", "max hops for floods this node forwards" },
  { T_ROUTING, 0, "flood.max.advert", "get|set flood.max.advert <0-64>", "max hops for adverts this node forwards" },
  { T_ROUTING, 0, "flood.max.unscoped", "get|set flood.max.unscoped <0-64>", "max hops for floods with no region" },

  // adverts and neighbors
  { T_ADVERTS, 0, "advert", "advert", "send a flood advert now (reaches the whole mesh)" },
  { T_ADVERTS, 0, "advert.zerohop", "advert.zerohop", "send an advert to direct neighbors only" },
  { T_ADVERTS, 0, "advert.interval", "get|set advert.interval <60-240>", "minutes between zero-hop adverts, 0 = off" },
  { T_ADVERTS, 0, "flood.advert.interval", "get|set flood.advert.interval <3-168>", "hours between flood adverts, 0 = off" },
  { T_ADVERTS, 0, "discover.neighbors", "discover.neighbors", "ask nearby repeaters to answer (refreshes neighbors)" },
  { T_ADVERTS, 0, "neighbor.remove", "neighbor.remove <key prefix>", "forget neighbors whose key starts with the prefix (a space: all)" },

  // access
  { T_ACCESS, 0, "password", "password <new>", "set the admin password (the reply shows it)" },
  { T_ACCESS, 0, "guest.password", "get|set guest.password <pw>", "password for guest logins, empty = none" },
  { T_ACCESS, 0, "setperm", "setperm <pubkey> <0-3>", "client access: 0 guest (removes), 1 read-only, 2 read-write, 3 admin" },
  { T_ACCESS, H_USB, "acl", "get acl", "list clients and their permissions" },
  { T_ACCESS, 0, "allow.read.only", "get|set allow.read.only on|off", "room servers only; no effect on a repeater" },

  // system
  { T_SYSTEM, 0, "reboot", "reboot", "restart the node" },
  { T_SYSTEM, 0, "poweroff", "poweroff", "turn the node off (also: shutdown). Needs a button or power cycle to start" },
  { T_SYSTEM, H_HIDDEN, "shutdown", "shutdown", "same as poweroff" },
  { T_SYSTEM, 0, "start ota", "start ota", "turn on Bluetooth for a firmware update; the reply has its MAC. reboot cancels" },
  { T_SYSTEM, H_USB, "erase", "erase", "format the file system: deletes settings, keys and contacts" },
  { T_SYSTEM, 0, "powersaving", "powersaving [on|off]", "sleep between radio events" },
  { T_SYSTEM, 0, "adc.multiplier", "get|set adc.multiplier <x>", "calibrate the battery reading, 0 = board default" },
  { T_SYSTEM, 0, "log start", "log start|stop|erase", "record received packets to flash, stop, delete the record" },
  { T_SYSTEM, 0, "log stop", "log start|stop|erase", "record received packets to flash, stop, delete the record" },
  { T_SYSTEM, 0, "log erase", "log start|stop|erase", "record received packets to flash, stop, delete the record" },
  { T_SYSTEM, H_USB, "log", "log", "print the packet record saved in flash" },

  // regions (flood scopes)
  { T_REGIONS, 0, "region", "region", "list regions; F = floods allowed" },
  { T_REGIONS, 0, "region put", "region put <name> [parent]", "add a region (floods allowed)" },
  { T_REGIONS, 0, "region remove", "region remove <name>", "delete a region (must have no children)" },
  { T_REGIONS, 0, "region allowf", "region allowf <name>", "allow floods scoped to the region" },
  { T_REGIONS, 0, "region denyf", "region denyf <name>", "block floods scoped to the region" },
  { T_REGIONS, 0, "region get", "region get <name>", "show a region, its parent and F flag" },
  { T_REGIONS, 0, "region list", "region list allowed|denied", "names of regions with floods allowed or blocked" },
  { T_REGIONS, 0, "region home", "region home [name]", "show or set this node's home region" },
  { T_REGIONS, 0, "region default", "region default [name|<null>]", "show or set the scope for floods without one; saved" },
  { T_REGIONS, 0, "region def", "region def <name>[|jump] ...", "define a hierarchy on one line (see docs)" },
  { T_REGIONS, 0, "region save", "region save", "save region changes to flash (otherwise lost at reboot)" },
  { T_REGIONS, H_USB, "region load", "region load", "bulk-load an indented region list; a blank line ends it" },

#if ENV_INCLUDE_GPS == 1
  // gps
  { T_GPS, 0, "gps", "gps [on|off]", "GPS state (on/off, fix, satellites), or turn it on or off" },
  { T_GPS, 0, "gps sync", "gps sync", "set the clock from GPS" },
  { T_GPS, 0, "gps setloc", "gps setloc", "set lat/lon from the GPS position" },
  { T_GPS, 0, "gps advert", "gps advert [none|share|prefs]", "location in adverts: none, live GPS, or lat/lon settings" },
  { T_GPS, 0, "gps diag", "gps diag", "GPS diagnostics" },
#endif

  // sensors
  { T_SENSORS, 0, "sensor list", "sensor list [start]", "sensor settings and values" },
  { T_SENSORS, 0, "sensor get", "sensor get <key>", "one sensor setting" },
  { T_SENSORS, 0, "sensor set", "sensor set <key> <value>", "change a sensor setting" },

#ifdef WITH_BRIDGE
  // bridge
  { T_BRIDGE, 0, "bridge.type", "get bridge.type", "compiled bridge type (rs232, espnow)" },
  { T_BRIDGE, 0, "bridge.enabled", "get|set bridge.enabled on|off", "turn the bridge on or off" },
  { T_BRIDGE, 0, "bridge.delay", "get|set bridge.delay <0-10000>", "ms to delay packets sent through the bridge" },
  { T_BRIDGE, 0, "bridge.source", "get|set bridge.source rx|tx", "bridge received packets, or packets this node sends" },
#ifdef WITH_RS232_BRIDGE
  { T_BRIDGE, 0, "bridge.baud", "get|set bridge.baud <rate>", "RS-232 speed, 9600-115200" },
#endif
#ifdef WITH_ESPNOW_BRIDGE
  { T_BRIDGE, 0, "bridge.channel", "get|set bridge.channel <1-14>", "ESP-NOW channel" },
  { T_BRIDGE, 0, "bridge.secret", "get|set bridge.secret <secret>", "ESP-NOW secret, up to 15 characters" },
#endif
#endif
};

#define NUM_ENTRIES ((int)(sizeof(entries) / sizeof(entries[0])))

// Append to a bounded buffer; returns false (and leaves the buffer as it was) if it doesn't fit.
static bool append(char* buf, int max, const char* s) {
  int n = strlen(buf), m = strlen(s);
  if (n + m > max) return false;
  memcpy(&buf[n], s, m + 1);
  return true;
}

static void usage(const HelpEntry& e, bool serial, char* out, int max) {
  snprintf(out, max + 1, "%s - %s%s", e.syntax, e.desc, (e.flags & H_USB) && !serial ? " [USB serial only]" : "");
}

static bool topicHasEntries(int t, bool serial) {
  for (int i = 0; i < NUM_ENTRIES; i++) {
    if (entries[i].topic == t && !(entries[i].flags & H_HIDDEN) && (serial || !(entries[i].flags & H_USB))) return true;
  }
  return false;
}

static void listTopics(char* out, int max) {
  out[0] = 0;
  append(out, max, "Topics:");
  for (int t = 0; t < T_COUNT; t++) {
    if (!topicHasEntries(t, false)) continue;
    append(out, max, " ");
    append(out, max, topic_names[t]);
  }
  append(out, max, ". Send help <topic>, or help <command> for usage");
}

static int findTopic(const char* s) {
  for (int t = 0; t < T_COUNT; t++) {
    if (strcmp(s, topic_names[t]) == 0 && topicHasEntries(t, true)) return t;
  }
  return -1;
}

static const HelpEntry* findEntry(const char* s) {
  for (int i = 0; i < NUM_ENTRIES; i++) {
    if (strcmp(s, entries[i].name) == 0) return &entries[i];
  }
  return NULL;
}

// "get txdelay", "bot learn 1234...", "get pwrmgt.bootmv": try the text as given, then without get/set,
// then dropping trailing words, then dropping trailing ".parts".
static const HelpEntry* lookup(const char* arg) {
  char key[48];
  snprintf(key, sizeof(key), "%s", arg);
  if (strncmp(key, "get ", 4) == 0 || strncmp(key, "set ", 4) == 0) memmove(key, &key[4], strlen(&key[4]) + 1);
  for (;;) {
    const HelpEntry* e = findEntry(key);
    if (e) return e;
    char* sp = strrchr(key, ' ');
    if (sp) { *sp = 0; continue; }
    char* dot = strrchr(key, '.');
    if (dot) { *dot = 0; continue; }
    return NULL;
  }
}

static void serialFullList(void (*emit)(const char*)) {
  char line[100];
  for (int t = 0; t < T_COUNT; t++) {
    if (!topicHasEntries(t, true)) continue;
    snprintf(line, sizeof(line), "%-9s", topic_names[t]);
    bool first = true;
    for (int i = 0; i < NUM_ENTRIES; i++) {
      const HelpEntry& e = entries[i];
      if (e.topic != t || (e.flags & H_HIDDEN)) continue;
      if (i > 0 && strcmp(e.syntax, entries[i - 1].syntax) == 0) continue;
      char item[64];
      snprintf(item, sizeof(item), "%s%s%s", first ? "" : ", ", e.syntax, (e.flags & H_USB) ? "*" : "");
      if (strlen(line) + strlen(item) > 79) {   // wrap
        emit(line);
        snprintf(line, sizeof(line), "%-9s%s", "", first ? item : item + 2);
      } else {
        strcat(line, item);
      }
      first = false;
    }
    emit(line);
  }
  emit("* USB serial only. help <topic> or help <command> for details.");
}

void helpCommand(const char* arg, bool serial, void (*emit)(const char* line), char* reply) {
  char a[48];
  while (*arg == ' ') arg++;
  snprintf(a, sizeof(a), "%s", arg);
  for (int n = strlen(a); n > 0 && a[n - 1] == ' '; n--) a[n - 1] = 0;
  reply[0] = 0;

  if (a[0] == 0) {
    if (serial && emit) serialFullList(emit);
    else listTopics(reply, HELP_REPLY_MAX);
    return;
  }

  int t = findTopic(a);
  if (t >= 0) {
    if (serial && emit) {   // every command's usage, one per line
      char line[200];
      for (int i = 0; i < NUM_ENTRIES; i++) {
        const HelpEntry& e = entries[i];
        if (e.topic != t || (e.flags & H_HIDDEN)) continue;
        if (i > 0 && strcmp(e.syntax, entries[i - 1].syntax) == 0) continue;
        snprintf(line, sizeof(line), "  %s - %s%s", e.syntax, e.desc, (e.flags & H_USB) ? " [USB serial only]" : "");
        emit(line);
      }
      return;
    }
    // Command names, with two-word commands grouped: "region put|remove|get", "bot on|off|name"
    char list[400];
    char group[16] = "";   // first word of the group being built
    snprintf(list, sizeof(list), "%s:", topic_names[t]);
    bool first = true;
    for (int i = 0; i < NUM_ENTRIES; i++) {
      const HelpEntry& e = entries[i];
      if (e.topic != t || (e.flags & (H_HIDDEN | H_USB))) continue;
      const char* sp = strchr(e.name, ' ');
      if (sp && (int)(sp - e.name) < (int)sizeof(group) && strncmp(e.name, group, sp - e.name) == 0
          && group[sp - e.name] == 0) {
        append(list, sizeof(list) - 1, "|");   // same first word as the previous item
        append(list, sizeof(list) - 1, sp + 1);
        continue;
      }
      // a command named like its topic ("time", "gps") can't be looked up by name, so show its syntax
      const char* item = strcmp(e.name, topic_names[t]) == 0 ? e.syntax : e.name;
      append(list, sizeof(list) - 1, first ? " " : ", ");
      append(list, sizeof(list) - 1, item);
      group[0] = 0;
      if (sp && (int)(sp - e.name) < (int)sizeof(group)) {
        memcpy(group, e.name, sp - e.name);
        group[sp - e.name] = 0;
      }
      first = false;
    }
    if (strlen(list) > HELP_REPLY_MAX) strcpy(&list[HELP_REPLY_MAX - 3], "...");
    strcpy(reply, list);
    return;
  }

  const HelpEntry* e = lookup(a);
  if (e) {
    usage(*e, serial, reply, HELP_REPLY_MAX);
  } else {
    snprintf(reply, HELP_REPLY_MAX + 1, "No help for '%.40s'. Send help for the topics", a);
  }
}

#ifdef HELP_TEXT_TEST
// Host check: g++ -DHELP_TEXT_TEST [-DREPEATER_BOT] HelpText.cpp && ./a.out
static int problems = 0;
static void show(const char* line) { printf("%s\r\n", line); }
static void check(const char* arg) {
  char reply[200];
  helpCommand(arg, false, NULL, reply);
  int n = strlen(reply);
  printf("[%3d] help %s\n      %s\n", n, arg, reply);
  if (n > HELP_REPLY_MAX - 3) { printf("      ^^^ TOO LONG\n"); problems++; }   // 3 = app's "XX|" prefix
  if (n >= 3 && strcmp(&reply[n - 3], "...") == 0) { printf("      ^^^ TRUNCATED\n"); problems++; }
}
int main() {
  for (int i = 0; i < NUM_ENTRIES; i++) {
    for (int j = i + 1; j < NUM_ENTRIES; j++)
      if (strcmp(entries[i].name, entries[j].name) == 0) { printf("DUPLICATE %s\n", entries[i].name); problems++; }
    if (findTopic(entries[i].name) >= 0) printf("note: '%s' is also a topic name; help %s shows the topic\n", entries[i].name, entries[i].name);
  }
  check("");
  for (int t = 0; t < T_COUNT; t++) if (topicHasEntries(t, false)) check(topic_names[t]);
  for (int i = 0; i < NUM_ENTRIES; i++) check(entries[i].name);
  check("get txdelay"); check("set radio 910.525,62.5,7,5"); check("get pwrmgt.bootmv"); check("bot learn 0011 x");
  check("nonsense");
  printf("\n--- serial: help\n"); helpCommand("", true, show, (char[200]){0});
  printf("\n--- serial: help time\n"); { char r[200]; helpCommand("time", true, show, r); }
  printf("\n%d problems\n", problems);
  return problems != 0;
}
#endif
