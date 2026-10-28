#pragma once

// Body models of the rune wizard class (imported from Season 21).
namespace Character::RuneWizard
{
    // Opens the helm, armor, pants, gloves and boots models of the three looks (see SKIN_CLASS_RUNEWIZARD*).
    void OpenBodyModels();

    // Opens the textures of the models opened by OpenBodyModels().
    void OpenBodyTextures();

    // Opens the model and the textures the character creation screen shows the class with (MODEL_FACE_RUNEWIZARD).
    void OpenCreationFace();

    // Frees the model opened by OpenCreationFace().
    void ReleaseCreationFace();
}
