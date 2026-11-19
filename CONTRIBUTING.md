# Contributing

Thanks for helping. Issues and pull requests are welcome, especially
reports from disks that still do not sleep with the plugin installed.

## Before you start

- Read [AGENTS.md](AGENTS.md) for the rules that always apply (they are
  for people too).
- For anything larger than a fix, open an issue first so we can agree on
  the approach.

## Making a change

```sh
make test         # unit tests, no hardware needed
make docker-deb   # build the .deb in Docker (any OS)
```

- CI runs the tests, PHP/JSON/YAML/shell syntax checks, the package
  build and lintian on every pull request. Workflows from first-time
  contributors start once the maintainer approves them.
- Nothing may wake a sleeping disk: activity comes only from
  `/sys/block/*/stat`, and disk commands only with `smartctl -n standby`.
- The spindown time per disk is OMV's own (Storage › Disks); the plugin
  has no settings object of its own.
- New config fields need a version bump in `debian/changelog` and a
  confdb migration. Versions are PEP 440: `8.0.N`.
- Commit messages: an imperative subject, and a body explaining why.
- `main` is protected: every change reaches it through a pull request
  from a branch, including the maintainer's, and CI must pass first.
  Rebase on `main` rather than merging it in; pull requests are squashed
  or rebased, never merged with a merge commit.
- Don't include host names, IP addresses or serial numbers in code, docs,
  logs or screenshots.

Changes to the daemon are tested on a real NAS before release. If you
tested on your own hardware, say which disks and what you checked in the
pull request.

## Releases

The maintainer adds the `debian/changelog` entry and pushes a `v<version>`
tag; CI builds the package and publishes the GitHub release.

## Code of conduct

Everyone taking part is expected to follow the
[code of conduct](CODE_OF_CONDUCT.md).
