#include "deck_interface.h"
#include "pmm.h"  // Physical memory manager
#include "vmm.h"  // Virtual memory manager
#include "klib.h"
#include "../storage/tagfs.h"  // TagFS - Tag-based filesystem

// ============================================================================
// STORAGE DECK - Memory & Filesystem Operations
// ============================================================================

// Результат аллокации памяти
typedef struct {
    void* address;
    uint64_t size;
} MemoryAllocResult;

// Простая структура file descriptor (заглушка)
typedef struct {
    int fd;
    char path[256];
    uint64_t size;
    uint64_t position;
} FileDescriptor;

// Глобальный счетчик FD
static volatile uint64_t next_fd = 100;

// ============================================================================
// MEMORY OPERATIONS
// ============================================================================

static void* memory_alloc(uint64_t size) {
    // Вычисляем количество страниц (4KB каждая)
    size_t page_count = (size + 4095) / 4096;

    // Аллоцируем память через VMM (используем kernel context)
    void* addr = vmm_alloc_pages(vmm_get_kernel_context(), page_count,
                                 VMM_FLAGS_KERNEL_RW);

    if (addr) {
        kprintf("[STORAGE] Allocated %lu bytes (%lu pages) at %p\n",
                size, page_count, addr);
    } else {
        kprintf("[STORAGE] Failed to allocate %lu bytes\n", size);
    }

    return addr;
}

static void memory_free(void* addr, uint64_t size) {
    size_t page_count = (size + 4095) / 4096;
    vmm_free_pages(vmm_get_kernel_context(), addr, page_count);
    kprintf("[STORAGE] Freed memory at %p (%lu pages)\n", addr, page_count);
}

// ============================================================================
// FILESYSTEM OPERATIONS (STUBS - для демонстрации v1)
// ============================================================================

static FileDescriptor* fs_open(const char* path) {
    // TODO: реальное открытие файла через VFS

    FileDescriptor* fd_info = (FileDescriptor*)kmalloc(sizeof(FileDescriptor));
    fd_info->fd = atomic_increment_u64(&next_fd);

    // Копируем путь
    int i = 0;
    while (path[i] && i < 255) {
        fd_info->path[i] = path[i];
        i++;
    }
    fd_info->path[i] = 0;

    fd_info->size = 0;  // TODO: получить реальный размер
    fd_info->position = 0;

    kprintf("[STORAGE] Opened file '%s' with fd=%d\n", path, fd_info->fd);
    return fd_info;
}

static int fs_close(int fd) {
    // TODO: реальное закрытие файла
    kprintf("[STORAGE] Closed fd=%d\n", fd);
    return 0;  // Success
}

static int fs_read(int fd, void* buffer, uint64_t size) {
    // TODO: реальное чтение
    kprintf("[STORAGE] Read %lu bytes from fd=%d\n", size, fd);
    return size;  // Возвращаем количество прочитанных байт
}

static int fs_write(int fd, const void* buffer, uint64_t size) {
    // TODO: реальная запись
    kprintf("[STORAGE] Wrote %lu bytes to fd=%d\n", size, fd);
    return size;
}

// ============================================================================
// PROCESSING FUNCTION
// ============================================================================

int storage_deck_process(RoutingEntry* entry) {
    Event* event = &entry->event_copy;

    switch (event->type) {
        // === MEMORY OPERATIONS ===
        case EVENT_MEMORY_ALLOC: {
            uint64_t size = *(uint64_t*)event->data;
            void* addr = memory_alloc(size);

            if (addr) {
                deck_complete(entry, DECK_PREFIX_STORAGE, addr);
                kprintf("[STORAGE] Event %lu: allocated %lu bytes\n",
                        event->id, size);
                return 1;
            } else {
                deck_error(entry, DECK_PREFIX_STORAGE, 1);
                kprintf("[STORAGE] Event %lu: allocation failed\n", event->id);
                return 0;
            }
        }

        case EVENT_MEMORY_FREE: {
            void* addr = *(void**)event->data;
            uint64_t size = *(uint64_t*)(event->data + 8);
            memory_free(addr, size);
            deck_complete(entry, DECK_PREFIX_STORAGE, 0);
            kprintf("[STORAGE] Event %lu: freed memory at %p\n", event->id, addr);
            return 1;
        }

        case EVENT_MEMORY_MAP: {
            // TODO: Memory mapping
            kprintf("[STORAGE] Event %lu: memory map - STUB\n", event->id);
            deck_complete(entry, DECK_PREFIX_STORAGE, 0);
            return 1;
        }

        // === FILESYSTEM OPERATIONS ===
        case EVENT_FILE_OPEN: {
            const char* path = (const char*)event->data;
            FileDescriptor* fd_info = fs_open(path);

            if (fd_info) {
                deck_complete(entry, DECK_PREFIX_STORAGE, fd_info);
                kprintf("[STORAGE] Event %lu: opened '%s' (fd=%d)\n",
                        event->id, path, fd_info->fd);
                return 1;
            } else {
                deck_error(entry, DECK_PREFIX_STORAGE, 2);
                kprintf("[STORAGE] Event %lu: failed to open '%s'\n",
                        event->id, path);
                return 0;
            }
        }

        case EVENT_FILE_CLOSE: {
            int fd = *(int*)event->data;
            fs_close(fd);
            deck_complete(entry, DECK_PREFIX_STORAGE, 0);
            kprintf("[STORAGE] Event %lu: closed fd=%d\n", event->id, fd);
            return 1;
        }

        case EVENT_FILE_READ: {
            // Payload: [fd:4 bytes][size:8 bytes]
            int fd = *(int*)event->data;
            uint64_t size = *(uint64_t*)(event->data + 4);

            // TODO: выделить буфер и прочитать данные
            deck_complete(entry, DECK_PREFIX_STORAGE, 0);
            kprintf("[STORAGE] Event %lu: read %lu bytes from fd=%d\n",
                    event->id, size, fd);
            return 1;
        }

        case EVENT_FILE_WRITE: {
            // Payload: [fd:4 bytes][size:8 bytes][data:...]
            int fd = *(int*)event->data;
            uint64_t size = *(uint64_t*)(event->data + 4);
            void* data = event->data + 12;

            fs_write(fd, data, size);
            deck_complete(entry, DECK_PREFIX_STORAGE, 0);
            kprintf("[STORAGE] Event %lu: wrote %lu bytes to fd=%d\n",
                    event->id, size, fd);
            return 1;
        }

        case EVENT_FILE_STAT: {
            const char* path = (const char*)event->data;
            // TODO: stat implementation
            deck_complete(entry, DECK_PREFIX_STORAGE, 0);
            kprintf("[STORAGE] Event %lu: stat '%s'\n", event->id, path);
            return 1;
        }

        // === TAGFS OPERATIONS ===
        case EVENT_FILE_CREATE_TAGGED: {
            // Payload: [tag_count:4][tags:Tag[]...]
            uint32_t tag_count = *(uint32_t*)event->data;
            Tag* tags = (Tag*)(event->data + 4);

            uint64_t inode_id = tagfs_create_file(tags, tag_count);
            if (inode_id != TAGFS_INVALID_INODE) {
                deck_complete(entry, DECK_PREFIX_STORAGE, (void*)inode_id);
                kprintf("[STORAGE] Event %lu: created file inode=%lu with %u tags\n",
                        event->id, inode_id, tag_count);
                return 1;
            } else {
                deck_error(entry, DECK_PREFIX_STORAGE, 10);
                kprintf("[STORAGE] Event %lu: failed to create tagged file\n", event->id);
                return 0;
            }
        }

        case EVENT_FILE_QUERY: {
            // Payload: [tag_count:4][operator:1][tags:Tag[]...]
            uint32_t tag_count = *(uint32_t*)event->data;
            uint8_t op = *(uint8_t*)(event->data + 4);
            Tag* tags = (Tag*)(event->data + 8);

            // Allocate result arrays
            uint64_t* result_inodes = (uint64_t*)kmalloc(256 * sizeof(uint64_t));
            TagQuery query;
            query.tags = tags;
            query.tag_count = tag_count;
            query.op = (QueryOperator)op;
            query.result_inodes = result_inodes;
            query.result_count = 0;
            query.result_capacity = 256;

            int success = tagfs_query(&query);
            if (success) {
                // Pass results back (will be in Response)
                deck_complete(entry, DECK_PREFIX_STORAGE, result_inodes);
                kprintf("[STORAGE] Event %lu: query found %u files\n",
                        event->id, query.result_count);
                return 1;
            } else {
                kfree(result_inodes);
                deck_error(entry, DECK_PREFIX_STORAGE, 11);
                kprintf("[STORAGE] Event %lu: query failed\n", event->id);
                return 0;
            }
        }

        case EVENT_FILE_TAG_ADD: {
            // Payload: [inode_id:8][tag:Tag]
            uint64_t inode_id = *(uint64_t*)event->data;
            Tag* tag = (Tag*)(event->data + 8);

            int success = tagfs_add_tag(inode_id, tag);
            if (success) {
                deck_complete(entry, DECK_PREFIX_STORAGE, 0);
                kprintf("[STORAGE] Event %lu: added tag %s:%s to inode=%lu\n",
                        event->id, tag->key, tag->value, inode_id);
                return 1;
            } else {
                deck_error(entry, DECK_PREFIX_STORAGE, 12);
                kprintf("[STORAGE] Event %lu: failed to add tag to inode=%lu\n",
                        event->id, inode_id);
                return 0;
            }
        }

        case EVENT_FILE_TAG_REMOVE: {
            // Payload: [inode_id:8][key:32]
            uint64_t inode_id = *(uint64_t*)event->data;
            const char* key = (const char*)(event->data + 8);

            int success = tagfs_remove_tag(inode_id, key);
            if (success) {
                deck_complete(entry, DECK_PREFIX_STORAGE, 0);
                kprintf("[STORAGE] Event %lu: removed tag '%s' from inode=%lu\n",
                        event->id, key, inode_id);
                return 1;
            } else {
                deck_error(entry, DECK_PREFIX_STORAGE, 13);
                kprintf("[STORAGE] Event %lu: failed to remove tag from inode=%lu\n",
                        event->id, inode_id);
                return 0;
            }
        }

        case EVENT_FILE_TAG_GET: {
            // Payload: [inode_id:8]
            uint64_t inode_id = *(uint64_t*)event->data;

            Tag* tags = (Tag*)kmalloc(TAGFS_MAX_TAGS_PER_FILE * sizeof(Tag));
            uint32_t count = 0;

            int success = tagfs_get_tags(inode_id, tags, &count);
            if (success) {
                deck_complete(entry, DECK_PREFIX_STORAGE, tags);
                kprintf("[STORAGE] Event %lu: retrieved %u tags from inode=%lu\n",
                        event->id, count, inode_id);
                return 1;
            } else {
                kfree(tags);
                deck_error(entry, DECK_PREFIX_STORAGE, 14);
                kprintf("[STORAGE] Event %lu: failed to get tags from inode=%lu\n",
                        event->id, inode_id);
                return 0;
            }
        }

        default:
            kprintf("[STORAGE] Unknown event type %d\n", event->type);
            deck_error(entry, DECK_PREFIX_STORAGE, 3);
            return 0;
    }
}

// ============================================================================
// INITIALIZATION & RUN
// ============================================================================

DeckContext storage_deck_context;

void storage_deck_init(void) {
    deck_init(&storage_deck_context, "Storage", DECK_PREFIX_STORAGE, storage_deck_process);

    // Initialize TagFS
    tagfs_init();
    kprintf("[STORAGE] TagFS initialized\n");
}

int storage_deck_run_once(void) {
    return deck_run_once(&storage_deck_context);
}

void storage_deck_run(void) {
    deck_run(&storage_deck_context);
}
