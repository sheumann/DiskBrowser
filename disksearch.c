/*********************************************************************
 * Searching for disks
 *********************************************************************/

#ifdef __ORCAC__
#pragma noroot
#endif

#include <control.h>
#include <list.h>
#include <misctool.h>
#include <quickdraw.h>
#include <qdaux.h>
#include <window.h>
#include <tcpip.h>

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

/* Rectangle of the disk list control */
static Rect diskListRect = {45, 10, 147, 386};

/* Current length in disk list */
static unsigned diskListLength = 0;

/* Number of current page of results from server */
int pageNum;

static boolean processDoc(json_value *docObj);
static char *EncodeQueryString(char *queryString);

struct diskListEntry moreResultsEntry = {moreResultsString , 0, "", ""};

static void InsertDiskListEntry(struct diskListEntry *entry);

/* Do a search */
void DoSearch(boolean getMore) {
    static char queryBuf[257];
    static boolean gsDisksOnly = true; /* User preference: IIGS disks only? */

    char *searchURL = NULL;
    enum NetDiskError result = 0;

    unsigned initialDiskListLength = diskListLength;

    WaitCursor();

    TCPIPConnect(NULL);

    if (!getMore) {
        FreeJSON();
        GetLETextByID(window, searchLine, (StringPtr)&queryBuf);
        gsDisksOnly = (FindRadioButton(window, 2) == 0);
        pageNum = 0;
    } else {
        if (pageNum >= MAX_PAGES - 1) {
            InitCursor();
            SysBeep();
            return;
        }
        pageNum++;
    }

    char *queryString = EncodeQueryString(queryBuf+1);
    if (queryString == NULL) {
        result = OUT_OF_MEMORY;
        goto errorReturn;
    }

    asprintf(&searchURL,
             "http://archive.org/advancedsearch.php?"
             "q=emulator%%3A%s%%20(%s)"
             "&fl%%5B%%5D=identifier&fl%%5B%%5D=title"
             "&fl%%5B%%5D=emulator_ext"
             "&sort%%5B%%5D=titleSorter+asc"
             "&rows=%i&page=%i&output=json", 
             gsDisksOnly ? "apple2gs" : "apple2%2A", 
             queryString,
             PAGE_SIZE,
             pageNum + 1);
    if (searchURL == NULL) {
        result = URL_TOO_LONG;
        goto errorReturn;
    }
    free(queryString);
    queryString = NULL;

    if (!getMore) {
        FreeJSON();
    }
    result = ReadJSONFromURL(searchURL, &json[pageNum]);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    json_value *response = 
        findEntryInObject(json[pageNum], "response", json_object);
    if (response == NULL) {
        result = NOT_EXPECTED_CONTENTS;
        goto errorReturn;
    }

    if (!getMore) {
        diskListLength = 0;
    } else {
        diskListLength--;
    }
    json_value *docs = findEntryInObject(response, "docs", json_array);
    if (docs == NULL) {
        result = NOT_EXPECTED_CONTENTS;
        goto errorReturn;
    }
    processArray(docs, json_object, processDoc);
    
    if (!getMore) {
        diskList[0].memFlag = 0x80;
    }
    
    json_value *numFoundValue = 
        findEntryInObject(response, "numFound", json_integer);
    if (numFoundValue != NULL && numFoundValue->u.integer > diskListLength) {
        InsertDiskListEntry(&moreResultsEntry);
    }

    /* Update state of controls once disk list is available */
    HiliteControl(noHilite, disksListHandle);
    SetCtlMoreFlags(
        GetCtlMoreFlags(disksListHandle) | fCtlCanBeTarget | fCtlWantEvents,
        disksListHandle);
    
    NewList2(NULL, 1, (Ref) diskList, refIsPointer, 
             diskListLength, (Handle)disksListHandle);

    if (getMore) {
        if (diskListLength >= initialDiskListLength) {
            SelectMember2(initialDiskListLength, (Handle)disksListHandle);
            ValidRect(&diskListRect);
        }
    }

    if (diskListLength > 0) {
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
    if (diskListLength >= DISK_LIST_MAX_LENGTH)
        return false;

    if (docObj == NULL || docObj->type != json_object)
        return false;
    json_value *id = findEntryInObject(docObj, "identifier", json_string);
    json_value *title = findEntryInObject(docObj, "title", json_string);
    json_value *ext = findEntryInObject(docObj, "emulator_ext", json_string);
    if (id == NULL || title == NULL || ext == NULL)
        return true;
    diskList[diskListLength].idPtr = id->u.string.ptr;
    // TODO character set translation
    diskList[diskListLength].memPtr = title->u.string.ptr;
    diskList[diskListLength].extPtr = ext->u.string.ptr;
    diskList[diskListLength++].memFlag = 0;
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

static void InsertDiskListEntry(struct diskListEntry *entry) {
    if (diskListLength >= DISK_LIST_MAX_LENGTH)
        return;
        
    diskList[diskListLength++] = *entry;
}

void FreeJSON(void) {
    for (int i = 0; i < MAX_PAGES; i++) {
        if (json[i] != NULL) {
            json_value_free(json[i]);
            json[i] = NULL;
        }
    }
}

