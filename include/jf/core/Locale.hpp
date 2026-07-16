#pragma once

#include <string>
#include <unordered_map>

namespace jf {

// docs/localization.md: data/locales/ja.json and en.json hold a flat
// Key->string table each (dotted stable IDs like "ui.button.confirm", not
// nested JSON). Both files must share the exact same Key set; loadLocales()
// treats any mismatch, parse failure, or missing file as a hard failure
// rather than silently falling back, per the spec's "欠落時に英語、内部ID、
// `?`へ黙ってフォールバックしない" rule.
bool loadLocales(const std::string& dataDir);

// Returns the localized string for `key`. A Key that loadLocales() didn't
// find in both tables returns a visibly broken "[[MISSING:key]]" marker
// instead of the English text or the raw key, so a translation gap is
// obvious on screen rather than silent.
std::string tr(const std::string& key, bool japanese);

// `{name}`-style named-argument substitution on top of tr(key, japanese).
// Kept separate from the plain lookup above since none of this Slice's
// migrated strings need it yet; exists so M4-style later Slices (battle
// messages, result screens) can adopt Locale Keys without a second Formatter
// mechanism appearing later.
std::string tr(const std::string& key, bool japanese, const std::unordered_map<std::string, std::string>& args);

// Concatenates every Japanese Value currently loaded, for feeding into the
// app's Glyph Atlas charset (src/main.cpp's loadAppFont()) - see
// docs/localization.md's "新規表示文を追加するときのチェックリスト" item 2:
// a Japanese string whose Glyphs were never registered renders as silent
// Tofu/"？？" at runtime. Loading all Locale Values through this one
// function keeps future Key additions covered automatically instead of
// requiring a manual per-string charset edit.
std::string allJapaneseGlyphText();

} // namespace jf
