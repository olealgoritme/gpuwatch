# gpuwatch

A sleek terminal GPU **core + VRAM temperature** watcher for Linux — reading
temperatures **directly from hardware registers** (no NVML, no driver API, no
external runtime). Built on [flux.h](https://github.com/olealgoritme/flux.h).

Ships as both a **standalone app** (TUI / ASCII / JSON) and a small **C library**
you can drop into your own program.

> Experimental. Reads reverse-engineered on-die and GDDR sensors. Currently
> supports **RTX 5090 (Blackwell / GB202, GDDR7)**; more GPUs are one table
> entry away (see *Adding a GPU*).

## Requirements

- Linux, root (reads `/dev/mem`)
- Kernel boot parameter `iomem=relaxed`
- `libpci` (`sudo apt install libpci-dev`)

## Install

Prebuilt packages for each release are on the
[**latest release**](https://github.com/olealgoritme/gpuwatch/releases/latest) page.

Debian / Ubuntu:

```
curl -sSLO https://github.com/olealgoritme/gpuwatch/releases/latest/download/gpuwatch_amd64.deb
sudo apt install ./gpuwatch_amd64.deb
```

Fedora / RHEL / openSUSE:

```
curl -sSLO https://github.com/olealgoritme/gpuwatch/releases/latest/download/gpuwatch.x86_64.rpm
sudo rpm -i gpuwatch.x86_64.rpm
```

Portable tarball (any distro):

```
curl -sSL https://github.com/olealgoritme/gpuwatch/releases/latest/download/gpuwatch-linux-x86_64.tar.gz | tar xz
```

## Build from source

```
make            # builds build/libgpuwatch.a and build/gpuwatch
sudo make install   # installs to /usr/local (bin + lib + header)
```

## Run standalone

```
sudo gpuwatch                 # sleek TUI (default)
sudo gpuwatch --ascii         # plain CLI, pipe-friendly, loops every second
sudo gpuwatch --ascii --once  # single sample and exit
sudo gpuwatch --json          # JSON (one object per sample)
sudo gpuwatch --json --once   # single JSON sample
sudo gpuwatch --interval 5    # sample every 5s (ascii/json)
```

TUI keys: `m` toggle per-module view · `q` quit.

ASCII output:

```
gpu0 RTX 5090 [01:00.0] GB202/GDDR7  core=45C  vram=50C  modules=[48,48,50,50,48,48,50,50]
```

JSON output:

```json
{"gpus":[{"index":0,"name":"RTX 5090","arch":"GB202","vram":"GDDR7",
  "pci":"01:00.0","core_c":45,"vram_c":50,"modules_c":[48,48,50,50,48,48,50,50]}]}
```

## Use as a library

Link `libgpuwatch.a` and include `gpuwatch.h`. The whole API is three calls:

```c
#include "gpuwatch.h"
#include <stdio.h>

int main(void) {
    gpuwatch_ctx ctx;
    int n = gpuwatch_init(&ctx);          // < 0 on fatal error
    if (n < 0) { fprintf(stderr, "%s\n", gpuwatch_strerror(n)); return 1; }

    gpuwatch_sample(&ctx);                 // refresh temps (call each frame)

    for (int i = 0; i < ctx.count; i++) {
        gpuwatch_gpu *g = &ctx.gpus[i];
        printf("%s: core=%d%s vram=%d%s\n", g->name,
               g->core_temp, g->core_valid ? "C" : " (n/a)",
               g->vram_temp, g->vram_valid ? "C" : " (n/a)");
        for (int m = 0; m < g->module_count; m++)
            printf("  module %d: %dC\n", m, g->module_temp[m]);
    }

    gpuwatch_shutdown(&ctx);
    return 0;
}
```

Build it:

```
cc -O2 myapp.c -Ipath/to/gpuwatch libgpuwatch.a -lpci -o myapp
# or, if installed:
cc -O2 myapp.c -lgpuwatch -lpci -o myapp
```

Every field (`core_temp`, `vram_temp`, per-module `module_temp[]`, and the
matching `*_valid` flags) is documented in `gpuwatch.h`. All reads are read-only;
an out-of-range register offset can never read outside the device aperture.

## Adding a GPU

Support is data-driven. Add one row to `k_table[]` in `lib/gpuwatch.c` with the
PCI id, names, and the register recipe (core offset + decode, VRAM offset +
decode, and an optional per-module stride). The sampling logic is generic over
that recipe — no other code changes needed.

## How it works

- **Core/die temp** and **GDDR memory temp** are read straight from the GPU's
  BAR0 MMIO registers via `/dev/mem`.
- On the RTX 5090 the GDDR7 memory temperature is read per memory partition from
  the FBPA DRAM sensors and reported as the hotspot (hottest module), with each
  module also available individually.
- No NVML/driver dependency means it works headless and adds nothing to load.

## License

MIT © xuw (olealgoritme). Bundles `flux.h` (MIT, same author).
