#ifndef __HTML_H
#define __HTML_H

#include <string.h>
#include <stdlib.h>

char *replace_str(const char *str, const char *old, const char *new_str);
char *wrap_html(char *s);

#endif