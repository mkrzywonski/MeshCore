#pragma once

// Repeater CLI help.
//
//   help                 serial: every command, grouped by topic; remote: the list of topics
//   help <topic>         serial: usage of each command in the topic; remote: the command names
//   help <command>       usage of one command ("help get txdelay", "help txdelay", "help bot learn")
//
// Remote (app) replies must fit one packet, so they stay under HELP_REPLY_MAX characters.
// Commands that only work over USB serial are left out of remote topic lists and marked in their usage.

#define HELP_REPLY_MAX  150

// arg: the text after "help" (may be empty or start with spaces).
// serial: true when the command came from the USB console.
// emit: prints one line on the console (serial only; may be NULL for remote).
// reply: the CLI reply buffer (at least HELP_REPLY_MAX + 1 bytes); left empty when everything went to emit.
void helpCommand(const char* arg, bool serial, void (*emit)(const char* line), char* reply);
