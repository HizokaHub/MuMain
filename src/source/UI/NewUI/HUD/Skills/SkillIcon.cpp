#include "stdafx.h"
#include "UI/NewUI/HUD/Skills/SkillIcon.h"

#include "Character/RuneWizardSkills.h"
#include "UI/NewUI/HUD/NewUIMainFrameWindow.h"  // KOS_* kinds of skill atlas

using namespace SEASON3B;

namespace UI::Skills::Icon
{

void Render(int skill, float x, float y, float width, float height)
{
    float fU, fV;
    int iKindofSkill = 0;
    bool kindIsSkill2 = false;
    int iconColumn = 0;
    int iconRow = 0;

    BYTE bySkillUseType = SkillAttribute[skill].SkillUseType;
    int Skill_Icon = SkillAttribute[skill].Magic_Icon;

    if (skill >= AT_PET_COMMAND_DEFAULT && skill <= AT_PET_COMMAND_END)
    {
        fU = ((skill - AT_PET_COMMAND_DEFAULT) % 8) * width / 256.f;
        fV = ((skill - AT_PET_COMMAND_DEFAULT) / 8) * height / 256.f;
        iKindofSkill = KOS_COMMAND;
    }
    else if (skill == AT_SKILL_PLASMA_STORM_FENRIR)
    {
        fU = 4 * width / 256.f;
        fV = 0.f;
        iKindofSkill = KOS_COMMAND;
    }
    else if ((skill >= AT_SKILL_ALICE_DRAINLIFE && skill <= AT_SKILL_ALICE_THORNS))
    {
        fU = ((skill - AT_SKILL_ALICE_DRAINLIFE) % 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill >= AT_SKILL_ALICE_SLEEP && skill <= AT_SKILL_ALICE_BLIND)
    {
        fU = ((skill - AT_SKILL_ALICE_SLEEP + 4) % 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_ALICE_BERSERKER || skill == AT_SKILL_ALICE_BERSERKER_STR)
    {
        fU = 10 * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill >= AT_SKILL_ALICE_WEAKNESS && skill <= AT_SKILL_ALICE_ENERVATION)
    {
        fU = (skill - AT_SKILL_ALICE_WEAKNESS + 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill >= AT_SKILL_SUMMON_EXPLOSION && skill <= AT_SKILL_SUMMON_REQUIEM)
    {
        fU = ((skill - AT_SKILL_SUMMON_EXPLOSION + 6) % 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_SUMMON_POLLUTION)
    {
        fU = 11 * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_STRIKE_OF_DESTRUCTION)
    {
        fU = 7 * width / 256.f;
        fV = 2 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_CHAOTIC_DISEIER)
    {
        fU = 3 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_RECOVER)
    {
        fU = 9 * width / 256.f;
        fV = 2 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_MULTI_SHOT)
    {
        fU = 0 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_FLAME_STRIKE)
    {
        fU = 1 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_GIGANTIC_STORM)
    {
        fU = 2 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_LIGHTNING_SHOCK)
    {
        fU = 2 * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_LIGHTNING_SHOCK_STR)
    {
        fU = 6 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill == AT_SKILL_EXPANSION_OF_WIZARDRY)
    {
        fU = 8 * width / 256.f;
        fV = 2 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (Character::RuneWizard::Skills::IconCell(skill, kindIsSkill2, iconColumn, iconRow))
    {
        fU = iconColumn * width / 256.f;
        fV = iconRow * height / 256.f;
        iKindofSkill = kindIsSkill2 ? KOS_SKILL2 : KOS_SKILL1;
    }
    else if (bySkillUseType == 4)
    {
        fU = (width / 256.f) * (Skill_Icon % 12);
        fV = (height / 256.f) * ((Skill_Icon / 12) + 4);
        iKindofSkill = KOS_SKILL2;
    }
    else if (skill >= AT_SKILL_KILLING_BLOW)
    {
        fU = ((skill - AT_SKILL_KILLING_BLOW) % 12) * width / 256.f;
        fV = ((skill - AT_SKILL_KILLING_BLOW) / 12) * height / 256.f;
        iKindofSkill = KOS_SKILL3;
    }
    else if (skill >= AT_SKILL_SPIRAL_SLASH)
    {
        fU = ((skill - AT_SKILL_SPIRAL_SLASH) % 8) * width / 256.f;
        fV = ((skill - AT_SKILL_SPIRAL_SLASH) / 8) * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else
    {
        fU = ((skill - 1) % 8) * width / 256.f;
        fV = ((skill - 1) / 8) * height / 256.f;
        iKindofSkill = KOS_SKILL1;
    }
    int iTextureIndex = 0;
    switch (iKindofSkill)
    {
    case KOS_COMMAND:
    {
        iTextureIndex = CNewUISkillList::IMAGE_COMMAND;
    }break;
    case KOS_SKILL1:
    {
        iTextureIndex = CNewUISkillList::IMAGE_SKILL1;
    }break;
    case KOS_SKILL2:
    {
        iTextureIndex = CNewUISkillList::IMAGE_SKILL2;
    }break;
    case KOS_SKILL3:
    {
        iTextureIndex = CNewUISkillList::IMAGE_SKILL3;
    }break;
    }

    if (skill >= AT_SKILL_MASTER_BEGIN)
    {
        RenderImage(BITMAP_INTERFACE_MASTER_BEGIN + 2, x, y, width, height, (20.f / 512.f) * (Skill_Icon % 25), ((28.f / 512.f) * ((Skill_Icon / 25))), 20.f / 512.f, 28.f / 512.f);
    }
    else if (iTextureIndex != 0)
    {
        RenderBitmap(iTextureIndex, x, y, width, height, fU, fV, width / 256.f, height / 256.f);
    }
}

}
