# Build instructions
## Prerequisites

### Building
- [CMake](https://cmake.org/)
- [clang](https://clang.llvm.org/)
- [lld](https://lld.llvm.org/) (Version containg lld-link)

### Creating image
- [dd](https://www.man7.org/linux/man-pages/man1/dd.1.html) (Used to create disk image)
- mtools (Used to create and manage FAT filesystem)
   - [Arch package](https://archlinux.org/packages/extra/x86_64/mtools/)
   - [Ubuntu package](http://manpages.ubuntu.com/manpages/trusty/man1/mtools.1.html)
 
### Running
- [QEMU](https://www.qemu.org/)
- [OVMF](https://github.com/tianocore/tianocore.github.io/wiki/OVMF) (UEFI firmware)

## Instructions for building and running on linux
```
mkdir build
cd build
cmake .. -DUEFI_FIRMWARE=<path to OVMF.fd> -DQEMU_ARGS=<any extra qemu args>
make run
```
