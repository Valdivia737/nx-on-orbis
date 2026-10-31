# source this: tools for building C deps (OpenSSL, FFmpeg...) for the PS4 from Git Bash
P=/c/ps4dev/nx-on-orbis/tools/msys-perl/usr
export PERL5LIB=$P/share/perl5/core_perl:$P/lib/perl5/core_perl
export PATH="/c/ps4dev/nx-on-orbis/tools/bin:$P/bin:/c/ps4dev/nx-on-orbis/tools/usr/bin:/c/ps4dev/tools/llvm/bin:/c/ps4dev/tools/cmake-4.4.3-windows-x86_64/bin:/c/ps4dev/tools/ninja:/c/ps4dev/tools/glslang/bin:$PATH"
export PS4_SDK=C:/ps4dev/nx-on-orbis/sdk-dl/orbis-sdk-v1/sdk
export PS4_COMPAT=C:/ps4dev/nx-on-orbis/sdk-dl/orbis-sdk-v1/orbis-compat
export PS4_OVERLAY=C:/ps4dev/nx-on-orbis/toolchain/include-overlay
export PS4_PREFIX=C:/ps4dev/nx-on-orbis/prefix
export PS4_CFLAGS="--target=x86_64-pc-freebsd12-elf -O2 -fPIC -funwind-tables -D__PS4__ -D__ORBIS__ -D_BSD_SOURCE=1 -U__FreeBSD__ -isysroot $PS4_SDK -isystem $PS4_OVERLAY -isystem $PS4_COMPAT/include -isystem $PS4_SDK/include -include orbis_prefix.h"
