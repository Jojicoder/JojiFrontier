#include "jf/core/UnitClass.hpp"

namespace jf {

std::string toString(UnitClass unitClass) {
    switch (unitClass) {
        case UnitClass::Lord:        return "Lord / ロード";
        case UnitClass::ArmorKnight: return "Armor Knight / 重装騎士";
        case UnitClass::Archer:      return "Archer / アーチャー";
        case UnitClass::Mage:        return "Mage / 魔道士";
        case UnitClass::Bandit:      return "Bandit / 盗賊";
        case UnitClass::Soldier:     return "Soldier / 兵士";
    }
    return "Unknown";
}

} // namespace jf
