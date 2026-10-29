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

    bool IconCell(int skill, bool& inSkill2Atlas, int& column, int& row)
    {
        switch (skill)
        {
        case AT_SKILL_RUNE_MAGIC_ARROW: inSkill2Atlas = false; column = 7; row = 2; return true;
        case AT_SKILL_RUNE_PLASMA_BALL: inSkill2Atlas = false; column = 0; row = 2; return true;
        case AT_SKILL_RUNE_LIGHTNING_STORM: inSkill2Atlas = false; column = 2; row = 0; return true;
        case AT_SKILL_RUNE_BURST: inSkill2Atlas = false; column = 1; row = 5; return true;
        case AT_SKILL_RUNE_HASTE: inSkill2Atlas = true; column = 5; row = 5; return true;
        case AT_SKILL_RUNE_RUNIC_SPEAR: inSkill2Atlas = false; column = 6; row = 5; return true;
        case AT_SKILL_RUNE_PHRASE: inSkill2Atlas = true; column = 4; row = 1; return true;
        default: return false;
        }
    }

    int EffectSkillOf(int skill)
    {
        return IsRuneSkill(skill) ? skill : VisualSkillOf(skill);
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
