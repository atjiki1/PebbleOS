# PebbleOS multi-link (personal build)

[한국어](README.md) · **English**

This is a personal firmware built from the official
[coredevices/PebbleOS](https://github.com/coredevices/PebbleOS) releases with a set of patches on top.
It lets a Pebble Time 2 (obelix) or Pebble Round 2 (getafix) stay connected to **an iPhone and an
iPad at the same time**, groups notifications by sender, and gathers installed apps in an
"Apps" folder.

- Firmware: `v<version>-multilink` under [Releases](https://github.com/atjiki1/PebbleOS/releases)
- Korean language pack: `v<version>-ko` under [atjiki1/PebbleOS-Korean](https://github.com/atjiki1/PebbleOS-Korean/releases)

Everything that differs from the official firmware is listed below. Everything else behaves
exactly like the official firmware.

## 1. A second device (iPhone + iPad)

| | Official firmware | This firmware |
|---|---|---|
| Connections | One phone | One phone + one second device (two BLE links) |
| Notifications from the second device | None | Notifications from a second iOS device such as an iPad (ANCS) |
| Reconnecting | Phone only | Both reconnect automatically; the second device appears under iOS Settings → Bluetooth → "Other Devices" |

**Adding a second device**
1. On the watch, open Settings → Bluetooth and keep that menu open.
2. On the iPad, open Settings → Bluetooth and tap the watch to pair.
3. The watch vibrates once the device is connected. It reconnects automatically after that.

- Only one second device is supported. To replace it, forget the current one in the watch's
  Bluetooth menu, then pair the new one.
- The Pebble app runs on the phone (the first device) only. The second device only sends
  notifications.
- If both devices post the same notification within 30 seconds, only the phone's copy is kept.
- Notifications from the second device end their subtitle with `· <device name>`.

## 2. Notifications by sender

Two settings under Settings → Notifications. Both take Never / 1 Day / 1 Week / All.

**Group by Sender**: this setting also exists in the official firmware.
- The official firmware groups Android notifications only. This firmware also groups
  **iPhone and iPad notifications**.
- Notifications are grouped by app and notification title. The same name in two different apps
  makes two groups.
- While this setting is on:
  - Opening a group lets you scroll through that sender's messages only.
  - Opening a single row shows just that notification. Back returns to the list.

**Popup by Sender**: a new setting in this firmware.
- The popup for a new message also shows that sender's earlier messages with their times, up to
  10 messages.
- More messages from the same sender join the same card instead of adding cards.
- Dismissing that card also dismisses the earlier messages it shows, on the iPhone and iPad as
  well. Closing it with Back dismisses nothing.

Notifications are stored the same way as in the official firmware: one by one, and the oldest are
removed first when storage is full.

## 3. The "Apps" folder in the launcher

- The watch's app list gets an **"Apps" row right after Settings**.
  - If you move Settings in the Pebble app, "Apps" follows it.
- "Apps" opens the **apps installed through the Pebble app, sorted by name**.
  - Sorting ignores case; Korean names come after English ones.
- The main list keeps only the built-in apps (Settings, Notifications, Music, Alarms, Weather, ...).
  - Their order can still be changed in the Pebble app.
- The "Apps" row is hidden while no app is installed.
- Closing an app launched from the folder returns to the folder.
- The "Apps" row exists on the watch only. The list in the iPhone Pebble app does not change.

## 4. Version shown on the watch

- Settings → System → Information shows the official version (e.g. `v4.47.3`). This keeps the
  Pebble app from offering an update.
- The downloaded files have `-multilink` in their names. You can tell that this build is installed
  by the second-device hint in the watch's Bluetooth menu.
- **Do not install the official update if the Pebble app offers one.** It removes all of these
  features. When a new official version comes out, this repository builds the multi-link version
  automatically.

## 5. Automatic builds

`.github/workflows/multilink-release.yml`
- Checks the official repository every day for a new release tag (`vX.Y.Z`).
- For a new tag, applies the patches in `.multilink/patches/` in order to the official source.
- Builds four boards (obelix pvt/dvt, getafix dvt/dvt2) with `CONFIG_BT_MULTI_LINK_EXPERIMENT=y`.
- Publishes the files as the `<tag>-multilink` release, together with `pebbleos.pot`, the list
  of strings in that firmware.
- Reports success or failure through Bark.
- If the official code changes too much for the patches to apply, the build fails and the patches
  have to be updated for the new version.

## 6. Patches

| Patch | Change |
|---|---|
| 0001 | Allow two BLE links and manage the second device's link (`CONFIG_BT_MULTI_LINK_EXPERIMENT`) |
| 0002 | Store the second device's bonding separately from the phone's |
| 0003 | Pair the second device from the Bluetooth menu and reconnect it when it drops |
| 0004 | Add an ANCS (notification) client for the second device |
| 0005 | Solicit ANCS in the reconnection advertising so iOS reconnects the second device by itself |
| 0006 | Solicit ANCS while pairing too, so the watch shows under iOS "Other Devices" |
| 0007 | Link the second device's bonding when it reconnects (for the device name) |
| 0008 | Keep one copy of a notification posted by both devices; name the second device |
| 0009 | Group iOS notifications by sender; how groups and single rows open |
| 0010 | Show a sender's earlier messages in the popup (Popup by Sender) |
| 0011 | Gather installed apps in an "Apps" folder after Settings, sorted by name |

The code lives in two branches:
- `multi-link-experiment`: development history of 0001-0008
- `ml-grouping`: 0001-0011 on top of the latest official tag

To regenerate the patches:

    git format-patch <tag>..ml-grouping -o .multilink/patches

## 7. Known limitations

- The second device must be an iOS device (ANCS).
- The second device only provides notifications and notification actions such as Dismiss. Apps,
  settings sync and voice replies work through the phone only.
- A sleeping iPad can take a few minutes to reconnect. Opening Settings → Bluetooth on the iPad
  connects it right away.
- Restarting the watch clears all stored notifications, as in the official firmware.

This is a personal build and is not officially supported.
