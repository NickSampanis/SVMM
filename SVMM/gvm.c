#include "gvm.h"
#include "haxm.h"
#include <stdio.h>

#define PAGE_SIZE 0x1000

//GLOBALS
HANDLE hGvm, hGvmCpu, hGvmVm;
struct kvm_run* kvm_run;
volatile unsigned char* pio_data;
DWORD kvm_events;
struct kvm_userspace_memory_region mem_slots[256];
DWORD mem_slots_num;

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
    DWORD bytes;
    BOOL ret;

    hGvmCpu = hGvmVm = hGvm = NULL;
    hGvm = CreateFileA("\\\\.\\gvm", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hGvm == INVALID_HANDLE_VALUE) {
        perror("Driver GVM isn't loaded");
        return -1;
    }

    //create vm
    bytes = 0;
    DeviceIoControl(hGvm, GVM_CREATE_VM, NULL, 0, &hGvmVm, sizeof(hGvmVm), &bytes, NULL);
    if (bytes != sizeof(ULONG64) || !hGvmVm) {
        perror("Driver GVM DeviceIoControl GVM_CREATE_VM");
        goto error;
    }

    bytes = 0;
    DeviceIoControl(hGvmVm, GVM_CREATE_VCPU, NULL, 0, &hGvmCpu, sizeof(hGvmCpu), &bytes, NULL);
    if (bytes != sizeof(ULONG64) || !hGvmCpu) {
        perror("Driver GVM DeviceIoControl GVM_CREATE_VCPU");
        goto error;
    }
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0;
    //kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xe0000);
    kvm_userspace_mem.userspace_addr = (uint64_t)ram;
    kvm_userspace_mem.memory_size = 0xa0000;
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

    printf("memory starts 0x%llx\n", (ULONG64)ram);

    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0xc0000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xc0000);
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
    kvm_userspace_mem.guest_phys_addr = 0xe0000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xe0000);
    kvm_userspace_mem.memory_size = 0x20000; 
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

    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0x100000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + 0x100000);
    //kvm_userspace_mem.memory_size = 1024 * 1024 * 256 - 0x100000;
    kvm_userspace_mem.memory_size = ram_size - 0x100000;
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

    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0xfffe0000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + ram_size + 0xe0000);
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
    mem_slots_num = 5;

    //TO DELETE make mmio 0x110000 - 0x120000
    /*
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0;
    kvm_userspace_mem.userspace_addr = 0;
    kvm_userspace_mem.memory_size = 0;
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
    
    memset(&kvm_userspace_mem, '\0', sizeof(kvm_userspace_mem));
    kvm_userspace_mem.guest_phys_addr = 0x100000;
    kvm_userspace_mem.userspace_addr = ((uint64_t)ram + 0x100000);
    kvm_userspace_mem.memory_size = ram_size - 0x100000;
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
    */
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
    ret = DeviceIoControl(hGvmCpu, GVM_CREATE_PIT_TIMER,
        NULL, 0, NULL, 0, &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_CREATE_PIT_TIMER");
        goto error;
    }
    
    
    return 0;
error:
    if (hGvm)
        CloseHandle(hGvm);
    if (hGvmVm)
        CloseHandle(hGvmVm);
    if (hGvmCpu)
        CloseHandle(hGvmCpu);
    return -1;
}

void gvm_register_mmio(unsigned int address, size_t size)
{
    struct kvm_userspace_memory_region newBlock;
    unsigned long memSize;
    unsigned int i, j;
    BOOL ret;
    DWORD bytes;

    for (i = 0; i < mem_slots_num; i++) {
        /* 1st case mmio address within one block */
        //address = 0xa0000
        //mem_slots[i].guest_phys_addr = 0xc0000
        if (mem_slots[i].guest_phys_addr >= address + size)
            continue;
        else if (mem_slots[i].guest_phys_addr <= address && address + size <= mem_slots[i].guest_phys_addr + mem_slots[i].memory_size) {
            /* if its the start of slot  */
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
                /* if its in the middle, we need to split it, in two blocks */
                memSize = mem_slots[i].memory_size;
                mem_slots[i].memory_size = 0;
                ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
                    &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
                    &bytes, (LPOVERLAPPED)NULL);
                if (!ret) {
                    perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
                    exit(-1);
                }

                newBlock.memory_size = memSize - (address - mem_slots[i].guest_phys_addr + size);
                mem_slots[i].memory_size = address - mem_slots[i].guest_phys_addr;
                
                ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
                    &mem_slots[i], sizeof(struct kvm_userspace_memory_region), NULL, 0,
                    &bytes, (LPOVERLAPPED)NULL);
                if (!ret) {
                    perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
                    exit(-1);
                }

                /* new block */
                newBlock.guest_phys_addr = address + size;
                newBlock.userspace_addr = mem_slots[i].userspace_addr + size;
                newBlock.slot = mem_slots_num;
                newBlock.flags = 0;
                ret = DeviceIoControl(hGvmVm, GVM_SET_USER_MEMORY_REGION,
                    &newBlock, sizeof(struct kvm_userspace_memory_region), NULL, 0,
                    &bytes, (LPOVERLAPPED)NULL);
                if (!ret) {
                    perror("Driver GVM DeviceIoControl GVM_SET_USER_MEMORY_REGION");
                    exit(-1);
                }
                /* move the  new block next to the one we split, move all the others one step */
                for (j = mem_slots_num; j >= i; j--)
                    mem_slots[j] = mem_slots[j - 1];
                mem_slots[i + 1] = newBlock;
                mem_slots_num++;
            }

            break;
        }
    }
}

void gvm_remove_mmio(unsigned int address, size_t size)
{
    unsigned int i;
    unsigned long memSize;
    BOOL ret;
    DWORD bytes;

    for (i = 0; i < mem_slots_num; i++) {
        /* 1st case mmio address within one block */
        if (mem_slots[i].guest_phys_addr >= address && address + size <= mem_slots[i].guest_phys_addr + mem_slots[i].memory_size) {
            /* if its the start of slot  */
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
        perror("Driver GVM DeviceIoControl GVM_SET_REGS");
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

    

    memset(&kvm_sregs, '\0', sizeof(kvm_regs));
    ret = DeviceIoControl(hGvmCpu, GVM_GET_SREGS,
        NULL, 0, &kvm_sregs, sizeof(kvm_sregs), &bytes,
        (LPOVERLAPPED)NULL);
    if (!ret) {
        perror("Driver GVM DeviceIoControl GVM_SET_REGS");
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
