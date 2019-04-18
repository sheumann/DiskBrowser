#ifndef BROWSERUTIL_H
#define BROWSERUTIL_H

void ShowErrorAlert(enum NetDiskError error, int alertNumber);
enum NetDiskError ReadJSONFromURL(char *url, json_value** jsonResult);

#endif
