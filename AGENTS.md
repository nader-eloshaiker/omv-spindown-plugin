# AGENTS.md

openmediavault-spindown: an OpenMediaVault 8 plugin that spins down idle
hard disks which ignore the standby timer OMV gives them. A small Python
daemon (`usr/sbin/spindownd`) watches `/sys/block/*/stat` and sends
`smartctl -n standby -s standby,now`; OMV (PHP RPC, salt, YAML workbench)
configures it. The spindown time per disk is OMV's own
(`conf.system.hdparm`, Storage › Disks).

## Rules that always apply

1. **No AI attribution anywhere**: no `Co-Authored-By`, session links or
   "generated with" lines in commits, PRs, code or docs.
2. **No personal data in the repo**: no IP addresses, host names or serial
   numbers. Use `NAS_HOST`.
3. **Never wake a sleeping disk**: activity only from `/sys/block/*/stat`;
   disk commands only with `smartctl -n standby`. Never poll a disk's
   state just to display it.
4. **Versions are PEP 440** (`8.0.N`; major = OMV major; no `~`). New
   config fields need a version bump and a confdb migration. Defaults live
   only in the datamodel.
5. Spinning disks down and up is a hardware action: tests on a real NAS
   need the owner's consent. Agents never type passwords.
6. Run `make test` before every build.

## Layout

| Path | What |
|-|-|
| `usr/sbin/spindownd` | the daemon (`run`, `status`) |
| `tests/test_spindownd.py` | unit tests with a fake clock and fake disks |
| `srv/salt/omv/deploy/spindown/` | writes `/etc/spindownd/spindownd.json`, starts/stops the service |
| `usr/share/openmediavault/` | datamodels, RPC (`Spindown`), engine module, workbench pages (Storage › Spin down) |
| `debian/` | packaging; the unit is `openmediavault-spindown.spindownd.service` |
| `docs/` | the GitHub Pages site (`index.md`) |

## Commands

```sh
make test                      # unit tests, no hardware needed
make docker-deb                # build the .deb in Docker (any OS)
make deploy NAS=root@NAS_HOST  # install on a test NAS over SSH
```
