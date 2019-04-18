/*********************************************************************
 * Mounting disks
 *********************************************************************/

#ifdef __ORCAC__
#pragma noroot
#endif

#include <finder.h>
#include <list.h>
#include <locator.h>
#include <misctool.h>
#include <quickdraw.h>
#include <qdaux.h>

#include <stdio.h>
#include <string.h>

#include "mounturl.h"
#include "asprintf.h"
#include "json.h"
#include "jsonutil.h"

#include "diskbrowser.h"
#include "diskmount.h"
#include "browserutil.h"

/* Item ID and file extension for the disk image we want to open */
static char *wantedExt;
static char *wantedItemID;

/* Device name of mounted to disk (to give to Finder) */
static GSString32 devName;

static boolean processFile(json_value *fileObj);
static void MountFile(char *itemID, char *fileName);

void DoMount(void) {
    unsigned int itemNumber = NextMember2(0, (Handle)disksListHandle);
    char *filesURL = NULL;
    enum NetDiskError result = 0;
    json_value *filesJSON = NULL;
    
    WaitCursor();
    
    if (itemNumber == 0) {
        // shouldn't happen
        result = 1;
        goto errorReturn;
    }
    itemNumber--;
    
    if (diskList[itemNumber].idPtr == NULL) {
        result = 2;
        goto errorReturn;
    }
    
    asprintf(&filesURL,
             "http://archive.org/metadata/%s/files",
             diskList[itemNumber].idPtr);
    if (filesURL == NULL) {
        result = 3;
        goto errorReturn;
    }
    
    result = ReadJSONFromURL(filesURL, &filesJSON);
    if (result != OPERATION_SUCCESSFUL)
        goto errorReturn;

    json_value *resultJSON = findEntryInObject(filesJSON, "result", json_array);
    if (resultJSON == NULL) {
        result = NOT_EXPECTED_CONTENTS;
        goto errorReturn;
    }
    
    wantedExt = diskList[itemNumber].extPtr;
    wantedItemID = diskList[itemNumber].idPtr;
    processArray(resultJSON, json_object, processFile);

errorReturn:
    if (filesJSON != NULL)
        json_value_free(filesJSON);
    free(filesURL);
    
    InitCursor();
    if (result != 0)
        ShowErrorAlert(result, mountErrorAlert);
}

static boolean processFile(json_value *fileObj) {
    if (fileObj == NULL || fileObj->type != json_object)
        return false;
        
    json_value *name = findEntryInObject(fileObj, "name", json_string);
    if (name == NULL)
        return true;
        
    char *nameExt = strrchr(name->u.string.ptr, '.');
    if (nameExt == NULL)
        return true;
    if (strcmp(nameExt + 1, wantedExt) == 0) {
        MountFile(wantedItemID, name->u.string.ptr);
        return false;
    }
    
    return true;
}

static void MountFile(char *itemID, char *fileName) {
    static struct MountURLRec mountURLRec = {sizeof(struct MountURLRec)};

    char *fileURL = NULL;
    asprintf(&fileURL, "http://archive.org/download/%s/%s", itemID, fileName);
    if (fileURL == NULL) {
        SysBeep();
        return;
    }
    
    mountURLRec.result = NETDISK_NOT_PRESENT;
    mountURLRec.url = fileURL;
    mountURLRec.flags = flgUseCache;

    SendRequest(MountURL, sendToName|stopAfterOne, (Long)NETDISK_REQUEST_NAME,
                (Long)&mountURLRec, NULL);

    if (mountURLRec.result == OPERATION_SUCCESSFUL) {
        devName.length = snprintf(devName.text, sizeof(devName.text), ".D%u", 
                                  mountURLRec.devNum);
        wantToOpenWindow = 2;
    }

    free(fileURL);
}

/* Show Finder window for newly-mounted disk (once Finder is ready) */
void ShowDiskWindow(void) {
    static struct tellFinderOpenWindowOut tfowOut;

    /*
     * Wait till the Finder has had time to recognize the new disk
     * before opening the window for it.  Otherwise, it may crash.
     */
    if (wantToOpenWindow && --wantToOpenWindow == 0) {
        SendRequest(tellFinderOpenWindow, sendToName|stopAfterOne,
                    (Long)NAME_OF_FINDER, (Long)&devName, (Ptr)&tfowOut);
    }
}
