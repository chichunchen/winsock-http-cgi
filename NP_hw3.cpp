#include <windows.h>
#include <list>
#include <stdlib.h>
using namespace std;

#include "resource.h"
#include "request.h"

/* Define Macro */

#define DEBUG				0
#define SERVER_PORT			7799
#define WM_SOCKET_NOTIFY	(WM_USER + 1)
#define WM_SERVER_RESPONSE	7777
#define RECV_BUF_SIZE		10000

/* Function Prototypes */

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
void write_cgi_header(int connfd);
void write_html_header(int connfd);
int parse_query_string(char *qs);
void html_init(int connfd);
void html_end(int connfd);
void server_hanlder(HWND hwndEdit, HWND hwnd, SOCKET sock);
void setup_connection(HWND hwndEdit, HWND hwnd, int index);
void serve_connection();

//=================================================================
//	Global Variables
//=================================================================
list <SOCKET> Socks;
char buf[RECV_BUF_SIZE];
int  content_length;
char request_filename[100];
Request requests[5];

void WriteToSock(SOCKET sock, char *s) {

	int r;

	r = send(sock, s, strlen(s), 0);
	if (r == SOCKET_ERROR) {
		OutputDebugString("=== Error: socket send ===\r\n");
		WSACleanup();
		return;
	}

}

// HTML Client
char *replace_str(const char *str, const char *old, const char *new_str) {

	char *ret, *r;
	const char *p, *q;
	int oldlen = strlen(old);
	int count, retlen, newlen = strlen(new_str);
	int samesize = (oldlen == newlen), l;

	if (!samesize) {
		for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
			count++;
		retlen = p - str + strlen(p) + count * (newlen - oldlen);
	}
	else
		retlen = strlen(str);

	if ((ret = (char *)malloc(retlen + 1)) == NULL)
		return NULL;

	r = ret, p = str;
	while (1) {
		if (!samesize && !count--)
			break;
		if ((q = strstr(p, old)) == NULL)
			break;
		l = q - p;
		memcpy(r, p, l);
		r += l;
		memcpy(r, new_str, newlen);
		r += newlen;
		p = q + oldlen;
	}
	strcpy(r, p);

	return ret;

}

char *wrap_html(char *s) {
	fprintf(stderr, "wrapping:\n%s\n", s);
	char *r;
	r = s;
	r = replace_str(r, "&", "&amp;");
	r = replace_str(r, "\"", "&quot;");
	r = replace_str(r, "<", "&lt;");
	r = replace_str(r, ">", "&gt;");
	r = replace_str(r, "\r\n", "\n");
	r = replace_str(r, "\n", "<br />");
	return r;
}

void write_head_at(SOCKET sock, int num, char *_content) {
	char content[RECV_BUF_SIZE];
	sprintf(content, "<script>document.all['res_tr_head'].innerHTML += \"<td>%s</td>\";</script>", _content);
	WriteToSock(sock, content);
}

void write_content_at(SOCKET sock, int num, char *_content, int bold) {
	char content[RECV_BUF_SIZE];
	if (bold)
		sprintf(content, "<script>document.all('c-%d').innerHTML += \"<b>%s</b>\";</script>", num, _content);
	else
		sprintf(content, "<script>document.all('c-%d').innerHTML += \"%s\";</script>", num, _content);
	WriteToSock(sock, content);
}

void write_content_init(SOCKET sock, int num) {
	char content[RECV_BUF_SIZE];
	sprintf(content, "<script>\
					 		    document.all('res_tr_content').innerHTML += \"\
														<td id='c-%d'></td>\";</script>", num);
	WriteToSock(sock, content);
}

void serve_req_at(SOCKET sock, int num) {

	write_head_at(sock, num, requests[num].ip);
	write_content_init(sock, num);

}

void serve_req(SOCKET sock) {
	int i;
	for (i = 0; i < REQUEST_MAX_NUM; i++) {
		Request r = requests[i];
		if (!(r.ip && r.port && r.filename))   continue;
		serve_req_at(sock, i);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	static HWND hwndEdit;
	static SOCKET msock, ssock, csock;
	static struct sockaddr_in sa;

	int err;


	switch(Message) 
	{
		case WM_INITDIALOG:
			hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_LISTEN:

					WSAStartup(MAKEWORD(2, 0), &wsaData);

					//create master socket
					msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					if( msock == INVALID_SOCKET ) {
						EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
						WSACleanup();
						return TRUE;
					}

					err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

					if ( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
						closesocket(msock);
						WSACleanup();
						return TRUE;
					}

					//fill the address info about server
					sa.sin_family		= AF_INET;
					sa.sin_port			= htons(SERVER_PORT);
					sa.sin_addr.s_addr	= INADDR_ANY;

					//bind socket
					err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
						WSACleanup();
						return FALSE;
					}

					err = listen(msock, 2);
		
					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
						WSACleanup();
						return FALSE;
					}
					else {
						EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
					}

					break;
				case ID_EXIT:
					EndDialog(hwnd, 0);
					break;
			};
			break;

		case WM_CLOSE:
			EndDialog(hwnd, 0);
			break;

		case WM_SOCKET_NOTIFY:
			switch( WSAGETSELECTEVENT(lParam) )
			{
				case FD_ACCEPT:
					ssock = accept(msock, NULL, NULL);
					Socks.push_back(ssock);
					EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
					break;
				case FD_READ:
					ZeroMemory(buf, RECV_BUF_SIZE);
					content_length = recv(ssock, buf, RECV_BUF_SIZE, 0);
					EditPrintf(hwndEdit, TEXT("=== length: (%d) ===\r\n"), content_length);
					EditPrintf(hwndEdit, TEXT("=== buf: \r\n %s ===\r\n"), buf);

					/* parse method */
					char *method;
					method = strtok(buf, " ");
					EditPrintf(hwndEdit, TEXT("=== method: (%s) ===\r\n"), method);

					/* r=/asdf?h1=aaa */
					char *r;
					r = strtok(NULL, " ");
					EditPrintf(hwndEdit, TEXT("=== r: (%s) ===\r\n"), r);

					/* url=/asdf query_str=h1=aaa */
					char *url, *query_str;
					url = strtok(r, "?");
					strcpy(request_filename, url+1);
					EditPrintf(hwndEdit, TEXT("=== url: (%s) ===\r\n"), url);
					query_str = strtok(NULL, "");
					EditPrintf(hwndEdit, TEXT("=== query string: (%s) ===\r\n"), query_str);

					parse_query_string(query_str);

					FILE *file_ptr;		/* for open the request html file */

					/* start our cgi client */
					if (strcmp(request_filename, "hw3.cgi") == 0) {
						write_html_header(ssock);

						html_init(ssock);

						/* update the layout correspond to requests */
						serve_req(ssock);

						/* start server connections */
						//server_hanlder(hwndEdit, hwnd, ssock);

						html_end(ssock);

						closesocket(ssock);
					}
					/* request_filename opened successfully */
					else if ((file_ptr = fopen(request_filename, "rb")) != NULL) {
						/* the request filename success */
						write_html_header(ssock);

						/* open file by the query from request */
						EditPrintf(hwndEdit, TEXT("=== file: (%s) ===\r\n"), request_filename);

						fseek(file_ptr, 0, SEEK_END);
						int fsize;
						fsize = ftell(file_ptr);
						fseek(file_ptr, 0, SEEK_SET);
						ZeroMemory(buf, RECV_BUF_SIZE);
						if (fsize != fread(buf, fsize, 1, file_ptr)) {
							OutputDebugString("fread failed");
						}
						//OutputDebugString(buf);
						send(ssock, buf, strlen(buf), 0);

						/* release resources */
						fclose(file_ptr);
						closesocket(ssock);
					}
					break;
				case FD_WRITE:
					break;
				case FD_CLOSE:
					break;
			};
			break;
		
		default:
			return FALSE;

	};

	return TRUE;
}

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
     TCHAR   szBuffer [1024] ;
     va_list pArgList ;

     va_start (pArgList, szFormat) ;
     wvsprintf (szBuffer, szFormat, pArgList) ;
     va_end (pArgList) ;

     SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
     SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
     SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
	 return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0); 
}

/*-------------------------------------------------------------*/
/*--------------------- response helper -----------------------*/
/*-------------------------------------------------------------*/

void write_cgi_header(int connfd)
{
	char *content = "HTTP/1.1 200 OK\n";
	send(connfd, content, strlen(content), 0);
}

void write_html_header(int connfd)
{
	char *content = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
	send(connfd, content, strlen(content), 0);
}

/*-------------------------------------------------------------*/
/*--------------------------- html ----------------------------*/
/*-------------------------------------------------------------*/

void html_init(int connfd)
{
	char *content = "<html> \
        <head> \
        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" /> \
        <title>Network Programming Homework 3</title> \
        </head> \
        <body bgcolor=#336699> \
        <font face=\"Courier New\" size=2 color=#FFFF99>";

    char *table_html = "<table width=\"800\" border=\"1\">\
                        <tr id=\"res_tr_head\"></tr>\
                        <tr id=\"res_tr_content\"></tr>\
                        </table>";
	send(connfd, content, strlen(content), 0);
	send(connfd, table_html, strlen(table_html), 0);
}

void html_end(int connfd)
{
	char *content = "</body></html>";
	send(connfd, content, strlen(content), 0);
}

/*-------------------------------------------------------------*/
/*---------------------- handle server ------------------------*/
/*-------------------------------------------------------------*/

void server_hanlder(HWND hwndEdit, HWND hwnd, SOCKET sock)
{
	for (int i = 0; i < REQUEST_MAX_NUM; i++) {
		Request r = requests[i];
		if (!(r.ip && r.port && r.filename)) continue;

		/* connect */
		setup_connection(hwndEdit, hwnd, i);
		OutputDebugString("i\n");
	}
	// serve
	serve_connection();
}

/* setup connection by given index of requests */
void setup_connection(HWND hwndEdit, HWND hwnd, int index)
{

}

void serve_connection()
{

}

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
			requests[index].ip = (char*) malloc(REQUEST_HOST_SIZE);
			strcpy(requests[index].ip, value_tok);
			/*OutputDebugString("host");
			OutputDebugString(requests[index].ip);*/
		}
	}
	else if (capital == 'p') {

		value_tok = strtok_s(NULL, "\0", &saveptr);

		if (value_tok) {
			requests[index].port = (char *) malloc(REQUEST_PORT_SIZE);
			strcpy(requests[index].port, value_tok);
			/*OutputDebugString("port");
			OutputDebugString(requests[index].port);*/
		}
	}
	else if (capital == 'f') {

		value_tok = strtok_s(NULL, "\0", &saveptr);

		if (value_tok) {
			requests[index].filename = (char *) malloc(REQUEST_FILENAME_SIZE);
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