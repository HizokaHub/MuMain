#pragma once

// The build number of this client. The updater (Actualizar.ps1) writes it to eclipse-build.txt together with the files of each version; the server
// refuses MOBA matches to clients older than the minimum build it asks for (see MobaClientBuild.cs on the server).
namespace World::Moba::ClientBuild
{
    // Reads the build from eclipse-build.txt (next to the executable). 0 when the file does not exist: a client that was not installed by the updater.
    unsigned int Read();

    // Tells the server the build of this client: C1 08 D5 36 build(u32 LE). Call it once the session is logged in.
    void SendToServer();
}
