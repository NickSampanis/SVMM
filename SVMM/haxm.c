#include "haxm.h"
#include <stdio.h>

HANDLE hHaxm, hHaxVm, hHaxCpu;
volatile struct hax_tunnel* tunnel;
volatile unsigned char* iobuf;

//BX_MEM_C::getHostMemAddr
// 0x00000 - 0x7ffff    DOS area (512K)
// 0x80000 - 0x9ffff    Optional fixed memory hole (128K)
// 0xa0000 - 0xbffff    Standard PCI/ISA Video Mem / SMMRAM (128K)
// 0xc0000 - 0xdffff    Expansion Card BIOS and Buffer Area (128K)
// 0xe0000 - 0xeffff    Lower BIOS Area (64K)
// 0xf0000 - 0xfffff    Upper BIOS Area (64K)
int haxm_init(void *ram, size_t ram_size)
{
    int vmid, vcpuid;
    char vmname[MAX_PATH], vmcpu[MAX_PATH];
    DWORD bytes;
    struct hax_set_ram_info2 info;
    struct hax_tunnel_info tinfo;
    BOOL ret;

    vmid = 0;
    tunnel = NULL;
    hHaxCpu = hHaxVm = hHaxm = NULL;
    hHaxm = CreateFileA("\\\\.\\HAX", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hHaxm == INVALID_HANDLE_VALUE) {
        perror("Driver HAX isn't loaded");
        return -1;
    }
    //create vm
    bytes = 0;
    DeviceIoControl(hHaxm, HAX_IOCTL_CREATE_VM, NULL, 0, &vmid, sizeof(vmid), &bytes, NULL);
    if (bytes != sizeof(int))
        goto error;
    memset(vmname, '\0', sizeof(vmname));
    _snprintf_s(vmname, MAX_PATH - 1, MAX_PATH - 1, "\\\\.\\hax_vm%02d", vmid);
    hHaxVm = CreateFileA(vmname, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hHaxVm == INVALID_HANDLE_VALUE) {
        perror("Driver HAX isn't loaded");
        return -1;
    }
    //map rom
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xFFE00000;
    info.va = ((uint64_t)ram + ram_size);
    info.size = (1 << 21);
    info.flags = HAX_MEMSLOT_STANDALONE;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;

    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xe0000;
    info.va = ((uint64_t)ram + ram_size + 0x1e0000);
    info.size = (0x20000);
    info.flags = 0;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    /*
    //map the other roms (vga, net card, etc)
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xe0000;
    info.va = ((uint64_t)ram + ram_size + 0x1e0000);
    info.size = (0x20000);
    info.flags = 0;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    */
    //map ram
    /*
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0;
    info.va = ((uint64_t)ram + ram_size + (1 << 21));
    info.size = (0xc0000);
    info.flags = HAX_MEMSLOT_STANDALONE;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    */
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0;
    info.va = ((uint64_t)ram);
    info.size = (0xc0000);
    info.flags = HAX_MEMSLOT_STANDALONE;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    /*
    //map
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xE0000;
    info.va = ((uint64_t)ram + ram_size); 
    info.size = (1 << 21);
    info.flags = HAX_MEMSLOT_STANDALONE;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    */
    /*
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xF0000;
    info.va = ((uint64_t)ram + 0xF0000);
    info.size = 64 * 1024;
    info.flags = HAX_MEMSLOT_STANDALONE;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    */
    
    /*
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xe0000;
    info.va = ((uint64_t)ram + ram_size + 0xe0000);
    info.size = (0x20000);
    info.flags = 0;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;

    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xC0000;
    info.va = ((uint64_t)ram + 0xC0000);
    info.size = 0x20000;
    info.flags = HAX_MEMSLOT_STANDALONE;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    */
    /*
    memset(&info, '\0', sizeof(info));
    info.pa_start = 0xFFFF0000;
    info.va = ((uint64_t)ram + ram_size + 0xF0000);
    info.size = 64 * 1024;
    info.flags = HAX_MEMSLOT_STANDALONE;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_SET_RAM2,
        &info, sizeof(info), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret)
        goto error;
    */
    //create cpu
    vcpuid = 0;
    ret = DeviceIoControl(hHaxVm, HAX_VM_IOCTL_VCPU_CREATE, &vcpuid, sizeof(vcpuid), NULL, 0, &bytes, NULL);
    if (!ret)
        goto error;
    memset(vmcpu, '\0', sizeof(vmcpu));
    snprintf(vmcpu, MAX_PATH - 1, "\\\\.\\hax_vm%02d_vcpu%02d", vmid, 0);
    hHaxCpu = CreateFileA(vmcpu, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hHaxCpu == INVALID_HANDLE_VALUE) {
        perror("Driver HAX isn't loaded");
        goto error;
    }
    //create tunnel info
    memset(&tinfo, '\0', sizeof(tinfo));
    ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_SETUP_TUNNEL, NULL, 0, &tinfo, sizeof(tinfo), &bytes, NULL);
    if (!ret)
        goto error;
    tunnel = (struct hax_tunnel*)tinfo.va;
    iobuf = (unsigned char*)tinfo.io_va;
    
    return 0;
error:

    if (hHaxm)
        CloseHandle(hHaxm);
    if (hHaxVm)
        CloseHandle(hHaxVm);
    if (hHaxCpu)
        CloseHandle(hHaxCpu);
    return -1;
}



NTSTATUS haxm_get_registers(struct Registers* Registers)
{
    DWORD Bytes;
    BOOL Ret;

    Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_GET_REGS, NULL, 0, &Registers->context, sizeof(struct vcpu_state_t), &Bytes, NULL);
    if (!Ret) {
        fprintf(stderr, "HAX_VCPU_GET_REGS 0x%x", GetLastError());
        return GetLastError();
    }
    Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_GET_FPU, NULL, 0, &Registers->fx, sizeof(struct fx_layout), &Bytes, NULL);
    if (!Ret) {
        fprintf(stderr, "HAX_VCPU_IOCTL_GET_FPU 0x%x", GetLastError());
        return GetLastError();
    }

    memset(&Registers->msrs, '\0', sizeof(struct hax_msr_data));
    Registers->msrs.nr_msr = 9;
    Registers->msrs.entries[0].entry = IA32_TSC;
    Registers->msrs.entries[1].entry = IA32_KERNEL_GS_BASE;
    Registers->msrs.entries[2].entry = IA32_APIC_BASE;
    Registers->msrs.entries[3].entry = IA32_CR_PAT;
    Registers->msrs.entries[4].entry = IA32_STAR;
    Registers->msrs.entries[5].entry = IA32_LSTAR;
    Registers->msrs.entries[6].entry = IA32_CSTAR;
    Registers->msrs.entries[7].entry = IA32_SF_MASK;
    Registers->msrs.entries[8].entry = IA32_TSC_AUX;
    Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_GET_MSRS, NULL, 0, &Registers->msrs, sizeof(struct hax_msr_data), &Bytes, NULL);
    if (!Ret) {
        fprintf(stderr, "HAX_VCPU_IOCTL_GET_MSRS 0x%x", GetLastError());
        return GetLastError();
    }

    return 0;
}

NTSTATUS haxm_set_registers(struct Registers* Registers)
{
    DWORD Bytes;
    BOOL Ret;

    Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_SET_REGS, &Registers->context, sizeof(struct vcpu_state_t), NULL, 0, &Bytes, NULL);
    if (!Ret) {
        fprintf(stderr, "HAX_VCPU_SET_REGS 0x%x", GetLastError());
        return GetLastError();
    }
    Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_SET_FPU, &Registers->fx, sizeof(struct fx_layout), NULL, 0, &Bytes, NULL);
    if (!Ret) {
        fprintf(stderr, "HAX_VCPU_IOCTL_SET_FPU 0x%x", GetLastError());
        return GetLastError();
    }
    memset(&Registers->msrs, '\0', sizeof(struct hax_msr_data));
    Registers->msrs.nr_msr = 9;
    Registers->msrs.entries[0].entry = IA32_TSC;
    Registers->msrs.entries[0].value = Registers->tsc;
    Registers->msrs.entries[1].entry = IA32_KERNEL_GS_BASE;
    Registers->msrs.entries[1].value = Registers->kernelgsbase;
    Registers->msrs.entries[2].entry = IA32_APIC_BASE;
    Registers->msrs.entries[2].value = Registers->apicbase;
    Registers->msrs.entries[3].entry = IA32_CR_PAT;
    Registers->msrs.entries[3].value = Registers->pat;
    Registers->msrs.entries[4].entry = IA32_STAR;
    Registers->msrs.entries[4].value = Registers->star;
    Registers->msrs.entries[5].entry = IA32_LSTAR;
    Registers->msrs.entries[5].value = Registers->lstar;
    Registers->msrs.entries[6].entry = IA32_CSTAR;
    Registers->msrs.entries[6].value = Registers->cstar;
    Registers->msrs.entries[7].entry = IA32_SF_MASK;
    Registers->msrs.entries[7].value = Registers->fmask;
    Registers->msrs.entries[8].entry = IA32_TSC_AUX;
    Registers->msrs.entries[8].value = Registers->tsc_aux;
    Ret = DeviceIoControl(hHaxCpu, HAX_VCPU_IOCTL_SET_MSRS, &Registers->msrs, sizeof(struct hax_msr_data), &Registers->msrs, sizeof(struct hax_msr_data), &Bytes, NULL);
    if (!Ret) {
        fprintf(stderr, "HAX_VCPU_IOCTL_SET_MSRS 0x%x", GetLastError());
        return GetLastError();
    }
    return 0;
}
