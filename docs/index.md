---
title: Make idle hard disks sleep in OpenMediaVault
description: >-
  Disks set to spin down in OpenMediaVault but never sleep, such as WD Red
  drives? This free OMV plugin puts them to sleep after their spindown
  time, without ever waking a sleeping disk.
---

# Make idle hard disks sleep in OpenMediaVault

[Download the latest release](https://github.com/nader-eloshaiker/omv-spindown-plugin/releases/latest){: .btn }
[Source on GitHub](https://github.com/nader-eloshaiker/omv-spindown-plugin){: .btn }

[OpenMediaVault](https://www.openmediavault.org/) (OMV) lets you set a
*Spindown time* for each disk in **Storage › Disks**. It hands that time
to the disk, and the disk is meant to go to sleep by itself once it has
been idle that long. Some disks, notably **WD Red** drives, stop doing
that: they stay spinning day and night, even though nothing is using
them. Yet they still go to sleep straight away when told to.
([OMV issue #1892](https://github.com/openmediavault/openmediavault/issues/1892)
has many reports.)

This plugin does the waiting for them:

- ✅ **Uses the Spindown time you already set** in Storage › Disks. Nothing
  new to configure, and a disk with spindown *Disabled* is left alone.
- ✅ **Never wakes a sleeping disk.** It watches the kernel's activity
  counters, which does not touch the disk, and the sleep command does
  nothing to a disk that is already asleep.
- ✅ **Puts it back to sleep after every wake-up**, whether a file was
  read or written.
- ✅ **Shows what is going on**: each disk's spindown time, how long it has
  been idle, whether it is asleep, and when it last went to sleep.

Disks that do sleep by themselves are not affected: they are already
asleep when the plugin checks, so it does nothing.

## Install

1. Download `openmediavault-spindown_<version>_all.deb` from the
   [latest release](https://github.com/nader-eloshaiker/omv-spindown-plugin/releases/latest).
2. In the OMV web interface open **System › Plugins**, click **Upload**,
   choose the file, then select the plugin and click **Install**.
3. Make sure your disks have a **Spindown time** in **Storage › Disks**
   (edit the disk).
4. Open **Storage › Spin down › Settings**, tick **Spin down idle disks**,
   click **Save**, then **Apply**.
5. **Storage › Spin down › Disks** shows each disk going to sleep.

## Still awake?

The plugin only helps when nothing is using the disk. If the Disks page
shows a disk that never stays idle for its spindown time, something is
reading or writing it: Docker containers or apps with data on that disk,
media servers scanning libraries, clients browsing shares, or ZFS with
`atime=on`. Move busy app data to an SSD, and keep the hard disks for
bulk storage.

## How it works

Every 30 seconds `spindownd` reads `/sys/block/<disk>/stat`, the
kernel's count of completed reads and writes. When a disk's count has
not changed for its spindown time, it runs
`smartctl -n standby -s standby,now` on it once. `-n standby` means
smartctl checks the power state first and stops if the disk is asleep,
so a sleeping disk is never woken. Health checks (smartd, temperature
monitors) do not change the counters, so they neither count as use nor
stop the disk from sleeping here.

If a disk is still idle after another spindown time, the plugin asks
again, in case something like a S.M.A.R.T. check woke it without any
reads or writes.

## License

Free and open source, GPL-3.0-or-later.
