/*********************************************************************
 * Searching for disks
 *********************************************************************/

#ifdef __ORCAC__
#pragma noroot
#endif

#include <control.h>
#include <list.h>
#include <quickdraw.h>
#include <qdaux.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdiskerror.h"
#include "asprintf.h"
#include "json.h"
#include "jsonutil.h"

#include "diskbrowser.h"
#include "disksearch.h"
#include "browserutil.h"

/* Current position in disk list */
static int diskListPos = 0;

static boolean processDoc(json_value *docObj);
static char *EncodeQueryString(char *queryString);

/* Do a search */
void DoSearch(void) {
    static char queryBuf[257];

    char *searchURL = NULL;
    enum NetDiskError result = 0;

    WaitCursor();

    GetLETextByID(window, searchLine, (StringPtr)&queryBuf);

    char *queryString = EncodeQueryString(queryBuf+1);
    if (queryString == NULL) {
        result = OUT_OF_MEMORY;
        goto errorReturn;
    }

    asprintf(&searchURL,
             "http://archive.org/advancedsearch.php?"
             "q=emulator%%3A%s%%20%s"
             "&fl%%5B%%5D=identifier&fl%%5B%%5D=title"
             "&fl%%5B%%5D=emulator_ext"
             "&sort%%5B%%5D=titleSorterRaw+asc"
             "&rows=%i&page=%i&output=json", 
             gsDisksOnly ? "apple2gs" : "apple2%2A", 
             queryString,
             DISK_LIST_LENGTH,
             /*pageNum*/ 0);
    if (searchURL == NULL) {
        result = URL_TOO_LONG;
        goto errorReturn;
    }
    free(queryString);
    queryString = NULL;

    if (json)
        json_value_free(json);
    result = ReadJSONFromURL(searchURL, &json);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    json_value *response = findEntryInObject(json, "response", json_object);
    if (response == NULL) {
        result = NOT_EXPECTED_CONTENTS;
        goto errorReturn;
    }

    diskListPos = 0;
    json_value *docs = findEntryInObject(response, "docs", json_array);
    if (docs == NULL) {
        result = NOT_EXPECTED_CONTENTS;
        goto errorReturn;
    }
    processArray(docs, json_object, processDoc);
    
    diskList[0].memFlag = 0x80;
    for (int i = 1; i < DISK_LIST_LENGTH; i++) {
        diskList[i].memFlag = 0;
    }

    /* Update state of controls once disk list is available */
    HiliteControl(noHilite, disksListHandle);
    SetCtlMoreFlags(
        GetCtlMoreFlags(disksListHandle) | fCtlCanBeTarget | fCtlWantEvents,
        disksListHandle);
    
    NewList2(NULL, 1, (Ref) diskList, refIsPointer, 
             diskListPos, (Handle)disksListHandle);

    if (diskListPos > 0) {
        if (FindTargetCtl() != disksListHandle) {
            MakeThisCtlTarget(disksListHandle);
            CallCtlDefProc(disksListHandle, ctlChangeTarget, 0);
        }
    }

    free(searchURL);
    InitCursor();
    return;

errorReturn:
    NewList2(NULL, 1, (Ref) diskList, refIsPointer, 0, (Handle)disksListHandle);
    free(queryString);
    free(searchURL);
    InitCursor();
    ShowErrorAlert(result, searchErrorAlert);
}

static boolean processDoc(json_value *docObj) {
    if (diskListPos >= DISK_LIST_LENGTH)
        return false;

    if (docObj == NULL || docObj->type != json_object)
        return false;
    json_value *id = findEntryInObject(docObj, "identifier", json_string);
    json_value *title = findEntryInObject(docObj, "title", json_string);
    json_value *ext = findEntryInObject(docObj, "emulator_ext", json_string);
    if (id == NULL || title == NULL || ext == NULL)
        return true;
    diskList[diskListPos].idPtr = id->u.string.ptr;
    // TODO character set translation
    diskList[diskListPos].memPtr = title->u.string.ptr;
    diskList[diskListPos++].extPtr = ext->u.string.ptr;
    return true;
}

static char *EncodeQueryString(char *queryString) {
    long sizeNeeded;

    char *encodedStr = NULL;
    sizeNeeded = strlen(queryString);
    if (sizeNeeded > 10000)
        return NULL;
    encodedStr = malloc(sizeNeeded*3 + 1);
    if (encodedStr == NULL)
        return NULL;

    char *s = encodedStr;
    char c;
    while ((c = *queryString++) != '\0') {
        if (c > 0x7F) {
            //TODO character set translation
            c = '*';
        }
        if ((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9')) {
            *s++ = c;
        } else {
            snprintf(s, 4, "%%%02X", (unsigned char)c);
            s+= 3;
        }
    }
    *s = '\0';
    return encodedStr;
}
