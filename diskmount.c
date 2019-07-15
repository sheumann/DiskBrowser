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
#include "strcasecmp.h"

#include "diskbrowser.h"
#include "diskmount.h"
#include "browserutil.h"
#include "disksearch.h"

/* Item ID and file extension for the disk image we want to open */
static char *wantedExt;
static char *wantedItemID;

/* Device name of mounted to disk (to give to Finder) */
static GSString32 devName;

static boolean processFile(json_value *fileObj);
static void MountFile(char *itemID, char *fileName, char *fileExt);

void DoMount(void) {
    unsigned int itemNumber = NextMember2(0, (Handle)disksListHandle);
    char *filesURL = NULL;
    enum NetDiskError result = 0;
    json_value *filesJSON = NULL;
        
    if (itemNumber == 0) {
        // shouldn't happen
        return;
    }
    itemNumber--;

    if (diskList[itemNumber].memPtr == noResultsString) {
        return;
    }

    WaitCursor();
    
    if (diskList[itemNumber].memPtr == moreResultsString) {
        DoSearch(true);
        return;
    }
    
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
        MountFile(wantedItemID, name->u.string.ptr, nameExt + 1);
        return false;
    }
    
    return true;
}

static void MountFile(char *itemID, char *fileName, char *fileExt) {
    static struct MountURLRec mountURLRec = {sizeof(struct MountURLRec)};

    if (strcasecmp(fileExt, "woz") == 0 || strcasecmp(fileExt, "nib") == 0) {
        ShowErrorAlert(UNSUPPORTED_IMAGE_TYPE, mountErrorAlert);
        return;
    }

    char *fileURL = NULL;
    asprintf(&fileURL, "http://archive.org/download/%s/%s", itemID, fileName);
    if (fileURL == NULL) {
        SysBeep();
        return;
    }
    
    mountURLRec.result = NETDISK_NOT_PRESENT;
    mountURLRec.url = fileURL;
    mountURLRec.flags = flgUseCache;
    
    if (strcasecmp(fileExt, "do") == 0) {
        mountURLRec.format = formatDOSOrder;
    } else if (strcasecmp(fileExt, "po") == 0) {
        mountURLRec.format = formatRaw;
    } else if (strcasecmp(fileExt, "2mg") == 0) {
        mountURLRec.format = format2mg;
    } else {
        mountURLRec.format = formatAutoDetect;
    }

    SendRequest(MountURL, sendToName|stopAfterOne, (Long)NETDISK_REQUEST_NAME,
                (Long)&mountURLRec, NULL);

    if (mountURLRec.result == OPERATION_SUCCESSFUL) {
        devName.length = snprintf(devName.text, sizeof(devName.text), ".D%u", 
                                  mountURLRec.devNum);
        wantToOpenWindow = 2;
    } else {
        ShowErrorAlert(mountURLRec.result, mountErrorAlert);
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
