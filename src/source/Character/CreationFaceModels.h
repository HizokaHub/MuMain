#pragma once

// The character creation screen shows every class with its own "face" model (Data\Logo\NewFace*.bmd), not with the body models.
namespace Character::CreationFace
{
    // Returns true if the model type is one of the creation screen models.
    bool IsModel(int modelType);

    // Returns the creation screen model of a class.
    int ModelOf(CLASS_TYPE classType);
}
