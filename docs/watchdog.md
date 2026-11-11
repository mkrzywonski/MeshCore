# Watchdogs (Repeater)

A repeater on a roof or a tower that hangs stays dead until someone climbs up and power-cycles it. Two
watchdogs let it recover by itself.

## Hardware watchdog

`set watchdog <seconds>` (default **300**, range 30-3600, `0`/`off` to disable).

The repeater's main loop feeds the chip's watchdog timer on every pass, thousands of times a second. If the
firmware stops (stuck in a loop, deadlocked, or crashed into a fault handler), nothing feeds the timer, and
when it runs out the hardware resets the chip. The repeater boots normally, sends its advert and carries on.

`get watchdog` shows the setting and why the node last reset, for example:

```
> 300s, last reset: Watchdog
```

`Watchdog` there means it froze and recovered. Other values: `Cold Boot` (power-on), `Soft Reset` (`reboot`,
or the receive watchdog), `Reset Pin`, `CPU Lockup`.

**nRF52 details** (RAK4631, SenseCAP Solar, T-Echo...):

- Once started, the watchdog can't be stopped or changed until the chip resets, and it survives a soft
  `reboot`. A new value (or `off`) takes effect after the next power-on or watchdog reset; until then
  `get watchdog` shows the running value too.
- It keeps counting while the CPU sleeps, so a hang that leaves the CPU asleep is still caught. It pauses
  while a debugger halts the CPU.
- The bootloader feeds it during firmware updates (USB and Bluetooth DFU), so an update isn't interrupted.
- It starts once the repeater has loaded its settings, near the end of startup.

Boards without support (currently everything but nRF52) report `not running: unsupported on this board`.

## Receive watchdog

`set rx.watchdog <minutes>` (default **off**, range 5-1440).

If the repeater hasn't received a single valid packet for this many minutes, it reboots. This catches the case
the hardware watchdog can't: the firmware keeps running, but the radio has stopped receiving.

Only use it at a site that always hears traffic. On a busy repeater 30 minutes without a packet means
something is wrong; on a quiet one it's normal, and the node would reboot for nothing. `get rx.watchdog`
shows the time since the last packet and how many times it has rebooted the node:

```
> 30min, last rx 4s ago, 0 reboots
```

## Testing

A build with `-D WATCHDOG_TEST` adds commands that stop the firmware on purpose, each a different way. They
work over USB serial only, and a normal build doesn't contain them.

| Command | What it does | Imitates |
|---|---|---|
| `wdtest spin` | loops forever with the CPU busy | firmware stuck in a loop |
| `wdtest block` | suspends the main loop forever; the CPU idles and sleeps | a deadlock |
| `wdtest fault` | writes to an invalid address | a crash |

Procedure: `set watchdog 30`, power-cycle so it applies, then run each test. The serial port drops and the node
should be back about 30 s later, with `get watchdog` showing `last reset: Watchdog` (a crash may reset at once
with `CPU Lockup` or similar instead). For the receive watchdog, `set rx.watchdog 5` and move the radio to an
unused frequency (`set freq`, reboot): it should reboot after 5 minutes and count `1 reboots`. Put both back
afterwards and flash a normal build.

## Settings file

Both settings are kept in `/watchdog` on the node's filesystem, so they survive reboots and firmware
updates.
