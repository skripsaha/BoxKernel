#include "deck_interface.h"
#include "klib.h"

// ============================================================================
// OPERATIONS DECK - Process & IPC Operations
// ============================================================================

// Process control block (заглушка - в реальности это полная структура)
typedef struct {
    uint64_t pid;
    uint64_t parent_pid;
    char name[64];
    uint64_t state;  // RUNNING, SLEEPING, ZOMBIE, etc.
    void* page_table;
    uint64_t rsp;    // Stack pointer
    uint64_t rip;    // Instruction pointer
} ProcessControlBlock;

// Глобальный счетчик PID
static volatile uint64_t next_pid = 1;

// ============================================================================
// PROCESS OPERATIONS
// ============================================================================

static ProcessControlBlock* process_create(const char* name, void* entry_point) {
    // TODO: Интеграция с реальным планировщиком и VMM

    ProcessControlBlock* pcb = (ProcessControlBlock*)kmalloc(sizeof(ProcessControlBlock));
    if (!pcb) {
        kprintf("[OPERATIONS] Failed to allocate PCB\n");
        return 0;
    }

    pcb->pid = atomic_increment_u64(&next_pid);
    pcb->parent_pid = 0;  // TODO: получить текущий PID

    // Копируем имя
    int i = 0;
    while (name[i] && i < 63) {
        pcb->name[i] = name[i];
        i++;
    }
    pcb->name[i] = 0;

    pcb->state = 1;  // RUNNING
    pcb->page_table = 0;  // TODO: создать page table
    pcb->rsp = 0;  // TODO: выделить stack
    pcb->rip = (uint64_t)entry_point;

    kprintf("[OPERATIONS] Created process '%s' with PID=%lu\n", name, pcb->pid);

    return pcb;
}

static int process_kill(uint64_t pid) {
    // TODO: Найти процесс в таблице и убить
    kprintf("[OPERATIONS] Killed process PID=%lu\n", pid);
    return 0;  // Success
}

static int process_wait(uint64_t pid) {
    // TODO: Ждать завершения процесса
    kprintf("[OPERATIONS] Waiting for process PID=%lu\n", pid);
    return 0;
}

static int process_getpid(void) {
    // TODO: Вернуть текущий PID
    return 1;  // Placeholder
}

// ============================================================================
// IPC OPERATIONS (STUBS - для v1)
// ============================================================================

static int ipc_send(uint64_t target_pid, void* data, uint64_t size) {
    kprintf("[OPERATIONS] IPC send to PID=%lu, size=%lu bytes - STUB\n", target_pid, size);
    return 0;
}

static int ipc_recv(uint64_t* from_pid, void* buffer, uint64_t max_size) {
    kprintf("[OPERATIONS] IPC recv - STUB\n");
    return 0;
}

// ============================================================================
// PROCESSING FUNCTION
// ============================================================================

int operations_deck_process(RoutingEntry* entry) {
    Event* event = &entry->event_copy;

    switch (event->type) {
        // === PROCESS OPERATIONS ===
        case EVENT_PROC_CREATE: {
            // Payload: [name_len:4][name:...][entry_point:8]
            uint32_t name_len = *(uint32_t*)event->data;
            const char* name = (const char*)(event->data + 4);
            void* entry_point = *(void**)(event->data + 4 + name_len);

            ProcessControlBlock* pcb = process_create(name, entry_point);

            if (pcb) {
                kprintf("[OPERATIONS] Event %lu: created process PID=%lu\n",
                        event->id, pcb->pid);
                deck_complete(entry, DECK_PREFIX_OPERATIONS, pcb);
                return 1;
            } else {
                kprintf("[OPERATIONS] Event %lu: failed to create process\n",
                        event->id);
                deck_error(entry, DECK_PREFIX_OPERATIONS, 1);
                return 0;
            }
        }

        case EVENT_PROC_EXIT: {
            uint64_t exit_code = *(uint64_t*)event->data;
            kprintf("[OPERATIONS] Event %lu: process exit with code %lu\n",
                    event->id, exit_code);
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            return 1;
        }

        case EVENT_PROC_KILL: {
            uint64_t pid = *(uint64_t*)event->data;
            process_kill(pid);
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            kprintf("[OPERATIONS] Event %lu: killed PID=%lu\n", event->id, pid);
            return 1;
        }

        case EVENT_PROC_WAIT: {
            uint64_t pid = *(uint64_t*)event->data;
            process_wait(pid);
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            kprintf("[OPERATIONS] Event %lu: waited for PID=%lu\n", event->id, pid);
            return 1;
        }

        case EVENT_PROC_GETPID: {
            int pid = process_getpid();
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            kprintf("[OPERATIONS] Event %lu: getpid() = %d\n", event->id, pid);
            return 1;
        }

        case EVENT_PROC_SIGNAL: {
            // Payload: [pid:8][signal:4]
            uint64_t pid = *(uint64_t*)event->data;
            uint32_t signal = *(uint32_t*)(event->data + 8);
            kprintf("[OPERATIONS] Event %lu: signal %u to PID=%lu\n",
                    event->id, signal, pid);
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            return 1;
        }

        // === IPC OPERATIONS (STUBS) ===
        case EVENT_IPC_SEND: {
            // Payload: [target_pid:8][size:8][data:...]
            uint64_t target_pid = *(uint64_t*)event->data;
            uint64_t size = *(uint64_t*)(event->data + 8);
            void* data = event->data + 16;

            ipc_send(target_pid, data, size);
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            kprintf("[OPERATIONS] Event %lu: IPC send to PID=%lu\n",
                    event->id, target_pid);
            return 1;
        }

        case EVENT_IPC_RECV: {
            kprintf("[OPERATIONS] Event %lu: IPC recv - STUB\n", event->id);
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            return 1;
        }

        case EVENT_IPC_SHM_CREATE:
        case EVENT_IPC_SHM_ATTACH:
        case EVENT_IPC_PIPE_CREATE: {
            kprintf("[OPERATIONS] Event %lu: IPC operation (type=%d) - STUB\n",
                    event->id, event->type);
            deck_complete(entry, DECK_PREFIX_OPERATIONS, 0);
            return 1;
        }

        default:
            kprintf("[OPERATIONS] Unknown event type %d\n", event->type);
            deck_error(entry, DECK_PREFIX_OPERATIONS, 2);
            return 0;
    }
}

// ============================================================================
// INITIALIZATION & RUN
// ============================================================================

DeckContext operations_deck_context;

void operations_deck_init(void) {
    deck_init(&operations_deck_context, "Operations", DECK_PREFIX_OPERATIONS, operations_deck_process);
}

int operations_deck_run_once(void) {
    return deck_run_once(&operations_deck_context);
}

void operations_deck_run(void) {
    deck_run(&operations_deck_context);
}
