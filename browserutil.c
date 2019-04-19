/*********************************************************************
 * Utilities for disk mounting & searching
 *********************************************************************/

#ifdef __ORCAC__
#pragma noroot
#endif

#include <quickdraw.h>
#include <window.h>

#include <stdio.h>

#include "session.h"
#include "seturl.h"
#include "http.h"
#include "readtcp.h"
#include "tcpconnection.h"
#include "json.h"
#include "netdiskerror.h"


void ShowErrorAlert(enum NetDiskError error, int alertNumber) {
    char *subs[1];

    subs[0] = ErrorString(error);

    InitCursor();
    AlertWindow(awResource+awCString+awButtonLayout,
                (Pointer)subs, alertNumber);
}

enum NetDiskError ReadJSONFromURL(char *url, json_value** jsonResult) {
    static Session sess = {0};

    enum NetDiskError result = OPERATION_SUCCESSFUL;
    char *netBuf = NULL; /* Buffer for data received from network */

    *jsonResult = NULL;

    result = SetURL(&sess, url, FALSE, FALSE);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    result = DoHTTPRequest(&sess);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    /* Limit content to <64k, to avoid any problems with 16-bit computations */
    if (sess.contentLength == 0 || sess.contentLength > 0xffff)
        sess.contentLength = 0xffff;

    netBuf = malloc(sess.contentLength + 1);
    if (netBuf == NULL) {
        result = OUT_OF_MEMORY;
        goto errorReturn;
    }

    InitReadTCP(&sess, sess.contentLength, netBuf);
    while (TryReadTCP(&sess) == rsWaiting)
        /* keep reading */ ;
    sess.contentLength -= sess.readCount;
    *(netBuf + sess.contentLength) = 0;
    if (sess.contentLength == 0) {
        result = NO_RESPONSE;
        goto errorReturn;
    }

    *jsonResult = json_parse(netBuf, sess.contentLength);
    if (*jsonResult == NULL) {
        result = JSON_PARSING_ERROR;
        goto errorReturn;
    }

errorReturn:
    free(netBuf);
    EndTCPConnection(&sess);
    return result;
}
