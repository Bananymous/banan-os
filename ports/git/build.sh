#!/bin/bash ../install.sh

NAME='git'
VERSION='2.53.0'
DOWNLOAD_URL="https://www.kernel.org/pub/software/scm/git/git-$VERSION.tar.xz#5818bd7d80b061bbbdfec8a433d609dc8818a05991f731ffc4a561e2ca18c653"
DEPENDENCIES=('zlib' 'openssl' 'curl' 'libiconv')
CONFIGURE_OPTIONS=(
	'--with-curl'
	'--with-openssl'
	ac_cv_fread_reads_directories=no
	ac_cv_iconv_omits_bom=no
	ac_cv_lib_curl_curl_global_init=yes
	ac_cv_snprintf_returns_bogus=no
)
