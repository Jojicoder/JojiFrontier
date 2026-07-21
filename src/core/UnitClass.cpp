#include "jf/core/UnitClass.hpp"

namespace jf {

std::string toString(UnitClass unitClass) {
    switch (unitClass) {
        case UnitClass::MarchCaptain: return "March Captain / 行軍隊長";
        case UnitClass::VeteranGuard: return "Veteran Guard / 古参守備兵";
        case UnitClass::WatchArcher: return "Watch Archer / 監視弓兵";
        case UnitClass::FrontierScout: return "Frontier Scout / 辺境斥候";
        case UnitClass::Spearman: return "Spearman / 槍兵";
        case UnitClass::DawnChirurgeon: return "Dawn Chirurgeon / 暁の衛生兵";
        case UnitClass::HeavyInfantry: return "Heavy Infantry / 重装兵";
        case UnitClass::FrontierEngineer: return "Frontier Engineer / 辺境工兵";
        case UnitClass::MessengerCavalry: return "Messenger Cavalry / 伝令騎兵";
        case UnitClass::FrontierRanger: return "Frontier Ranger / 辺境猟兵";
        case UnitClass::BannerBearer: return "Banner Bearer / 旗手";
        case UnitClass::Bandit: return "Bandit / 盗賊";
        case UnitClass::Wolf: return "Wolf / 狼";
        case UnitClass::AshenhornBoar: return "Ashenhorn Boar / 灰角大猪";
    }
    return "Unknown";
}

bool providesFormationBonus(UnitClass unitClass) { return unitClass == UnitClass::MarchCaptain; }
bool hasZoneOfControl(UnitClass unitClass) { return unitClass == UnitClass::VeteranGuard; }
bool ignoresAshPenalty(UnitClass unitClass) { return unitClass == UnitClass::FrontierScout; }
bool hasBrace(UnitClass unitClass) { return unitClass == UnitClass::Spearman; }
bool canHeal(UnitClass unitClass) { return unitClass == UnitClass::DawnChirurgeon; }
int passiveEvasionBonus(UnitClass unitClass) { return unitClass == UnitClass::FrontierScout ? 10 : 0; }
bool hasHeavyArmor(UnitClass unitClass) { return unitClass == UnitClass::HeavyInfantry; }
bool canFieldFortify(UnitClass unitClass) { return unitClass == UnitClass::FrontierEngineer; }
bool canReMove(UnitClass unitClass) { return unitClass == UnitClass::MessengerCavalry; }
bool canSetSimpleTrap(UnitClass unitClass) { return unitClass == UnitClass::FrontierRanger; }
bool hasBannerAura(UnitClass unitClass) { return unitClass == UnitClass::BannerBearer; }

} // namespace jf
