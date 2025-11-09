#!/bin/bash ../install.sh

NAME='openssh'
VERSION='10.2p1'
DOWNLOAD_URL="https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-$VERSION.tar.gz#ccc42c0419937959263fa1dbd16dafc18c56b984c03562d2937ce56a60f798b2"
DEPENDENCIES=('zlib' 'openssl')
CONFIG_SUB=('config.sub')
MAKE_INSTALL_TARGETS=('install-nokeys')
CONFIGURE_OPTIONS=(
	'--sysconfdir=/etc'
	'--sbindir=/usr/bin'
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
