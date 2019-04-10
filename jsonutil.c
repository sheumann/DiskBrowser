#include <string.h>
#include "jsonutil.h"

json_value *findEntryInObject(json_value *obj, json_char *name, json_type type) {
    if (obj == NULL || obj->type != json_object)
        return NULL;
    json_object_entry *entry = obj->u.object.values;
    for (unsigned int i = 0; i < obj->u.object.length; i++,entry++) {
        if (entry->name_length != strlen(name)) {
            continue;
        }
        if (memcmp(name, entry->name, entry->name_length) == 0) {
            if (entry->value->type == type)
                return entry->value;
            else
                return NULL;
        }
    }
    return NULL;
}


boolean processArray(json_value *arr, json_type member_type, boolean (*f)(json_value*)) {
    if (arr == NULL || arr->type != json_array)
        return false;
    for (unsigned int i = 0; i < arr->u.array.length; i++) {
        json_value *v = arr->u.array.values[i];
        if (v->type != member_type)
            continue;
        if (f(v) == false)
            return false;
    }
    return true;
}
