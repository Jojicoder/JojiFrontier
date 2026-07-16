#include "jf/core/Locale.hpp"

#include <fstream>
#include <iostream>
#include <optional>

#include <nlohmann/json.hpp>

namespace jf {

namespace {

using json = nlohmann::json;

std::unordered_map<std::string, std::string> gEnglishTable;
std::unordered_map<std::string, std::string> gJapaneseTable;

// Mirrors src/data/GameData.cpp's readJsonFile(): open, parse, and on any
// failure log to std::cerr and return std::nullopt rather than throwing.
std::optional<std::unordered_map<std::string, std::string>> readLocaleFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open locale file: " << path << std::endl;
        return std::nullopt;
    }
    std::unordered_map<std::string, std::string> table;
    try {
        json parsed;
        file >> parsed;
        if (!parsed.is_object()) {
            std::cerr << "Locale file " << path << " must be a flat JSON object" << std::endl;
            return std::nullopt;
        }
        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
            if (!it.value().is_string()) {
                std::cerr << "Locale file " << path << " key " << it.key() << " must be a string" << std::endl;
                return std::nullopt;
            }
            std::string value = it.value().get<std::string>();
            if (value.empty()) {
                std::cerr << "Locale file " << path << " key " << it.key() << " has an empty value" << std::endl;
                return std::nullopt;
            }
            if (!table.emplace(it.key(), std::move(value)).second) {
                std::cerr << "Locale file " << path << " has duplicate key " << it.key() << std::endl;
                return std::nullopt;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse locale file " << path << ": " << e.what() << std::endl;
        return std::nullopt;
    }
    return table;
}

} // namespace

bool loadLocales(const std::string& dataDir) {
    auto en = readLocaleFile(dataDir + "/locales/en.json");
    auto ja = readLocaleFile(dataDir + "/locales/ja.json");
    if (!en || !ja) return false;

    if (en->size() != ja->size()) {
        std::cerr << "Locale key count mismatch: en.json has " << en->size() << ", ja.json has " << ja->size()
                  << std::endl;
        return false;
    }
    for (const auto& [key, value] : *en) {
        (void)value;
        if (ja->find(key) == ja->end()) {
            std::cerr << "Locale key present in en.json but missing in ja.json: " << key << std::endl;
            return false;
        }
    }

    gEnglishTable = std::move(*en);
    gJapaneseTable = std::move(*ja);
    return true;
}

std::string tr(const std::string& key, bool japanese) {
    const auto& table = japanese ? gJapaneseTable : gEnglishTable;
    auto it = table.find(key);
    if (it == table.end()) return "[[MISSING:" + key + "]]";
    return it->second;
}

std::string tr(const std::string& key, bool japanese, const std::unordered_map<std::string, std::string>& args) {
    std::string result = tr(key, japanese);
    for (const auto& [name, value] : args) {
        const std::string placeholder = "{" + name + "}";
        std::size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), value);
            pos += value.size();
        }
    }
    return result;
}

std::string allJapaneseGlyphText() {
    std::string text;
    for (const auto& [key, value] : gJapaneseTable) {
        (void)key;
        text += value;
    }
    return text;
}

} // namespace jf
