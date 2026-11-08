# Trusted Clock Source (Repeaters)

A repeater can keep its clock right by following another node that you trust. Name that node once with
`set clock.source <public key>`; from then on, whenever the repeater receives a packet from it that carries a
timestamp, it sets its own clock forward to match.

## Why

MeshCore has no network time. Each node keeps its own clock, and other nodes use its timestamps to reject
replays: an advert is ignored unless it's newer than the last one heard from the same node.

Most repeater boards have no battery-backed clock chip. When such a repeater reboots (a firmware update, a
brown-out on a solar node), its clock restarts at a fixed date in 2024. Nodes that already know it then ignore
its adverts as stale, so it drops out of contact lists and maps until someone runs `clock sync` or `time`.
It still forwards traffic, so the mesh keeps working, but the node is effectively invisible to its neighbours.

With a clock source set, the repeater fixes its clock as soon as it hears the source again: usually at the
source's next advert, or straight away if someone sends an advert from it.

## What counts as a timestamp from the source

Only timestamps that provably come from the source's key:

- its **adverts**, which are signed
- **logins**, **requests** and **CLI messages** it sends to this repeater, which are encrypted with a key only
  the source and this repeater can compute

A good source is a nearby repeater that rarely reboots and whose clock you keep right, for example one on mains
power. A companion works too, but companions only advertise when their owner asks them to.

## Limits

| Situation | What happens |
|---|---|
| Source is ahead by less than 5 s | Ignored: that's just the time the packet spent in transit |
| Source is ahead by 5 s to 7 days | Clock steps forward to the source's time |
| Source is ahead by more than 7 days | Rejected (counted in `get clock.source`) |
| Source is behind | Ignored: the clock never goes backwards |
| This repeater's clock is unset (earlier than its firmware's build time) | Any source time from the build time up to about 10 years after it is accepted |

- **Forward only:** the mesh only needs each node's timestamps to keep increasing, and going backwards would make
  other nodes ignore this one. It also means old packets from the source, replayed by someone else, can't move the
  clock.
- **7-day limit:** protects against a source that was set far into the future by mistake. A clock can't be wound
  back except by `clkreboot`, so following such a source would be hard to undo.
- **Unset clock:** after a reboot the clock is years behind, so the 7-day limit doesn't apply until it's set.

## Commands

| Command | |
|---|---|
| `set clock.source <64 hex chars>` | Follow this node's clock. Needs the full public key. Saved to flash (`/clock_src`), so it survives reboots and firmware updates. |
| `set clock.source off` | Stop following a clock source and delete the saved key. |
| `get clock.source` | The source's key (first 8 bytes), when it was last heard and how its clock compared, how often the clock was stepped and by how much, and how many steps were rejected. |

Example status:

```
> 61EF26624E42BA51... heard 13:57 (advert, diff +0s), stepped 1x, last +78375737s at 13:57
```

`diff` is the source's clock minus this repeater's at that moment. After a step it's 0; afterwards it's normally a
few seconds negative, because packets take time to arrive.

## Tips

- After a reboot, sending an advert from the source (from the app, or `advert` on its console) fixes this
  repeater's clock within seconds.
- Choose a source this repeater hears directly or reliably. Adverts that have to travel several hops arrive less
  often.
- If the source itself has no clock chip and both lose power, neither has the right time until one of them is set
  with `clock sync` or `time`; the other then follows.
