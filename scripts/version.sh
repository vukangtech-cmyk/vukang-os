#!/usr/bin/env bash
# VUKANG OS: build and run any released version (git tag) side by side.
#
# Every version is checked out into its own git worktree under
# build/versions/<tag>, built with that version's own Makefile and run with
# its own disk image, so versions never overwrite each other's files or data.
#
#   scripts/version.sh list                 tags available
#   scripts/version.sh run <tag>            QEMU window      (make run)
#   scripts/version.sh run-serial <tag>     shell in terminal (make run-serial)
#   scripts/version.sh iso <tag>            build only, print the ISO path
#   scripts/version.sh test <tag>           that version's automated tests
#   scripts/version.sh clean [<tag>]        remove one (or all) worktrees
#
# <tag> may be a tag (v0.1.0), "latest" (newest tag) or a branch (main).
set -euo pipefail

ROOT="$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)"
DIR="$ROOT/build/versions"
cd "$ROOT"

die() { echo "version: $*" >&2; exit 1; }

fetch_tags() {
    # Pick up releases pushed from another machine; offline is fine.
    git fetch --tags --quiet origin 2>/dev/null || true
}

tags() { git tag --list 'v*' --sort=-v:refname; }

resolve() {
    local want="$1"
    [ -n "$want" ] || die "taja toleo, mfano: v0.1.0  (angalia: make versions)"
    if [ "$want" = "latest" ]; then
        want="$(tags | head -n1)"
        [ -n "$want" ] || die "hakuna tag yoyote bado"
    fi
    git rev-parse --verify --quiet "$want^{commit}" >/dev/null \
        || die "toleo '$want' halipo. Matoleo yaliyopo: $(tags | tr '\n' ' ')"
    echo "$want"
}

prepare() {
    local ver="$1" wt="$DIR/$1" want have
    git worktree prune
    want="$(git rev-parse "$ver^{commit}")"
    if [ -d "$wt" ]; then
        have="$(git -C "$wt" rev-parse HEAD)"
        if [ "$have" != "$want" ]; then          # e.g. branch moved on
            git -C "$wt" checkout --quiet --detach "$want"
        fi
    else
        mkdir -p "$DIR"
        git worktree add --quiet --detach "$wt" "$want"
    fi
    echo "$wt"
}

cmd="${1:-list}"
case "$cmd" in
list)
    fetch_tags
    echo "Matoleo ya VUKANG OS (mapya kwanza):"
    for t in $(tags); do
        built=""
        [ -f "$DIR/$t/build/vukang-os.iso" ] && built="  [imejengwa]"
        printf '  %-8s %s%s\n' "$t" "$(git log -1 --format='%ad  %s' --date=short "$t")" "$built"
    done
    echo
    echo "Mfano: make run-version V=v0.1.0   (au V=latest, V=main)"
    ;;
run|run-serial|iso|test)
    fetch_tags
    ver="$(resolve "${2:-}")"
    wt="$(prepare "$ver")"
    echo "==> VUKANG OS $ver  ($wt)"
    case "$cmd" in
    iso)  make -C "$wt" iso && echo "ISO: $wt/build/vukang-os.iso" ;;
    *)    make -C "$wt" "$cmd" ;;
    esac
    ;;
clean)
    if [ -n "${2:-}" ]; then
        [ -d "$DIR/$2" ] && git worktree remove --force "$DIR/$2"
    else
        for wt in "$DIR"/*/; do
            [ -d "$wt" ] && git worktree remove --force "$wt"
        done
        rm -rf "$DIR"
    fi
    git worktree prune
    echo "Imefutwa."
    ;;
*)
    sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
    ;;
esac
