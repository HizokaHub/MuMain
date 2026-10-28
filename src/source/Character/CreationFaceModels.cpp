#include "stdafx.h"
#include "CreationFaceModels.h"

namespace Character::CreationFace
{
    bool IsModel(const int modelType)
    {
        if (modelType == MODEL_FACE_RUNEWIZARD)
        {
            return true;
        }

        return modelType >= MODEL_FACE && modelType < MODEL_FACE + MAX_CLASS;
    }

    int ModelOf(const CLASS_TYPE classType)
    {
        if (classType == CLASS_RUNEWIZARD)
        {
            return MODEL_FACE_RUNEWIZARD;
        }

        return MODEL_FACE + classType;
    }
}
