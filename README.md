![lines](https://img.shields.io/tokei/lines/github/bananymous/banan-os)
![license](https://img.shields.io/github/license/bananymous/banan-os)

# banan-os
This is my hobby operating system written in C++. Currently runs on x86 based architectures (32 and 64 bit). Currently this operating system supports ext2 filesystem, ata drives (pio mode), vesa/vbe graphics mode and multithreaded processing.

![ss](https://user-images.githubusercontent.com/68776844/230458915-a0c3f1b4-e747-401a-bc0c-07a62d18927c.png)

## Building
> **_NOTE:_** I will be using ninja in these build instructions but you may use any other buildsystem cmake supports. If you want to use ninja, you can either define a environment variable 'CMAKE_GENERATOR=Ninja' or pass '-G Ninja' to all commands invoking cmake.

Create the build directory and cofigure cmake
```sh
mkdir build
cd build
cmake ..
```

&nbsp;

> **_NOTE:_** The following step has to be done only once. This might take a long time since we are compiling binutils and gcc.

To build the toolchain for this os. You can run the following command.
```sh
ninja toolchain
cmake --fresh .. # We need to reconfigure cmake to use the new compiler
ninja libstdc++
```

&nbsp;

To build the os itself you can run either of the following commands. You will need root access since we need to mount the disk image to install grub.
```sh
ninja qemu
ninja bochs
```

&nbsp;

You can also build the kernel or disk image without running it:
```sh
ninja kernel
ninja image
```

&nbsp;

If you have corrupted your disk image or want to create new one, you can either manually delete _banan-os.img_ and cmake will automatically create you a new one or you can run the following command.
```sh
ninja image-full
```
