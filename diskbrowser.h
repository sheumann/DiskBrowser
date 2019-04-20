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

/* Disks list control */
extern CtlRecHndl disksListHandle;

struct diskListEntry {
    char *memPtr;
    Byte memFlag;
    char *idPtr;
    char *extPtr;
};

/* Maximum number of entries in list */
#define DISK_LIST_MAX_LENGTH (int)(0xFFFF/sizeof(struct diskListEntry))

/* How many results to fetch at a time */
#define PAGE_SIZE 50

#define MAX_PAGES ((DISK_LIST_MAX_LENGTH + PAGE_SIZE - 1) / PAGE_SIZE)

/* List of disks, used for list control & mounting disks */
extern struct diskListEntry *diskList;

/* String for "More Results" (used to identify that list entry) */
extern char moreResultsString[];

/* Do we want to open a window with disk contents? Counts down until ready. */
extern int wantToOpenWindow;

/* JSON for current disk list (needs to be kept in memory while it's shown) */
extern json_value *json[MAX_PAGES];

/* User ID of this program */
extern Word myUserID;

#endif
