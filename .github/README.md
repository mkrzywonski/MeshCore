# MeshCore: repeater additions (fork)

This is a fork of [MeshCore](https://github.com/meshcore-dev/MeshCore) that adds a few features to the
**repeater** firmware. Everything else is upstream MeshCore, unchanged. For MeshCore itself, see the
[upstream README](../README.md) and the [MeshCore docs](https://meshcore-dev.github.io/meshcore/).

Based on upstream **v1.18.0**. All changes are in `examples/simple_repeater/` and `docs/`; the shared library in
`src/` is untouched.

## What's added

### Repeater bot: "can the mesh hear me?"

An optional test bot that lives inside the repeater. DM it from any companion and it replies with the path your
message took and how strong the last hop was:

```
61,36,b4 -3.5dB -110dBm
```

That's three relays (your end first) and the signal at the repeater. It's a quick check of two-way connectivity from
wherever you are, without anyone at the other end. It has its own identity and name, doesn't touch channel traffic,
and is conservative on airtime: requests over its rate limits are dropped, never queued.

Built in only with `-D REPEATER_BOT=1`. → [Repeater Bot](../docs/repeater_bot.md)

### Trusted clock source

`set clock.source <public key>` makes a repeater follow a node you trust: authenticated timestamps from it (signed
adverts, logins, admin messages, bot DMs) set the clock forward, within sane limits. A repeater with no clock chip
then gets the right time back on its own after a reboot, instead of restarting in 2024 and having its adverts
ignored as stale. → [Trusted Clock Source](../docs/clock_source.md)

### Watchdogs

A repeater that hangs on a roof stays dead until someone climbs up. `set watchdog <secs>` (default 300) runs the
chip's hardware watchdog: if the firmware stops, the chip resets and the repeater comes back by itself, and
`get watchdog` shows `last reset: Watchdog`. `set rx.watchdog <mins>` (off by default, for busy sites) reboots it
if no packet has been received for that long, which catches a radio that stopped receiving. nRF52 boards only
for the hardware watchdog. → [Watchdogs](../docs/watchdog.md)

### `help` command

`help` or `?` lists every command, grouped by topic (lora, timekeeping, routing, bot, ...). `help <topic>` lists a topic's
commands and `help <command>` shows its usage. Over remote admin from the app, every reply fits in one packet, and
commands that only work over USB serial are left out. → [CLI Commands](../docs/cli_commands.md#list-the-available-commands-repeater-only)

## Branches

| Branch | Contents |
|---|---|
| **`deploy`** | All four features, merged. **Build from this branch.** |
| `feature/repeater-bot` | The bot only |
| `feature/clock-source` | The clock source only |
| `feature/watchdog` | The watchdogs only |
| `feature/repeater-help` | `help` only |
| `main` | Upstream MeshCore, unchanged |

Each feature branch starts from upstream and stands alone, so any of them can be offered upstream separately.
`deploy` adds only what joins them up (for example, `help` listing the bot's commands).

## Building

Same as upstream ([PlatformIO](https://platformio.org/)), from the `deploy` branch. Pick your board's `*_repeater`
environment:

```
git clone -b deploy https://github.com/mkrzywonski/MeshCore.git
cd MeshCore

# repeater with the bot
PLATFORMIO_BUILD_FLAGS="-D REPEATER_BOT=1" pio run -e RAK_4631_repeater

# repeater with the bot, logging packets to serial for an MQTT observer (e.g. meshcoretomqtt)
PLATFORMIO_BUILD_FLAGS="-D REPEATER_BOT=1 -D MESH_PACKET_LOGGING=1" pio run -e Heltec_v3_repeater

# repeater without the bot (clock source, watchdogs and help are always included)
pio run -e RAK_4631_repeater
```

Running on RAK4631 and Heltec V3; the bot has also run on a LilyGo T-Echo. Flash the result as you would any MeshCore build. Updating over an
existing MeshCore repeater keeps its keys and settings.

## Status

This is a personal fork, running on a few repeaters in Central Texas. Issues and suggestions are welcome. If
upstream adopts any of these features, they'll be removed from here.

License: MIT, as upstream (see [license.txt](../license.txt)).
