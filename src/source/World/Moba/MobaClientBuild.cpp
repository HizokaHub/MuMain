#include "stdafx.h"
#include "World/Moba/MobaClientBuild.h"

#include "Network/Server/WSclient.h"   // SocketClient

#include <cstdio>
#include <cstdlib>

namespace World::Moba::ClientBuild
{

namespace
{
    constexpr const char* BUILD_FILE = "eclipse-build.txt";
    constexpr int BUILD_TEXT_SIZE = 32;
    constexpr int PACKET_LENGTH = 8;
    constexpr BYTE MOBA_PACKET_HEADER = 0xD5;
    constexpr BYTE SUBCODE_CLIENT_BUILD = 0x36;
    constexpr int BITS_PER_BYTE = 8;
}

unsigned int Read()
{
    FILE* file = nullptr;
    if (fopen_s(&file, BUILD_FILE, "rb") != 0 || file == nullptr)
        return 0;

    char text[BUILD_TEXT_SIZE] = {};
    fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    return static_cast<unsigned int>(strtoul(text, nullptr, 10));
}

void SendToServer()
{
    if (SocketClient == nullptr || !SocketClient->IsConnected())
        return;

    const unsigned int build = Read();
    BYTE buf[PACKET_LENGTH];
    buf[0] = 0xC1;
    buf[1] = PACKET_LENGTH;
    buf[2] = MOBA_PACKET_HEADER;
    buf[3] = SUBCODE_CLIENT_BUILD;
    for (int i = 0; i < 4; ++i)
        buf[4 + i] = static_cast<BYTE>((build >> (i * BITS_PER_BYTE)) & 0xFF);
    SocketClient->Send(buf, PACKET_LENGTH);
}

}
