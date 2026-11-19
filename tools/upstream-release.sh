#!/bin/sh
#
# This file is part of openmediavault-spindown.
#
# openmediavault-spindown is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# upstream-release.sh <tag>: run on the development machine. Builds a local
# branch omv-extras/<tag> for the release pull request to the omv-extras
# fork (remote "upstream"): the fork's main, changed to match <tag>, minus
# the files that belong only to this repository. Never pushes. The process
# is in MAINTAINING.md.
set -eu

# Paths that exist only in this repository and never go to the fork.
FORK_ONLY='
CONTRIBUTING.md
CODE_OF_CONDUCT.md
SECURITY.md
MAINTAINING.md
tools/upstream-release.sh
.github/CODEOWNERS
.github/ISSUE_TEMPLATE
.github/pull_request_template.md
.github/dependabot.yml
'
FORK_REPO=OpenMediaVault-Plugin-Developers/openmediavault-spindown
OWN_REPO=nader-eloshaiker/omv-spindown-plugin

die() { echo "upstream-release: $*" >&2; exit 1; }

# Everything runs inside main(), so the shell has read the whole script
# before the branch switch removes this file from the working tree.
main() {
    tag=${1:?usage: tools/upstream-release.sh v8.0.N}
    version=${tag#v}
    branch="omv-extras/$tag"
    me=$(git config user.email)

    [ -z "$(git status --porcelain --untracked-files=no)" ] ||
        die "commit or stash your changes first"
    git remote get-url upstream >/dev/null 2>&1 ||
        die "no remote 'upstream'; add it: git remote add upstream https://github.com/$FORK_REPO.git"
    git fetch -q upstream
    git fetch -q --tags origin
    git rev-parse -q --verify "refs/tags/$tag" >/dev/null || die "no tag $tag"
    ! git rev-parse -q --verify "refs/heads/$branch" >/dev/null ||
        die "branch $branch already exists"

    # Commits made on the fork that this repository does not have yet.
    # Release commits from this script are ours and are skipped.
    missing=$(git cherry "$tag" upstream/main | sed -n 's/^+ //p' |
        while read -r c; do
            [ "$(git log -1 --format=%ae "$c")" = "$me" ] || echo "$c"
        done)
    if [ -n "$missing" ]; then
        echo "The fork has commits this repository does not have yet:" >&2
        for c in $missing; do git log -1 --format='  %h %an: %s' "$c" >&2; done
        die "bring them in first (MAINTAINING.md, 'Changes made on the fork'), then tag again"
    fi

    start=$(git rev-parse --abbrev-ref HEAD)
    # If anything below fails, drop the half-built branch and go back.
    trap 'git reset -q --hard; git switch -q "$start"; git branch -q -D "$branch";
          echo "upstream-release: failed; nothing was kept" >&2' EXIT
    git switch -q -c "$branch" upstream/main
    # Match the tag exactly, including deletions...
    git rm -rq .
    git checkout "$tag" -- .
    # ...except the fork-only paths, which keep the fork's own state.
    echo "$FORK_ONLY" | while read -r p; do
        [ -n "$p" ] || continue
        git rm -rqf --ignore-unmatch -- "$p"
        if git cat-file -e "upstream/main:$p" 2>/dev/null; then
            git checkout upstream/main -- "$p"
        fi
    done

    if git diff --cached --quiet; then
        trap - EXIT
        git switch -q "$start"
        git branch -q -D "$branch"
        echo "Nothing to send: the fork already matches $tag."
        exit 0
    fi

    # The commit message is the newest debian/changelog entry, as in CI.
    notes=$(awk 'NR > 2 && /^ -- / { exit } NR > 2 { sub(/^  /, ""); print }' debian/changelog)
    git commit -q -m "openmediavault-spindown $version" -m "$notes" \
        -m "From $OWN_REPO $tag."
    trap - EXIT
    git switch -q "$start"

    echo "Built $branch: the fork's main plus $tag."
    git diff --stat "upstream/main" "$branch"
    cat <<EOF

Review what the fork will receive:
  git diff upstream/main $branch
Then, with the owner's OK:
  git push -u origin $branch
  gh pr create --repo $FORK_REPO --base main --head nader-eloshaiker:$branch \\
    --title "openmediavault-spindown $version"
EOF
}

main "$@"
