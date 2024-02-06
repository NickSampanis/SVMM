#include "windbg.h"
#include "SVMM.h"
#include "haxm.h"
#include "gvm.h"
#include <stdio.h>
#include <winsock.h>
#include <winioctl.h>

#pragma comment(lib, "Ws2_32.lib")

struct sockaddr_in server;
extern struct Registers Registers;

#ifdef HAXM
extern HANDLE hHaxCpu;
#elif GVM
extern HANDLE hGvmCpu;
#endif


//Globals
SOCKET windbg_sock;
LONG packetId;
BYTE Stepping;
BREAKPOINT_ENTRY KdBreakpointTable[32];
static BYTE ctxBuffer[0x1000];
static ULONG ctxSize;

//WINDBG CODE

ULONG ComputeChecksum(BYTE* in, DWORD size)
{
	ULONG i, sum;

	sum = 0;
	for (i = 0; i < size; i++)
		sum += in[i];
	return sum;
}

LONG KdSendPacket(KD_PACKET* req)
{
	req->checksum = ComputeChecksum(req->data, (DWORD)req->length);
	return sendto(windbg_sock, (const char*)req, (int)sizeof(KD_PACKET) + req->length, 0, (struct sockaddr*)&server, (int)sizeof(server));
}

LONG KdSendPacketAck(KD_PACKET* req)
{
	KD_PACKET* rep;
	BYTE buffer[1024];
	LONG ret;

	rep = NULL;
	ret = 0;
	do {
		req->checksum = ComputeChecksum(req->data, req->length);
		sendto(windbg_sock, (const char*)req, (int)sizeof(KD_PACKET) + req->length, 0, (struct sockaddr*)&server, (int)sizeof(server));
		do {
			memset(buffer, '\0', sizeof(buffer));
			ret = recvfrom(windbg_sock, (char*)buffer, (int)sizeof(buffer), 0, NULL, NULL);
		} while (ret == -1 || ret == 0);
		rep = (KD_PACKET*)buffer;
        //req->id = (rep->id & 0x7fffffff) + 2;
	} while (rep->packetType != KD_PACKET_TYPE_ACKNOWLEDGE);

	return ret;
}

NTSTATUS WindbgHandshake(PCSTR windbg_host)
{
	LONG status, timeout;
	CHAR *port;
	uint8_t buffer[2048];
	DWORD bytes;
	PKD_PACKET pkt;
	struct hax_debug_t hax_debug;
    struct kvm_guest_debug kvm_debug;
	PDBGKD_ANY_WAIT_STATE_CHANGE waitChange;


	/* setup keys */
	port = strchr(windbg_host, ':');
	if (!port) {
		fprintf(stderr,"error WindbgHandshake missing port\n");
		return STATUS_INVALID_PARAMETER;
	}
	*port++ = 0;

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

	

	memset(&KdBreakpointTable, '\0', sizeof(KdBreakpointTable));
	/* Socket connect */
#if defined(__MINGW32__) || defined(_MSC_VER)
	WSADATA wsaData;
	WSAStartup(2, &wsaData);
#endif
	windbg_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (windbg_sock == -1) {
		fprintf(stderr, "error WindbgHandshake socket\n");
        return STATUS_INVALID_PARAMETER;
	}
	timeout = 1;
	setsockopt(windbg_sock,
		SOL_SOCKET,
		SO_RCVTIMEO,
		(const char*)&timeout,
		sizeof(timeout));
	packetId = 0;

	memset(&server, '\0', sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(port));
	server.sin_addr.S_un.S_addr = inet_addr(windbg_host);
	status = connect(windbg_sock, (struct sockaddr*)&server, sizeof(server));
	if (status == -1) {
		fprintf(stderr, "error WindbgHandshake connect\n");
        return STATUS_INVALID_PARAMETER;
	}
    //wait for windbg
    do {
        Sleep(1000);
        memset(buffer, 0x41, sizeof(buffer));
        status = sendto(windbg_sock, (const char*)buffer, 512, 0, (struct sockaddr*)&server, sizeof(server));
        memset(buffer, '\0', sizeof(buffer));
        status = recvfrom(windbg_sock, (char*)buffer, sizeof(buffer), 0, NULL, NULL);
        if (status == -1)
            continue;
        memset(buffer, '\0', sizeof(buffer));

        pkt = (PKD_PACKET)buffer;
        pkt->packetLeader = KD_PACKET_DATA;
        pkt->packetType = KD_PACKET_TYPE_STATE_CHANGE64;
        pkt->length = sizeof(DBGKD_ANY_WAIT_STATE_CHANGE);
        pkt->id = packetId;
        waitChange = (PDBGKD_ANY_WAIT_STATE_CHANGE)(pkt + 1);
        waitChange->NewState = DbgKdExceptionStateChange;
        waitChange->ProcessorLevel = 0;
        waitChange->Processor = 0;
        waitChange->NumberProcessors = 1;
        waitChange->Thread = 0;
        waitChange->u.Exception.FirstChance = 0x1;
        waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
        //waitChange->u.Exception.ExceptionRecord.ExceptionAddress = BX_CPU_THIS->get_rip() | (BX_CPU_THIS->sregs[BX_SEG_REG_CS].selector.value << 16);
        waitChange->u.Exception.ExceptionRecord.ExceptionAddress = (Registers.context._cr0 & 1) ? Registers.context._rip : (Registers.context._rip | ((ULONG64)Registers.context._cs.selector << 4));
        waitChange->ProgramCounter = waitChange->u.Exception.ExceptionRecord.ExceptionAddress;
        waitChange->u.Exception.ExceptionRecord.ExceptionFlags = 0;
        waitChange->u.Exception.ExceptionRecord.ExceptionRecord = 0;
        waitChange->u.Exception.ExceptionRecord.NumberParameters = 0;
        waitChange->ControlReport.Dr6 = Registers.context._dr6;
        waitChange->ControlReport.Dr7 = Registers.context._dr7;
        waitChange->ControlReport.EFlags = (ULONG)Registers.context._rflags;
        waitChange->ControlReport.ReportFlags = 3;
        waitChange->ControlReport.SegCs = Registers.context._cs.selector;
        waitChange->ControlReport.SegDs = Registers.context._ds.selector;
        waitChange->ControlReport.SegEs = Registers.context._es.selector;
        waitChange->ControlReport.SegFs = Registers.context._fs.selector;
        memcpy((void*)waitChange->ControlReport.InstructionStream, SvmmGetHostAddress(Registers.context._rip), 16);
        //memcpy((BYTE*)waitChange->ControlReport.InstructionStream, (BYTE*)BX_CPU_THIS->getHostMemAddr(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 0), 16);
        //BX_CPU_THIS_PTR access_read_physical(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 16, (void*)waitChange->ControlReport.InstructionStream);
        KdSendPacketAck(pkt);
    } while (status == -1);
    
    return 0;
}
VOID KdLoadSymbols(CHAR* ImageName, ULONG64 ImageBase, ULONG64 ImageSize, ULONG64 CheckSum)
{
    BYTE buffer[2048];
    PDBGKD_ANY_WAIT_STATE_CHANGE waitChange;
    PKD_PACKET pkt;

    memset(buffer, '\0', sizeof(buffer));
    pkt = (PKD_PACKET)buffer;
    pkt->packetLeader = KD_PACKET_DATA;
    pkt->packetType = KD_PACKET_TYPE_STATE_CHANGE64;
    pkt->length = sizeof(DBGKD_ANY_WAIT_STATE_CHANGE) + strlen(ImageName) + 1;
    packetId += 2;
    pkt->id = packetId;

    waitChange = (PDBGKD_ANY_WAIT_STATE_CHANGE)(pkt + 1);
    waitChange->NewState = DbgKdLoadSymbolsStateChange;
    waitChange->ProcessorLevel = 0;
    waitChange->Processor = 0;
    waitChange->NumberProcessors = 1;
    waitChange->Thread = 0;

    waitChange->u.LoadSymbols.BaseOfDll = ImageBase;
    waitChange->u.LoadSymbols.CheckSum = CheckSum;
    waitChange->u.LoadSymbols.SizeOfImage = ImageSize;
    waitChange->u.LoadSymbols.UnloadSymbols = FALSE;
    waitChange->u.LoadSymbols.PathNameLength = strlen(ImageName) + 1;

    waitChange->ControlReport.Dr6 = Registers.context._dr6;
    waitChange->ControlReport.Dr7 = Registers.context._dr7;
    waitChange->ControlReport.EFlags = (ULONG)Registers.context._rflags;
    waitChange->ControlReport.ReportFlags = 3;
    waitChange->ControlReport.SegCs = Registers.context._cs.selector;
    waitChange->ControlReport.SegDs = Registers.context._ds.selector;
    waitChange->ControlReport.SegEs = Registers.context._es.selector;
    waitChange->ControlReport.SegFs = Registers.context._fs.selector;

    memcpy((void*)waitChange->ControlReport.InstructionStream, SvmmGetHostAddress(Registers.context._rip), 16);
    memcpy(waitChange + 1, ImageName, strlen(ImageName));

    KdSendPacketAck(pkt);

}


VOID KdSendSingleStep()
{
    BYTE buffer[2048];
    PDBGKD_ANY_WAIT_STATE_CHANGE waitChange;
    PKD_PACKET pkt;

    memset(buffer, '\0', sizeof(buffer));
    pkt = (PKD_PACKET)buffer;
    pkt->packetLeader = KD_PACKET_DATA;
    pkt->packetType = KD_PACKET_TYPE_STATE_CHANGE64;
    pkt->length = sizeof(DBGKD_ANY_WAIT_STATE_CHANGE);
    packetId += 2;
    pkt->id = packetId;

    waitChange = (PDBGKD_ANY_WAIT_STATE_CHANGE)(pkt + 1);
    waitChange->NewState = DbgKdExceptionStateChange;
    waitChange->ProcessorLevel = 0;
    waitChange->Processor = 0;
    waitChange->NumberProcessors = 1;
    waitChange->Thread = 0;
    waitChange->u.Exception.FirstChance = 0x1;
    if (Registers.context._rflags & EFLAGS_TF_MASK)
        waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_SINGLE_STEP;
    else
        waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
    waitChange->u.Exception.ExceptionRecord.ExceptionAddress = (Registers.context._cr0 & 1) ? Registers.context._rip : (Registers.context._rip | ((ULONG64)Registers.context._cs.selector << 4));
    waitChange->ProgramCounter = waitChange->u.Exception.ExceptionRecord.ExceptionAddress;
    waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
    waitChange->u.Exception.ExceptionRecord.ExceptionFlags = 0;
    waitChange->u.Exception.ExceptionRecord.ExceptionRecord = 0;
    waitChange->u.Exception.ExceptionRecord.NumberParameters = 0;
    waitChange->ControlReport.Dr6 = Registers.context._dr6;
    waitChange->ControlReport.Dr7 = Registers.context._dr7;
    waitChange->ControlReport.EFlags = (ULONG)Registers.context._rflags;
    waitChange->ControlReport.ReportFlags = 3;
    waitChange->ControlReport.SegCs = Registers.context._cs.selector;
    waitChange->ControlReport.SegDs = Registers.context._ds.selector;
    waitChange->ControlReport.SegEs = Registers.context._es.selector;
    waitChange->ControlReport.SegFs = Registers.context._fs.selector;

    memcpy((void*)waitChange->ControlReport.InstructionStream, SvmmGetHostAddress(Registers.context._rip), 16);

    //BX_CPU_THIS_PTR access_read_physical(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 16, (void*)waitChange->ControlReport.InstructionStream);
    //memcpy((BYTE*)waitChange->ControlReport.InstructionStream, (BYTE*)BX_CPU_THIS->getHostMemAddr(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 0), 16);
    /*
    struct kvm_guest_debug kvm_debug;
    DWORD bytes;
    memset(&kvm_debug, '\0', sizeof(kvm_debug));
    kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_SINGLESTEP | GVM_GUESTDBG_USE_SW_BP;
    DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);
    */

    KdSendPacketAck(pkt);

}

LONG __stdcall
KdParsePacket(KD_PACKET* req)
{
    LONG i;
    UCHAR tmp;
    KD_PACKET* rep;
    __CONTEXT ctx;
    DWORD bytes;
    static uint8_t repBuffer[2048];
    struct hax_debug_t hax_debug;
    struct kvm_guest_debug kvm_debug;
    PCONTEXT_EX ctxEx;
    PXSAVE_AREA_HEADER  XStateHeader;
    AMD64_KSPECIAL_REGISTERS special;
    PDBGKD_MANIPULATE_STATE64 manipReq, manipRep;
    PDBGKD_ANY_WAIT_STATE_CHANGE waitChange;

    memset(repBuffer, '\0', sizeof(repBuffer));
    memset(&ctxEx, '\0', sizeof(ctxEx));
    memset(&ctx, '\0', sizeof(ctx));
    //tmp = NULL;
    i = 0;
    rep = (KD_PACKET*)repBuffer;
    if (req->packetLeader == KD_PACKET_CTRL) {
        switch (req->packetType) {
        case KD_PACKET_TYPE_RESET:
            /* Send KD RESET ACK */
            memset(rep, '\0', sizeof(KD_PACKET));
            rep->packetLeader = KD_PACKET_CTRL;
            rep->packetType = KD_PACKET_TYPE_RESET;
            KdSendPacket(rep);

            /* Send State Change */
            memset(rep, '\0', sizeof(KD_PACKET) + sizeof(DBGKD_ANY_WAIT_STATE_CHANGE));
            rep->packetLeader = KD_PACKET_DATA;
            rep->packetType = KD_PACKET_TYPE_STATE_CHANGE64;
            rep->length = sizeof(DBGKD_ANY_WAIT_STATE_CHANGE);
            rep->id = INITIAL_PACKET_ID;
            //packetId = 0;
            waitChange = (PDBGKD_ANY_WAIT_STATE_CHANGE)(rep + 1);

            waitChange->NewState = DbgKdLoadSymbolsStateChange;
            waitChange->ProcessorLevel = 0;
            waitChange->Processor = 0;
            waitChange->NumberProcessors = 1;
            waitChange->ProgramCounter = 0xffff;
            waitChange->Thread = 0; // *(ULONG64*)(KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbCurrentThread);
            waitChange->u.LoadSymbols.BaseOfDll = 0; // KernelBase;
            waitChange->u.LoadSymbols.CheckSum = 0; // *(DWORD*)(KernelBase + PE_OFFSET(KernelBase) + NT_HDR_SIZE + CHKSUM_OFFSET);
            waitChange->u.LoadSymbols.ProcessId = 0;
            waitChange->u.LoadSymbols.SizeOfImage = 0;
            waitChange->u.LoadSymbols.UnloadSymbols = 0;

            /*
            waitChange->NewState = DbgKdExceptionStateChange;
            waitChange->ProcessorLevel = 0;
            waitChange->Processor = 0;
            waitChange->NumberProcessors = 1;
            waitChange->ProgramCounter = ((CONTEXT*)(KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbProcStateContext))->Rip;
            waitChange->Thread = *(ULONG64*)(KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbCurrentThread);
            */
            waitChange->u.LoadSymbols.PathNameLength = 0x22;
            waitChange->u.LoadSymbols.BaseOfDll = 0; // KernelBase;
            waitChange->u.LoadSymbols.CheckSum = 0; // *(DWORD*)(KernelBase + PE_OFFSET(KernelBase) + NT_HDR_SIZE + CHKSUM_OFFSET);
            waitChange->u.LoadSymbols.ProcessId = 0xffffffff;
            waitChange->u.LoadSymbols.SizeOfImage = 0; // *(DWORD*)(KernelBase + PE_OFFSET(KernelBase) + NT_HDR_SIZE + SIZE_IMAGE_OFFSET);;
            waitChange->u.LoadSymbols.UnloadSymbols = 0;

            /*
            waitChange->u.Exception.FirstChance = 1;

            waitChange->u.Exception.ExceptionRecord = *ExceptionRecord;

            waitChange->u.Exception.ExceptionRecord.ExceptionCode = 0x80000003L;
            */
            waitChange->ControlReport.Dr6 = 0;
            waitChange->ControlReport.Dr7 = 0;
            waitChange->ControlReport.EFlags = 0;
            waitChange->ControlReport.ReportFlags = 3;
            waitChange->ControlReport.SegCs = 0;
            waitChange->ControlReport.SegDs = 0;
            waitChange->ControlReport.SegEs = 0;
            waitChange->ControlReport.SegFs = 0;
            memcpy((BYTE*)waitChange->ControlReport.InstructionStream, (BYTE*)"\x41", 1);
            //BX_CPU_THIS_PTR access_read_physical(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 16, (void*)waitChange->ControlReport.InstructionStream);
            KdSendPacket(rep);
            break;
        case KD_PACKET_TYPE_RESEND:
            KdSendPacket(rep);
            break;
        case KD_PACKET_TYPE_ACKNOWLEDGE:
            /* Send ack */
            memset(rep, '\0', sizeof(KD_PACKET));
            rep->packetLeader = KD_PACKET_CTRL;
            rep->packetType = KD_PACKET_TYPE_ACKNOWLEDGE;
            //rep->id = (req->id & 0x7fffffff) + 2;
            packetId += 2;
            rep->id = packetId;
            KdSendPacket(rep);
            break;
        default:
            //NotSupported(req, NULL);
            break;
        }
    }
    else if (req->packetLeader == KD_PACKET_DATA &&
        req->packetType == KD_PACKET_TYPE_STATE_MANIPULATE) {
        manipReq = (PDBGKD_MANIPULATE_STATE64)(req + 1);
        /* Send ack */
        memset(rep, '\0', sizeof(KD_PACKET));
        rep->packetLeader = KD_PACKET_CTRL;
        rep->packetType = KD_PACKET_TYPE_ACKNOWLEDGE;
        rep->id = req->id;
        KdSendPacket(rep);
        /* set reply header common values */
        memset(rep, '\0', sizeof(KD_PACKET));
        rep->packetLeader = KD_PACKET_DATA;
        rep->packetType = KD_PACKET_TYPE_STATE_MANIPULATE;
        //packetId = (~0x80000000 & req->id) + 2;
        packetId += 2;
        rep->id = packetId;
        rep->length = sizeof(DBGKD_MANIPULATE_STATE64);
        manipRep = (PDBGKD_MANIPULATE_STATE64)(rep + 1);
        memset(manipRep, '\0', sizeof(DBGKD_MANIPULATE_STATE64));
        printf("api 0x%x\n", manipReq->ApiNumber);
        switch (manipReq->ApiNumber) {
        case DbgKdGetVersionApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;

            manipRep->ApiNumber = DbgKdGetVersionApi;
            manipRep->u.GetVersion64.MajorVersion = 4 << 8;
            manipRep->u.GetVersion64.MinorVersion = MINOR_VERSION;
            manipRep->u.GetVersion64.ProtocolVersion = MAJOR_PROT_VERSION;
            manipRep->u.GetVersion64.KdSecondaryVersion = MINOR_PROT_VERSION;
            manipRep->u.GetVersion64.Flags = 0x7;
            manipRep->u.GetVersion64.MachineType = KD_MACH_AMD64;
            manipRep->u.GetVersion64.MaxPacketType = KD_MAX_PACKET_TYPE;
            manipRep->u.GetVersion64.MaxManipulate = KD_MAX_MANIPULATE;
            manipRep->u.GetVersion64.KernBase = 0x1000;
            manipRep->u.GetVersion64.PsLoadedModuleList = 0; // dbgData.PsLoadedModuleList;
            manipRep->u.GetVersion64.DebuggerDataList = 0;// dbgData.Header.List.Blink;
            //memcpy((BYTE*)&manipRep->u.GetVersion64, (BYTE*)&KdVersionBlock, sizeof(KdVersionBlock));
            rep->length += sizeof(DBGKD_GET_VERSION64);
            KdSendPacketAck(rep);
            break;
        case DbgKdReadVirtualMemoryApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
            manipRep->u.ReadMemory.ActualBytesRead = min(manipReq->u.ReadMemory.TransferCount, KD_MAX_PACKET_SIZE - sizeof(DBGKD_MANIPULATE_STATE64) - sizeof(KD_PACKET));
#ifdef __DEBUG__
            printf("addr = 0x%llx\n", manipRep->u.ReadMemory.TargetBaseAddress);
#endif
            if (manipRep->u.ReadMemory.TargetBaseAddress != 0xfffff780000003d8 && manipRep->u.ReadMemory.TargetBaseAddress != 0) {
                //&& BX_CPU_THIS->getHostMemAddr(manipRep->u.ReadMemory.TargetBaseAddress, 0)) {
                //memcpy((BYTE*)(manipRep + 1), (BYTE*)BX_CPU_THIS->getHostMemAddr(manipRep->u.ReadMemory.TargetBaseAddress, 0), manipRep->u.ReadMemory.ActualBytesRead);
                //BX_CPU_THIS_PTR access_read_physical(manipRep->u.ReadMemory.TargetBaseAddress, manipRep->u.ReadMemory.ActualBytesRead, (void*)(manipRep + 1));
                
                //memcpy((void*)(manipRep + 1), SvmmGetHostAddress(manipRep->u.ReadMemory.TargetBaseAddress), manipRep->u.ReadMemory.ActualBytesRead);
                
                memcpy((void*)(manipRep + 1), SvmmGetHostAddress(manipRep->u.ReadMemory.TargetBaseAddress), manipRep->u.ReadMemory.ActualBytesRead);
                rep->length += (USHORT)manipRep->u.ReadMemory.ActualBytesRead;
            }
            else {
                manipRep->u.ReadMemory.ActualBytesRead = 0;
            }
            KdSendPacketAck(rep);
            break;
        case DbgKdWriteVirtualMemoryApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
            manipRep->u.WriteMemory.ActualBytesWritten = min(manipReq->u.WriteMemory.TransferCount, KD_MAX_PACKET_SIZE - sizeof(DBGKD_MANIPULATE_STATE64) - sizeof(KD_PACKET));
            //memcpy((BYTE*)BX_CPU_THIS->getHostMemAddr(manipRep->u.WriteMemory.TargetBaseAddress, 0), (BYTE*)(manipReq + 1), manipRep->u.WriteMemory.ActualBytesWritten);
            //BX_CPU_THIS_PTR access_write_physical(manipRep->u.WriteMemory.TargetBaseAddress, manipRep->u.WriteMemory.ActualBytesWritten, (void*)(manipReq + 1));
            memcpy((void*)(manipRep + 1), SvmmGetHostAddress(manipRep->u.WriteMemory.TargetBaseAddress), manipRep->u.WriteMemory.ActualBytesWritten);

            KdSendPacketAck(rep);
            break;
        case DbgKdReadControlSpaceApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
            manipRep->u.ReadMemory.ActualBytesRead = min(manipReq->u.ReadMemory.TransferCount, KD_MAX_PACKET_SIZE - sizeof(DBGKD_MANIPULATE_STATE64) - sizeof(KD_PACKET));
            switch (manipReq->u.ReadMemory.TargetBaseAddress) {
            case DEBUG_CONTROL_SPACE_PCR:
                //tmp = KeGetCurrentPrcb() - PRCB_OFFSET;
                //memcpy((BYTE*)(manipRep + 1), (BYTE*)&tmp, sizeof(ULONG64));
                manipRep->u.ReadMemory.ActualBytesRead = 0;
                break;
            case DEBUG_CONTROL_SPACE_PRCB:
                //tmp = KeGetCurrentPrcb();
                //memcpy((BYTE*)(manipRep + 1), (BYTE*)&tmp, sizeof(ULONG64));
                manipRep->u.ReadMemory.ActualBytesRead = 0;
                break;
            case DEBUG_CONTROL_SPACE_KSPECIAL:
                //tmp = KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbProcStateSpecialReg;
                //memcpy((BYTE*)(manipRep + 1), (BYTE*)tmp, manipRep->u.ReadMemory.ActualBytesRead);
                
                memset(&special, '\0', sizeof(special));
                special.Cr0 = Registers.context._cr0;
                special.Cr2 = Registers.context._cr2;
                special.Cr3 = Registers.context._cr3;
                special.Cr4 = Registers.context._cr4;
                special.Cr8 = Registers.cr8;
                special.KernelDr0 = Registers.context._dr0;
                special.KernelDr1 = Registers.context._dr1;
                special.KernelDr2 = Registers.context._dr2;
                special.KernelDr3 = Registers.context._dr3;
                special.KernelDr6 = Registers.context._dr6;
                special.KernelDr7 = Registers.context._dr7;
                special.Gdtr.Base = Registers.context._gdt.base;
                special.Gdtr.Limit = Registers.context._gdt.limit;
                special.Idtr.Base = Registers.context._idt.base;
                special.Idtr.Limit = Registers.context._idt.limit;
                special.LastBranchFromRip = Registers.context._rip;
                special.LastBranchToRip = Registers.context._rip;
                special.LastExceptionToRip = Registers.context._rip;
                special.LastExceptionFromRip = Registers.context._rip;
                special.Xcr0 = Registers.xcr0;
                special.Tr = Registers.context._tr.selector;

                memcpy((BYTE*)(manipRep + 1), &special, sizeof(special));
                manipRep->u.ReadMemory.ActualBytesRead = sizeof(special);
                break;
            case DEBUG_CONTROL_SPACE_THREAD:
                //tmp = KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbCurrentThread;
                //memcpy((BYTE*)(manipRep + 1), (BYTE*)&tmp, sizeof(ULONG64));
                manipRep->u.ReadMemory.ActualBytesRead = 0;
                break;
            }
            rep->length += (USHORT)manipRep->u.ReadMemory.ActualBytesRead;
            KdSendPacketAck(rep);
            break;
        case DbgKdWriteControlSpaceApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
            manipRep->u.WriteMemory.ActualBytesWritten = min(manipReq->u.WriteMemory.TransferCount, KD_MAX_PACKET_SIZE - sizeof(DBGKD_MANIPULATE_STATE64) - sizeof(KD_PACKET));
            switch (manipRep->u.WriteMemory.TargetBaseAddress) {
            case DEBUG_CONTROL_SPACE_KSPECIAL:
                //tmp = KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbProcStateSpecialReg;
                
                memset(&special, '\0', sizeof(special));
                memcpy(&special, (BYTE*)(manipReq + 1), sizeof(special));
                Registers.context._cr0 = special.Cr0;
                Registers.context._cr2 = special.Cr2;
                Registers.context._cr3 = special.Cr3;
                Registers.context._cr4 = special.Cr4;
                //BX_CPU_THIS_PTR lapic.set_tpr(special.Cr8);
                Registers.context._dr0 = special.KernelDr0;
                Registers.context._dr1 = special.KernelDr1;
                Registers.context._dr2 = special.KernelDr2;
                Registers.context._dr3 = special.KernelDr3;
                Registers.context._dr6 = special.KernelDr6;
                Registers.context._dr7 = special.KernelDr7;

                if (Registers.context._dr7 & 15) {
#ifdef HAXM
                    memset(&debug, '\0', sizeof(debug));
                    debug.dr[0] = special.KernelDr0;
                    debug.dr[1] = special.KernelDr1;
                    debug.dr[2] = special.KernelDr2;
                    debug.dr[3] = special.KernelDr3;
                    debug.dr[7] = special.KernelDr7;
                    debug.control = HAX_DEBUG_ENABLE | HAX_DEBUG_STEP | HAX_DEBUG_USE_SW_BP | HAX_DEBUG_USE_HW_BP;
                    DeviceIoControl(hHaxCpu, HAX_IOCTL_VCPU_DEBUG, &debug, sizeof(debug), (LPVOID)NULL, 0, &bytes, NULL);

#elif GVM
                    memset(&kvm_debug, '\0', sizeof(kvm_debug));
                    kvm_debug.arch.debugreg[0] = special.KernelDr0;
                    kvm_debug.arch.debugreg[1] = special.KernelDr1;
                    kvm_debug.arch.debugreg[2] = special.KernelDr2;
                    kvm_debug.arch.debugreg[3] = special.KernelDr3;
                    kvm_debug.arch.debugreg[7] = special.KernelDr7;
                    kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_SINGLESTEP | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;
                    DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);

#endif
                }

                Registers.context._gdt.base = special.Gdtr.Base;
                Registers.context._gdt.limit = special.Gdtr.Limit;
                Registers.context._idt.base = special.Idtr.Base;
                Registers.context._idt.limit = special.Idtr.Limit;
                Registers.xcr0 = special.Xcr0;
                Registers.context._tr.selector = special.Tr;
                SvmmSetRegisters(&Registers);
                //RIP = special.LastBranchFromRip;
                memcpy((BYTE*)(manipRep + 1), &special, sizeof(special));
                break;
            }
            KdSendPacketAck(rep);
            break;
        case DbgKdSetSpecialCallApi:
        case DbgKdClearSpecialCallsApi:
        case DbgKdSetInternalBreakPointApi:
        case DbgKdClearAllInternalBreakpointsApi:
            break;
        case DbgKdGetContextApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
            ctx.Rax = Registers.context._rax;
            ctx.Rcx = Registers.context._rcx;
            ctx.Rdx = Registers.context._rdx;
            ctx.Rbx = Registers.context._rbx;
            ctx.Rsp = Registers.context._rsp;
            ctx.Rbp = Registers.context._rbp;
            ctx.Rsi = Registers.context._rsi;
            ctx.Rdi = Registers.context._rdi;
            ctx.r8 = Registers.context._r8;
            ctx.r9 = Registers.context._r9;
            ctx.r10 = Registers.context._r10;
            ctx.r11 = Registers.context._r11;
            ctx.r12 = Registers.context._r12;
            ctx.r13 = Registers.context._r13;
            ctx.r14 = Registers.context._r14;
            ctx.r15 = Registers.context._r15;
            ctx.Rip = (Registers.context._cr0 & 1) ? Registers.context._rip : (Registers.context._rip | ((ULONG64)Registers.context._cs.selector << 4));
            ctx.SegCs = Registers.context._cs.selector;
            ctx.SegDs = Registers.context._ds.selector;
            ctx.SegEs = Registers.context._es.selector;
            ctx.SegFs = Registers.context._fs.selector;
            ctx.SegSs = Registers.context._ss.selector;
            ctx.SegGs = Registers.context._gs.selector;
            ctx.EFlags = (DWORD)Registers.context._rflags;
            ctx.Dr0 = Registers.context._dr0;
            ctx.Dr1 = Registers.context._dr1;
            ctx.Dr2 = Registers.context._dr2;
            ctx.Dr3 = Registers.context._dr3;
            ctx.Dr6 = Registers.context._dr6;
            ctx.Dr7 = Registers.context._dr7;
            memcpy((BYTE*)(manipRep + 1), (BYTE*)&ctx, sizeof(__CONTEXT));
            rep->length += sizeof(__CONTEXT);
            KdSendPacketAck(rep);
            break;
        case DbgKdSetContextApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
            //printf("DbgKdSetContextApi\n");
            //tmp = KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbProcStateContext;

            memcpy(&ctx, (BYTE*)(manipReq + 1), sizeof(__CONTEXT));
            Registers.context._rax = ctx.Rax;
            Registers.context._rcx = ctx.Rcx;
            Registers.context._rdx = ctx.Rdx;
            Registers.context._rbx = ctx.Rbx;
            Registers.context._rsp = ctx.Rsp;
            Registers.context._rbp = ctx.Rbp;
            Registers.context._rsi = ctx.Rsi;
            Registers.context._rdi = ctx.Rdi;
            Registers.context._r8 = ctx.r8;
            Registers.context._r9 = ctx.r9;
            Registers.context._r10 = ctx.r10;
            Registers.context._r11 = ctx.r11;
            Registers.context._r12 = ctx.r12;
            Registers.context._r13 = ctx.r13;
            Registers.context._r14 = ctx.r14;
            Registers.context._r15 = ctx.r15;
            Registers.context._rip = (Registers.context._cr0 & 1) ? ctx.Rip : (ctx.Rip & 0xffff);
            //printf("rip = 0x%llx\n", Registers.context._rax);
            Registers.context._cs.selector = ctx.SegCs;
            Registers.context._ds.selector = ctx.SegDs;
            Registers.context._es.selector = ctx.SegEs;
            Registers.context._fs.selector = ctx.SegFs;
            Registers.context._ss.selector = ctx.SegSs;
            Registers.context._gs.selector = ctx.SegGs;
            Registers.context._rflags = ctx.EFlags;
            Registers.context._dr0 = ctx.Dr0;
            Registers.context._dr1 = ctx.Dr1;
            Registers.context._dr2 = ctx.Dr2;
            Registers.context._dr3 = ctx.Dr3;
            Registers.context._dr6 = ctx.Dr6;
            Registers.context._dr7 = ctx.Dr7;
            KdSendPacketAck(rep);
            SvmmSetRegisters(&Registers);
            break;
        case DbgKdContinueApi:
            exit(-1);
            return 0;
        case DbgKdContinueApi2:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;

            //ReadMemory(windbgPtr.ctx, (BYTE*)&ctx, sizeof(ctx));
            //ctx.Rip = 0x7ff756dd10eb;
            //req = (PDBGKD_MANIPULATE_STATE64)((kd_packet_t*)buffer + 1);
#ifdef __DEBUG__
            printf("TraceFlag = 0x%x\n", manipRep->u.Continue2.ControlSet.TraceFlag);
            printf("Dr7 = 0x%llx\n", manipRep->u.Continue2.ControlSet.Dr7);
#endif
            memset(&kvm_debug, '\0', sizeof(kvm_debug));
            if (manipRep->u.Continue2.ControlSet.TraceFlag) {
                Stepping = 1;
                Registers.context._rflags |= EFLAGS_TF_MASK;
                kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_SINGLESTEP | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;
            }
            else {
                Stepping = 0;
                Registers.context._rflags &= (~EFLAGS_TF_MASK);
                kvm_debug.control = GVM_GUESTDBG_ENABLE | GVM_GUESTDBG_USE_SW_BP | GVM_GUESTDBG_USE_HW_BP;
            }
            Registers.context._dr7 = manipRep->u.Continue2.ControlSet.Dr7;
            DeviceIoControl(hGvmCpu, GVM_SET_GUEST_DEBUG, &kvm_debug, sizeof(kvm_debug), (LPVOID)NULL, 0, &bytes, NULL);

            SvmmSetRegisters(&Registers);
            //WriteMemory(windbgPtr.ctx, (BYTE*)&ctx, sizeof(ctx));
            //ReadMemory(windbgPtr.state, (BYTE*)&state, sizeof(_AMD64_KSPECIAL_REGISTERS));
            //state.special_ctx.KernelDr6 = 0;
            //state.special_ctx.KernelDr7 = req->u.Continue2.ControlSet.Dr7;
            //Registers.context._dr6 = 0;
            
            //WriteMemory(windbgPtr.state, (BYTE*)&state, sizeof(_AMD64_KSPECIAL_REGISTERS));

            //to delete 
            //ReadMemory(windbgPtr.ctx, (BYTE*)&ctx, sizeof(_CONTEXT));
            //ReadMemory(windbgPtr.state, (BYTE*)&state, sizeof(_AMD64_KSPECIAL_REGISTERS));
            //WriteMemory(0x728ffdfa28, (BYTE*)&ctx.Rdx, sizeof(ctx.Rdx));
            //pkt = (kd_packet_t*)buffer;

            
            /*
            * manipRep->ReturnStatus = 0;
            * 
            rep->packetLeader = KD_PACKET_DATA;
            rep->packetType = KD_PACKET_TYPE_STATE_CHANGE64;
            rep->length = sizeof(DBGKD_ANY_WAIT_STATE_CHANGE);
            waitChange = (PDBGKD_ANY_WAIT_STATE_CHANGE)(rep + 1);
            waitChange->NewState = DbgKdExceptionStateChange;
            waitChange->ProcessorLevel = 0;
            waitChange->Processor = 0;
            waitChange->NumberProcessors = 1;
            waitChange->ProgramCounter = 0xfffff;
            waitChange->Thread = 0;
            waitChange->u.Exception.FirstChance = 0x1;
            waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_SINGLE_STEP;
            waitChange->u.Exception.ExceptionRecord.ExceptionAddress = 0xffff;
            waitChange->u.Exception.ExceptionRecord.ExceptionFlags = 0;
            waitChange->u.Exception.ExceptionRecord.ExceptionRecord = 0;
            waitChange->u.Exception.ExceptionRecord.NumberParameters = 0;
            */
            KdSendPacketAck(rep);

            
#ifdef __DEBUG__
            printf("continue2\n");
#endif

            return DbgKdContinueApi2;
            break;
        case DbgKdGetContextExApi:
            
            *manipRep = *manipReq;
            //manipRep->ApiNumber = DbgKdGetContextApi;
            manipRep->ReturnStatus = 0;
#ifdef __DEBUG__

            printf("DbgKdGetContextExApi\n");
#endif
            manipRep->u.GetContextEx.BytesCopied = min(manipReq->u.GetContextEx.ByteCount, KD_MAX_PACKET_SIZE - sizeof(DBGKD_MANIPULATE_STATE64) - sizeof(KD_PACKET));
            if (!manipReq->u.GetContextEx.Offset) {
                //tmp = KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbProcStateContext;
                memset(ctxBuffer, '\0', sizeof(ctxBuffer));
                memset(&ctx, '\0', sizeof(ctx));
                ctx.Rax = Registers.context._rax;
                ctx.Rcx = Registers.context._rcx;
                ctx.Rdx = Registers.context._rdx;
                ctx.Rbx = Registers.context._rbx;
                ctx.Rsp = Registers.context._rsp;
                ctx.Rbp = Registers.context._rbp;
                ctx.Rsi = Registers.context._rsi;
                ctx.Rdi = Registers.context._rdi;
                ctx.r8 = Registers.context._r8;
                ctx.r9 = Registers.context._r9;
                ctx.r10 = Registers.context._r10;
                ctx.r11 = Registers.context._r11;
                ctx.r12 = Registers.context._r12;
                ctx.r13 = Registers.context._r13;
                ctx.r14 = Registers.context._r14;
                ctx.r15 = Registers.context._r15;
                ctx.Rip = (Registers.context._cr0 & 1) ? Registers.context._rip : (Registers.context._rip | ((ULONG64)Registers.context._cs.selector << 4));
                ctx.SegCs = Registers.context._cs.selector;
                ctx.SegDs = Registers.context._ds.selector;
                ctx.SegEs = Registers.context._es.selector;
                ctx.SegFs = Registers.context._fs.selector;
                ctx.SegSs = Registers.context._ss.selector;
                ctx.SegGs = Registers.context._gs.selector;
                ctx.EFlags = (DWORD)Registers.context._rflags;
                ctx.Dr0 = Registers.context._dr0;
                ctx.Dr1 = Registers.context._dr1;
                ctx.Dr2 = Registers.context._dr2;
                ctx.Dr3 = Registers.context._dr3;
                ctx.Dr6 = Registers.context._dr6;
                ctx.Dr7 = Registers.context._dr7;
                /*
                if (BX_CPU_THIS_PTR cr0.get32() & 1)
                  ctx.Rip = RIP;
                else
                  ctx.Rip = RIP | (BX_CPU_THIS->sregs[BX_SEG_REG_CS].selector.value << 4);
                */
                memcpy((BYTE*)ctxBuffer, &ctx, sizeof(CONTEXT));
                ctxEx = (PCONTEXT_EX)((BYTE*)ctxBuffer + sizeof(CONTEXT));
                ctxEx->Legacy.Length = (ULONG)sizeof(PCONTEXT_EX);
                ctxEx->Legacy.Offset = -(LONG)ctxEx->Legacy.Length;
                ctxEx->All.Length = (ULONG)((BYTE*)(ctxEx + 1) - (BYTE*)ctxBuffer);
                ctxEx->All.Offset = ctxEx->Legacy.Offset;

                if ((((CONTEXT*)ctxBuffer)->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE) {

                    XStateHeader = (PXSAVE_AREA_HEADER)
                        ALIGN_UP_POINTER_BY((ULONG_PTR)(ctxEx + 1), XSAVE_ALIGN);

                    memset((BYTE*)XStateHeader, '\0', sizeof(XSAVE_AREA_HEADER));
                    ctxEx->XState.Offset = (LONG)((BYTE*)XStateHeader - (BYTE*)ctxEx);
                    ctxEx->XState.Length =
                        (*(ULONG*)(AMD64_USER_SHARED_DATA + AMD64_XSAVE_SIZE_OFFSET)) -
                        sizeof(XSAVE_FORMAT);
                    ctxEx->All.Length =
                        ctxEx->XState.Offset + ctxEx->XState.Length -
                        ctxEx->All.Offset;
                    //memcpy((BYTE*)XStateHeader, (BYTE*)*(ULONG64*)(KeGetCurrentPrcb() + prcbXsave), ctxEx->XState.Length);

                }
                ctxSize = manipRep->u.GetContextEx.ByteCount = ctxEx->All.Length;

                manipRep->u.GetContextEx.Offset = 0;
            }
            manipRep->u.GetContextEx.ByteCount = ctxSize;
            memcpy((BYTE*)(manipRep + 1), ctxBuffer + manipReq->u.GetContextEx.Offset, manipRep->u.GetContextEx.BytesCopied);
            rep->length += (USHORT)manipRep->u.GetContextEx.BytesCopied;

            KdSendPacketAck(rep);
            break;
        case DbgKdSetContextExApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
#ifdef __DEBUG__
            printf("DbgKdSetContextExApi\n");
#endif
            /*
            if (!manipReq->u.SetContextEx.Offset) {
                tmp = KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbProcStateContext;
                memset(ctxBuffer, '\0', sizeof(ctxBuffer));
                memcpy((BYTE*)ctxBuffer, tmp, sizeof(CONTEXT));
                ctxEx = (PCONTEXT_EX)((BYTE*)ctxBuffer + sizeof(CONTEXT));
                ctxEx->Legacy.Length = (ULONG)sizeof(PCONTEXT_EX);
                ctxEx->Legacy.Offset = -(LONG)ctxEx->Legacy.Length;
                ctxEx->All.Length = (ULONG)((BYTE*)(ctxEx + 1) - (BYTE*)ctxBuffer);
                ctxEx->All.Offset = ctxEx->Legacy.Offset;
                if ((((CONTEXT*)ctxBuffer)->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE) {
                    XStateHeader = (PXSAVE_AREA_HEADER)
                        ALIGN_UP_POINTER_BY((ULONG_PTR)(ctxEx + 1), XSAVE_ALIGN);

                    memset((BYTE*)XStateHeader, '\0', sizeof(XSAVE_AREA_HEADER));
                    ctxEx->XState.Offset = (LONG)((BYTE*)XStateHeader - (BYTE*)ctxEx);
                    ctxEx->XState.Length =
                        (*(ULONG64*)(AMD64_USER_SHARED_DATA + AMD64_XSAVE_SIZE_OFFSET)) -
                        sizeof(XSAVE_FORMAT);
                    ctxEx->All.Length =
                        ctxEx->XState.Offset + ctxEx->XState.Length -
                        ctxEx->All.Offset;
                    memcpy((BYTE*)XStateHeader, (BYTE*)*(ULONG64*)(KeGetCurrentPrcb() + prcbXsave), ctxEx->XState.Length);
                }
                ctxSize = manipRep->u.GetContextEx.ByteCount = ctxEx->All.Length;
                manipRep->u.GetContextEx.Offset = 0;
            }
            memcpy()
            */

            if (manipReq->u.SetContextEx.Offset > sizeof(CONTEXT))
                manipRep->u.SetContextEx.Offset = sizeof(CONTEXT);
            if (manipReq->u.SetContextEx.ByteCount > (sizeof(CONTEXT) - manipReq->u.SetContextEx.Offset))
                manipRep->u.SetContextEx.ByteCount = sizeof(CONTEXT) - manipReq->u.SetContextEx.Offset;
            manipRep->u.SetContextEx.BytesCopied = min(sizeof(CONTEXT), manipReq->u.SetContextEx.ByteCount);
            //memcpy(KeGetCurrentPrcb() + KdDebuggerDataBlock.OffsetPrcbProcStateContext + manipRep->u.SetContextEx.Offset, (BYTE*)(manipReq + 1), min(sizeof(CONTEXT), manipRep->u.SetContextEx.BytesCopied));
            //memcpy(ctxBuffer + manipRep->u.SetContextEx.Offset, (BYTE*)(manipReq + 1), min(sizeof(CONTEXT), manipRep->u.SetContextEx.BytesCopied));
            if (manipRep->u.SetContextEx.Offset == sizeof(CONTEXT)) {
                memcpy(&ctx, (BYTE*)(manipReq + 1), manipRep->u.SetContextEx.ByteCount);
                Registers.context._rax = ctx.Rax;
                Registers.context._rcx = ctx.Rcx;
                Registers.context._rdx = ctx.Rdx;
                Registers.context._rbx = ctx.Rbx;
                Registers.context._rsp = ctx.Rsp;
                Registers.context._rbp = ctx.Rbp;
                Registers.context._rsi = ctx.Rsi;
                Registers.context._rdi = ctx.Rdi;
                Registers.context._r8 = ctx.r8;
                Registers.context._r9 = ctx.r9;
                Registers.context._r10 = ctx.r10;
                Registers.context._r11 = ctx.r11;
                Registers.context._r12 = ctx.r12;
                Registers.context._r13 = ctx.r13;
                Registers.context._r14 = ctx.r14;
                Registers.context._r15 = ctx.r15;
                Registers.context._rip = (Registers.context._cr0 & 1) ? ctx.Rip : (ctx.Rip & 0xffff);
                //printf("rip = 0x%llx\n", Registers.context._rax);
                Registers.context._cs.selector = ctx.SegCs;
                Registers.context._ds.selector = ctx.SegDs;
                Registers.context._es.selector = ctx.SegEs;
                Registers.context._fs.selector = ctx.SegFs;
                Registers.context._ss.selector = ctx.SegSs;
                Registers.context._gs.selector = ctx.SegGs;
                Registers.context._rflags = ctx.EFlags;
                Registers.context._dr0 = ctx.Dr0;
                Registers.context._dr1 = ctx.Dr1;
                Registers.context._dr2 = ctx.Dr2;
                Registers.context._dr3 = ctx.Dr3;
                Registers.context._dr6 = ctx.Dr6;
                Registers.context._dr7 = ctx.Dr7;
            }
            memcpy(ctxBuffer + manipRep->u.SetContextEx.Offset, (BYTE*)(manipReq + 1), manipRep->u.SetContextEx.ByteCount);
            rep->length += (USHORT)manipRep->u.SetContextEx.BytesCopied;
            KdSendPacketAck(rep);
            SvmmSetRegisters(&Registers);
            break;
        case DbgKdRestoreBreakPointApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;

            i = manipRep->u.RestoreBreakPoint.BreakPointHandle - 1;
            //printf("breakpoint idx %d\n", i);
            if (manipRep->u.RestoreBreakPoint.BreakPointHandle >= BREAKPOINT_TABLE_SIZE ||
                !KdBreakpointTable[i].Flags || !KdBreakpointTable[i].Address) {
                manipRep->ReturnStatus = 0xC0000001;
                KdSendPacketAck(rep);
                break;
            }
            if (KdBreakpointTable[i].Flags & KD_BREAKPOINT_SUSPENDED) {
                if (!(KdBreakpointTable[i].Flags & KD_BREAKPOINT_NEEDS_REPLACE))
                    KdBreakpointTable[i].Flags = 0;
                KdSendPacketAck(rep);
                break;
            }
            if (KdBreakpointTable[i].Flags & KD_BREAKPOINT_NEEDS_WRITE) {
                KdBreakpointTable[i].Flags &= ~KD_BREAKPOINT_NEEDS_WRITE;
                KdSendPacketAck(rep);
                break;
            }
            //*(BYTE*)BX_CPU_THIS->getHostMemAddr((ULONG64)KdBreakpointTable[i].Address, 0) = (BYTE)KdBreakpointTable[i].Content;
            *(BYTE*)SvmmGetHostAddress((ULONG64)KdBreakpointTable[i].Address) = (BYTE)KdBreakpointTable[i].Content;
            //BX_CPU_THIS_PTR access_write_physical((bx_phy_address)KdBreakpointTable[i].Address, 1, (void*)&KdBreakpointTable[i].Content);
            KdBreakpointTable[i].Flags = 0;
            KdSendPacketAck(rep);
            break;
        case DbgKdWriteBreakPointApi:
            tmp = 0xcc;
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
#ifdef __DEBUG__

            printf("break address = 0x%llx\n", manipReq->u.WriteBreakPoint.BreakPointAddress);
#endif
            /* Init with invalid value */
            manipRep->u.WriteBreakPoint.BreakPointHandle = BREAKPOINT_TABLE_SIZE;
            for (i = 0; i < BREAKPOINT_TABLE_SIZE; i++) {
                /* check if bp already exists */
                if (manipReq->u.WriteBreakPoint.BreakPointAddress == (ULONG64)KdBreakpointTable[i].Address &&
                    KdBreakpointTable[i].Flags & KD_BREAKPOINT_IN_USE) {
                    if (KdBreakpointTable[i].Flags & KD_BREAKPOINT_NEEDS_WRITE) {
                        KdBreakpointTable[i].Flags &= ~KD_BREAKPOINT_NEEDS_WRITE;
                        break;
                    }
                    else {
                        manipRep->u.WriteBreakPoint.BreakPointHandle = BREAKPOINT_TABLE_SIZE;
                        break;
                    }
                }
            }

            if (i == BREAKPOINT_TABLE_SIZE) {
                for (i = 0; i < BREAKPOINT_TABLE_SIZE; i++) {
                    if (!KdBreakpointTable[i].Address && !KdBreakpointTable[i].Flags) {
                        manipRep->u.WriteBreakPoint.BreakPointHandle = i;
                        break;
                    }
                }
            }

            /* If Not Found */
            if (manipRep->u.WriteBreakPoint.BreakPointHandle == BREAKPOINT_TABLE_SIZE ||
                !manipReq->u.WriteBreakPoint.BreakPointAddress) {
                manipRep->u.WriteBreakPoint.BreakPointHandle = 0;
                manipRep->ReturnStatus = 0xC0000001;
                rep->length += sizeof(DBGKD_WRITE_BREAKPOINT64);
                KdSendPacketAck(rep);
                break;
            }


            KdBreakpointTable[i].Address = (PVOID)manipRep->u.WriteBreakPoint.BreakPointAddress;
            KdBreakpointTable[i].Process = 0; //*(ULONG64 **)((KeGetCurrentPrcb() + 8) + 0x220);
            KdBreakpointTable[i].BreakInstruction = 0xcc;
            //KdBreakpointTable[i].Content = 0;
            //KdBreakpointTable[i].Content = *(BYTE*)BX_CPU_THIS->getHostMemAddr(manipRep->u.WriteBreakPoint.BreakPointAddress, 0);
            //SvmmGetHostAddress
            KdBreakpointTable[i].Content = *(BYTE*)SvmmGetHostAddress(manipRep->u.WriteBreakPoint.BreakPointAddress);
            //BX_CPU_THIS_PTR access_read_physical(manipRep->u.WriteBreakPoint.BreakPointAddress, 1, (void*)&KdBreakpointTable[i].Content);
            KdBreakpointTable[i].Flags = KD_BREAKPOINT_IN_USE;
            KdBreakpointTable[i].BreakInstructionSize = sizeof(BYTE);
            KdBreakpointTable[i].BreakInstructionAlignment = 0;
            //BX_CPU_THIS_PTR access_write_physical(manipRep->u.WriteBreakPoint.BreakPointAddress, 1, (void*)&tmp);
            //*(BYTE*)BX_CPU_THIS->getHostMemAddr(manipRep->u.WriteBreakPoint.BreakPointAddress, 0) = 0xcc;
            *(BYTE*)SvmmGetHostAddress(manipRep->u.WriteBreakPoint.BreakPointAddress) = 0xcc;
            /*
            KdBreakpointTable[i].Address = (PVOID)manipRep->u.WriteBreakPoint.BreakPointAddress;
            KdBreakpointTable[i].Process = *(ULONG64**)((KeGetCurrentPrcb() + 8) + 0x220);
            KdBreakpointTable[i].BreakInstruction = 0xb1cd;
            KdBreakpointTable[i].Content = *(WORD*)KdBreakpointTable[i].Address;
            KdBreakpointTable[i].Flags = KD_BREAKPOINT_IN_USE;
            KdBreakpointTable[i].BreakInstructionSize = sizeof(WORD);
            KdBreakpointTable[i].BreakInstructionAlignment = 0;
            */
            manipRep->u.WriteBreakPoint.BreakPointHandle = i + 1;
            rep->length += sizeof(DBGKD_WRITE_BREAKPOINT64);
            KdSendPacketAck(rep);
            break;
            /*
            case DbgKdSearchMemoryApi:
                *manipRep = *manipReq;
                manipRep->ReturnStatus = 0;
                rep->length += sizeof(DBGKD_SEARCH_MEMORY);
                KdSendPacketAck(rep);
                break;

            case DbgKdFillMemoryApi:
                *manipRep = *manipReq;
                manipRep->ReturnStatus = 0;
                memset(
                break;
            */
        case DbgKdQueryMemoryApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;

            if (manipRep->u.QueryMemory.Address > 0x7fffffffffffffff)
                manipRep->u.QueryMemory.AddressSpace = 2;
            else
                manipRep->u.QueryMemory.AddressSpace = 0;
            manipRep->u.QueryMemory.Flags =
                DBGKD_QUERY_MEMORY_READ |
                DBGKD_QUERY_MEMORY_WRITE |
                DBGKD_QUERY_MEMORY_EXECUTE;
            manipRep->u.QueryMemory.AddressSpace = 0;
            manipRep->u.QueryMemory.Reserved = 0;
            rep->length += sizeof(DBGKD_QUERY_MEMORY);
            KdSendPacketAck(rep);
            break;
        case DbgKdWriteCustomBreakPointApi:
            *manipRep = *manipReq;
            manipRep->ReturnStatus = 0;
#ifdef __DEBUG__

            printf("hw break addr = 0x%llx\n", manipRep->u.WriteCustomBreakPoint.BreakPointAddress);
#endif
            KdSendPacketAck(rep);
            break;
        default:
            //NotSupported(req, manipReq);
            break;
        }
    }
    else if (req->packetLeader == KD_PACKET_BREAK) {
        manipReq = (PDBGKD_MANIPULATE_STATE64)(req + 1);
        /* Send ack */
        memset(rep, '\0', sizeof(KD_PACKET));
        rep->packetLeader = KD_PACKET_CTRL;
        rep->packetType = KD_PACKET_TYPE_ACKNOWLEDGE;
        rep->id = req->id;
        KdSendPacket(rep);
        /* Send State Change */
        memset(rep, '\0', sizeof(KD_PACKET) + sizeof(DBGKD_ANY_WAIT_STATE_CHANGE));
        rep->packetLeader = KD_PACKET_DATA;
        rep->packetType = KD_PACKET_TYPE_STATE_CHANGE64;
        rep->length = sizeof(DBGKD_ANY_WAIT_STATE_CHANGE);
        
        packetId =+ 2;
        rep->id = packetId;

        waitChange = (PDBGKD_ANY_WAIT_STATE_CHANGE)(rep + 1);
        waitChange->NewState = DbgKdExceptionStateChange;
        waitChange->ProcessorLevel = 0;
        waitChange->Processor = 0;
        waitChange->NumberProcessors = 1;
        waitChange->Thread = 0; //0x4141414141414141;
        waitChange->u.Exception.FirstChance = 0x1;
        waitChange->u.Exception.ExceptionRecord.ExceptionAddress = Registers.context._rip;
        waitChange->ProgramCounter = waitChange->u.Exception.ExceptionRecord.ExceptionAddress;
        waitChange->u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
        waitChange->u.Exception.ExceptionRecord.ExceptionFlags = 0;
        waitChange->u.Exception.ExceptionRecord.ExceptionRecord = 0;
        waitChange->u.Exception.ExceptionRecord.NumberParameters = 0;
        waitChange->ControlReport.Dr6 = Registers.context._dr6;
        waitChange->ControlReport.Dr7 = Registers.context._dr7;
        waitChange->ControlReport.EFlags = (ULONG)Registers.context._rflags;
        waitChange->ControlReport.ReportFlags = 3;
        waitChange->ControlReport.SegCs = Registers.context._cs.selector;
        waitChange->ControlReport.SegDs = Registers.context._ds.selector;
        waitChange->ControlReport.SegEs = Registers.context._es.selector;
        waitChange->ControlReport.SegFs = Registers.context._fs.selector;
        //memcpy((BYTE*)waitChange->ControlReport.InstructionStream, (BYTE*)BX_CPU_THIS->getHostMemAddr(BX_CPU_THIS->get_rip(), 0), 16);
        //BX_CPU_THIS_PTR access_read_physical(waitChange->u.Exception.ExceptionRecord.ExceptionAddress, 16, (void*)waitChange->ControlReport.InstructionStream);
        //memcpy((void*)waitChange->ControlReport.InstructionStream, SvmmGetHostAddress(waitChange->u.Exception.ExceptionRecord.ExceptionAddress), 16);
        memcpy((void*)waitChange->ControlReport.InstructionStream, SvmmGetHostAddress(Registers.context._rip), 16);
        KdSendPacketAck(rep);
        
        return KD_PACKET_BREAK;
    }
    return 1;
}
