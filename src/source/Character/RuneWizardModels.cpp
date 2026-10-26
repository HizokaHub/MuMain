#include "stdafx.h"
#include "RuneWizardModels.h"
#include "Data/DataHandler/LoadData.h"

namespace Character::RuneWizard
{
    namespace
    {
        // The Season 21 files are numbered by class slot: HelmClass09.bmd is the rune wizard (ninth slot).
        constexpr int kFileClassNumber = 9;

        constexpr wchar_t kPlayerModelDir[] = L"Data\\Player\\";
        constexpr wchar_t kPlayerTextureDir[] = L"Player\\";

        struct BodyPart
        {
            int firstModel;                 // MODEL_BODY_HELM, MODEL_BODY_ARMOR, ...
            const wchar_t* filePrefix;      // "Helm", "Armor", ...
        };

        constexpr BodyPart kBodyParts[] = {
            { MODEL_BODY_HELM, L"Helm" },
            { MODEL_BODY_ARMOR, L"Armor" },
            { MODEL_BODY_PANTS, L"Pant" },
            { MODEL_BODY_GLOVES, L"Glove" },
            { MODEL_BODY_BOOTS, L"Boot" },
        };

        struct Look
        {
            CLASS_SKIN_INDEX skin;
            const wchar_t* generationDigit; // "" for HelmClass09, "3" for HelmClass309 (the same naming as the other classes)
        };

        constexpr Look kLooks[] = {
            { SKIN_CLASS_RUNEWIZARD, L"" },
            { SKIN_CLASS_RUNEWIZARD_3RD, L"3" },
            { SKIN_CLASS_RUNEWIZARD_4TH, L"4" },
        };
    }

    void OpenBodyModels()
    {
        for (const Look& look : kLooks)
        {
            for (const BodyPart& part : kBodyParts)
            {
                wchar_t fileName[32];
                mu_swprintf(fileName, L"%lsClass%ls", part.filePrefix, look.generationDigit);
                gLoadData.AccessModel(part.firstModel + look.skin, kPlayerModelDir, fileName, kFileClassNumber);
            }
        }
    }

    void OpenBodyTextures()
    {
        for (const Look& look : kLooks)
        {
            for (const BodyPart& part : kBodyParts)
            {
                gLoadData.OpenTexture(part.firstModel + look.skin, kPlayerTextureDir);
            }
        }
    }
}
