#include "platform.h"

#include "kernel/arch/x86_64/acpi.h"
#include "kernel/arch/x86_64/apic/lapic.h"
#include "kernel/arch/x86_64/apic/ioapic.h"
#include "kernel/arch/x86_64/apic/gsi.h"

int platform_init(void)
{
    int result = acpi_init();

    if (result != 0)
        return result;

    result = lapic_init();

    if (result != 0)
        return result;

    result = ioapic_init();

    if (result != 0)
        return result;

    result = gsi_init();

    if (result != 0)
        return result;

    return 0;
}
