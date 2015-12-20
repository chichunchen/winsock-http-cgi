#ifndef __PARSER_H
#define __PARSER_H

#include <string.h>
#include <stdlib.h>
#include "request.h"

extern Request requests[5];

int parse_query_string(char *qs);

#endif