#ifndef JSONUTIL_H
#define JSONUTIL_H

#include "json.h"

#ifdef __ORCAC__
#include <types.h>
#else
#include <stdbool.h>
typedef _Bool boolean;
#endif

/*
 * Find an entry with specified name and type in a json object.
 */
json_value *findEntryInObject(json_value *obj, json_char *name, json_type type);

/*
 * Call f on each member of type member_type in a json array.
 * Stops and returns false if any call to f returns false.
 */
boolean processArray(json_value *arr, json_type member_type, boolean (*f)(json_value*));

#endif
