#!/usr/bin/env bash
# docs/localization.md「自動検証」item 6: "C++のプレイヤー表示経路に日本語・
# 英語の直書きがない". This is a lightweight stand-in for that check - it
# doesn't parse the AST, just counts `kJa*` constant *declarations* in
# main.cpp, which is exactly what M3-B's migration eliminated one batch at a
# time. Only `kJaJapaneseNative` is allowed to remain (docs/localization.md's
# own "proper noun" exception: a language picker's native-script label for
# switching TO that language is not itself a translatable string - see its
# doc comment in src/main.cpp).
#
# This does NOT catch arbitrary player-facing literals or bilingual fields in
# definition objects. It guards the legacy `kJa*` pattern eliminated by M3-B;
# Locale key-set/formatter behavior is covered separately by test_locale.cpp.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MAIN_CPP="${SCRIPT_DIR}/../src/main.cpp"

count=$(grep -c '^const std::string kJa' "$MAIN_CPP")

if [ "$count" -ne 1 ]; then
    echo "check_localization: expected exactly 1 kJa* constant (kJaJapaneseNative)," \
         "found $count in $MAIN_CPP" >&2
    grep -n '^const std::string kJa' "$MAIN_CPP" >&2
    exit 1
fi

if ! grep -q '^const std::string kJaJapaneseNative' "$MAIN_CPP"; then
    echo "check_localization: the one allowed kJa* constant should be kJaJapaneseNative," \
         "but it was not found" >&2
    exit 1
fi

echo "check_localization: OK (only kJaJapaneseNative remains, as expected)"
