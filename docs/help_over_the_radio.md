# Help over the radio

What `help` replies when sent from the MeshCore app (remote admin → Command Line), for this fork's `deploy`
build (with the repeater bot). Each reply fits in one packet (at most 150 characters).

Over USB serial, `help` prints every command grouped by topic and `help <topic>` prints each command's usage,
so serial output is longer than what's shown here.

Generated from `examples/simple_repeater/HelpText.cpp`; regenerate after changing it.

## Topics

```
> help
Topics: info node timekeeping lora routing adverts access system regions gps sensors bot. Send help <topic>, or help <command> for usage
```

## info

```
> help info
info: help, ver, board, power, neighbors, role, bootloader.ver, pwrmgt, clear stats
```

```
> help help
help [topic|command] - list topics, a topic's commands, or a command's usage
```

```
> help ver
ver - firmware version and build date
```

```
> help board
board - hardware name
```

```
> help power
power - battery mV, and whether USB, solar or a charger is supplying power
```

```
> help neighbors
neighbors - repeaters heard directly, newest first, as id:seconds ago:SNR x4 (47 = 11.75 dB)
```

```
> help role
get role - firmware role (repeater)
```

```
> help bootloader.ver
get bootloader.ver - bootloader version (nRF52 boards)
```

```
> help pwrmgt
get pwrmgt.support|source|bootreason|bootmv - power management: supported?, power source, last reset/shutdown reason, boot voltage
```

```
> help stats-core
stats-core - battery, uptime, queue, errors (JSON). In the app: Status screen [USB serial only]
```

```
> help stats-radio
stats-radio - noise floor, last RSSI/SNR, airtime (JSON). In the app: Status screen [USB serial only]
```

```
> help stats-packets
stats-packets - packets received/sent, flood/direct (JSON). In the app: Status screen [USB serial only]
```

```
> help clear stats
clear stats - reset the packet and airtime counters
```

## node

```
> help node
node: name, lat, lon, owner.info, public.key, prv.key
```

```
> help name
get|set name <name> - node name shown in apps and on maps
```

```
> help lat
get|set lat <degrees> - latitude shown on maps, decimal (30.0711)
```

```
> help lon
get|set lon <degrees> - longitude shown on maps, decimal (-97.8206)
```

```
> help owner.info
get|set owner.info <text> - contact info others can read; | starts a new line
```

```
> help public.key
get public.key - this node's public key (64 hex)
```

```
> help prv.key
get|set prv.key <128 hex> - identity private key. get: USB serial only. set: reboot to apply
```

## timekeeping

```
> help timekeeping
timekeeping: clock, clock sync, time, clock.source, clkreboot
```

```
> help clock
clock - show the time (UTC)
```

```
> help clock sync
clock sync - set the clock to the sender's time (your phone, in the app). Forward only
```

```
> help time
time <epoch secs> - set the clock to a Unix time. Forward only
```

```
> help clock.source
get|set clock.source <64 hex pubkey>|off - follow a trusted node's clock (see docs/clock_source.md)
```

```
> help clkreboot
clkreboot - reset the clock to 2024 and reboot (undoes a clock set in the future)
```

## lora

```
> help lora
lora: radio, freq, tx, tempradio, af, dutycycle, cad, int.thresh, radio.rxgain, agc.reset.interval
```

```
> help radio
get|set radio <freq>,<bw>,<sf>,<cr> - LoRa settings, e.g. 910.525,62.5,7,5. Reboot to apply
```

```
> help freq
get|set freq <MHz> - frequency. set: USB serial only, reboot to apply
```

```
> help tx
get|set tx <dBm> - transmit power (SX1262: up to 22)
```

```
> help tempradio
tempradio <freq>,<bw>,<sf>,<cr>,<mins> - use other LoRa settings for <mins>, then go back
```

```
> help af
get|set af <0-9> - airtime factor: stay quiet af x airtime after sending; duty cycle 1/(1+af)
```

```
> help dutycycle
get|set dutycycle <1-100> - max % of time transmitting (sets af)
```

```
> help cad
get|set cad on|off - channel activity detection: listen before transmitting
```

```
> help int.thresh
get|set int.thresh <n> - local interference threshold, 0 = off
```

```
> help radio.rxgain
get|set radio.rxgain on|off - boosted receive gain
```

```
> help agc.reset.interval
get|set agc.reset.interval <secs> - reset receiver gain every n secs (multiple of 4, 0 = off). Austin Mesh: 4
```

## routing

```
> help routing
routing: repeat, txdelay, direct.txdelay, rxdelay, multi.acks, path.hash.mode, loop.detect, flood.max, flood.max.advert, flood.max.unscoped
```

```
> help repeat
get|set repeat on|off - forward other nodes' packets. off: still answers logins and the bot
```

```
> help txdelay
get|set txdelay <0-2> - flood retransmit delay factor; higher with more neighbors (Austin Mesh table)
```

```
> help direct.txdelay
get|set direct.txdelay <0-2> - retransmit delay factor for direct (routed) packets
```

```
> help rxdelay
get|set rxdelay <0-20> - wait longer on weaker copies of a flood so better paths win. Austin Mesh: 3
```

```
> help multi.acks
get|set multi.acks 0|1 - send an extra ack for direct messages. Austin Mesh: 1
```

```
> help path.hash.mode
get|set path.hash.mode 0|1|2 - size of this node's id in paths: 1, 2 or 3 bytes
```

```
> help loop.detect
get|set loop.detect off|minimal|moderate|strict - drop floods that already passed through here
```

```
> help flood.max
get|set flood.max <0-64> - max hops for floods this node forwards
```

```
> help flood.max.advert
get|set flood.max.advert <0-64> - max hops for adverts this node forwards
```

```
> help flood.max.unscoped
get|set flood.max.unscoped <0-64> - max hops for floods with no region
```

## adverts

```
> help adverts
adverts: advert, advert.zerohop, advert.interval, flood.advert.interval, discover.neighbors, neighbor.remove
```

```
> help advert
advert - send a flood advert now (reaches the whole mesh)
```

```
> help advert.zerohop
advert.zerohop - send an advert to direct neighbors only
```

```
> help advert.interval
get|set advert.interval <60-240> - minutes between zero-hop adverts, 0 = off
```

```
> help flood.advert.interval
get|set flood.advert.interval <3-168> - hours between flood adverts, 0 = off
```

```
> help discover.neighbors
discover.neighbors - ask nearby repeaters to answer (refreshes neighbors)
```

```
> help neighbor.remove
neighbor.remove <key prefix> - forget neighbors whose key starts with the prefix (a space: all)
```

## access

```
> help access
access: password, guest.password, setperm, allow.read.only
```

```
> help password
password <new> - set the admin password (the reply shows it)
```

```
> help guest.password
get|set guest.password <pw> - password for guest logins, empty = none
```

```
> help setperm
setperm <pubkey> <0-3> - client access: 0 guest (removes), 1 read-only, 2 read-write, 3 admin
```

```
> help acl
get acl - list clients and their permissions [USB serial only]
```

```
> help allow.read.only
get|set allow.read.only on|off - room servers only; no effect on a repeater
```

## system

```
> help system
system: reboot, poweroff, start ota, powersaving, adc.multiplier, log start|stop|erase
```

```
> help reboot
reboot - restart the node
```

```
> help poweroff
poweroff - turn the node off (also: shutdown). Needs a button or power cycle to start
```

```
> help start ota
start ota - turn on Bluetooth for a firmware update; the reply has its MAC. reboot cancels
```

```
> help erase
erase - format the file system: deletes settings, keys and contacts [USB serial only]
```

```
> help powersaving
powersaving [on|off] - sleep between radio events
```

```
> help adc.multiplier
get|set adc.multiplier <x> - calibrate the battery reading, 0 = board default
```

```
> help log start
log start|stop|erase - record received packets to flash, stop, delete the record
```

```
> help log
log - print the packet record saved in flash [USB serial only]
```

## regions

```
> help regions
regions: region, region put|remove|allowf|denyf|get|list|home|default|def|save
```

```
> help region
region - list regions; F = floods allowed
```

```
> help region put
region put <name> [parent] - add a region (floods allowed)
```

```
> help region remove
region remove <name> - delete a region (must have no children)
```

```
> help region allowf
region allowf <name> - allow floods scoped to the region
```

```
> help region denyf
region denyf <name> - block floods scoped to the region
```

```
> help region get
region get <name> - show a region, its parent and F flag
```

```
> help region list
region list allowed|denied - names of regions with floods allowed or blocked
```

```
> help region home
region home [name] - show or set this node's home region
```

```
> help region default
region default [name|<null>] - show or set the scope for floods without one; saved
```

```
> help region def
region def <name>[|jump] ... - define a hierarchy on one line (see docs)
```

```
> help region save
region save - save region changes to flash (otherwise lost at reboot)
```

```
> help region load
region load - bulk-load an indented region list; a blank line ends it [USB serial only]
```

## gps

```
> help gps
gps: gps [on|off], gps sync|setloc|advert|diag
```

`help gps` shows the topic, so ask with `help gps [on|off]`:

```
> help gps [on|off]
gps [on|off] - GPS state (on/off, fix, satellites), or turn it on or off
```

```
> help gps sync
gps sync - set the clock from GPS
```

```
> help gps setloc
gps setloc - set lat/lon from the GPS position
```

```
> help gps advert
gps advert [none|share|prefs] - location in adverts: none, live GPS, or lat/lon settings
```

```
> help gps diag
gps diag - GPS diagnostics
```

## sensors

```
> help sensors
sensors: sensor list|get|set
```

```
> help sensor list
sensor list [start] - sensor settings and values
```

```
> help sensor get
sensor get <key> - one sensor setting
```

```
> help sensor set
sensor set <key> <value> - change a sensor setting
```

## bot

```
> help bot
bot: bot, bot on|off|name|advert|key|format|learn|cache|log
```

`help bot` shows the topic, so ask with `help bot status`:

```
> help bot status
bot - status: on/off, id, contacts, messages in/out, reply format
```

```
> help bot on
bot on|off - turn the bot on or off
```

```
> help bot name
bot name [<name>|-] - show or set the bot's name; - = "<repeater name> Bot"
```

```
> help bot advert
bot advert - send a flood advert for the bot
```

```
> help bot key
bot key - the bot's public key
```

```
> help bot format
bot format [short|long] - reply style; short fits long paths better
```

```
> help bot learn
bot learn <64 hex pubkey> [name] - add a contact whose adverts don't reach the bot
```

```
> help bot cache
bot cache [clear] - contacts saved to flash (kept across reboots), or forget them
```

```
> help bot log
bot log [n|clear] - what the bot did; remote: n-th newest entry (default 1), serial: all
```

## Not found

```
> help nonsense
No help for 'nonsense'. Send help for the topics
```

