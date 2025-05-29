#!/bin/bash ../install.sh

NAME='git'
VERSION='2.48.1'
DOWNLOAD_URL="https://www.kernel.org/pub/software/scm/git/git-$VERSION.tar.gz#51b4d03b1e311ba673591210f94f24a4c5781453e1eb188822e3d9cdc04c2212"
DEPENDENCIES=('zlib' 'openssl' 'curl')
CONFIGURE_OPTIONS=(
	'--with-curl'
	'--with-openssl'
	ac_cv_fread_reads_directories=no
	ac_cv_snprintf_returns_bogus=no
	ac_cv_lib_curl_curl_global_init=yes
)

build() {
	make -j$(nproc) CURL_LDFLAGS='-lcurl -lssl -lcrypto -lz -lzstd' || exit 1
}
