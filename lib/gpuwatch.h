// gpuwatch.h - GPU core + VRAM temperature reader library.
//
// Reads NVIDIA GPU temperatures on Linux directly from BAR0 hardware registers
// (no NVML, no driver API). Requires root, iomem=relaxed, and libpci.
#ifndef GPUWATCH_H
#define GPUWATCH_H

#include <stdint.h>

#define GPUWATCH_MAX_GPUS    16
#define GPUWATCH_MAX_MODULES 16

typedef struct {
    // identity
    uint16_t    dev_id;
    uint8_t     bus, dev, func;
    const char *name;
    const char *arch;
    const char *vram;

    // temperatures (Celsius); *_valid is 0 if unavailable
    int  core_temp;   int core_valid;    // GPU die / core sensor
    int  vram_temp;   int vram_valid;    // VRAM hotspot (max across modules)

    // per-module VRAM temps. module_count is how many are populated.
    int  module_temp[GPUWATCH_MAX_MODULES];
    int  module_count;

    // internal
    uint64_t     bar0;        // BAR0 physical base (may be a 64-bit address)
    uint64_t     bar0_size;   // BAR0 region size, for bounds-checking reads
    const void  *model;       // -> gpuwatch_model describing the register recipe
} gpuwatch_gpu;

typedef struct {
    gpuwatch_gpu gpus[GPUWATCH_MAX_GPUS];
    int          count;
    int          mem_fd;      // /dev/mem, owned by this context (-1 if closed)
} gpuwatch_ctx;

// Result of gpuwatch_init.
typedef enum {
    GPUWATCH_OK          =  0,   // >=0 GPUs detected (see ctx->count)
    GPUWATCH_ERR_DEVMEM  = -1,   // cannot open /dev/mem (need root)
    GPUWATCH_ERR_PCI     = -2,   // libpci init failed
    GPUWATCH_ERR_ARG     = -3,   // NULL ctx
} gpuwatch_status;

// Detect supported GPUs and open resources. Returns the number of GPUs found
// (>= 0) on success, or a negative gpuwatch_status on fatal error. Safe to call
// once per ctx; call gpuwatch_shutdown before re-initialising the same ctx.
int  gpuwatch_init(gpuwatch_ctx *ctx);

// Human-readable message for a negative gpuwatch_init return.
const char *gpuwatch_strerror(int status);

// Refresh all temperatures for every detected GPU. Call once per frame.
void gpuwatch_sample(gpuwatch_ctx *ctx);

// Release resources.
void gpuwatch_shutdown(gpuwatch_ctx *ctx);

#endif // GPUWATCH_H
