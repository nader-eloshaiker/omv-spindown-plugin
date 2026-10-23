# openmediavault-spindown: make idle hard disks sleep in OpenMediaVault

**[🌐 Project website](https://nader-eloshaiker.github.io/omv-spindown-plugin/)** ·
[⬇️ Download](https://github.com/nader-eloshaiker/omv-spindown-plugin/releases/latest) ·
[🐞 Report a problem](https://github.com/nader-eloshaiker/omv-spindown-plugin/issues)

**Your disks are set to spin down in OpenMediaVault, but they never do?
This free plugin puts them to sleep.**

[OpenMediaVault](https://www.openmediavault.org/) (OMV) lets you set a
*Spindown time* for each disk in **Storage › Disks**. It hands that time
to the disk, and the disk is meant to go to sleep by itself once it has
been idle that long. Some disks, notably **WD Red** drives, stop doing
that: they stay spinning day and night, even though nothing is using
them. Yet they still go to sleep straight away when told to.
([OMV issue #1892](https://github.com/openmediavault/openmediavault/issues/1892)
has many reports.)

This plugin does the waiting for them:

- ✅ **Point and click**: pick a disk, click **Edit**, choose how long it may
  sit idle. No commands, no settings files. A Spindown time you already
  set in Storage › Disks is picked up as it is.
- ✅ **Never wakes a sleeping disk.** It watches the kernel's activity
  counters, which does not touch the disk, and the sleep command does
  nothing to a disk that is already asleep.
- ✅ **Puts it back to sleep after every wake-up**, whether a file was
  read or written.
- ✅ **Shows what is going on**: for each disk, whether it is asleep or
  in use, how soon it will sleep, and when it last went to sleep.

Disks that do sleep by themselves are not affected: they are already
asleep when the plugin checks, so it does nothing.

## Install

1. Download `openmediavault-spindown_<version>_all.deb` from the
   [latest release](https://github.com/nader-eloshaiker/omv-spindown-plugin/releases/latest).
2. In the OMV web interface open **System › Plugins**, click **Upload**,
   choose the file, then select the plugin and click **Install**.
3. Open **Storage › Spin down**. It lists your hard disks.
4. Select a disk, click **Edit**, choose **Put to sleep after** (for
   example *10 minutes*) and click **Save**. Do the same for each disk.
5. Click **Apply** in the yellow bar at the top.

The **Status** column then counts down to sleep and shows *Asleep* once
the disk is sleeping. Choose *Never* for a disk that should stay awake.

## Still awake?

The plugin only helps when nothing is using the disk. If the Spin down
page shows a disk that never stays idle for its spindown time, something is
reading or writing it: Docker containers or apps with data on that disk,
media servers scanning libraries, clients browsing shares, or ZFS with
`atime=on`. Move busy app data to an SSD, and keep the hard disks for
bulk storage.

Clicking **Apply** in OMV, or installing a plugin, wakes every hard disk
for a moment: OMV's configuration tool (Salt) scans the disks for LVM
volumes each time it runs. The disks go back to sleep after their time.

## How it works

The time you choose is stored as the disk's *Spindown time*, the same
setting as in Storage › Disks, so both pages always agree. Every 30
seconds `spindownd` reads `/sys/block/<disk>/stat`, the
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

## Build from source

```sh
make test          # unit tests
make docker-deb    # build the .deb in Docker (any OS); lands in dist/
make deploy NAS=root@NAS_HOST
```

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
