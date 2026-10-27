#pragma once

// Skills of the rune wizard (imported from Season 21): numbers AT_SKILL_RUNE_MAGIC_ARROW .. AT_SKILL_RUNE_PHRASE.
namespace Character::RuneWizard::Skills
{
    // Whether the skill is one of the rune wizard's own.
    bool IsRuneSkill(int skill);

    // Whether the skill is cast on a place of the ground (it travels the area packet) and not on a target.
    bool IsAreaSkill(int skill);

    // The skill whose cast pose and effects an own skill borrows while it has none of its own; any other skill is returned as it is.
    int VisualSkillOf(int skill);
}
