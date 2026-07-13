#!/bin/sh
set -eu

ROOT=${JOJI_WORLD_BIBLE_DIR:-"$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)/JojiWorldBible"}

check_file() {
    file=$1
    if [ ! -f "$file" ]; then
        echo "Missing World Bible source: $file" >&2
        exit 1
    fi
}

check_file "$ROOT/AGENTS.md"
check_file "$ROOT/README.md"
check_file "$ROOT/CANON_POLICY.md"
check_file "$ROOT/military.md"
check_file "$ROOT/naming.md"
check_file "$ROOT/docs/world/embermarch.md"
check_file "$ROOT/docs/story/war_of_rivermark/class_tree_ja.md"

echo "World Bible sources available at: $ROOT"
