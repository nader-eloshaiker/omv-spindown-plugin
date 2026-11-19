## What and why

<!-- What does this change, and what problem does it solve? -->

## Tested on

<!-- "Unit tests only", or the disk models, OMV version and what you
checked (the disk went to sleep, stayed asleep, the Spin down page...).
Leave out host names, IP addresses and serial numbers. -->

## Checklist

- [ ] `make test` passes
- [ ] Nothing wakes a sleeping disk (activity only from `/sys/block/*/stat`,
      disk commands only with `smartctl -n standby`)
- [ ] New config fields: version bumped in `debian/changelog` and a confdb
      migration added
- [ ] Settings in the UI are clicks in plain words, not free-text paths
- [ ] README and the project website (`docs/index.md`) updated if
      behaviour changed
