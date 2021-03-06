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
#include <list.h>
#include <quickdraw.h>
#include <window.h>

#include <string.h>

#include "json.h"

#include "diskbrowser.h"
#include "browserevents.h"
#include "browserwindow.h"
#include "disksearch.h"
#include "diskmount.h"

/* Task record used for TaskMasterDA */
static WmTaskRec taskRec;

/* Search line control */
static CtlRecHndl searchLineHandle;

static void HandleEvent(int eventCode, WmTaskRec *taskRec);
static boolean DoLEEdit (int editAction);

/* NDA-style action routine for our window */
#pragma databank 1
static int ActionProc(EventRecord *eventRec, int actionCode) {
    int handledAction = 0;

    if (!windowOpened)
        return 0;

    GrafPortPtr port = GetPort();
    SetPort(window);

    switch (actionCode) {
    case eventAction:
        /* Copy basic event rec & use our own wmTaskMask, as per IIgs TN 84 */
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
    static unsigned int lastSelection;

    switch (eventCode) {
    case wInControl:
        switch (taskRec->wmTaskData4) {
        case searchButton:
            DoSearch(false);
            break;

        case mountDiskButton:
            DoMount();
            break;
        
        case forIIGSRadio:
        case forAnyAppleIIRadio:
            if (FindTargetCtl() != searchLineHandle) {
                MakeThisCtlTarget(searchLineHandle);
            }
            break;
        
        case disksList:
            if (taskRec->what == mouseDownEvt) {
                if (taskRec->wmClickCount == 2) {
                    SubPt(&taskRec->wmLastClickPt, &taskRec->where);
                    if (taskRec->where.h >= -5 && taskRec->where.h <= 5 &&
                        taskRec->where.v >= -3 && taskRec->where.v <= 3 &&
                        NextMember2(0, (Handle)disksListHandle) == lastSelection)
                    {
                        DoMount();
                    }
                } else {
                    lastSelection = NextMember2(0, (Handle)disksListHandle);
                }
            }
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

void InitEventState(void) {
    memset(&taskRec, sizeof(taskRec), 0);
    searchLineHandle = GetCtlHandleFromID(window, searchLine);
}

