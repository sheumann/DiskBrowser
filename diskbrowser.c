/*********************************************************************
 * Disk browser main program & initialization
 *********************************************************************/

#pragma rtl

#include <finder.h>
#include <list.h>
#include <locator.h>
#include <memory.h>
#include <menu.h>
#include <resources.h>
#include <tcpip.h>

#include <orca.h>

#include "mounturl.h"
#include "json.h"

#include "diskbrowser.h"
#include "browserevents.h"
#include "browserwindow.h"
#include "diskmount.h"

/* The browser window */
GrafPortPtr window;

/* Is the window open? */
Boolean windowOpened;

/* Disks list control */
CtlRecHndl disksListHandle;

/* List of disks, used for list control & mounting disks */
struct diskListEntry *diskList;

/* Do we want to open a window with disk contents? Counts down until ready. */
int wantToOpenWindow = 0;

/* JSON for current disk list (needs to be kept in memory while it's shown) */
json_value *json[MAX_PAGES];

/* User ID of this program */
Word myUserID;

/* String for "More Results" (used to identify that list entry) */
char moreResultsString[] = "                               <More Results>";

/* String to indicate no results were found */
char noResultsString[] = "                                No Disks Found";

/***/

static char menuTitle[] = "\pArchive.org Disk Browser\xC9";

static char diskbrowserRequestName[] = "\pSTH~DiskBrowser~";

/* ID of our menu item in the Finder (or 0 if we're not active) */
static Word menuItemID = 0;


/*********************************************************************
 * Window handling and initialization
 *********************************************************************/

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
    
    tellFinderAddToExtrasOut dataOutRec = {0};

    SendRequest(tellFinderAddToExtras, sendToName|stopAfterOne,
                (Long)NAME_OF_FINDER,
                (Long)&menuItem,
                (Ptr)&dataOutRec);

    if (dataOutRec.finderResult == 0) {
        menuItemID = dataOutRec.menuItemID;
    } else {
        menuItemID = 0;
    }
}

void RemoveMenuItem(void) {
    tellFinderRemoveFromExtrasOut dataOutRec = {0};

    if (menuItemID == 0)
        return;

    SendRequest(tellFinderRemoveFromExtras, sendToName|stopAfterOne,
                (Long)NAME_OF_FINDER,
                (Long)menuItemID,
                (Ptr)&dataOutRec);
    menuItemID = 0;
}

void DoGoAway(void) {
    CloseBrowserWindow();
    RemoveMenuItem();
    AcceptRequests(NULL, myUserID, NULL);
    ResourceShutDown();
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

    case finderSaysGoodbye:
        RemoveMenuItem();
        break;

    case finderSaysExtrasChosen:
        if ((dataIn & 0x0000FFFF) == menuItemID) {
            ShowBrowserWindow();
            return 0x8000;
        }
        break;

    case finderSaysIdle:
        if (wantToOpenWindow) {
            ShowDiskWindow();
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
    static struct MountURLRec mountURLRec = {sizeof(struct MountURLRec)};
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
    myUserID = MMStartUp();
    
    Word origResourceApp = GetCurResourceApp();
    ResourceStartUp(myUserID);
    SetCurResourceApp(origResourceApp);

    /* Bail out if NetDisk or Marinetti is not present */
    if (!NetDiskPresent() || !TCPIPStatus() || toolerror()) {
        return 1;
    }

    /* Check for List Manager, in case future Finder doesn't start it. */
    if (!ListStatus() || toolerror()) {
        return 1;
    }

    AcceptRequests(diskbrowserRequestName, myUserID, &requestProc);
}
