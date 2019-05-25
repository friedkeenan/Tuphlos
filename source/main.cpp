#include <stdio.h>
#include <unistd.h>

#include <switch.h>

#include "mtp.hpp"

int main(int argc, char **argv) {
    //consoleInit(NULL);
    /*FILE *log = fopen("/tuphlos.log", "w");
    dup2(fileno(log), STDOUT_FILENO);*/
    socketInitializeDefault();
    nxlinkStdio();

    printf("Tuphlos: An MTP Responder for the Nintendo Switch\n");
    consoleUpdate(NULL);

    MTPResponder responder;
    responder.insertStorage(0x00010001, "sdmc", u"SD Card");

    FsFileSystem fs;
    fsOpenBisFileSystem(&fs, 30, "");
    fsdevMountDevice("user", fs);
    responder.insertStorage(0x00020001, "user", u"User");

    while (appletMainLoop()) {
        hidScanInput();

        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO); // Probably need to put this in a separate thread
        if (kDown & KEY_PLUS)
            break;

        responder.loop();
    }

    socketExit();
    //fclose(log);
    //consoleExit(NULL);
    return 0;
}