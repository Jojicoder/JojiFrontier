#include <cassert>

#ifdef NDEBUG
#error "jf_locale_tests requires assertions; NDEBUG must not be defined"
#endif

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "jf/core/Locale.hpp"

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

} // namespace

int main() {
    const std::filesystem::path sourceLocales = std::filesystem::path(JF_SOURCE_DATA_DIR) / "locales";

    {
        assert(jf::loadLocales(JF_SOURCE_DATA_DIR));
        assert(jf::tr("ui.button.confirm", false) == "Confirm");
        assert(jf::tr("ui.button.confirm", true) == "決定");
        assert(jf::tr("ui.phase.player", false) == "PLAYER PHASE");
        assert(jf::tr("ui.phase.player", true) == "プレイヤーフェイズ");
    }

    {
        // Missing Key: neither table has it (never loaded), so both
        // languages must return the visible marker, never a silent
        // fallback to the other language or the raw key text.
        assert(jf::tr("ui.does_not_exist", false) == "[[MISSING:ui.does_not_exist]]");
        assert(jf::tr("ui.does_not_exist", true) == "[[MISSING:ui.does_not_exist]]");
    }

    {
        std::unordered_map<std::string, std::string> args = {{"name", "Elin"}};
        // No shipped Key uses {name} yet; this exercises the Formatter
        // directly against a Key whose raw Value happens to contain no
        // placeholder, confirming substitution is a no-op when there's
        // nothing to replace (and doesn't corrupt unrelated text).
        assert(jf::tr("ui.button.confirm", false, args) == "Confirm");
    }

    {
        // Corrupt a *copy* of the locale pair in a scratch directory (never
        // the real data/locales/*.json - if the expected-failure assert
        // below were to abort, that would skip any cleanup/restore code and
        // leave the real source file broken) and confirm loadLocales()
        // rejects a Key that's missing from one side.
        const std::filesystem::path scratchDir =
            std::filesystem::temp_directory_path() / "jf_locale_test_scratch";
        std::filesystem::remove_all(scratchDir);
        std::filesystem::create_directories(scratchDir / "locales");
        writeFile(scratchDir / "locales" / "en.json", readFile(sourceLocales / "en.json"));

        const std::string originalJa = readFile(sourceLocales / "ja.json");
        assert(originalJa.find("\"ui.button.confirm\": \"決定\",\n") != std::string::npos);
        std::string corrupted = originalJa;
        corrupted.replace(corrupted.find("\"ui.button.confirm\": \"決定\",\n"),
                           std::string("\"ui.button.confirm\": \"決定\",\n").size(), "");
        writeFile(scratchDir / "locales" / "ja.json", corrupted);
        assert(!jf::loadLocales(scratchDir.string()));

        std::filesystem::remove_all(scratchDir);
        // Restore the real tables for anything running after this block.
        assert(jf::loadLocales(JF_SOURCE_DATA_DIR));
    }

    return 0;
}
