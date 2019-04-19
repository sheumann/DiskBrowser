/*********************************************************************
 * Disk browser window handling
 *********************************************************************/

#ifdef __ORCAC__
#pragma noroot
#endif

#include <control.h>
#include <desk.h>
#include <list.h>
#include <loader.h>
#include <quickdraw.h>
#include <resources.h>
#include <window.h>
#include <gsos.h>

#include <orca.h>
#include <string.h>

#include "json.h"

#include "browserwindow.h"
#include "diskbrowser.h"
#include "browserevents.h"
#include "disksearch.h"

/* Rectangles outlining the buttons in the style of "default" buttons */
static Rect searchRect = {8, 305, 26, 414};
static Rect mountRect = {150, 301, 168, 414};

static char showMoreString[] = "\pShow More";
static char mountDiskString[] = "\pMount Disk";

/* Has the resource file been opened? (True while window is displayed.) */
static Boolean resourceFileOpened;

/* ID of our resource file */
static Word resourceFileID;

/* Record about our system window, to support processing by Desk Manager. */
/* See Prog. Ref. for System 6.0, page 20. */
static NDASysWindRecord sysWindRecord;

/* Is the search button (as opposed to mount) the current default? */
static Boolean defaultButtonIsSearch;

/* Search and mount button controls */
static CtlRecHndl mountButtonHandle;
static CtlRecHndl searchButtonHandle;

/* Target control ID last time we checked */
static unsigned long lastTargetCtlID = 0;

/* Is "More Results" selected in the disk list? */
static boolean moreResultsSelected = false;


static void DrawContents(void);
static void DrawButtonOutlines(boolean justChanged);


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

    window = NewWindow2(NULL, 0, DrawContents, NULL,
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
    mountButtonHandle = GetCtlHandleFromID(window, mountDiskButton);
    searchButtonHandle = GetCtlHandleFromID(window, searchButton);
    
    HiliteControl(inactiveHilite, disksListHandle);
    HiliteControl(inactiveHilite, mountButtonHandle);

    lastTargetCtlID = 0;
    defaultButtonIsSearch = true;
    
    wantToOpenWindow = 0;
    
    moreResultsSelected = false;
    
    InitEventState();
    
    diskList = malloc(DISK_LIST_MAX_LENGTH * sizeof(*diskList));
    if (diskList == NULL) {
        CloseWindow(window);
        windowOpened = false;
        window = NULL;
        goto cleanup;
    }

cleanup:
    if (resourceFileOpened && !windowOpened) {
        CloseResourceFile(resourceFileID);
        resourceFileOpened = false;
    }
    
    SetCurResourceApp(origResourceApp);
}


#pragma databank 1
void CloseBrowserWindow(void) {
    if (windowOpened) {
        CloseWindow(window);
        windowOpened = false;
        window = NULL;
    }

    if (resourceFileOpened) {
        CloseResourceFile(resourceFileID);
        resourceFileOpened = false;
    }
    
    free(diskList);

    FreeJSON();
}
#pragma databank 0


#pragma databank 1
static void DrawContents(void) {
    Word origResourceApp = GetCurResourceApp();
    SetCurResourceApp(myUserID);

    PenNormal();                    /* use a "normal" pen */
    DrawControls(GetPort());        /* draw controls in window */
    
    DrawButtonOutlines(false);
    
    SetCurResourceApp(origResourceApp);
}
#pragma databank 0


/* Draw outlines of buttons as default or not, as appropriate. */
static void DrawButtonOutlines(boolean justChanged) {
    PenNormal();
    SetPenSize(3, 1);
    if (defaultButtonIsSearch) {
        FrameRRect(&searchRect, 36, 12);
        if (justChanged) {
            SetSolidPenPat(white);
            FrameRRect(&mountRect, 36, 12);
        }
    } else {
        FrameRRect(&mountRect, 36, 12);
        if (justChanged) {
            SetSolidPenPat(white);
            FrameRRect(&searchRect, 36, 12);
        }
    }
}

/* Update state of controls based on user input */
void UpdateControlState(void) {
    unsigned long targetCtlID = GetCtlID(FindTargetCtl());
    if (targetCtlID != lastTargetCtlID) {
        /* Make appropriate button respond to the "return" key */
        lastTargetCtlID = targetCtlID;
        if (targetCtlID == searchLine) {
            SetCtlMoreFlags(GetCtlMoreFlags(mountButtonHandle) & 0x1FFF,
                            mountButtonHandle);
            SetCtlMoreFlags(GetCtlMoreFlags(searchButtonHandle) | fCtlWantEvents,
                            searchButtonHandle);
            defaultButtonIsSearch = true;
        } else if (targetCtlID == disksList) {
            SetCtlMoreFlags(GetCtlMoreFlags(searchButtonHandle) & 0x1FFF,
                            searchButtonHandle);
            SetCtlMoreFlags(GetCtlMoreFlags(mountButtonHandle) | fCtlWantEvents,
                            mountButtonHandle);
            defaultButtonIsSearch = false;
        }
        DrawButtonOutlines(true);
    }

    /* Only allow "Mount Disk" to be clicked if there is a disk selected */
    unsigned currentSelection = NextMember2(0, (Handle)disksListHandle);
    if (currentSelection != 0) {
        if (diskList[currentSelection-1].memPtr == moreResultsString) {
            if (!moreResultsSelected) {
                SetCtlMoreFlags(GetCtlMoreFlags(mountButtonHandle) & 0xFFFC, 
                                mountButtonHandle);
                SetCtlTitle(showMoreString, (Handle)mountButtonHandle);
                moreResultsSelected = true;
            }
        } else {
            if (moreResultsSelected) {
                SetCtlMoreFlags(GetCtlMoreFlags(mountButtonHandle) & 0xFFFC, 
                                mountButtonHandle);
                SetCtlTitle(mountDiskString, (Handle)mountButtonHandle);
                moreResultsSelected = false;
            }
        }
        HiliteControl(noHilite, mountButtonHandle);
    } else {
        HiliteControl(inactiveHilite, mountButtonHandle);
    }
}
