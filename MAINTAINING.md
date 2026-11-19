# Maintaining

How changes and releases move between this repository and the omv-extras
fork. This file stays in this repository; it is never sent to the fork.

## The two repositories

| Repository | Role |
|-|-|
| `nader-eloshaiker/omv-spindown-plugin` (`origin`) | The source. All development happens here: README, website, issues, GitHub releases. |
| `OpenMediaVault-Plugin-Developers/openmediavault-spindown` (`upstream`) | The omv-extras fork, maintained by Aaron Murray (`ryecoaaron`). omv-extras builds its package from here. We have read access only. |

The fork receives one pull request per release, not one per change, so
reviewing it stays light for its maintainer.

`gh` picks the `upstream` remote by default in this checkout. Name the
repository on every `gh` command: `--repo nader-eloshaiker/omv-spindown-plugin`
for this one, `--repo OpenMediaVault-Plugin-Developers/openmediavault-spindown`
for the fork.

## Day to day

1. Branch from `main`, commit, push the branch and open a pull request
   here.
2. CI must pass (`build`). `main` is protected: no direct pushes, no
   force-pushes, linear history.
3. Squash-merge, or rebase-merge to keep someone else's commits under
   their name.

## Releasing

1. In a pull request: add the `debian/changelog` entry (PEP 440,
   `8.0.N`) and merge it.
2. Tag `main` with `v8.0.N` and push the tag. CI checks the tag against
   `debian/changelog`, builds the package and publishes the GitHub
   release.

## Sending a release to the fork

```sh
tools/upstream-release.sh v8.0.N
```

The script builds a local branch `omv-extras/v8.0.N`. It starts from the
fork's `main`, makes every file match the tag, and leaves out the
fork-only files (listed at the top of the script): the contributing
guide, security policy, code of conduct, issue and pull request
templates, `CODEOWNERS`, Dependabot, this file and the script itself.
The plugins in the omv-extras organisation carry only a README, so
these stay here. The result is one commit, `openmediavault-spindown 8.0.N`,
with the changelog entry as its message. The script never pushes.

It stops if the fork has commits that this repository does not have
yet, so that a release never undoes work done on the fork.

Then:

1. Review what the fork will receive: `git diff upstream/main omv-extras/v8.0.N`.
2. Push the branch here and open the pull request against the fork's
   `main`. The script prints both commands.
3. Once it is merged, delete the branch.

Shared files, such as `README.md` and `AGENTS.md`, go to the fork as they
are. Keep them correct in both places: link to files in this repository
with full URLs rather than relative paths.

## Changes made on the fork

The fork's maintainer sometimes commits there directly (packaging fixes,
translations). Bring those commits here before the next release:

```sh
git fetch upstream
git cherry main upstream/main     # + lines are missing here, apart from
                                  # our own release commits
git switch -c sync/upstream main
git cherry-pick <commit>...       # each missing commit, oldest first
git push -u origin sync/upstream
gh pr create --repo nader-eloshaiker/omv-spindown-plugin --base main --head sync/upstream
```

Rebase-merge that pull request, so the commits keep their author.
