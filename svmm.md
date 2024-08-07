
### Introduction
`SVMM`(Simple Virtual Machine Monitor) A virtual machine I developed in C, based on i440fx/piix3 architecture, using the `GVM` hypervisor. 

Provides all the virtual devices described in the i440fx/piix3 data sheets(PIT,
PIC, APIC, IDE, ATA/ATAPI, VGA, ACPI). Supports guest debugging with a new debugger written from scratch `SvmmDebugger`. It is using the BIOS and VGABios provided
from bochs. Wrote a small bios tests for it, to test the virtual devices I created. 

The initial idea was to re-write `bochs`(virtual machine-emulator written in c++) in c and instead of emulating the cpu, use a hypervisor to make it faster. 

To keep the asthetic of `bochs` I have copied only the gui(`win32gui.c`, `gui.c`) with small changes. In some cases I am trying to keep similar state machine virtual device structure(hence the multiple embedded switch cases in `harddisk.c`) so I can faster identify a bug in the virtual device.

Currently the project allows booting from `cdrom` and `vhd` hard disks. That being said most of my experimentation was with booting reactos, which doesn't fully boot due to lack of proper support for `IDE` controllers. 

This is a project that I work on my spare time and I am aiming to publicly release once it fully boots windows 10.

### SVMM
The `SVMM` is consisted of 3 projects. The `GVM` hypervisor, which is `kvm` ported for windows written by google, that I added few things to support the `VMX_EXIT_TIMER` intel feature. The `VMX_EXIT_TIMER` is used as a core timer source, to synchronize all timer devices(PIT, CMOS etc).

The main project `SVMM`, which is the virtual machine that implements all the virtual devices according to i440fx/piix3 data sheets and provides handlers for all the basic vmexits `MMIO`, `PORTIO` etc.

As the project was moving forward there were situation that debugging was needed. Initially that was handled by the `VMX_EXIT_MTF` exit, which creates a `vmexit` after the execution of every instruction. Then when I started booting operating systems like ReactOS(an open source version of windows 2003), bugs became more complex, which led to the creation of `SvmmDebugger`. A gui debugger written from scratch, which beyond all the basic debugger features it also supports `pdb` parsing so I can have source level debugging in ReactOS.

### SVMM Internals
The main source file of `SVMM` is `svmm.c`. `SVMM` first allocates allocates all the needed memory(ram) for the guest and initialize the communication with the hypervisor in `gvm_init()`. `gvm_init` also sets all the supported `MSR/CPUID` features according to the physical cpu, creates a virtual cpu and sets the `EPT` table to move to execution of bios. 

Later, calls each function that initializes a specific virtual device and loads the bios image and the `vgabios`. Then after the cpu registers have been set according to the manual, the execution of bios start calling the ioctl `GVM_RUN`, which is going to transit to the `vmx non-root` mode. The ioctl will only return in case of a `vmexit` that first gets handled by `GVM` and then we can further process it in our virtual machine if needed. The most common exits are related with the communication of a virtual device (via MMIO, PortIO, or interrupt).

In beggining `SVMM` was using `HAXM` instead of `GVM` hypervisor. As a result in the main source file `svmm.c`, there are still `#ifdef` statement that allow you to use `HAXM`. 



### SVMM Setup
To setup `SVMM` you should first make sure that you are able to load a non-signed driver in windows. Open a `cmd.exe` as an administrator and execute the following to load and execute the driver.

```cmd
sc create gvm type=kernel binpath="C\Users\user\Desktop\gvm.sys"
sc start gvm
```

Then `SVMM` by default will look for reactos `bootcd.iso` that should be in the same directory with `Svmm.exe`. From a cmd you can simply execute it. It should run bios, then reactos boot process and freeze due to the lack of proper `IDE` support from reactos.

```cmd
svmm.exe
```

If you need a debugger support you should run it as

```cmd
svmm.exe 50001
```
and from another `cmd.exe`, do

```cmd
SvmmDebugger.exe
```
Click on the connect button and then `connect`. You should be able to step through the bios-reactos assembly code(with F10-F11, supports couple windbg commands).

If you want to check the support for `pdb` source code stepping or the source code of the debugger let me know.