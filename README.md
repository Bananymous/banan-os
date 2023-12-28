![license](https://img.shields.io/github/license/bananymous/banan-os)

# banan-os

This is my hobby operating system written in C++. Currently supports only x86\_64 architecture. We have a ext2 filesystem, basic ramfs, IDE disk drivers in ATA PIO mode, ATA AHCI drivers, userspace processes, executable loading from ELF format, linear VBE graphics and multithreaded processing on single core.

![screenshot from qemu running banan-os](assets/banan-os.png)

## Code structure

Each major component and library has its own subdirectory (kernel, userspace, libc, ...). Each directory contains directory *include*, which has **all** of the header files of the component. Every header is included by its absolute path.

## Building

### Needed packages

#### apt (tested on ubuntu 22.04)
```# apt install build-essential git ninja-build texinfo bison flex libgmp-dev libmpfr-dev libmpc-dev parted qemu-system-x86```

> ***NOTE:*** You need cmake version of atleast 2.26. If you are using cmake that is not found from PATH, you can set the CMAKE\_COMMAND environment variable to point to the correct cmake binary. Or you can let build script download correct version of cmake if you don't have one.

When you clone this reposity, make sure to also clone submodules. This can be done by cloning with the command ```git clone --recurse-submodules https://git.bananymous.com/bananymous/banan-os.git``` or if you have already cloned this repo, run ```git submodule init && git submodule update```.

To build the toolchain for this os. You can run the following command.
> ***NOTE:*** The following step has to be done only once. This might take a long time since we are compiling binutils and gcc.
```sh
./script/build.sh toolchain
```

To build the os itself you can run one of the following commands. You will need root access for disk image creation/modification.
```sh
./script/build.sh qemu
./script/build.sh qemu-nographic
./script/build.sh qemu-debug
./script/build.sh bochs
```

You can also build the kernel or disk image without running it:
```sh
./script/build.sh kernel
./script/build.sh image
```

If you have corrupted your disk image or want to create new one, you can either manually delete *build/banan-os.img* and build system will automatically create you a new one or you can run the following command.
```sh
./script/build.sh image-full
```

If you feel like ```./script/build.sh``` is too verbose, there exists a symlink _bos_ in this projects root directory. All build commands can be used with ```./bos args...``` instead.

I have also created shell completion script for zsh. You can either copy the file in _script/shell-completion/zsh/\_bos_ to _/usr/share/zsh/site-functions/_ or add the _script/shell-completion/zsh_ to your fpath in _.zshrc_.

### Contributing

Currently I don't accept contributions to this repository unless explicitly told otherwise. This is a learning project for me and I want to do everything myself. Feel free to fork/clone this repo and tinker with it yourself.
