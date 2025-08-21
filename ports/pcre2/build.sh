#!/bin/bash ../install.sh

NAME='pcre2'
VERSION='10.45'
DOWNLOAD_URL="https://github.com/PCRE2Project/pcre2/releases/download/pcre2-$VERSION/pcre2-$VERSION.tar.gz#0e138387df7835d7403b8351e2226c1377da804e0737db0e071b48f07c9d12ee"
CONFIG_SUB=('config.sub')

post_install() {
	# remove libtool files
	rm -f $BANAN_SYSROOT/usr/lib/libpcre2-8.la
	rm -f $BANAN_SYSROOT/usr/lib/libpcre2-posix.la
}
