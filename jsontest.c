#pragma lint -1

#include <stdio.h>
#include <string.h>
#include "json.h"
#include "jsonutil.h"

char jsonData[] = "{\"responseHeader\":{\"status\":0,\"QTime\":10,\"params\":{\"query\":\"emulator:apple2gs\",\"qin\":\"emulator:apple2gs\",\"fields\":\"identifier,title\",\"wt\":\"json\",\"rows\":\"3\",\"json.wrf\":\"callback\",\"start\":0}},\"response\":{\"numFound\":2428,\"start\":0,\"docs\":[{\"identifier\":\"TypeWest_disk_1_no_boot\",\"title\":\"TypeWest disk 1 (no boot)\"},{\"identifier\":\"a2gs_mindshadow\",\"title\":\"Mindshadow\"},{\"identifier\":\"a2gs_Impossible_Mission_II_1989_Epyx_a\",\"title\":\"Apple IIgs: Impossible Mission II (1989)(Epyx)[a]\"}]}}";


boolean processDoc(json_value *docObj) {
    if (docObj == NULL || docObj->type != json_object)
        return false;
    json_value *id = findEntryInObject(docObj, "identifier", json_string);
    json_value *title = findEntryInObject(docObj, "title", json_string);
    if (id == NULL || title == NULL)
        return true;
    printf("Document:\n");
    printf("  id = %s\n", id->u.string.ptr);
    printf("  title = %s\n", title->u.string.ptr);
    return true;
}


int main(void) {
    json_value *value = json_parse(jsonData, sizeof(jsonData)-1);

    if (value == NULL)
        return -1;
    
    json_value *response = findEntryInObject(value, "response", json_object);
    
    json_value *numFound = findEntryInObject(response, "numFound", json_integer);
    if (numFound != NULL) {
        printf("numFound = %li\n", (long)numFound->u.integer);
    }
    
    json_value *docs = findEntryInObject(response, "docs", json_array);
    processArray(docs, json_object, processDoc);
    
    json_value_free(value);
}
