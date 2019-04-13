#ifndef SESSION_H
#define SESSION_H

#include <types.h>

typedef struct Session {
    /* Marinetti TCP connection status */
    Word ipid;
    Boolean tcpLoggedIn;

    /* ReadTCP status */
    LongWord readCount;
    Byte *readPtr;
    LongWord lastReadTime;

    /* Marinetti error codes, both the tcperr* value and any tool error */
    Word tcperr;
    Word toolerr;

    /* HTTP request to send (if non-NULL, points to malloc'd buffer) */
    char *httpRequest;
    /* Length of HTTP request */
    LongWord httpRequestLen;
    
    /* HTTP response code */
    LongWord responseCode;

    /* IP address and TCP port of host */
    LongWord ipAddr;
    Word port;

    /* Domain name or IP address of host (p-string, but also null-terminated) */
    char hostName[257];
    /* Time (GetTick) of last DNS lookup */
    LongWord dnsTime;
    
    /* Number of redirects followed */
    Word redirectCount;
    
    /* Value reported by server in Content-Length header */
    LongWord contentLength;
} Session;


void EndNetDiskSession(Session *sess);

#endif
