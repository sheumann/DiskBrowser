#ifndef DISKBROWSER_H
#define DISKBROWSER_H

/* Resource/control IDs */
#define winDiskBrowser          1001

#define searchLine              1002
#define searchButton            1003
#define findDisksForText        1004
#define forIIGSRadio            1005
#define forAnyAppleIIRadio      1006
#define disksList               1007
#define mountDiskButton         1008

#define searchErrorAlert        3000
#define mountErrorAlert         3001


/* The browser window */
extern GrafPortPtr window;

/* Is the window open? */
extern Boolean windowOpened;

/* User preference: IIGS disks or all Apple II disks? */
extern boolean gsDisksOnly;

/* Disks list control */
extern CtlRecHndl disksListHandle;

/* Length of disks list */
#define DISK_LIST_LENGTH 30

struct diskListEntry {
    char *memPtr;
    Byte memFlag;
    char *idPtr;
    char *extPtr;
};

/* List of disks, used for list control & mounting disks */
extern struct diskListEntry diskList[DISK_LIST_LENGTH];

/* Do we want to open a window with disk contents? Counts down until ready. */
extern int wantToOpenWindow;

/* JSON for current disk list (needs to be kept in memory while it's shown) */
extern json_value *json;

/* User ID of this program */
extern Word myUserID;

#endif
