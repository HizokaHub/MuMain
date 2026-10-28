//*****************************************************************************
// File: CharMakeWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "CharMakeWin.h"
#include "Core/Input/Input.h"
#include "UI/Legacy/UIMng.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Audio/DSPlaySound.h"
#include "Engine/AI/ZzzAI.h"
#include "Scenes/SceneCore.h"
#include "UI/Legacy/UIControls.h"
#include "I18N/All.h"

#include "App/Platform/Windows/Local.h"
#include "CharacterManager.h"
#include "CreationFaceModels.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <iterator>
#include <string>

namespace
{
    constexpr std::array<DWORD, BTN_IMG_MAX> kJobButtonColors{
        CLRDW_BR_GRAY, CLRDW_BR_GRAY, CLRDW_WHITE, CLRDW_GRAY,
        CLRDW_BR_GRAY, CLRDW_BR_GRAY, CLRDW_WHITE, CLRDW_GRAY
    };

    constexpr std::array<int, MAX_CLASS> kClassButtonTextIds{
        20, 21, 22, 23, 24, 1687, 3150
    };

    // The rune wizard has no text in the S6 table (see CCharacterManager::GetCharacterClassText); its button comes after the 7 classes.
    constexpr int kRuneWizardButton = MAX_CLASS;
    constexpr const wchar_t* kRuneWizardDescription =
        L"The Rune Wizard casts runic magic: arrows, plasma and storms, and strengthens the whole party with its buffs.";

    // The classes in the order the buttons are listed, from top to bottom.
    constexpr std::array<CLASS_TYPE, CMW_JOB_COUNT> kJobButtonOrder{
        CLASS_WIZARD, CLASS_KNIGHT, CLASS_ELF, CLASS_SUMMONER,
        CLASS_RAGEFIGHTER, CLASS_DARK, CLASS_DARK_LORD, CLASS_RUNEWIZARD
    };

    constexpr int ButtonOf(CLASS_TYPE classType)
    {
        return classType == CLASS_RUNEWIZARD ? kRuneWizardButton : static_cast<int>(classType);
    }

    constexpr CLASS_TYPE ClassOfButton(int button)
    {
        return button == kRuneWizardButton ? CLASS_RUNEWIZARD : static_cast<CLASS_TYPE>(button);
    }

    constexpr std::size_t kMinCharacterNameLength = 4;

    constexpr int kSummonerDescriptionTextId = 1690;
    constexpr int kRageFighterDescriptionTextId = 3152;
    constexpr int kDefaultDescriptionBase = 1705;

    constexpr int kStatLabelBaseId = 1701;
    constexpr int kStatLineSpacing = 17;
    constexpr int kStatYOffset = 10;
    constexpr int kStatValueOffset = 54;
    constexpr int kStatTextOffsetX = 22;
    constexpr int kDarkLordStatHeight = 96;
    constexpr int kDefaultStatHeight = 80;
    constexpr const wchar_t* kDarkLordLeadershipStatValue = L"25";
    constexpr int kDarkLordLeadershipTextId = 1738;

    // The list starts under the tallest stat box (the dark lord one) and has to end above the OK button.
    constexpr int kJobButtonStartY = 122;
    constexpr int kJobButtonPitch = 25;
    constexpr int kOkButtonOffsetY = 325;
    constexpr int kCancelButtonOffsetX = 400;
    constexpr int kInputSpriteOffsetY = 317;
    constexpr int kInputTextOffsetX = 78;
    constexpr int kInputTextOffsetY = 21;
    constexpr int kDescSpriteOffsetY = 355;
    constexpr int kStatSpriteOffsetY = 24;
    constexpr int kDescriptionTextOffsetX = 10;
    constexpr int kDescriptionTextOffsetY = 12;
    constexpr int kDescriptionLineSpacing = 19;

    struct ClassStats
    {
        std::array<const wchar_t*, 4> values;
    };

    // Indexed by the button (see ButtonOf()). The rune wizard starts with the stats of the dark wizard.
    constexpr std::array<ClassStats, CMW_JOB_COUNT> kClassStatTable{ {
        ClassStats{ { L"18", L"18", L"15", L"30" } }, // Dark Wizard
        ClassStats{ { L"28", L"20", L"25", L"10" } }, // Dark Knight
        ClassStats{ { L"22", L"25", L"20", L"15" } }, // Elf
        ClassStats{ { L"26", L"26", L"26", L"26" } }, // Magic Gladiator
        ClassStats{ { L"26", L"20", L"20", L"15" } }, // Dark Lord
        ClassStats{ { L"21", L"21", L"18", L"23" } }, // Summoner
        ClassStats{ { L"32", L"27", L"25", L"20" } }, // Rage Fighter
        ClassStats{ { L"18", L"18", L"15", L"30" } }, // Rune Wizard
    } };

    struct ClassRenderParameters
    {
        bool overrideAngle;
        float angleX;
        float angleY;
        float angleZ;
        float scale;
        float positionOffsetX;
        float positionOffsetZ;
    };

    constexpr ClassRenderParameters GetRenderParameters(CLASS_TYPE classType)
    {
        switch (classType)
        {
        case CLASS_KNIGHT:
            return { true, 0.0f, 0.0f, -12.0f, 6.05f, 0.0f, 0.0f };
        case CLASS_WIZARD:
            return { true, 0.0f, 0.0f, -40.0f, 5.9f, 0.0f, 0.0f };
        case CLASS_ELF:
            return { true, 8.0f, 0.0f, 5.0f, 9.1f, 4.8f, 0.0f };
        case CLASS_DARK:
            return { true, 8.0f, 0.0f, -13.0f, 6.0f, 0.0f, 1.8f };
        case CLASS_DARK_LORD:
            return { true, 8.0f, 0.0f, -18.0f, 6.0f, 0.0f, 0.0f };
        case CLASS_SUMMONER:
            return { true, 2.0f, 0.0f, 2.0f, 9.1f, 4.8f, 4.0f };
        case CLASS_RAGEFIGHTER:
            return { false, 0.0f, 0.0f, 0.0f, 6.0f, 9.8f, -7.5f };
        case CLASS_RUNEWIZARD:
            return { false, 0.0f, 0.0f, 0.0f, 6.0f, 0.0f, 0.0f };
        default:
            return { false, 0.0f, 0.0f, 0.0f, 6.0f, 0.0f, 0.0f };
        }
    }

    constexpr int ResolveDescriptionTextId(CLASS_TYPE selectedClass)
    {
        if (selectedClass == CLASS_SUMMONER)
            return kSummonerDescriptionTextId;
        if (selectedClass == CLASS_RAGEFIGHTER)
            return kRageFighterDescriptionTextId;
        return kDefaultDescriptionBase + selectedClass;
    }

    const wchar_t* ResolveDescription(CLASS_TYPE selectedClass)
    {
        if (selectedClass == CLASS_RUNEWIZARD)
            return kRuneWizardDescription;
        return I18N::Game::Lookup(ResolveDescriptionTextId(selectedClass));
    }

    const wchar_t* ResolveButtonText(int button)
    {
        if (button == kRuneWizardButton)
            return gCharacterManager.GetCharacterClassText(CLASS_RUNEWIZARD);
        return I18N::Game::Lookup(kClassButtonTextIds[button]);
    }

    // The class number the server knows: the S6 classes travel as (class << 2) + skin, the rune wizard has its own number.
    CharacterClassNumber ResolveServerClass(CLASS_TYPE classType, int skin)
    {
        if (classType == CLASS_RUNEWIZARD)
            return static_cast<CharacterClassNumber>(RuneWizard);
        return static_cast<CharacterClassNumber>((classType << 2) + skin);
    }
}

#define	CMW_OK		0
#define	CMW_CANCEL	1



extern int g_iChatInputType;
extern CUITextInputBox* g_pSingleTextInputBox;

void MoveCharacterCamera(vec3_t Origin, vec3_t Position, vec3_t Angle);

CCharMakeWin::CCharMakeWin()
{
}

CCharMakeWin::~CCharMakeWin()
{
}

void CCharMakeWin::Create()
{
    CInput& rInput = CInput::Instance();
    CWin::Create(rInput.GetScreenWidth(), rInput.GetScreenHeight());

    m_winBack.Create(454, 406, -2);

    m_asprBack[CMW_SPR_INPUT].Create(346, 38, BITMAP_LOG_IN);

    m_asprBack[CMW_SPR_STAT].Create(108, 80);

    m_asprBack[CMW_SPR_DESC].Create(454, 51);

    for (int spriteIndex = CMW_SPR_STAT; spriteIndex < CMW_SPR_MAX; ++spriteIndex)
    {
        m_asprBack[spriteIndex].SetAlpha(143);
        m_asprBack[spriteIndex].SetColor(0, 0, 0);
    }

    std::array<DWORD, BTN_IMG_MAX> jobButtonColors = kJobButtonColors;
    for (int button = 0; button < CMW_JOB_COUNT; ++button)
    {
        m_abtnJob[button].Create(108, 26, BITMAP_LOG_IN + 1, 4, 2, 1, 0, 3, 3, 3, 0);
        m_abtnJob[button].SetText(ResolveButtonText(button), jobButtonColors.data());
        CWin::RegisterButton(&m_abtnJob[button]);
    }

    for (int i = 0; i < 2; ++i)
    {
        m_aBtn[i].Create(54, 30, BITMAP_BUTTON + i, 3, 2, 1);
        CWin::RegisterButton(&m_aBtn[i]);
    }

    std::fill(&m_aszJobDesc[0][0],
        &m_aszJobDesc[0][0] + (CMW_DESC_LINE_MAX * CMW_DESC_ROW_MAX), L'\0');
    m_nDescLine = 0;

    m_nSelJob = CLASS_KNIGHT;
    m_abtnJob[m_nSelJob].SetCheck(true);

    UpdateDisplay();
}

void CCharMakeWin::PreRelease()
{
    for (int i = 0; i < CMW_SPR_MAX; ++i)
        m_asprBack[i].Release();
    m_winBack.Release();
}

void CCharMakeWin::SetPosition(int nXCoord, int nYCoord)
{
    m_winBack.SetPosition(nXCoord, nYCoord);

    const int baseX = nXCoord + 346;
    m_asprBack[CMW_SPR_STAT].SetPosition(baseX, nYCoord + kStatSpriteOffsetY);

    for (std::size_t row = 0; row < kJobButtonOrder.size(); ++row)
    {
        const int rowY = nYCoord + kJobButtonStartY + static_cast<int>(row) * kJobButtonPitch;
        m_abtnJob[ButtonOf(kJobButtonOrder[row])].SetPosition(baseX, rowY);
    }

    const int baseY = nYCoord + kOkButtonOffsetY;
    m_aBtn[CMW_OK].SetPosition(baseX, baseY);
    m_aBtn[CMW_CANCEL].SetPosition(nXCoord + kCancelButtonOffsetX, baseY);

    m_asprBack[CMW_SPR_INPUT].SetPosition(nXCoord, nYCoord + kInputSpriteOffsetY);

    if (g_iChatInputType == 1)
    {
        g_pSingleTextInputBox->SetPosition(
            int((m_asprBack[CMW_SPR_INPUT].GetXPos() + kInputTextOffsetX) / g_fScreenRate_x),
            int((m_asprBack[CMW_SPR_INPUT].GetYPos() + kInputTextOffsetY) / g_fScreenRate_y));
    }

    m_asprBack[CMW_SPR_DESC].SetPosition(nXCoord, nYCoord + kDescSpriteOffsetY);
}

void CCharMakeWin::Show(bool bShow)
{
    CWin::Show(bShow);

    int i;
    for (i = 0; i < CMW_SPR_MAX; ++i)
        m_asprBack[i].Show(bShow);

    for (i = 0; i < CMW_JOB_COUNT; ++i)
        m_abtnJob[i].Show(bShow);
    for (i = 0; i < 2; ++i)
        m_aBtn[i].Show(bShow);

    if (bShow)
    {
        InputTextWidth = 73;
        ClearInput();
        InputEnable = true;
        InputNumber = 1;
        InputTextMax[0] = MAX_USERNAME_SIZE;
        if (g_iChatInputType == 1)
        {
            g_pSingleTextInputBox->SetState(UISTATE_NORMAL);
            g_pSingleTextInputBox->SetOption(UIOPTION_NULL);
            g_pSingleTextInputBox->SetBackColor(0, 0, 0, 0);
            g_pSingleTextInputBox->SetTextLimit(10);
            g_pSingleTextInputBox->GiveFocus();
        }
    }
    else
    {
        if (g_iChatInputType == 1)
        {
            g_pSingleTextInputBox->SetText(NULL);
            g_pSingleTextInputBox->SetState(UISTATE_HIDE);
        }
    }
}

bool CCharMakeWin::CursorInWin(int nArea)
{
    if (!CWin::m_bShow)
        return false;

    switch (nArea)
    {
    case WA_MOVE:
        return false;
    }

    return CWin::CursorInWin(nArea);
}

void CCharMakeWin::UpdateDisplay()
{
    for (auto& button : m_abtnJob)
        button.SetEnable(true);

#ifdef PBG_ADD_CHARACTERCARD
    for (int i = 0; i < CLASS_CHARACTERCARD_TOTALCNT; ++i)
    {
        if (!g_CharCardEnable.bCharacterEnable[i])
            m_abtnJob[i + CLASS_DARK].SetEnable(false);
    }
#else //PBG_ADD_CHARACTERCARD
    m_abtnJob[CLASS_SUMMONER].SetEnable(true);
#endif //PBG_ADD_CHARACTERCARD

    const bool isDarkLord = (m_nSelJob == CLASS_DARK_LORD);
    m_asprBack[CMW_SPR_STAT].SetSize(0, isDarkLord ? kDarkLordStatHeight : kDefaultStatHeight, Y);

    m_nDescLine = ::SeparateTextIntoLines(
        ResolveDescription(m_nSelJob),
        m_aszJobDesc[0],
        CMW_DESC_LINE_MAX,
        CMW_DESC_ROW_MAX);

    SelectCreateCharacter();
}

void CCharMakeWin::UpdateWhileActive(double dDeltaTick)
{
    for (int button = 0; button < CMW_JOB_COUNT; ++button)
    {
        if (!m_abtnJob[button].IsClick())
            continue;

        for (auto& jobButton : m_abtnJob)
            jobButton.SetCheck(false);
        m_abtnJob[button].SetCheck(true);

        const CLASS_TYPE clickedClass = ClassOfButton(button);
        if (m_nSelJob != clickedClass)
        {
            m_nSelJob = clickedClass;
            UpdateDisplay();
        }
        break;
    }

    {
        if (m_aBtn[CMW_OK].IsClick())
        {
            RequestCreateCharacter();
        }
        else if (m_aBtn[CMW_CANCEL].IsClick())
        {
            CUIMng::Instance().HideWin(this);
        }
        else if (CInput::Instance().IsKeyDown(VK_RETURN))
        {
            ::PlayBuffer(SOUND_CLICK01);
            RequestCreateCharacter();
        }
        else if (CInput::Instance().IsKeyDown(VK_ESCAPE))
        {
            ::PlayBuffer(SOUND_CLICK01);
            CUIMng::Instance().HideWin(this);
            CUIMng::Instance().SetSysMenuWinShow(false);
        }
    }
    UpdateCreateCharacter();
}

void CCharMakeWin::RequestCreateCharacter()
{
    if (g_iChatInputType == 1)
        g_pSingleTextInputBox->GetText(InputText[0]);

    CUIMng& rUIMng = CUIMng::Instance();

    const std::wstring characterName = InputText[0];

    // todo: check with regex from server
    if (characterName.length() < kMinCharacterNameLength)
        rUIMng.PopUpMsgWin(MESSAGE_MIN_LENGTH);
    else if (::CheckName())
        rUIMng.PopUpMsgWin(MESSAGE_ID_SPACE_ERROR);
    else if (CheckSpecialText(InputText[0]))
        rUIMng.PopUpMsgWin(MESSAGE_SPECIAL_NAME);
    else
    {
        const auto classByte = ResolveServerClass(CharacterView.Class, CharacterView.Skin);
        CurrentProtocolState = REQUEST_CREATE_CHARACTER;
        SocketClient->ToGameServer()->SendCreateCharacter(InputText[0], classByte);
        //SendRequestCreateCharacter(InputText[0], CharacterView.Class, CharacterView.Skin);
        rUIMng.HideWin(this);
        rUIMng.PopUpMsgWin(MESSAGE_WAIT);
    }
}

void CCharMakeWin::RenderControls()
{
    RenderCreateCharacter();
    BeginBitmap();
    ::EnableAlphaTest();

    for (auto& sprite : m_asprBack)
    {
        sprite.Render();
    }
    CWin::RenderButtons();
    g_pRenderText->SetFont(g_hFixFont);
    g_pRenderText->SetTextColor(CLRDW_WHITE);
    g_pRenderText->SetBgColor(0);

    const auto& stats = kClassStatTable[static_cast<std::size_t>(ButtonOf(m_nSelJob))];
    const int statBaseX = m_asprBack[CMW_SPR_STAT].GetXPos() + kStatTextOffsetX;
    for (std::size_t statIndex = 0; statIndex < stats.values.size(); ++statIndex)
    {
        const int statScreenY = int(
            (m_asprBack[CMW_SPR_STAT].GetYPos() + kStatYOffset + static_cast<int>(statIndex) * kStatLineSpacing)
            / g_fScreenRate_y);

        g_pRenderText->SetTextColor(CLRDW_ORANGE);
        g_pRenderText->RenderText(
            int((statBaseX + kStatValueOffset) / g_fScreenRate_x),
            statScreenY,
            stats.values[statIndex]);

        g_pRenderText->SetTextColor(CLRDW_WHITE);
        g_pRenderText->RenderText(
            int(statBaseX / g_fScreenRate_x),
            statScreenY,
            I18N::Game::Lookup(kStatLabelBaseId + static_cast<int>(statIndex)));
    }

    if (m_nSelJob == CLASS_DARK_LORD)
    {
        const int leadershipY = int(
            (m_asprBack[CMW_SPR_STAT].GetYPos() + kStatYOffset + 4 * kStatLineSpacing) / g_fScreenRate_y);

        g_pRenderText->SetTextColor(CLRDW_ORANGE);
        g_pRenderText->RenderText(
            int((statBaseX + kStatValueOffset) / g_fScreenRate_x),
            leadershipY,
            kDarkLordLeadershipStatValue);
        g_pRenderText->SetTextColor(CLRDW_WHITE);
        g_pRenderText->RenderText(
            int(statBaseX / g_fScreenRate_x),
            leadershipY,
            I18N::Game::Lookup(kDarkLordLeadershipTextId));
    }

    for (int lineIndex = 0; lineIndex < m_nDescLine; ++lineIndex)
    {
        g_pRenderText->RenderText(
            int((m_asprBack[CMW_SPR_DESC].GetXPos() + kDescriptionTextOffsetX) / g_fScreenRate_x),
            int((m_asprBack[CMW_SPR_DESC].GetYPos() + kDescriptionTextOffsetY + lineIndex * kDescriptionLineSpacing)
                / g_fScreenRate_y),
            m_aszJobDesc[lineIndex]);
    }

    g_pRenderText->SetFont(g_hFont);

    if (g_iChatInputType == 1)
        g_pSingleTextInputBox->Render();
    else if (g_iChatInputType == 0)
        ::RenderInputText(
            int((m_asprBack[CMW_SPR_INPUT].GetXPos() + 78) / g_fScreenRate_x),
            int((m_asprBack[CMW_SPR_INPUT].GetYPos() + 21) / g_fScreenRate_y),
            0);
}

void CCharMakeWin::SelectCreateCharacter()
{
    CharacterView.Class = m_nSelJob;
    CreateCharacterPointer(&CharacterView, Character::CreationFace::ModelOf(CharacterView.Class), 0, 0);
    CharacterView.Object.Kind = 0;
    SetAction(&CharacterView.Object, 1);
}

void CCharMakeWin::UpdateCreateCharacter()
{
    if (!CharacterAnimation(&CharacterView, &CharacterView.Object))
        SetAction(&CharacterView.Object, 0);
}

void CCharMakeWin::RenderCreateCharacter()
{
    OBJECT* o = &CharacterView.Object;
    vec3_t Position, Angle;

    Vector(1.0f, 1.0f, 1.0f, o->Light);
    Vector(10, -500.f, 48.f, Position);
    Vector(-90.f, 0.f, 0.f, Angle);
    g_Camera.FOV = 10.f;
    MoveCharacterCamera(CharacterView.Object.Position, Position, Angle);

    BeginOpengl(m_winBack.GetXPos() / g_fScreenRate_x, m_winBack.GetYPos() / g_fScreenRate_y, 410 / g_fScreenRate_x, 335 / g_fScreenRate_y);

    const ClassRenderParameters params = GetRenderParameters(CharacterView.Class);
    if (params.overrideAngle)
        Vector(params.angleX, params.angleY, params.angleZ, o->Angle);

    o->Scale = params.scale;

    if (params.positionOffsetX != 0.0f)
        CharacterView.Object.Position[0] += params.positionOffsetX;
    if (params.positionOffsetZ != 0.0f)
        CharacterView.Object.Position[2] += params.positionOffsetZ;

    RenderCharacter(&CharacterView, o);

    glViewport2(0, 0, WindowWidth, WindowHeight);

    EndOpengl();
}