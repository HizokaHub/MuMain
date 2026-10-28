#include "stdafx.h"
#include "RuneWizardSkills.h"

namespace Character::RuneWizard::Skills
{
    bool IsRuneSkill(int skill)
    {
        return skill >= AT_SKILL_RUNE_MAGIC_ARROW && skill <= AT_SKILL_RUNE_PHRASE;
    }

    bool IsSelfBuff(int skill)
    {
        return skill == AT_SKILL_RUNE_BURST || skill == AT_SKILL_RUNE_HASTE;
    }

    bool IsAreaSkill(int skill)
    {
        switch (skill)
        {
        case AT_SKILL_RUNE_PLASMA_BALL:
        case AT_SKILL_RUNE_LIGHTNING_STORM:
        case AT_SKILL_RUNE_RUNIC_SPEAR:
        case AT_SKILL_RUNE_PHRASE:
            return true;
        default:
            return false;
        }
    }

    int VisualSkillOf(int skill)
    {
        switch (skill)
        {
        case AT_SKILL_RUNE_MAGIC_ARROW: return AT_SKILL_ENERGYBALL;
        case AT_SKILL_RUNE_PLASMA_BALL: return AT_SKILL_STORM;
        case AT_SKILL_RUNE_LIGHTNING_STORM: return AT_SKILL_ICE_STORM;
        case AT_SKILL_RUNE_BURST: return AT_SKILL_EXPANSION_OF_WIZARDRY;
        case AT_SKILL_RUNE_HASTE: return AT_SKILL_ATTACK;
        case AT_SKILL_RUNE_RUNIC_SPEAR: return AT_SKILL_BLAST;
        case AT_SKILL_RUNE_PHRASE: return AT_SKILL_EVIL_SPIRIT;
        default: return skill;
        }
    }
}
