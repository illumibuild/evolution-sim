#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>
#include <SDL_image.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION

#include <nuklear.h>
#include <nuklear_sdl_renderer.h>

#define TITLE "evolution-sim"
#define VERSION "v0.1.0 alpha 2 preview"
#define RELEASE_DATE "01/08/2026"

typedef unsigned char uint8;

typedef unsigned short uint16;

typedef int int32;

typedef unsigned int uint32;

static struct nk_context *nk_ctx;

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

#define MIN_WINDOW_WIDTH 853
#define MIN_WINDOW_HEIGHT 480

#define TILE_SRC_WIDTH  180
#define TILE_SRC_HEIGHT 180

#define ANIMATION_MS 500
#define ANIMATION_FPS 8

#define ANIMATION_TILE_NONE     0x00
#define ANIMATION_CELL_NONE     0x10
#define ANIMATION_CELL_DEATH    0x01
#define ANIMATION_CELL_BIRTH    0x02
#define ANIMATION_CELL_DIVISION 0x03

#define FONT_SIZE 24

static uint32 rng_state, rng_seed;

void my_srand(uint32 seed) {
    rng_seed = rng_state = seed == 0 ? (uint32)time(NULL) : seed;
}

uint32 my_rand(void) {
    uint32 rng_output = rng_state;
    rng_output ^= rng_output >> 12;
    rng_output ^= rng_output << 25;
    rng_output ^= rng_output >> 27;
    rng_state = rng_output;
    return rng_output * 2685821657736338717;
}

static SDL_Window *window;

static SDL_Renderer *renderer;

static SDL_Texture *texture;

static struct nk_font *font;

static struct nk_font_atlas *font_atlas;

static const char *const icon_paths[] = {
    "resources/icons/start.png",
    "resources/icons/stop.png",
    "resources/icons/step.png",
    "resources/icons/zoom_in.png",
    "resources/icons/zoom_out.png",
    "resources/icons/speed_up.png",
    "resources/icons/slow_down.png",
    "resources/icons/quit.png"
};

#define ICON_COUNT sizeof(icon_paths) / sizeof(const char *)

static SDL_Texture *icon_textures[ICON_COUNT];

static struct nk_image icons[ICON_COUNT];

static void select_animation_frame(
    SDL_Rect *srcrect,
    uint8 animation,
    uint32 tick_diff,
    uint8 speed
) {
    srcrect->x =
        (animation / 0x10 + tick_diff / (1000 / (ANIMATION_FPS * speed))) * TILE_SRC_WIDTH;
    srcrect->y = animation % 0x10 * TILE_SRC_HEIGHT;
    srcrect->w = TILE_SRC_WIDTH;
    srcrect->h = TILE_SRC_HEIGHT;
}

#define COLOR_BG             nk_rgba(0x8C, 0x00, 0x3F, 0xFF)
#define COLOR_FG             nk_rgba(0xFF, 0x4B, 0x87, 0xFF)
#define COLOR_INACTIVE       nk_rgba(0xB4, 0x00, 0x51, 0xFF)
#define COLOR_ACTIVE         nk_rgba(0x78, 0x00, 0x36, 0xFF)
#define COLOR_TRIGGER        nk_rgba(0x50, 0x00, 0x24, 0xFF)
#define COLOR_BORDER         nk_rgba(0xC8, 0x00, 0x5A, 0xFF)
#define COLOR_CUSTOM_QUALITY nk_rgba(0x39, 0xFF, 0x14, 0xFF)

struct nk_style_button style_button_disabled;

static inline void nk_skin(void) {
    nk_ctx->style.window.fixed_background.data.color = nk_rgba_u32(0);
    nk_ctx->style.window.padding = nk_vec2(0, 0);
    nk_ctx->style.text.color = COLOR_FG;
    nk_ctx->style.edit.normal.data.color = COLOR_ACTIVE;
    nk_ctx->style.edit.active.data.color = COLOR_ACTIVE;
    nk_ctx->style.edit.border_color = COLOR_BORDER;
    nk_ctx->style.edit.cursor_normal = COLOR_FG;
    nk_ctx->style.edit.cursor_hover = COLOR_FG;
    nk_ctx->style.edit.cursor_text_hover = COLOR_ACTIVE;
    nk_ctx->style.edit.cursor_text_normal = COLOR_ACTIVE;
    nk_ctx->style.edit.text_normal = COLOR_FG;
    nk_ctx->style.edit.text_hover = COLOR_FG;
    nk_ctx->style.edit.text_active = COLOR_FG;
    nk_ctx->style.edit.selected_normal = COLOR_FG;
    nk_ctx->style.edit.selected_hover = COLOR_FG;
    nk_ctx->style.edit.selected_text_normal = COLOR_ACTIVE;
    nk_ctx->style.edit.selected_text_hover = COLOR_ACTIVE;
    nk_ctx->style.button.normal.data.color = COLOR_INACTIVE;
    nk_ctx->style.button.hover.data.color = COLOR_ACTIVE;
    nk_ctx->style.button.active.data.color = COLOR_TRIGGER;
    nk_ctx->style.button.border_color = COLOR_BORDER;
    nk_ctx->style.button.text_normal = COLOR_FG;
    nk_ctx->style.button.text_hover = COLOR_FG;
    nk_ctx->style.button.text_active = COLOR_FG;
    nk_ctx->style.button.image_padding = nk_vec2(0, 0);
    style_button_disabled = nk_ctx->style.button;
    style_button_disabled.normal.data.color = COLOR_TRIGGER;
    style_button_disabled.hover.data.color = COLOR_TRIGGER;
    style_button_disabled.active.data.color = COLOR_TRIGGER;
}

#define TEXTURE_PATH "resources/spritesheet.png"
#define FONT_PATH    "resources/Ubuntu-R.ttf"

static inline bool init(void) {
    if (
        SDL_Init(SDL_INIT_VIDEO) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)
    ) {
        return false;
    }
    window = SDL_CreateWindow(
        TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        return false;
    }
    SDL_SetWindowMinimumSize(window, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    nk_ctx = nk_sdl_init(window, renderer);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    SDL_Surface *surface = IMG_Load(TEXTURE_PATH);
    if (!surface) {
        return false;
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        return false;
    }
    nk_sdl_font_stash_begin(&font_atlas);
    font = nk_font_atlas_add_from_file(
        font_atlas,
        FONT_PATH,
        FONT_SIZE,
        NULL
    );
    nk_sdl_font_stash_end();
    nk_style_set_font(nk_ctx, &font->handle);
    for (uint8 i = 0; i < ICON_COUNT; ++i) {
        SDL_Surface *icon_surface = IMG_Load(icon_paths[i]);
        if (!icon_surface) {
            return false;
        }
        icon_textures[i] = SDL_CreateTextureFromSurface(renderer, icon_surface);
        SDL_FreeSurface(icon_surface);
        if (!icon_textures[i]) {
            return false;
        }
        icons[i] = nk_image_ptr(icon_textures[i]);
    }
    nk_skin();
    return true;
}

static inline void quit(void) {
    for (uint8 i = 0; i < ICON_COUNT; ++i) {
        SDL_DestroyTexture(icon_textures[i]);
        icon_textures[i] = NULL;
    }
    nk_font_atlas_cleanup(font_atlas);
    SDL_DestroyTexture(texture);
    texture = NULL;
    nk_sdl_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

int32 window_w, window_h;

enum UXState {
    UX_ANY,
    UX_CREATION,
    UX_GENERATION,
    UX_SIM
};

static enum UXState ux_state = UX_CREATION;

enum GUIElementType {
    GUI_LABEL,
    GUI_TEXT,
    GUI_INPUT,
    GUI_BUTTON,
    GUI_ICON_BUTTON,
    GUI_PANEL
};

struct GUILabelElement {
    const enum GUIElementType type;
    const char *const text;
    const enum nk_text_align alignment;
    const enum UXState active_ux_state;
    struct nk_rect (*const pos)();
};

#define label_world_size_text "World size (min. 10x10):"
#define label_world_size_alignment NK_TEXT_ALIGN_LEFT
#define label_world_size_active_ux_state UX_CREATION

struct nk_rect label_world_size_pos() {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 - 72,
        300,
        24
    );
}

#define label_seed_text "Seed (leave blank for random):"
#define label_seed_alignment NK_TEXT_ALIGN_LEFT
#define label_seed_active_ux_state UX_CREATION

struct nk_rect label_seed_pos() {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 + 8,
        300,
        24
    );
}

#define label_mul_text "x"
#define label_mul_alignment NK_TEXT_ALIGN_CENTERED
#define label_mul_active_ux_state UX_CREATION

struct nk_rect label_mul_pos() {
    return nk_rect(
        window_w / 2 - 11,
        window_h / 2 - 32,
        22,
        24
    );
}

#define label_generating_text "Generating world..."
#define label_generating_alignment NK_TEXT_ALIGN_CENTERED
#define label_generating_active_ux_state UX_GENERATION

struct nk_rect label_generating_pos() {
    return nk_rect(
        10,
        window_h / 2 - 12,
        window_w - 20,
        24
    );
}

#define label_version_text VERSION
#define label_version_alignment NK_TEXT_ALIGN_RIGHT
#define label_version_active_ux_state UX_ANY

struct nk_rect label_version_pos() {
    return nk_rect(
        10,
        window_h - 58,
        window_w - 20,
        24
    );
}

#define label_release_date_text RELEASE_DATE
#define label_release_date_alignment NK_TEXT_ALIGN_RIGHT
#define label_release_date_active_ux_state UX_ANY

struct nk_rect label_release_date_pos() {
    return nk_rect(
        10,
        window_h - 34,
        window_w - 20,
        24
    );
}

struct GUITextElement {
    const enum GUIElementType type;
    const uint32 max;
    char *buffer;
    const enum nk_text_align alignment;
    struct nk_rect (*const pos)();
    const enum UXState active_ux_state;
};

#define text_error_max 42
#define text_error_alignment NK_TEXT_ALIGN_CENTERED
#define text_error_active_ux_state UX_CREATION

struct nk_rect text_error_pos() {
    return nk_rect(
        window_w / 2 - 300,
        window_h / 2 + 200,
        600,
        24
    );
}

#define text_zoom_max 4
#define text_zoom_alignment NK_TEXT_ALIGN_CENTERED
#define text_zoom_active_ux_state UX_SIM

struct nk_rect text_zoom_pos() {
    return nk_rect(
        260,
        20,
        80,
        24
    );
}

#define text_speed_max 2
#define text_speed_alignment NK_TEXT_ALIGN_CENTERED
#define text_speed_active_ux_state UX_SIM

struct nk_rect text_speed_pos() {
    return nk_rect(
        500,
        20,
        40,
        24
    );
}

#define text_report_world_size_max 56
#define text_report_world_size_alignment NK_TEXT_ALIGN_LEFT
#define text_report_world_size_active_ux_state UX_SIM

struct nk_rect text_report_world_size_pos() {
    return nk_rect(
        10,
        window_h - 106,
        window_w - 20,
        24
    );
}

#define text_report_seed_max 26
#define text_report_seed_alignment NK_TEXT_ALIGN_LEFT
#define text_report_seed_active_ux_state UX_SIM

struct nk_rect text_report_seed_pos() {
    return nk_rect(
        10,
        window_h - 82,
        window_w - 20,
        24
    );
}

#define text_report_gen_max 32
#define text_report_gen_alignment NK_TEXT_ALIGN_LEFT
#define text_report_gen_active_ux_state UX_SIM

struct nk_rect text_report_gen_pos() {
    return nk_rect(
        10,
        window_h - 58,
        window_w - 20,
        24
    );
}

#define text_report_live_cell_count_max 32
#define text_report_live_cell_count_alignment NK_TEXT_ALIGN_LEFT
#define text_report_live_cell_count_active_ux_state UX_SIM

struct nk_rect text_report_live_cell_count_pos() {
    return nk_rect(
        10,
        window_h - 34,
        window_w - 20,
        24
    );
}

struct GUIInputElement {
    const enum GUIElementType type;
    const uint32 max;
    char *buffer;
    nk_bool (*filter)(const struct nk_text_edit *, nk_rune);
    const enum UXState active_ux_state;
    struct nk_rect (*const pos)();
};

#define input_world_width_max 5
#define input_world_width_filter nk_filter_decimal
#define input_world_width_active_ux_state UX_CREATION

struct nk_rect input_world_width_pos() {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 - 40,
        134,
        40
    );
}

#define input_world_height_max 5
#define input_world_height_filter nk_filter_decimal
#define input_world_height_active_ux_state UX_CREATION

struct nk_rect input_world_height_pos() {
    return nk_rect(
        window_w / 2 + 16,
        window_h / 2 - 40,
        134,
        40
    );
}

#define input_seed_max 19
#define input_seed_filter nk_filter_decimal
#define input_seed_active_ux_state UX_CREATION

struct nk_rect input_seed_pos() {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 + 40,
        300,
        40
    );
}

struct GUIButtonElement {
    const enum GUIElementType type;
    const char *const text;
    bool is_enabled, is_pressed;
    const enum UXState active_ux_state;
    struct nk_rect (*const pos)();
};

#define button_generate_text "Generate"
#define button_generate_active_ux_state UX_CREATION

struct nk_rect button_generate_pos() {
    return nk_rect(
        window_w / 2 - 100,
        window_h / 2 + 120,
        200,
        40
    );
}

struct GUIIconButtonElement {
    const enum GUIElementType type;
    const struct nk_image *const icon;
    bool is_enabled, is_pressed;
    const enum UXState active_ux_state;
    struct nk_rect (*const pos)();
};

#define icon_button_start_icon &icons[0]
#define icon_button_start_active_ux_state UX_SIM

struct nk_rect icon_button_start_pos() {
    return nk_rect(
        10,
        10,
        40,
        40
    );
}

#define icon_button_stop_icon &icons[1]
#define icon_button_stop_active_ux_state UX_SIM

struct nk_rect icon_button_stop_pos() {
    return nk_rect(
        60,
        10,
        40,
        40
    );
}

#define icon_button_step_icon &icons[2]
#define icon_button_step_active_ux_state UX_SIM

struct nk_rect icon_button_step_pos() {
    return nk_rect(
        110,
        10,
        40,
        40
    );
}

#define icon_button_zoom_in_icon &icons[3]
#define icon_button_zoom_in_active_ux_state UX_SIM

struct nk_rect icon_button_zoom_in_pos() {
    return nk_rect(
        210,
        10,
        40,
        40
    );
}

#define icon_button_zoom_out_icon &icons[4]
#define icon_button_zoom_out_active_ux_state UX_SIM

struct nk_rect icon_button_zoom_out_pos() {
    return nk_rect(
        350,
        10,
        40,
        40
    );
}

#define icon_button_speed_up_icon &icons[5]
#define icon_button_speed_up_active_ux_state UX_SIM

struct nk_rect icon_button_speed_up_pos() {
    return nk_rect(
        450,
        10,
        40,
        40
    );
}

#define icon_button_slow_down_icon &icons[6]
#define icon_button_slow_down_active_ux_state UX_SIM

struct nk_rect icon_button_slow_down_pos() {
    return nk_rect(
        550,
        10,
        40,
        40
    );
}

#define icon_button_quit_icon &icons[7]
#define icon_button_quit_active_ux_state UX_SIM

struct nk_rect icon_button_quit_pos() {
    return nk_rect(
        window_w - 50,
        10,
        40,
        40
    );
}

struct GUIPanelElement {
    const enum GUIElementType type;
    const char *const id;
    struct nk_rect (*const pos)();
    const enum UXState active_ux_state;
};

#define panel_controls_id "controls"
#define panel_controls_active_ux_state UX_SIM

struct nk_rect panel_controls_pos() {
    return nk_rect(
        -2,
        -2,
        window_w + 4,
        62
    );
}

union GUIElement {
    const enum GUIElementType type;
    struct GUILabelElement label_element;
    struct GUITextElement text_element;
    struct GUIInputElement input_element;
    struct GUIButtonElement button_element;
    struct GUIIconButtonElement icon_button_element;
    struct GUIPanelElement panel_element;
};

static union GUIElement gui_elements[] = {
    {
        .label_element = {
            .type = GUI_LABEL,
            .text = label_world_size_text,
            .alignment = label_world_size_alignment,
            .active_ux_state = label_world_size_active_ux_state,
            .pos = label_world_size_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .text = label_seed_text,
            .alignment = label_seed_alignment,
            .active_ux_state = label_seed_active_ux_state,
            .pos = label_seed_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .text = label_mul_text,
            .alignment = label_mul_alignment,
            .active_ux_state = label_mul_active_ux_state,
            .pos = label_mul_pos
        }
    },
    {
        .input_element = {
            .type = GUI_INPUT,
            .max = input_world_width_max,
            .buffer = NULL,
            .filter = input_world_width_filter,
            .active_ux_state = input_world_width_active_ux_state,
            .pos = input_world_width_pos
        }
    },
    {
        .input_element = {
            .type = GUI_INPUT,
            .max = input_world_height_max,
            .buffer = NULL,
            .filter = input_world_height_filter,
            .active_ux_state = input_world_height_active_ux_state,
            .pos = input_world_height_pos
        }
    },
    {
        .input_element = {
            .type = GUI_INPUT,
            .max = input_seed_max,
            .buffer = NULL,
            .filter = input_seed_filter,
            .active_ux_state = input_seed_active_ux_state,
            .pos = input_seed_pos
        }
    },
    {
        .button_element = {
            .type = GUI_BUTTON,
            .text = button_generate_text,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = button_generate_active_ux_state,
            .pos = button_generate_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .max = text_error_max,
            .buffer = NULL,
            .alignment = text_error_alignment,
            .active_ux_state = text_error_active_ux_state,
            .pos = text_error_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .text = label_generating_text,
            .alignment = label_generating_alignment,
            .active_ux_state = label_generating_active_ux_state,
            .pos = label_generating_pos
        }
    },
    {
        .panel_element = {
            .type = GUI_PANEL,
            .id = panel_controls_id,
            .active_ux_state = panel_controls_active_ux_state,
            .pos = panel_controls_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_start_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_start_active_ux_state,
            .pos = icon_button_start_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_stop_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_stop_active_ux_state,
            .pos = icon_button_stop_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_step_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_step_active_ux_state,
            .pos = icon_button_step_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_zoom_in_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_zoom_in_active_ux_state,
            .pos = icon_button_zoom_in_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_zoom_out_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_zoom_out_active_ux_state,
            .pos = icon_button_zoom_out_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .max = text_zoom_max,
            .buffer = NULL,
            .alignment = text_zoom_alignment,
            .active_ux_state = text_zoom_active_ux_state,
            .pos = text_zoom_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_speed_up_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_speed_up_active_ux_state,
            .pos = icon_button_speed_up_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_slow_down_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_slow_down_active_ux_state,
            .pos = icon_button_slow_down_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .max = text_speed_max,
            .buffer = NULL,
            .alignment = text_speed_alignment,
            .active_ux_state = text_speed_active_ux_state,
            .pos = text_speed_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .icon = icon_button_quit_icon,
            .is_enabled = false,
            .is_pressed = false,
            .active_ux_state = icon_button_quit_active_ux_state,
            .pos = icon_button_quit_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .max = text_report_world_size_max,
            .buffer = NULL,
            .alignment = text_report_world_size_alignment,
            .active_ux_state = text_report_world_size_active_ux_state,
            .pos = text_report_world_size_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .max = text_report_seed_max,
            .buffer = NULL,
            .alignment = text_report_seed_alignment,
            .active_ux_state = text_report_seed_active_ux_state,
            .pos = text_report_seed_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .max = text_report_gen_max,
            .buffer = NULL,
            .alignment = text_report_gen_alignment,
            .active_ux_state = text_report_gen_active_ux_state,
            .pos = text_report_gen_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .max = text_report_live_cell_count_max,
            .buffer = NULL,
            .alignment = text_report_live_cell_count_alignment,
            .active_ux_state = text_report_live_cell_count_active_ux_state,
            .pos = text_report_live_cell_count_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .text = label_version_text,
            .alignment = label_version_alignment,
            .active_ux_state = label_version_active_ux_state,
            .pos = label_version_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .text = label_release_date_text,
            .alignment = label_release_date_alignment,
            .active_ux_state = label_release_date_active_ux_state,
            .pos = label_release_date_pos
        }
    }
};

#define GUI_ELEMENT_COUNT sizeof(gui_elements) / sizeof(union GUIElement)

#define label_world_size            gui_elements[0].label_element
#define label_seed                  gui_elements[1].label_element
#define label_mul                   gui_elements[2].label_element
#define input_world_width           gui_elements[3].input_element
#define input_world_height          gui_elements[4].input_element
#define input_seed                  gui_elements[5].input_element
#define button_generate             gui_elements[6].button_element
#define text_error                  gui_elements[7].text_element
#define label_generating            gui_elements[8].label_element
#define panel_controls              gui_elements[9].panel_element
#define icon_button_start           gui_elements[10].icon_button_element
#define icon_button_stop            gui_elements[11].icon_button_element
#define icon_button_step            gui_elements[12].icon_button_element
#define icon_button_zoom_in         gui_elements[13].icon_button_element
#define icon_button_zoom_out        gui_elements[14].icon_button_element
#define text_zoom                   gui_elements[15].text_element
#define icon_button_speed_up        gui_elements[16].icon_button_element
#define icon_button_slow_down       gui_elements[17].icon_button_element
#define text_speed                  gui_elements[18].text_element
#define icon_button_quit            gui_elements[19].icon_button_element
#define text_report_world_size      gui_elements[20].text_element
#define text_report_seed            gui_elements[21].text_element
#define text_report_gen             gui_elements[22].text_element
#define text_report_live_cell_count gui_elements[23].text_element
#define label_version               gui_elements[24].text_element
#define label_release_date          gui_elements[25].text_element

static inline bool start_gui(void) {
    for (uint8 i = 0; i < GUI_ELEMENT_COUNT; ++i) {
        switch (gui_elements[i].type) {
        case GUI_TEXT:
            {
                struct GUITextElement *gui_text_element =
                    &gui_elements[i].text_element;
                gui_text_element->buffer = (char *)calloc(gui_text_element->max + 1, 1);
                if (!gui_text_element->buffer) {
                    return false;
                }
            }
            break;
        case GUI_INPUT:
            {
                struct GUIInputElement *gui_input_element =
                    &gui_elements[i].input_element;
                gui_input_element->buffer = (char *)calloc(gui_input_element->max + 1, 1);
                if (!gui_input_element->buffer) {
                    return false;
                }
            }
            break;
        default:;
        }
    }
    return true;
}

static inline void do_gui(void) {
    if (
        nk_begin(
            nk_ctx,
            "",
            nk_rect(0, 0, window_w, window_h),
            NK_WINDOW_NO_SCROLLBAR
        )
    ) {
        nk_layout_space_begin(
            nk_ctx,
            NK_STATIC,
            window_h,
            GUI_ELEMENT_COUNT
        );
        for (uint8 i = 0; i < GUI_ELEMENT_COUNT; ++i) {
            switch (gui_elements[i].type) {
            case GUI_LABEL:
                {
                    struct GUILabelElement *gui_label_element =
                        &gui_elements[i].label_element;
                    if (
                        ux_state != gui_label_element->active_ux_state &&
                        gui_label_element->active_ux_state != UX_ANY
                    ) {
                        continue;
                    }
                    nk_layout_space_push(
                        nk_ctx,
                        gui_label_element->pos()
                    );
                    nk_text(
                        nk_ctx,
                        gui_label_element->text,
                        strlen(gui_label_element->text),
                        gui_label_element->alignment
                    );
                }
                break;
            case GUI_TEXT:
                {
                    struct GUITextElement *gui_text_element =
                        &gui_elements[i].text_element;
                    if (
                        ux_state != gui_text_element->active_ux_state &&
                        gui_text_element->active_ux_state != UX_ANY
                    ) {
                        memset(gui_text_element->buffer, '\0', gui_text_element->max);
                        continue;
                    }
                    nk_layout_space_push(
                        nk_ctx,
                        gui_text_element->pos()
                    );
                    nk_text(
                        nk_ctx,
                        gui_text_element->buffer,
                        strlen(gui_text_element->buffer),
                        gui_text_element->alignment
                    );
                }
                break;
            case GUI_INPUT:
                {
                    struct GUIInputElement *gui_input_element =
                        &gui_elements[i].input_element;
                    if (
                        ux_state != gui_input_element->active_ux_state &&
                        gui_input_element->active_ux_state != UX_ANY
                    ) {
                        memset(gui_input_element->buffer, '\0', gui_input_element->max);
                        continue;
                    }
                    nk_layout_space_push(
                        nk_ctx,
                        gui_input_element->pos()
                    );
                    nk_edit_string_zero_terminated(
                        nk_ctx,
                        NK_EDIT_FIELD,
                        gui_input_element->buffer,
                        gui_input_element->max + 1,
                        gui_input_element->filter
                    );
                }
                break;
            case GUI_BUTTON:
                {
                    struct GUIButtonElement *gui_button_element =
                        &gui_elements[i].button_element;
                    if (
                        ux_state != gui_button_element->active_ux_state &&
                        gui_button_element->active_ux_state != UX_ANY
                    ) {
                        gui_button_element->is_enabled = false;
                        gui_button_element->is_pressed = false;
                        continue;
                    }
                    nk_layout_space_push(
                        nk_ctx,
                        gui_button_element->pos()
                    );
                    if (gui_button_element->is_enabled) {
                        gui_button_element->is_pressed = (bool)nk_button_label(
                            nk_ctx,
                            gui_button_element->text
                        );
                    } else {
                        gui_button_element->is_pressed = false;
                        nk_button_label_styled(
                            nk_ctx,
                            &style_button_disabled,
                            gui_button_element->text
                        );
                    }
                }
                break;
            case GUI_ICON_BUTTON:
                {
                    struct GUIIconButtonElement *gui_icon_button_element =
                        &gui_elements[i].icon_button_element;
                    if (
                        ux_state != gui_icon_button_element->active_ux_state &&
                        gui_icon_button_element->active_ux_state != UX_ANY
                    ) {
                        gui_icon_button_element->is_enabled = false;
                        gui_icon_button_element->is_pressed = false;
                        continue;
                    }
                    nk_layout_space_push(
                        nk_ctx,
                        gui_icon_button_element->pos()
                    );
                    if (gui_icon_button_element->is_enabled) {
                        gui_icon_button_element->is_pressed = (bool)nk_button_image(
                            nk_ctx,
                            *gui_icon_button_element->icon
                        );
                    } else {
                        gui_icon_button_element->is_pressed = false;
                        nk_button_image_styled(
                            nk_ctx,
                            &style_button_disabled,
                            *gui_icon_button_element->icon
                        );
                    }
                }
                break;
            case GUI_PANEL:
                {
                    struct GUIPanelElement *gui_panel_element =
                        &gui_elements[i].panel_element;
                    if (
                        ux_state != gui_panel_element->active_ux_state &&
                        gui_panel_element->active_ux_state != UX_ANY
                    ) {
                        continue;
                    }
                    nk_fill_rect(
                        nk_window_get_canvas(nk_ctx),
                        gui_panel_element->pos(),
                        0,
                        COLOR_BG
                    );
                    nk_stroke_rect(
                        nk_window_get_canvas(nk_ctx),
                        gui_panel_element->pos(),
                        0,
                        1,
                        COLOR_BORDER
                    );
                }
            }
        }
        nk_layout_space_end(nk_ctx);
    }
    nk_end(nk_ctx);
}

static inline void end_gui(void) {
    for (uint8 i = 0; i < GUI_ELEMENT_COUNT; ++i) {
        if (gui_elements[i].type == GUI_INPUT) {
            struct GUIInputElement *gui_input_element = &gui_elements[i].input_element;
            free(gui_input_element->buffer);
        }
    }
}

struct Cell {
    uint32 age, energy;
};

struct Tile {
    uint32 energy;
    struct Cell cell;
};

struct World {
    uint32 gen;
    uint16 w, h;
    struct Tile *tilemap;
};

static struct World world;

#define tile_at(x, y) world.tilemap[(y) * (uint32)world.w + (x)]

#define MAX_GENERATION_OPS_PER_TICK 1000000

static bool generate() {
    static uint16 x = 0, y = 0;
    uint32 operations = 0;
    for (; x < world.w; ++x) {
        for (; y < world.h; ++y) {
            struct Tile *tile = &tile_at(x, y);
            uint32 base = my_rand() % (77 * 76 / 2);
            for (uint32 i = 0; i < 76; ++i) {
                if (base < 76 - i) {
                    tile->energy = i;
                    break;
                }
                base -= 76 - i;
            }
            if (my_rand() % 50 == 0) {
                tile->cell.energy = my_rand() % 6 + 5;
            }
            if (++operations > MAX_GENERATION_OPS_PER_TICK) {
                return false;
            }
        }
        y = 0;
    }
    x = 0;
    y = 0;
    return true;
}

enum Direction {
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_NONE
};

struct CellAction {
    uint16 x, y;
    enum Direction direction;
};

uint32 live_cell_count, dying_cell_count, born_cell_count;

struct CellAction *cell_deaths = NULL, *cell_divisions = NULL;

static void advance(void) {
    ++world.gen;
    free(cell_deaths);
    free(cell_divisions);
    live_cell_count = 0;
    dying_cell_count = 0;
    born_cell_count = 0;
    cell_deaths = NULL;
    cell_divisions = NULL;
    for (uint16 x = 0; x < world.w; ++x) {
        for (uint16 y = 0; y < world.h; ++y) {
            struct Tile *const tile = &tile_at(x, y);
            if (tile->cell.energy == 0) {
                continue;
            }
            ++tile->cell.age;
            const uint8
                harvested_energy =
                    (tile->energy >= 100 ? 3 : tile->energy >= 50 ? 2 : 1) +
                    (tile->cell.age >= 40 ? 2 : tile->cell.age >= 20 ? 1 : 0),
                actual_harvested_energy = harvested_energy > tile->energy ?
                    tile->energy : harvested_energy;
            tile->energy -= actual_harvested_energy;
            tile->cell.energy += actual_harvested_energy;
            if (--tile->cell.energy == 0) {
                cell_deaths = (struct CellAction *)realloc(
                    cell_deaths, ++dying_cell_count * sizeof(struct CellAction)
                );
                cell_deaths[dying_cell_count - 1].x = x;
                cell_deaths[dying_cell_count - 1].y = y;
                cell_deaths[dying_cell_count - 1].direction = DIRECTION_NONE;
            } else {
                ++live_cell_count;
            }
            if (tile->cell.energy >= 10 && tile->cell.age >= 10) {
                struct Tile *selected_tile = NULL;
                struct Tile *const adjacent_tiles[4]= {
                    y > 0 ? &tile_at(x, y - 1) : NULL,
                    x < world.w ? &tile_at(x + 1, y) : NULL,
                    y < world.h - 1 ? &tile_at(x, y + 1) : NULL,
                    x > 0 ? &tile_at(x - 1, y) : NULL
                };
                enum Direction direction = DIRECTION_NONE;
                for (uint8 i = 0; i < 4; ++i) {
                    if (
                        adjacent_tiles[i] &&
                        adjacent_tiles[i]->cell.energy == 0 &&
                        (
                            !selected_tile ||
                            adjacent_tiles[i]->energy > selected_tile->energy
                        )
                    ) {
                        selected_tile = adjacent_tiles[i];
                        direction = i;
                    }
                }
                if (selected_tile) {
                    cell_divisions = (struct CellAction *)realloc(
                        cell_divisions, ++born_cell_count * sizeof(struct CellAction)
                    );
                    cell_divisions[born_cell_count - 1].x = x;
                    cell_divisions[born_cell_count - 1].y = y;
                    cell_divisions[born_cell_count - 1].direction = direction;
                    tile->cell.age = 0;
                    selected_tile->cell.age = 0;
                    tile->cell.energy /= 2;
                    selected_tile->cell.energy = tile->cell.energy;
                }
            }
        }
    }
}

bool is_running = true;

int32 scroll_x, scroll_y;

static bool ux_creation(void) {
    uintmax_t potential_w = UINTMAX_MAX, potential_h = UINTMAX_MAX;
    if (input_world_width.buffer[0] != '\0' && input_world_height.buffer[0] != '\0') {
        errno = 0;
        potential_w = strtoumax(input_world_width.buffer, NULL, 10);
        if (errno != 0) {
            return false;
        }
        potential_h = strtoumax(input_world_height.buffer, NULL, 10);
        if (errno != 0) {
            return false;
        }
        button_generate.is_enabled =
            potential_w >= 10 &&
            potential_w <= UINT16_MAX &&
            potential_h >= 10 &&
            potential_h <= UINT16_MAX;
    } else {
        button_generate.is_enabled = false;
    }
    if (button_generate.is_pressed) {
        errno = 0;
        const uint32 seed = input_seed.buffer[0] == '\0' ?
            0 : (uint32)strtoumax(input_seed.buffer, NULL, 10);
        if (errno != 0) {
            return false;
        }
        my_srand(seed);
        world.tilemap =
            (struct Tile *)calloc(potential_w * potential_h, sizeof(struct Tile));
        if (!world.tilemap) {
            snprintf(
                text_error.buffer,
                text_error_max + 1,
                "Out of memory! Try making a smaller world!"
            );
            return true;
        }
        world.gen = 0;
        world.w = potential_w;
        world.h = potential_h;
        ux_state = UX_GENERATION;
    }
    return true;
}

static bool ux_generation(void) {
    if (generate()) {
        live_cell_count = 0;
        for (uint16 x = 0; x < world.w; ++x) {
            for (uint16 y = 0; y < world.h; ++y) {
                if (tile_at(x, y).cell.energy != 0) {
                    ++live_cell_count;
                }
            }
        }
        ux_state = UX_SIM;
    }
    return true;
}

#define ZOOM_SPEED 5

#define MIN_ZOOM 5
#define DEFAULT_ZOOM 25
#define MAX_ZOOM 100

#define MIN_SPEED 1
#define DEFAULT_SPEED 1
#define MAX_SPEED 8

static bool ux_sim(void) {
    static bool is_ready = false;
    static uint32 last_gen = UINT32_MAX;
    static uint8 zoom = DEFAULT_ZOOM, speed = DEFAULT_SPEED;
    static int32 cam_x = 0, cam_y = 0, drag_x = 0, drag_y = 0;
    static bool  is_dragging = false, has_acted = false, auto_mode = false;
    static uint32 animation_tick = 0;
    if (!is_ready) {
        snprintf(
            text_zoom.buffer,
            text_zoom_max + 1,
            "%u%%",
            (uint32)DEFAULT_ZOOM
        );
        snprintf(
            text_speed.buffer,
            text_speed_max + 1,
            "%ux",
            (uint32)DEFAULT_SPEED
        );
        snprintf(
            text_report_world_size.buffer,
            text_report_world_size_max + 1,
            "World size: %ux%u",
            world.w,
            world.h
        );
        snprintf(
            text_report_seed.buffer,
            text_report_seed_max + 1,
            "Seed: %u",
            rng_seed
        );
        is_ready = true;
    }
    if (world.gen != last_gen) {
        snprintf(
            text_report_gen.buffer,
            text_report_gen_max + 1,
            "Generation: %u",
            world.gen
        );
        snprintf(
            text_report_live_cell_count.buffer,
            text_report_live_cell_count_max + 1,
            "Live cells: %u",
            live_cell_count
        );
    }
    icon_button_start.is_enabled = !auto_mode;
    icon_button_stop.is_enabled = auto_mode;
    icon_button_step.is_enabled = !auto_mode && animation_tick == 0;
    icon_button_zoom_in.is_enabled = zoom < MAX_ZOOM;
    icon_button_zoom_out.is_enabled = zoom > MIN_ZOOM;
    icon_button_speed_up.is_enabled = speed < MAX_SPEED;
    icon_button_slow_down.is_enabled = speed > MIN_SPEED;
    icon_button_quit.is_enabled = true;
    if (icon_button_quit.is_pressed) {
        is_ready = false;
        last_gen = UINT32_MAX;
        zoom = DEFAULT_ZOOM;
        speed = DEFAULT_SPEED;
        cam_x = 0;
        cam_y = 0;
        is_dragging = false;
        auto_mode = false;
        free(world.tilemap);
        world.tilemap = NULL;
        ux_state = UX_CREATION;
        return true;
    }
    int32 mouse_x, mouse_y;
    uint32 mouse_state = SDL_GetMouseState((int *)&mouse_x, (int *)&mouse_y);
    if ((scroll_y > 0 && zoom < MAX_ZOOM) || (scroll_y < 0 && zoom > MIN_ZOOM)) {
        const uint8 new_zoom = zoom + (scroll_y < 0 ? -ZOOM_SPEED : ZOOM_SPEED);
        const double factor = (double)new_zoom / zoom;
        cam_x = (cam_x - mouse_x) * factor + mouse_x;
        cam_y = (cam_y - mouse_y) * factor + mouse_y;
        zoom = new_zoom;
        snprintf(
            text_zoom.buffer,
            text_zoom_max + 1,
            "%u%%",
            (uint32)zoom
        );
    }
    const uint32 curr_tick = SDL_GetTicks();
    if (icon_button_step.is_pressed) {
        if (!has_acted && animation_tick == 0) {
            advance();
            animation_tick = curr_tick;
        }
        has_acted = true;
    } else if (icon_button_zoom_in.is_pressed) {
        if (!has_acted && zoom < MAX_ZOOM) {
            const uint8 new_zoom = zoom + ZOOM_SPEED;
            const double factor = (double)new_zoom / zoom;
            const int32 center_x = window_w / 2, center_y = window_h / 2;
            cam_x = (cam_x - center_x) * factor + center_x;
            cam_y = (cam_y - center_y) * factor + center_y;
            zoom = new_zoom;
            snprintf(
                text_zoom.buffer,
                text_zoom_max + 1,
                "%u%%",
                (uint32)zoom
            );
        }
        has_acted = true;
    } else if (icon_button_zoom_out.is_pressed) {
        if (!has_acted && zoom > MIN_ZOOM) {
            const uint8 new_zoom = zoom - ZOOM_SPEED;
            const double factor = (double)new_zoom / zoom;
            const int32 center_x = window_w / 2, center_y = window_h / 2;
            cam_x = (cam_x - center_x) * factor + center_x;
            cam_y = (cam_y - center_y) * factor + center_y;
            zoom = new_zoom;
            snprintf(
                text_zoom.buffer,
                text_zoom_max + 1,
                "%u%%",
                (uint32)zoom
            );
        }
        has_acted = true;
    } else if (icon_button_speed_up.is_pressed) {
        if (!has_acted && speed < MAX_SPEED) {
            speed *= 2;
            snprintf(
                text_speed.buffer,
                text_speed_max + 1,
                "%ux",
                (uint32)speed
            );
        }
        has_acted = true;
    } else if (icon_button_slow_down.is_pressed) {
        if (!has_acted && speed > MIN_SPEED) {
            speed /= 2;
            snprintf(
                text_speed.buffer,
                text_speed_max + 1,
                "%ux",
                (uint32)speed
            );
        }
        has_acted = true;
    } else {
        has_acted = false;
    }
    const struct nk_rect panel_controls_pos_rect = panel_controls.pos();
    if (
        mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT) &&
        mouse_x >= 0 &&
        mouse_y >= panel_controls_pos_rect.h &&
        mouse_x < window_w &&
        mouse_y < window_h
    ) {
        if (!is_dragging) {
            is_dragging = true;
            drag_x = mouse_x;
            drag_y = mouse_y;
        } else {
            int32 dx = mouse_x - drag_x, dy = mouse_y - drag_y;
            cam_x += dx;
            cam_y += dy;
            drag_x = mouse_x;
            drag_y = mouse_y;
        }
    } else {
        is_dragging = false;
    }
    if (icon_button_start.is_pressed) {
        auto_mode = true;
    } else if (icon_button_stop.is_pressed) {
        auto_mode = false;
    }
    if (auto_mode && animation_tick == 0) {
        advance();
        animation_tick = curr_tick;
    }
    if (animation_tick != 0 && curr_tick >= animation_tick + ANIMATION_MS / speed) {
        animation_tick = 0;
    }
    const int32
        tile_dst_width = TILE_SRC_WIDTH * zoom / 100,
        tile_dst_height = TILE_SRC_HEIGHT * zoom / 100;
    if (
        window_w > world.w * tile_dst_width ||
        (cam_x > 0 && cam_x < -((int32)world.w * (int32)tile_dst_width - window_w))
    ) {
        cam_x = -((int32)world.w * (int32)tile_dst_width - window_w) / 2;
    } else if (cam_x > 0) {
        cam_x = 0;
    } else if (cam_x < -((int32)world.w * (int32)tile_dst_width - window_w)) {
        cam_x = -((int32)world.w * (int32)tile_dst_width - window_w);
    }
    if (
        window_h - panel_controls_pos_rect.h > world.h * tile_dst_height ||
        (
            cam_y > 0 &&
            cam_y < -(
                (int32)world.h * (int32)tile_dst_height - window_h + panel_controls_pos_rect.h
            )
        )
    ) {
        cam_y = -(
            (int32)world.h * (int32)tile_dst_height - window_h + panel_controls_pos_rect.h
        ) / 2;
    } else if (cam_y > 0) {
        cam_y = 0;
    } else if (
        cam_y < -(
            (int32)world.h * (int32)tile_dst_height - window_h + panel_controls_pos_rect.h
        )
    ) {
        cam_y = -(
            (int32)world.h * (int32)tile_dst_height - window_h + panel_controls_pos_rect.h
        );
    }
    for (uint16 x = 0; x < world.w; ++x) {
        for (uint16 y = 0; y < world.h; ++y) {
            SDL_Rect srcrect, dstrect = {
                .x =
                    (uint32)x * tile_dst_width + cam_x,
                .y =
                    (uint32)y *
                    tile_dst_height +
                    panel_controls_pos_rect.h +
                    cam_y,
                .w =
                    tile_dst_width,
                .h =
                    tile_dst_height
            };
            if (
                dstrect.x + dstrect.w < 0 ||
                dstrect.y + dstrect.h < 0 ||
                dstrect.x >= window_w ||
                dstrect.y >= window_h
            ) {
                continue;
            }
            if (
                SDL_SetRenderDrawColor(
                    renderer,
                    COLOR_CUSTOM_QUALITY.r,
                    COLOR_CUSTOM_QUALITY.g,
                    COLOR_CUSTOM_QUALITY.b,
                    0x02 * (tile_at(x, y).energy < 76 ? tile_at(x, y).energy : 76)
                ) != 0 ||
                SDL_RenderFillRect(renderer, &dstrect) != 0
            ) {
                return false;
            }
            select_animation_frame(
                &srcrect,
                ANIMATION_TILE_NONE,
                0,
                speed
            );
            if (SDL_RenderCopy(renderer, texture, &srcrect, &dstrect) != 0) {
                return false;
            }
            if (tile_at(x, y).cell.energy != 0) {
                select_animation_frame(
                    &srcrect,
                    ANIMATION_CELL_NONE,
                    0,
                    speed
                );
                double angle = 0.0;
                if (animation_tick != 0) {
                    for (uint32 i = 0; i < born_cell_count; ++i) {
                        if (cell_divisions[i].x == x && cell_divisions[i].y == y) {
                            select_animation_frame(
                                &srcrect,
                                ANIMATION_CELL_DIVISION,
                                curr_tick - animation_tick,
                                speed
                            );
                            angle = cell_divisions[i].direction * 90.0;
                            break;
                        } else {
                            uint16
                                look_x = cell_divisions[i].x,
                                look_y = cell_divisions[i].y;
                            bool is_overflowing = false;
                            switch (cell_divisions[i].direction) {
                            case DIRECTION_UP:
                                if (look_y == 0) {
                                    is_overflowing = true;
                                } else {
                                    --look_y;
                                }
                                break;
                            case DIRECTION_RIGHT:
                                if (look_x == world.w - 1) {
                                    is_overflowing = true;
                                } else {
                                    ++look_x;
                                }
                                break;
                            case DIRECTION_DOWN:
                                if (look_y == world.h - 1) {
                                    is_overflowing = true;
                                } else {
                                    ++look_y;
                                }
                                break;
                            case DIRECTION_LEFT:
                                if (look_x == 0) {
                                    is_overflowing = true;
                                } else {
                                    --look_x;
                                }
                                break;
                            case DIRECTION_NONE:
                                is_overflowing = true;
                            }
                            if (!is_overflowing && x == look_x && y == look_y) {
                                select_animation_frame(
                                    &srcrect,
                                    ANIMATION_CELL_BIRTH,
                                    curr_tick - animation_tick,
                                    speed
                                );
                                angle = cell_divisions[i].direction * 90.0;
                                break;
                            }
                        }
                    }
                }
                if (
                    SDL_RenderCopyEx(
                        renderer,
                        texture,
                        &srcrect,
                        &dstrect,
                        angle,
                        NULL,
                        SDL_FLIP_NONE
                    ) != 0
                ) {
                    return false;
                }
            } else if (animation_tick != 0) {
                for (uint32 i = 0; i < dying_cell_count; ++i) {
                    if (cell_deaths[i].x == x && cell_deaths[i].y == y) {
                        select_animation_frame(
                            &srcrect,
                            ANIMATION_CELL_DEATH,
                            curr_tick - animation_tick,
                            speed
                        );
                        if (SDL_RenderCopy(renderer, texture, &srcrect, &dstrect) != 0) {
                            return false;
                        }
                        break;
                    }
                }
            }
        }
    }
    return true;
}

static inline bool tick(void) {
    SDL_GetWindowSize(window, (int *)&window_w, (int *)&window_h);
    scroll_x = 0;
    scroll_y = 0;
    SDL_Event ev;
    nk_input_begin(nk_ctx);
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            is_running = false;
            return true;
        case SDL_MOUSEWHEEL:
            scroll_x = ev.wheel.x;
            scroll_y = ev.wheel.y;
        }
        nk_sdl_handle_event(&ev);
    }
    nk_input_end(nk_ctx);
    do_gui();
    if (
        SDL_SetRenderDrawColor(
            renderer,
            COLOR_BG.r,
            COLOR_BG.g,
            COLOR_BG.b,
            COLOR_BG.a
        ) != 0 ||
        SDL_RenderClear(renderer) != 0
    ) {
        return false;
    }
    switch (ux_state) {
    case UX_CREATION:
        if (!ux_creation()) {
            return false;
        }
        break;
    case UX_GENERATION:
        if (!ux_generation()) {
            return false;
        }
        break;
    case UX_SIM:
        if (!ux_sim()) {
            return false;
        }
        break;
    default:
        return false;
    }
    nk_sdl_render(NK_ANTI_ALIASING_ON);
    SDL_RenderPresent(renderer);
    return true;
}

int main() {
    printf("%s %s - %s\n", TITLE, VERSION, RELEASE_DATE);
    if (!init()) {
        fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    } else if (!start_gui()) {
        return 1;
    }
    while (is_running) {
        if (!tick()) {
            fprintf(stderr, "%s\n", SDL_GetError());
            return 1;
        }
    }
    free(world.tilemap);
    end_gui();
    quit();
    return 0;
}
