#include <stdint.h>
#include <stddef.h>

#include "limine.h"
#include "kernel/boot/boot.h"
#include "kernel/console/console.h"

/* ============================================================
   LIMINE FRAMEBUFFER REQUEST
   ============================================================ */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request
    limine_framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

static struct limine_framebuffer *kernel_framebuffer = NULL;

/* ============================================================
   LIMINE EXECUTABLE ADDRESS REQUEST
   ============================================================ */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request
    limine_executable_address_request = {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
        .revision = 0,
        .response = NULL
    };

/* ============================================================
   KERNEL IMAGE METADATA
   ============================================================ */

extern char __kernel_end[];

extern char __kernel_text_start[];
extern char __kernel_text_end[];

extern char __kernel_rodata_start[];
extern char __kernel_rodata_end[];

extern char __kernel_data_start[];
extern char __kernel_data_end[];

extern char __kernel_bss_start[];
extern char __kernel_bss_end[];

static uint64_t kernel_physical_base = 0;
static uint64_t kernel_virtual_base = 0;
static uint64_t kernel_image_size = 0;

uint64_t boot_get_kernel_physical_base(void)
{
    return kernel_physical_base;
}

uint64_t boot_get_kernel_virtual_base(void)
{
    return kernel_virtual_base;
}

uint64_t boot_get_kernel_image_size(void)
{
    return kernel_image_size;
}

uint64_t boot_get_kernel_text_start(void)
{
    return (uint64_t)(uintptr_t)__kernel_text_start;
}

uint64_t boot_get_kernel_text_end(void)
{
    return (uint64_t)(uintptr_t)__kernel_text_end;
}

uint64_t boot_get_kernel_rodata_start(void)
{
    return (uint64_t)(uintptr_t)__kernel_rodata_start;
}

uint64_t boot_get_kernel_rodata_end(void)
{
    return (uint64_t)(uintptr_t)__kernel_rodata_end;
}

uint64_t boot_get_kernel_data_start(void)
{
    return (uint64_t)(uintptr_t)__kernel_data_start;
}

uint64_t boot_get_kernel_data_end(void)
{
    return (uint64_t)(uintptr_t)__kernel_data_end;
}

uint64_t boot_get_kernel_bss_start(void)
{
    return (uint64_t)(uintptr_t)__kernel_bss_start;
}

uint64_t boot_get_kernel_bss_end(void)
{
    return (uint64_t)(uintptr_t)__kernel_bss_end;
}

/* ============================================================
   BOOT INITIALIZATION
   ============================================================ */

void boot_init(void)
{
    serial_init();

    serial_write_string(
        "BATOS KERNEL STARTING...\n"
    );

    /* --------------------------------------------------------
       LIMINE EXECUTABLE ADDRESS VERIFICATION
       -------------------------------------------------------- */

    serial_write_string(
        "\nEXECUTABLE ADDRESS VERIFICATION\n"
    );

    if (limine_executable_address_request.response)
    {
        uint64_t executable_physical =
            limine_executable_address_request.response->physical_base;

        uint64_t executable_virtual =
            limine_executable_address_request.response->virtual_base;

        kernel_physical_base =
            executable_physical;

        kernel_virtual_base =
            executable_virtual;

        if ((uint64_t)(uintptr_t)__kernel_end <
            kernel_virtual_base)
        {
            kernel_physical_base = 0;
            kernel_virtual_base = 0;
            kernel_image_size = 0;
        }
        else
        {
            kernel_image_size =
                (uint64_t)(uintptr_t)__kernel_end -
                kernel_virtual_base;
        }

        serial_write_string(
            "EXECUTABLE PHYSICAL BASE: "
        );

        serial_write_hex(
            executable_physical
        );

        serial_write_string("\n");

        serial_write_string(
            "EXECUTABLE VIRTUAL BASE: "
        );

        serial_write_hex(
            executable_virtual
        );

        serial_write_string("\n");

        serial_write_string(
            "EXECUTABLE ADDRESS: OK\n"
        );
    }
    else
    {
        serial_write_string(
            "EXECUTABLE ADDRESS: FAILED\n"
        );
    }

    /* --------------------------------------------------------
       FRAMEBUFFER
       -------------------------------------------------------- */

    if (limine_framebuffer_request.response &&
        limine_framebuffer_request.response->framebuffer_count > 0)
    {
        kernel_framebuffer =
            limine_framebuffer_request.response->framebuffers[0];

        console_set_framebuffer(kernel_framebuffer);

        framebuffer_clear(0x00000000);

        draw_text(
            40,
            40,
            "BATOS OS",
            0x00FFFFFF,
            5
        );

        draw_text(
            40,
            110,
            "KERNEL RUNNING",
            0x00FFFFFF,
            3
        );

        framebuffer_console_init();

        serial_write_string(
            "FRAMEBUFFER OK\n"
        );
    }
    else
    {
        serial_write_string(
            "FRAMEBUFFER ERROR\n"
        );
    }
}
