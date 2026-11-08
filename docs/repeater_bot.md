# Repeater Bot (REPEATER_BOT build option)

A small test bot that runs inside repeater firmware. Send it a direct message from any companion and it replies with
**how the repeater heard you**: the path your message took and the signal strength of the last hop. It's a quick way
to check two-way connectivity from wherever you are, without anyone at the other end.

The bot is a "virtual companion": it has its own identity and name (by default `<repeater name> Bot`) and advertises
as a chat node, separately from the repeater. It needs no extra hardware. It only answers direct messages sent to it.
It never touches channel traffic, and the repeater forwards packets exactly as before.

## Building

The bot is compiled in only when you ask for it:

```
PLATFORMIO_BUILD_FLAGS="-D REPEATER_BOT=1" pio run -e RAK_4631_repeater
```

Any `*_repeater` environment should work; it has been run on RAK4631, Heltec V3 and T-Echo. Without the flag the
firmware is identical to a normal repeater.

On first boot the bot creates its own keypair (stored as `_bot`), and the serial console prints its ID after the
repeater's.

## Using it

1. Add the bot as a contact. It sends an advert two seconds after each of the repeater's adverts (local and flood),
   so it shows up wherever the repeater's adverts reach. `bot advert` sends one straight away.
2. Send it any direct message.
3. Read the reply.

### Reading a reply

The default (short) replies are kept small on purpose. A long reply takes about three times as long on air as an
ACK and gets lost far more often over many hops.

| Reply | Meaning |
|---|---|
| `61,36,b4 -3.5dB -110dBm` | Your message was flooded and relayed by repeaters `61`, `36` then `b4` (your end first), and the bot heard the last hop at SNR -3.5 dB, RSSI -110 dBm |
| `~61,0d,c0 -5.0dB -114dBm` | Your message came by a direct route. `~` marks a route rather than a measured path: a direct packet's path is used up on the way, so the bot shows its own stored route to you, reversed |
| `~? -5.0dB -114dBm` | A direct message along a route the bot doesn't know (routes are kept in RAM, so this happens after a reboot) |
| `13.0dB -38dBm` | No relays: the bot's repeater heard your radio directly |

Hop IDs are the first byte(s) of each repeater's public key, the same IDs the app shows. The repeater hosting the bot
never appears in the path, because it's the one receiving the message. Neither does the bot's own ID.

`bot format long` switches to sentences, for example
`Heard you: 3 hops via 61,36,b4 | SNR -3.50 dB RSSI -110 dBm`.

## How it works

- **Knowing who you are.** To decrypt a message, the bot needs the sender's public key. It learns keys from chat
  adverts the repeater hears and keeps up to 40 in RAM, evicting the one heard least recently. Adverts often don't
  travel far, so it also keeps a **contact cache** in flash: anyone who DMs it successfully is saved, up to 16 entries,
  oldest replaced first. A contact that's already saved costs no flash write. `bot learn` adds a key by hand, for a
  sender whose adverts never reach the bot.
- **Replying.** The bot ACKs like a normal companion. For a flooded message it sends the route back with the ACK,
  waits briefly for your companion's own route, then replies directly. If a direct reply isn't ACKed, it is sent once
  more by flood.
- **Retries.** A resent message (same timestamp) is ACKed again but doesn't produce a second reply.

### Airtime limits

The bot must never become a load on the mesh, so it's conservative:

- at most one reply every **3 seconds per contact**
- at most **12 replies per minute** overall
- at most 4 replies in progress at once

Requests over a limit are **dropped, not queued**. A burst of requests must not turn the bot into a denial of service
on the repeater or the mesh.

## Commands

All bot commands work over the serial console and remote admin.

| Command | |
|---|---|
| `bot` | Status: name, on/off, ID, contacts known (and saved), messages received, replies sent, reply format |
| `bot on` / `bot off` | Turn the bot on or off (saved) |
| `bot name <name>` | Set the bot's name. `bot name -` goes back to `<repeater name> Bot` |
| `bot advert` | Send the bot's advert now (flood) |
| `bot key` | The bot's public key |
| `bot format [short\|long]` | Show or set the reply format. Default `short` |
| `bot learn <64 hex chars> [name]` | Add a contact by public key and save it to the cache |
| `bot cache` | List the saved contacts. `bot cache clear` empties the cache |
| `bot log [n\|clear]` | What the bot did with recent messages (last 32 events, RAM only). Over serial, prints them all; over remote admin, returns the n-th newest (default 1) |

## Files

| File | Contents |
|---|---|
| `_bot` (identity store) | The bot's keypair |
| `/bot_prefs` | On/off, name and reply format |
| `/bot_cache` | The contact cache |
