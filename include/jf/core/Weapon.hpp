#pragma once

#include <string>

namespace jf {

enum class DamageType {
    Physical,
    Magical
};

struct Weapon {
    std::string id;
    std::string name;
    int might = 0;
    int minRange = 1;
    int maxRange = 1;
    DamageType damageType = DamageType::Physical;
};

} // namespace jf
