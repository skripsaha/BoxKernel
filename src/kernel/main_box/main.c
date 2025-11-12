#include "vga.h"
#include "klib.h"
#include "fpu.h"
#include "cpu.h"
#include "e820.h"
#include "vmm.h"
#include "pmm.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "ata.h"
#include "tagfs.h"
#include "task.h"
#include "pit.h"
#include "eventdriven_system.h"
#include "eventdriven_demo.h"
#include "serial.h"

extern uint8_t user_experience_level;

static uint32_t rand_seed = 912346;

// TEMPORARY FIX: Hardcoded E820 map for testing (const to place in .rodata, not BSS)
// TODO: Fix E820 map loading from bootloader
const e820_entry_t temp_e820[] = {
    {.base = 0x0, .length = 0x9FC00, .type = E820_USABLE, .acpi = 0},         // 0-639KB usable
    {.base = 0x100000, .length = 0x7F00000, .type = E820_USABLE, .acpi = 0},  // 1MB-128MB usable
};

uint32_t simple_rand(){
    // Fixed: use proper 32-bit LCG parameters (from Numerical Recipes)
    rand_seed = (1103515245U * rand_seed + 12345U) & 0x7FFFFFFF;
    return rand_seed;
}

void create_utid(int count) {
    uint32_t utids[count];
    for (int i = 0; i < count; i++){
        utids[i] = simple_rand() & 0xFFFFFF;
        kprintf("UTID %d: 0x%x\n", i, utids[i]);
        kprintf("Real address: %x\n" ,&utids[i]);
    }
}

void asm_stop(){
    asm("cli");
    asm("hlt");
}

// void touchors_init() {
//     kprintf("Reserving Memory Regions for touchors...");
//     uintptr_t touchors_zone = vmm_reserve_region(NULL, 0x01000000, 100, 1); // reverse_range(0x01000000, 0x04FFFFFF);
//     kprintf("Zone start: %x. Zone end: %x.", vmm_reserve_region(NULL, 0x01000000, 100, 1), touchors_zone + 100);
// }


void kernel_main(void) {
    // Initialize serial port FIRST for early debugging
    serial_init();
    serial_print("BoxOS: Serial port initialized\n");

    vga_init();
    kprintf("%[S]BoxOS Starting...%[D]\n");


    kprintf("%[H]Initializing core systems...%[D]\n\n");

    enable_fpu();
    kprintf("%[S] FPU enabled%[D]\n");

    mem_init();
    kprintf("%[S] Memory allocator initialized%[D]\n");

    // Use hardcoded E820 map (const to avoid BSS zeroing)
    e820_set_entries((e820_entry_t*)temp_e820, 2);
    kprintf("%[S] E820 map initialized (hardcoded for testing)%[D]\n");

    pmm_init();
    kprintf("%[S] Physical memory manager initialized%[D]\n");

    vmm_init();
    vmm_test_basic();
    kprintf("%[S] Virtual memory manager initialized%[D]\n");

    // === STORAGE SYSTEM INITIALIZATION ===
    kprintf("\n%[H]=== Initializing Storage System ===%[D]\n");
    ata_init();
    kprintf("%[S] ATA disk driver initialized%[D]\n");

    tagfs_init();
    kprintf("%[S] TagFS filesystem initialized%[D]\n");

    // === TASK SYSTEM INITIALIZATION ===
    kprintf("\n%[H]=== Initializing Task System ===%[D]\n");
    task_system_init();
    kprintf("%[S] Task system initialized%[D]\n");

    // === ТЕСТ GDT ===
    kprintf("\n%[H]=== Step 1: GDT Setup ===%[D]\n");
    gdt_init();
    gdt_test();
    
    // === ТЕСТ IDT ===
    kprintf("\n%[H]=== Step 2: IDT Setup ===%[D]\n");
    idt_init();
    idt_test();

    // === ТЕСТ TSS ===
    kprintf("\n%[H]=== Step 3: TSS Setup ===%[D]\n");
    tss_init();
    tss_test();

    // === ТЕСТ PIC ===
    kprintf("\n%[H]=== Step 4: PIC Setup === %[D]");
    pic_init();
    pic_test();

    // === PIT (TIMER) SETUP ===
    kprintf("\n%[H]=== Step 5: PIT Timer Setup ===%[D]\n");
    pit_init(100);  // 100 Hz = 10ms per tick
    kprintf("%[S] PIT timer initialized (100 Hz)%[D]\n");

    kprintf("\n%[S] All core systems initialized!%[D]\n");

    // === EVENT-DRIVEN SYSTEM INITIALIZATION ===
    kprintf("\n%[H]=== Step 6: Event-Driven System === %[D]\n");
    eventdriven_system_init();
    eventdriven_system_start();
    kprintf("%[S] Event-driven system initialized!%[D]\n");
    kprintf("%[H]System ready. Enabling interrupts for testing...%[D]\n");
    
    // Включаем прерывания для тестирования
    // kprintf("%[W]Enabling interrupts in 3 seconds...%[D]\n");
    // for(int i = 3; i > 0; i--) {
    //     kprintf("%[W]%d...%[D]\n", i);
    //     delay(10000);  // 1 секунда задержка
    // }
    
    // // touchors_init();
    // create_utid(20);

    vga_clear_screen();

    kprintf("%[H]Interrupts enabled! You should see timer/keyboard events.%[D]\n");
    kprintf("%[S]Hello my friend!%[D]\n");
    asm volatile("sti");  // Включаем прерывания

    if(user_experience_level == 0){
        kprintf("\nHello there! You are newbie user I see.\nYou can use Box right now! Enjoy :)\n");
        kprintf("\n%[S]Mode: Newbie%[D]\n");
    }
    else if(user_experience_level == 1) {
        kprintf("\n%[S]Mode: Programmer%[D]\n");
    }
    else {
        kprintf("\n%[S]Mode: Gamer%[D]\n");
    }
    kprintf("In: %i\n", user_experience_level);

    cpu_print_detailed_info();

    // === EVENT-DRIVEN DEMONSTRATION ===
    kprintf("\n%[H]=== Running Event-Driven System Demo === %[D]\n");
    eventdriven_demo_run();
    kprintf("%[S] Demo completed!%[D]\n");

    // Главный цикл с HLT
    kprintf("\n%[H]Entering idle loop...%[D]\n");
    while (1) {
        asm volatile("hlt");  // Ждем прерывания
    }

}
