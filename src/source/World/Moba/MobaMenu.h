#pragma once

#include <string>
#include <vector>

// MOBA menu: the server-driven menus of the NPCs of the mode (the queue master, the private rooms, the shop categories). The
// server sends them with C2 hi lo D5 06 (title, text, options) and the player answers with the option picked (C1 06 D5 07
// menuId option). An option whose label starts with '#' is not sent when clicked: it opens a box to type a room code, and
// the code is sent with C1 len D5 1D. The menu is drawn by SEASON3B::CMobaMenuPanel; this module holds the state and talks to
// the server.
namespace World::Moba::Menu
{
    constexpr wchar_t CODE_OPTION_MARK = L'#';
    constexpr int MAX_CODE_LENGTH = 5;

    enum class Mode : unsigned char
    {
        Hidden = 0,
        Options,
        Code,
    };

    struct State
    {
        Mode mode = Mode::Hidden;
        unsigned char menuId = 0;
        std::wstring title;
        std::wstring text;
        std::vector<std::wstring> options;   // without the '#' mark
        std::vector<bool> asksCode;          // per option: it opens the code box
        std::wstring code;
    };

    // Opens (or updates) the menu with what the server sent.
    void Open(unsigned char menuId, std::wstring title, std::wstring text, const std::vector<std::wstring>& options);

    // The current state.
    const State& Get();

    // The player clicked the option: sent to the server, or the code box opens.
    void Choose(int option);

    // Types a character into the code box (letters and digits only, up to MAX_CODE_LENGTH).
    void TypeCode(wchar_t character);

    // Deletes the last character of the code.
    void EraseCode();

    // Sends the code typed and closes the menu; ignored while the code is empty.
    void SubmitCode();

    // Back from the code box to the options.
    void BackToOptions();

    // The player closes the menu without choosing: the server is told the dialog is closed.
    void CloseByPlayer();
}
