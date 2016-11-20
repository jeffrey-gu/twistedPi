/*

MotiveClient2.cpp

This program connects to a NatNet server, receives a data stream, and writes that data stream
to a WinSock connection (and/or ASCII file). Makes use of NatNetClient class.

*/

#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <winsock2.h>

#include "NatNetTypes.h"
#include "NatNetClient.h"

#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <string>
#include <iostream>

#include <time.h>
#include "windows.h"	//can be used to get sys time in 100 nanosec intervals
//#include <sys/time.h>	//does not exist for windows

//PRAGMAS
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#pragma pack(push, 1)	//no padding

//CONSTANTS
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

//METHODS:
void _WriteFrame(FILE* fp, sFrameOfMocapData* data);
void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData);
void resetClient();
int CreateClient(int iConnectionType);

int OpenExportServer();
int CloseExportServer();

unsigned __int64 WindowsTickToUnixSeconds(__int64 windowsTicks);

//VARIABLES
unsigned int MyServersDataPort = 1511;
unsigned int MyServersCommandPort = 1500;
int iConnectionType = ConnectionType_Multicast;

NatNetClient* theClient;
FILE* fp;

SOCKET ClientSocket;
HANDLE hComm;

char szMyIPAddress[128] = "";
char szServerIPAddress[128] = "";

__int64 timeStamp;

/*******************PAYLOAD DESIGN************************/
struct payload_t {
	unsigned __int16 milli;
	unsigned __int8 sec, min, hour;
	float payload_x, payload_y, payload_z;
	//float marker[1];

	/*payload_t() {
		timestamp=0;
		x = 0;
		y = 0;
		z = 0;
	}*/
};
/*******************************************************/

int _tmain(int argc, _TCHAR* argv[]) {
	int iResult;
	OpenExportServer();

	strncpy(szMyIPAddress, "", 1);
	strncpy(szServerIPAddress, "", 1);

	iResult = CreateClient(iConnectionType);
	if(iResult != ErrorCode_OK) {
		printf("Error init'ing NatNetClient. Exiting");
		return 1;
	}

	printf("NatNetClient initialized - ready.\n");

	//Retrieve data descriptions from NatNet Server
	printf("\nRequesting Data Descriptions...\n");
	sDataDescriptions* pDataDefs = NULL;
	int nBodies = theClient->GetDataDescriptions(&pDataDefs);
	if(!pDataDefs) {
		printf("Unable to retrieve Data Descriptions.\n");
	}

	//TODO: print them out or write descriptions to file?
	//TODO: write to header?

	//USER INTERFACE INTERACTION:
	printf("\nClient is connected to server and listening for data...\n");
	int c;
	bool bExit = false;

	while(c = _getch()) {
		switch(c) {
			case 'q':
				bExit = true;
				break;
			case 'r':
				resetClient();
				break;
			case 'f': {
				sFrameOfMocapData* pData = theClient->GetLastFrameOfData();
				printf("Most Recent Frame: %d", pData->iFrame);
				break;
					  }
			case 'c':
				iResult = CreateClient(iConnectionType);
				break;
			default:
				break;
		}
		if(bExit) {break;}
	}

	//Finished
	theClient->Uninitialize();
	//Write footer to file?
	//TODO: if outputting to file, close file descriptor
}


int CreateClient(int iConnectionType) {

	//if previous server exists, discard
	if(theClient) {
		theClient->Uninitialize();
		delete theClient;
	}

	theClient = new NatNetClient(iConnectionType);

	//set callback
	theClient->SetVerbosityLevel(Verbosity_Debug);
    theClient->SetDataCallback( DataHandler, theClient );	// this function will receive data from the server

	//init client and connect to NatNet server
	int retCode = theClient->Initialize(szMyIPAddress, szServerIPAddress);	//local machine
	if(retCode != ErrorCode_OK) {
		printf("Unable to connect to NatNet server. Error code: %d\nExiting", retCode);
		return ErrorCode_Internal;
	}
	
	sServerDescription ServerDescription;
    memset(&ServerDescription, 0, sizeof(ServerDescription));
    theClient->GetServerDescription(&ServerDescription);
    if(!ServerDescription.HostPresent)
    {
        printf("Unable to connect to server. Host not present. Exiting.");
        return 1;
    }

	return ErrorCode_OK;
}

void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData) {
	
	/**************GET TIMESTAMP***************/
	//time.h implementation...does not give absolute time
	//float timeStamp = (float) clock()/CLOCKS_PER_SEC;

	//windows.h implementation
	//FILETIME ftime;
	//GetSystemTimeAsFileTime(&ftime); 
	//memcpy(&timeStamp, &ftime, sizeof(__int64));
	//unsigned __int64 now = WindowsTickToUnixSeconds(timeStamp);

	SYSTEMTIME stime;
	GetLocalTime(&stime); 
	unsigned __int16 milli = (unsigned __int16) (stime.wMilliseconds);
	unsigned __int8 sec = (unsigned __int8) (stime.wSecond);
	unsigned __int8 min = (unsigned __int8) (stime.wMinute);
	unsigned __int8 hour= (unsigned __int8) (stime.wHour);

	////todo: test if system time is correct time zone
	//printf("The system time is: %s\n", now);
	/******************************************/

	int iSendResult;
	char temp[sizeof(payload_t)];
	std::size_t payload_size = sizeof(payload_t);

	NatNetClient* pClient = (NatNetClient*) pUserData;

	payload_t message = {0};
	//payload_t* message = new payload_t();

	//fill payload with rigid body data
	for(int i = 0; i<data->nRigidBodies; ++i) {
		sRigidBodyData rbData = data->RigidBodies[i];	
		
		//payload_t load = {timeStamp, {rbData.x, rbData.y, rbData.z}};
		payload_t load = {milli, sec, min, hour, rbData.x, rbData.y, rbData.z};
		memcpy(&message, &load, payload_size);
	}
	
	memcpy(temp, &message, payload_size);	//load into temp
	//payload_t* message_ptr = &message;

	printf("size of payload: %d bytes\n", (int) sizeof(temp));
	printf("\ttime:%u::%u::%u::%u\tx:%f\ty:%f\tz:%f\n\n", message.hour, message.min, message.sec, message.milli, message.payload_x, message.payload_y, message.payload_z);

	//send payload
	iSendResult = send(ClientSocket, temp, (int)payload_size, 0);
	if (iSendResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
    }
	iSendResult = send(ClientSocket, "\n", 1, 0);	//delimiter for twisted
}

int OpenExportServer() {

	//Initialiaze WinSock
	WSADATA wsaData;
	int iResult;

	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);	//init use of Winsock DLL
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	//determine values in sockaddr structure
	struct addrinfo *result = NULL;
	struct addrinfo hints;
	SOCKET ListenSocket = INVALID_SOCKET;	//for listening to client requests
	ClientSocket = INVALID_SOCKET;	//GLOBAL var - for handling connection requests


	ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

	// Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	//create socket for listening for client connections
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}
	
	//set up TCP listening socket: bind server to network address on system
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

	//free address info after use
	freeaddrinfo(result);

	//listen on socket
	if ( listen( ListenSocket, SOMAXCONN ) == SOCKET_ERROR ) {
		printf( "Listen failed with error: %ld\n", WSAGetLastError() );
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// Accept a client socket
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// once ClientSocket accepted, no longer need ListenSocket (1:1 server->client)
	closesocket(ListenSocket);

	return 0;
}

int CloseExportServer() {
	int iResult;
	iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}


void resetClient() {
	int iSuccess;

	printf("\n\nre-setting Client\n\n.");

	iSuccess = theClient->Uninitialize();
	if(iSuccess != 0)
		printf("error un-initting Client\n");

	iSuccess = theClient->Initialize("", "");
	if(iSuccess != 0)
		printf("error re-initting Client\n");
}

unsigned __int64 WindowsTickToUnixSeconds(__int64 windowsTicks)
{
     return (unsigned)(windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}