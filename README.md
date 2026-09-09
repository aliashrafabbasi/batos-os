# BATOS Kernel

BATOS Kernel is an independently developed x86_64 operating system kernel, built from scratch as the architectural foundation for a complete, modern operating system. It is not derived from Linux, BSD, or any other existing kernel — every subsystem is designed and implemented as part of its own architecture.

---

## Vision

BATOS is intended to grow into a complete, modern operating system: fast, reliable, secure, and built on an architecture that is understandable and auditable from the hardware layer upward.

The kernel is the foundation for a full system stack — process and memory management, drivers, storage, networking, security, a userspace runtime, a graphics layer, and application support — all designed around a coherent set of architectural principles rather than assembled from borrowed components.

A distinguishing long-term goal of the project is **AI-native system design**: treating autonomous agents as a first-class, explicitly governed part of the operating system, rather than bolting AI access onto an architecture that wasn't designed for it.

---

## Core Principles

- **From-scratch architecture** — no forked kernel, no inherited subsystem code.
- **Correctness before complexity** — each layer is expected to be correct before the next is built on top of it.
- **Explicit verification** — hardware and software state is checked, not assumed.
- **Security by design** — isolation and least-privilege are architectural properties, not features added later.
- **Hardware-aware engineering** — subsystems are built with a clear understanding of the underlying x86_64 platform, not abstracted away prematurely.
- **Clear kernel/userspace boundaries** — a well-defined syscall interface separates trusted and untrusted code.
- **Modular subsystems** — memory, interrupts, scheduling, drivers, and storage are designed as distinct, replaceable components.
- **Resource efficiency** — the kernel is designed to be lightweight, avoiding unnecessary overhead.
- **Auditable behavior** — system and (eventually) agent actions are designed to be traceable.

---

## System Architecture

```mermaid
flowchart TD
    USER["User"] --> APPS["Applications"]
    APPS --> RUNTIME["Userspace Runtime & Libraries"]
    RUNTIME --> API["System APIs"]
    API --> SYSCALL["Syscall Interface"]
    SYSCALL --> KERNEL

    subgraph KERNEL["BATOS Kernel"]
        direction TB

        subgraph EXEC["Execution"]
            PROC["Process Management"]
            THREAD["Thread Management"]
            SCHED["Scheduler"]
        end

        subgraph MEMSUB["Memory"]
            PMM["Physical Memory Manager"]
            VMM["Virtual Memory Manager"]
            ASPACE["Address Spaces"]
        end

        subgraph COMM["Communication"]
            IPCM["IPC"]
            SIGNAL["Signals / Events"]
        end

        subgraph SECSUB["Security"]
            CAP["Capabilities"]
            SANDBOXK["Sandboxing"]
            AUDIT["Audit / Logging"]
        end

        subgraph IOSUB["I/O & Storage"]
            VFSK["VFS"]
            FSK["Filesystems"]
            BLOCKK["Block Layer"]
        end

        subgraph NETSUB["Networking"]
            SOCKK["Socket Layer"]
            PROTOK["Protocol Stack"]
        end

        subgraph DRVSUB["Drivers"]
            DRVFW["Driver Framework"]
            BUSK["Bus Enumeration (PCI, etc.)"]
        end

        subgraph HALSUB["Hardware Abstraction"]
            INTC["Interrupt Controllers"]
            TIMERC["Timers"]
            MMIOC["MMIO / Platform Access"]
        end

        EXEC --> MEMSUB
        EXEC --> COMM
        EXEC --> SECSUB
        IOSUB --> HALSUB
        NETSUB --> DRVSUB
        DRVSUB --> HALSUB
        SECSUB --> IOSUB
        SECSUB --> NETSUB
    end

    KERNEL --> HW["CPU / Memory / Devices"]
```

---

## Kernel Architecture

```mermaid
flowchart TD
    BOOT["Boot & Architecture Layer"] --> CPUINIT["CPU Initialization<br/>GDT · TSS · IDT"]
    CPUINIT --> EXC["Exception & Fault Handling"]
    EXC --> INTARCH["Interrupt Architecture<br/>Controller Discovery · Routing · Dispatch"]

    INTARCH --> MEMARCH["Memory Architecture"]
    subgraph MEMARCH["Memory Architecture"]
        direction LR
        PMEM["Physical Memory Management"] --> VMEM["Virtual Memory Management"]
        VMEM --> ADDR["Address Space Construction & Isolation"]
    end

    MEMARCH --> PROCARCH["Process Architecture"]
    subgraph PROCARCH["Process Architecture"]
        direction LR
        SCHEDA["Scheduler"] --> PROCA["Processes & Threads"]
        PROCA --> CTXA["Context Switching"]
    end

    PROCARCH --> BOUNDARY["Kernel / Userspace Boundary"]
    subgraph BOUNDARY["Kernel / Userspace Boundary"]
        direction LR
        SYSC["Syscalls"] --> IPCA["IPC"]
        IPCA --> SECA["Security & Capabilities"]
    end

    BOUNDARY --> PLATFORM["Platform & Device Layer"]
    subgraph PLATFORM["Platform & Device Layer"]
        direction LR
        DISC["Hardware Discovery"] --> DRVA["Driver Framework"]
        DRVA --> STORA["Storage Subsystem"]
        DRVA --> NETA["Networking"]
    end
```

These represent the architecture BATOS is designed around. Not all subsystems are implemented at any given point — the kernel is developed incrementally, subsystem by subsystem, with each layer expected to be correct before the next is built on it.

---

## Hardware & Platform Layer

```mermaid
flowchart TD
    FW["Firmware / Bootloader"] --> CPUMODE["CPU Mode Transition<br/>Real Mode → Long Mode"]
    CPUMODE --> CPUSETUP["CPU Structure Setup<br/>GDT · TSS · IDT"]
    CPUSETUP --> ACPI["ACPI / Platform Discovery"]

    subgraph ACPI["ACPI / Platform Discovery"]
        direction LR
        RSDP["Firmware Root Pointer"] --> TABLES["System Description Tables"]
        TABLES --> TOPOLOGY["CPU / Interrupt Topology"]
    end

    ACPI --> INTCTRL["Interrupt Controllers"]

    subgraph INTCTRL["Interrupt Controllers"]
        direction LR
        LOCALIC["Local Interrupt Controller (per-core)"]
        IOIC["I/O Interrupt Controller"]
        LEGACYIC["Legacy Controller (compatibility)"]
    end

    INTCTRL --> MEMPLAT["Memory Management"]
    MEMPLAT --> BUSDISC["Bus / Device Discovery<br/>PCI and similar"]
    BUSDISC --> DRVPLAT["Drivers"]
    DRVPLAT --> SVC["Kernel Services"]
```

BATOS boots via a standard boot protocol handoff and brings the CPU into 64-bit long mode before initializing its own memory and interrupt infrastructure. Platform discovery is used to identify interrupt controllers, CPU topology, and devices, rather than relying on hardcoded assumptions about the underlying hardware.

---

## Memory Architecture

```mermaid
flowchart TD
    APP["Process Virtual Address Space"] --> VA["Virtual Address"]
    VA --> WALK["Page Table Walk"]

    subgraph WALK["Page Table Walk"]
        direction LR
        L1["Top-Level Table"] --> L2["Directory"] --> L3["Directory / Table"] --> L4["Page Table Entry"]
    end

    WALK --> PERM{"Permission & Presence Check"}
    PERM -->|"valid"| FRAME["Physical Frame"]
    PERM -->|"invalid"| FAULT["Page Fault Handler"]

    FRAME --> PMMA["Physical Memory Manager<br/>Frame Allocation & Tracking"]
    PMMA --> RAM["Physical RAM"]

    FAULT --> RESOLVE["Resolve / Allocate / Terminate"]
```

Memory management is split into two cooperating layers:

- **Physical Memory Manager (PMM)** — tracks and allocates physical memory frames, based on the memory map provided at boot.
- **Virtual Memory Manager (VMM)** — constructs and manages page tables, builds address spaces, and enforces memory isolation between the kernel and (eventually) individual processes.

Address spaces, mapping permissions, and memory protection are treated as core correctness properties of the kernel — an incorrect mapping is expected to be caught, not silently tolerated.

---

## Interrupt Architecture

```mermaid
flowchart TD
    DEV["Device Signal"] --> SRC{"Legacy or Modern Routing?"}
    SRC -->|"legacy"| PIC["Legacy Interrupt Controller"]
    SRC -->|"modern"| IOAPIC["I/O Interrupt Controller"]

    PIC --> VECTOR["Interrupt Vector Assignment"]
    IOAPIC --> GSI["Global System Interrupt Resolution"]
    GSI --> REDIR["Redirection Table Entry<br/>Mask · Polarity · Trigger Mode"]
    REDIR --> LAPIC["Local Interrupt Controller (per-core)"]
    LAPIC --> VECTOR

    VECTOR --> CPU["CPU"]
    CPU --> IDT["Interrupt Descriptor Table"]
    IDT --> DISPATCH["Kernel Interrupt Dispatcher"]

    subgraph DISPATCH["Kernel Interrupt Dispatcher"]
        direction LR
        CLASSIFY["Classify Interrupt"] --> ROUTE{"Exception, Timer, or Device?"}
        ROUTE -->|"exception"| EXHANDLE["Exception Handler"]
        ROUTE -->|"timer"| TIMEHANDLE["Timer Handler"]
        ROUTE -->|"device"| DEVHANDLE["Device Driver Handler"]
    end

    EXHANDLE --> EOI["Signal End-of-Interrupt"]
    TIMEHANDLE --> EOI
    DEVHANDLE --> EOI
    EOI --> RESUME["Resume Interrupted Context"]
```

Interrupt handling is treated as a foundational kernel subsystem rather than an incidental one: nearly every higher-level feature (scheduling, drivers, timers, I/O) depends on interrupts being routed and dispatched correctly. BATOS's interrupt architecture is designed around modern APIC-based routing, with legacy controller support retained for compatibility during platform bring-up.

---

## Userspace Architecture

```mermaid
flowchart TD
    APP["Application Process"] --> RTLIB["Userspace Runtime / Libraries"]
    RTLIB --> API["System API Layer"]
    API --> SYSC["Syscall Entry"]

    SYSC --> GATE{"Permission & Capability Check"}
    GATE -->|"granted"| KSERV["Kernel Service Execution"]
    GATE -->|"denied"| DENY["Reject / Error Return"]

    KSERV --> RET["Return to Userspace"]

    subgraph ISOLATION["Per-Process Isolation"]
        direction LR
        ADDR["Private Address Space"]
        RES["Owned Resources / Handles"]
        PERMS["Granted Capabilities"]
    end

    APP -.-> ISOLATION

    subgraph COMM2["Inter-Process Communication"]
        direction LR
        IPCCHAN["Channels / Message Passing"]
        SHAREDMEM["Controlled Shared Memory"]
    end

    APP -.->|"mediated"| COMM2
```

The long-term architecture enforces a strict separation between userspace and the kernel:

- Each process owns an isolated address space and an explicit set of granted capabilities.
- All kernel access happens through the syscall interface — no ambient kernel access from userspace.
- IPC is mediated rather than direct, preserving isolation between processes.

---

## Storage Architecture

```mermaid
flowchart TD
    APP["Applications"] --> FSAPI["Filesystem API"]
    FSAPI --> VFS["Virtual Filesystem Layer"]

    subgraph VFS["Virtual Filesystem Layer"]
        direction LR
        NS["Namespace / Path Resolution"] --> CACHE["Metadata & Buffer Cache"]
    end

    VFS --> FSDRV{"Filesystem Implementation"}
    FSDRV --> FS1["Filesystem Type A"]
    FSDRV --> FS2["Filesystem Type B"]

    FS1 --> BLOCK["Block Layer"]
    FS2 --> BLOCK

    subgraph BLOCK["Block Layer"]
        direction LR
        SCHED2["I/O Scheduling"] --> QUEUE["Request Queue"]
    end

    BLOCK --> STORDRV["Storage Device Drivers"]
    STORDRV --> HW2["Storage Hardware"]
```

Storage is designed around a virtual filesystem (VFS) layer that abstracts over concrete filesystem implementations, sitting above a block layer that abstracts over physical storage drivers. This allows filesystem and driver implementations to evolve independently of the applications using them.

---

## Networking Architecture

```mermaid
flowchart TD
    APP["Applications"] --> SOCKAPI["Socket API"]
    SOCKAPI --> TRANSPORT["Transport Layer"]
    TRANSPORT --> NETWORKL["Network Layer"]
    NETWORKL --> LINKL["Link Layer"]
    LINKL --> NETDEV["Network Device Abstraction"]
    NETDEV --> NETDRV["Network Drivers"]
    NETDRV --> HW3["Network Hardware"]

    subgraph STACKPOLICY["Cross-Cutting Concerns"]
        direction LR
        BUFMGMT["Buffer Management"]
        FILTER["Filtering / Firewalling"]
    end

    TRANSPORT -.-> STACKPOLICY
    NETWORKL -.-> STACKPOLICY
```

Networking is architected as a layered stack: applications interact with a socket-style API, backed by transport and network layers, which communicate with hardware through a device abstraction and concrete drivers — keeping protocol logic independent of specific hardware.

---

## Security Architecture

```mermaid
flowchart TD
    ACTOR["Process / Thread / Agent"] --> REQ["Resource Request"]
    REQ --> CHECK{"Capability Held?"}
    CHECK -->|"no"| REJECT["Denied"]
    CHECK -->|"yes"| SCOPE{"Within Granted Scope?"}
    SCOPE -->|"no"| REJECT
    SCOPE -->|"yes"| SANDBOXCHK["Sandbox Boundary Check"]
    SANDBOXCHK --> ALLOW["Access Granted"]
    ALLOW --> LOG["Audit Log Entry"]
    REJECT --> LOG

    subgraph MODEL["Underlying Model"]
        direction LR
        LEASTPRIV["Least Privilege by Default"]
        ISOLATE["Process / Agent Isolation"]
        MEDIATE["Mediated IPC"]
    end

    CHECK -.-> MODEL
```

Security is treated as a first-class architectural concern, not a layer added after functionality exists. The intended model is built around strict kernel/user separation, capability-based access rather than ambient authority, least-privilege defaults, process isolation, sandboxing for untrusted contexts, mediated IPC, and auditable operations for anything crossing the kernel boundary.

These are architectural goals guiding design decisions throughout the kernel; they are not claims that every guarantee is fully enforced at every stage of development.

---

## Graphics & Desktop

```mermaid
flowchart TD
    FB["Framebuffer / Display Layer"] --> GFX["Graphics Subsystem<br/>Drawing & Buffering Primitives"]
    GFX --> COMPOSITOR["Compositor / Window System"]
    INPUT["Input Subsystem<br/>Keyboard · Pointer · Other"] --> COMPOSITOR
    COMPOSITOR --> WM["Window / Surface Management"]
    WM --> DESKTOP["Desktop Environment"]
    DESKTOP --> APPS2["Applications"]
```

This is architectural direction rather than a description of current capability — the graphics stack is intended to be built incrementally once core kernel services are stable.

---

## AI-Native / Agentic OS

A defining part of the BATOS vision is integrating AI agents as **controlled, governed system-level actors** — not by giving an AI model unrestricted root or kernel access, but by routing agent actions through the same permission and capability infrastructure that governs any other system component.

```mermaid
flowchart TD
    USER["User"] --> AGENT["AI Agent"]
    AGENT --> INTENT["Intent Understanding"]
    INTENT --> PLAN["Planning"]
    PLAN --> ACTIONS["Proposed Actions"]

    ACTIONS --> POLICY{"Policy / Permission Engine"}
    POLICY -->|"sensitive action"| APPROVAL["User Approval Requested"]
    POLICY -->|"within granted scope"| SANDBOX["Capability / Sandbox Boundary"]
    APPROVAL -->|"approved"| SANDBOX
    APPROVAL -->|"denied"| STOP["Action Blocked"]

    SANDBOX --> SVC["OS Services"]
    SVC --> SC["Syscalls"]
    SC --> KERNELA["BATOS Kernel"]
    KERNELA --> HW4["Hardware"]

    SANDBOX --> AUDITLOG["Auditable Action Log"]
```

```mermaid
flowchart LR
    ORCH["Governance & Policy Layer"] --> A1["System Management Agent"]
    ORCH --> A2["Development Agent"]
    ORCH --> A3["File Operations Agent"]
    ORCH --> A4["Networking Agent"]
    ORCH --> A5["Security Monitoring Agent"]
    ORCH --> A6["Resource / Performance Agent"]
    ORCH --> A7["Application-Level Agent"]

    A1 --> CAPLAYER["Shared Capability & Sandbox Layer"]
    A2 --> CAPLAYER
    A3 --> CAPLAYER
    A4 --> CAPLAYER
    A5 --> CAPLAYER
    A6 --> CAPLAYER
    A7 --> CAPLAYER

    CAPLAYER --> KERNELB["BATOS Kernel"]
```

Key properties this architecture is designed around: explicit, scoped permissions per agent and per action; sandboxing that limits an agent's reach to what it has been granted; capability-based access consistent with the kernel's general security model; auditable agent actions; user approval for sensitive or irreversible operations; and kernel protection against unrestricted or implicit AI access — an agent is a governed client of the OS, not a privileged extension of it.

This is a long-term architectural direction that depends on the underlying process isolation, syscall, and capability infrastructure existing first.

---

## Development Roadmap

```mermaid
flowchart TD
    A["Kernel Foundation<br/>Boot · CPU Init · Core Structures"] --> B["Memory & Interrupt Architecture"]
    B --> C["Processes & Scheduling"]
    C --> D["Syscalls & Userspace"]
    D --> E["Drivers & Hardware Support"]
    E --> F["Storage & Filesystems"]
    E --> G["Networking"]
    F --> H["Security Hardening"]
    G --> H
    H --> I["Graphics & Desktop"]
    I --> J["Developer Ecosystem"]
    J --> K["AI-Native System Layer"]
    K --> L["Production Hardening"]
```

This roadmap reflects the overall direction of the project rather than a fixed schedule. Each stage builds on the correctness of the ones before it.

---

## Repository Structure

```
batos-os/
├── boot/            # Boot-stage assets
├── kernel/          # Kernel source
│   └── arch/x86_64/ # Architecture-specific subsystems (CPU, memory, interrupts, platform discovery)
├── limine/          # Bootloader integration
├── linker.ld        # Kernel linker script
├── limine.conf       # Boot configuration
└── Makefile          # Build system
```

Directory contents will grow as subsystems are added; this structure reflects the architectural layout rather than an exhaustive file listing.

---

## Building

```bash
make
```

Produces a bootable kernel image and ISO using the project's Makefile and linker script.

---

## Running

```bash
qemu-system-x86_64 -cdrom build/batos.iso -serial stdio
```

---

## Engineering Philosophy

BATOS is developed under a consistent cycle: **build → inspect → verify → test → document.**

Each subsystem is implemented, then inspected against its expected hardware/software state, verified through direct testing rather than assumption, and only then built upon. Large, unverified rewrites are avoided in favor of incremental, checkable progress — correctness at each layer is treated as a prerequisite for the next.

---

## Project Status

BATOS is an actively developed, early-stage operating system project. Core kernel subsystems are being implemented incrementally, following the architecture described above.

---

## License

No license is currently specified for this repository.