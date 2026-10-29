#pragma once

// Skills of the rune wizard (imported from Season 21): numbers AT_SKILL_RUNE_MAGIC_ARROW .. AT_SKILL_RUNE_PHRASE.
namespace Character::RuneWizard::Skills
{
    // Whether the skill is one of the rune wizard's own.
    bool IsRuneSkill(int skill);

    // Whether the skill is a buff the rune wizard always casts on itself (Burst, Haste), whatever character is selected.
    bool IsSelfBuff(int skill);

    // Whether the skill is cast on a place of the ground (it travels the area packet) and not on a target.
    bool IsAreaSkill(int skill);

    // Where the rune wizard skill's icon is in the skill icon atlases: the atlas (newui_skill2 if inSkill2Atlas, else newui_skill) and the cell.
    // Returns false for any other skill. Until the rune wizard has icons of its own, each skill borrows a distinct cell of the S6 atlases.
    bool IconCell(int skill, bool& inSkill2Atlas, int& column, int& row);

    // The skill whose cast effect the release of the cast shows: the skill itself when it has an effect of its own
    // (every rune wizard skill), otherwise whatever VisualSkillOf returns. The cast pose still follows VisualSkillOf.
    int EffectSkillOf(int skill);

    // The skill whose cast pose and effects an own skill borrows while it has none of its own; any other skill is returned as it is.
    int VisualSkillOf(int skill);
}
