#include "stdafx.h"
#include "World/Moba/MobaMenu.h"

#include "Network/Server/WSclient.h"  // SendMobaNpcMenuSelect, SendMobaJoinCode, SendMobaCloseNpcDialog

#include <cwctype>

namespace World::Moba::Menu
{

namespace
{
    State s_state;

    void Close()
    {
        s_state = State{};
    }

    bool IsCodeCharacter(wchar_t character)
    {
        return (character >= L'A' && character <= L'Z') || (character >= L'0' && character <= L'9');
    }
}

void Open(unsigned char menuId, std::wstring title, std::wstring text, const std::vector<std::wstring>& options)
{
    s_state = State{};
    s_state.mode = Mode::Options;
    s_state.menuId = menuId;
    s_state.title = std::move(title);
    s_state.text = std::move(text);
    for (const std::wstring& option : options)
    {
        const bool asksCode = !option.empty() && option.front() == CODE_OPTION_MARK;
        s_state.options.push_back(asksCode ? option.substr(1) : option);
        s_state.asksCode.push_back(asksCode);
    }
}

const State& Get()
{
    return s_state;
}

void Choose(int option)
{
    if (s_state.mode != Mode::Options || option < 0 || option >= static_cast<int>(s_state.options.size()))
        return;

    if (s_state.asksCode[option])
    {
        s_state.mode = Mode::Code;
        s_state.code.clear();
        return;
    }

    // The server answers with the next window or closes the dialog itself: it must not be told it was closed.
    SendMobaNpcMenuSelect(s_state.menuId, static_cast<unsigned char>(option));
    Close();
}

void TypeCode(wchar_t character)
{
    character = static_cast<wchar_t>(std::towupper(character));
    if (s_state.mode != Mode::Code || !IsCodeCharacter(character) || static_cast<int>(s_state.code.size()) >= MAX_CODE_LENGTH)
        return;

    s_state.code.push_back(character);
}

void EraseCode()
{
    if (s_state.mode == Mode::Code && !s_state.code.empty())
        s_state.code.pop_back();
}

void SubmitCode()
{
    if (s_state.mode != Mode::Code || s_state.code.empty())
        return;

    SendMobaJoinCode(s_state.code);
    Close();
}

void BackToOptions()
{
    if (s_state.mode == Mode::Code)
        s_state.mode = Mode::Options;
}

void CloseByPlayer()
{
    if (s_state.mode == Mode::Hidden)
        return;

    SendMobaCloseNpcDialog();
    Close();
}

}
