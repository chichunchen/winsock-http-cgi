#include "parser.h"

/*-------------------------------------------------------------*/
/*--------------------- req parse helper ----------------------*/
/*-------------------------------------------------------------*/

void parse_key_value(char *token)
{
	int index = 0;
	char *value_tok,
		*saveptr,
		capital;

	/* get key */
	value_tok = strtok_s(token, "=", &saveptr);
	sscanf(value_tok, "%c%d", &capital, &index);
	index--;

	/* store value */
	if (capital == 'h') {

		value_tok = strtok_s(NULL, "\0", &saveptr);

		if (value_tok) {
			requests[index].ip = (char*)malloc(REQUEST_HOST_SIZE);
			strcpy(requests[index].ip, value_tok);
			/*OutputDebugString("host");
			OutputDebugString(requests[index].ip);*/
		}
	}
	else if (capital == 'p') {

		value_tok = strtok_s(NULL, "\0", &saveptr);

		if (value_tok) {
			requests[index].port = (char *)malloc(REQUEST_PORT_SIZE);
			strcpy(requests[index].port, value_tok);
			/*OutputDebugString("port");
			OutputDebugString(requests[index].port);*/
		}
	}
	else if (capital == 'f') {

		value_tok = strtok_s(NULL, "\0", &saveptr);

		if (value_tok) {
			requests[index].filename = (char *)malloc(REQUEST_FILENAME_SIZE);
			strcpy(requests[index].filename, value_tok);
			/*OutputDebugString("file");
			OutputDebugString(requests[index].filename);*/
		}
	}
}

int parse_query_string(char *qs)
{
	char *token, *save_ptr;

	if (!qs) return -1;

	token = strtok_s(qs, "&", &save_ptr);
	//OutputDebugString("token ");
	//OutputDebugString(token);

	int  i = 0;
	while (token) {
		parse_key_value(token);
		token = strtok_s(NULL, "&", &save_ptr);
		//OutputDebugString("token ");
		//OutputDebugString(token);

		i++;
	}

	return 0;
}