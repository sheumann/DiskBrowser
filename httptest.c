#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tcpip.h>
#include <locator.h>
#include <misctool.h>
#include <memory.h>
#include <orca.h>
#include "http.h"
#include "session.h"
#include "seturl.h"
#include "hostname.h"
#include "tcpconnection.h"
#include "readtcp.h"

int main(int argc, char **argv) {
    TLStartUp();
    
    LoadOneTool(54,0x200);
    TCPIPStartUp();
    
    if (TCPIPGetConnectStatus() == FALSE) {
        TCPIPConnect(NULL);
        if (toolerror())
            goto exit;
    }

    Session sess = {0};

    if (argc < 2)
        goto exit;

    char *url = argv[1];
    
    enum NetDiskError result = SetURL(&sess, url, TRUE, FALSE);

    if (result != OPERATION_SUCCESSFUL) {
        printf("SetURL error %i\n", (int)result);
        goto exit;
    }

    printf("Request:\n");
    printf("=========\n");
    int i;
    for (i = 0; sess.httpRequest[i] != '\0'; i++) {
        char ch = sess.httpRequest[i];
        if (ch != '\r')
            putchar(ch);
    }
    printf("=========\n");
    printf("\n");

    enum NetDiskError requestResult;
    requestResult = DoHTTPRequest(&sess);
    printf("RequestResult %i\n", requestResult);
    printf("Response code %lu\n", sess.responseCode);

    if (requestResult == OPERATION_SUCCESSFUL) {
        printf("contentLength = %lu\n", sess.contentLength);
    }
    
    if (sess.contentLength == 0)
        sess.contentLength = 0xffff;

    char *buf = malloc(sess.contentLength);
    if (buf == NULL)
        goto cleanup;
    
    InitReadTCP(&sess, sess.contentLength, buf);
    while (TryReadTCP(&sess) == rsWaiting)
        /* keep reading */ ;

    printf("Read %lu bytes\n", sess.contentLength - sess.readCount);

cleanup:
    EndTCPConnection(&sess);

exit:
    TCPIPShutDown();
    UnloadOneTool(54);
    TLShutDown();
}
