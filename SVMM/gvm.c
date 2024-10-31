#include "gvm.h"
#include "haxm.h"
#include "timer.h"
#include <stdio.h>
#include "SVMM.h"

//GLOBALS
HANDLE hGvmDev, hGvmCpu, hGvmVm;
struct kvm_run* kvm_run;
volatile unsigned char* pio_data;
struct kvm_userspace_memory_region mem_slots[256];
struct kvm_cpuid *cpuids;
struct kvm_msr_list* msr_list;
struct kvm_msrs* msrs;

DWORD mem_slots_num;
DWORD freeslots;

extern BYTE* Ram;

//BX_MEM_C::getHostMemAddr
// 0x00000 - 0x7ffff    DOS area (512K)
// 0x80000 - 0x9ffff    Optional fixed memory hole (128K)
// 0xa0000 - 0xbffff    Standard PCI/ISA Video Mem / SMMRAM (128K) (MMIO)
// 0xc0000 - 0xdffff    Expansion Card BIOS and Buffer Area (128K) (VGA ROM)
// 0xe0000 - 0xeffff    Lower BIOS Area (64K)
// 0xf0000 - 0xfffff    Upper BIOS Area (64K)
int gvm_init(void* ram, size_t ram_size)
{
    struct kvm_userspace_memory_region kvm_userspace_mem;
    DWORD bytes, i;
    BOOL ret;

    hGvmCpu = hGvmVm = hGvmDev = NULL;
    hGvmDev = CreateFileA("\\\\.\\gvm", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hGvmDev == INVALID_HANDLE_VALUE) {
        perror("Driver GVM isn't loaded");
        return -1;
    }

    //create vm
    bytes = 0;
    DeviceIoControl(hGvmDev, GVM_CREATE_VM, NULL, 0, &hGvmVm, sizeof(hGvmVm), &bytes, NULL);
    if (bytes != sizeof(ULONG64) || !hGvmVm) {
        perror("Driver GVM DeviceIoControl GVM_CREATE_VM");
        goto error;
    }
    /*
    //GVM_CREATE_IRQCHIP
    ret = DeviceIoControl(hGvmVm, GVM_CREATE_IRQCHIP,
        NULL, 0, NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_CREATE_IRQCHIP");
        goto error;
    }
    */
   

    cpuids = malloc(sizeof(struct kvm_cpuid_entry) * GVM_MAX_CPUID_ENTRIES + sizeof(struct kvm_cpuid));
    if (!cpuids) {
        perror("Driver GVM malloc cpuids");
        goto error;
    }
    memset(cpuids, '\0', sizeof(struct kvm_cpuid_entry) * GVM_MAX_CPUID_ENTRIES + sizeof(struct kvm_cpuid));
    cpuids->nent = GVM_MAX_CPUID_ENTRIES;

    ret = DeviceIoControl(hGvmDev, GVM_GET_SUPPORTED_CPUID,
        cpuids, sizeof(struct kvm_cpuid_entry) * GVM_MAX_CPUID_ENTRIES + sizeof(struct kvm_cpuid),
        cpuids, sizeof(struct kvm_cpuid_entry) * GVM_MAX_CPUID_ENTRIES + sizeof(struct kvm_cpuid), &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_GET_SUPPORTED_CPUID");
        goto error;
    }
    
    for (i = 0; i < GVM_MAX_CPUID_ENTRIES; i++) {
        printf("cpuid function 0x%x eax = 0x%x ebx = 0x%x ecx = 0x%x edx = 0x%x\n", cpuids->entries[i].function, cpuids->entries[i].eax, cpuids->entries[i].ebx, cpuids->entries[i].ecx, cpuids->entries[i].edx);
        //to avoid windows bsod, we should set edx accordingly to disable rdtsc
        if (cpuids->entries[i].function == 0x80000001)
            cpuids->entries[i].edx &= ~(1UL << 27);
        //if (cpuids->entries[i].function == 1 && cpuids->entries[i].edx & 0x200)
          //  cpuids->entries[i].edx &= ~(1 << 9); //disable apic

    }
    
    
    msr_list = calloc(1, sizeof(struct kvm_msr_list));
    
    ret = DeviceIoControl(hGvmDev, GVM_GET_MSR_INDEX_LIST,
        msr_list, sizeof(struct kvm_msr_list),
        msr_list, sizeof(struct kvm_msr_list), &bytes,
        (LPOVERLAPPED)NULL);
    bytes = msr_list->nmsrs;
    printf("number of msrs %d\n", msr_list->nmsrs);
    free(msr_list);
    msr_list = malloc(sizeof(struct kvm_msr_list) + sizeof(__u32) * bytes);
    memset(msr_list, '\0', sizeof(struct kvm_msr_list) + sizeof(__u32) * bytes);
    msr_list->nmsrs = bytes;
    ret = DeviceIoControl(hGvmDev, GVM_GET_MSR_INDEX_LIST,
        msr_list, sizeof(struct kvm_msr_list) + sizeof(__u32) * bytes,
        msr_list, sizeof(struct kvm_msr_list) + sizeof(__u32) * bytes, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_GET_MSR_INDEX_LIST");
        goto error;
    }
    
    for (i = 0; i < msr_list->nmsrs; i++) {
        printf("msr[%d] = 0x%x\n", i, msr_list->indices[i]);
    }
    DeviceIoControl(hGvmVm, GVM_CREATE_VCPU, NULL, 0, &hGvmCpu, sizeof(hGvmCpu), &bytes, NULL);
    if (bytes != sizeof(ULONG64) || !hGvmCpu) {
        perror("Driver GVM DeviceIoControl GVM_CREATE_VCPU");
        goto error;
    }
    
    msrs = (struct kvm_msrs*)calloc(1, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msr_list->nmsrs);
    /*
    msrs->nmsrs = 3;
    
    msrs->entries[2].index = MSR_CORE_PERF_GLOBAL_CTRL;
    msrs->entries[2].data = (1ULL << 32);
    msrs->entries[1].index = MSR_CORE_PERF_FIXED_CTR_CTRL;
    msrs->entries[1].data = 3 | (1 << 3);// | (1 << 2); //
    msrs->entries[0].index = MSR_CORE_PERF_FIXED_CTR0;
    msrs->entries[0].data = (1ULL << 48) - TICK_PERIOD;
    

    ret = DeviceIoControl(hGvmCpu, GVM_SET_MSRS,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_MSRS");
        goto error;
    }
    */
    msrs->nmsrs = msr_list->nmsrs;
    for (i = 0; i < msr_list->nmsrs; i++) {
        msrs->entries[i].index = msr_list->indices[i];
    }

    ret = DeviceIoControl(hGvmCpu, GVM_GET_MSRS,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs,
        msrs, sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry) * msrs->nmsrs, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_GET_MSRS");
        goto error;
    }
    printf("GVM_GET_MSRS msrs %d\n", msrs->nmsrs);
    for (i = 0; i < msrs->nmsrs; i++) {
        printf("msrs[%d] 0x%x= 0x%llx\n", i, msrs->entries[i].index, msrs->entries[i].data);
    }
    
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0;
    kvm_userspace_mem.userspace_addr = (uint64_t)ram;
    kvm_userspace_mem.memory_size = 0x30000;
    kvm_userspace_mem.slot = 0;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;
    }
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;

    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0x30000;
    kvm_userspace_mem.userspace_addr = (uint64_t)ram + 0x30000;
    kvm_userspace_mem.memory_size = 0x20000;
    kvm_userspace_mem.slot = 1;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;
    }
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;

    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0x50000;
    kvm_userspace_mem.userspace_addr = (uint64_t)ram + 0x50000;
    kvm_userspace_mem.memory_size = 0x50000;
    kvm_userspace_mem.slot = 2;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;
    }
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;

    printf("memory starts 0x%llx\n", (ULONG64)ram);
    
    //0xa0000-0xbfff vga mmio, can be changed so that 0xa0000-0xbfff will point to SMM RAM
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0xa0000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xa0000);
    kvm_userspace_mem.memory_size = 0x20000;
    kvm_userspace_mem.slot = 3;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;
    
    }
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;

    //0xc0000-0xe0000 points to vga rom
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0xc0000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xc0000);
    kvm_userspace_mem.memory_size = 0x20000;
    kvm_userspace_mem.slot = 4;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;

    }
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;

    //0xe0000-0x100000 points to bios rom
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0xe0000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xe0000);
    kvm_userspace_mem.memory_size = 0x20000;
    kvm_userspace_mem.slot = 5;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;
    }
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;

    //all the ram 0x100000-40000000 0xfffe0000  0xfec00000 0xfed00000 0xfee00000 
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0x100000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + 0x100000);
    //kvm_userspace_mem.memory_size = 1024 * 1024 * 256 - 0x100000;
    kvm_userspace_mem.memory_size = ram_size - 0x100000;
    kvm_userspace_mem.slot = 6;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;
    }
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;
    
    //0xfffe0000-0x10000000 points to bios rom
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0xfffe0000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xe0000);
    kvm_userspace_mem.memory_size = 0x20000;
    kvm_userspace_mem.slot = 7;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        goto error;
    }
    
    mem_slots[kvm_userspace_mem.slot] = kvm_userspace_mem;
    
    mem_slots_num = kvm_userspace_mem.slot + 1;
    freeslots = mem_slots_num;
    //SMM to address space 1, has 0xa0000 always pointing to SMM RAM
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0xa0000;
    kvm_userspace_mem.userspace_addr = (uint64_t)ram + ram_size + ROM_SIZE + PAGE_SIZE;
    kvm_userspace_mem.memory_size = 0x20000;
    kvm_userspace_mem.slot = 0 | (1 << 16);
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl 0xa0000 GVM_SET_USER_MEMORY_REGION");
        exit(-1);
    }
    //SMM to address space 1, has 0x30000 pointing to ram
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0x30000;
    kvm_userspace_mem.userspace_addr = (uint64_t)ram + ram_size + ROM_SIZE + PAGE_SIZE;
    kvm_userspace_mem.memory_size = 0x20000;
    kvm_userspace_mem.slot = 1 | (1 << 16);
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl 0x30000 GVM_SET_USER_MEMORY_REGION");
        exit(-1);
    }


    //GVM_VCPU_MMAP
    ret = DeviceIoControl(hGvmCpu, GVM_VCPU_MMAP,
        NULL, 0, &kvm_run, sizeof(kvm_run), &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_VCPU_MMAP");
        goto error;
    }

    pio_data = (void*)((size_t)kvm_run + PAGE_SIZE);

    
    //GVM_CREATE_PIT_TIMER    
    /*
    ret = DeviceIoControl(hGvmCpu, GVM_CREATE_PIT_TIMER,
        NULL, 0, NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_CREATE_PIT_TIMER");
        goto error;
    }
    */
    ret = DeviceIoControl(hGvmCpu, GVM_SET_CPUID,
        cpuids, sizeof(struct kvm_cpuid_entry) * 80 + sizeof(struct kvm_cpuid),
        cpuids, sizeof(struct kvm_cpuid_entry) * 80 + sizeof(struct kvm_cpuid),
        &bytes, (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_CPUID");
        goto error;
    }
    /*
    struct kvm_vcpu_events event;
    memset(&event, '\0', sizeof(event));
    event.nmi.pending = 1;
    event.flags = GVM_VCPUEVENT_VALID_NMI_PENDING;
    ret = DeviceIoControl(hGvmCpu, GVM_SET_VCPU_EVENTS,
        &event, sizeof(struct kvm_vcpu_events),
        &event, sizeof(struct kvm_vcpu_events),
        &bytes, (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_VCPU_EVENTS");
        goto error;
    }
    */
    
    return 0;
error:
    if (hGvmDev)
        CloseHandle(hGvmDev);
    if (hGvmVm)
        CloseHandle(hGvmVm);
    if (hGvmCpu)
        CloseHandle(hGvmCpu);
    exit(-1);
}

void gvm_unmap_memory(unsigned int address, size_t size)
{
    LONG left, right;
    ULONG i, bytes, memSize;
    BOOL ret;

    left = 0;
    right = mem_slots_num;
    i = 0;
    while (left <= right) {
        i = (left + right) / 2;
        if (mem_slots[i].guest_phys_addr + mem_slots[i].memory_size <= address)
            left = i + 1;
        else if (mem_slots[i].guest_phys_addr <= address && address + size <= mem_slots[i].guest_phys_addr + mem_slots[i].memory_size)
            break;
        else
            right = i - 1;
    }
    if (mem_slots[i].guest_phys_addr > address || mem_slots[i].guest_phys_addr + mem_slots[i].memory_size < address)
        return;
    memSize = mem_slots[i].memory_size;
    mem_slots[i].memory_size = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
        &bytes, (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        exit(-1);
    }
    mem_slots[i].memory_size = memSize;


}

void gvm_map_memory(unsigned int address, unsigned int size, uint64_t ram)
{
    LONG left, right;
    ULONG i, bytes, memSize;
    BOOL ret;

    left = 0;
    right = mem_slots_num;
    i = 0;
    while (left <= right) {
        i = (left + right) / 2;
        if (mem_slots[i].guest_phys_addr + mem_slots[i].memory_size <= address)
            left = i + 1;
        else if (mem_slots[i].guest_phys_addr <= address && address + size <= mem_slots[i].guest_phys_addr + mem_slots[i].memory_size)
            break;
        else
            right = i - 1;
    }
    /*
    //unmap
    memSize = mem_slots[i].memory_size;
    mem_slots[i].memory_size = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
        &bytes, (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        exit(-1);
    }
    */
    //map
    //mem_slots[i].memory_size = memSize;
    mem_slots[i].userspace_addr = ram;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
        &bytes, (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        exit(-1);
    }

}

//calling GVM_SET_USER_MEMORY_REGION with mem_slots[i].memory_size == 0, makes mem_slots[i].guest_phys_addr an MMIO
void gvm_register_mmio(unsigned int address, size_t size)
{
    gvm_unmap_memory(address, size);
}

void gvm_remove_mmio(unsigned int address, size_t size)
{
    gvm_map_memory(address, size, (ULONG64)Ram + address);
}

/*
int gvm_unmap_memory(unsigned int address, size_t size)
{
    struct kvm_userspace_memory_region newBlock;
    unsigned long memSize;
    unsigned int i, j, found;
    BOOL ret;
    DWORD bytes;

    int left, right;

    left = 0;
    right = mem_slots_num;
    i = 0;
    while (left <= right) {
        i = (left + right) / 2;
        if (mem_slots[i].guest_phys_addr + mem_slots[i].memory_size <= address)
            left = i + 1;
        else if (mem_slots[i].guest_phys_addr <= address && address + size <= mem_slots[i].guest_phys_addr + mem_slots[i].memory_size) {
            break;
        }
        else
            right = i - 1;
    }
    // if its the start of slot  
    if (mem_slots[i].guest_phys_addr == address) {
        memSize = mem_slots[i].memory_size;
        mem_slots[i].memory_size = 0;
        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }
        if (memSize - size) {
            mem_slots[i].guest_phys_addr = address + size;
            mem_slots[i].userspace_addr += size;
            mem_slots[i].memory_size = memSize - size;
            ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
                &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
                &bytes, (LPOVERLAPPED)NULL);
            if (!ret) {
                perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
                exit(-1);
            }
        }
        else {
            // remove the current block, keep the others 
            for (j = i; j < mem_slots_num - 1; j++)
                mem_slots[j] = mem_slots[j + 1];
            mem_slots_num--;
        }
    }
    else if (mem_slots[i].guest_phys_addr <= address && address + size <= mem_slots[i].guest_phys_addr + mem_slots[i].memory_size) {
        // if its in the middle, we need to split it, in two blocks 
        memSize = mem_slots[i].memory_size;
        mem_slots[i].memory_size = 0;
        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }

        // first block
        newBlock.memory_size = memSize - (address - mem_slots[i].guest_phys_addr + size);
        mem_slots[i].memory_size = address - mem_slots[i].guest_phys_addr;

        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }

        // new block 
        newBlock.guest_phys_addr = address + size;
        newBlock.userspace_addr = mem_slots[i].userspace_addr + mem_slots[i].memory_size;
        newBlock.slot = mem_slots_num + 1;
        newBlock.flags = 0;
        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &newBlock, sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }
        // move the  new block next to the one we split, move all the others one step 
        for (j = mem_slots_num + 1; j > i; j--)
            mem_slots[j] = mem_slots[j - 1];
        mem_slots[i + 1] = newBlock;
        mem_slots_num++;

    }
    else {
        // else the address doesn't overlap, so do nothing 
        return i;
    }
    found = i;
    
    //remove all to clear tlbs?
    for (i = 0; i < mem_slots_num; i++) {
        memSize = mem_slots[i].memory_size;
        mem_slots[i].memory_size = 0;
        //remove
        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }
        //add
        mem_slots[i].memory_size = memSize;
    }
    //add them all with proper slots
    for (i = 0; i < mem_slots_num; i++) {

        mem_slots[i].slot = i;
        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }
    }
    
    
    return found;

}

void gvm_map_memory(unsigned int address, unsigned int size, uint64_t ram)
{
    int i, j;
    BOOL ret;
    ULONG bytes;
    unsigned long memSize;
    struct kvm_userspace_memory_region kvm_userspace_mem;
    


    i = gvm_unmap_memory(address, size);

    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = address;
    kvm_userspace_mem.userspace_addr = (uint64_t)ram;
    kvm_userspace_mem.memory_size = size;
    kvm_userspace_mem.slot = mem_slots_num + 1;
    //kvm_userspace_mem.slot = freeslots + 1;
    kvm_userspace_mem.flags = 0;
    ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
        &kvm_userspace_mem, sizeof(kvm_userspace_mem), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
        exit(-1);
    }
    for (j = mem_slots_num + 1; j > i; j--)
        mem_slots[j] = mem_slots[j - 1];
    mem_slots[i] = kvm_userspace_mem;
    mem_slots_num++;
    
    //remove all to clear tlbs?
    for (i = 0; i < mem_slots_num; i++) {
        memSize = mem_slots[i].memory_size;
        mem_slots[i].memory_size = 0;
        //remove
        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }
        //add
        mem_slots[i].memory_size = memSize;
    }
    //add them all with proper slots
    for (i = 0; i < mem_slots_num; i++) {

        mem_slots[i].slot = i;
        ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
            &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
            &bytes, (LPOVERLAPPED)NULL);
        if (!ret) {
            perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
            exit(-1);
        }
    }
}

//calling GVM_SET_USER_MEMORY_REGION with mem_slots[i].memory_size == 0, makes mem_slots[i].guest_phys_addr an MMIO
void gvm_register_mmio(unsigned int address, size_t size)
{
    gvm_unmap_memory(address,  size);
}

void gvm_remove_mmio(unsigned int address, size_t size)
{
    unsigned int i;
    unsigned long memSize;
    BOOL ret;
    DWORD bytes;

    for (i = 0; i < mem_slots_num; i++) {
        // 1st case mmio address within one block 
        if (mem_slots[i].guest_phys_addr >= address && address + size <= mem_slots[i].guest_phys_addr + mem_slots[i].memory_size) {
            // if its the start of slot  
            memSize = mem_slots[i].memory_size;
            mem_slots[i].memory_size = 0;
            ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
                &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
                &bytes, (LPOVERLAPPED)NULL);
            if (!ret) {
                perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
                exit(-1);
            }

            mem_slots[i].guest_phys_addr = address;
            mem_slots[i].userspace_addr -= size;
            mem_slots[i].memory_size += memSize + size;

            ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
                &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
                &bytes, (LPOVERLAPPED)NULL);
            if (!ret) {
                perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
                exit(-1);
            }
            break;
        }
    }
}
*/

static inline gvm_set_segment(struct kvm_segment *kvm_seg, struct segment_desc_t hax_seg)
{
    kvm_seg->base = hax_seg.base; 
    kvm_seg->limit = hax_seg.limit; 
    kvm_seg->selector = hax_seg.selector; 
    kvm_seg->type = hax_seg.type; 
    kvm_seg->s = hax_seg.desc; 
    kvm_seg->dpl = hax_seg.dpl; 
    kvm_seg->present = hax_seg.present; 
    kvm_seg->avl = hax_seg.available; 
    kvm_seg->l = hax_seg.long_mode; 
    kvm_seg->db = hax_seg.operand_size; 
    kvm_seg->g = hax_seg.granularity; 

}

static inline gvm_get_segment(struct kvm_segment kvm_seg, struct segment_desc_t *hax_seg)
{
    hax_seg->base = kvm_seg.base; 
    hax_seg->limit = kvm_seg.limit; 
    hax_seg->selector = kvm_seg.selector; 
    hax_seg->type = kvm_seg.type; 
    hax_seg->desc = kvm_seg.s; 
    hax_seg->dpl = kvm_seg.dpl; 
    hax_seg->present = kvm_seg.present; 
    hax_seg->available = kvm_seg.avl; 
    hax_seg->long_mode = kvm_seg.l; 
    hax_seg->operand_size = kvm_seg.db; 
    hax_seg->granularity = kvm_seg.g; 
}


NTSTATUS gvm_set_registers(struct Registers* Registers)
{
    struct kvm_regs kvm_regs;
    struct kvm_sregs kvm_sregs;
    DWORD bytes;
    BOOL ret;

    memset(&kvm_regs, '\0', sizeof(kvm_regs));
    kvm_regs.rax = Registers->context._rax;
    kvm_regs.rcx = Registers->context._rcx;
    kvm_regs.rbx = Registers->context._rbx;
    kvm_regs.rdx = Registers->context._rdx;
    kvm_regs.rsi = Registers->context._rsi;
    kvm_regs.rdi = Registers->context._rdi;
    kvm_regs.rsp = Registers->context._rsp;
    kvm_regs.rbp = Registers->context._rbp;
    kvm_regs.r8 = Registers->context._r8;
    kvm_regs.r9 = Registers->context._r9;
    kvm_regs.r10 = Registers->context._r10;
    kvm_regs.r11 = Registers->context._r11;
    kvm_regs.r12 = Registers->context._r12;
    kvm_regs.r13 = Registers->context._r13;
    kvm_regs.r14 = Registers->context._r14;
    kvm_regs.r15 = Registers->context._r15;
    kvm_regs.rip = Registers->context._rip;
    kvm_regs.rflags = Registers->context._rflags;

    ret = DeviceIoControl(hGvmCpu, GVM_SET_REGS,
        &kvm_regs, sizeof(kvm_regs), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_REGS");
        return ret;
    }
    
    memset(&kvm_sregs, '\0', sizeof(kvm_sregs));
    kvm_sregs.apic_base = Registers->apicbase;
    kvm_sregs.cr0 = Registers->context._cr0;
    kvm_sregs.cr2 = Registers->context._cr2;
    kvm_sregs.cr3 = Registers->context._cr3;
    kvm_sregs.cr4 = Registers->context._cr4;
    kvm_sregs.cr8 = Registers->cr8;
    kvm_sregs.apic_base = Registers->apicbase;
    kvm_sregs.efer = Registers->context._efer;

    gvm_set_segment(&kvm_sregs.cs, Registers->context._cs);
    gvm_set_segment(&kvm_sregs.ds, Registers->context._ds);
    gvm_set_segment(&kvm_sregs.es, Registers->context._es);
    gvm_set_segment(&kvm_sregs.fs, Registers->context._fs);
    gvm_set_segment(&kvm_sregs.gs, Registers->context._gs);
    gvm_set_segment(&kvm_sregs.ss, Registers->context._ss);
    gvm_set_segment(&kvm_sregs.tr, Registers->context._tr);
    gvm_set_segment(&kvm_sregs.ldt, Registers->context._ldt);

    kvm_sregs.gdt.base = Registers->context._gdt.base;
    kvm_sregs.gdt.limit = Registers->context._gdt.limit;
    kvm_sregs.idt.base = Registers->context._idt.base;
    kvm_sregs.idt.limit = Registers->context._idt.limit;

    ret = DeviceIoControl(hGvmCpu, GVM_SET_SREGS,
        &kvm_sregs, sizeof(kvm_sregs), NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_SREGS");
        return ret;
    }
    return 0;

}

NTSTATUS gvm_get_registers(struct Registers* Registers)
{
    struct kvm_regs kvm_regs;
    struct kvm_sregs kvm_sregs;
    DWORD bytes;
    BOOL ret;

    memset(&kvm_regs, '\0', sizeof(kvm_regs));
    ret = DeviceIoControl(hGvmCpu, GVM_GET_REGS,
        NULL, 0, &kvm_regs, sizeof(kvm_regs), &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_GET_REGS");
        return ret;
    }

    Registers->context._rax = kvm_regs.rax;
    Registers->context._rcx = kvm_regs.rcx;
    Registers->context._rbx = kvm_regs.rbx;
    Registers->context._rdx = kvm_regs.rdx;
    Registers->context._rsi = kvm_regs.rsi;
    Registers->context._rdi = kvm_regs.rdi;
    Registers->context._rsp = kvm_regs.rsp;
    Registers->context._rbp = kvm_regs.rbp;
    Registers->context._r8 = kvm_regs.r8;
    Registers->context._r9 = kvm_regs.r9;
    Registers->context._r10 = kvm_regs.r10;
    Registers->context._r11 = kvm_regs.r11;
    Registers->context._r12 = kvm_regs.r12;
    Registers->context._r13 = kvm_regs.r13;
    Registers->context._r14 = kvm_regs.r14;
    Registers->context._r15 = kvm_regs.r15;
    Registers->context._rip = kvm_regs.rip;
    Registers->context._rflags = kvm_regs.rflags;

    

    memset(&kvm_sregs, '\0', sizeof(kvm_sregs));
    ret = DeviceIoControl(hGvmCpu, GVM_GET_SREGS,
        NULL, 0, &kvm_sregs, sizeof(kvm_sregs), &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_GET_SREGS");
        return ret;
    }


    Registers->apicbase = kvm_sregs.apic_base;
    Registers->context._cr0 = kvm_sregs.cr0;
    Registers->context._cr2 = kvm_sregs.cr2;
    Registers->context._cr3 = kvm_sregs.cr3;
    Registers->context._cr4 = kvm_sregs.cr4;
    Registers->cr8 = kvm_sregs.cr8;
    Registers->context._efer = kvm_sregs.efer;

    gvm_get_segment(kvm_sregs.cs, &Registers->context._cs);
    gvm_get_segment(kvm_sregs.ds, &Registers->context._ds);
    gvm_get_segment(kvm_sregs.es, &Registers->context._es);
    gvm_get_segment(kvm_sregs.fs, &Registers->context._fs);
    gvm_get_segment(kvm_sregs.gs, &Registers->context._gs);
    gvm_get_segment(kvm_sregs.ss, &Registers->context._ss);
    gvm_get_segment(kvm_sregs.tr, &Registers->context._tr);
    gvm_get_segment(kvm_sregs.ldt, &Registers->context._ldt);

    Registers->context._gdt.base = kvm_sregs.gdt.base;
    Registers->context._gdt.limit = kvm_sregs.gdt.limit;
    Registers->context._idt.base = kvm_sregs.idt.base;
    Registers->context._idt.limit = kvm_sregs.idt.limit;

    return ret;
}
