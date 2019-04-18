/*********************************************************************
 * Event handling
 *********************************************************************/

#ifdef __ORCAC__
#pragma noroot
#endif

#include <control.h>
#include <desk.h>
#include <event.h>
#include <lineedit.h>
#include <quickdraw.h>
#include <window.h>

#include <string.h>

#include "json.h"

#include "diskbrowser.h"
#include "browserevents.h"
#include "browserwindow.h"
#include "disksearch.h"
#include "diskmount.h"

static void HandleEvent(int eventCode, WmTaskRec *taskRec);
static boolean DoLEEdit (int editAction);

/* NDA-style action routine for our window */
#pragma databank 1
static int ActionProc(EventRecord *eventRec, int actionCode) {
    static WmTaskRec taskRec;
    int handledAction = 0;

    if (!windowOpened)
        return 0;

    GrafPortPtr port = GetPort();
    SetPort(window);

    switch (actionCode) {
    case eventAction:
        /* Copy basic event rec & use our own wmTaskMask, as per IIgs TN 84 */
        memset(&taskRec, sizeof(taskRec), 0);
        memmove(&taskRec, eventRec, 16);
        taskRec.wmTaskMask = 0x1F7FFF; /* everything except tmInfo */
        
        HandleEvent(TaskMasterDA(0, &taskRec), &taskRec);
        UpdateControlState();
        break;

    case cursorAction:
        break;
        
    case cutAction:
    case copyAction:
    case pasteAction:
    case clearAction:
        handledAction = DoLEEdit(actionCode);
        break;
    }

    SetPort(port);

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

/* Handle an event after TaskMasterDA processing */
static void HandleEvent(int eventCode, WmTaskRec *taskRec) {
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

        case mountDiskButton:
            DoMount();
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

static boolean DoLEEdit (int editAction) {
    CtlRecHndl ctl;         /* target control handle */
    unsigned long id;       /* control ID */
    GrafPortPtr port;       /* caller's GrafPort */

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
    return (id == searchLine);
}
