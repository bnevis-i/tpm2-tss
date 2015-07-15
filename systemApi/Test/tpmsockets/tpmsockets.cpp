//**********************************************************************;
// Copyright (c) 2015, Intel Corporation
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
// THE POSSIBILITY OF SUCH DAMAGE.
//**********************************************************************;

//
// NOTE:  this file is used in two places:  application SAPI (to
// communicate with RM) and RM calls to SAPI (to communicate with
// TPM simulator.
//
// There will be a few small differences between the two uses and
// these will be handled via #ifdef's and different header files.
//

//
// NOTE:  uncomment following if you think you need to see all
// socket communications.
//
//#define DEBUG_SOCKETS

#define DEBUG

#include <stdio.h>
#include <stdlib.h>   // Needed for _wtoi

#include "tpm20.h"
#include "tpmsockets.h"
#include "tss2_sysapi_util.h"
#include "tpmclient.h"

#define TCTI_CONTEXT_INTEL ( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext )

#ifdef __cplusplus
extern "C" {
#endif

extern FILE *outFp;

#ifdef SAPI_CLIENT
extern int TpmClientPrintf( UINT8 type, const char *format, ... );
int (*tpmSocketsPrintf)( UINT8 type, const char *format, ...) = TpmClientPrintf;
#else
extern int ResMgrPrintf( UINT8 type, const char *format, ... );
int (*tpmSocketsPrintf)( UINT8 type, const char *format, ...) = ResMgrPrintf;
#endif

UINT8 rmDebugPrefix = 0;

void DebugPrintBuffer( UINT8 *buffer, UINT32 length )
{
    UINT32  i;
    
    for( i = 0; i < length; i++ )
    {
        if( ( i % 16 ) == 0 )
        {
            (*tpmSocketsPrintf)(NO_PREFIX, "\n");
            PrintRMDebugPrefix();
        }
        
        (*tpmSocketsPrintf)(NO_PREFIX,  "%2.2x ", buffer[i] );
    }
    (*tpmSocketsPrintf)(NO_PREFIX,  "\n\n" );
    fflush( stdout );
}

void DebugPrintBufferOpen( UINT8 *buffer, UINT32 length )
{
    UINT32  i;

    OpenOutFile( &outFp );

    if( outFp != 0 )
    {
        for( i = 0; i < length; i++ )
        {
            if( ( i % 16 ) == 0 )
            {
                (*tpmSocketsPrintf)(NO_PREFIX, "\n");
                PrintRMDebugPrefix();
            }

            (*tpmSocketsPrintf)(NO_PREFIX,  "%2.2x ", buffer[i] );
        }
        (*tpmSocketsPrintf)(NO_PREFIX,  "\n\n" );
        fflush( stdout );
    }

    CloseOutFile( &outFp );
}

#ifdef SHARED_OUT_FILE
static int openCnt = 0;

void OpenOutFile( FILE **outFp )
{
    if( *outFp == 0 )
    {
        if( 0 == strcmp( outFileName, "" ) )
        {
            *outFp = stdout;
        }
        else
        {
            if( openCnt == 0 )
            {
                do
                {
                    *outFp = fopen( &outFileName[0], "a+" );
                }
                while( *outFp == 0 );
            }
            openCnt++;
        }
    }
    else
    {
        if( ( *outFp != stdout ) && ( openCnt < 0xff ) )
            openCnt++;
    }
}

void CloseOutFile( FILE **outFp )
{
    if( 0 != strcmp( outFileName, "" ) )
    {
        if( *outFp != 0 )
        {
            if( openCnt == 1 && *outFp != stdout )
            {
                fclose( *outFp );
                *outFp = 0;
            }
            if( openCnt > 0 )
                openCnt--;
        }
    }
}
#else
void OpenOutFile( FILE **outFilePtr )
{
    *outFilePtr = stdout;
}    

void CloseOutFile( FILE **outFilePtr )
{
}
#endif

void PrintRMDebugPrefix()
{
    if( rmDebugPrefix )
        (*tpmSocketsPrintf)(NO_PREFIX,  "||  " );
}

#ifndef _WIN32
void WSACleanup()
{
}

int WSAGetLastError()
{
    return errno;
}

#endif

TSS2_RC sendBytes( SOCKET tpmSock, const char *data, int len )
{
    int iResult = 0;
    int sentLength = 0;
    
#ifdef DEBUG_SOCKETS
    PrintRMDebugPrefix();
    (*tpmSocketsPrintf)(NO_PREFIX, "Send Bytes to socket #0x%x: \n", tpmSock );
    DebugPrintBuffer( (UINT8 *)data, len );
#endif    

    for( sentLength = 0; sentLength < len; len -= iResult, sentLength += iResult )
    {
        iResult = send( tpmSock, data, len, 0  );
        if (iResult == SOCKET_ERROR) {
            (*tpmSocketsPrintf)(NO_PREFIX, "send failed with error: %d\n", WSAGetLastError() );
//            closesocket(tpmSock);
//            WSACleanup();
//            exit(1);
            return TSS2_TCTI_RC_IO_ERROR;
        }
    }
    
    return TSS2_RC_SUCCESS;
}

TSS2_RC SocketSendSessionEnd(
    TSS2_TCTI_CONTEXT *tctiContext,       /* in */
    UINT8 tpmCmdServer )
{
    UINT32 tpmSendCommand = TPM_SESSION_END;  // Value for "send command" to MS simulator.
    SOCKET sock;

    if( tpmCmdServer )
    {
        sock = TCTI_CONTEXT_INTEL->tpmSock;
    }
    else
    {
        sock = TCTI_CONTEXT_INTEL->otherSock;
    }
        
    tpmSendCommand = CHANGE_ENDIAN_DWORD(tpmSendCommand);
    sendBytes( sock, (char *)&tpmSendCommand, 4 );

    return( TSS2_RC_SUCCESS );
}



TSS2_RC SocketSendTpmCommand(
    TSS2_TCTI_CONTEXT *tctiContext,       /* in */
    size_t             command_size,      /* in */
    uint8_t           *command_buffer     /* in */
    )
{
    UINT32 tpmSendCommand = MS_SIM_TPM_SEND_COMMAND;  // Value for "send command" to MS simulator.
    UINT32 cnt, cnt1;
    UINT8 locality;
#ifdef SAPI_CLIENT    
    UINT8 debugMsgLevel, statusBits;
#endif
    
    UINT32 commandCode = CHANGE_ENDIAN_DWORD( ( (TPM20_Header_In *)command_buffer )->commandCode );

    if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext )->status.debugMsgLevel == TSS2_TCTI_DEBUG_MSG_ENABLED )
    {
#ifdef DEBUG
        (*tpmSocketsPrintf)(NO_PREFIX, "\n" );
        (*tpmSocketsPrintf)(rmDebugPrefix, "Cmd sent: %s\n", commandCodeStrings[ commandCode - TPM_CC_FIRST ]  );
#endif
#ifdef DEBUG_SOCKETS
        (*tpmSocketsPrintf)(rmDebugPrefix, "Command sent on socket #0x%x: %s\n", TCTI_CONTEXT_INTEL->tpmSock, commandCodeStrings[ commandCode - TPM_CC_FIRST ]  );
#endif        
    }
    // Size TPM 1.2 and TPM 2.0 headers overlap exactly, we can use
    // either 1.2 or 2.0 header to get the size.
    cnt = CHANGE_ENDIAN_DWORD(((TPM20_Header_In *) command_buffer)->commandSize);

    // Send TPM_SEND_COMMAND
    tpmSendCommand = CHANGE_ENDIAN_DWORD(tpmSendCommand);
    sendBytes( TCTI_CONTEXT_INTEL->tpmSock, (char *)&tpmSendCommand, 4 );
    
    // Send the locality
    locality = (UINT8)( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.locality;
    sendBytes( TCTI_CONTEXT_INTEL->tpmSock, (char *)&locality, 1 );

#ifdef SAPI_CLIENT    
    // Send the debug level
    debugMsgLevel = (UINT8)( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.debugMsgLevel;
    sendBytes( TCTI_CONTEXT_INTEL->tpmSock, (char *)&debugMsgLevel, 1 );

    // Send status bits
    statusBits = (UINT8)( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.commandSent;
    statusBits |= ( (UINT8)( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.rmDebugPrefix ) << 1;
    sendBytes( TCTI_CONTEXT_INTEL->tpmSock, (char *)&statusBits, 1 );
#endif
    
#ifdef DEBUG
    if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext )->status.debugMsgLevel == TSS2_TCTI_DEBUG_MSG_ENABLED )
    {
        (*tpmSocketsPrintf)(rmDebugPrefix, "Locality = %d", ( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.locality );
    }
#endif
    
    // Send number of bytes.
    cnt1 = cnt;
    cnt = CHANGE_ENDIAN_DWORD(cnt);
    sendBytes( TCTI_CONTEXT_INTEL->tpmSock, (char *)&cnt, 4 );
    
    // Send the TPM command buffer
    sendBytes( TCTI_CONTEXT_INTEL->tpmSock, (char *)command_buffer, cnt1 );
    
#ifdef DEBUG
    if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext )->status.debugMsgLevel == TSS2_TCTI_DEBUG_MSG_ENABLED )
    {
        DEBUG_PRINT_BUFFER( command_buffer, cnt1 );
    }
#endif
    ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.commandSent = 1;

    return TSS2_RC_SUCCESS;
}

TSS2_RC SocketCancel(
    TSS2_TCTI_CONTEXT *tctiContext
    )
{
    TSS2_RC rval = TSS2_RC_SUCCESS;

    if( tctiContext == 0 )
    {
        rval = TSS2_TCTI_RC_BAD_REFERENCE;
    }
    else if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.commandSent != 1 )
    {
        rval = TSS2_TCTI_RC_BAD_SEQUENCE;
    }
    else
    {
        rval = (TSS2_RC)PlatformCommand( tctiContext, MS_SIM_CANCEL_ON );
#if 0
        if( rval == TSS2_RC_SUCCESS )
        {
            rval = (TSS2_RC)PlatformCommand( tctiContext, MS_SIM_CANCEL_OFF );
            if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext )->status.debugMsgLevel == TSS2_TCTI_DEBUG_MSG_ENABLED )
            {
                (*tpmSocketsPrintf)(NO_PREFIX, "%s sent cancel ON command:\n", interfaceName );
            }
        }
#endif        
    }

    return rval;
}

TSS2_RC SocketSetLocality(
    TSS2_TCTI_CONTEXT *tctiContext,       /* in */
    uint8_t           locality     /* in */
    )
{
    TSS2_RC rval = TSS2_RC_SUCCESS;

    if( tctiContext == 0 )
    {
        rval = TSS2_TCTI_RC_BAD_REFERENCE;
    }
    else if( ( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.locality != locality )
    {
        if ( ( (TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.commandSent == 1 )
        {
            rval = TSS2_TCTI_RC_BAD_SEQUENCE;
        }
        else
        {
            ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.locality = locality;
        }
    }

    return rval;
}

void CloseSockets( SOCKET otherSock, SOCKET tpmSock)
{
    closesocket(otherSock);
    closesocket(tpmSock);
}    

void SocketFinalize(
    TSS2_TCTI_CONTEXT *tctiContext       /* in */
    )
{
    // Send session end messages to servers.
    SocketSendSessionEnd( tctiContext, 1 );
    SocketSendSessionEnd( tctiContext, 0 );

    CloseSockets( TCTI_CONTEXT_INTEL->otherSock, TCTI_CONTEXT_INTEL->tpmSock );

    free( tctiContext );
}


TSS2_RC recvBytes( SOCKET tpmSock, unsigned char *data, int len )
{
    int iResult = 0;
    int length;
    int bytesRead;
    
    for( bytesRead = 0, length = len; bytesRead != len; length -= iResult, bytesRead += iResult )
    {
        iResult = recv( tpmSock, (char *)&( data[bytesRead] ), length, 0);
        if (iResult == SOCKET_ERROR) {
            PrintRMDebugPrefix();
            (*tpmSocketsPrintf)(NO_PREFIX, "In recvBytes, recv failed (socket: 0x%x) with error: %d\n",
                    tpmSock, WSAGetLastError() );
//            closesocket(tpmSock);
//            WSACleanup();
//            exit(1);
            return TSS2_TCTI_RC_IO_ERROR;
        }
    }

#ifdef DEBUG_SOCKETS
    (*tpmSocketsPrintf)( rmDebugPrefix, "Receive Bytes from socket #0x%x: \n", tpmSock );
    DebugPrintBuffer( data, len );
#endif
    
    return TSS2_RC_SUCCESS;
}

TSS2_RC SocketReceiveTpmResponse(
    TSS2_TCTI_CONTEXT *tctiContext,     /* in */
    size_t          *response_size,     /* out */
    unsigned char   *response_buffer,    /* in */
    int32_t         timeout
    )
{
    UINT32 trash;
    size_t responseSize = 0;
    TSS2_RC rval = TSS2_RC_SUCCESS;
    fd_set readFds;
    struct timeval tv, *tvPtr;
    int32_t timeoutMsecs = timeout % 1000;
    int iResult;
    
    if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext )->status.debugMsgLevel == TSS2_TCTI_DEBUG_MSG_ENABLED )
    {
#ifdef DEBUG
        (*tpmSocketsPrintf)( rmDebugPrefix, "Response Received: " );
#endif
#ifdef DEBUG_SOCKETS
         (*tpmSocketsPrintf)( rmDebugPrefix, "from socket #0x%x:\n", TCTI_CONTEXT_INTEL->tpmSock );
#endif
    }

    if( *response_size > 0 )
    {
        if( timeout == TSS2_TCTI_TIMEOUT_BLOCK )
        {
            tvPtr = 0;
        }
        else
        {
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = timeoutMsecs * 1000;
            tvPtr = &tv;
        }

        FD_ZERO( &readFds );
        FD_SET( TCTI_CONTEXT_INTEL->tpmSock, &readFds );

        iResult = select( TCTI_CONTEXT_INTEL->tpmSock+1, &readFds, 0, 0, tvPtr );
        if( iResult == 0 )
        {
            (*tpmSocketsPrintf)(NO_PREFIX, "select failed due to timeout, socket #: 0x%x\n", TCTI_CONTEXT_INTEL->tpmSock );
            rval = TSS2_TCTI_RC_TRY_AGAIN;
            goto retSocketReceiveTpmResponse;
        }
        else if( iResult == SOCKET_ERROR )
        {
            (*tpmSocketsPrintf)(NO_PREFIX, "select failed with socket error: %d\n", WSAGetLastError() );
            rval = TSS2_TCTI_RC_IO_ERROR;
            goto retSocketReceiveTpmResponse;
        }
        else if ( iResult != 1 )
        {
            (*tpmSocketsPrintf)(NO_PREFIX, "select failed, read the wrong # of bytes: %d\n", iResult );
            rval = TSS2_TCTI_RC_IO_ERROR;
            goto retSocketReceiveTpmResponse;
        }

        // Receive the size of the response.
        recvBytes( TCTI_CONTEXT_INTEL->tpmSock, (unsigned char *)&responseSize, 4 );
        
		responseSize = CHANGE_ENDIAN_DWORD( responseSize );
                
        // Receive the TPM response.
        recvBytes( TCTI_CONTEXT_INTEL->tpmSock, (unsigned char *)response_buffer, responseSize );

#ifdef DEBUG
        if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext )->status.debugMsgLevel == TSS2_TCTI_DEBUG_MSG_ENABLED )
        {
            DEBUG_PRINT_BUFFER( response_buffer, responseSize );
        }
#endif    

        // Receive the appended four bytes of 0's
        recvBytes( TCTI_CONTEXT_INTEL->tpmSock, (unsigned char *)&trash, 4 );
    }

    if( responseSize < *response_size )
    {
        *response_size = responseSize;
    }
    
    ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.commandSent = 0;

    // Turn cancel off.
    rval = (TSS2_RC)PlatformCommand( tctiContext, MS_SIM_CANCEL_OFF );

    if( ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext )->status.debugMsgLevel == TSS2_TCTI_DEBUG_MSG_ENABLED )
    {
//        (*tpmSocketsPrintf)(NO_PREFIX,  "%s sent cancel OFF command:\n", interfaceName );
    }

retSocketReceiveTpmResponse:
    return rval;
}

#ifdef __cplusplus
}
#endif


TSS2_RC PlatformCommand(
    TSS2_TCTI_CONTEXT *tctiContext,     /* in */
    char cmd )
{
    int iResult = 0;            // used to return function results
    char sendbuf[] = { 0x0,0x0,0x0,0x0 };
    char recvbuf[] = { 0x0, 0x0, 0x0, 0x0 };
	TSS2_RC rval = TSS2_RC_SUCCESS;

    sendbuf[3] = cmd;

    OpenOutFile( &outFp );

    // Send the command
    iResult = send( TCTI_CONTEXT_INTEL->otherSock, sendbuf, 4, 0 );
    
    if (iResult == SOCKET_ERROR) {
        (*tpmSocketsPrintf)(NO_PREFIX, "send failed with error: %d\n", WSAGetLastError() );
        closesocket(TCTI_CONTEXT_INTEL->otherSock);
        WSACleanup();
        rval = TSS2_TCTI_RC_IO_ERROR;
    }
    else
    {
#ifdef DEBUG_SOCKETS
        (*tpmSocketsPrintf)( rmDebugPrefix, "Send Bytes to socket #0x%x: \n", TCTI_CONTEXT_INTEL->otherSock );
        DebugPrintBuffer( (UINT8 *)sendbuf, 4 );
#endif

        // Read result
        iResult = recv( TCTI_CONTEXT_INTEL->otherSock, recvbuf, 4, 0);
        if (iResult == SOCKET_ERROR) {
            (*tpmSocketsPrintf)(NO_PREFIX, "In PlatformCommand, recv failed (socket: 0x%x) with error: %d\n",
                    TCTI_CONTEXT_INTEL->otherSock, WSAGetLastError() );
            closesocket(TCTI_CONTEXT_INTEL->otherSock);
            WSACleanup();
            rval = TSS2_TCTI_RC_IO_ERROR;
        }
        else if( recvbuf[0] != 0 || recvbuf[1] != 0 || recvbuf[2] != 0 || recvbuf[3] != 0 )
        {
            (*tpmSocketsPrintf)(NO_PREFIX, "PlatformCommand failed with error: %d\n", recvbuf[3] );
            closesocket(TCTI_CONTEXT_INTEL->otherSock);
            WSACleanup();
            rval = TSS2_TCTI_RC_IO_ERROR;
        }
        else
        {
#ifdef DEBUG_SOCKETS
            (*tpmSocketsPrintf)(NO_PREFIX, "Receive bytes from socket #0x%x: \n", TCTI_CONTEXT_INTEL->otherSock );
            DebugPrintBuffer( (UINT8 *)recvbuf, 4 );
#endif
        }
    }

    CloseOutFile( &outFp );
    
    return rval;
}

int InitSockets( char *hostName, int port, UINT8 serverSockets, SOCKET *otherSock, SOCKET *tpmSock )
{
    sockaddr_in otherService;
    sockaddr_in tpmService;
    
    int iResult = 0;            // used to return function results

#ifdef _WIN32
    WSADATA wsaData = {0};
    static UINT8 socketsEnabled = 0;

    if( socketsEnabled == 0 )
    {
        // Initialize Winsock
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            (*tpmSocketsPrintf)(NO_PREFIX, "WSAStartup failed: %d\n", iResult);
            return 1;
        }
        socketsEnabled = 1;
    }
#endif
    
    *otherSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*otherSock == INVALID_SOCKET)
    {
        (*tpmSocketsPrintf)(NO_PREFIX, "socket creation failed with error = %d\n", WSAGetLastError() );
        return(1);
    }
    else {
        (*tpmSocketsPrintf)(NO_PREFIX, "socket created:  0x%x\n", *otherSock );
        otherService.sin_family = AF_INET;
        otherService.sin_addr.s_addr = inet_addr( hostName );
        otherService.sin_port = htons(port + 1);

        if( serverSockets )
        {
            // Bind the socket.
            iResult = bind(*otherSock, (SOCKADDR *) &otherService, sizeof (otherService));
            if (iResult == SOCKET_ERROR) {
                (*tpmSocketsPrintf)(NO_PREFIX, "bind failed with error %u\n", WSAGetLastError());
                closesocket(*otherSock);
                WSACleanup();
                return 1;
            }
            else
            {
                (*tpmSocketsPrintf)(NO_PREFIX, "bind to IP address:port:  %s:%d\n", hostName, port + 1 );
            }

            iResult = listen( *otherSock, 4 );
            if (iResult == SOCKET_ERROR) {
                (*tpmSocketsPrintf)(NO_PREFIX, "listen failed with error %u\n", WSAGetLastError());
                closesocket(*otherSock);
                WSACleanup();
                return 1;
            }
            else
            {
                (*tpmSocketsPrintf)(NO_PREFIX, "Other CMD server listening to socket:  0x%x\n", *otherSock );
            }
        }
    }        
        
    *tpmSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*tpmSock == INVALID_SOCKET)
    {
        (*tpmSocketsPrintf)(NO_PREFIX, "socket creation failed with error = %d\n", WSAGetLastError() );
        closesocket( *otherSock );
        return(1);
    }
    else {
        (*tpmSocketsPrintf)(NO_PREFIX, "socket created:  0x%x\n", *tpmSock );
        tpmService.sin_family = AF_INET;
        tpmService.sin_addr.s_addr = inet_addr( hostName );
        tpmService.sin_port = htons( port );

        if( serverSockets )
        {
            // Bind the socket.
            iResult = bind(*tpmSock, (SOCKADDR *) &tpmService, sizeof (tpmService));
            if (iResult == SOCKET_ERROR) {
                (*tpmSocketsPrintf)(NO_PREFIX, "bind failed with error %u\n", WSAGetLastError());
                closesocket(*tpmSock);
                WSACleanup();
                return 1;
            }
            else
            {
                (*tpmSocketsPrintf)(NO_PREFIX, "bind to IP address:port:  %s:%d\n", hostName, port );
            }

            iResult = listen( *tpmSock, 4 );
            if (iResult == SOCKET_ERROR) {
                (*tpmSocketsPrintf)(NO_PREFIX, "listen failed with error %u\n", WSAGetLastError());
                closesocket(*tpmSock);
                WSACleanup();
                return 1;
            }
            else
            {
                (*tpmSocketsPrintf)(NO_PREFIX, "TPM CMD server listening to socket:  0x%x\n", *tpmSock );
            }
        }
    }

    if( !serverSockets )
    {
        // Connect to server.
        iResult = connect(*otherSock, (SOCKADDR *) &otherService, sizeof (otherService));
        if (iResult == SOCKET_ERROR) {
            (*tpmSocketsPrintf)(NO_PREFIX, "connect function failed with error: %d\n", WSAGetLastError() );
            iResult = closesocket(*otherSock);
            WSACleanup();
            return 1;
        }
        else
        {
            (*tpmSocketsPrintf)(NO_PREFIX, "Client connected to server on port:  %d\n", port + 1 );
        }

        // Connect to server.
        iResult = connect(*tpmSock, (SOCKADDR *) &tpmService, sizeof (tpmService));
        if (iResult == SOCKET_ERROR) {
            (*tpmSocketsPrintf)(NO_PREFIX, "connect function failed with error: %d\n", WSAGetLastError() );
            iResult = closesocket(*otherSock);
            if (iResult == SOCKET_ERROR)
            {
                (*tpmSocketsPrintf)(NO_PREFIX, "closesocket function failed with error: %d\n", WSAGetLastError() );
            }
            WSACleanup();
            return 1;
        }
        else
        {
            (*tpmSocketsPrintf)(NO_PREFIX, "Client connected to server on port:  %d\n", port );
        }
    }
    
    return 0;
}

#define HOSTNAME_LENGTH 200
#define PORT_LENGTH 4

TSS2_RC InitTcti (
    TSS2_TCTI_CONTEXT *tctiContext, // OUT
    size_t *contextSize,            // IN/OUT
    const char *config,              // IN
    const uint64_t magic,
    const uint32_t version,
	const char *interfaceName,
    const uint8_t serverSockets
    )
{
    TSS2_RC rval = TSS2_RC_SUCCESS;
    char hostName[200];
    int port;
    SOCKET otherSock;
    SOCKET tpmSock;

    if( tctiContext == NULL )
    {
        *contextSize = sizeof( TSS2_TCTI_CONTEXT_INTEL );
        return TSS2_RC_SUCCESS;
    }
    else
    {
        OpenOutFile( &outFp );
        (*tpmSocketsPrintf)(NO_PREFIX, "Initializing %s Interface\n", interfaceName );

        // Init TCTI context.
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->magic = magic;
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->version = version;
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->transmit = SocketSendTpmCommand;
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->receive = SocketReceiveTpmResponse;
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->finalize = SocketFinalize;
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->cancel = SocketCancel;
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->getPollHandles = 0;
        ((TSS2_TCTI_CONTEXT_COMMON_V1 *)tctiContext)->setLocality = SocketSetLocality;
        ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.locality = 3;
        ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.commandSent = 0;
        ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->status.rmDebugPrefix = 0;
        ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->currentTctiContext = 0;

        // Get hostname and port.
        if( ( strlen( config ) + 2 ) <= ( HOSTNAME_LENGTH  ) )
        {
            if( 1 == sscanf( config, "%199s", hostName ) ) 
            {
                if( strlen( config) - ( strlen( hostName ) + 2 ) <= PORT_LENGTH )
                {
                    if( 1 != sscanf( &config[strlen( hostName )], "%d", &port ) )
                    {
                        return( TSS2_TCTI_RC_BAD_VALUE );
                    }
                }
                else
                {
                    return( TSS2_TCTI_RC_BAD_VALUE );
                }
            }
            else
            {
                return( TSS2_TCTI_RC_BAD_VALUE );
            }
        }
        else
        {
            return( TSS2_TCTI_RC_INSUFFICIENT_BUFFER );
        }

        rval = (TSS2_RC) InitSockets( &hostName[0], port, serverSockets, &otherSock, &tpmSock );
        if( rval == TSS2_RC_SUCCESS )
        {
            ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->otherSock = otherSock;
            ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->tpmSock = tpmSock;
        }
        else
        {
            closesocket( otherSock );
        }            
        CloseOutFile( &outFp );
    }

    return rval;
}

TSS2_RC TeardownTcti (
    TSS2_TCTI_CONTEXT *tctiContext, // OUT
    const char *config,              // IN        
	const char *interfaceName
    )
{
    OpenOutFile( &outFp );
    (*tpmSocketsPrintf)(NO_PREFIX, "Tearing down %s Interface\n", interfaceName );
    CloseOutFile( &outFp );

    ((TSS2_TCTI_CONTEXT_INTEL *)tctiContext)->finalize( tctiContext );

  
    return TSS2_RC_SUCCESS;
}


