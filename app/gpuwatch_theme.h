// gpuwatch_theme.h — colors, thresholds, layout, and timing.
#ifndef GPUWATCH_THEME_H
#define GPUWATCH_THEME_H

#include "flux.h"

// ── Layout ──────────────────────────────────────────────────────────────────
// Content width is the live terminal width (flux_cols()), floored at a minimum;
// the UI fills the pane edge-to-edge and reflows on resize.
#define UI_WIDTH_MIN        40
#define UI_HISTORY_LEN      512     // history ring capacity
#define UI_HISTORY_HEIGHT   10      // history chart rows
#define UI_GAUGE_MIN        0.0f
#define UI_GAUGE_MAX        110.0f

// ── Timing ──────────────────────────────────────────────────────────────────
#define UI_FPS              4       // TUI render rate
#define UI_SAMPLE_MS        1000    // hardware sample interval (TUI)
#define CLI_INTERVAL_DEFAULT 1      // seconds (ascii/json loop)

// ── Temperature thresholds (°C) — drive the colour bands ────────────────────
#define TEMP_WARN_C         60.0f
#define TEMP_HOT_C          85.0f

// ── Temperature bar display range (°C) ──────────────────────────────────────
// Bars are scaled to this fixed window (not autoscaled), so small differences
// between modules are visible and the scale is stable frame to frame.
#define TEMP_BAR_LO         30.0f
#define TEMP_BAR_HI         100.0f

// Bar glyphs.
#define GLYPH_BAR_FULL      "\xe2\x96\x88"   // █ full cell
#define GLYPH_BAR_EMPTY     "\xe2\x96\x91"   // ░ light shade
#define BAR_LABEL_W         6                // left label column width
#define BAR_VALUE_W         6                // right "  NN°C" column width
#define MODULE_CELL_W       10               // width of one compact module cell

// ── Accent gradient (brand) for banner / frames ─────────────────────────────
#define RGB_ACCENT_A        ((FluxRGB){180, 130, 255})   // purple
#define RGB_ACCENT_B        ((FluxRGB){100, 255, 200})   // teal
#define RGB_GLOW            ((FluxRGB){140, 120, 255})

// ── Semantic colours (reuse flux theme tokens, named for our use) ───────────
#define COL_OK              FLUX_THEME_OK_FG
#define COL_WARN            FLUX_THEME_WARN_FG
#define COL_HOT             FLUX_THEME_ERR_FG
#define COL_ACCENT          FLUX_THEME_ACCENT_FG
#define COL_ACCENT_DIM      FLUX_THEME_ACCENT_DIM_FG
#define COL_TEXT            FLUX_THEME_TEXT_FG
#define COL_TEXT2           FLUX_THEME_TEXT2_FG
#define COL_DIM             FLUX_THEME_TEXT_DIM_FG
#define COL_BRAND           FLUX_THEME_BRAND_PURPLE_FG
#define COL_HIST_CORE       FLUX_THEME_ACCENT_FG        // core valley fill
#define COL_HIST_VRAM       FLUX_THEME_BRAND_PURPLE_FG  // vram valley fill

// History chart y-range (°C) — fixed so both charts share a stable scale.
#define HIST_Y_LO           30.0f
#define HIST_Y_HI           100.0f
#define HIST_GAP            "  "                        // gap between the two charts

// Text styling.
#define STYLE_BOLD          FLUX_BOLD
#define STYLE_RESET         "\x1b[0m"

#endif // GPUWATCH_THEME_H
