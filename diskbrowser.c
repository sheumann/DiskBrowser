#pragma rtl

#include <string.h>
#include <locator.h>
#include <menu.h>
#include <resources.h>
#include <loader.h>
#include <misctool.h>
#include <memory.h>
#include <control.h>
#include <lineedit.h>
#include <list.h>
#include <desk.h>
#include <quickdraw.h>
#include <qdaux.h>
#include <gsos.h>
#include <orca.h>
#include <finder.h>
#include <tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "session.h"
#include "mounturl.h"
#include "seturl.h"
#include "http.h"
#include "readtcp.h"
#include "tcpconnection.h"
#include "json.h"
#include "jsonutil.h"

static char menuTitle[] = "\pArchive.org Disk Browser\xC9";
static char windowTitle[] = "\p  Archive.org Disk Browser  ";

char diskbrowserRequestName[] = "\pSTH~DiskBrowser~";
char finderRequestName[] = "\pApple~Finder~";

#define winDiskBrowser 1001

#define searchLine 1002
#define searchButton 1003
#define findDisksForText 1004
#define forIIGSRadio 1005
#define forAnyAppleIIRadio 1006
#define disksList 1007
#define mountDiskButton 1008

#define searchButtonDefault (searchButton+50)
#define mountDiskButtonDefault (mountDiskButton+50)

#define searchErrorAlert 3000

Word resourceFileID;

/* ID of our menu item in the Finder (or 0 if we're not active) */
Word menuItemID = 0;

Word myUserID;

GrafPortPtr window;

CtlRecHndl disksListHandle;
CtlRecHndl mountButtonHandle, mountButtonDefaultHandle;
CtlRecHndl searchButtonHandle, searchButtonDefaultHandle;

unsigned long lastTargetCtlID = 0;

Boolean resourceFileOpened, windowOpened;

/* User preference */
boolean gsDisksOnly = true;

char queryBuf[257];

int pageNum = 0;

#define DISK_LIST_LENGTH 30

struct diskListEntry {
    char *memPtr;
    Byte memFlag;
    char *idPtr;
};

struct diskListEntry diskList[DISK_LIST_LENGTH];

int diskListPos = 0;

static struct MountURLRec mountURLRec = {sizeof(struct MountURLRec)};

Session sess = {0};

/* Record about our system window, to support processing by Desk Manager. */
/* See Prog. Ref. for System 6.0, page 20. */
static NDASysWindRecord sysWindRecord;

/* Buffer for data received from network */
char *netBuf = NULL;

json_value *json = NULL;

void InstallMenuItem(void) {
    static MenuItemTemplate menuItem = {
        /* .version = */ 0x8000, /* show dividing line */
        /* .itemID = */ 0,
        /* .itemChar = */ 0,
        /* .itemAltChar = */ 0,
        /* .itemCheck = */ 0,
        /* .itemFlag = */ refIsPointer,
        /* .itemTitleRef = */ (Long)&menuTitle
    };
    
    tellFinderAddToExtrasOut dataOutRec;

    SendRequest(tellFinderAddToExtras, sendToName|stopAfterOne,
                (Long)&finderRequestName,
                (Long)&menuItem,
                (Ptr)&dataOutRec);

    if (dataOutRec.finderResult == 0) {
        menuItemID = dataOutRec.menuItemID;
    } else {
        menuItemID = 0;
    }
}

#pragma databank 1
void DrawContents(void) {
    Word origResourceApp = GetCurResourceApp();
    SetCurResourceApp(myUserID);

    PenNormal();                    /* use a "normal" pen */
    DrawControls(GetPort());        /* draw controls in window */
    
    SetCurResourceApp(origResourceApp);
}
#pragma databank 0

#pragma databank 1
void CloseBrowserWindow(void) {
    if (windowOpened) {
        CloseWindow(window);
        windowOpened = false;
        window = NULL;
        
        /* reset state */
        gsDisksOnly = true;
    }

    if (resourceFileOpened && !windowOpened) {
        CloseResourceFile(resourceFileID);
        resourceFileOpened = false;
    }
    
    free(netBuf);
    netBuf = NULL;
    
    if (json) {
        json_value_free(json);
        json = NULL;
    }
    
    EndTCPConnection(&sess);
}
#pragma databank 0

boolean DoLEEdit (int editAction) {
    CtlRecHndl ctl;         /* target control handle */
    unsigned long id;       /* control ID */
    GrafPortPtr port;       /* caller's GrafPort */

    port = GetPort();
    SetPort(window);
    ctl = FindTargetCtl();
    id = GetCtlID(ctl);
    if (id == searchLine) {
        LEFromScrap();
        switch (editAction) {
            case cutAction: 
                LECut((LERecHndl) GetCtlTitle(ctl));          
                LEToScrap();
                break;
            case copyAction:
                LECopy((LERecHndl) GetCtlTitle(ctl));
                LEToScrap();
                break;
            case pasteAction:
                LEPaste((LERecHndl) GetCtlTitle(ctl));  
                break;
            case clearAction:
                LEDelete((LERecHndl) GetCtlTitle(ctl));     
                break;
        };
    };
    SetPort(port);
    return (id == searchLine);
}

boolean processDoc(json_value *docObj) {
    if (diskListPos >= DISK_LIST_LENGTH)
        return false;

    if (docObj == NULL || docObj->type != json_object)
        return false;
    json_value *id = findEntryInObject(docObj, "identifier", json_string);
    json_value *title = findEntryInObject(docObj, "title", json_string);
    if (id == NULL || title == NULL)
        return true;
    diskList[diskListPos].idPtr = id->u.string.ptr;
    // TODO character set translation
    diskList[diskListPos++].memPtr = title->u.string.ptr;
    return true;
}

char *EncodeQueryString(char *queryString) {
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

void ShowErrorAlert(enum NetDiskError error) {
    char numStr[6] = "";
    char *subs[1] = {numStr};
    
    snprintf(numStr, sizeof(numStr), "%u", error);

    AlertWindow(awResource+awCString+awButtonLayout,
                (Pointer)subs, searchErrorAlert);
}

/* Do a search */
void DoSearch(void) {
    char *searchURL = NULL;
    int urlLength = 0;
    enum NetDiskError result = 0;

    WaitCursor();

    GetLETextByID(window, searchLine, (StringPtr)&queryBuf);

    char *queryString = EncodeQueryString(queryBuf+1);
    if (queryString == NULL) {
        result = OUT_OF_MEMORY;
        goto errorReturn;
    }

    for (int i = 0; i < 2; i++) {
        urlLength = snprintf(searchURL, urlLength, 
                             "http://archive.org/advancedsearch.php?"
                             "q=emulator%%3A%s%%20%s"
                             "&fl%%5B%%5D=identifier&fl%%5B%%5D=title"
                             "&fl%%5B%%5D=emulator_ext"
                             "&sort%%5B%%5D=titleSorterRaw+asc"
                             "&rows=%i&page=%i&output=json", 
                             gsDisksOnly ? "apple2gs" : "apple2%2A", 
                             queryString,
                             DISK_LIST_LENGTH,
                             pageNum);
        if (urlLength <= 0) {
            result = URL_TOO_LONG;
            goto errorReturn;
        }
        if (i == 0) {
            searchURL = malloc(urlLength);
            if (searchURL == NULL) {
                result = OUT_OF_MEMORY;
                goto errorReturn;
            }
        }
    }
    free(queryString);
    queryString = NULL;

    result = SetURL(&sess, searchURL, FALSE, FALSE);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    result = DoHTTPRequest(&sess);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    /* Limit content to <64k, to avoid any problems with 16-bit computations */
    if (sess.contentLength == 0 || sess.contentLength > 0xffff)
        sess.contentLength = 0xffff;

    free(netBuf);
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


    if (json)
        json_value_free(json);
    json = json_parse(netBuf, sess.contentLength);
    if (json == NULL) {
        result = JSON_PARSING_ERROR;
        goto errorReturn;
    }

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
    EndTCPConnection(&sess);
    InitCursor();
    return;

errorReturn:
    NewList2(NULL, 1, (Ref) diskList, refIsPointer, 0, (Handle)disksListHandle);
    free(queryString);
    free(searchURL);
    EndTCPConnection(&sess);
    InitCursor();
    ShowErrorAlert(result);
}

/* Handle an event after TaskMasterDA processing */
void HandleEvent(int eventCode, WmTaskRec *taskRec) {
    switch (eventCode) {
    case wInControl:
        switch (taskRec->wmTaskData4) {
        case searchButton:
        case searchButtonDefault:
            DoSearch();
            break;
        
        case forIIGSRadio:
            gsDisksOnly = true;
            break;
        case forAnyAppleIIRadio:
            gsDisksOnly = false;
            break;

        case mountDiskButton:
        case mountDiskButtonDefault:
            // TODO
            break;
        }
        break;
    
    case keyDownEvt:
    case autoKeyEvt:
        /* Handle keyboard shortcuts for cut/copy/paste */
        if (taskRec->modifiers & appleKey) {
            switch (taskRec->message & 0x000000FF) {
            case 'x': case 'X':
                DoLEEdit(cutAction);
                break;
            case 'c': case 'C':
                DoLEEdit(copyAction);
                break;
            case 'v': case 'V':
                DoLEEdit(pasteAction);
                break;
            }
        }
        break;
    }

    unsigned long targetCtlID = GetCtlID(FindTargetCtl());
    if (targetCtlID != lastTargetCtlID) {
        /* Make button corresponding to target control the default */
        lastTargetCtlID = targetCtlID;
        if (targetCtlID == searchLine) {
            SetCtlMoreFlags(GetCtlMoreFlags(mountButtonDefaultHandle) & 0x1FFF,
                            mountButtonDefaultHandle);
            SetCtlMoreFlags(GetCtlMoreFlags(searchButtonDefaultHandle) | 0x3000,
                            searchButtonDefaultHandle);
            ShowControl(searchButtonDefaultHandle);
            HideControl(mountButtonDefaultHandle);
        } else if (targetCtlID == disksList) {
            SetCtlMoreFlags(GetCtlMoreFlags(searchButtonDefaultHandle) & 0x1FFF,
                            searchButtonDefaultHandle);
            SetCtlMoreFlags(GetCtlMoreFlags(mountButtonDefaultHandle) | 0x3000,
                            mountButtonDefaultHandle);
            ShowControl(mountButtonDefaultHandle);
            HideControl(searchButtonDefaultHandle);
        }
    }

    /* Only allow "Mount Disk" to be clicked if there is a disk selected */
    if (NextMember2(0, (Handle)disksListHandle) != 0) {
        HiliteControl(noHilite, mountButtonHandle);
        HiliteControl(noHilite, mountButtonDefaultHandle);
    } else {
        HiliteControl(inactiveHilite, mountButtonHandle);
        HiliteControl(inactiveHilite, mountButtonDefaultHandle);
    }
}

/* NDA-style action routine for our window */
#pragma databank 1
int ActionProc(EventRecord *eventRec, int actionCode) {
    static WmTaskRec taskRec;
    int handledAction = 0;

    switch (actionCode) {
    case eventAction:
        /* Copy basic event rec & use our own wmTaskMask, as per IIgs TN 84 */
        memset(&taskRec, sizeof(taskRec), 0);
        memmove(&taskRec, eventRec, 16);
        taskRec.wmTaskMask = 0x1F7FFF; /* everything except tmInfo */
        
        HandleEvent(TaskMasterDA(0, &taskRec), &taskRec);
        break;

    case cursorAction:
        break;
        
    case cutAction:
    case copyAction:
    case pasteAction:
    case clearAction:
        if (windowOpened) {
            handledAction = DoLEEdit(actionCode);
        }
        break;
    }

    return handledAction;
}
#pragma databank 0

asm void actionProcWrapper(void) {
    pha
    phy
    phx
    jsl ActionProc
    rtl
}

void ShowBrowserWindow(void) {
    if (windowOpened) {
        BringToFront(window);
        return;
    }

    Word origResourceApp = GetCurResourceApp();
    SetCurResourceApp(myUserID);

    resourceFileID = 
        OpenResourceFile(readEnable, NULL, LGetPathname2(myUserID, 1));
    if (toolerror()) {
        resourceFileID = 0;
        resourceFileOpened = false;
        goto cleanup;
    }
    resourceFileOpened = true;

    window = NewWindow2(windowTitle, 0, DrawContents, NULL,
                        refIsResource, winDiskBrowser, rWindParam1);
    if (toolerror()) {
        window = NULL;
        windowOpened = false;
        goto cleanup;
    }
    windowOpened = true;

    SetSysWindow(window);
    
    AuxWindInfoRecord *auxWindInfo = GetAuxWindInfo(window);
    if (toolerror()) {
        CloseWindow(window);
        windowOpened = false;
        window = NULL;
        goto cleanup;
    }
    
    memset(&sysWindRecord, sizeof(sysWindRecord), 0);
    sysWindRecord.closeProc = (ProcPtr)CloseBrowserWindow;
    sysWindRecord.actionProc = (ProcPtr)actionProcWrapper;
    sysWindRecord.eventMask = 0xFFFF; //0x03FF;
    sysWindRecord.memoryID = myUserID;
    auxWindInfo->NDASysWindPtr = (Ptr)&sysWindRecord;
    
    disksListHandle = GetCtlHandleFromID(window, disksList);
    mountButtonDefaultHandle = GetCtlHandleFromID(window, mountDiskButtonDefault);
    searchButtonDefaultHandle = GetCtlHandleFromID(window, searchButtonDefault);
    mountButtonHandle = GetCtlHandleFromID(window, mountDiskButton);
    searchButtonHandle = GetCtlHandleFromID(window, searchButton);
    
    HideControl(GetCtlHandleFromID(window, mountDiskButtonDefault));
    
    HiliteControl(inactiveHilite, disksListHandle);
    HiliteControl(inactiveHilite, mountButtonHandle);
    HiliteControl(inactiveHilite, mountButtonDefaultHandle);
    
    lastTargetCtlID = 0;

cleanup:
    if (resourceFileOpened && !windowOpened) {
        CloseResourceFile(resourceFileID);
        resourceFileOpened = false;
    }
    
    SetCurResourceApp(origResourceApp);
}

void DoGoAway(void) {
    CloseBrowserWindow();

    ResourceShutDown();
    
    /* TODO remove menu item, other cleanup? */
}

/*
 * Request procedure which may be called by the Finder.
 */
#pragma databank 1
#pragma toolparms 1
static pascal Word requestProc(Word reqCode, Long dataIn, void *dataOut) {
    switch (reqCode) {
    case finderSaysHello:
        InstallMenuItem();
        break;

    case finderSaysExtrasChosen:
        if ((dataIn & 0x0000FFFF) == menuItemID) {
            ShowBrowserWindow();
            return 0x8000;
        }
        break;
    
    case srqGoAway:
        DoGoAway();
        ((srqGoAwayOut*)dataOut)->resultID = myUserID;
        ((srqGoAwayOut*)dataOut)->resultFlags = 0;
        return 0x8000;
        break;
    }
    
    return 0;
}
#pragma toolparms 0
#pragma databank 0

boolean NetDiskPresent(void)
{
    mountURLRec.result = NETDISK_NOT_PRESENT;
    mountURLRec.url = "";
    mountURLRec.flags = flgUseCache;

    SendRequest(MountURL, sendToName|stopAfterOne, (Long)NETDISK_REQUEST_NAME,
                (Long)&mountURLRec, NULL);

    if (mountURLRec.result == NETDISK_NOT_PRESENT) {
        return false;
    }

    return true;
}

int main(void) {
    myUserID = MMStartUp(); //userid() & 0xF0FF;
    
    Word origResourceApp = GetCurResourceApp();
    ResourceStartUp(myUserID);
    SetCurResourceApp(origResourceApp);

    /* Bail out if NetDisk or Marinetti is not present */
    if (!NetDiskPresent() || !TCPIPStatus() || toolerror()) {
        return 1;
    }

    AcceptRequests(diskbrowserRequestName, myUserID, &requestProc);
}
