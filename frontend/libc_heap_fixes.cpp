// SPDX-License-Identifier: GPL-3.0-or-later
// eden-ps4: musl's heap growth, with its step capped.
//
// The SDK's musl (oldmalloc) grows the heap through __expand_heap, which on the PS4 has no brk and
// always maps: max(request, 16 KiB << (mmap_step / 2)), then mmap_step++. The minimum doubles every
// two expansions, so after ~30 of them a 96-byte operator new asks for 512 MiB. orbis-compat serves
// those mappings from direct-memory carve-outs that are committed at once and capped at 1.5 GiB,
// and with dynarmic's code caches at 64 MiB (more blocks, more JIT bookkeeping) the expansions went
// 128, 128, 256, 256 MiB in one second and the 512 MiB one was refused: operator new returned null
// with 866 MiB of direct memory still free. This definition replaces musl's (the linker takes it
// before extracting expand_heap.o from libc.a) and is the same code, disassembled from the SDK's,
// except that the step stops doubling at MaxStepBytes.

#include <cerrno>
#include <cstddef>
#include <cstdint>

extern "C" void* __mmap(void* addr, size_t len, int prot, int flags, int fd, long off);

namespace {

constexpr size_t Page = 0x4000; // musl's PAGE_SIZE on the PS4 (orbis_mmap.cpp)
constexpr size_t MaxStepBytes = 32ull << 20;
constexpr int ProtReadWrite = 3;   // PROT_READ | PROT_WRITE
constexpr int PrivateAnon = 0x1002; // MAP_PRIVATE | MAP_ANON, the tuple orbis-compat serves from carve-outs

unsigned g_mmap_step;

} // Anonymous namespace

/// Called by musl's malloc with its heap lock held: maps at least *pn bytes, stores the mapped
/// size in *pn and returns the area, or returns null (errno ENOMEM for an absurd size).
extern "C" void* __expand_heap(size_t* pn) {
    size_t n = *pn;
    if (n >= 0x7fffffffffffc000ull) {
        errno = ENOMEM;
        return nullptr;
    }
    n += (0 - n) & (Page - 1);
    size_t min = Page << (g_mmap_step / 2);
    if (min > MaxStepBytes) {
        min = MaxStepBytes;
    }
    if (n < min) {
        n = min;
    }
    void* area = __mmap(nullptr, n, ProtReadWrite, PrivateAnon, -1, 0);
    if (area == reinterpret_cast<void*>(-1)) {
        return nullptr;
    }
    *pn = n;
    ++g_mmap_step;
    return area;
}
