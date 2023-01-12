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
 
CFLAGS:=$(CFLAGS) -Iinclude -ffreestanding -Wall -Wextra -Werror=return-type
CPPFLAGS:=$(CPPFLAGS)
LIBBANK_CFLAGS:=$(CFLAGS) -D__is_kernel -Iinclude -ffreestanding -Wall -Wextra
LIBBANK_CPPFLAGS:=$(CPPFLAGS) -fno-rtti -fno-exceptions -fno-threadsafe-statics
 
ARCHDIR=arch/$(HOSTARCH)
 
include $(ARCHDIR)/make.config
 
CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(ARCH_CPPFLAGS)
LIBBANK_CFLAGS:=$(LIBBANK_CFLAGS) $(KERNEL_ARCH_CFLAGS)
LIBBANK_CPPFLAGS:=$(LIBBANK_CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
 
BUILDDIR=$(abspath build)
 
FREEOBJS=			\
$(ARCH_FREEOBJS)	\
BAN/String.o		\
BAN/StringView.o	\
BAN/Memory.o		\
 
HOSTEDOBJS=\
$(ARCH_HOSTEDOBJS) \
 
OBJS=\
$(FREEOBJS) \
$(HOSTEDOBJS) \
 
LIBBANK_OBJS=$(FREEOBJS:.o=.bank.o)
 
BINARIES=libbank.a
 
.PHONY: all always clean install install-headers install-libs
.SUFFIXES: .o .bank.o .cpp .S
 
all: $(BINARIES)
 
libban.a: always $(OBJS)
	cd $(BUILDDIR) && $(AR) rcs $@ $(OBJS)
 
libbank.a: always $(LIBBANK_OBJS)
	cd $(BUILDDIR) && $(AR) rcs $@ $(LIBBANK_OBJS)
 
.cpp.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(CFLAGS) $(CPPFLAGS)
 
.S.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(CFLAGS) $(CPPFLAGS)
 
.cpp.bank.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(LIBBANK_CFLAGS) $(LIBBANK_CPPFLAGS)
 
.S.bank.o:
	$(CXX) -MD -c $< -o $(BUILDDIR)/$@ $(LIBBANK_CFLAGS) $(LIBBANK_CPPFLAGS)
 
clean:
	rm -rf $(BUILDDIR)
 
always:
	mkdir -p $(BUILDDIR)/BAN
 
install: install-headers install-libs
 
install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.
 
install-libs: $(BINARIES)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(BUILDDIR)/$(BINARIES) $(DESTDIR)$(LIBDIR)
 
-include $(OBJS:.o=.d)
-include $(LIBBANK_OBJS:.o=.d)