#include "stdafx.h"
#include "World/Moba/MobaResults.h"

#include "Network/Server/WSclient.h"  // SendMobaReturnToTown

#include <algorithm>
#include <chrono>

namespace World::Moba::Results
{

namespace
{
    constexpr double MS_PER_SECOND = 1000.0;

    State s_state;
    double s_deadlineMs = 0.0;

    double NowMs()
    {
        using namespace std::chrono;
        return static_cast<double>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }
}

void Show(State state, int secondsToReturn)
{
    s_state = std::move(state);
    s_state.visible = true;
    s_deadlineMs = NowMs() + std::max(0, secondsToReturn) * MS_PER_SECOND;
}

const State& Get()
{
    return s_state;
}

double RemainingMs()
{
    return std::max(0.0, s_deadlineMs - NowMs());
}

bool IsVictory()
{
    const auto mine = std::find_if(s_state.rows.begin(), s_state.rows.end(), [](const Row& row) { return row.mine; });
    return mine != s_state.rows.end() && mine->team == s_state.winner;
}

void Hide()
{
    s_state = State{};
}

void ReturnToTown()
{
    if (!s_state.visible)
        return;

    SendMobaReturnToTown();
    Hide();
}

}
