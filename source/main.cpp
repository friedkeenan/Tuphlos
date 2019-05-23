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
    printf("INSERT STORAGE\n");
    responder.insertStorage(0x00010001, "sdmc", u"SD Card");
    printf("INSERT STORAGE DONE\n");

    while (appletMainLoop()) {
        hidScanInput();

        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
        if (kDown & KEY_PLUS)
            break;

        Result rc = responder.loop();
        if (R_FAILED(rc)) {
            printf("An error occured: 0x%x\n", rc);
            consoleUpdate(NULL);
        }
    }

    socketExit();
    //fclose(log);
    //consoleExit(NULL);
    return 0;
}