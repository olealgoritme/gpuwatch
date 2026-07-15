// gpuwatch.c — direct-from-hardware GPU temperature reader (no NVML).
//
// To add a GPU: add one row to k_table[] with its PCI id, names, and a register
// recipe. The sampling logic is fully generic over that recipe.
//
// Design notes / safety:
//   * All hardware access is read-only (/dev/mem O_RDONLY, PROT_READ).
//   * Every register offset is bounds-checked against the BAR0 size before the
//     mmap, so a bad recipe can never read outside the device aperture.
//   * The /dev/mem fd lives in the context (no global state; re-entrant across
//     independent contexts).
//   * On any read/decode failure the corresponding *_valid flag is cleared and
//     stale values are never presented as current.
#define _GNU_SOURCE
#include "gpuwatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <pci/pci.h>

// ── decode kinds ────────────────────────────────────────────────────────────
enum decode {
    DEC_NONE = 0,       // sensor not present
    DEC_FIXED16_256,    // THERM die: (raw & 0xFFFF) / 256
    DEC_ADA_12_32,      // Ada/Ampere VRAM: (raw & 0xFFF) / 32
    DEC_GDDR_MRCODE,    // FBPA DQR MR-code in bits 23:16
};

// A per-module (per memory partition) VRAM sensor block.
typedef struct {
    uint32_t base;        // offset of module 0's sensor
    uint32_t stride;      // added per module (0 disables per-module scan)
    uint32_t vld_off;     // offset from a module's base to its validity reg
    int      max_modules; // upper bound on partitions to probe
} module_map;

// The register recipe for one GPU family.
typedef struct {
    uint16_t     dev_id;
    const char  *name, *arch, *vram;
    uint32_t     core_off;   enum decode core_dec;
    uint32_t     vram_off;   enum decode vram_dec;   // single/broadcast VRAM reg
    module_map   modules;                            // per-module VRAM (optional)
} gpuwatch_model;

// ── GPU table — ADD NEW GPUs HERE ───────────────────────────────────────────
static const gpuwatch_model k_table[] = {
    {
        .dev_id = 0x2b85, .name = "RTX 5090", .arch = "GB202", .vram = "GDDR7",
        .core_off = 0x00AD0A9C, .core_dec = DEC_FIXED16_256,
        .vram_off = 0x009A24C0, .vram_dec = DEC_GDDR_MRCODE,
        .modules  = { .base = 0x009024C0, .stride = 0x4000, .vld_off = 0x10, .max_modules = 16 },
    },
    // Example (untested) — Ada/Ampere single-register VRAM sensor:
    // { .dev_id=0x2684, .name="RTX 4090", .arch="AD102", .vram="GDDR6X",
    //   .vram_off=0x0000E2A8, .vram_dec=DEC_ADA_12_32 },
};

#define NELEMS(a) (sizeof(a)/sizeof((a)[0]))

// PCI class (high byte) for display controllers.
#define PCI_CLASS_VGA 0x03

// ── raw MMIO read (bounds-checked) ──────────────────────────────────────────
// Reads a 32-bit register at BAR0+off for gpu g. Returns 0 on success.
// Fails safely if off is outside the BAR, the fd is bad, or mmap fails.
static int read_reg(const gpuwatch_ctx *ctx, const gpuwatch_gpu *g,
                    uint32_t off, uint32_t *out)
{
    if (ctx->mem_fd < 0 || g->bar0 == 0) return -1;
    // Bounds check: the whole 4-byte read must lie inside the BAR.
    if (g->bar0_size != 0 && ((uint64_t)off + 4u) > g->bar0_size) return -1;

    long pg = sysconf(_SC_PAGE_SIZE);
    if (pg <= 0) return -1;
    uint64_t phys = g->bar0 + (uint64_t)off;
    uint64_t base = phys & ~((uint64_t)pg - 1);
    size_t   within = (size_t)(phys - base);

    volatile void *m = mmap(0, (size_t)pg, PROT_READ, MAP_SHARED, ctx->mem_fd, (off_t)base);
    if (m == MAP_FAILED) return -1;
    uint32_t v = *(volatile uint32_t *)((const uint8_t *)m + within);
    munmap((void *)m, (size_t)pg);
    *out = v;
    return 0;
}

// ── decoders ────────────────────────────────────────────────────────────────
static int decode(enum decode d, uint32_t raw, int *ok)
{
    *ok = 1;
    switch (d) {
    case DEC_FIXED16_256: return (int)((raw & 0xFFFFu) / 256u);
    case DEC_ADA_12_32:   return (int)((raw & 0x0FFFu) / 32u);
    case DEC_GDDR_MRCODE: {
        int code = (int)((raw >> 16) & 0xFFu);
        if (code > 80) code = 80;
        return (code > 19) ? (code - 20) * 2 : -(40 - code * 2);
    }
    case DEC_NONE:
    default: *ok = 0; return 0;
    }
}

static int plausible(int c) { return c > -40 && c < 130; }

static const gpuwatch_model *lookup(uint16_t dev_id)
{
    for (size_t i = 0; i < NELEMS(k_table); i++)
        if (k_table[i].dev_id == dev_id) return &k_table[i];
    return NULL;
}

// ── public API ──────────────────────────────────────────────────────────────
const char *gpuwatch_strerror(int status)
{
    switch (status) {
    case GPUWATCH_ERR_DEVMEM: return "cannot open /dev/mem (run as root; boot with iomem=relaxed)";
    case GPUWATCH_ERR_PCI:    return "PCI access initialisation failed";
    case GPUWATCH_ERR_ARG:    return "invalid argument";
    default:                  return (status >= 0) ? "ok" : "unknown error";
    }
}

int gpuwatch_init(gpuwatch_ctx *ctx)
{
    if (!ctx) return GPUWATCH_ERR_ARG;
    memset(ctx, 0, sizeof(*ctx));
    ctx->mem_fd = -1;

    ctx->mem_fd = open("/dev/mem", O_RDONLY | O_CLOEXEC);
    if (ctx->mem_fd == -1) return GPUWATCH_ERR_DEVMEM;

    struct pci_access *pacc = pci_alloc();
    if (!pacc) { close(ctx->mem_fd); ctx->mem_fd = -1; return GPUWATCH_ERR_PCI; }
    pci_init(pacc);
    pci_scan_bus(pacc);

    for (struct pci_dev *d = pacc->devices; d && ctx->count < GPUWATCH_MAX_GPUS; d = d->next) {
        pci_fill_info(d, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS | PCI_FILL_SIZES);

        // Only NVIDIA display controllers (skip the HDA audio function that
        // shares the board, and any non-GPU device that reuses an id).
        if (d->vendor_id != 0x10de) continue;
        if ((d->device_class >> 8) != PCI_CLASS_VGA &&
            (d->device_class >> 8) != 0x02 /* 3D controller = 0x0302 */) {
            // device_class is the full class/subclass; VGA=0x0300, 3D=0x0302.
            if (d->device_class != 0x0300 && d->device_class != 0x0302) continue;
        }

        const gpuwatch_model *k = lookup(d->device_id);
        if (!k) continue;

        // BAR0 must be a valid, non-zero memory BAR.
        pciaddr_t raw_bar = d->base_addr[0];
        if (raw_bar == 0) continue;
        if (raw_bar & PCI_BASE_ADDRESS_SPACE_IO) continue;   // I/O BAR, not memory
        uint64_t bar0 = (uint64_t)(raw_bar & ~(pciaddr_t)0xfULL);
        if (bar0 == 0) continue;

        gpuwatch_gpu *g = &ctx->gpus[ctx->count++];
        memset(g, 0, sizeof(*g));
        g->dev_id = d->device_id;
        g->bus = (uint8_t)d->bus; g->dev = (uint8_t)d->dev; g->func = (uint8_t)d->func;
        g->name = k->name; g->arch = k->arch; g->vram = k->vram;
        g->bar0 = bar0;
        g->bar0_size = (uint64_t)d->size[0];   // 0 if unknown -> bounds check skipped
        g->model = k;
    }

    pci_cleanup(pacc);
    return ctx->count;
}

static void sample_one(const gpuwatch_ctx *ctx, gpuwatch_gpu *g)
{
    const gpuwatch_model *k = g->model;
    uint32_t raw;
    int ok;

    g->core_valid = 0;
    g->vram_valid = 0;
    g->vram_temp  = 0;
    g->module_count = 0;

    if (!k) return;

    // Core / die temperature.
    if (k->core_dec != DEC_NONE &&
        read_reg(ctx, g, k->core_off, &raw) == 0) {
        int t = decode(k->core_dec, raw, &ok);
        if (ok && plausible(t)) { g->core_temp = t; g->core_valid = 1; }
    }

    // Per-module VRAM (preferred): validity- and poison-gated scan.
    int hottest = -128;
    const module_map *mm = &k->modules;
    if (mm->stride && mm->max_modules && k->vram_dec != DEC_NONE) {
        int limit = mm->max_modules;
        if (limit > GPUWATCH_MAX_MODULES) limit = GPUWATCH_MAX_MODULES;
        for (int p = 0; p < limit; p++) {
            uint32_t off = mm->base + (uint32_t)p * mm->stride;
            uint32_t vld = 0, dq = 0;
            if (read_reg(ctx, g, off + mm->vld_off, &vld) != 0) continue;
            if (read_reg(ctx, g, off, &dq) != 0) continue;
            if (((vld >> 24) & 0xFu) != 0xFu) continue;              // not all valid
            if ((dq & 0xFFFF0000u) == 0xBADF0000u) continue;         // poison sentinel
            int t = decode(k->vram_dec, dq, &ok);
            if (!ok || !plausible(t)) continue;
            g->module_temp[g->module_count++] = t;
            if (t > hottest) hottest = t;
        }
    }

    if (g->module_count > 0) {
        g->vram_temp = hottest;
        g->vram_valid = 1;
    } else if (k->vram_dec != DEC_NONE &&
               read_reg(ctx, g, k->vram_off, &raw) == 0) {
        int t = decode(k->vram_dec, raw, &ok);
        if (ok && plausible(t)) { g->vram_temp = t; g->vram_valid = 1; }
    }
}

void gpuwatch_sample(gpuwatch_ctx *ctx)
{
    if (!ctx) return;
    for (int i = 0; i < ctx->count; i++)
        sample_one(ctx, &ctx->gpus[i]);
}

void gpuwatch_shutdown(gpuwatch_ctx *ctx)
{
    if (!ctx) return;
    if (ctx->mem_fd != -1) { close(ctx->mem_fd); ctx->mem_fd = -1; }
    ctx->count = 0;
}
