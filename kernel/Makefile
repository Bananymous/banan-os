DEFAULT_HOST!=../default-host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)
 
CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=
 
DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include
 
CFLAGS:=$(CFLAGS) -D__is_kernel -Iinclude -fstack-protector -ffreestanding -Wall -Wextra -Werror=return-type -fno-omit-frame-pointer -mno-sse -mno-sse2
CPPFLAGS:=$(CPPFLAGS) -fno-rtti -fno-exceptions
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib -lk -lbank -lgcc
 
ARCHDIR=arch/$(HOSTARCH)
 
include $(ARCHDIR)/make.config
 
CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

BUILDDIR=$(abspath build)
 
KERNEL_OBJS=					\
$(KERNEL_ARCH_OBJS)				\
kernel/APIC.o					\
kernel/build_libc.o				\
kernel/CPUID.o					\
kernel/Debug.o					\
kernel/font.o					\
kernel/Input.o					\
kernel/InterruptController.o	\
kernel/kernel.o					\
kernel/kmalloc.o				\
kernel/PIC.o					\
kernel/PIT.o					\
kernel/RTC.o					\
kernel/Serial.o					\
kernel/Shell.o					\
kernel/SpinLock.o				\
kernel/SSP.o					\
kernel/TTY.o					\
kernel/VesaTerminalDriver.o		\
icxxabi.o						\
 
OBJS=					\
$(ARCHDIR)/crti.o		\
$(ARCHDIR)/crtbegin.o	\
$(KERNEL_OBJS)			\
$(ARCHDIR)/crtend.o		\
$(ARCHDIR)/crtn.o		\
 
LINK_LIST=				\
$(LDFLAGS)				\
$(ARCHDIR)/crti.o		\
$(ARCHDIR)/crtbegin.o	\
$(KERNEL_OBJS)			\
$(LIBS)					\
$(ARCHDIR)/crtend.o		\
$(ARCHDIR)/crtn.o		\
 
.PHONY: all always clean install install-headers install-kernel
.SUFFIXES: .o .c .cpp .S
 
all: banan-os.kernel
 
banan-os.kernel: always $(OBJS) $(ARCHDIR)/linker.ld
	cd $(BUILDDIR) && $(CXX) -T ../$(ARCHDIR)/linker.ld -o banan-os.kernel $(CFLAGS) $(CPPFLAGS) $(LINK_LIST)
	cd $(BUILDDIR) && grub-file --is-x86-multiboot banan-os.kernel
 
$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $(BUILDDIR)/$@
 
.c.o:
	$(CC) -MD -c $< -o $(BUILDDIR)/$@ $(CFLAGS)
 
.cpp.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(CFLAGS) $(CPPFLAGS)
 
.S.o:
	$(CC) -MD -c $< -o $(BUILDDIR)/$@ $(CFLAGS)
 
always:
	mkdir -p $(BUILDDIR)/$(ARCHDIR)
	mkdir -p $(BUILDDIR)/kernel

clean:
	rm -rf $(BUILDDIR)

install: install-headers install-kernel
 
install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.
 
install-kernel: banan-os.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp $(BUILDDIR)/banan-os.kernel $(DESTDIR)$(BOOTDIR)
 
-include $(OBJS:.o=.d)