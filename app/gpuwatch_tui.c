// gpuwatch_tui.c — GPU core + VRAM temperature watcher.
//
// Modes (see --help):
//   (default)   sleek TUI (flux.h)
//   --ascii     plain looping/one-shot CLI output (pipe-friendly)
//   --json      JSON output
//
// Reads temperatures directly from hardware via lib/gpuwatch (no NVML).
// All user-facing strings live in gpuwatch_strings.h; all style/tunables in
// gpuwatch_theme.h (single source of truth — no literals in this file).
#define FLUX_IMPL
#include "flux.h"
#include "gpuwatch.h"
#include "gpuwatch_strings.h"
#include "gpuwatch_theme.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// ── app state ───────────────────────────────────────────────────────────────
typedef struct {
    gpuwatch_ctx gpu;
    float core_hist[GPUWATCH_MAX_GPUS][UI_HISTORY_LEN];
    float vram_hist[GPUWATCH_MAX_GPUS][UI_HISTORY_LEN];
    int   hist_len[GPUWATCH_MAX_GPUS];
    int   show_modules;
} App;

static void push_hist(float *ring, int *len, float v)
{
    if (*len < UI_HISTORY_LEN) { ring[(*len)++] = v; return; }
    memmove(ring, ring + 1, (UI_HISTORY_LEN - 1) * sizeof(float));
    ring[UI_HISTORY_LEN - 1] = v;
}

static void record_history(App *a)
{
    for (int i = 0; i < a->gpu.count; i++) {
        gpuwatch_gpu *g = &a->gpu.gpus[i];
        int len_before = a->hist_len[i];
        // core ring advances the shared length; vram uses a throwaway counter so
        // both rings stay the same length in lockstep.
        int tmp = len_before;
        push_hist(a->core_hist[i], &a->hist_len[i],
                  g->core_valid ? (float)g->core_temp : 0.0f);
        push_hist(a->vram_hist[i], &tmp,
                  g->vram_valid ? (float)g->vram_temp : 0.0f);
    }
}

// ── TUI ─────────────────────────────────────────────────────────────────────
// Live content width: fill the terminal, clamped to sane bounds.
static int ui_width(void)
{
    int w = flux_cols() - 2 * UI_SIDE_MARGIN;
    if (w < UI_WIDTH_MIN) w = UI_WIDTH_MIN;
    if (w > UI_WIDTH_MAX) w = UI_WIDTH_MAX;
    return w;
}

static FluxCmd tui_init(FluxModel *m)
{
    App *a = m->state;
    a->show_modules = 1;
    // Sample immediately so the first rendered frame is fully populated
    // (no blank/animating-in launch).
    gpuwatch_sample(&a->gpu);
    record_history(a);
    return flux_tick(UI_SAMPLE_MS);
}

static FluxCmd tui_update(FluxModel *m, FluxMsg msg)
{
    App *a = m->state;
    if (flux_key_is(msg, S_KEY_QUIT) || flux_key_is(msg, "C-c")) return FLUX_CMD_QUIT;
    if (flux_key_is(msg, S_KEY_MODULES)) a->show_modules = !a->show_modules;
    if (msg.type == MSG_TICK) {
        gpuwatch_sample(&a->gpu);
        record_history(a);
        return flux_tick(UI_SAMPLE_MS);
    }
    return FLUX_CMD_NONE;
}

// Colour for a temperature, by threshold band (SSOT for temp colouring).
static const char *temp_color(int c)
{
    if ((float)c >= TEMP_HOT_C)  return COL_HOT;
    if ((float)c >= TEMP_WARN_C) return COL_WARN;
    return COL_OK;
}

// Render one temperature row:  LABEL  ███████████░░░░░  NN°C
// Bar is scaled to the fixed [TEMP_BAR_LO, TEMP_BAR_HI] window so differences
// are visible and the scale never jumps. Colour follows the threshold band.
static void temp_bar(FluxSB *sb, const char *label, int valid, int temp, int w)
{
    flux_sb_appendf(sb, "%s%-*s%s ", COL_TEXT2, BAR_LABEL_W, label, COL_TEXT);
    int bar_w = w - BAR_LABEL_W - 1 - BAR_VALUE_W;
    if (bar_w < 4) bar_w = 4;

    if (!valid) {
        flux_sb_appendf(sb, "%s%-*s%s\n", COL_DIM, bar_w + 1 + BAR_VALUE_W, S_LBL_NA, COL_TEXT);
        return;
    }

    float t = ((float)temp - TEMP_BAR_LO) / (TEMP_BAR_HI - TEMP_BAR_LO);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int fill = (int)(t * (float)bar_w + 0.5f);

    const char *col = temp_color(temp);
    flux_sb_append(sb, col);
    for (int i = 0; i < fill; i++)    flux_sb_append(sb, GLYPH_BAR_FULL);
    flux_sb_append(sb, COL_DIM);
    for (int i = fill; i < bar_w; i++) flux_sb_append(sb, GLYPH_BAR_EMPTY);

    flux_sb_appendf(sb, " %s%3d%s%s\n", col, temp, S_DEG_C, COL_TEXT);
}

static void tui_view(FluxModel *m, char *buf, int bufsz)
{
    App *a = m->state;
    FluxSB sb; flux_sb_init(&sb, buf, bufsz);
    const int w = ui_width();   // fills the terminal, reflows on resize

    // Flashy gradient banner + tagline header.
    flux_gradient_text(&sb, S_TITLE_BANNER, RGB_ACCENT_A, RGB_ACCENT_B, w);
    flux_sb_nl(&sb);
    flux_header(&sb, S_APP_NAME, S_APP_TAGLINE, w);
    flux_sb_nl(&sb);

    if (a->gpu.count == 0) {
        flux_alert(&sb, FLUX_KIND_ERROR, S_NOGPU_TITLE, S_NOGPU_BODY, w);
        flux_sb_nl(&sb);
        FluxKeyHint h[] = {{S_KEY_QUIT, S_HINT_QUIT}};
        flux_footer(&sb, h, 1, w);
        return;
    }

    for (int i = 0; i < a->gpu.count; i++) {
        gpuwatch_gpu *g = &a->gpu.gpus[i];

        flux_sb_appendf(&sb, "%s%s%s  %s" S_FMT_PCI " · %s · %s%s\n",
            COL_ACCENT, g->name, COL_DIM, COL_TEXT2,
            g->bus, g->dev, g->func, g->arch, g->vram, COL_TEXT);
        flux_sb_nl(&sb);

        temp_bar(&sb, S_LBL_CORE, g->core_valid, g->core_temp, w);
        temp_bar(&sb, S_LBL_VRAM, g->vram_valid, g->vram_temp, w);
        flux_sb_nl(&sb);

        if (a->hist_len[i] > 1) {
            // Two side-by-side "valley" area charts: CORE (left), VRAM (right).
            // Each renders into its own scratch buffer, then flux_hbox joins them.
            int gap = (int)sizeof(HIST_GAP) - 1;
            int half = (w - gap) / 2;
            if (half < UI_WIDTH_MIN / 2) half = UI_WIDTH_MIN / 2;

            static char lbuf[64 * 1024], rbuf[64 * 1024];
            FluxSB lsb, rsb;
            flux_sb_init(&lsb, lbuf, sizeof lbuf);
            flux_sb_init(&rsb, rbuf, sizeof rbuf);

            // Bold, coloured panel titles (the widget's own title is dim/grey).
            flux_sb_appendf(&lsb, "%s%s%s%s\n", STYLE_BOLD, COL_HIST_CORE, S_TITLE_CORE, STYLE_RESET);
            flux_sb_appendf(&rsb, "%s%s%s%s\n", STYLE_BOLD, COL_HIST_VRAM, S_TITLE_VRAM, STYLE_RESET);

            FluxAreaOpts ca = {0};
            ca.color = COL_HIST_CORE;   // title drawn above; NULL here
            ca.y_min = HIST_Y_LO; ca.y_max = HIST_Y_HI; ca.show_axes = 1;
            flux_area_chart(&lsb, a->core_hist[i], a->hist_len[i], half, UI_HISTORY_HEIGHT, &ca);

            FluxAreaOpts va = {0};
            va.color = COL_HIST_VRAM;
            va.y_min = HIST_Y_LO; va.y_max = HIST_Y_HI; va.show_axes = 1;
            flux_area_chart(&rsb, a->vram_hist[i], a->hist_len[i], half, UI_HISTORY_HEIGHT, &va);

            const char *panels[2] = { flux_sb_str(&lsb), flux_sb_str(&rsb) };
            const int   widths[2] = { half, half };
            flux_hbox(&sb, panels, widths, 2, HIST_GAP);
            flux_sb_nl(&sb);
        }

        if (g->module_count > 0) {
            if (a->show_modules) {
                flux_sb_appendf(&sb, "%s%s%s\n", COL_TEXT2, S_TITLE_MODULES, COL_TEXT);
                // Compact colour-coded grid: "m0 48°C  m1 50°C  ..." to width.
                int per_row = w / MODULE_CELL_W;
                if (per_row < 1) per_row = 1;
                for (int mo = 0; mo < g->module_count; mo++) {
                    const char *col = temp_color(g->module_temp[mo]);
                    flux_sb_appendf(&sb, "%s" S_MODULE_PREFIX "%d %s%2d%s%s  ",
                        COL_DIM, mo, col, g->module_temp[mo], S_DEG_C, COL_TEXT);
                    if ((mo + 1) % per_row == 0) flux_sb_nl(&sb);
                }
                if (g->module_count % per_row != 0) flux_sb_nl(&sb);
            } else {
                // Collapsed: one-line hint so the layout below stays put.
                flux_sb_appendf(&sb, "%s%s %s(%d modules · press %s to show)%s\n",
                    COL_TEXT2, S_TITLE_MODULES, COL_DIM,
                    g->module_count, S_KEY_MODULES, COL_TEXT);
            }
            flux_sb_nl(&sb);
        }
        if (a->gpu.count > 1) { flux_divider(&sb, w, NULL); flux_sb_nl(&sb); }
    }

    FluxKeyHint h[] = {{S_KEY_MODULES, S_HINT_MODULES}, {S_KEY_QUIT, S_HINT_QUIT}};
    flux_footer(&sb, h, 2, w);
}

static int run_tui(App *a)
{
    FluxProgram p = {
        .model = { .state = a, .init = tui_init, .update = tui_update, .view = tui_view },
        .alt_screen = 1, .fps = UI_FPS,
    };
    flux_run(&p);
    return 0;
}

// ── ASCII mode ──────────────────────────────────────────────────────────────
static void print_ascii(App *a)
{
    for (int i = 0; i < a->gpu.count; i++) {
        gpuwatch_gpu *g = &a->gpu.gpus[i];
        printf(S_FMT_ASCII_HEAD, i, g->name, g->bus, g->dev, g->func, g->arch, g->vram);
        fputs(S_FMT_ASCII_CORE, stdout);
        if (g->core_valid) printf(S_FMT_TEMP_C, g->core_temp); else fputs(S_LBL_NA, stdout);
        fputs(S_FMT_ASCII_VRAM, stdout);
        if (g->vram_valid) printf(S_FMT_TEMP_C, g->vram_temp); else fputs(S_LBL_NA, stdout);
        if (g->module_count > 0) {
            fputs(S_FMT_ASCII_MOD_OPEN, stdout);
            for (int mo = 0; mo < g->module_count; mo++)
                printf("%s%d", mo ? S_SEP_COMMA : "", g->module_temp[mo]);
            fputs(S_FMT_ASCII_MOD_CLOSE, stdout);
        }
        fputc('\n', stdout);
    }
}

// ── JSON mode ───────────────────────────────────────────────────────────────
static void print_json(App *a)
{
    printf("{\"" J_GPUS "\":[");
    for (int i = 0; i < a->gpu.count; i++) {
        gpuwatch_gpu *g = &a->gpu.gpus[i];
        printf("%s{", i ? S_SEP_COMMA : "");
        printf("\"" J_INDEX "\":%d,", i);
        printf("\"" J_NAME "\":\"%s\",", g->name);
        printf("\"" J_ARCH "\":\"%s\",", g->arch);
        printf("\"" J_VRAM "\":\"%s\",", g->vram);
        printf("\"" J_PCI "\":\"" S_FMT_PCI "\",", g->bus, g->dev, g->func);
        if (g->core_valid) printf("\"" J_CORE_C "\":%d,", g->core_temp);
        else               printf("\"" J_CORE_C "\":" J_NULL ",");
        if (g->vram_valid) printf("\"" J_VRAM_C "\":%d,", g->vram_temp);
        else               printf("\"" J_VRAM_C "\":" J_NULL ",");
        printf("\"" J_MODULES_C "\":[");
        for (int mo = 0; mo < g->module_count; mo++)
            printf("%s%d", mo ? S_SEP_COMMA : "", g->module_temp[mo]);
        printf("]}");
    }
    printf("]}\n");
}

// ── main ────────────────────────────────────────────────────────────────────
static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static void usage(const char *argv0) { printf(S_USAGE, argv0); }

int main(int argc, char **argv)
{
    enum { MODE_TUI, MODE_ASCII, MODE_JSON } mode = MODE_TUI;
    int interval = CLI_INTERVAL_DEFAULT, once = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], S_ARG_ASCII)) mode = MODE_ASCII;
        else if (!strcmp(argv[i], S_ARG_JSON))  mode = MODE_JSON;
        else if (!strcmp(argv[i], S_ARG_ONCE))  once = 1;
        else if (!strcmp(argv[i], S_ARG_INTERVAL) && i + 1 < argc) {
            interval = atoi(argv[++i]); if (interval < 1) interval = 1;
        }
        else if (!strcmp(argv[i], S_ARG_HELP_LONG) || !strcmp(argv[i], S_ARG_HELP_SHORT)) {
            usage(argv[0]); return 0;
        }
        else { fprintf(stderr, S_ERR_UNKNOWN_OPT, argv[i]); usage(argv[0]); return 2; }
    }

    static App app;
    int rc = gpuwatch_init(&app.gpu);
    if (rc < 0) {
        fprintf(stderr, S_ERR_PREFIX "%s\n", gpuwatch_strerror(rc));
        return 1;
    }
    if (app.gpu.count == 0 && mode != MODE_TUI) {
        fprintf(stderr, S_ERR_PREFIX S_ERR_NO_GPU "\n");
        gpuwatch_shutdown(&app.gpu);
        return 1;
    }

    int ret = 0;
    if (mode == MODE_TUI) {
        ret = run_tui(&app);
    } else {
        signal(SIGINT, on_sig);
        signal(SIGTERM, on_sig);
        do {
            gpuwatch_sample(&app.gpu);
            if (mode == MODE_ASCII) print_ascii(&app);
            else                    print_json(&app);
            fflush(stdout);
            if (once || g_stop) break;
            sleep((unsigned)interval);
        } while (!g_stop);
    }

    gpuwatch_shutdown(&app.gpu);
    return ret;
}
