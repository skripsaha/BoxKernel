#include "eventdriven_demo.h"
#include "../eventdriven_system.h"
#include "../userlib/eventapi.h"
#include "klib.h"

// ============================================================================
// DEMO - Симуляция user space приложения
// ============================================================================

void eventdriven_demo_run(void) {
    kprintf("\n");
    kprintf("============================================================\n");
    kprintf("  EVENT-DRIVEN ARCHITECTURE DEMONSTRATION\n");
    kprintf("============================================================\n");
    kprintf("\n");

    // 1. Получаем доступ к ring buffers
    EventRingBuffer* to_kernel = eventdriven_get_user_to_kernel_ring();
    ResponseRingBuffer* from_kernel = eventdriven_get_kernel_to_user_ring();

    // 2. Инициализируем user API
    eventapi_init(to_kernel, from_kernel);

    kprintf("[DEMO] User API initialized\n");
    kprintf("\n");

    // ========================================================================
    // ТЕСТ 1: Memory Allocation
    // ========================================================================

    kprintf("--- TEST 1: Asynchronous Memory Allocation ---\n");
    kprintf("[DEMO] Requesting memory allocation (4096 bytes)...\n");

    uint64_t event_id_1 = eventapi_memory_alloc(4096);
    kprintf("[DEMO] Event submitted (async, no blocking!)\n");

    // Симулируем другую работу (не блокируемся!)
    kprintf("[DEMO] Doing other work while kernel processes request...\n");
    for (int i = 0; i < 5; i++) {
        kprintf("[DEMO] Working... (%d/5)\n", i + 1);

        // Обрабатываем события в фоне (в реальной системе это работает на других ядрах)
        eventdriven_process_events(50);  // 50 итераций pipeline

        delay(1000);  // Симуляция работы
    }

    kprintf("[DEMO] Checking for response...\n");
    Response* resp1 = eventapi_poll_response(event_id_1);

    if (resp1) {
        if (resp1->status == EVENT_STATUS_SUCCESS) {
            kprintf("[DEMO] SUCCESS! Memory allocated\n");
            kprintf("[DEMO] Result: %p\n", *(void**)resp1->result);
        } else {
            kprintf("[DEMO] FAILED: status=%d\n", resp1->status);
        }
    } else {
        kprintf("[DEMO] Response not ready yet (would continue polling in real app)\n");
    }

    kprintf("\n");

    // ========================================================================
    // ТЕСТ 2: File Open
    // ========================================================================

    kprintf("--- TEST 2: Asynchronous File Open ---\n");
    kprintf("[DEMO] Opening file '/test.txt'...\n");

    uint64_t event_id_2 = eventapi_file_open("/test.txt");
    kprintf("[DEMO] Event submitted (async!)\n");

    kprintf("[DEMO] Doing other work...\n");
    eventdriven_process_events(100);  // Обрабатываем события
    delay(2000);

    kprintf("[DEMO] Checking for response...\n");
    Response* resp2 = eventapi_poll_response(event_id_2);

    if (resp2) {
        if (resp2->status == EVENT_STATUS_SUCCESS) {
            kprintf("[DEMO] SUCCESS! File opened\n");
        } else {
            kprintf("[DEMO] FAILED\n");
        }
    } else {
        kprintf("[DEMO] Response not ready yet\n");
    }

    kprintf("\n");

    // ========================================================================
    // ТЕСТ 3: Batch Events
    // ========================================================================

    kprintf("--- TEST 3: Batch Event Submission ---\n");
    kprintf("[DEMO] Submitting 10 events in rapid succession...\n");

    for (int i = 0; i < 10; i++) {
        uint64_t size = 1024 * (i + 1);
        eventapi_memory_alloc(size);
        kprintf("[DEMO] Event %d submitted (size=%lu)\n", i, size);
    }

    kprintf("[DEMO] All 10 events submitted WITHOUT BLOCKING!\n");
    kprintf("[DEMO] Kernel will process them in parallel...\n");

    // Обрабатываем все события
    eventdriven_process_events(200);

    kprintf("\n");

    // ========================================================================
    // СТАТИСТИКА
    // ========================================================================

    kprintf("--- SYSTEM STATISTICS ---\n");
    eventdriven_print_full_stats();

    kprintf("\n");
    kprintf("============================================================\n");
    kprintf("  DEMONSTRATION COMPLETE\n");
    kprintf("============================================================\n");
    kprintf("\n");

    kprintf("KEY TAKEAWAYS:\n");
    kprintf("1. Events submitted ASYNCHRONOUSLY - no blocking!\n");
    kprintf("2. User can continue working while kernel processes\n");
    kprintf("3. Multiple events can be in flight simultaneously\n");
    kprintf("4. Results retrieved via polling (non-blocking)\n");
    kprintf("5. NO SYSCALLS - only lock-free ring buffers!\n");
    kprintf("\n");
}
