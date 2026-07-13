#include "jf/core/UnitClass.hpp"

namespace jf {

std::string toString(UnitClass unitClass) {
    switch (unitClass) {
        case UnitClass::MarchCaptain: return "March Captain / 行軍隊長";
        case UnitClass::VeteranGuard: return "Veteran Guard / 古参守備兵";
        case UnitClass::WatchArcher: return "Watch Archer / 監視弓兵";
        case UnitClass::FrontierScout: return "Frontier Scout / 国境斥候";
        case UnitClass::Spearman: return "Spearman / 槍兵";
        case UnitClass::DawnChirurgeon: return "Dawn Chirurgeon / 暁の治療師";
        case UnitClass::Bandit: return "Bandit / 盗賊";
    }
    return "Unknown";
}

bool providesFormationBonus(UnitClass unitClass) { return unitClass == UnitClass::MarchCaptain; }
bool hasZoneOfControl(UnitClass unitClass) { return unitClass == UnitClass::VeteranGuard; }
bool ignoresAshPenalty(UnitClass unitClass) { return unitClass == UnitClass::FrontierScout; }
bool hasBrace(UnitClass unitClass) { return unitClass == UnitClass::Spearman; }
bool canHeal(UnitClass unitClass) { return unitClass == UnitClass::DawnChirurgeon; }

} // namespace jf
