// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.
//
// wds-wsl-scan: enumerates a Linux directory tree inside a WSL distribution and
// streams one binary record per entry to standard output. WinDirStat runs it
// through wsl.exe so the walk uses native Linux system calls instead of the
// Plan 9 file server behind \\wsl.localhost.
//
// Usage: wds-wsl-scan [--self-delete] <root>
//
// Stream layout (little endian):
//   header   "WDSWSL" 0x01 0x00, u32 version, u32 reserved
//   entry    u8 kind=1, u8 type, u8 flags, u8 0, u16 nameLength, u16 0, u32 links,
//            u64 inode, u64 parentInode, u64 size, u64 allocated, i64 mtimeSeconds,
//            u32 mtimeNanoseconds, u32 0, then nameLength bytes of the name
//   failure  u8 kind=2, u8[3] 0, u32 errno, u64 inode          (a folder that could not be listed)
//   end      u8 kind=3, u8[7] 0, u64 entryCount                (absent if the walk was cut short)
// The root is the first entry; it has parent inode 0 and an empty name.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef STATX_ATTR_MOUNT_ROOT
#define STATX_ATTR_MOUNT_ROOT 0x00002000
#endif

enum { KIND_ENTRY = 1, KIND_FAILURE = 2, KIND_END = 3 };
enum { TYPE_FILE = 0, TYPE_DIRECTORY = 1, TYPE_SYMLINK = 2, TYPE_OTHER = 3 };
enum { FLAG_MOUNT = 1 };
enum { STREAM_VERSION = 1, ENTRY_HEADER = 60, OUTPUT_CHUNK = 256 * 1024, LISTING_BUFFER = 64 * 1024 };

struct linux_dirent64
{
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

struct task
{
    char* path;
    uint64_t inode;
};

static struct
{
    pthread_mutex_t lock;
    pthread_cond_t ready;
    struct task* items;
    size_t count, capacity, pending;
    int failed;
} queue = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0, 0, 0, 0 };

static pthread_mutex_t outputLock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t entryCount;
static uint64_t rootDevMajor, rootDevMinor;
static int openFlags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NOATIME;

struct output
{
    unsigned char* data;
    size_t used;
    uint64_t entries;
};

static void Fail(const char* message)
{
    fprintf(stderr, "wds-wsl-scan: %s\n", message);
    exit(2);
}

static void WriteAll(const unsigned char* data, size_t size)
{
    while (size > 0)
    {
        const ssize_t written = write(STDOUT_FILENO, data, size);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) _exit(3); // the reader went away; nothing useful remains to do
        data += written;
        size -= (size_t)written;
    }
}

static void Flush(struct output* out)
{
    pthread_mutex_lock(&outputLock);
    WriteAll(out->data, out->used);
    entryCount += out->entries;
    pthread_mutex_unlock(&outputLock);
    out->used = 0;
    out->entries = 0;
}

static unsigned char* Reserve(struct output* out, const size_t size)
{
    if (out->used + size > OUTPUT_CHUNK) Flush(out);
    unsigned char* place = out->data + out->used;
    out->used += size;
    return place;
}

static void Put16(unsigned char* p, const uint16_t v) { memcpy(p, &v, sizeof v); }
static void Put32(unsigned char* p, const uint32_t v) { memcpy(p, &v, sizeof v); }
static void Put64(unsigned char* p, const uint64_t v) { memcpy(p, &v, sizeof v); }

static void EmitEntry(struct output* out, const struct statx* sx, const uint64_t parent,
    const char* name, const size_t nameLength, const unsigned type, const unsigned flags)
{
    unsigned char* p = Reserve(out, ENTRY_HEADER + nameLength);
    memset(p, 0, ENTRY_HEADER);
    p[0] = KIND_ENTRY;
    p[1] = (unsigned char)type;
    p[2] = (unsigned char)flags;
    Put16(p + 4, (uint16_t)nameLength);
    Put32(p + 8, sx->stx_nlink);
    Put64(p + 12, sx->stx_ino);
    Put64(p + 20, parent);
    Put64(p + 28, type == TYPE_DIRECTORY ? 0 : sx->stx_size);
    Put64(p + 36, sx->stx_blocks * 512ull);
    Put64(p + 44, (uint64_t)sx->stx_mtime.tv_sec);
    Put32(p + 52, sx->stx_mtime.tv_nsec);
    memcpy(p + ENTRY_HEADER, name, nameLength);
    ++out->entries;
}

static void EmitFailure(struct output* out, const uint64_t inode, const int error)
{
    unsigned char* p = Reserve(out, 16);
    memset(p, 0, 16);
    p[0] = KIND_FAILURE;
    Put32(p + 4, (uint32_t)error);
    Put64(p + 8, inode);
}

static void Push(char* path, const uint64_t inode)
{
    pthread_mutex_lock(&queue.lock);
    if (queue.count == queue.capacity)
    {
        const size_t capacity = queue.capacity ? queue.capacity * 2 : 4096;
        struct task* items = realloc(queue.items, capacity * sizeof *items);
        if (items == NULL) Fail("out of memory");
        queue.items = items;
        queue.capacity = capacity;
    }
    queue.items[queue.count++] = (struct task){ path, inode };
    ++queue.pending;
    pthread_cond_signal(&queue.ready);
    pthread_mutex_unlock(&queue.lock);
}

static int Pop(struct task* task)
{
    pthread_mutex_lock(&queue.lock);
    while (queue.count == 0 && queue.pending > 0) pthread_cond_wait(&queue.ready, &queue.lock);
    const int found = queue.count > 0;
    if (found) *task = queue.items[--queue.count]; // depth first keeps the queue short
    pthread_mutex_unlock(&queue.lock);
    return found;
}

static void Finish(void)
{
    pthread_mutex_lock(&queue.lock);
    if (--queue.pending == 0) pthread_cond_broadcast(&queue.ready);
    pthread_mutex_unlock(&queue.lock);
}

// Directories cannot be hard linked, so a directory inode seen twice on the root device is a bind
// mount that the kernel did not report as a mount root (kernels before 5.8 lack that attribute).
static struct
{
    pthread_mutex_t lock;
    uint64_t* slots;
    size_t capacity, count;
} visited = { PTHREAD_MUTEX_INITIALIZER, NULL, 0, 0 };

static int InsertSlot(uint64_t* slots, const size_t capacity, const uint64_t inode)
{
    for (size_t i = (inode * 0x9E3779B97F4A7C15ull) & (capacity - 1);; i = (i + 1) & (capacity - 1))
    {
        if (slots[i] == inode) return 0;
        if (slots[i] == 0) { slots[i] = inode; return 1; }
    }
}

static int FirstVisit(const uint64_t inode)
{
    if (inode == 0) return 1;
    pthread_mutex_lock(&visited.lock);
    if ((visited.count + 1) * 2 > visited.capacity)
    {
        const size_t capacity = visited.capacity ? visited.capacity * 2 : 65536;
        uint64_t* slots = calloc(capacity, sizeof *slots);
        if (slots == NULL) Fail("out of memory");
        for (size_t i = 0; i < visited.capacity; ++i)
            if (visited.slots[i] != 0) InsertSlot(slots, capacity, visited.slots[i]);
        free(visited.slots);
        visited.slots = slots;
        visited.capacity = capacity;
    }
    const int inserted = InsertSlot(visited.slots, visited.capacity, inode);
    visited.count += (size_t)inserted;
    pthread_mutex_unlock(&visited.lock);
    return inserted;
}

static unsigned TypeOf(const mode_t mode)
{
    return S_ISDIR(mode) ? TYPE_DIRECTORY : S_ISLNK(mode) ? TYPE_SYMLINK : S_ISREG(mode) ? TYPE_FILE : TYPE_OTHER;
}

static void ListDirectory(struct output* out, const struct task* task, unsigned char* buffer)
{
    int fd = open(task->path, openFlags);
    if (fd < 0 && errno == EPERM && (openFlags & O_NOATIME))
    {
        openFlags &= ~O_NOATIME; // O_NOATIME needs ownership unless running as root
        fd = open(task->path, openFlags);
    }
    if (fd < 0)
    {
        EmitFailure(out, task->inode, errno);
        return;
    }

    const size_t baseLength = strlen(task->path);
    for (;;)
    {
        const long read = syscall(SYS_getdents64, fd, buffer, LISTING_BUFFER);
        if (read < 0 && errno == EINTR) continue;
        if (read < 0) { EmitFailure(out, task->inode, errno); break; }
        if (read == 0) break;

        for (long offset = 0; offset < read;)
        {
            const struct linux_dirent64* entry = (const struct linux_dirent64*)(buffer + offset);
            offset += entry->d_reclen;
            const char* name = entry->d_name;
            if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;

            struct statx sx;
            if (statx(fd, name, AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT | AT_STATX_DONT_SYNC,
                STATX_TYPE | STATX_MODE | STATX_NLINK | STATX_INO | STATX_SIZE | STATX_BLOCKS | STATX_MTIME,
                &sx) != 0) continue; // removed since it was listed

            const size_t nameLength = strlen(name);
            const unsigned type = TypeOf(sx.stx_mode);
            unsigned flags = 0;
            if (type == TYPE_DIRECTORY &&
                ((sx.stx_attributes_mask & sx.stx_attributes & STATX_ATTR_MOUNT_ROOT) != 0 ||
                sx.stx_dev_major != rootDevMajor || sx.stx_dev_minor != rootDevMinor ||
                !FirstVisit(sx.stx_ino))) flags |= FLAG_MOUNT;
            EmitEntry(out, &sx, task->inode, name, nameLength, type, flags);
            if (type != TYPE_DIRECTORY || (flags & FLAG_MOUNT)) continue;

            char* path = malloc(baseLength + nameLength + 2);
            if (path == NULL) Fail("out of memory");
            memcpy(path, task->path, baseLength);
            size_t length = baseLength;
            if (length == 0 || path[length - 1] != '/') path[length++] = '/';
            memcpy(path + length, name, nameLength + 1);
            Push(path, sx.stx_ino);
        }
    }
    close(fd);
}

static void* Worker(void* unused)
{
    (void)unused;
    struct output out = { malloc(OUTPUT_CHUNK), 0, 0 };
    unsigned char* buffer = malloc(LISTING_BUFFER);
    if (out.data == NULL || buffer == NULL) Fail("out of memory");

    for (struct task task; Pop(&task);)
    {
        ListDirectory(&out, &task, buffer);
        free(task.path);
        Finish();
    }
    Flush(&out);
    free(out.data);
    free(buffer);
    return NULL;
}

static void DeleteSelf(void)
{
    char path[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", path, sizeof path - 1);
    if (length <= 0) return;
    path[length] = '\0';
    unlink(path);
}

int main(int argc, char** argv)
{
    int arg = 1;
    if (arg < argc && strcmp(argv[arg], "--self-delete") == 0) { DeleteSelf(); ++arg; }
    if (arg + 1 != argc) Fail("usage: wds-wsl-scan [--self-delete] <root>");
    signal(SIGPIPE, SIG_DFL);

    const char* root = argv[arg];
    struct statx sx;
    if (statx(AT_FDCWD, root, AT_STATX_DONT_SYNC, STATX_TYPE | STATX_MODE | STATX_NLINK | STATX_INO |
        STATX_SIZE | STATX_BLOCKS | STATX_MTIME, &sx) != 0) Fail(strerror(errno));
    if (!S_ISDIR(sx.stx_mode)) Fail("root is not a directory");
    rootDevMajor = sx.stx_dev_major;
    rootDevMinor = sx.stx_dev_minor;
    FirstVisit(sx.stx_ino);

    unsigned char header[16] = { 'W', 'D', 'S', 'W', 'S', 'L', 1, 0 };
    Put32(header + 8, STREAM_VERSION);
    WriteAll(header, sizeof header);

    struct output out = { malloc(OUTPUT_CHUNK), 0, 0 };
    if (out.data == NULL) Fail("out of memory");
    EmitEntry(&out, &sx, 0, "", 0, TYPE_DIRECTORY, 0);
    Flush(&out);
    char* rootPath = strdup(root);
    if (rootPath == NULL) Fail("out of memory");
    Push(rootPath, sx.stx_ino);

    // Directory listings mostly wait on metadata reads, so oversubscribe the processors.
    long processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (processors < 1) processors = 1;
    const long threads = processors * 2 > 64 ? 64 : processors * 2;
    pthread_t workers[64];
    long started = 0;
    for (; started < threads; ++started)
        if (pthread_create(&workers[started], NULL, Worker, NULL) != 0) break;
    if (started == 0) Worker(NULL);
    for (long i = 0; i < started; ++i) pthread_join(workers[i], NULL);

    unsigned char end[16] = { KIND_END };
    Put64(end + 8, entryCount);
    WriteAll(end, sizeof end);
    return 0;
}
