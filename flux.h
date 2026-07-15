#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
/*
 * flux.h -- Ink-style cell-based TUI framework for C99
 * Single-header, zero-dependency (libc + pthreads only).
 *
 * USAGE (define in exactly ONE translation unit):
 *   #define FLUX_IMPL
 *   #include "flux.h"
 *
 * Copyright (c) 2026 xuw (olealgoritme)
 * LICENSE: MIT
 */
#ifndef FLUX_H
#define FLUX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ══════════════════════════════════════════════════════════════════
 * CONFIGURATION
 * ═════════════════════════════════════════════════════════════════ */

#define FLUX_MAX_NODES     512
#define FLUX_MAX_CHILDREN  64
#define FLUX_STYLE_POOL    256
#define FLUX_MAX_LINES     512
#define FLUX_RENDER_BUF    131072
#define FLUX_PATCH_BUF     (FLUX_RENDER_BUF * 2)
#define FLUX_INPUT_MAX     4095
#define FLUX_PASTE_MAX     16384
#define FLUX_REGION_MAX    8

/* ══════════════════════════════════════════════════════════════════
 * ANSI HELPER MACROS
 * ═════════════════════════════════════════════════════════════════ */

#ifndef FLUX_RESET
#define FLUX_RESET     "\x1b[0m"
#endif
#define FLUX_BOLD      "\x1b[1m"
#define FLUX_DIM       "\x1b[2m"
#define FLUX_ITALIC    "\x1b[3m"
#define FLUX_UNDERLINE "\x1b[4m"
#define FLUX_STRIKE    "\x1b[9m"
#define FLUX_FG(r,g,b) "\x1b[38;2;" #r ";" #g ";" #b "m"
#define FLUX_BG(r,g,b) "\x1b[48;2;" #r ";" #g ";" #b "m"

/* ══════════════════════════════════════════════════════════════════
 * MESSAGES
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    MSG_NONE = 0,
    MSG_KEY,
    MSG_WINSIZE,
    MSG_QUIT,
    MSG_TICK,
    MSG_CUSTOM,
    MSG_PASTE,
    MSG_MOUSE,
} FluxMsgType;

typedef enum {
    FLUX_MOUSE_PRESS = 0,
    FLUX_MOUSE_RELEASE,
    FLUX_MOUSE_MOVE,
    FLUX_MOUSE_WHEEL_UP,
    FLUX_MOUSE_WHEEL_DOWN,
} FluxMouseEvent;

typedef struct {
    FluxMsgType type;
    union {
        struct { char key[16]; int rune; int ctrl; int alt; } key;
        struct { int cols, rows; }                            winsize;
        struct { int id; void *data; }                        custom;
        struct { char text[FLUX_PASTE_MAX]; int len; }        paste;
        struct {
            FluxMouseEvent event;
            int x, y;          /* 1-based cell coordinates */
            int button;        /* 0=left, 1=middle, 2=right (for press/release) */
            int shift, alt, ctrl;
        } mouse;
    } u;
} FluxMsg;

/* ══════════════════════════════════════════════════════════════════
 * COMMANDS
 * ═════════════════════════════════════════════════════════════════ */

typedef FluxMsg (*FluxCmdFn)(void *ctx);
typedef struct { FluxCmdFn fn; void *ctx; } FluxCmd;
#define FLUX_CMD_NONE ((FluxCmd){NULL,NULL})

/* ══════════════════════════════════════════════════════════════════
 * STYLE
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    int fg_r, fg_g, fg_b;    /* -1 = default/unset */
    int bg_r, bg_g, bg_b;    /* -1 = default/unset */
    uint8_t bold;
    uint8_t dim;
    uint8_t italic;
    uint8_t underline;
    uint8_t strikethrough;
    uint8_t inverse;
} FluxStyle;

#define FLUX_STYLE_NONE ((FluxStyle){-1,-1,-1,-1,-1,-1,0,0,0,0,0,0})

typedef struct {
    FluxStyle styles[FLUX_STYLE_POOL];
    int       count;
} FluxStylePool;

/* ══════════════════════════════════════════════════════════════════
 * CELL
 * ═════════════════════════════════════════════════════════════════ */

/* Cell byte buffer must hold one complete grapheme cluster — ZWJ
 * family / profession / flag-tag sequences routinely run 18-25 bytes
 * (👨‍👩‍👧 = 18, 🏃‍♀️‍➡️ = 18, family-of-five = 25). 8 bytes was enough
 * for a single 4-byte SMP emoji but split clusters across cells, which
 * caused the cell-diff to bleed and the chrome to drift on every
 * skin-toned / family / flag emoji. 32 leaves headroom for the longest
 * common ZWJ sequence + a flag-tag suffix. Issue #351. */
#define FLUX_CELL_CH_MAX 32

typedef struct {
    char    ch[FLUX_CELL_CH_MAX];
    int16_t style_id;
    int8_t  width;
} FluxCell;

/* ══════════════════════════════════════════════════════════════════
 * SCREEN BUFFER
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    FluxCell *cells;
    int       rows;
    int       cols;
    int       damage_x1;
    int       damage_y1;
    int       damage_x2;
    int       damage_y2;
} FluxScreen;

/* ══════════════════════════════════════════════════════════════════
 * NODE TYPES
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    FLUX_NODE_BOX,
    FLUX_NODE_TEXT,
    FLUX_NODE_RAW,
} FluxNodeType;

typedef enum {
    FLUX_DIR_VERTICAL,
    FLUX_DIR_HORIZONTAL,
} FluxDirection;

typedef enum {
    FLUX_SIZE_AUTO,
    FLUX_SIZE_FIXED,
    FLUX_SIZE_FILL,
    FLUX_SIZE_PERCENT,
} FluxSizeMode;

typedef struct {
    FluxSizeMode mode;
    int          value;
} FluxSize;

/* ══════════════════════════════════════════════════════════════════
 * NODE
 * ═════════════════════════════════════════════════════════════════ */

typedef struct FluxNode FluxNode;
struct FluxNode {
    FluxNodeType type;
    int          id;

    FluxStyle    style;
    int          padding[4];

    FluxDirection direction;
    FluxSize     width;
    FluxSize     height;

    int          cx, cy;
    int          cw, ch;

    char        *text;
    int          text_len;
    int          wrap;

    FluxNode    *children[FLUX_MAX_CHILDREN];
    int          child_count;
    FluxNode    *parent;

    int          dirty;
    int          layout_dirty;

    int          prev_cx, prev_cy, prev_cw, prev_ch;
};

/* ══════════════════════════════════════════════════════════════════
 * NODE POOL
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    FluxNode  nodes[FLUX_MAX_NODES];
    int       count;
    int       next_id;
} FluxNodePool;

/* ══════════════════════════════════════════════════════════════════
 * DIFF PATCH
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    FLUX_PATCH_MOVE,
    FLUX_PATCH_STYLE,
    FLUX_PATCH_WRITE,
    FLUX_PATCH_ERASE,
} FluxPatchType;

typedef struct {
    FluxPatchType type;
    union {
        struct { int x, y; }          move;
        struct { int style_id; }      style;
        struct { char text[256]; }    write;
        struct { int count; }         erase;
    } u;
} FluxPatch;

/* ══════════════════════════════════════════════════════════════════
 * STRING BUILDER
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    char *buf;
    int   len;
    int   cap;
} FluxSB;

/* ══════════════════════════════════════════════════════════════════
 * VIEWPORT REGIONS
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    FLUX_REGION_FIXED,
    FLUX_REGION_FILL,
} FluxRegionType;

typedef void (*FluxRegionRenderFn)(FluxSB *sb, int width, int height, void *ctx);

typedef struct {
    FluxRegionType type;
    int height;
    FluxRegionRenderFn render;
    void *ctx;
} FluxRegion;

typedef struct {
    FluxRegion regions[FLUX_REGION_MAX];
    int count;
    int width;
    int total_height;
} FluxViewport;

/* ══════════════════════════════════════════════════════════════════
 * WIDGETS
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    const char **frames;
    int frame_count, current;
    const char *label;
    /* Optional rate gating. period_ms == 0 → advance every tick
     * (legacy behaviour). When > 0, flux_spinner_tick_at advances
     * only when (now_ms - last_advance_ms) >= period_ms. Set via
     * flux_spinner_set_fps so callers don't have to do the FPS↔ms
     * arithmetic at the call site. Lets the spinner glyph share
     * the same Hz as a sibling shimmer/animation, so the two read
     * as one synchronized animation rather than two competing
     * ones. */
    int      period_ms;
    uint64_t last_advance_ms;
} FluxSpinner;

typedef struct {
    char buf[FLUX_INPUT_MAX + 1];
    int  cursor, len;
    const char *placeholder;
    int  submitted;
} FluxInput;

typedef struct {
    const char **icons, **labels;
    int count, active;
} FluxTabs;

/* ── Border ─────────────────────────────────────────────────── */

typedef struct {
    const char *tl, *tr, *bl, *br;
    const char *h,  *v;
    const char *ml, *mr, *mt, *mb, *c;
} FluxBorder;

/* ── Confirm dialog ─────────────────────────────────────────── */

typedef struct {
    const char *title, *message;
    const char *yes_label, *no_label;
    int selected;
    int answered;
    int result;
} FluxConfirm;

/* ── List widget ────────────────────────────────────────────── */

typedef void (*FluxListItemFn)(FluxSB *sb, int index, int selected,
                               int width, void *ctx);
typedef struct {
    int cursor, scroll, count, visible;
    FluxListItemFn render_item;
} FluxList;

/* ── Table widget ───────────────────────────────────────────── */

typedef void (*FluxTableCellFn)(FluxSB *sb, int row, int col,
                                int width, void *ctx);
typedef struct {
    const char **headers;
    const int  *widths;
    int col_count, row_count, visible, scroll, follow;
    FluxTableCellFn render_cell;
} FluxTable;

/* ── Kanban widget ──────────────────────────────────────────── */

#define FLUX_KB_TITLE_MAX  40
#define FLUX_KB_DESC_MAX   80
#define FLUX_KB_MAX_CARDS  32
#define FLUX_KB_MAX_COLS   8
#define FLUX_KB_BROWSE     0
#define FLUX_KB_EDIT       1
#define FLUX_KB_F_TITLE    0
#define FLUX_KB_F_DESC     1
#define FLUX_KB_F_OK       2
#define FLUX_KB_F_CANCEL   3

typedef struct {
    char title[FLUX_KB_TITLE_MAX + 1];
    char desc[FLUX_KB_DESC_MAX + 1];
} FluxKbCard;

typedef struct {
    char name[32];
    FluxKbCard cards[FLUX_KB_MAX_CARDS];
    int count;
} FluxKbCol;

typedef struct {
    FluxKbCol cols[FLUX_KB_MAX_COLS];
    int ncols, cur_col, cur_row, col_width, visible;
    int scroll[FLUX_KB_MAX_COLS];
    int mode, edit_col, edit_row, edit_focus, grabbed;
    FluxInput edit_title, edit_desc;
    int dirty;
} FluxKanban;

/* ══════════════════════════════════════════════════════════════════
 * MODEL / PROGRAM
 * ═════════════════════════════════════════════════════════════════ */

typedef struct FluxModel FluxModel;
struct FluxModel {
    void  *state;
    FluxCmd (*init)  (FluxModel *m);
    FluxCmd (*update)(FluxModel *m, FluxMsg msg);

    /* Legacy (line-diff) view path. Writes a full ANSI string into
     * `buf`. flux_run splits on '\n' and emits whole lines that
     * differ from the previous frame. Coarse but simple. */
    void  (*view)  (FluxModel *m, char *buf, int bufsz);

    /* Cell-diff view path. Build a FluxNode tree rooted at the
     * returned pointer; flux_run lays it out, renders into a cell
     * buffer, diffs against the previous frame's cells, and emits
     * cell-level ANSI deltas (cursor moves + style changes + writes)
     * to stdout. The pool is owned by flux_run and is reset between
     * frames; the returned tree is valid only until the next call.
     * Return NULL to render an empty screen.
     *
     * If view_tree is non-NULL, flux_run uses this path and ignores
     * `view`. Mutually exclusive in practice. */
    FluxNode *(*view_tree)(FluxModel *m, FluxNodePool *pool,
                           int width, int height);

    void  (*free)  (FluxModel *m);
};

typedef struct {
    FluxModel model;
    int alt_screen;
    int mouse;
    int fps;
    /* Enable terminal focus-event reporting (CSI ? 1004 h/l). When 1
     * AND alt_screen is 1, flux emits \x1b[?1004h on setup and
     * \x1b[?1004l on teardown, and parses \x1b[I (focus-in) and
     * \x1b[O (focus-out) from stdin to drive flux_tty_focused. Apps
     * (and widgets that paint a software cursor) can read
     * flux_tty_focused to hide the cursor when the tty loses focus —
     * useful in tmux split panes where every pane otherwise shows a
     * cursor. tmux users need `set -g focus-events on` in tmux.conf
     * for the events to pass through. Default 0 (off) for backward
     * compatibility. */
    int focus_events;
} FluxProgram;

/* ══════════════════════════════════════════════════════════════════
 * RENDERER STATE (internal)
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    FluxScreen    front;
    FluxScreen    back;
    FluxStylePool styles;
    FluxNodePool  pool;
    FluxNode     *root;
    int           force;
    char          obuf[FLUX_PATCH_BUF];
    int           olen;
} FluxRenderer;

/* ══════════════════════════════════════════════════════════════════
 * EXTERN DECLARATIONS: Spinner frame arrays & border presets
 * ═════════════════════════════════════════════════════════════════ */

#ifdef __GNUC__
#define FLUX_UNUSED_ __attribute__((unused))
#else
#define FLUX_UNUSED_
#endif

extern FLUX_UNUSED_ const char *FLUX_SPINNER_DOT[];
extern FLUX_UNUSED_ const int   FLUX_SPINNER_DOT_N;
extern FLUX_UNUSED_ const char *FLUX_SPINNER_LINE[];
extern FLUX_UNUSED_ const int   FLUX_SPINNER_LINE_N;

extern const FluxBorder FLUX_BORDER_ROUNDED;
extern const FluxBorder FLUX_BORDER_SHARP;
extern const FluxBorder FLUX_BORDER_DOUBLE;
extern const FluxBorder FLUX_BORDER_THICK;
extern const FluxBorder FLUX_BORDER_NONE;

/* ══════════════════════════════════════════════════════════════════
 * TICK MACRO
 * ═════════════════════════════════════════════════════════════════ */

#define FLUX_TICK(ms)  flux_tick(ms)
#define FLUX_CMD_QUIT  flux_cmd_quit()

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Event loop + Terminal queries
 * ═════════════════════════════════════════════════════════════════ */

void    flux_run(FluxProgram *p);
int     flux_key_is(FluxMsg msg, const char *name);
FluxCmd flux_tick(int ms);
FluxCmd flux_cmd_quit(void);
FluxCmd flux_cmd_custom(int id, void *data);
int     flux_cols(void);
int     flux_rows(void);

/* THREAD-SAFE: post a MSG_CUSTOM into the runloop from any thread.
 * Returns 0 on success, -1 if the loop isn't running. The custom message
 * is delivered to update() as MSG_CUSTOM with the given id + data on the
 * next loop iteration. POSIX guarantees pipe writes < PIPE_BUF are atomic,
 * so this is safe to call from worker threads, signal handlers (data must
 * already be allocated), or anywhere else. */
int     flux_post_custom(int id, void *data);

/* Request that the next render bypass cell-diff and force-emit every
 * line. Use when an overlay rect transitions from visible to hidden
 * and the underlying line content needs to be repainted (the line-diff
 * skip would otherwise leave the overlay's "blanked" cells visible). */
void    flux_request_force_redraw(void);

/* Returns 1 if the controlling tty currently has focus, 0 if it does
 * not. Driven by terminal CSI ? 1004 focus-event reporting; only
 * meaningful when FluxProgram.focus_events was set before flux_run.
 * Defaults to 1 — terminals that do not support focus events will
 * never emit a focus-out, so focused-by-default is the safe assumption.
 * Widgets that paint a software cursor (e.g. cawd's bordered input
 * box) can gate the cursor block on this so the cursor disappears
 * when the user switches to another tmux pane / terminal window. */
int     flux_tty_focused(void);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: String builder
 * ═════════════════════════════════════════════════════════════════ */

void        flux_sb_init(FluxSB *sb, char *backing, int cap);
void        flux_sb_append(FluxSB *sb, const char *s);
void        flux_sb_appendf(FluxSB *sb, const char *fmt, ...);
void        flux_sb_nl(FluxSB *sb);
const char *flux_sb_str(FluxSB *sb);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Text utilities
 * ═════════════════════════════════════════════════════════════════ */

int   flux_strwidth(const char *s);
int   flux_count_lines(const char *s);
int   flux_count_text_lines(const char *s, int len);
int   flux_wrapped_height(const char *text, int text_len,
                                 int wrap_width);
void  flux_pad_lines(char *buf, int bufsz, int target_lines);
void  flux_pad(char *dst, int dstsz, const char *src, int width);
char *flux_fg(char *buf, int r, int g, int b);
char *flux_bg(char *buf, int r, int g, int b);

/* ── Responsive text-fitting primitives (100% safe) ─────────────────
 *
 * flux_truncate: copy up to max_w DISPLAY CELLS of `text` into `out`.
 *   If text is wider than max_w, append `suffix` (default "…" if NULL).
 *   ANSI-aware (CSI/OSC escapes are copied through but don't count).
 *   UTF-8 wide-char aware (CJK / emoji count as 2 cells).
 *   Never splits a UTF-8 codepoint or ANSI escape.
 *   Always NUL-terminates `out`. Returns the visible width written.
 *
 * flux_fit: emit EXACTLY target_w display cells into `sb`. Truncates
 *   (with suffix) if too wide, pads with spaces if too narrow. No newline.
 *   `align` is one of FLUX_ALIGN_*.
 *
 * With these, any widget can accept arbitrary user text and still honor
 * its "N cells per row" contract — the end user cannot overflow. */

typedef enum {
    FLUX_ALIGN_LEFT = 0,
    FLUX_ALIGN_RIGHT,
    FLUX_ALIGN_CENTER,
} FluxAlign;

int  flux_truncate(const char *text, int max_w,
                   const char *suffix,
                   char *out, int out_cap);
void flux_fit(FluxSB *sb, const char *text, int target_w,
              const char *suffix, FluxAlign align);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Layout helpers
 * ═════════════════════════════════════════════════════════════════ */

void flux_divider(FluxSB *sb, int width, const char *color);
void flux_hbox(FluxSB *sb, const char **panels, const int *widths,
                      int count, const char *gap);
void flux_box(char *out_buf, int out_sz, const char *content,
                     const FluxBorder *border, int inner_w,
                     const char *border_clr, const char *content_bg);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Node tree (new cell-based renderer)
 * ═════════════════════════════════════════════════════════════════ */

void      flux_pool_init(FluxNodePool *pool);
FluxNode *flux_node_alloc(FluxNodePool *pool, FluxNodeType type);
void      flux_node_free(FluxNodePool *pool, FluxNode *node);
void      flux_node_add_child(FluxNode *parent, FluxNode *child);
void      flux_node_remove_child(FluxNode *parent, FluxNode *child);
void      flux_node_set_text(FluxNode *node, const char *text);
void      flux_node_set_style(FluxNode *node, FluxStyle style);
void      flux_node_set_size(FluxNode *node, FluxSize w, FluxSize h);
void      flux_node_mark_dirty(FluxNode *node);
void      flux_node_mark_layout_dirty(FluxNode *node);
void      flux_node_clear_dirty(FluxNode *node);
int       flux_node_is_clean(const FluxNode *node);
FluxNode *flux_node_box(FluxNodePool *pool, FluxDirection dir);
FluxNode *flux_node_text(FluxNodePool *pool, const char *text,
                                FluxStyle style);
FluxNode *flux_node_raw(FluxNodePool *pool, const char *ansi_str);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Screen buffer
 * ═════════════════════════════════════════════════════════════════ */

int  flux_screen_init(FluxScreen *scr, int rows, int cols);
void flux_screen_free(FluxScreen *scr);
int  flux_screen_resize(FluxScreen *scr, int rows, int cols);
void flux_screen_clear(FluxScreen *scr);
void flux_screen_set_cell(FluxScreen *scr, int x, int y,
                                 const char *ch, int ch_len,
                                 int style_id, int width);
const FluxCell *flux_screen_get_cell(const FluxScreen *scr,
                                            int x, int y);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Style pool
 * ═════════════════════════════════════════════════════════════════ */

void flux_style_pool_init(FluxStylePool *pool);
int  flux_style_pool_intern(FluxStylePool *pool, const FluxStyle *s);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Layout engine
 * ═════════════════════════════════════════════════════════════════ */

void flux_layout_compute(FluxNode *node, int avail_w, int avail_h);
void flux_layout_resolve_absolute(FluxNode *root,
                                         int screen_w, int screen_h);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Renderer
 * ═════════════════════════════════════════════════════════════════ */

void flux_render_tree(FluxScreen *scr, FluxScreen *prev,
                             FluxStylePool *pool, FluxNode *root);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Diff engine
 * ═════════════════════════════════════════════════════════════════ */

int flux_diff_screens(const FluxScreen *prev, const FluxScreen *next,
                             const FluxStylePool *pool,
                             char *obuf, int obufsz);
int flux_diff_full(const FluxScreen *next, const FluxStylePool *pool,
                          char *obuf, int obufsz);
int flux_diff_has_changes(const FluxScreen *prev,
                                 const FluxScreen *next);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Input
 * ═════════════════════════════════════════════════════════════════ */

void flux_input_init(FluxInput *inp, const char *placeholder);
void flux_input_clear(FluxInput *inp);
int  flux_input_update(FluxInput *inp, FluxMsg msg);
void flux_input_render(FluxInput *inp, FluxSB *sb, int width,
                              const char *color, const char *cursor_clr);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Spinner
 * ═════════════════════════════════════════════════════════════════ */

void flux_spinner_init(FluxSpinner *s, const char **frames,
                              int nframes, const char *label);
void flux_spinner_tick(FluxSpinner *s);
void flux_spinner_render(FluxSpinner *s, FluxSB *sb,
                                const char *color);
/* Optional Hz throttle — see FluxSpinner doc above. */
void flux_spinner_set_fps(FluxSpinner *s, int fps);
void flux_spinner_tick_at(FluxSpinner *s, uint64_t now_ms);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Tabs
 * ═════════════════════════════════════════════════════════════════ */

void flux_tabs_init(FluxTabs *t, const char **icons,
                           const char **labels, int count);
int  flux_tabs_update(FluxTabs *t, FluxMsg msg);
void flux_tabs_render(FluxTabs *t, FluxSB *sb,
                             const char *active_clr,
                             const char *dim_clr,
                             const char *hint);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Confirm
 * ═════════════════════════════════════════════════════════════════ */

void flux_confirm_init(FluxConfirm *c, const char *title,
                              const char *msg, const char *yes_label,
                              const char *no_label);
int  flux_confirm_update(FluxConfirm *c, FluxMsg msg);
void flux_confirm_render(FluxConfirm *c, FluxSB *sb, int width,
                                const char *border_clr,
                                const char *yes_clr,
                                const char *no_clr);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — List
 * ═════════════════════════════════════════════════════════════════ */

void flux_list_init(FluxList *l, int count, int visible,
                           FluxListItemFn render);
int  flux_list_update(FluxList *l, FluxMsg msg);
void flux_list_render(FluxList *l, FluxSB *sb, int width,
                             void *ctx);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Table
 * ═════════════════════════════════════════════════════════════════ */

void flux_table_init(FluxTable *t, const char **headers,
                            const int *widths, int cols, int visible,
                            FluxTableCellFn render);
void flux_table_set_rows(FluxTable *t, int rows);
int  flux_table_update(FluxTable *t, FluxMsg msg);
void flux_table_render(FluxTable *t, FluxSB *sb, void *ctx,
                               const char *header_clr,
                               const char *border_clr);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Viewport
 * ═════════════════════════════════════════════════════════════════ */

void flux_viewport_init(FluxViewport *vp, int width, int height);
void flux_viewport_add(FluxViewport *vp, FluxRegionType type,
                              int height, FluxRegionRenderFn render,
                              void *ctx);
void flux_viewport_render(FluxViewport *vp, char *buf, int bufsz);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Popup, Sparkline, Bar
 * ═════════════════════════════════════════════════════════════════ */

void flux_popup(FluxSB *sb, int area_w, int area_h,
                       const char *title, const char **items, int count,
                       int selected, const char *hint,
                       const char *border_clr, const char *select_bg,
                       const char *normal_bg, const char *accent_clr);
void flux_sparkline(FluxSB *sb, float *ring, int len, int head,
                           const char *color);
void flux_bar(FluxSB *sb, float value, int width,
                     const char *fill_clr, const char *empty_clr);

/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Kanban
 * ═════════════════════════════════════════════════════════════════ */

void flux_kanban_init(FluxKanban *kb, int ncols,
                             const char **col_names, int col_width);
int  flux_kanban_add(FluxKanban *kb, int col,
                            const char *title, const char *desc);
int  flux_kanban_del(FluxKanban *kb, int col, int row);
int  flux_kanban_move(FluxKanban *kb, int col, int row,
                             int to_col);
int  flux_kanban_dirty(FluxKanban *kb);
int  flux_kanban_update(FluxKanban *kb, FluxMsg msg);
void flux_kanban_render(FluxKanban *kb, FluxSB *sb,
                               const char **col_colors,
                               const char *hint);

/* ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════
 * AGENT UI WIDGETS (storm-style) — added by the parallel build swarm
 * ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════ */


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Theme — Storm/Tokyonight palette
 * ═════════════════════════════════════════════════════════════════ */

/* ── Surfaces (use as bg escapes) ────────────────────────────────────── */
#define FLUX_THEME_WINDOW_BG       "\x1b[48;2;0;0;0m"        /* pure black — outer margin / chrome ring */
#define FLUX_THEME_HEADER_BG       "\x1b[48;2;0;0;0m"        /* pure black — header band rows */
#define FLUX_THEME_INNER_BG        "\x1b[48;2;22;27;34m"     /* #161b22 — defined for legacy callers; not painted on content rows */
#define FLUX_THEME_TITLEBAR_BG     "\x1b[48;2;14;14;14m"     /* #0e0e0e */
#define FLUX_THEME_PANEL_BG        "\x1b[48;2;26;27;38m"     /* #1a1b26 */
#define FLUX_THEME_CODE_BG         "\x1b[48;2;26;27;38m"     /* same */
#define FLUX_THEME_BUTTON_OK_BG    "\x1b[48;2;39;45;45m"     /* #272d2d */
#define FLUX_THEME_BUTTON_NO_BG    "\x1b[48;2;48;36;48m"     /* #302430 */

/* ── Border / dividers (fg escapes used on panel) ───────────────────── */
#define FLUX_THEME_BORDER_FG       "\x1b[38;2;48;54;61m"     /* #30363d */
#define FLUX_THEME_BORDER_WARN_FG  "\x1b[38;2;66;57;51m"     /* #423933 */
#define FLUX_THEME_DIVIDER_FG      "\x1b[38;2;38;42;52m"     /* #262a34 */

/* ── Text ladder ────────────────────────────────────────────────────── */
#define FLUX_THEME_TEXT_FG         "\x1b[38;2;192;202;245m"  /* #c0caf5 */
#define FLUX_THEME_TEXT2_FG        "\x1b[38;2;156;164;199m"  /* #9ca4c7 */
#define FLUX_THEME_TEXT_DIM_FG     "\x1b[38;2;86;95;125m"    /* #565f7d */
#define FLUX_THEME_TEXT_OFF_FG     "\x1b[38;2;60;64;93m"     /* #3c405d */
#define FLUX_THEME_TEXT_INV_FG     "\x1b[38;2;14;14;14m"     /* #0e0e0e */

/* ── Brand / accent ─────────────────────────────────────────────────── */
#define FLUX_THEME_ACCENT_FG       "\x1b[38;2;174;198;255m"  /* #aec6ff */
#define FLUX_THEME_ACCENT_DIM_FG   "\x1b[38;2;71;78;113m"    /* #474e71 */
#define FLUX_THEME_BRAND_PURPLE_FG "\x1b[38;2;180;130;255m"  /* #b482ff */

/* ── Semantic ───────────────────────────────────────────────────────── */
#define FLUX_THEME_OK_FG           "\x1b[38;2;141;184;97m"   /* #8db861 */
#define FLUX_THEME_OK_DIM_FG       "\x1b[38;2;95;120;73m"    /* #5f7849 */
#define FLUX_THEME_WARN_FG         "\x1b[38;2;254;225;156m"  /* #fee19c */
#define FLUX_THEME_ERR_FG          "\x1b[38;2;204;101;121m"  /* #cc6579 */
#define FLUX_THEME_ERR_DIM_FG      "\x1b[38;2;137;73;90m"    /* #89495a */

/* ── Diff ───────────────────────────────────────────────────────────── */
#define FLUX_THEME_DIFF_ADD_FG     "\x1b[38;2;141;184;97m"   /* same as OK */
#define FLUX_THEME_DIFF_DEL_FG     "\x1b[38;2;200;99;119m"   /* #c86377 */

/* ── Window-control dots ────────────────────────────────────────────── */
#define FLUX_THEME_TRAFFIC_R_FG    "\x1b[38;2;255;95;87m"    /* #ff5f57 */
#define FLUX_THEME_TRAFFIC_Y_FG    "\x1b[38;2;254;188;46m"   /* #febc2e */
#define FLUX_THEME_TRAFFIC_G_FG    "\x1b[38;2;40;200;64m"    /* #28c840 */



/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Window chrome
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *border_fg;     /* default FLUX_THEME_BORDER_FG    */
    const char *bar_bg;        /* default FLUX_THEME_TITLEBAR_BG  */
    const char *panel_bg;      /* default FLUX_THEME_PANEL_BG     */
    const char *title_fg;      /* default FLUX_THEME_TEXT_DIM_FG  */
    const char *dot_close;     /* default FLUX_THEME_TRAFFIC_R_FG */
    const char *dot_min;       /* default FLUX_THEME_TRAFFIC_Y_FG */
    const char *dot_max;       /* default FLUX_THEME_TRAFFIC_G_FG */
    int         show_dots;     /* default 1                       */
    int         show_divider;  /* default 1                       */
} FluxChromeOpts;

/* Render a macOS-style rounded window chrome around `content`.
 *
 *   inner_w = content width INSIDE the vertical borders (no side padding).
 *   Each row of the output measures (inner_w + 2) cells.
 *   `content` is split on '\n'; each line is assumed to already measure
 *   exactly inner_w cells (use flux_pad if needed).
 *
 *   Output rows = 3 (top border + title bar + divider) + content_lines + 1.
 *   If opts->show_divider == 0 the divider row is skipped.
 */
void flux_window_chrome(char *out_buf, int out_sz,
                        const char *content, const char *title,
                        int inner_w, const FluxChromeOpts *opts /*nullable*/);



/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Activity row
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    FLUX_ACT_PENDING = 0,
    FLUX_ACT_RUNNING,
    FLUX_ACT_DONE,
    FLUX_ACT_FAILED
} FluxActivityStatus;

typedef struct {
    const char        *label;
    FluxActivityStatus status;
    int                duration_ms;   /* <=0 → no duration shown */
    int                spinner_frame; /* used when RUNNING       */
} FluxActivity;

/* Render one activity row, padded to `width` cells, terminated by '\n'.
 * `indent` = leading space columns (typically 2). */
void flux_activity_render(FluxSB *sb, const FluxActivity *a,
                          int width, int indent);

/* Walk an array; same width/indent contract as flux_activity_render. */
void flux_activity_list_render(FluxSB *sb,
                               const FluxActivity *items, int n,
                               int width, int indent);

/* Format "340ms" / "1.2s" / "1m 03s" / "2h05m". Returns chars written
 * (excluding NUL). Returns 0 if ms <= 0 or cap is too small. */
int  flux_activity_format_duration(int ms, char *out, int cap);



/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Diff block
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    FLUX_DIFF_CONTEXT = 0,   /* leading space — dim fg */
    FLUX_DIFF_REMOVED = 1,   /* '-' marker, red fg     */
    FLUX_DIFF_ADDED   = 2,   /* '+' marker, green fg   */
} FluxDiffKind;

typedef struct {
    FluxDiffKind kind;
    const char  *text;       /* line content, no leading marker, no '\n' */
} FluxDiffLine;

/* Render a code-diff card into `sb` as a sequence of rows.
 *
 *   row 1     filename header (dim, FLUX_THEME_TEXT_OFF_FG), padded to width
 *   row 2..N  one row per FluxDiffLine:
 *                "  " + colored_marker + " " + colored_text + space pad
 *
 * Each emitted row measures EXACTLY `width` cells and ends with '\n'.
 * NO border is drawn here — the caller wraps this widget in chrome / panel.
 * If `filename` is NULL the filename row is skipped.
 * All diff text is treated as ASCII for width math (uses strlen()).
 */
void flux_diff_block_render(FluxSB *sb,
                            const char *filename,
                            const FluxDiffLine *lines, int n,
                            int width);



/* Banner: PUBLIC API: Widgets — Button primitive */

typedef struct {
    const char *label;
    const char *fg_filled;
    const char *bg_filled;
    const char *fg_outline;
} FluxButton;

/* Render exactly one button: " label " padded with one space each side.
 * filled=1 → emit bg+fg+bold; filled=0 → emit fg only.
 * Width consumed = flux_strwidth(label) + 2.
 * No trailing newline. */
void flux_button_render(FluxSB *sb, const FluxButton *btn, int filled);



/* Banner: PUBLIC API: Widgets — Approval (filled + outlined buttons) */

typedef struct {
    const char *prompt;
    FluxButton *buttons;
    int         n;
    int         selected;
    int         answered;
    int         result;
    const char *prompt_fg;
    const char *border_fg;
    const char *panel_bg;
} FluxApproval;

void flux_approval_init(FluxApproval *a, const char *prompt,
                        FluxButton *buttons, int n);
int  flux_approval_update(FluxApproval *a, FluxMsg msg);
void flux_approval_render(FluxApproval *a, FluxSB *sb, int width);



/* Banner: PUBLIC API: Widgets — Status bar */

typedef struct {
    const char *brand;
    int         tokens;
    float       cost;
    float       progress;
    int         bar_cells;
} FluxStatusBar;

void flux_statusbar_render(FluxSB *sb, const FluxStatusBar *s, int total_width);
void flux_inline_bar(FluxSB *sb, float progress, int cells,
                     const char *fill_clr, const char *empty_clr);



/* Banner: PUBLIC API: Widgets — Brand label */

void flux_brand(FluxSB *sb,
                const char *icon, const char *icon_clr,
                const char *text, const char *text_clr);

/* ── FluxRate — per-widget FPS gating ───────────────────────────────
 *
 * The flux_run main loop ticks at one rate (e.g. 120 Hz). Animated
 * widgets often want to advance at slower rates (e.g. spinner = 10 Hz,
 * blinkdot = 2 Hz). FluxRate is a 12-byte struct that lets each widget
 * gate its own tick to a configurable interval, so the global loop stays
 * fast (snappy input + scroll) while individual animations breathe at
 * their own pace.
 *
 * Usage:
 *   FluxRate r;
 *   flux_rate_init(&r, 100);            // 10 Hz (period 100ms)
 *   if (msg.type == MSG_TICK) {
 *       uint64_t now = flux_now_ms();
 *       if (flux_rate_due(&r, now))     // 1 if interval elapsed
 *           flux_spinner_tick(&spin);   // advance only when due
 *   }
 *
 * Convenience: FLUX_FPS_TO_MS(fps) → period in ms.
 */
typedef struct {
    uint64_t last_ms;     /* monotonic ms of last "due" event (0 = never) */
    int      period_ms;   /* interval between ticks */
} FluxRate;

#define FLUX_FPS_TO_MS(fps) ((fps) > 0 ? (1000 / (fps)) : 0)

void     flux_rate_init(FluxRate *r, int period_ms);
void     flux_rate_set_fps(FluxRate *r, int fps);
int      flux_rate_due(FluxRate *r, uint64_t now_ms);
void     flux_rate_reset(FluxRate *r);
uint64_t flux_now_ms(void);   /* monotonic milliseconds since arbitrary epoch */

/* ── Aligned message row ────────────────────────────────────────────
 * Render one chat-style row: <indent><glyph in gutter><space><body>.
 * The glyph is padded to EXACTLY `gutter_w` display cells regardless of
 * its own width, so body text always starts at the same column even when
 * glyphs vary in cell width (e.g. 1-cell `>` vs 2-cell `⚙`). The whole
 * row is fitted to `width` cells via flux_fit and ends with '\n'. */
void flux_message_row(FluxSB *sb,
                      const char *glyph, const char *glyph_clr,
                      const char *body,  const char *body_clr,
                      int width, int indent, int gutter_w);

/* ── Scrollable viewport ────────────────────────────────────────────
 * Owns scroll offset; clips arbitrary newline-separated content to a
 * viewport_h window and emits exactly viewport_h rows of `width` cells.
 * Handles wheel/up/down/j/k/pgup/pgdn/home/end automatically. */

typedef struct {
    int scroll;          /* current row offset (clamped on render) */
    int total_lines;     /* set on render — full content line count */
    int viewport_h;      /* set on render — visible rows */
    int wheel_step;      /* default 2; rows per wheel tick */
    int page_step;       /* default 6; rows per pgup/pgdn */
} FluxScroll;

void flux_scroll_init(FluxScroll *s);
/* Render the visible window of `content` to `sb`. Pads with blank rows
 * if content is shorter than viewport_h. Updates total_lines + viewport_h
 * on the FluxScroll. Caller does NOT need to clamp scroll. */
void flux_scrollview_render(FluxScroll *s, FluxSB *sb,
                            const char *content,
                            int width, int viewport_h);
/* Render a one-row scroll bar + position indicator + key hint. */
void flux_scroll_indicator(const FluxScroll *s, FluxSB *sb, int width);
/* Process wheel and key events. Returns 1 if scroll changed. */
int  flux_scroll_update(FluxScroll *s, FluxMsg msg);

/* ── Clickable tab bar ──────────────────────────────────────────────
 * Owns tab labels + active index + hit-box cache. Handles 1-9 number
 * keys, Tab/Shift+Tab cycling, and mouse left-click hit-testing. */

#ifndef FLUX_TABBAR_MAX
#define FLUX_TABBAR_MAX 16
#endif

typedef struct {
    const char **labels;     /* caller-owned array, count entries */
    int          count;
    int          active;
    /* Internal hit-box cache (set by render, read by update) */
    int          x_start[FLUX_TABBAR_MAX];
    int          x_end  [FLUX_TABBAR_MAX];
    int          y;
} FluxTabBar;

void flux_tabbar_init(FluxTabBar *t, const char **labels, int n);
/* Render the tab strip to sb at the given screen-coord origin so click
 * hit-testing can resolve correctly. width = output cells per row. */
void flux_tabbar_render(FluxTabBar *t, FluxSB *sb, int width,
                        int screen_x_origin, int screen_y);
/* Process number keys, Tab, Shift+Tab, and left mouse click. */
int  flux_tabbar_update(FluxTabBar *t, FluxMsg msg);

/* ── Layout helper ──────────────────────────────────────────────────
 * Compute a viewport height that leaves chrome + desktop margin. */
int  flux_layout_viewport_h(int total_h, int chrome_rows, int margin_rows,
                            int min_h, int max_h);


/* ══════════════════════════════════════════════════════════════════
 * STORM-PARITY WIDGETS (v4 — added by parallel impl swarm)
 * ══════════════════════════════════════════════════════════════════ */


/* ─── B1_core_primitives ─────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Core primitives (B1)
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *fg;       /* ANSI fg escape, NULL → FLUX_THEME_TEXT_FG  */
    const char *bg;       /* ANSI bg escape, NULL → none                */
    unsigned    bold      : 1;
    unsigned    italic    : 1;
    unsigned    underline : 1;
    unsigned    strike    : 1;
    unsigned    dim       : 1;
    unsigned    inverse   : 1;
} FluxTextStyle;

void flux_newline  (FluxSB *sb, int n);
void flux_spacer   (FluxSB *sb, int width, int rows);
void flux_text     (FluxSB *sb, const char *text, const FluxTextStyle *style);
void flux_heading  (FluxSB *sb, int level, const char *text, int width);
void flux_paragraph(FluxSB *sb, const char *text, int width);
void flux_ol       (FluxSB *sb, const char **items, int n,
                    int width, int start_num);
void flux_ul       (FluxSB *sb, const char **items, int n,
                    int width, const char *glyph);
void flux_kbd      (FluxSB *sb, const char *label);

/* ─── B2_form_input ─────────────────────────────────────── */
/* ════════════════════════════════════════════════════════════════════
 *  flux.h — Batch B2: Form & Input widgets
 *  Generated per spec tasks/parity/plans/B2_form_input.md
 *
 *  Widgets:
 *    1. FluxCheckbox     — toggle
 *    2. FluxSwitch       — pill on/off
 *    3. FluxRadio        — single-select group
 *    4. FluxSelect       — collapsed value + popover
 *    5. FluxTextArea     — multi-line editor (UTF-8)
 *    6. FluxMaskedInput  — masked input with literals
 *    7. FluxSearchInput  — magnifier-prefix wrapper around FluxInput
 *    8. FluxChatInput    — multi-line composer + history
 *    9. FluxStepper      — numeric -/+
 *   10. FluxForm         — multi-field form with validation
 *
 *  All widgets follow the FluxInput / FluxConfirm convention:
 *      init() zeroes + defaults; update() returns 1 if visible state
 *      changed, 0 otherwise; render() emits exactly N rows of `width`
 *      cells (each `\n`-terminated) via flux_fit. NULL color pointers
 *      resolve to FLUX_THEME_* defaults at render time.
 * ════════════════════════════════════════════════════════════════════ */



#include <limits.h>

/* ── 1. FluxCheckbox ──────────────────────────────────────────────── */

typedef struct {
    int   checked;            /* 0 unchecked, 1 checked, 2 indeterminate */
    const char *label;
    int   disabled, focused;
    const char *box_fg;       /* default ACCENT_FG checked / TEXT_DIM_FG */
    const char *label_fg;     /* default TEXT_FG */
} FluxCheckbox;

void flux_checkbox_init(FluxCheckbox *cb, const char *label, int initial);
int  flux_checkbox_update(FluxCheckbox *cb, FluxMsg msg);
void flux_checkbox_render(FluxCheckbox *cb, FluxSB *sb, int width);

/* ── 2. FluxSwitch ────────────────────────────────────────────────── */

typedef struct {
    int   on;
    const char *label, *on_text, *off_text;
    int   disabled, focused;
    const char *on_bg;
    const char *off_bg;
    const char *label_fg;
} FluxSwitch;

void flux_switch_init(FluxSwitch *sw, const char *label, int initial);
int  flux_switch_update(FluxSwitch *sw, FluxMsg msg);
void flux_switch_render(FluxSwitch *sw, FluxSB *sb, int width);

/* ── 3. FluxRadio ─────────────────────────────────────────────────── */

typedef struct {
    const char **labels;
    int   count, selected, highlight;
    int   focused;
    int   disabled_mask;       /* bit i = option i disabled */
    const char *marker_fg;
    const char *label_fg;
    const char *dim_fg;
} FluxRadio;

void flux_radio_init(FluxRadio *r, const char **labels, int count, int initial);
int  flux_radio_update(FluxRadio *r, FluxMsg msg);
void flux_radio_render(FluxRadio *r, FluxSB *sb, int width);

/* ── 4. FluxSelect ────────────────────────────────────────────────── */

typedef struct {
    const char **labels;
    const char **values;       /* NULL → labels used as values */
    int   count, selected, highlight;
    int   open;
    int   max_visible;         /* default 6 */
    int   scroll;
    int   focused;
    const char *placeholder;
    const char *border_fg;
    const char *value_fg;
    const char *select_bg;
} FluxSelect;

void flux_select_init(FluxSelect *s, const char **labels, int count, int initial);
int  flux_select_update(FluxSelect *s, FluxMsg msg);
void flux_select_render(FluxSelect *s, FluxSB *sb, int width);
int  flux_select_height(const FluxSelect *s);

/* ── 5. FluxTextArea ──────────────────────────────────────────────── */

typedef struct {
    char  buf[FLUX_INPUT_MAX + 1];
    int   len, cursor;          /* byte offsets */
    int   scroll_row, visible_rows;
    int   tab_size;             /* default 2 */
    int   wrap;                 /* 1 = soft wrap */
    int   submitted;
    const char *placeholder, *fg, *cursor_fg, *placeholder_fg;
} FluxTextArea;

void flux_textarea_init(FluxTextArea *ta, const char *placeholder);
void flux_textarea_clear(FluxTextArea *ta);
int  flux_textarea_update(FluxTextArea *ta, FluxMsg msg);
void flux_textarea_render(FluxTextArea *ta, FluxSB *sb, int width, int rows);

/* ── 6. FluxMaskedInput ───────────────────────────────────────────── */

typedef struct {
    char  raw[FLUX_INPUT_MAX + 1];   /* user-typed slots only */
    int   raw_len, raw_cursor;
    const char *mask;
    int   submitted, focused;
    const char *fg, *literal_fg, *placeholder_fg;
} FluxMaskedInput;

void        flux_masked_init(FluxMaskedInput *mi, const char *mask);
void        flux_masked_clear(FluxMaskedInput *mi);
int         flux_masked_update(FluxMaskedInput *mi, FluxMsg msg);
void        flux_masked_render(FluxMaskedInput *mi, FluxSB *sb, int width);
const char *flux_masked_value(FluxMaskedInput *mi);

/* ── 7. FluxSearchInput ───────────────────────────────────────────── */

typedef struct {
    FluxInput   inner;
    int         loading, spin_frame;
    int         result_count, total_count;   /* -1 = hide */
    int         cleared;
    const char *icon;
    const char *icon_fg;
    const char *count_fg;
} FluxSearchInput;

void        flux_search_init(FluxSearchInput *s, const char *placeholder);
int         flux_search_update(FluxSearchInput *s, FluxMsg msg);
void        flux_search_render(FluxSearchInput *s, FluxSB *sb, int width);
const char *flux_search_value(const FluxSearchInput *s);

/* ── 8. FluxChatInput ─────────────────────────────────────────────── */

#ifndef FLUX_CHAT_HISTORY_MAX
#define FLUX_CHAT_HISTORY_MAX 64
#endif

typedef struct {
    FluxTextArea ta;
    char         history[FLUX_CHAT_HISTORY_MAX][FLUX_INPUT_MAX + 1];
    int          history_count;
    int          history_pos;       /* -1 = composing live */
    char         draft[FLUX_INPUT_MAX + 1];
    char         consumed[FLUX_INPUT_MAX + 1];
    int          submitted;
    int          multiline;
    const char  *prompt;
    const char  *prompt_fg;
} FluxChatInput;

void        flux_chat_init(FluxChatInput *c, const char *placeholder);
int         flux_chat_update(FluxChatInput *c, FluxMsg msg);
void        flux_chat_render(FluxChatInput *c, FluxSB *sb, int width, int max_rows);
const char *flux_chat_consume(FluxChatInput *c);

/* ── 9. FluxStepper ───────────────────────────────────────────────── */

typedef struct {
    long  value, min, max;
    long  step;
    long  fast_step;
    int   focused;
    const char *label, *fmt;
    const char *btn_fg;
    const char *value_fg;
    const char *disabled_fg;
} FluxStepper;

void flux_stepper_init(FluxStepper *s, long initial, long min, long max, long step);
int  flux_stepper_update(FluxStepper *s, FluxMsg msg);
void flux_stepper_render(FluxStepper *s, FluxSB *sb, int width);

/* ── 10. FluxForm ─────────────────────────────────────────────────── */

typedef enum {
    FLUX_FIELD_INPUT,
    FLUX_FIELD_TEXTAREA,
    FLUX_FIELD_MASKED,
    FLUX_FIELD_CHECKBOX,
    FLUX_FIELD_SWITCH,
    FLUX_FIELD_RADIO,
    FLUX_FIELD_SELECT,
    FLUX_FIELD_STEPPER
} FluxFieldKind;

typedef const char *(*FluxFieldValidate)(void *widget, void *ctx);

typedef struct {
    const char       *name, *label;
    FluxFieldKind     kind;
    void             *widget;
    FluxFieldValidate validate;
    void             *vctx;
    const char       *error;
    int               rows;
} FluxFormField;

#ifndef FLUX_FORM_FIELDS_MAX
#define FLUX_FORM_FIELDS_MAX 16
#endif

typedef struct {
    FluxFormField fields[FLUX_FORM_FIELDS_MAX];
    int           field_count;
    int           focus;            /* -1 = action row */
    int           cancel_focused;
    int           submitted, cancelled;
    const char   *submit_label, *cancel_label;
    const char   *title, *border_fg, *label_fg, *error_fg;
} FluxForm;

void flux_form_init(FluxForm *f, const char *title);
int  flux_form_add(FluxForm *f, const FluxFormField *field);
int  flux_form_update(FluxForm *f, FluxMsg msg);
void flux_form_render(FluxForm *f, FluxSB *sb, int width);
int  flux_form_validate(FluxForm *f);

/* ─── B3_status_display ─────────────────────────────────────── */
/* Shared kind enum (alert, status_msg, toast). */
typedef enum {
    FLUX_KIND_INFO    = 0,
    FLUX_KIND_SUCCESS = 1,
    FLUX_KIND_WARNING = 2,
    FLUX_KIND_ERROR   = 3
} FluxKind;

/* Footer key hint pair. */
typedef struct {
    const char *key;     /* e.g. "^C"  */
    const char *label;   /* e.g. "quit" */
} FluxKeyHint;

/* Transient overlay timer (only stateful widget in this batch). */
typedef struct {
    FluxKind    kind;
    const char *message;     /* borrowed; caller owns storage */
    long        shown_at_ms;
    long        ttl_ms;      /* default 3000; <=0 means sticky */
    int         visible;
} FluxToast;

/* 1. Alert — bordered box with kind-coloured icon + bold title and body. */
void flux_alert(FluxSB *sb, FluxKind kind,
                const char *title,
                const char *message,
                int width);

/* 2. Badge — inline pill, no newline. */
void flux_badge(FluxSB *sb, const char *text,
                const char *fg_clr,
                const char *bg_clr);

/* 3. Tag — inline `[label]` chip, fg colour only. */
void flux_tag(FluxSB *sb, const char *text,
              const char *fg_clr);

/* 4. Toast — transient overlay row. */
void flux_toast_init(FluxToast *t, FluxKind kind,
                     const char *message, long ttl_ms);
void flux_toast_show(FluxToast *t, long now_ms);
void flux_toast_tick(FluxToast *t, long now_ms);
int  flux_toast_visible(const FluxToast *t);
void flux_toast_render(const FluxToast *t, FluxSB *sb, int width);

/* 5. Tooltip — bubble + arrow pointing at (x, y). */
void flux_tooltip(FluxSB *sb, const char *body,
                  int x, int y,
                  int width);

/* 6. Header — title + optional right info + rule. */
void flux_header(FluxSB *sb, const char *title,
                 const char *right_text,
                 int width);

/* 7. Footer — rule + key hints joined by " · ". */
void flux_footer(FluxSB *sb, const FluxKeyHint *hints, int n, int width);

/* 8. Avatar — inline parenthesised initials. */
void flux_avatar(FluxSB *sb,
                 const char *initials,
                 const char *fg_clr);

/* 9. Status message — coloured dot + message, single row. */
void flux_status_msg(FluxSB *sb, FluxKind kind,
                     const char *msg, int width);

/* 10. Placeholder — centred empty-state (icon, title, optional hint). */
void flux_placeholder(FluxSB *sb,
                      const char *icon,
                      const char *title,
                      const char *hint,
                      int width);

/* 11. Link — OSC-8 hyperlink wrapping coloured underlined text. */
void flux_link(FluxSB *sb, const char *text, const char *url,
               const char *fg_clr);

/* ─── B4_nav_modal ─────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Overlay (backdrop dim helper, stateless)
 * ═════════════════════════════════════════════════════════════════ */

void flux_overlay(FluxSB *sb, const char *color, int alpha_pct,
                  int w, int h);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Modal (popover dialog)
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    FLUX_MODAL_SM   = 0,
    FLUX_MODAL_MD   = 1,
    FLUX_MODAL_LG   = 2,
    FLUX_MODAL_FULL = 3
} FluxModalSize;

typedef struct {
    const char   *title;          /* nullable */
    const char   *body;           /* '\n'-separated, pre-wrapped */
    FluxModalSize size;
    int           is_open;        /* caller toggles to show/hide */
    int           closed;         /* widget sets to 1 on Esc; caller resets */
    const char   *border_fg, *panel_bg, *title_fg, *body_fg;
    int           show_esc_hint;  /* default 1 */
} FluxModal;

void flux_modal_init  (FluxModal *m, const char *title, const char *body,
                       FluxModalSize sz);
int  flux_modal_update(FluxModal *m, FluxMsg msg);
void flux_modal_render(FluxModal *m, FluxSB *sb,
                       int screen_w, int screen_h, int dim_alpha_pct);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Accordion (multi-section collapsible)
 * ═════════════════════════════════════════════════════════════════ */

#define FLUX_ACCORDION_MAX 16

typedef struct {
    const char *title;
    const char *body;       /* '\n'-separated, pre-wrapped */
    int         open;
} FluxAccordionSection;

typedef struct {
    FluxAccordionSection sections[FLUX_ACCORDION_MAX];
    int          count, cursor;
    int          exclusive;       /* 1 = at most one open */
    const char  *header_fg, *active_fg, *body_fg, *marker_fg;
} FluxAccordion;

void flux_accordion_init  (FluxAccordion *a,
                           FluxAccordionSection *s, int n, int exclusive);
int  flux_accordion_update(FluxAccordion *a, FluxMsg msg);
void flux_accordion_render(FluxAccordion *a, FluxSB *sb, int width);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Collapsible (single section)
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *title, *body;
    int         expanded, focused;
    const char *title_fg, *body_fg, *marker_fg;
} FluxCollapsible;

void flux_collapsible_init  (FluxCollapsible *c,
                             const char *t, const char *b, int expanded);
int  flux_collapsible_update(FluxCollapsible *c, FluxMsg msg);
void flux_collapsible_render(FluxCollapsible *c, FluxSB *sb, int width);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — ContentSwitcher (segmented [A | B | C])
 * ═════════════════════════════════════════════════════════════════ */

#define FLUX_CSW_MAX 8

typedef struct {
    const char *labels[FLUX_CSW_MAX];
    int         count, active;
    const char *active_bg, *active_fg, *idle_fg, *border_fg;
    int         x_start[FLUX_CSW_MAX];
    int         x_end  [FLUX_CSW_MAX];
    int         y;
} FluxContentSwitcher;

void flux_content_switcher_init  (FluxContentSwitcher *s,
                                  const char **labels, int n);
int  flux_content_switcher_update(FluxContentSwitcher *s, FluxMsg msg);
void flux_content_switcher_render(FluxContentSwitcher *s, FluxSB *sb,
                                  int width, int screen_x, int screen_y);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Menu (vertical menu list)
 * ═════════════════════════════════════════════════════════════════ */

#define FLUX_MENU_MAX 64

typedef struct {
    const char *label, *shortcut, *icon;
    char        hotkey;       /* 0 = none */
    int         disabled, separator, user_id;
} FluxMenuItem;

typedef struct {
    FluxMenuItem *items;      /* caller-owned */
    int           count, cursor, scroll, visible;
    int           selected_id, activated;   /* widget sets on Enter */
    const char   *normal_fg, *active_bg, *active_fg, *hint_fg, *divider_fg;
} FluxMenu;

void flux_menu_init  (FluxMenu *m, FluxMenuItem *items, int n, int visible);
int  flux_menu_update(FluxMenu *m, FluxMsg msg);
void flux_menu_render(FluxMenu *m, FluxSB *sb, int width);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — CommandPalette (fuzzy search + list)
 * ═════════════════════════════════════════════════════════════════ */

#define FLUX_CP_MAX 256

typedef struct {
    const char *id, *name, *description, *category, *shortcut;
    int         disabled;
} FluxCmdItem;

typedef struct {
    FluxCmdItem *items;       /* caller-owned, immutable */
    int          count;
    FluxInput    query;
    int          filtered_idx  [FLUX_CP_MAX];
    int          filtered_score[FLUX_CP_MAX];
    int          filtered_count, cursor, scroll, visible;
    int          is_open, activated;
    const char  *selected_id;
    const char  *border_fg, *panel_bg, *active_bg, *match_fg;
} FluxCommandPalette;

void flux_command_palette_init  (FluxCommandPalette *p,
                                 FluxCmdItem *items, int n);
int  flux_command_palette_update(FluxCommandPalette *p, FluxMsg msg);
void flux_command_palette_render(FluxCommandPalette *p, FluxSB *sb,
                                 int screen_w, int screen_h);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Paginator
 * ═════════════════════════════════════════════════════════════════ */

typedef enum {
    FLUX_PG_DOTS     = 0,
    FLUX_PG_NUMBERS  = 1,
    FLUX_PG_FRACTION = 2
} FluxPaginatorStyle;

typedef struct {
    int                total, current;   /* current is 0-based */
    FluxPaginatorStyle style;
    int                changed;
    const char        *active_fg, *idle_fg, *arrow_fg;
} FluxPaginator;

void flux_paginator_init  (FluxPaginator *p, int total, int current,
                           FluxPaginatorStyle s);
int  flux_paginator_update(FluxPaginator *p, FluxMsg msg);
void flux_paginator_render(FluxPaginator *p, FluxSB *sb, int width);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Breadcrumb (stateless)
 * ═════════════════════════════════════════════════════════════════ */

void flux_breadcrumb(FluxSB *sb,
                     const char **parts, int n,
                     int active, int width,           /* active=-1 ⇒ last */
                     const char *normal_fg,
                     const char *active_fg,
                     const char *sep_fg);


/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — Help panel (stateless table of bindings)
 * ═════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *keys;        /* "Ctrl+S", "↑↓" */
    const char *description;
    const char *category;    /* nullable; same-cat items group together */
} FluxHelpBinding;

void flux_help_panel(FluxSB *sb,
                     const FluxHelpBinding *bindings, int n,
                     int width,
                     const char *title,                /* nullable */
                     const char *key_fg,
                     const char *desc_fg,
                     const char *cat_fg);              /* nullable */

/* ─── B5_ai_widgets ─────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════
 * PUBLIC API: Widgets — B5 AI widgets
 * ═════════════════════════════════════════════════════════════════ */

/* ── 1. FluxBlinkDot ────────────────────────────────────────────── */

typedef enum {
    FLUX_DOT_PENDING,
    FLUX_DOT_RUNNING,
    FLUX_DOT_STREAMING,
    FLUX_DOT_COMPLETED,
    FLUX_DOT_FAILED,
    FLUX_DOT_CANCELLED
} FluxDotState;

typedef struct {
    FluxDotState state;
    int          frame;
    int          interval_ms;
    const char  *on_glyph;
    const char  *off_glyph;
} FluxBlinkDot;

void flux_blinkdot_init  (FluxBlinkDot *d, FluxDotState s);
void flux_blinkdot_tick  (FluxBlinkDot *d);
void flux_blinkdot_render(FluxBlinkDot *d, FluxSB *sb);

/* ── 2. flux_command_block ──────────────────────────────────────── */

void flux_command_block(FluxSB *sb,
                        const char *cmd,
                        const char *output,
                        int   exit_code,
                        int   duration_ms,
                        int   width);

/* ── 3. FluxCommandDropdown ─────────────────────────────────────── */

typedef struct {
    const char *name;
    const char *desc;
} FluxCmdDropItem;

typedef struct {
    const FluxCmdDropItem *items;
    int   n_items;
    int   selected;
    int   max_visible;
    int   scroll;
    char  filter[64];
    int   filter_len;
    int   closed;
    int   chosen;
    const char *highlight_fg;
    const char *indicator;
} FluxCommandDropdown;

void flux_cmd_dropdown_init  (FluxCommandDropdown *d,
                              const FluxCmdDropItem *items, int n);
int  flux_cmd_dropdown_update(FluxCommandDropdown *d, FluxMsg msg);
void flux_cmd_dropdown_render(FluxCommandDropdown *d, FluxSB *sb, int width);

/* ── 4. flux_context_window ─────────────────────────────────────── */

void flux_context_window(FluxSB *sb, long used, long total, int width);

/* ── 5. flux_cost_tracker ───────────────────────────────────────── */

void flux_cost_tracker(FluxSB *sb,
                       long   prompt_tokens, long completion_tokens,
                       double prompt_cost_per_1m,
                       double completion_cost_per_1m,
                       double session_prior_usd,
                       int    width);

/* ── 6. flux_message_bubble ─────────────────────────────────────── */

typedef enum {
    FLUX_ROLE_USER,
    FLUX_ROLE_ASSISTANT,
    FLUX_ROLE_SYSTEM,
    FLUX_ROLE_TOOL
} FluxRole;

void flux_message_bubble(FluxSB *sb,
                         FluxRole role,
                         const char *text,
                         const char *timestamp,
                         int width);

/* ── 7. flux_model_badge ────────────────────────────────────────── */

void flux_model_badge(FluxSB *sb,
                      const char *provider,
                      const char *model,
                      long max_tokens,
                      int  width);

/* ── 8. FluxOpTree ──────────────────────────────────────────────── */

typedef enum {
    FLUX_OP_PENDING,
    FLUX_OP_RUNNING,
    FLUX_OP_COMPLETED,
    FLUX_OP_FAILED,
    FLUX_OP_CANCELLED
} FluxOpStatus;

typedef struct {
    const char  *id;
    const char  *label;
    const char  *detail;
    FluxOpStatus status;
    int          depth;
    int          duration_ms;
    int          has_children;
} FluxOpItem;

typedef struct {
    FluxOpItem    *items;
    int            n;
    unsigned char *expanded;
    int            selected;
    int            spinner_frame;
    int            spinner_interval_ms;
} FluxOpTree;

void flux_op_tree_init  (FluxOpTree *t,
                         FluxOpItem *items, unsigned char *expanded, int n);
int  flux_op_tree_update(FluxOpTree *t, FluxMsg msg);
void flux_op_tree_tick  (FluxOpTree *t);
void flux_op_tree_render(FluxOpTree *t, FluxSB *sb, int width);

/* ── 9. FluxShimmerText ─────────────────────────────────────────── */

typedef struct {
    const char *text;
    int         offset;
    int         shimmer_w;     /* width of bright band, in cells */
    int         step;          /* cells advanced per tick (>=1).
                                * Default 1 = smooth glide. The
                                * caller controls perceived speed via
                                * the FluxRate gating their tick — a
                                * narrower step + higher gating rate
                                * looks smoother than wide steps. 0
                                * is normalised to 1 in tick(). */
    int         interval_ms;
    int         active;
    const char *base_fg;
    const char *shimmer_fg;
} FluxShimmerText;

void flux_shimmer_init  (FluxShimmerText *s, const char *text);
void flux_shimmer_tick  (FluxShimmerText *s);
void flux_shimmer_render(FluxShimmerText *s, FluxSB *sb);

/* ── 10. FluxStreamingText ──────────────────────────────────────── */

typedef struct {
    const char *text;
    int         text_len;
    int         revealed;
    int         speed;
    int         interval_ms;
    int         show_cursor;
    int         cursor_on;
    int         cursor_frame;
    int         cursor_interval_ms;
    int         full_revealed;
    const char *fg;
    const char *cursor_fg;
} FluxStreamingText;

void flux_streaming_init    (FluxStreamingText *s, const char *text);
void flux_streaming_set_text(FluxStreamingText *s, const char *text);
void flux_streaming_tick    (FluxStreamingText *s);
void flux_streaming_render  (FluxStreamingText *s, FluxSB *sb);
void flux_streaming_text_done(FluxStreamingText *s);

/* ── 11. flux_token_stream ──────────────────────────────────────── */

void flux_token_stream(FluxSB *sb,
                       double tok_per_sec,
                       long   total_tokens,
                       long   max_tokens,
                       int    width);

/* ─── B6_data_charts ─────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════
 * B6 — DATA & CHARTS  (flux.h, C99 strict, zero malloc)
 *
 * 13 widgets:
 *   area_chart, bar_chart, line_chart (+ multi), scatter, histogram
 *   (+ from_samples), heatmap, gauge, dl, pretty,
 *   FluxRichLog, FluxTree, FluxVirtualList, FluxDataGrid
 *
 * Foundation: FluxBraille (2x4 sub-pixel canvas).
 * All renderers honour the "exactly `width` cells per row" contract.
 * ═════════════════════════════════════════════════════════════════ */



#include <stdint.h>
#include <stdarg.h>

/* ─── Color schemes ─────────────────────────────────────────────── */

extern const char *FLUX_CHART_PALETTE[5];
extern const char *FLUX_HEATMAP_RAMP[6];
extern const char *FLUX_AXIS_COLOR;

const char *flux_chart_color(int series_idx);
const char *flux_heatmap_color(float t);

/* ─── Braille canvas ────────────────────────────────────────────── */

typedef struct {
    int       cols, rows;       /* size in cells */
    int       pw, ph;           /* pixels: cols*2, rows*4 */
    uint8_t  *bits;             /* cols*rows bytes (caller-owned) */
    uint32_t *color;            /* cols*rows ints, per-cell fg (0=default) */
} FluxBraille;

int  flux_braille_init      (FluxBraille *c, int cols, int rows,
                             uint8_t *bits_buf, uint32_t *color_buf);
void flux_braille_clear     (FluxBraille *c);
void flux_braille_set_pixel (FluxBraille *c, int x, int y, uint32_t color);
void flux_braille_line      (FluxBraille *c, int x0, int y0, int x1, int y1,
                             uint32_t color);
void flux_braille_render_row(const FluxBraille *c, FluxSB *sb, int row,
                             const char *default_color);
void flux_braille_render    (const FluxBraille *c, FluxSB *sb,
                             const char *default_color);

/* ─── Chart helpers ─────────────────────────────────────────────── */

void flux_resample (const float *src, int sn, float *dst, int dn);
int  flux_fmt_axis (char *out, int sz, float v, int width);

/* ─── 1. area_chart ─────────────────────────────────────────────── */

typedef struct {
    const char *title, *color;
    float       y_min, y_max;       /* equal = autoscale */
    int         show_axes, fill_sparse;
} FluxAreaOpts;

void flux_area_chart(FluxSB *sb, const float *values, int n,
                     int width, int height, const FluxAreaOpts *opts);

/* ─── 2. bar_chart ──────────────────────────────────────────────── */

typedef struct {
    const char  *title, *color;
    const char **bar_colors;
    int          show_values, show_axes, bar_gap, horizontal;
} FluxBarChartOpts;

void flux_bar_chart(FluxSB *sb, const float *values, int n,
                    int width, int height,
                    const char *const *labels,
                    const FluxBarChartOpts *opts);

/* ─── 3. line_chart ─────────────────────────────────────────────── */

typedef struct {
    const char *title, *color;
    float       y_min, y_max;
    int         show_axes, show_grid, show_points;
    const char *const *x_labels;
    int                x_label_n;
} FluxLineChartOpts;

void flux_line_chart(FluxSB *sb, const float *values, int n,
                     int width, int height,
                     const FluxLineChartOpts *opts);

typedef struct {
    const float *data;
    int          n;
    const char  *color, *name;
} FluxSeries;

void flux_line_chart_multi(FluxSB *sb, const FluxSeries *series, int series_n,
                           int width, int height,
                           const FluxLineChartOpts *opts);

/* ─── 4. scatter ────────────────────────────────────────────────── */

typedef struct { float x, y; } FluxPoint;

typedef struct {
    const char *title, *color;
    float       x_min, x_max, y_min, y_max;
    int         show_axes, show_trend, dot_size;
} FluxScatterOpts;

void flux_scatter(FluxSB *sb, const FluxPoint *pts, int n,
                  int width, int height, const FluxScatterOpts *opts);

/* ─── 5. histogram ──────────────────────────────────────────────── */

typedef struct {
    const char *title, *color, *x_label;
    int         show_counts, show_axes;
} FluxHistogramOpts;

void flux_histogram(FluxSB *sb, const int *bins, int n,
                    int width, int height, const FluxHistogramOpts *opts);

void flux_histogram_from_samples(FluxSB *sb, const float *samples, int n_samples,
                                 int n_bins, int width, int height,
                                 const FluxHistogramOpts *opts);

/* ─── 6. heatmap ────────────────────────────────────────────────── */

typedef struct {
    const char        *title;
    const char *const *row_labels;
    const char *const *col_labels;
    int                cell_w, show_values;
    float              v_min, v_max;
} FluxHeatmapOpts;

void flux_heatmap(FluxSB *sb, const float *matrix,
                  int rows, int cols, int width,
                  const FluxHeatmapOpts *opts);

/* ─── 7. gauge ──────────────────────────────────────────────────── */

typedef struct {
    const char        *label, *color;
    int                show_value, arc;
    const float       *thresh_at;
    const char *const *thresh_color;
    int                thresh_n;
} FluxGaugeOpts;

void flux_gauge(FluxSB *sb, float value, float min, float max,
                int width, const FluxGaugeOpts *opts);

/* ─── 8. dl (definition list) ───────────────────────────────────── */

typedef struct { const char *term, *def; } FluxDLItem;

typedef struct {
    const char *term_color;
    int         layout;          /* 0 inline, 1 stacked */
    int         separator_line;
} FluxDLOpts;

void flux_dl(FluxSB *sb, const FluxDLItem *items, int n,
             int width, const FluxDLOpts *opts);

/* ─── 9. pretty (JSON-ish printer) ──────────────────────────────── */

typedef struct {
    int indent;        /* spaces per level, default 2 */
    int color;         /* 1 = colorize keys/strings/numbers */
    int max_depth;     /* 0 = unlimited */
} FluxPrettyOpts;

void flux_pretty(FluxSB *sb, const char *json_text,
                 int width, const FluxPrettyOpts *opts);

/* ─── 10. FluxRichLog ───────────────────────────────────────────── */

typedef enum {
    FLUX_LOG_DEBUG,
    FLUX_LOG_INFO,
    FLUX_LOG_WARN,
    FLUX_LOG_ERROR,
    FLUX_LOG_OK
} FluxLogLevel;

typedef struct {
    FluxLogLevel level;
    int          ts_ms;
    char         text[240];
} FluxLogLine;

typedef struct {
    FluxLogLine *ring;
    int          cap, head, count;
    int          show_timestamp, show_level, autoscroll;
    FluxScroll   scroll;
} FluxRichLog;

void flux_richlog_init  (FluxRichLog *l, FluxLogLine *buf, int cap);
void flux_richlog_add   (FluxRichLog *l, FluxLogLevel lvl, const char *text);
void flux_richlog_addf  (FluxRichLog *l, FluxLogLevel lvl, const char *fmt, ...);
void flux_richlog_clear (FluxRichLog *l);
int  flux_richlog_update(FluxRichLog *l, FluxMsg msg);
void flux_richlog_render(FluxRichLog *l, FluxSB *sb, int width, int viewport_h);

/* ─── 11. FluxTree ──────────────────────────────────────────────── */

typedef struct {
    const char *label;
    int         depth;
    int         is_branch;
    void       *user;
} FluxTreeNode;

typedef struct {
    const FluxTreeNode *nodes;
    int                 count, cursor, scroll, visible;
    uint8_t            *collapsed;
} FluxTree;

void flux_tree_init    (FluxTree *t, const FluxTreeNode *nodes, int count,
                        uint8_t *collapsed_buf, int visible);
int  flux_tree_update  (FluxTree *t, FluxMsg msg);
void flux_tree_render  (FluxTree *t, FluxSB *sb, int width);
int  flux_tree_selected(const FluxTree *t);

/* ─── 12. FluxVirtualList ───────────────────────────────────────── */

typedef void (*FluxVListItemFn)(FluxSB *sb, int index, int selected,
                                int width, void *ctx);

typedef struct {
    int             count, visible;
    int             cursor, scroll;
    int             item_height;
    FluxVListItemFn render_item;
} FluxVirtualList;

void flux_vlist_init     (FluxVirtualList *l, int count, int visible,
                          FluxVListItemFn render);
void flux_vlist_set_count(FluxVirtualList *l, int new_count);
int  flux_vlist_update   (FluxVirtualList *l, FluxMsg msg);
void flux_vlist_render   (FluxVirtualList *l, FluxSB *sb,
                          int width, void *ctx);

/* ─── 13. FluxDataGrid ──────────────────────────────────────────── */

typedef enum { FLUX_DG_TEXT, FLUX_DG_NUM, FLUX_DG_BOOL } FluxDGType;
typedef enum { FLUX_DG_NONE, FLUX_DG_ASC, FLUX_DG_DESC } FluxDGSort;

typedef struct {
    const char *title;
    int         width, min_width;
    FluxDGType  type;
    FluxDGSort  sort;
} FluxDGCol;

typedef const char *(*FluxDGCellFn)(int row, int col, void *ctx);
typedef int         (*FluxDGCmpFn) (int a, int b, int col, void *ctx);

typedef struct {
    FluxDGCol   *cols;
    int          col_count, row_count;
    int          page_size, page;
    int          cursor_row;
    int          cursor_col;
    int          sort_col;
    FluxDGCellFn cell_text;
    FluxDGCmpFn  compare;
    int         *row_order;
} FluxDataGrid;

void flux_dg_init    (FluxDataGrid *g, FluxDGCol *cols, int col_count,
                      int row_count, int page_size,
                      int *row_order_buf, FluxDGCellFn cell);
void flux_dg_set_rows(FluxDataGrid *g, int row_count);
void flux_dg_sort_by (FluxDataGrid *g, int col, FluxDGSort dir, void *ctx);
int  flux_dg_update  (FluxDataGrid *g, FluxMsg msg, void *ctx);
void flux_dg_render  (FluxDataGrid *g, FluxSB *sb, int width, void *ctx);

/* ─── B7_effects ─────────────────────────────────────── */
/* ── Batch B7 — Effects & decoration ───────────────────────────────── */

typedef struct { unsigned char r, g, b; } FluxRGB;

/* Inline RGB→RGB linear interpolation, clamped to [0,1]. */
static inline FluxRGB flux_rgb_lerp(FluxRGB a, FluxRGB b, float t) {
    if (t < 0.f) t = 0.f; else if (t > 1.f) t = 1.f;
    return (FluxRGB){
        (unsigned char)(a.r + (b.r - a.r) * t + 0.5f),
        (unsigned char)(a.g + (b.g - a.g) * t + 0.5f),
        (unsigned char)(a.b + (b.b - a.b) * t + 0.5f) };
}

/* Truecolor SGR helpers. Silent no-op on NULL sb. */
void flux_sb_fg_rgb(FluxSB *sb, FluxRGB c);
void flux_sb_bg_rgb(FluxSB *sb, FluxRGB c);

/* Content callback for flux_gradient_border. Must emit exactly
 * `inner_h` rows of exactly `inner_w` cells each, '\n'-terminated. */
typedef void (*FluxBorderContentFn)(FluxSB *sb,
                                    int inner_w, int inner_h, void *ctx);

/* MULTI-LINE: emits exactly 3 rows ('\n'-terminated), each `width` cells.
 * 7-segment-style large block-font glyphs for digits 0-9 plus ":.-+ ".
 * Lowercase letters upper-cased before lookup; unknown chars render as
 * blank glyph. `color == NULL` → FLUX_THEME_BRAND_PURPLE_FG. `str == NULL`
 * is a no-op. Each glyph is 3×3 cells with a 1-cell trailing gap (dropped
 * on the final char before fitting). */
void flux_digits(FluxSB *sb, const char *str, const char *color, int width);

/* Single row + '\n'. Renders ` <text> ` with leading/trailing space carrying
 * a quarter-intensity halo bg, body bold over halo bg. Remainder of `width`
 * is plain pad. `fg == NULL` → FLUX_THEME_BRAND_PURPLE_FG. `glow_color == NULL`
 * → same as fg. Halo derived by parsing trailing R;G;B from glow_color's
 * `\x1b[38;2;…m` form; on parse failure falls back to FLUX_THEME_PANEL_BG. */
void flux_glow_text(FluxSB *sb, const char *text,
                    const char *fg, const char *glow_color, int width);

/* Single row + '\n'. Per-cell foreground gradient across `text`; remainder
 * of `width` is plain space. UTF-8 single-cell glyphs count as one cell;
 * wide (CJK) glyphs count as two and share their colour. Embedded '\n' in
 * `text` forces early row termination. `text == NULL` is a no-op. */
void flux_gradient_text(FluxSB *sb, const char *text,
                        FluxRGB start, FluxRGB end, int width);

/* Single row + '\n'. `progress` clamped to [0,1]. Filled cells use '█'
 * coloured along the gradient; soft-edge cell uses ▓/▒/░ per the same
 * thresholds as flux_bar (>0.66, >0.33, else); empty tail is ░ in
 * FLUX_THEME_TEXT_DIM_FG. */
void flux_gradient_bar(FluxSB *sb, float progress, int width,
                       FluxRGB start, FluxRGB end);

/* MULTI-LINE: emits exactly `h` rows ('\n'-terminated), each `w` cells.
 * Rounded border (`╭ ╮ ╰ ╯ ─ │`) with each border cell coloured along the
 * gradient walking the perimeter clockwise from top-left. `w < 2` or
 * `h < 2` is a no-op. If `content != NULL`, callback is invoked once into
 * a thread-local 8 KiB scratch FluxSB; callback MUST emit `h-2` rows of
 * exactly `w-2` cells. Inner content is NOT coloured by the gradient. */
void flux_gradient_border(FluxSB *sb, int w, int h,
                          FluxRGB start, FluxRGB end,
                          FluxBorderContentFn content, void *ctx);


/* ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════
 * AI DEMO WIDGETS (v5 — OpenClaw Workstation)
 *
 * 11 widgets needed for ai_demo.c. All single-header, zero-alloc in
 * renderers, honor width contract. Added by Agent F2.
 *
 *   1.  flux_easing          — motion curves: expo / cubic / back /
 *                              bounce / spring. Pure float→float.
 *   2.  FluxChannelBadge     — HITL/TG/AUTO/GH/SL/API hue pill
 *   3.  FluxHttpBadge        — HTTP method + status pill
 *   4.  FluxFocusRing        — glowing overlay ring for focused pane
 *   5.  FluxToastCenter      — stacked spring-in fade-out toast queue
 *   6.  FluxTicker           — horizontal scrolling event strip
 *   7.  FluxGanttRow         — parallel-task timeline strip
 *   8.  FluxParticleBurst    — one-shot particle effect
 *   9.  FluxSplitPane        — 2/3-pane split with focus tracking
 *  10.  FluxAgentCard        — orchestra hero card
 *  11.  FluxRequestTrace     — expandable HTTP request row
 * ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════ */

/* ─── 1. flux_easing helpers ──────────────────────────────────────
 *
 * Pure functions, t in [0,1]. Output is usually in [0,1] except
 * ease_out_back which can overshoot slightly past 1.0. Use to drive
 * per-frame animations from a normalized progress. All are FLUX_UNUSED_
 * to avoid warnings when a caller only uses some of them.
 *
 *   float t = (flux_now_ms() - t0) / dur;
 *   float eased = flux_ease_out_expo(t);
 *   int y = (int)(eased * target_y);
 */
float flux_ease_out_expo   (float t);
float flux_ease_in_out_cubic(float t);
float flux_ease_out_back   (float t);
float flux_ease_out_bounce (float t);
float flux_spring          (float t, float stiffness, float damping);

/* ─── 2. FluxChannelBadge ─────────────────────────────────────────
 *
 * Hue-per-channel pill. Each channel owns a color (see spec §6.2):
 *   HITL=#bb9af7 TG=#7dcfff AUTO=#9ece6a GH=#ff9e64 SL=#f7768e API=#e0af68
 *
 * Usage:
 *   flux_channel_badge(sb, FLUX_CH_HITL, FLUX_BADGE_OK, 16);
 * Emits exactly `width` cells + '\n'. Uses flux_gradient_text for accent.
 */
typedef enum {
    FLUX_CH_HITL = 0,
    FLUX_CH_TG,
    FLUX_CH_AUTO,
    FLUX_CH_GH,
    FLUX_CH_SL,
    FLUX_CH_API
} FluxChannelId;

typedef enum {
    FLUX_BADGE_IDLE = 0,      /* grey */
    FLUX_BADGE_OK,            /* green */
    FLUX_BADGE_RUN,           /* purple (streaming) */
    FLUX_BADGE_WARN,          /* amber */
    FLUX_BADGE_ERR            /* red */
} FluxBadgeStatus;

FluxRGB     flux_channel_rgb  (FluxChannelId id);
const char *flux_channel_label(FluxChannelId id);
void        flux_channel_badge(FluxSB *sb, FluxChannelId id,
                               FluxBadgeStatus status, int width);

/* ─── 3. FluxHttpBadge ────────────────────────────────────────────
 *
 * Method + status-code pill. GET=blue, POST=green, PUT=amber, DELETE=red.
 * Status: 2xx green, 3xx cyan, 4xx amber, 5xx red. Layout:
 *    " METHOD  CODE "   e.g. " POST  200 "
 * Emits exactly `width` cells + '\n'; status_code<=0 hides the code.
 */
void flux_http_badge(FluxSB *sb, const char *method, int status_code,
                     int width);

/* ─── 4. FluxFocusRing ────────────────────────────────────────────
 *
 * Draws a subtle glowing rounded-corner border around a rect. Does NOT
 * fill the interior — render this AFTER the pane content.  `intensity`
 * in [0,1] controls alpha-like brightness (use flux_ease_* to animate).
 *
 *   flux_focus_ring(sb, 2, 5, 40, 10, (FluxRGB){180,130,255}, 0.8f);
 *
 * Emits ANSI cursor-positioning escapes so it can overlay arbitrary
 * content already in the frame. Uses 2-step gradient between the
 * accent and a dimmer version.
 */
void flux_focus_ring(FluxSB *sb, int x, int y, int w, int h,
                     FluxRGB accent, float intensity);

/* ─── 4b. FluxOverlayLayer — cell-diff overlay primitive ─────────
 *
 * A rectangular floating layer above the base view. Owns a prev/curr
 * cell buffer for its own region and emits ONLY the cells that
 * actually changed since the last frame — preserves flux.h's cell-
 * diff invariant for overlays (toasts, dialogs, palettes, popups).
 *
 * Lifecycle:
 *   1. Caller calls flux_overlay_begin(o, x, y, w, h) at start of frame
 *   2. Caller writes rows via flux_overlay_putln(o, row_idx, line)
 *      — line is ANSI/UTF-8 raw, MUST be exactly w display cells
 *      — unwritten rows default to spaces
 *   3. Caller calls flux_overlay_emit(o, sb) to append the diff to sb
 *
 * Resize-safe: if w/h change between frames, the prev buffer is
 * cleared so all cells re-emit (forced full repaint).
 *
 * Disable: pass w=0 or h=0 to begin() to mark layer hidden;
 * emit() then clears the previous region.
 */

#ifndef FLUX_OVERLAY_MAX_CELLS
#define FLUX_OVERLAY_MAX_CELLS  (256 * 64)   /* 16384 cells = 128 col * 128 row */
#endif

#ifndef FLUX_OVERLAY_BYTES_PER_CELL
#define FLUX_OVERLAY_BYTES_PER_CELL 64       /* fg+bg+attrs+char+reset+wrap */
#endif

typedef struct {
    int  cx, cy, cw, ch;          /* current rect (1-based screen coords) */
    int  px, py, pw, ph;           /* previous rect                        */
    int  active;                   /* 1 = in begin/draw window             */
    int  prev_visible;             /* was the layer painted last frame?    */
    /* prev/curr cell content as escaped strings — null-terminated per cell.
     * stored row-major; max FLUX_OVERLAY_MAX_CELLS slots of BYTES_PER_CELL each. */
    char *prev_cells;
    char *curr_cells;
    int   row_w[64];               /* cached width of each curr row (cells) */
    int   ncells;                  /* total cells in current rect          */
} FluxOverlayLayer;

void flux_overlay_init   (FluxOverlayLayer *o);
void flux_overlay_free   (FluxOverlayLayer *o);
/* Open a frame. (x,y) are 1-based; w/h in cells. w=0 or h=0 hides. */
void flux_overlay_begin  (FluxOverlayLayer *o, int x, int y, int w, int h);
/* Write one full row (raw bytes — ANSI + UTF-8). row_idx 0..h-1. */
void flux_overlay_putln  (FluxOverlayLayer *o, int row_idx,
                          const char *line);
/* Compare curr vs prev, emit minimal CUP-positioned cell diffs to sb,
 * then promote curr→prev. */
void flux_overlay_emit   (FluxOverlayLayer *o, FluxSB *sb);

/* ─── 4c. FluxDialog — modal dialog with N colored buttons ───────
 *
 * Self-contained modal: dim backdrop + centered card + N buttons
 * (1..4) + result. Uses FluxOverlayLayer for cell-diff correctness.
 *
 * Usage:
 *   FluxDialog d;
 *   flux_dialog_init(&d);
 *   flux_dialog_open(&d, (FluxDialogCfg){
 *       .title  = "Quit?",
 *       .body   = "All channels will stop.",
 *       .n_buttons = 2,
 *       .buttons[0] = { "Quit", FLUX_DIALOG_BTN_DANGER, "q" },
 *       .buttons[1] = { "Stay", FLUX_DIALOG_BTN_DEFAULT, NULL },
 *   });
 *   in update: flux_dialog_update(&d, msg);
 *   in view:   flux_dialog_render(&d, sb, screen_w, screen_h);
 *   when d.answered: read d.result (button index, 0..n-1) or -1 on Esc
 */

typedef enum {
    FLUX_DIALOG_BTN_DEFAULT = 0,
    FLUX_DIALOG_BTN_PRIMARY,    /* highlighted, accent color           */
    FLUX_DIALOG_BTN_DANGER,     /* red, for destructive actions        */
    FLUX_DIALOG_BTN_SUCCESS,    /* green                               */
} FluxDialogBtnKind;

typedef struct {
    const char       *label;
    FluxDialogBtnKind kind;
    const char       *shortcut;   /* optional 1-char hint, NULL = none  */
} FluxDialogBtn;

typedef struct {
    const char     *title;
    const char     *body;
    int             n_buttons;
    FluxDialogBtn   buttons[4];
    int             default_idx;       /* button focused on open        */
    int             width_cells;       /* 0 → auto                      */
    int             dim_backdrop;      /* 1 = dim entire screen         */
} FluxDialogCfg;

typedef struct {
    int               open;
    int               answered;        /* set when user picks a button  */
    int               result;          /* button index, -1 = Esc        */
    int               focus;           /* current button index          */
    FluxDialogCfg     cfg;
    FluxOverlayLayer  layer;
} FluxDialog;

void flux_dialog_init   (FluxDialog *d);
void flux_dialog_free   (FluxDialog *d);
void flux_dialog_open   (FluxDialog *d, FluxDialogCfg cfg);
void flux_dialog_close  (FluxDialog *d);
int  flux_dialog_update (FluxDialog *d, FluxMsg msg);   /* 1 if consumed */
void flux_dialog_render (FluxDialog *d, FluxSB *sb,
                         int screen_w, int screen_h);

/* ─── 4d. FluxCursorList — cursor + auto-scroll for variable rows ─
 *
 * Tracks a cursor index over a list of variable-height rows and
 * auto-scrolls the parent FluxScroll so the cursor stays visible.
 * Generic — used by GH cards, Audit entries, Slack threads, etc.
 *
 * Caller responsibilities each render frame:
 *   1. flux_cursor_list_begin(&cl, scroll, n_items, viewport_h, margin)
 *   2. for each item:
 *        flux_cursor_list_item(&cl, idx, item_height_rows, &row_offset)
 *      where row_offset receives the row index this item starts at.
 *   3. (optional) flux_cursor_list_end(&cl)
 *
 * After rendering, the FluxScroll's offset is automatically adjusted
 * if the cursor moved and is now outside the viewport (with a margin
 * of `margin` rows on top + bottom).
 *
 * The widget tracks "last known cursor" internally so passive scroll
 * (mouse wheel, PgDn) doesn't fight cursor-follow — auto-scroll only
 * triggers when the cursor actually moved since last render.
 */

typedef struct {
    int          cursor;
    int          last_cursor;        /* tracked across frames     */
    int          cursor_top;         /* row offset of cursor item, -1 = not seen */
    int          cursor_bot;
    int          running_row;        /* accumulator while iterating */
    int          n_items;
    int          viewport_h;
    int          margin;
    FluxScroll  *scroll;
    int          rendering;          /* between begin/end         */
} FluxCursorList;

void flux_cursor_list_init  (FluxCursorList *cl);
/* Start a new render frame. Call once per frame BEFORE iterating items. */
void flux_cursor_list_begin (FluxCursorList *cl, FluxScroll *scroll,
                             int cursor, int n_items,
                             int viewport_h, int margin);
/* Notify per item: idx 0-based, item_height in rows.
 * out_row_offset is the row index this item begins at (0-based). */
void flux_cursor_list_item  (FluxCursorList *cl, int idx,
                             int item_height, int *out_row_offset);
/* End the frame — applies cursor-follow auto-scroll if cursor moved. */
void flux_cursor_list_end   (FluxCursorList *cl);

/* Move helpers — clamp + return new cursor. */
int  flux_cursor_list_move  (int cursor, int n, int delta);

/* ─── 4e. FluxComposer — multi-line composer with paste-collapse ──
 *
 * A multi-line, auto-expanding text composer designed for chat / agent
 * UIs. Replaces the single-line FluxChatInput when richer editing is
 * needed.
 *
 * Features:
 *   - 1..max_rows visible_rows, auto-grows with content
 *   - UTF-8 / wide-char aware soft wrap
 *   - Multi-line editing (Shift+Enter newline, Enter submits)
 *   - Up/Down history scrubbing — only when caret is on first/last row
 *   - Paste-collapse: a paste of ≥3 lines OR ≥200 chars becomes a
 *     compact chip "[Paste #N · L lines]" in display while keeping
 *     full content addressable. consume() returns expanded text.
 *   - Focus arbitration via flux_composer_focus(c, 1/0) — when
 *     unfocused the widget swallows nothing.
 *
 * Caller pattern:
 *   FluxComposer c;
 *   flux_composer_init(&c);
 *   flux_composer_configure(&c, (FluxComposerCfg){.max_rows = 8});
 *
 *   // each frame:
 *   flux_composer_layout(&c, available_width);
 *   int rows_to_reserve = c.visible_rows;       // for layout
 *
 *   // in update:
 *   if (focused) flux_composer_update(&c, msg);
 *   const char *sent = flux_composer_consume(&c);
 *   if (sent) handle_user_message(sent);
 *
 *   // in render:
 *   flux_composer_render(&c, sb, available_width);
 */

#ifndef FLUX_COMPOSER_TEXT_CAP
#define FLUX_COMPOSER_TEXT_CAP   8192
#endif
#ifndef FLUX_COMPOSER_HIST_MAX
#define FLUX_COMPOSER_HIST_MAX   100
#endif
#ifndef FLUX_COMPOSER_PASTES_MAX
#define FLUX_COMPOSER_PASTES_MAX 8
#endif
#ifndef FLUX_COMPOSER_PASTE_CAP
#define FLUX_COMPOSER_PASTE_CAP  (64 * 1024)
#endif
#ifndef FLUX_COMPOSER_SEGS_MAX
#define FLUX_COMPOSER_SEGS_MAX   64
#endif
#ifndef FLUX_COMPOSER_SUBMIT_CAP
#define FLUX_COMPOSER_SUBMIT_CAP (FLUX_COMPOSER_TEXT_CAP + \
                                  FLUX_COMPOSER_PASTES_MAX * FLUX_COMPOSER_PASTE_CAP)
#endif
#ifndef FLUX_COMPOSER_WRAP_ROWS_MAX
#define FLUX_COMPOSER_WRAP_ROWS_MAX 256
#endif

typedef struct {
    int   min_rows;          /* default 1                          */
    int   max_rows;          /* default 8                          */
    int   history_enabled;   /* default 1                          */
    int   paste_collapse;    /* default 1                          */
    int   paste_collapse_lines;  /* default 3 (>= → collapse)      */
    int   paste_collapse_chars;  /* default 200                    */
    const char *placeholder;
} FluxComposerCfg;

typedef enum { FLUX_COMP_SEG_TEXT = 0, FLUX_COMP_SEG_PASTE = 1 } FluxComposerSegKind;

typedef struct {
    FluxComposerSegKind kind;
    int                 off;     /* TEXT: offset into c->text   */
    int                 len;     /* TEXT: byte length           */
    int                 paste_id;/* PASTE: index into pastes[]  */
} FluxComposerSeg;

typedef struct {
    int  lines;
    int  bytes;
    int  used;                                  /* slot occupancy */
    char content[FLUX_COMPOSER_PASTE_CAP];
} FluxComposerPaste;

typedef enum {
    FLUX_COMPOSER_OK = 0,
    FLUX_COMPOSER_ERR_TEXT_FULL,           /* hard input limit reached    */
    FLUX_COMPOSER_ERR_PASTE_TOO_BIG,       /* paste > FLUX_COMPOSER_PASTE_CAP */
    FLUX_COMPOSER_ERR_PASTE_SLOTS_FULL,    /* no free paste slot          */
    FLUX_COMPOSER_ERR_SEGS_FULL,           /* segment table saturated     */
    FLUX_COMPOSER_ERR_HISTORY_FULL,        /* history at cap (recycled)   */
} FluxComposerErr;

typedef struct {
    /* config + state */
    FluxComposerCfg     cfg;
    int                 focused;

    /* error model — last error + sticky last_error_ms for UI flash */
    FluxComposerErr     last_err;
    char                last_err_msg[128];
    uint64_t            last_err_ms;

    /* edit storage */
    char                text[FLUX_COMPOSER_TEXT_CAP];
    int                 text_len;
    FluxComposerSeg     segs[FLUX_COMPOSER_SEGS_MAX];
    int                 nsegs;
    FluxComposerPaste   pastes[FLUX_COMPOSER_PASTES_MAX];
    int                 paste_seq;              /* monotonic id   */

    /* caret */
    int                 caret_seg;
    int                 caret_off;              /* byte offset within seg */

    /* history */
    char                history[FLUX_COMPOSER_HIST_MAX][FLUX_COMPOSER_TEXT_CAP];
    int                 hist_count;
    int                 hist_browse;            /* -1 = live edit */

    /* layout cache (computed by flux_composer_layout) */
    int                 visible_rows;
    int                 wrap_total_rows;
    int                 caret_row;
    int                 caret_col;
    int                 view_top_row;           /* scroll within max_rows */

    /* submission */
    int                 submitted;
    char                last_submit[FLUX_COMPOSER_SUBMIT_CAP];

    /* Down-handoff: set when user presses Down on the last visible
     * row. Caller polls flux_composer_descend_pending(c) and clears
     * via flux_composer_clear_descend(c) after handing focus off. */
    int                 descend_pending;
} FluxComposer;

void        flux_composer_init     (FluxComposer *c);
void        flux_composer_configure(FluxComposer *c, FluxComposerCfg cfg);
void        flux_composer_focus    (FluxComposer *c, int focused);
int         flux_composer_update   (FluxComposer *c, FluxMsg msg);
void        flux_composer_layout   (FluxComposer *c, int width);
void        flux_composer_render   (FluxComposer *c, FluxSB *sb, int width);
const char *flux_composer_consume  (FluxComposer *c);   /* expanded text */
void        flux_composer_clear    (FluxComposer *c);
/* Last error state — caller may toast/render. cleared on next OK action. */
FluxComposerErr flux_composer_last_err(FluxComposer *c, const char **out_msg);
/* Total used bytes (text + paste content) for the "X% full" indicator. */
int         flux_composer_used_bytes(FluxComposer *c);
int         flux_composer_capacity_bytes(FluxComposer *c);
/* Down-handoff signal — set by render() when the user pressed Down
 * on the last visible row. Caller checks + clears so they can move
 * focus elsewhere (e.g. into a status bar). */
int         flux_composer_descend_pending(FluxComposer *c);
void        flux_composer_clear_descend(FluxComposer *c);

/* ─── 4f. FluxAppBar — configurable status bar with item callbacks ─
 *
 * A reusable status / action strip. Apps register items by id +
 * label + callbacks. Items are dynamic — add/remove anytime; values
 * pulled via callbacks each render so they always reflect live state.
 *
 * Three item kinds:
 *   - VALUE/TEXT:  display "label  value"
 *   - TOGGLE:      bool pulled each render, rendered as ⏵⏵ ON / ⏸ OFF
 *   - HINT:        keybind hint (dim) — read-only, shows the shortcut
 *
 * Activation:
 *   - Direct shortcut (item.shortcut) matched anywhere — bar consumes
 *     and calls on_activate.
 *   - Focus + Enter — Down enters the bar from below, ←/→ cycle items,
 *     Enter calls on_activate, Esc returns focus.
 *
 * Width-aware: items have a priority; under width pressure, lowest
 * priority items hide first.
 *
 * Animations:
 *   - Focused item: breathing reverse-video chip + accent underline
 *   - Toggles ON: subtle gradient sweep (1.5 s period) across the chip
 */

typedef enum {
    FLUX_APPBAR_LEFT = 0,
    FLUX_APPBAR_CENTER,
    FLUX_APPBAR_RIGHT,
} FluxAppBarAlign;

typedef enum {
    FLUX_APPBAR_KIND_TEXT   = 0,
    FLUX_APPBAR_KIND_VALUE,
    FLUX_APPBAR_KIND_TOGGLE,
    FLUX_APPBAR_KIND_HINT,
    FLUX_APPBAR_KIND_BADGE,
} FluxAppBarItemKind;

#ifndef FLUX_APPBAR_MAX_ITEMS
#define FLUX_APPBAR_MAX_ITEMS 16
#endif

typedef struct {
    char                id[24];
    FluxAppBarItemKind  kind;
    FluxAppBarAlign     align;
    char                icon[8];
    char                label[40];
    char                shortcut[24];
    /* Live value pull (kind=VALUE/BADGE): write up to out_sz-1 bytes. */
    void              (*value_fn)(void *ctx, char *out, int out_sz);
    /* Live bool pull (kind=TOGGLE): non-zero = on. */
    int               (*bool_fn)(void *ctx);
    /* Activation. */
    void              (*on_activate)(void *ctx);
    void               *ctx;
    int                 priority;       /* higher → kept under pressure */
    int                 interactive;    /* 1 = focusable + activatable */
    char                fg[24];         /* override fg (ANSI escape)    */
} FluxAppBarItem;

typedef struct {
    FluxAppBarItem items[FLUX_APPBAR_MAX_ITEMS];
    int            n_items;
    int            focused_idx;     /* -1 = bar unfocused             */
    int            has_focus;       /* 1 = bar owns keys              */
    char           separator[8];    /* default " · "                  */
    uint64_t       last_anim_ms;
    /* Resolved per render: x positions of each item (for hit-test). */
    int            item_x[FLUX_APPBAR_MAX_ITEMS];
    int            item_w[FLUX_APPBAR_MAX_ITEMS];
} FluxAppBar;

void flux_appbar_init      (FluxAppBar *);
/* Register an item. Returns its index, or -1 on overflow. id must be
 * unique; if it already exists the item is replaced. */
int  flux_appbar_add       (FluxAppBar *, FluxAppBarItem item);
void flux_appbar_remove    (FluxAppBar *, const char *id);
/* Set focus on/off the bar (caller drives this via Down/Up handoff). */
void flux_appbar_set_focus (FluxAppBar *, int focused);
int  flux_appbar_has_focus (FluxAppBar *);
/* Programmatic activation by id (for tests / external triggers). */
int  flux_appbar_activate  (FluxAppBar *, const char *id);
/* Update — returns 1 if consumed. Handles both focused-arrow nav AND
 * direct shortcut matching (ctrl+x style chords + single-letter keys). */
int  flux_appbar_update    (FluxAppBar *, FluxMsg msg);
/* Render in exactly `width` cells, single row. */
void flux_appbar_render    (FluxAppBar *, FluxSB *sb, int width, uint64_t now_ms);

/* ─── 5. FluxToastCenter ──────────────────────────────────────────
 *
 * Ring-buffer of pending toasts, rendered bottom-right. Spring-in,
 * fade-out via easing helpers. Not bounded by caller — owns a small
 * fixed ring. NOTE: the singular `FluxToast` struct + `flux_toast_*`
 * API above is a different, older single-toast widget. The stacked
 * queue uses the `*_center_*` prefix and the item type `FluxToastItem`.
 *
 *   FluxToastCenter tc;
 *   flux_toast_center_init(&tc);
 *   flux_toast_center_push(&tc, FLUX_TOAST_OK, "Merged", "#1248", 3000);
 *   // in tick:
 *   flux_toast_center_tick(&tc, flux_now_ms());
 *   // in view:
 *   flux_toast_center_render(&tc, sb, screen_w, screen_h);
 */
typedef enum {
    FLUX_TOAST_INFO = 0,
    FLUX_TOAST_OK,
    FLUX_TOAST_WARN,
    FLUX_TOAST_ERR
} FluxToastKind;

#ifndef FLUX_TOAST_CENTER_MAX
#define FLUX_TOAST_CENTER_MAX 6
#endif

typedef struct {
    FluxToastKind kind;
    char          title[64];
    char          body[96];
    uint64_t      created_ms;
    uint64_t      ttl_ms;
    uint64_t      now_ms;     /* updated by tick */
    int           alive;
} FluxToastItem;

typedef enum {
    FLUX_TOAST_POS_BOTTOM_RIGHT = 0,   /* default */
    FLUX_TOAST_POS_BOTTOM_LEFT,
    FLUX_TOAST_POS_BOTTOM_CENTER,
    FLUX_TOAST_POS_TOP_RIGHT,
    FLUX_TOAST_POS_TOP_LEFT,
    FLUX_TOAST_POS_TOP_CENTER,
    FLUX_TOAST_POS_CUSTOM,             /* use cfg.custom_x, cfg.custom_y */
} FluxToastPos;

/* Floating overlay placement, fully terminal-aware.
 *
 *   Position    = anchor preset OR explicit (custom_x, custom_y)
 *   Width       = absolute cells OR fraction of screen_w
 *   Offset      = absolute cells OR fraction of screen_{w,h}
 *   Safe area   = pixels reserved at each edge (chrome, footer, etc.)
 *
 * Everything resolves against the live screen each frame, so resize
 * is handled automatically; no caller-side recomputation. */
typedef struct {
    FluxToastPos anchor;
    int          custom_x, custom_y;       /* for FLUX_TOAST_POS_CUSTOM */
    int          width_cells;              /* 0 → use width_pct          */
    float        width_pct;                /* 0..1 of screen_w           */
    int          offset_x_cells;
    float        offset_x_pct;             /* 0..1 of screen_w           */
    int          offset_y_cells;
    float        offset_y_pct;             /* 0..1 of screen_h           */
    int          safe_top, safe_bottom, safe_left, safe_right;
    int          max_stack;                /* 0 → CENTER_MAX             */
} FluxToastConfig;

typedef struct {
    FluxToastItem     toasts[FLUX_TOAST_CENTER_MAX];
    int               count;
    FluxToastConfig   cfg;
    FluxOverlayLayer  layer;     /* cell-diff overlay — preserves invariant */
} FluxToastCenter;

void flux_toast_center_init  (FluxToastCenter *tc);
void flux_toast_center_configure(FluxToastCenter *tc, FluxToastConfig cfg);
void flux_toast_center_push  (FluxToastCenter *tc, FluxToastKind kind,
                              const char *title, const char *body,
                              uint64_t ttl_ms);
void flux_toast_center_tick  (FluxToastCenter *tc, uint64_t now_ms);
/* Renders toasts AND self-clears expired toast cells. Position resolved
 * each frame from cfg + (screen_w, screen_h). */
void flux_toast_center_render(FluxToastCenter *tc, FluxSB *sb,
                              int screen_w, int screen_h);

/* ─── 6. FluxTicker ───────────────────────────────────────────────
 *
 * Horizontal scrolling event strip: latest events pushed from the
 * right, separated by '·', with shimmer sweep over newest arrival.
 *
 *   FluxTicker t; flux_ticker_init(&t);
 *   flux_ticker_push(&t, FLUX_CH_GH, "PR #1248 merged");
 *   // tick advances shimmer
 *   flux_ticker_tick(&t, flux_now_ms());
 *   flux_ticker_render(&t, sb, width);
 */
#ifndef FLUX_TICKER_MAX
#define FLUX_TICKER_MAX 16
#endif

typedef struct {
    FluxChannelId channel;
    char          text[96];
    uint64_t      arrived_ms;
} FluxTickerEvent;

typedef struct {
    FluxTickerEvent events[FLUX_TICKER_MAX];
    int             head;         /* oldest */
    int             count;
    uint64_t        now_ms;
    int             shimmer_off;  /* advances per tick */
} FluxTicker;

void flux_ticker_init  (FluxTicker *t);
void flux_ticker_push  (FluxTicker *t, FluxChannelId ch, const char *text);
void flux_ticker_tick  (FluxTicker *t, uint64_t now_ms);
void flux_ticker_render(FluxTicker *t, FluxSB *sb, int width);

/* ─── 7. FluxGanttRow ─────────────────────────────────────────────
 *
 * Parallel-task timeline strip: each task occupies a row drawn as a
 * labelled coloured block using 1/8-th block glyphs for sub-cell
 * precision. `window_ms` is how much time fits into `width` cells
 * counting back from `now_ms`. `end_ms == 0` means "still running".
 *
 *   FluxGanttTask tasks[] = {
 *     {"tester",  {180,130,255}, t0, 0},
 *     {"refactor",{120,200,120}, t0+2000, t0+4500},
 *   };
 *   flux_gantt_row(sb, tasks, 2, 30000, flux_now_ms(), 60);
 * Emits exactly `n_tasks` rows of `width` cells + '\n' each.
 */
typedef struct {
    const char *label;
    FluxRGB     color;
    uint64_t    start_ms;
    uint64_t    end_ms;   /* 0 = still running */
} FluxGanttTask;

void flux_gantt_row(FluxSB *sb, const FluxGanttTask *tasks, int n_tasks,
                    uint64_t window_ms, uint64_t now_ms, int width);

/* ─── 8. FluxParticleBurst ────────────────────────────────────────
 *
 * One-shot particle effect — spawn at a cell, particles fly outward
 * with gravity, fade over ~600ms.
 *
 *   FluxParticleBurst pb; flux_particle_burst_init(&pb);
 *   flux_particle_burst_spawn(&pb, 40, 12, (FluxRGB){180,255,120}, 24);
 *   // per tick:
 *   flux_particle_burst_tick(&pb, flux_now_ms());
 *   flux_particle_burst_render(&pb, sb, screen_w, screen_h);
 */
#ifndef FLUX_PARTICLE_MAX
#define FLUX_PARTICLE_MAX 64
#endif

typedef struct {
    float    x, y;      /* cell-space */
    float    vx, vy;
    FluxRGB  color;
    uint64_t spawned_ms;
    int      ttl_ms;
    int      alive;
} FluxParticle;

typedef struct {
    FluxParticle p[FLUX_PARTICLE_MAX];
    int          count;
    uint64_t     now_ms;
    uint64_t     last_ms;
    unsigned     seed;
} FluxParticleBurst;

void flux_particle_burst_init  (FluxParticleBurst *pb);
void flux_particle_burst_spawn (FluxParticleBurst *pb, int x, int y,
                                FluxRGB color, int n_particles);
void flux_particle_burst_tick  (FluxParticleBurst *pb, uint64_t now_ms);
void flux_particle_burst_render(FluxParticleBurst *pb, FluxSB *sb,
                                int screen_w, int screen_h);

/* ─── 9. FluxSplitPane ────────────────────────────────────────────
 *
 * 2- or 3-pane split with focus tracking. Each pane is rendered via a
 * caller-supplied callback. Orientation = horizontal (side-by-side)
 * or vertical (top-to-bottom).
 *
 *   FluxSplitPane sp;
 *   flux_split_init(&sp, FLUX_SPLIT_HORIZONTAL, 2);
 *   FluxSplitRenderFn fns[2] = { pane_a, pane_b };
 *   void *ctxs[2] = { &a, &b };
 *   flux_split_render(&sp, sb, 80, 24, fns, ctxs);
 *
 * Tab / Shift-Tab in update() cycles focus.
 */
typedef enum {
    FLUX_SPLIT_HORIZONTAL = 0,   /* panes side-by-side */
    FLUX_SPLIT_VERTICAL          /* panes stacked      */
} FluxSplitOrient;

#ifndef FLUX_SPLIT_MAX_PANES
#define FLUX_SPLIT_MAX_PANES 3
#endif

typedef void (*FluxSplitRenderFn)(FluxSB *sb, int w, int h, int focused,
                                  void *ctx);

typedef struct {
    FluxSplitOrient orient;
    int             n_panes;
    int             focus;       /* which pane currently has focus */
    int             ratios[FLUX_SPLIT_MAX_PANES]; /* weighted sizes, 0 = equal */
} FluxSplitPane;

void flux_split_init  (FluxSplitPane *sp, FluxSplitOrient orient, int n_panes);
int  flux_split_update(FluxSplitPane *sp, FluxMsg msg);
void flux_split_render(FluxSplitPane *sp, FluxSB *sb, int w, int h,
                       FluxSplitRenderFn *fns, void **ctxs);

/* ─── 10. FluxAgentCard ───────────────────────────────────────────
 *
 * Orchestra hero card: channel badge + blink dot + tool line + mini
 * sparkline of token-rate + elapsed/cost + rounded border. Border
 * pulses gently when focused (caller animates `pulse_t`).
 *
 *   FluxAgentCard c = {
 *      .name = "Tester", .channel = FLUX_CH_AUTO,
 *      .state = FLUX_DOT_RUNNING, .current_tool = "pytest -q",
 *      .tokens_total = 14203, .cost_usd = 0.12f, .elapsed_ms = 8421,
 *      .focused = 1, .pulse_t = 0.4f,
 *   };
 *   // caller fills token_rate_ring[0..31] with tok/s values.
 *   flux_agent_card_render(&c, sb, 30);
 * Emits exactly `height=7` rows of `width` cells.
 */
typedef struct {
    const char       *name;
    FluxChannelId     channel;
    FluxDotState      state;
    const char       *current_tool;
    float             token_rate_ring[32];
    int               token_rate_head;
    long              tokens_total;
    float             cost_usd;
    int               elapsed_ms;
    int               focused;
    float             pulse_t;   /* [0,1] driven by caller via easing */
} FluxAgentCard;

#define FLUX_AGENT_CARD_H 7

void flux_agent_card_render(const FluxAgentCard *card, FluxSB *sb, int width);

/* ─── 11. FluxRequestTrace ────────────────────────────────────────
 *
 * Expandable HTTP request row. Collapsed = one line:
 *   [POST] /v1/chat        200  412ms  1.2KB
 * Expanded = adds headers + request body + response body (rendered
 * via flux_pretty for JSON-looking payloads). Caller owns the string
 * buffers.
 *
 *   FluxRequestTrace tr = { "POST", "/v1/chat", 200, 412, 1234,
 *                           "Authorization: ...\nContent-Type: json",
 *                           "{\"model\":\"opus\"}",
 *                           "{\"id\":\"msg_01\"}" };
 *   flux_request_trace_render(&tr, sb, width, 1);
 * Collapsed emits 1 row. Expanded emits variable rows — see doc.
 */
typedef struct {
    const char *method;       /* GET / POST / ... */
    const char *path;
    int         status;
    int         latency_ms;
    long        size_bytes;
    const char *headers;      /* '\n'-separated, may be NULL */
    const char *body;         /* request body, may be NULL */
    const char *response;     /* response body, may be NULL */
} FluxRequestTrace;

void flux_request_trace_render(const FluxRequestTrace *tr, FluxSB *sb,
                               int width, int expanded);


#ifdef __cplusplus
}
#endif
#endif /* FLUX_H */


/* ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION
 * ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════ */
#if defined(FLUX_IMPL) && !defined(FLUX_IMPL_DONE)
#define FLUX_IMPL_DONE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <stdint.h>

/* ══════════════════════════════════════════════════════════════════
 * SPINNER FRAME ARRAYS (definitions)
 * ═════════════════════════════════════════════════════════════════ */

FLUX_UNUSED_ const char *FLUX_SPINNER_DOT[] = {
    "\xe2\xa0\x8b", /* braille dot 1 */
    "\xe2\xa0\x99", /* braille dot 2 */
    "\xe2\xa0\xb9", /* braille dot 3 */
    "\xe2\xa0\xb8", /* braille dot 4 */
    "\xe2\xa0\xbc", /* braille dot 5 */
    "\xe2\xa0\xb4", /* braille dot 6 */
    "\xe2\xa0\xa6", /* braille dot 7 */
    "\xe2\xa0\xa7", /* braille dot 8 */
    "\xe2\xa0\x87", /* braille dot 9 */
    "\xe2\xa0\x8f"  /* braille dot 10 */
};
FLUX_UNUSED_ const int FLUX_SPINNER_DOT_N = 10;

FLUX_UNUSED_ const char *FLUX_SPINNER_LINE[] = {
    "\xe2\xa0\x82", /* braille line 1 */
    "\xe2\xa0\x92", /* braille line 2 */
    "\xe2\xa0\x90", /* braille line 3 */
    "\xe2\xa0\xb0", /* braille line 4 */
    "\xe2\xa0\xa0", /* braille line 5 */
    "\xe2\xa0\xa4", /* braille line 6 */
    "\xe2\xa0\x84", /* braille line 7 */
    "\xe2\xa0\x86"  /* braille line 8 */
};
FLUX_UNUSED_ const int FLUX_SPINNER_LINE_N = 8;

/* ══════════════════════════════════════════════════════════════════
 * BORDER PRESETS (definitions)
 * ═════════════════════════════════════════════════════════════════ */

const FluxBorder FLUX_BORDER_ROUNDED =
    {"╭","╮","╰","╯","─","│","├","┤","┬","┴","┼"};
const FluxBorder FLUX_BORDER_SHARP =
    {"┌","┐","└","┘","─","│","├","┤","┬","┴","┼"};
const FluxBorder FLUX_BORDER_DOUBLE =
    {"╔","╗","╚","╝","═","║","╠","╣","╦","╩","╬"};
const FluxBorder FLUX_BORDER_THICK =
    {"┏","┓","┗","┛","━","┃","┣","┫","┳","┻","╋"};
const FluxBorder FLUX_BORDER_NONE =
    {" "," "," "," "," "," "," "," "," "," "," "};


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 1: Screen + Style  (01_screen_style.c)              ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* -- Style Pool -- */

void flux_style_pool_init(FluxStylePool *pool) {
    memset(pool, 0, sizeof(*pool));
}

int flux_style_eq(const FluxStyle *a, const FluxStyle *b) {
    return a->fg_r == b->fg_r && a->fg_g == b->fg_g && a->fg_b == b->fg_b
        && a->bg_r == b->bg_r && a->bg_g == b->bg_g && a->bg_b == b->bg_b
        && a->bold == b->bold && a->dim == b->dim
        && a->italic == b->italic && a->underline == b->underline
        && a->strikethrough == b->strikethrough && a->inverse == b->inverse;
}

int flux_style_pool_intern(FluxStylePool *pool, const FluxStyle *s) {
    int i;
    for (i = 0; i < pool->count; i++) {
        if (flux_style_eq(&pool->styles[i], s)) {
            return i;
        }
    }
    if (pool->count >= FLUX_STYLE_POOL) {
        return -1;
    }
    pool->styles[pool->count] = *s;
    return pool->count++;
}

/* -- Style to ANSI -- */

int flux_style_to_ansi(const FluxStyle *s, const FluxStyle *prev,
                              char *buf, int bufsz) {
    int off = 0;
    int need_reset = 0;
    int have_attrs = 0;

    if (bufsz <= 0) {
        return 0;
    }
    buf[0] = '\0';

    if (prev == NULL) {
        need_reset = 1;
    } else {
        if ((prev->bold && !s->bold)
            || (prev->dim && !s->dim)
            || (prev->italic && !s->italic)
            || (prev->underline && !s->underline)
            || (prev->strikethrough && !s->strikethrough)
            || (prev->inverse && !s->inverse)) {
            need_reset = 1;
        }
        if ((prev->fg_r >= 0 && s->fg_r < 0)
            || (prev->bg_r >= 0 && s->bg_r < 0)) {
            need_reset = 1;
        }
    }

    off += snprintf(buf + off, bufsz - off, "\x1b[");
    if (off >= bufsz) {
        return bufsz - 1;
    }

    if (need_reset) {
        off += snprintf(buf + off, bufsz - off, "0");
        if (off >= bufsz) {
            return bufsz - 1;
        }
        have_attrs = 1;

        if (s->bold) {
            off += snprintf(buf + off, bufsz - off, ";1");
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->dim) {
            off += snprintf(buf + off, bufsz - off, ";2");
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->italic) {
            off += snprintf(buf + off, bufsz - off, ";3");
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->underline) {
            off += snprintf(buf + off, bufsz - off, ";4");
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->inverse) {
            off += snprintf(buf + off, bufsz - off, ";7");
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->strikethrough) {
            off += snprintf(buf + off, bufsz - off, ";9");
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->fg_r >= 0) {
            off += snprintf(buf + off, bufsz - off, ";38;2;%d;%d;%d",
                            s->fg_r, s->fg_g, s->fg_b);
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->bg_r >= 0) {
            off += snprintf(buf + off, bufsz - off, ";48;2;%d;%d;%d",
                            s->bg_r, s->bg_g, s->bg_b);
            if (off >= bufsz) { return bufsz - 1; }
        }
    } else {
        if (s->bold && !prev->bold) {
            off += snprintf(buf + off, bufsz - off, "%s1",
                            have_attrs ? ";" : "");
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->dim && !prev->dim) {
            off += snprintf(buf + off, bufsz - off, "%s2",
                            have_attrs ? ";" : "");
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->italic && !prev->italic) {
            off += snprintf(buf + off, bufsz - off, "%s3",
                            have_attrs ? ";" : "");
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->underline && !prev->underline) {
            off += snprintf(buf + off, bufsz - off, "%s4",
                            have_attrs ? ";" : "");
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->inverse && !prev->inverse) {
            off += snprintf(buf + off, bufsz - off, "%s7",
                            have_attrs ? ";" : "");
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->strikethrough && !prev->strikethrough) {
            off += snprintf(buf + off, bufsz - off, "%s9",
                            have_attrs ? ";" : "");
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->fg_r >= 0
            && (s->fg_r != prev->fg_r || s->fg_g != prev->fg_g
                || s->fg_b != prev->fg_b)) {
            off += snprintf(buf + off, bufsz - off, "%s38;2;%d;%d;%d",
                            have_attrs ? ";" : "",
                            s->fg_r, s->fg_g, s->fg_b);
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
        if (s->bg_r >= 0
            && (s->bg_r != prev->bg_r || s->bg_g != prev->bg_g
                || s->bg_b != prev->bg_b)) {
            off += snprintf(buf + off, bufsz - off, "%s48;2;%d;%d;%d",
                            have_attrs ? ";" : "",
                            s->bg_r, s->bg_g, s->bg_b);
            have_attrs = 1;
            if (off >= bufsz) { return bufsz - 1; }
        }
    }

    if (!have_attrs) {
        buf[0] = '\0';
        return 0;
    }

    off += snprintf(buf + off, bufsz - off, "m");
    if (off >= bufsz) {
        return bufsz - 1;
    }
    return off;
}

/* -- Cell comparison -- */

int flux_cell_eq(const FluxCell *a, const FluxCell *b) {
    return a->style_id == b->style_id
        && a->width == b->width
        && memcmp(a->ch, b->ch, FLUX_CELL_CH_MAX) == 0;
}

/* -- Screen buffer -- */

void flux_screen_clear(FluxScreen *scr) {
    int i;
    int total = scr->rows * scr->cols;
    for (i = 0; i < total; i++) {
        memset(scr->cells[i].ch, 0, FLUX_CELL_CH_MAX);
        scr->cells[i].ch[0] = ' ';
        scr->cells[i].style_id = -1;
        scr->cells[i].width = 1;
    }
    scr->damage_x1 = 0;
    scr->damage_y1 = 0;
    scr->damage_x2 = scr->cols;
    scr->damage_y2 = scr->rows;
}

int flux_screen_init(FluxScreen *scr, int rows, int cols) {
    scr->rows = rows;
    scr->cols = cols;
    scr->cells = (FluxCell *)malloc((size_t)rows * (size_t)cols * sizeof(FluxCell));
    if (!scr->cells) {
        scr->rows = 0;
        scr->cols = 0;
        return -1;
    }
    flux_screen_clear(scr);
    return 0;
}

void flux_screen_free(FluxScreen *scr) {
    free(scr->cells);
    scr->cells = NULL;
    scr->rows = 0;
    scr->cols = 0;
}

int flux_screen_resize(FluxScreen *scr, int rows, int cols) {
    FluxCell *new_cells;
    int copy_rows, copy_cols;
    int y;

    if (rows == scr->rows && cols == scr->cols) {
        return 0;
    }

    new_cells = (FluxCell *)malloc((size_t)rows * (size_t)cols * sizeof(FluxCell));
    if (!new_cells) {
        return -1;
    }

    {
        int i;
        int total = rows * cols;
        for (i = 0; i < total; i++) {
            memset(new_cells[i].ch, 0, FLUX_CELL_CH_MAX);
            new_cells[i].ch[0] = ' ';
            new_cells[i].style_id = -1;
            new_cells[i].width = 1;
        }
    }

    copy_rows = scr->rows < rows ? scr->rows : rows;
    copy_cols = scr->cols < cols ? scr->cols : cols;

    for (y = 0; y < copy_rows; y++) {
        memcpy(&new_cells[y * cols],
               &scr->cells[y * scr->cols],
               (size_t)copy_cols * sizeof(FluxCell));
    }

    free(scr->cells);
    scr->cells = new_cells;
    scr->rows = rows;
    scr->cols = cols;

    scr->damage_x1 = 0;
    scr->damage_y1 = 0;
    scr->damage_x2 = cols;
    scr->damage_y2 = rows;

    return 0;
}

void flux_screen_set_cell(FluxScreen *scr, int x, int y,
                                 const char *ch, int ch_len,
                                 int style_id, int width) {
    FluxCell *cell;
    int copy_len;

    if (x < 0 || x >= scr->cols || y < 0 || y >= scr->rows) {
        return;
    }

    cell = &scr->cells[y * scr->cols + x];

    copy_len = ch_len < FLUX_CELL_CH_MAX - 1 ? ch_len : FLUX_CELL_CH_MAX - 1;
    memset(cell->ch, 0, FLUX_CELL_CH_MAX);
    if (ch && copy_len > 0) {
        memcpy(cell->ch, ch, (size_t)copy_len);
    }

    cell->style_id = (int16_t)style_id;
    cell->width = (int8_t)width;

    if (scr->damage_x1 > scr->damage_x2) {
        scr->damage_x1 = x;
        scr->damage_y1 = y;
        scr->damage_x2 = x + 1;
        scr->damage_y2 = y + 1;
    } else {
        if (x < scr->damage_x1) { scr->damage_x1 = x; }
        if (y < scr->damage_y1) { scr->damage_y1 = y; }
        if (x + 1 > scr->damage_x2) { scr->damage_x2 = x + 1; }
        if (y + 1 > scr->damage_y2) { scr->damage_y2 = y + 1; }
    }
}

const FluxCell *flux_screen_get_cell(const FluxScreen *scr, int x, int y) {
    if (x < 0 || x >= scr->cols || y < 0 || y >= scr->rows) {
        return NULL;
    }
    return &scr->cells[y * scr->cols + x];
}

void flux_screen_damage_reset(FluxScreen *scr) {
    scr->damage_x1 = scr->cols;
    scr->damage_y1 = scr->rows;
    scr->damage_x2 = 0;
    scr->damage_y2 = 0;
}

void flux_screen_damage_mark(FluxScreen *scr,
                                    int x, int y, int w, int h) {
    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }
    if (x2 > scr->cols) { x2 = scr->cols; }
    if (y2 > scr->rows) { y2 = scr->rows; }

    if (x >= x2 || y >= y2) {
        return;
    }

    if (scr->damage_x1 > scr->damage_x2) {
        scr->damage_x1 = x;
        scr->damage_y1 = y;
        scr->damage_x2 = x2;
        scr->damage_y2 = y2;
    } else {
        if (x < scr->damage_x1) { scr->damage_x1 = x; }
        if (y < scr->damage_y1) { scr->damage_y1 = y; }
        if (x2 > scr->damage_x2) { scr->damage_x2 = x2; }
        if (y2 > scr->damage_y2) { scr->damage_y2 = y2; }
    }
}

void flux_screen_blit(FluxScreen *dst, const FluxScreen *src,
                             int sx, int sy, int dx, int dy, int w, int h) {
    int y;
    int src_x_start, src_y_start;
    int dst_x_start, dst_y_start;
    int copy_w, copy_h;

    src_x_start = sx < 0 ? 0 : sx;
    src_y_start = sy < 0 ? 0 : sy;

    dst_x_start = dx + (src_x_start - sx);
    dst_y_start = dy + (src_y_start - sy);

    copy_w = w - (src_x_start - sx);
    copy_h = h - (src_y_start - sy);

    if (src_x_start + copy_w > src->cols) {
        copy_w = src->cols - src_x_start;
    }
    if (src_y_start + copy_h > src->rows) {
        copy_h = src->rows - src_y_start;
    }

    if (dst_x_start < 0) {
        copy_w += dst_x_start;
        src_x_start -= dst_x_start;
        dst_x_start = 0;
    }
    if (dst_y_start < 0) {
        copy_h += dst_y_start;
        src_y_start -= dst_y_start;
        dst_y_start = 0;
    }
    if (dst_x_start + copy_w > dst->cols) {
        copy_w = dst->cols - dst_x_start;
    }
    if (dst_y_start + copy_h > dst->rows) {
        copy_h = dst->rows - dst_y_start;
    }

    if (copy_w <= 0 || copy_h <= 0) {
        return;
    }

    for (y = 0; y < copy_h; y++) {
        memcpy(&dst->cells[(dst_y_start + y) * dst->cols + dst_x_start],
               &src->cells[(src_y_start + y) * src->cols + src_x_start],
               (size_t)copy_w * sizeof(FluxCell));
    }

    flux_screen_damage_mark(dst, dst_x_start, dst_y_start, copy_w, copy_h);
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 2: Node tree  (02_node_tree.c)                      ║
 * ╚════════════════════════════════════════════════════════════════╝ */

void flux_pool_init(FluxNodePool *pool) {
    memset(pool, 0, sizeof(*pool));
    pool->next_id = 1;
}

FluxNode *flux_node_alloc(FluxNodePool *pool, FluxNodeType type) {
    if (!pool) return NULL;

    int i;
    for (i = 0; i < FLUX_MAX_NODES; i++) {
        if (pool->nodes[i].id == 0) {
            FluxNode *n = &pool->nodes[i];
            memset(n, 0, sizeof(*n));
            n->type = type;
            n->id = pool->next_id++;
            n->dirty = 1;
            n->layout_dirty = 1;
            n->style.fg_r = -1;
            n->style.fg_g = -1;
            n->style.fg_b = -1;
            n->style.bg_r = -1;
            n->style.bg_g = -1;
            n->style.bg_b = -1;
            pool->count++;
            return n;
        }
    }
    return NULL;
}

void flux_node_free(FluxNodePool *pool, FluxNode *node);
void flux_node_remove_child(FluxNode *parent, FluxNode *child);
void flux_node_mark_dirty(FluxNode *node);
void flux_node_mark_layout_dirty(FluxNode *node);

void flux_node_free(FluxNodePool *pool, FluxNode *node) {
    if (!pool || !node || node->id == 0) return;

    while (node->child_count > 0) {
        flux_node_free(pool, node->children[node->child_count - 1]);
    }

    if (node->parent) {
        flux_node_remove_child(node->parent, node);
    }

    if (node->text) {
        free(node->text);
        node->text = NULL;
    }

    node->id = 0;
    pool->count--;
}

void flux_node_add_child(FluxNode *parent, FluxNode *child) {
    if (!parent || !child) return;
    if (parent->child_count >= FLUX_MAX_CHILDREN) return;

    if (child->parent && child->parent != parent) {
        flux_node_remove_child(child->parent, child);
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
    flux_node_mark_dirty(parent);
    flux_node_mark_layout_dirty(parent);
}

void flux_node_remove_child(FluxNode *parent, FluxNode *child) {
    if (!parent || !child) return;

    int i;
    for (i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            int j;
            for (j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->children[parent->child_count - 1] = NULL;
            parent->child_count--;
            child->parent = NULL;
            flux_node_mark_dirty(parent);
            flux_node_mark_layout_dirty(parent);
            return;
        }
    }
}

void flux_node_set_text(FluxNode *node, const char *text) {
    if (!node) return;

    if (node->text) {
        free(node->text);
        node->text = NULL;
        node->text_len = 0;
    }

    if (text) {
        int len = (int)strlen(text);
        node->text = (char *)malloc(len + 1);
        if (node->text) {
            memcpy(node->text, text, len + 1);
            node->text_len = len;
        }
    }

    flux_node_mark_dirty(node);
}

void flux_node_set_style(FluxNode *node, FluxStyle style) {
    if (!node) return;
    node->style = style;
    flux_node_mark_dirty(node);
}

void flux_node_set_size(FluxNode *node, FluxSize w, FluxSize h) {
    if (!node) return;
    node->width = w;
    node->height = h;
    flux_node_mark_layout_dirty(node);
}

void flux_node_mark_dirty(FluxNode *node) {
    FluxNode *cur;
    if (!node) return;

    for (cur = node; cur != NULL; cur = cur->parent) {
        if (cur->dirty) break;
        cur->dirty = 1;
    }
}

void flux_node_mark_layout_dirty(FluxNode *node) {
    FluxNode *cur;
    if (!node) return;

    for (cur = node; cur != NULL; cur = cur->parent) {
        if (cur->layout_dirty) break;
        cur->layout_dirty = 1;
    }
}

void flux_node_clear_dirty(FluxNode *node) {
    int i;
    if (!node) return;

    node->dirty = 0;
    node->layout_dirty = 0;

    for (i = 0; i < node->child_count; i++) {
        flux_node_clear_dirty(node->children[i]);
    }
}

int flux_node_is_clean(const FluxNode *node) {
    int i;
    if (!node) return 1;
    if (node->dirty || node->layout_dirty) return 0;

    for (i = 0; i < node->child_count; i++) {
        if (!flux_node_is_clean(node->children[i])) return 0;
    }
    return 1;
}

FluxNode *flux_node_box(FluxNodePool *pool, FluxDirection dir) {
    FluxNode *n = flux_node_alloc(pool, FLUX_NODE_BOX);
    if (n) {
        n->direction = dir;
    }
    return n;
}

FluxNode *flux_node_text(FluxNodePool *pool, const char *text,
                                FluxStyle style) {
    FluxNode *n = flux_node_alloc(pool, FLUX_NODE_TEXT);
    if (n) {
        n->style = style;
        if (text) {
            int len = (int)strlen(text);
            n->text = (char *)malloc(len + 1);
            if (n->text) {
                memcpy(n->text, text, len + 1);
                n->text_len = len;
            }
        }
        n->wrap = 1;
    }
    return n;
}

FluxNode *flux_node_raw(FluxNodePool *pool, const char *ansi_str) {
    FluxNode *n = flux_node_alloc(pool, FLUX_NODE_RAW);
    if (n && ansi_str) {
        int len = (int)strlen(ansi_str);
        n->text = (char *)malloc(len + 1);
        if (n->text) {
            memcpy(n->text, ansi_str, len + 1);
            n->text_len = len;
        }
    }
    return n;
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 3: Text utilities + String builder  (07)            ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* ── Unicode ranges (issue #351) ──────────────────────────────────── *
 *
 * Single source of truth for all width/cluster math below. Block
 * boundaries are taken straight from the Unicode 15 charts; the macros
 * are deliberately structural (block-level) so we stop the per-emoji
 * whack-a-mole that earlier iterations of `_flux_wt_is_wide_cp` did.
 *
 * Width policy (chrome-safe, terminal-realistic):
 *   - East_Asian_Width = W or F → 2 cells.
 *   - Misc Symbols 2600-26FF + CJK Symbols 3000-303F + Enclosed CJK
 *     3200-33FF → 2 cells. Modern terminals (kitty/iTerm/wezterm) all
 *     render these as wide whether or not the user types VS16.
 *   - Box drawing 2500-257F + Block Elements 2580-259F + Geometric
 *     Shapes 25A0-25FF + Dingbats 2700-27BF → 1 cell. The TUI's
 *     chrome (╭ ╮ │ ─ ├ ┤ ░ ▒ █ ◆ ◉ ● ✓ ✗ ❯) lives there.
 *   - Codepoints inside the chrome-narrow ranges that ARE
 *     emoji-presentation (✅ ❌ ❓ ❗ ➕ ✨ ⌚ ⏰ ◽ ⬛ ⬜ ⭐ ⭕) are
 *     re-promoted to wide via FLUX_CP_IS_BMP_EMOJI_FORCED_WIDE.
 *   - SMP emoji (1F000..1FFFD) → 2 cells (every terminal agrees).
 *   - Modifiers (Fitzpatrick, VS, ZWJ, keycap, tags, RI continuation)
 *     → 0 cells; consumed by `flux_grapheme_advance` into the cluster.
 *   - VS16 (FE0F) inside `flux_grapheme_advance` promotes a default-
 *     narrow base (e.g. ⬆) to wide (⬆️).
 *
 * `flux_cp_width()` is the stateless oracle. `flux_grapheme_advance()`
 * is the stateful walker that handles RI flag pairs + ZWJ chains by
 * remembering the previous codepoint inside the cluster. */

#define FLUX_IN_RANGE(cp, lo, hi) \
    ((unsigned int)(cp) >= (unsigned int)(lo) && \
     (unsigned int)(cp) <= (unsigned int)(hi))

/* East_Asian_Width = W / F (canonical wide). */
#define FLUX_CP_IS_EAW_WIDE(cp) ( \
    FLUX_IN_RANGE(cp, 0x1100,  0x115F) || /* Hangul Jamo */            \
    FLUX_IN_RANGE(cp, 0x2329,  0x232A) || /* angle brackets */         \
    FLUX_IN_RANGE(cp, 0x2E80,  0x303E) || /* CJK Radicals + punct */   \
    FLUX_IN_RANGE(cp, 0x3041,  0x33FF) || /* Kana + CJK compat */      \
    FLUX_IN_RANGE(cp, 0x3400,  0x4DBF) || /* CJK Ext-A */              \
    FLUX_IN_RANGE(cp, 0x4E00,  0x9FFF) || /* CJK Unified */            \
    FLUX_IN_RANGE(cp, 0xA000,  0xA4CF) || /* Yi */                     \
    FLUX_IN_RANGE(cp, 0xA960,  0xA97F) || /* Hangul Jamo Ext-A */      \
    FLUX_IN_RANGE(cp, 0xAC00,  0xD7A3) || /* Hangul Syllables */       \
    FLUX_IN_RANGE(cp, 0xF900,  0xFAFF) || /* CJK Compat Ideographs */  \
    FLUX_IN_RANGE(cp, 0xFE10,  0xFE19) || /* Vertical Forms */         \
    FLUX_IN_RANGE(cp, 0xFE30,  0xFE6F) || /* CJK Compat Forms */       \
    FLUX_IN_RANGE(cp, 0xFF00,  0xFF60) || /* Fullwidth ASCII */        \
    FLUX_IN_RANGE(cp, 0xFFE0,  0xFFE6) || /* Fullwidth signs */        \
    FLUX_IN_RANGE(cp, 0x1B000, 0x1B0FF) || /* Kana Supplement */       \
    FLUX_IN_RANGE(cp, 0x20000, 0x2A6DF) || /* CJK Ext-B */             \
    FLUX_IN_RANGE(cp, 0x2A700, 0x2CEAF) || /* CJK Ext-C/D/E */         \
    FLUX_IN_RANGE(cp, 0x2CEB0, 0x2EBEF) || /* CJK Ext-F */             \
    FLUX_IN_RANGE(cp, 0x2F800, 0x2FA1F)    /* CJK Compat Supp */       \
)

/* SMP emoji blocks that are uniformly wide. The carve-outs around
 * 0x1F000-0x1F0FF (mahjong/domino/playing cards) are terminal-renderer
 * dependent; we mark the singleton wide ones explicitly. */
#define FLUX_CP_IS_SMP_EMOJI_WIDE(cp) ( \
    (cp) == 0x1F004 || /* mahjong red dragon */                        \
    (cp) == 0x1F0CF || /* playing card joker */                        \
    FLUX_IN_RANGE(cp, 0x1F100, 0x1F1FF) || /* Enclosed Alphanum Supp */\
    FLUX_IN_RANGE(cp, 0x1F200, 0x1F2FF) || /* Enclosed Ideographic */  \
    FLUX_IN_RANGE(cp, 0x1F300, 0x1F64F) || /* Misc Sym + Emoticons */  \
    FLUX_IN_RANGE(cp, 0x1F680, 0x1F6FF) || /* Transport + Map */       \
    FLUX_IN_RANGE(cp, 0x1F780, 0x1F7FF) || /* Geometric Shapes Ext */  \
    FLUX_IN_RANGE(cp, 0x1F900, 0x1F9FF) || /* Supp Sym + Pictographs */\
    FLUX_IN_RANGE(cp, 0x1FA00, 0x1FAFF)    /* Sym + Pictographs Ext-A*/\
)

/* BMP blocks where the chrome glyphs the TUI uses live. Treated as
 * narrow so the cell-diff stays in sync with what terminals render
 * for `╭ ╮ │ ─ ├ ┤ ░ ▒ █ ◆ ◉ ● ✓ ✗ ❯` etc. Specific emoji-
 * presentation codepoints inside these ranges are re-promoted to
 * wide via FLUX_CP_IS_BMP_EMOJI_FORCED_WIDE below. */
#define FLUX_CP_IS_CHROME_NARROW(cp) ( \
    FLUX_IN_RANGE(cp, 0x2300, 0x23FF) || /* Misc Technical */          \
    FLUX_IN_RANGE(cp, 0x2500, 0x257F) || /* Box Drawing */             \
    FLUX_IN_RANGE(cp, 0x2580, 0x259F) || /* Block Elements */          \
    FLUX_IN_RANGE(cp, 0x25A0, 0x25FF) || /* Geometric Shapes */        \
    FLUX_IN_RANGE(cp, 0x2700, 0x27BF)    /* Dingbats */                \
)

/* Codepoints inside the chrome-narrow ranges (Misc Technical, Box
 * Drawing, Block Elements, Geometric Shapes, Dingbats) that have
 * Emoji_Presentation=Yes per Unicode 15.1 emoji-data.txt and are
 * therefore rendered 2 cells by every modern terminal. Listing these
 * explicitly keeps the chrome glyphs (✓ ✗ ╭ ╮ │ ◆ ◉ etc., all
 * Emoji_Presentation=No) at 1 cell. Codepoints outside the
 * chrome-narrow ranges don't go through this gate — they fall to
 * the broad Misc-Symbols / SMP / EAW rules. */
#define FLUX_CP_IS_BMP_EMOJI_FORCED_WIDE(cp) ( \
    /* Misc Technical (2300-23FF) — the time/playback set */           \
    (cp) == 0x231A || (cp) == 0x231B ||                                \
    (cp) == 0x23E9 || (cp) == 0x23EA ||                                \
    (cp) == 0x23EB || (cp) == 0x23EC ||                                \
    (cp) == 0x23F0 || (cp) == 0x23F3 ||                                \
    /* Geometric Shapes (25A0-25FF) — small/medium black/white sq */   \
    (cp) == 0x25FD || (cp) == 0x25FE ||                                \
    /* Dingbats (2700-27BF) — emoji-presentation set */                \
    (cp) == 0x2705 || (cp) == 0x270A ||                                \
    (cp) == 0x270B || (cp) == 0x2728 ||                                \
    (cp) == 0x274C || (cp) == 0x274E ||                                \
    (cp) == 0x2753 || (cp) == 0x2754 ||                                \
    (cp) == 0x2755 || (cp) == 0x2757 ||                                \
    (cp) == 0x2795 || (cp) == 0x2796 || (cp) == 0x2797 ||              \
    (cp) == 0x27B0 || (cp) == 0x27BF                                   \
)

/* Misc Sym Arrows (2B00-2BFF) Emoji_Presentation=Yes set. Sits
 * outside the chrome-narrow ranges so doesn't need the override
 * gate, but lives here next to its peers. */
#define FLUX_CP_IS_MISC_SYM_ARROWS_EMOJI(cp) ( \
    (cp) == 0x2B1B || (cp) == 0x2B1C ||                                \
    (cp) == 0x2B50 || (cp) == 0x2B55                                   \
)

/* BMP wide set. Modern terminals render Misc Symbols (2600-26FF) as
 * wide regardless of VS16 — match that. Add the Misc Sym Arrows
 * emoji set, the chrome-range emoji-presentation overrides, and the
 * EAW-wide CJK blocks. Anything inside the chrome-narrow ranges
 * that's NOT in the override list stays 1 cell (chrome safety). */
#define FLUX_CP_IS_BMP_EMOJI_WIDE(cp) ( \
    FLUX_CP_IS_BMP_EMOJI_FORCED_WIDE(cp) ||                            \
    FLUX_CP_IS_MISC_SYM_ARROWS_EMOJI(cp) ||                            \
    (!FLUX_CP_IS_CHROME_NARROW(cp) && (                                \
        FLUX_IN_RANGE(cp, 0x2600, 0x26FF) ||                           \
        FLUX_IN_RANGE(cp, 0x3000, 0x303F) ||                           \
        FLUX_IN_RANGE(cp, 0x3200, 0x33FF)))                            \
)

/* Zero-width modifiers / joiners. Consumed into a grapheme cluster by
 * flux_grapheme_advance — never produce their own cell. */
#define FLUX_CP_IS_ZWJ(cp)              ((cp) == 0x200D)
#define FLUX_CP_IS_KEYCAP_COMBINER(cp)  ((cp) == 0x20E3)
#define FLUX_CP_IS_VS(cp)               FLUX_IN_RANGE(cp, 0xFE00, 0xFE0F)
#define FLUX_CP_IS_VS_SUPP(cp)          FLUX_IN_RANGE(cp, 0xE0100, 0xE01EF)
#define FLUX_CP_IS_FITZPATRICK(cp)      FLUX_IN_RANGE(cp, 0x1F3FB, 0x1F3FF)
#define FLUX_CP_IS_TAG(cp)              FLUX_IN_RANGE(cp, 0xE0000, 0xE007F)
#define FLUX_CP_IS_REGIONAL(cp)         FLUX_IN_RANGE(cp, 0x1F1E6, 0x1F1FF)
#define FLUX_CP_IS_COMBINING(cp)        FLUX_IN_RANGE(cp, 0x0300, 0x036F)

/* Stateless per-codepoint cell width: 0 (modifier), 1 (narrow),
 * 2 (wide). Use flux_grapheme_advance for cluster-correct width. */
static int flux_cp_width(unsigned int cp) {
    /* Zero-width: combining marks, joiners, modifiers. */
    if (cp == 0)                       return 0;
    if (FLUX_CP_IS_ZWJ(cp))            return 0;
    if (FLUX_CP_IS_VS(cp))             return 0;
    if (FLUX_CP_IS_VS_SUPP(cp))        return 0;
    if (FLUX_CP_IS_FITZPATRICK(cp))    return 0;
    if (FLUX_CP_IS_TAG(cp))            return 0;
    if (FLUX_CP_IS_KEYCAP_COMBINER(cp))return 0;
    if (FLUX_CP_IS_COMBINING(cp))      return 0;
    /* Wide ranges. */
    if (FLUX_CP_IS_EAW_WIDE(cp))       return 2;
    if (FLUX_CP_IS_BMP_EMOJI_WIDE(cp)) return 2;
    if (FLUX_CP_IS_SMP_EMOJI_WIDE(cp)) return 2;
    /* Default narrow. */
    return 1;
}

/* Decode one UTF-8 codepoint from `s` (length `len`).
 * Returns bytes consumed (0 on bad input). Writes codepoint to *cp. */
static int flux_utf8_decode(const char *s, int len, unsigned int *cp) {
    if (len < 1) return 0;
    unsigned char c = (unsigned char)s[0];
    int seq;
    unsigned int v;
    if (c < 0x80)         { *cp = c; return 1; }
    else if (c < 0xC0)    { *cp = c; return 1; } /* stray continuation */
    else if (c < 0xE0)    { seq = 2; v = c & 0x1F; }
    else if (c < 0xF0)    { seq = 3; v = c & 0x0F; }
    else                  { seq = 4; v = c & 0x07; }
    if (seq > len) return 0;
    for (int k = 1; k < seq; k++) {
        unsigned char cc = (unsigned char)s[k];
        if ((cc & 0xC0) != 0x80) return 0;
        v = (v << 6) | (cc & 0x3F);
    }
    *cp = v;
    return seq;
}

/* Walk one grapheme cluster from `s`. A subset of UAX #29 sufficient
 * for emoji + chrome content:
 *   - Base codepoint sets the cluster's display width.
 *   - Continuation codepoints absorbed into the cluster: ZWJ, VS,
 *     supplementary VS, Fitzpatrick, keycap combiner, tags, combining
 *     marks. Each contributes 0 cells.
 *   - After ZWJ, the next codepoint joins the cluster regardless of
 *     type (this covers ZWJ family / profession / flag sequences).
 *   - Two consecutive Regional Indicators form one cluster (a flag);
 *     a third RI starts a new cluster.
 *
 * Writes the byte count of the cluster into *bytes_out and the
 * display cell width into *cells_out. Returns 0 if input is empty.
 *
 * `*prev_was_ri_in_out` carries one bit of state across calls so a
 * caller walking a string can detect "second of a flag pair vs.
 * lone RI" — pass &flag where flag starts at 0 and let this fn
 * update it. */
static int flux_grapheme_advance(const char *s, int len,
                                 int *prev_was_ri_in_out,
                                 int *bytes_out, int *cells_out) {
    if (bytes_out)  *bytes_out = 0;
    if (cells_out)  *cells_out = 0;
    if (len <= 0 || !s) return 0;

    unsigned int cp;
    int b = flux_utf8_decode(s, len, &cp);
    if (b <= 0) {
        /* Bad byte — consume one and return zero-width to make
         * progress without crashing the renderer. */
        if (bytes_out) *bytes_out = 1;
        if (cells_out) *cells_out = 0;
        if (prev_was_ri_in_out) *prev_was_ri_in_out = 0;
        return 1;
    }

    int total_b = b;
    int width = flux_cp_width(cp);

    /* RI pairing: if the previous cluster ended with one RI and this
     * one starts with another, fold this into the same conceptual
     * cluster — the flag occupies the previous cluster's slot, so
     * this RI contributes 0 cells. The caller updates state as we
     * return; here we treat it as a fresh 0-width cluster so the
     * column accounting matches what terminals do (RI pair = 2 cells
     * total, charged to the FIRST RI). */
    if (FLUX_CP_IS_REGIONAL(cp)) {
        if (prev_was_ri_in_out && *prev_was_ri_in_out) {
            width = 0;
            *prev_was_ri_in_out = 0;
        } else {
            width = 2;  /* lone RI: pre-charge full flag width */
            if (prev_was_ri_in_out) *prev_was_ri_in_out = 1;
        }
        if (bytes_out) *bytes_out = total_b;
        if (cells_out) *cells_out = width;
        return total_b;
    }
    if (prev_was_ri_in_out) *prev_was_ri_in_out = 0;

    /* Absorb continuation codepoints into the cluster. ZWJ specifically
     * forces the NEXT codepoint to join too, even if it's a base. */
    int saw_zwj = 0;
    while (total_b < len) {
        unsigned int next_cp;
        int nb = flux_utf8_decode(s + total_b, len - total_b, &next_cp);
        if (nb <= 0) break;

        int continuation =
            FLUX_CP_IS_ZWJ(next_cp)             ||
            FLUX_CP_IS_VS(next_cp)              ||
            FLUX_CP_IS_VS_SUPP(next_cp)         ||
            FLUX_CP_IS_FITZPATRICK(next_cp)     ||
            FLUX_CP_IS_TAG(next_cp)             ||
            FLUX_CP_IS_KEYCAP_COMBINER(next_cp) ||
            FLUX_CP_IS_COMBINING(next_cp);

        if (continuation) {
            /* VS16 (FE0F) promotes a default-narrow base to wide
             * (e.g. ❤️ digit-keycap base ↑). Apply once per cluster. */
            if (next_cp == 0xFE0F && width == 1) width = 2;
            if (FLUX_CP_IS_ZWJ(next_cp)) saw_zwj = 1;
            total_b += nb;
            continue;
        }

        if (saw_zwj) {
            /* Force-join the post-ZWJ codepoint; reset zwj flag. The
             * joined codepoint's own width is contributed only if the
             * cluster had no width yet (rare; usually base set width). */
            if (width == 0) width = flux_cp_width(next_cp);
            total_b += nb;
            saw_zwj = 0;
            continue;
        }

        break;
    }

    if (bytes_out) *bytes_out = total_b;
    if (cells_out) *cells_out = width;
    return total_b;
}

/* -- Wide codepoint detection (delegates to unified `flux_cp_width`) --
 *
 * Pre-#351 this was a hand-curated mirror of flux_render__char_width;
 * the duplication kept drifting and the cell-diff would bleed when
 * one helper added a range and the other didn't. Both now route
 * through `flux_cp_width` so the contract has exactly one definition.
 * Returns 1 when the codepoint occupies 2 cells, 0 otherwise. */
static int _flux_wt_is_wide_cp(unsigned int cp) {
    return flux_cp_width(cp) == 2;
}

/* -- flux_strwidth -- */

int flux_strwidth(const char *s) {
    int w = 0;
    int ri_state = 0;

    if (!s) return 0;

    /* Cache total length so the cluster-walker doesn't re-scan to EOL
     * on every advance — without this, flux_strwidth was O(n²) on
     * long strings. */
    const char *origin = s;
    int total_len = (int)strlen(s);

    while (*s) {
        unsigned char c = (unsigned char)*s;

        /* ANSI escapes — invisible. CSI/OSC parsing matches the rest
         * of the file's escape handling. */
        if (c == '\x1b') {
            s++;
            if (*s == '[' || *s == '(') {
                s++;
                while (*s && !((*s >= 'A' && *s <= 'Z')
                            || (*s >= 'a' && *s <= 'z')
                            || *s == '~')) {
                    s++;
                }
                if (*s) s++;
            } else if (*s == ']') {
                s++;
                while (*s && *s != '\x07'
                       && !(*s == '\x1b' && *(s + 1) == '\\')) {
                    s++;
                }
                if (*s == '\x07') {
                    s++;
                } else if (*s == '\x1b') {
                    s += 2;
                }
            } else if (*s) {
                s++;
            }
            continue;
        }

        if (c == '\t') { w += 4; s++; continue; }
        if (c == '\n' || c == '\r') { s++; ri_state = 0; continue; }
        if ((c & 0xC0) == 0x80)     { s++; continue; }   /* stray cont */
        if (c < 0x20)               { s++; continue; }

        /* Cluster-correct width via the shared grapheme walker. Handles
         * RI flag pairs (US flag = 2 cells, not 4), ZWJ family chains
         * (👨‍👩‍👧 = 2 cells, not 6), Fitzpatrick skin tones (👋🏽 = 2,
         * not 4), and VS16 emoji-presentation promotion. */
        int gb, gw;
        int remaining = total_len - (int)(s - origin);
        int n = flux_grapheme_advance(s, remaining, &ri_state, &gb, &gw);
        if (n <= 0) { s++; continue; }
        w += gw;
        s += gb;
    }
    return w;
}

/* -- flux_count_text_lines -- */

int flux_count_text_lines(const char *s, int len) {
    int lines;
    int i;

    if (!s || len <= 0) return 0;

    lines = 1;
    for (i = 0; i < len; i++) {
        if (s[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

/* -- flux_wrapped_height -- */

int flux_wrapped_height(const char *text, int text_len,
                               int wrap_width) {
    int total_lines = 0;
    const char *p = text;
    const char *end = text + text_len;

    if (!text || text_len <= 0) return 0;
    if (wrap_width <= 0) wrap_width = 1;

    while (p < end) {
        const char *line_end = p;
        while (line_end < end && *line_end != '\n') {
            line_end++;
        }

        if (line_end == p) {
            total_lines++;
        } else {
            const char *lp = p;
            while (lp < line_end) {
                const char *scan = lp;
                int col = 0;
                const char *last_break = NULL;

                while (scan < line_end && col < wrap_width) {
                    unsigned char c = (unsigned char)*scan;

                    if (c == '\x1b') {
                        scan++;
                        if (scan < line_end
                            && (*scan == '[' || *scan == '(')) {
                            scan++;
                            while (scan < line_end
                                   && !((*scan >= 'A' && *scan <= 'Z')
                                     || (*scan >= 'a' && *scan <= 'z')
                                     || *scan == '~')) {
                                scan++;
                            }
                            if (scan < line_end) scan++;
                        } else if (scan < line_end && *scan == ']') {
                            scan++;
                            while (scan < line_end && *scan != '\x07'
                                   && !(*scan == '\x1b'
                                        && scan + 1 < line_end
                                        && *(scan + 1) == '\\')) {
                                scan++;
                            }
                            if (scan < line_end && *scan == '\x07') {
                                scan++;
                            } else if (scan < line_end
                                       && *scan == '\x1b') {
                                scan += 2;
                            }
                        } else if (scan < line_end) {
                            scan++;
                        }
                        continue;
                    }

                    if (c == '\t') {
                        if (col + 4 > wrap_width && col > 0) break;
                        col += 4;
                        scan++;
                        last_break = scan;
                        continue;
                    }

                    if ((c & 0xC0) == 0x80) {
                        scan++;
                        continue;
                    }

                    if (c < 0x20) {
                        scan++;
                        continue;
                    }

                    {
                        int char_w = 1;
                        int char_bytes = 1;
                        if (c >= 0xF0 && scan + 3 < line_end
                            && (scan[1] & 0xC0) == 0x80) {
                            unsigned int cp = ((c & 0x07) << 18)
                                | (((unsigned char)scan[1] & 0x3F) << 12)
                                | (((unsigned char)scan[2] & 0x3F) << 6)
                                | ((unsigned char)scan[3] & 0x3F);
                            char_w = _flux_wt_is_wide_cp(cp) ? 2 : 1;
                            char_bytes = 4;
                        } else if (c >= 0xE0 && scan + 2 < line_end
                                   && (scan[1] & 0xC0) == 0x80) {
                            unsigned int cp = ((c & 0x0F) << 12)
                                | (((unsigned char)scan[1] & 0x3F) << 6)
                                | ((unsigned char)scan[2] & 0x3F);
                            char_w = _flux_wt_is_wide_cp(cp) ? 2 : 1;
                            char_bytes = 3;
                        } else if (c >= 0xC0) {
                            char_bytes = 2;
                        }

                        if (col + char_w > wrap_width && col > 0) break;

                        if (c == ' ') {
                            last_break = scan + 1;
                        } else if (c == '-') {
                            last_break = scan + char_bytes;
                        }

                        col += char_w;
                        scan += char_bytes;
                    }
                }

                total_lines++;

                if (scan >= line_end) {
                    lp = line_end;
                } else if (last_break && last_break > lp) {
                    lp = last_break;
                } else {
                    lp = scan;
                }
            }
        }

        p = line_end;
        if (p < end && *p == '\n') {
            p++;
        }
    }

    return total_lines > 0 ? total_lines : 1;
}

/* -- FluxSB string builder -- */

void flux_sb_init(FluxSB *sb, char *backing, int cap) {
    sb->buf = backing;
    sb->len = 0;
    sb->cap = cap;
    if (cap > 0) {
        sb->buf[0] = '\0';
    }
}

void flux_sb_append(FluxSB *sb, const char *s) {
    int l;

    if (!s) return;

    l = (int)strlen(s);
    if (sb->len + l >= sb->cap - 1) {
        l = sb->cap - 1 - sb->len;
    }
    if (l <= 0) return;
    memcpy(sb->buf + sb->len, s, (size_t)l);
    sb->len += l;
    sb->buf[sb->len] = '\0';
}

void flux_sb_appendf(FluxSB *sb, const char *fmt, ...) {
    char tmp[2048];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    flux_sb_append(sb, tmp);
}

void flux_sb_nl(FluxSB *sb) {
    flux_sb_append(sb, "\n");
}

const char *flux_sb_str(FluxSB *sb) {
    return sb->buf;
}

/* -- Layout helpers -- */

void flux_divider(FluxSB *sb, int width, const char *color) {
    int i;

    if (color) flux_sb_append(sb, color);
    for (i = 0; i < width; i++) {
        flux_sb_append(sb, "\xe2\x94\x80");
    }
    if (color) flux_sb_append(sb, FLUX_RESET);
}

void flux_pad(char *dst, int dstsz, const char *src, int width) {
    int w;
    int srcbytes;
    int copy;
    int pad;
    int p;

    if (!dst || dstsz <= 0) return;
    if (!src) src = "";

    w = flux_strwidth(src);
    srcbytes = (int)strlen(src);
    copy = srcbytes < dstsz - 1 ? srcbytes : dstsz - 1;
    memcpy(dst, src, (size_t)copy);
    dst += copy;
    dstsz -= copy;

    pad = width - w;
    if (pad > 0 && dstsz > 1) {
        p = pad < dstsz - 1 ? pad : dstsz - 1;
        memset(dst, ' ', (size_t)p);
        dst += p;
        dstsz -= p;
    }
    *dst = '\0';
}

/* ── flux_truncate / flux_fit  (responsive primitives) ─────────────── */

/* Walk one "atom" starting at s[0]: either an ANSI escape (CSI/OSC) which
 * contributes 0 cells, or one UTF-8 codepoint contributing 1 or 2 cells.
 * Writes the byte count of the atom into *bytes_out and the visible cell
 * width into *cells_out. *is_escape_out is set to non-zero for escapes. */
static void _flux_atom(const char *s, int len,
                       int *bytes_out, int *cells_out, int *is_escape_out) {
    if (len <= 0) {
        *bytes_out = 0; *cells_out = 0; *is_escape_out = 0;
        return;
    }
    unsigned char c = (unsigned char)s[0];
    if (c == 0x1b) {
        /* ESC sequence: CSI = ESC[ ... final-byte (@..~), OSC = ESC] ... BEL/ST */
        int j = 1;
        if (j < len && (s[j] == '[' || s[j] == '(')) {
            j++;
            while (j < len && !((s[j] >= 'A' && s[j] <= 'Z') ||
                                (s[j] >= 'a' && s[j] <= 'z') ||
                                s[j] == '~')) j++;
            if (j < len) j++;
        } else if (j < len && s[j] == ']') {
            j++;
            while (j < len && s[j] != 0x07 &&
                   !(s[j] == 0x1b && j+1 < len && s[j+1] == '\\')) j++;
            if (j < len && s[j] == 0x07) j++;
            else if (j+1 < len && s[j] == 0x1b) j += 2;
        } else if (j < len) {
            j++;
        }
        *bytes_out = j; *cells_out = 0; *is_escape_out = 1;
        return;
    }
    /* Skip C0 control bytes other than tab — treat as zero-width zero-byte */
    if (c == '\n' || c == '\r') {
        *bytes_out = 1; *cells_out = 0; *is_escape_out = 0;
        return;
    }
    if (c == '\t') {
        *bytes_out = 1; *cells_out = 4; *is_escape_out = 0;
        return;
    }
    /* UTF-8 leading byte. Width comes from `flux_cp_width` so all the
     * zero-width cases (ZWJ, VS, VS_supp, Fitzpatrick, keycap, tags,
     * combining marks) drop to 0 automatically — the old manual
     * zero-out only covered ZWJ + VS, missing the rest. */
    unsigned int cp;
    int seq = flux_utf8_decode(s, len, &cp);
    if (seq <= 0) { seq = 1; cp = (unsigned char)s[0]; }
    *bytes_out = seq;
    *cells_out = flux_cp_width(cp);
    *is_escape_out = 0;
}

int flux_truncate(const char *text, int max_w,
                  const char *suffix, char *out, int out_cap) {
    if (!out || out_cap <= 0) return 0;
    out[0] = '\0';
    if (!text) return 0;
    if (max_w <= 0) return 0;

    if (!suffix) suffix = "\xe2\x80\xa6"; /* … */
    int suffix_w = flux_strwidth(suffix);

    /* First pass: measure total visible width. */
    int total_w = 0;
    int len = (int)strlen(text);
    int j = 0;
    while (j < len && text[j] != '\n') {
        int b, w, e;
        _flux_atom(text + j, len - j, &b, &w, &e);
        if (b == 0) break;
        total_w += w;
        j += b;
    }

    int o = 0;
    int written_w = 0;
    int budget;
    int needs_suffix = (total_w > max_w);
    if (needs_suffix && suffix_w >= max_w) {
        /* Suffix alone wider than allowed: emit prefix of suffix that fits. */
        int sj = 0, slen = (int)strlen(suffix);
        while (sj < slen) {
            int b, w, e;
            _flux_atom(suffix + sj, slen - sj, &b, &w, &e);
            if (b == 0) break;
            if (!e && written_w + w > max_w) break;
            if (o + b >= out_cap) break;
            memcpy(out + o, suffix + sj, (size_t)b);
            o  += b;
            sj += b;
            if (!e) written_w += w;
        }
        out[o] = '\0';
        return written_w;
    }
    budget = needs_suffix ? max_w - suffix_w : max_w;
    if (budget < 0) budget = 0;

    /* Walk text, copying atoms while budget allows.  Escapes always copy. */
    j = 0;
    int saw_sgr = 0;
    while (j < len && text[j] != '\n') {
        int b, w, e;
        _flux_atom(text + j, len - j, &b, &w, &e);
        if (b == 0) break;
        if (!e && written_w + w > budget) break;
        if (o + b + 1 >= out_cap) break;
        memcpy(out + o, text + j, (size_t)b);
        o += b;
        if (e) saw_sgr = 1;
        else   written_w += w;
        j += b;
    }

    if (needs_suffix) {
        const char *sfx = suffix;
        int sj = 0, slen = (int)strlen(suffix);
        /* Emit suffix verbatim. */
        while (sj < slen && o + 1 < out_cap) {
            int b, w, e;
            _flux_atom(sfx + sj, slen - sj, &b, &w, &e);
            if (b == 0 || o + b + 1 >= out_cap) break;
            memcpy(out + o, sfx + sj, (size_t)b);
            o += b;
            sj += b;
            if (!e) written_w += w;
        }
    }

    /* Defensive RESET if any SGR was emitted, so the caller's row stays clean. */
    if (saw_sgr) {
        const char *R = "\x1b[0m";
        int rl = 4;
        if (o + rl < out_cap) {
            memcpy(out + o, R, (size_t)rl);
            o += rl;
        }
    }
    out[o] = '\0';
    return written_w;
}

void flux_fit(FluxSB *sb, const char *text, int target_w,
              const char *suffix, FluxAlign align) {
    if (!sb || target_w <= 0) return;
    if (!text) text = "";

    int w = flux_strwidth(text);
    /* Stop measuring at the first newline (we treat input as single-line). */
    {
        const char *nl = strchr(text, '\n');
        if (nl) {
            int line_len = (int)(nl - text);
            char tmp[1024];
            if (line_len < (int)sizeof tmp) {
                memcpy(tmp, text, (size_t)line_len);
                tmp[line_len] = '\0';
                w = flux_strwidth(tmp);
            }
        }
    }

    char tmp[2048];
    const char *emit;
    int emit_w;

    if (w > target_w) {
        emit_w = flux_truncate(text, target_w, suffix, tmp, (int)sizeof tmp);
        emit = tmp;
    } else {
        /* Strip newline-tail if any so we never emit a real \n. */
        const char *nl = strchr(text, '\n');
        if (nl) {
            int line_len = (int)(nl - text);
            if (line_len < (int)sizeof tmp) {
                memcpy(tmp, text, (size_t)line_len);
                tmp[line_len] = '\0';
                emit = tmp;
                emit_w = flux_strwidth(tmp);
            } else {
                emit = text; emit_w = w;
            }
        } else {
            emit = text;
            emit_w = w;
        }
    }

    int pad = target_w - emit_w;
    if (pad < 0) pad = 0;
    int left_pad  = (align == FLUX_ALIGN_RIGHT)  ? pad
                  : (align == FLUX_ALIGN_CENTER) ? pad / 2
                  : 0;
    int right_pad = pad - left_pad;

    int i;
    for (i = 0; i < left_pad;  i++) flux_sb_append(sb, " ");
    flux_sb_append(sb, emit);
    for (i = 0; i < right_pad; i++) flux_sb_append(sb, " ");
}

void flux_hbox(FluxSB *sb, const char **panels, const int *widths,
                      int count, const char *gap) {
    #define _FLUX_HB_MAXLINES 512
    #define _FLUX_HB_MAXPANELS 8
    static char _hb_buf[_FLUX_HB_MAXPANELS][128*1024];
    char *_hb_lines[_FLUX_HB_MAXPANELS][_FLUX_HB_MAXLINES];
    int _hb_nlines[_FLUX_HB_MAXPANELS];
    int maxl = 0;
    int p_idx, i;

    if (!gap) gap = "  ";
    if (count > _FLUX_HB_MAXPANELS) count = _FLUX_HB_MAXPANELS;

    for (p_idx = 0; p_idx < count; p_idx++) {
        char *s;
        char *nl;

        strncpy(_hb_buf[p_idx], panels[p_idx],
                sizeof(_hb_buf[p_idx]) - 1);
        _hb_buf[p_idx][sizeof(_hb_buf[p_idx]) - 1] = '\0';
        _hb_nlines[p_idx] = 0;
        s = _hb_buf[p_idx];

        while (*s && _hb_nlines[p_idx] < _FLUX_HB_MAXLINES) {
            _hb_lines[p_idx][_hb_nlines[p_idx]++] = s;
            nl = strchr(s, '\n');
            if (!nl) break;
            *nl = '\0';
            s = nl + 1;
        }
        if (_hb_nlines[p_idx] > maxl) {
            maxl = _hb_nlines[p_idx];
        }
    }

    for (i = 0; i < maxl; i++) {
        for (p_idx = 0; p_idx < count; p_idx++) {
            char pad_buf[512];
            const char *line;

            line = (i < _hb_nlines[p_idx])
                 ? _hb_lines[p_idx][i] : "";
            flux_pad(pad_buf, (int)sizeof(pad_buf), line,
                     widths[p_idx]);
            flux_sb_append(sb, pad_buf);
            if (p_idx < count - 1) {
                flux_sb_append(sb, gap);
            }
        }
        flux_sb_nl(sb);
    }
    #undef _FLUX_HB_MAXLINES
    #undef _FLUX_HB_MAXPANELS
}

int flux_count_lines(const char *s) {
    int n = 0;

    if (!s || !*s) return 0;

    while (*s) {
        if (*s == '\n') n++;
        s++;
    }

    return n + 1;
}

void flux_pad_lines(char *buf, int bufsz, int target_lines) {
    int cur = 0;
    const char *s;
    int len;

    if (!buf || bufsz <= 0) return;

    s = buf;
    while (*s) {
        if (*s == '\n') cur++;
        s++;
    }
    len = (int)(s - buf);
    while (cur < target_lines && len < bufsz - 2) {
        buf[len++] = '\n';
        cur++;
    }
    buf[len] = '\0';
}

/* -- Box rendering -- */

static void _flux_wt_ap(char **o, int *rem, const char *s) {
    int l = (int)strlen(s);
    if (l > *rem) l = *rem;
    if (l <= 0) return;
    memcpy(*o, s, (size_t)l);
    *o += l;
    *rem -= l;
    **o = '\0';
}

void flux_box(char *out_buf, int out_sz, const char *content,
                     const FluxBorder *border, int inner_w,
                     const char *border_clr, const char *content_bg) {
    #define _FLUX_BOX_MAXLINES 256
    static char _box_tmp[32768];
    char *lptr[_FLUX_BOX_MAXLINES];
    int lwid[_FLUX_BOX_MAXLINES];
    int nl = 0;
    int max_w = 0;
    int iw;
    char *o;
    int rem;
    char *p;
    const char *R = FLUX_RESET;
    int i;

    if (!out_buf || out_sz <= 0) return;
    out_buf[0] = '\0';
    if (!content) content = "";

    strncpy(_box_tmp, content, sizeof(_box_tmp) - 1);
    _box_tmp[sizeof(_box_tmp) - 1] = '\0';

    p = _box_tmp;
    while (*p && nl < _FLUX_BOX_MAXLINES) {
        char *eol;
        lptr[nl] = p;
        eol = strchr(p, '\n');
        if (eol) *eol = '\0';
        lwid[nl] = flux_strwidth(p);
        if (lwid[nl] > max_w) max_w = lwid[nl];
        nl++;
        if (!eol) break;
        p = eol + 1;
    }

    iw = inner_w > 0 ? inner_w : max_w;

    o = out_buf;
    rem = out_sz - 1;

    if (border_clr) _flux_wt_ap(&o, &rem, border_clr);
    _flux_wt_ap(&o, &rem, border->tl);
    for (i = 0; i < iw + 2; i++) {
        _flux_wt_ap(&o, &rem, border->h);
    }
    _flux_wt_ap(&o, &rem, border->tr);
    if (border_clr) _flux_wt_ap(&o, &rem, R);
    _flux_wt_ap(&o, &rem, "\n");

    for (i = 0; i < nl; i++) {
        int pad_val;
        if (border_clr) _flux_wt_ap(&o, &rem, border_clr);
        _flux_wt_ap(&o, &rem, border->v);
        if (border_clr) _flux_wt_ap(&o, &rem, R);
        _flux_wt_ap(&o, &rem, " ");

        if (content_bg) _flux_wt_ap(&o, &rem, content_bg);
        _flux_wt_ap(&o, &rem, lptr[i]);

        pad_val = iw - lwid[i];
        if (pad_val > 0) {
            char sp[512];
            if (pad_val > 511) pad_val = 511;
            memset(sp, ' ', (size_t)pad_val);
            sp[pad_val] = '\0';
            _flux_wt_ap(&o, &rem, sp);
        }
        if (content_bg) _flux_wt_ap(&o, &rem, R);

        _flux_wt_ap(&o, &rem, " ");
        if (border_clr) _flux_wt_ap(&o, &rem, border_clr);
        _flux_wt_ap(&o, &rem, border->v);
        if (border_clr) _flux_wt_ap(&o, &rem, R);
        _flux_wt_ap(&o, &rem, "\n");
    }

    if (border_clr) _flux_wt_ap(&o, &rem, border_clr);
    _flux_wt_ap(&o, &rem, border->bl);
    for (i = 0; i < iw + 2; i++) {
        _flux_wt_ap(&o, &rem, border->h);
    }
    _flux_wt_ap(&o, &rem, border->br);
    if (border_clr) _flux_wt_ap(&o, &rem, R);

    #undef _FLUX_BOX_MAXLINES
}

/* -- ANSI color helpers -- */

char *flux_fg(char *buf, int r, int g, int b) {
    snprintf(buf, 32, "\x1b[38;2;%d;%d;%dm", r, g, b);
    return buf;
}

char *flux_bg(char *buf, int r, int g, int b) {
    snprintf(buf, 32, "\x1b[48;2;%d;%d;%dm", r, g, b);
    return buf;
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 4: Layout engine  (03_layout.c)                     ║
 * ╚════════════════════════════════════════════════════════════════╝ */

void flux_layout_measure_text(FluxNode *node, int max_w) {
    if (!node) return;
    if (!node->text && node->text_len == 0) {
        node->cw = 0;
        node->ch = 0;
        return;
    }

    if (node->type == FLUX_NODE_RAW) {
        int max_line_w = 0;
        int lines = 1;
        const char *p = node->text;
        const char *line_start = p;
        const char *end = node->text + node->text_len;

        while (p < end) {
            if (*p == '\n') {
                char save = *p;
                *(char *)p = '\0';
                int w = flux_strwidth(line_start);
                *(char *)p = save;
                if (w > max_line_w) max_line_w = w;
                lines++;
                p++;
                line_start = p;
            } else {
                p++;
            }
        }
        if (line_start < end) {
            int w = flux_strwidth(line_start);
            if (w > max_line_w) max_line_w = w;
        }

        node->cw = max_line_w;
        if (max_w > 0 && node->cw > max_w) node->cw = max_w;
        node->ch = lines;
        return;
    }

    if (node->wrap && max_w > 0) {
        int natural_w = 0;
        const char *p = node->text;
        const char *end = node->text + node->text_len;
        const char *line_start = p;

        while (p < end) {
            if (*p == '\n') {
                char save = *p;
                *(char *)p = '\0';
                int w = flux_strwidth(line_start);
                *(char *)p = save;
                if (w > natural_w) natural_w = w;
                p++;
                line_start = p;
            } else {
                p++;
            }
        }
        if (line_start < end) {
            int w = flux_strwidth(line_start);
            if (w > natural_w) natural_w = w;
        }

        node->cw = (natural_w < max_w) ? natural_w : max_w;
        if (node->cw < 1) node->cw = 1;
        node->ch = flux_wrapped_height(node->text, node->text_len, node->cw);
    } else {
        int max_line_w = 0;
        const char *p = node->text;
        const char *end = node->text + node->text_len;
        const char *line_start = p;

        while (p < end) {
            if (*p == '\n') {
                char save = *p;
                *(char *)p = '\0';
                int w = flux_strwidth(line_start);
                *(char *)p = save;
                if (w > max_line_w) max_line_w = w;
                p++;
                line_start = p;
            } else {
                p++;
            }
        }
        if (line_start < end) {
            int w = flux_strwidth(line_start);
            if (w > max_line_w) max_line_w = w;
        }

        node->cw = max_line_w;
        if (max_w > 0 && node->cw > max_w) node->cw = max_w;
        node->ch = flux_count_text_lines(node->text, node->text_len);
    }

    if (node->ch < 1) node->ch = 1;
}

void flux_layout_compute(FluxNode *node, int avail_w, int avail_h) {
    if (!node) return;

    node->prev_cx = node->cx;
    node->prev_cy = node->cy;
    node->prev_cw = node->cw;
    node->prev_ch = node->ch;

    int pad_top    = node->padding[0];
    int pad_right  = node->padding[1];
    int pad_bottom = node->padding[2];
    int pad_left   = node->padding[3];
    int pad_h = pad_top + pad_bottom;
    int pad_w = pad_left + pad_right;

    int inner_w = avail_w - pad_w;
    int inner_h = avail_h - pad_h;
    if (inner_w < 0) inner_w = 0;
    if (inner_h < 0) inner_h = 0;

    if (node->type == FLUX_NODE_TEXT || node->type == FLUX_NODE_RAW) {
        flux_layout_measure_text(node, inner_w);

        int content_w = node->cw;
        int content_h = node->ch;

        switch (node->width.mode) {
        case FLUX_SIZE_FIXED:
            node->cw = node->width.value;
            break;
        case FLUX_SIZE_FILL:
            node->cw = avail_w;
            break;
        case FLUX_SIZE_PERCENT:
            node->cw = avail_w * node->width.value / 100;
            break;
        case FLUX_SIZE_AUTO:
        default:
            node->cw = content_w + pad_w;
            break;
        }

        switch (node->height.mode) {
        case FLUX_SIZE_FIXED:
            node->ch = node->height.value;
            break;
        case FLUX_SIZE_FILL:
            node->ch = avail_h;
            break;
        case FLUX_SIZE_PERCENT:
            node->ch = avail_h * node->height.value / 100;
            break;
        case FLUX_SIZE_AUTO:
        default:
            node->ch = content_h + pad_h;
            break;
        }

        if (node->cw > avail_w && avail_w > 0) node->cw = avail_w;
        if (node->ch > avail_h && avail_h > 0) node->ch = avail_h;
        if (node->cw < 0) node->cw = 0;
        if (node->ch < 0) node->ch = 0;

        node->layout_dirty = 0;
        return;
    }

    /* BOX node */
    int outer_w, outer_h;
    int auto_w = (node->width.mode == FLUX_SIZE_AUTO);
    int auto_h = (node->height.mode == FLUX_SIZE_AUTO);

    switch (node->width.mode) {
    case FLUX_SIZE_FIXED:   outer_w = node->width.value; break;
    case FLUX_SIZE_FILL:    outer_w = avail_w;           break;
    case FLUX_SIZE_PERCENT: outer_w = avail_w * node->width.value / 100; break;
    case FLUX_SIZE_AUTO:
    default:                outer_w = avail_w;           break;
    }

    switch (node->height.mode) {
    case FLUX_SIZE_FIXED:   outer_h = node->height.value; break;
    case FLUX_SIZE_FILL:    outer_h = avail_h;            break;
    case FLUX_SIZE_PERCENT: outer_h = avail_h * node->height.value / 100; break;
    case FLUX_SIZE_AUTO:
    default:                outer_h = avail_h;            break;
    }

    if (outer_w > avail_w && avail_w > 0) outer_w = avail_w;
    if (outer_h > avail_h && avail_h > 0) outer_h = avail_h;

    int content_area_w = outer_w - pad_w;
    int content_area_h = outer_h - pad_h;
    if (content_area_w < 0) content_area_w = 0;
    if (content_area_h < 0) content_area_h = 0;

    if (node->child_count == 0) {
        node->cw = auto_w ? pad_w : outer_w;
        node->ch = auto_h ? pad_h : outer_h;
        node->layout_dirty = 0;
        return;
    }

    if (node->direction == FLUX_DIR_VERTICAL) {
        int used_h = 0;
        int fill_count = 0;

        int i;
        for (i = 0; i < node->child_count; i++) {
            FluxNode *child = node->children[i];
            if (!child) continue;

            switch (child->height.mode) {
            case FLUX_SIZE_FIXED:
                used_h += child->height.value;
                break;
            case FLUX_SIZE_PERCENT:
                used_h += content_area_h * child->height.value / 100;
                break;
            case FLUX_SIZE_FILL:
                fill_count++;
                break;
            case FLUX_SIZE_AUTO:
            default:
                flux_layout_compute(child, content_area_w, content_area_h);
                used_h += child->ch;
                break;
            }
        }

        int remaining = content_area_h - used_h;
        if (remaining < 0) remaining = 0;
        int fill_each = (fill_count > 0) ? remaining / fill_count : 0;
        int fill_extra = (fill_count > 0) ? remaining % fill_count : 0;

        int y_offset = pad_top;
        int max_child_w = 0;

        for (i = 0; i < node->child_count; i++) {
            FluxNode *child = node->children[i];
            if (!child) continue;

            int child_avail_h;

            switch (child->height.mode) {
            case FLUX_SIZE_FIXED:
                child_avail_h = child->height.value;
                break;
            case FLUX_SIZE_PERCENT:
                child_avail_h = content_area_h * child->height.value / 100;
                break;
            case FLUX_SIZE_FILL:
                child_avail_h = fill_each;
                if (fill_extra > 0) {
                    child_avail_h++;
                    fill_extra--;
                }
                break;
            case FLUX_SIZE_AUTO:
            default:
                child_avail_h = child->ch;
                break;
            }

            if (child->height.mode != FLUX_SIZE_AUTO) {
                flux_layout_compute(child, content_area_w, child_avail_h);
            }

            child->cx = pad_left;
            child->cy = y_offset;
            y_offset += child->ch;

            if (child->cw > max_child_w) max_child_w = child->cw;
        }

        int total_content_h = y_offset - pad_top;
        node->cw = auto_w ? (max_child_w + pad_w) : outer_w;
        node->ch = auto_h ? (total_content_h + pad_h) : outer_h;

    } else {
        int used_w = 0;
        int fill_count = 0;

        int i;
        for (i = 0; i < node->child_count; i++) {
            FluxNode *child = node->children[i];
            if (!child) continue;

            switch (child->width.mode) {
            case FLUX_SIZE_FIXED:
                used_w += child->width.value;
                break;
            case FLUX_SIZE_PERCENT:
                used_w += content_area_w * child->width.value / 100;
                break;
            case FLUX_SIZE_FILL:
                fill_count++;
                break;
            case FLUX_SIZE_AUTO:
            default:
                flux_layout_compute(child, content_area_w, content_area_h);
                used_w += child->cw;
                break;
            }
        }

        int remaining = content_area_w - used_w;
        if (remaining < 0) remaining = 0;
        int fill_each = (fill_count > 0) ? remaining / fill_count : 0;
        int fill_extra = (fill_count > 0) ? remaining % fill_count : 0;

        int x_offset = pad_left;
        int max_child_h = 0;

        for (i = 0; i < node->child_count; i++) {
            FluxNode *child = node->children[i];
            if (!child) continue;

            int child_avail_w;

            switch (child->width.mode) {
            case FLUX_SIZE_FIXED:
                child_avail_w = child->width.value;
                break;
            case FLUX_SIZE_PERCENT:
                child_avail_w = content_area_w * child->width.value / 100;
                break;
            case FLUX_SIZE_FILL:
                child_avail_w = fill_each;
                if (fill_extra > 0) {
                    child_avail_w++;
                    fill_extra--;
                }
                break;
            case FLUX_SIZE_AUTO:
            default:
                child_avail_w = child->cw;
                break;
            }

            if (child->width.mode != FLUX_SIZE_AUTO) {
                flux_layout_compute(child, child_avail_w, content_area_h);
            }

            child->cx = x_offset;
            child->cy = pad_top;
            x_offset += child->cw;

            if (child->ch > max_child_h) max_child_h = child->ch;
        }

        int total_content_w = x_offset - pad_left;
        node->cw = auto_w ? (total_content_w + pad_w) : outer_w;
        node->ch = auto_h ? (max_child_h + pad_h) : outer_h;
    }

    if (node->cw > avail_w && avail_w > 0) node->cw = avail_w;
    if (node->ch > avail_h && avail_h > 0) node->ch = avail_h;
    if (node->cw < 0) node->cw = 0;
    if (node->ch < 0) node->ch = 0;

    node->layout_dirty = 0;
}

void flux_layout_to_absolute(FluxNode *node) {
    int i;
    if (!node) return;

    for (i = 0; i < node->child_count; i++) {
        FluxNode *child = node->children[i];
        if (!child) continue;

        child->cx += node->cx;
        child->cy += node->cy;

        flux_layout_to_absolute(child);
    }
}

void flux_layout_resolve_absolute(FluxNode *root, int screen_w, int screen_h) {
    if (!root) return;

    flux_layout_compute(root, screen_w, screen_h);

    root->cx = 0;
    root->cy = 0;

    flux_layout_to_absolute(root);
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 5: Node renderer  (04_render_node.c)                ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* -- UTF-8 helpers -- */

int flux_render__utf8_len(const char *p) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* Per-codepoint cell width for the render layout. Pre-#351 this was a
 * hand-curated mirror of `_flux_wt_is_wide_cp`; both now route through
 * `flux_cp_width` so they cannot drift. The cell pipeline still calls
 * this codepoint-by-codepoint — for cluster-correct width (RI flag
 * pairs, Fitzpatrick, ZWJ chains) callers should use
 * `flux_grapheme_advance` instead. */
int flux_render__char_width(const char *p, int byte_len) {
    unsigned int cp;
    int n = flux_utf8_decode(p, byte_len, &cp);
    if (n <= 0) return 1;
    return flux_cp_width(cp);
}

int flux_render__skip_ansi(const char *p, int remaining) {
    int i;
    if (remaining < 2) return 1;
    if (p[1] != '[') return 1;
    for (i = 2; i < remaining; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c >= 0x40 && c <= 0x7E) {
            return i + 1;
        }
    }
    return remaining;
}

/* -- ANSI SGR parser for RAW mode -- */

int flux_render__parse_sgr(const char *p, int remaining, FluxStyle *s) {
    int params[16];
    int param_count = 0;
    int cur = 0;
    int has_cur = 0;
    int i, j;

    for (i = 0; i < remaining; i++) {
        char c = p[i];
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            has_cur = 1;
        } else if (c == ';') {
            if (has_cur && param_count < 16) {
                params[param_count++] = cur;
            } else if (!has_cur && param_count < 16) {
                params[param_count++] = 0;
            }
            cur = 0;
            has_cur = 0;
        } else if (c == 'm') {
            if (has_cur && param_count < 16) {
                params[param_count++] = cur;
            } else if (param_count == 0) {
                params[param_count++] = 0;
            }
            break;
        } else {
            return i + 1;
        }
    }

    for (j = 0; j < param_count; j++) {
        int code = params[j];
        if (code == 0) {
            s->fg_r = s->fg_g = s->fg_b = -1;
            s->bg_r = s->bg_g = s->bg_b = -1;
            s->bold = s->dim = s->italic = 0;
            s->underline = s->strikethrough = s->inverse = 0;
        } else if (code == 1) {
            s->bold = 1;
        } else if (code == 2) {
            s->dim = 1;
        } else if (code == 3) {
            s->italic = 1;
        } else if (code == 4) {
            s->underline = 1;
        } else if (code == 7) {
            s->inverse = 1;
        } else if (code == 9) {
            s->strikethrough = 1;
        } else if (code == 22) {
            s->bold = 0; s->dim = 0;
        } else if (code == 23) {
            s->italic = 0;
        } else if (code == 24) {
            s->underline = 0;
        } else if (code == 27) {
            s->inverse = 0;
        } else if (code == 29) {
            s->strikethrough = 0;
        } else if (code >= 30 && code <= 37) {
            static const int basic[8][3] = {
                {0,0,0}, {170,0,0}, {0,170,0}, {170,85,0},
                {0,0,170}, {170,0,170}, {0,170,170}, {170,170,170}
            };
            int idx = code - 30;
            s->fg_r = basic[idx][0];
            s->fg_g = basic[idx][1];
            s->fg_b = basic[idx][2];
        } else if (code == 38) {
            if (j + 1 < param_count && params[j + 1] == 2
                && j + 4 < param_count) {
                s->fg_r = params[j + 2];
                s->fg_g = params[j + 3];
                s->fg_b = params[j + 4];
                j += 4;
            } else if (j + 1 < param_count && params[j + 1] == 5
                       && j + 2 < param_count) {
                s->fg_r = params[j + 2];
                s->fg_g = -2;
                s->fg_b = -2;
                j += 2;
            }
        } else if (code == 39) {
            s->fg_r = s->fg_g = s->fg_b = -1;
        } else if (code >= 40 && code <= 47) {
            static const int basic[8][3] = {
                {0,0,0}, {170,0,0}, {0,170,0}, {170,85,0},
                {0,0,170}, {170,0,170}, {0,170,170}, {170,170,170}
            };
            int idx = code - 40;
            s->bg_r = basic[idx][0];
            s->bg_g = basic[idx][1];
            s->bg_b = basic[idx][2];
        } else if (code == 48) {
            if (j + 1 < param_count && params[j + 1] == 2
                && j + 4 < param_count) {
                s->bg_r = params[j + 2];
                s->bg_g = params[j + 3];
                s->bg_b = params[j + 4];
                j += 4;
            } else if (j + 1 < param_count && params[j + 1] == 5
                       && j + 2 < param_count) {
                s->bg_r = params[j + 2];
                s->bg_g = -2;
                s->bg_b = -2;
                j += 2;
            }
        } else if (code == 49) {
            s->bg_r = s->bg_g = s->bg_b = -1;
        } else if (code >= 90 && code <= 97) {
            static const int bright[8][3] = {
                {85,85,85}, {255,85,85}, {85,255,85}, {255,255,85},
                {85,85,255}, {255,85,255}, {85,255,255}, {255,255,255}
            };
            int idx = code - 90;
            s->fg_r = bright[idx][0];
            s->fg_g = bright[idx][1];
            s->fg_b = bright[idx][2];
        } else if (code >= 100 && code <= 107) {
            static const int bright[8][3] = {
                {85,85,85}, {255,85,85}, {85,255,85}, {255,255,85},
                {85,85,255}, {255,85,255}, {85,255,255}, {255,255,255}
            };
            int idx = code - 100;
            s->bg_r = bright[idx][0];
            s->bg_g = bright[idx][1];
            s->bg_b = bright[idx][2];
        }
    }

    return i + 1;
}

/* -- Fill helper -- */

void flux_render__fill_rect(FluxScreen *scr, int x, int y,
                                   int w, int h, int style_id) {
    int row, col;
    for (row = y; row < y + h; row++) {
        for (col = x; col < x + w; col++) {
            flux_screen_set_cell(scr, col, row, " ", 1, style_id, 1);
        }
    }
}

/* -- TEXT node renderer -- */

void flux_render_text_to_screen(FluxScreen *scr, FluxStylePool *pool,
                                       FluxNode *node) {
    int style_id;
    int bx, by, bw, bh;
    int px, py;
    int inner_x, inner_y;
    int inner_w, inner_h;
    const char *p;
    int remaining;

    if (!scr || !pool || !node) return;

    style_id = flux_style_pool_intern(pool, &node->style);

    bx = node->cx;
    by = node->cy;
    bw = node->cw;
    bh = node->ch;

    inner_x = bx + node->padding[3];
    inner_y = by + node->padding[0];
    inner_w = bw - node->padding[3] - node->padding[1];
    inner_h = bh - node->padding[0] - node->padding[2];

    if (inner_w <= 0 || inner_h <= 0) {
        flux_render__fill_rect(scr, bx, by, bw, bh, style_id);
        return;
    }

    flux_render__fill_rect(scr, bx, by, bw, bh, style_id);

    if (!node->text || node->text_len == 0) return;

    px = inner_x;
    py = inner_y;
    p = node->text;
    remaining = node->text_len;

    /* Carries the "previous codepoint was a Regional Indicator" bit
     * across grapheme calls so RI flag pairs collapse to one cluster
     * (US flag = 1F1FA + 1F1F8 = 2 cells, not 4). */
    int ri_state = 0;

    while (remaining > 0 && (py - inner_y) < inner_h) {
        if (*p == '\n') {
            py++;
            px = inner_x;
            p++;
            remaining--;
            ri_state = 0;
            continue;
        }

        if (*p == '\x1b') {
            int skip = flux_render__skip_ansi(p, remaining);
            p += skip;
            remaining -= skip;
            continue;
        }

        if (px >= inner_x + inner_w) {
            py++;
            px = inner_x;
            ri_state = 0;
            if ((py - inner_y) >= inner_h) break;
        }

        if (node->wrap && px > inner_x) {
            int word_w = 0;
            const char *wp = p;
            int wr = remaining;
            int wp_ri = ri_state;
            while (wr > 0 && *wp != ' ' && *wp != '-' && *wp != '\n'
                   && *wp != '\x1b') {
                int gb, gw;
                int n = flux_grapheme_advance(wp, wr, &wp_ri, &gb, &gw);
                if (n <= 0) break;
                word_w += gw;
                wp += gb;
                wr -= gb;
                if (wr > 0 && *wp == '-') {
                    int hb, hw;
                    int hn = flux_grapheme_advance(wp, wr, &wp_ri, &hb, &hw);
                    if (hn <= 0) break;
                    word_w += hw;
                    break;
                }
            }
            if (px + word_w > inner_x + inner_w && word_w <= inner_w) {
                py++;
                px = inner_x;
                if ((py - inner_y) >= inner_h) break;
            }
        }

        {
            int blen, cw;
            (void)flux_grapheme_advance(p, remaining, &ri_state,
                                         &blen, &cw);
            if (blen <= 0) break;

            if (px == inner_x && *p == ' ' && node->wrap) {
                if (p > node->text) {
                    const char *prev = p - 1;
                    if (*prev != '\n') {
                        p += blen;
                        remaining -= blen;
                        continue;
                    }
                }
            }

            if (px + cw > inner_x + inner_w) {
                if (node->wrap) {
                    py++;
                    px = inner_x;
                    if ((py - inner_y) >= inner_h) break;
                    continue;
                } else {
                    p += blen;
                    remaining -= blen;
                    continue;
                }
            }

            /* One cell per grapheme cluster — `blen` may be up to ~25
             * bytes for a ZWJ family / flag / Fitzpatrick sequence,
             * which is why FLUX_CELL_CH_MAX was bumped to 32. */
            flux_screen_set_cell(scr, px, py, p, blen, style_id, cw);

            if (cw == 2) {
                flux_screen_set_cell(scr, px + 1, py, "", 0, style_id, 0);
            }

            px += cw;
            p += blen;
            remaining -= blen;
        }
    }
}

/* -- RAW node renderer -- */

void flux_render_raw_to_screen(FluxScreen *scr, FluxStylePool *pool,
                                      FluxNode *node) {
    FluxStyle cur_style;
    int style_id;
    int bx, by, bw, bh;
    int px, py;
    const char *p;
    int remaining;

    if (!scr || !pool || !node) return;
    if (!node->text || node->text_len == 0) return;

    bx = node->cx;
    by = node->cy;
    bw = node->cw;
    bh = node->ch;

    cur_style = node->style;
    style_id = flux_style_pool_intern(pool, &cur_style);

    flux_render__fill_rect(scr, bx, by, bw, bh, style_id);

    px = bx;
    py = by;
    p = node->text;
    remaining = node->text_len;
    int ri_state = 0;

    while (remaining > 0 && (py - by) < bh) {
        if (*p == '\x1b' && remaining >= 2 && p[1] == '[') {
            int seq_len;
            const char *scan = p + 2;
            int scan_rem = remaining - 2;
            char final_byte = 0;
            int is_sgr = 0;

            while (scan_rem > 0) {
                unsigned char c = (unsigned char)*scan;
                if (c >= 0x40 && c <= 0x7E) {
                    final_byte = (char)c;
                    is_sgr = (c == 'm');
                    break;
                }
                scan++;
                scan_rem--;
            }

            if (is_sgr) {
                int consumed = flux_render__parse_sgr(p + 2, remaining - 2,
                                                      &cur_style);
                seq_len = 2 + consumed;
                style_id = flux_style_pool_intern(pool, &cur_style);
            } else {
                /* Cursor positioning + erase-line — honor inside the node
                 * box so SB output that drives layout via CUP/CHA/VPA/EL
                 * (cawd's flux_viewport_render does this densely) renders
                 * positionally instead of being skipped.
                 *
                 * Parameters are decimal, semicolon-separated, '0'..'9' +
                 * ';'. The terminal default for a missing/zero parameter
                 * differs per command (1 for CUP rows/cols, 0 for EL). */
                if (final_byte == 'H' || final_byte == 'f') {
                    /* CUP / HVP — \x1b[<row>;<col>H, 1-based, both default 1. */
                    int row = 0, col = 0;
                    int have_row = 0;
                    const char *q = p + 2;
                    int qr = remaining - 2;
                    while (qr > 0 && *q != ';' && *q != final_byte) {
                        if (*q >= '0' && *q <= '9') {
                            row = row * 10 + (*q - '0');
                            have_row = 1;
                        }
                        q++; qr--;
                    }
                    if (qr > 0 && *q == ';') {
                        q++; qr--;
                        while (qr > 0 && *q != final_byte) {
                            if (*q >= '0' && *q <= '9') col = col * 10 + (*q - '0');
                            q++; qr--;
                        }
                    }
                    if (!have_row) row = 1;
                    if (col == 0)  col = 1;
                    px = bx + (col - 1);
                    py = by + (row - 1);
                    if (px < bx) px = bx;
                    if (py < by) py = by;
                } else if (final_byte == 'G') {
                    /* CHA — \x1b[<col>G, default 1, row unchanged. */
                    int col = 0;
                    const char *q = p + 2;
                    int qr = remaining - 2;
                    while (qr > 0 && *q != final_byte) {
                        if (*q >= '0' && *q <= '9') col = col * 10 + (*q - '0');
                        q++; qr--;
                    }
                    if (col == 0) col = 1;
                    px = bx + (col - 1);
                    if (px < bx) px = bx;
                } else if (final_byte == 'd') {
                    /* VPA — \x1b[<row>d, default 1, col unchanged. */
                    int row = 0;
                    const char *q = p + 2;
                    int qr = remaining - 2;
                    while (qr > 0 && *q != final_byte) {
                        if (*q >= '0' && *q <= '9') row = row * 10 + (*q - '0');
                        q++; qr--;
                    }
                    if (row == 0) row = 1;
                    py = by + (row - 1);
                    if (py < by) py = by;
                } else if (final_byte == 'K') {
                    /* EL — erase in line. Param 0=cursor→EOL (default),
                     * 1=BOL→cursor, 2=whole line. We treat EOL/whole as
                     * "fill remaining cells with current style". BOL is
                     * rare; ignore for now. */
                    int param = 0;
                    const char *q = p + 2;
                    int qr = remaining - 2;
                    while (qr > 0 && *q != final_byte) {
                        if (*q >= '0' && *q <= '9') param = param * 10 + (*q - '0');
                        q++; qr--;
                    }
                    if (param == 0 || param == 2) {
                        int x;
                        for (x = px; x < bx + bw; x++) {
                            flux_screen_set_cell(scr, x, py, " ", 1, style_id, 1);
                        }
                    }
                }
                /* All other CSI sequences are visual no-ops on a cell
                 * buffer (cursor save/restore, mode set/reset, scroll
                 * region, etc.) — skip them. */
                seq_len = flux_render__skip_ansi(p, remaining);
            }

            p += seq_len;
            remaining -= seq_len;
            continue;
        }

        if (*p == '\x1b') {
            p++;
            remaining--;
            continue;
        }

        if (*p == '\n') {
            py++;
            px = bx;
            p++;
            remaining--;
            continue;
        }

        if (*p == '\r') {
            px = bx;
            p++;
            remaining--;
            continue;
        }

        if (*p == '\t') {
            int tab_stop = ((px - bx + 8) & ~7) + bx;
            while (px < tab_stop && (px - bx) < bw) {
                flux_screen_set_cell(scr, px, py, " ", 1, style_id, 1);
                px++;
            }
            p++;
            remaining--;
            continue;
        }

        if (px >= bx + bw) {
            while (remaining > 0 && *p != '\n') {
                if (*p == '\x1b') {
                    int skip = flux_render__skip_ansi(p, remaining);
                    if (remaining >= 2 && p[1] == '[') {
                        const char *scan2 = p + 2;
                        int sr = remaining - 2;
                        while (sr > 0) {
                            unsigned char c = (unsigned char)*scan2;
                            if (c >= 0x40 && c <= 0x7E) {
                                if (c == 'm') {
                                    flux_render__parse_sgr(p + 2, remaining - 2,
                                                           &cur_style);
                                    style_id = flux_style_pool_intern(pool,
                                                                      &cur_style);
                                }
                                break;
                            }
                            scan2++;
                            sr--;
                        }
                    }
                    p += skip;
                    remaining -= skip;
                } else {
                    int blen = flux_render__utf8_len(p);
                    if (blen > remaining) blen = remaining;
                    p += blen;
                    remaining -= blen;
                }
            }
            continue;
        }

        {
            int blen, cw;
            (void)flux_grapheme_advance(p, remaining, &ri_state, &blen, &cw);
            if (blen <= 0) break;

            if (px + cw > bx + bw) {
                p += blen;
                remaining -= blen;
                continue;
            }

            flux_screen_set_cell(scr, px, py, p, blen, style_id, cw);
            if (cw == 2) {
                flux_screen_set_cell(scr, px + 1, py, "", 0, style_id, 0);
            }

            px += cw;
            p += blen;
            remaining -= blen;
        }
    }
}

/* -- Recursive node renderer -- */

void flux_render_node(FluxScreen *scr, FluxScreen *prev,
                             FluxStylePool *pool, FluxNode *node) {
    if (!scr || !pool || !node) return;

    if (!node->dirty && prev && prev->cells
        && node->cx == node->prev_cx && node->cy == node->prev_cy
        && node->cw == node->prev_cw && node->ch == node->prev_ch) {
        flux_screen_blit(scr, prev,
                         node->prev_cx, node->prev_cy,
                         node->cx, node->cy,
                         node->cw, node->ch);
        return;
    }

    switch (node->type) {
    case FLUX_NODE_TEXT:
        flux_render_text_to_screen(scr, pool, node);
        break;

    case FLUX_NODE_RAW:
        flux_render_raw_to_screen(scr, pool, node);
        break;

    case FLUX_NODE_BOX: {
        int has_bg = (node->style.bg_r >= 0 || node->style.bg_g >= 0
                      || node->style.bg_b >= 0);
        if (has_bg) {
            int bg_style = flux_style_pool_intern(pool, &node->style);
            flux_render__fill_rect(scr, node->cx, node->cy,
                                   node->cw, node->ch, bg_style);
        }

        {
            int i;
            for (i = 0; i < node->child_count; i++) {
                flux_render_node(scr, prev, pool, node->children[i]);
            }
        }
        break;
    }
    }

    node->prev_cx = node->cx;
    node->prev_cy = node->cy;
    node->prev_cw = node->cw;
    node->prev_ch = node->ch;
}

/* -- Entry point -- */

void flux_render_tree(FluxScreen *scr, FluxScreen *prev,
                             FluxStylePool *pool, FluxNode *root) {
    if (!scr || !pool || !root) return;

    {
        int total = scr->rows * scr->cols;
        int i;
        for (i = 0; i < total; i++) {
            FluxCell *c = &scr->cells[i];
            c->ch[0] = ' ';
            c->ch[1] = '\0';
            c->style_id = -1;
            c->width = 1;
        }
    }

    scr->damage_x1 = scr->cols;
    scr->damage_y1 = scr->rows;
    scr->damage_x2 = 0;
    scr->damage_y2 = 0;

    flux_render_node(scr, prev, pool, root);

    flux_node_clear_dirty(root);
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 6: Diff engine  (05_diff_engine.c)                  ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* Output buffer macros for diff engine (local scope in functions that use them) */

int flux_diff_cursor_move(int cx, int cy, int tx, int ty,
                                 char *buf, int bufsz) {
    int off = 0;
    int dx, dy;

    if (cx == tx && cy == ty) {
        return 0;
    }

    dx = tx - cx;
    dy = ty - cy;

    if (dy == 0) {
        if (dx == 1) {
            off += snprintf(buf + off, bufsz - off, "\x1b[C");
            return off;
        }
        if (dx > 1 && dx <= 9) {
            off += snprintf(buf + off, bufsz - off, "\x1b[%dC", dx);
            return off;
        }
        if (tx == 0) {
            buf[off++] = '\r';
            return off;
        }
        if (tx > 0 && tx <= 9) {
            int cr_len = 1 + 3 + (tx >= 10 ? 2 : 1);
            int abs_len = 4 + (ty + 1 >= 10 ? (ty + 1 >= 100 ? 3 : 2) : 1)
                        + (tx + 1 >= 10 ? (tx + 1 >= 100 ? 3 : 2) : 1);
            if (cr_len < abs_len) {
                off += snprintf(buf + off, bufsz - off, "\r");
                if (tx > 0) {
                    off += snprintf(buf + off, bufsz - off, "\x1b[%dC", tx);
                }
                return off;
            }
        }
    }

    if (tx == 0 && ty == 0) {
        off += snprintf(buf + off, bufsz - off, "\x1b[H");
    } else if (tx == 0) {
        off += snprintf(buf + off, bufsz - off, "\x1b[%dH", ty + 1);
    } else {
        off += snprintf(buf + off, bufsz - off, "\x1b[%d;%dH", ty + 1, tx + 1);
    }
    return off;
}

int flux_diff_emit(const FluxScreen *prev, const FluxScreen *next,
                          const FluxStylePool *pool, char *obuf, int obufsz) {
    int pos = 0;
    int cur_x = 0, cur_y = 0;
    int cur_style_id = -1;
    int rows, cols;
    int y, x;
    int is_last_row;
    int last_row_wrapped = 0;

    /* Local OCAT/OCATF/OCATN macros */
    #define DIFF_OCAT(s) do {                            \
        int _l = (int)strlen(s);                         \
        if (pos + _l < obufsz) {                         \
            memcpy(obuf + pos, s, _l);                   \
            pos += _l;                                   \
        }                                                \
    } while (0)

    #define DIFF_OCATF(...) do {                         \
        char _t[64];                                     \
        snprintf(_t, sizeof _t, __VA_ARGS__);            \
        DIFF_OCAT(_t);                                   \
    } while (0)

    #define DIFF_OCATN(s, n) do {                        \
        if (pos + (n) < obufsz) {                        \
            memcpy(obuf + pos, (s), (n));                \
            pos += (n);                                  \
        }                                                \
    } while (0)

    if (!next || !next->cells || !pool || !obuf || obufsz <= 0) {
        return 0;
    }

    rows = next->rows;
    cols = next->cols;

    if (prev && (prev->rows != rows || prev->cols != cols)) {
        prev = NULL;
    }

    DIFF_OCAT("\x1b[?25l");

    for (y = 0; y < rows; y++) {
        is_last_row = (y == rows - 1);

        for (x = 0; x < cols; x++) {
            const FluxCell *nc;
            const FluxCell *pc;
            const FluxStyle *ns;
            int ch_len;

            nc = flux_screen_get_cell(next, x, y);
            if (!nc) {
                continue;
            }

            if (nc->width == 0) {
                continue;
            }

            if (prev) {
                pc = flux_screen_get_cell(prev, x, y);
                if (pc && flux_cell_eq(pc, nc)) {
                    /* Cell unchanged — skip emit. Do NOT advance
                     * cur_x: we wrote nothing, so the terminal's
                     * actual cursor stays where it was after the
                     * last emitted cell. The next emitted cell's
                     * flux_diff_cursor_move will correctly emit a
                     * `\x1b[NC` to skip past the gap. (Bug fixed
                     * Apr 2026: previously cur_x = x + nc->width
                     * here desynced our tracking from the terminal
                     * and caused subsequent cells to overwrite
                     * trailing-space gaps. Symptom: "Type a message"
                     * losing its 3-space lead-in after `( o.o )`.) */
                    continue;
                }
            }

            if (is_last_row && !last_row_wrapped) {
                DIFF_OCAT("\x1b[?7l");
                last_row_wrapped = 1;
            }

            {
                char movebuf[32];
                int mlen = flux_diff_cursor_move(cur_x, cur_y, x, y,
                                                 movebuf, sizeof movebuf);
                if (mlen > 0) {
                    DIFF_OCATN(movebuf, mlen);
                }
            }

            if (nc->style_id != cur_style_id) {
                if (nc->style_id >= 0 && nc->style_id < pool->count) {
                    const FluxStyle *prev_style = NULL;
                    char stylebuf[128];
                    int slen;

                    ns = &pool->styles[nc->style_id];
                    if (cur_style_id >= 0 && cur_style_id < pool->count) {
                        prev_style = &pool->styles[cur_style_id];
                    }
                    slen = flux_style_to_ansi(ns, prev_style,
                                              stylebuf, sizeof stylebuf);
                    if (slen > 0) {
                        DIFF_OCATN(stylebuf, slen);
                    }
                } else if (nc->style_id == -1 && cur_style_id != -1) {
                    DIFF_OCAT("\x1b[0m");
                }
                cur_style_id = nc->style_id;
            }

            ch_len = (int)strlen(nc->ch);
            if (ch_len > 0) {
                DIFF_OCATN(nc->ch, ch_len);
            } else {
                DIFF_OCAT(" ");
                ch_len = 1;
            }

            cur_x = x + nc->width;
            cur_y = y;
        }
    }

    if (last_row_wrapped) {
        DIFF_OCAT("\x1b[?7h");
    }

    if (cur_style_id != -1) {
        DIFF_OCAT("\x1b[0m");
    }

    DIFF_OCAT("\x1b[1;1H");

    if (pos < obufsz) {
        obuf[pos] = '\0';
    }

    #undef DIFF_OCAT
    #undef DIFF_OCATF
    #undef DIFF_OCATN

    return pos;
}

int flux_diff_screens(const FluxScreen *prev, const FluxScreen *next,
                             const FluxStylePool *pool,
                             char *obuf, int obufsz) {
    return flux_diff_emit(prev, next, pool, obuf, obufsz);
}

int flux_diff_full(const FluxScreen *next, const FluxStylePool *pool,
                          char *obuf, int obufsz) {
    return flux_diff_emit(NULL, next, pool, obuf, obufsz);
}

int flux_diff_has_changes(const FluxScreen *prev,
                                 const FluxScreen *next) {
    int total, i;

    if (!prev || !next) {
        return 1;
    }
    if (prev->rows != next->rows || prev->cols != next->cols) {
        return 1;
    }
    if (!prev->cells || !next->cells) {
        return 1;
    }

    total = prev->rows * prev->cols;
    for (i = 0; i < total; i++) {
        if (!flux_cell_eq(&prev->cells[i], &next->cells[i])) {
            return 1;
        }
    }
    return 0;
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 7: Widgets -- Input  (08_widget_input.c)            ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* -- Input UTF-8 helpers -- */

static int _flux_inp_utf8_len(const char *buf, int pos, int len) {
    unsigned char c;

    if (pos >= len) return 0;
    c = (unsigned char)buf[pos];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return (pos + 2 <= len) ? 2 : 1;
    if ((c & 0xF0) == 0xE0) return (pos + 3 <= len) ? 3 : 1;
    if ((c & 0xF8) == 0xF0) return (pos + 4 <= len) ? 4 : 1;
    return 1;
}

static int _flux_inp_prev_char(const char *buf, int pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && ((unsigned char)buf[pos] & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}

static int _flux_inp_char_width(const char *buf, int pos, int len) {
    char tmp[8];
    int clen = _flux_inp_utf8_len(buf, pos, len);

    if (clen <= 0) return 0;
    if (clen > 7) clen = 7;
    memcpy(tmp, buf + pos, (size_t)clen);
    tmp[clen] = '\0';
    return flux_strwidth(tmp);
}

/* -- Init / Clear -- */

void flux_input_init(FluxInput *inp, const char *placeholder) {
    memset(inp, 0, sizeof(*inp));
    inp->placeholder = placeholder;
}

void flux_input_clear(FluxInput *inp) {
    inp->buf[0] = '\0';
    inp->cursor = 0;
    inp->len = 0;
    inp->submitted = 0;
}

/* -- Update -- */

int flux_input_update(FluxInput *inp, FluxMsg msg) {
    if (msg.type == MSG_PASTE) {
        int avail = FLUX_INPUT_MAX - inp->len;
        int ins = msg.u.paste.len;

        if (ins <= 0) return 1;
        if (ins > avail) ins = avail;
        if (ins <= 0) return 1;

        memmove(inp->buf + inp->cursor + ins,
                inp->buf + inp->cursor,
                (size_t)(inp->len - inp->cursor));
        memcpy(inp->buf + inp->cursor, msg.u.paste.text, (size_t)ins);
        inp->len += ins;
        inp->cursor += ins;
        inp->buf[inp->len] = '\0';
        return 1;
    }

    if (msg.type != MSG_KEY) return 0;

    if (flux_key_is(msg, "enter")) {
        inp->submitted = 1;
        return 1;
    }

    if (flux_key_is(msg, "backspace")) {
        if (inp->cursor > 0) {
            int prev = _flux_inp_prev_char(inp->buf, inp->cursor);
            int del = inp->cursor - prev;
            memmove(inp->buf + prev,
                    inp->buf + inp->cursor,
                    (size_t)(inp->len - inp->cursor));
            inp->len -= del;
            inp->cursor = prev;
            inp->buf[inp->len] = '\0';
        }
        return 1;
    }

    if (flux_key_is(msg, "delete")) {
        if (inp->cursor < inp->len) {
            int clen = _flux_inp_utf8_len(inp->buf, inp->cursor, inp->len);
            memmove(inp->buf + inp->cursor,
                    inp->buf + inp->cursor + clen,
                    (size_t)(inp->len - inp->cursor - clen));
            inp->len -= clen;
            inp->buf[inp->len] = '\0';
        }
        return 1;
    }

    if (flux_key_is(msg, "left")) {
        if (inp->cursor > 0) {
            inp->cursor = _flux_inp_prev_char(inp->buf, inp->cursor);
        }
        return 1;
    }

    if (flux_key_is(msg, "right")) {
        if (inp->cursor < inp->len) {
            int clen = _flux_inp_utf8_len(inp->buf, inp->cursor, inp->len);
            inp->cursor += clen;
        }
        return 1;
    }

    if (flux_key_is(msg, "home") || flux_key_is(msg, "ctrl+a")) {
        inp->cursor = 0;
        return 1;
    }

    if (flux_key_is(msg, "end") || flux_key_is(msg, "ctrl+e")) {
        inp->cursor = inp->len;
        return 1;
    }

    if (flux_key_is(msg, "ctrl+u")) {
        inp->buf[0] = '\0';
        inp->cursor = 0;
        inp->len = 0;
        return 1;
    }

    if (flux_key_is(msg, "ctrl+k")) {
        inp->len = inp->cursor;
        inp->buf[inp->len] = '\0';
        return 1;
    }

    if (flux_key_is(msg, "ctrl+w")) {
        int target = inp->cursor;

        while (target > 0 && inp->buf[target - 1] == ' ') {
            target--;
        }
        while (target > 0 && inp->buf[target - 1] != ' ') {
            target--;
        }

        if (target < inp->cursor) {
            int del = inp->cursor - target;
            memmove(inp->buf + target,
                    inp->buf + inp->cursor,
                    (size_t)(inp->len - inp->cursor));
            inp->len -= del;
            inp->cursor = target;
            inp->buf[inp->len] = '\0';
        }
        return 1;
    }

    if (msg.u.key.rune >= 32 && msg.u.key.rune < 127) {
        if (inp->len < FLUX_INPUT_MAX) {
            memmove(inp->buf + inp->cursor + 1,
                    inp->buf + inp->cursor,
                    (size_t)(inp->len - inp->cursor));
            inp->buf[inp->cursor] = (char)msg.u.key.rune;
            inp->len++;
            inp->cursor++;
            inp->buf[inp->len] = '\0';
        }
        return 1;
    }

    if (msg.u.key.rune >= 128 && msg.u.key.key[0] != '\0') {
        int klen = (int)strlen(msg.u.key.key);
        if (klen > 0 && inp->len + klen <= FLUX_INPUT_MAX) {
            memmove(inp->buf + inp->cursor + klen,
                    inp->buf + inp->cursor,
                    (size_t)(inp->len - inp->cursor));
            memcpy(inp->buf + inp->cursor, msg.u.key.key, (size_t)klen);
            inp->len += klen;
            inp->cursor += klen;
            inp->buf[inp->len] = '\0';
        }
        return 1;
    }

    return 0;
}

/* -- Render -- */

void flux_input_render(FluxInput *inp, FluxSB *sb, int width,
                              const char *color, const char *cursor_clr) {
    int i, col, pad;

    if (width <= 0) return;
    if (!color) color = "";
    if (!cursor_clr) cursor_clr = "\x1b[7m";

    if (inp->len == 0) {
        flux_sb_append(sb, cursor_clr);
        flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_RESET);

        if (inp->placeholder && inp->placeholder[0]) {
            flux_sb_append(sb, "\x1b[2m");
            flux_sb_append(sb, color);

            i = 0;
            col = 0;
            while (inp->placeholder[i] && col < width - 1) {
                int clen = _flux_inp_utf8_len(inp->placeholder, i,
                    (int)strlen(inp->placeholder));
                char tmp[8];
                int cw;

                if (clen > 7) clen = 7;
                memcpy(tmp, inp->placeholder + i, (size_t)clen);
                tmp[clen] = '\0';
                cw = flux_strwidth(tmp);
                if (col + cw > width - 1) break;
                flux_sb_append(sb, tmp);
                col += cw;
                i += clen;
            }
            flux_sb_append(sb, FLUX_RESET);

            pad = width - 1 - col;
        } else {
            pad = width - 1;
        }

        while (pad > 0) {
            flux_sb_append(sb, " ");
            pad--;
        }
        return;
    }

    {
        int vis_start;
        int vis_col;
        int cursor_col;
        int pos;

        cursor_col = 0;
        pos = 0;
        while (pos < inp->cursor) {
            cursor_col += _flux_inp_char_width(inp->buf, pos, inp->len);
            pos += _flux_inp_utf8_len(inp->buf, pos, inp->len);
        }

        vis_start = 0;
        if (cursor_col >= width) {
            int skipped_cols = 0;
            int target = cursor_col - width + 1;
            pos = 0;
            while (pos < inp->len && skipped_cols < target) {
                int cw = _flux_inp_char_width(inp->buf, pos, inp->len);
                int clen = _flux_inp_utf8_len(inp->buf, pos, inp->len);
                skipped_cols += cw;
                pos += clen;
            }
            vis_start = pos;
        }

        flux_sb_append(sb, color);
        pos = vis_start;
        vis_col = 0;

        while (pos < inp->len && vis_col < width) {
            int clen = _flux_inp_utf8_len(inp->buf, pos, inp->len);
            int cw = _flux_inp_char_width(inp->buf, pos, inp->len);
            char tmp[8];

            if (vis_col + cw > width) break;
            if (clen > 7) clen = 7;
            memcpy(tmp, inp->buf + pos, (size_t)clen);
            tmp[clen] = '\0';

            if (pos == inp->cursor) {
                flux_sb_append(sb, cursor_clr);
                flux_sb_append(sb, tmp);
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_append(sb, color);
            } else {
                flux_sb_append(sb, tmp);
            }

            vis_col += cw;
            pos += clen;
        }

        if (inp->cursor == inp->len && vis_col < width) {
            flux_sb_append(sb, cursor_clr);
            flux_sb_append(sb, " ");
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, color);
            vis_col++;
        }

        pad = width - vis_col;
        while (pad > 0) {
            flux_sb_append(sb, " ");
            pad--;
        }

        flux_sb_append(sb, FLUX_RESET);
    }
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 8: Spinner, Tabs, Confirm, List, Table  (09)        ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* -- Spinner -- */

void flux_spinner_init(FluxSpinner *s, const char **frames,
                              int nframes, const char *label) {
    s->frames = frames;
    s->frame_count = nframes;
    s->current = 0;
    s->label = label;
    s->period_ms = 0;            /* default: no rate gating  */
    s->last_advance_ms = 0;
}

void flux_spinner_tick(FluxSpinner *s) {
    if (s->frame_count > 0) {
        s->current = (s->current + 1) % s->frame_count;
    }
}

void flux_spinner_set_fps(FluxSpinner *s, int fps) {
    if (!s) return;
    s->period_ms = (fps > 0) ? FLUX_FPS_TO_MS(fps) : 0;
    s->last_advance_ms = 0;
}

void flux_spinner_tick_at(FluxSpinner *s, uint64_t now_ms) {
    if (!s) return;
    if (s->period_ms > 0) {
        if (s->last_advance_ms != 0 &&
            now_ms - s->last_advance_ms < (uint64_t)s->period_ms) {
            return;
        }
        s->last_advance_ms = now_ms;
    }
    if (s->frame_count > 0) {
        s->current = (s->current + 1) % s->frame_count;
    }
}

void flux_spinner_render(FluxSpinner *s, FluxSB *sb,
                                const char *color) {
    if (color) {
        flux_sb_append(sb, color);
    }
    if (s->frames && s->frame_count > 0) {
        flux_sb_append(sb, s->frames[s->current]);
    }
    if (color) {
        flux_sb_append(sb, FLUX_RESET);
    }
    flux_sb_append(sb, " ");
    if (s->label) {
        flux_sb_append(sb, s->label);
    }
}

/* -- Tabs -- */

void flux_tabs_init(FluxTabs *t, const char **icons,
                           const char **labels, int count) {
    t->icons = icons;
    t->labels = labels;
    t->count = count;
    t->active = 0;
}

int flux_tabs_update(FluxTabs *t, FluxMsg msg) {
    if (msg.type != MSG_KEY || t->count <= 0) {
        return 0;
    }
    if (strcmp(msg.u.key.key, "tab") == 0) {
        t->active = (t->active + 1) % t->count;
        return 1;
    }
    if (strcmp(msg.u.key.key, "shift+tab") == 0) {
        t->active = (t->active - 1 + t->count) % t->count;
        return 1;
    }
    return 0;
}

void flux_tabs_render(FluxTabs *t, FluxSB *sb,
                             const char *active_clr,
                             const char *dim_clr,
                             const char *hint) {
    int i;
    int tab_starts[64];
    int tab_widths[64];
    int col = 0;
    int max_tabs = t->count < 64 ? t->count : 64;

    for (i = 0; i < max_tabs; i++) {
        if (i > 0) {
            flux_sb_append(sb, " \xe2\x94\x82 ");
            col += 3;
        }
        tab_starts[i] = col;
        if (i == t->active) {
            flux_sb_append(sb, active_clr ? active_clr : "");
            flux_sb_append(sb, "\x1b[1m"); /* bold only, no underline */
        } else {
            flux_sb_append(sb, dim_clr ? dim_clr : "");
        }
        if (t->icons && t->icons[i]) {
            flux_sb_append(sb, t->icons[i]);
            flux_sb_append(sb, " ");
        }
        if (t->labels && t->labels[i]) {
            flux_sb_append(sb, t->labels[i]);
        }
        {
            int tw = 0;
            if (t->icons && t->icons[i]) {
                tw += flux_strwidth(t->icons[i]) + 1;
            }
            if (t->labels && t->labels[i]) {
                tw += flux_strwidth(t->labels[i]);
            }
            tab_widths[i] = tw;
            col += tw;
        }
        flux_sb_append(sb, FLUX_RESET);
    }

    if (hint) {
        flux_sb_append(sb, "  ");
        flux_sb_append(sb, hint);
    }
    flux_sb_nl(sb);

    {
        int pos = 0;
        int active_start = (t->active >= 0 && t->active < max_tabs)
                           ? tab_starts[t->active] : -1;
        int active_width = (t->active >= 0 && t->active < max_tabs)
                           ? tab_widths[t->active] : 0;
        int total_width = col;

        for (pos = 0; pos < total_width; pos++) {
            if (active_start >= 0
                && pos >= active_start
                && pos < active_start + active_width) {
                flux_sb_append(sb, "\xe2\x94\x80");
            } else {
                flux_sb_append(sb, " ");
            }
        }
        flux_sb_nl(sb);
    }
}

/* -- Confirm dialog -- */

void flux_confirm_init(FluxConfirm *c, const char *title,
                              const char *msg, const char *yes_label,
                              const char *no_label) {
    c->title = title;
    c->message = msg;
    c->yes_label = yes_label ? yes_label : "Yes";
    c->no_label = no_label ? no_label : "No";
    c->selected = 0;
    c->answered = 0;
    c->result = 0;
}

int flux_confirm_update(FluxConfirm *c, FluxMsg msg) {
    if (msg.type != MSG_KEY || c->answered) {
        return 0;
    }
    if (strcmp(msg.u.key.key, "left") == 0) {
        c->selected = 0;
        return 1;
    }
    if (strcmp(msg.u.key.key, "right") == 0) {
        c->selected = 1;
        return 1;
    }
    if (strcmp(msg.u.key.key, "tab") == 0
        || strcmp(msg.u.key.key, "shift+tab") == 0) {
        c->selected = c->selected ? 0 : 1;
        return 1;
    }
    if (strcmp(msg.u.key.key, "enter") == 0) {
        c->answered = 1;
        c->result = c->selected;
        return 1;
    }
    if (msg.u.key.key[0] == 'y' && msg.u.key.key[1] == '\0') {
        c->answered = 1;
        c->result = 0;
        c->selected = 0;
        return 1;
    }
    if (msg.u.key.key[0] == 'n' && msg.u.key.key[1] == '\0') {
        c->answered = 1;
        c->result = 1;
        c->selected = 1;
        return 1;
    }
    return 0;
}

void flux_confirm_render(FluxConfirm *c, FluxSB *sb, int width,
                                const char *border_clr,
                                const char *yes_clr,
                                const char *no_clr) {
    int inner_w = width - 4;
    int i;
    int title_len, msg_len, yes_len, no_len;
    int buttons_w, left_pad;
    const char *R = FLUX_RESET;
    const char *h = "\xe2\x94\x80";

    if (inner_w < 10) inner_w = 10;

    title_len = c->title ? flux_strwidth(c->title) : 0;
    msg_len = c->message ? flux_strwidth(c->message) : 0;
    yes_len = flux_strwidth(c->yes_label);
    no_len = flux_strwidth(c->no_label);

    /* Top border */
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x95\xad");
    if (title_len > 0 && title_len + 4 <= inner_w + 2) {
        flux_sb_append(sb, h);
        flux_sb_append(sb, h);
        flux_sb_append(sb, " ");
        if (border_clr) flux_sb_append(sb, R);
        flux_sb_append(sb, "\x1b[1m");
        flux_sb_append(sb, c->title);
        flux_sb_append(sb, R);
        if (border_clr) flux_sb_append(sb, border_clr);
        flux_sb_append(sb, " ");
        for (i = 0; i < inner_w + 2 - title_len - 4; i++) {
            flux_sb_append(sb, h);
        }
    } else {
        for (i = 0; i < inner_w + 2; i++) {
            flux_sb_append(sb, h);
        }
    }
    flux_sb_append(sb, "\xe2\x95\xae");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);

    /* Empty line */
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    for (i = 0; i < inner_w + 2; i++) {
        flux_sb_append(sb, " ");
    }
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);

    /* Message line */
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_append(sb, "  ");
    if (c->message) {
        flux_sb_append(sb, c->message);
    }
    {
        int pad_val = inner_w - msg_len;
        for (i = 0; i < pad_val; i++) {
            flux_sb_append(sb, " ");
        }
    }
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);

    /* Empty line */
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    for (i = 0; i < inner_w + 2; i++) {
        flux_sb_append(sb, " ");
    }
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);

    /* Button row */
    buttons_w = (4 + yes_len) + 4 + (4 + no_len);
    left_pad = (inner_w + 2 - buttons_w) / 2;
    if (left_pad < 1) left_pad = 1;

    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);

    for (i = 0; i < left_pad; i++) {
        flux_sb_append(sb, " ");
    }

    if (c->selected == 0) {
        if (yes_clr) flux_sb_append(sb, yes_clr);
        flux_sb_append(sb, "\x1b[1m");
        flux_sb_appendf(sb, "[ %s ]", c->yes_label);
        flux_sb_append(sb, R);
    } else {
        flux_sb_appendf(sb, "  %s  ", c->yes_label);
    }

    flux_sb_append(sb, "    ");

    if (c->selected == 1) {
        if (no_clr) flux_sb_append(sb, no_clr);
        flux_sb_append(sb, "\x1b[1m");
        flux_sb_appendf(sb, "[ %s ]", c->no_label);
        flux_sb_append(sb, R);
    } else {
        flux_sb_appendf(sb, "  %s  ", c->no_label);
    }

    {
        int used = left_pad + buttons_w;
        int rpad = inner_w + 2 - used;
        for (i = 0; i < rpad; i++) {
            flux_sb_append(sb, " ");
        }
    }

    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);

    /* Empty line */
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    for (i = 0; i < inner_w + 2; i++) {
        flux_sb_append(sb, " ");
    }
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);

    /* Bottom border */
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x95\xb0");
    for (i = 0; i < inner_w + 2; i++) {
        flux_sb_append(sb, h);
    }
    flux_sb_append(sb, "\xe2\x95\xaf");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);
}

/* -- List -- */

void flux_list_init(FluxList *l, int count, int visible,
                           FluxListItemFn render) {
    l->cursor = 0;
    l->scroll = 0;
    l->count = count;
    l->visible = visible;
    l->render_item = render;
}

static void _flux_list_clamp_scroll(FluxList *l) {
    if (l->cursor < l->scroll) {
        l->scroll = l->cursor;
    }
    if (l->cursor >= l->scroll + l->visible) {
        l->scroll = l->cursor - l->visible + 1;
    }
    if (l->scroll < 0) {
        l->scroll = 0;
    }
    if (l->count > l->visible && l->scroll > l->count - l->visible) {
        l->scroll = l->count - l->visible;
    }
}

int flux_list_update(FluxList *l, FluxMsg msg) {
    if (msg.type != MSG_KEY || l->count <= 0) {
        return 0;
    }
    if (strcmp(msg.u.key.key, "up") == 0
        || (msg.u.key.key[0] == 'k' && msg.u.key.key[1] == '\0')) {
        if (l->cursor > 0) {
            l->cursor--;
            _flux_list_clamp_scroll(l);
        }
        return 1;
    }
    if (strcmp(msg.u.key.key, "down") == 0
        || (msg.u.key.key[0] == 'j' && msg.u.key.key[1] == '\0')) {
        if (l->cursor < l->count - 1) {
            l->cursor++;
            _flux_list_clamp_scroll(l);
        }
        return 1;
    }
    if (strcmp(msg.u.key.key, "pgup") == 0) {
        l->cursor -= l->visible;
        if (l->cursor < 0) l->cursor = 0;
        _flux_list_clamp_scroll(l);
        return 1;
    }
    if (strcmp(msg.u.key.key, "pgdown") == 0) {
        l->cursor += l->visible;
        if (l->cursor >= l->count) l->cursor = l->count - 1;
        _flux_list_clamp_scroll(l);
        return 1;
    }
    if (strcmp(msg.u.key.key, "home") == 0) {
        l->cursor = 0;
        _flux_list_clamp_scroll(l);
        return 1;
    }
    if (strcmp(msg.u.key.key, "end") == 0) {
        l->cursor = l->count - 1;
        _flux_list_clamp_scroll(l);
        return 1;
    }
    return 0;
}

void flux_list_render(FluxList *l, FluxSB *sb, int width,
                             void *ctx) {
    int i;
    int end;

    if (!l->render_item || l->count <= 0) {
        return;
    }

    _flux_list_clamp_scroll(l);

    end = l->scroll + l->visible;
    if (end > l->count) end = l->count;

    for (i = l->scroll; i < end; i++) {
        l->render_item(sb, i, (i == l->cursor) ? 1 : 0, width, ctx);
        flux_sb_nl(sb);
    }
}

/* -- Table -- */

void flux_table_init(FluxTable *t, const char **headers,
                            const int *widths, int cols, int visible,
                            FluxTableCellFn render) {
    t->headers = headers;
    t->widths = widths;
    t->col_count = cols;
    t->row_count = 0;
    t->visible = visible;
    t->scroll = 0;
    t->follow = 0;
    t->render_cell = render;
}

void flux_table_set_rows(FluxTable *t, int rows) {
    t->row_count = rows;
    if (t->follow && rows > t->visible) {
        t->scroll = rows - t->visible;
    }
}

int flux_table_update(FluxTable *t, FluxMsg msg) {
    int max_scroll;

    if (msg.type != MSG_KEY) {
        return 0;
    }

    max_scroll = t->row_count - t->visible;
    if (max_scroll < 0) max_scroll = 0;

    if (strcmp(msg.u.key.key, "up") == 0
        || (msg.u.key.key[0] == 'k' && msg.u.key.key[1] == '\0')) {
        if (t->scroll > 0) {
            t->scroll--;
            t->follow = 0;
        }
        return 1;
    }
    if (strcmp(msg.u.key.key, "down") == 0
        || (msg.u.key.key[0] == 'j' && msg.u.key.key[1] == '\0')) {
        if (t->scroll < max_scroll) {
            t->scroll++;
            if (t->scroll >= max_scroll) {
                t->follow = 1;
            }
        }
        return 1;
    }
    if (strcmp(msg.u.key.key, "pgup") == 0) {
        t->scroll -= t->visible;
        if (t->scroll < 0) t->scroll = 0;
        t->follow = 0;
        return 1;
    }
    if (strcmp(msg.u.key.key, "pgdown") == 0) {
        t->scroll += t->visible;
        if (t->scroll > max_scroll) t->scroll = max_scroll;
        if (t->scroll >= max_scroll) t->follow = 1;
        return 1;
    }
    if (strcmp(msg.u.key.key, "home") == 0) {
        t->scroll = 0;
        t->follow = 0;
        return 1;
    }
    if (strcmp(msg.u.key.key, "end") == 0) {
        t->scroll = max_scroll;
        t->follow = 1;
        return 1;
    }
    return 0;
}

static void _flux_table_render_row_sep(FluxTable *t, FluxSB *sb,
                                       const char *border_clr) {
    int c, w;
    const char *R = FLUX_RESET;

    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x9c");
    for (c = 0; c < t->col_count; c++) {
        if (c > 0) {
            flux_sb_append(sb, "\xe2\x94\xbc");
        }
        w = t->widths[c] + 2;
        {
            int k;
            for (k = 0; k < w; k++) {
                flux_sb_append(sb, "\xe2\x94\x80");
            }
        }
    }
    flux_sb_append(sb, "\xe2\x94\xa4");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);
}

static void _flux_table_render_top(FluxTable *t, FluxSB *sb,
                                   const char *border_clr) {
    int c, w;
    const char *R = FLUX_RESET;

    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x8c");
    for (c = 0; c < t->col_count; c++) {
        if (c > 0) {
            flux_sb_append(sb, "\xe2\x94\xac");
        }
        w = t->widths[c] + 2;
        {
            int k;
            for (k = 0; k < w; k++) {
                flux_sb_append(sb, "\xe2\x94\x80");
            }
        }
    }
    flux_sb_append(sb, "\xe2\x94\x90");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);
}

static void _flux_table_render_bottom(FluxTable *t, FluxSB *sb,
                                      const char *border_clr) {
    int c, w;
    const char *R = FLUX_RESET;

    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x94");
    for (c = 0; c < t->col_count; c++) {
        if (c > 0) {
            flux_sb_append(sb, "\xe2\x94\xb4");
        }
        w = t->widths[c] + 2;
        {
            int k;
            for (k = 0; k < w; k++) {
                flux_sb_append(sb, "\xe2\x94\x80");
            }
        }
    }
    flux_sb_append(sb, "\xe2\x94\x98");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);
}

static void _flux_table_render_data_row(FluxTable *t, FluxSB *sb,
                                        int row, void *ctx,
                                        const char *border_clr,
                                        const char *text_clr) {
    int c;
    const char *R = FLUX_RESET;

    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_clr) flux_sb_append(sb, R);

    for (c = 0; c < t->col_count; c++) {
        int cell_start;

        flux_sb_append(sb, " ");
        cell_start = sb->len;

        if (text_clr) flux_sb_append(sb, text_clr);
        if (t->render_cell) {
            t->render_cell(sb, row, c, t->widths[c], ctx);
        }
        if (text_clr) flux_sb_append(sb, R);

        {
            char measure_buf[1024];
            int written_len = sb->len - cell_start;
            int vis_w;

            if (written_len > 0 && written_len < (int)sizeof(measure_buf)) {
                memcpy(measure_buf, sb->buf + cell_start, (size_t)written_len);
                measure_buf[written_len] = '\0';
                vis_w = flux_strwidth(measure_buf);
            } else {
                vis_w = 0;
            }
            {
                int pad_val = t->widths[c] - vis_w;
                int k;
                for (k = 0; k < pad_val; k++) {
                    flux_sb_append(sb, " ");
                }
            }
        }

        flux_sb_append(sb, " ");
        if (border_clr) flux_sb_append(sb, border_clr);
        flux_sb_append(sb, "\xe2\x94\x82");
        if (border_clr) flux_sb_append(sb, R);
    }
    flux_sb_nl(sb);
}

void flux_table_render(FluxTable *t, FluxSB *sb, void *ctx,
                               const char *header_clr,
                               const char *border_clr) {
    int r, end;
    const char *R = FLUX_RESET;

    _flux_table_render_top(t, sb, border_clr);

    if (t->headers) {
        if (border_clr) flux_sb_append(sb, border_clr);
        flux_sb_append(sb, "\xe2\x94\x82");
        if (border_clr) flux_sb_append(sb, R);
        {
            int c;
            for (c = 0; c < t->col_count; c++) {
                int hdr_w;

                flux_sb_append(sb, " ");
                if (header_clr) flux_sb_append(sb, header_clr);
                flux_sb_append(sb, "\x1b[1m");
                if (t->headers[c]) {
                    flux_sb_append(sb, t->headers[c]);
                    hdr_w = flux_strwidth(t->headers[c]);
                } else {
                    hdr_w = 0;
                }
                flux_sb_append(sb, R);

                {
                    int pad_val = t->widths[c] - hdr_w;
                    int k;
                    for (k = 0; k < pad_val; k++) {
                        flux_sb_append(sb, " ");
                    }
                }

                flux_sb_append(sb, " ");
                if (border_clr) flux_sb_append(sb, border_clr);
                flux_sb_append(sb, "\xe2\x94\x82");
                if (border_clr) flux_sb_append(sb, R);
            }
        }
        flux_sb_nl(sb);

        _flux_table_render_row_sep(t, sb, border_clr);
    }

    end = t->scroll + t->visible;
    if (end > t->row_count) end = t->row_count;

    for (r = t->scroll; r < end; r++) {
        _flux_table_render_data_row(t, sb, r, ctx, border_clr, NULL);
    }

    _flux_table_render_bottom(t, sb, border_clr);
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 9: Viewport, Popup, Sparkline, Bar, Kanban  (10)    ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* -- Viewport -- */

void flux_viewport_init(FluxViewport *vp, int width, int height) {
    if (!vp) return;
    memset(vp, 0, sizeof(*vp));
    vp->width = width;
    vp->total_height = height;
    vp->count = 0;
}

void flux_viewport_add(FluxViewport *vp, FluxRegionType type,
                              int height, FluxRegionRenderFn render,
                              void *ctx) {
    FluxRegion *r;

    if (!vp || vp->count >= FLUX_REGION_MAX) return;

    r = &vp->regions[vp->count];
    r->type = type;
    r->height = height;
    r->render = render;
    r->ctx = ctx;
    vp->count++;
}

void flux_viewport_render(FluxViewport *vp, char *buf, int bufsz) {
    #define _VP_TMPBUF 65536
    #define _VP_MAXLINES 512
    char tmpbuf[_VP_TMPBUF];
    FluxSB tmp;
    char *lines[_VP_MAXLINES];
    int line_count;
    int fixed_sum;
    int fill_h;
    int out_line;
    int i, j;
    char *o;
    int rem;

    if (!vp || !buf || bufsz <= 0) return;
    buf[0] = '\0';

    fixed_sum = 0;
    for (i = 0; i < vp->count; i++) {
        if (vp->regions[i].type == FLUX_REGION_FIXED) {
            fixed_sum += vp->regions[i].height;
        }
    }
    fill_h = vp->total_height - fixed_sum;
    if (fill_h < 0) fill_h = 0;

    o = buf;
    rem = bufsz - 1;
    out_line = 0;

    for (i = 0; i < vp->count && out_line < vp->total_height; i++) {
        int region_h;
        char *p;
        char *nl;

        region_h = (vp->regions[i].type == FLUX_REGION_FILL)
                 ? fill_h : vp->regions[i].height;
        if (region_h <= 0) continue;

        flux_sb_init(&tmp, tmpbuf, _VP_TMPBUF);
        if (vp->regions[i].render) {
            vp->regions[i].render(&tmp, vp->width, region_h,
                                  vp->regions[i].ctx);
        }

        line_count = 0;
        p = tmpbuf;
        while (*p && line_count < _VP_MAXLINES) {
            lines[line_count++] = p;
            nl = strchr(p, '\n');
            if (!nl) break;
            *nl = '\0';
            p = nl + 1;
        }

        for (j = 0; j < region_h && out_line < vp->total_height; j++) {
            const char *line_str;
            int len;

            if (out_line > 0) {
                if (rem > 0) {
                    *o++ = '\n';
                    rem--;
                }
            }

            line_str = (j < line_count) ? lines[j] : "";
            len = (int)strlen(line_str);
            if (len > rem) len = rem;
            if (len > 0) {
                memcpy(o, line_str, (size_t)len);
                o += len;
                rem -= len;
            }
            out_line++;
        }
    }

    while (out_line < vp->total_height) {
        if (out_line > 0) {
            if (rem > 0) {
                *o++ = '\n';
                rem--;
            }
        }
        out_line++;
    }

    *o = '\0';
    #undef _VP_TMPBUF
    #undef _VP_MAXLINES
}

/* -- Popup -- */

void flux_popup(FluxSB *sb, int area_w, int area_h,
                       const char *title, const char **items, int count,
                       int selected, const char *hint,
                       const char *border_clr, const char *select_bg,
                       const char *normal_bg, const char *accent_clr) {
    int max_item_w;
    int box_w, box_h;
    int inner_w;
    int pad_top, pad_left;
    int title_w, hint_w;
    int i, j;
    const char *R = FLUX_RESET;

    if (!sb || !items || count <= 0) return;

    max_item_w = 0;
    for (i = 0; i < count; i++) {
        int w = flux_strwidth(items[i]);
        if (w > max_item_w) max_item_w = w;
    }

    inner_w = max_item_w + 4;
    title_w = title ? flux_strwidth(title) : 0;
    hint_w = hint ? flux_strwidth(hint) : 0;

    if (title_w + 4 > inner_w) inner_w = title_w + 4;
    if (hint_w + 4 > inner_w) inner_w = hint_w + 4;

    box_w = inner_w + 2;
    if (box_w > area_w - 4) {
        box_w = area_w - 4;
        if (box_w < 8) box_w = 8;
        inner_w = box_w - 2;
    }

    box_h = count + 2;
    if (box_h > area_h - 2) {
        box_h = area_h - 2;
        if (box_h < 3) box_h = 3;
    }

    pad_top = (area_h - box_h) / 2;
    pad_left = (area_w - box_w) / 2;
    if (pad_top < 0) pad_top = 0;
    if (pad_left < 0) pad_left = 0;

    for (i = 0; i < pad_top; i++) {
        flux_sb_nl(sb);
    }

    /* Top border */
    for (j = 0; j < pad_left; j++) {
        flux_sb_append(sb, " ");
    }
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x95\xad");

    if (title && title_w > 0) {
        int dashes_total = inner_w - title_w - 2;
        int dashes_left = dashes_total / 2;
        int dashes_right = dashes_total - dashes_left;
        if (dashes_left < 1) dashes_left = 1;
        if (dashes_right < 1) dashes_right = 1;
        for (j = 0; j < dashes_left; j++) {
            flux_sb_append(sb, "\xe2\x94\x80");
        }
        flux_sb_append(sb, " ");
        if (accent_clr) flux_sb_append(sb, accent_clr);
        flux_sb_append(sb, title);
        if (border_clr) flux_sb_append(sb, border_clr);
        flux_sb_append(sb, " ");
        for (j = 0; j < dashes_right; j++) {
            flux_sb_append(sb, "\xe2\x94\x80");
        }
    } else {
        for (j = 0; j < inner_w; j++) {
            flux_sb_append(sb, "\xe2\x94\x80");
        }
    }

    flux_sb_append(sb, "\xe2\x95\xae");
    if (border_clr) flux_sb_append(sb, R);
    flux_sb_nl(sb);

    /* Item rows */
    {
        int visible = box_h - 2;
        for (i = 0; i < visible && i < count; i++) {
            int item_w;
            int pad_val;

            for (j = 0; j < pad_left; j++) {
                flux_sb_append(sb, " ");
            }

            if (border_clr) flux_sb_append(sb, border_clr);
            flux_sb_append(sb, "\xe2\x94\x82");
            flux_sb_append(sb, R);

            if (i == selected) {
                if (select_bg) flux_sb_append(sb, select_bg);
                flux_sb_append(sb, " ");
                if (accent_clr) flux_sb_append(sb, accent_clr);
                flux_sb_append(sb, "\xe2\x96\xb8 ");
                if (select_bg) flux_sb_append(sb, select_bg);
            } else {
                if (normal_bg) flux_sb_append(sb, normal_bg);
                flux_sb_append(sb, "   ");
            }

            flux_sb_append(sb, items[i]);

            item_w = flux_strwidth(items[i]);
            pad_val = inner_w - 3 - item_w - 1;
            for (j = 0; j < pad_val; j++) {
                flux_sb_append(sb, " ");
            }
            flux_sb_append(sb, " ");
            flux_sb_append(sb, R);

            if (border_clr) flux_sb_append(sb, border_clr);
            flux_sb_append(sb, "\xe2\x94\x82");
            flux_sb_append(sb, R);
            flux_sb_nl(sb);
        }
    }

    /* Bottom border */
    for (j = 0; j < pad_left; j++) {
        flux_sb_append(sb, " ");
    }
    if (border_clr) flux_sb_append(sb, border_clr);
    flux_sb_append(sb, "\xe2\x95\xb0");

    if (hint && hint_w > 0) {
        int dashes_total = inner_w - hint_w - 2;
        int dashes_left = dashes_total / 2;
        int dashes_right = dashes_total - dashes_left;
        if (dashes_left < 1) dashes_left = 1;
        if (dashes_right < 1) dashes_right = 1;
        for (j = 0; j < dashes_left; j++) {
            flux_sb_append(sb, "\xe2\x94\x80");
        }
        flux_sb_append(sb, " ");
        if (accent_clr) flux_sb_append(sb, accent_clr);
        flux_sb_append(sb, hint);
        if (border_clr) flux_sb_append(sb, border_clr);
        flux_sb_append(sb, " ");
        for (j = 0; j < dashes_right; j++) {
            flux_sb_append(sb, "\xe2\x94\x80");
        }
    } else {
        for (j = 0; j < inner_w; j++) {
            flux_sb_append(sb, "\xe2\x94\x80");
        }
    }

    flux_sb_append(sb, "\xe2\x95\xaf");
    if (border_clr) flux_sb_append(sb, R);

    {
        int rows_used = pad_top + box_h;
        for (i = rows_used; i < area_h; i++) {
            flux_sb_nl(sb);
        }
    }
}

/* -- Sparkline -- */

void flux_sparkline(FluxSB *sb, float *ring, int len, int head,
                           const char *color) {
    static const char *bars[8] = {
        "\xe2\x96\x81",
        "\xe2\x96\x82",
        "\xe2\x96\x83",
        "\xe2\x96\x84",
        "\xe2\x96\x85",
        "\xe2\x96\x86",
        "\xe2\x96\x87",
        "\xe2\x96\x88"
    };
    float vmin, vmax, range;
    int i, idx;

    if (!sb || !ring || len <= 0) return;

    vmin = ring[0];
    vmax = ring[0];
    for (i = 0; i < len; i++) {
        if (ring[i] < vmin) vmin = ring[i];
        if (ring[i] > vmax) vmax = ring[i];
    }
    range = vmax - vmin;

    if (color) flux_sb_append(sb, color);

    for (i = 0; i < len; i++) {
        float v;
        int level;

        idx = (head + i) % len;
        v = ring[idx];

        if (range > 0.0f) {
            level = (int)(((v - vmin) / range) * 7.0f + 0.5f);
        } else {
            level = 3;
        }
        if (level < 0) level = 0;
        if (level > 7) level = 7;

        flux_sb_append(sb, bars[level]);
    }

    if (color) flux_sb_append(sb, FLUX_RESET);
}

/* -- Bar -- */

void flux_bar(FluxSB *sb, float value, int width,
                     const char *fill_clr, const char *empty_clr) {
    int filled, i;

    if (!sb || width <= 0) return;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    filled = (int)(value * (float)width + 0.5f);
    if (filled > width) filled = width;

    if (fill_clr) flux_sb_append(sb, fill_clr);
    for (i = 0; i < filled; i++) {
        flux_sb_append(sb, "\xe2\x96\x88");
    }
    if (fill_clr) flux_sb_append(sb, FLUX_RESET);

    if (empty_clr) flux_sb_append(sb, empty_clr);
    for (i = filled; i < width; i++) {
        flux_sb_append(sb, "\xe2\x96\x91");
    }
    if (empty_clr) flux_sb_append(sb, FLUX_RESET);
}

/* -- Kanban -- */

#define _KB_MODE_NAVIGATE 0
#define _KB_MODE_EDIT     1

void flux_kanban_init(FluxKanban *kb, int ncols,
                             const char **col_names, int col_width) {
    int i;

    if (!kb) return;
    memset(kb, 0, sizeof(*kb));

    if (ncols > FLUX_KB_MAX_COLS) ncols = FLUX_KB_MAX_COLS;
    kb->ncols = ncols;
    kb->col_width = col_width > 8 ? col_width : 8;
    kb->visible = 1;

    for (i = 0; i < ncols; i++) {
        if (col_names && col_names[i]) {
            snprintf(kb->cols[i].name, sizeof(kb->cols[i].name),
                     "%s", col_names[i]);
        }
        kb->cols[i].count = 0;
    }
}

int flux_kanban_add(FluxKanban *kb, int col,
                           const char *title, const char *desc) {
    FluxKbCard *card;

    if (!kb || col < 0 || col >= kb->ncols) return -1;
    if (kb->cols[col].count >= FLUX_KB_MAX_CARDS) return -1;

    card = &kb->cols[col].cards[kb->cols[col].count];
    snprintf(card->title, sizeof(card->title), "%s",
             title ? title : "");
    snprintf(card->desc, sizeof(card->desc), "%s",
             desc ? desc : "");
    kb->cols[col].count++;
    kb->dirty = 1;
    return 0;
}

int flux_kanban_del(FluxKanban *kb, int col, int row) {
    FluxKbCol *c;
    int i;

    if (!kb || col < 0 || col >= kb->ncols) return -1;
    c = &kb->cols[col];
    if (row < 0 || row >= c->count) return -1;

    for (i = row; i < c->count - 1; i++) {
        c->cards[i] = c->cards[i + 1];
    }
    c->count--;
    kb->dirty = 1;
    return 0;
}

int flux_kanban_move(FluxKanban *kb, int col, int row,
                            int to_col) {
    FluxKbCard card;
    FluxKbCol *src;

    if (!kb) return -1;
    if (col < 0 || col >= kb->ncols) return -1;
    if (to_col < 0 || to_col >= kb->ncols) return -1;
    if (col == to_col) return 0;

    src = &kb->cols[col];
    if (row < 0 || row >= src->count) return -1;
    if (kb->cols[to_col].count >= FLUX_KB_MAX_CARDS) return -1;

    card = src->cards[row];
    flux_kanban_del(kb, col, row);
    kb->cols[to_col].cards[kb->cols[to_col].count] = card;
    kb->cols[to_col].count++;
    kb->dirty = 1;
    return 0;
}

int flux_kanban_dirty(FluxKanban *kb) {
    int d;
    if (!kb) return 0;
    d = kb->dirty;
    kb->dirty = 0;
    return d;
}

int flux_kanban_update(FluxKanban *kb, FluxMsg msg) {
    FluxKbCol *col;

    if (!kb || !kb->visible) return 0;
    if (msg.type != MSG_KEY) return 0;

    if (kb->mode == _KB_MODE_EDIT) {
        if (flux_key_is(msg, "esc")) {
            kb->mode = _KB_MODE_NAVIGATE;
            return 1;
        }
        if (flux_key_is(msg, "tab")) {
            kb->edit_focus = kb->edit_focus ? 0 : 1;
            return 1;
        }
        if (flux_key_is(msg, "enter")) {
            FluxKbCard *card;
            int ec = kb->edit_col;
            int er = kb->edit_row;

            if (ec >= 0 && ec < kb->ncols
                && er >= 0 && er < kb->cols[ec].count) {
                card = &kb->cols[ec].cards[er];
                memcpy(card->title, kb->edit_title.buf,
                       FLUX_KB_TITLE_MAX);
                card->title[FLUX_KB_TITLE_MAX] = '\0';
                memcpy(card->desc, kb->edit_desc.buf,
                       FLUX_KB_DESC_MAX);
                card->desc[FLUX_KB_DESC_MAX] = '\0';
                kb->dirty = 1;
            } else if (ec >= 0 && ec < kb->ncols
                       && er == kb->cols[ec].count) {
                flux_kanban_add(kb, ec, kb->edit_title.buf,
                                kb->edit_desc.buf);
            }
            kb->mode = _KB_MODE_NAVIGATE;
            return 1;
        }
        if (kb->edit_focus == 0) {
            flux_input_update(&kb->edit_title, msg);
        } else {
            flux_input_update(&kb->edit_desc, msg);
        }
        return 1;
    }

    col = &kb->cols[kb->cur_col];

    if (flux_key_is(msg, "h") || flux_key_is(msg, "left")) {
        if (kb->grabbed && kb->cur_col > 0
            && col->count > 0 && kb->cur_row < col->count) {
            int old_col = kb->cur_col;
            int old_row = kb->cur_row;
            flux_kanban_move(kb, old_col, old_row, old_col - 1);
            kb->cur_col = old_col - 1;
            kb->cur_row = kb->cols[kb->cur_col].count - 1;
        } else if (kb->cur_col > 0) {
            kb->cur_col--;
            if (kb->cur_row >= kb->cols[kb->cur_col].count) {
                kb->cur_row = kb->cols[kb->cur_col].count - 1;
            }
            if (kb->cur_row < 0) kb->cur_row = 0;
        }
        return 1;
    }

    if (flux_key_is(msg, "l") || flux_key_is(msg, "right")) {
        if (kb->grabbed && kb->cur_col < kb->ncols - 1
            && col->count > 0 && kb->cur_row < col->count) {
            int old_col = kb->cur_col;
            int old_row = kb->cur_row;
            flux_kanban_move(kb, old_col, old_row, old_col + 1);
            kb->cur_col = old_col + 1;
            kb->cur_row = kb->cols[kb->cur_col].count - 1;
        } else if (kb->cur_col < kb->ncols - 1) {
            kb->cur_col++;
            if (kb->cur_row >= kb->cols[kb->cur_col].count) {
                kb->cur_row = kb->cols[kb->cur_col].count - 1;
            }
            if (kb->cur_row < 0) kb->cur_row = 0;
        }
        return 1;
    }

    if (flux_key_is(msg, "j") || flux_key_is(msg, "down")) {
        if (kb->cur_row < col->count - 1) {
            kb->cur_row++;
        }
        return 1;
    }

    if (flux_key_is(msg, "k") || flux_key_is(msg, "up")) {
        if (kb->cur_row > 0) {
            kb->cur_row--;
        }
        return 1;
    }

    if (flux_key_is(msg, "enter")) {
        if (col->count > 0 && kb->cur_row < col->count) {
            FluxKbCard *card = &col->cards[kb->cur_row];
            kb->mode = _KB_MODE_EDIT;
            kb->edit_col = kb->cur_col;
            kb->edit_row = kb->cur_row;
            kb->edit_focus = 0;
            flux_input_init(&kb->edit_title, "Title");
            flux_input_init(&kb->edit_desc, "Description");
            snprintf(kb->edit_title.buf, sizeof(kb->edit_title.buf),
                     "%s", card->title);
            kb->edit_title.len = (int)strlen(kb->edit_title.buf);
            kb->edit_title.cursor = kb->edit_title.len;
            snprintf(kb->edit_desc.buf, sizeof(kb->edit_desc.buf),
                     "%s", card->desc);
            kb->edit_desc.len = (int)strlen(kb->edit_desc.buf);
            kb->edit_desc.cursor = kb->edit_desc.len;
        }
        return 1;
    }

    if (flux_key_is(msg, "n")) {
        kb->mode = _KB_MODE_EDIT;
        kb->edit_col = kb->cur_col;
        kb->edit_row = col->count;
        kb->edit_focus = 0;
        flux_input_init(&kb->edit_title, "Title");
        flux_input_init(&kb->edit_desc, "Description");
        return 1;
    }

    if (flux_key_is(msg, "d")) {
        if (col->count > 0 && kb->cur_row < col->count) {
            flux_kanban_del(kb, kb->cur_col, kb->cur_row);
            if (kb->cur_row >= col->count && col->count > 0) {
                kb->cur_row = col->count - 1;
            }
            if (col->count == 0) kb->cur_row = 0;
        }
        return 1;
    }

    if (flux_key_is(msg, "g")) {
        kb->grabbed = !kb->grabbed;
        return 1;
    }

    return 0;
}

static void _flux_kb_render_card(FluxSB *sb, const FluxKbCard *card,
                                 int width, int is_selected, int is_grabbed,
                                 const char *col_color) {
    int inner_w;
    int tw, dw, pad_val;
    int j;
    const char *R = FLUX_RESET;

    inner_w = width - 2;
    if (inner_w < 1) inner_w = 1;

    if (col_color) flux_sb_append(sb, col_color);
    flux_sb_append(sb, "\xe2\x94\x8c");
    for (j = 0; j < inner_w; j++) {
        flux_sb_append(sb, "\xe2\x94\x80");
    }
    flux_sb_append(sb, "\xe2\x94\x90");
    flux_sb_append(sb, R);
    flux_sb_nl(sb);

    if (col_color) flux_sb_append(sb, col_color);
    flux_sb_append(sb, "\xe2\x94\x82");
    flux_sb_append(sb, R);

    if (is_selected) {
        if (is_grabbed) {
            flux_sb_append(sb, "\x1b[7m");
        } else {
            flux_sb_append(sb, "\x1b[1m");
        }
    }

    tw = flux_strwidth(card->title);
    pad_val = inner_w - tw;
    if (pad_val < 0) pad_val = 0;
    flux_sb_append(sb, card->title);
    for (j = 0; j < pad_val; j++) {
        flux_sb_append(sb, " ");
    }

    if (is_selected) flux_sb_append(sb, R);

    if (col_color) flux_sb_append(sb, col_color);
    flux_sb_append(sb, "\xe2\x94\x82");
    flux_sb_append(sb, R);
    flux_sb_nl(sb);

    if (card->desc[0]) {
        if (col_color) flux_sb_append(sb, col_color);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, R);

        flux_sb_append(sb, "\x1b[2m");
        dw = flux_strwidth(card->desc);
        if (dw > inner_w) {
            char trunc[FLUX_KB_DESC_MAX + 1];
            int ti;
            for (ti = 0; ti < inner_w - 1 && card->desc[ti]; ti++) {
                trunc[ti] = card->desc[ti];
            }
            trunc[ti] = '\0';
            flux_sb_append(sb, trunc);
            flux_sb_append(sb, "\xe2\x80\xa6");
        } else {
            flux_sb_append(sb, card->desc);
            pad_val = inner_w - dw;
            for (j = 0; j < pad_val; j++) {
                flux_sb_append(sb, " ");
            }
        }
        flux_sb_append(sb, R);

        if (col_color) flux_sb_append(sb, col_color);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, R);
        flux_sb_nl(sb);
    }

    if (col_color) flux_sb_append(sb, col_color);
    flux_sb_append(sb, "\xe2\x94\x94");
    for (j = 0; j < inner_w; j++) {
        flux_sb_append(sb, "\xe2\x94\x80");
    }
    flux_sb_append(sb, "\xe2\x94\x98");
    flux_sb_append(sb, R);
    flux_sb_nl(sb);
}

void flux_kanban_render(FluxKanban *kb, FluxSB *sb,
                               const char **col_colors,
                               const char *hint) {
    #define _KB_COL_BUF 8192
    #define _KB_MAX_RENDER_COLS 8
    char col_bufs[_KB_MAX_RENDER_COLS][_KB_COL_BUF];
    const char *panels[_KB_MAX_RENDER_COLS];
    int widths[_KB_MAX_RENDER_COLS];
    int i, j;
    const char *R = FLUX_RESET;

    if (!kb || !sb || !kb->visible) return;

    for (i = 0; i < kb->ncols && i < _KB_MAX_RENDER_COLS; i++) {
        FluxSB col_sb;
        FluxKbCol *col_data = &kb->cols[i];
        const char *cc = (col_colors && col_colors[i])
                       ? col_colors[i] : "";
        int name_w, name_pad_l, name_pad_r;

        flux_sb_init(&col_sb, col_bufs[i], _KB_COL_BUF);

        name_w = flux_strwidth(col_data->name);
        name_pad_l = (kb->col_width - name_w) / 2;
        name_pad_r = kb->col_width - name_w - name_pad_l;
        if (name_pad_l < 0) name_pad_l = 0;
        if (name_pad_r < 0) name_pad_r = 0;

        if (cc[0]) flux_sb_append(&col_sb, cc);
        flux_sb_append(&col_sb, "\x1b[1m");
        for (j = 0; j < name_pad_l; j++) {
            flux_sb_append(&col_sb, " ");
        }
        flux_sb_append(&col_sb, col_data->name);
        for (j = 0; j < name_pad_r; j++) {
            flux_sb_append(&col_sb, " ");
        }
        flux_sb_append(&col_sb, R);
        flux_sb_nl(&col_sb);

        if (cc[0]) flux_sb_append(&col_sb, cc);
        for (j = 0; j < kb->col_width; j++) {
            flux_sb_append(&col_sb, "\xe2\x94\x80");
        }
        flux_sb_append(&col_sb, R);
        flux_sb_nl(&col_sb);

        for (j = 0; j < col_data->count; j++) {
            int is_sel = (i == kb->cur_col && j == kb->cur_row);
            int is_grab = is_sel && kb->grabbed;
            _flux_kb_render_card(&col_sb, &col_data->cards[j],
                                 kb->col_width, is_sel, is_grab,
                                 cc);
        }

        if (kb->mode == _KB_MODE_EDIT && kb->edit_col == i) {
            flux_sb_append(&col_sb, "\xe2\x94\x80 ");
            if (cc[0]) flux_sb_append(&col_sb, cc);
            flux_sb_append(&col_sb, "Title: ");
            flux_sb_append(&col_sb, R);
            flux_input_render(&kb->edit_title, &col_sb,
                              kb->col_width - 8,
                              kb->edit_focus == 0 ? cc : "",
                              "\x1b[7m");
            flux_sb_nl(&col_sb);

            flux_sb_append(&col_sb, "\xe2\x94\x80 ");
            if (cc[0]) flux_sb_append(&col_sb, cc);
            flux_sb_append(&col_sb, "Desc:  ");
            flux_sb_append(&col_sb, R);
            flux_input_render(&kb->edit_desc, &col_sb,
                              kb->col_width - 8,
                              kb->edit_focus == 1 ? cc : "",
                              "\x1b[7m");
            flux_sb_nl(&col_sb);
        }

        panels[i] = col_bufs[i];
        widths[i] = kb->col_width;
    }

    flux_hbox(sb, panels, widths, kb->ncols < _KB_MAX_RENDER_COLS
              ? kb->ncols : _KB_MAX_RENDER_COLS, "  ");

    if (hint) {
        flux_sb_append(sb, "\x1b[2m");
        flux_sb_append(sb, hint);
        flux_sb_append(sb, R);
        flux_sb_nl(sb);
    }

    #undef _KB_COL_BUF
    #undef _KB_MAX_RENDER_COLS
}


/* ╔════════════════════════════════════════════════════════════════╗
 * ║  SECTION 10: Event loop  (06_event_loop.c) -- LAST           ║
 * ╚════════════════════════════════════════════════════════════════╝ */

/* -- Globals -- */

static struct termios      _flux_orig_termios;
static volatile sig_atomic_t _flux_cols  = 80;
static volatile sig_atomic_t _flux_rows  = 24;
static volatile sig_atomic_t _flux_winch = 0;
static volatile sig_atomic_t _flux_force = 0;
static volatile int        _flux_running = 0;
static int                 _flux_pipe_r  = -1;
static int                 _flux_pipe_w  = -1;

/* -- Terminal queries -- */

int flux_cols(void) { return (int)_flux_cols; }
int flux_rows(void) { return (int)_flux_rows; }

void flux_request_force_redraw(void) { _flux_force = 1; }

/* Drives flux_tty_focused(). Updated by the input parser when it sees
 * \x1b[I (focus-in, sets to 1) or \x1b[O (focus-out, sets to 0).
 * Defaults to 1 so terminals that never report focus events behave
 * as "always focused" rather than "never focused". */
static volatile sig_atomic_t _flux_tty_focused = 1;

int flux_tty_focused(void) { return (int)_flux_tty_focused; }

static void _flux_update_winsize(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        _flux_cols = ws.ws_col > 0 ? ws.ws_col : 80;
        _flux_rows = ws.ws_row > 0 ? ws.ws_row : 24;
    }
}

/* -- Signal handlers -- */

static void _flux_sigwinch_handler(int sig) {
    (void)sig;
    _flux_winch = 1;
}

static void _flux_sigint_handler(int sig) {
    (void)sig;
    FluxMsg m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_QUIT;
    ssize_t w = write(_flux_pipe_w, &m, sizeof(m));
    (void)w;
}

/* -- Terminal setup / teardown -- */

static void _flux_setup_term(int alt_screen, int mouse, int focus_events) {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &_flux_orig_termios);

    raw = _flux_orig_termios;
    raw.c_lflag &= ~(unsigned)(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(unsigned)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |=  (unsigned)CS8;
    raw.c_oflag &= ~(unsigned)OPOST;
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    _flux_update_winsize();

    if (alt_screen) {
        fputs("\x1b[?1049h", stdout);
        /* Focus-event reporting (xterm CSI ? 1004): only meaningful
         * inside the alt-screen — enabling it in cooked-mode shells
         * leaks \x1b[I/\x1b[O bytes into the user's prompt on resume.
         * Gated on alt_screen for that reason. */
        if (focus_events) {
            fputs("\x1b[?1004h", stdout);
        }
    }

    /* Paint the whole alt-screen with the window bg tone so the first
     * frame draws over a uniform dark surface — no terminal-default
     * seam during initial render. The bg sticks through the [2J clear,
     * then we reset attrs and home the cursor. */
    fputs(FLUX_THEME_WINDOW_BG "\x1b[2J\x1b[0m\x1b[?25l\x1b[H", stdout);

    fputs("\x1b[?2004h", stdout);

    if (mouse) {
        /* 1002 = button-event tracking: clicks, releases, drags, wheel — but
         * NOT bare hover-motion. (1003 = any-event sends motion for every
         * pixel which floods the event stream and can split mouse sequences
         * across read() calls.) 1006 = SGR encoding. */
        fputs("\x1b[?1002h\x1b[?1006h", stdout);
    }

    fflush(stdout);

    signal(SIGWINCH, _flux_sigwinch_handler);
    signal(SIGINT,   _flux_sigint_handler);
    signal(SIGTERM,  _flux_sigint_handler);
}

static void _flux_restore_term(int alt_screen, int mouse, int focus_events) {
    if (mouse) {
        fputs("\x1b[?1002l\x1b[?1006l", stdout);
    }

    /* Disable focus events BEFORE leaving the alt-screen so the
     * \x1b[?1004l lands while we still own the alt buffer; otherwise
     * it bleeds into the user's normal shell. */
    if (alt_screen && focus_events) {
        fputs("\x1b[?1004l", stdout);
    }

    if (alt_screen) {
        fputs("\x1b[?1049l", stdout);
    }

    fputs("\x1b[?2004l", stdout);

    fputs("\x1b[?25h\x1b[0m", stdout);
    fflush(stdout);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &_flux_orig_termios);
}

/* -- Key parser -- */

static int _flux_parse_key(const char *buf, int len, FluxMsg *out) {
    const unsigned char *u = (const unsigned char *)buf;
    unsigned char b;

    memset(out, 0, sizeof(*out));
    out->type = MSG_KEY;

    if (len <= 0) {
        out->type = MSG_NONE;
        return 0;
    }

    b = u[0];

    if (b >= 1 && b <= 26 && b != '\r' && b != '\t' && b != '\n') {
        out->u.key.ctrl = 1;
        snprintf(out->u.key.key, sizeof(out->u.key.key),
                 "ctrl+%c", 'a' + b - 1);
        return 1;
    }

    if (b == '\r' || b == '\n') {
        snprintf(out->u.key.key, sizeof(out->u.key.key), "enter");
        return 1;
    }
    if (b == '\t') {
        snprintf(out->u.key.key, sizeof(out->u.key.key), "tab");
        return 1;
    }
    if (b == 127) {
        snprintf(out->u.key.key, sizeof(out->u.key.key), "backspace");
        return 1;
    }

    if (b == 27) {
        if (len == 1) {
            /* Bare ESC — treat as a real Esc keypress. There's a tiny
             * race where a CSI sequence could split across read()s and
             * the leading ESC lands here alone; in practice reads are
             * chunky enough that SGR/CSI arrive together. Emitting
             * "esc" here is critical for modal dismiss UX. */
            snprintf(out->u.key.key, sizeof(out->u.key.key), "esc");
            return 1;
        }

        if (len == 2 && u[1] >= 32 && u[1] != '[' && u[1] != ']' && u[1] != 'O') {
            out->u.key.alt = 1;
            snprintf(out->u.key.key, sizeof(out->u.key.key),
                     "alt+%c", (char)u[1]);
            return 2;
        }
        /* ESC + '[' / ']' / 'O' with len==2: incomplete CSI/OSC/SS3 —
         * drop the partial bytes silently rather than misparse. */
        if (len == 2 && (u[1] == '[' || u[1] == ']' || u[1] == 'O')) {
            out->type = MSG_NONE;
            return 2;
        }

        if (len >= 3 && u[1] == '[') {
            switch (u[2]) {
            case 'A':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "up");
                return 3;
            case 'B':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "down");
                return 3;
            case 'C':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "right");
                return 3;
            case 'D':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "left");
                return 3;
            case 'H':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "home");
                return 3;
            case 'F':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "end");
                return 3;
            case 'Z':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "shift+tab");
                return 3;
            case 'I':
                /* xterm focus-event reporting (CSI ? 1004): focus IN.
                 * Only ever arrives if focus_events was enabled at
                 * setup. Update the global flag, request a force
                 * redraw so widgets that read flux_tty_focused()
                 * repaint, and swallow the bytes (no app-visible
                 * key event — apps that need to react can hook
                 * their own MSG_TICK / poll flux_tty_focused()). */
                _flux_tty_focused = 1;
                _flux_force = 1;
                out->type = MSG_NONE;
                return 3;
            case 'O':
                /* CSI ? 1004 focus OUT — mirror of 'I' above. */
                _flux_tty_focused = 0;
                _flux_force = 1;
                out->type = MSG_NONE;
                return 3;
            default:
                break;
            }

            if (len >= 4 && u[3] == '~') {
                switch (u[2]) {
                case '2':
                    snprintf(out->u.key.key, sizeof(out->u.key.key), "insert");
                    return 4;
                case '3':
                    snprintf(out->u.key.key, sizeof(out->u.key.key), "delete");
                    return 4;
                case '5':
                    snprintf(out->u.key.key, sizeof(out->u.key.key), "pgup");
                    return 4;
                case '6':
                    snprintf(out->u.key.key, sizeof(out->u.key.key), "pgdown");
                    return 4;
                default:
                    break;
                }
            }

            if (len >= 5 && u[4] == '~') {
                int code = (u[2] - '0') * 10 + (u[3] - '0');
                const char *fname = NULL;
                switch (code) {
                case 15: fname = "f5";  break;
                case 17: fname = "f6";  break;
                case 18: fname = "f7";  break;
                case 19: fname = "f8";  break;
                case 20: fname = "f9";  break;
                case 21: fname = "f10"; break;
                case 23: fname = "f11"; break;
                case 24: fname = "f12"; break;
                default: break;
                }
                if (fname) {
                    snprintf(out->u.key.key, sizeof(out->u.key.key),
                             "%s", fname);
                    return 5;
                }
            }

            if (u[2] == '<') {
                /* SGR mouse protocol: ESC[<btn;col;rowM (press) / m (release) */
                int btn = 0, col = 0, row = 0;
                int mi = 3;
                int consumed;
                char terminator = 0;

                while (mi < len && u[mi] >= '0' && u[mi] <= '9') {
                    btn = btn * 10 + (u[mi] - '0'); mi++;
                }
                if (mi < len && u[mi] == ';') mi++;
                while (mi < len && u[mi] >= '0' && u[mi] <= '9') {
                    col = col * 10 + (u[mi] - '0'); mi++;
                }
                if (mi < len && u[mi] == ';') mi++;
                while (mi < len && u[mi] >= '0' && u[mi] <= '9') {
                    row = row * 10 + (u[mi] - '0'); mi++;
                }
                if (mi < len && (u[mi] == 'M' || u[mi] == 'm')) {
                    terminator = (char)u[mi];
                    mi++;
                }
                consumed = mi;

                /* SAFETY: if we did not find an M/m terminator, this is a
                 * partial / malformed mouse sequence (e.g. truncated read).
                 * Drop the bytes we walked but emit NO event — never invent
                 * a phantom press from incomplete data. */
                if (terminator == 0) {
                    out->type = MSG_NONE;
                    return consumed > 0 ? consumed : 1;
                }

                /* Decode button + modifier bits */
                int base   = btn & 3;       /* 0=left,1=middle,2=right (3=release-with-1000-mode) */
                int is_motion = (btn & 32) != 0;
                int is_wheel  = (btn & 64) != 0;
                int shift  = (btn & 4)  ? 1 : 0;
                int alt    = (btn & 8)  ? 1 : 0;
                int ctrl   = (btn & 16) ? 1 : 0;

                if (is_wheel) {
                    out->type = MSG_MOUSE;
                    out->u.mouse.event = (btn & 1) ? FLUX_MOUSE_WHEEL_DOWN
                                                   : FLUX_MOUSE_WHEEL_UP;
                    out->u.mouse.x = col;
                    out->u.mouse.y = row;
                    out->u.mouse.button = -1;
                    out->u.mouse.shift = shift;
                    out->u.mouse.alt   = alt;
                    out->u.mouse.ctrl  = ctrl;
                    return consumed;
                }

                out->type = MSG_MOUSE;
                if (is_motion) {
                    out->u.mouse.event = FLUX_MOUSE_MOVE;
                    out->u.mouse.button = base;
                } else if (terminator == 'm') {
                    out->u.mouse.event = FLUX_MOUSE_RELEASE;
                    out->u.mouse.button = base;
                } else {
                    /* terminator == 'M' AND not motion AND not wheel → real press */
                    out->u.mouse.event = FLUX_MOUSE_PRESS;
                    out->u.mouse.button = base;
                }
                out->u.mouse.x = col;
                out->u.mouse.y = row;
                out->u.mouse.shift = shift;
                out->u.mouse.alt   = alt;
                out->u.mouse.ctrl  = ctrl;
                return consumed;
            }

            {
                int ci;
                for (ci = 2; ci < len; ci++) {
                    if (u[ci] >= 0x40 && u[ci] <= 0x7E) {
                        return ci + 1;
                    }
                }
            }
            out->type = MSG_NONE;
            return len;
        }

        if (len >= 3 && u[1] == 'O') {
            switch (u[2]) {
            case 'P':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "f1");
                return 3;
            case 'Q':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "f2");
                return 3;
            case 'R':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "f3");
                return 3;
            case 'S':
                snprintf(out->u.key.key, sizeof(out->u.key.key), "f4");
                return 3;
            default:
                break;
            }
        }

        out->type = MSG_NONE;
        return len;
    }

    if (b < 0x80) {
        out->u.key.key[0] = (char)b;
        out->u.key.key[1] = '\0';
        out->u.key.rune = (int)b;
        return 1;
    }

    {
        int seqlen;
        if (b < 0xC0) {
            seqlen = 1;
        } else if (b < 0xE0) {
            seqlen = 2;
        } else if (b < 0xF0) {
            seqlen = 3;
        } else {
            seqlen = 4;
        }
        if (seqlen > len) {
            seqlen = len;
        }
        if (seqlen > (int)(sizeof(out->u.key.key)) - 1) {
            seqlen = (int)(sizeof(out->u.key.key)) - 1;
        }
        memcpy(out->u.key.key, buf, (size_t)seqlen);
        out->u.key.key[seqlen] = '\0';
        out->u.key.rune = (int)b;
        return seqlen;
    }
}

/* -- Key matching -- */

int flux_key_is(FluxMsg msg, const char *name) {
    return msg.type == MSG_KEY && strcmp(msg.u.key.key, name) == 0;
}

/* -- Built-in commands -- */

static FluxMsg _flux_quit_fn(void *ctx) {
    FluxMsg m;
    (void)ctx;
    memset(&m, 0, sizeof(m));
    m.type = MSG_QUIT;
    return m;
}

FluxCmd flux_cmd_quit(void) {
    FluxCmd c;
    c.fn = _flux_quit_fn;
    c.ctx = NULL;
    return c;
}

typedef struct {
    int ms;
} _FluxTickCtx;

static FluxMsg _flux_tick_fn(void *ctx) {
    _FluxTickCtx *t = (_FluxTickCtx *)ctx;
    struct timespec ts;
    FluxMsg m;

    ts.tv_sec  = t->ms / 1000;
    ts.tv_nsec = (t->ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);

    free(ctx);
    memset(&m, 0, sizeof(m));
    m.type = MSG_TICK;
    return m;
}

FluxCmd flux_tick(int ms) {
    _FluxTickCtx *c = (_FluxTickCtx *)malloc(sizeof(*c));
    FluxCmd cmd;
    if (!c) {
        return FLUX_CMD_NONE;
    }
    c->ms = ms;
    cmd.fn  = _flux_tick_fn;
    cmd.ctx = c;
    return cmd;
}

typedef struct {
    int   id;
    void *data;
} _FluxCustomCtx;

static FluxMsg _flux_custom_fn(void *ctx) {
    _FluxCustomCtx *c = (_FluxCustomCtx *)ctx;
    FluxMsg m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_CUSTOM;
    m.u.custom.id   = c->id;
    m.u.custom.data = c->data;
    free(ctx);
    return m;
}

FluxCmd flux_cmd_custom(int id, void *data) {
    _FluxCustomCtx *c = (_FluxCustomCtx *)malloc(sizeof(*c));
    FluxCmd cmd;
    if (!c) {
        return FLUX_CMD_NONE;
    }
    c->id   = id;
    c->data = data;
    cmd.fn  = _flux_custom_fn;
    cmd.ctx = c;
    return cmd;
}

/* Thread-safe MSG_CUSTOM injection. POSIX pipe writes < PIPE_BUF (4096)
 * are atomic, so this is safe to call concurrently from any thread. */
int flux_post_custom(int id, void *data) {
    FluxMsg m;
    ssize_t w;
    if (_flux_pipe_w < 0) return -1;
    memset(&m, 0, sizeof m);
    m.type        = MSG_CUSTOM;
    m.u.custom.id = id;
    m.u.custom.data = data;
    w = write(_flux_pipe_w, &m, sizeof(m));
    return (w == (ssize_t)sizeof(m)) ? 0 : -1;
}

/* -- Command dispatch (threaded) -- */

typedef struct {
    FluxCmd cmd;
    int     pipe_w;
} _FluxCmdThread;

static void *_flux_cmd_runner(void *arg) {
    _FluxCmdThread *t = (_FluxCmdThread *)arg;
    FluxMsg r = t->cmd.fn(t->cmd.ctx);
    ssize_t w = write(t->pipe_w, &r, sizeof(r));
    (void)w;
    free(t);
    return NULL;
}

static void _flux_dispatch_cmd(FluxCmd cmd, int pipe_w) {
    _FluxCmdThread *t;
    pthread_t tid;
    pthread_attr_t a;

    if (!cmd.fn) {
        return;
    }

    t = (_FluxCmdThread *)malloc(sizeof(*t));
    if (!t) {
        return;
    }
    t->cmd    = cmd;
    t->pipe_w = pipe_w;

    pthread_attr_init(&a);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &a, _flux_cmd_runner, t);
    pthread_attr_destroy(&a);
}

/* -- Line-level diff renderer (backward-compatible path) -- */

static char _flux_prev_frame[FLUX_RENDER_BUF];
static char _flux_cur_frame[FLUX_RENDER_BUF];

static int _flux_split_lines(char *s, char **lines, int maxlines) {
    int n = 0;
    char *p = s;
    char *nl;

    while (*p && n < maxlines) {
        lines[n++] = p;
        nl = strchr(p, '\n');
        if (!nl) {
            break;
        }
        *nl = '\0';
        p = nl + 1;
    }
    return n;
}

static void _flux_render_linediff(FluxModel *model, int force) {
    static char cur_copy[FLUX_RENDER_BUF];
    static char prev_copy[FLUX_RENDER_BUF];
    static char *clines[FLUX_MAX_LINES];
    static char *plines[FLUX_MAX_LINES];
    static char obuf[FLUX_RENDER_BUF * 2];
    int olen = 0;
    int nc, np;
    int rows_now;
    int i, maxlines;

    memset(_flux_cur_frame, 0, FLUX_RENDER_BUF);
    model->view(model, _flux_cur_frame, FLUX_RENDER_BUF - 1);

    if (!force && strcmp(_flux_prev_frame, _flux_cur_frame) == 0) {
        return;
    }

    memcpy(cur_copy, _flux_cur_frame, FLUX_RENDER_BUF);
    nc = _flux_split_lines(cur_copy, clines, FLUX_MAX_LINES);

    memcpy(prev_copy, _flux_prev_frame, FLUX_RENDER_BUF);
    np = _flux_split_lines(prev_copy, plines, FLUX_MAX_LINES);

    rows_now = (int)_flux_rows;
    maxlines = nc < rows_now ? nc : rows_now;

    /* Local macros for line-diff output buffering */
    #define LD_OFLUSH() do { \
        if (olen > 0) { \
            fwrite(obuf, 1, (size_t)olen, stdout); \
            olen = 0; \
        } \
    } while (0)

    #define LD_OCAT(s) do { \
        int _sl = (int)strlen(s); \
        if (olen + _sl >= (int)sizeof(obuf)) { LD_OFLUSH(); } \
        memcpy(obuf + olen, s, (size_t)_sl); \
        olen += _sl; \
    } while (0)

    #define LD_OCATF(...) do { \
        char _tmp[64]; \
        snprintf(_tmp, sizeof(_tmp), __VA_ARGS__); \
        LD_OCAT(_tmp); \
    } while (0)

    LD_OCAT("\x1b[?25l");

    for (i = 0; i < maxlines; i++) {
        if (!force && i < np && strcmp(clines[i], plines[i]) == 0) {
            continue;
        }

        LD_OCATF("\x1b[%d;1H", i + 1);

        if (i + 1 == rows_now) {
            LD_OCAT("\x1b[?7l");
            LD_OCAT(clines[i]);
            LD_OCAT("\x1b[0m");
            LD_OCAT("\x1b[?7h");
        } else {
            LD_OCAT(clines[i]);
            LD_OCAT("\x1b[0m\x1b[K");
        }
    }

    for (i = maxlines; i < np && i < rows_now; i++) {
        LD_OCATF("\x1b[%d;1H", i + 1);
        if (i + 1 == rows_now) {
            LD_OCAT("\x1b[?7l");
            LD_OCAT("\x1b[0m");
            LD_OCAT("\x1b[?7h");
        } else {
            LD_OCAT("\x1b[0m\x1b[K");
        }
    }

    LD_OCAT("\x1b[1;1H");

    LD_OFLUSH();
    fflush(stdout);

    #undef LD_OFLUSH
    #undef LD_OCAT
    #undef LD_OCATF

    memcpy(_flux_prev_frame, _flux_cur_frame, FLUX_RENDER_BUF);
}

/* -- Cell-level diff renderer (FluxModel.view_tree path) ------------
 *
 * When a model provides view_tree, flux_run uses this renderer instead
 * of the line-diff path above. Pipeline per frame:
 *
 *   1. Reset the node pool (cheap — just zeros the count + ids).
 *   2. Call view_tree(model, &pool, cols, rows) → root FluxNode*.
 *   3. flux_layout_compute(root, cols, rows) — depth-first layout.
 *   4. flux_render_tree(back, front, &styles, root) — fill the back
 *      cell buffer; front carries last frame's cells for skip-cell
 *      optimisations inside flux_render_node.
 *   5. flux_diff_screens(front, back, &styles, obuf, ...) — compute
 *      cursor-move + style + write deltas into obuf.
 *   6. fwrite(obuf) → stdout. Swap front and back so next frame's
 *      "front" is the previous "back".
 *
 * Renderer state is owned here as static data sized for the lifetime
 * of one flux_run call. The screens are resized lazily when the
 * terminal grows. */

static FluxRenderer    _flux_tree_r;
static int             _flux_tree_inited = 0;

static void _flux_render_tree_diff(FluxModel *model, int force) {
    if (!model || !model->view_tree) return;

    int cols = (int)_flux_cols;
    int rows = (int)_flux_rows;
    if (cols <= 0 || rows <= 0) return;

    /* First-time init: pool, style pool, screens. */
    if (!_flux_tree_inited) {
        flux_pool_init(&_flux_tree_r.pool);
        flux_style_pool_init(&_flux_tree_r.styles);
        if (flux_screen_init(&_flux_tree_r.front, rows, cols) != 0) return;
        if (flux_screen_init(&_flux_tree_r.back,  rows, cols) != 0) {
            flux_screen_free(&_flux_tree_r.front);
            return;
        }
        _flux_tree_inited = 1;
        force = 1;  /* nothing on screen yet — emit a full frame */
    }

    /* Resize if the terminal changed since last frame. */
    if (_flux_tree_r.front.rows != rows || _flux_tree_r.front.cols != cols) {
        flux_screen_resize(&_flux_tree_r.front, rows, cols);
        flux_screen_resize(&_flux_tree_r.back,  rows, cols);
        force = 1;
    }

    /* Reset the pool — every frame builds a fresh tree. We don't
     * try to preserve nodes across frames in this minimal wiring;
     * dirty marks are still useful for flux_render_node's skip
     * paths but the tree topology is rebuilt each call. */
    flux_pool_init(&_flux_tree_r.pool);

    FluxNode *root = model->view_tree(model, &_flux_tree_r.pool,
                                      cols, rows);
    if (!root) {
        /* Empty render — clear the back buffer and diff-emit. */
        flux_screen_clear(&_flux_tree_r.back);
    } else {
        flux_layout_compute(root, cols, rows);
        flux_render_tree(&_flux_tree_r.back, &_flux_tree_r.front,
                         &_flux_tree_r.styles, root);
    }

    /* Skip emit if nothing changed and no force pending. The diff
     * has-changes check is cheap (cell-by-cell pointer compare) and
     * lets idle frames cost effectively zero stdout traffic. */
    if (!force && !flux_diff_has_changes(&_flux_tree_r.front,
                                          &_flux_tree_r.back)) {
        /* Swap buffers so next frame's "front" is this frame's
         * "back" — keeps the front-as-prev invariant. */
        FluxScreen tmp = _flux_tree_r.front;
        _flux_tree_r.front = _flux_tree_r.back;
        _flux_tree_r.back  = tmp;
        return;
    }

    /* Cursor hide while we paint. */
    fwrite("\x1b[?25l", 1, 6, stdout);

    int n;
    if (force) {
        n = flux_diff_full(&_flux_tree_r.back, &_flux_tree_r.styles,
                           _flux_tree_r.obuf, FLUX_PATCH_BUF);
    } else {
        n = flux_diff_screens(&_flux_tree_r.front, &_flux_tree_r.back,
                              &_flux_tree_r.styles,
                              _flux_tree_r.obuf, FLUX_PATCH_BUF);
    }
    if (n > 0) {
        fwrite(_flux_tree_r.obuf, 1, (size_t)n, stdout);
    }
    /* Park cursor at home for now; widgets can re-position via the
     * patch stream above when they need a visible caret. */
    fwrite("\x1b[1;1H", 1, 6, stdout);
    fflush(stdout);

    /* Swap front/back: next frame's "front" is this frame's "back". */
    FluxScreen tmp = _flux_tree_r.front;
    _flux_tree_r.front = _flux_tree_r.back;
    _flux_tree_r.back  = tmp;
}

static void _flux_render_tree_shutdown(void) {
    if (!_flux_tree_inited) return;
    flux_screen_free(&_flux_tree_r.front);
    flux_screen_free(&_flux_tree_r.back);
    _flux_tree_inited = 0;
}

/* -- Main event loop -- */

void flux_run(FluxProgram *p) {
    int pipefd[2];
    int fl;
    unsigned char ibuf[64];
    struct timeval tv;
    long frame_us;
    FluxCmd ic;

    if (p->fps <= 0) {
        p->fps = 30;
    }
    if (!p->alt_screen && !p->mouse && !p->fps) {
        p->alt_screen = 1;
    }

    if (pipe(pipefd) != 0) {
        perror("pipe");
        return;
    }
    _flux_pipe_r = pipefd[0];
    _flux_pipe_w = pipefd[1];

    fl = fcntl(_flux_pipe_r, F_GETFL, 0);
    fcntl(_flux_pipe_r, F_SETFL, fl | O_NONBLOCK);

    _flux_setup_term(p->alt_screen, p->mouse, p->focus_events);

    _flux_running = 1;
    memset(_flux_prev_frame, 0, FLUX_RENDER_BUF);

    ic = p->model.init(&p->model);
    if (p->model.view_tree) {
        _flux_render_tree_diff(&p->model, 1);
    } else {
        _flux_render_linediff(&p->model, 1);
    }
    _flux_dispatch_cmd(ic, _flux_pipe_w);

    frame_us = 1000000L / p->fps;

    while (_flux_running) {
        int need_force = 0;
        int ret, mfd;
        fd_set fds;

        if (_flux_winch) {
            FluxMsg wm;
            FluxCmd c;

            _flux_winch = 0;
            _flux_update_winsize();

            memset(&wm, 0, sizeof(wm));
            wm.type = MSG_WINSIZE;
            wm.u.winsize.cols = (int)_flux_cols;
            wm.u.winsize.rows = (int)_flux_rows;

            c = p->model.update(&p->model, wm);
            _flux_dispatch_cmd(c, _flux_pipe_w);
            need_force = 1;
        }

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(_flux_pipe_r, &fds);
        mfd = _flux_pipe_r > STDIN_FILENO ? _flux_pipe_r : STDIN_FILENO;

        tv.tv_sec  = 0;
        tv.tv_usec = (long)frame_us;

        ret = select(mfd + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            int n = (int)read(STDIN_FILENO, ibuf, sizeof(ibuf));
            if (n > 0) {
                if (n >= 6 && ibuf[0] == '\x1b' && ibuf[1] == '['
                    && ibuf[2] == '2' && ibuf[3] == '0'
                    && ibuf[4] == '0' && ibuf[5] == '~') {
                    FluxMsg pm;
                    FluxCmd c;
                    int pi = 0;
                    int rest;

                    memset(&pm, 0, sizeof(pm));
                    pm.type = MSG_PASTE;

                    rest = n - 6;
                    if (rest > 0 && rest < FLUX_PASTE_MAX) {
                        memcpy(pm.u.paste.text, ibuf + 6, (size_t)rest);
                        pi = rest;
                    }

                    while (pi < FLUX_PASTE_MAX - 1) {
                        char *endm;
                        int nr;

                        if (pi >= 6) {
                            endm = (char *)memmem(pm.u.paste.text,
                                                   (size_t)pi,
                                                   "\x1b[201~", 6);
                            if (endm) {
                                pi = (int)(endm - pm.u.paste.text);
                                break;
                            }
                        }

                        nr = (int)read(STDIN_FILENO,
                                       pm.u.paste.text + pi,
                                       (size_t)(FLUX_PASTE_MAX - 1 - pi));
                        if (nr <= 0) {
                            break;
                        }
                        pi += nr;
                    }

                    pm.u.paste.text[pi] = '\0';
                    pm.u.paste.len = pi;

                    while (pm.u.paste.len > 0
                           && pm.u.paste.text[pm.u.paste.len - 1] == '\n') {
                        pm.u.paste.len--;
                        pm.u.paste.text[pm.u.paste.len] = '\0';
                    }

                    if (pm.u.paste.len > 0) {
                        c = p->model.update(&p->model, pm);
                        if (c.fn == _flux_quit_fn) {
                            goto done;
                        }
                        _flux_dispatch_cmd(c, _flux_pipe_w);
                    }
                } else {
                    int off = 0;
                    while (off < n) {
                        FluxMsg km;
                        FluxCmd c;
                        int consumed;

                        consumed = _flux_parse_key(
                            (const char *)(ibuf + off),
                            n - off,
                            &km);

                        if (km.type == MSG_QUIT) {
                            goto done;
                        }
                        if (km.type == MSG_NONE) {
                            if (consumed <= 0) {
                                break;
                            }
                            off += consumed;
                            continue;
                        }

                        c = p->model.update(&p->model, km);
                        if (c.fn == _flux_quit_fn) {
                            goto done;
                        }
                        _flux_dispatch_cmd(c, _flux_pipe_w);
                        off += consumed;
                    }
                }
            }
        }

        if (ret > 0 && FD_ISSET(_flux_pipe_r, &fds)) {
            FluxMsg pm;
            while (read(_flux_pipe_r, &pm, sizeof(pm))
                   == (ssize_t)sizeof(pm)) {
                FluxCmd c;

                if (pm.type == MSG_QUIT) {
                    goto done;
                }

                c = p->model.update(&p->model, pm);
                if (c.fn == _flux_quit_fn) {
                    goto done;
                }
                _flux_dispatch_cmd(c, _flux_pipe_w);
            }
        }

        if (_flux_force) { need_force = 1; _flux_force = 0; }
        if (p->model.view_tree) {
            _flux_render_tree_diff(&p->model, need_force);
        } else {
            _flux_render_linediff(&p->model, need_force);
        }
    }

done:
    _flux_running = 0;
    _flux_render_tree_shutdown();
    _flux_restore_term(p->alt_screen, p->mouse, p->focus_events);
    if (p->model.free) {
        p->model.free(&p->model);
    }
    close(_flux_pipe_r);
    close(_flux_pipe_w);
    _flux_pipe_r = -1;
    _flux_pipe_w = -1;
}

/* ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════
 * AGENT UI WIDGETS — implementations
 * ══════════════════════════════════════════════════════════════════
 * ══════════════════════════════════════════════════════════════════ */


/* ── Window chrome ──────────────────────────────────────────── */

static void _flux_chrome_ap(char **o, int *rem, const char *s) {
    if (!s || !*s || *rem <= 0) return;
    int n = (int)strlen(s);
    if (n > *rem) n = *rem;
    memcpy(*o, s, (size_t)n);
    *o   += n;
    *rem -= n;
}

void flux_window_chrome(char *out_buf, int out_sz,
                        const char *content, const char *title,
                        int inner_w, const FluxChromeOpts *opts) {
    FluxChromeOpts d;
    d.border_fg    = FLUX_THEME_BORDER_FG;
    d.bar_bg       = FLUX_THEME_TITLEBAR_BG;
    d.panel_bg     = FLUX_THEME_PANEL_BG;
    d.title_fg     = FLUX_THEME_TEXT_DIM_FG;
    d.dot_close    = FLUX_THEME_TRAFFIC_R_FG;
    d.dot_min      = FLUX_THEME_TRAFFIC_Y_FG;
    d.dot_max      = FLUX_THEME_TRAFFIC_G_FG;
    d.show_dots    = 1;
    d.show_divider = 1;
    if (!opts) opts = &d;

    if (!out_buf || out_sz <= 0) return;
    if (inner_w < 0) inner_w = 0;

    const FluxBorder *B = &FLUX_BORDER_ROUNDED;
    const char *R       = FLUX_RESET;
    char *o   = out_buf;
    int   rem = out_sz - 1;

    int i;

    /* row 0: top border ─────────────────────────────────────── */
    _flux_chrome_ap(&o, &rem, opts->border_fg);
    _flux_chrome_ap(&o, &rem, B->tl);
    for (i = 0; i < inner_w; i++)
        _flux_chrome_ap(&o, &rem, B->h);
    _flux_chrome_ap(&o, &rem, B->tr);
    _flux_chrome_ap(&o, &rem, R);
    _flux_chrome_ap(&o, &rem, "\n");

    /* row 1: title bar ─────────────────────────────────────── */
    /* layout inside the two side borders (inner_w + 2 cells of bg):
     *   [bar_bg fills the whole strip]
     *   " ● ● ● " <filler spaces> "TITLE "
     *   = 1 + (3 dots * 2 - 1) + 1  = 7 cells of dots block (with trailing space)
     *   Actually: " " + dot + " " + dot + " " + dot + " " = 7 cells.
     *   Then: filler + title + " " = inner_w + 2 - 7 cells.
     */
    _flux_chrome_ap(&o, &rem, opts->border_fg);
    _flux_chrome_ap(&o, &rem, B->v);
    _flux_chrome_ap(&o, &rem, R);

    /* paint bar_bg across the full interior */
    _flux_chrome_ap(&o, &rem, opts->bar_bg);

    int dots_w = 0;
    if (opts->show_dots) {
        /* " ● ● ● " — each dot is one cell wide, separated by single
         * space, with one space pad on each side. Total = 7 cells. */
        _flux_chrome_ap(&o, &rem, " ");
        _flux_chrome_ap(&o, &rem, opts->dot_close);
        _flux_chrome_ap(&o, &rem, "\xe2\x97\x8f");      /* ● */
        _flux_chrome_ap(&o, &rem, " ");
        _flux_chrome_ap(&o, &rem, opts->dot_min);
        _flux_chrome_ap(&o, &rem, "\xe2\x97\x8f");
        _flux_chrome_ap(&o, &rem, " ");
        _flux_chrome_ap(&o, &rem, opts->dot_max);
        _flux_chrome_ap(&o, &rem, "\xe2\x97\x8f");
        _flux_chrome_ap(&o, &rem, " ");
        dots_w = 7;
    }

    /* Right-align title; truncate if it doesn't fit the remaining space.
     * Available cells for title text = inner_w - dots_w - 1 (right pad). */
    int interior_w = inner_w;
    int title_avail = interior_w - dots_w - 1;
    if (title_avail < 0) title_avail = 0;
    char title_fit[256];
    int title_w = 0;
    if (title && title_avail > 0) {
        title_w = flux_truncate(title, title_avail, "\xe2\x80\xa6",
                                title_fit, (int)sizeof title_fit);
    } else {
        title_fit[0] = '\0';
    }
    int title_block = title_w > 0 ? (title_w + 1) : 0;
    int filler = interior_w - dots_w - title_block;
    if (filler < 0) filler = 0;

    /* re-establish bar bg before padding (we may have emitted fg for dots) */
    _flux_chrome_ap(&o, &rem, opts->bar_bg);
    for (i = 0; i < filler; i++)
        _flux_chrome_ap(&o, &rem, " ");

    if (title_w > 0) {
        _flux_chrome_ap(&o, &rem, opts->bar_bg);
        _flux_chrome_ap(&o, &rem, opts->title_fg);
        _flux_chrome_ap(&o, &rem, FLUX_BOLD);
        _flux_chrome_ap(&o, &rem, title_fit);
        _flux_chrome_ap(&o, &rem, R);
        _flux_chrome_ap(&o, &rem, opts->bar_bg);
        _flux_chrome_ap(&o, &rem, " ");
    }

    _flux_chrome_ap(&o, &rem, R);
    _flux_chrome_ap(&o, &rem, opts->border_fg);
    _flux_chrome_ap(&o, &rem, B->v);
    _flux_chrome_ap(&o, &rem, R);
    _flux_chrome_ap(&o, &rem, "\n");

    /* row 2: divider ─────────────────────────────────────── */
    if (opts->show_divider) {
        _flux_chrome_ap(&o, &rem, opts->border_fg);
        _flux_chrome_ap(&o, &rem, B->ml);
        for (i = 0; i < inner_w; i++)
            _flux_chrome_ap(&o, &rem, B->h);
        _flux_chrome_ap(&o, &rem, B->mr);
        _flux_chrome_ap(&o, &rem, R);
        _flux_chrome_ap(&o, &rem, "\n");
    }

    /* Content rows: │ <line> │
     *
     * Each input line is run through a scratch FluxSB + flux_fit so the
     * emitted text is GUARANTEED to be exactly inner_w display cells —
     * truncated with '…' if too wide, padded with spaces if too narrow.
     * Caller can pass any text and the chrome cannot break. */
    if (content && *content) {
        const char *p = content;
        while (*p) {
            const char *eol = strchr(p, '\n');
            int line_len = eol ? (int)(eol - p) : (int)strlen(p);
            char line_buf[4096];
            char fit_buf[4096];
            int copy_len = line_len < (int)sizeof line_buf - 1
                         ? line_len : (int)sizeof line_buf - 1;
            if (copy_len > 0) memcpy(line_buf, p, (size_t)copy_len);
            line_buf[copy_len] = '\0';

            FluxSB row;
            flux_sb_init(&row, fit_buf, (int)sizeof fit_buf);
            flux_fit(&row, line_buf, inner_w, NULL, FLUX_ALIGN_LEFT);

            _flux_chrome_ap(&o, &rem, opts->border_fg);
            _flux_chrome_ap(&o, &rem, B->v);
            _flux_chrome_ap(&o, &rem, R);
            _flux_chrome_ap(&o, &rem, flux_sb_str(&row));
            _flux_chrome_ap(&o, &rem, R);
            _flux_chrome_ap(&o, &rem, opts->border_fg);
            _flux_chrome_ap(&o, &rem, B->v);
            _flux_chrome_ap(&o, &rem, R);
            _flux_chrome_ap(&o, &rem, "\n");

            if (!eol) break;
            p = eol + 1;
        }
    }

    /* bottom border ─────────────────────────────────────── */
    _flux_chrome_ap(&o, &rem, opts->border_fg);
    _flux_chrome_ap(&o, &rem, B->bl);
    for (i = 0; i < inner_w; i++)
        _flux_chrome_ap(&o, &rem, B->h);
    _flux_chrome_ap(&o, &rem, B->br);
    _flux_chrome_ap(&o, &rem, R);
    _flux_chrome_ap(&o, &rem, "\n");

    *o = '\0';
}


int flux_activity_format_duration(int ms, char *out, int cap) {
    int n;
    if (!out || cap <= 0 || ms <= 0) {
        if (out && cap > 0) out[0] = '\0';
        return 0;
    }
    if (ms < 1000) {
        n = snprintf(out, (size_t)cap, "%dms", ms);
    } else if (ms < 60000) {
        double s = (double)ms / 1000.0;
        n = snprintf(out, (size_t)cap, "%.1fs", s);
    } else if (ms < 3600000) {
        int m = ms / 60000;
        int s = (ms % 60000) / 1000;
        n = snprintf(out, (size_t)cap, "%dm %02ds", m, s);
    } else {
        int h = ms / 3600000;
        int m = (ms % 3600000) / 60000;
        n = snprintf(out, (size_t)cap, "%dh%02dm", h, m);
    }
    if (n < 0) {
        out[0] = '\0';
        return 0;
    }
    if (n >= cap) {
        out[cap - 1] = '\0';
        return cap - 1;
    }
    return n;
}

/* (Deprecated activity-only truncator removed — flux_truncate handles it now.) */

void flux_activity_render(FluxSB *sb, const FluxActivity *a,
                          int width, int indent) {
    const char *glyph;
    const char *glyph_clr;
    const char *label_clr;
    char dur_buf[32];
    int  dur_w;
    int  inner_w;
    int  glyph_w = 1;
    int  gap_w   = 1;
    int  label_w_full;
    int  label_max;
    char label_trunc[256];
    int  label_w;
    const char *label_to_emit;
    int  pad_w;
    int  i;

    if (!sb || !a) return;
    if (indent < 0) indent = 0;
    if (width  < indent + glyph_w + gap_w + 1) {
        /* nothing sensible to draw; emit a blank line of width */
        for (i = 0; i < width; i++) flux_sb_append(sb, " ");
        flux_sb_append(sb, "\n");
        return;
    }

    /* Pick glyph + colors per status. */
    switch (a->status) {
        case FLUX_ACT_DONE:
            glyph     = "\xe2\x9c\x93";        /* ✓ */
            glyph_clr = FLUX_THEME_OK_FG;
            label_clr = FLUX_THEME_TEXT2_FG;
            break;
        case FLUX_ACT_RUNNING: {
            int idx = a->spinner_frame;
            if (idx < 0) idx = -idx;
            glyph     = FLUX_SPINNER_DOT[idx % FLUX_SPINNER_DOT_N];
            glyph_clr = FLUX_THEME_ACCENT_FG;
            label_clr = FLUX_THEME_TEXT_FG;
            break;
        }
        case FLUX_ACT_FAILED:
            glyph     = "\xe2\x9c\x97";        /* ✗ */
            glyph_clr = FLUX_THEME_ERR_FG;
            label_clr = FLUX_THEME_ERR_FG;
            break;
        case FLUX_ACT_PENDING:
        default:
            glyph     = "\xe2\x97\xa6";        /* ◦ */
            glyph_clr = FLUX_THEME_TEXT_OFF_FG;
            label_clr = FLUX_THEME_TEXT_DIM_FG;
            break;
    }
    /* Use a FIXED 2-cell gutter for the glyph so labels always start at
     * the same column regardless of whether the glyph is 1- or 2-cells.
     * (✓/✗ classify as 2 cells in flux's symbol table; ⠋/◦ are 1 cell.) */
    glyph_w = 2;
    (void)flux_strwidth(glyph);   /* still calling for side-effect-of-thought */

    /* Duration formatting. */
    dur_w = 0;
    dur_buf[0] = '\0';
    if (a->duration_ms > 0) {
        dur_w = flux_activity_format_duration(a->duration_ms,
                                              dur_buf, (int)sizeof dur_buf);
        if (dur_w > 0) dur_w = flux_strwidth(dur_buf);
    }

    inner_w = width - indent;
    label_w_full = a->label ? flux_strwidth(a->label) : 0;

    /* Compute the most label cells we can afford while leaving at least
     * a 1-cell pad between label and duration (or 0 if no duration). */
    if (dur_w > 0) {
        label_max = inner_w - glyph_w - gap_w - dur_w - 1;
        if (label_max < 1) {
            /* Duration too wide; drop the duration. */
            dur_w = 0;
            dur_buf[0] = '\0';
            label_max = inner_w - glyph_w - gap_w;
        }
    } else {
        label_max = inner_w - glyph_w - gap_w;
    }
    if (label_max < 0) label_max = 0;

    /* True width-safe label truncation via flux_truncate (UTF-8 + ANSI). */
    if (a->label && label_w_full > label_max) {
        label_w = flux_truncate(a->label, label_max, "\xe2\x80\xa6",
                                label_trunc, (int)sizeof label_trunc);
        label_to_emit = label_trunc;
    } else {
        label_w = label_w_full;
        label_to_emit = a->label ? a->label : "";
    }

    if (dur_w > 0) {
        pad_w = inner_w - glyph_w - gap_w - label_w - dur_w;
    } else {
        pad_w = inner_w - glyph_w - gap_w - label_w;
    }
    if (pad_w < 0) pad_w = 0;

    /* Emit. */
    for (i = 0; i < indent; i++) flux_sb_append(sb, " ");

    /* Glyph: fit to fixed 2-cell gutter so column-0 of the label is the
     * same regardless of glyph width. */
    {
        char G[128]; FluxSB g;
        flux_sb_init(&g, G, (int)sizeof G);
        flux_sb_append(&g, glyph_clr);
        flux_sb_append(&g, glyph);
        flux_sb_append(&g, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&g), glyph_w, NULL, FLUX_ALIGN_LEFT);
    }
    flux_sb_append(sb, " ");

    flux_sb_append(sb, label_clr);
    if (label_to_emit[0]) flux_sb_append(sb, label_to_emit);
    flux_sb_append(sb, FLUX_RESET);

    for (i = 0; i < pad_w; i++) flux_sb_append(sb, " ");

    if (dur_w > 0) {
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(sb, dur_buf);
        flux_sb_append(sb, FLUX_RESET);
    }

    flux_sb_append(sb, "\n");
}

void flux_activity_list_render(FluxSB *sb,
                               const FluxActivity *items, int n,
                               int width, int indent) {
    int i;
    if (!sb || !items || n <= 0) return;
    for (i = 0; i < n; i++) {
        flux_activity_render(sb, &items[i], width, indent);
    }
}


/* Pick fg color escape for a diff kind. */
static const char *flux__diff_fg(FluxDiffKind k) {
    switch (k) {
        case FLUX_DIFF_ADDED:   return FLUX_THEME_DIFF_ADD_FG;
        case FLUX_DIFF_REMOVED: return FLUX_THEME_DIFF_DEL_FG;
        case FLUX_DIFF_CONTEXT: /* fall through */
        default:                return FLUX_THEME_TEXT_DIM_FG;
    }
}

/* Pick marker glyph for a diff kind. */
static char flux__diff_marker(FluxDiffKind k) {
    switch (k) {
        case FLUX_DIFF_ADDED:   return '+';
        case FLUX_DIFF_REMOVED: return '-';
        case FLUX_DIFF_CONTEXT: /* fall through */
        default:                return ' ';
    }
}

/* Append `n` spaces to sb (n may be <= 0 → no-op). */
static void flux__diff_pad_spaces(FluxSB *sb, int n) {
    int i;
    for (i = 0; i < n; i++) flux_sb_append(sb, " ");
}

void flux_diff_block_render(FluxSB *sb,
                            const char *filename,
                            const FluxDiffLine *lines, int n,
                            int width)
{
    if (!sb) return;
    if (width < 4) width = 4;     /* defensive minimum — never silent no-op */

    /* ---- filename row: "  <filename>"  fitted to `width` cells ──────── */
    if (filename) {
        char L[1024]; FluxSB row;
        flux_sb_init(&row, L, (int)sizeof L);
        flux_sb_append(&row, "  ");
        flux_sb_append(&row, FLUX_THEME_TEXT_OFF_FG);
        flux_sb_append(&row, filename);
        flux_sb_append(&row, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }

    /* ---- diff line rows: "  X text"  fitted to `width` cells ────────── */
    if (lines && n > 0) {
        int i;
        for (i = 0; i < n; i++) {
            const FluxDiffLine *Ln = &lines[i];
            const char *fg     = flux__diff_fg(Ln->kind);
            char        marker = flux__diff_marker(Ln->kind);
            const char *text   = Ln->text ? Ln->text : "";

            char L[2048]; FluxSB row;
            flux_sb_init(&row, L, (int)sizeof L);
            flux_sb_append(&row, "  ");
            flux_sb_append(&row, fg);
            { char m[2] = { marker, 0 }; flux_sb_append(&row, m); }
            flux_sb_append(&row, " ");
            flux_sb_append(&row, text);
            flux_sb_append(&row, FLUX_RESET);
            flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(sb, "\n");
        }
    }

    (void)flux__diff_pad_spaces;  /* legacy helper, no longer used */
}


void flux_button_render(FluxSB *sb, const FluxButton *btn, int filled) {
    if (!sb || !btn) return;
    const char *label = btn->label ? btn->label : "";

    if (filled) {
        if (btn->bg_filled) flux_sb_append(sb, btn->bg_filled);
        if (btn->fg_filled) flux_sb_append(sb, btn->fg_filled);
        flux_sb_append(sb, FLUX_BOLD);
    } else {
        if (btn->fg_outline) flux_sb_append(sb, btn->fg_outline);
    }

    flux_sb_append(sb, " ");
    flux_sb_append(sb, label);
    flux_sb_append(sb, " ");
    flux_sb_append(sb, FLUX_RESET);
}


/* lowercase a single ASCII byte (avoids locale-sensitive tolower) */
static int flux__approval_lower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

void flux_approval_init(FluxApproval *a, const char *prompt,
                        FluxButton *buttons, int n) {
    if (!a) return;
    a->prompt    = prompt;
    a->buttons   = buttons;
    a->n         = n;
    a->selected  = 0;
    a->answered  = 0;
    a->result    = 0;
    a->prompt_fg = NULL;
    a->border_fg = NULL;
    a->panel_bg  = NULL;
}

int flux_approval_update(FluxApproval *a, FluxMsg msg) {
    int i;
    const char *k;
    int ch;

    if (!a || a->answered || msg.type != MSG_KEY || a->n <= 0) {
        return 0;
    }
    k = msg.u.key.key;

    if (strcmp(k, "left") == 0 || strcmp(k, "shift+tab") == 0) {
        int prev = a->selected;
        a->selected = a->selected - 1;
        if (a->selected < 0) a->selected = 0;
        return a->selected != prev ? 1 : 0;
    }
    if (strcmp(k, "right") == 0 || strcmp(k, "tab") == 0) {
        int prev = a->selected;
        a->selected = a->selected + 1;
        if (a->selected > a->n - 1) a->selected = a->n - 1;
        return a->selected != prev ? 1 : 0;
    }
    if (strcmp(k, "enter") == 0) {
        a->answered = 1;
        a->result   = a->selected;
        return 1;
    }

    /* single-char shortcuts */
    if (k[0] == '\0' || k[1] != '\0') {
        return 0;
    }
    ch = flux__approval_lower((unsigned char)k[0]);

    /* y/a → first; n/d → last */
    if (ch == 'y' || ch == 'a') {
        a->selected = 0;
        a->answered = 1;
        a->result   = 0;
        return 1;
    }
    if (ch == 'n' || ch == 'd') {
        a->selected = a->n - 1;
        a->answered = 1;
        a->result   = a->n - 1;
        return 1;
    }

    /* first letter of any button label, case-insensitive */
    for (i = 0; i < a->n; i++) {
        const char *lbl = a->buttons[i].label;
        if (lbl && lbl[0] != '\0'
            && flux__approval_lower((unsigned char)lbl[0]) == ch) {
            a->selected = i;
            a->answered = 1;
            a->result   = i;
            return 1;
        }
    }
    return 0;
}

void flux_approval_render(FluxApproval *a, FluxSB *sb, int width) {
    const char *border_fg;
    const char *prompt_fg;
    const char *R = FLUX_RESET;
    const char *h = "\xe2\x94\x80"; /* ─ */
    const char *v = "\xe2\x94\x82"; /* │ */
    int i;
    int prompt_w;
    int buttons_used;
    int prompt_pad;
    int buttons_pad;

    if (!a || !sb || width < 4) return;

    border_fg = a->border_fg ? a->border_fg : FLUX_THEME_BORDER_WARN_FG;
    prompt_fg = a->prompt_fg ? a->prompt_fg : FLUX_THEME_TEXT_FG;

    /* ── Row 1: top border ─────────────────────────────────────────── */
    flux_sb_append(sb, border_fg);
    flux_sb_append(sb, "\xe2\x95\xad"); /* ╭ */
    for (i = 0; i < width - 2; i++) {
        flux_sb_append(sb, h);
    }
    flux_sb_append(sb, "\xe2\x95\xae"); /* ╮ */
    flux_sb_append(sb, R);
    flux_sb_append(sb, "\n");

    /* ── Row 2: prompt line ────────────────────────────────────────── */
    /* Prompt row — interior is `width - 2` cells. Build "  <prompt>" into
     * a scratch SB and let flux_fit guarantee the row width. */
    {
        char L[2048]; FluxSB row;
        flux_sb_init(&row, L, (int)sizeof L);
        flux_sb_append(&row, "  ");
        flux_sb_append(&row, prompt_fg);
        if (a->prompt) flux_sb_append(&row, a->prompt);
        flux_sb_append(&row, FLUX_RESET);

        flux_sb_append(sb, border_fg);
        flux_sb_append(sb, v);
        flux_sb_append(sb, R);
        flux_fit(sb, flux_sb_str(&row), width - 2, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, border_fg);
        flux_sb_append(sb, v);
        flux_sb_append(sb, R);
        flux_sb_append(sb, "\n");
        (void)prompt_w; (void)prompt_pad;
    }

    /* Buttons row — interior is `width - 2` cells. Build into scratch SB:
     * "  [Allow]  [Deny]" then flux_fit. If labels overflow, individual
     * buttons are NOT shrunk (visual integrity > truncated labels); the
     * fit primitive will trim from the right with '…'. */
    {
        char L[4096]; FluxSB row;
        flux_sb_init(&row, L, (int)sizeof L);
        flux_sb_append(&row, "  ");
        for (i = 0; i < a->n; i++) {
            flux_button_render(&row, &a->buttons[i], i == a->selected);
            if (i + 1 < a->n) flux_sb_append(&row, " ");
        }

        flux_sb_append(sb, border_fg);
        flux_sb_append(sb, v);
        flux_sb_append(sb, R);
        flux_fit(sb, flux_sb_str(&row), width - 2, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, border_fg);
        flux_sb_append(sb, v);
        flux_sb_append(sb, R);
        flux_sb_append(sb, "\n");
        (void)buttons_used; (void)buttons_pad;
    }

    /* ── Row 4: blank padding row ──────────────────────────────────── */
    flux_sb_append(sb, border_fg);
    flux_sb_append(sb, v);
    flux_sb_append(sb, R);
    for (i = 0; i < width - 2; i++) {
        flux_sb_append(sb, " ");
    }
    flux_sb_append(sb, border_fg);
    flux_sb_append(sb, v);
    flux_sb_append(sb, R);
    flux_sb_append(sb, "\n");

    /* ── Row 5: bottom border ──────────────────────────────────────── */
    flux_sb_append(sb, border_fg);
    flux_sb_append(sb, "\xe2\x95\xb0"); /* ╰ */
    for (i = 0; i < width - 2; i++) {
        flux_sb_append(sb, h);
    }
    flux_sb_append(sb, "\xe2\x95\xaf"); /* ╯ */
    flux_sb_append(sb, R);
    flux_sb_append(sb, "\n");

    (void)a->panel_bg; /* reserved; current spec doesn't paint inner fill */
}


void flux_inline_bar(FluxSB *sb, float progress, int cells,
                     const char *fill_clr, const char *empty_clr) {
    int filled, i;
    if (!sb || cells <= 0) return;
    if (progress < 0.0f) progress = 0.0f;
    else if (progress > 1.0f) progress = 1.0f;

    filled = (int)(progress * (float)cells + 0.5f);
    if (filled < 0) filled = 0;
    if (filled > cells) filled = cells;

    if (fill_clr) flux_sb_append(sb, fill_clr);
    for (i = 0; i < filled; i++) flux_sb_append(sb, "\xe2\x96\x88"); /* █ */
    if (fill_clr) flux_sb_append(sb, FLUX_RESET);

    if (empty_clr) flux_sb_append(sb, empty_clr);
    for (i = filled; i < cells; i++) flux_sb_append(sb, "\xe2\x96\x92"); /* ▒ */
    if (empty_clr) flux_sb_append(sb, FLUX_RESET);
}

static void flux__statusbar_format_tokens(int n, char *out, int cap) {
    if (n < 0) n = 0;
    if (n < 1000) {
        snprintf(out, (size_t)cap, "%d tok", n);
    } else if (n < 1000000) {
        snprintf(out, (size_t)cap, "%.1fK tok", (double)n / 1000.0);
    } else {
        snprintf(out, (size_t)cap, "%.1fM tok", (double)n / 1000000.0);
    }
}

static void flux__statusbar_format_cost(float c, char *out, int cap) {
    if (c < 0.0f) c = 0.0f;
    snprintf(out, (size_t)cap, "$%.2f", (double)c);
}

static void flux__statusbar_format_pct(float p, char *out, int cap) {
    int pct;
    if (p < 0.0f) p = 0.0f;
    else if (p > 1.0f) p = 1.0f;
    pct = (int)(p * 100.0f + 0.5f);
    snprintf(out, (size_t)cap, "%d%%", pct);
}

/* Build the right segment with a configurable feature mask.  We try a
 * full version, then progressively drop optional pieces if the row would
 * not fit in `total_width` together with the left brand. Returns the
 * final visible width of `R`. */
static int flux__statusbar_build_right(FluxSB *R, const FluxStatusBar *s,
                                       int features, int cells) {
    char tok[24], cost[24], pct[16];
    flux__statusbar_format_tokens(s->tokens, tok, (int)sizeof tok);
    flux__statusbar_format_cost(s->cost, cost, (int)sizeof cost);
    flux__statusbar_format_pct(s->progress, pct, (int)sizeof pct);

    int first = 1;
    if (features & 1) { /* tokens */
        flux_sb_append(R, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(R, tok);
        flux_sb_append(R, FLUX_RESET);
        first = 0;
    }
    if (features & 2) { /* cost */
        if (!first) flux_sb_append(R, " \xc2\xb7 ");
        flux_sb_append(R, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(R, cost);
        flux_sb_append(R, FLUX_RESET);
        first = 0;
    }
    if (features & 4) { /* bar */
        if (!first) flux_sb_append(R, "  ");
        flux_inline_bar(R, s->progress, cells,
                        FLUX_THEME_ACCENT_FG, FLUX_THEME_ACCENT_DIM_FG);
        first = 0;
    }
    if (features & 8) { /* pct */
        if (!first) flux_sb_append(R, "  ");
        flux_sb_append(R, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(R, pct);
        flux_sb_append(R, FLUX_RESET);
        first = 0;
    }
    return flux_strwidth(flux_sb_str(R));
}

void flux_statusbar_render(FluxSB *sb, const FluxStatusBar *s, int total_width) {
    char lbuf[256], rbuf[512];
    FluxSB L, R;
    int lw, rw, gap, i, cells;

    if (!sb || !s) return;
    if (total_width <= 0) return;

    cells = s->bar_cells > 0 ? s->bar_cells : 8;

    /* LEFT: dim "✦ <brand>" + RESET — always emitted but truncated to fit. */
    flux_sb_init(&L, lbuf, sizeof lbuf);
    flux_sb_append(&L, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(&L, "\xe2\x9c\xa6 ");
    flux_sb_append(&L, s->brand ? s->brand : "");
    flux_sb_append(&L, FLUX_RESET);

    /* RIGHT: try feature ladder from richest to leanest until it fits. */
    static const int LADDERS[] = {
        15,  /* tok + cost + bar + pct */
        13,  /* tok + bar + pct        */
         9,  /* tok + pct              */
         8,  /*           pct          */
         0,  /* nothing                */
    };
    int ladder_n = (int)(sizeof LADDERS / sizeof LADDERS[0]);
    int li;
    int picked = 0;
    rw = 0;
    for (li = 0; li < ladder_n; li++) {
        flux_sb_init(&R, rbuf, sizeof rbuf);
        rw = flux__statusbar_build_right(&R, s, LADDERS[li], cells);
        lw = flux_strwidth(flux_sb_str(&L));
        if (lw + rw + 1 <= total_width) { picked = 1; break; }
        /* Try shrinking the bar cells too at the richest level. */
        if (li == 0 && cells > 4) { cells = cells - 2; li = -1; continue; }
    }
    if (!picked) {
        /* Even minimal doesn't fit — emit just left segment fitted. */
        flux_fit(sb, flux_sb_str(&L), total_width, NULL, FLUX_ALIGN_LEFT);
        return;
    }

    /* Truncate left if even the leanest ladder still leaves no room. */
    int left_avail = total_width - rw - 1;
    if (left_avail < 0) left_avail = 0;
    char L_fit[256]; FluxSB L2;
    flux_sb_init(&L2, L_fit, sizeof L_fit);
    flux_fit(&L2, flux_sb_str(&L), left_avail, NULL, FLUX_ALIGN_LEFT);
    lw = flux_strwidth(flux_sb_str(&L2));

    gap = total_width - lw - rw;
    if (gap < 1) gap = 1;

    flux_sb_append(sb, flux_sb_str(&L2));
    for (i = 0; i < gap; i++) flux_sb_append(sb, " ");
    flux_sb_append(sb, flux_sb_str(&R));
}


void flux_brand(FluxSB *sb,
                const char *icon, const char *icon_clr,
                const char *text, const char *text_clr)
{
    if (sb == NULL) return;

    if (icon != NULL) {
        if (icon_clr != NULL) flux_sb_append(sb, icon_clr);
        flux_sb_append(sb, icon);
        flux_sb_append(sb, FLUX_RESET);
    }

    if (icon != NULL && text != NULL) {
        flux_sb_append(sb, " ");
    }

    if (text != NULL) {
        if (text_clr != NULL) flux_sb_append(sb, text_clr);
        flux_sb_append(sb, text);
        flux_sb_append(sb, FLUX_RESET);
    }
}

/* ─── Scrollview / tabbar / layout — implementations ─────────────── */

void flux_scroll_init(FluxScroll *s) {
    if (!s) return;
    s->scroll = 0;
    s->total_lines = 0;
    s->viewport_h = 0;
    s->wheel_step = 2;
    s->page_step  = 6;
}

void flux_scrollview_render(FluxScroll *s, FluxSB *sb,
                            const char *content,
                            int width, int viewport_h)
{
    if (!s || !sb || viewport_h <= 0) return;
    if (!content) content = "";

    /* Count total lines */
    int total = 0;
    for (const char *q = content; *q; q++) if (*q == '\n') total++;
    s->total_lines = total;
    s->viewport_h  = viewport_h;

    /* Clamp scroll */
    int max_scroll = total - viewport_h;
    if (max_scroll < 0) max_scroll = 0;
    if (s->scroll < 0) s->scroll = 0;
    if (s->scroll > max_scroll) s->scroll = max_scroll;

    /* Skip `scroll` lines */
    const char *p = content;
    int skipped = 0;
    while (*p && skipped < s->scroll) {
        if (*p == '\n') skipped++;
        p++;
    }

    /* Emit viewport_h rows. Lines that already fit width are passed
     * through verbatim — re-fitting via flux_fit can corrupt embedded
     * ANSI/UTF-8 mid-codepoint when the source row already contains
     * complex styling. Only re-fit if the source line's display width
     * differs from `width`. */
    int emitted = 0;
    while (*p && emitted < viewport_h) {
        const char *nl = strchr(p, '\n');
        int line_len = nl ? (int)(nl - p) : (int)strlen(p);
        if (line_len <= 0) {
            for (int j = 0; j < width; j++) flux_sb_append(sb, " ");
        } else {
            /* Check display width without copying. */
            char snap[8192];
            int copy = line_len < (int)sizeof snap - 1 ? line_len : (int)sizeof snap - 1;
            memcpy(snap, p, (size_t)copy);
            snap[copy] = '\0';
            int dw = flux_strwidth(snap);
            if (dw == width) {
                /* exact match — emit verbatim, ANSI/UTF-8 preserved */
                for (int j = 0; j < copy; j++) {
                    char c[2] = { snap[j], 0 };
                    flux_sb_append(sb, c);
                }
            } else {
                flux_fit(sb, snap, width, NULL, FLUX_ALIGN_LEFT);
            }
        }
        flux_sb_append(sb, "\n");
        emitted++;
        if (!nl) break;
        p = nl + 1;
    }
    /* Pad with blanks if content is short */
    while (emitted < viewport_h) {
        int j;
        for (j = 0; j < width; j++) flux_sb_append(sb, " ");
        flux_sb_append(sb, "\n");
        emitted++;
    }
}

void flux_scroll_indicator(const FluxScroll *s, FluxSB *sb, int width) {
    if (!s || !sb || width <= 0) return;
    int total = s->total_lines;
    int vh    = s->viewport_h;
    int max_scroll = total - vh;
    if (max_scroll < 0) max_scroll = 0;
    int start = s->scroll + 1;
    int end   = s->scroll + vh;
    if (end > total) end = total;
    if (total == 0) { start = 0; end = 0; }

    char L[1024]; FluxSB l;
    flux_sb_init(&l, L, (int)sizeof L);
    flux_sb_append(&l, "  ");
    flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(&l, "scroll  ");
    float prog = (max_scroll > 0) ? (float)s->scroll / (float)max_scroll : 0.0f;
    int bar_cells = 16;
    if (width < 60) bar_cells = 8;
    if (width < 40) bar_cells = 4;
    flux_inline_bar(&l, prog, bar_cells,
                    FLUX_THEME_ACCENT_FG, FLUX_THEME_TEXT_OFF_FG);
    flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_appendf(&l, "  %d-%d of %d   ", start, end, total);
    flux_sb_append(&l, FLUX_THEME_TEXT_OFF_FG);
    flux_sb_append(&l, "[wheel | \xe2\x86\x91\xe2\x86\x93 | j/k | pgup/pgdn]");
    flux_sb_append(&l, FLUX_RESET);

    flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}

int flux_scroll_update(FluxScroll *s, FluxMsg msg) {
    if (!s) return 0;
    int prev = s->scroll;
    int wheel = s->wheel_step > 0 ? s->wheel_step : 2;
    int page  = s->page_step  > 0 ? s->page_step  : 6;

    if (msg.type == MSG_MOUSE) {
        if (msg.u.mouse.event == FLUX_MOUSE_WHEEL_UP)   s->scroll -= wheel;
        if (msg.u.mouse.event == FLUX_MOUSE_WHEEL_DOWN) s->scroll += wheel;
    } else if (msg.type == MSG_KEY) {
        const char *k = msg.u.key.key;
        if (strcmp(k, "up")    == 0 || strcmp(k, "k") == 0) s->scroll--;
        if (strcmp(k, "down")  == 0 || strcmp(k, "j") == 0) s->scroll++;
        if (strcmp(k, "pgup")   == 0)                       s->scroll -= page;
        if (strcmp(k, "pgdown") == 0)                       s->scroll += page;
        if (strcmp(k, "home")   == 0)                       s->scroll = 0;
        if (strcmp(k, "end")    == 0)                       s->scroll = 9999999;
        if (strcmp(k, "scroll_up")   == 0)                  s->scroll -= wheel;
        if (strcmp(k, "scroll_down") == 0)                  s->scroll += wheel;
    }
    /* Clamp here only against current viewport_h; render will re-clamp
     * against latest content. */
    if (s->scroll < 0) s->scroll = 0;
    return s->scroll != prev;
}

void flux_tabbar_init(FluxTabBar *t, const char **labels, int n) {
    if (!t) return;
    t->labels = labels;
    t->count  = n < FLUX_TABBAR_MAX ? n : FLUX_TABBAR_MAX;
    t->active = 0;
    t->y = 0;
    int i;
    for (i = 0; i < FLUX_TABBAR_MAX; i++) {
        t->x_start[i] = 0;
        t->x_end  [i] = 0;
    }
}

void flux_tabbar_render(FluxTabBar *t, FluxSB *sb, int width,
                        int screen_x_origin, int screen_y)
{
    if (!t || !sb || width <= 0) return;
    char L[2048]; FluxSB l;
    flux_sb_init(&l, L, (int)sizeof L);

    int col = 0;
    flux_sb_append(&l, "  ");
    col += 2;

    t->y = screen_y;
    int i;
    for (i = 0; i < t->count; i++) {
        const char *lbl = t->labels && t->labels[i] ? t->labels[i] : "";
        char body[64];
        snprintf(body, sizeof body, " %d  %s ", i + 1, lbl);
        int body_w = flux_strwidth(body);

        t->x_start[i] = screen_x_origin + col;
        t->x_end  [i] = screen_x_origin + col + body_w - 1;

        if (i == t->active) {
            flux_sb_append(&l, FLUX_THEME_BUTTON_OK_BG);
            flux_sb_append(&l, FLUX_THEME_ACCENT_FG);
            flux_sb_append(&l, FLUX_BOLD);
            flux_sb_append(&l, body);
            flux_sb_append(&l, FLUX_RESET);
        } else {
            flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
            flux_sb_append(&l, body);
            flux_sb_append(&l, FLUX_RESET);
        }
        col += body_w;
        flux_sb_append(&l, "  ");
        col += 2;
    }
    flux_sb_append(&l, FLUX_THEME_TEXT_OFF_FG);
    flux_sb_append(&l, "[click | 1-9 | Tab]  [q] quit");
    flux_sb_append(&l, FLUX_RESET);

    flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}

int flux_tabbar_update(FluxTabBar *t, FluxMsg msg) {
    if (!t || t->count <= 0) return 0;
    int prev = t->active;
    if (msg.type == MSG_KEY) {
        const char *k = msg.u.key.key;
        /* Number keys 1..9 → tab index 0..8 */
        if (k[0] >= '1' && k[0] <= '9' && k[1] == '\0') {
            int idx = k[0] - '1';
            if (idx < t->count) t->active = idx;
        }
        if (strcmp(k, "tab") == 0) {
            t->active = (t->active + 1) % t->count;
        }
        if (strcmp(k, "shift+tab") == 0) {
            t->active = (t->active - 1 + t->count) % t->count;
        }
    } else if (msg.type == MSG_MOUSE &&
               msg.u.mouse.event == FLUX_MOUSE_PRESS &&
               msg.u.mouse.button == 0) {
        int i;
        if (msg.u.mouse.y == t->y) {
            for (i = 0; i < t->count; i++) {
                if (msg.u.mouse.x >= t->x_start[i] &&
                    msg.u.mouse.x <= t->x_end  [i]) {
                    t->active = i;
                    break;
                }
            }
        }
    }
    return t->active != prev;
}

int flux_layout_viewport_h(int total_h, int chrome_rows, int margin_rows,
                           int min_h, int max_h)
{
    int v = total_h - chrome_rows - margin_rows;
    if (min_h > 0 && v < min_h) v = min_h;
    if (max_h > 0 && v > max_h) v = max_h;
    if (v < 1) v = 1;
    return v;
}

/* ── FluxRate — implementation ───────────────────────────────────── */

uint64_t flux_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000L);
}

void flux_rate_init(FluxRate *r, int period_ms) {
    if (!r) return;
    if (period_ms < 0) period_ms = 0;
    r->period_ms = period_ms;
    r->last_ms   = 0;          /* 0 = never; first call to _due returns 1 */
}

void flux_rate_set_fps(FluxRate *r, int fps) {
    if (!r) return;
    r->period_ms = (fps > 0) ? (1000 / fps) : 0;
}

int flux_rate_due(FluxRate *r, uint64_t now_ms) {
    if (!r) return 0;
    if (r->period_ms <= 0) {       /* 0 = "always due" (every call) */
        r->last_ms = now_ms ? now_ms : 1;
        return 1;
    }
    if (r->last_ms == 0) {         /* never armed — fire immediately + arm */
        r->last_ms = now_ms ? now_ms : 1;
        return 1;
    }
    if (now_ms - r->last_ms >= (uint64_t)r->period_ms) {
        /* Catch-up safe: advance by full periods so we don't drift, but
         * cap to "now" if we fell behind by many intervals (e.g. after a
         * suspend). One frame fires per call regardless of backlog. */
        uint64_t delta = now_ms - r->last_ms;
        uint64_t skip  = delta / (uint64_t)r->period_ms;
        if (skip > 1000) skip = 1000;  /* sanity */
        r->last_ms += skip * (uint64_t)r->period_ms;
        if (r->last_ms == 0) r->last_ms = 1;
        return 1;
    }
    return 0;
}

void flux_rate_reset(FluxRate *r) {
    if (!r) return;
    r->last_ms = 0;
}

void flux_message_row(FluxSB *sb,
                      const char *glyph, const char *glyph_clr,
                      const char *body,  const char *body_clr,
                      int width, int indent, int gutter_w)
{
    if (!sb || width <= 0) return;
    if (indent < 0)  indent = 0;
    if (gutter_w < 1) gutter_w = 1;

    /* Build the row in a scratch SB, then run the whole thing through
     * flux_fit so the final emitted width is EXACTLY `width` cells. */
    char L[4096]; FluxSB l;
    flux_sb_init(&l, L, (int)sizeof L);

    /* indent */
    int i;
    for (i = 0; i < indent; i++) flux_sb_append(&l, " ");

    /* glyph cell — measure the glyph and pad to gutter_w. We compose a
     * tiny scratch holding "<color><glyph><reset>" and pass it through
     * flux_fit so that ANY glyph (1-cell, 2-cell, missing) lands as
     * exactly gutter_w cells in the gutter. */
    {
        char G[256]; FluxSB g;
        flux_sb_init(&g, G, (int)sizeof G);
        if (glyph_clr) flux_sb_append(&g, glyph_clr);
        if (glyph)     flux_sb_append(&g, glyph);
        if (glyph_clr) flux_sb_append(&g, FLUX_RESET);
        flux_fit(&l, flux_sb_str(&g), gutter_w, NULL, FLUX_ALIGN_LEFT);
    }
    flux_sb_append(&l, " ");

    /* body */
    if (body_clr) flux_sb_append(&l, body_clr);
    if (body)     flux_sb_append(&l, body);
    if (body_clr) flux_sb_append(&l, FLUX_RESET);

    /* Whole row fitted to width — no overflow possible. */
    flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}


/* ══════════════════════════════════════════════════════════════════
 * STORM-PARITY WIDGETS — implementations
 * ══════════════════════════════════════════════════════════════════ */


/* ─── B1_core_primitives impl ────────────────────────────────── */
/* ── flux_newline ─────────────────────────────────────────────────── */
void flux_newline(FluxSB *sb, int n) {
    int i;
    if (!sb || n <= 0) return;
    for (i = 0; i < n; i++) flux_sb_append(sb, "\n");
}

/* ── flux_spacer ──────────────────────────────────────────────────── */
void flux_spacer(FluxSB *sb, int width, int rows) {
    int r;
    if (!sb || width <= 0 || rows <= 0) return;
    for (r = 0; r < rows; r++) {
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
}

/* ── flux_text ────────────────────────────────────────────────────── */
void flux_text(FluxSB *sb, const char *text, const FluxTextStyle *style) {
    if (!sb || !text) return;
    if (!style) {
        flux_sb_append(sb, text);
        return;
    }
    if (style->bold)      flux_sb_append(sb, FLUX_BOLD);
    if (style->dim)       flux_sb_append(sb, FLUX_DIM);
    if (style->italic)    flux_sb_append(sb, FLUX_ITALIC);
    if (style->underline) flux_sb_append(sb, FLUX_UNDERLINE);
    if (style->strike)    flux_sb_append(sb, FLUX_STRIKE);
    if (style->inverse)   flux_sb_append(sb, "\x1b[7m");
    flux_sb_append(sb, style->fg ? style->fg : FLUX_THEME_TEXT_FG);
    if (style->bg)        flux_sb_append(sb, style->bg);
    flux_sb_append(sb, text);
    flux_sb_append(sb, FLUX_RESET);
}

/* ── flux_heading ─────────────────────────────────────────────────── */
void flux_heading(FluxSB *sb, int level, const char *text, int width) {
    char        scratch_buf[2048];
    FluxSB      row;
    const char *t;
    int         i;

    if (!sb || width <= 0) return;
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    t = text ? text : "";

    flux_sb_init(&row, scratch_buf, (int)sizeof scratch_buf);

    if (level == 1) {
        /* Uppercased, bold, accent. */
        char up[1024];
        int n = (int)strlen(t);
        if (n >= (int)sizeof up) n = (int)sizeof up - 1;
        for (i = 0; i < n; i++) {
            unsigned char c = (unsigned char)t[i];
            up[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
        }
        up[n] = '\0';

        flux_sb_appendf(&row, "%s%s%s%s",
                        FLUX_BOLD, FLUX_THEME_ACCENT_FG, up, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");

        /* Rule row. */
        flux_sb_init(&row, scratch_buf, (int)sizeof scratch_buf);
        flux_sb_append(&row, FLUX_THEME_BORDER_FG);
        for (i = 0; i < width; i++) flux_sb_append(&row, "\xe2\x94\x80"); /* ─ */
        flux_sb_append(&row, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");

        /* Spacer row. */
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
        return;
    }

    if (level == 2) {
        flux_sb_appendf(&row, "%s%s%s%s",
                        FLUX_BOLD, FLUX_THEME_TEXT_FG, t, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
        return;
    }

    /* level == 3 */
    flux_sb_appendf(&row, "%s%s%s%s",
                    FLUX_BOLD, FLUX_THEME_TEXT2_FG, t, FLUX_RESET);
    flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}

/* ── flux_paragraph (internal helpers) ─────────────────────────────
 *
 * Word-wrap honoring display cells.  We walk the input as a stream of
 * "atoms" (one display unit each, except ANSI escapes which have w=0)
 * and group runs of non-space atoms into words.  When a word doesn't
 * fit the remaining width we flush the line; words longer than `width`
 * are hard-split mid-word.  `\n` forces a hard break.
 *
 * We do not call into the private `_flux_atom`; instead we hand-roll a
 * small UTF-8 + ANSI scanner so this header stays self-contained. */

static int _b1_atom(const char *s, int n, int *byte_len, int *cell_w) {
    /* Returns 1 if atom is an escape (cell_w == 0), 0 otherwise.
     * *byte_len gets the byte length, *cell_w gets the display width. */
    unsigned char c;
    if (n <= 0) { *byte_len = 0; *cell_w = 0; return 0; }
    c = (unsigned char)s[0];

    /* CSI / OSC / generic ESC sequence: ESC [ ... final  or  ESC ] ... BEL/ST */
    if (c == 0x1b && n >= 2) {
        int i = 1;
        if (s[1] == '[') {
            i = 2;
            while (i < n) {
                unsigned char d = (unsigned char)s[i];
                i++;
                if (d >= 0x40 && d <= 0x7e) break;
            }
        } else if (s[1] == ']') {
            i = 2;
            while (i < n) {
                unsigned char d = (unsigned char)s[i];
                if (d == 0x07) { i++; break; }
                if (d == 0x1b && i + 1 < n && s[i+1] == '\\') { i += 2; break; }
                i++;
            }
        } else {
            i = 2;
        }
        *byte_len = i;
        *cell_w   = 0;
        return 1;
    }

    /* UTF-8 decode → codepoint → width via flux_strwidth on a 1-atom slice. */
    int blen;
    if      (c < 0x80)              blen = 1;
    else if ((c & 0xE0) == 0xC0)    blen = 2;
    else if ((c & 0xF0) == 0xE0)    blen = 3;
    else if ((c & 0xF8) == 0xF0)    blen = 4;
    else                            blen = 1;
    if (blen > n) blen = n;

    /* Newline / control: width 0 here so caller can react explicitly. */
    if (c == '\n' || c == '\r' || c == '\t' || c == 0x07) {
        *byte_len = 1; *cell_w = 0; return 0;
    }

    char tmp[8];
    int  i;
    for (i = 0; i < blen && i < 7; i++) tmp[i] = s[i];
    tmp[i] = '\0';
    *byte_len = blen;
    *cell_w   = flux_strwidth(tmp);
    if (*cell_w < 0) *cell_w = 0;
    return 0;
}

static void _b1_emit_line(FluxSB *sb, const char *line, int width) {
    /* line is already the right cell-width (or we let flux_fit truncate). */
    flux_fit(sb, line, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}

/* ── flux_paragraph ──────────────────────────────────────────────── */
void flux_paragraph(FluxSB *sb, const char *text, int width) {
    char line_buf[4096];
    int  line_len = 0;     /* bytes in line_buf  */
    int  line_w   = 0;     /* display cells held */
    int  i, n;

    if (!sb || width <= 0 || !text) return;
    n = (int)strlen(text);
    if (n == 0) return;

    i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)text[i];

        /* Hard newline → flush line, advance past '\n'. */
        if (c == '\n') {
            line_buf[line_len] = '\0';
            _b1_emit_line(sb, line_buf, width);
            line_len = 0; line_w = 0;
            i++;
            continue;
        }

        /* Space → if at line start (no visible cells yet) skip; else
         * append a single space (counts as 1 cell). */
        if (c == ' ') {
            if (line_w > 0 && line_w < width
                && line_len + 1 < (int)sizeof line_buf) {
                line_buf[line_len++] = ' ';
                line_w++;
            }
            i++;
            continue;
        }

        /* Build a "word" = consecutive non-space, non-newline atoms. */
        int word_start = i;
        int word_w = 0;
        int j = i;
        while (j < n) {
            unsigned char d = (unsigned char)text[j];
            if (d == ' ' || d == '\n') break;
            int blen, w;
            (void)_b1_atom(text + j, n - j, &blen, &w);
            if (blen == 0) { j++; continue; }
            word_w += w;
            j += blen;
        }
        int word_blen = j - word_start;

        /* Word fits on current line? */
        if (line_w + word_w <= width) {
            if (line_len + word_blen < (int)sizeof line_buf) {
                int k;
                for (k = 0; k < word_blen; k++)
                    line_buf[line_len++] = text[word_start + k];
                line_w += word_w;
            }
            i = j;
            continue;
        }

        /* Word doesn't fit and line has content → flush, retry on new line. */
        if (line_w > 0) {
            /* Trim trailing space. */
            while (line_len > 0 && line_buf[line_len - 1] == ' ') {
                line_len--; line_w--;
            }
            line_buf[line_len] = '\0';
            _b1_emit_line(sb, line_buf, width);
            line_len = 0; line_w = 0;
            continue; /* retry word at top of loop */
        }

        /* Line is empty AND word > width → hard-split mid-word. */
        {
            int k = word_start;
            int cur_w = 0;
            int cur_len = 0;
            while (k < j) {
                int blen, w;
                int is_esc = _b1_atom(text + k, j - k, &blen, &w);
                if (blen == 0) { k++; continue; }
                if (!is_esc && cur_w + w > width) break;
                if (cur_len + blen >= (int)sizeof line_buf) break;
                int kk;
                for (kk = 0; kk < blen; kk++)
                    line_buf[cur_len++] = text[k + kk];
                if (!is_esc) cur_w += w;
                k += blen;
            }
            line_buf[cur_len] = '\0';
            _b1_emit_line(sb, line_buf, width);
            line_len = 0; line_w = 0;
            i = k;
            /* Skip exactly one separating space if the next char is space. */
            if (i < n && text[i] == ' ') i++;
        }
    }

    /* Flush any tail. */
    if (line_w > 0 || line_len > 0) {
        while (line_len > 0 && line_buf[line_len - 1] == ' ') {
            line_len--; if (line_w > 0) line_w--;
        }
        line_buf[line_len] = '\0';
        _b1_emit_line(sb, line_buf, width);
    }
}

/* ── flux_ol ──────────────────────────────────────────────────────── */
void flux_ol(FluxSB *sb, const char **items, int n,
             int width, int start_num) {
    char   row_buf[4096];
    FluxSB row;
    int    i;
    int    last_num;
    int    prefix_digits;
    int    prefix_w;     /* visible cells of "%*d. " */
    int    item_avail;

    if (!sb || !items || n <= 0 || width <= 0) return;
    if (start_num <= 0) start_num = 1;

    last_num = start_num + n - 1;
    /* Count digits in last_num. */
    prefix_digits = 1;
    {
        int v = last_num;
        if (v < 0) v = -v;
        while (v >= 10) { prefix_digits++; v /= 10; }
    }
    prefix_w = prefix_digits + 2;     /* ". " */
    item_avail = width - prefix_w;
    if (item_avail < 0) item_avail = 0;

    for (i = 0; i < n; i++) {
        const char *it = items[i] ? items[i] : "";
        char fitted[2048];

        flux_sb_init(&row, row_buf, (int)sizeof row_buf);

        /* Colored prefix: right-align number to prefix_digits, then ". ". */
        flux_sb_appendf(&row, "%s%*d.%s ",
                        FLUX_THEME_TEXT2_FG,
                        prefix_digits, start_num + i,
                        FLUX_RESET);

        /* Truncate item to remaining width. */
        flux_truncate(it, item_avail, NULL, fitted, (int)sizeof fitted);
        flux_sb_append(&row, FLUX_THEME_TEXT_FG);
        flux_sb_append(&row, fitted);
        flux_sb_append(&row, FLUX_RESET);

        flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
}

/* ── flux_ul ──────────────────────────────────────────────────────── */
void flux_ul(FluxSB *sb, const char **items, int n,
             int width, const char *glyph) {
    char   row_buf[4096];
    FluxSB row;
    int    i;
    int    glyph_w;
    int    item_avail;

    if (!sb || !items || n <= 0 || width <= 0) return;
    if (!glyph) glyph = "\xe2\x80\xa2"; /* • */
    glyph_w    = flux_strwidth(glyph);
    if (glyph_w < 0) glyph_w = 0;
    item_avail = width - glyph_w - 1;
    if (item_avail < 0) item_avail = 0;

    for (i = 0; i < n; i++) {
        const char *it = items[i] ? items[i] : "";
        char fitted[2048];

        flux_sb_init(&row, row_buf, (int)sizeof row_buf);

        flux_sb_append(&row, FLUX_THEME_TEXT2_FG);
        flux_sb_append(&row, glyph);
        flux_sb_append(&row, FLUX_RESET);
        flux_sb_append(&row, " ");

        flux_truncate(it, item_avail, NULL, fitted, (int)sizeof fitted);
        flux_sb_append(&row, FLUX_THEME_TEXT_FG);
        flux_sb_append(&row, fitted);
        flux_sb_append(&row, FLUX_RESET);

        flux_fit(sb, flux_sb_str(&row), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
}

/* ── flux_kbd ─────────────────────────────────────────────────────── */
void flux_kbd(FluxSB *sb, const char *label) {
    if (!sb) return;
    if (!label) label = "";
    flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(sb, "[");
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_append(sb, FLUX_BOLD);
    flux_sb_append(sb, FLUX_THEME_TEXT2_FG);
    flux_sb_append(sb, label);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(sb, "]");
    flux_sb_append(sb, FLUX_RESET);
}

/* ─── B2_form_input impl ────────────────────────────────── */
/* ── Internal UTF-8 helpers (file-scope, separate from input.c) ───── */

static int _flux_b2_utf8_len(const char *buf, int pos, int len) {
    unsigned char c;
    if (pos >= len) return 0;
    c = (unsigned char)buf[pos];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return (pos + 2 <= len) ? 2 : 1;
    if ((c & 0xF0) == 0xE0) return (pos + 3 <= len) ? 3 : 1;
    if ((c & 0xF8) == 0xF0) return (pos + 4 <= len) ? 4 : 1;
    return 1;
}

static int _flux_b2_prev_char(const char *buf, int pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && ((unsigned char)buf[pos] & 0xC0) == 0x80) pos--;
    return pos;
}

static int _flux_b2_char_width(const char *buf, int pos, int len) {
    char tmp[8];
    int  clen = _flux_b2_utf8_len(buf, pos, len);
    if (clen <= 0) return 0;
    if (clen > 7)  clen = 7;
    memcpy(tmp, buf + pos, (size_t)clen);
    tmp[clen] = '\0';
    return flux_strwidth(tmp);
}

/* Return 1 if msg encodes a printable rune we can insert. */
static int _flux_b2_take_rune(FluxMsg msg, char *out, int *out_len) {
    int klen;
    if (msg.u.key.rune >= 32 && msg.u.key.rune < 127) {
        out[0] = (char)msg.u.key.rune;
        out[1] = '\0';
        *out_len = 1;
        return 1;
    }
    if (msg.u.key.rune >= 128 && msg.u.key.key[0] != '\0') {
        klen = (int)strlen(msg.u.key.key);
        if (klen <= 0 || klen > 7) return 0;
        memcpy(out, msg.u.key.key, (size_t)klen);
        out[klen] = '\0';
        *out_len = klen;
        return 1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * 1. FluxCheckbox
 * ════════════════════════════════════════════════════════════════════ */

void flux_checkbox_init(FluxCheckbox *cb, const char *label, int initial) {
    if (!cb) return;
    memset(cb, 0, sizeof(*cb));
    cb->label   = label;
    cb->checked = initial ? 1 : 0;
    cb->focused = 1;
}

int flux_checkbox_update(FluxCheckbox *cb, FluxMsg msg) {
    if (!cb || cb->disabled) return 0;
    if (msg.type != MSG_KEY) return 0;
    if (flux_key_is(msg, "space") || flux_key_is(msg, " ")
        || flux_key_is(msg, "enter")) {
        cb->checked = cb->checked ? 0 : 1;
        return 1;
    }
    return 0;
}

void flux_checkbox_render(FluxCheckbox *cb, FluxSB *sb, int width) {
    char       row[1024];
    FluxSB     r;
    const char *box_fg, *label_fg, *dim;
    const char *marker;

    if (!cb || !sb || width <= 0) return;

    box_fg   = cb->box_fg
                 ? cb->box_fg
                 : (cb->checked ? FLUX_THEME_ACCENT_FG : FLUX_THEME_TEXT_DIM_FG);
    label_fg = cb->label_fg ? cb->label_fg : FLUX_THEME_TEXT_FG;
    dim      = cb->disabled ? FLUX_DIM : "";

    if (cb->checked == 2)      marker = "[-]";
    else if (cb->checked == 1) marker = "[x]";
    else                       marker = "[ ]";

    flux_sb_init(&r, row, (int)sizeof row);
    flux_sb_append(&r, dim);
    flux_sb_append(&r, cb->focused ? "> " : "  ");
    flux_sb_append(&r, box_fg);
    flux_sb_append(&r, marker);
    flux_sb_append(&r, FLUX_RESET);
    flux_sb_append(&r, dim);
    flux_sb_append(&r, " ");
    flux_sb_append(&r, label_fg);
    if (cb->label) flux_sb_append(&r, cb->label);
    flux_sb_append(&r, FLUX_RESET);

    flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);
}

/* ════════════════════════════════════════════════════════════════════
 * 2. FluxSwitch
 * ════════════════════════════════════════════════════════════════════ */

void flux_switch_init(FluxSwitch *sw, const char *label, int initial) {
    if (!sw) return;
    memset(sw, 0, sizeof(*sw));
    sw->label    = label;
    sw->on       = initial ? 1 : 0;
    sw->on_text  = "ON";
    sw->off_text = "OFF";
    sw->focused  = 1;
}

int flux_switch_update(FluxSwitch *sw, FluxMsg msg) {
    if (!sw || sw->disabled) return 0;
    if (msg.type != MSG_KEY) return 0;
    if (flux_key_is(msg, "space") || flux_key_is(msg, " ")
        || flux_key_is(msg, "enter")) {
        sw->on = sw->on ? 0 : 1;
        return 1;
    }
    return 0;
}

void flux_switch_render(FluxSwitch *sw, FluxSB *sb, int width) {
    char       row[1024];
    FluxSB     r;
    const char *bg, *label_fg, *dim;
    const char *on_text, *off_text;

    if (!sw || !sb || width <= 0) return;

    bg       = sw->on ? (sw->on_bg  ? sw->on_bg  : "\x1b[48;2;95;120;73m")
                      : (sw->off_bg ? sw->off_bg : "\x1b[48;2;60;64;93m");
    label_fg = sw->label_fg ? sw->label_fg : FLUX_THEME_TEXT_FG;
    dim      = sw->disabled ? FLUX_DIM : "";
    on_text  = sw->on_text  ? sw->on_text  : "ON";
    off_text = sw->off_text ? sw->off_text : "OFF";

    flux_sb_init(&r, row, (int)sizeof row);
    flux_sb_append(&r, dim);
    flux_sb_append(&r, sw->focused ? "> " : "  ");
    flux_sb_append(&r, bg);
    flux_sb_append(&r, FLUX_BOLD);
    if (sw->on) {
        /* "[●  ON]" pill in 6 inner cells: ●  XX  → use string with width 6 */
        flux_sb_append(&r, " \xe2\x97\x8f ");   /* " ● " width 3 */
        flux_sb_append(&r, on_text);            /* ON width 2 */
        flux_sb_append(&r, " ");                /* pad to ~6 */
    } else {
        flux_sb_append(&r, " ");
        flux_sb_append(&r, off_text);           /* OFF width 3 */
        flux_sb_append(&r, " \xe2\x97\x8f ");
    }
    flux_sb_append(&r, FLUX_RESET);
    flux_sb_append(&r, dim);
    flux_sb_append(&r, " ");
    flux_sb_append(&r, label_fg);
    if (sw->label) flux_sb_append(&r, sw->label);
    flux_sb_append(&r, FLUX_RESET);

    flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);
}

/* ════════════════════════════════════════════════════════════════════
 * 3. FluxRadio
 * ════════════════════════════════════════════════════════════════════ */

static int _flux_radio_disabled(const FluxRadio *r, int i) {
    if (i < 0 || i >= 32) return 0;
    return (r->disabled_mask >> i) & 1;
}

void flux_radio_init(FluxRadio *r, const char **labels, int count, int initial) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->labels    = labels;
    r->count     = count;
    r->selected  = (initial >= 0 && initial < count) ? initial : 0;
    r->highlight = r->selected;
    r->focused   = 1;
}

int flux_radio_update(FluxRadio *r, FluxMsg msg) {
    int prev_h, prev_s, i;

    if (!r || r->count <= 0 || msg.type != MSG_KEY) return 0;

    prev_h = r->highlight;
    prev_s = r->selected;

    if (flux_key_is(msg, "up") || flux_key_is(msg, "k")) {
        for (i = r->highlight - 1; i >= 0; i--) {
            if (!_flux_radio_disabled(r, i)) { r->highlight = i; break; }
        }
        return r->highlight != prev_h;
    }
    if (flux_key_is(msg, "down") || flux_key_is(msg, "j")) {
        for (i = r->highlight + 1; i < r->count; i++) {
            if (!_flux_radio_disabled(r, i)) { r->highlight = i; break; }
        }
        return r->highlight != prev_h;
    }
    if (flux_key_is(msg, "home")) {
        for (i = 0; i < r->count; i++) {
            if (!_flux_radio_disabled(r, i)) { r->highlight = i; break; }
        }
        return r->highlight != prev_h;
    }
    if (flux_key_is(msg, "end")) {
        for (i = r->count - 1; i >= 0; i--) {
            if (!_flux_radio_disabled(r, i)) { r->highlight = i; break; }
        }
        return r->highlight != prev_h;
    }
    if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")
        || flux_key_is(msg, " ")) {
        if (!_flux_radio_disabled(r, r->highlight)) {
            r->selected = r->highlight;
        }
        return r->selected != prev_s;
    }
    return 0;
}

void flux_radio_render(FluxRadio *r, FluxSB *sb, int width) {
    char       row[1024];
    FluxSB     rs;
    int        i;
    const char *marker_fg, *label_fg, *dim_fg;

    if (!r || !sb || width <= 0) return;

    marker_fg = r->marker_fg ? r->marker_fg : FLUX_THEME_ACCENT_FG;
    label_fg  = r->label_fg  ? r->label_fg  : FLUX_THEME_TEXT_FG;
    dim_fg    = r->dim_fg    ? r->dim_fg    : FLUX_THEME_TEXT_DIM_FG;

    for (i = 0; i < r->count; i++) {
        int disabled = _flux_radio_disabled(r, i);
        int selected = (i == r->selected);
        int highlight = (i == r->highlight) && r->focused;

        flux_sb_init(&rs, row, (int)sizeof row);
        flux_sb_append(&rs, highlight ? "> " : "  ");
        if (disabled) {
            flux_sb_append(&rs, dim_fg);
            flux_sb_append(&rs, selected ? "(\xe2\x80\xa2)" : "( )");
        } else if (selected) {
            flux_sb_append(&rs, marker_fg);
            flux_sb_append(&rs, "(\xe2\x80\xa2)");   /* (•) */
        } else {
            flux_sb_append(&rs, dim_fg);
            flux_sb_append(&rs, "( )");
        }
        flux_sb_append(&rs, FLUX_RESET);
        flux_sb_append(&rs, " ");
        flux_sb_append(&rs, disabled ? dim_fg : label_fg);
        if (r->labels && r->labels[i]) flux_sb_append(&rs, r->labels[i]);
        flux_sb_append(&rs, FLUX_RESET);

        flux_fit(sb, flux_sb_str(&rs), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }
}

/* ════════════════════════════════════════════════════════════════════
 * 4. FluxSelect
 * ════════════════════════════════════════════════════════════════════ */

void flux_select_init(FluxSelect *s, const char **labels, int count, int initial) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->labels      = labels;
    s->count       = count;
    s->selected    = (initial >= 0 && initial < count) ? initial : -1;
    s->highlight   = (s->selected >= 0) ? s->selected : 0;
    s->max_visible = 6;
    s->focused     = 1;
}

int flux_select_height(const FluxSelect *s) {
    int vis;
    if (!s) return 1;
    if (!s->open) return 1;
    vis = s->max_visible > 0 ? s->max_visible : 6;
    if (vis > s->count) vis = s->count;
    return 1 + (vis < 0 ? 0 : vis);
}

static void _flux_select_clamp_scroll(FluxSelect *s) {
    int vis = s->max_visible > 0 ? s->max_visible : 6;
    if (vis > s->count) vis = s->count;
    if (s->highlight < 0) s->highlight = 0;
    if (s->highlight >= s->count) s->highlight = s->count - 1;
    if (s->highlight < s->scroll) s->scroll = s->highlight;
    if (s->highlight >= s->scroll + vis) s->scroll = s->highlight - vis + 1;
    if (s->scroll < 0) s->scroll = 0;
    if (s->scroll + vis > s->count) s->scroll = s->count - vis;
    if (s->scroll < 0) s->scroll = 0;
}

int flux_select_update(FluxSelect *s, FluxMsg msg) {
    int prev_o, prev_h, prev_s, vis;

    if (!s || s->count <= 0 || msg.type != MSG_KEY) return 0;

    prev_o = s->open; prev_h = s->highlight; prev_s = s->selected;
    vis = s->max_visible > 0 ? s->max_visible : 6;
    if (vis > s->count) vis = s->count;

    if (!s->open) {
        if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")
            || flux_key_is(msg, " ") || flux_key_is(msg, "down")) {
            s->open = 1;
            if (s->highlight < 0) s->highlight = 0;
            _flux_select_clamp_scroll(s);
            return 1;
        }
        if (flux_key_is(msg, "left")) {
            int n = s->selected;
            if (n < 0) n = 0; else n = (n - 1 + s->count) % s->count;
            s->selected = n; s->highlight = n;
            return s->selected != prev_s;
        }
        if (flux_key_is(msg, "right")) {
            int n = s->selected;
            if (n < 0) n = 0; else n = (n + 1) % s->count;
            s->selected = n; s->highlight = n;
            return s->selected != prev_s;
        }
        return 0;
    }

    /* open */
    if (flux_key_is(msg, "esc")) {
        s->open = 0;
        return 1;
    }
    if (flux_key_is(msg, "up") || flux_key_is(msg, "k")) {
        if (s->highlight > 0) s->highlight--;
        _flux_select_clamp_scroll(s);
        return s->highlight != prev_h;
    }
    if (flux_key_is(msg, "down") || flux_key_is(msg, "j")) {
        if (s->highlight < s->count - 1) s->highlight++;
        _flux_select_clamp_scroll(s);
        return s->highlight != prev_h;
    }
    if (flux_key_is(msg, "home")) {
        s->highlight = 0; _flux_select_clamp_scroll(s);
        return s->highlight != prev_h;
    }
    if (flux_key_is(msg, "end")) {
        s->highlight = s->count - 1; _flux_select_clamp_scroll(s);
        return s->highlight != prev_h;
    }
    if (flux_key_is(msg, "pgup")) {
        s->highlight -= vis;
        _flux_select_clamp_scroll(s);
        return s->highlight != prev_h;
    }
    if (flux_key_is(msg, "pgdown") || flux_key_is(msg, "pgdn")) {
        s->highlight += vis;
        _flux_select_clamp_scroll(s);
        return s->highlight != prev_h;
    }
    if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")
        || flux_key_is(msg, " ")) {
        s->selected = s->highlight;
        s->open     = 0;
        return 1;
    }
    (void)prev_o;
    return 0;
}

void flux_select_render(FluxSelect *s, FluxSB *sb, int width) {
    char       row[2048];
    FluxSB     rs;
    const char *border_fg, *value_fg, *select_bg;
    int        vis, i;
    const char *value_text;

    if (!s || !sb || width <= 0) return;

    border_fg = s->border_fg ? s->border_fg : FLUX_THEME_BORDER_FG;
    value_fg  = s->value_fg  ? s->value_fg  : FLUX_THEME_TEXT_FG;
    select_bg = s->select_bg ? s->select_bg : FLUX_THEME_PANEL_BG;

    /* Row 1: collapsed face */
    flux_sb_init(&rs, row, (int)sizeof row);
    flux_sb_append(&rs, border_fg);
    flux_sb_append(&rs, "[ ");
    flux_sb_append(&rs, FLUX_RESET);
    if (s->selected >= 0 && s->labels && s->labels[s->selected]) {
        flux_sb_append(&rs, value_fg);
        value_text = s->labels[s->selected];
        flux_sb_append(&rs, value_text);
        flux_sb_append(&rs, FLUX_RESET);
    } else if (s->placeholder) {
        flux_sb_append(&rs, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&rs, s->placeholder);
        flux_sb_append(&rs, FLUX_RESET);
    }
    /* sentinel for caret column — flux_fit will pad before adding caret */
    {
        char       face[2048];
        FluxSB     face_sb;
        const char *body = flux_sb_str(&rs);
        int         caret_w = 4; /* " ▾ ]" */
        if (width < caret_w + 2) caret_w = width;
        flux_sb_init(&face_sb, face, (int)sizeof face);
        flux_fit(&face_sb, body, width - caret_w, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(&face_sb, border_fg);
        flux_sb_append(&face_sb, " \xe2\x96\xbe ]");  /* " ▾ ]" */
        flux_sb_append(&face_sb, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&face_sb), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }

    if (!s->open) return;

    vis = s->max_visible > 0 ? s->max_visible : 6;
    if (vis > s->count) vis = s->count;

    for (i = 0; i < vis; i++) {
        int idx = s->scroll + i;
        char       buf[2048];
        FluxSB     b;
        int        is_hl = (idx == s->highlight);

        if (idx < 0 || idx >= s->count) {
            /* pad blank row */
            flux_sb_init(&b, buf, (int)sizeof buf);
            flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
            continue;
        }

        flux_sb_init(&b, buf, (int)sizeof buf);
        if (is_hl) flux_sb_append(&b, select_bg);
        flux_sb_append(&b, is_hl ? "\xe2\x96\xb8 " : "  ");  /* ▸ */
        flux_sb_append(&b, value_fg);
        if (s->labels && s->labels[idx]) flux_sb_append(&b, s->labels[idx]);
        flux_sb_append(&b, FLUX_RESET);

        flux_fit(sb, flux_sb_str(&b), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }
}

/* ════════════════════════════════════════════════════════════════════
 * 5. FluxTextArea — multi-line editor
 * ════════════════════════════════════════════════════════════════════ */

void flux_textarea_init(FluxTextArea *ta, const char *placeholder) {
    if (!ta) return;
    memset(ta, 0, sizeof(*ta));
    ta->placeholder = placeholder;
    ta->tab_size    = 2;
    ta->wrap        = 1;
}

void flux_textarea_clear(FluxTextArea *ta) {
    if (!ta) return;
    ta->buf[0]    = '\0';
    ta->len       = 0;
    ta->cursor    = 0;
    ta->scroll_row = 0;
    ta->submitted = 0;
}

/* Insert raw bytes at cursor; returns number inserted. */
static int _flux_ta_insert_bytes(FluxTextArea *ta, const char *src, int n) {
    int avail = FLUX_INPUT_MAX - ta->len;
    if (n <= 0) return 0;
    if (n > avail) n = avail;
    if (n <= 0) return 0;
    memmove(ta->buf + ta->cursor + n,
            ta->buf + ta->cursor,
            (size_t)(ta->len - ta->cursor));
    memcpy(ta->buf + ta->cursor, src, (size_t)n);
    ta->len    += n;
    ta->cursor += n;
    ta->buf[ta->len] = '\0';
    return n;
}

/* Find byte offset of beginning of logical line containing `pos`. */
static int _flux_ta_line_start(const FluxTextArea *ta, int pos) {
    while (pos > 0 && ta->buf[pos - 1] != '\n') pos--;
    return pos;
}

/* Find byte offset of '\n' (or len) at end of logical line containing `pos`. */
static int _flux_ta_line_end(const FluxTextArea *ta, int pos) {
    while (pos < ta->len && ta->buf[pos] != '\n') pos++;
    return pos;
}

/* Display column of `pos` on its logical line. */
static int _flux_ta_col_of(const FluxTextArea *ta, int pos) {
    int ls = _flux_ta_line_start(ta, pos);
    int col = 0;
    int p = ls;
    while (p < pos) {
        col += _flux_b2_char_width(ta->buf, p, ta->len);
        p   += _flux_b2_utf8_len(ta->buf, p, ta->len);
    }
    return col;
}

/* Move within a logical line to nearest byte offset for display column. */
static int _flux_ta_pos_at_col(const FluxTextArea *ta, int line_start, int target_col) {
    int p = line_start, col = 0;
    while (p < ta->len && ta->buf[p] != '\n') {
        int cw = _flux_b2_char_width(ta->buf, p, ta->len);
        if (col + cw > target_col) break;
        col += cw;
        p   += _flux_b2_utf8_len(ta->buf, p, ta->len);
    }
    return p;
}

int flux_textarea_update(FluxTextArea *ta, FluxMsg msg) {
    if (!ta) return 0;

    if (msg.type == MSG_PASTE) {
        return _flux_ta_insert_bytes(ta, msg.u.paste.text, msg.u.paste.len) > 0;
    }
    if (msg.type != MSG_KEY) return 0;

    /* Submission: spec says Ctrl+Enter — terminals usually can't send that
     * distinct from Enter. Accept ctrl+s and ctrl+j as conventional aliases. */
    if (flux_key_is(msg, "ctrl+s") || flux_key_is(msg, "ctrl+j")) {
        ta->submitted = 1;
        return 1;
    }

    if (flux_key_is(msg, "enter")) {
        return _flux_ta_insert_bytes(ta, "\n", 1) > 0;
    }

    if (flux_key_is(msg, "tab")) {
        int i, ts = ta->tab_size > 0 ? ta->tab_size : 2;
        char spaces[16];
        if (ts > 16) ts = 16;
        for (i = 0; i < ts; i++) spaces[i] = ' ';
        return _flux_ta_insert_bytes(ta, spaces, ts) > 0;
    }

    if (flux_key_is(msg, "backspace")) {
        if (ta->cursor > 0) {
            int prev = _flux_b2_prev_char(ta->buf, ta->cursor);
            int del  = ta->cursor - prev;
            memmove(ta->buf + prev, ta->buf + ta->cursor,
                    (size_t)(ta->len - ta->cursor));
            ta->len    -= del;
            ta->cursor  = prev;
            ta->buf[ta->len] = '\0';
            return 1;
        }
        return 0;
    }

    if (flux_key_is(msg, "delete")) {
        if (ta->cursor < ta->len) {
            int clen = _flux_b2_utf8_len(ta->buf, ta->cursor, ta->len);
            memmove(ta->buf + ta->cursor, ta->buf + ta->cursor + clen,
                    (size_t)(ta->len - ta->cursor - clen));
            ta->len -= clen;
            ta->buf[ta->len] = '\0';
            return 1;
        }
        return 0;
    }

    if (flux_key_is(msg, "left")) {
        if (ta->cursor > 0) {
            ta->cursor = _flux_b2_prev_char(ta->buf, ta->cursor);
            return 1;
        }
        return 0;
    }
    if (flux_key_is(msg, "right")) {
        if (ta->cursor < ta->len) {
            ta->cursor += _flux_b2_utf8_len(ta->buf, ta->cursor, ta->len);
            return 1;
        }
        return 0;
    }

    if (flux_key_is(msg, "up")) {
        int ls   = _flux_ta_line_start(ta, ta->cursor);
        int col;
        if (ls == 0) return 0;
        col = _flux_ta_col_of(ta, ta->cursor);
        {
            int prev_end = ls - 1;            /* on '\n' */
            int prev_ls  = _flux_ta_line_start(ta, prev_end);
            ta->cursor = _flux_ta_pos_at_col(ta, prev_ls, col);
        }
        return 1;
    }
    if (flux_key_is(msg, "down")) {
        int le = _flux_ta_line_end(ta, ta->cursor);
        int col;
        if (le >= ta->len) return 0;
        col = _flux_ta_col_of(ta, ta->cursor);
        ta->cursor = _flux_ta_pos_at_col(ta, le + 1, col);
        return 1;
    }

    if (flux_key_is(msg, "home")) {
        int ls = _flux_ta_line_start(ta, ta->cursor);
        if (ta->cursor == ls) return 0;
        ta->cursor = ls;
        return 1;
    }
    if (flux_key_is(msg, "end")) {
        int le = _flux_ta_line_end(ta, ta->cursor);
        if (ta->cursor == le) return 0;
        ta->cursor = le;
        return 1;
    }
    if (flux_key_is(msg, "ctrl+home")) {
        if (ta->cursor == 0) return 0;
        ta->cursor = 0;
        return 1;
    }
    if (flux_key_is(msg, "ctrl+end")) {
        if (ta->cursor == ta->len) return 0;
        ta->cursor = ta->len;
        return 1;
    }

    if (flux_key_is(msg, "pgup")) {
        int rows = ta->visible_rows > 0 ? ta->visible_rows : 1;
        int i;
        for (i = 0; i < rows; i++) {
            int ls = _flux_ta_line_start(ta, ta->cursor);
            if (ls == 0) break;
            ta->cursor = ls - 1;
        }
        return 1;
    }
    if (flux_key_is(msg, "pgdown") || flux_key_is(msg, "pgdn")) {
        int rows = ta->visible_rows > 0 ? ta->visible_rows : 1;
        int i;
        for (i = 0; i < rows; i++) {
            int le = _flux_ta_line_end(ta, ta->cursor);
            if (le >= ta->len) break;
            ta->cursor = le + 1;
        }
        return 1;
    }

    if (flux_key_is(msg, "ctrl+u")) {
        int ls = _flux_ta_line_start(ta, ta->cursor);
        if (ls == ta->cursor) return 0;
        memmove(ta->buf + ls, ta->buf + ta->cursor,
                (size_t)(ta->len - ta->cursor));
        ta->len   -= (ta->cursor - ls);
        ta->cursor = ls;
        ta->buf[ta->len] = '\0';
        return 1;
    }
    if (flux_key_is(msg, "ctrl+k")) {
        int le = _flux_ta_line_end(ta, ta->cursor);
        if (le == ta->cursor) return 0;
        memmove(ta->buf + ta->cursor, ta->buf + le,
                (size_t)(ta->len - le));
        ta->len -= (le - ta->cursor);
        ta->buf[ta->len] = '\0';
        return 1;
    }
    if (flux_key_is(msg, "ctrl+w")) {
        int target = ta->cursor;
        while (target > 0 && (ta->buf[target - 1] == ' '
                              || ta->buf[target - 1] == '\t'))
            target--;
        while (target > 0 && ta->buf[target - 1] != ' '
                          && ta->buf[target - 1] != '\t'
                          && ta->buf[target - 1] != '\n')
            target--;
        if (target == ta->cursor) return 0;
        memmove(ta->buf + target, ta->buf + ta->cursor,
                (size_t)(ta->len - ta->cursor));
        ta->len   -= (ta->cursor - target);
        ta->cursor = target;
        ta->buf[ta->len] = '\0';
        return 1;
    }

    /* Printable rune insert */
    {
        char tmp[8];
        int  tlen;
        if (_flux_b2_take_rune(msg, tmp, &tlen)) {
            return _flux_ta_insert_bytes(ta, tmp, tlen) > 0;
        }
    }
    return 0;
}

/* Render layout: each logical line, optionally soft-wrapped at `width`.
 * We build the visual lines as a series of (start, end) byte ranges into
 * a small stack array; if more than render limit, we scroll. */

#define _FLUX_TA_VLINES_MAX 256

void flux_textarea_render(FluxTextArea *ta, FluxSB *sb, int width, int rows) {
    const char *fg, *cur_fg, *ph_fg;
    int  vstart[_FLUX_TA_VLINES_MAX];
    int  vend  [_FLUX_TA_VLINES_MAX];
    int  v_count = 0;
    int  i, p;
    int  cursor_vline = 0;
    int  scroll;

    if (!ta || !sb || width <= 0 || rows <= 0) return;
    fg     = ta->fg          ? ta->fg          : FLUX_THEME_TEXT_FG;
    cur_fg = ta->cursor_fg   ? ta->cursor_fg   : "\x1b[7m";
    ph_fg  = ta->placeholder_fg ? ta->placeholder_fg : FLUX_THEME_TEXT_DIM_FG;
    ta->visible_rows = rows;

    /* Empty buffer → placeholder + cursor on row 0, blank padding. */
    if (ta->len == 0) {
        char       row[2048];
        FluxSB     r;
        flux_sb_init(&r, row, (int)sizeof row);
        flux_sb_append(&r, cur_fg);
        flux_sb_append(&r, " ");
        flux_sb_append(&r, FLUX_RESET);
        if (ta->placeholder) {
            flux_sb_append(&r, ph_fg);
            flux_sb_append(&r, ta->placeholder);
            flux_sb_append(&r, FLUX_RESET);
        }
        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
        for (i = 1; i < rows; i++) {
            flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
        }
        return;
    }

    /* Build visual lines. */
    p = 0;
    while (p <= ta->len && v_count < _FLUX_TA_VLINES_MAX) {
        int line_end = _flux_ta_line_end(ta, p);
        if (ta->wrap) {
            int q = p, col = 0;
            int seg_start = p;
            while (q < line_end) {
                int cw = _flux_b2_char_width(ta->buf, q, ta->len);
                int cl = _flux_b2_utf8_len(ta->buf, q, ta->len);
                if (col + cw > width) {
                    vstart[v_count] = seg_start;
                    vend  [v_count] = q;
                    v_count++;
                    if (v_count >= _FLUX_TA_VLINES_MAX) break;
                    seg_start = q;
                    col = 0;
                }
                col += cw;
                q   += cl;
            }
            if (v_count < _FLUX_TA_VLINES_MAX) {
                vstart[v_count] = seg_start;
                vend  [v_count] = q;
                v_count++;
            }
        } else {
            if (v_count < _FLUX_TA_VLINES_MAX) {
                vstart[v_count] = p;
                vend  [v_count] = line_end;
                v_count++;
            }
        }
        if (line_end >= ta->len) break;
        p = line_end + 1;
    }
    if (v_count == 0) {
        vstart[0] = 0; vend[0] = 0; v_count = 1;
    }

    /* Find which visual line holds cursor. */
    for (i = 0; i < v_count; i++) {
        if (ta->cursor >= vstart[i] && ta->cursor <= vend[i]) {
            cursor_vline = i;
            /* Prefer end of previous line if cursor sits exactly at start
             * of a wrapped segment AND the prev segment is on same logical
             * line (so the cursor visually appears on the seam). For
             * simplicity we accept the later-line position. */
            break;
        }
    }

    /* Adjust scroll_row so cursor visible. */
    scroll = ta->scroll_row;
    if (cursor_vline < scroll) scroll = cursor_vline;
    if (cursor_vline >= scroll + rows) scroll = cursor_vline - rows + 1;
    if (scroll < 0) scroll = 0;
    if (scroll > v_count - 1) scroll = v_count - 1;
    ta->scroll_row = scroll;

    /* Emit `rows` rows. */
    for (i = 0; i < rows; i++) {
        int  vi = scroll + i;
        char row[4096];
        FluxSB r;

        flux_sb_init(&r, row, (int)sizeof row);

        if (vi >= v_count) {
            flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
            continue;
        }

        flux_sb_append(&r, fg);
        {
            int s_pos = vstart[vi], e_pos = vend[vi];
            int q = s_pos, col = 0;
            int cur_drawn = 0;

            while (q < e_pos && col < width) {
                int cw = _flux_b2_char_width(ta->buf, q, ta->len);
                int cl = _flux_b2_utf8_len(ta->buf, q, ta->len);
                char tmp[8];
                int  copy = cl > 7 ? 7 : cl;

                if (col + cw > width) break;
                memcpy(tmp, ta->buf + q, (size_t)copy);
                tmp[copy] = '\0';

                if (q == ta->cursor) {
                    flux_sb_append(&r, cur_fg);
                    flux_sb_append(&r, tmp);
                    flux_sb_append(&r, FLUX_RESET);
                    flux_sb_append(&r, fg);
                    cur_drawn = 1;
                } else {
                    flux_sb_append(&r, tmp);
                }
                col += cw;
                q   += cl;
            }
            /* Cursor at end of visual line on the row that owns it. */
            if (!cur_drawn && ta->cursor == e_pos
                && vi == cursor_vline && col < width) {
                flux_sb_append(&r, cur_fg);
                flux_sb_append(&r, " ");
                flux_sb_append(&r, FLUX_RESET);
                flux_sb_append(&r, fg);
                col++;
                (void)col;
            }
            flux_sb_append(&r, FLUX_RESET);
        }

        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }
}

/* ════════════════════════════════════════════════════════════════════
 * 6. FluxMaskedInput
 * ════════════════════════════════════════════════════════════════════ */

static int _flux_masked_is_slot(char m) {
    return m == '#' || m == 'A' || m == '*';
}

static int _flux_masked_match(char m, int rune) {
    if (m == '#') return rune >= '0' && rune <= '9';
    if (m == 'A') return (rune >= 'A' && rune <= 'Z') || (rune >= 'a' && rune <= 'z');
    if (m == '*') return rune >= 32 && rune < 127;
    return 0;
}

/* Count number of slot-positions in mask. */
static int _flux_masked_slots(const char *mask) {
    int n = 0;
    if (!mask) return 0;
    while (*mask) { if (_flux_masked_is_slot(*mask)) n++; mask++; }
    return n;
}

void flux_masked_init(FluxMaskedInput *mi, const char *mask) {
    if (!mi) return;
    memset(mi, 0, sizeof(*mi));
    mi->mask    = mask;
    mi->focused = 1;
}

void flux_masked_clear(FluxMaskedInput *mi) {
    if (!mi) return;
    mi->raw[0]    = '\0';
    mi->raw_len   = 0;
    mi->raw_cursor = 0;
    mi->submitted = 0;
}

int flux_masked_update(FluxMaskedInput *mi, FluxMsg msg) {
    int slots;

    if (!mi || !mi->mask) return 0;

    if (msg.type != MSG_KEY) return 0;
    slots = _flux_masked_slots(mi->mask);

    if (flux_key_is(msg, "enter")) {
        mi->submitted = 1;
        return 1;
    }
    if (flux_key_is(msg, "backspace")) {
        if (mi->raw_cursor > 0) {
            int prev = mi->raw_cursor - 1;
            memmove(mi->raw + prev, mi->raw + mi->raw_cursor,
                    (size_t)(mi->raw_len - mi->raw_cursor));
            mi->raw_len--;
            mi->raw_cursor = prev;
            mi->raw[mi->raw_len] = '\0';
            return 1;
        }
        return 0;
    }
    if (flux_key_is(msg, "delete")) {
        if (mi->raw_cursor < mi->raw_len) {
            memmove(mi->raw + mi->raw_cursor, mi->raw + mi->raw_cursor + 1,
                    (size_t)(mi->raw_len - mi->raw_cursor - 1));
            mi->raw_len--;
            mi->raw[mi->raw_len] = '\0';
            return 1;
        }
        return 0;
    }
    if (flux_key_is(msg, "left")) {
        if (mi->raw_cursor > 0) { mi->raw_cursor--; return 1; }
        return 0;
    }
    if (flux_key_is(msg, "right")) {
        if (mi->raw_cursor < mi->raw_len) { mi->raw_cursor++; return 1; }
        return 0;
    }
    if (flux_key_is(msg, "home") || flux_key_is(msg, "ctrl+a")) {
        if (mi->raw_cursor == 0) return 0;
        mi->raw_cursor = 0;
        return 1;
    }
    if (flux_key_is(msg, "end") || flux_key_is(msg, "ctrl+e")) {
        if (mi->raw_cursor == mi->raw_len) return 0;
        mi->raw_cursor = mi->raw_len;
        return 1;
    }
    if (flux_key_is(msg, "ctrl+u")) {
        if (mi->raw_len == 0) return 0;
        mi->raw[0]     = '\0';
        mi->raw_len    = 0;
        mi->raw_cursor = 0;
        return 1;
    }

    /* Printable: insert if matches the next slot type. We compute which
     * mask slot index our current cursor maps to (raw_cursor) and check
     * the corresponding mask char. */
    if (msg.u.key.rune >= 32 && msg.u.key.rune < 127
        && mi->raw_len < slots && mi->raw_len < FLUX_INPUT_MAX) {
        const char *m = mi->mask;
        int slot_idx = mi->raw_cursor;
        int seen = 0;
        while (*m) {
            if (_flux_masked_is_slot(*m)) {
                if (seen == slot_idx) {
                    if (_flux_masked_match(*m, msg.u.key.rune)) {
                        memmove(mi->raw + mi->raw_cursor + 1,
                                mi->raw + mi->raw_cursor,
                                (size_t)(mi->raw_len - mi->raw_cursor));
                        mi->raw[mi->raw_cursor] = (char)msg.u.key.rune;
                        mi->raw_len++;
                        mi->raw_cursor++;
                        mi->raw[mi->raw_len] = '\0';
                        return 1;
                    }
                    return 0;
                }
                seen++;
            }
            m++;
        }
        return 0;
    }
    return 0;
}

void flux_masked_render(FluxMaskedInput *mi, FluxSB *sb, int width) {
    char       row[2048];
    FluxSB     r;
    const char *fg, *lit_fg, *ph_fg;
    const char *m;
    int        slot_idx = 0;

    if (!mi || !sb || width <= 0) return;
    fg     = mi->fg              ? mi->fg              : FLUX_THEME_TEXT_FG;
    lit_fg = mi->literal_fg      ? mi->literal_fg      : FLUX_THEME_TEXT_DIM_FG;
    ph_fg  = mi->placeholder_fg  ? mi->placeholder_fg  : FLUX_THEME_TEXT_OFF_FG;
    m      = mi->mask ? mi->mask : "";

    flux_sb_init(&r, row, (int)sizeof row);

    while (*m) {
        char one[2];
        one[1] = '\0';
        if (_flux_masked_is_slot(*m)) {
            if (slot_idx < mi->raw_len) {
                if (slot_idx == mi->raw_cursor) {
                    flux_sb_append(&r, "\x1b[7m");
                    one[0] = mi->raw[slot_idx];
                    flux_sb_append(&r, one);
                    flux_sb_append(&r, FLUX_RESET);
                } else {
                    flux_sb_append(&r, fg);
                    one[0] = mi->raw[slot_idx];
                    flux_sb_append(&r, one);
                    flux_sb_append(&r, FLUX_RESET);
                }
            } else if (slot_idx == mi->raw_cursor) {
                flux_sb_append(&r, "\x1b[7m");
                flux_sb_append(&r, "_");
                flux_sb_append(&r, FLUX_RESET);
            } else {
                flux_sb_append(&r, ph_fg);
                flux_sb_append(&r, "_");
                flux_sb_append(&r, FLUX_RESET);
            }
            slot_idx++;
        } else {
            flux_sb_append(&r, lit_fg);
            one[0] = *m;
            flux_sb_append(&r, one);
            flux_sb_append(&r, FLUX_RESET);
        }
        m++;
    }

    flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);
}

const char *flux_masked_value(FluxMaskedInput *mi) {
    /* Return formatted mask with typed values substituted.
     * Uses static buffer per call. Truncates literals beyond raw_len. */
    static char out[FLUX_INPUT_MAX + 1];
    int  oi = 0;
    int  slot_idx = 0;
    const char *m;
    if (!mi || !mi->mask) { out[0] = '\0'; return out; }
    m = mi->mask;
    while (*m && oi < (int)sizeof out - 1) {
        if (_flux_masked_is_slot(*m)) {
            if (slot_idx >= mi->raw_len) break;
            out[oi++] = mi->raw[slot_idx++];
        } else {
            out[oi++] = *m;
        }
        m++;
    }
    out[oi] = '\0';
    return out;
}

/* ════════════════════════════════════════════════════════════════════
 * 7. FluxSearchInput
 * ════════════════════════════════════════════════════════════════════ */

static const char *_FLUX_SEARCH_SPIN[] = {
    "\xe2\xa0\x8b","\xe2\xa0\x99","\xe2\xa0\xb9","\xe2\xa0\xb8",
    "\xe2\xa0\xbc","\xe2\xa0\xb4","\xe2\xa0\xa6","\xe2\xa0\xa7",
    "\xe2\xa0\x87","\xe2\xa0\x8f"
};
#define _FLUX_SEARCH_SPIN_N 10

void flux_search_init(FluxSearchInput *s, const char *placeholder) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    flux_input_init(&s->inner, placeholder);
    s->result_count = -1;
    s->total_count  = -1;
    s->icon         = "\xe2\x8c\x95 ";  /* "⌕ " */
}

int flux_search_update(FluxSearchInput *s, FluxMsg msg) {
    if (!s) return 0;
    if (msg.type == MSG_TICK) {
        if (s->loading) {
            s->spin_frame = (s->spin_frame + 1) % _FLUX_SEARCH_SPIN_N;
            return 1;
        }
        return 0;
    }
    if (msg.type == MSG_KEY) {
        if (flux_key_is(msg, "esc") || flux_key_is(msg, "ctrl+l")) {
            int had = s->inner.len > 0;
            flux_input_clear(&s->inner);
            s->cleared = 1;
            return had ? 1 : 1;
        }
    }
    return flux_input_update(&s->inner, msg);
}

void flux_search_render(FluxSearchInput *s, FluxSB *sb, int width) {
    char       inner_buf[4096];
    FluxSB     ib;
    char       row[8192];
    FluxSB     r;
    const char *icon_fg, *count_fg;
    char       count_text[64];
    int        count_w = 0;
    int        icon_w;
    int        avail;

    if (!s || !sb || width <= 0) return;
    icon_fg  = s->icon_fg  ? s->icon_fg  : FLUX_THEME_ACCENT_FG;
    count_fg = s->count_fg ? s->count_fg : FLUX_THEME_TEXT_DIM_FG;

    count_text[0] = '\0';
    if (s->result_count >= 0) {
        if (s->total_count >= 0)
            snprintf(count_text, sizeof count_text, "(%d of %d)",
                     s->result_count, s->total_count);
        else
            snprintf(count_text, sizeof count_text, "(%d)", s->result_count);
        count_w = flux_strwidth(count_text) + 1;  /* + 1 leading space */
    }

    icon_w = s->icon ? flux_strwidth(s->icon) : 0;
    avail = width - icon_w - count_w;
    if (avail < 1) avail = 1;

    /* Build the inner-input segment to width = avail using flux_input_render
     * (which already pads to its width). */
    flux_sb_init(&ib, inner_buf, (int)sizeof inner_buf);
    if (s->loading) {
        flux_sb_append(&ib, icon_fg);
        flux_sb_append(&ib, _FLUX_SEARCH_SPIN[s->spin_frame % _FLUX_SEARCH_SPIN_N]);
        flux_sb_append(&ib, FLUX_RESET);
        flux_sb_append(&ib, " ");
    }
    flux_input_render(&s->inner, &ib, avail - (s->loading ? 2 : 0),
                      NULL, NULL);

    flux_sb_init(&r, row, (int)sizeof row);
    if (s->icon) {
        flux_sb_append(&r, icon_fg);
        flux_sb_append(&r, s->icon);
        flux_sb_append(&r, FLUX_RESET);
    }
    flux_sb_append(&r, flux_sb_str(&ib));
    if (count_w > 0) {
        flux_sb_append(&r, " ");
        flux_sb_append(&r, count_fg);
        flux_sb_append(&r, count_text);
        flux_sb_append(&r, FLUX_RESET);
    }

    flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);
}

const char *flux_search_value(const FluxSearchInput *s) {
    if (!s) return "";
    return s->inner.buf;
}

/* ════════════════════════════════════════════════════════════════════
 * 8. FluxChatInput
 * ════════════════════════════════════════════════════════════════════ */

void flux_chat_init(FluxChatInput *c, const char *placeholder) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    flux_textarea_init(&c->ta, placeholder);
    c->history_pos = -1;
    c->prompt      = "> ";
}

static void _flux_chat_save_draft(FluxChatInput *c) {
    int n = c->ta.len;
    if (n > FLUX_INPUT_MAX) n = FLUX_INPUT_MAX;
    memcpy(c->draft, c->ta.buf, (size_t)n);
    c->draft[n] = '\0';
}

static void _flux_chat_load(FluxChatInput *c, const char *text) {
    int n = (int)strlen(text);
    if (n > FLUX_INPUT_MAX) n = FLUX_INPUT_MAX;
    memcpy(c->ta.buf, text, (size_t)n);
    c->ta.buf[n]   = '\0';
    c->ta.len      = n;
    c->ta.cursor   = n;
    c->ta.scroll_row = 0;
}

int flux_chat_update(FluxChatInput *c, FluxMsg msg) {
    int submit_now = 0;
    int browse_up = 0, browse_down = 0;

    if (!c) return 0;

    if (msg.type == MSG_KEY) {
        /* Submission semantics. */
        if (c->multiline) {
            if (flux_key_is(msg, "ctrl+s") || flux_key_is(msg, "ctrl+j"))
                submit_now = 1;
        } else {
            if (flux_key_is(msg, "enter"))
                submit_now = 1;
        }

        /* History browsing — only if at row 0 / last row of textarea. */
        if (!submit_now && flux_key_is(msg, "up")) {
            int ls = _flux_ta_line_start(&c->ta, c->ta.cursor);
            if (ls == 0) browse_up = 1;
        }
        if (!submit_now && flux_key_is(msg, "down")) {
            int le = _flux_ta_line_end(&c->ta, c->ta.cursor);
            if (le == c->ta.len) browse_down = 1;
        }
    }

    if (submit_now) {
        if (c->ta.len > 0) {
            int n = c->ta.len;
            if (n > FLUX_INPUT_MAX) n = FLUX_INPUT_MAX;
            memcpy(c->consumed, c->ta.buf, (size_t)n);
            c->consumed[n] = '\0';
            c->submitted = 1;
        } else {
            c->consumed[0] = '\0';
        }
        return 1;
    }

    if (browse_up) {
        if (c->history_count == 0) return 0;
        if (c->history_pos == -1) {
            _flux_chat_save_draft(c);
            c->history_pos = c->history_count - 1;
        } else if (c->history_pos > 0) {
            c->history_pos--;
        } else {
            return 0;
        }
        _flux_chat_load(c, c->history[c->history_pos]);
        return 1;
    }
    if (browse_down) {
        if (c->history_pos == -1) return 0;
        if (c->history_pos < c->history_count - 1) {
            c->history_pos++;
            _flux_chat_load(c, c->history[c->history_pos]);
        } else {
            c->history_pos = -1;
            _flux_chat_load(c, c->draft);
        }
        return 1;
    }

    /* Forward to textarea. Treat its `submitted` flag as same. */
    {
        int changed = flux_textarea_update(&c->ta, msg);
        if (c->ta.submitted) {
            c->ta.submitted = 0;
            if (c->ta.len > 0) {
                int n = c->ta.len;
                if (n > FLUX_INPUT_MAX) n = FLUX_INPUT_MAX;
                memcpy(c->consumed, c->ta.buf, (size_t)n);
                c->consumed[n] = '\0';
                c->submitted = 1;
            }
            return 1;
        }
        return changed;
    }
}

void flux_chat_render(FluxChatInput *c, FluxSB *sb, int width, int max_rows) {
    char       inner[8192];
    FluxSB     ib;
    const char *prompt, *prompt_fg;
    int        i, plen, prompt_w, inner_w;
    int        first_seen = 0;

    if (!c || !sb || width <= 0 || max_rows <= 0) return;
    prompt    = c->prompt    ? c->prompt    : "> ";
    prompt_fg = c->prompt_fg ? c->prompt_fg : FLUX_THEME_ACCENT_FG;
    prompt_w  = flux_strwidth(prompt);
    inner_w   = width - prompt_w;
    if (inner_w < 1) { prompt_w = 0; inner_w = width; }

    flux_sb_init(&ib, inner, (int)sizeof inner);
    flux_textarea_render(&c->ta, &ib, inner_w, max_rows);

    /* ib now contains max_rows newline-terminated rows. Walk rows and
     * prefix the first one with the prompt; pad-prefix subsequent rows
     * with `prompt_w` spaces. Each emitted line stays width-aligned. */
    {
        const char *p = flux_sb_str(&ib);
        const char *line_start = p;
        for (; *p; p++) {
            if (*p == '\n') {
                /* emit prefix */
                if (!first_seen) {
                    if (prompt_w > 0) {
                        flux_sb_append(sb, prompt_fg);
                        flux_sb_append(sb, prompt);
                        flux_sb_append(sb, FLUX_RESET);
                    }
                    first_seen = 1;
                } else {
                    for (i = 0; i < prompt_w; i++) flux_sb_append(sb, " ");
                }
                /* copy slice [line_start..p) then nl */
                {
                    int  n = (int)(p - line_start);
                    char tmp[8192];
                    if (n > (int)sizeof tmp - 1) n = (int)sizeof tmp - 1;
                    memcpy(tmp, line_start, (size_t)n);
                    tmp[n] = '\0';
                    flux_sb_append(sb, tmp);
                }
                flux_sb_append(sb, "\n");
                plen = 0; (void)plen;
                line_start = p + 1;
            }
        }
    }
}

const char *flux_chat_consume(FluxChatInput *c) {
    if (!c || !c->submitted) return NULL;
    /* Push to history (avoid dup with most recent). */
    if (c->consumed[0] != '\0') {
        int dup = (c->history_count > 0
                   && strcmp(c->history[c->history_count - 1], c->consumed) == 0);
        if (!dup) {
            if (c->history_count < FLUX_CHAT_HISTORY_MAX) {
                int n = (int)strlen(c->consumed);
                if (n > FLUX_INPUT_MAX) n = FLUX_INPUT_MAX;
                memcpy(c->history[c->history_count], c->consumed, (size_t)n);
                c->history[c->history_count][n] = '\0';
                c->history_count++;
            } else {
                int i;
                for (i = 1; i < FLUX_CHAT_HISTORY_MAX; i++) {
                    memcpy(c->history[i - 1], c->history[i],
                           sizeof c->history[0]);
                }
                {
                    int n = (int)strlen(c->consumed);
                    if (n > FLUX_INPUT_MAX) n = FLUX_INPUT_MAX;
                    memcpy(c->history[FLUX_CHAT_HISTORY_MAX - 1],
                           c->consumed, (size_t)n);
                    c->history[FLUX_CHAT_HISTORY_MAX - 1][n] = '\0';
                }
            }
        }
    }
    c->history_pos = -1;
    c->draft[0]    = '\0';
    flux_textarea_clear(&c->ta);
    c->submitted = 0;
    return c->consumed;
}

/* ════════════════════════════════════════════════════════════════════
 * 9. FluxStepper
 * ════════════════════════════════════════════════════════════════════ */

void flux_stepper_init(FluxStepper *s, long initial, long min, long max, long step) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->min       = min;
    s->max       = max;
    s->step      = step > 0 ? step : 1;
    s->fast_step = 10 * s->step;
    s->fmt       = "%ld";
    s->focused   = 1;
    s->value     = initial;
    if (s->value < s->min) s->value = s->min;
    if (s->value > s->max) s->value = s->max;
}

static int _flux_stepper_apply(FluxStepper *s, long delta) {
    long nv = s->value + delta;
    if (delta > 0 && nv < s->value) nv = s->max;       /* overflow */
    if (delta < 0 && nv > s->value) nv = s->min;       /* underflow */
    if (nv < s->min) nv = s->min;
    if (nv > s->max) nv = s->max;
    if (nv == s->value) return 0;
    s->value = nv;
    return 1;
}

int flux_stepper_update(FluxStepper *s, FluxMsg msg) {
    long prev;
    if (!s || msg.type != MSG_KEY) return 0;
    prev = s->value;

    if (flux_key_is(msg, "right") || flux_key_is(msg, "up")
        || flux_key_is(msg, "+") || flux_key_is(msg, "=")) {
        return _flux_stepper_apply(s, s->step);
    }
    if (flux_key_is(msg, "left") || flux_key_is(msg, "down")
        || flux_key_is(msg, "-")) {
        return _flux_stepper_apply(s, -s->step);
    }
    if (flux_key_is(msg, "pgup")) {
        return _flux_stepper_apply(s, s->fast_step ? s->fast_step : 10);
    }
    if (flux_key_is(msg, "pgdown") || flux_key_is(msg, "pgdn")) {
        return _flux_stepper_apply(s, -(s->fast_step ? s->fast_step : 10));
    }
    if (flux_key_is(msg, "home")) {
        if (s->value == s->min) return 0;
        s->value = s->min; return 1;
    }
    if (flux_key_is(msg, "end")) {
        if (s->value == s->max) return 0;
        s->value = s->max; return 1;
    }
    /* Digit append (overwrite-mode-append): build new value = prev*10+d
     * if positive, else snap to digit. */
    if (msg.u.key.rune >= '0' && msg.u.key.rune <= '9') {
        long d = msg.u.key.rune - '0';
        long nv;
        if (s->value < 0) nv = -d;
        else {
            nv = s->value * 10 + d;
            if (nv / 10 != s->value) nv = s->max;  /* overflow guard */
        }
        if (nv < s->min) nv = s->min;
        if (nv > s->max) nv = s->max;
        if (nv == s->value) return 0;
        s->value = nv;
        return 1;
    }
    (void)prev;
    return 0;
}

void flux_stepper_render(FluxStepper *s, FluxSB *sb, int width) {
    char       row[1024];
    FluxSB     r;
    const char *btn_fg, *value_fg, *dis_fg;
    const char *minus_fg, *plus_fg;
    char       value_text[64];

    if (!s || !sb || width <= 0) return;
    btn_fg   = s->btn_fg      ? s->btn_fg      : FLUX_THEME_ACCENT_FG;
    value_fg = s->value_fg    ? s->value_fg    : FLUX_THEME_TEXT_FG;
    dis_fg   = s->disabled_fg ? s->disabled_fg : FLUX_THEME_TEXT_DIM_FG;
    minus_fg = (s->value <= s->min) ? dis_fg : btn_fg;
    plus_fg  = (s->value >= s->max) ? dis_fg : btn_fg;

    snprintf(value_text, sizeof value_text,
             s->fmt ? s->fmt : "%ld", s->value);

    flux_sb_init(&r, row, (int)sizeof row);
    if (s->label) {
        flux_sb_append(&r, value_fg);
        flux_sb_append(&r, s->label);
        flux_sb_append(&r, FLUX_RESET);
        flux_sb_append(&r, " ");
    }
    flux_sb_append(&r, minus_fg);
    flux_sb_append(&r, "[ - ]");
    flux_sb_append(&r, FLUX_RESET);
    flux_sb_append(&r, " ");
    flux_sb_append(&r, value_fg);
    if (s->focused) flux_sb_append(&r, FLUX_BOLD);
    flux_sb_append(&r, value_text);
    flux_sb_append(&r, FLUX_RESET);
    flux_sb_append(&r, " ");
    flux_sb_append(&r, plus_fg);
    flux_sb_append(&r, "[ + ]");
    flux_sb_append(&r, FLUX_RESET);

    flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);
}

/* ════════════════════════════════════════════════════════════════════
 * 10. FluxForm
 * ════════════════════════════════════════════════════════════════════ */

static int _flux_form_field_rows(const FluxFormField *f) {
    int r = f->rows;
    if (r > 0) return r;
    switch (f->kind) {
    case FLUX_FIELD_TEXTAREA: return 4;
    case FLUX_FIELD_RADIO: {
        FluxRadio *rd = (FluxRadio *)f->widget;
        return rd ? rd->count : 1;
    }
    case FLUX_FIELD_SELECT: {
        FluxSelect *sl = (FluxSelect *)f->widget;
        return sl ? flux_select_height(sl) : 1;
    }
    default: return 1;
    }
}

static int _flux_form_dispatch_update(FluxFormField *f, FluxMsg msg) {
    if (!f || !f->widget) return 0;
    switch (f->kind) {
    case FLUX_FIELD_INPUT:    return flux_input_update   ((FluxInput    *)f->widget, msg);
    case FLUX_FIELD_TEXTAREA: return flux_textarea_update((FluxTextArea *)f->widget, msg);
    case FLUX_FIELD_MASKED:   return flux_masked_update  ((FluxMaskedInput*)f->widget, msg);
    case FLUX_FIELD_CHECKBOX: return flux_checkbox_update((FluxCheckbox *)f->widget, msg);
    case FLUX_FIELD_SWITCH:   return flux_switch_update  ((FluxSwitch   *)f->widget, msg);
    case FLUX_FIELD_RADIO:    return flux_radio_update   ((FluxRadio    *)f->widget, msg);
    case FLUX_FIELD_SELECT:   return flux_select_update  ((FluxSelect   *)f->widget, msg);
    case FLUX_FIELD_STEPPER:  return flux_stepper_update ((FluxStepper  *)f->widget, msg);
    }
    return 0;
}

static void _flux_form_dispatch_render(FluxFormField *f, FluxSB *sb, int width) {
    if (!f || !f->widget) return;
    switch (f->kind) {
    case FLUX_FIELD_INPUT:
        flux_input_render((FluxInput *)f->widget, sb, width, NULL, NULL);
        flux_sb_nl(sb);
        break;
    case FLUX_FIELD_TEXTAREA:
        flux_textarea_render((FluxTextArea *)f->widget, sb, width,
                             _flux_form_field_rows(f));
        break;
    case FLUX_FIELD_MASKED:   flux_masked_render  ((FluxMaskedInput*)f->widget, sb, width); break;
    case FLUX_FIELD_CHECKBOX: flux_checkbox_render((FluxCheckbox *)f->widget, sb, width); break;
    case FLUX_FIELD_SWITCH:   flux_switch_render  ((FluxSwitch   *)f->widget, sb, width); break;
    case FLUX_FIELD_RADIO:    flux_radio_render   ((FluxRadio    *)f->widget, sb, width); break;
    case FLUX_FIELD_SELECT:   flux_select_render  ((FluxSelect   *)f->widget, sb, width); break;
    case FLUX_FIELD_STEPPER:  flux_stepper_render ((FluxStepper  *)f->widget, sb, width); break;
    }
}

void flux_form_init(FluxForm *f, const char *title) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->title        = title;
    f->submit_label = "Submit";
    f->cancel_label = "Cancel";
    f->focus        = 0;
}

int flux_form_add(FluxForm *f, const FluxFormField *field) {
    if (!f || !field || f->field_count >= FLUX_FORM_FIELDS_MAX) return 0;
    f->fields[f->field_count++] = *field;
    return 1;
}

int flux_form_validate(FluxForm *f) {
    int i;
    int ok = 1;
    if (!f) return 0;
    for (i = 0; i < f->field_count; i++) {
        FluxFormField *fld = &f->fields[i];
        const char *err = NULL;
        if (fld->validate) err = fld->validate(fld->widget, fld->vctx);
        fld->error = err;
        if (err) ok = 0;
    }
    return ok;
}

static int _flux_form_first_invalid(const FluxForm *f) {
    int i;
    for (i = 0; i < f->field_count; i++) {
        if (f->fields[i].error) return i;
    }
    return -1;
}

int flux_form_update(FluxForm *f, FluxMsg msg) {
    int i;
    if (!f) return 0;

    if (msg.type == MSG_KEY) {
        if (flux_key_is(msg, "esc")) {
            f->cancelled = 1;
            return 1;
        }
        if (flux_key_is(msg, "tab")) {
            f->focus++;
            if (f->focus >= f->field_count) f->focus = -1;
            return 1;
        }
        if (flux_key_is(msg, "shift+tab")) {
            f->focus--;
            if (f->focus < -1) f->focus = f->field_count - 1;
            return 1;
        }
        if (f->focus == -1) {
            if (flux_key_is(msg, "left") || flux_key_is(msg, "right")) {
                f->cancel_focused = !f->cancel_focused;
                return 1;
            }
            if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")
                || flux_key_is(msg, " ")) {
                if (f->cancel_focused) {
                    f->cancelled = 1;
                } else {
                    if (flux_form_validate(f)) {
                        f->submitted = 1;
                    } else {
                        int bad = _flux_form_first_invalid(f);
                        if (bad >= 0) f->focus = bad;
                    }
                }
                return 1;
            }
        }
    }

    if (f->focus >= 0 && f->focus < f->field_count) {
        FluxFormField *fld = &f->fields[f->focus];
        int r = _flux_form_dispatch_update(fld, msg);
        if (r) fld->error = NULL;     /* clear stale validation on edit */
        return r;
    }

    /* Unhandled */
    (void)i;
    return 0;
}

void flux_form_render(FluxForm *f, FluxSB *sb, int width) {
    int  i;
    char buf[8192];
    FluxSB sub;
    const char *label_fg, *error_fg;
    int inner_w;

    if (!f || !sb || width <= 0) return;

    label_fg  = f->label_fg  ? f->label_fg  : FLUX_THEME_TEXT_DIM_FG;
    error_fg  = f->error_fg  ? f->error_fg  : FLUX_THEME_ERR_FG;
    (void)f->border_fg;  /* reserved for future bordered variant */
    inner_w   = width - 2;
    if (inner_w < 1) inner_w = 1;

    /* Title row (optional) */
    if (f->title) {
        char       row[1024];
        FluxSB     r;
        flux_sb_init(&r, row, (int)sizeof row);
        flux_sb_append(&r, FLUX_BOLD);
        flux_sb_append(&r, label_fg);
        flux_sb_append(&r, f->title);
        flux_sb_append(&r, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }

    for (i = 0; i < f->field_count; i++) {
        FluxFormField *fld = &f->fields[i];
        int active = (i == f->focus);
        int rows;

        /* Label row */
        {
            char       row[1024];
            FluxSB     r;
            flux_sb_init(&r, row, (int)sizeof row);
            flux_sb_append(&r, active ? FLUX_THEME_ACCENT_FG : label_fg);
            if (active) flux_sb_append(&r, FLUX_BOLD);
            if (fld->label) flux_sb_append(&r, fld->label);
            flux_sb_append(&r, FLUX_RESET);
            flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
        }

        /* Field rows: render into sub-SB at width=inner_w then re-emit
         * each line with a 1-cell left/right gutter (we use spaces). */
        rows = _flux_form_field_rows(fld);
        flux_sb_init(&sub, buf, (int)sizeof buf);
        _flux_form_dispatch_render(fld, &sub, inner_w);
        {
            const char *p = flux_sb_str(&sub);
            const char *line_start = p;
            int rendered_rows = 0;
            for (; *p; p++) {
                if (*p == '\n') {
                    int  n = (int)(p - line_start);
                    char tmp[8192];
                    if (n > (int)sizeof tmp - 1) n = (int)sizeof tmp - 1;
                    memcpy(tmp, line_start, (size_t)n);
                    tmp[n] = '\0';
                    flux_sb_append(sb, " ");
                    flux_sb_append(sb, tmp);
                    flux_sb_append(sb, " ");
                    flux_sb_append(sb, "\n");
                    rendered_rows++;
                    line_start = p + 1;
                }
            }
            /* Pad if widget under-emitted */
            while (rendered_rows < rows) {
                flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
                flux_sb_nl(sb);
                rendered_rows++;
            }
        }

        /* Error row (optional) */
        if (fld->error) {
            char       row[1024];
            FluxSB     r;
            flux_sb_init(&r, row, (int)sizeof row);
            flux_sb_append(&r, "  ");
            flux_sb_append(&r, error_fg);
            flux_sb_append(&r, fld->error);
            flux_sb_append(&r, FLUX_RESET);
            flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
        }
    }

    /* Gap row */
    flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);

    /* Action row: right-aligned [ Submit ] [ Cancel ] */
    {
        char       row[1024];
        FluxSB     r;
        const char *submit_label = f->submit_label ? f->submit_label : "Submit";
        const char *cancel_label = f->cancel_label ? f->cancel_label : "Cancel";
        int submit_focus = (f->focus == -1) && !f->cancel_focused;
        int cancel_focus = (f->focus == -1) &&  f->cancel_focused;

        flux_sb_init(&r, row, (int)sizeof row);
        if (submit_focus) {
            flux_sb_append(&r, FLUX_THEME_BUTTON_OK_BG);
            flux_sb_append(&r, FLUX_BOLD);
            flux_sb_append(&r, FLUX_THEME_ACCENT_FG);
        } else {
            flux_sb_append(&r, FLUX_THEME_TEXT_DIM_FG);
        }
        flux_sb_append(&r, "[ ");
        flux_sb_append(&r, submit_label);
        flux_sb_append(&r, " ]");
        flux_sb_append(&r, FLUX_RESET);
        flux_sb_append(&r, " ");
        if (cancel_focus) {
            flux_sb_append(&r, FLUX_THEME_BUTTON_NO_BG);
            flux_sb_append(&r, FLUX_BOLD);
            flux_sb_append(&r, FLUX_THEME_ERR_FG);
        } else {
            flux_sb_append(&r, FLUX_THEME_TEXT_DIM_FG);
        }
        flux_sb_append(&r, "[ ");
        flux_sb_append(&r, cancel_label);
        flux_sb_append(&r, " ]");
        flux_sb_append(&r, FLUX_RESET);

        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_RIGHT);
        flux_sb_nl(sb);
    }
}

/* ─── B3_status_display impl ────────────────────────────────── */
/* Internal: kind → colour string. */
static const char *_flux_kind_clr(FluxKind k) {
    switch (k) {
        case FLUX_KIND_SUCCESS: return FLUX_THEME_OK_FG;
        case FLUX_KIND_WARNING: return FLUX_THEME_WARN_FG;
        case FLUX_KIND_ERROR:   return FLUX_THEME_ERR_FG;
        case FLUX_KIND_INFO:
        default:                return FLUX_THEME_ACCENT_FG;
    }
}

/* Internal: kind → glyph for alert. */
static const char *_flux_kind_alert_glyph(FluxKind k) {
    switch (k) {
        case FLUX_KIND_SUCCESS: return "\xe2\x9c\x93"; /* ✓ */
        case FLUX_KIND_WARNING: return "\xe2\x9a\xa0"; /* ⚠ */
        case FLUX_KIND_ERROR:   return "\xe2\x9c\x97"; /* ✗ */
        case FLUX_KIND_INFO:
        default:                return "\xe2\x84\xb9"; /* ℹ */
    }
}

/* Internal: kind → glyph for toast (distinct visual). */
static const char *_flux_kind_toast_glyph(FluxKind k) {
    switch (k) {
        case FLUX_KIND_SUCCESS: return "\xe2\x9c\x94"; /* ✔ */
        case FLUX_KIND_WARNING: return "\xe2\x96\xb2"; /* ▲ */
        case FLUX_KIND_ERROR:   return "\xe2\x9c\x98"; /* ✘ */
        case FLUX_KIND_INFO:
        default:                return "\xe2\x97\x86"; /* ◆ */
    }
}

/* Internal: dot for status_msg. */
#define _FLUX_STATUS_DOT "\xe2\x97\x8f" /* ● */

/* Internal: rounded box glyphs (UTF-8). */
#define _FLUX_BOX_TL "\xe2\x95\xad" /* ╭ */
#define _FLUX_BOX_TR "\xe2\x95\xae" /* ╮ */
#define _FLUX_BOX_BL "\xe2\x95\xb0" /* ╰ */
#define _FLUX_BOX_BR "\xe2\x95\xaf" /* ╯ */
#define _FLUX_BOX_H  "\xe2\x94\x80" /* ─ */
#define _FLUX_BOX_V  "\xe2\x94\x82" /* │ */

/* ── 1. flux_alert ────────────────────────────────────────────────── */

void flux_alert(FluxSB *sb, FluxKind kind,
                const char *title,
                const char *message,
                int width) {
    const char *border_clr = _flux_kind_clr(kind);
    const char *glyph      = _flux_kind_alert_glyph(kind);
    int inner_w;
    int i;

    if (!sb || width <= 0) return;
    if (width < 6) width = 6;
    inner_w = width - 4; /* │ space ... space │ */
    if (inner_w < 1) inner_w = 1;

    /* Top border: ╭───...───╮ totalling `width` cells. */
    flux_sb_append(sb, border_clr);
    flux_sb_append(sb, _FLUX_BOX_TL);
    for (i = 0; i < width - 2; i++) flux_sb_append(sb, _FLUX_BOX_H);
    flux_sb_append(sb, _FLUX_BOX_TR);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    /* Title row: │ {glyph} {title}{pad} │ — title bold in border colour. */
    if (title) {
        char head[256];
        /* Compose "{glyph} {title}" once so flux_fit handles truncation. */
        snprintf(head, sizeof head, "%s %s", glyph, title);
        flux_sb_append(sb, border_clr);
        flux_sb_append(sb, _FLUX_BOX_V);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_append(sb, " ");
        flux_sb_append(sb, border_clr);
        flux_sb_append(sb, FLUX_BOLD);
        flux_fit(sb, head, inner_w, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_append(sb, " ");
        flux_sb_append(sb, border_clr);
        flux_sb_append(sb, _FLUX_BOX_V);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Body rows — wrap on '\n' OR every inner_w cells. */
    {
        const char *p = message ? message : "";
        char chunk[1024];
        while (*p) {
            const char *nl = strchr(p, '\n');
            int line_len = nl ? (int)(nl - p) : (int)strlen(p);
            int cur_w;
            int copy;

            /* If line fits, emit and advance. */
            if (line_len < (int)sizeof chunk) {
                memcpy(chunk, p, (size_t)line_len);
                chunk[line_len] = '\0';
                cur_w = flux_strwidth(chunk);
            } else {
                memcpy(chunk, p, sizeof chunk - 1);
                chunk[sizeof chunk - 1] = '\0';
                cur_w = flux_strwidth(chunk);
            }

            if (cur_w <= inner_w) {
                /* Whole line fits */
                flux_sb_append(sb, border_clr);
                flux_sb_append(sb, _FLUX_BOX_V);
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_THEME_TEXT_FG);
                flux_fit(sb, chunk, inner_w, NULL, FLUX_ALIGN_LEFT);
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_append(sb, " ");
                flux_sb_append(sb, border_clr);
                flux_sb_append(sb, _FLUX_BOX_V);
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_nl(sb);
                if (nl) { p = nl + 1; continue; }
                break;
            }

            /* Need to chop at inner_w cells. flux_fit truncates with "…",
             * but for body wrap we want to split, not ellipsise. So we
             * walk bytes counting cells until we reach inner_w. */
            {
                int w = 0;
                int j = 0;
                int last_safe = 0;
                while (j < line_len) {
                    /* simple UTF-8 char width via flux_strwidth on a
                     * 1-codepoint substring */
                    char one[8];
                    int b = 1;
                    unsigned char c0 = (unsigned char)p[j];
                    if      ((c0 & 0x80) == 0x00) b = 1;
                    else if ((c0 & 0xE0) == 0xC0) b = 2;
                    else if ((c0 & 0xF0) == 0xE0) b = 3;
                    else if ((c0 & 0xF8) == 0xF0) b = 4;
                    if (b > line_len - j) break;
                    if (b >= (int)sizeof one) break;
                    memcpy(one, p + j, (size_t)b);
                    one[b] = '\0';
                    {
                        int cw = flux_strwidth(one);
                        if (w + cw > inner_w) break;
                        w += cw;
                        j += b;
                        last_safe = j;
                    }
                }
                if (last_safe == 0) last_safe = line_len; /* defensive */
                copy = last_safe;
                if (copy >= (int)sizeof chunk) copy = (int)sizeof chunk - 1;
                memcpy(chunk, p, (size_t)copy);
                chunk[copy] = '\0';
            }

            flux_sb_append(sb, border_clr);
            flux_sb_append(sb, _FLUX_BOX_V);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, " ");
            flux_sb_append(sb, FLUX_THEME_TEXT_FG);
            flux_fit(sb, chunk, inner_w, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, " ");
            flux_sb_append(sb, border_clr);
            flux_sb_append(sb, _FLUX_BOX_V);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);

            p += copy;
        }
    }

    /* Bottom border. */
    flux_sb_append(sb, border_clr);
    flux_sb_append(sb, _FLUX_BOX_BL);
    for (i = 0; i < width - 2; i++) flux_sb_append(sb, _FLUX_BOX_H);
    flux_sb_append(sb, _FLUX_BOX_BR);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 2. flux_badge ────────────────────────────────────────────────── */

void flux_badge(FluxSB *sb, const char *text,
                const char *fg_clr,
                const char *bg_clr) {
    if (!sb) return;
    if (!text) text = "";
    if (!fg_clr) fg_clr = FLUX_THEME_TEXT_INV_FG;
    if (!bg_clr) bg_clr = FLUX_THEME_ACCENT_FG;
    /* Convert FG colour to a BG sequence by patching the SGR prefix.
     * We can't generally do that, so accept that bg_clr must already be
     * a background SGR, OR rely on a heuristic: if it starts with
     * "\x1b[38;" we synthesise the matching "\x1b[48;" version. */
    if (strncmp(bg_clr, "\x1b[38;", 5) == 0) {
        char buf[64];
        snprintf(buf, sizeof buf, "\x1b[48;%s", bg_clr + 5);
        flux_sb_append(sb, buf);
    } else {
        flux_sb_append(sb, bg_clr);
    }
    flux_sb_append(sb, fg_clr);
    flux_sb_append(sb, " ");
    flux_sb_append(sb, text);
    flux_sb_append(sb, " ");
    flux_sb_append(sb, FLUX_RESET);
}

/* ── 3. flux_tag ──────────────────────────────────────────────────── */

void flux_tag(FluxSB *sb, const char *text, const char *fg_clr) {
    if (!sb) return;
    if (!text) text = "";
    if (!fg_clr) fg_clr = FLUX_THEME_ACCENT_FG;
    flux_sb_append(sb, fg_clr);
    flux_sb_append(sb, "[");
    flux_sb_append(sb, text);
    flux_sb_append(sb, "]");
    flux_sb_append(sb, FLUX_RESET);
}

/* ── 4. FluxToast ─────────────────────────────────────────────────── */

void flux_toast_init(FluxToast *t, FluxKind kind,
                     const char *message, long ttl_ms) {
    if (!t) return;
    t->kind        = kind;
    t->message     = message;
    t->ttl_ms      = (ttl_ms == 0) ? 3000 : ttl_ms;
    t->shown_at_ms = 0;
    t->visible     = 0;
}

void flux_toast_show(FluxToast *t, long now_ms) {
    if (!t) return;
    t->shown_at_ms = now_ms;
    t->visible     = 1;
}

void flux_toast_tick(FluxToast *t, long now_ms) {
    if (!t) return;
    if (!t->visible) return;
    if (t->ttl_ms <= 0) return; /* sticky */
    if (now_ms - t->shown_at_ms >= t->ttl_ms) t->visible = 0;
}

int flux_toast_visible(const FluxToast *t) {
    if (!t) return 0;
    return t->visible ? 1 : 0;
}

void flux_toast_render(const FluxToast *t, FluxSB *sb, int width) {
    char body[512];
    const char *clr;
    const char *glyph;

    if (!sb || !t || width <= 0) return;
    if (!t->visible) return;

    clr   = _flux_kind_clr(t->kind);
    glyph = _flux_kind_toast_glyph(t->kind);

    /* Compose " {glyph} {message}" then fit to `width` over panel bg. */
    snprintf(body, sizeof body, " %s%s%s%s %s",
             FLUX_BOLD, clr, glyph, FLUX_RESET,
             t->message ? t->message : "");

    flux_sb_append(sb, FLUX_THEME_PANEL_BG);
    flux_sb_append(sb, FLUX_THEME_TEXT_FG);
    flux_fit(sb, body, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 5. flux_tooltip ──────────────────────────────────────────────── */

void flux_tooltip(FluxSB *sb, const char *body,
                  int x, int y, int width) {
    const char *clr = FLUX_THEME_BORDER_FG;
    int inner_w;
    int arrow_up;
    int i;

    if (!sb || width <= 0) return;
    (void)x;
    if (!body) body = "";
    if (width < 6) width = 6;
    inner_w = width - 4;
    if (inner_w < 1) inner_w = 1;

    /* Bubble height is 3 (top, body, bottom). If anchor row is above
     * the height, flip arrow to ▲ (bubble below anchor). */
    arrow_up = (y < 3) ? 1 : 0;

    /* Top border (with arrow if pointing up). */
    flux_sb_append(sb, clr);
    flux_sb_append(sb, _FLUX_BOX_TL);
    if (arrow_up) {
        int half = (width - 2) / 2;
        for (i = 0; i < half; i++) flux_sb_append(sb, _FLUX_BOX_H);
        flux_sb_append(sb, "\xe2\x96\xb2"); /* ▲ */
        for (i = 0; i < (width - 2) - half - 1; i++)
            flux_sb_append(sb, _FLUX_BOX_H);
    } else {
        for (i = 0; i < width - 2; i++) flux_sb_append(sb, _FLUX_BOX_H);
    }
    flux_sb_append(sb, _FLUX_BOX_TR);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    /* Body row. */
    flux_sb_append(sb, clr);
    flux_sb_append(sb, _FLUX_BOX_V);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_append(sb, " ");
    flux_sb_append(sb, FLUX_THEME_TEXT_FG);
    flux_fit(sb, body, inner_w, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_append(sb, " ");
    flux_sb_append(sb, clr);
    flux_sb_append(sb, _FLUX_BOX_V);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    /* Bottom border (with arrow if pointing down). NO trailing newline. */
    flux_sb_append(sb, clr);
    flux_sb_append(sb, _FLUX_BOX_BL);
    if (!arrow_up) {
        int half = (width - 2) / 2;
        for (i = 0; i < half; i++) flux_sb_append(sb, _FLUX_BOX_H);
        flux_sb_append(sb, "\xe2\x96\xbc"); /* ▼ */
        for (i = 0; i < (width - 2) - half - 1; i++)
            flux_sb_append(sb, _FLUX_BOX_H);
    } else {
        for (i = 0; i < width - 2; i++) flux_sb_append(sb, _FLUX_BOX_H);
    }
    flux_sb_append(sb, _FLUX_BOX_BR);
    flux_sb_append(sb, FLUX_RESET);
}

/* ── 6. flux_header ───────────────────────────────────────────────── */

void flux_header(FluxSB *sb, const char *title,
                 const char *right_text, int width) {
    int right_w;
    int title_budget;
    int i;

    if (!sb || width <= 0) return;
    if (!title) title = "";
    right_w = right_text ? flux_strwidth(right_text) : 0;

    /* Row 1: " {title} {pad} {right_text} " — total `width` cells.
     * Layout: 1 leading space + title + pad + right + 1 trailing space. */
    title_budget = width - 2 - right_w - (right_text ? 1 : 0);
    if (title_budget < 1) {
        title_budget = 1;
        right_w = 0;
        right_text = NULL;
    }

    flux_sb_append(sb, " ");
    flux_sb_append(sb, FLUX_BOLD);
    flux_sb_append(sb, FLUX_THEME_TEXT_FG);
    flux_fit(sb, title, title_budget, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);

    if (right_text) {
        flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(sb, right_text);
        flux_sb_append(sb, FLUX_RESET);
    } else {
        /* Pad to fill width - already handled by flux_fit + leading space. */
    }
    flux_sb_append(sb, " ");
    flux_sb_nl(sb);

    /* Row 2: divider rule. */
    flux_sb_append(sb, FLUX_THEME_DIVIDER_FG);
    for (i = 0; i < width; i++) flux_sb_append(sb, _FLUX_BOX_H);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 7. flux_footer ───────────────────────────────────────────────── */

void flux_footer(FluxSB *sb, const FluxKeyHint *hints, int n, int width) {
    char line[1024];
    int len = 0;
    int i;

    if (!sb || width <= 0) return;

    /* Row 1: rule. */
    flux_sb_append(sb, FLUX_THEME_DIVIDER_FG);
    for (i = 0; i < width; i++) flux_sb_append(sb, _FLUX_BOX_H);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    /* Row 2: " {key} {label} · {key} {label} · ... " padded to width.
     * We progressively add hints while the visible width stays within
     * `width - 2` (1 leading + 1 trailing space). On overflow, we stop —
     * later hints are dropped. */
    line[0] = '\0';
    {
        int budget = width - 2;
        int cur_w = 0;
        for (i = 0; i < n && hints; i++) {
            char chunk[256];
            int chunk_visible_w;
            int chunk_len;
            const char *key   = hints[i].key   ? hints[i].key   : "";
            const char *label = hints[i].label ? hints[i].label : "";

            /* Visible: "<key> <label>" plus optional " · " separator. */
            if (cur_w == 0) {
                chunk_visible_w = flux_strwidth(key) + 1 + flux_strwidth(label);
                chunk_len = snprintf(chunk, sizeof chunk,
                                     "%s%s%s %s%s%s",
                                     FLUX_THEME_ACCENT_FG, key, FLUX_RESET,
                                     FLUX_THEME_TEXT_DIM_FG, label, FLUX_RESET);
            } else {
                chunk_visible_w = 3 /* " · " */
                                + flux_strwidth(key) + 1 + flux_strwidth(label);
                chunk_len = snprintf(chunk, sizeof chunk,
                                     " \xc2\xb7 %s%s%s %s%s%s",
                                     FLUX_THEME_ACCENT_FG, key, FLUX_RESET,
                                     FLUX_THEME_TEXT_DIM_FG, label, FLUX_RESET);
            }
            if (cur_w + chunk_visible_w > budget) break;
            if (len + chunk_len >= (int)sizeof line) break;
            memcpy(line + len, chunk, (size_t)chunk_len);
            len += chunk_len;
            line[len] = '\0';
            cur_w += chunk_visible_w;
        }
        (void)cur_w;
    }

    flux_sb_append(sb, " ");
    flux_fit(sb, line, width - 2, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, " ");
    flux_sb_nl(sb);
}

/* ── 8. flux_avatar ───────────────────────────────────────────────── */

void flux_avatar(FluxSB *sb, const char *initials, const char *fg_clr) {
    if (!sb) return;
    if (!initials) initials = "";
    if (!fg_clr) fg_clr = FLUX_THEME_BRAND_PURPLE_FG;
    flux_sb_append(sb, fg_clr);
    flux_sb_append(sb, "(");
    flux_sb_append(sb, initials);
    flux_sb_append(sb, ")");
    flux_sb_append(sb, FLUX_RESET);
}

/* ── 9. flux_status_msg ───────────────────────────────────────────── */

void flux_status_msg(FluxSB *sb, FluxKind kind, const char *msg, int width) {
    char line[1024];
    const char *clr = _flux_kind_clr(kind);

    if (!sb || width <= 0) return;
    if (!msg) msg = "";

    /* " ●{reset} {msg} " — fit-pads to width. */
    snprintf(line, sizeof line, " %s%s%s %s",
             clr, _FLUX_STATUS_DOT, FLUX_RESET, msg);

    flux_sb_append(sb, FLUX_THEME_TEXT_FG);
    flux_fit(sb, line, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 10. flux_placeholder ─────────────────────────────────────────── */

void flux_placeholder(FluxSB *sb,
                      const char *icon,
                      const char *title,
                      const char *hint,
                      int width) {
    char buf[512];

    if (!sb || width <= 0) return;
    if (!icon) icon = "\xc2\xb7"; /* · */
    if (!title) return; /* required */

    /* Row 1 — icon (dim). */
    snprintf(buf, sizeof buf, "%s%s%s",
             FLUX_THEME_TEXT_DIM_FG, icon, FLUX_RESET);
    flux_fit(sb, buf, width, NULL, FLUX_ALIGN_CENTER);
    flux_sb_nl(sb);

    /* Row 2 — bold title. */
    snprintf(buf, sizeof buf, "%s%s%s%s",
             FLUX_BOLD, FLUX_THEME_TEXT_FG, title, FLUX_RESET);
    flux_fit(sb, buf, width, NULL, FLUX_ALIGN_CENTER);
    flux_sb_nl(sb);

    /* Row 3 — optional hint. */
    if (hint) {
        snprintf(buf, sizeof buf, "%s%s%s",
                 FLUX_THEME_TEXT_OFF_FG, hint, FLUX_RESET);
        flux_fit(sb, buf, width, NULL, FLUX_ALIGN_CENTER);
        flux_sb_nl(sb);
    }
}

/* ── 11. flux_link ────────────────────────────────────────────────── */

void flux_link(FluxSB *sb, const char *text, const char *url,
               const char *fg_clr) {
    if (!sb) return;
    if (!text) text = "";
    if (!url)  url  = "";
    if (!fg_clr) fg_clr = FLUX_THEME_ACCENT_FG;
    flux_sb_appendf(sb,
                    "%s\x1b[4m\x1b]8;;%s\x1b\\%s\x1b]8;;\x1b\\\x1b[0m",
                    fg_clr, url, text);
}

/* ─── B4_nav_modal impl ────────────────────────────────── */
#include <ctype.h>
#include <limits.h>

/* ── Internal: count newline-separated lines (>=1 if non-empty). ── */
static int _flux_b4_count_lines(const char *s) {
    int n = 1;
    if (!s || !*s) return 0;
    while (*s) {
        if (*s == '\n') n++;
        s++;
    }
    /* If string ends with '\n', the trailing empty line is real but the
     * terminator does not represent a separate row. */
    return n;
}

/* ── Internal: copy line `idx` from a '\n'-separated string into out. ── */
static void _flux_b4_get_line(const char *s, int idx, char *out, int cap) {
    int cur = 0, j = 0;
    if (cap <= 0) return;
    out[0] = '\0';
    if (!s) return;
    while (*s && cur < idx) {
        if (*s == '\n') cur++;
        s++;
    }
    while (*s && *s != '\n' && j < cap - 1) {
        out[j++] = *s++;
    }
    out[j] = '\0';
}

/* ── Internal: emit exactly `width` cells of pure spaces with a bg. ── */
static void _flux_b4_blank_row(FluxSB *sb, const char *bg, int width) {
    int i;
    if (width <= 0) { flux_sb_append(sb, "\n"); return; }
    if (bg) flux_sb_append(sb, bg);
    for (i = 0; i < width; i++) flux_sb_append(sb, " ");
    if (bg) flux_sb_append(sb, FLUX_RESET);
    flux_sb_append(sb, "\n");
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: flux_overlay
 * ═════════════════════════════════════════════════════════════════ */

void flux_overlay(FluxSB *sb, const char *color, int alpha_pct,
                  int w, int h) {
    int y;
    const char *bg;

    if (!sb || w <= 0 || h <= 0) return;

    if (color) {
        bg = color;
    } else if (alpha_pct <= 25) {
        /* nearly transparent — fall back to window bg */
        bg = FLUX_THEME_WINDOW_BG;
    } else if (alpha_pct <= 50) {
        bg = FLUX_THEME_TITLEBAR_BG;
    } else if (alpha_pct <= 75) {
        bg = FLUX_THEME_WINDOW_BG;
    } else {
        bg = "\x1b[48;2;0;0;0m";
    }

    for (y = 0; y < h; y++) {
        _flux_b4_blank_row(sb, bg, w);
    }
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: FluxModal
 * ═════════════════════════════════════════════════════════════════ */

static const int _FLUX_MODAL_W[4] = { 30, 50, 70, -1 /* full */ };

void flux_modal_init(FluxModal *m, const char *title, const char *body,
                     FluxModalSize sz) {
    if (!m) return;
    m->title         = title;
    m->body          = body;
    m->size          = sz;
    m->is_open       = 0;
    m->closed        = 0;
    m->border_fg     = NULL;
    m->panel_bg      = NULL;
    m->title_fg      = NULL;
    m->body_fg       = NULL;
    m->show_esc_hint = 1;
}

int flux_modal_update(FluxModal *m, FluxMsg msg) {
    if (!m || !m->is_open) return 0;
    if (msg.type != MSG_KEY) return 0;
    if (flux_key_is(msg, "esc")) {
        m->is_open = 0;
        m->closed  = 1;
        return 1;
    }
    /* Focus-trap intent: swallow Tab / Shift+Tab. */
    if (flux_key_is(msg, "tab") || flux_key_is(msg, "shift+tab")) {
        return 1;
    }
    return 0;
}

/* Helper: render a border-bounded "interior" row inside a modal box,
 * given x-position & widths. Caller passes the inside content already
 * fitted to inner cells. */
static void _flux_modal_box_row(FluxSB *sb,
                                const char *border_fg,
                                const char *panel_bg,
                                const char *backdrop_bg,
                                int box_x, int box_w, int screen_w,
                                const char *inside /*already inner_w cells*/) {
    int i;
    int left_pad  = box_x;
    int right_pad = screen_w - box_x - box_w;

    if (left_pad < 0)  left_pad = 0;
    if (right_pad < 0) right_pad = 0;

    if (left_pad > 0) {
        if (backdrop_bg) flux_sb_append(sb, backdrop_bg);
        for (i = 0; i < left_pad; i++) flux_sb_append(sb, " ");
        if (backdrop_bg) flux_sb_append(sb, FLUX_RESET);
    }
    /* left border */
    if (border_fg) flux_sb_append(sb, border_fg);
    if (panel_bg)  flux_sb_append(sb, panel_bg);
    flux_sb_append(sb, "\xe2\x94\x82"); /* │ */
    if (border_fg) flux_sb_append(sb, FLUX_RESET);
    /* inside */
    if (panel_bg) flux_sb_append(sb, panel_bg);
    flux_sb_append(sb, inside);
    if (panel_bg) flux_sb_append(sb, FLUX_RESET);
    /* right border */
    if (border_fg) flux_sb_append(sb, border_fg);
    if (panel_bg)  flux_sb_append(sb, panel_bg);
    flux_sb_append(sb, "\xe2\x94\x82");
    if (border_fg) flux_sb_append(sb, FLUX_RESET);
    if (panel_bg)  flux_sb_append(sb, FLUX_RESET);
    if (right_pad > 0) {
        if (backdrop_bg) flux_sb_append(sb, backdrop_bg);
        for (i = 0; i < right_pad; i++) flux_sb_append(sb, " ");
        if (backdrop_bg) flux_sb_append(sb, FLUX_RESET);
    }
    flux_sb_append(sb, "\n");
}

void flux_modal_render(FluxModal *m, FluxSB *sb,
                       int screen_w, int screen_h, int dim_alpha_pct) {
    int box_w, box_h, box_x, box_y;
    int inner_w;
    int body_lines;
    int title_rows, hint_rows;
    int i, y, row_in_box;
    const char *border_fg;
    const char *panel_bg;
    const char *title_fg;
    const char *body_fg;
    const char *backdrop_bg;

    if (!m || !sb || screen_w <= 0 || screen_h <= 0) return;
    if (!m->is_open) return;

    border_fg = m->border_fg ? m->border_fg : FLUX_THEME_BORDER_FG;
    panel_bg  = m->panel_bg  ? m->panel_bg  : FLUX_THEME_PANEL_BG;
    title_fg  = m->title_fg  ? m->title_fg  : FLUX_THEME_TEXT_FG;
    body_fg   = m->body_fg   ? m->body_fg   : FLUX_THEME_TEXT2_FG;

    /* pick backdrop preset */
    if (dim_alpha_pct <= 25)      backdrop_bg = FLUX_THEME_WINDOW_BG;
    else if (dim_alpha_pct <= 50) backdrop_bg = FLUX_THEME_TITLEBAR_BG;
    else if (dim_alpha_pct <= 75) backdrop_bg = FLUX_THEME_WINDOW_BG;
    else                          backdrop_bg = "\x1b[48;2;0;0;0m";

    /* dimensions */
    if (m->size == FLUX_MODAL_FULL) box_w = screen_w - 4;
    else                            box_w = _FLUX_MODAL_W[m->size];
    if (box_w > screen_w - 4) box_w = screen_w - 4;
    if (box_w < 12)           box_w = 12;
    if (box_w > screen_w)     box_w = screen_w;

    inner_w = box_w - 2;
    if (inner_w < 1) inner_w = 1;

    body_lines = _flux_b4_count_lines(m->body);
    title_rows = (m->title && m->title[0]) ? 2 : 0;  /* title + spacer */
    hint_rows  = m->show_esc_hint ? 2 : 0;            /* spacer + hint */

    box_h = 2 /* top + bottom border */
          + 2 /* top + bottom inner pad */
          + title_rows
          + body_lines
          + hint_rows;
    if (box_h > screen_h)    box_h = screen_h;
    if (box_h < 4)           box_h = 4;

    box_x = (screen_w - box_w) / 2;
    box_y = (screen_h - box_h) / 2;
    if (box_x < 0) box_x = 0;
    if (box_y < 0) box_y = 0;

    /* Walk every screen row. */
    for (y = 0; y < screen_h; y++) {
        if (y < box_y || y >= box_y + box_h) {
            _flux_b4_blank_row(sb, backdrop_bg, screen_w);
            continue;
        }
        row_in_box = y - box_y;

        /* top border row */
        if (row_in_box == 0) {
            int lp = box_x, rp = screen_w - box_x - box_w;
            if (lp < 0) lp = 0;
            if (rp < 0) rp = 0;
            if (lp > 0) {
                flux_sb_append(sb, backdrop_bg);
                for (i = 0; i < lp; i++) flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_RESET);
            }
            flux_sb_append(sb, border_fg);
            flux_sb_append(sb, panel_bg);
            flux_sb_append(sb, "\xe2\x95\xad"); /* ╭ */
            for (i = 0; i < inner_w; i++) flux_sb_append(sb, "\xe2\x94\x80"); /* ─ */
            flux_sb_append(sb, "\xe2\x95\xae"); /* ╮ */
            flux_sb_append(sb, FLUX_RESET);
            if (rp > 0) {
                flux_sb_append(sb, backdrop_bg);
                for (i = 0; i < rp; i++) flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_RESET);
            }
            flux_sb_append(sb, "\n");
            continue;
        }
        /* bottom border row */
        if (row_in_box == box_h - 1) {
            int lp = box_x, rp = screen_w - box_x - box_w;
            if (lp < 0) lp = 0;
            if (rp < 0) rp = 0;
            if (lp > 0) {
                flux_sb_append(sb, backdrop_bg);
                for (i = 0; i < lp; i++) flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_RESET);
            }
            flux_sb_append(sb, border_fg);
            flux_sb_append(sb, panel_bg);
            flux_sb_append(sb, "\xe2\x95\xb0"); /* ╰ */
            for (i = 0; i < inner_w; i++) flux_sb_append(sb, "\xe2\x94\x80");
            flux_sb_append(sb, "\xe2\x95\xaf"); /* ╯ */
            flux_sb_append(sb, FLUX_RESET);
            if (rp > 0) {
                flux_sb_append(sb, backdrop_bg);
                for (i = 0; i < rp; i++) flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_RESET);
            }
            flux_sb_append(sb, "\n");
            continue;
        }

        /* interior rows: build inside-content (inner_w cells), then the
         * helper draws the side borders + backdrop margins. */
        {
            char  inside_buf[4096];
            FluxSB ins;
            int    interior_idx = row_in_box - 1; /* 0-based among interior */
            int    title_block  = title_rows;     /* 0 or 2 */
            int    body_start   = 1 + title_block; /* skip top pad + title block */
            int    body_end     = body_start + body_lines;

            flux_sb_init(&ins, inside_buf, (int)sizeof inside_buf);

            if (interior_idx == 0) {
                /* top inner pad */
                flux_fit(&ins, "", inner_w, NULL, FLUX_ALIGN_LEFT);
            } else if (title_block && interior_idx == 1) {
                /* title row */
                char buf[1024];
                if (title_fg) flux_sb_append(&ins, title_fg);
                flux_sb_append(&ins, FLUX_BOLD);
                flux_truncate(m->title ? m->title : "",
                              inner_w - 2, "\xe2\x80\xa6",
                              buf, (int)sizeof buf);
                {
                    char tmp[1280];
                    int n = snprintf(tmp, sizeof tmp, " %s ", buf);
                    if (n < 0) n = 0;
                    flux_fit(&ins, tmp, inner_w, "\xe2\x80\xa6",
                             FLUX_ALIGN_LEFT);
                }
                if (title_fg) flux_sb_append(&ins, FLUX_RESET);
            } else if (title_block && interior_idx == 2) {
                /* spacer between title and body */
                flux_fit(&ins, "", inner_w, NULL, FLUX_ALIGN_LEFT);
            } else if (interior_idx >= body_start
                    && interior_idx <  body_end) {
                int   line_idx = interior_idx - body_start;
                char  line[2048];
                char  fitted[3072];
                FluxSB fb;
                _flux_b4_get_line(m->body, line_idx, line, (int)sizeof line);
                flux_sb_init(&fb, fitted, (int)sizeof fitted);
                if (body_fg) flux_sb_append(&fb, body_fg);
                {
                    char tmp[2200];
                    int n = snprintf(tmp, sizeof tmp, " %s", line);
                    if (n < 0) n = 0;
                    flux_fit(&fb, tmp, inner_w, "\xe2\x80\xa6",
                             FLUX_ALIGN_LEFT);
                }
                if (body_fg) flux_sb_append(&fb, FLUX_RESET);
                flux_sb_append(&ins, flux_sb_str(&fb));
            } else if (hint_rows
                    && interior_idx == body_end + 1
                    /* the row right before the bottom inner pad */) {
                char tmp[64];
                snprintf(tmp, sizeof tmp, " Esc to close ");
                if (title_fg) flux_sb_append(&ins, FLUX_THEME_TEXT_DIM_FG);
                flux_fit(&ins, tmp, inner_w, "\xe2\x80\xa6",
                         FLUX_ALIGN_RIGHT);
                if (title_fg) flux_sb_append(&ins, FLUX_RESET);
            } else {
                /* spacer / bottom inner pad */
                flux_fit(&ins, "", inner_w, NULL, FLUX_ALIGN_LEFT);
            }

            _flux_modal_box_row(sb, border_fg, panel_bg, backdrop_bg,
                                box_x, box_w, screen_w,
                                flux_sb_str(&ins));
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: FluxAccordion
 * ═════════════════════════════════════════════════════════════════ */

void flux_accordion_init(FluxAccordion *a, FluxAccordionSection *s,
                         int n, int exclusive) {
    int i;
    if (!a) return;
    if (n < 0) n = 0;
    if (n > FLUX_ACCORDION_MAX) n = FLUX_ACCORDION_MAX;
    a->count     = n;
    a->cursor    = 0;
    a->exclusive = exclusive ? 1 : 0;
    a->header_fg = a->active_fg = a->body_fg = a->marker_fg = NULL;
    for (i = 0; i < n; i++) {
        a->sections[i] = s[i];
        if (a->exclusive && a->sections[i].open) {
            int j;
            for (j = i + 1; j < n; j++) a->sections[j].open = 0;
        }
    }
}

int flux_accordion_update(FluxAccordion *a, FluxMsg msg) {
    if (!a || a->count <= 0) return 0;
    if (msg.type != MSG_KEY) return 0;

    if (flux_key_is(msg, "up") || flux_key_is(msg, "k")) {
        if (a->cursor > 0) { a->cursor--; return 1; }
        return 0;
    }
    if (flux_key_is(msg, "down") || flux_key_is(msg, "j")) {
        if (a->cursor < a->count - 1) { a->cursor++; return 1; }
        return 0;
    }
    if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")) {
        int cur = a->cursor;
        int i;
        if (cur < 0 || cur >= a->count) return 0;
        a->sections[cur].open = !a->sections[cur].open;
        if (a->exclusive && a->sections[cur].open) {
            for (i = 0; i < a->count; i++) {
                if (i != cur) a->sections[i].open = 0;
            }
        }
        return 1;
    }
    return 0;
}

void flux_accordion_render(FluxAccordion *a, FluxSB *sb, int width) {
    int i, k;
    const char *header_fg, *active_fg, *body_fg, *marker_fg;

    if (!a || !sb || width <= 0) return;

    header_fg = a->header_fg ? a->header_fg : FLUX_THEME_TEXT_FG;
    active_fg = a->active_fg ? a->active_fg : FLUX_THEME_ACCENT_FG;
    body_fg   = a->body_fg   ? a->body_fg   : FLUX_THEME_TEXT2_FG;
    marker_fg = a->marker_fg ? a->marker_fg : FLUX_THEME_TEXT_DIM_FG;

    for (i = 0; i < a->count; i++) {
        FluxAccordionSection *s = &a->sections[i];
        char buf[2048];
        const char *marker = s->open ? "\xe2\x96\xbc" : "\xe2\x96\xb6"; /* ▼ ▶ */
        const char *fg     = (i == a->cursor) ? active_fg : header_fg;

        snprintf(buf, sizeof buf, "%s%s%s %s%s%s",
                 marker_fg ? marker_fg : "",
                 marker,
                 marker_fg ? FLUX_RESET : "",
                 fg ? fg : "",
                 s->title ? s->title : "",
                 fg ? FLUX_RESET : "");
        flux_fit(sb, buf, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");

        if (s->open && s->body) {
            int nlines = _flux_b4_count_lines(s->body);
            for (k = 0; k < nlines; k++) {
                char line[2048];
                char row[2200];
                _flux_b4_get_line(s->body, k, line, (int)sizeof line);
                snprintf(row, sizeof row, "  %s%s%s",
                         body_fg ? body_fg : "",
                         line,
                         body_fg ? FLUX_RESET : "");
                flux_fit(sb, row, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
                flux_sb_append(sb, "\n");
            }
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: FluxCollapsible
 * ═════════════════════════════════════════════════════════════════ */

void flux_collapsible_init(FluxCollapsible *c, const char *t,
                           const char *b, int expanded) {
    if (!c) return;
    c->title     = t;
    c->body      = b;
    c->expanded  = expanded ? 1 : 0;
    c->focused   = 0;
    c->title_fg  = NULL;
    c->body_fg   = NULL;
    c->marker_fg = NULL;
}

int flux_collapsible_update(FluxCollapsible *c, FluxMsg msg) {
    if (!c || !c->focused) return 0;
    if (msg.type != MSG_KEY) return 0;
    if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")) {
        c->expanded = !c->expanded;
        return 1;
    }
    return 0;
}

void flux_collapsible_render(FluxCollapsible *c, FluxSB *sb, int width) {
    const char *title_fg, *body_fg, *marker_fg;
    const char *marker;
    char buf[2048];
    int k;

    if (!c || !sb || width <= 0) return;

    title_fg  = c->title_fg  ? c->title_fg  : FLUX_THEME_TEXT_FG;
    body_fg   = c->body_fg   ? c->body_fg   : FLUX_THEME_TEXT2_FG;
    marker_fg = c->marker_fg ? c->marker_fg : FLUX_THEME_TEXT_DIM_FG;
    marker    = c->expanded ? "\xe2\x96\xbe" : "\xe2\x96\xb8"; /* ▾ ▸ */

    snprintf(buf, sizeof buf, "%s%s%s %s%s%s",
             marker_fg ? marker_fg : "",
             marker,
             marker_fg ? FLUX_RESET : "",
             title_fg ? title_fg : "",
             c->title ? c->title : "",
             title_fg ? FLUX_RESET : "");
    flux_fit(sb, buf, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");

    if (c->expanded && c->body) {
        int nlines = _flux_b4_count_lines(c->body);
        for (k = 0; k < nlines; k++) {
            char line[2048];
            char row[2200];
            _flux_b4_get_line(c->body, k, line, (int)sizeof line);
            snprintf(row, sizeof row, "  %s%s%s",
                     body_fg ? body_fg : "",
                     line,
                     body_fg ? FLUX_RESET : "");
            flux_fit(sb, row, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
            flux_sb_append(sb, "\n");
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: FluxContentSwitcher
 * ═════════════════════════════════════════════════════════════════ */

void flux_content_switcher_init(FluxContentSwitcher *s,
                                const char **labels, int n) {
    int i;
    if (!s) return;
    if (n < 0) n = 0;
    if (n > FLUX_CSW_MAX) n = FLUX_CSW_MAX;
    s->count     = n;
    s->active    = 0;
    s->active_bg = NULL;
    s->active_fg = NULL;
    s->idle_fg   = NULL;
    s->border_fg = NULL;
    s->y         = 0;
    for (i = 0; i < n; i++) {
        s->labels[i]  = labels ? labels[i] : "";
        s->x_start[i] = 0;
        s->x_end[i]   = 0;
    }
}

int flux_content_switcher_update(FluxContentSwitcher *s, FluxMsg msg) {
    int i;
    if (!s || s->count <= 0) return 0;

    if (msg.type == MSG_KEY) {
        if (flux_key_is(msg, "left") || flux_key_is(msg, "h")) {
            s->active = (s->active - 1 + s->count) % s->count;
            return 1;
        }
        if (flux_key_is(msg, "right") || flux_key_is(msg, "l")) {
            s->active = (s->active + 1) % s->count;
            return 1;
        }
        if (msg.u.key.key[0] >= '1' && msg.u.key.key[0] <= '9'
         && msg.u.key.key[1] == '\0') {
            int idx = msg.u.key.key[0] - '1';
            if (idx < s->count) { s->active = idx; return 1; }
        }
    }
    if (msg.type == MSG_MOUSE
     && msg.u.mouse.event == FLUX_MOUSE_PRESS
     && msg.u.mouse.button == 0) {
        if (msg.u.mouse.y == s->y) {
            for (i = 0; i < s->count; i++) {
                if (msg.u.mouse.x >= s->x_start[i]
                 && msg.u.mouse.x <= s->x_end[i]) {
                    if (s->active != i) { s->active = i; return 1; }
                    return 0;
                }
            }
        }
    }
    return 0;
}

void flux_content_switcher_render(FluxContentSwitcher *s, FluxSB *sb,
                                  int width, int screen_x, int screen_y) {
    int i, used = 0;
    const char *active_bg, *active_fg, *idle_fg, *border_fg;
    char rowbuf[4096];
    FluxSB row;

    if (!s || !sb || width <= 0) return;

    active_bg = s->active_bg ? s->active_bg : FLUX_THEME_BUTTON_OK_BG;
    active_fg = s->active_fg ? s->active_fg : FLUX_THEME_TEXT_FG;
    idle_fg   = s->idle_fg   ? s->idle_fg   : FLUX_THEME_TEXT_DIM_FG;
    border_fg = s->border_fg ? s->border_fg : FLUX_THEME_BORDER_FG;

    flux_sb_init(&row, rowbuf, (int)sizeof rowbuf);
    s->y = screen_y;

    /* Leading bar */
    flux_sb_append(&row, border_fg);
    flux_sb_append(&row, "\xe2\x94\x82"); /* │ */
    flux_sb_append(&row, FLUX_RESET);
    used += 1;

    for (i = 0; i < s->count; i++) {
        const char *lbl = s->labels[i] ? s->labels[i] : "";
        int lw = flux_strwidth(lbl);
        int seg_w = lw + 2; /* one space pad each side */

        s->x_start[i] = screen_x + used;
        s->x_end[i]   = screen_x + used + seg_w - 1;

        if (i == s->active) {
            flux_sb_append(&row, active_bg);
            flux_sb_append(&row, active_fg);
            flux_sb_append(&row, FLUX_BOLD);
            flux_sb_append(&row, " ");
            flux_sb_append(&row, lbl);
            flux_sb_append(&row, " ");
            flux_sb_append(&row, FLUX_RESET);
        } else {
            flux_sb_append(&row, idle_fg);
            flux_sb_append(&row, " ");
            flux_sb_append(&row, lbl);
            flux_sb_append(&row, " ");
            flux_sb_append(&row, FLUX_RESET);
        }
        used += seg_w;

        flux_sb_append(&row, border_fg);
        flux_sb_append(&row, "\xe2\x94\x82");
        flux_sb_append(&row, FLUX_RESET);
        used += 1;
    }

    flux_fit(sb, flux_sb_str(&row), width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: FluxMenu
 * ═════════════════════════════════════════════════════════════════ */

static int _flux_menu_is_navigable(const FluxMenuItem *it) {
    return !(it->disabled || it->separator);
}

static void _flux_menu_clamp_scroll(FluxMenu *m) {
    int vis = m->visible > m->count ? m->count : m->visible;
    if (vis <= 0) { m->scroll = 0; return; }
    if (m->cursor < m->scroll) m->scroll = m->cursor;
    if (m->cursor >= m->scroll + vis) m->scroll = m->cursor - vis + 1;
    if (m->scroll < 0) m->scroll = 0;
    if (m->count > vis && m->scroll > m->count - vis)
        m->scroll = m->count - vis;
}

void flux_menu_init(FluxMenu *m, FluxMenuItem *items, int n, int visible) {
    int i;
    if (!m) return;
    m->items       = items;
    m->count       = n < 0 ? 0 : n;
    m->visible     = visible <= 0 ? n : visible;
    m->cursor      = 0;
    m->scroll      = 0;
    m->selected_id = 0;
    m->activated   = 0;
    m->normal_fg   = NULL;
    m->active_bg   = NULL;
    m->active_fg   = NULL;
    m->hint_fg     = NULL;
    m->divider_fg  = NULL;

    /* place cursor on first navigable item */
    for (i = 0; i < m->count; i++) {
        if (_flux_menu_is_navigable(&items[i])) { m->cursor = i; break; }
    }
}

int flux_menu_update(FluxMenu *m, FluxMsg msg) {
    int i, dir;
    if (!m || m->count <= 0) return 0;
    if (msg.type != MSG_KEY) return 0;

    if (flux_key_is(msg, "up") || flux_key_is(msg, "k")
     || flux_key_is(msg, "down") || flux_key_is(msg, "j")) {
        dir = (flux_key_is(msg, "up") || flux_key_is(msg, "k")) ? -1 : 1;
        i = m->cursor;
        for (;;) {
            i += dir;
            if (i < 0 || i >= m->count) return 0;
            if (_flux_menu_is_navigable(&m->items[i])) {
                m->cursor = i;
                _flux_menu_clamp_scroll(m);
                return 1;
            }
        }
    }
    if (flux_key_is(msg, "home")) {
        for (i = 0; i < m->count; i++) {
            if (_flux_menu_is_navigable(&m->items[i])) {
                m->cursor = i; _flux_menu_clamp_scroll(m); return 1;
            }
        }
        return 0;
    }
    if (flux_key_is(msg, "end")) {
        for (i = m->count - 1; i >= 0; i--) {
            if (_flux_menu_is_navigable(&m->items[i])) {
                m->cursor = i; _flux_menu_clamp_scroll(m); return 1;
            }
        }
        return 0;
    }
    if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")) {
        if (_flux_menu_is_navigable(&m->items[m->cursor])) {
            m->selected_id = m->items[m->cursor].user_id;
            m->activated   = 1;
            return 1;
        }
        return 0;
    }
    /* Number jump */
    if (msg.u.key.key[0] >= '1' && msg.u.key.key[0] <= '9'
     && msg.u.key.key[1] == '\0') {
        int idx = msg.u.key.key[0] - '1';
        if (idx < m->count && _flux_menu_is_navigable(&m->items[idx])) {
            m->cursor = idx; _flux_menu_clamp_scroll(m); return 1;
        }
    }
    /* Hotkey match (single printable char) */
    if (msg.u.key.key[0] != '\0' && msg.u.key.key[1] == '\0'
     && !msg.u.key.ctrl && !msg.u.key.alt) {
        char c = (char)tolower((unsigned char)msg.u.key.key[0]);
        for (i = 0; i < m->count; i++) {
            if (!_flux_menu_is_navigable(&m->items[i])) continue;
            if (m->items[i].hotkey
             && (char)tolower((unsigned char)m->items[i].hotkey) == c) {
                m->cursor      = i;
                m->selected_id = m->items[i].user_id;
                m->activated   = 1;
                _flux_menu_clamp_scroll(m);
                return 1;
            }
        }
    }
    return 0;
}

void flux_menu_render(FluxMenu *m, FluxSB *sb, int width) {
    int i, vis;
    const char *normal_fg, *active_bg, *active_fg, *hint_fg, *divider_fg;

    if (!m || !sb || width <= 0 || m->count <= 0) return;

    normal_fg  = m->normal_fg  ? m->normal_fg  : FLUX_THEME_TEXT_FG;
    active_bg  = m->active_bg  ? m->active_bg  : FLUX_THEME_BUTTON_OK_BG;
    active_fg  = m->active_fg  ? m->active_fg  : FLUX_THEME_ACCENT_FG;
    hint_fg    = m->hint_fg    ? m->hint_fg    : FLUX_THEME_TEXT_DIM_FG;
    divider_fg = m->divider_fg ? m->divider_fg : FLUX_THEME_DIVIDER_FG;

    _flux_menu_clamp_scroll(m);
    vis = m->visible > m->count ? m->count : m->visible;
    if (vis <= 0) vis = m->count;

    for (i = 0; i < vis; i++) {
        int idx = m->scroll + i;
        FluxMenuItem *it;
        if (idx >= m->count) {
            flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(sb, "\n");
            continue;
        }
        it = &m->items[idx];

        if (it->separator) {
            int k;
            if (divider_fg) flux_sb_append(sb, divider_fg);
            for (k = 0; k < width; k++)
                flux_sb_append(sb, "\xe2\x94\x80"); /* ─ */
            if (divider_fg) flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, "\n");
            continue;
        }

        {
            char  body[2048];
            char  rowbuf[4096];
            FluxSB row;
            int   active = (idx == m->cursor);

            flux_sb_init(&row, rowbuf, (int)sizeof rowbuf);

            if (active) {
                flux_sb_append(&row, active_bg);
                flux_sb_append(&row, active_fg);
                flux_sb_append(&row, FLUX_BOLD);
                flux_sb_append(&row, "\xe2\x96\xb8 "); /* ▸  */
            } else {
                flux_sb_append(&row, normal_fg);
                flux_sb_append(&row, "  ");
            }
            if (it->disabled) flux_sb_append(&row, FLUX_DIM);

            if (it->icon && it->icon[0]) {
                flux_sb_append(&row, it->icon);
                flux_sb_append(&row, " ");
            }
            flux_sb_append(&row, it->label ? it->label : "");

            if (it->shortcut && it->shortcut[0]) {
                int label_w = (it->label ? flux_strwidth(it->label) : 0)
                            + (it->icon  ? flux_strwidth(it->icon) + 1 : 0);
                int sc_w    = flux_strwidth(it->shortcut);
                int dots    = width - 2 /* prefix */ - label_w - sc_w - 1;
                int k;
                if (dots < 1) dots = 1;
                flux_sb_append(&row, " ");
                if (!active) {
                    flux_sb_append(&row, hint_fg ? hint_fg : "");
                }
                for (k = 0; k < dots; k++) flux_sb_append(&row, " ");
                flux_sb_append(&row, it->shortcut);
                if (!active && hint_fg) flux_sb_append(&row, FLUX_RESET);
            }

            flux_sb_append(&row, FLUX_RESET);

            /* Build body string then fit. flux_fit handles padding for
             * active rows by appending spaces (no bg coverage gap is OK
             * since active row contains its own bg up to label end). */
            snprintf(body, sizeof body, "%s", flux_sb_str(&row));
            flux_fit(sb, body, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
            flux_sb_append(sb, "\n");
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: FluxCommandPalette
 * ═════════════════════════════════════════════════════════════════ */

static int _flux_cp_fuzzy(const char *target, const char *q) {
    int t = 0, score = 0, last = -1, i;
    if (!target) return INT_MAX;
    if (!q || !*q) return 0;
    for (i = 0; q[i]; i++) {
        char qc = (char)tolower((unsigned char)q[i]);
        int found = 0;
        while (target[t]) {
            char tc = (char)tolower((unsigned char)target[t]);
            if (tc == qc) {
                int gap  = (last < 0) ? t : t - last - 1;
                int prev = (t == 0) ? ' '
                         : (int)tolower((unsigned char)target[t - 1]);
                score += gap;
                if (prev == ' ' || prev == '-' || prev == '_') score -= 2;
                last = t;
                t++;
                found = 1;
                break;
            }
            t++;
        }
        if (!found) return INT_MAX;
    }
    return score;
}

static int _flux_cp_score_item(const FluxCmdItem *it, const char *q) {
    int a = _flux_cp_fuzzy(it->name,        q);
    int b = _flux_cp_fuzzy(it->description, q);
    int c = _flux_cp_fuzzy(it->category,    q);
    int min = a;
    if (b < min) min = b;
    if (c < min) min = c;
    return min;
}

static void _flux_cp_filter(FluxCommandPalette *p) {
    int i, j, k;
    const char *q = p->query.buf;
    int empty = (p->query.len == 0);

    p->filtered_count = 0;
    for (i = 0; i < p->count && p->filtered_count < FLUX_CP_MAX; i++) {
        int sc = empty ? 0 : _flux_cp_score_item(&p->items[i], q);
        if (sc == INT_MAX) continue;
        p->filtered_idx  [p->filtered_count] = i;
        p->filtered_score[p->filtered_count] = sc;
        p->filtered_count++;
    }
    /* insertion sort ascending by score */
    for (i = 1; i < p->filtered_count; i++) {
        int idx_i = p->filtered_idx[i];
        int sc_i  = p->filtered_score[i];
        j = i - 1;
        while (j >= 0 && p->filtered_score[j] > sc_i) {
            p->filtered_idx  [j + 1] = p->filtered_idx[j];
            p->filtered_score[j + 1] = p->filtered_score[j];
            j--;
        }
        p->filtered_idx  [j + 1] = idx_i;
        p->filtered_score[j + 1] = sc_i;
    }
    if (p->cursor >= p->filtered_count) p->cursor = 0;
    if (p->cursor < 0)                  p->cursor = 0;
    if (p->scroll > p->cursor)          p->scroll = p->cursor;
    k = p->visible > 0 ? p->visible : 8;
    if (p->cursor >= p->scroll + k)     p->scroll = p->cursor - k + 1;
    if (p->scroll < 0)                  p->scroll = 0;
}

void flux_command_palette_init(FluxCommandPalette *p,
                               FluxCmdItem *items, int n) {
    int i;
    if (!p) return;
    p->items = items;
    p->count = n < 0 ? 0 : (n > FLUX_CP_MAX ? FLUX_CP_MAX : n);
    flux_input_init(&p->query, "Type to search…");
    for (i = 0; i < FLUX_CP_MAX; i++) {
        p->filtered_idx[i]   = 0;
        p->filtered_score[i] = 0;
    }
    p->filtered_count = 0;
    p->cursor         = 0;
    p->scroll         = 0;
    p->visible        = 8;
    p->is_open        = 0;
    p->activated      = 0;
    p->selected_id    = NULL;
    p->border_fg      = NULL;
    p->panel_bg       = NULL;
    p->active_bg      = NULL;
    p->match_fg       = NULL;
    _flux_cp_filter(p);
}

int flux_command_palette_update(FluxCommandPalette *p, FluxMsg msg) {
    if (!p || !p->is_open) return 0;
    if (msg.type != MSG_KEY) return 0;

    if (flux_key_is(msg, "esc")) {
        p->is_open = 0;
        return 1;
    }
    if (flux_key_is(msg, "enter")) {
        if (p->filtered_count > 0
         && p->cursor >= 0 && p->cursor < p->filtered_count) {
            int idx = p->filtered_idx[p->cursor];
            p->selected_id = p->items[idx].id;
            p->activated   = 1;
            p->is_open     = 0;
        }
        return 1;
    }
    if (flux_key_is(msg, "up")) {
        if (p->cursor > 0) {
            p->cursor--;
            if (p->cursor < p->scroll) p->scroll = p->cursor;
            return 1;
        }
        return 0;
    }
    if (flux_key_is(msg, "down")) {
        if (p->cursor < p->filtered_count - 1) {
            p->cursor++;
            if (p->visible > 0 && p->cursor >= p->scroll + p->visible) {
                p->scroll = p->cursor - p->visible + 1;
            }
            return 1;
        }
        return 0;
    }
    /* Otherwise: defer to the input. If it changed, re-filter. */
    if (flux_input_update(&p->query, msg)) {
        _flux_cp_filter(p);
        return 1;
    }
    return 0;
}

void flux_command_palette_render(FluxCommandPalette *p, FluxSB *sb,
                                 int screen_w, int screen_h) {
    int box_w, box_h, box_x, box_y;
    int inner_w, list_h;
    int y, i;
    const char *border_fg, *panel_bg, *active_bg, *match_fg;
    const char *backdrop_bg;

    if (!p || !sb || screen_w <= 0 || screen_h <= 0) return;
    if (!p->is_open) return;

    border_fg = p->border_fg ? p->border_fg : FLUX_THEME_BORDER_FG;
    panel_bg  = p->panel_bg  ? p->panel_bg  : FLUX_THEME_PANEL_BG;
    active_bg = p->active_bg ? p->active_bg : FLUX_THEME_BUTTON_OK_BG;
    match_fg  = p->match_fg  ? p->match_fg  : FLUX_THEME_ACCENT_FG;
    backdrop_bg = FLUX_THEME_TITLEBAR_BG;

    box_w = 60;
    if (box_w > screen_w - 4) box_w = screen_w - 4;
    if (box_w < 20)           box_w = 20;
    inner_w = box_w - 2;
    if (inner_w < 4) inner_w = 4;

    /* Layout rows:
     *   top border, query row, divider, list rows…, bottom border. */
    list_h = p->visible;
    if (list_h <= 0) list_h = 8;
    if (list_h > p->filtered_count && p->filtered_count > 0)
        list_h = p->filtered_count;
    if (list_h < 1) list_h = 1;

    box_h = 2 /* top+bot border */
          + 1 /* query */
          + 1 /* divider */
          + list_h;
    if (box_h > screen_h) box_h = screen_h;

    p->visible = list_h; /* keep struct in sync for update math */

    box_x = (screen_w - box_w) / 2;
    box_y = (screen_h - box_h) / 2;
    if (box_x < 0) box_x = 0;
    if (box_y < 0) box_y = 0;

    for (y = 0; y < screen_h; y++) {
        int row_in_box;

        if (y < box_y || y >= box_y + box_h) {
            _flux_b4_blank_row(sb, backdrop_bg, screen_w);
            continue;
        }
        row_in_box = y - box_y;

        if (row_in_box == 0 || row_in_box == box_h - 1) {
            int lp = box_x, rp = screen_w - box_x - box_w;
            const char *L = (row_in_box == 0)
                          ? "\xe2\x95\xad" : "\xe2\x95\xb0";
            const char *R = (row_in_box == 0)
                          ? "\xe2\x95\xae" : "\xe2\x95\xaf";
            if (lp < 0) lp = 0;
            if (rp < 0) rp = 0;
            if (lp > 0) {
                flux_sb_append(sb, backdrop_bg);
                for (i = 0; i < lp; i++) flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_RESET);
            }
            flux_sb_append(sb, border_fg);
            flux_sb_append(sb, panel_bg);
            flux_sb_append(sb, L);
            for (i = 0; i < inner_w; i++) flux_sb_append(sb, "\xe2\x94\x80");
            flux_sb_append(sb, R);
            flux_sb_append(sb, FLUX_RESET);
            if (rp > 0) {
                flux_sb_append(sb, backdrop_bg);
                for (i = 0; i < rp; i++) flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_RESET);
            }
            flux_sb_append(sb, "\n");
            continue;
        }

        {
            char  inside[8192];
            FluxSB ins;
            flux_sb_init(&ins, inside, (int)sizeof inside);

            if (row_in_box == 1) {
                /* query row: " > " + input fitted to remainder */
                char  qbuf[4096];
                FluxSB qb;
                flux_sb_init(&qb, qbuf, (int)sizeof qbuf);
                flux_sb_append(&qb, " ");
                flux_sb_append(&qb, match_fg);
                flux_sb_append(&qb, "\xe2\x9d\xaf");  /* ❯ */
                flux_sb_append(&qb, FLUX_RESET);
                flux_sb_append(&qb, " ");
                {
                    char  ib[3072];
                    FluxSB ibsb;
                    flux_sb_init(&ibsb, ib, (int)sizeof ib);
                    flux_input_render(&p->query, &ibsb,
                                      inner_w - 4,
                                      FLUX_THEME_TEXT_FG, NULL);
                    flux_sb_append(&qb, flux_sb_str(&ibsb));
                }
                flux_sb_append(&qb, " ");
                flux_fit(&ins, flux_sb_str(&qb), inner_w,
                         "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
            } else if (row_in_box == 2) {
                /* divider */
                int k;
                flux_sb_append(&ins, border_fg);
                for (k = 0; k < inner_w; k++)
                    flux_sb_append(&ins, "\xe2\x94\x80");
                flux_sb_append(&ins, FLUX_RESET);
            } else {
                int list_row = row_in_box - 3;
                int idx_in_filt = p->scroll + list_row;
                if (idx_in_filt < 0 || idx_in_filt >= p->filtered_count) {
                    if (p->filtered_count == 0 && list_row == 0) {
                        flux_sb_append(&ins, FLUX_THEME_TEXT_DIM_FG);
                        flux_fit(&ins, " No matches ", inner_w,
                                 "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
                        flux_sb_append(&ins, FLUX_RESET);
                    } else {
                        flux_fit(&ins, "", inner_w, NULL, FLUX_ALIGN_LEFT);
                    }
                } else {
                    int real_idx = p->filtered_idx[idx_in_filt];
                    FluxCmdItem *it = &p->items[real_idx];
                    char  rowbuf[4096];
                    FluxSB row;
                    int    active = (idx_in_filt == p->cursor);

                    flux_sb_init(&row, rowbuf, (int)sizeof rowbuf);

                    if (active) {
                        flux_sb_append(&row, active_bg);
                        flux_sb_append(&row, FLUX_THEME_TEXT_FG);
                        flux_sb_append(&row, FLUX_BOLD);
                        flux_sb_append(&row, " \xe2\x96\xb8 ");
                    } else {
                        flux_sb_append(&row, FLUX_THEME_TEXT_FG);
                        flux_sb_append(&row, "   ");
                    }
                    flux_sb_append(&row, it->name ? it->name : "");
                    if (it->category && it->category[0]) {
                        flux_sb_append(&row, "  ");
                        flux_sb_append(&row, FLUX_THEME_TEXT_DIM_FG);
                        flux_sb_append(&row, it->category);
                    }
                    if (it->shortcut && it->shortcut[0]) {
                        flux_sb_append(&row, "  ");
                        flux_sb_append(&row, FLUX_THEME_TEXT_DIM_FG);
                        flux_sb_append(&row, it->shortcut);
                    }
                    flux_sb_append(&row, FLUX_RESET);
                    flux_fit(&ins, flux_sb_str(&row), inner_w,
                             "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
                }
            }

            _flux_modal_box_row(sb, border_fg, panel_bg, backdrop_bg,
                                box_x, box_w, screen_w,
                                flux_sb_str(&ins));
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: FluxPaginator
 * ═════════════════════════════════════════════════════════════════ */

void flux_paginator_init(FluxPaginator *p, int total, int current,
                         FluxPaginatorStyle s) {
    if (!p) return;
    p->total     = total < 0 ? 0 : total;
    p->current   = current;
    if (p->current < 0)                         p->current = 0;
    if (p->total > 0 && p->current >= p->total) p->current = p->total - 1;
    p->style     = s;
    p->changed   = 0;
    p->active_fg = NULL;
    p->idle_fg   = NULL;
    p->arrow_fg  = NULL;
}

int flux_paginator_update(FluxPaginator *p, FluxMsg msg) {
    int prev;
    if (!p || p->total <= 0) return 0;
    if (msg.type != MSG_KEY) return 0;

    prev = p->current;
    if (flux_key_is(msg, "left") || flux_key_is(msg, "h")) {
        if (p->current > 0) p->current--;
    } else if (flux_key_is(msg, "right") || flux_key_is(msg, "l")) {
        if (p->current < p->total - 1) p->current++;
    } else if (flux_key_is(msg, "home")) {
        p->current = 0;
    } else if (flux_key_is(msg, "end")) {
        p->current = p->total - 1;
    } else {
        return 0;
    }
    p->changed = (p->current != prev);
    return p->changed;
}

void flux_paginator_render(FluxPaginator *p, FluxSB *sb, int width) {
    char  buf[1024];
    FluxSB row;
    const char *active_fg, *idle_fg, *arrow_fg;

    if (!p || !sb || width <= 0) return;

    active_fg = p->active_fg ? p->active_fg : FLUX_THEME_ACCENT_FG;
    idle_fg   = p->idle_fg   ? p->idle_fg   : FLUX_THEME_TEXT_DIM_FG;
    arrow_fg  = p->arrow_fg  ? p->arrow_fg  : FLUX_THEME_TEXT_FG;

    flux_sb_init(&row, buf, (int)sizeof buf);

    if (p->style == FLUX_PG_DOTS) {
        int i;
        for (i = 0; i < p->total; i++) {
            if (i > 0) flux_sb_append(&row, " ");
            if (i == p->current) {
                flux_sb_append(&row, active_fg);
                flux_sb_append(&row, "\xe2\x97\x8f"); /* ● */
                flux_sb_append(&row, FLUX_RESET);
            } else {
                flux_sb_append(&row, idle_fg);
                flux_sb_append(&row, "\xe2\x97\x8b"); /* ○ */
                flux_sb_append(&row, FLUX_RESET);
            }
        }
    } else if (p->style == FLUX_PG_FRACTION) {
        char tmp[64];
        snprintf(tmp, sizeof tmp, "%d / %d", p->current + 1, p->total);
        flux_sb_append(&row, active_fg);
        flux_sb_append(&row, tmp);
        flux_sb_append(&row, FLUX_RESET);
    } else {
        /* NUMBERS */
        int i;
        int show[256];
        int sc = 0;
        int prev = -2;
        flux_sb_append(&row, arrow_fg);
        flux_sb_append(&row, "\xe2\x80\xb9"); /* ‹ */
        flux_sb_append(&row, FLUX_RESET);
        flux_sb_append(&row, " ");

        if (p->total <= 7) {
            for (i = 0; i < p->total && sc < 256; i++) show[sc++] = i;
        } else {
            int c = p->current;
            int seen[256];
            for (i = 0; i < 256; i++) seen[i] = 0;
            seen[0] = seen[p->total - 1] = 1;
            if (c >= 0 && c < p->total)         seen[c] = 1;
            if (c - 1 >= 0)                     seen[c - 1] = 1;
            if (c + 1 < p->total)               seen[c + 1] = 1;
            for (i = 0; i < p->total && sc < 256; i++) {
                if (seen[i]) show[sc++] = i;
            }
        }

        for (i = 0; i < sc; i++) {
            int n = show[i];
            char tmp[32];
            if (prev >= 0 && n > prev + 1) {
                flux_sb_append(&row, idle_fg);
                flux_sb_append(&row, "\xe2\x80\xa6"); /* … */
                flux_sb_append(&row, FLUX_RESET);
                flux_sb_append(&row, " ");
            }
            if (n == p->current) {
                snprintf(tmp, sizeof tmp, "[%d]", n + 1);
                flux_sb_append(&row, active_fg);
                flux_sb_append(&row, FLUX_BOLD);
                flux_sb_append(&row, tmp);
                flux_sb_append(&row, FLUX_RESET);
            } else {
                snprintf(tmp, sizeof tmp, "%d", n + 1);
                flux_sb_append(&row, idle_fg);
                flux_sb_append(&row, tmp);
                flux_sb_append(&row, FLUX_RESET);
            }
            flux_sb_append(&row, " ");
            prev = n;
        }
        flux_sb_append(&row, arrow_fg);
        flux_sb_append(&row, "\xe2\x80\xba"); /* › */
        flux_sb_append(&row, FLUX_RESET);
    }

    flux_fit(sb, flux_sb_str(&row), width, "\xe2\x80\xa6", FLUX_ALIGN_CENTER);
    flux_sb_append(sb, "\n");
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: flux_breadcrumb
 * ═════════════════════════════════════════════════════════════════ */

void flux_breadcrumb(FluxSB *sb,
                     const char **parts, int n,
                     int active, int width,
                     const char *normal_fg,
                     const char *active_fg,
                     const char *sep_fg) {
    int i;
    int act = active;
    char  buf[4096];
    FluxSB row;
    int total_w;

    if (!sb || width <= 0) return;
    if (n <= 0 || !parts) {
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
        return;
    }
    if (act < 0 || act >= n) act = n - 1;

    if (!normal_fg) normal_fg = FLUX_THEME_TEXT_DIM_FG;
    if (!active_fg) active_fg = FLUX_THEME_TEXT_FG;
    if (!sep_fg)    sep_fg    = FLUX_THEME_TEXT_OFF_FG;

    /* Compute total visible cells. Separator " › " counts 3 cells. */
    total_w = 0;
    for (i = 0; i < n; i++) {
        total_w += parts[i] ? flux_strwidth(parts[i]) : 0;
        if (i + 1 < n) total_w += 3;
    }

    /* Drop leading parts with "… › " prefix until it fits. */
    {
        int start = 0;
        int cur_w = total_w;
        while (start < n && cur_w + (start > 0 ? 4 : 0) > width) {
            int part_w = parts[start] ? flux_strwidth(parts[start]) : 0;
            int sep_w  = (start + 1 < n) ? 3 : 0;
            cur_w -= (part_w + sep_w);
            start++;
        }
        flux_sb_init(&row, buf, (int)sizeof buf);

        if (start > 0) {
            flux_sb_append(&row, sep_fg);
            flux_sb_append(&row, "\xe2\x80\xa6"); /* … */
            flux_sb_append(&row, " \xe2\x80\xba "); /* ` › ` */
            flux_sb_append(&row, FLUX_RESET);
        }
        for (i = start; i < n; i++) {
            const char *p = parts[i] ? parts[i] : "";
            if (i == act) {
                flux_sb_append(&row, active_fg);
                flux_sb_append(&row, FLUX_BOLD);
                flux_sb_append(&row, p);
                flux_sb_append(&row, FLUX_RESET);
            } else {
                flux_sb_append(&row, normal_fg);
                flux_sb_append(&row, p);
                flux_sb_append(&row, FLUX_RESET);
            }
            if (i + 1 < n) {
                flux_sb_append(&row, sep_fg);
                flux_sb_append(&row, " \xe2\x80\xba "); /* ` › ` */
                flux_sb_append(&row, FLUX_RESET);
            }
        }
    }

    flux_fit(sb, flux_sb_str(&row), width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}


/* ══════════════════════════════════════════════════════════════════
 * IMPLEMENTATION: flux_help_panel
 * ═════════════════════════════════════════════════════════════════ */

void flux_help_panel(FluxSB *sb,
                     const FluxHelpBinding *bindings, int n,
                     int width,
                     const char *title,
                     const char *key_fg,
                     const char *desc_fg,
                     const char *cat_fg) {
    int i, k;
    int key_w, desc_w;
    const char *prev_cat = NULL;

    if (!sb || width <= 0) return;
    if (!key_fg)  key_fg  = FLUX_THEME_ACCENT_FG;
    if (!desc_fg) desc_fg = FLUX_THEME_TEXT_FG;
    if (!cat_fg)  cat_fg  = FLUX_THEME_TEXT_DIM_FG;

    key_w = width / 3;
    if (key_w < 6) key_w = 6;
    desc_w = width - key_w - 1; /* gap */
    if (desc_w < 4) desc_w = width - key_w;

    if (title && title[0]) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s%s%s",
                 cat_fg, title, FLUX_RESET);
        flux_fit(sb, buf, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }

    if (n <= 0 || !bindings) return;

    for (i = 0; i < n; i++) {
        const FluxHelpBinding *b = &bindings[i];
        char  rowbuf[4096];
        char  kbuf[1024];
        char  dbuf[1024];
        FluxSB row, kfit, dfit;

        if (b->category && (!prev_cat
                         || strcmp(b->category, prev_cat) != 0)) {
            if (prev_cat) {
                flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
                flux_sb_append(sb, "\n");
            }
            {
                char hb[1024];
                snprintf(hb, sizeof hb, "%s%s%s",
                         cat_fg, b->category, FLUX_RESET);
                flux_fit(sb, hb, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
                flux_sb_append(sb, "\n");
            }
            prev_cat = b->category;
        }

        flux_sb_init(&kfit, kbuf, (int)sizeof kbuf);
        flux_sb_init(&dfit, dbuf, (int)sizeof dbuf);
        flux_sb_init(&row,  rowbuf, (int)sizeof rowbuf);

        {
            char tmp[512];
            snprintf(tmp, sizeof tmp, "%s%s%s",
                     key_fg,
                     b->keys ? b->keys : "",
                     FLUX_RESET);
            flux_fit(&kfit, tmp, key_w, "\xe2\x80\xa6", FLUX_ALIGN_RIGHT);
        }
        {
            char tmp[512];
            snprintf(tmp, sizeof tmp, "%s%s%s",
                     desc_fg,
                     b->description ? b->description : "",
                     FLUX_RESET);
            flux_fit(&dfit, tmp, desc_w, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
        }
        flux_sb_append(&row, flux_sb_str(&kfit));
        flux_sb_append(&row, " ");
        flux_sb_append(&row, flux_sb_str(&dfit));
        flux_fit(sb, flux_sb_str(&row), width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");

        (void)k;
    }
}

/* ─── B5_ai_widgets impl ────────────────────────────────── */
#include <string.h>
#include <stdio.h>

/* ── shared helpers ─────────────────────────────────────────────── */

/* K/M number formatter: 999, 12.3K, 1.2M */
static void flux__fmt_kn(long n, char *out, int cap) {
    if (n < 0) n = 0;
    if (n < 1000L) {
        snprintf(out, (size_t)cap, "%ld", n);
    } else if (n < 1000000L) {
        snprintf(out, (size_t)cap, "%.1fK", (double)n / 1000.0);
    } else if (n < 1000000000L) {
        snprintf(out, (size_t)cap, "%.1fM", (double)n / 1000000.0);
    } else {
        snprintf(out, (size_t)cap, "%.1fB", (double)n / 1000000000.0);
    }
}

/* Color thresholds for usage bars (<70% ok, <90% warn, else err). */
static const char *flux__usage_color(double frac) {
    if (frac < 0.70) return FLUX_THEME_OK_FG;
    if (frac < 0.90) return FLUX_THEME_WARN_FG;
    return FLUX_THEME_ERR_FG;
}

/* Length of the next UTF-8 codepoint starting at *p. Returns 0 at NUL. */
static int flux__utf8_len(const char *p) {
    unsigned char c;
    if (!p || !*p) return 0;
    c = (unsigned char)*p;
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* Number of "cells" in one UTF-8 codepoint: 1 for ASCII/narrow, 2 for
 * common wide ranges. We only need this for cell-by-cell shimmer. */
static int flux__utf8_cells(const char *p, int byte_len) {
    unsigned int cp;
    if (byte_len <= 1) return 1;
    if (byte_len == 2) {
        cp = ((unsigned char)p[0] & 0x1F) << 6;
        cp |= (unsigned char)p[1] & 0x3F;
    } else if (byte_len == 3) {
        cp  = ((unsigned char)p[0] & 0x0F) << 12;
        cp |= ((unsigned char)p[1] & 0x3F) << 6;
        cp |=  (unsigned char)p[2] & 0x3F;
    } else {
        cp  = ((unsigned char)p[0] & 0x07) << 18;
        cp |= ((unsigned char)p[1] & 0x3F) << 12;
        cp |= ((unsigned char)p[2] & 0x3F) << 6;
        cp |=  (unsigned char)p[3] & 0x3F;
    }
    /* Common wide ranges. */
    if (cp >= 0x1100 && cp <= 0x115F) return 2;
    if (cp >= 0x2E80 && cp <= 0x303E) return 2;
    if (cp >= 0x3041 && cp <= 0x33FF) return 2;
    if (cp >= 0x3400 && cp <= 0x4DBF) return 2;
    if (cp >= 0x4E00 && cp <= 0x9FFF) return 2;
    if (cp >= 0xA000 && cp <= 0xA4CF) return 2;
    if (cp >= 0xAC00 && cp <= 0xD7A3) return 2;
    if (cp >= 0xF900 && cp <= 0xFAFF) return 2;
    if (cp >= 0xFE30 && cp <= 0xFE4F) return 2;
    if (cp >= 0xFF00 && cp <= 0xFF60) return 2;
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return 2;
    if (cp >= 0x1F300 && cp <= 0x1FAFF) return 2;
    if (cp >= 0x20000 && cp <= 0x2FFFD) return 2;
    return 1;
}


/* ══════════════════════════════════════════════════════════════════
 * 1. FluxBlinkDot
 * ═════════════════════════════════════════════════════════════════ */

void flux_blinkdot_init(FluxBlinkDot *d, FluxDotState s) {
    if (!d) return;
    d->state       = s;
    d->frame       = 0;
    d->interval_ms = 500;
    d->on_glyph    = "\xe2\x97\x8f";  /* ● */
    d->off_glyph   = " ";
}

void flux_blinkdot_tick(FluxBlinkDot *d) {
    if (!d) return;
    /* No-op for terminal states. */
    switch (d->state) {
        case FLUX_DOT_COMPLETED:
        case FLUX_DOT_FAILED:
        case FLUX_DOT_CANCELLED:
            return;
        default:
            d->frame++;
            return;
    }
}

void flux_blinkdot_render(FluxBlinkDot *d, FluxSB *sb) {
    const char *clr;
    int visible;
    const char *on, *off;
    if (!d || !sb) return;

    on  = d->on_glyph  ? d->on_glyph  : "\xe2\x97\x8f";
    off = d->off_glyph ? d->off_glyph : " ";

    switch (d->state) {
        case FLUX_DOT_PENDING:    clr = FLUX_THEME_TEXT_DIM_FG;     break;
        case FLUX_DOT_RUNNING:    clr = FLUX_THEME_WARN_FG;         break;
        case FLUX_DOT_STREAMING:  clr = FLUX_THEME_BRAND_PURPLE_FG; break;
        case FLUX_DOT_COMPLETED:  clr = FLUX_THEME_OK_FG;           break;
        case FLUX_DOT_FAILED:     clr = FLUX_THEME_ERR_FG;          break;
        case FLUX_DOT_CANCELLED:  clr = FLUX_THEME_TEXT_OFF_FG;     break;
        default:                  clr = FLUX_THEME_TEXT_FG;         break;
    }

    /* Solid for terminal states; blink (visible iff frame%2==0) for active. */
    switch (d->state) {
        case FLUX_DOT_COMPLETED:
        case FLUX_DOT_FAILED:
        case FLUX_DOT_CANCELLED:
        case FLUX_DOT_PENDING:
            visible = 1;
            break;
        case FLUX_DOT_STREAMING:
            /* fast-blink: every other frame as well, but caller can call
             * tick more often. Same boolean rule — frame%2==0 → on. */
            visible = (d->frame % 2 == 0);
            break;
        default:  /* RUNNING */
            visible = (d->frame % 2 == 0);
            break;
    }

    /* Always emit exactly 1 cell (no '\n'). flux_fit guarantees width=1. */
    {
        char L[64]; FluxSB l;
        flux_sb_init(&l, L, (int)sizeof L);
        flux_sb_append(&l, clr);
        flux_sb_append(&l, visible ? on : off);
        flux_sb_append(&l, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&l), 1, NULL, FLUX_ALIGN_LEFT);
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 2. flux_command_block
 * ═════════════════════════════════════════════════════════════════ */

/* Render one fitted row to sb, ending with '\n'. */
static void flux__cb_emit_row(FluxSB *sb, const char *content, int width) {
    flux_fit(sb, content, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}

void flux_command_block(FluxSB *sb,
                        const char *cmd,
                        const char *output,
                        int   exit_code,
                        int   duration_ms,
                        int   width)
{
    char header[1024]; FluxSB H;
    char dur_buf[32];
    int dur_w = 0;
    const char *V = "\xe2\x94\x82"; /* │ */

    if (!sb || width < 8) return;

    /* ── Header ─────────────────────────────────────────────────── */
    flux_sb_init(&H, header, (int)sizeof header);
    flux_sb_append(&H, FLUX_THEME_BORDER_FG);
    flux_sb_append(&H, "\xe2\x94\x8c\xe2\x94\x80"); /* ┌─ */
    flux_sb_append(&H, FLUX_RESET);
    flux_sb_append(&H, " ");
    flux_sb_append(&H, FLUX_THEME_ACCENT_FG);
    flux_sb_append(&H, "$");
    flux_sb_append(&H, FLUX_RESET);
    flux_sb_append(&H, " ");
    flux_sb_append(&H, FLUX_THEME_TEXT_FG);
    flux_sb_append(&H, "\x1b[1m");
    flux_sb_append(&H, cmd ? cmd : "");
    flux_sb_append(&H, FLUX_RESET);

    if (exit_code >= 0) {
        flux_sb_append(&H, " ");
        if (exit_code == 0) {
            flux_sb_append(&H, FLUX_THEME_OK_FG);
            flux_sb_append(&H, "\xe2\x9c\x93"); /* ✓ */
        } else {
            flux_sb_append(&H, FLUX_THEME_ERR_FG);
            flux_sb_append(&H, "\xe2\x9c\x97 "); /* ✗ */
            {
                char ec[16];
                snprintf(ec, sizeof ec, "%d", exit_code);
                flux_sb_append(&H, ec);
            }
        }
        flux_sb_append(&H, FLUX_RESET);
    }
    if (duration_ms >= 0) {
        dur_w = flux_activity_format_duration(duration_ms,
                                              dur_buf, (int)sizeof dur_buf);
        if (dur_w > 0) {
            flux_sb_append(&H, " ");
            flux_sb_append(&H, FLUX_THEME_TEXT_DIM_FG);
            flux_sb_append(&H, "(");
            flux_sb_append(&H, dur_buf);
            flux_sb_append(&H, ")");
            flux_sb_append(&H, FLUX_RESET);
        }
    }
    flux__cb_emit_row(sb, flux_sb_str(&H), width);

    /* ── Output rows ────────────────────────────────────────────── */
    if (output && *output) {
        const char *p = output;
        const char *line_start = output;
        int inner_w = width - 4; /* "│ " + content + " │" approx */
        if (inner_w < 1) inner_w = 1;

        for (;;) {
            char line_buf[2048];
            int  line_len;
            char row[3072]; FluxSB R;
            char fitted[2048]; FluxSB F;
            int copy_n;

            if (*p == '\n' || *p == '\0') {
                line_len = (int)(p - line_start);
                if (line_len < 0) line_len = 0;
                copy_n = line_len < (int)sizeof line_buf - 1
                       ? line_len : (int)sizeof line_buf - 1;
                memcpy(line_buf, line_start, (size_t)copy_n);
                line_buf[copy_n] = '\0';

                flux_sb_init(&R, row, (int)sizeof row);
                flux_sb_append(&R, FLUX_THEME_BORDER_FG);
                flux_sb_append(&R, V);
                flux_sb_append(&R, FLUX_RESET);
                flux_sb_append(&R, " ");
                flux_sb_append(&R, FLUX_THEME_CODE_BG);
                flux_sb_append(&R, FLUX_THEME_TEXT2_FG);

                flux_sb_init(&F, fitted, (int)sizeof fitted);
                flux_fit(&F, line_buf, inner_w, NULL, FLUX_ALIGN_LEFT);
                flux_sb_append(&R, flux_sb_str(&F));

                flux_sb_append(&R, FLUX_RESET);
                flux_sb_append(&R, " ");
                flux_sb_append(&R, FLUX_THEME_BORDER_FG);
                flux_sb_append(&R, V);
                flux_sb_append(&R, FLUX_RESET);

                flux__cb_emit_row(sb, flux_sb_str(&R), width);

                if (*p == '\0') break;
                line_start = p + 1;
            }
            p++;
        }

        /* Bottom border. */
        {
            char B[256]; FluxSB b;
            int i;
            flux_sb_init(&b, B, (int)sizeof B);
            flux_sb_append(&b, FLUX_THEME_BORDER_FG);
            flux_sb_append(&b, "\xe2\x94\x94"); /* └ */
            for (i = 1; i < width - 1; i++) flux_sb_append(&b, "\xe2\x94\x80"); /* ─ */
            flux_sb_append(&b, "\xe2\x94\x98"); /* ┘ */
            flux_sb_append(&b, FLUX_RESET);
            flux_sb_append(&b, "\n");
            flux_sb_append(sb, flux_sb_str(&b));
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 3. FluxCommandDropdown
 * ═════════════════════════════════════════════════════════════════ */

void flux_cmd_dropdown_init(FluxCommandDropdown *d,
                            const FluxCmdDropItem *items, int n) {
    if (!d) return;
    d->items        = items;
    d->n_items      = n;
    d->selected     = 0;
    d->max_visible  = 6;
    d->scroll       = 0;
    d->filter[0]    = '\0';
    d->filter_len   = 0;
    d->closed       = 0;
    d->chosen       = -1;
    d->highlight_fg = FLUX_THEME_ACCENT_FG;
    d->indicator    = "\xe2\x96\xb8 "; /* ▸ space */
}

/* Case-insensitive substring match. */
static int flux__icontains(const char *hay, const char *needle) {
    int hl, nl, i, j;
    if (!hay || !needle) return 0;
    if (!*needle) return 1;
    hl = (int)strlen(hay);
    nl = (int)strlen(needle);
    if (nl > hl) return 0;
    for (i = 0; i <= hl - nl; i++) {
        int ok = 1;
        for (j = 0; j < nl; j++) {
            char a = hay[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

/* Index of the i-th visible (filtered) entry; -1 if out of range.
 * Also returns the filtered count via *out_total. */
static int flux__cdd_filtered_at(const FluxCommandDropdown *d, int i,
                                 int *out_total) {
    int seen = 0, total = 0, k;
    int found = -1;
    for (k = 0; k < d->n_items; k++) {
        const FluxCmdDropItem *it = &d->items[k];
        if (d->filter_len == 0 || flux__icontains(it->name, d->filter)) {
            if (seen == i) found = k;
            seen++; total++;
        }
    }
    if (out_total) *out_total = total;
    return found;
}

int flux_cmd_dropdown_update(FluxCommandDropdown *d, FluxMsg msg) {
    int total;
    if (!d || msg.type != MSG_KEY) return 0;
    if (d->closed) return 0;

    if (flux_key_is(msg, "enter")) {
        int idx = flux__cdd_filtered_at(d, d->selected, &total);
        if (idx >= 0) {
            d->chosen = idx;
            return 1;
        }
        return 0;
    }
    if (flux_key_is(msg, "esc")) {
        if (d->filter_len > 0) {
            d->filter_len = 0;
            d->filter[0]  = '\0';
            d->selected   = 0;
            d->scroll     = 0;
        } else {
            d->closed = 1;
        }
        return 1;
    }
    if (flux_key_is(msg, "up")) {
        if (d->selected > 0) d->selected--;
        if (d->selected < d->scroll) d->scroll = d->selected;
        return 1;
    }
    if (flux_key_is(msg, "down")) {
        flux__cdd_filtered_at(d, 0, &total);
        if (d->selected < total - 1) d->selected++;
        if (d->max_visible > 0 &&
            d->selected >= d->scroll + d->max_visible) {
            d->scroll = d->selected - d->max_visible + 1;
        }
        return 1;
    }
    if (flux_key_is(msg, "backspace")) {
        if (d->filter_len > 0) {
            d->filter_len--;
            d->filter[d->filter_len] = '\0';
            d->selected = 0;
            d->scroll   = 0;
            return 1;
        }
        return 0;
    }
    /* Printable single-byte key */
    if (msg.u.key.rune > 31 && msg.u.key.rune < 127 &&
        !msg.u.key.ctrl && !msg.u.key.alt) {
        if (d->filter_len < (int)sizeof d->filter - 1) {
            d->filter[d->filter_len++] = (char)msg.u.key.rune;
            d->filter[d->filter_len]   = '\0';
            d->selected = 0;
            d->scroll   = 0;
            return 1;
        }
    }
    return 0;
}

/* Render one option row to sb (already fitted to width). */
static void flux__cdd_render_row(FluxSB *sb, const FluxCmdDropItem *it,
                                 int selected, const char *highlight_fg,
                                 const char *indicator,
                                 const char *filter, int filter_len,
                                 int width)
{
    char L[2048]; FluxSB l;
    flux_sb_init(&l, L, (int)sizeof L);

    /* Indicator gutter (2 cells). */
    if (selected) {
        flux_sb_append(&l, highlight_fg ? highlight_fg : FLUX_THEME_ACCENT_FG);
        flux_sb_append(&l, indicator ? indicator : "\xe2\x96\xb8 ");
        flux_sb_append(&l, FLUX_RESET);
    } else {
        flux_sb_append(&l, "  ");
    }

    /* Name with bolded match (single substring). */
    {
        const char *name = it->name ? it->name : "";
        const char *match_p = NULL;
        int match_off = -1;
        if (filter_len > 0) {
            int hl = (int)strlen(name);
            int nl = filter_len;
            int i, j;
            for (i = 0; i <= hl - nl; i++) {
                int ok = 1;
                for (j = 0; j < nl; j++) {
                    char a = name[i + j];
                    char b = filter[j];
                    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                    if (a != b) { ok = 0; break; }
                }
                if (ok) { match_off = i; break; }
            }
        }
        flux_sb_append(&l, selected ? FLUX_THEME_TEXT_FG : FLUX_THEME_TEXT2_FG);
        if (match_off >= 0) {
            char head[256], mid[64];
            int head_n = match_off < (int)sizeof head - 1
                       ? match_off : (int)sizeof head - 1;
            int mid_n = filter_len < (int)sizeof mid - 1
                      ? filter_len : (int)sizeof mid - 1;
            memcpy(head, name, (size_t)head_n);
            head[head_n] = '\0';
            memcpy(mid, name + match_off, (size_t)mid_n);
            mid[mid_n] = '\0';
            flux_sb_append(&l, head);
            flux_sb_append(&l, "\x1b[1m");
            flux_sb_append(&l, FLUX_THEME_ACCENT_FG);
            flux_sb_append(&l, mid);
            flux_sb_append(&l, FLUX_RESET);
            flux_sb_append(&l, selected ? FLUX_THEME_TEXT_FG : FLUX_THEME_TEXT2_FG);
            flux_sb_append(&l, name + match_off + filter_len);
            (void)match_p;
        } else {
            flux_sb_append(&l, name);
        }
        flux_sb_append(&l, FLUX_RESET);
    }

    if (it->desc && *it->desc) {
        flux_sb_append(&l, "  ");
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, it->desc);
        flux_sb_append(&l, FLUX_RESET);
    }

    flux_fit(sb, flux_sb_str(&l), width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}

void flux_cmd_dropdown_render(FluxCommandDropdown *d, FluxSB *sb, int width) {
    int total;
    int max_vis;
    int show_top, show_bot;
    int start, end, i;

    if (!d || !sb || width <= 0) return;
    if (d->closed) return;

    flux__cdd_filtered_at(d, 0, &total);

    /* Filter line */
    if (d->filter_len > 0) {
        char L[256]; FluxSB l;
        flux_sb_init(&l, L, (int)sizeof L);
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, "/ ");
        flux_sb_append(&l, FLUX_RESET);
        flux_sb_append(&l, FLUX_THEME_TEXT_FG);
        flux_sb_append(&l, d->filter);
        flux_sb_append(&l, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }

    max_vis = d->max_visible > 0 ? d->max_visible : 6;
    if (max_vis > total) max_vis = total;
    if (d->scroll < 0) d->scroll = 0;
    if (d->scroll > total - max_vis) d->scroll = total - max_vis;
    if (d->scroll < 0) d->scroll = 0;

    show_top = d->scroll > 0;
    show_bot = d->scroll + max_vis < total;

    if (show_top) {
        char L[64]; FluxSB l;
        flux_sb_init(&l, L, (int)sizeof L);
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, "  …");
        flux_sb_append(&l, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }

    start = d->scroll;
    end   = start + max_vis;
    for (i = start; i < end; i++) {
        int idx = flux__cdd_filtered_at(d, i, NULL);
        if (idx < 0) break;
        flux__cdd_render_row(sb, &d->items[idx], i == d->selected,
                             d->highlight_fg, d->indicator,
                             d->filter, d->filter_len, width);
    }

    if (show_bot) {
        char L[64]; FluxSB l;
        flux_sb_init(&l, L, (int)sizeof L);
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, "  …");
        flux_sb_append(&l, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 4. flux_context_window
 * ═════════════════════════════════════════════════════════════════ */

void flux_context_window(FluxSB *sb, long used, long total, int width) {
    char L[1024]; FluxSB l;
    char used_s[16], total_s[16], left_s[16];
    char tail[128];
    int  bar_cells;
    double frac;
    long left;
    const char *clr;
    int pct;
    int i, filled;

    if (!sb || width <= 0) return;
    if (used < 0) used = 0;
    if (total <= 0) total = 1;
    if (used > total) used = total;

    frac = (double)used / (double)total;
    pct  = (int)(frac * 100.0 + 0.5);
    left = total - used;
    clr  = flux__usage_color(frac);

    bar_cells = (int)((width * 4 + 9) / 10); /* ceil(width * 0.4) */
    if (bar_cells < 1) bar_cells = 1;
    if (bar_cells > width - 4) bar_cells = width - 4;

    filled = (int)(frac * (double)bar_cells + 0.5);
    if (filled < 0) filled = 0;
    if (filled > bar_cells) filled = bar_cells;

    flux_sb_init(&l, L, (int)sizeof L);
    flux_sb_append(&l, clr);
    for (i = 0; i < filled; i++) flux_sb_append(&l, "\xe2\x96\x88"); /* █ */
    flux_sb_append(&l, FLUX_RESET);
    flux_sb_append(&l, FLUX_THEME_TEXT_OFF_FG);
    for (i = filled; i < bar_cells; i++) flux_sb_append(&l, "\xe2\x96\x91"); /* ░ */
    flux_sb_append(&l, FLUX_RESET);

    flux__fmt_kn(used,  used_s,  (int)sizeof used_s);
    flux__fmt_kn(total, total_s, (int)sizeof total_s);
    flux__fmt_kn(left,  left_s,  (int)sizeof left_s);
    snprintf(tail, sizeof tail, " %s/%s (%d%%) \xc2\xb7 %s left",
             used_s, total_s, pct, left_s);

    flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(&l, tail);
    flux_sb_append(&l, FLUX_RESET);

    flux_fit(sb, flux_sb_str(&l), width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}


/* ══════════════════════════════════════════════════════════════════
 * 5. flux_cost_tracker
 * ═════════════════════════════════════════════════════════════════ */

static const char *flux__cost_color(double usd) {
    if (usd < 0.10) return FLUX_THEME_OK_FG;
    if (usd < 1.00) return FLUX_THEME_WARN_FG;
    return FLUX_THEME_ERR_FG;
}

void flux_cost_tracker(FluxSB *sb,
                       long   prompt_tokens, long completion_tokens,
                       double prompt_cost_per_1m,
                       double completion_cost_per_1m,
                       double session_prior_usd,
                       int    width)
{
    double pin, pout, total;
    const char *cost_clr;
    char content[256];
    int  inner_w = width - 2;
    int  i;

    if (!sb || width < 12) return;

    if (prompt_tokens     < 0) prompt_tokens     = 0;
    if (completion_tokens < 0) completion_tokens = 0;
    if (session_prior_usd < 0) session_prior_usd = 0;

    pin   = (double)prompt_tokens     / 1e6 * prompt_cost_per_1m;
    pout  = (double)completion_tokens / 1e6 * completion_cost_per_1m;
    total = pin + pout + session_prior_usd;
    cost_clr = flux__cost_color(total);

    /* Top border: ┌── Cost ─...─┐ */
    {
        char L[1024]; FluxSB l;
        flux_sb_init(&l, L, (int)sizeof L);
        flux_sb_append(&l, FLUX_THEME_BORDER_FG);
        flux_sb_append(&l, "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80"); /* ┌── */
        flux_sb_append(&l, " ");
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, "Cost");
        flux_sb_append(&l, FLUX_RESET);
        flux_sb_append(&l, " ");
        flux_sb_append(&l, FLUX_THEME_BORDER_FG);
        for (i = 0; i < width - 9; i++) flux_sb_append(&l, "\xe2\x94\x80");
        flux_sb_append(&l, "\xe2\x94\x90"); /* ┐ */
        flux_sb_append(&l, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }

    /* Helper to emit one bordered body row given a pre-built `f`. */
    {
        char R[2048]; FluxSB r;
        char F[2048]; FluxSB f;
        char tot[32];

        /* Total row */
        flux_sb_init(&r, R, (int)sizeof R);
        flux_sb_init(&f, F, (int)sizeof F);
        flux_sb_append(&r, FLUX_THEME_BORDER_FG);
        flux_sb_append(&r, "\xe2\x94\x82"); /* │ */
        flux_sb_append(&r, FLUX_RESET);
        snprintf(tot, sizeof tot, "$%.4f", total);
        flux_sb_append(&f, " ");
        flux_sb_append(&f, FLUX_THEME_TEXT_FG);
        flux_sb_append(&f, "Total: ");
        flux_sb_append(&f, FLUX_RESET);
        flux_sb_append(&f, "\x1b[1m");
        flux_sb_append(&f, cost_clr);
        flux_sb_append(&f, tot);
        flux_sb_append(&f, FLUX_RESET);
        flux_fit(&r, flux_sb_str(&f), inner_w, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(&r, FLUX_THEME_BORDER_FG);
        flux_sb_append(&r, "\xe2\x94\x82");
        flux_sb_append(&r, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
    (void)content;

    {
        char R[2048]; FluxSB r;
        char F[2048]; FluxSB f;
        char tk[32], pr[32], val[32];

        /* In row */
        flux_sb_init(&r, R, (int)sizeof R);
        flux_sb_init(&f, F, (int)sizeof F);
        flux_sb_append(&r, FLUX_THEME_BORDER_FG);
        flux_sb_append(&r, "\xe2\x94\x82");
        flux_sb_append(&r, FLUX_RESET);
        snprintf(tk, sizeof tk, "%ld", prompt_tokens);
        snprintf(pr, sizeof pr, "$%g/M", prompt_cost_per_1m);
        snprintf(val, sizeof val, "= $%.4f", pin);
        flux_sb_append(&f, "   ");
        flux_sb_append(&f, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&f, "In:  ");
        flux_sb_append(&f, tk);
        flux_sb_append(&f, " \xc3\x97 "); /* × */
        flux_sb_append(&f, pr);
        flux_sb_append(&f, " ");
        flux_sb_append(&f, val);
        flux_sb_append(&f, FLUX_RESET);
        flux_fit(&r, flux_sb_str(&f), inner_w, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(&r, FLUX_THEME_BORDER_FG);
        flux_sb_append(&r, "\xe2\x94\x82");
        flux_sb_append(&r, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }

    {
        char R[2048]; FluxSB r;
        char F[2048]; FluxSB f;
        char tk[32], pr[32], val[32];

        /* Out row */
        flux_sb_init(&r, R, (int)sizeof R);
        flux_sb_init(&f, F, (int)sizeof F);
        flux_sb_append(&r, FLUX_THEME_BORDER_FG);
        flux_sb_append(&r, "\xe2\x94\x82");
        flux_sb_append(&r, FLUX_RESET);
        snprintf(tk, sizeof tk, "%ld", completion_tokens);
        snprintf(pr, sizeof pr, "$%g/M", completion_cost_per_1m);
        snprintf(val, sizeof val, "= $%.4f", pout);
        flux_sb_append(&f, "   ");
        flux_sb_append(&f, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&f, "Out: ");
        flux_sb_append(&f, tk);
        flux_sb_append(&f, " \xc3\x97 ");
        flux_sb_append(&f, pr);
        flux_sb_append(&f, " ");
        flux_sb_append(&f, val);
        flux_sb_append(&f, FLUX_RESET);
        flux_fit(&r, flux_sb_str(&f), inner_w, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(&r, FLUX_THEME_BORDER_FG);
        flux_sb_append(&r, "\xe2\x94\x82");
        flux_sb_append(&r, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }

    /* Bottom border */
    {
        char L[1024]; FluxSB l;
        flux_sb_init(&l, L, (int)sizeof L);
        flux_sb_append(&l, FLUX_THEME_BORDER_FG);
        flux_sb_append(&l, "\xe2\x94\x94"); /* └ */
        for (i = 1; i < width - 1; i++) flux_sb_append(&l, "\xe2\x94\x80");
        flux_sb_append(&l, "\xe2\x94\x98"); /* ┘ */
        flux_sb_append(&l, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 6. flux_message_bubble
 * ═════════════════════════════════════════════════════════════════ */

static void flux__role_style(FluxRole role,
                             const char **glyph, const char **clr) {
    switch (role) {
        case FLUX_ROLE_USER:
            *glyph = "\xe2\x80\xba"; /* › */
            *clr   = FLUX_THEME_ACCENT_FG;
            break;
        case FLUX_ROLE_ASSISTANT:
            *glyph = "\xe2\x9c\xa6"; /* ✦ */
            *clr   = FLUX_THEME_BRAND_PURPLE_FG;
            break;
        case FLUX_ROLE_SYSTEM:
            *glyph = "\xe2\x97\x8f"; /* ● */
            *clr   = FLUX_THEME_WARN_FG;
            break;
        case FLUX_ROLE_TOOL:
        default:
            *glyph = "\xe2\x9a\x99"; /* ⚙ */
            *clr   = FLUX_THEME_ACCENT_FG;
            break;
    }
}

/* Word-wrap one paragraph into lines of at most max_w cells.  We treat
 * spaces as breakpoints; long un-broken tokens are hard-cut. */
static void flux__wrap_emit(FluxSB *sb, const char *text, int max_w,
                            int indent_w, int width)
{
    const char *p = text ? text : "";
    int line_w = 0;
    char buf[1024]; FluxSB lb;
    flux_sb_init(&lb, buf, (int)sizeof buf);

    while (*p) {
        const char *tok_start;
        int tok_w_bytes = 0;
        int tok_w_cells = 0;

        /* Skip / consume one leading space (count toward width). */
        if (*p == ' ') {
            if (line_w + 1 > max_w) {
                /* flush */
                int i;
                char R[1280]; FluxSB r;
                flux_sb_init(&r, R, (int)sizeof R);
                for (i = 0; i < indent_w; i++) flux_sb_append(&r, " ");
                flux_sb_append(&r, FLUX_THEME_TEXT_FG);
                flux_sb_append(&r, flux_sb_str(&lb));
                flux_sb_append(&r, FLUX_RESET);
                flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
                flux_sb_append(sb, "\n");
                flux_sb_init(&lb, buf, (int)sizeof buf);
                line_w = 0;
                p++;
                continue;
            }
            flux_sb_append(&lb, " ");
            line_w++;
            p++;
            continue;
        }

        /* Measure next token (until space or end). */
        tok_start = p;
        while (*p && *p != ' ') {
            int bl = flux__utf8_len(p);
            int cw = flux__utf8_cells(p, bl);
            tok_w_bytes += bl;
            tok_w_cells += cw;
            p += bl;
        }

        if (line_w + tok_w_cells > max_w && line_w > 0) {
            /* Flush current line, then place token on a fresh one. */
            int i;
            char R[1280]; FluxSB r;
            flux_sb_init(&r, R, (int)sizeof R);
            for (i = 0; i < indent_w; i++) flux_sb_append(&r, " ");
            flux_sb_append(&r, FLUX_THEME_TEXT_FG);
            flux_sb_append(&r, flux_sb_str(&lb));
            flux_sb_append(&r, FLUX_RESET);
            flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(sb, "\n");
            flux_sb_init(&lb, buf, (int)sizeof buf);
            line_w = 0;
        }

        if (tok_w_cells > max_w) {
            /* Hard-cut huge token */
            char tmp[512];
            int n = tok_w_bytes < (int)sizeof tmp - 1
                  ? tok_w_bytes : (int)sizeof tmp - 1;
            int i;
            char R[1280]; FluxSB r;
            memcpy(tmp, tok_start, (size_t)n);
            tmp[n] = '\0';
            flux_sb_init(&r, R, (int)sizeof R);
            for (i = 0; i < indent_w; i++) flux_sb_append(&r, " ");
            flux_sb_append(&r, FLUX_THEME_TEXT_FG);
            flux_sb_append(&r, tmp);
            flux_sb_append(&r, FLUX_RESET);
            flux_fit(sb, flux_sb_str(&r), width, "\xe2\x80\xa6",
                     FLUX_ALIGN_LEFT);
            flux_sb_append(sb, "\n");
            flux_sb_init(&lb, buf, (int)sizeof buf);
            line_w = 0;
        } else {
            char tmp[512];
            int n = tok_w_bytes < (int)sizeof tmp - 1
                  ? tok_w_bytes : (int)sizeof tmp - 1;
            memcpy(tmp, tok_start, (size_t)n);
            tmp[n] = '\0';
            flux_sb_append(&lb, tmp);
            line_w += tok_w_cells;
        }
    }

    if (line_w > 0) {
        int i;
        char R[1280]; FluxSB r;
        flux_sb_init(&r, R, (int)sizeof R);
        for (i = 0; i < indent_w; i++) flux_sb_append(&r, " ");
        flux_sb_append(&r, FLUX_THEME_TEXT_FG);
        flux_sb_append(&r, flux_sb_str(&lb));
        flux_sb_append(&r, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&r), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
}

void flux_message_bubble(FluxSB *sb,
                         FluxRole role,
                         const char *text,
                         const char *timestamp,
                         int width)
{
    const char *glyph, *clr;
    int gutter_w = 2;
    int max_w;
    const char *p, *line_start;
    int first_line = 1;

    if (!sb || width <= 0) return;
    flux__role_style(role, &glyph, &clr);
    max_w = width - 4;
    if (max_w < 1) max_w = 1;

    p = line_start = text ? text : "";

    for (;;) {
        if (*p == '\n' || *p == '\0') {
            char line_buf[2048];
            int line_len = (int)(p - line_start);
            int copy_n = line_len < (int)sizeof line_buf - 1
                       ? line_len : (int)sizeof line_buf - 1;
            memcpy(line_buf, line_start, (size_t)copy_n);
            line_buf[copy_n] = '\0';

            if (first_line) {
                /* Build first-line with timestamp right-aligned. */
                char body_part[2048];
                int  ts_w = timestamp ? flux_strwidth(timestamp) : 0;
                int  body_max = max_w - (ts_w > 0 ? (ts_w + 1) : 0);
                int  body_w;
                int  pad;
                char L[3072]; FluxSB l;
                int  i;

                if (body_max < 1) body_max = 1;

                /* Truncate body to body_max if needed. */
                {
                    int full_w = flux_strwidth(line_buf);
                    if (full_w > body_max) {
                        flux_truncate(line_buf, body_max, "\xe2\x80\xa6",
                                      body_part, (int)sizeof body_part);
                        body_w = body_max;
                    } else {
                        int n = (int)strlen(line_buf);
                        if (n >= (int)sizeof body_part)
                            n = (int)sizeof body_part - 1;
                        memcpy(body_part, line_buf, (size_t)n);
                        body_part[n] = '\0';
                        body_w = full_w;
                    }
                }

                flux_sb_init(&l, L, (int)sizeof L);
                /* glyph in gutter */
                {
                    char G[64]; FluxSB g;
                    flux_sb_init(&g, G, (int)sizeof G);
                    flux_sb_append(&g, clr);
                    flux_sb_append(&g, glyph);
                    flux_sb_append(&g, FLUX_RESET);
                    flux_fit(&l, flux_sb_str(&g), gutter_w, NULL,
                             FLUX_ALIGN_LEFT);
                }
                flux_sb_append(&l, " ");
                flux_sb_append(&l, FLUX_THEME_TEXT_FG);
                flux_sb_append(&l, body_part);
                flux_sb_append(&l, FLUX_RESET);
                pad = max_w - body_w - (ts_w > 0 ? (ts_w + 1) : 0);
                if (pad < 0) pad = 0;
                for (i = 0; i < pad; i++) flux_sb_append(&l, " ");
                if (timestamp && ts_w > 0) {
                    flux_sb_append(&l, " ");
                    flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
                    flux_sb_append(&l, timestamp);
                    flux_sb_append(&l, FLUX_RESET);
                }
                flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
                flux_sb_append(sb, "\n");
                first_line = 0;
            } else {
                flux__wrap_emit(sb, line_buf, max_w, 2, width);
            }

            if (*p == '\0') break;
            line_start = p + 1;
        }
        p++;
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 7. flux_model_badge
 * ═════════════════════════════════════════════════════════════════ */

static const char *flux__provider_color(const char *prov) {
    if (!prov) return FLUX_THEME_TEXT2_FG;
    /* Case-insensitive prefix-ish compare on first byte. */
    if (!strcmp(prov, "Anthropic") || !strcmp(prov, "anthropic"))
        return FLUX_THEME_BRAND_PURPLE_FG;
    if (!strcmp(prov, "OpenAI") || !strcmp(prov, "openai"))
        return FLUX_THEME_OK_FG;
    if (!strcmp(prov, "Google") || !strcmp(prov, "google"))
        return FLUX_THEME_ACCENT_FG;
    if (!strcmp(prov, "Meta") || !strcmp(prov, "meta"))
        return FLUX_THEME_WARN_FG;
    if (!strcmp(prov, "local") || !strcmp(prov, "Local"))
        return FLUX_THEME_TEXT_DIM_FG;
    return FLUX_THEME_TEXT2_FG;
}

void flux_model_badge(FluxSB *sb,
                      const char *provider,
                      const char *model,
                      long max_tokens,
                      int  width)
{
    char L[1024]; FluxSB l;
    const char *pclr = flux__provider_color(provider);

    if (!sb || width <= 0) return;

    flux_sb_init(&l, L, (int)sizeof L);
    flux_sb_append(&l, pclr);
    flux_sb_append(&l, "\xe2\x97\x86"); /* ◆ */
    flux_sb_append(&l, FLUX_RESET);
    flux_sb_append(&l, " ");

    if (provider && *provider) {
        flux_sb_append(&l, "\x1b[1m");
        flux_sb_append(&l, pclr);
        flux_sb_append(&l, provider);
        flux_sb_append(&l, FLUX_RESET);
        flux_sb_append(&l, " ");
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, "\xc2\xb7"); /* · */
        flux_sb_append(&l, FLUX_RESET);
        flux_sb_append(&l, " ");
    }
    if (model && *model) {
        flux_sb_append(&l, "\x1b[1m");
        flux_sb_append(&l, FLUX_THEME_TEXT_FG);
        flux_sb_append(&l, model);
        flux_sb_append(&l, FLUX_RESET);
    }
    if (max_tokens > 0) {
        char mt[16];
        flux__fmt_kn(max_tokens, mt, (int)sizeof mt);
        flux_sb_append(&l, " ");
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, mt);
        flux_sb_append(&l, FLUX_RESET);
    }

    flux_fit(sb, flux_sb_str(&l), width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}


/* ══════════════════════════════════════════════════════════════════
 * 8. FluxOpTree
 * ═════════════════════════════════════════════════════════════════ */

void flux_op_tree_init(FluxOpTree *t,
                       FluxOpItem *items, unsigned char *expanded, int n) {
    int i;
    if (!t) return;
    t->items               = items;
    t->n                   = n;
    t->expanded            = expanded;
    t->selected            = 0;
    t->spinner_frame       = 0;
    t->spinner_interval_ms = 80;
    if (expanded) {
        for (i = 0; i < n; i++) expanded[i] = 1; /* expanded by default */
    }
}

void flux_op_tree_tick(FluxOpTree *t) {
    if (!t) return;
    t->spinner_frame++;
}

/* Determine if row `i` is visible given current expansion.  An item at
 * depth d is visible iff every ancestor at shallower depth has its
 * `expanded` bit set. The "ancestor" is the most recent earlier row
 * with depth < d. */
static int flux__op_row_visible(const FluxOpTree *t, int i) {
    int d = t->items[i].depth;
    int j;
    int cur_d = d;
    for (j = i - 1; j >= 0; j--) {
        if (t->items[j].depth < cur_d) {
            if (t->expanded && !t->expanded[j]) return 0;
            cur_d = t->items[j].depth;
            if (cur_d == 0) return 1;
        }
    }
    return 1;
}

/* Map a "visible row index" → underlying items index. */
static int flux__op_visible_at(const FluxOpTree *t, int v_idx) {
    int seen = 0, i;
    for (i = 0; i < t->n; i++) {
        if (flux__op_row_visible(t, i)) {
            if (seen == v_idx) return i;
            seen++;
        }
    }
    return -1;
}

static int flux__op_visible_count(const FluxOpTree *t) {
    int n = 0, i;
    for (i = 0; i < t->n; i++)
        if (flux__op_row_visible(t, i)) n++;
    return n;
}

int flux_op_tree_update(FluxOpTree *t, FluxMsg msg) {
    int vis;
    if (!t || msg.type != MSG_KEY) return 0;
    vis = flux__op_visible_count(t);

    if (flux_key_is(msg, "up")) {
        if (t->selected > 0) t->selected--;
        return 1;
    }
    if (flux_key_is(msg, "down")) {
        if (t->selected < vis - 1) t->selected++;
        return 1;
    }
    if (flux_key_is(msg, "enter") || flux_key_is(msg, "space")) {
        int idx = flux__op_visible_at(t, t->selected);
        if (idx >= 0 && t->expanded && t->items[idx].has_children) {
            t->expanded[idx] = (unsigned char)!t->expanded[idx];
            return 1;
        }
    }
    return 0;
}

/* Build is_last[depth] table: for each depth ≤ d, is this row the last
 * visible sibling at that depth? Walks forward for each depth. */
static void flux__op_build_lasts(const FluxOpTree *t, int row_i,
                                 int *is_last /* len 16 */)
{
    int d = t->items[row_i].depth;
    int dd, j;
    if (d > 15) d = 15;

    for (dd = 0; dd <= d; dd++) is_last[dd] = 0;

    /* For each depth on the connector path, find whether there is a
     * later visible row whose depth is dd at the same parent context. */
    for (dd = 0; dd <= d; dd++) {
        /* The connector at column dd reflects whether the *ancestor at
         * depth dd* (the row that owns this column) has any later
         * visible sibling. For column == d, the row itself is the
         * "ancestor" being tested. For column < d we need the most
         * recent earlier row with depth dd. */
        int anc = -1;
        if (dd == d) {
            anc = row_i;
        } else {
            for (j = row_i - 1; j >= 0; j--) {
                if (t->items[j].depth == dd) { anc = j; break; }
                if (t->items[j].depth < dd)  { break; }
            }
        }
        if (anc < 0) { is_last[dd] = 1; continue; }
        /* Look for a later visible sibling at depth dd whose path
         * matches the same parent. We walk forward; if we hit a depth
         * < dd we stop (left the parent). */
        is_last[dd] = 1;
        for (j = anc + 1; j < t->n; j++) {
            if (t->items[j].depth < dd) break;
            if (t->items[j].depth == dd && flux__op_row_visible(t, j)) {
                is_last[dd] = 0;
                break;
            }
        }
    }
}

void flux_op_tree_render(FluxOpTree *t, FluxSB *sb, int width) {
    int i;
    int v_idx = 0;
    if (!t || !sb || width <= 0) return;

    for (i = 0; i < t->n; i++) {
        const FluxOpItem *it;
        int is_last[16];
        int dd;
        const char *icon;
        const char *icon_clr;
        const char *twirl = "";
        char L[2048]; FluxSB l;
        char dur_buf[32];
        int  selected;

        if (!flux__op_row_visible(t, i)) continue;
        it = &t->items[i];
        selected = (v_idx == t->selected);
        v_idx++;

        flux__op_build_lasts(t, i, is_last);

        /* Status icon + color */
        switch (it->status) {
            case FLUX_OP_PENDING:
                icon = "\xe2\x97\x8b"; /* ○ */
                icon_clr = FLUX_THEME_TEXT_DIM_FG;
                break;
            case FLUX_OP_RUNNING: {
                int idx = t->spinner_frame;
                if (idx < 0) idx = -idx;
                icon = FLUX_SPINNER_DOT[idx % FLUX_SPINNER_DOT_N];
                icon_clr = FLUX_THEME_WARN_FG;
                break;
            }
            case FLUX_OP_COMPLETED:
                icon = "\xe2\x9c\x93"; /* ✓ */
                icon_clr = FLUX_THEME_OK_FG;
                break;
            case FLUX_OP_FAILED:
                icon = "\xe2\x9c\x97"; /* ✗ */
                icon_clr = FLUX_THEME_ERR_FG;
                break;
            case FLUX_OP_CANCELLED:
            default:
                icon = "\xe2\x8a\x98"; /* ⊘ */
                icon_clr = FLUX_THEME_TEXT_OFF_FG;
                break;
        }

        if (it->has_children && t->expanded) {
            twirl = t->expanded[i]
                  ? "\xe2\x96\xbe " /* ▾ */
                  : "\xe2\x96\xb8 "; /* ▸ */
        }

        flux_sb_init(&l, L, (int)sizeof L);
        flux_sb_append(&l, FLUX_THEME_TEXT_OFF_FG);
        for (dd = 0; dd < it->depth; dd++) {
            if (dd == it->depth - 1) {
                flux_sb_append(&l, is_last[dd]
                                   ? "\xe2\x94\x94\xe2\x94\x80 "  /* └─  */
                                   : "\xe2\x94\x9c\xe2\x94\x80 "); /* ├─  */
            } else {
                flux_sb_append(&l, is_last[dd]
                                   ? "   "
                                   : "\xe2\x94\x82  "); /* │   */
            }
        }
        flux_sb_append(&l, FLUX_RESET);

        /* Status icon */
        flux_sb_append(&l, icon_clr);
        flux_sb_append(&l, icon);
        flux_sb_append(&l, FLUX_RESET);
        flux_sb_append(&l, " ");

        /* Twirl */
        if (twirl[0]) {
            flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
            flux_sb_append(&l, twirl);
            flux_sb_append(&l, FLUX_RESET);
        }

        /* Label (selected → reverse) */
        if (selected) flux_sb_append(&l, "\x1b[7m");
        flux_sb_append(&l, FLUX_THEME_TEXT_FG);
        flux_sb_append(&l, it->label ? it->label : "");
        flux_sb_append(&l, FLUX_RESET);

        if (it->detail && *it->detail) {
            flux_sb_append(&l, " ");
            flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
            flux_sb_append(&l, it->detail);
            flux_sb_append(&l, FLUX_RESET);
        }
        if (it->duration_ms > 0) {
            int dw = flux_activity_format_duration(it->duration_ms,
                                                   dur_buf,
                                                   (int)sizeof dur_buf);
            if (dw > 0) {
                flux_sb_append(&l, " ");
                flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
                flux_sb_append(&l, "(");
                flux_sb_append(&l, dur_buf);
                flux_sb_append(&l, ")");
                flux_sb_append(&l, FLUX_RESET);
            }
        }

        flux_fit(sb, flux_sb_str(&l), width, "\xe2\x80\xa6",
                 FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 9. FluxShimmerText
 * ═════════════════════════════════════════════════════════════════ */

void flux_shimmer_init(FluxShimmerText *s, const char *text) {
    if (!s) return;
    s->text        = text;
    s->offset      = 0;
    s->shimmer_w   = 3;
    s->step        = 1;
    s->interval_ms = 80;
    s->active      = 1;
    s->base_fg     = FLUX_THEME_TEXT_DIM_FG;
    s->shimmer_fg  = FLUX_THEME_ACCENT_FG;
}

void flux_shimmer_tick(FluxShimmerText *s) {
    int text_w, period, step;
    if (!s || !s->active || !s->text) return;
    text_w = flux_strwidth(s->text);
    if (s->shimmer_w < 1) s->shimmer_w = 1;
    /* `step` defaults to 1 (smooth single-cell glide). Older callers
     * that didn't set the field get the legacy "skip a band-width per
     * tick" behaviour where step == shimmer_w (which is what the old
     * `2*shimmer_w` formulation produced when the offset was kept in
     * [-shimmer_w, period)). */
    step = s->step > 0 ? s->step : 1;
    period = text_w + s->shimmer_w * 2;
    if (period < 1) period = 1;
    s->offset = ((s->offset + step + s->shimmer_w) % period)
              - s->shimmer_w;
    /* Keeps offset in [-shimmer_w, text_w + shimmer_w). */
}

void flux_shimmer_render(FluxShimmerText *s, FluxSB *sb) {
    const char *p;
    int cell = 0;
    int sw, lo, hi;
    if (!s || !sb || !s->text) return;
    sw = s->shimmer_w > 0 ? s->shimmer_w : 1;
    lo = s->offset;
    hi = s->offset + sw;

    if (!s->active) {
        flux_sb_append(sb, s->base_fg ? s->base_fg : FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(sb, s->text);
        flux_sb_append(sb, FLUX_RESET);
        return;
    }

    /* Cell-by-cell emission with run-length color spans. */
    p = s->text;
    {
        int in_shimmer = 0;
        int color_set = 0;
        const char *base = s->base_fg     ? s->base_fg     : FLUX_THEME_TEXT_DIM_FG;
        const char *shim = s->shimmer_fg  ? s->shimmer_fg  : FLUX_THEME_ACCENT_FG;
        while (*p) {
            int bl = flux__utf8_len(p);
            int cw = flux__utf8_cells(p, bl);
            int now_shimmer = (cell >= lo && cell < hi);
            if (!color_set || now_shimmer != in_shimmer) {
                if (color_set) flux_sb_append(sb, FLUX_RESET);
                if (now_shimmer) {
                    flux_sb_append(sb, "\x1b[1m");
                    flux_sb_append(sb, shim);
                } else {
                    flux_sb_append(sb, base);
                }
                in_shimmer = now_shimmer;
                color_set = 1;
            }
            {
                char tmp[8];
                int n = bl < 7 ? bl : 7;
                memcpy(tmp, p, (size_t)n);
                tmp[n] = '\0';
                flux_sb_append(sb, tmp);
            }
            cell += cw;
            p    += bl;
        }
        if (color_set) flux_sb_append(sb, FLUX_RESET);
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 10. FluxStreamingText
 * ═════════════════════════════════════════════════════════════════ */

void flux_streaming_init(FluxStreamingText *s, const char *text) {
    if (!s) return;
    s->text               = text;
    s->text_len           = text ? (int)strlen(text) : 0;
    s->revealed           = 0;
    s->speed              = 2;
    s->interval_ms        = 80;
    s->show_cursor        = 1;
    s->cursor_on          = 1;
    s->cursor_frame       = 0;
    s->cursor_interval_ms = 530;
    s->full_revealed      = 0;
    s->fg                 = FLUX_THEME_TEXT_FG;
    s->cursor_fg          = FLUX_THEME_ACCENT_FG;
}

void flux_streaming_set_text(FluxStreamingText *s, const char *text) {
    if (!s) return;
    s->text     = text;
    s->text_len = text ? (int)strlen(text) : 0;
    if (s->revealed > s->text_len) s->revealed = s->text_len;
}

void flux_streaming_tick(FluxStreamingText *s) {
    if (!s) return;
    s->cursor_frame++;
    s->cursor_on = (s->cursor_frame % 2 == 0);
    if (s->full_revealed) {
        s->revealed = s->text_len;
        return;
    }
    if (s->revealed < s->text_len) {
        int step = s->speed > 0 ? s->speed : 1;
        s->revealed += step;
        if (s->revealed > s->text_len) s->revealed = s->text_len;
    }
}

void flux_streaming_text_done(FluxStreamingText *s) {
    if (!s) return;
    s->full_revealed = 1;
    s->revealed      = s->text_len;
}

void flux_streaming_render(FluxStreamingText *s, FluxSB *sb) {
    char buf[4096];
    int n;
    if (!s || !sb) return;
    n = s->revealed;
    if (n < 0) n = 0;
    if (n > s->text_len) n = s->text_len;
    if (n > (int)sizeof buf - 1) n = (int)sizeof buf - 1;
    if (s->text && n > 0) {
        memcpy(buf, s->text, (size_t)n);
    }
    buf[n] = '\0';

    flux_sb_append(sb, s->fg ? s->fg : FLUX_THEME_TEXT_FG);
    flux_sb_append(sb, buf);
    flux_sb_append(sb, FLUX_RESET);

    if (s->show_cursor) {
        int cursor_visible;
        if (s->revealed >= s->text_len) {
            /* fully revealed → static cursor (no blink) */
            cursor_visible = 1;
        } else {
            cursor_visible = s->cursor_on;
        }
        if (cursor_visible) {
            flux_sb_append(sb, s->cursor_fg ? s->cursor_fg
                                            : FLUX_THEME_ACCENT_FG);
            flux_sb_append(sb, "\xe2\x96\x8a"); /* ▊ */
            flux_sb_append(sb, FLUX_RESET);
        } else {
            flux_sb_append(sb, " ");
        }
    }
}


/* ══════════════════════════════════════════════════════════════════
 * 11. flux_token_stream
 * ═════════════════════════════════════════════════════════════════ */

void flux_token_stream(FluxSB *sb,
                       double tok_per_sec,
                       long   total_tokens,
                       long   max_tokens,
                       int    width)
{
    char L[1024]; FluxSB l;
    char rate_buf[32];
    char tot_buf[16];
    const char *rate_clr;
    int  i;

    if (!sb || width <= 0) return;
    if (tok_per_sec < 0) tok_per_sec = 0;
    if (total_tokens < 0) total_tokens = 0;

    rate_clr = (tok_per_sec >= 30.0) ? FLUX_THEME_OK_FG : FLUX_THEME_WARN_FG;
    snprintf(rate_buf, sizeof rate_buf, "%.0f tok/s", tok_per_sec);
    flux__fmt_kn(total_tokens, tot_buf, (int)sizeof tot_buf);

    flux_sb_init(&l, L, (int)sizeof L);
    flux_sb_append(&l, rate_clr);
    flux_sb_append(&l, rate_buf);
    flux_sb_append(&l, FLUX_RESET);

    flux_sb_append(&l, " ");
    flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(&l, "\xc2\xb7"); /* · */
    flux_sb_append(&l, FLUX_RESET);
    flux_sb_append(&l, " ");

    flux_sb_append(&l, FLUX_THEME_TEXT2_FG);
    flux_sb_append(&l, tot_buf);
    flux_sb_append(&l, " tok");
    flux_sb_append(&l, FLUX_RESET);

    if (max_tokens > 0) {
        int bar_cells = 8;
        double frac = (double)total_tokens / (double)max_tokens;
        int filled;
        int pct;
        const char *bar_clr;
        char pct_buf[16];

        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        bar_clr = flux__usage_color(frac);
        filled  = (int)(frac * (double)bar_cells + 0.5);
        if (filled < 0) filled = 0;
        if (filled > bar_cells) filled = bar_cells;
        pct = (int)(frac * 100.0 + 0.5);
        snprintf(pct_buf, sizeof pct_buf, " %d%%", pct);

        flux_sb_append(&l, " ");
        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, "\xc2\xb7");
        flux_sb_append(&l, FLUX_RESET);
        flux_sb_append(&l, " ");

        flux_sb_append(&l, bar_clr);
        for (i = 0; i < filled; i++) flux_sb_append(&l, "\xe2\x96\x88"); /* █ */
        flux_sb_append(&l, FLUX_RESET);
        flux_sb_append(&l, FLUX_THEME_TEXT_OFF_FG);
        for (i = filled; i < bar_cells; i++)
            flux_sb_append(&l, "\xe2\x96\x91"); /* ░ */
        flux_sb_append(&l, FLUX_RESET);

        flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
        flux_sb_append(&l, pct_buf);
        flux_sb_append(&l, FLUX_RESET);
    }

    flux_fit(sb, flux_sb_str(&l), width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
    flux_sb_append(sb, "\n");
}

/* ─── B6_data_charts impl ────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════
 * Color palettes
 * ═════════════════════════════════════════════════════════════════ */

const char *FLUX_CHART_PALETTE[5] = {
    "\x1b[38;5;51m",   /* cyan    */
    "\x1b[38;5;201m",  /* magenta */
    "\x1b[38;5;226m",  /* yellow  */
    "\x1b[38;5;46m",   /* green   */
    "\x1b[38;5;39m"    /* blue    */
};

const char *FLUX_HEATMAP_RAMP[6] = {
    "\x1b[38;5;236m",  /* very dim */
    "\x1b[38;5;238m",
    "\x1b[38;5;58m",
    "\x1b[38;5;130m",
    "\x1b[38;5;202m",
    "\x1b[38;5;208m"   /* bright orange */
};

const char *FLUX_AXIS_COLOR = "\x1b[38;5;240m";

const char *flux_chart_color(int series_idx) {
    int i;
    if (series_idx < 0) series_idx = -series_idx;
    i = series_idx % 5;
    return FLUX_CHART_PALETTE[i];
}

const char *flux_heatmap_color(float t) {
    int i;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    i = (int)(t * 5.0f + 0.5f);
    if (i < 0) i = 0;
    if (i > 5) i = 5;
    return FLUX_HEATMAP_RAMP[i];
}

/* ══════════════════════════════════════════════════════════════════
 * Internal small helpers
 * ═════════════════════════════════════════════════════════════════ */

static const uint8_t FLUX_BR_BITS[2][4] = {
    { 0x01, 0x02, 0x04, 0x40 },   /* left col, rows 0..3 */
    { 0x08, 0x10, 0x20, 0x80 }    /* right col, rows 0..3 */
};

static int _flux_min_i(int a, int b) { return a < b ? a : b; }
__attribute__((unused)) static int _flux_max_i(int a, int b) { return a > b ? a : b; }
__attribute__((unused)) static float _flux_min_f(float a, float b) { return a < b ? a : b; }
__attribute__((unused)) static float _flux_max_f(float a, float b) { return a > b ? a : b; }

static void _flux_too_small(FluxSB *sb, int width) {
    if (width < 1) width = 1;
    flux_fit(sb, "(too small)", width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);
}

static void _flux_pad_n(FluxSB *sb, int n) {
    int i;
    for (i = 0; i < n; i++) flux_sb_append(sb, " ");
}

/* Encode a Unicode codepoint U+2800..U+28FF as 3-byte UTF-8 (E2 A0 80 + bits). */
static void _flux_emit_braille(FluxSB *sb, uint8_t bits) {
    char b[4];
    unsigned int cp = 0x2800u + (unsigned int)bits;
    b[0] = (char)(0xE0 | ((cp >> 12) & 0x0F));
    b[1] = (char)(0x80 | ((cp >>  6) & 0x3F));
    b[2] = (char)(0x80 | ( cp        & 0x3F));
    b[3] = '\0';
    flux_sb_append(sb, b);
}

/* RGB → "\x1b[38;2;R;G;Bm" buffer. buf must be ≥ 24 chars. */
static void _flux_rgb_fg(char *buf, uint32_t rgb) {
    int r = (int)((rgb >> 16) & 0xFF);
    int g = (int)((rgb >>  8) & 0xFF);
    int b = (int)( rgb        & 0xFF);
    /* snprintf is C99 */
    snprintf(buf, 24, "\x1b[38;2;%d;%d;%dm", r, g, b);
}

/* ══════════════════════════════════════════════════════════════════
 * Braille canvas
 * ═════════════════════════════════════════════════════════════════ */

int flux_braille_init(FluxBraille *c, int cols, int rows,
                      uint8_t *bits_buf, uint32_t *color_buf) {
    int i, n;
    if (!c || cols <= 0 || rows <= 0 || !bits_buf) return 0;
    c->cols = cols;
    c->rows = rows;
    c->pw = cols * 2;
    c->ph = rows * 4;
    c->bits = bits_buf;
    c->color = color_buf;
    n = cols * rows;
    for (i = 0; i < n; i++) c->bits[i] = 0;
    if (color_buf) for (i = 0; i < n; i++) color_buf[i] = 0;
    return 1;
}

void flux_braille_clear(FluxBraille *c) {
    int i, n;
    if (!c) return;
    n = c->cols * c->rows;
    for (i = 0; i < n; i++) c->bits[i] = 0;
    if (c->color) for (i = 0; i < n; i++) c->color[i] = 0;
}

void flux_braille_set_pixel(FluxBraille *c, int x, int y, uint32_t color) {
    int col, row, dx, dy, idx;
    if (!c || x < 0 || y < 0 || x >= c->pw || y >= c->ph) return;
    col = x / 2;
    row = y / 4;
    dx  = x & 1;
    dy  = y & 3;
    idx = row * c->cols + col;
    c->bits[idx] |= FLUX_BR_BITS[dx][dy];
    if (c->color && color) c->color[idx] = color;
}

void flux_braille_line(FluxBraille *c, int x0, int y0, int x1, int y1,
                       uint32_t color) {
    int dx, dy, sx, sy, err, e2;
    if (!c) return;
    dx = x1 - x0; if (dx < 0) dx = -dx;
    dy = y1 - y0; if (dy < 0) dy = -dy;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx - dy;
    for (;;) {
        flux_braille_set_pixel(c, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void flux_braille_render_row(const FluxBraille *c, FluxSB *sb, int row,
                             const char *default_color) {
    int col;
    uint32_t cur_color = 0;
    int color_open = 0;
    if (!c || !sb || row < 0 || row >= c->rows) return;
    if (default_color) {
        flux_sb_append(sb, default_color);
        color_open = 1;
    }
    for (col = 0; col < c->cols; col++) {
        int idx = row * c->cols + col;
        uint8_t bits = c->bits[idx];
        uint32_t cc  = c->color ? c->color[idx] : 0u;
        if (cc != cur_color) {
            char buf[24];
            if (cc != 0) {
                _flux_rgb_fg(buf, cc);
                flux_sb_append(sb, buf);
            } else {
                flux_sb_append(sb, FLUX_RESET);
                if (default_color) flux_sb_append(sb, default_color);
            }
            cur_color = cc;
            color_open = 1;
        }
        _flux_emit_braille(sb, bits);
    }
    if (color_open) flux_sb_append(sb, FLUX_RESET);
}

void flux_braille_render(const FluxBraille *c, FluxSB *sb,
                         const char *default_color) {
    int r;
    if (!c || !sb) return;
    for (r = 0; r < c->rows; r++) {
        flux_braille_render_row(c, sb, r, default_color);
        flux_sb_nl(sb);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * Resample / axis fmt
 * ═════════════════════════════════════════════════════════════════ */

void flux_resample(const float *src, int sn, float *dst, int dn) {
    int i, i0, i1;
    float t, f;
    if (!src || !dst || sn <= 0 || dn <= 0) return;
    if (sn == 1) {
        for (i = 0; i < dn; i++) dst[i] = src[0];
        return;
    }
    for (i = 0; i < dn; i++) {
        if (dn == 1) {
            dst[i] = src[0];
            continue;
        }
        t = (float)i * (float)(sn - 1) / (float)(dn - 1);
        i0 = (int)t;
        i1 = i0 + 1;
        if (i1 >= sn) i1 = sn - 1;
        f = t - (float)i0;
        dst[i] = src[i0] * (1.0f - f) + src[i1] * f;
    }
}

int flux_fmt_axis(char *out, int sz, float v, int width) {
    float a = v < 0 ? -v : v;
    int n;
    if (!out || sz <= 0) return 0;
    if (a >= 1.0e9f)      n = snprintf(out, (size_t)sz, "%.1fG", v / 1.0e9f);
    else if (a >= 1.0e6f) n = snprintf(out, (size_t)sz, "%.1fM", v / 1.0e6f);
    else if (a >= 1.0e3f) n = snprintf(out, (size_t)sz, "%.1fk", v / 1.0e3f);
    else if (a >= 100.0f) n = snprintf(out, (size_t)sz, "%.0f", v);
    else if (a >= 10.0f)  n = snprintf(out, (size_t)sz, "%.1f", v);
    else                  n = snprintf(out, (size_t)sz, "%.2f", v);
    if (n < 0) n = 0;
    if (n >= sz) n = sz - 1;
    /* right-pad to width */
    while (n < width && n < sz - 1) {
        out[n++] = ' ';
    }
    out[n] = '\0';
    return n;
}

/* ══════════════════════════════════════════════════════════════════
 * Auto-range helpers
 * ═════════════════════════════════════════════════════════════════ */

static void _flux_autorange(const float *vals, int n, float *lo, float *hi) {
    int i;
    float mn, mx;
    if (n <= 0) { *lo = 0; *hi = 1; return; }
    mn = mx = vals[0];
    for (i = 1; i < n; i++) {
        if (vals[i] < mn) mn = vals[i];
        if (vals[i] > mx) mx = vals[i];
    }
    if (mn == mx) { mn -= 1.0f; mx += 1.0f; }
    *lo = mn;
    *hi = mx;
}

/* ══════════════════════════════════════════════════════════════════
 * Title / axis row helpers
 * ═════════════════════════════════════════════════════════════════ */

static void _flux_chart_title(FluxSB *sb, const char *title, int width) {
    if (!title || !*title) return;
    flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
    flux_fit(sb, title, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ══════════════════════════════════════════════════════════════════
 * 1. area_chart
 * ═════════════════════════════════════════════════════════════════ */

void flux_area_chart(FluxSB *sb, const float *values, int n,
                     int width, int height, const FluxAreaOpts *opts) {
    FluxAreaOpts d = {0};
    int show_axes;
    int gutter;
    int chart_w, chart_h;
    int i;
    int title_rows;
    float ymin, ymax;
    /* stack canvases — capped */
    enum { MAX_CELLS = 8192 };
    static uint8_t  bits[MAX_CELLS];
    static uint32_t cols[MAX_CELLS];
    FluxBraille cv;
    int ncells;
    int pw, ph;
    int xi;
    char y_top[16], y_bot[16];

    if (!sb || !values || n <= 0 || width < 8 || height < 3) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    if (opts) d = *opts;
    show_axes = d.show_axes;
    gutter    = show_axes ? 7 : 0;

    title_rows = (d.title && *d.title) ? 1 : 0;
    chart_h = height - title_rows - (show_axes ? 1 : 0);
    if (chart_h < 1) chart_h = 1;
    chart_w = width - gutter;
    if (chart_w < 4) {
        if (d.title) _flux_chart_title(sb, d.title, width);
        for (i = 0; i < height - title_rows; i++) {
            flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
        }
        return;
    }

    if (d.y_min == d.y_max) {
        _flux_autorange(values, n, &ymin, &ymax);
    } else { ymin = d.y_min; ymax = d.y_max; }
    if (ymax <= ymin) ymax = ymin + 1.0f;

    pw = chart_w * 2;
    ph = chart_h * 4;
    ncells = chart_w * chart_h;
    if (ncells > MAX_CELLS) ncells = MAX_CELLS;
    flux_braille_init(&cv, chart_w, chart_h, bits, cols);
    flux_braille_clear(&cv);

    /* Resample to pw points & plot column fill + outline */
    {
        static float scratch[2048];
        int dn = pw;
        if (dn > 2048) dn = 2048;
        flux_resample(values, n, scratch, dn);
        for (xi = 0; xi < dn; xi++) {
            float v = scratch[xi];
            float t = (v - ymin) / (ymax - ymin);
            int y_top_p;
            int y;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            y_top_p = (int)((1.0f - t) * (float)(ph - 1) + 0.5f);
            for (y = ph - 1; y >= y_top_p; y--) {
                if (d.fill_sparse) {
                    if (((xi + y) & 1) == 0)
                        flux_braille_set_pixel(&cv, xi, y, 0);
                } else {
                    flux_braille_set_pixel(&cv, xi, y, 0);
                }
            }
        }
    }

    /* emit */
    if (d.title) _flux_chart_title(sb, d.title, width);
    flux_fmt_axis(y_top, sizeof y_top, ymax, gutter ? gutter - 1 : 0);
    flux_fmt_axis(y_bot, sizeof y_bot, ymin, gutter ? gutter - 1 : 0);

    for (i = 0; i < chart_h; i++) {
        if (show_axes) {
            const char *axis = (i == 0) ? y_top
                             : (i == chart_h - 1) ? y_bot
                             : "      ";
            flux_sb_append(sb, FLUX_AXIS_COLOR);
            flux_fit(sb, axis, gutter - 1, NULL, FLUX_ALIGN_RIGHT);
            flux_sb_append(sb, "\xe2\x94\x82");  /* │ */
            flux_sb_append(sb, FLUX_RESET);
        }
        flux_braille_render_row(&cv, sb, i,
            d.color ? d.color : FLUX_CHART_PALETTE[0]);
        flux_sb_nl(sb);
    }
    if (show_axes) {
        flux_sb_append(sb, FLUX_AXIS_COLOR);
        for (i = 0; i < gutter - 1; i++) flux_sb_append(sb, " ");
        flux_sb_append(sb, "\xe2\x94\x94"); /* └ */
        for (i = 0; i < chart_w; i++) flux_sb_append(sb, "\xe2\x94\x80"); /* ─ */
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * 2. bar_chart
 * ═════════════════════════════════════════════════════════════════ */

/* Vertical sub-row glyphs U+2581..U+2588 */
static const char *FLUX_BAR_VBLOCKS[9] = {
    " ",
    "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83",
    "\xe2\x96\x84", "\xe2\x96\x85", "\xe2\x96\x86",
    "\xe2\x96\x87", "\xe2\x96\x88"
};
/* Horizontal sub-col glyphs U+258F..U+2588 (left fills) */
static const char *FLUX_BAR_HBLOCKS[9] = {
    " ",
    "\xe2\x96\x8f", "\xe2\x96\x8e", "\xe2\x96\x8d",
    "\xe2\x96\x8c", "\xe2\x96\x8b", "\xe2\x96\x8a",
    "\xe2\x96\x89", "\xe2\x96\x88"
};

void flux_bar_chart(FluxSB *sb, const float *values, int n,
                    int width, int height,
                    const char *const *labels,
                    const FluxBarChartOpts *opts) {
    FluxBarChartOpts d;
    float vmax_pos = 0.0f, vmin_neg = 0.0f, vrange;
    int gap;
    int title_rows, label_rows;
    int chart_h;
    int gutter;
    int chart_w;
    int bar_w;
    int row, i;
    int avail;
    int total_bw;
    const char *uniform;
    int has_neg = 0, has_pos = 0;

    if (opts) d = *opts; else { FluxBarChartOpts z = {0}; d = z; }
    gap = d.bar_gap > 0 ? d.bar_gap : 1;

    if (!sb || !values || n <= 0 || width < 8 || height < 3) {
        if (sb) _flux_too_small(sb, width);
        return;
    }

    for (i = 0; i < n; i++) {
        if (values[i] > vmax_pos) { vmax_pos = values[i]; has_pos = 1; }
        if (values[i] < vmin_neg) { vmin_neg = values[i]; has_neg = 1; }
    }
    if (!has_pos && !has_neg) vmax_pos = 1.0f;
    vrange = vmax_pos - vmin_neg;
    if (vrange <= 0) vrange = 1.0f;

    uniform = d.color ? d.color : FLUX_CHART_PALETTE[0];

    /* horizontal layout — one bar per row */
    if (d.horizontal) {
        int max_label_w = 0;
        int label_w;
        char val_buf[16];
        if (d.title) _flux_chart_title(sb, d.title, width);
        if (labels) {
            for (i = 0; i < n; i++) {
                int lw = labels[i] ? flux_strwidth(labels[i]) : 0;
                if (lw > max_label_w) max_label_w = lw;
            }
        }
        label_w = max_label_w > 0 ? _flux_min_i(max_label_w, width / 3) : 0;
        for (i = 0; i < n && i < height; i++) {
            int bar_cells_w;
            int full, frac8, full_w;
            float t;
            const char *clr = (d.bar_colors && d.bar_colors[i])
                              ? d.bar_colors[i] : uniform;
            int used = 0;
            int val_w = d.show_values ? 6 : 0;
            int bar_room = width - label_w - val_w - 2;
            if (bar_room < 1) bar_room = 1;
            t = (values[i] - vmin_neg) / vrange;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            bar_cells_w = (int)(t * (float)bar_room * 8.0f + 0.5f);
            full = bar_cells_w / 8;
            frac8 = bar_cells_w & 7;
            full_w = full + (frac8 > 0 ? 1 : 0);

            if (label_w > 0) {
                flux_sb_append(sb, FLUX_THEME_TEXT2_FG);
                flux_fit(sb, labels && labels[i] ? labels[i] : "",
                         label_w, NULL, FLUX_ALIGN_LEFT);
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_append(sb, " ");
                used += label_w + 1;
            }
            flux_sb_append(sb, clr);
            {
                int j;
                for (j = 0; j < full; j++)  flux_sb_append(sb, FLUX_BAR_HBLOCKS[8]);
                if (frac8 > 0)             flux_sb_append(sb, FLUX_BAR_HBLOCKS[frac8]);
            }
            flux_sb_append(sb, FLUX_RESET);
            used += full_w;
            for (; used < width - val_w; used++) flux_sb_append(sb, " ");
            if (d.show_values) {
                snprintf(val_buf, sizeof val_buf, "%6.2f", values[i]);
                flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
                flux_fit(sb, val_buf, val_w, NULL, FLUX_ALIGN_RIGHT);
                flux_sb_append(sb, FLUX_RESET);
            }
            flux_sb_nl(sb);
        }
        return;
    }

    /* vertical layout */
    title_rows = (d.title && *d.title) ? 1 : 0;
    label_rows = labels ? 1 : 0;
    gutter = d.show_axes ? 6 : 0;
    chart_h = height - title_rows - label_rows - (d.show_axes ? 1 : 0);
    if (chart_h < 1) chart_h = 1;
    chart_w = width - gutter;
    if (chart_w < n) {
        bar_w = 1;
        gap = 0;
    } else {
        avail = chart_w - (n - 1) * gap;
        bar_w = avail / n;
        if (bar_w < 1) { bar_w = 1; gap = 0; }
    }
    total_bw = n * bar_w + (n - 1) * gap;
    if (total_bw > chart_w) total_bw = chart_w;

    if (d.title) _flux_chart_title(sb, d.title, width);

    /* For each row top→bottom */
    for (row = 0; row < chart_h; row++) {
        int filled_left;
        if (d.show_axes) {
            char y_buf[16];
            float v = vmax_pos - (vmax_pos - vmin_neg) *
                      ((float)row / (float)(chart_h ? chart_h : 1));
            flux_fmt_axis(y_buf, sizeof y_buf, v, gutter - 1);
            flux_sb_append(sb, FLUX_AXIS_COLOR);
            flux_fit(sb, y_buf, gutter - 1, NULL, FLUX_ALIGN_RIGHT);
            flux_sb_append(sb, "\xe2\x94\x82");
            flux_sb_append(sb, FLUX_RESET);
        }
        filled_left = 0;
        for (i = 0; i < n; i++) {
            int j;
            float v = values[i];
            int bar_cells_h;
            int full, frac8;
            const char *clr = (d.bar_colors && d.bar_colors[i])
                              ? d.bar_colors[i] : uniform;
            float t = (v - vmin_neg) / vrange;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            bar_cells_h = (int)(t * (float)chart_h * 8.0f + 0.5f);
            full  = bar_cells_h / 8;
            frac8 = bar_cells_h & 7;
            /* the bar occupies the bottom `full` rows fully + 1 partial */
            {
                int row_from_bottom = chart_h - 1 - row;
                const char *glyph;
                if (row_from_bottom < full) {
                    glyph = FLUX_BAR_VBLOCKS[8];
                } else if (row_from_bottom == full && frac8 > 0) {
                    glyph = FLUX_BAR_VBLOCKS[frac8];
                } else {
                    glyph = " ";
                }
                flux_sb_append(sb, clr);
                for (j = 0; j < bar_w; j++) flux_sb_append(sb, glyph);
                flux_sb_append(sb, FLUX_RESET);
            }
            filled_left += bar_w;
            if (i < n - 1 && filled_left + gap <= chart_w) {
                for (j = 0; j < gap; j++) flux_sb_append(sb, " ");
                filled_left += gap;
            }
        }
        for (; filled_left < chart_w; filled_left++) flux_sb_append(sb, " ");
        flux_sb_nl(sb);
    }

    /* axis bottom */
    if (d.show_axes) {
        flux_sb_append(sb, FLUX_AXIS_COLOR);
        for (i = 0; i < gutter - 1; i++) flux_sb_append(sb, " ");
        flux_sb_append(sb, "\xe2\x94\x94");
        for (i = 0; i < chart_w; i++) flux_sb_append(sb, "\xe2\x94\x80");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
    /* labels row */
    if (labels) {
        int filled = 0;
        if (d.show_axes) {
            for (i = 0; i < gutter; i++) flux_sb_append(sb, " ");
        }
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        for (i = 0; i < n; i++) {
            int j;
            const char *lbl = labels[i] ? labels[i] : "";
            flux_fit(sb, lbl, bar_w, NULL, FLUX_ALIGN_CENTER);
            filled += bar_w;
            if (i < n - 1 && filled + gap <= chart_w) {
                for (j = 0; j < gap; j++) flux_sb_append(sb, " ");
                filled += gap;
            }
        }
        for (; filled < chart_w; filled++) flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * 3. line_chart
 * ═════════════════════════════════════════════════════════════════ */

static void _flux_line_chart_render(FluxSB *sb,
                                    const FluxSeries *series, int sn,
                                    int width, int height,
                                    const FluxLineChartOpts *opts) {
    FluxLineChartOpts d;
    int show_axes, gutter;
    int title_rows;
    int chart_w, chart_h;
    int s, i, j;
    float ymin, ymax;
    int pw, ph;
    char y_top[16], y_bot[16];
    enum { MAX_CELLS = 8192 };
    static uint8_t  bits[MAX_CELLS];
    static uint32_t colsbuf[MAX_CELLS];
    FluxBraille cv;
    static float scratch[2048];

    if (opts) d = *opts; else { FluxLineChartOpts z = {0}; d = z; }
    show_axes = d.show_axes;
    gutter = show_axes ? 7 : 0;

    if (!sb || !series || sn <= 0 || width < 8 || height < 3) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    title_rows = (d.title && *d.title) ? 1 : 0;
    chart_h = height - title_rows - (show_axes ? 1 : 0);
    if (chart_h < 1) chart_h = 1;
    chart_w = width - gutter;
    if (chart_w < 4) {
        if (d.title) _flux_chart_title(sb, d.title, width);
        for (i = 0; i < height - title_rows; i++) {
            flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
        }
        return;
    }
    if (chart_h * chart_w > MAX_CELLS) {
        chart_h = MAX_CELLS / chart_w;
        if (chart_h < 1) chart_h = 1;
    }
    pw = chart_w * 2;
    ph = chart_h * 4;

    /* Compute y range over all series */
    if (d.y_min == d.y_max) {
        int first = 1;
        ymin = ymax = 0;
        for (s = 0; s < sn; s++) {
            for (i = 0; i < series[s].n; i++) {
                float v = series[s].data[i];
                if (first) { ymin = ymax = v; first = 0; }
                else {
                    if (v < ymin) ymin = v;
                    if (v > ymax) ymax = v;
                }
            }
        }
        if (ymin == ymax) { ymin -= 1.0f; ymax += 1.0f; }
    } else { ymin = d.y_min; ymax = d.y_max; }
    if (ymax <= ymin) ymax = ymin + 1.0f;

    flux_braille_init(&cv, chart_w, chart_h, bits, colsbuf);
    flux_braille_clear(&cv);

    /* draw each series */
    for (s = 0; s < sn; s++) {
        int dn = pw;
        int last_x = -1, last_y = -1;
        if (dn > 2048) dn = 2048;
        if (series[s].n <= 0) continue;
        flux_resample(series[s].data, series[s].n, scratch, dn);
        for (i = 0; i < dn; i++) {
            float v = scratch[i];
            float t = (v - ymin) / (ymax - ymin);
            int y_p;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            y_p = (int)((1.0f - t) * (float)(ph - 1) + 0.5f);
            if (last_x >= 0)
                flux_braille_line(&cv, last_x, last_y, i, y_p, 0);
            last_x = i;
            last_y = y_p;
            if (d.show_points) {
                /* 3x3 cluster around point */
                int dx, dy;
                for (dx = -1; dx <= 1; dx++)
                    for (dy = -1; dy <= 1; dy++)
                        flux_braille_set_pixel(&cv, i + dx, y_p + dy, 0);
            }
        }
        (void)j;
        /* with single-color cells, default_color carries the series tint */
    }

    /* Title */
    if (d.title) _flux_chart_title(sb, d.title, width);

    flux_fmt_axis(y_top, sizeof y_top, ymax, gutter ? gutter - 1 : 0);
    flux_fmt_axis(y_bot, sizeof y_bot, ymin, gutter ? gutter - 1 : 0);

    /* If multi-series, use first series color as default tint */
    {
        const char *tint = (sn > 0 && series[0].color) ? series[0].color :
                           (d.color ? d.color : FLUX_CHART_PALETTE[0]);
        for (i = 0; i < chart_h; i++) {
            if (show_axes) {
                const char *axis = (i == 0) ? y_top
                                : (i == chart_h - 1) ? y_bot
                                : "      ";
                flux_sb_append(sb, FLUX_AXIS_COLOR);
                flux_fit(sb, axis, gutter - 1, NULL, FLUX_ALIGN_RIGHT);
                flux_sb_append(sb, "\xe2\x94\x82");
                flux_sb_append(sb, FLUX_RESET);
            }
            if (d.show_grid && (i == chart_h / 2)) {
                /* dim grid line drawn under braille — emit braille first then space-pad */
            }
            flux_braille_render_row(&cv, sb, i, tint);
            flux_sb_nl(sb);
        }
    }

    if (show_axes) {
        flux_sb_append(sb, FLUX_AXIS_COLOR);
        for (i = 0; i < gutter - 1; i++) flux_sb_append(sb, " ");
        flux_sb_append(sb, "\xe2\x94\x94");
        for (i = 0; i < chart_w; i++) flux_sb_append(sb, "\xe2\x94\x80");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    if (d.x_labels && d.x_label_n > 0) {
        int filled = 0;
        if (show_axes) for (i = 0; i < gutter; i++) flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        {
            int seg = chart_w / d.x_label_n;
            if (seg < 1) seg = 1;
            for (i = 0; i < d.x_label_n && filled < chart_w; i++) {
                int w = (i == d.x_label_n - 1) ? (chart_w - filled) : seg;
                flux_fit(sb, d.x_labels[i] ? d.x_labels[i] : "",
                         w, NULL, FLUX_ALIGN_LEFT);
                filled += w;
            }
        }
        for (; filled < chart_w; filled++) flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
}

void flux_line_chart(FluxSB *sb, const float *values, int n,
                     int width, int height,
                     const FluxLineChartOpts *opts) {
    FluxSeries s;
    s.data = values;
    s.n = n;
    s.color = (opts && opts->color) ? opts->color : NULL;
    s.name = NULL;
    _flux_line_chart_render(sb, &s, 1, width, height, opts);
}

void flux_line_chart_multi(FluxSB *sb, const FluxSeries *series, int series_n,
                           int width, int height,
                           const FluxLineChartOpts *opts) {
    _flux_line_chart_render(sb, series, series_n, width, height, opts);
}

/* ══════════════════════════════════════════════════════════════════
 * 4. scatter
 * ═════════════════════════════════════════════════════════════════ */

void flux_scatter(FluxSB *sb, const FluxPoint *pts, int n,
                  int width, int height, const FluxScatterOpts *opts) {
    FluxScatterOpts d;
    int show_axes, gutter;
    int title_rows;
    int chart_w, chart_h, pw, ph;
    int i;
    float xmn, xmx, ymn, ymx;
    char y_top[16], y_bot[16];
    enum { MAX_CELLS = 8192 };
    static uint8_t  bits[MAX_CELLS];
    static uint32_t colsbuf[MAX_CELLS];
    FluxBraille cv;

    if (opts) d = *opts; else { FluxScatterOpts z = {0}; d = z; }
    show_axes = d.show_axes;
    gutter = show_axes ? 7 : 0;

    if (!sb || !pts || n <= 0 || width < 8 || height < 3) {
        if (sb) _flux_too_small(sb, width);
        return;
    }

    title_rows = (d.title && *d.title) ? 1 : 0;
    chart_h = height - title_rows - (show_axes ? 1 : 0);
    if (chart_h < 1) chart_h = 1;
    chart_w = width - gutter;
    if (chart_w < 4) {
        if (d.title) _flux_chart_title(sb, d.title, width);
        for (i = 0; i < height - title_rows; i++) {
            flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
        }
        return;
    }
    if (chart_h * chart_w > MAX_CELLS) {
        chart_h = MAX_CELLS / chart_w;
        if (chart_h < 1) chart_h = 1;
    }
    pw = chart_w * 2;
    ph = chart_h * 4;

    /* range */
    if (d.x_min == d.x_max) {
        xmn = xmx = pts[0].x;
        for (i = 1; i < n; i++) {
            if (pts[i].x < xmn) xmn = pts[i].x;
            if (pts[i].x > xmx) xmx = pts[i].x;
        }
        if (xmn == xmx) { xmn -= 1.0f; xmx += 1.0f; }
    } else { xmn = d.x_min; xmx = d.x_max; }
    if (d.y_min == d.y_max) {
        ymn = ymx = pts[0].y;
        for (i = 1; i < n; i++) {
            if (pts[i].y < ymn) ymn = pts[i].y;
            if (pts[i].y > ymx) ymx = pts[i].y;
        }
        if (ymn == ymx) { ymn -= 1.0f; ymx += 1.0f; }
    } else { ymn = d.y_min; ymx = d.y_max; }
    if (xmx <= xmn) xmx = xmn + 1.0f;
    if (ymx <= ymn) ymx = ymn + 1.0f;

    flux_braille_init(&cv, chart_w, chart_h, bits, colsbuf);
    flux_braille_clear(&cv);

    /* plot */
    for (i = 0; i < n; i++) {
        float tx = (pts[i].x - xmn) / (xmx - xmn);
        float ty = (pts[i].y - ymn) / (ymx - ymn);
        int x_p, y_p;
        if (tx < 0 || tx > 1 || ty < 0 || ty > 1) continue;
        x_p = (int)(tx * (float)(pw - 1) + 0.5f);
        y_p = (int)((1.0f - ty) * (float)(ph - 1) + 0.5f);
        flux_braille_set_pixel(&cv, x_p, y_p, 0);
        if (d.dot_size >= 2) {
            flux_braille_set_pixel(&cv, x_p + 1, y_p, 0);
            flux_braille_set_pixel(&cv, x_p, y_p + 1, 0);
            flux_braille_set_pixel(&cv, x_p + 1, y_p + 1, 0);
        }
    }

    /* trend line: simple least squares */
    if (d.show_trend && n >= 2) {
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        double m, b;
        int k;
        float y0, y1;
        int x0p, y0p, x1p, y1p;
        for (i = 0; i < n; i++) {
            sx += pts[i].x; sy += pts[i].y;
            sxx += pts[i].x * pts[i].x;
            sxy += pts[i].x * pts[i].y;
        }
        {
            double denom = (double)n * sxx - sx * sx;
            if (denom != 0) {
                m = ((double)n * sxy - sx * sy) / denom;
                b = (sy - m * sx) / (double)n;
                y0 = (float)(m * xmn + b);
                y1 = (float)(m * xmx + b);
                x0p = 0;
                x1p = pw - 1;
                {
                    float t0 = (y0 - ymn) / (ymx - ymn);
                    float t1 = (y1 - ymn) / (ymx - ymn);
                    if (t0 < 0) t0 = 0;
                    if (t0 > 1) t0 = 1;
                    if (t1 < 0) t1 = 0;
                    if (t1 > 1) t1 = 1;
                    y0p = (int)((1.0f - t0) * (float)(ph - 1) + 0.5f);
                    y1p = (int)((1.0f - t1) * (float)(ph - 1) + 0.5f);
                }
                /* dashed: 3 on, 3 off */
                {
                    int dxp = x1p - x0p;
                    int dyp = y1p - y0p;
                    int adxp = dxp < 0 ? -dxp : dxp;
                    int adyp = dyp < 0 ? -dyp : dyp;
                    int steps = adxp > adyp ? adxp : adyp;
                    if (steps < 1) steps = 1;
                    for (k = 0; k <= steps; k++) {
                        if ((k / 3) & 1) continue;
                        {
                            int xx = x0p + (k * dxp) / steps;
                            int yy = y0p + (k * dyp) / steps;
                            flux_braille_set_pixel(&cv, xx, yy, 0);
                        }
                    }
                }
            }
        }
    }

    if (d.title) _flux_chart_title(sb, d.title, width);
    flux_fmt_axis(y_top, sizeof y_top, ymx, gutter ? gutter - 1 : 0);
    flux_fmt_axis(y_bot, sizeof y_bot, ymn, gutter ? gutter - 1 : 0);

    for (i = 0; i < chart_h; i++) {
        if (show_axes) {
            const char *axis = (i == 0) ? y_top
                            : (i == chart_h - 1) ? y_bot
                            : "      ";
            flux_sb_append(sb, FLUX_AXIS_COLOR);
            flux_fit(sb, axis, gutter - 1, NULL, FLUX_ALIGN_RIGHT);
            flux_sb_append(sb, "\xe2\x94\x82");
            flux_sb_append(sb, FLUX_RESET);
        }
        flux_braille_render_row(&cv, sb, i,
            d.color ? d.color : FLUX_CHART_PALETTE[0]);
        flux_sb_nl(sb);
    }
    if (show_axes) {
        flux_sb_append(sb, FLUX_AXIS_COLOR);
        for (i = 0; i < gutter - 1; i++) flux_sb_append(sb, " ");
        flux_sb_append(sb, "\xe2\x94\x94");
        for (i = 0; i < chart_w; i++) flux_sb_append(sb, "\xe2\x94\x80");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * 5. histogram
 * ═════════════════════════════════════════════════════════════════ */

void flux_histogram(FluxSB *sb, const int *bins, int n,
                    int width, int height, const FluxHistogramOpts *opts) {
    FluxBarChartOpts bo;
    static float vals[512];
    int i;
    FluxHistogramOpts d;
    if (opts) d = *opts; else { FluxHistogramOpts z = {0}; d = z; }
    if (!sb || !bins || n <= 0) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    if (n > 512) n = 512;
    for (i = 0; i < n; i++) vals[i] = (float)bins[i];
    {
        FluxBarChartOpts z = {0};
        bo = z;
    }
    bo.title = d.title;
    bo.color = d.color;
    bo.show_values = d.show_counts;
    bo.show_axes = d.show_axes;
    bo.bar_gap = 0;
    bo.horizontal = 0;
    flux_bar_chart(sb, vals, n, width, height, NULL, &bo);
    if (d.x_label && d.x_label[0]) {
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        flux_fit(sb, d.x_label, width, NULL, FLUX_ALIGN_CENTER);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
}

void flux_histogram_from_samples(FluxSB *sb, const float *samples, int n_samples,
                                 int n_bins, int width, int height,
                                 const FluxHistogramOpts *opts) {
    static int bins[512];
    int i, b;
    float mn, mx, range;
    if (!sb || !samples || n_samples <= 0 || n_bins <= 0) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    if (n_bins > 512) n_bins = 512;
    mn = mx = samples[0];
    for (i = 1; i < n_samples; i++) {
        if (samples[i] < mn) mn = samples[i];
        if (samples[i] > mx) mx = samples[i];
    }
    if (mn == mx) mx = mn + 1.0f;
    range = mx - mn;
    for (i = 0; i < n_bins; i++) bins[i] = 0;
    for (i = 0; i < n_samples; i++) {
        b = (int)(((samples[i] - mn) / range) * (float)n_bins);
        if (b < 0) b = 0;
        if (b >= n_bins) b = n_bins - 1;
        bins[b]++;
    }
    flux_histogram(sb, bins, n_bins, width, height, opts);
}

/* ══════════════════════════════════════════════════════════════════
 * 6. heatmap
 * ═════════════════════════════════════════════════════════════════ */

void flux_heatmap(FluxSB *sb, const float *matrix,
                  int rows, int cols, int width,
                  const FluxHeatmapOpts *opts) {
    FluxHeatmapOpts d;
    int cell_w;
    int max_label_w = 0;
    int label_gutter;
    int avail;
    int r, c;
    float vmn, vmx;
    int i;

    if (opts) d = *opts; else { FluxHeatmapOpts z = {0}; d = z; }
    if (!sb || !matrix || rows <= 0 || cols <= 0 || width < 4) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    cell_w = d.cell_w > 0 ? d.cell_w : 3;

    if (d.row_labels) {
        for (r = 0; r < rows; r++) {
            int w = d.row_labels[r] ? flux_strwidth(d.row_labels[r]) : 0;
            if (w > max_label_w) max_label_w = w;
        }
    }
    label_gutter = max_label_w > 0 ? max_label_w + 1 : 0;
    avail = width - label_gutter;
    if (avail < cols) {
        cell_w = 1;
    } else {
        int max_cw = avail / cols;
        if (cell_w > max_cw) cell_w = max_cw;
        if (cell_w < 1) cell_w = 1;
    }

    if (d.v_min == d.v_max) {
        int n = rows * cols;
        vmn = vmx = matrix[0];
        for (i = 1; i < n; i++) {
            if (matrix[i] < vmn) vmn = matrix[i];
            if (matrix[i] > vmx) vmx = matrix[i];
        }
        if (vmn == vmx) { vmn -= 1.0f; vmx += 1.0f; }
    } else { vmn = d.v_min; vmx = d.v_max; }
    if (vmx <= vmn) vmx = vmn + 1.0f;

    if (d.title) _flux_chart_title(sb, d.title, width);

    /* col labels */
    if (d.col_labels) {
        int filled = 0;
        if (label_gutter) { _flux_pad_n(sb, label_gutter); filled += label_gutter; }
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        for (c = 0; c < cols && filled + cell_w <= width; c++) {
            flux_fit(sb, d.col_labels[c] ? d.col_labels[c] : "",
                     cell_w, NULL, FLUX_ALIGN_CENTER);
            filled += cell_w;
        }
        for (; filled < width; filled++) flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    for (r = 0; r < rows; r++) {
        int filled = 0;
        if (label_gutter) {
            flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
            flux_fit(sb, d.row_labels && d.row_labels[r] ? d.row_labels[r] : "",
                     label_gutter - 1, NULL, FLUX_ALIGN_RIGHT);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, " ");
            filled += label_gutter;
        }
        for (c = 0; c < cols && filled + cell_w <= width; c++) {
            float v = matrix[r * cols + c];
            float t = (v - vmn) / (vmx - vmn);
            const char *clr = flux_heatmap_color(t);
            char bg[32];
            int bright = t > 0.55f;
            int br, bg_ch, bb;
            int rr = 0, gg = 0, bb2 = 0;
            (void)br; (void)bg_ch; (void)bb;
            /* synth bg from ramp index */
            {
                int idx = (int)(t * 5.0f + 0.5f);
                if (idx < 0) idx = 0;
                if (idx > 5) idx = 5;
                /* match ramp xterm-256 → roughly RGB */
                {
                    static const int RAMP_RGB[6][3] = {
                        { 30, 30, 30 }, { 48, 48, 48 }, { 95, 95, 0 },
                        { 175, 95, 0 }, { 255, 95, 0 }, { 255, 135, 0 }
                    };
                    rr = RAMP_RGB[idx][0];
                    gg = RAMP_RGB[idx][1];
                    bb2 = RAMP_RGB[idx][2];
                }
            }
            snprintf(bg, sizeof bg, "\x1b[48;2;%d;%d;%dm", rr, gg, bb2);
            flux_sb_append(sb, bg);
            flux_sb_append(sb, bright ? "\x1b[38;2;0;0;0m" : "\x1b[38;2;255;255;255m");
            (void)clr;
            if (d.show_values && cell_w >= 3) {
                char buf[16];
                snprintf(buf, sizeof buf, "%.1f", v);
                flux_fit(sb, buf, cell_w, NULL, FLUX_ALIGN_CENTER);
            } else {
                int j;
                for (j = 0; j < cell_w; j++) flux_sb_append(sb, " ");
            }
            flux_sb_append(sb, FLUX_RESET);
            filled += cell_w;
        }
        for (; filled < width; filled++) flux_sb_append(sb, " ");
        flux_sb_nl(sb);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * 7. gauge
 * ═════════════════════════════════════════════════════════════════ */

void flux_gauge(FluxSB *sb, float value, float min, float max,
                int width, const FluxGaugeOpts *opts) {
    FluxGaugeOpts d;
    float t;
    const char *fill_clr;
    int label_w, val_w, bar_room;
    int filled = 0;
    int i;

    if (opts) d = *opts; else { FluxGaugeOpts z = {0}; d = z; }
    if (!sb || width < 4) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    if (max <= min) max = min + 1.0f;
    t = (value - min) / (max - min);
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    fill_clr = d.color ? d.color : FLUX_THEME_BRAND_PURPLE_FG;
    if (d.thresh_at && d.thresh_color && d.thresh_n > 0) {
        for (i = 0; i < d.thresh_n; i++) {
            if (value >= d.thresh_at[i]) fill_clr = d.thresh_color[i];
        }
    }

    /* arc layout */
    if (d.arc && width >= 11) {
        int arc_w = width;
        int center = arc_w / 2;
        int active = (int)(t * (float)arc_w + 0.5f);
        const char *active_clr = fill_clr;
        const char *dim_clr = FLUX_THEME_TEXT_DIM_FG;

        /* row 1: ╭─...─╮ */
        flux_sb_append(sb, dim_clr);
        flux_sb_append(sb, "\xe2\x95\xad"); filled = 1;
        for (i = 1; i < arc_w - 1; i++) {
            int act = (i < active);
            if (act) flux_sb_append(sb, active_clr); else flux_sb_append(sb, dim_clr);
            flux_sb_append(sb, "\xe2\x94\x80");
        }
        flux_sb_append(sb, dim_clr);
        flux_sb_append(sb, "\xe2\x95\xae");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);

        /* row 2: │   ◯   │  with label/value centered */
        {
            char buf[32];
            int n;
            const char *lbl = d.label ? d.label : "";
            if (d.show_value) {
                n = snprintf(buf, sizeof buf, "%s %d%%",
                             lbl, (int)(t * 100.0f + 0.5f));
            } else {
                n = snprintf(buf, sizeof buf, "%s", lbl);
            }
            (void)n;
            flux_sb_append(sb, dim_clr);
            flux_sb_append(sb, "\xe2\x94\x82");
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, fill_clr);
            flux_fit(sb, buf, arc_w - 2, NULL, FLUX_ALIGN_CENTER);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, dim_clr);
            flux_sb_append(sb, "\xe2\x94\x82");
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);
        }
        /* row 3: ╰─...─╯ with center marker */
        flux_sb_append(sb, dim_clr);
        flux_sb_append(sb, "\xe2\x95\xb0"); /* ╰ */
        for (i = 1; i < arc_w - 1; i++) {
            if (i == center) {
                flux_sb_append(sb, fill_clr);
                flux_sb_append(sb, "\xe2\x97\xaf"); /* ◯ */
                flux_sb_append(sb, dim_clr);
            } else {
                flux_sb_append(sb, "\xe2\x94\x80");
            }
        }
        flux_sb_append(sb, "\xe2\x95\xaf"); /* ╯ */
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
        return;
    }

    /* bar layout */
    label_w = (d.label && *d.label) ? flux_strwidth(d.label) + 1 : 0;
    val_w = d.show_value ? 5 : 0;
    bar_room = width - label_w - val_w;
    if (bar_room < 1) bar_room = 1;

    if (label_w) {
        flux_sb_append(sb, FLUX_THEME_TEXT2_FG);
        flux_sb_append(sb, d.label);
        flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_RESET);
        filled += label_w;
    }
    {
        int cells8 = (int)(t * (float)bar_room * 8.0f + 0.5f);
        int full = cells8 / 8;
        int frac = cells8 & 7;
        flux_sb_append(sb, fill_clr);
        for (i = 0; i < full && filled < width - val_w; i++) {
            flux_sb_append(sb, "\xe2\x96\x88");
            filled++;
        }
        if (frac && filled < width - val_w) {
            /* pick mid-block from the 1-7 sub-block range */
            static const char *PARTIALS[8] = {
                " ", "\xe2\x96\x91", "\xe2\x96\x91", "\xe2\x96\x92",
                "\xe2\x96\x92", "\xe2\x96\x92", "\xe2\x96\x93", "\xe2\x96\x93"
            };
            flux_sb_append(sb, PARTIALS[frac]);
            filled++;
        }
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        for (; filled < width - val_w; filled++) flux_sb_append(sb, "\xe2\x96\x91");
        flux_sb_append(sb, FLUX_RESET);
    }
    if (d.show_value) {
        char buf[16];
        snprintf(buf, sizeof buf, " %d%%", (int)(t * 100.0f + 0.5f));
        flux_sb_append(sb, FLUX_THEME_TEXT_FG);
        flux_fit(sb, buf, val_w, NULL, FLUX_ALIGN_RIGHT);
        flux_sb_append(sb, FLUX_RESET);
    }
    flux_sb_nl(sb);
}

/* ══════════════════════════════════════════════════════════════════
 * 8. dl (definition list)
 * ═════════════════════════════════════════════════════════════════ */

void flux_dl(FluxSB *sb, const FluxDLItem *items, int n,
             int width, const FluxDLOpts *opts) {
    FluxDLOpts d;
    int i;
    int term_w;
    const char *term_clr;

    if (opts) d = *opts; else { FluxDLOpts z = {0}; d = z; }
    if (!sb || !items || n <= 0 || width < 4) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    term_clr = d.term_color ? d.term_color : FLUX_THEME_BRAND_PURPLE_FG;

    if (d.layout == 1) {
        /* stacked */
        for (i = 0; i < n; i++) {
            flux_sb_append(sb, "\x1b[1m");
            flux_sb_append(sb, term_clr);
            flux_fit(sb, items[i].term ? items[i].term : "",
                     width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);
            flux_sb_append(sb, "  ");
            flux_sb_append(sb, FLUX_THEME_TEXT_FG);
            flux_fit(sb, items[i].def ? items[i].def : "",
                     width - 2, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);
            if (d.separator_line && i < n - 1) {
                flux_divider(sb, width, FLUX_THEME_DIVIDER_FG);
                flux_sb_nl(sb);
            }
        }
        return;
    }

    /* inline */
    term_w = 0;
    for (i = 0; i < n; i++) {
        int tw = items[i].term ? flux_strwidth(items[i].term) : 0;
        if (tw > term_w) term_w = tw;
    }
    term_w += 2;
    if (term_w > width / 2) term_w = width / 2;

    for (i = 0; i < n; i++) {
        flux_sb_append(sb, term_clr);
        flux_fit(sb, items[i].term ? items[i].term : "",
                 term_w, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_append(sb, "  ");
        flux_sb_append(sb, FLUX_THEME_TEXT_FG);
        flux_fit(sb, items[i].def ? items[i].def : "",
                 width - term_w - 2, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
        if (d.separator_line && i < n - 1) {
            flux_divider(sb, width, FLUX_THEME_DIVIDER_FG);
            flux_sb_nl(sb);
        }
    }
}

/* ══════════════════════════════════════════════════════════════════
 * 9. pretty (JSON-ish)
 * ═════════════════════════════════════════════════════════════════ */

static int _flux_pp_isnumstart(char c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '+';
}
static int _flux_pp_isws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void flux_pretty(FluxSB *sb, const char *json_text,
                 int width, const FluxPrettyOpts *opts) {
    FluxPrettyOpts d;
    int indent;
    int color;
    int max_depth;
    int depth = 0;
    int line_w = 0;
    const char *p;
    const char *KEY_CLR = "\x1b[38;5;51m";
    const char *STR_CLR = "\x1b[38;5;46m";
    const char *NUM_CLR = "\x1b[38;5;226m";
    const char *LIT_CLR = "\x1b[38;5;201m";
    int last_was_string = 0;
    int last_string_start_line = 0; /* unused */

    if (opts) d = *opts; else { FluxPrettyOpts z = {0}; d = z; }
    indent = d.indent > 0 ? d.indent : 2;
    color = d.color;
    max_depth = d.max_depth;
    (void)last_string_start_line;

    if (!sb || !json_text || width < 4) {
        if (sb) _flux_too_small(sb, width);
        return;
    }

    p = json_text;
    while (*p) {
        char c = *p;
        if (_flux_pp_isws(c)) { p++; continue; }

        /* opening */
        if (c == '{' || c == '[') {
            char ch[2] = { c, '\0' };
            flux_sb_append(sb, ch);
            line_w++;
            if (max_depth == 0 || depth < max_depth) {
                depth++;
                flux_sb_nl(sb);
                line_w = depth * indent;
                _flux_pad_n(sb, line_w);
            }
            p++;
            last_was_string = 0;
            continue;
        }
        if (c == '}' || c == ']') {
            if (max_depth == 0 || depth <= max_depth) {
                if (depth > 0) depth--;
                flux_sb_nl(sb);
                line_w = depth * indent;
                _flux_pad_n(sb, line_w);
            }
            {
                char ch[2] = { c, '\0' };
                flux_sb_append(sb, ch);
                line_w++;
            }
            p++;
            last_was_string = 0;
            continue;
        }
        if (c == ',') {
            flux_sb_append(sb, ",");
            line_w++;
            flux_sb_nl(sb);
            line_w = depth * indent;
            _flux_pad_n(sb, line_w);
            p++;
            last_was_string = 0;
            continue;
        }
        if (c == ':') {
            flux_sb_append(sb, ": ");
            line_w += 2;
            p++;
            last_was_string = 0;
            continue;
        }
        if (c == '"') {
            /* string token */
            const char *start = p;
            int slen;
            const char *qc;
            char buf[256];
            int avail = width - line_w;
            if (avail < 4) avail = 4;
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p += 2;
                else p++;
            }
            if (*p == '"') p++;
            slen = (int)(p - start);
            if (slen > (int)sizeof(buf) - 1) slen = (int)sizeof(buf) - 1;
            memcpy(buf, start, (size_t)slen);
            buf[slen] = '\0';
            /* check if followed by ':' → key */
            qc = p;
            while (*qc && _flux_pp_isws(*qc)) qc++;
            if (*qc == ':') {
                if (color) flux_sb_append(sb, KEY_CLR);
            } else {
                if (color) flux_sb_append(sb, STR_CLR);
            }
            if (slen > avail) {
                /* truncate mid-token */
                char tbuf[256];
                int n = avail - 1;
                if (n < 1) n = 1;
                if (n > (int)sizeof(tbuf) - 4) n = (int)sizeof(tbuf) - 4;
                memcpy(tbuf, buf, (size_t)n);
                tbuf[n] = '\xe2'; tbuf[n+1] = '\x80'; tbuf[n+2] = '\xa6';
                tbuf[n+3] = '\0';
                flux_sb_append(sb, tbuf);
                line_w += n + 1;
            } else {
                flux_sb_append(sb, buf);
                line_w += slen;
            }
            if (color) flux_sb_append(sb, FLUX_RESET);
            last_was_string = 1;
            continue;
        }
        if (_flux_pp_isnumstart(c) ||
            ((c == '.') && p[1] >= '0' && p[1] <= '9')) {
            const char *start = p;
            int slen;
            char buf[64];
            while (*p && (
                (*p >= '0' && *p <= '9') ||
                *p == '-' || *p == '+' || *p == '.' ||
                *p == 'e' || *p == 'E')) p++;
            slen = (int)(p - start);
            if (slen > (int)sizeof(buf) - 1) slen = (int)sizeof(buf) - 1;
            memcpy(buf, start, (size_t)slen);
            buf[slen] = '\0';
            if (color) flux_sb_append(sb, NUM_CLR);
            flux_sb_append(sb, buf);
            if (color) flux_sb_append(sb, FLUX_RESET);
            line_w += slen;
            last_was_string = 0;
            continue;
        }
        /* literal: true / false / null */
        if ((c == 't' && p[1]=='r' && p[2]=='u' && p[3]=='e') ||
            (c == 'f' && p[1]=='a' && p[2]=='l' && p[3]=='s' && p[4]=='e') ||
            (c == 'n' && p[1]=='u' && p[2]=='l' && p[3]=='l')) {
            int kn = (c == 'f') ? 5 : 4;
            char buf[8];
            memcpy(buf, p, (size_t)kn);
            buf[kn] = '\0';
            if (color) flux_sb_append(sb, LIT_CLR);
            flux_sb_append(sb, buf);
            if (color) flux_sb_append(sb, FLUX_RESET);
            line_w += kn;
            p += kn;
            last_was_string = 0;
            continue;
        }
        /* fallback verbatim */
        {
            char ch[2] = { c, '\0' };
            flux_sb_append(sb, ch);
            line_w++;
            p++;
        }
        last_was_string = 0;
    }
    (void)last_was_string;
    flux_sb_nl(sb);
}

/* ══════════════════════════════════════════════════════════════════
 * 10. FluxRichLog
 * ═════════════════════════════════════════════════════════════════ */

void flux_richlog_init(FluxRichLog *l, FluxLogLine *buf, int cap) {
    int i;
    if (!l) return;
    l->ring = buf;
    l->cap = cap;
    l->head = 0;
    l->count = 0;
    l->show_timestamp = 1;
    l->show_level = 1;
    l->autoscroll = 1;
    flux_scroll_init(&l->scroll);
    if (buf) for (i = 0; i < cap; i++) {
        buf[i].text[0] = '\0';
        buf[i].ts_ms = 0;
        buf[i].level = FLUX_LOG_INFO;
    }
}

static void _flux_richlog_push(FluxRichLog *l, FluxLogLevel lvl,
                               const char *text) {
    int slot;
    if (!l || !l->ring || l->cap <= 0) return;
    slot = (l->head + l->count) % l->cap;
    if (l->count == l->cap) {
        /* overwrite oldest */
        slot = l->head;
        l->head = (l->head + 1) % l->cap;
    } else {
        l->count++;
    }
    l->ring[slot].level = lvl;
    l->ring[slot].ts_ms = 0;
    {
        int i;
        for (i = 0; i < (int)sizeof(l->ring[slot].text) - 1 && text && text[i]; i++) {
            l->ring[slot].text[i] = text[i];
        }
        l->ring[slot].text[i] = '\0';
    }
}

void flux_richlog_add(FluxRichLog *l, FluxLogLevel lvl, const char *text) {
    _flux_richlog_push(l, lvl, text);
}

void flux_richlog_addf(FluxRichLog *l, FluxLogLevel lvl, const char *fmt, ...) {
    char buf[240];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    _flux_richlog_push(l, lvl, buf);
}

void flux_richlog_clear(FluxRichLog *l) {
    if (!l) return;
    l->head = 0;
    l->count = 0;
}

int flux_richlog_update(FluxRichLog *l, FluxMsg msg) {
    if (!l) return 0;
    return flux_scroll_update(&l->scroll, msg);
}

void flux_richlog_render(FluxRichLog *l, FluxSB *sb, int width, int viewport_h) {
    int start, end, i, row;
    int rows_emitted = 0;
    static const char *const LVL_CLR[5] = {
        FLUX_THEME_TEXT_DIM_FG, /* DEBUG */
        FLUX_THEME_TEXT_FG,     /* INFO */
        FLUX_THEME_WARN_FG,     /* WARN */
        FLUX_THEME_ERR_FG,      /* ERROR */
        FLUX_THEME_OK_FG        /* OK */
    };
    static const char *const LVL_TAG[5] = {
        "DEBUG", "INFO ", "WARN ", "ERROR", "OK   "
    };

    if (!l || !sb || width < 4 || viewport_h < 1) return;

    if (l->autoscroll) {
        l->scroll.scroll = l->count > viewport_h ? l->count - viewport_h : 0;
    }
    start = l->scroll.scroll;
    if (start < 0) start = 0;
    if (start > l->count) start = l->count;
    end = start + viewport_h;
    if (end > l->count) end = l->count;

    for (row = start; row < end; row++) {
        int idx = (l->head + row) % l->cap;
        FluxLogLine *ln = &l->ring[idx];
        int used = 0;
        if (l->show_timestamp) {
            char ts[24];
            int ms = ln->ts_ms;
            int s = ms / 1000;
            int mm = s / 60;
            int ss = s % 60;
            int rr = ms % 1000;
            snprintf(ts, sizeof ts, "[%02d:%02d.%03d] ", mm, ss, rr);
            flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
            flux_sb_append(sb, ts);
            flux_sb_append(sb, FLUX_RESET);
            used += 13;
        }
        if (l->show_level) {
            int lv = (int)ln->level;
            if (lv < 0) lv = 0;
            if (lv > 4) lv = 4;
            flux_sb_append(sb, LVL_CLR[lv]);
            if (lv == FLUX_LOG_ERROR) flux_sb_append(sb, "\x1b[1m");
            flux_sb_append(sb, LVL_TAG[lv]);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, " ");
            used += 6;
        }
        {
            int rem = width - used;
            if (rem < 1) rem = 1;
            flux_sb_append(sb, FLUX_THEME_TEXT_FG);
            flux_fit(sb, ln->text, rem, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
            flux_sb_append(sb, FLUX_RESET);
        }
        flux_sb_nl(sb);
        rows_emitted++;
    }
    /* pad */
    for (i = rows_emitted; i < viewport_h; i++) {
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }
    l->scroll.total_lines = l->count;
    l->scroll.viewport_h = viewport_h;
}

/* ══════════════════════════════════════════════════════════════════
 * 11. FluxTree
 * ═════════════════════════════════════════════════════════════════ */

static int _flux_tree_visible(const FluxTree *t, int idx) {
    int i;
    int min_depth;
    if (idx < 0 || idx >= t->count) return 0;
    if (t->nodes[idx].depth == 0) return 1;
    /* walk back: if any ancestor is collapsed, hidden */
    min_depth = t->nodes[idx].depth;
    for (i = idx - 1; i >= 0; i--) {
        if (t->nodes[i].depth < min_depth) {
            min_depth = t->nodes[i].depth;
            if (t->nodes[i].is_branch && t->collapsed &&
                (t->collapsed[i >> 3] & (1u << (i & 7))))
                return 0;
            if (min_depth == 0) return 1;
        }
    }
    return 1;
}

static int _flux_tree_next_visible(const FluxTree *t, int from, int dir) {
    int i = from + dir;
    while (i >= 0 && i < t->count) {
        if (_flux_tree_visible(t, i)) return i;
        i += dir;
    }
    return from;
}

void flux_tree_init(FluxTree *t, const FluxTreeNode *nodes, int count,
                    uint8_t *collapsed_buf, int visible) {
    int i, bytes;
    if (!t) return;
    t->nodes = nodes;
    t->count = count;
    t->cursor = 0;
    t->scroll = 0;
    t->visible = visible;
    t->collapsed = collapsed_buf;
    if (collapsed_buf) {
        bytes = (count + 7) / 8;
        for (i = 0; i < bytes; i++) collapsed_buf[i] = 0;
    }
}

int flux_tree_update(FluxTree *t, FluxMsg msg) {
    if (!t || msg.type != MSG_KEY || t->count <= 0) return 0;
    if (flux_key_is(msg, "up") || msg.u.key.key[0] == 'k') {
        t->cursor = _flux_tree_next_visible(t, t->cursor, -1);
        if (t->cursor < t->scroll) t->scroll = t->cursor;
        return 1;
    }
    if (flux_key_is(msg, "down") || msg.u.key.key[0] == 'j') {
        t->cursor = _flux_tree_next_visible(t, t->cursor, +1);
        return 1;
    }
    if (flux_key_is(msg, "right") || flux_key_is(msg, "enter") ||
        msg.u.key.key[0] == 'l') {
        if (t->collapsed && t->nodes[t->cursor].is_branch) {
            int byte = t->cursor >> 3;
            uint8_t mask = (uint8_t)(1u << (t->cursor & 7));
            t->collapsed[byte] &= (uint8_t)~mask;  /* expand */
            return 1;
        }
    }
    if (flux_key_is(msg, "left") || msg.u.key.key[0] == 'h') {
        if (t->collapsed && t->nodes[t->cursor].is_branch) {
            int byte = t->cursor >> 3;
            uint8_t mask = (uint8_t)(1u << (t->cursor & 7));
            if (!(t->collapsed[byte] & mask)) {
                t->collapsed[byte] |= mask;
                return 1;
            }
        }
        /* otherwise: jump to parent */
        {
            int d = t->nodes[t->cursor].depth;
            int i;
            if (d == 0) return 0;
            for (i = t->cursor - 1; i >= 0; i--) {
                if (t->nodes[i].depth < d) {
                    t->cursor = i;
                    if (t->cursor < t->scroll) t->scroll = t->cursor;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void flux_tree_render(FluxTree *t, FluxSB *sb, int width) {
    int rendered = 0;
    int i;
    if (!t || !sb || t->count <= 0 || width < 4) return;
    /* clamp scroll up to cursor */
    if (t->cursor < t->scroll) t->scroll = t->cursor;
    /* also clamp by skipping forward */
    {
        int v = 0;
        int first_visible = -1;
        for (i = 0; i < t->count; i++) {
            if (!_flux_tree_visible(t, i)) continue;
            if (v == t->scroll) { first_visible = i; break; }
            v++;
        }
        if (first_visible < 0) first_visible = 0;
        for (i = first_visible; i < t->count && rendered < t->visible; i++) {
            const FluxTreeNode *n;
            int j;
            int used = 0;
            int sel;
            if (!_flux_tree_visible(t, i)) continue;
            n = &t->nodes[i];
            sel = (i == t->cursor);
            if (sel) flux_sb_append(sb, "\x1b[7m"); /* inverse */
            for (j = 0; j < n->depth * 2 && used < width; j++) {
                flux_sb_append(sb, " ");
                used++;
            }
            if (used + 2 <= width) {
                if (n->is_branch) {
                    int collapsed = (t->collapsed &&
                        (t->collapsed[i >> 3] & (1u << (i & 7))));
                    flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
                    flux_sb_append(sb, collapsed ? "\xe2\x96\xb8" : "\xe2\x96\xbe");
                } else {
                    flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
                    flux_sb_append(sb, "\xc2\xb7"); /* · */
                }
                flux_sb_append(sb, " ");
                if (!sel) flux_sb_append(sb, FLUX_RESET);
                used += 2;
            }
            flux_sb_append(sb, sel ? "\x1b[7m" : FLUX_THEME_TEXT_FG);
            {
                int rem = width - used;
                if (rem < 1) rem = 1;
                flux_fit(sb, n->label ? n->label : "", rem,
                         "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
            }
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);
            rendered++;
        }
    }
    /* pad */
    for (i = rendered; i < t->visible; i++) {
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }
}

int flux_tree_selected(const FluxTree *t) {
    if (!t) return -1;
    return t->cursor;
}

/* ══════════════════════════════════════════════════════════════════
 * 12. FluxVirtualList
 * ═════════════════════════════════════════════════════════════════ */

void flux_vlist_init(FluxVirtualList *l, int count, int visible,
                     FluxVListItemFn render) {
    if (!l) return;
    l->count = count;
    l->visible = visible;
    l->cursor = 0;
    l->scroll = 0;
    l->item_height = 1;
    l->render_item = render;
}

void flux_vlist_set_count(FluxVirtualList *l, int new_count) {
    if (!l) return;
    l->count = new_count;
    if (l->cursor >= new_count) l->cursor = new_count - 1;
    if (l->cursor < 0) l->cursor = 0;
    if (l->scroll > l->cursor) l->scroll = l->cursor;
}

static void _flux_vlist_clamp(FluxVirtualList *l) {
    int ih = l->item_height > 0 ? l->item_height : 1;
    int rows_per_view = l->visible / ih;
    if (rows_per_view < 1) rows_per_view = 1;
    if (l->cursor < 0) l->cursor = 0;
    if (l->cursor >= l->count) l->cursor = l->count - 1;
    if (l->cursor < l->scroll) l->scroll = l->cursor;
    if (l->cursor >= l->scroll + rows_per_view)
        l->scroll = l->cursor - rows_per_view + 1;
    if (l->scroll < 0) l->scroll = 0;
}

int flux_vlist_update(FluxVirtualList *l, FluxMsg msg) {
    int rows_per_view;
    int ih;
    if (!l || msg.type != MSG_KEY || l->count <= 0) return 0;
    ih = l->item_height > 0 ? l->item_height : 1;
    rows_per_view = l->visible / ih;
    if (rows_per_view < 1) rows_per_view = 1;
    if (flux_key_is(msg, "up") || msg.u.key.key[0] == 'k') {
        if (l->cursor > 0) { l->cursor--; _flux_vlist_clamp(l); return 1; }
    }
    if (flux_key_is(msg, "down") || msg.u.key.key[0] == 'j') {
        if (l->cursor < l->count - 1) { l->cursor++; _flux_vlist_clamp(l); return 1; }
    }
    if (flux_key_is(msg, "pgup")) {
        l->cursor -= rows_per_view;
        _flux_vlist_clamp(l);
        return 1;
    }
    if (flux_key_is(msg, "pgdown")) {
        l->cursor += rows_per_view;
        _flux_vlist_clamp(l);
        return 1;
    }
    if (msg.u.key.key[0] == 'g' && msg.u.key.key[1] == '\0') {
        l->cursor = 0;
        l->scroll = 0;
        return 1;
    }
    if (msg.u.key.key[0] == 'G' && msg.u.key.key[1] == '\0') {
        l->cursor = l->count - 1;
        _flux_vlist_clamp(l);
        return 1;
    }
    if (flux_key_is(msg, "home")) {
        l->cursor = 0; l->scroll = 0;
        return 1;
    }
    if (flux_key_is(msg, "end")) {
        l->cursor = l->count - 1;
        _flux_vlist_clamp(l);
        return 1;
    }
    return 0;
}

void flux_vlist_render(FluxVirtualList *l, FluxSB *sb, int width, void *ctx) {
    int ih, rows_per_view, i, end, painted = 0;
    if (!l || !sb || !l->render_item) return;
    if (l->count <= 0 || width < 1 || l->visible < 1) return;
    ih = l->item_height > 0 ? l->item_height : 1;
    rows_per_view = l->visible / ih;
    if (rows_per_view < 1) rows_per_view = 1;
    _flux_vlist_clamp(l);
    end = l->scroll + rows_per_view;
    if (end > l->count) end = l->count;
    for (i = l->scroll; i < end; i++) {
        int sel = (i == l->cursor);
        l->render_item(sb, i, sel, width, ctx);
        flux_sb_nl(sb);
        painted += ih;
    }
    for (i = painted; i < l->visible; i++) {
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * 13. FluxDataGrid
 * ═════════════════════════════════════════════════════════════════ */

void flux_dg_init(FluxDataGrid *g, FluxDGCol *cols, int col_count,
                  int row_count, int page_size,
                  int *row_order_buf, FluxDGCellFn cell) {
    int i;
    if (!g) return;
    g->cols = cols;
    g->col_count = col_count;
    g->row_count = row_count;
    g->page_size = page_size > 0 ? page_size : 10;
    g->page = 0;
    g->cursor_row = 0;
    g->cursor_col = 0;
    g->sort_col = -1;
    g->cell_text = cell;
    g->compare = NULL;
    g->row_order = row_order_buf;
    if (row_order_buf) {
        for (i = 0; i < row_count; i++) row_order_buf[i] = i;
    }
}

void flux_dg_set_rows(FluxDataGrid *g, int row_count) {
    int i;
    if (!g) return;
    g->row_count = row_count;
    if (g->row_order) {
        for (i = 0; i < row_count; i++) g->row_order[i] = i;
    }
    if (g->page * g->page_size >= row_count) g->page = 0;
}

/* simple insertion sort — adequate for moderate rows w/o malloc */
void flux_dg_sort_by(FluxDataGrid *g, int col, FluxDGSort dir, void *ctx) {
    int i, j, ka;
    if (!g || !g->row_order || g->row_count <= 1) return;
    if (col < 0 || col >= g->col_count) return;
    g->sort_col = col;
    g->cols[col].sort = dir;
    if (dir == FLUX_DG_NONE) {
        for (i = 0; i < g->row_count; i++) g->row_order[i] = i;
        g->sort_col = -1;
        return;
    }
    for (i = 1; i < g->row_count; i++) {
        ka = g->row_order[i];
        j = i - 1;
        while (j >= 0) {
            int cmp;
            if (g->compare) {
                cmp = g->compare(g->row_order[j], ka, col, ctx);
            } else if (g->cell_text) {
                const char *a = g->cell_text(g->row_order[j], col, ctx);
                const char *b = g->cell_text(ka, col, ctx);
                cmp = strcmp(a ? a : "", b ? b : "");
            } else cmp = 0;
            if (dir == FLUX_DG_DESC) cmp = -cmp;
            if (cmp > 0) {
                g->row_order[j + 1] = g->row_order[j];
                j--;
            } else break;
        }
        g->row_order[j + 1] = ka;
    }
}

int flux_dg_update(FluxDataGrid *g, FluxMsg msg, void *ctx) {
    int n_pages;
    if (!g || msg.type != MSG_KEY) return 0;
    n_pages = (g->row_count + g->page_size - 1) / g->page_size;
    if (n_pages < 1) n_pages = 1;
    if (flux_key_is(msg, "up") && g->cursor_row > 0) {
        g->cursor_row--;
        return 1;
    }
    if (flux_key_is(msg, "down") &&
        g->cursor_row < g->page_size - 1 &&
        g->page * g->page_size + g->cursor_row < g->row_count - 1) {
        g->cursor_row++;
        return 1;
    }
    if (flux_key_is(msg, "left") && g->cursor_col > 0) {
        g->cursor_col--;
        return 1;
    }
    if (flux_key_is(msg, "right") && g->cursor_col < g->col_count - 1) {
        g->cursor_col++;
        return 1;
    }
    if (flux_key_is(msg, "pgup")) {
        if (g->page > 0) g->page--;
        return 1;
    }
    if (flux_key_is(msg, "pgdown")) {
        if (g->page < n_pages - 1) g->page++;
        return 1;
    }
    if (flux_key_is(msg, "home")) {
        g->page = 0; g->cursor_row = 0;
        return 1;
    }
    if (flux_key_is(msg, "end")) {
        g->page = n_pages - 1;
        g->cursor_row = 0;
        return 1;
    }
    if (msg.u.key.key[0] == 's' && msg.u.key.key[1] == '\0') {
        FluxDGSort cur = g->cols[g->cursor_col].sort;
        FluxDGSort nxt = (cur == FLUX_DG_NONE) ? FLUX_DG_ASC
                       : (cur == FLUX_DG_ASC)  ? FLUX_DG_DESC
                       :                          FLUX_DG_NONE;
        flux_dg_sort_by(g, g->cursor_col, nxt, ctx);
        return 1;
    }
    if (msg.u.key.key[0] == '+' && msg.u.key.key[1] == '\0') {
        g->cols[g->cursor_col].width++;
        return 1;
    }
    if (msg.u.key.key[0] == '-' && msg.u.key.key[1] == '\0') {
        FluxDGCol *c = &g->cols[g->cursor_col];
        int mn = c->min_width > 0 ? c->min_width : 3;
        if (c->width > mn) c->width--;
        return 1;
    }
    return 0;
}

void flux_dg_render(FluxDataGrid *g, FluxSB *sb, int width, void *ctx) {
    int i, c, used;
    int start, end;
    int n_pages;
    char foot[128];

    if (!g || !sb || width < 8) {
        if (sb) _flux_too_small(sb, width);
        return;
    }
    n_pages = (g->row_count + g->page_size - 1) / g->page_size;
    if (n_pages < 1) n_pages = 1;

    /* header */
    used = 0;
    flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(sb, "\x1b[1m");
    for (c = 0; c < g->col_count && used < width; c++) {
        int cw = g->cols[c].width;
        char hd[64];
        const char *arrow =
            (g->cols[c].sort == FLUX_DG_ASC)  ? " \xe2\x96\xb2" :
            (g->cols[c].sort == FLUX_DG_DESC) ? " \xe2\x96\xbc" : "";
        snprintf(hd, sizeof hd, "%s%s",
                 g->cols[c].title ? g->cols[c].title : "", arrow);
        if (used + cw > width) cw = width - used;
        if (cw < 1) break;
        flux_fit(sb, hd, cw, NULL, FLUX_ALIGN_LEFT);
        used += cw;
        if (c < g->col_count - 1 && used + 1 <= width) {
            flux_sb_append(sb, " ");
            used++;
        }
    }
    for (; used < width; used++) flux_sb_append(sb, " ");
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    /* divider */
    flux_divider(sb, width, FLUX_THEME_BORDER_FG);
    flux_sb_nl(sb);

    /* rows */
    start = g->page * g->page_size;
    end = start + g->page_size;
    if (end > g->row_count) end = g->row_count;
    for (i = start; i < end; i++) {
        int rel = i - start;
        int row_idx = g->row_order ? g->row_order[i] : i;
        used = 0;
        for (c = 0; c < g->col_count && used < width; c++) {
            int cw = g->cols[c].width;
            int sel = (rel == g->cursor_row && c == g->cursor_col);
            const char *txt = g->cell_text ? g->cell_text(row_idx, c, ctx) : "";
            if (used + cw > width) cw = width - used;
            if (cw < 1) break;
            if (sel) flux_sb_append(sb, "\x1b[7m");
            else if (rel == g->cursor_row) flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
            else flux_sb_append(sb, FLUX_THEME_TEXT_FG);
            flux_fit(sb, txt ? txt : "", cw,
                     "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
            flux_sb_append(sb, FLUX_RESET);
            used += cw;
            if (c < g->col_count - 1 && used + 1 <= width) {
                flux_sb_append(sb, " ");
                used++;
            }
        }
        for (; used < width; used++) flux_sb_append(sb, " ");
        flux_sb_nl(sb);
    }
    /* pad to page_size */
    for (i = end; i < start + g->page_size; i++) {
        flux_fit(sb, "", width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }

    /* footer */
    {
        const char *sort_name = (g->sort_col >= 0 && g->sort_col < g->col_count)
                              ? g->cols[g->sort_col].title : "—";
        const char *sort_dir = (g->sort_col < 0) ? "" :
            (g->cols[g->sort_col].sort == FLUX_DG_ASC) ? " \xe2\x96\xb2" :
            (g->cols[g->sort_col].sort == FLUX_DG_DESC) ? " \xe2\x96\xbc" : "";
        snprintf(foot, sizeof foot,
                 "Page %d/%d  Sort: %s%s   ←→ resize  s sort",
                 g->page + 1, n_pages, sort_name ? sort_name : "—", sort_dir);
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        flux_fit(sb, foot, width, "\xe2\x80\xa6", FLUX_ALIGN_LEFT);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
}

/* === END B6 === */

/* ─── B7_effects impl ────────────────────────────────── */
/* ── B7 helpers ─────────────────────────────────────────────────────── */

void flux_sb_fg_rgb(FluxSB *sb, FluxRGB c) {
    if (!sb) return;
    flux_sb_appendf(sb, "\x1b[38;2;%u;%u;%um",
                    (unsigned)c.r, (unsigned)c.g, (unsigned)c.b);
}

void flux_sb_bg_rgb(FluxSB *sb, FluxRGB c) {
    if (!sb) return;
    flux_sb_appendf(sb, "\x1b[48;2;%u;%u;%um",
                    (unsigned)c.r, (unsigned)c.g, (unsigned)c.b);
}

/* Parse a trailing R;G;B triple from a `\x1b[38;2;R;G;Bm` (or 48;2) escape.
 * Returns 1 on success, 0 on parse failure (256-color, named colour, etc.). */
static int _flux_parse_rgb_escape(const char *esc, FluxRGB *out) {
    const char *p;
    int r, g, b;
    int n;

    if (!esc || !out) return 0;
    /* Find "38;2;" or "48;2;" anywhere in the string. */
    p = NULL;
    {
        const char *q = esc;
        while (*q) {
            if ((q[0] == '3' || q[0] == '4') && q[1] == '8' && q[2] == ';' &&
                q[3] == '2' && q[4] == ';') {
                p = q + 5;
                break;
            }
            q++;
        }
    }
    if (!p) return 0;
    n = 0;
    if (sscanf(p, "%d;%d;%d%n", &r, &g, &b, &n) != 3) return 0;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    out->r = (unsigned char)r;
    out->g = (unsigned char)g;
    out->b = (unsigned char)b;
    return 1;
}

/* ── 1. flux_digits ─────────────────────────────────────────────────── */

/* Glyphs are 3 cells wide × 3 rows tall. UTF-8 encodes ▄ ▀ █ as 3 bytes
 * each, ' ' as 1 byte. We store glyph rows as raw UTF-8 strings. */
static const char *_flux_digit_glyph(char ch, int row) {
    /* Order: 0..9, ':', '.', '-', '+', ' ' (default). */
    static const char *G[15][3] = {
        /* 0 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  "\xe2\x96\x88 \xe2\x96\x88",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
        /* 1 */ { "\xe2\x96\x84\xe2\x96\x88 ",
                  " \xe2\x96\x88 ",
                  " \xe2\x96\x88 " },
        /* 2 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x84",
                  " \xe2\x96\x84\xe2\x96\x80",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
        /* 3 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x84",
                  " \xe2\x96\x84\xe2\x96\x88",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x80" },
        /* 4 */ { "\xe2\x96\x88 \xe2\x96\x88",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  "  \xe2\x96\x88" },
        /* 5 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  "\xe2\x96\x80\xe2\x96\x88\xe2\x96\x84",
                  " \xe2\x96\x88\xe2\x96\x88" },
        /* 6 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x84",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x84",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
        /* 7 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  "  \xe2\x96\x88",
                  "  \xe2\x96\x88" },
        /* 8 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88" },
        /* 9 */ { "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
                  " \xe2\x96\x84\xe2\x96\x88" },
        /* : */ { "   ",
                  " \xe2\x96\x80 ",
                  " \xe2\x96\x84 " },
        /* . */ { "   ",
                  "   ",
                  " \xe2\x96\x84 " },
        /* - */ { "   ",
                  "\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84",
                  "   " },
        /* + */ { " \xe2\x96\x84 ",
                  "\xe2\x96\x84\xe2\x96\x88\xe2\x96\x84",
                  " \xe2\x96\x80 " },
        /* ' '*/{ "   ", "   ", "   " }
    };
    int idx;

    if (ch >= '0' && ch <= '9')      idx = ch - '0';
    else if (ch == ':')              idx = 10;
    else if (ch == '.')              idx = 11;
    else if (ch == '-')              idx = 12;
    else if (ch == '+')              idx = 13;
    else                             idx = 14; /* blank */
    if (row < 0 || row > 2) row = 0;
    return G[idx][row];
}

void flux_digits(FluxSB *sb, const char *str, const char *color, int width) {
    char line[2048];
    int row;
    int n;
    int i;
    int li;
    char up;
    const char *glyph;
    int glen;

    if (!sb || !str || width <= 0) return;
    if (!color) color = FLUX_THEME_BRAND_PURPLE_FG;

    n = (int)strlen(str);

    for (row = 0; row < 3; row++) {
        li = 0;
        for (i = 0; i < n; i++) {
            up = str[i];
            if (up >= 'a' && up <= 'z') up = (char)(up - 'a' + 'A');
            glyph = _flux_digit_glyph(up, row);
            glen = (int)strlen(glyph);
            if (li + glen + 2 >= (int)sizeof line) break;
            memcpy(line + li, glyph, (size_t)glen);
            li += glen;
            /* Trailing 1-cell gap, dropped after final char. */
            if (i + 1 < n) {
                line[li++] = ' ';
            }
        }
        line[li] = '\0';

        flux_sb_append(sb, color);
        flux_fit(sb, line, width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
}

/* ── 2. flux_glow_text ──────────────────────────────────────────────── */

void flux_glow_text(FluxSB *sb, const char *text,
                    const char *fg, const char *glow_color, int width) {
    FluxRGB halo;
    int parsed;
    int tw;
    char trunc[1024];
    const char *use_text;
    int body_cells;
    int padded;
    int i;

    if (!sb || !text || width <= 0) return;
    if (!fg) fg = FLUX_THEME_BRAND_PURPLE_FG;
    if (!glow_color) glow_color = fg;

    parsed = _flux_parse_rgb_escape(glow_color, &halo);
    if (parsed) {
        halo.r = (unsigned char)(halo.r / 4);
        halo.g = (unsigned char)(halo.g / 4);
        halo.b = (unsigned char)(halo.b / 4);
    }

    tw = flux_strwidth(text);
    use_text = text;
    if (tw + 2 > width) {
        if (width <= 2) {
            /* No room for any text body; emit just halo padding inside width. */
            flux_truncate("", 0, NULL, trunc, (int)sizeof trunc);
            use_text = trunc;
        } else {
            flux_truncate(text, width - 2, "\xe2\x80\xa6",
                          trunc, (int)sizeof trunc);
            use_text = trunc;
        }
        tw = flux_strwidth(use_text);
    }

    body_cells = tw + 2; /* leading + body + trailing space */
    if (body_cells > width) body_cells = width;

    /* Emit halo bg + leading space. */
    if (parsed) flux_sb_bg_rgb(sb, halo);
    else        flux_sb_append(sb, FLUX_THEME_PANEL_BG);
    flux_sb_append(sb, " ");

    /* Body: bold fg over halo bg. */
    flux_sb_append(sb, "\x1b[1m");
    flux_sb_append(sb, fg);
    flux_sb_append(sb, use_text);
    flux_sb_append(sb, "\x1b[22m"); /* normal intensity */

    /* Trailing halo space (only if width allows). */
    if (body_cells >= 2) {
        flux_sb_append(sb, " ");
    }
    flux_sb_append(sb, FLUX_RESET);

    /* Plain pad to width. */
    padded = width - body_cells;
    for (i = 0; i < padded; i++) flux_sb_append(sb, " ");

    flux_sb_nl(sb);
}

/* ── 3. flux_gradient_text ──────────────────────────────────────────── */

/* Walk one UTF-8 codepoint at *p (within `len` bytes). Stops at '\n'.
 * Writes byte count to *bytes and cell width (1 or 2) to *cells.
 * Returns 0 if no more atoms (EOF or '\n'). */
static int _flux_b7_next_atom(const char *p, int len,
                              int *bytes, int *cells) {
    unsigned char c;
    int seq;
    unsigned int cp;
    int k;
    unsigned char cc;

    if (len <= 0) return 0;
    c = (unsigned char)p[0];
    if (c == '\n' || c == '\0') return 0;
    if (c < 0x80)      { seq = 1; cp = c; }
    else if (c < 0xC0) { seq = 1; cp = c; }
    else if (c < 0xE0) { seq = 2; cp = c & 0x1F; }
    else if (c < 0xF0) { seq = 3; cp = c & 0x0F; }
    else               { seq = 4; cp = c & 0x07; }
    if (seq > len) seq = len;
    for (k = 1; k < seq; k++) {
        cc = (unsigned char)p[k];
        if ((cc & 0xC0) != 0x80) { seq = k; break; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    *bytes = seq;
    *cells = _flux_wt_is_wide_cp(cp) ? 2 : 1;
    if (cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F)) *cells = 0;
    return 1;
}

void flux_gradient_text(FluxSB *sb, const char *text,
                        FluxRGB start, FluxRGB end, int width) {
    int len;
    int j;
    int cell_idx; /* cell index along the gradient */
    int total_cells; /* cells we'll actually paint */
    int b, w;
    int budget;
    char buf[8];
    FluxRGB c;
    float t;
    int pad;
    int i;

    if (!sb || !text || width <= 0) return;

    len = (int)strlen(text);

    /* First pass: count how many cells we'll actually render
     * (capped by width, stopping at '\n'). */
    total_cells = 0;
    j = 0;
    while (j < len) {
        if (!_flux_b7_next_atom(text + j, len - j, &b, &w)) break;
        if (total_cells + w > width) break;
        total_cells += w;
        j += b;
    }

    /* Second pass: emit each atom with its gradient colour. */
    cell_idx = 0;
    j = 0;
    budget = total_cells;
    while (j < len && cell_idx < budget) {
        if (!_flux_b7_next_atom(text + j, len - j, &b, &w)) break;
        if (cell_idx + w > budget) break;
        if (w == 0) {
            /* Zero-width: emit verbatim, no colour change. */
            if (b > 0 && b < (int)sizeof buf) {
                memcpy(buf, text + j, (size_t)b);
                buf[b] = '\0';
                flux_sb_append(sb, buf);
            }
            j += b;
            continue;
        }
        t = (total_cells <= 1) ? 0.f
                               : (float)cell_idx / (float)(total_cells - 1);
        c = flux_rgb_lerp(start, end, t);
        flux_sb_fg_rgb(sb, c);
        if (b > 0 && b < (int)sizeof buf) {
            memcpy(buf, text + j, (size_t)b);
            buf[b] = '\0';
            flux_sb_append(sb, buf);
        }
        cell_idx += w;
        j += b;
    }

    flux_sb_append(sb, FLUX_RESET);

    pad = width - total_cells;
    if (pad < 0) pad = 0;
    for (i = 0; i < pad; i++) flux_sb_append(sb, " ");

    flux_sb_nl(sb);
}

/* ── 4. flux_gradient_bar ───────────────────────────────────────────── */

void flux_gradient_bar(FluxSB *sb, float progress, int width,
                       FluxRGB start, FluxRGB end) {
    int filled_full;
    float frac;
    int has_soft;
    const char *soft_glyph;
    int i;
    float t;
    FluxRGB c;
    float scaled;

    if (!sb || width <= 0) return;
    if (progress < 0.f) progress = 0.f;
    else if (progress > 1.f) progress = 1.f;

    scaled = progress * (float)width;
    filled_full = (int)scaled; /* floor for non-negative */
    if (filled_full > width) filled_full = width;
    frac = scaled - (float)filled_full;

    has_soft = 0;
    soft_glyph = "\xe2\x96\x91"; /* ░ */
    if (filled_full < width && frac > 0.001f) {
        has_soft = 1;
        if      (frac > 0.66f) soft_glyph = "\xe2\x96\x93"; /* ▓ */
        else if (frac > 0.33f) soft_glyph = "\xe2\x96\x92"; /* ▒ */
        else                   soft_glyph = "\xe2\x96\x91"; /* ░ */
    }

    for (i = 0; i < width; i++) {
        t = (width <= 1) ? 0.f : (float)i / (float)(width - 1);
        c = flux_rgb_lerp(start, end, t);
        if (i < filled_full) {
            flux_sb_fg_rgb(sb, c);
            flux_sb_append(sb, "\xe2\x96\x88"); /* █ */
        } else if (has_soft && i == filled_full) {
            flux_sb_fg_rgb(sb, c);
            flux_sb_append(sb, soft_glyph);
        } else {
            flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
            flux_sb_append(sb, "\xe2\x96\x91"); /* ░ */
        }
    }
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 5. flux_gradient_border ────────────────────────────────────────── */

/* Emit one perimeter cell at index `k` (0..P-1) with colour interpolated
 * along the gradient, given border glyph `g` (UTF-8). */
static void _flux_b7_border_cell(FluxSB *sb, int k, int P,
                                 FluxRGB start, FluxRGB end,
                                 const char *g) {
    float t;
    FluxRGB c;

    t = (P <= 1) ? 0.f : (float)k / (float)(P - 1);
    c = flux_rgb_lerp(start, end, t);
    flux_sb_fg_rgb(sb, c);
    flux_sb_append(sb, g);
}

void flux_gradient_border(FluxSB *sb, int w, int h,
                          FluxRGB start, FluxRGB end,
                          FluxBorderContentFn content, void *ctx) {
    static __thread char scratch[8192];
    FluxSB inner;
    int inner_w, inner_h;
    int P;
    int row, col;
    int k_right, k_left;
    const char *p;
    const char *line_start;
    const char *line_end;
    int line_idx;

    if (!sb || w < 2 || h < 2) return;

    inner_w = w - 2;
    inner_h = h - 2;
    P = 2 * w + 2 * (h - 2);
    if (P < 1) P = 1;

    /* Render content into thread-local scratch (if any). */
    inner.buf = scratch;
    inner.len = 0;
    inner.cap = (int)sizeof scratch;
    scratch[0] = '\0';
    if (content && inner_h > 0 && inner_w > 0) {
        content(&inner, inner_w, inner_h, ctx);
    }

    /* Top row: ╭ + ─×(w-2) + ╮  (perimeter indices 0 .. w-1) */
    _flux_b7_border_cell(sb, 0, P, start, end, "\xe2\x95\xad"); /* ╭ */
    for (col = 0; col < inner_w; col++) {
        _flux_b7_border_cell(sb, 1 + col, P, start, end,
                             "\xe2\x94\x80"); /* ─ */
    }
    _flux_b7_border_cell(sb, w - 1, P, start, end, "\xe2\x95\xae"); /* ╮ */
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    /* Locate each content line within scratch. */
    p = scratch;
    line_idx = 0;

    /* Middle rows: │ + content + │  */
    for (row = 0; row < inner_h; row++) {
        /* Perimeter index for this row's left │: walking clockwise the
         * left column is the LAST stretch (after top, right, bottom). */
        k_right  = (w - 1) + 1 + row;             /* right column index */
        k_left   = (w - 1) + (h - 1) + (w - 1) + (inner_h - row);
        /* For top-left corner k=0; right column k = w..w+h-3 (inner_h
         * rows); bottom-right k = w+h-2; bottom row k = w+h-1..2w+h-3;
         * bottom-left k = 2w+h-2; left column k = 2w+h-1..2w+2h-4
         * (going UP the column, so row 0 is the LAST left-column cell). */

        /* Find next line in scratch. */
        line_start = p;
        line_end = p;
        if (content && line_idx < inner_h) {
            while (*line_end && *line_end != '\n') line_end++;
        }

        /* Left │ */
        _flux_b7_border_cell(sb, k_left, P, start, end, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);

        /* Inner content (NOT coloured by the gradient). */
        if (content && line_idx < inner_h && line_end > line_start) {
            int blen = (int)(line_end - line_start);
            char tmp[2048];
            if (blen >= (int)sizeof tmp) blen = (int)sizeof tmp - 1;
            memcpy(tmp, line_start, (size_t)blen);
            tmp[blen] = '\0';
            flux_fit(sb, tmp, inner_w, NULL, FLUX_ALIGN_LEFT);
            if (*line_end == '\n') p = line_end + 1;
            else                    p = line_end;
            line_idx++;
        } else {
            for (col = 0; col < inner_w; col++) flux_sb_append(sb, " ");
        }

        /* Right │ */
        _flux_b7_border_cell(sb, k_right, P, start, end, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Bottom row: ╰ + ─×(w-2) + ╯
     * Bottom-left ╰ is at index k = (w-1) + (h-1) + (w-1) = 2w+h-3.
     * Bottom row indices walking right-to-left along bottom would be
     * ╯ at w+h-2, then ─ cells, then ╰ at 2w+h-3. But clockwise traversal
     * from top-left goes top→right→bottom (left-to-right? no, clockwise
     * means bottom is right-to-left). Actually clockwise from top-left:
     *   top: left→right
     *   right: top→bottom
     *   bottom: right→left
     *   left: bottom→top
     * So bottom row emitted left-to-right has indices reversed:
     *   ╰ at k = 2w+h-3, then ─ at k = 2w+h-4 .. w+h-1, then ╯ at w+h-2.
     */
    {
        int k_br = (w - 1) + (h - 1);                /* ╯ */
        int k_bl = 2 * (w - 1) + (h - 1);            /* ╰ */
        _flux_b7_border_cell(sb, k_bl, P, start, end, "\xe2\x95\xb0"); /* ╰ */
        for (col = 0; col < inner_w; col++) {
            int k = k_bl - 1 - col;
            _flux_b7_border_cell(sb, k, P, start, end,
                                 "\xe2\x94\x80"); /* ─ */
        }
        _flux_b7_border_cell(sb, k_br, P, start, end, "\xe2\x95\xaf"); /* ╯ */
    }
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ─── B8_markdown_misc impl (self-contained block) ──── */
/* ============================================================
 * B8 — Markdown, syntax, dev, calendars, files, misc (15 widgets)
 *
 * Self-contained C99 add-on for flux.h. Include AFTER flux.h.
 * Requires FLUX_IMPL to be defined in the TU that includes both.
 *
 * Widgets (15):
 *   1.  flux_markdown                — md → ANSI
 *   2.  FluxMarkdownViewer           — md in scrollview
 *   3.  flux_syntax_highlight        — c/js/ts/json/sh/md
 *   4.  FluxPerfHud                  — fps/cpu/mem
 *   5.  FluxCalendar                 — month grid
 *   6.  FluxDatePicker               — calendar input
 *   7.  FluxDirTree                  — opendir browser
 *   8.  FluxFilePicker               — modal file picker
 *   9.  flux_welcome                 — splash
 *  10.  flux_loading                 — spinner row
 *  11.  flux_inline_diff             — word-level diff line
 *  12.  FluxOptionList               — multi-select list
 *  13.  FluxSelectionList            — alias of OptionList
 *  14.  FluxStopwatch                — counts up
 *  15.  FluxTimer                    — counts down
 *
 * Width contract: every emitted row is exactly `width` display cells,
 * padded with spaces, terminated by '\n'. ANSI escapes don't count.
 * width <= 0 is a no-op.
 * ============================================================ */
#include <stdint.h>
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────
 * Compatibility shims: alias names referenced in the spec.
 * ────────────────────────────────────────────────────────────── */
#ifndef FLUX_EVENT_DEFINED
#define FLUX_EVENT_DEFINED
typedef FluxMsg FluxEvent;
#endif

/* Theme aliases referenced by the spec. Map onto existing flux.h tokens. */
#ifndef FLUX_THEME_BRAND_FG
#define FLUX_THEME_BRAND_FG          FLUX_THEME_BRAND_PURPLE_FG
#endif
#ifndef FLUX_THEME_SYNTAX_COMMENT_FG
#define FLUX_THEME_SYNTAX_COMMENT_FG FLUX_THEME_TEXT_DIM_FG
#endif
#ifndef FLUX_THEME_SYNTAX_STRING_FG
#define FLUX_THEME_SYNTAX_STRING_FG  FLUX_THEME_OK_FG
#endif
#ifndef FLUX_THEME_SYNTAX_KEYWORD_FG
#define FLUX_THEME_SYNTAX_KEYWORD_FG FLUX_THEME_BRAND_PURPLE_FG
#endif
#ifndef FLUX_THEME_SYNTAX_NUMBER_FG
#define FLUX_THEME_SYNTAX_NUMBER_FG  FLUX_THEME_WARN_FG
#endif
#ifndef FLUX_THEME_SYNTAX_TYPE_FG
#define FLUX_THEME_SYNTAX_TYPE_FG    FLUX_THEME_ACCENT_FG
#endif
#ifndef FLUX_THEME_SYNTAX_OPERATOR_FG
#define FLUX_THEME_SYNTAX_OPERATOR_FG FLUX_THEME_TEXT2_FG
#endif
#ifndef FLUX_THEME_DIFF_REMOVED_FG
#define FLUX_THEME_DIFF_REMOVED_FG   FLUX_THEME_DIFF_DEL_FG
#endif
#ifndef FLUX_THEME_DIFF_ADDED_FG
#define FLUX_THEME_DIFF_ADDED_FG     FLUX_THEME_DIFF_ADD_FG
#endif

/* ============================================================
 * === FLUX.H DECLARATIONS ===
 * ============================================================ */

/* 1. Markdown ───────────────────────────────────────────────── */
void flux_markdown(FluxSB *sb, const char *md, int width);

/* 2. Markdown viewer ───────────────────────────────────────── */
typedef struct {
    const char *md;
    FluxScroll  scroll;
    int         show_toc;
    int         toc_width;
    int         toc_sel;
    int         toc_count;
} FluxMarkdownViewer;

void flux_md_viewer_init  (FluxMarkdownViewer *v, const char *md, int show_toc);
int  flux_md_viewer_update(FluxMarkdownViewer *v, const FluxEvent *ev);
void flux_md_viewer_render(FluxMarkdownViewer *v, FluxSB *sb, int width, int height);

/* 3. Syntax highlight ─────────────────────────────────────── */
typedef enum {
    FLUX_LANG_PLAIN = 0, FLUX_LANG_C, FLUX_LANG_JS, FLUX_LANG_TS,
    FLUX_LANG_JSON,      FLUX_LANG_SH, FLUX_LANG_MD
} FluxLang;

void flux_syntax_highlight(FluxSB *sb, const char *code,
                           FluxLang lang, int width);

/* 4. Perf HUD ─────────────────────────────────────────────── */
typedef struct {
    float    fps_hist[20];
    float    rt_hist[20];
    float    mem_hist[20];
    int      hist_idx, hist_full;
    uint64_t last_tick_ns;
    uint64_t last_proc_utime;
    uint64_t last_proc_stime;
    long     mem_total_kb;
    char     title[32];
    int      visible;
    /* derived (last sample) */
    float    cur_fps, cur_rt, cur_rss_mb, cur_cpu;
} FluxPerfHud;

void flux_perf_hud_init  (FluxPerfHud *h, const char *title);
void flux_perf_hud_tick  (FluxPerfHud *h, float render_ms);
void flux_perf_hud_render(const FluxPerfHud *h, FluxSB *sb, int width);

/* 5. Calendar ─────────────────────────────────────────────── */
typedef struct {
    int year, month, sel_day;
    int today_y, today_m, today_d;
    int week_starts_mon;
    int focused;
} FluxCalendar;

void flux_calendar_init  (FluxCalendar *c, int year, int month, int sel_day);
int  flux_calendar_update(FluxCalendar *c, const FluxEvent *ev);
void flux_calendar_render(const FluxCalendar *c, FluxSB *sb, int width);

/* 6. Date picker ──────────────────────────────────────────── */
typedef struct {
    int  year, month, day;
    int  open;
    int  focused;
    char fmt[16];
    char placeholder[32];
    FluxCalendar cal;
} FluxDatePicker;

void flux_datepicker_init  (FluxDatePicker *d, int y, int m, int day);
int  flux_datepicker_update(FluxDatePicker *d, const FluxEvent *ev);
void flux_datepicker_render(FluxDatePicker *d, FluxSB *sb, int width);

/* 7. Dir tree ─────────────────────────────────────────────── */
typedef struct FluxDirEntry {
    char name[256];
    int  is_dir;
    int  expanded;
    int  depth;
    int  parent_idx;
} FluxDirEntry;

typedef struct {
    char           root[1024];
    FluxDirEntry  *entries;
    int            count, cap;
    int            cursor;
    int            scroll_top;
    int            show_hidden;
    int            show_files;
    int            focused;
    /* scratch for selected_path */
    char           sel_path_buf[2048];
} FluxDirTree;

int   flux_dirtree_init   (FluxDirTree *t, const char *root_path);
void  flux_dirtree_free   (FluxDirTree *t);
int   flux_dirtree_update (FluxDirTree *t, const FluxEvent *ev);
void  flux_dirtree_render (FluxDirTree *t, FluxSB *sb, int width, int height);
const char *flux_dirtree_selected(FluxDirTree *t);

/* 8. File picker ──────────────────────────────────────────── */
typedef struct {
    FluxDirTree tree;
    FluxInput   filename;
    int         mode;       /* 0 open, 1 save */
    int         visible;
    char        result[1024];
    int         committed;
    char        exts[64];
    int         right_focus; /* 0 = tree, 1 = filename */
} FluxFilePicker;

void flux_filepicker_init  (FluxFilePicker *p, const char *root, int mode, const char *exts);
void flux_filepicker_free  (FluxFilePicker *p);
int  flux_filepicker_update(FluxFilePicker *p, const FluxEvent *ev);
void flux_filepicker_render(FluxFilePicker *p, FluxSB *sb, int width, int height);

/* 9. Welcome splash ───────────────────────────────────────── */
typedef struct { const char *key; const char *label; } FluxWelcomeHint;

void flux_welcome(FluxSB *sb,
                  const char *title,
                  const char *version,
                  const char *model,
                  const FluxWelcomeHint *hints, int n_hints,
                  int width);

/* 10. Loading row ─────────────────────────────────────────── */
void flux_loading(FluxSB *sb, const char *label, int frame, int width);

/* 11. Inline diff ─────────────────────────────────────────── */
void flux_inline_diff(FluxSB *sb,
                      const char *before, const char *after,
                      int width);

/* 12. Option list ─────────────────────────────────────────── */
typedef struct {
    const char *label;
    const char *value;
    int         selected;
    int         disabled;
    int         is_separator;
} FluxOption;

typedef struct {
    FluxOption *items;
    int         n;
    int         cursor;
    int         scroll_top;
    int         max_visible;
    int         focused;
    int         show_index;
    char        indicator;
} FluxOptionList;

void flux_optionlist_init  (FluxOptionList *l, FluxOption *items, int n);
int  flux_optionlist_update(FluxOptionList *l, const FluxEvent *ev);
void flux_optionlist_render(const FluxOptionList *l, FluxSB *sb, int width);

/* 13. Selection list (alias) ──────────────────────────────── */
typedef FluxOptionList FluxSelectionList;
typedef FluxOption     FluxSelectionItem;
#define flux_selectionlist_init    flux_optionlist_init
#define flux_selectionlist_update  flux_optionlist_update
#define flux_selectionlist_render  flux_optionlist_render

/* 14/15. Timer formats + Stopwatch + Timer ────────────────── */
typedef enum {
    FLUX_TIMER_FMT_MM_SS_MMM = 0,
    FLUX_TIMER_FMT_MM_SS,
    FLUX_TIMER_FMT_HH_MM_SS,
    FLUX_TIMER_FMT_SS_CS
} FluxTimerFmt;

typedef struct {
    uint64_t     start_ns;
    uint64_t     accum_ns;
    int          running;
    FluxTimerFmt fmt;
} FluxStopwatch;

void     flux_stopwatch_init  (FluxStopwatch *s, FluxTimerFmt fmt);
void     flux_stopwatch_start (FluxStopwatch *s);
void     flux_stopwatch_stop  (FluxStopwatch *s);
void     flux_stopwatch_reset (FluxStopwatch *s);
void     flux_stopwatch_tick  (FluxStopwatch *s);
uint64_t flux_stopwatch_ms    (const FluxStopwatch *s);
void     flux_stopwatch_render(const FluxStopwatch *s, FluxSB *sb, int width);

typedef struct {
    uint64_t     start_ns;
    uint64_t     duration_ns;
    int          running;
    int          finished;
    FluxTimerFmt fmt;
    const char  *prefix;
} FluxTimer;

void     flux_timer_init        (FluxTimer *t, uint64_t duration_ms, FluxTimerFmt fmt);
void     flux_timer_start       (FluxTimer *t);
void     flux_timer_stop        (FluxTimer *t);
void     flux_timer_reset       (FluxTimer *t);
int      flux_timer_tick        (FluxTimer *t);
uint64_t flux_timer_remaining_ms(const FluxTimer *t);
void     flux_timer_render      (const FluxTimer *t, FluxSB *sb, int width);


/* ============================================================
 * === FLUX.H IMPLEMENTATIONS ===
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef __linux__
#  include <unistd.h>
#  include <sys/types.h>
#endif

/* dir traversal */
#include <dirent.h>
#include <sys/stat.h>

/* ─ small local helpers ────────────────────────────────────── */
static void fb8_pad(FluxSB *sb, int n) { int i; for (i = 0; i < n; i++) flux_sb_append(sb, " "); }
static void fb8_row(FluxSB *sb, const char *s, int width) {
    flux_fit(sb, s ? s : "", width, "…", FLUX_ALIGN_LEFT);
    flux_sb_nl(sb);
}

static uint64_t fb8_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ============================================================
 * 1. flux_markdown
 * ============================================================ */

/* Inline tokenizer: walk `s`, emit ANSI-styled bytes into `sb` until
 * NUL. Honors: **bold** *italic* `code` ~~strike~~ [text](url) ![alt](u)
 * \\* escapes. Best-effort, no nesting beyond bold/italic toggle. */
static void fb8_md_inline(FluxSB *sb, const char *s) {
    int i = 0;
    int bold = 0, italic = 0, code = 0;
    if (!s) return;
    while (s[i]) {
        unsigned char c = (unsigned char)s[i];
        /* escape */
        if (c == '\\' && s[i+1]) {
            char tmp[2] = { s[i+1], 0 };
            flux_sb_append(sb, tmp);
            i += 2; continue;
        }
        /* inline code `...` */
        if (c == '`') {
            int j = i + 1;
            while (s[j] && s[j] != '`') j++;
            flux_sb_append(sb, FLUX_THEME_SYNTAX_STRING_FG);
            { char buf[256]; int len = j - i - 1; if (len > 255) len = 255;
              memcpy(buf, s + i + 1, len); buf[len] = 0;
              flux_sb_append(sb, buf); }
            flux_sb_append(sb, FLUX_RESET);
            i = s[j] ? j + 1 : j;
            (void)code;
            continue;
        }
        /* image ![alt](url) → dim [Image: alt] */
        if (c == '!' && s[i+1] == '[') {
            int j = i + 2;
            while (s[j] && s[j] != ']') j++;
            char alt[128]; int len = j - i - 2; if (len > 127) len = 127;
            memcpy(alt, s + i + 2, len); alt[len] = 0;
            int k = (s[j] == ']') ? j + 1 : j;
            if (s[k] == '(') { while (s[k] && s[k] != ')') k++; if (s[k]) k++; }
            flux_sb_append(sb, FLUX_DIM);
            flux_sb_append(sb, "[Image: ");
            flux_sb_append(sb, alt);
            flux_sb_append(sb, "]");
            flux_sb_append(sb, FLUX_RESET);
            i = k;
            continue;
        }
        /* link [text](url) → "text (url)" with accent */
        if (c == '[') {
            int j = i + 1;
            while (s[j] && s[j] != ']') j++;
            if (s[j] == ']' && s[j+1] == '(') {
                char text[128]; int len = j - i - 1;
                if (len > 127) len = 127;
                memcpy(text, s + i + 1, len); text[len] = 0;
                int k = j + 2, urlstart = k;
                while (s[k] && s[k] != ')') k++;
                char url[256]; int ulen = k - urlstart;
                if (ulen > 255) ulen = 255;
                memcpy(url, s + urlstart, ulen); url[ulen] = 0;
                flux_sb_append(sb, FLUX_UNDERLINE);
                flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
                flux_sb_append(sb, text);
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_append(sb, FLUX_DIM);
                flux_sb_append(sb, " (");
                flux_sb_append(sb, url);
                flux_sb_append(sb, ")");
                flux_sb_append(sb, FLUX_RESET);
                i = s[k] ? k + 1 : k;
                continue;
            }
        }
        /* strike ~~ */
        if (c == '~' && s[i+1] == '~') {
            int j = i + 2;
            while (s[j] && !(s[j] == '~' && s[j+1] == '~')) j++;
            char buf[256]; int len = j - i - 2; if (len > 255) len = 255;
            memcpy(buf, s + i + 2, len); buf[len] = 0;
            flux_sb_append(sb, FLUX_STRIKE);
            flux_sb_append(sb, FLUX_DIM);
            flux_sb_append(sb, buf);
            flux_sb_append(sb, FLUX_RESET);
            i = s[j] ? j + 2 : j;
            continue;
        }
        /* bold ** or __ */
        if ((c == '*' && s[i+1] == '*') || (c == '_' && s[i+1] == '_')) {
            bold = !bold;
            flux_sb_append(sb, bold ? FLUX_BOLD : FLUX_RESET);
            if (!bold && italic) flux_sb_append(sb, FLUX_ITALIC);
            i += 2; continue;
        }
        /* italic * or _ */
        if (c == '*' || c == '_') {
            italic = !italic;
            flux_sb_append(sb, italic ? FLUX_ITALIC : FLUX_RESET);
            if (!italic && bold) flux_sb_append(sb, FLUX_BOLD);
            i += 1; continue;
        }
        /* literal */
        { char tmp[2] = { (char)c, 0 }; flux_sb_append(sb, tmp); }
        i++;
    }
    flux_sb_append(sb, FLUX_RESET);
}

/* helper: emit a styled line padded to width, expects body has only one
 * logical row's worth of chars (we trust it fits or call flux_fit). */
static void fb8_md_styled_row(FluxSB *sb, const char *prefix_ansi,
                              const char *suffix_ansi,
                              const char *content, int width) {
    /* We compose: <prefix><content><reset>, then pad with spaces to
     * width. We measure plain content width via flux_strwidth. */
    int cw = flux_strwidth(content);
    int pad;
    if (cw > width) {
        char tmp[1024];
        flux_truncate(content, width, "…", tmp, sizeof tmp);
        if (prefix_ansi) flux_sb_append(sb, prefix_ansi);
        flux_sb_append(sb, tmp);
        if (suffix_ansi) flux_sb_append(sb, suffix_ansi);
        flux_sb_append(sb, FLUX_RESET);
        pad = width - flux_strwidth(tmp);
    } else {
        if (prefix_ansi) flux_sb_append(sb, prefix_ansi);
        flux_sb_append(sb, content);
        if (suffix_ansi) flux_sb_append(sb, suffix_ansi);
        flux_sb_append(sb, FLUX_RESET);
        pad = width - cw;
    }
    fb8_pad(sb, pad < 0 ? 0 : pad);
    flux_sb_nl(sb);
}

/* Render a single inline-styled paragraph line. Computes display width
 * of the *plain* text (post-strip), then emits styled inline + pad. */
static void fb8_md_para_row(FluxSB *sb, const char *text, int width) {
    /* Compute plain width by stripping md markers (rough, good enough). */
    int i = 0, w = 0;
    if (!text) { fb8_pad(sb, width); flux_sb_nl(sb); return; }
    while (text[i] && w < width) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\\' && text[i+1]) { w++; i += 2; continue; }
        if (c == '`') {
            i++;
            while (text[i] && text[i] != '`') { w++; i++; }
            if (text[i]) i++;
            continue;
        }
        if (c == '!' && text[i+1] == '[') {
            int j = i + 2; while (text[j] && text[j] != ']') j++;
            int len = j - i - 2;
            w += 9 + len;  /* "[Image: alt]" approx */
            int k = (text[j] == ']') ? j + 1 : j;
            if (text[k] == '(') { while (text[k] && text[k] != ')') k++; if (text[k]) k++; }
            i = k; continue;
        }
        if (c == '[') {
            int j = i + 1; while (text[j] && text[j] != ']') j++;
            if (text[j] == ']' && text[j+1] == '(') {
                int len = j - i - 1;
                int k = j + 2, ust = k;
                while (text[k] && text[k] != ')') k++;
                int ul = k - ust;
                w += len + ul + 3;
                i = text[k] ? k + 1 : k; continue;
            }
        }
        if (c == '~' && text[i+1] == '~') { i += 2; continue; }
        if ((c == '*' && text[i+1] == '*') || (c == '_' && text[i+1] == '_')) { i += 2; continue; }
        if (c == '*' || c == '_') { i += 1; continue; }
        w++; i++;
    }
    fb8_md_inline(sb, text);
    if (w < width) fb8_pad(sb, width - w);
    flux_sb_nl(sb);
}

/* Split md into lines by '\n' (writes nul-terminated copy into out_buf).
 * Returns line count. Each call clobbers buf; non-reentrant scratch use. */

void flux_markdown(FluxSB *sb, const char *md, int width) {
    if (!sb || !md || width <= 0) return;

    int in_code = 0;
    char fence_lang[32] = {0};
    int  ol_n = 1;

    const char *p = md;
    while (*p) {
        /* extract one line into scratch */
        char line[2048] = {0};
        int  llen = 0;
        while (*p && *p != '\n' && llen < (int)sizeof(line) - 1) {
            line[llen++] = *p++;
        }
        line[llen] = 0;
        if (*p == '\n') p++;

        /* fenced code start/end */
        if (line[0] == '`' && line[1] == '`' && line[2] == '`') {
            if (!in_code) {
                int li = 3, fi = 0;
                while (line[li] && fi < (int)sizeof(fence_lang) - 1)
                    fence_lang[fi++] = line[li++];
                fence_lang[fi] = 0;
                in_code = 1;
                /* top border + lang label */
                {
                    char border[256];
                    int bw = width;
                    if (bw > (int)sizeof(border) - 64) bw = sizeof(border) - 64;
                    /* row: ╭── lang ──...─╮ */
                    int label_w = (int)strlen(fence_lang);
                    int dashes = bw - 2 - (label_w ? label_w + 2 : 0);
                    if (dashes < 0) dashes = 0;
                    flux_sb_append(sb, FLUX_THEME_BORDER_FG);
                    flux_sb_append(sb, "\xe2\x95\xad"); /* ╭ */
                    int k;
                    int half = dashes / 2;
                    for (k = 0; k < half; k++) flux_sb_append(sb, "\xe2\x94\x80");
                    if (label_w) {
                        flux_sb_append(sb, " ");
                        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
                        flux_sb_append(sb, fence_lang);
                        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
                        flux_sb_append(sb, " ");
                    }
                    for (k = half; k < dashes; k++) flux_sb_append(sb, "\xe2\x94\x80");
                    flux_sb_append(sb, "\xe2\x95\xae"); /* ╮ */
                    flux_sb_append(sb, FLUX_RESET);
                    flux_sb_nl(sb);
                }
                continue;
            } else {
                /* close: bottom border ╰──╯ */
                int dashes = width - 2;
                if (dashes < 0) dashes = 0;
                flux_sb_append(sb, FLUX_THEME_BORDER_FG);
                flux_sb_append(sb, "\xe2\x95\xb0"); /* ╰ */
                int k; for (k = 0; k < dashes; k++) flux_sb_append(sb, "\xe2\x94\x80");
                flux_sb_append(sb, "\xe2\x95\xaf"); /* ╯ */
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_nl(sb);
                in_code = 0;
                fence_lang[0] = 0;
                continue;
            }
        }

        if (in_code) {
            /* render code line: │ <code> │ — inner width = width-4 */
            int inner = width - 4;
            if (inner < 1) inner = 1;
            flux_sb_append(sb, FLUX_THEME_BORDER_FG);
            flux_sb_append(sb, "\xe2\x94\x82"); /* │ */
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, " ");
            char tmp[2048];
            flux_truncate(line, inner, "…", tmp, sizeof tmp);
            flux_sb_append(sb, FLUX_THEME_TEXT_FG);
            flux_sb_append(sb, tmp);
            flux_sb_append(sb, FLUX_RESET);
            int used = flux_strwidth(tmp);
            fb8_pad(sb, inner - used);
            flux_sb_append(sb, " ");
            flux_sb_append(sb, FLUX_THEME_BORDER_FG);
            flux_sb_append(sb, "\xe2\x94\x82");
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);
            continue;
        }

        /* horizontal rule */
        if ((line[0] == '-' && line[1] == '-' && line[2] == '-' && (line[3] == 0 || line[3] == '-')) ||
            (line[0] == '*' && line[1] == '*' && line[2] == '*') ||
            (line[0] == '_' && line[1] == '_' && line[2] == '_')) {
            int k;
            flux_sb_append(sb, FLUX_DIM);
            flux_sb_append(sb, FLUX_THEME_DIVIDER_FG);
            for (k = 0; k < width; k++) flux_sb_append(sb, "\xe2\x94\x80");
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);
            continue;
        }

        /* heading */
        if (line[0] == '#') {
            int level = 0;
            while (line[level] == '#' && level < 6) level++;
            int off = level;
            while (line[off] == ' ') off++;
            const char *body = line + off;
            char buf[1024];
            if (level <= 3) {
                /* large heading */
                if (level == 1) snprintf(buf, sizeof buf, "%s", body);
                else if (level == 2) snprintf(buf, sizeof buf, "%s", body);
                else snprintf(buf, sizeof buf, "%s", body);
                /* prefix glyph */
                const char *clr = (level == 1) ? FLUX_THEME_BRAND_PURPLE_FG
                                : (level == 2) ? FLUX_THEME_ACCENT_FG
                                :                FLUX_THEME_TEXT_FG;
                /* compose styled line: bold + color */
                char header[1100];
                snprintf(header, sizeof header, "%s%s%s", FLUX_BOLD, clr, "");
                /* emit prefix style, body, reset, pad */
                flux_sb_append(sb, FLUX_BOLD);
                flux_sb_append(sb, clr);
                int bw = flux_strwidth(buf);
                if (bw > width) {
                    char tmp[1024]; flux_truncate(buf, width, "…", tmp, sizeof tmp);
                    flux_sb_append(sb, tmp);
                    flux_sb_append(sb, FLUX_RESET);
                    fb8_pad(sb, width - flux_strwidth(tmp));
                } else {
                    flux_sb_append(sb, buf);
                    flux_sb_append(sb, FLUX_RESET);
                    fb8_pad(sb, width - bw);
                }
                flux_sb_nl(sb);
            } else {
                fb8_md_styled_row(sb, FLUX_BOLD FLUX_DIM, NULL, body, width);
            }
            ol_n = 1;
            continue;
        }

        /* blockquote */
        if (line[0] == '>') {
            int off = 1;
            while (line[off] == ' ') off++;
            flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
            flux_sb_append(sb, "\xe2\x96\x8c"); /* ▌ */
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, " ");
            int inner = width - 2;
            if (inner < 1) inner = 1;
            const char *body = line + off;
            int w = flux_strwidth(body);
            if (w > inner) {
                char tmp[1024];
                flux_truncate(body, inner, "…", tmp, sizeof tmp);
                fb8_md_inline(sb, tmp);
                fb8_pad(sb, inner - flux_strwidth(tmp));
            } else {
                fb8_md_inline(sb, body);
                fb8_pad(sb, inner - w);
            }
            flux_sb_nl(sb);
            continue;
        }

        /* list item: -, *, +, or "N." */
        {
            int off = 0;
            while (line[off] == ' ') off++;
            int depth = off / 2;
            if (depth > 9) depth = 9;
            char ul = line[off];
            if ((ul == '-' || ul == '*' || ul == '+') && line[off+1] == ' ') {
                /* task list? */
                int bo = off + 2;
                if (line[bo] == '[' && line[bo+2] == ']' && line[bo+3] == ' ') {
                    const char *box = (line[bo+1] == 'x' || line[bo+1] == 'X')
                                      ? "\xe2\x98\x91"  /* ☑ */
                                      : "\xe2\x98\x90"; /* ☐ */
                    int k;
                    for (k = 0; k < depth * 2; k++) flux_sb_append(sb, " ");
                    flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
                    flux_sb_append(sb, box);
                    flux_sb_append(sb, FLUX_RESET);
                    flux_sb_append(sb, " ");
                    int prefix_w = depth * 2 + 2;
                    const char *body = line + bo + 4;
                    int bw = flux_strwidth(body);
                    int avail = width - prefix_w;
                    if (avail < 1) avail = 1;
                    if (bw > avail) {
                        char tmp[1024]; flux_truncate(body, avail, "…", tmp, sizeof tmp);
                        fb8_md_inline(sb, tmp);
                        fb8_pad(sb, avail - flux_strwidth(tmp));
                    } else {
                        fb8_md_inline(sb, body);
                        fb8_pad(sb, avail - bw);
                    }
                    flux_sb_nl(sb);
                    continue;
                }
                /* plain bullet */
                const char *glyphs[3] = { "\xe2\x80\xa2", "\xe2\x97\xa6", "\xe2\x96\xaa" };
                const char *g = glyphs[depth % 3];
                int k;
                for (k = 0; k < depth * 2; k++) flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
                flux_sb_append(sb, g);
                flux_sb_append(sb, FLUX_RESET);
                flux_sb_append(sb, " ");
                int prefix_w = depth * 2 + 2;
                const char *body = line + off + 2;
                int bw = flux_strwidth(body);
                int avail = width - prefix_w;
                if (avail < 1) avail = 1;
                if (bw > avail) {
                    char tmp[1024]; flux_truncate(body, avail, "…", tmp, sizeof tmp);
                    fb8_md_inline(sb, tmp);
                    fb8_pad(sb, avail - flux_strwidth(tmp));
                } else {
                    fb8_md_inline(sb, body);
                    fb8_pad(sb, avail - bw);
                }
                flux_sb_nl(sb);
                ol_n = 1;
                continue;
            }
            if (line[off] >= '0' && line[off] <= '9') {
                int j = off;
                while (line[j] >= '0' && line[j] <= '9') j++;
                if (line[j] == '.' && line[j+1] == ' ') {
                    int n;
                    sscanf(line + off, "%d", &n);
                    if (ol_n == 1) ol_n = n;
                    char prefix[16];
                    snprintf(prefix, sizeof prefix, "%d. ", ol_n);
                    int k;
                    for (k = 0; k < depth * 2; k++) flux_sb_append(sb, " ");
                    flux_sb_append(sb, FLUX_THEME_TEXT2_FG);
                    flux_sb_append(sb, prefix);
                    flux_sb_append(sb, FLUX_RESET);
                    int prefix_w = depth * 2 + (int)strlen(prefix);
                    const char *body = line + j + 2;
                    int bw = flux_strwidth(body);
                    int avail = width - prefix_w;
                    if (avail < 1) avail = 1;
                    if (bw > avail) {
                        char tmp[1024]; flux_truncate(body, avail, "…", tmp, sizeof tmp);
                        fb8_md_inline(sb, tmp);
                        fb8_pad(sb, avail - flux_strwidth(tmp));
                    } else {
                        fb8_md_inline(sb, body);
                        fb8_pad(sb, avail - bw);
                    }
                    flux_sb_nl(sb);
                    ol_n++;
                    continue;
                }
            }
        }

        /* blank → blank padded */
        if (line[0] == 0) {
            fb8_pad(sb, width);
            flux_sb_nl(sb);
            ol_n = 1;
            continue;
        }

        /* paragraph line — soft wrap by simple word-by-word split */
        {
            const char *src = line;
            int srcw = flux_strwidth(src);
            if (srcw <= width) {
                fb8_md_para_row(sb, src, width);
            } else {
                /* split on spaces, accumulate */
                char buf[2048]; int bw = 0; int blen = 0;
                int i = 0;
                while (src[i]) {
                    /* read one word */
                    int ws = 0;
                    while (src[i] == ' ') { ws++; i++; }
                    int word_start = i;
                    while (src[i] && src[i] != ' ') i++;
                    int word_len = i - word_start;
                    if (word_len == 0) break;
                    char word[512];
                    if (word_len > 511) word_len = 511;
                    memcpy(word, src + word_start, word_len); word[word_len] = 0;
                    int ww = flux_strwidth(word);
                    int sep = (bw == 0) ? 0 : 1;
                    if (bw + sep + ww > width && bw > 0) {
                        buf[blen] = 0;
                        fb8_md_para_row(sb, buf, width);
                        blen = 0; bw = 0; sep = 0;
                    }
                    if (sep) { buf[blen++] = ' '; bw++; }
                    if (blen + word_len < (int)sizeof(buf) - 1) {
                        memcpy(buf + blen, word, word_len);
                        blen += word_len;
                        bw += ww;
                    }
                    (void)ws;
                }
                if (blen > 0) { buf[blen] = 0; fb8_md_para_row(sb, buf, width); }
            }
            ol_n = 1;
        }
    }
}

/* ============================================================
 * 2. FluxMarkdownViewer
 * ============================================================ */

void flux_md_viewer_init(FluxMarkdownViewer *v, const char *md, int show_toc) {
    if (!v) return;
    memset(v, 0, sizeof *v);
    v->md = md;
    v->show_toc = show_toc;
    v->toc_width = show_toc ? 28 : 0;
    flux_scroll_init(&v->scroll);
    /* count headings */
    if (md) {
        const char *p = md; int in_code = 0;
        while (*p) {
            if (p[0] == '`' && p[1] == '`' && p[2] == '`') in_code = !in_code;
            if (!in_code && (p == md || *(p-1) == '\n') && *p == '#') {
                v->toc_count++;
            }
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }
}

int flux_md_viewer_update(FluxMarkdownViewer *v, const FluxEvent *ev) {
    if (!v || !ev) return 0;
    if (ev->type == MSG_KEY) {
        if (v->show_toc) {
            if (flux_key_is(*ev, "up")) {
                if (v->toc_sel > 0) v->toc_sel--;
                return 1;
            }
            if (flux_key_is(*ev, "down")) {
                if (v->toc_sel < v->toc_count - 1) v->toc_sel++;
                return 1;
            }
            if (flux_key_is(*ev, "enter")) {
                /* approximate: jump scroll proportionally */
                if (v->toc_count > 0) {
                    v->scroll.scroll = v->toc_sel * 4;
                }
                return 1;
            }
        }
        return flux_scroll_update(&v->scroll, *ev);
    }
    if (ev->type == MSG_MOUSE) {
        return flux_scroll_update(&v->scroll, *ev);
    }
    return 0;
}

void flux_md_viewer_render(FluxMarkdownViewer *v, FluxSB *sb, int width, int height) {
    if (!v || !sb || width <= 0 || height <= 0) return;
    if (!v->md) {
        int i; for (i = 0; i < height; i++) { fb8_pad(sb, width); flux_sb_nl(sb); }
        return;
    }
    int body_w = width;
    int toc_w = 0;
    if (v->show_toc && width > 32) {
        toc_w = v->toc_width > 0 ? v->toc_width : 28;
        if (toc_w > width / 2) toc_w = width / 2;
        body_w = width - toc_w - 1;
    }

    /* render md to scratch */
    static char scratch[131072];
    FluxSB tmp;
    flux_sb_init(&tmp, scratch, sizeof scratch);
    flux_markdown(&tmp, v->md, body_w);

    if (toc_w == 0) {
        flux_scrollview_render(&v->scroll, sb, flux_sb_str(&tmp), body_w, height);
        return;
    }

    /* Render row by row: TOC | body */
    /* TOC: extract heading lines */
    const char *headings[256];
    int hcount = 0;
    {
        const char *p = v->md; int in_code = 0;
        while (*p && hcount < 256) {
            if (p[0] == '`' && p[1] == '`' && p[2] == '`') in_code = !in_code;
            if (!in_code && (p == v->md || *(p-1) == '\n') && *p == '#') {
                headings[hcount++] = p;
            }
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }

    /* Slice body lines into an array of row pointers */
    const char *body_text = flux_sb_str(&tmp);
    /* count rows */
    int total = 0;
    {
        const char *p = body_text;
        while (*p) {
            total++;
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }
    int top = v->scroll.scroll;
    if (top > total - height) top = total - height;
    if (top < 0) top = 0;
    v->scroll.scroll = top;
    v->scroll.total_lines = total;
    v->scroll.viewport_h = height;

    /* iterate viewport rows */
    const char *p = body_text;
    int row = 0;
    while (*p && row < top) {
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        row++;
    }

    int r;
    for (r = 0; r < height; r++) {
        /* TOC cell */
        if (r < hcount) {
            const char *h = headings[r];
            /* skip # */
            int lvl = 0; while (h[lvl] == '#') lvl++;
            int off = lvl; while (h[off] == ' ') off++;
            char buf[256]; int j = 0;
            while (h[off + j] && h[off + j] != '\n' && j < (int)sizeof(buf) - 8) {
                buf[j] = h[off + j]; j++;
            }
            buf[j] = 0;
            char display[300];
            snprintf(display, sizeof display, "%*s%s%s",
                     (lvl - 1) * 2, "",
                     (r == v->toc_sel) ? "❯ " : "  ",
                     buf);
            char trunc[300];
            flux_truncate(display, toc_w, "…", trunc, sizeof trunc);
            if (r == v->toc_sel) flux_sb_append(sb, FLUX_BOLD FLUX_THEME_BRAND_PURPLE_FG);
            else                 flux_sb_append(sb, FLUX_THEME_TEXT2_FG);
            flux_sb_append(sb, trunc);
            flux_sb_append(sb, FLUX_RESET);
            int tw = flux_strwidth(trunc);
            fb8_pad(sb, toc_w - tw);
        } else {
            fb8_pad(sb, toc_w);
        }
        /* divider */
        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);

        /* body line */
        if (*p) {
            const char *line = p;
            while (*p && *p != '\n') p++;
            int len = (int)(p - line);
            if (*p == '\n') p++;
            char b[4096];
            if (len > (int)sizeof(b) - 1) len = sizeof(b) - 1;
            memcpy(b, line, len); b[len] = 0;
            /* body is already padded to body_w */
            flux_sb_append(sb, b);
            int bw = flux_strwidth(b);
            if (bw < body_w) fb8_pad(sb, body_w - bw);
        } else {
            fb8_pad(sb, body_w);
        }
        flux_sb_nl(sb);
    }
}

/* ============================================================
 * 3. flux_syntax_highlight
 * ============================================================ */

/* Per-language sorted keyword tables */
static const char *fb8_kw_c[]    = {
    "auto","break","case","char","const","continue","default","do",
    "double","else","enum","extern","float","for","goto","if","int",
    "long","register","return","short","signed","sizeof","static",
    "struct","switch","typedef","union","unsigned","void","volatile","while"
};
static const int fb8_kw_c_n = (int)(sizeof(fb8_kw_c) / sizeof(fb8_kw_c[0]));

static const char *fb8_kw_js[]   = {
    "async","await","break","case","catch","class","const","continue",
    "default","delete","do","else","export","extends","false","finally",
    "for","function","if","import","in","instanceof","let","new","null",
    "of","return","super","switch","this","throw","true","try","typeof",
    "undefined","var","void","while","with","yield"
};
static const int fb8_kw_js_n = (int)(sizeof(fb8_kw_js) / sizeof(fb8_kw_js[0]));

static const char *fb8_kw_ts[]   = {
    "any","as","async","await","boolean","break","case","catch","class",
    "const","continue","default","delete","do","else","enum","export",
    "extends","false","finally","for","function","if","implements",
    "import","in","instanceof","interface","let","never","new","null",
    "number","of","private","protected","public","readonly","return",
    "string","super","switch","this","throw","true","try","type","typeof",
    "undefined","var","void","while","yield"
};
static const int fb8_kw_ts_n = (int)(sizeof(fb8_kw_ts) / sizeof(fb8_kw_ts[0]));

static const char *fb8_kw_json[] = { "false","null","true" };
static const int fb8_kw_json_n = 3;

static const char *fb8_kw_sh[]   = {
    "case","do","done","elif","else","esac","export","fi","for",
    "function","if","in","local","return","then","while"
};
static const int fb8_kw_sh_n = (int)(sizeof(fb8_kw_sh) / sizeof(fb8_kw_sh[0]));

static int fb8_is_kw(const char *word, const char **table, int n) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = strcmp(word, table[mid]);
        if (cmp == 0) return 1;
        if (cmp < 0) hi = mid - 1; else lo = mid + 1;
    }
    return 0;
}

static int fb8_is_ident_start(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static int fb8_is_ident_cont(unsigned char c) {
    return fb8_is_ident_start(c) || (c >= '0' && c <= '9');
}

/* Highlight a single line into sb with target_w. Tracks cells emitted to
 * cap output at width. */
static void fb8_hl_line(FluxSB *sb, const char *line, FluxLang lang, int width) {
    int i = 0, w = 0;
    int len = (int)strlen(line);
    int (*is_kw_fn)(const char *) = NULL;
    const char **kwt = NULL; int kwn = 0;
    const char *line_cmt = "//";
    int line_cmt_len = 2;
    int has_block_cmt = 1;
    int has_preproc = 0;

    switch (lang) {
    case FLUX_LANG_C:
        kwt = fb8_kw_c; kwn = fb8_kw_c_n;
        has_preproc = 1;
        break;
    case FLUX_LANG_JS:
        kwt = fb8_kw_js; kwn = fb8_kw_js_n; break;
    case FLUX_LANG_TS:
        kwt = fb8_kw_ts; kwn = fb8_kw_ts_n; break;
    case FLUX_LANG_JSON:
        kwt = fb8_kw_json; kwn = fb8_kw_json_n;
        line_cmt = NULL; has_block_cmt = 0; break;
    case FLUX_LANG_SH:
        kwt = fb8_kw_sh; kwn = fb8_kw_sh_n;
        line_cmt = "#"; line_cmt_len = 1; has_block_cmt = 0; break;
    case FLUX_LANG_MD:
        line_cmt = NULL; has_block_cmt = 0; break;
    case FLUX_LANG_PLAIN:
    default:
        line_cmt = NULL; has_block_cmt = 0; break;
    }
    (void)is_kw_fn;

    /* preproc: line starts with # (after optional ws) for C */
    if (has_preproc) {
        int j = 0; while (line[j] == ' ' || line[j] == '\t') j++;
        if (line[j] == '#') {
            char tmp[2048];
            flux_truncate(line, width, "…", tmp, sizeof tmp);
            flux_sb_append(sb, FLUX_BOLD);
            flux_sb_append(sb, FLUX_THEME_SYNTAX_KEYWORD_FG);
            flux_sb_append(sb, tmp);
            flux_sb_append(sb, FLUX_RESET);
            int tw = flux_strwidth(tmp);
            fb8_pad(sb, width - tw);
            flux_sb_nl(sb);
            return;
        }
    }

    while (i < len && w < width) {
        unsigned char c = (unsigned char)line[i];

        /* line comment */
        if (line_cmt && i + line_cmt_len <= len &&
            strncmp(line + i, line_cmt, line_cmt_len) == 0) {
            int avail = width - w;
            char tmp[2048];
            flux_truncate(line + i, avail, "…", tmp, sizeof tmp);
            flux_sb_append(sb, FLUX_DIM);
            flux_sb_append(sb, FLUX_THEME_SYNTAX_COMMENT_FG);
            flux_sb_append(sb, tmp);
            flux_sb_append(sb, FLUX_RESET);
            w += flux_strwidth(tmp);
            i = len;
            break;
        }

        /* block comment slash-star ... star-slash */
        if (has_block_cmt && i + 1 < len && line[i] == '/' && line[i+1] == '*') {
            int j = i + 2;
            while (j + 1 < len && !(line[j] == '*' && line[j+1] == '/')) j++;
            if (j + 1 < len) j += 2; else j = len;
            int seg_len = j - i;
            char tmp[2048];
            if (seg_len > (int)sizeof(tmp) - 1) seg_len = sizeof(tmp) - 1;
            memcpy(tmp, line + i, seg_len); tmp[seg_len] = 0;
            int avail = width - w;
            char trunc[2048];
            flux_truncate(tmp, avail, "…", trunc, sizeof trunc);
            flux_sb_append(sb, FLUX_DIM);
            flux_sb_append(sb, FLUX_THEME_SYNTAX_COMMENT_FG);
            flux_sb_append(sb, trunc);
            flux_sb_append(sb, FLUX_RESET);
            w += flux_strwidth(trunc);
            i = j;
            continue;
        }

        /* string */
        if (c == '"' || c == '\'' ||
            ((lang == FLUX_LANG_JS || lang == FLUX_LANG_TS) && c == '`')) {
            char quote = (char)c;
            int j = i + 1;
            while (j < len && line[j] != quote) {
                if (line[j] == '\\' && j + 1 < len) j += 2; else j++;
            }
            if (j < len) j++;
            int seg_len = j - i;
            char tmp[2048];
            if (seg_len > (int)sizeof(tmp) - 1) seg_len = sizeof(tmp) - 1;
            memcpy(tmp, line + i, seg_len); tmp[seg_len] = 0;
            int avail = width - w;
            char trunc[2048];
            flux_truncate(tmp, avail, "…", trunc, sizeof trunc);
            flux_sb_append(sb, FLUX_THEME_SYNTAX_STRING_FG);
            flux_sb_append(sb, trunc);
            flux_sb_append(sb, FLUX_RESET);
            w += flux_strwidth(trunc);
            i = j;
            continue;
        }

        /* shell var $VAR or ${...} */
        if (lang == FLUX_LANG_SH && c == '$') {
            int j = i + 1;
            if (line[j] == '{') {
                while (j < len && line[j] != '}') j++;
                if (j < len) j++;
            } else {
                while (j < len && fb8_is_ident_cont((unsigned char)line[j])) j++;
            }
            int seg_len = j - i;
            char tmp[256];
            if (seg_len > (int)sizeof(tmp) - 1) seg_len = sizeof(tmp) - 1;
            memcpy(tmp, line + i, seg_len); tmp[seg_len] = 0;
            int avail = width - w;
            char trunc[256];
            flux_truncate(tmp, avail, "…", trunc, sizeof trunc);
            flux_sb_append(sb, FLUX_THEME_SYNTAX_TYPE_FG);
            flux_sb_append(sb, trunc);
            flux_sb_append(sb, FLUX_RESET);
            w += flux_strwidth(trunc);
            i = j;
            continue;
        }

        /* number */
        if (c >= '0' && c <= '9') {
            int j = i;
            if (line[i] == '0' && (line[i+1] == 'x' || line[i+1] == 'X')) {
                j = i + 2;
                while (j < len && (isxdigit((unsigned char)line[j]))) j++;
            } else {
                while (j < len && (isdigit((unsigned char)line[j]) || line[j] == '.')) j++;
            }
            int seg_len = j - i;
            char tmp[64];
            if (seg_len > 63) seg_len = 63;
            memcpy(tmp, line + i, seg_len); tmp[seg_len] = 0;
            int avail = width - w;
            char trunc[64];
            flux_truncate(tmp, avail, "…", trunc, sizeof trunc);
            flux_sb_append(sb, FLUX_THEME_SYNTAX_NUMBER_FG);
            flux_sb_append(sb, trunc);
            flux_sb_append(sb, FLUX_RESET);
            w += flux_strwidth(trunc);
            i = j;
            continue;
        }

        /* identifier */
        if (fb8_is_ident_start(c)) {
            int j = i + 1;
            while (j < len && fb8_is_ident_cont((unsigned char)line[j])) j++;
            int seg_len = j - i;
            char id[128];
            if (seg_len > 127) seg_len = 127;
            memcpy(id, line + i, seg_len); id[seg_len] = 0;
            int is_keyword = (kwt && fb8_is_kw(id, kwt, kwn));
            int is_type = 0;
            if ((lang == FLUX_LANG_TS || lang == FLUX_LANG_JS) && !is_keyword &&
                id[0] >= 'A' && id[0] <= 'Z') {
                is_type = 1;
            }
            int avail = width - w;
            char trunc[128];
            flux_truncate(id, avail, "…", trunc, sizeof trunc);
            if (is_keyword) {
                flux_sb_append(sb, FLUX_BOLD);
                flux_sb_append(sb, FLUX_THEME_SYNTAX_KEYWORD_FG);
            } else if (is_type) {
                flux_sb_append(sb, FLUX_THEME_SYNTAX_TYPE_FG);
            } else {
                flux_sb_append(sb, FLUX_THEME_TEXT_FG);
            }
            flux_sb_append(sb, trunc);
            flux_sb_append(sb, FLUX_RESET);
            w += flux_strwidth(trunc);
            i = j;
            continue;
        }

        /* operator-ish */
        if (strchr("+-*/%=<>!&|^~?:;,(){}[].", c)) {
            char tmp[2] = { (char)c, 0 };
            flux_sb_append(sb, FLUX_THEME_SYNTAX_OPERATOR_FG);
            flux_sb_append(sb, tmp);
            flux_sb_append(sb, FLUX_RESET);
            w++;
            i++;
            continue;
        }

        /* whitespace / other */
        {
            char tmp[2] = { (char)c, 0 };
            flux_sb_append(sb, tmp);
            if (c == '\t') {
                /* consume tab as 1 cell — keep width math simple */
                w++;
            } else {
                w++;
            }
            i++;
        }
    }

    if (w < width) fb8_pad(sb, width - w);
    flux_sb_nl(sb);
}

void flux_syntax_highlight(FluxSB *sb, const char *code,
                           FluxLang lang, int width) {
    if (!sb || !code || width <= 0) return;
    const char *p = code;
    while (*p) {
        char line[4096];
        int n = 0;
        while (*p && *p != '\n' && n < (int)sizeof(line) - 1)
            line[n++] = *p++;
        line[n] = 0;
        if (*p == '\n') p++;
        fb8_hl_line(sb, line, lang, width);
    }
}

/* ============================================================
 * 4. FluxPerfHud
 * ============================================================ */

#ifdef __linux__
static long fb8_clk_tck(void) {
    long t = sysconf(_SC_CLK_TCK);
    if (t <= 0) t = 100;
    return t;
}
static long fb8_page_size(void) {
    long p = sysconf(_SC_PAGESIZE);
    if (p <= 0) p = 4096;
    return p;
}

static int fb8_read_proc_stat(uint64_t *utime, uint64_t *stime, long *rss_pages) {
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return 0;
    char buf[2048];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return 0;
    buf[n] = 0;
    /* skip past comm: find ')' from end */
    char *rp = strrchr(buf, ')');
    if (!rp) return 0;
    rp++;
    /* fields after comm: state(3) ppid(4) ... utime is 14 (so offset 11 after comm) */
    int field = 3;
    char *tok = strtok(rp, " \t\n");
    long ut = 0, st = 0, rss = 0;
    while (tok) {
        field++;
        if (field == 14) ut = atol(tok);
        else if (field == 15) st = atol(tok);
        else if (field == 24) { rss = atol(tok); break; }
        tok = strtok(NULL, " \t\n");
    }
    if (utime) *utime = (uint64_t)ut;
    if (stime) *stime = (uint64_t)st;
    if (rss_pages) *rss_pages = rss;
    return 1;
}

static long fb8_read_meminfo_total_kb(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256];
    long kb = 0;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%ld", &kb);
            break;
        }
    }
    fclose(f);
    return kb;
}
#endif

void flux_perf_hud_init(FluxPerfHud *h, const char *title) {
    if (!h) return;
    memset(h, 0, sizeof *h);
    snprintf(h->title, sizeof h->title, "%s",
             title ? title : "flux.h HUD");
    h->visible = 1;
#ifdef __linux__
    h->mem_total_kb = fb8_read_meminfo_total_kb();
    h->last_tick_ns = fb8_now_ns();
    fb8_read_proc_stat(&h->last_proc_utime, &h->last_proc_stime, NULL);
#endif
}

void flux_perf_hud_tick(FluxPerfHud *h, float render_ms) {
    if (!h) return;
    uint64_t now = fb8_now_ns();
    float dt = (now - h->last_tick_ns) / 1e9f;
    if (dt <= 0.0f) dt = 1e-3f;
    h->cur_fps = 1.0f / dt;
    h->cur_rt  = render_ms;

#ifdef __linux__
    uint64_t ut = 0, st = 0; long rss = 0;
    if (fb8_read_proc_stat(&ut, &st, &rss)) {
        long tck = fb8_clk_tck();
        uint64_t du = ut - h->last_proc_utime;
        uint64_t ds = st - h->last_proc_stime;
        h->cur_cpu = (float)((du + ds) / (double)tck) / dt * 100.0f;
        h->last_proc_utime = ut;
        h->last_proc_stime = st;
        h->cur_rss_mb = (float)((double)rss * fb8_page_size() / (1024.0 * 1024.0));
    } else {
        h->cur_cpu = 0;
        h->cur_rss_mb = 0;
    }
#else
    h->cur_cpu = 0;
    h->cur_rss_mb = 0;
#endif

    h->fps_hist[h->hist_idx] = h->cur_fps;
    h->rt_hist [h->hist_idx] = render_ms;
    h->mem_hist[h->hist_idx] = h->cur_rss_mb;
    h->hist_idx = (h->hist_idx + 1) % 20;
    if (h->hist_idx == 0) h->hist_full = 1;
    h->last_tick_ns = now;
}

void flux_perf_hud_render(const FluxPerfHud *h, FluxSB *sb, int width) {
    if (!h || !sb || width <= 0) return;
    if (!h->visible) return;

    /* Single-line degraded mode */
    if (width <= 30) {
        char one[128];
        snprintf(one, sizeof one, "FPS %2.0f | RT %4.1fms",
                 h->cur_fps, h->cur_rt);
        fb8_row(sb, one, width);
        return;
    }

    /* Bordered round box: top, 4 rows, bottom = 6 rows */
    int inner = width - 2;

    /* row helpers */
    /* top */
    flux_sb_append(sb, FLUX_THEME_BORDER_FG);
    flux_sb_append(sb, "\xe2\x95\xad");
    int k; for (k = 0; k < inner; k++) flux_sb_append(sb, "\xe2\x94\x80");
    flux_sb_append(sb, "\xe2\x95\xae");
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    /* title row */
    {
        char buf[64];
        snprintf(buf, sizeof buf, " %s ", h->title);
        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_append(sb, FLUX_BOLD);
        flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
        char tmp[256]; flux_truncate(buf, inner, "…", tmp, sizeof tmp);
        flux_sb_append(sb, tmp);
        flux_sb_append(sb, FLUX_RESET);
        int tw = flux_strwidth(tmp);
        fb8_pad(sb, inner - tw);
        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* helper for rows: "│ <text>...        │" */
    {
        char body[256];
        char tmp[256];
        int tw;

        #define FB8_HUD_ROW() do { \
            flux_truncate(body, inner, "…", tmp, sizeof tmp); \
            flux_sb_append(sb, FLUX_THEME_BORDER_FG); flux_sb_append(sb, "\xe2\x94\x82"); flux_sb_append(sb, FLUX_RESET); \
            flux_sb_append(sb, FLUX_THEME_TEXT_FG); flux_sb_append(sb, tmp); flux_sb_append(sb, FLUX_RESET); \
            tw = flux_strwidth(tmp); fb8_pad(sb, inner - tw); \
            flux_sb_append(sb, FLUX_THEME_BORDER_FG); flux_sb_append(sb, "\xe2\x94\x82"); flux_sb_append(sb, FLUX_RESET); \
            flux_sb_nl(sb); \
        } while (0)

        snprintf(body, sizeof body, " FPS %3.0f", h->cur_fps);
        FB8_HUD_ROW();
        snprintf(body, sizeof body, " RT  %4.1fms", h->cur_rt);
        FB8_HUD_ROW();
        if (h->mem_total_kb > 0) {
            snprintf(body, sizeof body, " RSS %4.1fMB / %ldMB",
                     h->cur_rss_mb, h->mem_total_kb / 1024);
        } else {
            snprintf(body, sizeof body, " RSS %4.1fMB", h->cur_rss_mb);
        }
        FB8_HUD_ROW();
        snprintf(body, sizeof body, " CPU %4.1f%%", h->cur_cpu);
        FB8_HUD_ROW();

        #undef FB8_HUD_ROW
    }

    /* bottom */
    flux_sb_append(sb, FLUX_THEME_BORDER_FG);
    flux_sb_append(sb, "\xe2\x95\xb0");
    for (k = 0; k < inner; k++) flux_sb_append(sb, "\xe2\x94\x80");
    flux_sb_append(sb, "\xe2\x95\xaf");
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ============================================================
 * 5. FluxCalendar
 * ============================================================ */

static int fb8_is_leap(int y) {
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}
static int fb8_days_in_month(int y, int m) {
    static const int d[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (m < 1 || m > 12) return 30;
    if (m == 2 && fb8_is_leap(y)) return 29;
    return d[m - 1];
}

/* Zeller's congruence for Gregorian; returns 0..6 with 0 = Sunday. */
static int fb8_dow(int y, int m, int d) {
    if (m < 3) { m += 12; y--; }
    int K = y % 100;
    int J = y / 100;
    int h = (d + (13 * (m + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
    /* Zeller h: 0=Sat,1=Sun,2=Mon,...; convert to 0=Sun..6=Sat */
    int dow = (h + 6) % 7;
    return dow;
}

void flux_calendar_init(FluxCalendar *c, int year, int month, int sel_day) {
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->year = year; c->month = month; c->sel_day = sel_day;
    c->week_starts_mon = 0;
}

static void fb8_cal_advance_month(FluxCalendar *c, int delta) {
    int m = c->month + delta;
    while (m > 12) { m -= 12; c->year++; }
    while (m < 1)  { m += 12; c->year--; }
    c->month = m;
    int dim = fb8_days_in_month(c->year, c->month);
    if (c->sel_day > dim) c->sel_day = dim;
    if (c->sel_day < 1)   c->sel_day = 1;
}

int flux_calendar_update(FluxCalendar *c, const FluxEvent *ev) {
    if (!c || !ev) return 0;
    if (ev->type != MSG_KEY) return 0;
    int dim = fb8_days_in_month(c->year, c->month);
    if (c->sel_day < 1) c->sel_day = 1;
    if (flux_key_is(*ev, "left")) {
        c->sel_day--;
        if (c->sel_day < 1) {
            fb8_cal_advance_month(c, -1);
            c->sel_day = fb8_days_in_month(c->year, c->month);
        }
        return 1;
    }
    if (flux_key_is(*ev, "right")) {
        c->sel_day++;
        if (c->sel_day > dim) {
            fb8_cal_advance_month(c, +1);
            c->sel_day = 1;
        }
        return 1;
    }
    if (flux_key_is(*ev, "up")) {
        c->sel_day -= 7;
        if (c->sel_day < 1) {
            fb8_cal_advance_month(c, -1);
            int ndim = fb8_days_in_month(c->year, c->month);
            c->sel_day += ndim;
        }
        return 1;
    }
    if (flux_key_is(*ev, "down")) {
        c->sel_day += 7;
        if (c->sel_day > dim) {
            int over = c->sel_day - dim;
            fb8_cal_advance_month(c, +1);
            c->sel_day = over;
        }
        return 1;
    }
    if (flux_key_is(*ev, "pgup"))   { fb8_cal_advance_month(c, -1); return 1; }
    if (flux_key_is(*ev, "pgdown")) { fb8_cal_advance_month(c, +1); return 1; }
    return 0;
}

static const char *fb8_month_name(int m) {
    static const char *names[] = {
        "January","February","March","April","May","June",
        "July","August","September","October","November","December"
    };
    if (m < 1 || m > 12) return "?";
    return names[m - 1];
}

void flux_calendar_render(const FluxCalendar *c, FluxSB *sb, int width) {
    if (!c || !sb || width <= 0) return;
    int min_w = 22;
    if (width < min_w) min_w = width;

    /* refresh today */
    int today_y = 0, today_m = 0, today_d = 0;
    {
        time_t t = time(NULL);
        struct tm tmv;
#ifdef _GNU_SOURCE
        localtime_r(&t, &tmv);
#else
        struct tm *tp = localtime(&t);
        tmv = *tp;
#endif
        today_y = tmv.tm_year + 1900;
        today_m = tmv.tm_mon + 1;
        today_d = tmv.tm_mday;
    }

    /* row 1: < Month YYYY > */
    {
        char title[64];
        snprintf(title, sizeof title, "< %s %d >",
                 fb8_month_name(c->month), c->year);
        if (c->focused) flux_sb_append(sb, FLUX_BOLD);
        flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
        char tmp[64]; flux_truncate(title, width, "…", tmp, sizeof tmp);
        /* center */
        int tw = flux_strwidth(tmp);
        int lp = (width - tw) / 2;
        if (lp < 0) lp = 0;
        fb8_pad(sb, lp);
        flux_sb_append(sb, tmp);
        flux_sb_append(sb, FLUX_RESET);
        fb8_pad(sb, width - lp - tw);
        flux_sb_nl(sb);
    }

    /* row 2: weekday header */
    {
        const char *days_sun[] = { "Su","Mo","Tu","We","Th","Fr","Sa" };
        const char *days_mon[] = { "Mo","Tu","We","Th","Fr","Sa","Su" };
        const char **dn = c->week_starts_mon ? days_mon : days_sun;
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
        int header_w = 0;
        int i;
        for (i = 0; i < 7; i++) {
            flux_sb_append(sb, dn[i]);
            header_w += 2;
            if (i < 6) { flux_sb_append(sb, " "); header_w++; }
        }
        flux_sb_append(sb, FLUX_RESET);
        if (header_w < width) fb8_pad(sb, width - header_w);
        flux_sb_nl(sb);
    }

    /* day grid */
    int dim = fb8_days_in_month(c->year, c->month);
    int first_dow = fb8_dow(c->year, c->month, 1);
    if (c->week_starts_mon) first_dow = (first_dow + 6) % 7;
    int day = 1;
    int row, col;
    for (row = 0; row < 6 && day <= dim; row++) {
        int written = 0;
        for (col = 0; col < 7; col++) {
            int show = (row == 0 && col < first_dow) ? 0 : (day <= dim);
            if (show) {
                int is_sel = (day == c->sel_day);
                int is_today = (today_y == c->year && today_m == c->month && today_d == day);
                if (is_sel) {
                    flux_sb_append(sb, FLUX_BOLD);
                    flux_sb_append(sb, "\x1b[7m");
                    flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
                } else if (is_today) {
                    flux_sb_append(sb, FLUX_UNDERLINE);
                    flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
                } else {
                    flux_sb_append(sb, FLUX_THEME_TEXT_FG);
                }
                char tmp[16];
                snprintf(tmp, sizeof tmp, "%2d", day);
                flux_sb_append(sb, tmp);
                flux_sb_append(sb, FLUX_RESET);
                day++;
            } else {
                flux_sb_append(sb, "  ");
            }
            written += 2;
            if (col < 6) {
                flux_sb_append(sb, " ");
                written++;
            }
        }
        if (written < width) fb8_pad(sb, width - written);
        flux_sb_nl(sb);
    }
    /* If less than 6 day rows printed, pad to keep stable height */
    for (; row < 6; row++) {
        fb8_pad(sb, width);
        flux_sb_nl(sb);
    }
}

/* ============================================================
 * 6. FluxDatePicker
 * ============================================================ */

void flux_datepicker_init(FluxDatePicker *d, int y, int m, int day) {
    if (!d) return;
    memset(d, 0, sizeof *d);
    d->year = y; d->month = m; d->day = day;
    snprintf(d->fmt, sizeof d->fmt, "YYYY-MM-DD");
    snprintf(d->placeholder, sizeof d->placeholder, "Pick a date");
    flux_calendar_init(&d->cal, y > 0 ? y : 2026, m > 0 ? m : 1, day > 0 ? day : 1);
}

int flux_datepicker_update(FluxDatePicker *d, const FluxEvent *ev) {
    if (!d || !ev) return 0;
    if (!d->open) {
        if (d->focused && ev->type == MSG_KEY) {
            if (flux_key_is(*ev, "enter") ||
                (ev->u.key.key[0] == ' ' && ev->u.key.key[1] == 0)) {
                d->open = 1;
                return 1;
            }
        }
        return 0;
    }
    if (ev->type == MSG_KEY) {
        if (flux_key_is(*ev, "enter")) {
            d->year = d->cal.year;
            d->month = d->cal.month;
            d->day = d->cal.sel_day;
            d->open = 0;
            return 1;
        }
        if (flux_key_is(*ev, "esc")) {
            d->open = 0;
            return 1;
        }
    }
    return flux_calendar_update(&d->cal, ev);
}

static void fb8_format_date(const FluxDatePicker *d, char *out, int cap) {
    /* tokens: YYYY YY MMMM MMM MM DD */
    int oi = 0; int i = 0;
    const char *fmt = d->fmt;
    static const char *month_full[] = {
        "January","February","March","April","May","June","July","August",
        "September","October","November","December"
    };
    static const char *month_abbr[] = {
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
    };
    while (fmt[i] && oi < cap - 1) {
        if (strncmp(fmt + i, "YYYY", 4) == 0) {
            oi += snprintf(out + oi, cap - oi, "%04d", d->year); i += 4;
        } else if (strncmp(fmt + i, "YY", 2) == 0) {
            oi += snprintf(out + oi, cap - oi, "%02d", d->year % 100); i += 2;
        } else if (strncmp(fmt + i, "MMMM", 4) == 0) {
            if (d->month >= 1 && d->month <= 12)
                oi += snprintf(out + oi, cap - oi, "%s", month_full[d->month - 1]);
            i += 4;
        } else if (strncmp(fmt + i, "MMM", 3) == 0) {
            if (d->month >= 1 && d->month <= 12)
                oi += snprintf(out + oi, cap - oi, "%s", month_abbr[d->month - 1]);
            i += 3;
        } else if (strncmp(fmt + i, "MM", 2) == 0) {
            oi += snprintf(out + oi, cap - oi, "%02d", d->month); i += 2;
        } else if (strncmp(fmt + i, "DD", 2) == 0) {
            oi += snprintf(out + oi, cap - oi, "%02d", d->day); i += 2;
        } else {
            out[oi++] = fmt[i++];
        }
    }
    out[oi] = 0;
}

void flux_datepicker_render(FluxDatePicker *d, FluxSB *sb, int width) {
    if (!d || !sb || width <= 0) return;
    /* closed row */
    char text[128];
    if (d->day == 0) {
        snprintf(text, sizeof text, "[%s \xe2\x96\xbe]", d->placeholder);
    } else {
        char dt[64];
        fb8_format_date(d, dt, sizeof dt);
        snprintf(text, sizeof text, "[%s \xe2\x96\xbe]", dt);
    }
    if (d->focused) flux_sb_append(sb, FLUX_BOLD);
    flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
    char tmp[128]; flux_truncate(text, width, "…", tmp, sizeof tmp);
    flux_sb_append(sb, tmp);
    flux_sb_append(sb, FLUX_RESET);
    int tw = flux_strwidth(tmp);
    fb8_pad(sb, width - tw);
    flux_sb_nl(sb);

    if (d->open) {
        flux_calendar_render(&d->cal, sb, width);
    }
}

/* ============================================================
 * 7. FluxDirTree
 * ============================================================ */

static int fb8_dirtree_grow(FluxDirTree *t, int need) {
    if (need <= t->cap) return 0;
    int new_cap = t->cap ? t->cap : 256;
    while (new_cap < need) new_cap += 256;
    FluxDirEntry *ne = (FluxDirEntry *)realloc(t->entries, sizeof(FluxDirEntry) * new_cap);
    if (!ne) return -1;
    t->entries = ne;
    t->cap = new_cap;
    return 0;
}

static int fb8_dirent_cmp(const void *a, const void *b) {
    const FluxDirEntry *ea = (const FluxDirEntry *)a;
    const FluxDirEntry *eb = (const FluxDirEntry *)b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcmp(ea->name, eb->name);
}

/* Build absolute path of entry idx into out. */
static void fb8_dirtree_path_of(const FluxDirTree *t, int idx, char *out, int cap) {
    /* walk parent chain into stack, reverse for path */
    const FluxDirEntry *chain[64];
    int n = 0;
    int cur = idx;
    while (cur >= 0 && n < 64) {
        chain[n++] = &t->entries[cur];
        cur = t->entries[cur].parent_idx;
    }
    int oi = snprintf(out, cap, "%s", t->root);
    int i;
    for (i = n - 1; i >= 0; i--) {
        if (oi < cap - 1 && (oi == 0 || out[oi - 1] != '/'))
            out[oi++] = '/';
        int j = 0;
        while (chain[i]->name[j] && oi < cap - 1) out[oi++] = chain[i]->name[j++];
    }
    if (oi < cap) out[oi] = 0;
    else out[cap - 1] = 0;
}

/* Read directory at `path`, append matching children with given depth and parent_idx
 * after position `insert_at`. Returns number of children added. */
static int fb8_dirtree_read_into(FluxDirTree *t, const char *path,
                                 int insert_at, int depth, int parent_idx) {
    DIR *dir = opendir(path);
    if (!dir) return 0;
    /* gather children */
    FluxDirEntry tmp[1024];
    int n = 0;
    struct dirent *de;
    while ((de = readdir(dir)) && n < 1024) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (!t->show_hidden && de->d_name[0] == '.') continue;
        FluxDirEntry e;
        memset(&e, 0, sizeof e);
        snprintf(e.name, sizeof e.name, "%s", de->d_name);
        e.depth = depth;
        e.parent_idx = parent_idx;
#ifdef DT_DIR
        if (de->d_type == DT_DIR) e.is_dir = 1;
        else if (de->d_type == DT_UNKNOWN) {
            char full[2048];
            snprintf(full, sizeof full, "%s/%s", path, de->d_name);
            struct stat st;
            if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) e.is_dir = 1;
        }
#else
        char full[2048];
        snprintf(full, sizeof full, "%s/%s", path, de->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) e.is_dir = 1;
#endif
        if (!e.is_dir && !t->show_files) continue;
        tmp[n++] = e;
    }
    closedir(dir);
    qsort(tmp, n, sizeof(FluxDirEntry), fb8_dirent_cmp);
    if (fb8_dirtree_grow(t, t->count + n) < 0) return 0;
    /* shift right starting from insert_at */
    if (insert_at < t->count) {
        memmove(&t->entries[insert_at + n], &t->entries[insert_at],
                sizeof(FluxDirEntry) * (t->count - insert_at));
    }
    memcpy(&t->entries[insert_at], tmp, sizeof(FluxDirEntry) * n);
    t->count += n;
    /* fix parent_idx of shifted entries */
    int i;
    for (i = insert_at + n; i < t->count; i++) {
        if (t->entries[i].parent_idx >= insert_at)
            t->entries[i].parent_idx += n;
    }
    return n;
}

int flux_dirtree_init(FluxDirTree *t, const char *root_path) {
    if (!t || !root_path) return -1;
    memset(t, 0, sizeof *t);
    t->show_files = 1;
    /* realpath */
    char *rp = realpath(root_path, NULL);
    if (rp) {
        snprintf(t->root, sizeof t->root, "%s", rp);
        free(rp);
    } else {
        snprintf(t->root, sizeof t->root, "%s", root_path);
    }
    int n = fb8_dirtree_read_into(t, t->root, 0, 0, -1);
    if (n == 0) {
        /* not necessarily an error — could be empty dir; but if opendir
         * failed we want -1. Try opendir explicitly. */
        DIR *d = opendir(t->root);
        if (!d) return -1;
        closedir(d);
    }
    return 0;
}

void flux_dirtree_free(FluxDirTree *t) {
    if (!t) return;
    free(t->entries);
    t->entries = NULL;
    t->count = t->cap = 0;
}

static void fb8_dirtree_collapse(FluxDirTree *t, int idx) {
    if (idx < 0 || idx >= t->count) return;
    FluxDirEntry *p = &t->entries[idx];
    if (!p->expanded) return;
    int j = idx + 1;
    while (j < t->count && t->entries[j].depth > p->depth) j++;
    int removed = j - (idx + 1);
    if (removed > 0) {
        memmove(&t->entries[idx + 1], &t->entries[j],
                sizeof(FluxDirEntry) * (t->count - j));
        t->count -= removed;
        /* fix parent_idx of remaining entries */
        int i;
        for (i = idx + 1; i < t->count; i++) {
            if (t->entries[i].parent_idx > idx)
                t->entries[i].parent_idx -= removed;
        }
    }
    p->expanded = 0;
}

static void fb8_dirtree_expand(FluxDirTree *t, int idx) {
    if (idx < 0 || idx >= t->count) return;
    FluxDirEntry *p = &t->entries[idx];
    if (!p->is_dir || p->expanded) return;
    char path[2048];
    fb8_dirtree_path_of(t, idx, path, sizeof path);
    int added = fb8_dirtree_read_into(t, path, idx + 1, p->depth + 1, idx);
    (void)added;
    p->expanded = 1;
}

int flux_dirtree_update(FluxDirTree *t, const FluxEvent *ev) {
    if (!t || !ev) return 0;
    if (ev->type != MSG_KEY) return 0;
    if (flux_key_is(*ev, "up") || flux_key_is(*ev, "k")) {
        if (t->cursor > 0) t->cursor--;
        return 1;
    }
    if (flux_key_is(*ev, "down") || flux_key_is(*ev, "j")) {
        if (t->cursor < t->count - 1) t->cursor++;
        return 1;
    }
    if (flux_key_is(*ev, "right") || flux_key_is(*ev, "enter")) {
        if (t->cursor < t->count && t->entries[t->cursor].is_dir) {
            if (!t->entries[t->cursor].expanded) {
                fb8_dirtree_expand(t, t->cursor);
            } else {
                /* descend: move cursor to first child */
                if (t->cursor + 1 < t->count &&
                    t->entries[t->cursor + 1].depth > t->entries[t->cursor].depth) {
                    t->cursor++;
                }
            }
        }
        return 1;
    }
    if (flux_key_is(*ev, "left")) {
        if (t->cursor < t->count && t->entries[t->cursor].is_dir &&
            t->entries[t->cursor].expanded) {
            fb8_dirtree_collapse(t, t->cursor);
        } else if (t->cursor < t->count &&
                   t->entries[t->cursor].parent_idx >= 0) {
            t->cursor = t->entries[t->cursor].parent_idx;
        }
        return 1;
    }
    if (flux_key_is(*ev, "home")) { t->cursor = 0; return 1; }
    if (flux_key_is(*ev, "end"))  { t->cursor = t->count - 1; return 1; }
    if (flux_key_is(*ev, "pgup")) {
        t->cursor -= 10;
        if (t->cursor < 0) t->cursor = 0;
        return 1;
    }
    if (flux_key_is(*ev, "pgdown")) {
        t->cursor += 10;
        if (t->cursor >= t->count) t->cursor = t->count - 1;
        return 1;
    }
    if (ev->u.key.key[0] == '.' && ev->u.key.key[1] == 0) {
        t->show_hidden = !t->show_hidden;
        /* re-list everything: drop all entries, re-init root */
        t->count = 0;
        t->cursor = 0;
        t->scroll_top = 0;
        fb8_dirtree_read_into(t, t->root, 0, 0, -1);
        return 1;
    }
    return 0;
}

void flux_dirtree_render(FluxDirTree *t, FluxSB *sb, int width, int height) {
    if (!t || !sb || width <= 0 || height <= 0) return;
    if (t->cursor < 0) t->cursor = 0;
    if (t->cursor >= t->count) t->cursor = t->count - 1;
    if (t->cursor < t->scroll_top) t->scroll_top = t->cursor;
    if (t->cursor >= t->scroll_top + height) t->scroll_top = t->cursor - height + 1;
    if (t->scroll_top < 0) t->scroll_top = 0;

    int r;
    for (r = 0; r < height; r++) {
        int idx = t->scroll_top + r;
        if (idx >= t->count) {
            fb8_pad(sb, width);
            flux_sb_nl(sb);
            continue;
        }
        FluxDirEntry *e = &t->entries[idx];
        char line[1024];
        const char *glyph = e->is_dir
            ? (e->expanded ? "\xe2\x96\xbe" : "\xe2\x96\xb8")  /* ▾ ▸ */
            : "\xe2\x94\x80"; /* ─ */
        const char *cursor_pre = (idx == t->cursor) ? "\xe2\x9d\xaf " : "  ";
        snprintf(line, sizeof line, "%s%*s%s %s%s",
                 cursor_pre,
                 e->depth * 2, "",
                 glyph,
                 e->name,
                 e->is_dir ? "/" : "");
        char tmp[1024]; flux_truncate(line, width, "…", tmp, sizeof tmp);
        if (idx == t->cursor) {
            flux_sb_append(sb, FLUX_BOLD);
            flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
        } else if (e->is_dir) {
            flux_sb_append(sb, FLUX_THEME_ACCENT_FG);
        } else {
            flux_sb_append(sb, FLUX_THEME_TEXT_FG);
        }
        flux_sb_append(sb, tmp);
        flux_sb_append(sb, FLUX_RESET);
        int tw = flux_strwidth(tmp);
        fb8_pad(sb, width - tw);
        flux_sb_nl(sb);
    }
}

const char *flux_dirtree_selected(FluxDirTree *t) {
    if (!t || t->count == 0) return NULL;
    int idx = t->cursor;
    if (idx < 0 || idx >= t->count) return NULL;
    fb8_dirtree_path_of(t, idx, t->sel_path_buf, sizeof t->sel_path_buf);
    return t->sel_path_buf;
}

/* ============================================================
 * 8. FluxFilePicker
 * ============================================================ */

void flux_filepicker_init(FluxFilePicker *p, const char *root, int mode, const char *exts) {
    if (!p) return;
    memset(p, 0, sizeof *p);
    p->mode = mode;
    p->visible = 1;
    if (exts) snprintf(p->exts, sizeof p->exts, "%s", exts);
    flux_dirtree_init(&p->tree, root ? root : ".");
    flux_input_init(&p->filename, mode == 1 ? "filename.txt" : "");
    p->right_focus = 0;
    p->tree.focused = 1;
}

void flux_filepicker_free(FluxFilePicker *p) {
    if (!p) return;
    flux_dirtree_free(&p->tree);
}

static int fb8_ext_match(const char *exts_csv, const char *fname) {
    if (!exts_csv || exts_csv[0] == 0) return 1;
    const char *dot = strrchr(fname, '.');
    if (!dot) return 0;
    /* iterate comma-separated list */
    const char *p = exts_csv;
    while (*p) {
        const char *e = p;
        while (*e && *e != ',') e++;
        int len = (int)(e - p);
        /* compare with dot..len */
        int dn = (int)strlen(dot);
        if (len == dn) {
            int i, ok = 1;
            for (i = 0; i < len; i++) {
                if (tolower((unsigned char)p[i]) != tolower((unsigned char)dot[i])) { ok = 0; break; }
            }
            if (ok) return 1;
        }
        p = e;
        if (*p == ',') p++;
    }
    return 0;
}

int flux_filepicker_update(FluxFilePicker *p, const FluxEvent *ev) {
    if (!p || !ev) return 0;
    if (!p->visible) return 0;
    if (ev->type == MSG_KEY) {
        if (flux_key_is(*ev, "esc")) {
            p->visible = 0;
            return 1;
        }
        if (flux_key_is(*ev, "tab")) {
            p->right_focus = !p->right_focus;
            p->tree.focused = !p->right_focus;
            return 1;
        }
        if (flux_key_is(*ev, "enter")) {
            const char *sel = flux_dirtree_selected(&p->tree);
            if (p->mode == 0) {
                /* open: must be a file */
                if (sel && p->tree.cursor < p->tree.count &&
                    !p->tree.entries[p->tree.cursor].is_dir) {
                    if (fb8_ext_match(p->exts, p->tree.entries[p->tree.cursor].name)) {
                        snprintf(p->result, sizeof p->result, "%s", sel);
                        p->committed = 1;
                        p->visible = 0;
                        return 1;
                    }
                }
                /* else expand dir */
                if (p->tree.cursor < p->tree.count &&
                    p->tree.entries[p->tree.cursor].is_dir) {
                    return flux_dirtree_update(&p->tree, ev);
                }
            } else {
                /* save: cursor on dir + filename */
                const char *fn = p->filename.buf;
                if (sel && fn && fn[0]) {
                    int is_dir = (p->tree.cursor < p->tree.count &&
                                  p->tree.entries[p->tree.cursor].is_dir);
                    char dir[1024];
                    if (is_dir) {
                        size_t n = strlen(sel);
                        if (n >= sizeof(dir)) n = sizeof(dir) - 1;
                        memcpy(dir, sel, n); dir[n] = 0;
                    } else {
                        size_t n = strlen(sel);
                        if (n >= sizeof(dir)) n = sizeof(dir) - 1;
                        memcpy(dir, sel, n); dir[n] = 0;
                        char *slash = strrchr(dir, '/');
                        if (slash) *slash = 0;
                    }
                    /* compose result safely */
                    size_t dl = strlen(dir);
                    size_t fl = strlen(fn);
                    size_t cap = sizeof p->result;
                    if (dl >= cap) dl = cap - 1;
                    memcpy(p->result, dir, dl);
                    if (dl + 1 < cap) p->result[dl++] = '/';
                    if (dl + fl >= cap) fl = cap - 1 - dl;
                    memcpy(p->result + dl, fn, fl);
                    p->result[dl + fl] = 0;
                    p->committed = 1;
                    p->visible = 0;
                    return 1;
                }
            }
            return 0;
        }
    }
    if (p->right_focus) {
        return flux_input_update(&p->filename, *ev);
    }
    return flux_dirtree_update(&p->tree, ev);
}

void flux_filepicker_render(FluxFilePicker *p, FluxSB *sb, int width, int height) {
    if (!p || !sb || width <= 0 || height <= 0) return;
    if (!p->visible) return;
    int inner_w = width - 2;
    if (inner_w < 8) inner_w = 8;

    /* top border with title */
    const char *title = (p->mode == 0) ? "Open File…" : "Save As…";
    flux_sb_append(sb, FLUX_THEME_BORDER_FG);
    flux_sb_append(sb, "\xe2\x95\xad");
    {
        int label_w = (int)strlen(title) + 2;
        int dashes = inner_w - label_w;
        if (dashes < 0) dashes = 0;
        int half = dashes / 2;
        int k;
        for (k = 0; k < half; k++) flux_sb_append(sb, "\xe2\x94\x80");
        flux_sb_append(sb, " ");
        flux_sb_append(sb, FLUX_THEME_TEXT_FG);
        flux_sb_append(sb, FLUX_BOLD);
        flux_sb_append(sb, title);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
        flux_sb_append(sb, " ");
        for (k = half; k < dashes; k++) flux_sb_append(sb, "\xe2\x94\x80");
    }
    flux_sb_append(sb, "\xe2\x95\xae");
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);

    int body_h = height - 2;
    if (body_h < 1) body_h = 1;
    int left_w = (inner_w * 60) / 100;
    if (left_w < 10) left_w = inner_w / 2;
    int right_w = inner_w - left_w - 1;
    if (right_w < 4) right_w = 4;

    /* render tree to scratch */
    static char tree_scratch[65536];
    static char right_scratch[8192];
    FluxSB ts, rs;
    flux_sb_init(&ts, tree_scratch, sizeof tree_scratch);
    flux_dirtree_render(&p->tree, &ts, left_w, body_h);
    flux_sb_init(&rs, right_scratch, sizeof right_scratch);

    /* right pane content */
    {
        if (p->mode == 1) {
            /* filename input + preview */
            flux_input_render(&p->filename, &rs, right_w, FLUX_THEME_TEXT_FG, FLUX_THEME_BRAND_PURPLE_FG);
            flux_sb_nl(&rs);
        } else {
            /* preview: selected path */
            const char *sel = flux_dirtree_selected(&p->tree);
            char tmp[1024];
            flux_truncate(sel ? sel : "(no selection)", right_w, "…", tmp, sizeof tmp);
            flux_sb_append(&rs, FLUX_THEME_TEXT2_FG);
            flux_sb_append(&rs, tmp);
            flux_sb_append(&rs, FLUX_RESET);
            int tw = flux_strwidth(tmp);
            { int i; for (i = 0; i < right_w - tw; i++) flux_sb_append(&rs, " "); }
            flux_sb_nl(&rs);
        }
        /* fill rest */
        int filled = 1;
        int i;
        for (i = filled; i < body_h; i++) {
            int j; for (j = 0; j < right_w; j++) flux_sb_append(&rs, " ");
            flux_sb_nl(&rs);
        }
    }

    /* iterate body rows: │ tree_row │ right_row │ */
    const char *tp = flux_sb_str(&ts);
    const char *rp = flux_sb_str(&rs);
    int r;
    for (r = 0; r < body_h; r++) {
        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        /* tree row: read until newline */
        const char *line_end = strchr(tp, '\n');
        int len = line_end ? (int)(line_end - tp) : (int)strlen(tp);
        char buf[8192];
        if (len > (int)sizeof(buf) - 1) len = sizeof(buf) - 1;
        memcpy(buf, tp, len); buf[len] = 0;
        flux_sb_append(sb, buf);
        /* compute pad needed to fill left_w */
        int bw = flux_strwidth(buf);
        if (bw < left_w) fb8_pad(sb, left_w - bw);
        if (line_end) tp = line_end + 1; else tp += len;

        /* divider */
        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);

        /* right row */
        const char *rle = strchr(rp, '\n');
        int rlen = rle ? (int)(rle - rp) : (int)strlen(rp);
        if (rlen > (int)sizeof(buf) - 1) rlen = sizeof(buf) - 1;
        memcpy(buf, rp, rlen); buf[rlen] = 0;
        flux_sb_append(sb, buf);
        int rw_w = flux_strwidth(buf);
        if (rw_w < right_w) fb8_pad(sb, right_w - rw_w);
        if (rle) rp = rle + 1; else rp += rlen;

        flux_sb_append(sb, FLUX_THEME_BORDER_FG);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* bottom */
    flux_sb_append(sb, FLUX_THEME_BORDER_FG);
    flux_sb_append(sb, "\xe2\x95\xb0");
    {
        int k;
        for (k = 0; k < inner_w; k++) flux_sb_append(sb, "\xe2\x94\x80");
    }
    flux_sb_append(sb, "\xe2\x95\xaf");
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ============================================================
 * 9. flux_welcome
 * ============================================================ */

void flux_welcome(FluxSB *sb,
                  const char *title,
                  const char *version,
                  const char *model,
                  const FluxWelcomeHint *hints, int n_hints,
                  int width) {
    if (!sb || !title || width <= 0) return;

    /* compose centered rows */
    /* 1: blank */
    fb8_pad(sb, width); flux_sb_nl(sb);

    /* 2: title in box if it fits */
    {
        char inside[64];
        snprintf(inside, sizeof inside, "  %s  ", title);
        int iw = (int)strlen(inside);
        if (iw + 2 <= width) {
            /* top */
            int lp = (width - iw - 2) / 2;
            fb8_pad(sb, lp);
            flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
            flux_sb_append(sb, "\xe2\x95\x94"); /* ╔ */
            int k;
            for (k = 0; k < iw; k++) flux_sb_append(sb, "\xe2\x95\x90"); /* ═ */
            flux_sb_append(sb, "\xe2\x95\x97"); /* ╗ */
            flux_sb_append(sb, FLUX_RESET);
            fb8_pad(sb, width - lp - iw - 2);
            flux_sb_nl(sb);

            fb8_pad(sb, lp);
            flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
            flux_sb_append(sb, "\xe2\x95\x91");
            flux_sb_append(sb, FLUX_BOLD);
            flux_sb_append(sb, FLUX_THEME_TEXT_FG);
            flux_sb_append(sb, inside);
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
            flux_sb_append(sb, "\xe2\x95\x91");
            flux_sb_append(sb, FLUX_RESET);
            fb8_pad(sb, width - lp - iw - 2);
            flux_sb_nl(sb);

            fb8_pad(sb, lp);
            flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
            flux_sb_append(sb, "\xe2\x95\x9a"); /* ╚ */
            for (k = 0; k < iw; k++) flux_sb_append(sb, "\xe2\x95\x90");
            flux_sb_append(sb, "\xe2\x95\x9d"); /* ╝ */
            flux_sb_append(sb, FLUX_RESET);
            fb8_pad(sb, width - lp - iw - 2);
            flux_sb_nl(sb);
        } else {
            int tw = flux_strwidth(title);
            int lp = (width - tw) / 2;
            if (lp < 0) lp = 0;
            fb8_pad(sb, lp);
            flux_sb_append(sb, FLUX_BOLD);
            flux_sb_append(sb, title);
            flux_sb_append(sb, FLUX_RESET);
            fb8_pad(sb, width - lp - tw);
            flux_sb_nl(sb);
        }
    }

    /* version */
    if (version && version[0]) {
        int tw = flux_strwidth(version);
        int lp = (width - tw) / 2;
        if (lp < 0) lp = 0;
        fb8_pad(sb, lp);
        flux_sb_append(sb, FLUX_DIM);
        flux_sb_append(sb, version);
        flux_sb_append(sb, FLUX_RESET);
        fb8_pad(sb, width - lp - tw);
        flux_sb_nl(sb);
    }

    /* model */
    if (model && model[0]) {
        char buf[64];
        snprintf(buf, sizeof buf, "model: %s", model);
        int tw = flux_strwidth(buf);
        int lp = (width - tw) / 2;
        if (lp < 0) lp = 0;
        fb8_pad(sb, lp);
        flux_sb_append(sb, FLUX_DIM);
        flux_sb_append(sb, FLUX_ITALIC);
        flux_sb_append(sb, buf);
        flux_sb_append(sb, FLUX_RESET);
        fb8_pad(sb, width - lp - tw);
        flux_sb_nl(sb);
    }

    /* hints */
    if (hints && n_hints > 0) {
        /* divider */
        {
            int dw = width / 2;
            int lp = (width - dw) / 2;
            fb8_pad(sb, lp);
            flux_sb_append(sb, FLUX_DIM);
            int k;
            for (k = 0; k < dw; k++) flux_sb_append(sb, "\xe2\x94\x80");
            flux_sb_append(sb, FLUX_RESET);
            fb8_pad(sb, width - lp - dw);
            flux_sb_nl(sb);
        }
        /* compute longest key */
        int max_kw = 0;
        int i;
        for (i = 0; i < n_hints; i++) {
            int w = (int)(hints[i].key ? strlen(hints[i].key) : 0);
            if (w > max_kw) max_kw = w;
        }
        int max_lw = 0;
        for (i = 0; i < n_hints; i++) {
            int w = (int)(hints[i].label ? strlen(hints[i].label) : 0);
            if (w > max_lw) max_lw = w;
        }
        int row_w = max_kw + 3 + max_lw;
        if (row_w > width) row_w = width;
        int lp = (width - row_w) / 2;
        if (lp < 0) lp = 0;
        for (i = 0; i < n_hints; i++) {
            char row[256];
            const char *key = hints[i].key ? hints[i].key : "";
            const char *lbl = hints[i].label ? hints[i].label : "";
            snprintf(row, sizeof row, "%*s   %s", max_kw, key, lbl);
            char tmp[256]; flux_truncate(row, row_w, "…", tmp, sizeof tmp);
            fb8_pad(sb, lp);
            flux_sb_append(sb, FLUX_THEME_TEXT2_FG);
            flux_sb_append(sb, tmp);
            flux_sb_append(sb, FLUX_RESET);
            int tw = flux_strwidth(tmp);
            fb8_pad(sb, width - lp - tw);
            flux_sb_nl(sb);
        }
        /* divider */
        {
            int dw = width / 2;
            int dlp = (width - dw) / 2;
            fb8_pad(sb, dlp);
            flux_sb_append(sb, FLUX_DIM);
            int k;
            for (k = 0; k < dw; k++) flux_sb_append(sb, "\xe2\x94\x80");
            flux_sb_append(sb, FLUX_RESET);
            fb8_pad(sb, width - dlp - dw);
            flux_sb_nl(sb);
        }
    }

    /* footer */
    {
        const char *foot = "Press any key to continue";
        int tw = flux_strwidth(foot);
        int lp = (width - tw) / 2;
        if (lp < 0) lp = 0;
        fb8_pad(sb, lp);
        flux_sb_append(sb, FLUX_DIM);
        flux_sb_append(sb, foot);
        flux_sb_append(sb, FLUX_RESET);
        fb8_pad(sb, width - lp - tw);
        flux_sb_nl(sb);
    }
}

/* ============================================================
 * 10. flux_loading
 * ============================================================ */

void flux_loading(FluxSB *sb, const char *label, int frame, int width) {
    if (!sb || width <= 0) return;
    static const char *frames[] = {
        "\xe2\xa0\x8b","\xe2\xa0\x99","\xe2\xa0\xb9","\xe2\xa0\xb8",
        "\xe2\xa0\xbc","\xe2\xa0\xb4","\xe2\xa0\xa6","\xe2\xa0\xa7",
        "\xe2\xa0\x87","\xe2\xa0\x8f"
    };
    int idx = (frame % 10 + 10) % 10;
    flux_sb_append(sb, FLUX_THEME_BRAND_PURPLE_FG);
    flux_sb_append(sb, frames[idx]);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_append(sb, " ");
    int used = 2;
    if (label) {
        int avail = width - used;
        if (avail < 1) avail = 1;
        char tmp[1024];
        flux_truncate(label, avail, "…", tmp, sizeof tmp);
        flux_sb_append(sb, FLUX_THEME_TEXT_FG);
        flux_sb_append(sb, tmp);
        flux_sb_append(sb, FLUX_RESET);
        used += flux_strwidth(tmp);
    }
    if (used < width) fb8_pad(sb, width - used);
    flux_sb_nl(sb);
}

/* ============================================================
 * 11. flux_inline_diff
 * ============================================================ */

/* Coarse common-prefix/suffix diff. Emits one row of width cells. */
static void fb8_inline_diff_coarse(FluxSB *sb, const char *a, const char *b, int width) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    int p = 0;
    while (p < la && p < lb && a[p] == b[p]) p++;
    int sa = la, sb_ = lb;
    while (sa > p && sb_ > p && a[sa - 1] == b[sb_ - 1]) { sa--; sb_--; }
    /* prefix common, then [p..sa) removed, then [p..sb_) added, then suffix common */
    int used = 0;
    char tmp[2048];

    /* prefix */
    int pl = p;
    if (pl > 0 && used < width) {
        if (pl > (int)sizeof(tmp) - 1) pl = sizeof(tmp) - 1;
        memcpy(tmp, a, pl); tmp[pl] = 0;
        char trunc[2048];
        flux_truncate(tmp, width - used, "…", trunc, sizeof trunc);
        flux_sb_append(sb, FLUX_THEME_TEXT_FG);
        flux_sb_append(sb, trunc);
        flux_sb_append(sb, FLUX_RESET);
        used += flux_strwidth(trunc);
    }
    /* removed */
    if (sa - p > 0 && used < width) {
        int rl = sa - p;
        if (rl > (int)sizeof(tmp) - 1) rl = sizeof(tmp) - 1;
        memcpy(tmp, a + p, rl); tmp[rl] = 0;
        char trunc[2048];
        flux_truncate(tmp, width - used, "…", trunc, sizeof trunc);
        flux_sb_append(sb, FLUX_STRIKE);
        flux_sb_append(sb, FLUX_DIM);
        flux_sb_append(sb, FLUX_THEME_DIFF_DEL_FG);
        flux_sb_append(sb, trunc);
        flux_sb_append(sb, FLUX_RESET);
        used += flux_strwidth(trunc);
    }
    /* added */
    if (sb_ - p > 0 && used < width) {
        int al = sb_ - p;
        if (al > (int)sizeof(tmp) - 1) al = sizeof(tmp) - 1;
        memcpy(tmp, b + p, al); tmp[al] = 0;
        char trunc[2048];
        flux_truncate(tmp, width - used, "…", trunc, sizeof trunc);
        flux_sb_append(sb, FLUX_BOLD);
        flux_sb_append(sb, FLUX_THEME_DIFF_ADD_FG);
        flux_sb_append(sb, trunc);
        flux_sb_append(sb, FLUX_RESET);
        used += flux_strwidth(trunc);
    }
    /* suffix */
    int sl = la - sa;
    if (sl > 0 && used < width) {
        if (sl > (int)sizeof(tmp) - 1) sl = sizeof(tmp) - 1;
        memcpy(tmp, a + sa, sl); tmp[sl] = 0;
        char trunc[2048];
        flux_truncate(tmp, width - used, "…", trunc, sizeof trunc);
        flux_sb_append(sb, FLUX_THEME_TEXT_FG);
        flux_sb_append(sb, trunc);
        flux_sb_append(sb, FLUX_RESET);
        used += flux_strwidth(trunc);
    }
    if (used < width) fb8_pad(sb, width - used);
    flux_sb_nl(sb);
}

void flux_inline_diff(FluxSB *sb,
                      const char *before, const char *after,
                      int width) {
    if (!sb || width <= 0) return;
    if (!before) before = "";
    if (!after)  after = "";
    /* For v1, always use coarse diff (LCS-DP would need more careful
     * stack budget; the spec permits coarse fallback for >256 chars and
     * does not require LCS for shorter inputs). */
    fb8_inline_diff_coarse(sb, before, after, width);
}

/* ============================================================
 * 12. FluxOptionList (and 13. FluxSelectionList alias)
 * ============================================================ */

void flux_optionlist_init(FluxOptionList *l, FluxOption *items, int n) {
    if (!l) return;
    memset(l, 0, sizeof *l);
    l->items = items;
    l->n = n;
    l->indicator = '>';
    /* place cursor on first navigable */
    int i;
    for (i = 0; i < n; i++) {
        if (!items[i].disabled && !items[i].is_separator) {
            l->cursor = i;
            break;
        }
    }
}

static int fb8_opt_navigable(const FluxOption *o) {
    return !(o->disabled || o->is_separator);
}

int flux_optionlist_update(FluxOptionList *l, const FluxEvent *ev) {
    if (!l || !ev || ev->type != MSG_KEY) return 0;
    if (flux_key_is(*ev, "up") || flux_key_is(*ev, "k")) {
        int i = l->cursor - 1;
        while (i >= 0 && !fb8_opt_navigable(&l->items[i])) i--;
        if (i >= 0) l->cursor = i;
        return 1;
    }
    if (flux_key_is(*ev, "down") || flux_key_is(*ev, "j")) {
        int i = l->cursor + 1;
        while (i < l->n && !fb8_opt_navigable(&l->items[i])) i++;
        if (i < l->n) l->cursor = i;
        return 1;
    }
    if (flux_key_is(*ev, "home")) {
        int i = 0;
        while (i < l->n && !fb8_opt_navigable(&l->items[i])) i++;
        if (i < l->n) l->cursor = i;
        return 1;
    }
    if (flux_key_is(*ev, "end")) {
        int i = l->n - 1;
        while (i >= 0 && !fb8_opt_navigable(&l->items[i])) i--;
        if (i >= 0) l->cursor = i;
        return 1;
    }
    if (ev->u.key.key[0] == ' ' && ev->u.key.key[1] == 0) {
        if (l->cursor < l->n && fb8_opt_navigable(&l->items[l->cursor])) {
            l->items[l->cursor].selected = !l->items[l->cursor].selected;
        }
        return 1;
    }
    if (flux_key_is(*ev, "enter")) {
        if (l->cursor < l->n && fb8_opt_navigable(&l->items[l->cursor])) {
            l->items[l->cursor].selected = 1;
        }
        return 1;
    }
    if (ev->u.key.key[0] == 'a' && ev->u.key.key[1] == 0) {
        int i; for (i = 0; i < l->n; i++) if (fb8_opt_navigable(&l->items[i])) l->items[i].selected = 1;
        return 1;
    }
    if (ev->u.key.key[0] == 'n' && ev->u.key.key[1] == 0) {
        int i; for (i = 0; i < l->n; i++) l->items[i].selected = 0;
        return 1;
    }
    return 0;
}

void flux_optionlist_render(const FluxOptionList *l, FluxSB *sb, int width) {
    if (!l || !sb || width <= 0 || !l->items) return;
    int start = 0, end = l->n;
    if (l->max_visible > 0 && l->n > l->max_visible) {
        start = l->cursor - l->max_visible / 2;
        if (start < 0) start = 0;
        end = start + l->max_visible;
        if (end > l->n) { end = l->n; start = end - l->max_visible; if (start < 0) start = 0; }
    }
    int i;
    for (i = start; i < end; i++) {
        const FluxOption *o = &l->items[i];
        if (o->is_separator) {
            int k;
            flux_sb_append(sb, FLUX_DIM);
            flux_sb_append(sb, FLUX_THEME_DIVIDER_FG);
            for (k = 0; k < width; k++) flux_sb_append(sb, "\xe2\x94\x80");
            flux_sb_append(sb, FLUX_RESET);
            flux_sb_nl(sb);
            continue;
        }
        char ind = (i == l->cursor) ? l->indicator : ' ';
        char box[8];
        snprintf(box, sizeof box, "[%c]", o->selected ? 'x' : ' ');
        char prefix[16];
        if (l->show_index) {
            snprintf(prefix, sizeof prefix, "%c %s ", ind, box);
        } else {
            snprintf(prefix, sizeof prefix, "%c %s ", ind, box);
        }
        int prefix_w = (int)strlen(prefix);
        int avail = width - prefix_w;
        if (avail < 1) avail = 1;
        char tmp[1024];
        flux_truncate(o->label ? o->label : "", avail, "…", tmp, sizeof tmp);

        if (i == l->cursor && l->focused) flux_sb_append(sb, FLUX_BOLD FLUX_THEME_BRAND_PURPLE_FG);
        else if (o->disabled)             flux_sb_append(sb, FLUX_DIM FLUX_STRIKE FLUX_THEME_TEXT_DIM_FG);
        else                               flux_sb_append(sb, FLUX_THEME_TEXT_FG);

        flux_sb_append(sb, prefix);
        flux_sb_append(sb, tmp);
        flux_sb_append(sb, FLUX_RESET);
        int tw = flux_strwidth(tmp);
        fb8_pad(sb, width - prefix_w - tw);
        flux_sb_nl(sb);
    }
}

/* ============================================================
 * 14. FluxStopwatch
 * ============================================================ */

void flux_stopwatch_init(FluxStopwatch *s, FluxTimerFmt fmt) {
    if (!s) return;
    memset(s, 0, sizeof *s);
    s->fmt = fmt;
}
void flux_stopwatch_start(FluxStopwatch *s) {
    if (!s || s->running) return;
    s->start_ns = fb8_now_ns();
    s->running = 1;
}
void flux_stopwatch_stop(FluxStopwatch *s) {
    if (!s || !s->running) return;
    s->accum_ns += fb8_now_ns() - s->start_ns;
    s->running = 0;
}
void flux_stopwatch_reset(FluxStopwatch *s) {
    if (!s) return;
    s->accum_ns = 0;
    s->start_ns = s->running ? fb8_now_ns() : 0;
}
void flux_stopwatch_tick(FluxStopwatch *s) { (void)s; }
uint64_t flux_stopwatch_ms(const FluxStopwatch *s) {
    if (!s) return 0;
    uint64_t total = s->accum_ns;
    if (s->running) total += fb8_now_ns() - s->start_ns;
    return total / 1000000ull;
}

static void fb8_fmt_time(uint64_t ms, FluxTimerFmt fmt, char *out, int cap) {
    uint64_t total_ms = ms;
    uint64_t s   = total_ms / 1000;
    uint64_t mm  = s / 60;
    uint64_t ss  = s % 60;
    uint64_t hh  = mm / 60;
    mm = mm % 60;
    uint64_t mmm = total_ms % 1000;
    uint64_t cs  = (total_ms / 10) % 100;
    switch (fmt) {
    case FLUX_TIMER_FMT_MM_SS:
        snprintf(out, cap, "%02llu:%02llu", (unsigned long long)mm, (unsigned long long)ss);
        break;
    case FLUX_TIMER_FMT_HH_MM_SS:
        snprintf(out, cap, "%02llu:%02llu:%02llu",
                 (unsigned long long)hh, (unsigned long long)mm, (unsigned long long)ss);
        break;
    case FLUX_TIMER_FMT_SS_CS:
        snprintf(out, cap, "%02llu.%02llu",
                 (unsigned long long)s, (unsigned long long)cs);
        break;
    case FLUX_TIMER_FMT_MM_SS_MMM:
    default:
        snprintf(out, cap, "%02llu:%02llu.%03llu",
                 (unsigned long long)mm, (unsigned long long)ss, (unsigned long long)mmm);
        break;
    }
}

void flux_stopwatch_render(const FluxStopwatch *s, FluxSB *sb, int width) {
    if (!s || !sb || width <= 0) return;
    char buf[32];
    fb8_fmt_time(flux_stopwatch_ms(s), s->fmt, buf, sizeof buf);
    flux_sb_append(sb, FLUX_BOLD);
    flux_sb_append(sb, FLUX_THEME_TEXT_FG);
    char tmp[32]; flux_truncate(buf, width, "…", tmp, sizeof tmp);
    flux_sb_append(sb, tmp);
    flux_sb_append(sb, FLUX_RESET);
    int tw = flux_strwidth(tmp);
    fb8_pad(sb, width - tw);
    flux_sb_nl(sb);
}

/* ============================================================
 * 15. FluxTimer
 * ============================================================ */

void flux_timer_init(FluxTimer *t, uint64_t duration_ms, FluxTimerFmt fmt) {
    if (!t) return;
    memset(t, 0, sizeof *t);
    t->duration_ns = duration_ms * 1000000ull;
    t->fmt = fmt;
}
void flux_timer_start(FluxTimer *t) {
    if (!t || t->running) return;
    t->start_ns = fb8_now_ns();
    t->running = 1;
    t->finished = 0;
}
void flux_timer_stop(FluxTimer *t) {
    if (!t) return;
    t->running = 0;
}
void flux_timer_reset(FluxTimer *t) {
    if (!t) return;
    t->running = 0;
    t->finished = 0;
    t->start_ns = 0;
}
uint64_t flux_timer_remaining_ms(const FluxTimer *t) {
    if (!t) return 0;
    if (!t->running) {
        if (t->finished) return 0;
        return t->duration_ns / 1000000ull;
    }
    uint64_t now = fb8_now_ns();
    uint64_t elapsed = now - t->start_ns;
    if (elapsed >= t->duration_ns) return 0;
    return (t->duration_ns - elapsed) / 1000000ull;
}
int flux_timer_tick(FluxTimer *t) {
    if (!t) return 0;
    if (!t->running) return 0;
    if (t->finished) return 0;
    if (flux_timer_remaining_ms(t) == 0) {
        t->finished = 1;
        t->running = 0;
        return 1;
    }
    return 0;
}
void flux_timer_render(const FluxTimer *t, FluxSB *sb, int width) {
    if (!t || !sb || width <= 0) return;
    char tbuf[32];
    fb8_fmt_time(flux_timer_remaining_ms(t), t->fmt, tbuf, sizeof tbuf);
    char line[128];
    if (t->prefix && t->prefix[0]) {
        snprintf(line, sizeof line, "%s %s", t->prefix, tbuf);
    } else {
        snprintf(line, sizeof line, "%s", tbuf);
    }
    const char *clr;
    if (t->finished)                          clr = FLUX_THEME_ERR_FG;
    else if (flux_timer_remaining_ms(t) < 5000) clr = FLUX_THEME_WARN_FG;
    else                                      clr = FLUX_THEME_OK_FG;

    flux_sb_append(sb, FLUX_BOLD);
    flux_sb_append(sb, clr);
    char tmp[128]; flux_truncate(line, width, "…", tmp, sizeof tmp);
    flux_sb_append(sb, tmp);
    flux_sb_append(sb, FLUX_RESET);
    int tw = flux_strwidth(tmp);
    fb8_pad(sb, width - tw);
    flux_sb_nl(sb);
}

/* ══════════════════════════════════════════════════════════════════
 * AI DEMO WIDGETS — implementations (Agent F2)
 * Keep all helpers static so multiple TUs including flux.h do not
 * double-link. Renderers: no malloc, width contract honored.
 * ═════════════════════════════════════════════════════════════════ */

#include <math.h>

/* ── helpers ───────────────────────────────────────────────────── */

static float _fxd_clamp01(float t) {
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

static void _fxd_fg_rgb(FluxSB *sb, FluxRGB c) {
    flux_sb_appendf(sb, "\x1b[38;2;%u;%u;%um",
                    (unsigned)c.r, (unsigned)c.g, (unsigned)c.b);
}

static void _fxd_bg_rgb(FluxSB *sb, FluxRGB c) {
    flux_sb_appendf(sb, "\x1b[48;2;%u;%u;%um",
                    (unsigned)c.r, (unsigned)c.g, (unsigned)c.b);
}

/* ── 1. flux_easing ────────────────────────────────────────────── */

float flux_ease_out_expo(float t) {
    t = _fxd_clamp01(t);
    if (t >= 1.0f) return 1.0f;
    if (t <= 0.0f) return 0.0f;
    return 1.0f - powf(2.0f, -10.0f * t);
}

float flux_ease_in_out_cubic(float t) {
    t = _fxd_clamp01(t);
    if (t < 0.5f) return 4.0f * t * t * t;
    float f = -2.0f * t + 2.0f;
    return 1.0f - (f * f * f) * 0.5f;
}

float flux_ease_out_back(float t) {
    t = _fxd_clamp01(t);
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float m = t - 1.0f;
    return 1.0f + c3 * m * m * m + c1 * m * m;
}

float flux_ease_out_bounce(float t) {
    t = _fxd_clamp01(t);
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        float s = t - 1.5f / d1;
        return n1 * s * s + 0.75f;
    } else if (t < 2.5f / d1) {
        float s = t - 2.25f / d1;
        return n1 * s * s + 0.9375f;
    } else {
        float s = t - 2.625f / d1;
        return n1 * s * s + 0.984375f;
    }
}

float flux_spring(float t, float stiffness, float damping) {
    t = _fxd_clamp01(t);
    if (stiffness <= 0.0f) stiffness = 12.0f;
    if (damping   <= 0.0f) damping   = 4.0f;
    /* Damped sinusoid ending at 1.0 — light spring overshoot. */
    float decay = expf(-damping * t);
    float osc   = cosf(stiffness * t);
    return 1.0f - decay * osc;
}

/* ── 2. FluxChannelBadge ──────────────────────────────────────── */

FluxRGB flux_channel_rgb(FluxChannelId id) {
    switch (id) {
        case FLUX_CH_HITL: { FluxRGB c = {0xbb,0x9a,0xf7}; return c; }
        case FLUX_CH_TG:   { FluxRGB c = {0x7d,0xcf,0xff}; return c; }
        case FLUX_CH_AUTO: { FluxRGB c = {0x9e,0xce,0x6a}; return c; }
        case FLUX_CH_GH:   { FluxRGB c = {0xff,0x9e,0x64}; return c; }
        case FLUX_CH_SL:   { FluxRGB c = {0xf7,0x76,0x8e}; return c; }
        case FLUX_CH_API:  { FluxRGB c = {0xe0,0xaf,0x68}; return c; }
        default:           { FluxRGB c = {0x80,0x80,0x80}; return c; }
    }
}

const char *flux_channel_label(FluxChannelId id) {
    switch (id) {
        case FLUX_CH_HITL: return "HITL";
        case FLUX_CH_TG:   return "TG";
        case FLUX_CH_AUTO: return "AUTO";
        case FLUX_CH_GH:   return "GH";
        case FLUX_CH_SL:   return "SL";
        case FLUX_CH_API:  return "API";
        default:           return "?";
    }
}

static const char *_fxd_status_dot(FluxBadgeStatus s) {
    switch (s) {
        case FLUX_BADGE_OK:   return FLUX_THEME_OK_FG;
        case FLUX_BADGE_RUN:  return FLUX_THEME_BRAND_PURPLE_FG;
        case FLUX_BADGE_WARN: return FLUX_THEME_WARN_FG;
        case FLUX_BADGE_ERR:  return FLUX_THEME_ERR_FG;
        default:              return FLUX_THEME_TEXT_DIM_FG;
    }
}

void flux_channel_badge(FluxSB *sb, FluxChannelId id,
                        FluxBadgeStatus status, int width) {
    if (!sb || width <= 0) return;
    char body[64];
    const char *lbl = flux_channel_label(id);
    const char *dot_clr = _fxd_status_dot(status);
    /* " • HITL " */
    snprintf(body, sizeof body, " %s•\x1b[0m %s ",
             dot_clr, lbl);

    FluxRGB c = flux_channel_rgb(id);
    _fxd_fg_rgb(sb, c);
    flux_sb_append(sb, FLUX_BOLD);
    flux_fit(sb, body, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 3. FluxHttpBadge ─────────────────────────────────────────── */

static const char *_fxd_method_fg(const char *m) {
    if (!m) return FLUX_THEME_TEXT_DIM_FG;
    if (strcmp(m, "GET")    == 0) return "\x1b[38;2;125;207;255m"; /* blue  */
    if (strcmp(m, "POST")   == 0) return FLUX_THEME_OK_FG;         /* green */
    if (strcmp(m, "PUT")    == 0) return FLUX_THEME_WARN_FG;       /* amber */
    if (strcmp(m, "PATCH")  == 0) return FLUX_THEME_WARN_FG;
    if (strcmp(m, "DELETE") == 0) return FLUX_THEME_ERR_FG;        /* red   */
    if (strcmp(m, "HEAD")   == 0) return FLUX_THEME_TEXT_DIM_FG;
    return FLUX_THEME_TEXT_DIM_FG;
}

static const char *_fxd_status_fg(int code) {
    if (code >= 500) return FLUX_THEME_ERR_FG;
    if (code >= 400) return FLUX_THEME_WARN_FG;
    if (code >= 300) return "\x1b[38;2;125;207;255m"; /* cyan */
    if (code >= 200) return FLUX_THEME_OK_FG;
    return FLUX_THEME_TEXT_DIM_FG;
}

void flux_http_badge(FluxSB *sb, const char *method, int status_code,
                     int width) {
    if (!sb || width <= 0) return;
    const char *m = method ? method : "?";
    const char *mfg = _fxd_method_fg(m);
    char body[96];
    if (status_code > 0) {
        const char *sfg = _fxd_status_fg(status_code);
        snprintf(body, sizeof body,
                 " %s%s%s\x1b[0m  %s%d\x1b[0m ",
                 FLUX_BOLD, mfg, m, sfg, status_code);
    } else {
        snprintf(body, sizeof body,
                 " %s%s%s\x1b[0m ",
                 FLUX_BOLD, mfg, m);
    }
    flux_fit(sb, body, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 4. FluxFocusRing ─────────────────────────────────────────── */

void flux_focus_ring(FluxSB *sb, int x, int y, int w, int h,
                     FluxRGB accent, float intensity) {
    if (!sb || w < 2 || h < 2) return;
    intensity = _fxd_clamp01(intensity);
    /* Lerp between dim and full accent. */
    FluxRGB dim = { (unsigned char)(accent.r / 3 + 16),
                    (unsigned char)(accent.g / 3 + 16),
                    (unsigned char)(accent.b / 3 + 16) };
    FluxRGB c = flux_rgb_lerp(dim, accent, intensity);

    /* Move to position helpers — terminals use 1-based coordinates.
     * We emit cursor-move escapes so this can overlay existing frame
     * contents. */
    flux_sb_appendf(sb, "\x1b[%d;%dH", y, x);
    _fxd_fg_rgb(sb, c);
    flux_sb_append(sb, "\xe2\x95\xad"); /* ╭ */
    for (int i = 0; i < w - 2; i++) flux_sb_append(sb, "\xe2\x94\x80");
    flux_sb_append(sb, "\xe2\x95\xae"); /* ╮ */

    for (int row = 1; row < h - 1; row++) {
        flux_sb_appendf(sb, "\x1b[%d;%dH", y + row, x);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_appendf(sb, "\x1b[%d;%dH", y + row, x + w - 1);
        flux_sb_append(sb, "\xe2\x94\x82");
    }

    flux_sb_appendf(sb, "\x1b[%d;%dH", y + h - 1, x);
    flux_sb_append(sb, "\xe2\x95\xb0"); /* ╰ */
    for (int i = 0; i < w - 2; i++) flux_sb_append(sb, "\xe2\x94\x80");
    flux_sb_append(sb, "\xe2\x95\xaf"); /* ╯ */
    flux_sb_append(sb, FLUX_RESET);
}

/* ── 4b. FluxOverlayLayer ─────────────────────────────────────── */

/* Walk one display cell starting at *p (caller's pointer). Skips ANSI
 * escapes (CSI/OSC) into the output cell verbatim. Returns the byte
 * length of the cell (visible char + leading escapes). On failure
 * returns 0. */
static int _fxd_overlay_cell_take(const char *p, char *out, int out_sz) {
    if (!p || !out || out_sz < 2) return 0;
    int o = 0;
    /* Consume any ANSI escapes that prefix the visible char. */
    while (*p == 0x1b && p[1] == '[') {
        int n = 2;
        while (p[n] && p[n] < 0x40) n++;
        if (p[n]) n++;
        if (o + n + 1 >= out_sz) return 0;
        memcpy(out + o, p, (size_t)n);
        o += n; p += n;
    }
    if (!*p || *p == '\n') {
        out[o] = 0;
        return o;
    }
    /* Visible char — UTF-8 multi-byte aware. */
    unsigned char c = (unsigned char)*p;
    int cl = 1;
    if (c >= 0xF0) cl = 4;
    else if (c >= 0xE0) cl = 3;
    else if (c >= 0xC0) cl = 2;
    if (o + cl + 1 >= out_sz) return 0;
    memcpy(out + o, p, (size_t)cl);
    o += cl;
    out[o] = 0;
    return o;
}

void flux_overlay_init(FluxOverlayLayer *o) {
    if (!o) return;
    memset(o, 0, sizeof *o);
    o->prev_cells = (char *)calloc(FLUX_OVERLAY_MAX_CELLS,
                                   FLUX_OVERLAY_BYTES_PER_CELL);
    o->curr_cells = (char *)calloc(FLUX_OVERLAY_MAX_CELLS,
                                   FLUX_OVERLAY_BYTES_PER_CELL);
}

void flux_overlay_free(FluxOverlayLayer *o) {
    if (!o) return;
    free(o->prev_cells); free(o->curr_cells);
    o->prev_cells = o->curr_cells = NULL;
}

void flux_overlay_begin(FluxOverlayLayer *o, int x, int y, int w, int h) {
    if (!o) return;
    o->cx = x; o->cy = y; o->cw = w; o->ch = h;
    o->active = 1;
    o->ncells = (w > 0 && h > 0) ? w * h : 0;
    if (o->ncells > FLUX_OVERLAY_MAX_CELLS) o->ncells = FLUX_OVERLAY_MAX_CELLS;
    /* Initialize all cells to a single space. */
    for (int i = 0; i < o->ncells && o->curr_cells; i++) {
        char *slot = o->curr_cells + i * FLUX_OVERLAY_BYTES_PER_CELL;
        slot[0] = ' '; slot[1] = 0;
    }
    for (int r = 0; r < (h < 64 ? h : 64); r++) o->row_w[r] = 0;
}

void flux_overlay_putln(FluxOverlayLayer *o, int row_idx, const char *line) {
    if (!o || !o->active || !o->curr_cells || row_idx < 0 || row_idx >= o->ch) return;
    if (!line) line = "";
    const char *p = line;
    int col = 0;
    while (col < o->cw) {
        char cellbuf[FLUX_OVERLAY_BYTES_PER_CELL];
        int n = _fxd_overlay_cell_take(p, cellbuf, sizeof cellbuf);
        if (n <= 0 || cellbuf[0] == 0) break;
        char *slot = o->curr_cells +
                     (row_idx * o->cw + col) * FLUX_OVERLAY_BYTES_PER_CELL;
        size_t cl = strlen(cellbuf);
        if (cl >= FLUX_OVERLAY_BYTES_PER_CELL) cl = FLUX_OVERLAY_BYTES_PER_CELL - 1;
        memcpy(slot, cellbuf, cl); slot[cl] = 0;
        p += n;
        col++;
    }
    /* Pad remainder with spaces. */
    while (col < o->cw) {
        char *slot = o->curr_cells +
                     (row_idx * o->cw + col) * FLUX_OVERLAY_BYTES_PER_CELL;
        slot[0] = ' '; slot[1] = 0;
        col++;
    }
    if (row_idx < 64) o->row_w[row_idx] = o->cw;
}

void flux_overlay_emit(FluxOverlayLayer *o, FluxSB *sb) {
    if (!o || !sb) return;
    int hidden = (o->cw <= 0 || o->ch <= 0);
    int rect_changed = (o->cx != o->px || o->cy != o->py ||
                        o->cw != o->pw || o->ch != o->ph);

    /* If layer becomes hidden OR moves, blank the previous rect AND
     * request a force-redraw so the underlying chrome lines repaint
     * (the line-diff renderer would otherwise see no change in the
     * SB and leave the blanked cells visible on screen). */
    if ((hidden || rect_changed) && o->prev_visible &&
        o->pw > 0 && o->ph > 0) {
        for (int r = 0; r < o->ph; r++) {
            int row = o->py + r;
            if (row < 1) continue;
            flux_sb_appendf(sb, "\x1b[%d;%dH", row, o->px > 0 ? o->px : 1);
            flux_sb_append(sb, FLUX_RESET);
            for (int c = 0; c < o->pw; c++) flux_sb_append(sb, " ");
        }
        flux_request_force_redraw();
    }

    if (hidden || !o->curr_cells) {
        o->px = o->py = o->pw = o->ph = 0;
        o->prev_visible = 0;
        o->active = 0;
        return;
    }

    int force = rect_changed || !o->prev_visible;

    for (int r = 0; r < o->ch; r++) {
        int run_start = -1;
        int run_end   = -1;
        for (int c = 0; c < o->cw; c++) {
            int idx = (r * o->cw + c) * FLUX_OVERLAY_BYTES_PER_CELL;
            const char *cur_cell  = o->curr_cells + idx;
            const char *prev_cell = o->prev_cells + idx;
            int diff = force ||
                       strncmp(cur_cell, prev_cell, FLUX_OVERLAY_BYTES_PER_CELL) != 0;
            if (diff) {
                if (run_start < 0) run_start = c;
                run_end = c;
            } else if (run_start >= 0) {
                /* flush this run */
                flux_sb_appendf(sb, "\x1b[%d;%dH", o->cy + r, o->cx + run_start);
                flux_sb_append(sb, FLUX_RESET);
                for (int cc = run_start; cc <= run_end; cc++) {
                    int cidx = (r * o->cw + cc) * FLUX_OVERLAY_BYTES_PER_CELL;
                    flux_sb_append(sb, o->curr_cells + cidx);
                }
                run_start = -1; run_end = -1;
            }
        }
        if (run_start >= 0) {
            flux_sb_appendf(sb, "\x1b[%d;%dH", o->cy + r, o->cx + run_start);
            flux_sb_append(sb, FLUX_RESET);
            for (int cc = run_start; cc <= run_end; cc++) {
                int cidx = (r * o->cw + cc) * FLUX_OVERLAY_BYTES_PER_CELL;
                flux_sb_append(sb, o->curr_cells + cidx);
            }
        }
    }

    /* Promote curr → prev for next frame. */
    int total_bytes = o->ncells * FLUX_OVERLAY_BYTES_PER_CELL;
    if (total_bytes > FLUX_OVERLAY_MAX_CELLS * FLUX_OVERLAY_BYTES_PER_CELL)
        total_bytes = FLUX_OVERLAY_MAX_CELLS * FLUX_OVERLAY_BYTES_PER_CELL;
    memcpy(o->prev_cells, o->curr_cells, (size_t)total_bytes);
    o->px = o->cx; o->py = o->cy; o->pw = o->cw; o->ph = o->ch;
    o->prev_visible = 1;
    o->active = 0;
}

/* ── 4e. FluxComposer ─────────────────────────────────────────── */

static int _fxc_utf8_len(unsigned char b) {
    if (b < 0x80)       return 1;
    else if (b < 0xC0)  return 1;
    else if (b < 0xE0)  return 2;
    else if (b < 0xF0)  return 3;
    else                return 4;
}

void flux_composer_init(FluxComposer *c) {
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->cfg.min_rows             = 1;
    c->cfg.max_rows             = 8;
    c->cfg.history_enabled      = 1;
    c->cfg.paste_collapse       = 1;
    c->cfg.paste_collapse_lines = 3;
    c->cfg.paste_collapse_chars = 200;
    c->cfg.placeholder          = "Type a message…";
    c->hist_browse              = -1;
    c->visible_rows             = 1;
    c->nsegs                    = 0;
}

void flux_composer_configure(FluxComposer *c, FluxComposerCfg cfg) {
    if (!c) return;
    if (cfg.min_rows <= 0) cfg.min_rows = 1;
    if (cfg.max_rows <  cfg.min_rows) cfg.max_rows = cfg.min_rows;
    if (cfg.paste_collapse_lines <= 0) cfg.paste_collapse_lines = 3;
    if (cfg.paste_collapse_chars <= 0) cfg.paste_collapse_chars = 200;
    if (!cfg.placeholder) cfg.placeholder = "Type a message…";
    c->cfg = cfg;
}

void flux_composer_focus(FluxComposer *c, int focused) {
    if (!c) return;
    c->focused = focused ? 1 : 0;
}

void flux_composer_clear(FluxComposer *c) {
    if (!c) return;
    c->text_len = 0;
    c->text[0]  = 0;
    c->nsegs    = 0;
    c->caret_seg = 0;
    c->caret_off = 0;
    for (int i = 0; i < FLUX_COMPOSER_PASTES_MAX; i++) c->pastes[i].used = 0;
    c->hist_browse = -1;
    c->view_top_row = 0;
}

/* Find the active text segment. Append a new TEXT seg if last is PASTE. */
static int _fxc_active_text_seg(FluxComposer *c) {
    if (c->nsegs == 0 ||
        c->segs[c->nsegs - 1].kind != FLUX_COMP_SEG_TEXT) {
        if (c->nsegs >= FLUX_COMPOSER_SEGS_MAX) return -1;
        c->segs[c->nsegs].kind = FLUX_COMP_SEG_TEXT;
        c->segs[c->nsegs].off  = c->text_len;
        c->segs[c->nsegs].len  = 0;
        c->nsegs++;
        c->caret_seg = c->nsegs - 1;
        c->caret_off = 0;
    }
    return c->nsegs - 1;
}

static void _fxc_set_err(FluxComposer *c, FluxComposerErr e, const char *msg) {
    c->last_err = e;
    if (msg) {
        size_t n = strlen(msg);
        if (n >= sizeof c->last_err_msg) n = sizeof c->last_err_msg - 1;
        memcpy(c->last_err_msg, msg, n);
        c->last_err_msg[n] = 0;
    } else c->last_err_msg[0] = 0;
    c->last_err_ms = flux_now_ms();
}

/* Insert byte run at current caret. Returns 1 on full success, 0 if
 * truncated/rejected (sets last_err). */
static int _fxc_insert_bytes(FluxComposer *c, const char *src, int n) {
    if (n <= 0) return 1;
    int requested = n;
    if (c->text_len + n >= FLUX_COMPOSER_TEXT_CAP - 1) {
        n = FLUX_COMPOSER_TEXT_CAP - 1 - c->text_len;
        if (n <= 0) {
            _fxc_set_err(c, FLUX_COMPOSER_ERR_TEXT_FULL,
                         "Input limit reached (8KB)");
            return 0;
        }
    }
    int s = _fxc_active_text_seg(c);
    if (s < 0) {
        _fxc_set_err(c, FLUX_COMPOSER_ERR_SEGS_FULL,
                     "Composer too complex — submit or clear");
        return 0;
    }
    /* If caret isn't on this trailing TEXT seg, append a new one. */
    if (c->caret_seg != s) {
        if (c->nsegs >= FLUX_COMPOSER_SEGS_MAX) {
            _fxc_set_err(c, FLUX_COMPOSER_ERR_SEGS_FULL,
                         "Composer too complex — submit or clear");
            return 0;
        }
        s = c->nsegs;
        c->segs[s].kind = FLUX_COMP_SEG_TEXT;
        c->segs[s].off  = c->text_len;
        c->segs[s].len  = 0;
        c->nsegs++;
        c->caret_seg = s;
        c->caret_off = 0;
    }
    int seg_off = c->segs[s].off;
    int caret_byte = seg_off + c->caret_off;
    /* Shift tail right. */
    if (caret_byte < c->text_len) {
        memmove(c->text + caret_byte + n, c->text + caret_byte,
                (size_t)(c->text_len - caret_byte));
    }
    memcpy(c->text + caret_byte, src, (size_t)n);
    c->text_len += n;
    c->text[c->text_len] = 0;
    c->segs[s].len += n;
    c->caret_off   += n;
    /* Shift offsets of any segs starting after caret. */
    for (int i = 0; i < c->nsegs; i++) {
        if (i == s) continue;
        if (c->segs[i].kind == FLUX_COMP_SEG_TEXT && c->segs[i].off > seg_off)
            c->segs[i].off += n;
    }
    if (n < requested) {
        _fxc_set_err(c, FLUX_COMPOSER_ERR_TEXT_FULL,
                     "Input limit reached — paste truncated");
        return 0;
    }
    return 1;
}

/* Decide paste-collapse for an incoming blob. Returns 1 if collapsed. */
static int _fxc_paste_or_inline(FluxComposer *c, const char *blob, int len) {
    if (len <= 0) return 0;
    int newlines = 0;
    for (int i = 0; i < len; i++) if (blob[i] == '\n') newlines++;
    int as_chip = c->cfg.paste_collapse &&
                  (newlines + 1 >= c->cfg.paste_collapse_lines ||
                   len >= c->cfg.paste_collapse_chars);
    if (!as_chip) {
        _fxc_insert_bytes(c, blob, len);
        return 0;
    }
    /* Find a free paste slot. */
    int pid = -1;
    for (int i = 0; i < FLUX_COMPOSER_PASTES_MAX; i++) {
        if (!c->pastes[i].used) { pid = i; break; }
    }
    if (pid < 0) {
        char m[80];
        snprintf(m, sizeof m, "Too many pastes (max %d) — submit or clear",
                 FLUX_COMPOSER_PASTES_MAX);
        _fxc_set_err(c, FLUX_COMPOSER_ERR_PASTE_SLOTS_FULL, m);
        return 0;
    }
    if (c->nsegs >= FLUX_COMPOSER_SEGS_MAX) {
        _fxc_set_err(c, FLUX_COMPOSER_ERR_SEGS_FULL,
                     "Composer too complex — submit or clear");
        return 0;
    }
    if (len > FLUX_COMPOSER_PASTE_CAP - 1) {
        char m[80];
        snprintf(m, sizeof m, "Paste truncated — exceeds %dKB cap",
                 FLUX_COMPOSER_PASTE_CAP / 1024);
        _fxc_set_err(c, FLUX_COMPOSER_ERR_PASTE_TOO_BIG, m);
        len = FLUX_COMPOSER_PASTE_CAP - 1;
    }
    FluxComposerPaste *p = &c->pastes[pid];
    p->used  = 1;
    p->lines = newlines + 1;
    p->bytes = len;
    memcpy(p->content, blob, (size_t)len);
    p->content[len] = 0;
    /* Insert PASTE seg at caret position (split current TEXT seg). */
    int s = c->caret_seg;
    if (c->nsegs == 0 || s >= c->nsegs) {
        /* fresh — append paste seg */
        c->segs[c->nsegs].kind     = FLUX_COMP_SEG_PASTE;
        c->segs[c->nsegs].paste_id = pid;
        c->nsegs++;
        c->caret_seg = c->nsegs;
        c->caret_off = 0;
        c->paste_seq++;
        return 1;
    }
    if (c->segs[s].kind == FLUX_COMP_SEG_TEXT && c->caret_off > 0 &&
        c->caret_off < c->segs[s].len) {
        /* split TEXT seg — make room for two new segs. */
        if (c->nsegs + 2 > FLUX_COMPOSER_SEGS_MAX) {
            _fxc_set_err(c, FLUX_COMPOSER_ERR_SEGS_FULL,
                         "Composer too complex — submit or clear");
            return 0;
        }
        memmove(&c->segs[s + 3], &c->segs[s + 1],
                (size_t)((c->nsegs - s - 1) * sizeof(c->segs[0])));
        FluxComposerSeg orig = c->segs[s];
        c->segs[s].len  = c->caret_off;
        c->segs[s + 1].kind     = FLUX_COMP_SEG_PASTE;
        c->segs[s + 1].paste_id = pid;
        c->segs[s + 2].kind = FLUX_COMP_SEG_TEXT;
        c->segs[s + 2].off  = orig.off + c->caret_off;
        c->segs[s + 2].len  = orig.len  - c->caret_off;
        c->nsegs += 2;
        c->caret_seg = s + 2;
        c->caret_off = 0;
    } else {
        /* Insert paste before/after current seg, depending on caret. */
        int insert_at;
        if (c->segs[s].kind == FLUX_COMP_SEG_TEXT && c->caret_off >= c->segs[s].len) {
            insert_at = s + 1;
        } else if (c->segs[s].kind == FLUX_COMP_SEG_PASTE && c->caret_off > 0) {
            insert_at = s + 1;
        } else {
            insert_at = s;
        }
        if (c->nsegs >= FLUX_COMPOSER_SEGS_MAX) {
            _fxc_set_err(c, FLUX_COMPOSER_ERR_SEGS_FULL,
                         "Composer too complex — submit or clear");
            return 0;
        }
        if (insert_at < c->nsegs) {
            memmove(&c->segs[insert_at + 1], &c->segs[insert_at],
                    (size_t)((c->nsegs - insert_at) * sizeof(c->segs[0])));
        }
        c->segs[insert_at].kind     = FLUX_COMP_SEG_PASTE;
        c->segs[insert_at].paste_id = pid;
        c->nsegs++;
        c->caret_seg = insert_at + 1;
        c->caret_off = 0;
    }
    c->paste_seq++;
    return 1;
}

/* Delete the byte BEFORE caret. Handles UTF-8 (deletes whole codepoint).
 * If caret is at start of a TEXT seg AND prev seg is PASTE → delete chip. */
static void _fxc_backspace(FluxComposer *c) {
    if (c->nsegs == 0) return;
    if (c->caret_off == 0) {
        if (c->caret_seg == 0) return;
        int prev = c->caret_seg - 1;
        if (c->segs[prev].kind == FLUX_COMP_SEG_PASTE) {
            int pid = c->segs[prev].paste_id;
            if (pid >= 0 && pid < FLUX_COMPOSER_PASTES_MAX)
                c->pastes[pid].used = 0;
            memmove(&c->segs[prev], &c->segs[prev + 1],
                    (size_t)((c->nsegs - prev - 1) * sizeof(c->segs[0])));
            c->nsegs--;
            c->caret_seg = prev;
            c->caret_off = (prev < c->nsegs && c->segs[prev].kind == FLUX_COMP_SEG_TEXT)
                            ? c->segs[prev].len : 0;
            return;
        }
        if (c->segs[prev].kind == FLUX_COMP_SEG_TEXT) {
            c->caret_seg = prev;
            c->caret_off = c->segs[prev].len;
        }
    }
    if (c->caret_off == 0) return;
    int s = c->caret_seg;
    if (c->segs[s].kind != FLUX_COMP_SEG_TEXT) return;
    int seg_off = c->segs[s].off;
    int byte_pos = seg_off + c->caret_off;
    /* walk back over UTF-8 continuation bytes */
    int del = 1;
    while (byte_pos - del > seg_off &&
           (unsigned char)c->text[byte_pos - del] >= 0x80 &&
           (unsigned char)c->text[byte_pos - del] < 0xC0) del++;
    /* shift left */
    memmove(c->text + byte_pos - del, c->text + byte_pos,
            (size_t)(c->text_len - byte_pos));
    c->text_len -= del;
    c->text[c->text_len] = 0;
    c->segs[s].len -= del;
    c->caret_off  -= del;
    /* update later TEXT seg offsets */
    for (int i = 0; i < c->nsegs; i++) {
        if (i == s) continue;
        if (c->segs[i].kind == FLUX_COMP_SEG_TEXT && c->segs[i].off > seg_off)
            c->segs[i].off -= del;
    }
}

/* Build the expanded text into c->last_submit. Returns total length. */
static int _fxc_expand(FluxComposer *c) {
    int o = 0;
    for (int i = 0; i < c->nsegs; i++) {
        if (c->segs[i].kind == FLUX_COMP_SEG_TEXT) {
            int n = c->segs[i].len;
            if (o + n + 1 >= FLUX_COMPOSER_SUBMIT_CAP) n = FLUX_COMPOSER_SUBMIT_CAP - 1 - o;
            memcpy(c->last_submit + o, c->text + c->segs[i].off, (size_t)n);
            o += n;
        } else {
            int pid = c->segs[i].paste_id;
            if (pid >= 0 && pid < FLUX_COMPOSER_PASTES_MAX && c->pastes[pid].used) {
                int n = c->pastes[pid].bytes;
                if (o + n + 1 >= FLUX_COMPOSER_SUBMIT_CAP) n = FLUX_COMPOSER_SUBMIT_CAP - 1 - o;
                memcpy(c->last_submit + o, c->pastes[pid].content, (size_t)n);
                o += n;
            }
        }
    }
    c->last_submit[o] = 0;
    return o;
}

/* Wrap-aware row count. Builds row-end byte offsets in a transient
 * buffer accessible via c->wrap_total_rows + c->caret_row/caret_col. */
static int _fxc_wrap_count_rows(FluxComposer *c, int width) {
    if (width <= 0) { c->wrap_total_rows = 1; c->caret_row = 0; c->caret_col = 0; return 1; }
    int row = 0, col = 0;
    int caret_row = 0, caret_col = 0;
    int caret_seen = 0;
    for (int i = 0; i < c->nsegs; i++) {
        if (i == c->caret_seg && c->caret_off == 0 && !caret_seen) {
            caret_row = row; caret_col = col; caret_seen = 1;
        }
        if (c->segs[i].kind == FLUX_COMP_SEG_TEXT) {
            int seg_off = c->segs[i].off;
            int j = 0;
            while (j < c->segs[i].len) {
                unsigned char b = (unsigned char)c->text[seg_off + j];
                if (b == '\n') {
                    if (i == c->caret_seg && j == c->caret_off && !caret_seen) {
                        caret_row = row; caret_col = col; caret_seen = 1;
                    }
                    row++; col = 0; j++; continue;
                }
                int cl = _fxc_utf8_len(b);
                /* very simple: assume 1-cell per codepoint (no wide-char detect for caret math) */
                if (col >= width) { row++; col = 0; }
                if (i == c->caret_seg && j == c->caret_off && !caret_seen) {
                    caret_row = row; caret_col = col; caret_seen = 1;
                }
                col++;
                j += cl;
            }
            if (i == c->caret_seg && c->caret_off == c->segs[i].len && !caret_seen) {
                caret_row = row; caret_col = col; caret_seen = 1;
            }
        } else {
            int pid = c->segs[i].paste_id;
            int chip_w = 24;
            if (pid >= 0 && pid < FLUX_COMPOSER_PASTES_MAX && c->pastes[pid].used) {
                char tmp[32];
                snprintf(tmp, sizeof tmp, "[Paste #%d \xc2\xb7 %d lines]",
                         pid + 1, c->pastes[pid].lines);
                chip_w = (int)strlen(tmp);
            }
            if (chip_w > width) chip_w = width;
            if (col > 0 && col + chip_w > width) { row++; col = 0; }
            col += chip_w;
        }
    }
    if (!caret_seen) { caret_row = row; caret_col = col; }
    c->wrap_total_rows = row + 1;
    c->caret_row       = caret_row;
    c->caret_col       = caret_col;
    return c->wrap_total_rows;
}

void flux_composer_layout(FluxComposer *c, int width) {
    if (!c) return;
    _fxc_wrap_count_rows(c, width > 2 ? width - 2 : 1);
    int rows = c->wrap_total_rows;
    if (rows < c->cfg.min_rows) rows = c->cfg.min_rows;
    if (rows > c->cfg.max_rows) rows = c->cfg.max_rows;
    c->visible_rows = rows;
    /* Keep caret in view: scroll view if caret fell off bottom/top. */
    if (c->caret_row < c->view_top_row) c->view_top_row = c->caret_row;
    else if (c->caret_row >= c->view_top_row + c->visible_rows)
        c->view_top_row = c->caret_row - c->visible_rows + 1;
    if (c->view_top_row < 0) c->view_top_row = 0;
}

int flux_composer_update(FluxComposer *c, FluxMsg msg) {
    if (!c || !c->focused) return 0;

    if (msg.type == MSG_PASTE) {
        if (msg.u.paste.len > 0) {
            _fxc_paste_or_inline(c, msg.u.paste.text, msg.u.paste.len);
        }
        return 1;
    }
    if (msg.type != MSG_KEY) return 0;

    /* History scrub — only when caret is in the editing area. */
    if (c->cfg.history_enabled) {
        if (flux_key_is(msg, "up") && c->caret_row == 0) {
            int target = -1;
            if (c->hist_browse < 0 && c->hist_count > 0) {
                target = c->hist_count - 1;
            } else if (c->hist_browse > 0) {
                target = c->hist_browse - 1;
            } else { return 1; }
            if (target < 0 || target >= FLUX_COMPOSER_HIST_MAX) return 1;
            c->hist_browse = target;
            flux_composer_clear(c);
            int hl = (int)strlen(c->history[target]);
            if (hl > FLUX_COMPOSER_TEXT_CAP - 1) hl = FLUX_COMPOSER_TEXT_CAP - 1;
            memcpy(c->text, c->history[target], (size_t)hl);
            c->text[hl] = 0;
            c->text_len = hl;
            c->segs[0].kind = FLUX_COMP_SEG_TEXT;
            c->segs[0].off  = 0;
            c->segs[0].len  = hl;
            c->nsegs        = 1;
            c->caret_seg    = 0;
            c->caret_off    = hl;
            c->hist_browse  = target;
            return 1;
        }
        if (flux_key_is(msg, "down") && c->hist_browse >= 0 &&
            c->caret_row == c->wrap_total_rows - 1) {
            if (c->hist_browse + 1 >= c->hist_count) {
                flux_composer_clear(c);
            } else {
                int target = c->hist_browse + 1;
                if (target >= FLUX_COMPOSER_HIST_MAX) return 1;
                c->hist_browse = target;
                flux_composer_clear(c);
                int hl = (int)strlen(c->history[target]);
                if (hl > FLUX_COMPOSER_TEXT_CAP - 1) hl = FLUX_COMPOSER_TEXT_CAP - 1;
                memcpy(c->text, c->history[target], (size_t)hl);
                c->text[hl] = 0;
                c->text_len = hl;
                c->segs[0].kind = FLUX_COMP_SEG_TEXT;
                c->segs[0].off  = 0;
                c->segs[0].len  = hl;
                c->nsegs        = 1;
                c->caret_seg    = 0;
                c->caret_off    = hl;
            }
            return 1;
        }
    }
    /* Down on the last wrapped row WITHOUT history-mode = signal
     * the host to descend focus elsewhere (e.g. status bar). */
    if (flux_key_is(msg, "down") &&
        c->caret_row == c->wrap_total_rows - 1) {
        c->descend_pending = 1;
        return 1;
    }

    if (flux_key_is(msg, "enter")) {
        if (c->nsegs == 0 && c->text_len == 0) return 1;
        int n = _fxc_expand(c);
        c->submitted = 1;
        if (c->cfg.history_enabled && n > 0 &&
            c->hist_count < FLUX_COMPOSER_HIST_MAX) {
            int copy = n < FLUX_COMPOSER_TEXT_CAP - 1 ? n : FLUX_COMPOSER_TEXT_CAP - 1;
            memcpy(c->history[c->hist_count], c->last_submit, (size_t)copy);
            c->history[c->hist_count][copy] = 0;
            c->hist_count++;
        }
        flux_composer_clear(c);
        return 1;
    }
    if (flux_key_is(msg, "backspace")) { _fxc_backspace(c); return 1; }
    if (flux_key_is(msg, "left")) {
        if (c->caret_off > 0) {
            int del = 1;
            int seg_off = c->segs[c->caret_seg].off;
            while (c->caret_off - del > 0 &&
                   (unsigned char)c->text[seg_off + c->caret_off - del] >= 0x80 &&
                   (unsigned char)c->text[seg_off + c->caret_off - del] < 0xC0) del++;
            c->caret_off -= del;
        } else if (c->caret_seg > 0) {
            c->caret_seg--;
            c->caret_off = c->segs[c->caret_seg].kind == FLUX_COMP_SEG_TEXT
                            ? c->segs[c->caret_seg].len : 0;
        }
        return 1;
    }
    if (flux_key_is(msg, "right")) {
        if (c->caret_seg < c->nsegs) {
            if (c->segs[c->caret_seg].kind == FLUX_COMP_SEG_TEXT &&
                c->caret_off < c->segs[c->caret_seg].len) {
                int seg_off = c->segs[c->caret_seg].off;
                unsigned char b = (unsigned char)c->text[seg_off + c->caret_off];
                int cl = _fxc_utf8_len(b);
                c->caret_off += cl;
            } else {
                c->caret_seg++;
                c->caret_off = 0;
            }
        }
        return 1;
    }
    if (flux_key_is(msg, "home")) { c->caret_seg = 0; c->caret_off = 0; return 1; }
    if (flux_key_is(msg, "end"))  {
        c->caret_seg = c->nsegs;
        c->caret_off = 0;
        if (c->nsegs > 0 && c->segs[c->nsegs - 1].kind == FLUX_COMP_SEG_TEXT) {
            c->caret_seg = c->nsegs - 1;
            c->caret_off = c->segs[c->nsegs - 1].len;
        }
        return 1;
    }

    /* Printable insertion — UTF-8 aware. flux_key_is("char") fails;
     * we use the raw key bytes from the MSG_KEY struct. */
    if (msg.u.key.rune >= 0x20 || msg.u.key.rune == '\t' ||
        (unsigned char)msg.u.key.key[0] >= 0x20) {
        const char *k = msg.u.key.key;
        int kl = (int)strlen(k);
        if (kl > 0 && kl < 8) _fxc_insert_bytes(c, k, kl);
        return 1;
    }
    return 0;
}

/* Render the composer rows into sb. Always emits exactly visible_rows
 * lines of width cells. */
void flux_composer_render(FluxComposer *c, FluxSB *sb, int width) {
    if (!c || !sb || width <= 2) return;
    int inner_w = width - 2;
    int rows_to_draw = c->visible_rows;
    if (rows_to_draw <= 0) rows_to_draw = 1;

    /* Build a per-cell grid (max FLUX_COMPOSER_WRAP_ROWS_MAX rows ×
     * inner_w columns) so we can splice in a reverse-video cursor at
     * the exact caret cell. Each cell holds raw bytes (utf-8 char or
     * a chip styling sequence + glyph) so render is byte-faithful. */
    typedef struct { char bytes[16]; int wide; } _FxcCell;
    static _FxcCell grid[FLUX_COMPOSER_WRAP_ROWS_MAX][512];
    int nrows = 0;
    int  col = 0;
    int  caret_row = 0, caret_col = 0;
    int  caret_seen = 0;

    int empty = (c->text_len == 0 && c->nsegs == 0);

    /* Initialize first row to spaces */
    for (int j = 0; j < inner_w && j < 512; j++) {
        grid[0][j].bytes[0] = ' '; grid[0][j].bytes[1] = 0; grid[0][j].wide = 0;
    }

    if (!empty) {
        for (int i = 0; i < c->nsegs && nrows < FLUX_COMPOSER_WRAP_ROWS_MAX; i++) {
            /* Mark caret position when at start of this seg */
            if (i == c->caret_seg && c->caret_off == 0 && !caret_seen) {
                caret_row = nrows; caret_col = col; caret_seen = 1;
            }
            if (c->segs[i].kind == FLUX_COMP_SEG_TEXT) {
                int seg_off = c->segs[i].off;
                int j = 0;
                while (j < c->segs[i].len && nrows < FLUX_COMPOSER_WRAP_ROWS_MAX) {
                    if (i == c->caret_seg && j == c->caret_off && !caret_seen) {
                        caret_row = nrows; caret_col = col; caret_seen = 1;
                    }
                    unsigned char b = (unsigned char)c->text[seg_off + j];
                    if (b == '\n') {
                        nrows++; col = 0;
                        if (nrows < FLUX_COMPOSER_WRAP_ROWS_MAX)
                            for (int k = 0; k < inner_w && k < 512; k++) {
                                grid[nrows][k].bytes[0] = ' ';
                                grid[nrows][k].bytes[1] = 0;
                                grid[nrows][k].wide = 0;
                            }
                        j++; continue;
                    }
                    int cl = _fxc_utf8_len(b);
                    if (col >= inner_w) {
                        nrows++; col = 0;
                        if (nrows >= FLUX_COMPOSER_WRAP_ROWS_MAX) break;
                        for (int k = 0; k < inner_w && k < 512; k++) {
                            grid[nrows][k].bytes[0] = ' ';
                            grid[nrows][k].bytes[1] = 0;
                            grid[nrows][k].wide = 0;
                        }
                    }
                    if (col < 512) {
                        memcpy(grid[nrows][col].bytes, c->text + seg_off + j,
                               (size_t)(cl < 15 ? cl : 15));
                        grid[nrows][col].bytes[cl < 15 ? cl : 15] = 0;
                        grid[nrows][col].wide = 0;
                    }
                    col++;
                    j += cl;
                }
                if (i == c->caret_seg && c->caret_off == c->segs[i].len && !caret_seen) {
                    caret_row = nrows; caret_col = col; caret_seen = 1;
                }
            } else {
                int pid = c->segs[i].paste_id;
                char chip[48];
                if (pid >= 0 && pid < FLUX_COMPOSER_PASTES_MAX && c->pastes[pid].used) {
                    snprintf(chip, sizeof chip, "[Paste #%d \xc2\xb7 %d lines]",
                             pid + 1, c->pastes[pid].lines);
                } else {
                    snprintf(chip, sizeof chip, "[Paste]");
                }
                int chip_w = (int)strlen(chip);
                if (chip_w > inner_w) chip_w = inner_w;
                if (col + chip_w > inner_w) {
                    nrows++; col = 0;
                    if (nrows >= FLUX_COMPOSER_WRAP_ROWS_MAX) break;
                    for (int k = 0; k < inner_w && k < 512; k++) {
                        grid[nrows][k].bytes[0] = ' ';
                        grid[nrows][k].bytes[1] = 0;
                        grid[nrows][k].wide = 0;
                    }
                }
                /* Write chip char-by-char, marking first cell as wide
                 * (reverse-video styling around it). */
                for (int p = 0; p < chip_w && col < inner_w && col < 512; p++) {
                    grid[nrows][col].bytes[0] = chip[p];
                    grid[nrows][col].bytes[1] = 0;
                    grid[nrows][col].wide = (p == 0) ? 1 : (p == chip_w - 1 ? 2 : 3);
                    col++;
                }
            }
        }
    }
    nrows++;
    if (!caret_seen) { caret_row = nrows - 1; caret_col = col; }

    /* Show cursor: if focused (always reserve a cell at caret_col).
     * If empty and focused, caret at column 0. */
    if (empty && c->focused) { caret_row = 0; caret_col = 0; }

    /* Apply max_rows clamp visually (already in c->visible_rows). */
    int top = c->view_top_row;
    if (top < 0) top = 0;
    int max_top = nrows - rows_to_draw; if (max_top < 0) max_top = 0;
    if (top > max_top) top = max_top;

    for (int r = 0; r < rows_to_draw; r++) {
        char line_buf[2048]; FluxSB l; flux_sb_init(&l, line_buf, sizeof line_buf);
        flux_sb_append(&l, c->focused ? FLUX_THEME_ACCENT_FG : FLUX_THEME_TEXT_OFF_FG);
        if (r == 0) flux_sb_append(&l, c->focused ? "\xe2\x96\xb6 " : "  ");
        else        flux_sb_append(&l, "  ");
        flux_sb_append(&l, FLUX_RESET);

        int gr = top + r;
        if (gr >= nrows || (empty && gr == 0)) {
            if (empty && gr == 0) {
                /* placeholder w/ optional cursor at col 0 */
                if (c->focused) {
                    flux_sb_append(&l, "\x1b[7m \x1b[0m");
                    flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
                    if (c->cfg.placeholder)
                        flux_sb_append(&l, c->cfg.placeholder);
                    flux_sb_append(&l, FLUX_RESET);
                } else {
                    flux_sb_append(&l, FLUX_THEME_TEXT_DIM_FG);
                    if (c->cfg.placeholder)
                        flux_sb_append(&l, c->cfg.placeholder);
                    flux_sb_append(&l, FLUX_RESET);
                }
            }
        } else {
            /* Emit each cell. If this is the caret row and column,
             * wrap the cell in reverse video — visible cursor. */
            int in_chip = 0;
            for (int j = 0; j < inner_w; j++) {
                int is_caret = c->focused && gr == caret_row && j == caret_col;
                if (grid[gr][j].wide == 1) {
                    /* chip start — open reverse video */
                    flux_sb_append(&l, "\x1b[7m");
                    in_chip = 1;
                }
                if (is_caret && !in_chip) {
                    flux_sb_append(&l, "\x1b[7m");
                }
                flux_sb_append(&l, grid[gr][j].bytes);
                if (is_caret && !in_chip) {
                    flux_sb_append(&l, "\x1b[27m");
                }
                if (grid[gr][j].wide == 2) {
                    flux_sb_append(&l, "\x1b[27m");
                    in_chip = 0;
                }
            }
            if (in_chip) flux_sb_append(&l, "\x1b[27m");
            /* Caret beyond last filled column → draw at column `col`. */
            if (c->focused && gr == caret_row && caret_col >= col && r == 0) {
                /* nothing — caret is within grid loop */
            }
        }

        /* Status hint on the LAST visible row when ≥2 rows + content. */
        if (r == rows_to_draw - 1 && rows_to_draw >= 2 && !empty) {
            int used = flux_composer_used_bytes(c);
            int cap  = FLUX_COMPOSER_TEXT_CAP;
            int pct  = cap > 0 ? (int)((100ll * used) / cap) : 0;
            char status[256];
            const char *err_msg = NULL;
            FluxComposerErr e = flux_composer_last_err(c, &err_msg);
            if (e != FLUX_COMPOSER_OK && err_msg) {
                snprintf(status, sizeof status, "  ⚠ %s", err_msg);
            } else if (nrows > rows_to_draw) {
                snprintf(status, sizeof status, "  · %d rows, %d shown · %d%% full",
                         nrows, rows_to_draw, pct);
            } else if (pct >= 70) {
                snprintf(status, sizeof status, "  · %d%% full", pct);
            } else status[0] = 0;
            if (status[0]) {
                flux_sb_append(&l, FLUX_THEME_TEXT_OFF_FG);
                flux_sb_append(&l, status);
                flux_sb_append(&l, FLUX_RESET);
            }
        }

        flux_fit(sb, flux_sb_str(&l), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, "\n");
    }
    (void)caret_seen;
}

const char *flux_composer_consume(FluxComposer *c) {
    if (!c || !c->submitted) return NULL;
    c->submitted = 0;
    return c->last_submit;
}

FluxComposerErr flux_composer_last_err(FluxComposer *c, const char **out_msg) {
    if (!c) { if (out_msg) *out_msg = NULL; return FLUX_COMPOSER_OK; }
    /* Auto-clear after 2.5s. */
    if (c->last_err != FLUX_COMPOSER_OK &&
        flux_now_ms() - c->last_err_ms > 2500) {
        c->last_err = FLUX_COMPOSER_OK;
        c->last_err_msg[0] = 0;
    }
    if (out_msg) *out_msg = c->last_err_msg[0] ? c->last_err_msg : NULL;
    return c->last_err;
}

int flux_composer_used_bytes(FluxComposer *c) {
    if (!c) return 0;
    int total = c->text_len;
    for (int i = 0; i < FLUX_COMPOSER_PASTES_MAX; i++)
        if (c->pastes[i].used) total += c->pastes[i].bytes;
    return total;
}

int flux_composer_capacity_bytes(FluxComposer *c) {
    (void)c;
    return FLUX_COMPOSER_TEXT_CAP +
           FLUX_COMPOSER_PASTES_MAX * FLUX_COMPOSER_PASTE_CAP;
}

int flux_composer_descend_pending(FluxComposer *c) {
    return c ? c->descend_pending : 0;
}
void flux_composer_clear_descend(FluxComposer *c) {
    if (c) c->descend_pending = 0;
}

/* ── 4f. FluxAppBar ───────────────────────────────────────────── */

void flux_appbar_init(FluxAppBar *b) {
    if (!b) return;
    memset(b, 0, sizeof *b);
    b->focused_idx = -1;
    snprintf(b->separator, sizeof b->separator, " \xc2\xb7 ");
}

static int _fxa_find(FluxAppBar *b, const char *id) {
    if (!id) return -1;
    for (int i = 0; i < b->n_items; i++)
        if (strcmp(b->items[i].id, id) == 0) return i;
    return -1;
}

int flux_appbar_add(FluxAppBar *b, FluxAppBarItem item) {
    if (!b) return -1;
    int existing = _fxa_find(b, item.id);
    if (existing >= 0) { b->items[existing] = item; return existing; }
    if (b->n_items >= FLUX_APPBAR_MAX_ITEMS) return -1;
    b->items[b->n_items] = item;
    return b->n_items++;
}

void flux_appbar_remove(FluxAppBar *b, const char *id) {
    if (!b) return;
    int i = _fxa_find(b, id);
    if (i < 0) return;
    for (int j = i; j < b->n_items - 1; j++) b->items[j] = b->items[j + 1];
    b->n_items--;
    if (b->focused_idx >= b->n_items) b->focused_idx = b->n_items - 1;
}

void flux_appbar_set_focus(FluxAppBar *b, int focused) {
    if (!b) return;
    if (focused) {
        b->has_focus = 1;
        if (b->focused_idx < 0) {
            for (int i = 0; i < b->n_items; i++) {
                if (b->items[i].interactive) { b->focused_idx = i; break; }
            }
        }
    } else {
        b->has_focus = 0;
    }
}

int flux_appbar_has_focus(FluxAppBar *b) { return b ? b->has_focus : 0; }

int flux_appbar_activate(FluxAppBar *b, const char *id) {
    if (!b) return 0;
    int i = _fxa_find(b, id);
    if (i < 0 || !b->items[i].on_activate) return 0;
    b->items[i].on_activate(b->items[i].ctx);
    return 1;
}

static int _fxa_next_interactive(FluxAppBar *b, int from, int dir) {
    int n = b->n_items;
    if (n <= 0) return -1;
    int i = from;
    for (int k = 0; k < n; k++) {
        i = (i + dir + n) % n;
        if (b->items[i].interactive) return i;
    }
    return from;
}

int flux_appbar_update(FluxAppBar *b, FluxMsg msg) {
    if (!b) return 0;
    if (msg.type != MSG_KEY) return 0;
    /* Direct shortcut match — works regardless of focus. */
    for (int i = 0; i < b->n_items; i++) {
        if (b->items[i].shortcut[0] && flux_key_is(msg, b->items[i].shortcut)) {
            if (b->items[i].on_activate)
                b->items[i].on_activate(b->items[i].ctx);
            return 1;
        }
    }
    if (!b->has_focus) return 0;
    if (flux_key_is(msg, "esc") || flux_key_is(msg, "up")) {
        b->has_focus = 0;
        return 1;
    }
    if (flux_key_is(msg, "left") || flux_key_is(msg, "h")) {
        b->focused_idx = _fxa_next_interactive(b, b->focused_idx, -1);
        return 1;
    }
    if (flux_key_is(msg, "right") || flux_key_is(msg, "l") ||
        flux_key_is(msg, "tab")) {
        b->focused_idx = _fxa_next_interactive(b, b->focused_idx, +1);
        return 1;
    }
    if (flux_key_is(msg, "enter") || flux_key_is(msg, " ")) {
        if (b->focused_idx >= 0 && b->focused_idx < b->n_items &&
            b->items[b->focused_idx].on_activate) {
            b->items[b->focused_idx].on_activate(b->items[b->focused_idx].ctx);
        }
        return 1;
    }
    return 1;  /* swallow stray keys while focused */
}

/* Render one item to a local buffer, return display width.
 * Style based on kind, focus, animation. */
static int _fxa_render_item(FluxAppBar *b, int idx, FluxSB *sb,
                            uint64_t now_ms, int focused) {
    FluxAppBarItem *it = &b->items[idx];
    int w = 0;
    /* Pull live values first. */
    char value[64] = {0};
    int  is_on = 0;
    if (it->kind == FLUX_APPBAR_KIND_VALUE || it->kind == FLUX_APPBAR_KIND_BADGE) {
        if (it->value_fn) it->value_fn(it->ctx, value, sizeof value);
    }
    if (it->kind == FLUX_APPBAR_KIND_TOGGLE) {
        if (it->bool_fn) is_on = it->bool_fn(it->ctx);
    }

    /* Compute "shimmer" position for ON toggles — gradient sweep. */
    int shimmer_on = (it->kind == FLUX_APPBAR_KIND_TOGGLE && is_on);

    /* Open style. */
    if (focused) {
        flux_sb_append(sb, "\x1b[7m");      /* reverse video */
        flux_sb_append(sb, FLUX_BOLD);
    } else {
        if (it->fg[0]) flux_sb_append(sb, it->fg);
    }

    /* Compose payload string. */
    char payload[160];
    switch (it->kind) {
    case FLUX_APPBAR_KIND_TOGGLE:
        snprintf(payload, sizeof payload, "%s%s%s %s",
                 it->icon[0] ? it->icon : "", it->icon[0] ? " " : "",
                 it->label, is_on ? "on" : "off");
        break;
    case FLUX_APPBAR_KIND_VALUE:
        if (value[0])
            snprintf(payload, sizeof payload, "%s%s%s %s",
                     it->icon[0] ? it->icon : "", it->icon[0] ? " " : "",
                     value, it->label);
        else
            snprintf(payload, sizeof payload, "%s%s%s",
                     it->icon[0] ? it->icon : "", it->icon[0] ? " " : "",
                     it->label);
        break;
    case FLUX_APPBAR_KIND_BADGE:
        snprintf(payload, sizeof payload, "%s%s",
                 it->icon[0] ? it->icon : "", value[0] ? value : it->label);
        break;
    case FLUX_APPBAR_KIND_HINT:
        snprintf(payload, sizeof payload, "%s%s%s",
                 it->shortcut[0] ? it->shortcut : "", it->shortcut[0] ? " " : "",
                 it->label);
        break;
    default:
        snprintf(payload, sizeof payload, "%s", it->label); break;
    }

    /* For TOGGLE items that are ON, apply OK accent over the chip
     * (overrides anything not already set by focus). */
    if (shimmer_on && !focused) {
        flux_sb_append(sb, FLUX_THEME_OK_FG);
        flux_sb_append(sb, FLUX_BOLD);
    }
    if (it->kind == FLUX_APPBAR_KIND_HINT && !focused) {
        flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
    }

    /* Animated shimmer overlay: bright on alternating cells when
     * toggle is on — subtle but visible. UTF-8 aware: walks one
     * codepoint at a time, never splits a multi-byte char. */
    if (shimmer_on) {
        int phase = (int)((now_ms / 80) % 8);
        int cell  = 0;
        const char *p = payload;
        while (*p) {
            unsigned char b = (unsigned char)*p;
            int cl = (b < 0x80) ? 1 : (b < 0xC0) ? 1 :
                     (b < 0xE0) ? 2 : (b < 0xF0) ? 3 : 4;
            int hot = ((cell - phase + 16) % 8) < 2;
            if (hot) flux_sb_append(sb, FLUX_BOLD);
            char tmp[5] = {0};
            for (int j = 0; j < cl && p[j]; j++) tmp[j] = p[j];
            flux_sb_append(sb, tmp);
            if (hot) flux_sb_append(sb, "\x1b[22m");
            cell++; w++;
            p += cl;
        }
    } else {
        flux_sb_append(sb, payload);
        for (const char *p = payload; *p; p++) if ((*p & 0xc0) != 0x80) w++;
    }

    flux_sb_append(sb, FLUX_RESET);
    return w;
}

void flux_appbar_render(FluxAppBar *b, FluxSB *sb, int width, uint64_t now_ms) {
    if (!b || !sb || width <= 0) return;
    b->last_anim_ms = now_ms;

    /* First pass — measure all items, drop low-priority under pressure. */
    int n = b->n_items;
    int show[FLUX_APPBAR_MAX_ITEMS];
    int item_w[FLUX_APPBAR_MAX_ITEMS];
    char tmp[256];
    for (int i = 0; i < n; i++) {
        show[i] = 1;
        FluxSB t; flux_sb_init(&t, tmp, sizeof tmp);
        item_w[i] = _fxa_render_item(b, i, &t, now_ms, 0);
    }
    int sep_w = (int)strlen(b->separator);
    /* Hide low-priority items until everything fits. */
    while (1) {
        int total = 0, shown = 0;
        for (int i = 0; i < n; i++) if (show[i]) {
            total += item_w[i];
            if (shown > 0) total += sep_w;
            shown++;
        }
        if (total <= width - 2) break;
        int worst = -1, worst_pri = 1<<30;
        for (int i = 0; i < n; i++) if (show[i] && b->items[i].priority < worst_pri) {
            worst = i; worst_pri = b->items[i].priority;
        }
        if (worst < 0) break;
        show[worst] = 0;
    }

    /* Second pass — compose visible items left → right with separators.
     * Track item_x for hit-test. */
    int col = 0;
    int first = 1;
    flux_sb_append(sb, FLUX_THEME_TEXT_DIM_FG);
    flux_sb_append(sb, " ");
    flux_sb_append(sb, FLUX_RESET);
    col += 1;
    for (int i = 0; i < n; i++) {
        if (!show[i]) { b->item_x[i] = -1; b->item_w[i] = 0; continue; }
        if (!first) {
            flux_sb_append(sb, FLUX_THEME_TEXT_OFF_FG);
            flux_sb_append(sb, b->separator);
            flux_sb_append(sb, FLUX_RESET);
            col += sep_w;
        }
        first = 0;
        b->item_x[i] = col;
        int focused = (b->has_focus && b->focused_idx == i);
        int written = _fxa_render_item(b, i, sb, now_ms, focused);
        b->item_w[i] = written;
        col += written;
    }
    /* Pad to width. */
    int pad = width - col;
    for (int j = 0; j < pad; j++) flux_sb_append(sb, " ");
}

/* ── 5. FluxToastCenter ───────────────────────────────────────── */

void flux_toast_center_init(FluxToastCenter *tc) {
    if (!tc) return;
    memset(tc, 0, sizeof *tc);
    flux_overlay_init(&tc->layer);
}

void flux_toast_center_push(FluxToastCenter *tc, FluxToastKind kind,
                            const char *title, const char *body,
                            uint64_t ttl_ms) {
    if (!tc) return;
    /* Evict oldest if full. */
    if (tc->count >= FLUX_TOAST_CENTER_MAX) {
        for (int i = 1; i < tc->count; i++) tc->toasts[i - 1] = tc->toasts[i];
        tc->count--;
    }
    FluxToastItem *t = &tc->toasts[tc->count++];
    memset(t, 0, sizeof *t);
    t->kind   = kind;
    t->ttl_ms = ttl_ms == 0 ? 3000 : ttl_ms;
    t->alive  = 1;
    t->created_ms = flux_now_ms();
    t->now_ms     = t->created_ms;
    if (title) { strncpy(t->title, title, sizeof t->title - 1); t->title[sizeof t->title - 1] = 0; }
    if (body)  { strncpy(t->body,  body,  sizeof t->body  - 1); t->body [sizeof t->body  - 1] = 0; }
}

void flux_toast_center_tick(FluxToastCenter *tc, uint64_t now_ms) {
    if (!tc) return;
    int wr = 0;
    for (int r = 0; r < tc->count; r++) {
        FluxToastItem *t = &tc->toasts[r];
        t->now_ms = now_ms;
        uint64_t age = now_ms - t->created_ms;
        if (age < t->ttl_ms + 400) {  /* +400ms fade tail */
            if (wr != r) tc->toasts[wr] = *t;
            wr++;
        }
    }
    tc->count = wr;
}

static const char *_fxd_toast_fg(FluxToastKind k) {
    switch (k) {
        case FLUX_TOAST_OK:   return FLUX_THEME_OK_FG;
        case FLUX_TOAST_WARN: return FLUX_THEME_WARN_FG;
        case FLUX_TOAST_ERR:  return FLUX_THEME_ERR_FG;
        default:              return FLUX_THEME_TEXT_FG;
    }
}

void flux_toast_center_configure(FluxToastCenter *tc, FluxToastConfig cfg) {
    if (!tc) return;
    tc->cfg = cfg;
}

void flux_toast_center_render(FluxToastCenter *tc, FluxSB *sb,
                              int screen_w, int screen_h) {
    if (!tc || !sb || screen_w <= 0 || screen_h <= 0) return;

    /* No toasts → fully hide the overlay so the line-diff renderer
     * triggers a force-redraw on the disappearance frame (otherwise
     * blanking with spaces would leave gaps in the chrome). */
    if (tc->count == 0) {
        flux_overlay_begin(&tc->layer, 0, 0, 0, 0);
        flux_overlay_emit (&tc->layer, sb);
        return;
    }

    FluxToastConfig *c = &tc->cfg;

    int toast_w;
    if (c->width_cells > 0)        toast_w = c->width_cells;
    else if (c->width_pct > 0.0f)  toast_w = (int)(c->width_pct * (float)screen_w);
    else                           toast_w = 36;

    int avail_w = screen_w - c->safe_left - c->safe_right;
    if (toast_w > avail_w) toast_w = avail_w;
    if (toast_w < 8) {
        flux_overlay_begin(&tc->layer, 0, 0, 0, 0);
        flux_overlay_emit (&tc->layer, sb);
        return;
    }

    int avail_h = screen_h - c->safe_top - c->safe_bottom;
    int max_rows = c->max_stack > 0 ? c->max_stack : FLUX_TOAST_CENTER_MAX;
    if (max_rows > avail_h) max_rows = avail_h;
    if (max_rows < 1) {
        flux_overlay_begin(&tc->layer, 0, 0, 0, 0);
        flux_overlay_emit (&tc->layer, sb);
        return;
    }

    int off_x = c->offset_x_cells + (int)(c->offset_x_pct * (float)screen_w);
    int off_y = c->offset_y_cells + (int)(c->offset_y_pct * (float)screen_h);

    int is_top    = (c->anchor == FLUX_TOAST_POS_TOP_LEFT ||
                     c->anchor == FLUX_TOAST_POS_TOP_RIGHT ||
                     c->anchor == FLUX_TOAST_POS_TOP_CENTER);
    int is_left   = (c->anchor == FLUX_TOAST_POS_BOTTOM_LEFT ||
                     c->anchor == FLUX_TOAST_POS_TOP_LEFT);
    int is_center = (c->anchor == FLUX_TOAST_POS_BOTTOM_CENTER ||
                     c->anchor == FLUX_TOAST_POS_TOP_CENTER);
    int is_custom = (c->anchor == FLUX_TOAST_POS_CUSTOM);

    int x_anchor, y_anchor;
    if (is_custom) {
        x_anchor = c->custom_x > 0 ? c->custom_x : 1;
        y_anchor = c->custom_y > 0 ? c->custom_y : 1;
    } else {
        if (is_left)        x_anchor = c->safe_left + 1 + off_x;
        else if (is_center) x_anchor = c->safe_left + 1 + (avail_w - toast_w) / 2;
        else                x_anchor = screen_w - c->safe_right - toast_w + 1 - off_x;

        if (is_top) y_anchor = c->safe_top + 1 + off_y;
        else        y_anchor = screen_h - c->safe_bottom - max_rows + 1 - off_y;
    }
    if (x_anchor < 1) x_anchor = 1;
    if (x_anchor + toast_w > screen_w + 1) x_anchor = screen_w - toast_w + 1;
    if (y_anchor < 1) y_anchor = 1;
    if (y_anchor + max_rows > screen_h + 1) y_anchor = screen_h - max_rows + 1;

    /* Open the overlay layer at the resolved rect. Top-down stacking
     * inside the layer; for bottom anchors we still write into row 0..N-1
     * but the layer paints them all. Newest toast goes nearest the
     * anchor edge. */
    flux_overlay_begin(&tc->layer, x_anchor, y_anchor, toast_w, max_rows);

    /* Compose each row into the layer. Row 0 is closest to anchor edge:
     * for top anchors that's the top row, for bottom anchors the bottom. */
    int slot = 0;
    char row_buf[1024];
    for (int i = tc->count - 1; i >= 0 && slot < max_rows; i--, slot++) {
        FluxToastItem *t = &tc->toasts[i];
        uint64_t age = t->now_ms - t->created_ms;
        float in_t = age < 250 ? (float)age / 250.0f : 1.0f;
        float sp   = flux_spring(in_t, 10.0f, 5.0f);
        int dx = (int)((1.0f - sp) * 6.0f);
        if (dx < 0) dx = 0;
        const char *fg = _fxd_toast_fg(t->kind);
        char body[256];
        snprintf(body, sizeof body, " %s\xe2\x80\xa2\x1b[0m %s%s%s  %s",
                 fg, FLUX_BOLD, t->title, FLUX_RESET, t->body);
        /* Build a fitted row. */
        FluxSB rb; flux_sb_init(&rb, row_buf, (int)sizeof row_buf);
        if (dx > 0 && !is_left) {
            /* spring-in slide from right (push leading spaces) */
            for (int sp2 = 0; sp2 < dx && sp2 < toast_w; sp2++)
                flux_sb_append(&rb, " ");
            flux_sb_append(&rb, FLUX_THEME_PANEL_BG);
            flux_fit(&rb, body, toast_w - dx, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(&rb, FLUX_RESET);
        } else if (dx > 0) {
            flux_sb_append(&rb, FLUX_THEME_PANEL_BG);
            flux_fit(&rb, body, toast_w - dx, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(&rb, FLUX_RESET);
            for (int sp2 = 0; sp2 < dx && sp2 < toast_w; sp2++)
                flux_sb_append(&rb, " ");
        } else {
            flux_sb_append(&rb, FLUX_THEME_PANEL_BG);
            flux_fit(&rb, body, toast_w, NULL, FLUX_ALIGN_LEFT);
            flux_sb_append(&rb, FLUX_RESET);
        }
        int row_idx = is_top ? slot : (max_rows - 1 - slot);
        flux_overlay_putln(&tc->layer, row_idx, flux_sb_str(&rb));
    }

    flux_overlay_emit(&tc->layer, sb);
}

/* ── 4d. FluxCursorList ───────────────────────────────────────── */

void flux_cursor_list_init(FluxCursorList *cl) {
    if (!cl) return;
    memset(cl, 0, sizeof *cl);
    cl->last_cursor = -1;
}

/* Caller passes initial running row in `start_row` (e.g. 0, or chrome
 * prelude). Per-item heights accumulate from there. */
void flux_cursor_list_begin(FluxCursorList *cl, FluxScroll *scroll,
                            int cursor, int n_items,
                            int viewport_h, int margin) {
    if (!cl) return;
    cl->cursor       = cursor;
    cl->n_items      = n_items;
    cl->viewport_h   = viewport_h;
    cl->margin       = margin;
    cl->scroll       = scroll;
    cl->cursor_top   = -1;
    cl->cursor_bot   = -1;
    cl->running_row  = 0;
    cl->rendering    = 1;
}

void flux_cursor_list_item(FluxCursorList *cl, int idx,
                           int item_height, int *out_row_offset) {
    if (!cl || !cl->rendering) {
        if (out_row_offset) *out_row_offset = 0;
        return;
    }
    if (out_row_offset) *out_row_offset = cl->running_row;
    if (idx == cl->cursor) {
        cl->cursor_top = cl->running_row;
        cl->cursor_bot = cl->running_row + item_height;
    }
    cl->running_row += item_height;
}

void flux_cursor_list_end(FluxCursorList *cl) {
    if (!cl || !cl->rendering) return;
    cl->rendering = 0;
    if (!cl->scroll || cl->n_items <= 0 || cl->viewport_h <= 0) {
        cl->last_cursor = cl->cursor;
        return;
    }
    /* Auto-scroll only when cursor MOVED, AND it falls outside the
     * visible window. If the cursor item is taller than the viewport,
     * prefer top-alignment (show the start of the item). */
    if (cl->cursor != cl->last_cursor && cl->cursor_top >= 0) {
        int margin = cl->margin > 0 ? cl->margin : 2;
        int item_height = cl->cursor_bot - cl->cursor_top;
        if (cl->cursor_top < cl->scroll->scroll + margin) {
            cl->scroll->scroll = cl->cursor_top - margin;
            if (cl->scroll->scroll < 0) cl->scroll->scroll = 0;
        } else if (cl->cursor_bot > cl->scroll->scroll + cl->viewport_h - margin) {
            if (item_height + 2 * margin > cl->viewport_h) {
                /* taller than viewport — show top of item */
                cl->scroll->scroll = cl->cursor_top - margin;
                if (cl->scroll->scroll < 0) cl->scroll->scroll = 0;
            } else {
                cl->scroll->scroll = cl->cursor_bot - cl->viewport_h + margin;
            }
        }
        cl->last_cursor = cl->cursor;
    } else if (cl->last_cursor < 0) {
        /* First render — record cursor position but DO NOT auto-scroll. */
        cl->last_cursor = cl->cursor;
    }
}

int flux_cursor_list_move(int cursor, int n, int delta) {
    if (n <= 0) return 0;
    int c = cursor + delta;
    while (c < 0) c += n;
    return c % n;
}

/* ── 4c. FluxDialog ───────────────────────────────────────────── */

void flux_dialog_init(FluxDialog *d) {
    if (!d) return;
    memset(d, 0, sizeof *d);
    flux_overlay_init(&d->layer);
}

void flux_dialog_free(FluxDialog *d) {
    if (!d) return;
    flux_overlay_free(&d->layer);
}

void flux_dialog_open(FluxDialog *d, FluxDialogCfg cfg) {
    if (!d) return;
    d->cfg = cfg;
    d->open = 1;
    d->answered = 0;
    d->result = -1;
    d->focus = cfg.default_idx;
    if (d->focus < 0 || d->focus >= cfg.n_buttons) d->focus = 0;
}

void flux_dialog_close(FluxDialog *d) {
    if (!d) return;
    d->open = 0;
    /* Ensure prev rect is cleared on next render. */
    flux_overlay_begin(&d->layer, 0, 0, 0, 0);
}

int flux_dialog_update(FluxDialog *d, FluxMsg msg) {
    if (!d || !d->open || msg.type != MSG_KEY) return 0;
    int n = d->cfg.n_buttons;
    if (n < 1) return 0;
    if (flux_key_is(msg, "esc")) {
        d->answered = 1; d->result = -1; d->open = 0;
        return 1;
    }
    if (flux_key_is(msg, "left") || flux_key_is(msg, "h")) {
        d->focus = (d->focus - 1 + n) % n;
        return 1;
    }
    if (flux_key_is(msg, "right") || flux_key_is(msg, "l") ||
        flux_key_is(msg, "tab")) {
        d->focus = (d->focus + 1) % n;
        return 1;
    }
    if (flux_key_is(msg, "enter") || flux_key_is(msg, " ")) {
        d->answered = 1; d->result = d->focus; d->open = 0;
        return 1;
    }
    /* Numeric shortcuts 1..n */
    for (int i = 0; i < n; i++) {
        char key[2] = { (char)('1' + i), 0 };
        if (flux_key_is(msg, key)) {
            d->answered = 1; d->result = i; d->open = 0;
            return 1;
        }
    }
    /* Per-button shortcut chars */
    for (int i = 0; i < n; i++) {
        if (d->cfg.buttons[i].shortcut &&
            flux_key_is(msg, d->cfg.buttons[i].shortcut)) {
            d->answered = 1; d->result = i; d->open = 0;
            return 1;
        }
    }
    return 1;  /* swallow all other keys while open */
}

static const char *_fxd_dialog_btn_fg(FluxDialogBtnKind k, int focused) {
    if (focused) return FLUX_THEME_TEXT_INV_FG;
    switch (k) {
        case FLUX_DIALOG_BTN_PRIMARY: return FLUX_THEME_ACCENT_FG;
        case FLUX_DIALOG_BTN_DANGER:  return FLUX_THEME_ERR_FG;
        case FLUX_DIALOG_BTN_SUCCESS: return FLUX_THEME_OK_FG;
        default:                      return FLUX_THEME_TEXT_FG;
    }
}
static const char *_fxd_dialog_btn_bg(FluxDialogBtnKind k, int focused) {
    if (!focused) return "";
    switch (k) {
        case FLUX_DIALOG_BTN_PRIMARY: return FLUX_THEME_BUTTON_OK_BG;
        case FLUX_DIALOG_BTN_DANGER:  return FLUX_THEME_BUTTON_NO_BG;
        case FLUX_DIALOG_BTN_SUCCESS: return FLUX_THEME_BUTTON_OK_BG;
        default:                      return FLUX_THEME_PANEL_BG;
    }
}

void flux_dialog_render(FluxDialog *d, FluxSB *sb,
                        int screen_w, int screen_h) {
    if (!d || !sb) return;
    if (!d->open) {
        /* Hidden — clear previous rect once via the overlay. */
        flux_overlay_begin(&d->layer, 0, 0, 0, 0);
        flux_overlay_emit (&d->layer, sb);
        return;
    }
    if (screen_w <= 0 || screen_h <= 0) return;

    int box_w = d->cfg.width_cells > 0 ? d->cfg.width_cells : 50;
    if (box_w > screen_w - 4) box_w = screen_w - 4;
    if (box_w < 20) box_w = 20;

    int box_h = 6;  /* top + title + body + blank + buttons + bottom */
    if (box_h > screen_h - 2) box_h = screen_h - 2;
    if (box_h < 4) return;

    /* If dim_backdrop, paint dim across the WHOLE screen via the layer.
     * Otherwise, only the box rect is in the overlay. */
    int rect_x, rect_y, rect_w, rect_h;
    if (d->cfg.dim_backdrop) {
        rect_x = 1; rect_y = 1; rect_w = screen_w; rect_h = screen_h;
    } else {
        rect_x = (screen_w - box_w) / 2 + 1;
        rect_y = (screen_h - box_h) / 2 + 1;
        rect_w = box_w; rect_h = box_h;
    }

    flux_overlay_begin(&d->layer, rect_x, rect_y, rect_w, rect_h);

    /* For each row: compute what to write. */
    int box_inset_x = d->cfg.dim_backdrop ? (screen_w - box_w) / 2 : 0;
    int box_inset_y = d->cfg.dim_backdrop ? (screen_h - box_h) / 2 : 0;

    char rb[1024];
    for (int r = 0; r < rect_h; r++) {
        FluxSB row; flux_sb_init(&row, rb, (int)sizeof rb);
        int br = r - box_inset_y;  /* row within the box, or -1 if outside */
        if (br < 0 || br >= box_h) {
            /* dim backdrop row */
            flux_sb_append(&row, FLUX_THEME_TITLEBAR_BG);
            for (int c = 0; c < rect_w; c++) flux_sb_append(&row, " ");
        } else {
            /* compose box content row inside dim backdrop */
            if (d->cfg.dim_backdrop && box_inset_x > 0) {
                flux_sb_append(&row, FLUX_THEME_TITLEBAR_BG);
                for (int c = 0; c < box_inset_x; c++) flux_sb_append(&row, " ");
            }
            const char *border_fg = FLUX_THEME_BORDER_WARN_FG;
            const char *panel_bg  = FLUX_THEME_PANEL_BG;
            if (br == 0) {
                flux_sb_append(&row, border_fg);
                flux_sb_append(&row, "\xe2\x95\xad");
                if (d->cfg.title) {
                    int title_len = (int)strlen(d->cfg.title);
                    if (title_len > box_w - 6) title_len = box_w - 6;
                    flux_sb_append(&row, "\xe2\x94\x80\xe2\x94\x80 ");
                    flux_sb_append(&row, FLUX_RESET);
                    flux_sb_append(&row, FLUX_BOLD);
                    for (int i = 0; i < title_len; i++) {
                        char c[2] = { d->cfg.title[i], 0 };
                        flux_sb_append(&row, c);
                    }
                    flux_sb_append(&row, FLUX_RESET);
                    flux_sb_append(&row, border_fg);
                    flux_sb_append(&row, " ");
                    int dashes_left = box_w - 2 - 3 - 1 - title_len;
                    for (int i = 0; i < dashes_left; i++) flux_sb_append(&row, "\xe2\x94\x80");
                } else {
                    for (int i = 0; i < box_w - 2; i++) flux_sb_append(&row, "\xe2\x94\x80");
                }
                flux_sb_append(&row, "\xe2\x95\xae");
                flux_sb_append(&row, FLUX_RESET);
            } else if (br == box_h - 1) {
                flux_sb_append(&row, border_fg);
                flux_sb_append(&row, "\xe2\x95\xb0");
                for (int i = 0; i < box_w - 2; i++) flux_sb_append(&row, "\xe2\x94\x80");
                flux_sb_append(&row, "\xe2\x95\xaf");
                flux_sb_append(&row, FLUX_RESET);
            } else if (br == box_h - 2 && d->cfg.n_buttons > 0) {
                /* button row */
                flux_sb_append(&row, border_fg);
                flux_sb_append(&row, "\xe2\x94\x82");
                flux_sb_append(&row, FLUX_RESET);
                flux_sb_append(&row, panel_bg);
                /* Compute total button width: label + 2 padding per button */
                int total_w = 0;
                for (int i = 0; i < d->cfg.n_buttons; i++) {
                    int lw = (int)strlen(d->cfg.buttons[i].label);
                    total_w += lw + 4;
                    if (i + 1 < d->cfg.n_buttons) total_w += 2;
                }
                int pad = (box_w - 2 - total_w) / 2;
                for (int i = 0; i < pad; i++) flux_sb_append(&row, " ");
                int written = pad;
                for (int i = 0; i < d->cfg.n_buttons; i++) {
                    int focused = (i == d->focus);
                    const char *fg = _fxd_dialog_btn_fg(d->cfg.buttons[i].kind, focused);
                    const char *bg = _fxd_dialog_btn_bg(d->cfg.buttons[i].kind, focused);
                    flux_sb_append(&row, bg);
                    flux_sb_append(&row, fg);
                    if (focused) flux_sb_append(&row, FLUX_BOLD);
                    flux_sb_append(&row, focused ? "[ " : "  ");
                    flux_sb_append(&row, d->cfg.buttons[i].label);
                    flux_sb_append(&row, focused ? " ]" : "  ");
                    flux_sb_append(&row, FLUX_RESET);
                    flux_sb_append(&row, panel_bg);
                    written += (int)strlen(d->cfg.buttons[i].label) + 4;
                    if (i + 1 < d->cfg.n_buttons) {
                        flux_sb_append(&row, "  ");
                        written += 2;
                    }
                }
                int trail = box_w - 2 - written;
                for (int i = 0; i < trail; i++) flux_sb_append(&row, " ");
                flux_sb_append(&row, FLUX_RESET);
                flux_sb_append(&row, border_fg);
                flux_sb_append(&row, "\xe2\x94\x82");
                flux_sb_append(&row, FLUX_RESET);
            } else {
                /* body / blank row */
                flux_sb_append(&row, border_fg);
                flux_sb_append(&row, "\xe2\x94\x82");
                flux_sb_append(&row, FLUX_RESET);
                flux_sb_append(&row, panel_bg);
                if (br == 2 && d->cfg.body) {
                    int body_len = (int)strlen(d->cfg.body);
                    if (body_len > box_w - 4) body_len = box_w - 4;
                    flux_sb_append(&row, "  ");
                    for (int i = 0; i < body_len; i++) {
                        char c[2] = { d->cfg.body[i], 0 };
                        flux_sb_append(&row, c);
                    }
                    int trail = box_w - 2 - 2 - body_len;
                    for (int i = 0; i < trail; i++) flux_sb_append(&row, " ");
                } else {
                    for (int i = 0; i < box_w - 2; i++) flux_sb_append(&row, " ");
                }
                flux_sb_append(&row, FLUX_RESET);
                flux_sb_append(&row, border_fg);
                flux_sb_append(&row, "\xe2\x94\x82");
                flux_sb_append(&row, FLUX_RESET);
            }
            if (d->cfg.dim_backdrop) {
                int trail = rect_w - box_inset_x - box_w;
                if (trail > 0) {
                    flux_sb_append(&row, FLUX_THEME_TITLEBAR_BG);
                    for (int c = 0; c < trail; c++) flux_sb_append(&row, " ");
                    flux_sb_append(&row, FLUX_RESET);
                }
            }
        }
        flux_overlay_putln(&d->layer, r, flux_sb_str(&row));
    }
    flux_overlay_emit(&d->layer, sb);
}

/* ── 6. FluxTicker ────────────────────────────────────────────── */

void flux_ticker_init(FluxTicker *t) {
    if (!t) return;
    memset(t, 0, sizeof *t);
}

void flux_ticker_push(FluxTicker *t, FluxChannelId ch, const char *text) {
    if (!t || !text) return;
    int slot;
    if (t->count < FLUX_TICKER_MAX) {
        slot = (t->head + t->count) % FLUX_TICKER_MAX;
        t->count++;
    } else {
        t->head = (t->head + 1) % FLUX_TICKER_MAX;
        slot = (t->head + t->count - 1) % FLUX_TICKER_MAX;
    }
    FluxTickerEvent *ev = &t->events[slot];
    ev->channel = ch;
    strncpy(ev->text, text, sizeof ev->text - 1);
    ev->text[sizeof ev->text - 1] = 0;
    ev->arrived_ms = flux_now_ms();
}

void flux_ticker_tick(FluxTicker *t, uint64_t now_ms) {
    if (!t) return;
    t->now_ms = now_ms;
    t->shimmer_off++;
}

void flux_ticker_render(FluxTicker *t, FluxSB *sb, int width) {
    if (!t || !sb || width <= 0) return;
    /* Compose a scrolling strip of events separated by ' · '. We ignore
     * actual horizontal scroll animation here and render newest → oldest
     * left-to-right, shimmering the newest. */
    char line[1024];
    int li = 0;
    line[0] = 0;
    int n = t->count;
    uint64_t newest_arrived = 0;
    int newest_idx = -1;
    for (int i = 0; i < n; i++) {
        int idx = (t->head + n - 1 - i) % FLUX_TICKER_MAX;
        FluxTickerEvent *ev = &t->events[idx];
        if (ev->arrived_ms > newest_arrived) {
            newest_arrived = ev->arrived_ms;
            newest_idx = idx;
        }
    }

    for (int i = 0; i < n && li < (int)sizeof line - 64; i++) {
        int idx = (t->head + n - 1 - i) % FLUX_TICKER_MAX;
        FluxTickerEvent *ev = &t->events[idx];
        FluxRGB c = flux_channel_rgb(ev->channel);
        int shimmer = (idx == newest_idx) &&
                      (t->now_ms - ev->arrived_ms < 800);
        int add;
        if (shimmer) {
            add = snprintf(line + li, sizeof line - li,
                           "\x1b[1m\x1b[38;2;%u;%u;%um[%s] %s\x1b[0m · ",
                           (unsigned)c.r, (unsigned)c.g, (unsigned)c.b,
                           flux_channel_label(ev->channel), ev->text);
        } else {
            add = snprintf(line + li, sizeof line - li,
                           "\x1b[38;2;%u;%u;%um[%s]\x1b[0m %s · ",
                           (unsigned)c.r, (unsigned)c.g, (unsigned)c.b,
                           flux_channel_label(ev->channel), ev->text);
        }
        if (add < 0) break;
        li += add;
    }
    if (n == 0) {
        snprintf(line, sizeof line, "%s(ticker idle)%s",
                 FLUX_THEME_TEXT_DIM_FG, FLUX_RESET);
    }
    flux_fit(sb, line, width, NULL, FLUX_ALIGN_LEFT);
    flux_sb_append(sb, FLUX_RESET);
    flux_sb_nl(sb);
}

/* ── 7. FluxGanttRow ──────────────────────────────────────────── */

void flux_gantt_row(FluxSB *sb, const FluxGanttTask *tasks, int n_tasks,
                    uint64_t window_ms, uint64_t now_ms, int width) {
    if (!sb || !tasks || n_tasks <= 0 || width <= 0 || window_ms == 0) {
        if (sb && width > 0) {
            for (int i = 0; i < width; i++) flux_sb_append(sb, " ");
            flux_sb_nl(sb);
        }
        return;
    }
    uint64_t t0 = now_ms > window_ms ? now_ms - window_ms : 0;
    for (int i = 0; i < n_tasks; i++) {
        const FluxGanttTask *tk = &tasks[i];
        uint64_t s = tk->start_ms > t0 ? tk->start_ms : t0;
        uint64_t e = tk->end_ms == 0 ? now_ms : tk->end_ms;
        if (e < t0 || s > now_ms) {
            for (int c = 0; c < width; c++) flux_sb_append(sb, " ");
            flux_sb_nl(sb);
            continue;
        }
        if (s < t0) s = t0;
        if (e > now_ms) e = now_ms;
        int start_cell = (int)(((s - t0) * (uint64_t)width) / window_ms);
        int end_cell   = (int)(((e - t0) * (uint64_t)width) / window_ms);
        if (start_cell < 0) start_cell = 0;
        if (end_cell   > width) end_cell = width;

        /* Compute label to embed at start of the bar.  Truncate label
         * to fit within the row (leaving at least 2 cells for the bar
         * so a long label never eats the entire row). */
        char label[64];
        int label_max_w = width > 6 ? (width / 3) : width;
        if (label_max_w > 16) label_max_w = 16;
        if (label_max_w < 0)  label_max_w = 0;
        int label_w = 0;
        if (tk->label && label_max_w > 0) {
            label_w = flux_truncate(tk->label, label_max_w, "",
                                    label, sizeof label);
        } else {
            label[0] = 0;
        }
        if (label_w > width) label_w = width;
        int label_start = 0;
        int label_end   = label_w;   /* cols [label_start, label_end) */
        if (label_w == 0) label_end = 0;

        /* Emit cell-by-cell; each col produces exactly 1 display cell.  */
        int label_emitted = 0;
        for (int col = 0; col < width; col++) {
            if (col >= label_start && col < label_end) {
                if (!label_emitted) {
                    _fxd_fg_rgb(sb, tk->color);
                    flux_sb_append(sb, FLUX_BOLD);
                    flux_sb_append(sb, label);
                    flux_sb_append(sb, FLUX_RESET);
                    label_emitted = 1;
                }
                /* This col (and label_w - 1 subsequent) were covered by
                 * the label emission. */
            } else if (col >= start_cell && col < end_cell) {
                _fxd_bg_rgb(sb, tk->color);
                flux_sb_append(sb, " ");
                flux_sb_append(sb, FLUX_RESET);
            } else {
                flux_sb_append(sb, " ");
            }
            /* When we are inside the label range, advance col to end of label */
            if (col == label_start && label_w > 0 && label_emitted) {
                col = label_end - 1; /* for-loop ++ takes us to label_end */
            }
        }
        flux_sb_nl(sb);
    }
}

/* ── 8. FluxParticleBurst ─────────────────────────────────────── */

static unsigned _fxd_rand(unsigned *seed) {
    *seed = (*seed * 1103515245u) + 12345u;
    return (*seed >> 16) & 0x7fff;
}

void flux_particle_burst_init(FluxParticleBurst *pb) {
    if (!pb) return;
    memset(pb, 0, sizeof *pb);
    pb->seed = 0xbeef;
}

void flux_particle_burst_spawn(FluxParticleBurst *pb, int x, int y,
                               FluxRGB color, int n_particles) {
    if (!pb) return;
    if (n_particles > FLUX_PARTICLE_MAX) n_particles = FLUX_PARTICLE_MAX;
    uint64_t now = flux_now_ms();
    pb->last_ms = now;
    pb->now_ms  = now;
    int to_spawn = n_particles;
    for (int i = 0; i < FLUX_PARTICLE_MAX && to_spawn > 0; i++) {
        FluxParticle *p = &pb->p[i];
        if (p->alive) continue;
        p->alive = 1;
        p->x = (float)x;
        p->y = (float)y;
        float ang = ((float)(_fxd_rand(&pb->seed) % 360)) * 3.14159f / 180.0f;
        float spd = 0.15f + ((float)(_fxd_rand(&pb->seed) % 40)) / 100.0f;
        p->vx = cosf(ang) * spd;
        p->vy = sinf(ang) * spd - 0.3f;
        p->color = color;
        p->spawned_ms = now;
        p->ttl_ms = 600;
        to_spawn--;
        if (i >= pb->count) pb->count = i + 1;
    }
}

void flux_particle_burst_tick(FluxParticleBurst *pb, uint64_t now_ms) {
    if (!pb) return;
    uint64_t dt = pb->last_ms == 0 ? 0 : now_ms - pb->last_ms;
    if (dt > 100) dt = 100;
    float f = (float)dt;
    pb->now_ms = now_ms;
    pb->last_ms = now_ms;
    for (int i = 0; i < pb->count; i++) {
        FluxParticle *p = &pb->p[i];
        if (!p->alive) continue;
        p->x  += p->vx * f / 30.0f;
        p->y  += p->vy * f / 30.0f;
        p->vy += 0.03f * f / 30.0f;   /* gravity */
        if ((uint64_t)(now_ms - p->spawned_ms) >= (uint64_t)p->ttl_ms) {
            p->alive = 0;
        }
    }
}

void flux_particle_burst_render(FluxParticleBurst *pb, FluxSB *sb,
                                int screen_w, int screen_h) {
    if (!pb || !sb) return;
    for (int i = 0; i < pb->count; i++) {
        FluxParticle *p = &pb->p[i];
        if (!p->alive) continue;
        int px = (int)p->x, py = (int)p->y;
        if (px < 1 || py < 1 || px > screen_w || py > screen_h) continue;
        float life = (float)(pb->now_ms - p->spawned_ms) / (float)p->ttl_ms;
        if (life > 1.0f) life = 1.0f;
        float fade = 1.0f - life;
        FluxRGB c = { (unsigned char)(p->color.r * fade),
                      (unsigned char)(p->color.g * fade),
                      (unsigned char)(p->color.b * fade) };
        flux_sb_appendf(sb, "\x1b[%d;%dH", py, px);
        _fxd_fg_rgb(sb, c);
        flux_sb_append(sb, life < 0.5f ? "*" : "·");
        flux_sb_append(sb, FLUX_RESET);
    }
}

/* ── 9. FluxSplitPane ─────────────────────────────────────────── */

void flux_split_init(FluxSplitPane *sp, FluxSplitOrient orient, int n_panes) {
    if (!sp) return;
    if (n_panes < 1) n_panes = 1;
    if (n_panes > FLUX_SPLIT_MAX_PANES) n_panes = FLUX_SPLIT_MAX_PANES;
    memset(sp, 0, sizeof *sp);
    sp->orient = orient;
    sp->n_panes = n_panes;
}

int flux_split_update(FluxSplitPane *sp, FluxMsg msg) {
    if (!sp) return 0;
    if (msg.type != MSG_KEY) return 0;
    if (flux_key_is(msg, "tab")) {
        sp->focus = (sp->focus + 1) % sp->n_panes;
        return 1;
    }
    if (flux_key_is(msg, "shift+tab") || flux_key_is(msg, "s-tab")) {
        sp->focus = (sp->focus - 1 + sp->n_panes) % sp->n_panes;
        return 1;
    }
    return 0;
}

void flux_split_render(FluxSplitPane *sp, FluxSB *sb, int w, int h,
                       FluxSplitRenderFn *fns, void **ctxs) {
    if (!sp || !sb || !fns || w <= 0 || h <= 0) return;
    int n = sp->n_panes;
    if (n < 1) n = 1;
    int total_ratio = 0;
    for (int i = 0; i < n; i++) total_ratio += sp->ratios[i] > 0 ? sp->ratios[i] : 1;
    int used = 0;
    for (int i = 0; i < n; i++) {
        int r = sp->ratios[i] > 0 ? sp->ratios[i] : 1;
        int span;
        int total = (sp->orient == FLUX_SPLIT_HORIZONTAL) ? w : h;
        if (i == n - 1) {
            span = total - used;
        } else {
            span = (total * r) / total_ratio;
        }
        if (span < 1) span = 1;
        int pane_w = (sp->orient == FLUX_SPLIT_HORIZONTAL) ? span : w;
        int pane_h = (sp->orient == FLUX_SPLIT_HORIZONTAL) ? h    : span;
        if (fns[i]) fns[i](sb, pane_w, pane_h, (sp->focus == i),
                           ctxs ? ctxs[i] : NULL);
        used += span;
    }
}

/* ── 10. FluxAgentCard ────────────────────────────────────────── */

void flux_agent_card_render(const FluxAgentCard *card, FluxSB *sb, int width) {
    if (!sb || !card || width < 12) {
        if (sb && width > 0) {
            for (int i = 0; i < FLUX_AGENT_CARD_H; i++) {
                flux_fit(sb, "", width > 0 ? width : 1, NULL, FLUX_ALIGN_LEFT);
                flux_sb_nl(sb);
            }
        }
        return;
    }

    FluxRGB accent = flux_channel_rgb(card->channel);
    /* Border pulse — modulate brightness via pulse_t + back easing */
    float pulse = flux_ease_out_back(_fxd_clamp01(card->pulse_t));
    FluxRGB dim = { (unsigned char)(accent.r / 3 + 20),
                    (unsigned char)(accent.g / 3 + 20),
                    (unsigned char)(accent.b / 3 + 20) };
    FluxRGB border = card->focused ? flux_rgb_lerp(dim, accent, 0.5f + pulse * 0.5f)
                                   : dim;

    /* Row 0: top border with channel badge inline.
     * Emit directly to sb so we don't round-trip through flux_fit. */
    {
        int bw = width / 3;
        if (bw > 12) bw = 12;
        if (bw < 6)  bw = 6;
        if (bw > width - 4) bw = width - 4;
        if (bw < 0) bw = 0;

        _fxd_fg_rgb(sb, border);
        flux_sb_append(sb, "\xe2\x95\xad"); /* ╭ — 1 cell */
        flux_sb_append(sb, "\xe2\x94\x80"); /* ─ — 1 cell */
        flux_sb_append(sb, FLUX_RESET);

        if (bw > 0) {
            char badge_buf[128];
            FluxSB bb; flux_sb_init(&bb, badge_buf, sizeof badge_buf);
            flux_channel_badge(&bb, card->channel,
                card->state == FLUX_DOT_COMPLETED ? FLUX_BADGE_OK  :
                card->state == FLUX_DOT_FAILED    ? FLUX_BADGE_ERR :
                card->state == FLUX_DOT_RUNNING   ? FLUX_BADGE_RUN :
                                                    FLUX_BADGE_IDLE, bw);
            /* badge ends with \n — strip it. */
            const char *bs = flux_sb_str(&bb);
            int blen = (int)strlen(bs);
            while (blen > 0 && bs[blen - 1] == '\n') blen--;
            char tmp[128];
            if (blen >= (int)sizeof tmp) blen = (int)sizeof tmp - 1;
            memcpy(tmp, bs, (size_t)blen);
            tmp[blen] = 0;
            flux_sb_append(sb, tmp);
        }

        int remaining = width - 2 - bw - 1; /* 2 left corners, 1 right corner */
        if (remaining < 0) remaining = 0;
        _fxd_fg_rgb(sb, border);
        for (int i = 0; i < remaining; i++) flux_sb_append(sb, "\xe2\x94\x80");
        flux_sb_append(sb, "\xe2\x95\xae"); /* ╮ — 1 cell */
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Row 1: name + state dot */
    {
        FluxSB t; char tbuf[512]; flux_sb_init(&t, tbuf, sizeof tbuf);
        _fxd_fg_rgb(&t, border);
        flux_sb_append(&t, "\xe2\x94\x82"); /* │ */
        flux_sb_append(&t, " ");
        const char *state_clr =
            card->state == FLUX_DOT_FAILED    ? FLUX_THEME_ERR_FG :
            card->state == FLUX_DOT_RUNNING   ? FLUX_THEME_BRAND_PURPLE_FG :
            card->state == FLUX_DOT_COMPLETED ? FLUX_THEME_OK_FG :
                                                FLUX_THEME_TEXT_DIM_FG;
        char body[256];
        snprintf(body, sizeof body, "%s•\x1b[0m %s%s\x1b[0m",
                 state_clr, FLUX_BOLD, card->name ? card->name : "agent");
        flux_sb_append(&t, body);
        flux_fit(sb, flux_sb_str(&t), width - 1, NULL, FLUX_ALIGN_LEFT);
        _fxd_fg_rgb(sb, border);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Row 2: tool line */
    {
        FluxSB t; char tbuf[512]; flux_sb_init(&t, tbuf, sizeof tbuf);
        _fxd_fg_rgb(&t, border);
        flux_sb_append(&t, "\xe2\x94\x82 ");
        flux_sb_append(&t, FLUX_THEME_TEXT2_FG);
        char body[256];
        snprintf(body, sizeof body, "→ %s", card->current_tool ? card->current_tool : "(idle)");
        flux_sb_append(&t, body);
        flux_sb_append(&t, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&t), width - 1, NULL, FLUX_ALIGN_LEFT);
        _fxd_fg_rgb(sb, border);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Row 3: sparkline */
    {
        FluxSB t; char tbuf[512]; flux_sb_init(&t, tbuf, sizeof tbuf);
        _fxd_fg_rgb(&t, border);
        flux_sb_append(&t, "\xe2\x94\x82 ");
        float ring_copy[32];
        int spark_w = width - 4;
        if (spark_w < 1) spark_w = 1;
        if (spark_w > 32) spark_w = 32;
        for (int i = 0; i < spark_w; i++) {
            ring_copy[i] = card->token_rate_ring[i % 32];
        }
        char color_buf[32];
        snprintf(color_buf, sizeof color_buf, "\x1b[38;2;%u;%u;%um",
                 (unsigned)accent.r, (unsigned)accent.g, (unsigned)accent.b);
        FluxSB sparkbuf; char sb2[256];
        flux_sb_init(&sparkbuf, sb2, sizeof sb2);
        flux_sparkline(&sparkbuf, ring_copy, spark_w,
                       card->token_rate_head, color_buf);
        flux_sb_append(&t, flux_sb_str(&sparkbuf));
        flux_fit(sb, flux_sb_str(&t), width - 1, NULL, FLUX_ALIGN_LEFT);
        _fxd_fg_rgb(sb, border);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Row 4: elapsed · cost */
    {
        FluxSB t; char tbuf[512]; flux_sb_init(&t, tbuf, sizeof tbuf);
        _fxd_fg_rgb(&t, border);
        flux_sb_append(&t, "\xe2\x94\x82 ");
        char dur[16]; flux_activity_format_duration(card->elapsed_ms, dur, sizeof dur);
        char body[128];
        snprintf(body, sizeof body, "%s%s\x1b[0m · %s$%.3f\x1b[0m",
                 FLUX_THEME_TEXT_DIM_FG, dur,
                 FLUX_THEME_TEXT2_FG, (double)card->cost_usd);
        flux_sb_append(&t, body);
        flux_fit(sb, flux_sb_str(&t), width - 1, NULL, FLUX_ALIGN_LEFT);
        _fxd_fg_rgb(sb, border);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Row 5: tokens */
    {
        FluxSB t; char tbuf[512]; flux_sb_init(&t, tbuf, sizeof tbuf);
        _fxd_fg_rgb(&t, border);
        flux_sb_append(&t, "\xe2\x94\x82 ");
        char body[96];
        snprintf(body, sizeof body, "%s%ld tok\x1b[0m",
                 FLUX_THEME_TEXT_DIM_FG, card->tokens_total);
        flux_sb_append(&t, body);
        flux_fit(sb, flux_sb_str(&t), width - 1, NULL, FLUX_ALIGN_LEFT);
        _fxd_fg_rgb(sb, border);
        flux_sb_append(sb, "\xe2\x94\x82");
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }

    /* Row 6: bottom border */
    {
        FluxSB t; char tbuf[512]; flux_sb_init(&t, tbuf, sizeof tbuf);
        _fxd_fg_rgb(&t, border);
        flux_sb_append(&t, "\xe2\x95\xb0");
        for (int i = 0; i < width - 2; i++) flux_sb_append(&t, "\xe2\x94\x80");
        flux_sb_append(&t, "\xe2\x95\xaf");
        flux_sb_append(&t, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&t), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }
}

/* ── 11. FluxRequestTrace ─────────────────────────────────────── */

static void _fxd_fmt_size(long bytes, char *out, int cap) {
    if (bytes < 0) { snprintf(out, cap, "—"); return; }
    if (bytes < 1024) snprintf(out, cap, "%ldB", bytes);
    else if (bytes < 1024L * 1024L) snprintf(out, cap, "%.1fKB", bytes / 1024.0);
    else snprintf(out, cap, "%.1fMB", bytes / (1024.0 * 1024.0));
}

void flux_request_trace_render(const FluxRequestTrace *tr, FluxSB *sb,
                               int width, int expanded) {
    if (!sb || !tr || width <= 0) return;
    /* Collapsed header row */
    {
        FluxSB t; char tbuf[1024]; flux_sb_init(&t, tbuf, sizeof tbuf);
        const char *mfg = _fxd_method_fg(tr->method ? tr->method : "?");
        const char *sfg = _fxd_status_fg(tr->status);
        char size[32];
        _fxd_fmt_size(tr->size_bytes, size, sizeof size);
        char body[512];
        snprintf(body, sizeof body,
                 "%s%-6s\x1b[0m %-24s %s%3d\x1b[0m %4dms %s",
                 mfg, tr->method ? tr->method : "?",
                 tr->path ? tr->path : "",
                 sfg, tr->status, tr->latency_ms, size);
        flux_sb_append(&t, body);
        flux_fit(sb, flux_sb_str(&t), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_append(sb, FLUX_RESET);
        flux_sb_nl(sb);
    }
    if (!expanded) return;

    /* Divider */
    {
        FluxSB t; char tbuf[256]; flux_sb_init(&t, tbuf, sizeof tbuf);
        flux_sb_append(&t, FLUX_THEME_BORDER_FG);
        for (int i = 0; i < width; i++) flux_sb_append(&t, "\xe2\x94\x80");
        flux_sb_append(&t, FLUX_RESET);
        flux_fit(sb, flux_sb_str(&t), width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
    }

    /* Headers */
    if (tr->headers && tr->headers[0]) {
        const char *p = tr->headers;
        while (*p) {
            const char *nl = strchr(p, '\n');
            int len = nl ? (int)(nl - p) : (int)strlen(p);
            char hbuf[512];
            if (len >= (int)sizeof hbuf) len = sizeof hbuf - 1;
            memcpy(hbuf, p, len); hbuf[len] = 0;
            char full[600];
            snprintf(full, sizeof full, "%s %s\x1b[0m",
                     FLUX_THEME_TEXT_DIM_FG, hbuf);
            flux_fit(sb, full, width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
            if (!nl) break;
            p = nl + 1;
        }
    }

    /* Body */
    if (tr->body && tr->body[0]) {
        /* One row label. */
        char lab[64];
        snprintf(lab, sizeof lab, "%s request body:\x1b[0m", FLUX_THEME_TEXT_DIM_FG);
        flux_fit(sb, lab, width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
        /* Render body lines wrapped to width. Use flux_pretty for JSON. */
        FluxSB pb; char pbuf[4096]; flux_sb_init(&pb, pbuf, sizeof pbuf);
        int looks_json = tr->body[0] == '{' || tr->body[0] == '[';
        if (looks_json) {
            flux_pretty(&pb, tr->body, width, NULL);
        } else {
            flux_syntax_highlight(&pb, tr->body, FLUX_LANG_PLAIN, width);
        }
        const char *s = flux_sb_str(&pb);
        const char *p = s;
        while (*p) {
            const char *nl = strchr(p, '\n');
            int len = nl ? (int)(nl - p) : (int)strlen(p);
            char line[1024];
            if (len >= (int)sizeof line) len = sizeof line - 1;
            memcpy(line, p, len); line[len] = 0;
            flux_fit(sb, line, width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
            if (!nl) break;
            p = nl + 1;
        }
    }

    /* Response body */
    if (tr->response && tr->response[0]) {
        char lab[64];
        snprintf(lab, sizeof lab, "%s response:\x1b[0m", FLUX_THEME_TEXT_DIM_FG);
        flux_fit(sb, lab, width, NULL, FLUX_ALIGN_LEFT);
        flux_sb_nl(sb);
        FluxSB pb; char pbuf[4096]; flux_sb_init(&pb, pbuf, sizeof pbuf);
        int looks_json = tr->response[0] == '{' || tr->response[0] == '[';
        if (looks_json) flux_pretty(&pb, tr->response, width, NULL);
        else            flux_syntax_highlight(&pb, tr->response, FLUX_LANG_PLAIN, width);
        const char *s = flux_sb_str(&pb);
        const char *p = s;
        while (*p) {
            const char *nl = strchr(p, '\n');
            int len = nl ? (int)(nl - p) : (int)strlen(p);
            char line[1024];
            if (len >= (int)sizeof line) len = sizeof line - 1;
            memcpy(line, p, len); line[len] = 0;
            flux_fit(sb, line, width, NULL, FLUX_ALIGN_LEFT);
            flux_sb_nl(sb);
            if (!nl) break;
            p = nl + 1;
        }
    }
}

#endif /* FLUX_IMPL && !FLUX_IMPL_DONE */
