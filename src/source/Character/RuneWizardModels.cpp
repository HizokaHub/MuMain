#include "stdafx.h"
#include "RuneWizardModels.h"
#include "Data/DataHandler/LoadData.h"
#include "Render/Models/ZzzBMD.h"

namespace Character::RuneWizard
{
    namespace
    {
        // The Season 21 files are numbered by class slot: HelmClass09.bmd is the rune wizard (ninth slot).
        constexpr int kFileClassNumber = 9;

        constexpr wchar_t kPlayerModelDir[] = L"Data\\Player\\";
        constexpr wchar_t kPlayerTextureDir[] = L"Player\\";

        // NewFace09.bmd of Season 21 (converted with tools/bmd_convert.py); the other classes use NewFace01..07.
        constexpr wchar_t kCreationFaceDir[] = L"Data\Logo\\";
        constexpr wchar_t kCreationFaceFile[] = L"NewFaceRuneWizard";
        constexpr wchar_t kCreationFaceTextureDir[] = L"Logo\\";

        // The same animation speed the creation screen models of the other classes have.
        constexpr float kCreationFacePlaySpeed = 0.3f;
        constexpr int kCreationFaceActionCount = 2;

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

    void OpenCreationFace()
    {
        gLoadData.AccessModel(MODEL_FACE_RUNEWIZARD, kCreationFaceDir, kCreationFaceFile);
        gLoadData.OpenTexture(MODEL_FACE_RUNEWIZARD, kCreationFaceTextureDir);

        BMD& face = Models[MODEL_FACE_RUNEWIZARD];
        for (int action = 0; action < kCreationFaceActionCount && action < face.NumActions; ++action)
        {
            face.Actions[action].PlaySpeed = kCreationFacePlaySpeed;
        }
    }

    void ReleaseCreationFace()
    {
        Models[MODEL_FACE_RUNEWIZARD].Release();
    }
}
