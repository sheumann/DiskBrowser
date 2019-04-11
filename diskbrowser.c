#pragma rtl

#include <string.h>
#include <locator.h>
#include <menu.h>
#include <resources.h>
#include <loader.h>
#include <misctool.h>
#include <memory.h>
#include <control.h>
#include <desk.h>
#include <gsos.h>
#include <orca.h>
#include <finder.h>

static char menuTitle[] = "\pArchive.org Disk Browser\xC9";
static char windowTitle[] = "\p  Archive.org Disk Browser  ";

char diskbrowserRequestName[] = "\pSTH~DiskBrowser~";
char finderRequestName[] = "\pApple~Finder~";

#define winDiskBrowser 1001

Word resourceFileID;

/* ID of our menu item in the Finder (or 0 if we're not active) */
Word menuItemID = 0;

Word myUserID;

GrafPortPtr window;

Boolean resourceFileOpened, windowOpened;

/* Record about our system window, to support processing by Desk Manager. */
/* See Prog. Ref. for System 6.0, page 20. */
static NDASysWindRecord sysWindRecord;

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
    }

    if (resourceFileOpened && !windowOpened) {
        CloseResourceFile(resourceFileID);
        resourceFileOpened = false;
    }
}
#pragma databank 0

/* NDA-style action routine for our window */
#pragma databank 1
int ActionProc(EventRecord *eventRec, int actionCode) {
    static WmTaskRec taskRec;
    int handledEvent = 0;

    switch (actionCode) {
    case eventAction:
        /* Copy basic event rec & use our own wmTaskMask, as per IIgs TN 84 */
        memset(&taskRec, sizeof(taskRec), 0);
        memmove(&taskRec, eventRec, 16);
        taskRec.wmTaskMask = 0x1F7FFF; /* everything except tmInfo */
        
        TaskMasterDA(0, &taskRec);
        break;

    case cursorAction:
        break;
        
    case cutAction: // TODO
        break;
    case copyAction:
        break;
    case pasteAction:
        break;
    case clearAction:
        break;
    }

    return handledEvent;
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

cleanup:
    if (resourceFileOpened && !windowOpened) {
        CloseResourceFile(resourceFileID);
        resourceFileOpened = false;
    }
    
    SetCurResourceApp(origResourceApp);
}

void DoGoAway(void) {
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


int main(void) {
    myUserID = MMStartUp(); //userid() & 0xF0FF;
    
    Word origResourceApp = GetCurResourceApp();
    ResourceStartUp(myUserID);
    SetCurResourceApp(origResourceApp);

    AcceptRequests(diskbrowserRequestName, myUserID, &requestProc);
}
