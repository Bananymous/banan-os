#!/bin/bash ../install.sh

NAME='git'
VERSION='2.52.0'
DOWNLOAD_URL="https://www.kernel.org/pub/software/scm/git/git-$VERSION.tar.xz#3cd8fee86f69a949cb610fee8cd9264e6873d07fa58411f6060b3d62729ed7c5"
DEPENDENCIES=('zlib' 'openssl' 'curl')
CONFIGURE_OPTIONS=(
	'--with-curl'
	'--with-openssl'
	ac_cv_fread_reads_directories=no
	ac_cv_snprintf_returns_bogus=no
	ac_cv_lib_curl_curl_global_init=yes
)
