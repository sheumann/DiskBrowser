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
#define previousPageButton 1008
#define pageText 1009
#define pageNumberLine 1010
#define ofPagesText 1011
#define nextPageButton 1012
#define mountDiskButton 1013

Word resourceFileID;

/* ID of our menu item in the Finder (or 0 if we're not active) */
Word menuItemID = 0;

Word myUserID;

GrafPortPtr window;

Boolean resourceFileOpened, windowOpened;

/* User preference */
boolean gsDisksOnly = true;

char queryBuf[257];
#define queryString (queryBuf + 1)

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
    if ((id == searchLine) || (id == pageNumberLine)) {
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
    return ((id == searchLine) || (id == pageNumberLine));
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

/* Do a search */
void DoSearch(void) {
    char *searchURL = NULL;
    int urlLength = 0;
    enum NetDiskError result;

    WaitCursor();

    GetLETextByID(window, searchLine, (StringPtr)&queryBuf);

    for (int i = 0; i < 2; i++) {
        urlLength = snprintf(searchURL, urlLength, 
                             "http://archive.org/advancedsearch.php?"
                             "q=emulator%%3A%s%%20%s"
                             "&fl%%5B%%5D=identifier&fl%%5B%%5D=title"
                             "&fl%%5B%%5D=emulator_ext"
                             "&rows=%i&page=%i&output=json", 
                             gsDisksOnly ? "apple2gs" : "apple2*", 
                             queryString,
                             DISK_LIST_LENGTH,
                             pageNum);
        if (urlLength <= 0)
            goto errorReturn;
        if (i == 0) {
            searchURL = malloc(urlLength);
            if (searchURL == NULL)
                goto errorReturn;
        }
    }

    result = SetURL(&sess, searchURL, FALSE, FALSE);
    //TODO enable this once we have real code to build the URL
    //if (result != OPERATION_SUCCESSFUL)
    //    goto errorReturn;

    result = DoHTTPRequest(&sess);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    /* Limit content to <64k, to avoid any problems with 16-bit computations */
    if (sess.contentLength == 0 || sess.contentLength > 0xffff)
        sess.contentLength = 0xffff;

    free(netBuf);
    netBuf = malloc(sess.contentLength + 1);
    if (netBuf == NULL)
        goto errorReturn;

    InitReadTCP(&sess, sess.contentLength, netBuf);
    while (TryReadTCP(&sess) == rsWaiting)
        /* keep reading */ ;
    sess.contentLength -= sess.readCount;
    *(netBuf + (sess.contentLength)) = 0;

    if (json)
        json_value_free(json);
    json = json_parse(netBuf, sess.contentLength);
    if (json == NULL)
        goto errorReturn;

    json_value *response = findEntryInObject(json, "response", json_object);
    if (response == NULL)
        goto errorReturn;

    diskListPos = 0;
    json_value *docs = findEntryInObject(response, "docs", json_array);
    processArray(docs, json_object, processDoc);
    
    for (int i = 0; i < DISK_LIST_LENGTH; i++) {
        diskList[i].memFlag = 0;
    }

    /* Update state of controls once disk list is available */
    CtlRecHndl disksListHandle = GetCtlHandleFromID(window, disksList);
    HiliteControl(noHilite, disksListHandle);
    NewList2(NULL, 1, (Ref) diskList, refIsPointer, 
             diskListPos, (Handle)disksListHandle);

    SetCtlMoreFlags(
        GetCtlMoreFlags(disksListHandle) | fCtlCanBeTarget | fCtlWantEvents,
        disksListHandle);
    HiliteControl(noHilite, GetCtlHandleFromID(window, mountDiskButton));
    
    ShowControl(GetCtlHandleFromID(window, previousPageButton));
    ShowControl(GetCtlHandleFromID(window, pageText));
    ShowControl(GetCtlHandleFromID(window, pageNumberLine));
    ShowControl(GetCtlHandleFromID(window, ofPagesText));
    ShowControl(GetCtlHandleFromID(window, nextPageButton));
    
    free(searchURL);
    EndTCPConnection(&sess);
    InitCursor();
    return;

errorReturn:
    free(searchURL);
    EndTCPConnection(&sess);
    InitCursor();
    // TODO show error message
    SysBeep();
}

/* Handle an event after TaskMasterDA processing */
void HandleEvent(int eventCode, WmTaskRec *taskRec) {
    switch (eventCode) {
    case wInControl:
        switch (taskRec->wmTaskData4) {
        case searchButton:
            DoSearch();
            break;
        
        case forIIGSRadio:
            gsDisksOnly = true;
            break;
        case forAnyAppleIIRadio:
            gsDisksOnly = false;
            break;
        
        case previousPageButton:
            //TODO
            break;
        case nextPageButton:
            //TODO
            break;
        
        case mountDiskButton:
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
    
    HiliteControl(inactiveHilite, GetCtlHandleFromID(window, disksList));
    HiliteControl(inactiveHilite, GetCtlHandleFromID(window, mountDiskButton));
    
    HideControl(GetCtlHandleFromID(window, previousPageButton));
    HideControl(GetCtlHandleFromID(window, pageText));
    HideControl(GetCtlHandleFromID(window, pageNumberLine));
    HideControl(GetCtlHandleFromID(window, ofPagesText));
    HideControl(GetCtlHandleFromID(window, nextPageButton));

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
