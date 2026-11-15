// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

// Freestanding Linux executable: compiler, C++ library, and Linux UAPI headers plus direct system calls.
// FinderWsl.cpp consumes the stream described by FinderWSLProtocol.h.

#include "FinderWSLProtocol.h"
#include <array>
#include <bit>
#include <span>
#include <stdckdint.h>
#include <stdatomic.h>
#include <limits.h>
#include <asm/unistd.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/futex.h>
#include <linux/limits.h>
#include <linux/mman.h>
#include <linux/openat2.h>
#include <linux/sched.h>
#include <linux/stat.h>

#if !defined(__x86_64__) && !defined(__aarch64__)
#error Unsupported WSL processor
#endif

constexpr size_t ListingSize = 64 * 1024;
constexpr size_t OutputSize = 256 * 1024;
constexpr unsigned StatFields = STATX_BASIC_STATS | STATX_MNT_ID;
constexpr unsigned DirectoryFlags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NOATIME | O_CLOEXEC;
constexpr unsigned WorkerFlags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
    CLONE_SYSVSEM | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

static long SystemCall(long number, long a, long b, long c, long d, long e, long f)
{
#if defined(__x86_64__)
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    __asm__ volatile("syscall" : "+a"(number) : "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory", "cc");
    return number;
#else
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    register long x4 __asm__("x4") = e;
    register long x5 __asm__("x5") = f;
    __asm__ volatile("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc");
    return x0;
#endif
}

// Clang may emit these operations even in a freestanding translation unit.
extern "C" void* memcpy(void* __restrict destination, const void* __restrict source, size_t size)
{
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    for (size_t i = 0; i < size; ++i) out[i] = in[i];
    return destination;
}

extern "C" void* memset(void* destination, int value, size_t size)
{
    auto* out = static_cast<unsigned char*>(destination);
    for (size_t i = 0; i < size; ++i) out[i] = (unsigned char)value;
    return destination;
}

[[noreturn]] static void Exit(int code)
{
    SystemCall(__NR_exit_group, code, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

[[noreturn]] static void Fail(void)
{
    constexpr char message[] = "FinderWSLRunner: cannot complete the scan\n";
    SystemCall(__NR_write, 2, (long)message, sizeof(message) - 1, 0, 0, 0);
    Exit(2);
}

// The getdents64 record layout is not exported by the Linux UAPI headers.
struct DirectoryEntry
{
    uint64_t inode;
    int64_t offset;
    uint16_t length;
    uint8_t type;
    char name[];
};

struct Directory
{
    struct Directory* parent;
    long descriptor;
    uint64_t inode;
    size_t used, position;
    alignas(8) std::array<unsigned char, ListingSize> listing;
};

struct Worker
{
    atomic_int tid;
    struct Directory* freeDirectories;
    size_t outputUsed;
    uint64_t entries;
    std::array<unsigned char, OutputSize> output;
    alignas(16) std::array<unsigned char, 64 * 1024> stack;
};

struct Task
{
    long descriptor;
    uint64_t inode;
};

static constinit std::array<Worker, 64> workers{};
static constinit atomic_int outputLock, queueLock, queueChanged;
static constinit std::array<Task, 128> tasks{};
static constinit size_t taskCount, pending;
static constinit uint64_t rootMount;

static void Lock(atomic_int* lock)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(lock, &expected, 1, memory_order_acquire, memory_order_relaxed)) return;
    while (atomic_exchange_explicit(lock, 2, memory_order_acquire) != 0)
        SystemCall(__NR_futex, (long)lock, FUTEX_WAIT_PRIVATE, 2, 0, 0, 0);
}

static void Unlock(atomic_int* lock)
{
    if (atomic_exchange_explicit(lock, 0, memory_order_release) == 2)
        SystemCall(__NR_futex, (long)lock, FUTEX_WAKE_PRIVATE, 1, 0, 0, 0);
}

static bool Submit(long descriptor, uint64_t inode)
{
    Lock(&queueLock);
    const bool submitted = taskCount < tasks.size();
    if (submitted)
    {
        tasks[taskCount++] = Task{ descriptor, inode };
        ++pending;
        atomic_fetch_add_explicit(&queueChanged, 1, memory_order_relaxed);
    }
    Unlock(&queueLock);
    if (submitted) SystemCall(__NR_futex, (long)&queueChanged, FUTEX_WAKE_PRIVATE, 1, 0, 0, 0);
    return submitted;
}

static bool Take(struct Task* task)
{
    for (;;)
    {
        Lock(&queueLock);
        if (taskCount != 0 || pending == 0)
        {
            const bool found = taskCount != 0;
            if (found) *task = tasks[--taskCount];
            Unlock(&queueLock);
            return found;
        }
        const int sequence = atomic_load_explicit(&queueChanged, memory_order_relaxed);
        Unlock(&queueLock);
        SystemCall(__NR_futex, (long)&queueChanged, FUTEX_WAIT_PRIVATE, sequence, 0, 0, 0);
    }
}

static void Flush(struct Worker* worker)
{
    Lock(&outputLock);
    for (size_t offset = 0; offset < worker->outputUsed;)
    {
        const long written = SystemCall(__NR_write, 1, (long)(worker->output.data() + offset),
            (long)(worker->outputUsed - offset), 0, 0, 0);
        if (written == -EINTR) continue;
        if (written <= 0) Exit(3);
        offset += (size_t)written;
    }
    Unlock(&outputLock);
    worker->outputUsed = 0;
}

static void Emit(struct Worker* worker, const void* data, size_t size)
{
    memcpy(worker->output.data() + worker->outputUsed, data, size);
    worker->outputUsed += size;
}

static void EmitEntry(struct Worker* worker, const struct statx* status, uint64_t parent, const char* name,
    size_t nameLength, uint8_t type, uint8_t flags)
{
    uint64_t allocated;
    if (ckd_mul(&allocated, status->stx_blocks, 512u)) Fail();
    const struct WslEntryRecord record = {
        .kind = WslKindEntry, .type = type, .flags = flags, .reserved = 0,
        .nameLength = (uint16_t)nameLength, .reserved2 = 0, .links = status->stx_nlink,
        .inode = status->stx_ino, .parent = parent,
        .size = type == WslTypeDirectory ? 0 : status->stx_size, .allocated = allocated,
        .seconds = status->stx_mtime.tv_sec, .nanoseconds = status->stx_mtime.tv_nsec, .reserved3 = 0
    };
    // Keep a record and its name in the same locked write, even when workers flush concurrently.
    if (sizeof(record) + nameLength > worker->output.size() - worker->outputUsed) Flush(worker);
    Emit(worker, &record, sizeof(record));
    Emit(worker, name, nameLength);
    ++worker->entries;
}

static void EmitFailure(struct Worker* worker, uint64_t inode, long error)
{
    const struct WslShortRecord record = {
        .kind = WslKindFailure, .reserved = {}, .error = (uint32_t)-error, .value = inode
    };
    if (sizeof(record) > worker->output.size() - worker->outputUsed) Flush(worker);
    Emit(worker, &record, sizeof(record));
}

static long GetStatus(long descriptor, const char* name, struct statx* status)
{
    long result;
    do
    {
        // Do not follow links, trigger automounts, or synchronize remote metadata.
        result = SystemCall(__NR_statx, descriptor, (long)name,
            AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT | AT_EMPTY_PATH | AT_STATX_DONT_SYNC, StatFields, (long)status, 0);
    } while (result == -EINTR);
    if (result == 0 && (status->stx_mask & StatFields) != StatFields) return -EIO;
    return result;
}

static long OpenDirectory(long parent, const char* name, bool root)
{
    struct open_how how = {
        .flags = DirectoryFlags, .mode = 0,
        .resolve = root ? 0ull : RESOLVE_NO_XDEV | RESOLVE_NO_SYMLINKS | RESOLVE_BENEATH
    };
    for (;;)
    {
        // Resolve each child relative to its open parent; renames cannot redirect the walk through a link or mount.
        const long result = SystemCall(__NR_openat2, parent, (long)name, (long)&how, sizeof(how), 0, 0);
        if (result == -EPERM && (how.flags & O_NOATIME))
        {
            how.flags &= ~(uint64_t)O_NOATIME;
            continue;
        }
        if (result != -EINTR) return result;
    }
}

static struct Directory* PushDirectory(struct Worker* worker, struct Directory* parent,
    long descriptor, uint64_t inode)
{
    struct Directory* directory = worker->freeDirectories;
    if (directory != nullptr) worker->freeDirectories = directory->parent;
    else
    {
        const long memory = SystemCall(__NR_mmap, 0, sizeof(*directory),
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if ((uint64_t)memory >= (uint64_t)-4095) Fail();
        directory = (struct Directory*)memory;
    }
    directory->parent = parent;
    directory->descriptor = descriptor;
    directory->inode = inode;
    directory->used = directory->position = 0;
    return directory;
}

static void Scan(struct Worker* worker, const struct Task* task)
{
    struct Directory* directory = PushDirectory(worker, nullptr, task->descriptor, task->inode);
    while (directory != nullptr)
    {
        if (directory->position == directory->used)
        {
            const long count = SystemCall(__NR_getdents64, directory->descriptor, (long)directory->listing.data(),
                directory->listing.size(), 0, 0, 0);
            if (count == -EINTR) continue;
            if (count <= 0)
            {
                if (count < 0) EmitFailure(worker, directory->inode, count);
                SystemCall(__NR_close, directory->descriptor, 0, 0, 0, 0, 0);
                struct Directory* finished = directory;
                directory = directory->parent;
                finished->parent = worker->freeDirectories;
                worker->freeDirectories = finished;
                continue;
            }
            directory->used = (size_t)count;
            directory->position = 0;
        }

        // Validate variable-length kernel records before accessing their names.
        const size_t remaining = directory->used - directory->position;
        const auto* entry = reinterpret_cast<const DirectoryEntry*>(directory->listing.data() + directory->position);
        if (remaining < offsetof(struct DirectoryEntry, name) + 1 || entry->length > remaining ||
            entry->length < offsetof(struct DirectoryEntry, name) + 1 || entry->length % 8 != 0) Fail();
        const size_t capacity = entry->length - offsetof(struct DirectoryEntry, name);
        size_t length = 0;
        while (length < capacity && entry->name[length] != '\0') ++length;
        if (length == 0 || length == capacity || length > NAME_MAX) Fail();
        directory->position += entry->length;
        const char* name = entry->name;
        if (name[0] == '.' && (length == 1 || (length == 2 && name[1] == '.'))) continue;

        struct statx status;
        long result = GetStatus(directory->descriptor, name, &status);
        if (result < 0)
        {
            if (result != -ENOENT) EmitFailure(worker, directory->inode, result);
            continue;
        }
        const uint8_t type = S_ISDIR(status.stx_mode) ? WslTypeDirectory :
            S_ISLNK(status.stx_mode) ? WslTypeSymlink : S_ISREG(status.stx_mode) ? WslTypeFile : WslTypeOther;
        const uint8_t flags = status.stx_mnt_id != rootMount ? WslFlagMount : 0;
        EmitEntry(worker, &status, directory->inode, name, length, type, flags);
        if (type != WslTypeDirectory || flags != 0) continue;

        // Verify the opened directory still identifies the entry we emitted.
        const long child = OpenDirectory(directory->descriptor, name, false);
        struct statx opened;
        result = child < 0 ? child : GetStatus(child, "", &opened);
        if (result == 0 && (opened.stx_ino != status.stx_ino || opened.stx_mnt_id != rootMount)) result = -ESTALE;
        if (result < 0)
        {
            if (child >= 0) SystemCall(__NR_close, child, 0, 0, 0, 0, 0);
            EmitFailure(worker, status.stx_ino, result);
            continue;
        }
        // Bound queued descriptors; a busy pool walks additional children locally.
        if (!Submit(child, status.stx_ino)) directory = PushDirectory(worker, directory, child, status.stx_ino);
    }
}

static void WorkerMain(struct Worker* worker) __asm__("WorkerMain");

__attribute__((used)) static void WorkerMain(struct Worker* worker)
{
    struct Task task;
    while (Take(&task))
    {
        Scan(worker, &task);
        Lock(&queueLock);
        const bool finished = --pending == 0;
        if (finished) atomic_fetch_add_explicit(&queueChanged, 1, memory_order_relaxed);
        Unlock(&queueLock);
        if (finished) SystemCall(__NR_futex, (long)&queueChanged, FUTEX_WAKE_PRIVATE, INT_MAX, 0, 0, 0);
    }
    Flush(worker);
}

extern "C" long StartWorker(struct Worker* worker, void* stack, atomic_int* tid, long flags);

#define STRINGIFY_VALUE(value) #value
#define STRINGIFY(value) STRINGIFY_VALUE(value)

// clone installs a separate stack; the child must enter the worker without using the parent's stack frame.
#if defined(__x86_64__)
__asm__(".text\nStartWorker:\n"
    "sub $16,%rsi\nmov %rdi,(%rsi)\nmov %rcx,%rdi\nmov %rdx,%r10\nxor %r8d,%r8d\n"
    "mov $" STRINGIFY(__NR_clone) ",%eax\nsyscall\ntest %rax,%rax\njnz 1f\n"
    "mov (%rsp),%rdi\ncall WorkerMain\nmov $" STRINGIFY(__NR_exit) ",%eax\n"
    "xor %edi,%edi\nsyscall\nud2\n1: ret\n");
#else
__asm__(".text\nStartWorker:\n"
    "sub x1,x1,#16\nstr x0,[x1]\nmov x0,x3\nmov x4,x2\nmov x3,#0\n"
    "mov x8,#" STRINGIFY(__NR_clone) "\nsvc 0\ncbnz x0,1f\n"
    "ldr x0,[sp]\nbl WorkerMain\nmov x8,#" STRINGIFY(__NR_exit) "\n"
    "mov x0,#0\nsvc 0\nbrk #0\n1: ret\n");
#endif

[[noreturn]] static void Run(const long* stack) __asm__("Run");

[[noreturn]] __attribute__((used)) static void Run(const long* stack)
{
    const long argc = stack[0];
    char* const* argv = (char* const*)(stack + 1);
    long argument = 1;
    if (argc == 3)
    {
        constexpr char option[] = "--self-delete";
        for (size_t i = 0; i < sizeof(option); ++i) if (argv[1][i] != option[i]) Fail();
        std::array<char, PATH_MAX> path;
        const long length = SystemCall(__NR_readlinkat, AT_FDCWD, (long)"/proc/self/exe",
            (long)path.data(), path.size() - 1, 0, 0);
        if (length > 0 && (size_t)length < path.size() - 1)
        {
            path[length] = '\0';
            SystemCall(__NR_unlinkat, AT_FDCWD, (long)path.data(), 0, 0, 0, 0);
        }
        ++argument;
    }
    if (argc != argument + 1) Fail();

    const long descriptor = OpenDirectory(AT_FDCWD, argv[argument], true);
    struct statx root;
    if (descriptor < 0 || GetStatus(descriptor, "", &root) < 0) Fail();
    rootMount = root.stx_mnt_id;
    Emit(&workers[0], &WslStreamHeaderValue, sizeof(WslStreamHeaderValue));
    EmitEntry(&workers[0], &root, 0, "", 0, WslTypeDirectory, 0);
    Flush(&workers[0]);
    Submit(descriptor, root.stx_ino);

    // Metadata reads benefit from concurrent workers, including on a cold filesystem cache.
    std::array<uint64_t, 16> affinity{};
    unsigned processors = 0;
    SystemCall(__NR_sched_getaffinity, 0, sizeof(affinity), (long)affinity.data(), 0, 0, 0);
    for (const uint64_t bits : affinity) processors += std::popcount(bits);
    const unsigned wanted = processors == 0 ? 1 : processors > 32 ? 64 : processors * 2;
    unsigned started = 1;
    for (; started < wanted; ++started)
    {
        struct Worker* worker = &workers[started];
        if (StartWorker(worker, worker->stack.data() + worker->stack.size(), &worker->tid, WorkerFlags) < 0) break;
    }
    WorkerMain(&workers[0]);
    uint64_t entries = workers[0].entries;
    for (Worker& worker : std::span{ workers }.subspan(1, started - 1))
    {
        // The kernel clears the child TID and wakes a shared futex when the thread exits.
        for (int tid; (tid = atomic_load_explicit(&worker.tid, memory_order_acquire)) != 0;)
            SystemCall(__NR_futex, (long)&worker.tid, FUTEX_WAIT, tid, 0, 0, 0);
        entries += worker.entries;
    }
    const struct WslShortRecord end = { .kind = WslKindEnd, .reserved = {}, .error = 0, .value = entries };
    Emit(&workers[0], &end, sizeof(end));
    Flush(&workers[0]);
    Exit(0);
}

// The kernel supplies argc/argv on the initial stack, without a C runtime startup routine.
#if defined(__x86_64__)
__asm__(".text\n.global _start\n_start:\nmov %rsp,%rdi\nand $-16,%rsp\ncall Run\n");
#else
__asm__(".text\n.global _start\n_start:\nmov x0,sp\nbl Run\n");
#endif
