#include <windows.h>
#include <list>
#include <stdlib.h>
using namespace std;

#include "resource.h"
#include "parser.h"
#include "request.h"
#include "html.h"

/* Define Macro */

#define DEBUG				0
#define SERVER_PORT			7799
#define WM_SOCKET_NOTIFY	(WM_USER + 1)
#define SERVER_RESPONSE		7777
#define RECV_BUF_SIZE		10000
#define FILES_PATH_DIR		"C:\\Users\\lab1\\Documents\\GitHub\\winsock\\test\\"

/* Function Prototypes */

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
void write_cgi_header(int connfd);
void write_html_header(int connfd);
void html_init(int connfd);
void html_end(int connfd);
void server_hanlder(HWND hwndEdit, HWND hwnd, SOCKET sock);
int setup_connection(HWND hwndEdit, HWND hwnd, int index);
void write_command_init(HWND hwndEdit, HWND hwnd, int index);
void write_command_command();
void write_command_next(HWND hwndEdit, HWND hwnd, SOCKET sock, int index);
void HandleServerResponse(HWND hwndEdit, HWND hwnd, SOCKET sock, int ind, char *buf);

//=================================================================
//	Global Variables
//=================================================================
list <SOCKET> Socks;
char buf[RECV_BUF_SIZE];
int  content_length;
char request_filename[100];
Request requests[5];
int ServerCount = 0;

void WriteToSock(SOCKET sock, char *s)
{
	int r = send(sock, s, strlen(s), 0);
	if (r == SOCKET_ERROR) {
		OutputDebugString("=== Error: socket send ===\r\n");
		WSACleanup();
		return;
	}
}

// HTML Client

void write_head_at(SOCKET sock, int num, char *_content)
{
	char content[RECV_BUF_SIZE];
	sprintf(content, "<script>document.all['res_tr_head'].innerHTML += \"<td>%s</td>\";</script>", _content);
	WriteToSock(sock, content);
}

void write_content_at(SOCKET sock, int num, char *_content, int bold)
{
	char content[RECV_BUF_SIZE];
	if (bold)
		sprintf(content, "<script>document.all('c-%d').innerHTML += \"<b>%s</b>\";</script>", num, _content);
	else
		sprintf(content, "<script>document.all('c-%d').innerHTML += \"%s\";</script>", num, _content);
	WriteToSock(sock, content);
}

void write_content_init(SOCKET sock, int num)
{
	char content[RECV_BUF_SIZE];
	sprintf(content, "<script>\
					 		    document.all('res_tr_content').innerHTML += \"\
														<td id='c-%d'></td>\";</script>", num);
	WriteToSock(sock, content);
}

void serve_req_at(SOCKET sock, int num)
{
	write_head_at(sock, num, requests[num].ip);
	write_content_init(sock, num);
}

void serve_req(SOCKET sock)
{
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
						server_hanlder(hwndEdit, hwnd, ssock);

						html_end(ssock);
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

		/* response to remote server */
		case SERVER_RESPONSE:
			int i, r;
			EditPrintf(hwndEdit, TEXT("get server response: %x\r\n"), WSAGETSELECTEVENT(lParam));
			switch (WSAGETSELECTEVENT(lParam)) {
			case FD_ACCEPT:
				EditPrintf(hwndEdit, TEXT("=== [Server] Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
				break;
			case FD_READ:
				for (i = 0; i < REQUEST_MAX_NUM; i++) {
					csock = requests[i].socket;
					do {
						memset(buf, 0, sizeof(buf));
						r = recv(csock, buf, RECV_BUF_SIZE, 0);
						if (r > 0) {
							EditPrintf(hwndEdit, TEXT("=== [server] recv: START ===\r\n%s\r\n== recv: END ==\r\n"), buf);
							HandleServerResponse(hwndEdit, hwnd, ssock, i, buf);
						}
						else if (r == 0) {
							closesocket(csock);
							EditPrintf(hwndEdit, TEXT("[server] connection closed\r\n"));
						}
						else {
							EditPrintf(hwndEdit, TEXT("[server] recv failed: %d\r\n"), WSAGetLastError());
						}
					} while (r > 0);
				}

				break;
			case FD_WRITE:
				EditPrintf(hwndEdit, TEXT("=== [Server] fd write ===\r\n"));
				break;
			case FD_CLOSE:
				EditPrintf(hwndEdit, TEXT("=== [Server] fd close ===\r\n"));
				ServerCount--;
				if (ServerCount == 0) {
					for (i = 0; i < REQUEST_MAX_NUM; i++) {
						if (requests[i].ip)			requests[i].ip = NULL;
						if (requests[i].port)		requests[i].port = NULL;
						if (requests[i].socket)		requests[i].socket = 0;
						if (requests[i].filename)	requests[i].filename = NULL;
						if (requests[i].fp)			fclose(requests[i].fp);
					}
				}
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
		write_command_init(hwndEdit, hwnd, i);
		setup_connection(hwndEdit, hwnd, i);
		ServerCount++;
	}
}

/* setup connection by given index of requests */
int setup_connection(HWND hwndEdit, HWND hwnd, int index)
{
	/* Initialize Winsock */
	WSADATA wsaData;
	int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (r != NO_ERROR) {
		EditPrintf(hwndEdit, TEXT("WSAStartup function failed with error: %d\r\n"), r);
		return 1;
	}

	/* Create a SOCKET for connecting to server */
	SOCKET csock;
	csock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (csock == INVALID_SOCKET) {
		EditPrintf(hwndEdit, TEXT("socket function failed with error: %ld\r\n"), WSAGetLastError());
		WSACleanup();
		return 1;
	}

	/* The sockaddr_in structure specifies the address family, 
	   Ip address, and port of the server to be connected to */
	struct hostent *hostname;
	hostname = gethostbyname(requests[index].ip);
	if (hostname == NULL) {
		EditPrintf(hwndEdit, TEXT("get hostname failed with error: %ld\r\n"), WSAGetLastError());
		return 1;
	}

	sockaddr_in clientService;
	clientService.sin_family = AF_INET;
	memcpy(&clientService.sin_addr, hostname->h_addr_list[0], hostname->h_length);
	clientService.sin_port = htons(atoi(requests[index].port));

	/* Connect to server */
	r = connect(csock, (SOCKADDR *)& clientService, sizeof (clientService));
	if (r == SOCKET_ERROR) {
		EditPrintf(hwndEdit, TEXT("connect function failed with error: %ld\r\n"), WSAGetLastError());
		r = closesocket(csock);
		if (r == SOCKET_ERROR) {
			EditPrintf(hwndEdit, TEXT("closesocket function failed with error: %ld\r\n"), WSAGetLastError());
		}
		WSACleanup();
		return 1;
	}

	EditPrintf(hwndEdit, TEXT("connected to %s\r\n"), requests[index].ip);
	int err = WSAAsyncSelect(csock, hwnd, SERVER_RESPONSE, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

	if (err == SOCKET_ERROR) {
		EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
		closesocket(csock);
		WSACleanup();
		return 1;
	}

	requests[index].socket = csock;

	return 0;
}

int contain_prompt(char *s)
{
	int i;
	for (i = 0; i < strlen(s); i++) {
		if (s[i] == '%')	return 1;
	}
	return 0;
}

void HandleServerResponse(HWND hwndEdit, HWND hwnd, SOCKET sock, int ind, char *buf)
{
	EditPrintf(hwndEdit, TEXT("=== Handle Server Response: Start===\r\n%s\r\n=== End ===\r\n"), buf);
	write_content_at(sock, ind, wrap_html(buf), 0);
	if (contain_prompt(buf)) {
		// should send
		write_command_next(hwndEdit, hwnd, sock, ind);
	}
}

void write_command_init(HWND hwndEdit, HWND hwnd, int ind)
{
	char filepath[1000] = FILES_PATH_DIR;
	strcat(filepath, requests[ind].filename);
	OutputDebugString(filepath);
	if ((requests[ind].fp = fopen(filepath, "r")) == NULL){
		EditPrintf(hwndEdit, TEXT("fopen error: %d\r\n"), errno);
	}
}

void write_command_close(int ind) {
	fclose(requests[ind].fp);
}

void write_command_next(HWND hwndEdit, HWND hwnd, SOCKET sock, int ind)
{
	int n, r;
	char buf[RECV_BUF_SIZE];
	if (!fgets(buf, RECV_BUF_SIZE, requests[ind].fp)) {
		OutputDebugString("fgets error");
		return;
	}

	// write to html client
	write_content_at(sock, ind, wrap_html(buf), 1);
	if (buf[0] == '\n')	return;

	// write to server
	//EditPrintf(hwndEdit, TEXT("=== write_command_next send: Start===\r\n%s\r\n=== End ===\r\n"), buf);
	r = send(requests[ind].socket, buf, strlen(buf), 0);
	if (r == SOCKET_ERROR) {
		OutputDebugString("send error");
		closesocket(requests[ind].socket);
		WSACleanup();
		return;
	}
}