#pragma noroot

#include <stdio.h>
#include "netdiskerror.h"

static char errorBuf[20];

char *ErrorString(enum NetDiskError err) {
    switch (err) {
    case NO_DIBS_AVAILABLE:
        return "No more disks can be mounted via NetDisk.";
    case OUT_OF_MEMORY:
        return "Out of memory.";

    /* SetURL errors */
    case NAME_LOOKUP_FAILED:
        return "The archive.org server could not be found.";
    
    /* StartTCPConnection and DoHTTPRequest errors */
    case NETWORK_ERROR:
        return "A network error was encountered.";
    case NO_RESPONSE:
        return "The server did not respond to a request.";
    case INVALID_RESPONSE:
        return "The response from the server was invalid.";
    case EXCESSIVE_REDIRECTS:
        return "There were too many HTTP redirects.";
    case UNSUPPORTED_RESPONSE:
        return "An unsupported response was received from the server.";
    case UNSUPPORTED_HEADER_VALUE:
        return "An unsupported header value was received from the server.";
    case REDIRECT_ERROR:
        return "An error was encountered when trying to redirect to the "
               "location specified by the server.";
    case NOT_DESIRED_CONTENT:
        return "The server did not send the content that was expected.";
    case DIFFERENT_LENGTH:
        return "The length of the file on the server was different from what "
               "was expected.";

    /* File format errors */
    case UNSUPPORTED_2IMG_FILE:
        return "This 2mg file is not supported by NetDisk.";
    case NOT_MULTIPLE_OF_BLOCK_SIZE:
        return "The file is not a multiple of 512 bytes. It may not be a disk "
               "image file, or is not in a supported format.";

    /* JSON processing errors */
    case JSON_PARSING_ERROR:
    case NOT_EXPECTED_CONTENTS:
        return "The response from the server was invalid. "
               "It may not support your query.";
   }
    
    snprintf(errorBuf, sizeof(errorBuf), "Error code %i.", err);
    return errorBuf;
}
