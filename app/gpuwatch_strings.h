// gpuwatch_strings.h - user-facing strings, formats, and CLI tokens.
#ifndef GPUWATCH_STRINGS_H
#define GPUWATCH_STRINGS_H

// ── Branding / chrome ───────────────────────────────────────────────────────
#define S_APP_NAME            "gpuwatch"
#define S_TITLE_BANNER        "GPUWATCH"          // big gradient banner

// ── Labels ──────────────────────────────────────────────────────────────────
#define S_LBL_CORE            "CORE"
#define S_LBL_VRAM            "VRAM"
#define S_LBL_NA              "n/a"
#define S_DEG_C               "\xc2\xb0" "C"       // °C
#define S_MODULE_PREFIX       "m"                  // per-module row labels: m0..

// ── Section titles ──────────────────────────────────────────────────────────
#define S_TITLE_CORE          "CORE  °C"
#define S_TITLE_VRAM          "VRAM  °C"
#define S_TITLE_MODULES       "per-module GDDR7  °C"

// ── Empty / error states ────────────────────────────────────────────────────
#define S_NOGPU_TITLE         "No supported GPU found"
#define S_NOGPU_BODY          "Need a supported NVIDIA GPU, root privileges, and iomem=relaxed."
#define S_ERR_PREFIX          S_APP_NAME ": "
#define S_ERR_NO_GPU          "no supported GPU found"

// ── Footer key hints ────────────────────────────────────────────────────────
#define S_KEY_MODULES         "m"
#define S_HINT_MODULES        "modules"
#define S_KEY_QUIT            "q"
#define S_HINT_QUIT           "quit"

// ── printf formats (ASCII mode) ─────────────────────────────────────────────
#define S_FMT_ASCII_HEAD      "gpu%d %s [%02x:%02x.%x] %s/%s"
#define S_FMT_ASCII_CORE      "  core="
#define S_FMT_ASCII_VRAM      "  vram="
#define S_FMT_TEMP_C          "%dC"
#define S_FMT_ASCII_MOD_OPEN  "  modules=["
#define S_FMT_ASCII_MOD_CLOSE "]"
#define S_SEP_COMMA           ","

// ── PCI address format ──────────────────────────────────────────────────────
#define S_FMT_PCI             "%02x:%02x.%x"

// ── CLI tokens ──────────────────────────────────────────────────────────────
#define S_ARG_ASCII           "--ascii"
#define S_ARG_JSON            "--json"
#define S_ARG_ONCE            "--once"
#define S_ARG_INTERVAL        "--interval"
#define S_ARG_HELP_LONG       "--help"
#define S_ARG_HELP_SHORT      "-h"
#define S_ARG_VERSION         "--version"

// Fallback if not injected by the build (-DGPUWATCH_VERSION="x.y.z").
#ifndef GPUWATCH_VERSION
#define GPUWATCH_VERSION      "dev"
#endif

#define S_USAGE \
    "Usage: %s [" S_ARG_ASCII " | " S_ARG_JSON "] [" S_ARG_INTERVAL " SECONDS] [" S_ARG_ONCE "] [" S_ARG_HELP_LONG "]\n" \
    "  (default)          sleek TUI watcher\n" \
    "  " S_ARG_ASCII "            plain CLI output (pipe-friendly)\n" \
    "  " S_ARG_JSON "             JSON output\n" \
    "  " S_ARG_INTERVAL " N       seconds between samples in ascii/json (default 1)\n" \
    "  " S_ARG_ONCE "            print a single sample and exit (ascii/json)\n" \
    "  " S_ARG_VERSION "          print version and exit\n" \
    "  " S_ARG_HELP_LONG "             this help\n"

#define S_ERR_UNKNOWN_OPT     "unknown option: %s\n"

// ── JSON keys ───────────────────────────────────────────────────────────────
#define J_GPUS      "gpus"
#define J_INDEX     "index"
#define J_NAME      "name"
#define J_ARCH      "arch"
#define J_VRAM      "vram"
#define J_PCI       "pci"
#define J_CORE_C    "core_c"
#define J_VRAM_C    "vram_c"
#define J_MODULES_C "modules_c"
#define J_NULL      "null"

#endif // GPUWATCH_STRINGS_H
