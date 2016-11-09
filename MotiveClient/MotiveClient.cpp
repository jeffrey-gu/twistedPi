//=============================================================================
// Copyright � 2014 NaturalPoint, Inc. All Rights Reserved.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall NaturalPoint, Inc. or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//=============================================================================


/*

MotiveClient.cpp

This program connects to a NatNet server, receives a data stream, and writes that data stream
to an ascii file.  The purpose is to illustrate using the NatNetClient class.

Usage [optional]:

	SampleClient [ServerIP] [LocalIP] [OutputFilename]

	[ServerIP]			IP address of the server (e.g. 192.168.0.107) ( defaults to local machine)
	[OutputFilename]	Name of points file (pts) to write out.  defaults to Client-output.pts

*/

#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <winsock2.h>

#include "NatNetTypes.h"
#include "NatNetClient.h"

/**/ //Stephen's includes
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <string>
#include <iostream>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
/**/

#pragma warning( disable : 4996 )

void _WriteHeader(FILE* fp, sDataDescriptions* pBodyDefs);
void _WriteFrame(FILE* fp, sFrameOfMocapData* data);
void _WriteFooter(FILE* fp);
void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData);		// receives data from the server
void __cdecl MessageHandler(int msgType, char* msg);		            // receives NatNet error mesages
void resetClient();
int CreateClient(int iConnectionType);

unsigned int MyServersDataPort = 1511;
unsigned int MyServersCommandPort = 1500;
int iConnectionType = ConnectionType_Multicast;
//int iConnectionType = ConnectionType_Unicast;

/**/ //Stephen's additional methods
int SetupExportServer();
int CloseExportServer();
//int SetupSerialSocket();
//int CloseSerialSocket();
int SendDataExportServer(char recvbuf[DEFAULT_BUFLEN]);
int SendDataExportServer(char recvbuf[DEFAULT_BUFLEN],int dLength);
//int SendToSerialSocket(char recvbuf[DEFAULT_BUFLEN],int dLength);
SOCKET ClientSocket;
HANDLE hComm;
/**/

NatNetClient* theClient;
FILE* fp;

char szMyIPAddress[128] = "";
char szServerIPAddress[128] = "";

int _tmain(int argc, _TCHAR* argv[])
{
    int iResult;

	SetupExportServer();
	//SetupSerialSocket();

    // parse command line args
    if(argc>1)
    {
        strcpy(szServerIPAddress, argv[1]);	// specified on command line
        printf("Connecting to server at %s...\n", szServerIPAddress);
    }
    else
    {
        strcpy(szServerIPAddress, "");		// not specified - assume server is local machine
        printf("Connecting to server at LocalMachine\n");
    }
    if(argc>2)
    {
        strcpy(szMyIPAddress, argv[2]);	    // specified on command line
        printf("Connecting from %s...\n", szMyIPAddress);
    }
    else
    {
        strcpy(szMyIPAddress, "");          // not specified - assume server is local machine
        printf("Connecting from LocalMachine...\n");
    }

    // Create NatNet Client
    iResult = CreateClient(iConnectionType);
    if(iResult != ErrorCode_OK)
    {
        printf("Error initializing client.  See log for details.  Exiting");
        return 1;
    }
    else
    {
        printf("Client initialized and ready.\n");
    }


	// send/receive test request
	printf("[SampleClient] Sending Test Request\n");
	void* response;
	int nBytes;
	iResult = theClient->SendMessageAndWait("TestRequest", &response, &nBytes);
	if (iResult == ErrorCode_OK)
	{
		printf("[SampleClient] Received: %s", (char*)response);
	}

	// Retrieve Data Descriptions from server
	printf("\n\n[SampleClient] Requesting Data Descriptions...");
	sDataDescriptions* pDataDefs = NULL;
	int nBodies = theClient->GetDataDescriptions(&pDataDefs);
	if(!pDataDefs)
	{
		printf("[SampleClient] Unable to retrieve Data Descriptions.");
	}
	else
	{
        printf("[SampleClient] Received %d Data Descriptions:\n", pDataDefs->nDataDescriptions );
        for(int i=0; i < pDataDefs->nDataDescriptions; i++)
        {
            printf("Data Description # %d (type=%d)\n", i, pDataDefs->arrDataDescriptions[i].type);
            if(pDataDefs->arrDataDescriptions[i].type == Descriptor_MarkerSet)
            {
                // MarkerSet
                sMarkerSetDescription* pMS = pDataDefs->arrDataDescriptions[i].Data.MarkerSetDescription;
                printf("MarkerSet Name : %s\n", pMS->szName);
                for(int i=0; i < pMS->nMarkers; i++)
                    printf("%s\n", pMS->szMarkerNames[i]);

            }
            else if(pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
            {
                // RigidBody
                sRigidBodyDescription* pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
                printf("RigidBody Name : %s\n", pRB->szName);
                printf("RigidBody ID : %d\n", pRB->ID);
                printf("RigidBody Parent ID : %d\n", pRB->parentID);
                printf("Parent Offset : %3.2f,%3.2f,%3.2f\n", pRB->offsetx, pRB->offsety, pRB->offsetz);
            }
            //else if(pDataDefs->arrDataDescriptions[i].type == Descriptor_Skeleton)
            //{
            //    // Skeleton
            //    sSkeletonDescription* pSK = pDataDefs->arrDataDescriptions[i].Data.SkeletonDescription;
            //    printf("Skeleton Name : %s\n", pSK->szName);
            //    printf("Skeleton ID : %d\n", pSK->skeletonID);
            //    printf("RigidBody (Bone) Count : %d\n", pSK->nRigidBodies);
            //    for(int j=0; j < pSK->nRigidBodies; j++)
            //    {
            //        sRigidBodyDescription* pRB = &pSK->RigidBodies[j];
            //        printf("  RigidBody Name : %s\n", pRB->szName);
            //        printf("  RigidBody ID : %d\n", pRB->ID);
            //        printf("  RigidBody Parent ID : %d\n", pRB->parentID);
            //        printf("  Parent Offset : %3.2f,%3.2f,%3.2f\n", pRB->offsetx, pRB->offsety, pRB->offsetz);
            //    }
            //}
            else
            {
                printf("Unknown data type.");
                // Unknown
            }
        }
	}


	// Create data file for writing received stream into
	char szFile[MAX_PATH];
	char szFolder[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szFolder);
	if(argc > 3)
		sprintf(szFile, "%s\\%s", szFolder, argv[3]);
	else
		sprintf(szFile, "%s\\Client-output.pts",szFolder);
	fp = fopen(szFile, "w");
	if(!fp)
	{
		printf("error opening output file %s.  Exiting.", szFile);
		exit(1);
	}
	if(pDataDefs)
		_WriteHeader(fp, pDataDefs);

	// Ready to receive marker stream!
	printf("\nClient is connected to server and listening for data...\n");
	int c;
	bool bExit = false;
	while(c =_getch())
	{
		switch(c)
		{
			case 'q':
				bExit = true;
				break;
			case 'r':
				resetClient();
				break;
            case 'p':
                sServerDescription ServerDescription;
                memset(&ServerDescription, 0, sizeof(ServerDescription));
                theClient->GetServerDescription(&ServerDescription);
                if(!ServerDescription.HostPresent)
                {
                    printf("Unable to connect to server. Host not present. Exiting.");
                    return 1;
                }
                break;
            case 'f':
                {
                    sFrameOfMocapData* pData = theClient->GetLastFrameOfData();
                    printf("Most Recent Frame: %d", pData->iFrame);
                }
                break;
            case 'm':	                        // change to multicast
                iConnectionType = ConnectionType_Multicast;
                iResult = CreateClient(iConnectionType);
                if(iResult == ErrorCode_OK)
                    printf("Client connection type changed to Multicast.\n\n");
                else
                    printf("Error changing client connection type to Multicast.\n\n");
                break;
            case 'u':	                        // change to unicast
                iConnectionType = ConnectionType_Unicast;
                iResult = CreateClient(iConnectionType);
                if(iResult == ErrorCode_OK)
                    printf("Client connection type changed to Unicast.\n\n");
                else
                    printf("Error changing client connection type to Unicast.\n\n");
                break;
            case 'c' :                          // connect
                iResult = CreateClient(iConnectionType);
                break;
            case 'd' :                          // disconnect
                // note: applies to unicast connections only - indicates to Motive to stop sending packets to that client endpoint
                iResult = theClient->SendMessageAndWait("Disconnect", &response, &nBytes);
                if (iResult == ErrorCode_OK)
                    printf("[SampleClient] Disconnected");
                break;
			default:
				break;
		}
		if(bExit)
			break;
	}

	// Done - clean up.
	theClient->Uninitialize();
	_WriteFooter(fp);
	fclose(fp);

	CloseExportServer();

	return ErrorCode_OK;
}

// Establish a NatNet Client connection
int CreateClient(int iConnectionType)
{
    // release previous server
    if(theClient)
    {
        theClient->Uninitialize();
        delete theClient;
    }

    // create NatNet client
    theClient = new NatNetClient(iConnectionType);

    // [optional] use old multicast group
    //theClient->SetMulticastAddress("224.0.0.1");

    // print version info
    unsigned char ver[4];
    theClient->NatNetVersion(ver);
    printf("NatNet Sample Client (NatNet ver. %d.%d.%d.%d)\n", ver[0], ver[1], ver[2], ver[3]);

    // Set callback handlers
    theClient->SetMessageCallback(MessageHandler);
    theClient->SetVerbosityLevel(Verbosity_Debug);
    theClient->SetDataCallback( DataHandler, theClient );	// this function will receive data from the server

    // Init Client and connect to NatNet server
    // to use NatNet default port assigments
    int retCode = theClient->Initialize(szMyIPAddress, szServerIPAddress);
    // to use a different port for commands and/or data:
    //int retCode = theClient->Initialize(szMyIPAddress, szServerIPAddress, MyServersCommandPort, MyServersDataPort);
    if (retCode != ErrorCode_OK)
    {
        printf("Unable to connect to server.  Error code: %d. Exiting", retCode);
        return ErrorCode_Internal;
    }
    else
    {
        // print server info
        sServerDescription ServerDescription;
        memset(&ServerDescription, 0, sizeof(ServerDescription));
        theClient->GetServerDescription(&ServerDescription);
        if(!ServerDescription.HostPresent)
        {
            printf("Unable to connect to server. Host not present. Exiting.");
            return 1;
        }
        printf("[SampleClient] Server application info:\n");
        printf("Application: %s (ver. %d.%d.%d.%d)\n", ServerDescription.szHostApp, ServerDescription.HostAppVersion[0],
            ServerDescription.HostAppVersion[1],ServerDescription.HostAppVersion[2],ServerDescription.HostAppVersion[3]);
        printf("NatNet Version: %d.%d.%d.%d\n", ServerDescription.NatNetVersion[0], ServerDescription.NatNetVersion[1],
            ServerDescription.NatNetVersion[2], ServerDescription.NatNetVersion[3]);
        printf("Client IP:%s\n", szMyIPAddress);
        printf("Server IP:%s\n", szServerIPAddress);
        printf("Server Name:%s\n\n", ServerDescription.szHostComputerName);
    }

    return ErrorCode_OK;

}

// DataHandler receives data from the server
void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData)
{
	NatNetClient* pClient = (NatNetClient*) pUserData;

	if(fp)
		_WriteFrame(fp,data);

    int i=0;

    printf("FrameID : %d\n", data->iFrame);
    printf("Timestamp :  %3.2lf\n", data->fTimestamp);
    printf("Latency :  %3.2lf\n", data->fLatency);

    // FrameOfMocapData params
    bool bIsRecording = ((data->params & 0x01)!=0);
    bool bTrackedModelsChanged = ((data->params & 0x02)!=0);
    if(bIsRecording)
        printf("RECORDING\n");
    if(bTrackedModelsChanged)
        printf("Models Changed.\n");


    // timecode - for systems with an eSync and SMPTE timecode generator - decode to values
	int hour, minute, second, frame, subframe;
	bool bValid = pClient->DecodeTimecode(data->Timecode, data->TimecodeSubframe, &hour, &minute, &second, &frame, &subframe);
	// decode to friendly string
	char szTimecode[128] = "";
	pClient->TimecodeStringify(data->Timecode, data->TimecodeSubframe, szTimecode, 128);
	printf("Timecode : %s\n", szTimecode);

	// Other Markers
	printf("Other Markers [Count=%d]\n", data->nOtherMarkers);
	for(i=0; i < data->nOtherMarkers; i++)
	{
		printf("Other Marker %d : %3.2f\t%3.2f\t%3.2f\n",
			i,
			data->OtherMarkers[i][0],
			data->OtherMarkers[i][1],
			data->OtherMarkers[i][2]);
	}

	// Rigid Bodies
	printf("Rigid Bodies [Count=%d]\n", data->nRigidBodies);
	for(i=0; i < data->nRigidBodies; i++)
	{
        // params
        // 0x01 : bool, rigid body was successfully tracked in this frame
        bool bTrackingValid = data->RigidBodies[i].params & 0x01;

		printf("Rigid Body [ID=%d  Error=%3.2f  Valid=%d]\n", data->RigidBodies[i].ID, data->RigidBodies[i].MeanError, bTrackingValid);
		printf("\tx\ty\tz\tqx\tqy\tqz\tqw\n");
		printf("\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\n",
			data->RigidBodies[i].x,
			data->RigidBodies[i].y,
			data->RigidBodies[i].z,
			data->RigidBodies[i].qx,
			data->RigidBodies[i].qy,
			data->RigidBodies[i].qz,
			data->RigidBodies[i].qw);

		printf("\tRigid body markers [Count=%d]\n", data->RigidBodies[i].nMarkers);
		for(int iMarker=0; iMarker < data->RigidBodies[i].nMarkers; iMarker++)
		{
            printf("\t\t");
            if(data->RigidBodies[i].MarkerIDs)
                printf("MarkerID:%d", data->RigidBodies[i].MarkerIDs[iMarker]);
            if(data->RigidBodies[i].MarkerSizes)
                printf("\tMarkerSize:%3.2f", data->RigidBodies[i].MarkerSizes[iMarker]);
            if(data->RigidBodies[i].Markers)
                printf("\tMarkerPos:%3.2f,%3.2f,%3.2f\n" ,
                    data->RigidBodies[i].Markers[iMarker][0],
                    data->RigidBodies[i].Markers[iMarker][1],
                    data->RigidBodies[i].Markers[iMarker][2]);
        }
	}

	// skeletons
	/*printf("Skeletons [Count=%d]\n", data->nSkeletons);
	for(i=0; i < data->nSkeletons; i++)
	{
		sSkeletonData skData = data->Skeletons[i];
		printf("Skeleton [ID=%d  Bone count=%d]\n", skData.skeletonID, skData.nRigidBodies);
		for(int j=0; j< skData.nRigidBodies; j++)
		{
			sRigidBodyData rbData = skData.RigidBodyData[j];
			printf("Bone %d\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\n",
				rbData.ID, rbData.x, rbData.y, rbData.z, rbData.qx, rbData.qy, rbData.qz, rbData.qw );

			printf("\tRigid body markers [Count=%d]\n", rbData.nMarkers);
			for(int iMarker=0; iMarker < rbData.nMarkers; iMarker++)
			{
				printf("\t\t");
				if(rbData.MarkerIDs)
					printf("MarkerID:%d", rbData.MarkerIDs[iMarker]);
				if(rbData.MarkerSizes)
					printf("\tMarkerSize:%3.2f", rbData.MarkerSizes[iMarker]);
				if(rbData.Markers)
					printf("\tMarkerPos:%3.2f,%3.2f,%3.2f\n" ,
					data->RigidBodies[i].Markers[iMarker][0],
					data->RigidBodies[i].Markers[iMarker][1],
					data->RigidBodies[i].Markers[iMarker][2]);
			}
		}
	}*/

	// labeled markers
    bool bOccluded;     // marker was not visible (occluded) in this frame
    bool bPCSolved;     // reported position provided by point cloud solve
    bool bModelSolved;  // reported position provided by model solve
	printf("Labeled Markers [Count=%d]\n", data->nLabeledMarkers);
	for(i=0; i < data->nLabeledMarkers; i++)
	{
        bOccluded = ((data->LabeledMarkers[i].params & 0x01)!=0);
        bPCSolved = ((data->LabeledMarkers[i].params & 0x02)!=0);
        bModelSolved = ((data->LabeledMarkers[i].params & 0x04)!=0);
		sMarker marker = data->LabeledMarkers[i];
        int modelID, markerID;
        theClient->DecodeID(marker.ID, &modelID, &markerID);
		printf("Labeled Marker [ModelID=%d, MarkerID=%d, Occluded=%d, PCSolved=%d, ModelSolved=%d] [size=%3.2f] [pos=%3.2f,%3.2f,%3.2f]\n",
            modelID, markerID, bOccluded, bPCSolved, bModelSolved,  marker.size, marker.x, marker.y, marker.z);
	}

}

// MessageHandler receives NatNet error/debug messages
void __cdecl MessageHandler(int msgType, char* msg)
{
	printf("\n%s\n", msg);
}

/* File writing routines */
void _WriteHeader(FILE* fp, sDataDescriptions* pBodyDefs)
{
	int i=0;

    if(!pBodyDefs->arrDataDescriptions[0].type == Descriptor_MarkerSet)
        return;

	sMarkerSetDescription* pMS = pBodyDefs->arrDataDescriptions[0].Data.MarkerSetDescription;

	fprintf(fp, "<MarkerSet>\n\n");
	fprintf(fp, "<Name>\n%s\n</Name>\n\n", pMS->szName);

	fprintf(fp, "<Markers>\n");
	for(i=0; i < pMS->nMarkers; i++)
	{
		fprintf(fp, "%s\n", pMS->szMarkerNames[i]);
	}
	fprintf(fp, "</Markers>\n\n");

	fprintf(fp, "<Data>\n");
	fprintf(fp, "Frame#\t");
	for(i=0; i < pMS->nMarkers; i++)
	{
		fprintf(fp, "M%dX\tM%dY\tM%dZ\t", i, i, i);
	}
	fprintf(fp,"\n");

}

void _WriteFrame(FILE* fp, sFrameOfMocapData* data)
{
	// printf("_WriteFrame is called\n");
	//fprintf(fp, "%d", data->iFrame);
	/*for(int i =0; i < data->MocapData->nMarkers; i++)
	{
		fprintf(fp, "\t%.5f\t%.5f\t%.5f", data->MocapData->Markers[i][0], data->MocapData->Markers[i][1], data->MocapData->Markers[i][2]);
	}*/

	char recvbuf[DEFAULT_BUFLEN];
	memset( recvbuf, '\0', sizeof(recvbuf) );
    int recvbuflen = DEFAULT_BUFLEN;
	fprintf(fp, "frame number %d\n", data->iFrame);

	/*for(int i =0; i < data->MocapData->nMarkers; i++)
	{
		fprintf(fp, "\t%.5f\t%.5f\t%.5f", data->MocapData->Markers[i][0], data->MocapData->Markers[i][1], data->MocapData->Markers[i][2]);
	}*/
	// printf("About to enter for loop with %d\n", data->nRigidBodies);

  //////////////////////////////////////////////////////////
  //send frameID
  memcpy(temp, data->iFrame, sizeof(int));
  SendDataExportServer(temp,sizeof(int));
  SendDataExportServer("\n",1);

  ///////////////////////////////////////////////////////////

	for(int j=0; j< data->nRigidBodies; j++)
	{
		// printf("received data: writing to export server\n");

		sRigidBodyData rbData = data->RigidBodies[j];
		fprintf(fp,"rigidBody number %d id %d \t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\n",
			j, rbData.ID, rbData.x, rbData.y, rbData.z, rbData.qx, rbData.qy, rbData.qz, rbData.qw );
		sprintf(recvbuf,"rigidBody number %d id %d \t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\n",
			j, rbData.ID, rbData.x, rbData.y, rbData.z, rbData.qx, rbData.qy, rbData.qz, rbData.qw );
		//SendDataExportServer(recvbuf);
		char temp[sizeof(float)];

    //////////////////////////////////////////////////////////
    //also send rigid body ID and timestamp
    memcpy(temp, &rbData.ID, sizeof(float));
    SendDataExportServer(temp,sizeof(float));
		SendDataExportServer("\n",1);

    memcpy(temp, data->fTimestamp, sizeof(float));  //currently sending as a float
    SendDataExportServer(temp,sizeof(float));
		SendDataExportServer("\n",1);
    //////////////////////////////////////////////////////////

		memcpy(temp,&rbData.x,sizeof(float));
		SendDataExportServer(temp,sizeof(float));
		SendDataExportServer("\n",1);
		memcpy(temp,&rbData.y,sizeof(float));
		SendDataExportServer(temp,sizeof(float));
		SendDataExportServer("\n",1);
		memcpy(temp,&rbData.z,sizeof(float));
		SendDataExportServer(temp,sizeof(float));
		SendDataExportServer("\n",1);
		//memcpy(temp,&rbData.qx,sizeof(float));
		//SendDataExportServer(temp,sizeof(float));
		//SendDataExportServer("\n",1);
		//memcpy(temp,&rbData.qy,sizeof(float));
		//SendDataExportServer(temp,sizeof(float));
		//SendDataExportServer("\n",1);
		//memcpy(temp,&rbData.qz,sizeof(float));
		//SendDataExportServer(temp,sizeof(float));
		//SendDataExportServer("\n",1);
		//memcpy(temp,&rbData.qw,sizeof(float));
		//SendDataExportServer(temp,sizeof(float));
		//SendDataExportServer("\n",1);
	}
	fprintf(fp, "\n");
}

void _WriteFooter(FILE* fp)
{
	fprintf(fp, "</Data>\n\n");
	fprintf(fp, "</MarkerSet>\n");
}

void resetClient()
{
	int iSuccess;

	printf("\n\nre-setting Client\n\n.");

	iSuccess = theClient->Uninitialize();
	if(iSuccess != 0)
		printf("error un-initting Client\n");

	iSuccess = theClient->Initialize(szMyIPAddress, szServerIPAddress);
	if(iSuccess != 0)
		printf("error re-initting Client\n");


}

std::string bufferToString(char* buffer, int bufflen)
{
    std::string ret(buffer, bufflen);

    return ret;
}

int SetupExportServer()
{
	WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;


    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // No longer need server socket
    //closesocket(ListenSocket);
	return 0;
}

int SendDataExportServer(char recvbuf[DEFAULT_BUFLEN]){
	int iResult,iSendResult;

    iSendResult = send( ClientSocket, recvbuf, 512, 0 );
    if (iSendResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }
	return 0;
}

int SendDataExportServer(char recvbuf[DEFAULT_BUFLEN], int dLength){
	int iResult,iSendResult;

    iSendResult = send( ClientSocket, recvbuf, dLength, 0 );
    if (iSendResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }
	return 0;
}

int CloseExportServer(){
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

//int SendToSerialSocket(char recvbuf[DEFAULT_BUFLEN], int dLength){
//	OVERLAPPED osWrite = {0};
//	DWORD dwWritten;
//	BOOL fRes;
//
//	// Create this writes OVERLAPPED structure hEvent.
//	osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
//	if (osWrite.hEvent == NULL)
//		// Error creating overlapped event handle.
//		return FALSE;
//
//	// Issue write.
//	if (!WriteFile(hComm, recvbuf, dLength, &dwWritten, &osWrite)) {
//		if (GetLastError() != ERROR_IO_PENDING) {
//			// WriteFile failed, but it isn't delayed. Report error and abort.
//			fRes = FALSE;
//		}
//		else {
//			// Write is pending.
//			if (!GetOverlappedResult(hComm, &osWrite, &dwWritten, TRUE))
//			fRes = FALSE;
//			else
//			// Write operation completed successfully.
//			fRes = TRUE;
//		}
//	}
//	else
//		// WriteFile completed immediately.
//		fRes = TRUE;
//
//	CloseHandle(osWrite.hEvent);
//	return fRes;
//}
//
//int SetupSerialSocket(){
//	DCB config = {0};
//
//	hComm = CreateFile("COM3",
//                    GENERIC_READ | GENERIC_WRITE,
//                    0,
//                    NULL,
//                    OPEN_EXISTING,
//                    FILE_FLAG_OVERLAPPED,
//                    NULL);
//	if (hComm == INVALID_HANDLE_VALUE){
//		printf("Error opening Serial Socket");
//		return 1;
//	}
//	if((GetCommState(hComm, &config) == 0))
//    {
//        printf("Get configuration port has a problem.");
//        return 1;
//    }
//
//	config.DCBlength = sizeof(config);
//
//	config.BaudRate = 115200;
//	config.ByteSize = 8;
//
//	if (!SetCommState(hComm, &config)||!SetupComm( hComm, 10000, 10000 ))
//    {
//
//        printf( "Failed to Set Comm State Reason: %d\n",GetLastError());
//        return 1;
//    }
//
//	return 0;
//}
//int CloseSerialSocket(){
//	return 0;
//}
