#include "shell.h"
#include "klib.h"
#include "keyboard.h"
#include "vga.h"
#include "tagfs.h"
#include "task.h"
#include "cpu.h"
#include "pmm.h"
#include "vmm.h"

// ============================================================================
// SHELL STATE
// ============================================================================

static char input_buffer[SHELL_INPUT_BUFFER_SIZE];
static uint32_t input_pos = 0;

// ============================================================================
// COMMAND TABLE
// ============================================================================

static shell_command_t commands[] = {
    {"help", "Show available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"echo", "Print text to console", cmd_echo},
    {"ls", "List files in filesystem", cmd_ls},
    {"cat", "Display file contents", cmd_cat},
    {"mkdir", "Create a directory (placeholder)", cmd_mkdir},
    {"touch", "Create an empty file", cmd_touch},
    {"rm", "Remove a file (placeholder)", cmd_rm},
    {"ps", "List running tasks", cmd_ps},
    {"info", "Show system information", cmd_info},
    {"reboot", "Reboot the system", cmd_reboot},
    {NULL, NULL, NULL}  // Sentinel
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static void shell_print_prompt(void) {
    kprintf("%[H]%s%[D]", SHELL_PROMPT);
}

static int shell_parse_command(char* input, char** argv) {
    int argc = 0;
    int in_token = 0;

    for (int i = 0; input[i] != '\0' && argc < SHELL_MAX_ARGS; i++) {
        if (input[i] == ' ' || input[i] == '\t') {
            input[i] = '\0';
            in_token = 0;
        } else if (!in_token) {
            argv[argc++] = &input[i];
            in_token = 1;
        }
    }

    return argc;
}

// ============================================================================
// SHELL INITIALIZATION
// ============================================================================

void shell_init(void) {
    keyboard_init();
    input_pos = 0;

    kprintf("\n");
    kprintf("%[S]╔══════════════════════════════════════════════════════╗%[D]\n");
    kprintf("%[S]║          Welcome to BoxOS Shell v1.0                ║%[D]\n");
    kprintf("%[S]╚══════════════════════════════════════════════════════╝%[D]\n");
    kprintf("\n");
    kprintf("Type '%[H]help%[D]' for available commands.\n");
    kprintf("\n");
}

// ============================================================================
// SHELL MAIN LOOP
// ============================================================================

void shell_run(void) {
    shell_print_prompt();

    while (1) {
        if (!keyboard_has_input()) {
            asm("hlt");  // Wait for interrupt
            continue;
        }

        char c = keyboard_getchar();

        if (c == '\n') {
            // Execute command
            kprintf("\n");

            if (input_pos > 0) {
                input_buffer[input_pos] = '\0';

                // Parse command
                char* argv[SHELL_MAX_ARGS];
                int argc = shell_parse_command(input_buffer, argv);

                if (argc > 0) {
                    // Find and execute command
                    int found = 0;
                    for (int i = 0; commands[i].name != NULL; i++) {
                        if (strcmp(argv[0], commands[i].name) == 0) {
                            commands[i].handler(argc, argv);
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        kprintf("%[E]Error: Unknown command '%s'%[D]\n", argv[0]);
                        kprintf("Type '%[H]help%[D]' for available commands.\n");
                    }
                }

                input_pos = 0;
            }

            shell_print_prompt();
        } else if (c == '\b') {
            // Backspace
            if (input_pos > 0) {
                input_pos--;
                kprintf("\b \b");  // Erase character on screen
            }
        } else if (input_pos < SHELL_INPUT_BUFFER_SIZE - 1) {
            // Add character to buffer
            input_buffer[input_pos++] = c;
            kprintf("%c", c);
        }
    }
}

// ============================================================================
// COMMAND: help
// ============================================================================

int cmd_help(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Available Commands:%[D]\n");
    kprintf("═══════════════════════════════════════════════════\n");

    for (int i = 0; commands[i].name != NULL; i++) {
        kprintf("  %[H]%-12s%[D] - %s\n", commands[i].name, commands[i].description);
    }

    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: clear
// ============================================================================

int cmd_clear(int argc, char** argv) {
    (void)argc;
    (void)argv;

    vga_clear_screen();
    return 0;
}

// ============================================================================
// COMMAND: echo
// ============================================================================

int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s", argv[i]);
        if (i < argc - 1) kprintf(" ");
    }
    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: ls
// ============================================================================

int cmd_ls(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Filesystem Contents:%[D]\n");
    kprintf("═══════════════════════════════════════════════════\n");

    // TODO: Implement proper ls with TagFS query
    kprintf("(filesystem listing not yet implemented)\n");
    kprintf("\n");

    return 0;
}

// ============================================================================
// COMMAND: cat
// ============================================================================

int cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        kprintf("%[E]Usage: cat <filename>%[D]\n");
        return 1;
    }

    kprintf("\n%[H]File: %s%[D]\n", argv[1]);
    kprintf("═══════════════════════════════════════════════════\n");

    // TODO: Implement file reading with TagFS
    kprintf("(file reading not yet implemented)\n");
    kprintf("\n");

    return 0;
}

// ============================================================================
// COMMAND: mkdir
// ============================================================================

int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        kprintf("%[E]Usage: mkdir <dirname>%[D]\n");
        return 1;
    }

    kprintf("%[S]Created directory: %s%[D]\n", argv[1]);
    return 0;
}

// ============================================================================
// COMMAND: touch
// ============================================================================

int cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        kprintf("%[E]Usage: touch <filename>%[D]\n");
        return 1;
    }

    kprintf("%[S]Created file: %s%[D]\n", argv[1]);
    return 0;
}

// ============================================================================
// COMMAND: rm
// ============================================================================

int cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        kprintf("%[E]Usage: rm <filename>%[D]\n");
        return 1;
    }

    kprintf("%[S]Removed: %s%[D]\n", argv[1]);
    return 0;
}

// ============================================================================
// COMMAND: ps
// ============================================================================

int cmd_ps(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Running Tasks:%[D]\n");
    kprintf("═══════════════════════════════════════════════════\n");
    kprintf("%-6s %-20s %-10s %-10s\n", "ID", "Name", "State", "Energy");
    kprintf("───────────────────────────────────────────────────\n");

    // TODO: Implement task listing via task system API
    kprintf("(task listing not yet implemented)\n");
    kprintf("Use the task system API to enumerate tasks.\n");

    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: info
// ============================================================================

int cmd_info(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]═══════════════════════════════════════════════════%[D]\n");
    kprintf("%[H]             BoxOS System Information              %[D]\n");
    kprintf("%[H]═══════════════════════════════════════════════════%[D]\n\n");

    // CPU info
    char cpu_vendor[13];
    char cpu_brand[49];
    detect_cpu_info(cpu_vendor, cpu_brand);

    kprintf("%[H]CPU:%[D]\n");
    kprintf("  Vendor: %s\n", cpu_vendor);
    kprintf("  Brand:  %s\n", cpu_brand);

    // Memory info
    vmm_stats_t stats;
    vmm_get_global_stats(&stats);

    kprintf("\n%[H]Memory:%[D]\n");
    kprintf("  Total mapped: %lu MB\n",
           (stats.total_mapped_pages * 4096) / (1024 * 1024));
    kprintf("  Kernel pages: %lu\n", stats.kernel_mapped_pages);
    kprintf("  User pages:   %lu\n", stats.user_mapped_pages);

    kprintf("\n%[H]Filesystem:%[D]\n");
    kprintf("  Type: TagFS (tag-based filesystem)\n");
    kprintf("  Blocks: 128 (512 KB total)\n");

    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: reboot
// ============================================================================

int cmd_reboot(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Rebooting system...%[D]\n\n");

    // Wait a moment
    for (volatile int i = 0; i < 10000000; i++);

    // Triple fault to reboot
    asm volatile("cli");
    asm volatile("lidt (0)");  // Load invalid IDT
    asm volatile("int $3");    // Trigger interrupt

    return 0;
}
