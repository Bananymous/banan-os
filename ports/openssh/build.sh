#!/bin/bash ../install.sh

NAME='openssh'
VERSION='10.0p1'
DOWNLOAD_URL="https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-$VERSION.tar.gz#021a2e709a0edf4250b1256bd5a9e500411a90dddabea830ed59cef90eb9d85c"
DEPENDENCIES=('zlib' 'openssl')
CONFIG_SUB=('config.sub')
MAKE_INSTALL_TARGETS=('install-nokeys')
CONFIGURE_OPTIONS=(
	'--sysconfdir=/etc'
	'--sbindir=/usr/bin'
	'--disable-fd-passing'
	'CFLAGS=-Wno-deprecated-declarations'
)

post_configure() {
	sed -i 's|#define HAVE_IFADDRS_H 1|/* #undef HAVE_IFADDRS_H */|' config.h || exit 1
}

post_install() {
	passwd="$BANAN_SYSROOT/etc/passwd"
	test "$(tail -c 1 "$passwd")" && echo >> $passwd
	grep -q '^sshd:' "$passwd" || echo 'sshd:x:74:74:Privilege-separated SSH:/var/empty/sshd:/bin/nologin' >> "$passwd"

	group="$BANAN_SYSROOT/etc/group"
	test "$(tail -c 1 "$group")" && echo >> $group
	grep -q '^sshd:' "$group" || echo 'sshd:x:74:' >> "$group"
}
