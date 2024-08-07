#include <windows.h>
#include <stdio.h>
#include "gvm.h"
#include "SVMM.h"
#include "haxm.h"
#include "SvmmDbgServer.h"


#pragma comment(lib, "ws2_32.lib")  // Winsock Library

#ifdef GVM
extern struct kvm_run* kvm_run;
extern HANDLE hGvm, hGvmCpu, hGvmVm;
extern volatile unsigned char* pio_data;
#endif

extern ULONG64 RamSize;
extern BYTE* MemoryEnd;
extern struct Registers Registers;
SOCKET serverSocket, clientSocket;
ULONG SvmmPacketId;
BYTE buffer[4096];


static ULONG ComputeChecksum(BYTE* in, DWORD size)
{
    ULONG i, sum;

    sum = 0;
    for (i = 0; i < size; i++)
        sum += in[i];
    return sum;
}

VOID SvmmDbgInit(PCSTR DbgCommandLine)
{
    WSADATA wsaData;
    DWORD bytes;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    char * port;
    struct kvm_guest_debug kvm_debug;

    /*
    ip = DbgCommandLine;
    port = strchr(DbgCommandLine, ':');
    if (!port) {
        fprintf(stderr, "error WindbgHandshake missing port\n");
        return STATUS_INVALID_PARAMETER;
    }
    *port++ = 0;
    */
    port = DbgCommandLine;
#ifdef HAXM
    /* enable breakpoint-step */
    memset(&hax_debug, '\0', sizeof(hax_debug));
    debug.control = HAX_DEBUG_ENABLE | HAX_DEBUG_STEP | HAX_DEBUG_USE_SW_BP;
    DeviceIoControl(hHaxCpu, HAX_IOCTL_VCPU_DEBUG, &hax_debug, sizeof(hax_debug), (LPVOID)NULL, 0, &bytes, NULL);
#elif GVM
    //GVM_SET_GUEST_DEBUG

    memset(&kvm_debug, '\0', sizeof(kvm_debug));
    kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_SINGLESTEP | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;
    DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);

#endif

    // Initialize WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr,"WSAStartup failed");
        exit(-1);
    }

    // Create server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        fprintf(stderr,"Socket creation failed");
        exit(-1);
    }

    // Bind server socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(atoi(port)); // Choose a port
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        fprintf(stderr,"Socket binding failed");
        closesocket(serverSocket);
        exit(-1);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        fprintf(stderr,"Listening failed");
        closesocket(serverSocket);
        exit(-1);
    }

    printf("DbgServer listening port %d...\n", atoi(port));

    // Accept a client connection
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket == INVALID_SOCKET) {
        fprintf(stderr,"Client connection failed");
        closesocket(serverSocket);
        exit(-1);
    }
    SvmmPacketId = 0;
}

VOID SvmmDbgRecvAck()
{
    DBG_PACKET_HEADER pktHdr;
    LONG bytesReceived;

    memset(&pktHdr, '\0', sizeof(pktHdr));
    bytesReceived = recv(clientSocket, &pktHdr, sizeof(pktHdr), 0);
    if (bytesReceived == SOCKET_ERROR) {
        fprintf(stderr, "Error receiving data");
        exit(-1);
    }
    else if (bytesReceived == 0) {
        fprintf(stderr, "Client disconnected\n");
        exit(-1);
    }
    if (bytesReceived != sizeof(DBG_PACKET_HEADER)) {
        fprintf(stderr, "bytesReceived < sizeof(PACKET_HEADER)\n");
        exit(-1);
    }
    if (pktHdr.Magic != DBG_MAGIC) {
        fprintf(stderr, "pktHdr->Magic != DBG_MAGIC\n");
        exit(-1);
    }
    if (++SvmmPacketId != pktHdr.Id) {
        fprintf(stderr, "SvmmPacketId != pktHdr->Id\n");
        exit(-1);
    }

}
/*
VOID SvmmDbgSendAck()
{
    DBG_PACKET_HEADER pktHdr;
    LONG bytesSend;

    memset(&pktHdr, '\0', sizeof(pktHdr));
    pktHdr.Id = ++SvmmPacketId;
    pktHdr.Magic = DBG_MAGIC;
    pktHdr.Type = DBG_TYPE_ACK;
    bytesSend = send(clientSocket, &pktHdr, sizeof(pktHdr), 0);
    if (bytesSend != sizeof(pktHdr)) {
        fprintf(stderr, "bytesSend != sizeof(pktHdr)\n");
        exit(-1);
    }

}
*/
VOID SvmmDbgRecv(USHORT Type, BYTE* Buffer, SIZE_T Size)
{
    PDBG_PACKET_HEADER pktHdr;
    LONG bytesReceived;
    BYTE pkt[DBG_MAX_PACKET_DATA_SIZE + sizeof(DBG_PACKET_HEADER)];

    memset(pkt, '\0', sizeof(pkt));
    bytesReceived = recv(clientSocket, pkt, sizeof(DBG_PACKET_HEADER) + Size, 0);
    //printf("packet received %lld bytes\n", bytesReceived);

    pktHdr = (PDBG_PACKET_HEADER)pkt;

    if (bytesReceived == SOCKET_ERROR) {
        fprintf(stderr, "Error receiving data");
        exit(-1);
    }
    else if (bytesReceived == 0) {
        fprintf(stderr, "Client disconnected\n");
        exit(-1);
    }
    if (bytesReceived != sizeof(DBG_PACKET_HEADER) + Size) {
        fprintf(stderr, "bytesReceived != sizeof(DBG_PACKET_HEADER)\n");
        exit(-1);
    }
    if (pktHdr->Magic != DBG_MAGIC) {
        fprintf(stderr, "pktHdr->Magic != DBG_MAGIC\n");
        exit(-1);
    }
    if (++SvmmPacketId != pktHdr->Id) {
        fprintf(stderr, "SvmmPacketId != pktHdr->Id\n");
        exit(-1);
    }
    if (Type != pktHdr->Type) {
        fprintf(stderr, "Type != pktHdr->Type\n");
        exit(-1);
    }
    if (Buffer)
        memcpy(Buffer, pktHdr->Data, pktHdr->Size);
}

VOID SvmmDbgSend(USHORT Type, BYTE* Buffer, SIZE_T Size)
{
    PDBG_PACKET_HEADER pktHdr;
    LONG bytesSend;

    pktHdr = calloc(sizeof(BYTE), sizeof(DBG_PACKET_HEADER) + Size);
    pktHdr->Id = ++SvmmPacketId;
    pktHdr->Magic = DBG_MAGIC;
    pktHdr->Type = Type;
    pktHdr->Size = Size;
    if (Buffer) {
        memcpy(pktHdr->Data, (void*)Buffer, Size);
        pktHdr->Checksum = ComputeChecksum(pktHdr->Data, pktHdr->Size);
    }
    bytesSend = send(clientSocket, pktHdr, sizeof(DBG_PACKET_HEADER) + Size, 0);
    if (bytesSend != sizeof(DBG_PACKET_HEADER) + Size) {
        fprintf(stderr, "bytesSend != sizeof(DBG_PACKET_HEADER) + Size\n");
        exit(-1);
    }
    free(pktHdr);
    //SvmmDbgRecvAck();
}

BYTE SvmmDbgLoop()
{
    BOOL ret;
    fd_set readSet;
    LONG bytes;
    LONG64 i, result, bytesReceived, dataToRead;
    PDBG_PACKET_HEADER pktHdr;
    struct kvm_guest_debug kvm_debug;
    PDBG_PACKET_READ_REQUEST pktReadReq;
    PDBG_PACKET_WRITE_REQUEST pktWriteReq;
    PDBG_PACKET_INIT_REQUEST pktInitReq;
    PDBG_PACKET_PRINT_REQUEST pktPrintReq;
    DBG_PACKET_INIT_REPLY pktInitRep;
    struct kvm_msrs* msrs;

    while (1) {
        FD_ZERO(&readSet);
        FD_SET(clientSocket, &readSet);

        // Use select to check if there's data to read from the client
        result = select(clientSocket + 1, &readSet, NULL, NULL, NULL);
        if (result == SOCKET_ERROR) {
            fprintf(stderr, "Select failed\n");
            exit(-1);
        }
        if (result < 0 || !FD_ISSET(clientSocket, &readSet))
            break;
        
        memset(buffer, '\0', sizeof(buffer));
        bytesReceived = recv(clientSocket, buffer, sizeof(DBG_PACKET_HEADER), 0);
        //printf("packet received %lld bytes\n", bytesReceived);
        if (bytesReceived == SOCKET_ERROR) {
            fprintf(stderr, "Error receiving data");
            exit(-1);
        }
        else if (bytesReceived == 0) {
            fprintf(stderr, "Client disconnected\n");
            exit(-1);
        }
        if (bytesReceived != sizeof(DBG_PACKET_HEADER)) {
            fprintf(stderr, "bytesReceived < sizeof(PACKET_HEADER)\n");
            exit(-1);
        }
        pktHdr = (PDBG_PACKET_HEADER)buffer;
        if (pktHdr->Magic != DBG_MAGIC) {
            fprintf(stderr, "pktHdr->Magic != DBG_MAGIC\n");
            exit(-1);
        }
        if (++SvmmPacketId != pktHdr->Id) {
            fprintf(stderr, "SvmmPacketId != pktHdr->Id\n");
            exit(-1);
        }
        if (pktHdr->Size) {
            dataToRead = pktHdr->Size > sizeof(buffer) - sizeof(DBG_PACKET_HEADER) ? sizeof(buffer) - sizeof(DBG_PACKET_HEADER) : pktHdr->Size;
            bytesReceived = recv(clientSocket, buffer + sizeof(DBG_PACKET_HEADER), dataToRead, 0);
            if (bytesReceived && pktHdr->Checksum != ComputeChecksum(pktHdr->Data, dataToRead)) {
                fprintf(stderr, "pktHdr->Checksum != ComputeChecksum()\n");
                exit(-1);
            }
        }
        //SvmmDbgSendAck();
        switch (pktHdr->Type) {
            case DBG_TYPE_READ_REGISTERS:
                //SvmmGetRegisters(&Registers);
                SvmmDbgSend(DBG_TYPE_READ_REGISTERS, (BYTE *)&Registers, sizeof(struct Registers));
                break;
            case DBG_TYPE_WRITE_REGISTERS:
                //SvmmDbgRecv(DBG_TYPE_WRITE_REGISTERS, (BYTE*)&Registers, sizeof(struct Registers));
                if (pktHdr->Size != sizeof(Registers)) {
                    fprintf(stderr, "pktHdr->Size != sizeof(Registers)\n");
                    exit(-1);
                }
                memcpy(&Registers, pktHdr->Data, sizeof(Registers));
                SvmmSetRegisters(&Registers);
                if (Registers.context._dr7 & (DR1_ENABLED | DR2_ENABLED | DR3_ENABLED | DR4_ENABLED)) {
                    memset(&kvm_debug, '\0', sizeof(kvm_debug));
                    kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;
                    kvm_debug.arch.debugreg[0] = Registers.context._dr0;
                    kvm_debug.arch.debugreg[1] = Registers.context._dr1;
                    kvm_debug.arch.debugreg[2] = Registers.context._dr2;
                    kvm_debug.arch.debugreg[3] = Registers.context._dr3;
                    //kvm_debug.arch.debugreg[6] = DR6_RESERVED;
                    kvm_debug.arch.debugreg[7] = Registers.context._dr7;
                    DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);
                }
                break;
            case DBG_TYPE_READ_MEMORY:
                if (pktHdr->Size != sizeof(DBG_PACKET_READ_REQUEST)) {
                    fprintf(stderr, "pktHdr->Size != sizeof(DBG_PACKET_READ_REQUEST)\n");
                    exit(-1);
                }
                pktReadReq = (PDBG_PACKET_READ_REQUEST)pktHdr->Data;
                //printf("read request addr 0x%llx size 0x%llx\n", pktReadReq->Addr, pktReadReq->Size);
                
                if (SvmmGetHostPageFromGPA(pktReadReq->Addr) + pktReadReq->Size >= MemoryEnd) {
                    fprintf(stderr, "SvmmGetHostAddress(Req->Addr) + Req->Size >= MemoryEnd)\n");
                    exit(-1);
                }
                
                for (i = 0; i < (LONG64)pktReadReq->Size; i += DBG_MAX_PACKET_DATA_SIZE) {
                    SvmmDbgSend(DBG_TYPE_READ_MEMORY, SvmmGetHostPageFromGPA(pktReadReq->Addr + i),
                        (pktReadReq->Size - i) > DBG_MAX_PACKET_DATA_SIZE ? DBG_MAX_PACKET_DATA_SIZE : (pktReadReq->Size - i));
                }
                break;
            case DBG_TYPE_WRITE_MEMORY:
                if (pktHdr->Size != sizeof(DBG_PACKET_WRITE_REQUEST)) {
                    fprintf(stderr, "pktHdr->Size != sizeof(DBG_PACKET_WRITE_REQUEST)\n");
                    exit(-1);
                }
                pktWriteReq = (PDBG_PACKET_WRITE_REQUEST)pktHdr->Data;
                
                if (SvmmGetHostPageFromGPA(pktWriteReq->Addr) + pktWriteReq->Size >= MemoryEnd) {
                    fprintf(stderr, "SvmmGetHostAddress(Req->Addr) + Req->Size >= MemoryEnd)\n");
                    exit(-1);
                }
                for (i = 0; i < (LONG64)pktWriteReq->Size; i += DBG_MAX_PACKET_DATA_SIZE) {
                    SvmmDbgRecv(DBG_TYPE_WRITE_MEMORY, SvmmGetHostPageFromGPA(pktWriteReq->Addr + i),
                        (pktWriteReq->Size - i) > DBG_MAX_PACKET_DATA_SIZE ? DBG_MAX_PACKET_DATA_SIZE : (pktWriteReq->Size - i));
                }
                if (pktWriteReq->Size == 1 && *SvmmGetHostPageFromGPA(pktWriteReq->Addr) == 0xcc) {
                    memset(&kvm_debug, '\0', sizeof(kvm_debug));
                    kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;
                    DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);
                }

                break;
            case DBG_TYPE_INIT_REQ:
                if (pktHdr->Size != sizeof(DBG_PACKET_INIT_REQUEST)) {
                    fprintf(stderr, "pktHdr->Size != sizeof(DBG_PACKET_INIT_REQUEST)\n");
                    exit(-1);
                }
                pktInitReq = (PDBG_PACKET_INIT_REQUEST)pktHdr->Data;
                if (pktInitReq->Id != 1) {
                    fprintf(stderr, "pktInitReq->Id != 1\n");
                    exit(-1);
                }
                memset(&pktInitRep, '\0', sizeof(pktInitRep));
                pktInitRep.MemorySize = RamSize;
                pktInitRep.Id = SvmmPacketId;
                SvmmDbgSend(DBG_TYPE_INIT_REP, &pktInitRep, sizeof(DBG_PACKET_INIT_REPLY));
                printf("init!\n");
                break;
            case DBG_TYPE_STEP_INTO:
                memset(&kvm_debug, '\0', sizeof(kvm_debug));
                kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_SINGLESTEP | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;
                DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);
                return DBG_TYPE_STEP_INTO;
            case DBG_TYPE_STEP_OVER:
            case DBG_TYPE_RUN:
                memset(&kvm_debug, '\0', sizeof(kvm_debug));
                kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;

                kvm_debug.arch.debugreg[0] = Registers.context._dr0;
                kvm_debug.arch.debugreg[1] = Registers.context._dr1;
                kvm_debug.arch.debugreg[2] = Registers.context._dr2;
                kvm_debug.arch.debugreg[3] = Registers.context._dr3;
                //kvm_debug.arch.debugreg[6] = DR6_RESERVED;
                kvm_debug.arch.debugreg[7] = Registers.context._dr7;
                DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);
                
                return pktHdr->Type;
            case DBG_TYPE_PRINT:
                printf("%.s\n", pktHdr->Size, (CHAR *)pktHdr->Data);
                break;
            case DBG_TYPE_READ_MSR_REGISTERS:
                msrs = (struct kvm_msrs *)pktHdr->Data;
                ret = DeviceIoControl(hGvmCpu, GVM_GET_MSRS,
                    msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs,
                    msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs, &bytes,
                    (LPOVERLAPPED)NULL);
                printf("DBG_TYPE_READ_MSR_REGISTERS msrs %d\n", msrs->nmsrs);
                for (i = 0; i < msrs->nmsrs; i++) {
                    printf("msrs[%d] 0x%x= 0x%llx\n", i, msrs->entries[i].index, msrs->entries[i].data);
                }
                SvmmDbgSend(DBG_TYPE_READ_MSR_REGISTERS, (BYTE*)msrs, 
                    sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs);

                break;
            case DBG_TYPE_WRITE_MSR_REGISTERS:
                msrs = (struct kvm_msrs *)pktHdr->Data;
                ret = DeviceIoControl(hGvmCpu, GVM_SET_MSRS,
                    msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs,
                    msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs, &bytes,
                    (LPOVERLAPPED)NULL);
                SvmmDbgSend(DBG_TYPE_READ_REGISTERS, (BYTE *)msrs,
                    sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs);

                break;
            case DBG_TYPE_STATE_CHANGE_REQ:
                SvmmDbgSend(DBG_TYPE_STATE_CHANGE_REP, (BYTE*)NULL, 0);

                break;

        }
    }

	return 0;
}