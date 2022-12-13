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
INCLUDEDIR?=$(PREFIX)/include
LIBDIR?=$(EXEC_PREFIX)/lib
 
CFLAGS:=$(CFLAGS) -D__is_ban -Iinclude -ffreestanding -Wall -Wextra
CPPFLAGS:=$(CPPFLAGS)
LIBK_CFLAGS:=$(CFLAGS) -D__is_kernel
LIBK_CPPFLAGS:=$(CPPFLAGS)
 
ARCHDIR=arch/$(HOSTARCH)
 
include $(ARCHDIR)/make.config
 
CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(ARCH_CPPFLAGS)
BANK_CFLAGS:=$(BANK_CFLAGS) $(KERNEL_ARCH_CFLAGS)
BANK_CPPFLAGS:=$(BANK_CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
 
BUILDDIR=$(abspath build)
 
FREEOBJS=\
$(ARCH_FREEOBJS) \
 
HOSTEDOBJS=\
$(ARCH_HOSTEDOBJS) \
 
OBJS=\
$(FREEOBJS) \
$(HOSTEDOBJS) \
 
BANK_OBJS=$(FREEOBJS:.o=.bank.o)
 
BINARIES=libbank.a
 
.PHONY: all always clean install install-headers install-libs
.SUFFIXES: .o .bank.o .cpp .S
 
all: $(BINARIES)
 
libban.a: always $(OBJS)
	cd $(BUILDDIR) && $(AR) rcs $@ $(OBJS)
 
libbank.a: always $(LIBK_OBJS)
	cd $(BUILDDIR) && $(AR) rcs $@ $(LIBK_OBJS)
 
.cpp.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(CFLAGS) $(CPPFLAGS)
 
.S.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(CFLAGS) $(CPPFLAGS)
 
.cpp.bank.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)
 
.S.bank.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)
 
clean:
	rm -rf $(BUILDDIR)
 
always:
	mkdir -p $(BUILDDIR)
 
install: install-headers install-libs
 
install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.
 
install-libs: $(BINARIES)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(BUILDDIR)/$(BINARIES) $(DESTDIR)$(LIBDIR)
 
-include $(OBJS:.o=.d)
-include $(LIBK_OBJS:.o=.d)