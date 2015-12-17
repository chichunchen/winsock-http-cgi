#ifndef __REQUEST_H
#define __REQUEST_H

#include <stdio.h>

#define REQUEST_MAX_NUM         5
#define REQUEST_HOST_SIZE       128
#define REQUEST_PORT_SIZE       64
#define REQUEST_FILENAME_SIZE   1000

typedef struct {
	char *ip;
	char *port;
	char *filename;
	int socket;
	FILE *fp;
} Request;

#endif