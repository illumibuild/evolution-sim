#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>
#include <SDL_image.h>

#include <nuklear.h>
#include <nuklear_sdl_renderer.h>

#define TITLE "evolution-sim"
#define VERSION "v0.2.0 alpha 1 preview"
#define RELEASE_DATE "01/23/2026"

static uint32_t rng_state, rng_seed;

void rng_srand(uint32_t seed) {
    rng_state = rng_seed = seed == 0 ? (uint32_t)time(NULL) : seed;
}

uint32_t rng_rand(void) {
    uint32_t rng_output = (rng_state += 0x9E3779B9);
    rng_output = (rng_output ^ (rng_output >> 16)) * 0x85EBCA6B;
    rng_output = (rng_output ^ (rng_output >> 13)) * 0xC2B2AE35;
    return rng_output ^ (rng_output >> 16);
}

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

#define MIN_WINDOW_WIDTH 853
#define MIN_WINDOW_HEIGHT 480

#define TILE_WIDTH  180
#define TILE_HEIGHT 180

#define ICON_WIDTH  30
#define ICON_HEIGHT 30

#define ANIMATION_MS 500
#define ANIMATION_FPS 8

#define FONT_SIZE 24

static SDL_Window *window;

static SDL_Renderer *renderer;

static struct nk_context *nk_ctx;

static SDL_Texture *texture;

static struct nk_font *font;

static struct nk_font_atlas *font_atlas;

static struct nk_image icons[8];

#define STILL_TILE     0
#define STILL_CELL     1
#define STILL_POINTER 15

static inline void select_still_frame(
    SDL_Rect *srcrect,
    uint8_t atlas_idx
) {
    srcrect->x = atlas_idx * TILE_WIDTH;
    srcrect->y = 0;
}

#define ANIMATION_CELL_TWITCH   1
#define ANIMATION_CELL_DEATH    2
#define ANIMATION_CELL_BIRTH    3
#define ANIMATION_CELL_DIVISION 4

static inline void select_animation_frame(
    SDL_Rect *srcrect,
    uint8_t atlas_id,
    uint32_t tick_diff,
    uint8_t speed,
    uint8_t rotation
) {
    srcrect->x = (tick_diff / (1000 / (ANIMATION_FPS * speed)) + rotation % 4 * 4) * TILE_WIDTH;
    srcrect->y = atlas_id * TILE_HEIGHT;
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

#define TEXTURE_PATH "texture_atlas.png"
#define FONT_PATH    "Ubuntu-R.ttf"

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
    if (!font) {
        return false;
    }
    nk_style_set_font(nk_ctx, &font->handle);
    int32_t texture_w, texture_h;
    if (SDL_QueryTexture(texture, NULL, NULL, &texture_w, &texture_h) != 0) {
        return false;
    }
    for (uint8_t i = 0; i < sizeof(icons) / sizeof(struct nk_image); ++i) {
        icons[i] = nk_subimage_ptr(
            texture,
            texture_w,
            texture_h,
            nk_rect(
                i * ICON_WIDTH,
                texture_h / TILE_HEIGHT * TILE_HEIGHT,
                ICON_WIDTH,
                ICON_HEIGHT
            )
        );
    }
    nk_skin();
    return true;
}

static inline void quit(void) {
    nk_font_atlas_cleanup(font_atlas);
    SDL_DestroyTexture(texture);
    nk_sdl_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

int32_t window_w, window_h;

enum ux_state {
    UX_ANY,
    UX_CREATION,
    UX_GENERATION,
    UX_SIM
};

typedef uint8_t ux_state_t;

static ux_state_t ux_state = UX_CREATION;

enum gui_element_type {
    GUI_LABEL,
    GUI_TEXT,
    GUI_INPUT,
    GUI_BUTTON,
    GUI_ICON_BUTTON,
    GUI_PANEL
};

typedef uint8_t gui_element_type_t;

struct gui_label_element {
    const gui_element_type_t type;
    const ux_state_t ux_state;
    const enum nk_text_align alignment;
    const char *const text;
    struct nk_rect (*const pos)();
};

struct nk_rect label_world_size_pos(void) {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 - 72,
        300,
        24
    );
}

struct nk_rect label_seed_pos(void) {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 + 8,
        300,
        24
    );
}

struct nk_rect label_mul_pos(void) {
    return nk_rect(
        window_w / 2 - 11,
        window_h / 2 - 32,
        22,
        24
    );
}

struct nk_rect label_generating_pos(void) {
    return nk_rect(
        10,
        window_h / 2 - 12,
        window_w - 20,
        24
    );
}

struct nk_rect label_version_pos(void) {
    return nk_rect(
        10,
        window_h - 58,
        window_w - 20,
        24
    );
}

struct nk_rect label_release_date_pos(void) {
    return nk_rect(
        10,
        window_h - 34,
        window_w - 20,
        24
    );
}

struct gui_text_element {
    const gui_element_type_t type;
    const ux_state_t ux_state;
    const uint16_t max;
    const enum nk_text_align alignment;
    char *buffer;
    struct nk_rect (*const pos)();
};

struct nk_rect text_error_pos(void) {
    return nk_rect(
        window_w / 2 - 300,
        window_h / 2 + 200,
        600,
        24
    );
}

struct nk_rect text_zoom_pos(void) {
    return nk_rect(
        260,
        20,
        80,
        24
    );
}

struct nk_rect text_speed_pos(void) {
    return nk_rect(
        500,
        20,
        40,
        24
    );
}

struct nk_rect text_report_pointer_pos_pos(void) {
    return nk_rect(
        10,
        70,
        window_w - 20,
        24
    );
}

struct nk_rect text_report_curr_tile_energy_pos(void) {
    return nk_rect(
        10,
        104,
        window_w - 20,
        24
    );
}

struct nk_rect text_report_curr_cell_age_pos(void) {
    return nk_rect(
        10,
        138,
        window_w - 20,
        24
    );
}

struct nk_rect text_report_curr_cell_energy_pos(void) {
    return nk_rect(
        10,
        172,
        window_w - 20,
        24
    );
}

struct nk_rect text_report_world_size_pos(void) {
    return nk_rect(
        10,
        window_h - 106,
        window_w - 20,
        24
    );
}

struct nk_rect text_report_seed_pos(void) {
    return nk_rect(
        10,
        window_h - 82,
        window_w - 20,
        24
    );
}

struct nk_rect text_report_gen_pos(void) {
    return nk_rect(
        10,
        window_h - 58,
        window_w - 20,
        24
    );
}

struct nk_rect text_report_live_cell_count_pos(void) {
    return nk_rect(
        10,
        window_h - 34,
        window_w - 20,
        24
    );
}

struct gui_input_element {
    const gui_element_type_t type;
    const ux_state_t ux_state;
    const uint16_t max;
    char *buffer;
    nk_bool (*filter)(const struct nk_text_edit *, nk_rune);
    struct nk_rect (*const pos)();
};

struct nk_rect input_world_width_pos(void) {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 - 40,
        134,
        40
    );
}

struct nk_rect input_world_height_pos(void) {
    return nk_rect(
        window_w / 2 + 16,
        window_h / 2 - 40,
        134,
        40
    );
}

struct nk_rect input_seed_pos(void) {
    return nk_rect(
        window_w / 2 - 150,
        window_h / 2 + 40,
        300,
        40
    );
}

struct gui_button_element {
    const gui_element_type_t type;
    const ux_state_t ux_state;
    bool is_enabled, is_pressed;
    const char *const text;
    struct nk_rect (*const pos)();
};

struct nk_rect button_generate_pos(void) {
    return nk_rect(
        window_w / 2 - 100,
        window_h / 2 + 120,
        200,
        40
    );
}

struct gui_icon_button_element {
    const gui_element_type_t type;
    const ux_state_t ux_state;
    bool is_enabled, is_pressed;
    const struct nk_image *const icon;
    struct nk_rect (*const pos)();
};

struct nk_rect icon_button_start_pos(void) {
    return nk_rect(
        10,
        10,
        40,
        40
    );
}

struct nk_rect icon_button_stop_pos(void) {
    return nk_rect(
        60,
        10,
        40,
        40
    );
}

struct nk_rect icon_button_step_pos(void) {
    return nk_rect(
        110,
        10,
        40,
        40
    );
}

struct nk_rect icon_button_zoom_in_pos(void) {
    return nk_rect(
        210,
        10,
        40,
        40
    );
}

struct nk_rect icon_button_zoom_out_pos(void) {
    return nk_rect(
        350,
        10,
        40,
        40
    );
}

struct nk_rect icon_button_speed_up_pos(void) {
    return nk_rect(
        450,
        10,
        40,
        40
    );
}

struct nk_rect icon_button_slow_down_pos(void) {
    return nk_rect(
        550,
        10,
        40,
        40
    );
}

struct nk_rect icon_button_quit_pos(void) {
    return nk_rect(
        window_w - 50,
        10,
        40,
        40
    );
}

struct gui_panel_element {
    const gui_element_type_t type;
    const ux_state_t ux_state;
    const char *const id;
    struct nk_rect (*const pos)();
};

struct nk_rect panel_controls_pos(void) {
    return nk_rect(
        -2,
        -2,
        window_w + 4,
        62
    );
}

union gui_element {
    const gui_element_type_t type;
    struct gui_label_element label_element;
    struct gui_text_element text_element;
    struct gui_input_element input_element;
    struct gui_button_element button_element;
    struct gui_icon_button_element icon_button_element;
    struct gui_panel_element panel_element;
};

static union gui_element gui_elements[] = {
    {
        .label_element = {
            .type = GUI_LABEL,
            .ux_state = UX_CREATION,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .text = "World size (min. 10x10):",
            .pos = label_world_size_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .ux_state = UX_CREATION,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .text = "Seed (leave blank for random):",
            .pos = label_seed_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .ux_state = UX_CREATION,
            .alignment = NK_TEXT_ALIGN_CENTERED,
            .text = "x",
            .pos = label_mul_pos
        }
    },
    {
        .input_element = {
            .type = GUI_INPUT,
            .ux_state = UX_CREATION,
            .max = 5,
            .buffer = NULL,
            .filter = nk_filter_decimal,
            .pos = input_world_width_pos
        }
    },
    {
        .input_element = {
            .type = GUI_INPUT,
            .ux_state = UX_CREATION,
            .max = 5,
            .buffer = NULL,
            .filter = nk_filter_decimal,
            .pos = input_world_height_pos
        }
    },
    {
        .input_element = {
            .type = GUI_INPUT,
            .ux_state = UX_CREATION,
            .max = 19,
            .buffer = NULL,
            .filter = nk_filter_decimal,
            .pos = input_seed_pos
        }
    },
    {
        .button_element = {
            .type = GUI_BUTTON,
            .ux_state = UX_CREATION,
            .is_enabled = false,
            .is_pressed = false,
            .text = "Generate",
            .pos = button_generate_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_CREATION,
            .max = 42,
            .alignment = NK_TEXT_ALIGN_CENTERED,
            .buffer = NULL,
            .pos = text_error_pos
        }
    },
    {
        .label_element = {
            .ux_state = UX_GENERATION,
            .type = GUI_LABEL,
            .alignment = NK_TEXT_ALIGN_CENTERED,
            .text = "Generating world...",
            .pos = label_generating_pos
        }
    },
    {
        .panel_element = {
            .type = GUI_PANEL,
            .ux_state = UX_SIM,
            .id = "controls",
            .pos = panel_controls_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[0],
            .pos = icon_button_start_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[1],
            .pos = icon_button_stop_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[2],
            .pos = icon_button_step_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[3],
            .pos = icon_button_zoom_in_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[4],
            .pos = icon_button_zoom_out_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_SIM,
            .max = 4,
            .alignment = NK_TEXT_ALIGN_CENTERED,
            .buffer = NULL,
            .pos = text_zoom_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[5],
            .pos = icon_button_speed_up_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[6],
            .pos = icon_button_slow_down_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_SIM,
            .max = 2,
            .alignment = NK_TEXT_ALIGN_CENTERED,
            .buffer = NULL,
            .pos = text_speed_pos
        }
    },
    {
        .icon_button_element = {
            .type = GUI_ICON_BUTTON,
            .ux_state = UX_SIM,
            .is_enabled = false,
            .is_pressed = false,
            .icon = &icons[7],
            .pos = icon_button_quit_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_SIM,
            .max = 18,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_pointer_pos_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_SIM,
            .max = 23,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_curr_tile_energy_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_SIM,
            .max = 20,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_curr_cell_age_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_SIM,
            .max = 23,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_curr_cell_energy_pos
        }
    },
    {
        .text_element = {
            .type = GUI_TEXT,
            .ux_state = UX_SIM,
            .max = 56,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_world_size_pos
        }
    },
    {
        .text_element = {
            .ux_state = UX_SIM,
            .type = GUI_TEXT,
            .max = 26,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_seed_pos
        }
    },
    {
        .text_element = {
            .ux_state = UX_SIM,
            .type = GUI_TEXT,
            .max = 32,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_gen_pos
        }
    },
    {
        .text_element = {
            .ux_state = UX_SIM,
            .type = GUI_TEXT,
            .max = 32,
            .alignment = NK_TEXT_ALIGN_LEFT,
            .buffer = NULL,
            .pos = text_report_live_cell_count_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .ux_state = UX_ANY,
            .alignment = NK_TEXT_ALIGN_RIGHT,
            .text = VERSION,
            .pos = label_version_pos
        }
    },
    {
        .label_element = {
            .type = GUI_LABEL,
            .ux_state = UX_ANY,
            .alignment = NK_TEXT_ALIGN_RIGHT,
            .text = RELEASE_DATE,
            .pos = label_release_date_pos
        }
    }
};

#define GUI_ELEMENT_COUNT sizeof(gui_elements) / sizeof(union gui_element)

#define GUI_LABEL_WORLD_SIZE             gui_elements[ 0].label_element
#define GUI_LABEL_SEED                   gui_elements[ 1].label_element
#define GUI_LABEL_MUL                    gui_elements[ 2].label_element
#define GUI_INPUT_WORLD_WIDTH            gui_elements[ 3].input_element
#define GUI_INPUT_WORLD_HEIGHT           gui_elements[ 4].input_element
#define GUI_INPUT_SEED                   gui_elements[ 5].input_element
#define GUI_BUTTON_GENERATE              gui_elements[ 6].button_element
#define GUI_TEXT_ERROR                   gui_elements[ 7].text_element
#define GUI_LABEL_GENERATING             gui_elements[ 8].label_element
#define GUI_PANEL_CONTROLS               gui_elements[ 9].panel_element
#define GUI_ICON_BUTTON_START            gui_elements[10].icon_button_element
#define GUI_ICON_BUTTON_STOP             gui_elements[11].icon_button_element
#define GUI_ICON_BUTTON_STEP             gui_elements[12].icon_button_element
#define GUI_ICON_BUTTON_ZOOM_IN          gui_elements[13].icon_button_element
#define GUI_ICON_BUTTON_ZOOM_OUT         gui_elements[14].icon_button_element
#define GUI_TEXT_ZOOM                    gui_elements[15].text_element
#define GUI_ICON_BUTTON_SPEED_UP         gui_elements[16].icon_button_element
#define GUI_ICON_BUTTON_SLOW_DOWN        gui_elements[17].icon_button_element
#define GUI_TEXT_SPEED                   gui_elements[18].text_element
#define GUI_ICON_BUTTON_QUIT             gui_elements[19].icon_button_element
#define GUI_TEXT_REPORT_POINTER_POS      gui_elements[20].text_element
#define GUI_TEXT_REPORT_CURR_TILE_ENERGY gui_elements[21].text_element
#define GUI_TEXT_REPORT_CURR_CELL_AGE    gui_elements[22].text_element
#define GUI_TEXT_REPORT_CURR_CELL_ENERGY gui_elements[23].text_element
#define GUI_TEXT_REPORT_WORLD_SIZE       gui_elements[24].text_element
#define GUI_TEXT_REPORT_SEED             gui_elements[25].text_element
#define GUI_TEXT_REPORT_GEN              gui_elements[26].text_element
#define GUI_TEXT_REPORT_LIVE_CELL_COUNT  gui_elements[27].text_element
#define GUI_LABEL_VERSION                gui_elements[28].text_element
#define GUI_LABEL_RELEASE_DATE           gui_elements[29].text_element

static inline bool start_gui(void) {
    for (uint8_t i = 0; i < GUI_ELEMENT_COUNT; ++i) {
        switch (gui_elements[i].type) {
        case GUI_TEXT:
            {
                struct gui_text_element *gui_text_element =
                    &gui_elements[i].text_element;
                gui_text_element->buffer = (char *)calloc(gui_text_element->max + 1, 1);
                if (!gui_text_element->buffer) {
                    return false;
                }
            }
            break;
        case GUI_INPUT:
            {
                struct gui_input_element *gui_input_element =
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
        for (uint8_t i = 0; i < GUI_ELEMENT_COUNT; ++i) {
            switch (gui_elements[i].type) {
            case GUI_LABEL:
                {
                    struct gui_label_element *gui_label_element =
                        &gui_elements[i].label_element;
                    if (
                        ux_state != gui_label_element->ux_state &&
                        gui_label_element->ux_state != UX_ANY
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
                    struct gui_text_element *gui_text_element =
                        &gui_elements[i].text_element;
                    if (
                        ux_state != gui_text_element->ux_state &&
                        gui_text_element->ux_state != UX_ANY
                    ) {
                        if (gui_text_element->buffer[0] != '\0') {
                            memset(gui_text_element->buffer, '\0', gui_text_element->max);
                        }
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
                    struct gui_input_element *gui_input_element =
                        &gui_elements[i].input_element;
                    if (
                        ux_state != gui_input_element->ux_state &&
                        gui_input_element->ux_state != UX_ANY
                    ) {
                        if (gui_input_element->buffer[0] != '\0') {
                            memset(gui_input_element->buffer, '\0', gui_input_element->max);
                        }
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
                    struct gui_button_element *gui_button_element =
                        &gui_elements[i].button_element;
                    if (
                        ux_state != gui_button_element->ux_state &&
                        gui_button_element->ux_state != UX_ANY
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
                    struct gui_icon_button_element *gui_icon_button_element =
                        &gui_elements[i].icon_button_element;
                    if (
                        ux_state != gui_icon_button_element->ux_state &&
                        gui_icon_button_element->ux_state != UX_ANY
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
                    struct gui_panel_element *gui_panel_element =
                        &gui_elements[i].panel_element;
                    if (
                        ux_state != gui_panel_element->ux_state &&
                        gui_panel_element->ux_state != UX_ANY
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
    for (uint8_t i = 0; i < GUI_ELEMENT_COUNT; ++i) {
        switch (gui_elements[i].type) {
        case GUI_TEXT:
            {
                struct gui_text_element *gui_text_element =
                    &gui_elements[i].text_element;
                free(gui_text_element->buffer);
            }
            break;
        case GUI_INPUT:
            {
                struct gui_input_element *gui_input_element =
                    &gui_elements[i].input_element;
                free(gui_input_element->buffer);
            }
            break;
        default:;
        }
    }
}

struct cell {
    uint32_t age, energy;
};

enum ev_type {
    EVENT_NONE,
    EVENT_DEATH,
    EVENT_BIRTH,
    EVENT_DIVISION
};

typedef uint8_t ev_type_t;

enum ev_direction {
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_NONE
};

typedef uint8_t ev_direction_t;

struct ev {
    ev_type_t type;
    ev_direction_t direction;
};

struct tile {
    uint32_t energy;
    struct ev ev;
    struct cell cell;
};

struct world {
    uint32_t gen;
    uint16_t w, h;
    struct tile *tilemap;
};

static struct world world;

#define TILE_AT(x, y) world.tilemap[(y) * (uint32_t)world.w + (x)]

#define GENERATION_TILE_INIT_ENERGY_CAP 75
#define GENERATION_TILE_INIT_ENERGY_SUM (77 * 76 / 2)

#define MAX_GENERATION_OPS_PER_TICK 1000000

static bool generate(void) {
    static uint16_t x = 0, y = 0;
    uint32_t operations = 0;
    for (; x < world.w; ++x) {
        for (; y < world.h; ++y) {
            if (++operations > MAX_GENERATION_OPS_PER_TICK) {
                return false;
            }
            struct tile *tile = &TILE_AT(x, y);
            uint32_t base = rng_rand() % GENERATION_TILE_INIT_ENERGY_SUM;
            for (uint32_t i = 0; i <= GENERATION_TILE_INIT_ENERGY_CAP; ++i) {
                if (base <= GENERATION_TILE_INIT_ENERGY_CAP - i) {
                    tile->energy = i;
                    break;
                }
                base -= GENERATION_TILE_INIT_ENERGY_CAP + 1 - i;
            }
            if (rng_rand() % 10 == 0) {
                tile->cell.energy = rng_rand() % 6 + 5;
            }
        }
        y = 0;
    }
    x = 0;
    y = 0;
    return true;
}

uint32_t live_cell_count, dying_cell_count, born_cell_count;

static void advance(void) {
    ++world.gen;
    live_cell_count = 0;
    dying_cell_count = 0;
    born_cell_count = 0;
    for (uint16_t x = 0; x < world.w; ++x) {
        for (uint16_t y = 0; y < world.h; ++y) {
            struct tile *tile = &TILE_AT(x, y);
            tile->ev.type = EVENT_NONE;
            tile->ev.direction = DIRECTION_NONE;
        }
    }
    for (uint16_t x = 0; x < world.w; ++x) {
        for (uint16_t y = 0; y < world.h; ++y) {
            struct tile *tile = &TILE_AT(x, y);
            if (tile->cell.energy == 0) {
                continue;
            }
            ++tile->cell.age;
            const uint8_t
                harvested_energy =
                    (tile->energy >= 100 ? 3 : tile->energy >= 50 ? 2 : 1) +
                    (tile->cell.age >= 40 ? 2 : tile->cell.age >= 20 ? 1 : 0),
                actual_harvested_energy = harvested_energy > tile->energy ?
                    tile->energy : harvested_energy;
            tile->energy -= actual_harvested_energy;
            tile->cell.energy += actual_harvested_energy;
            if (--tile->cell.energy == 0) {
                tile->energy += tile->cell.age;
                ++dying_cell_count;
                tile->ev.type = EVENT_DEATH;
                tile->ev.direction = DIRECTION_NONE;
            } else {
                ++live_cell_count;
            }
            if (tile->cell.energy >= 10 && tile->cell.age >= 10) {
                struct tile *selected_tile = NULL;
                struct tile *adjacent_tiles[4]= {
                    y > 0 ? &TILE_AT(x, y - 1) : NULL,
                    x < world.w - 1 ? &TILE_AT(x + 1, y) : NULL,
                    y < world.h - 1 ? &TILE_AT(x, y + 1) : NULL,
                    x > 0 ? &TILE_AT(x - 1, y) : NULL
                };
                enum ev_direction ev_direction = DIRECTION_NONE;
                for (uint8_t i = 0; i < 4; ++i) {
                    if (
                        adjacent_tiles[i] &&
                        adjacent_tiles[i]->cell.energy == 0 &&
                        (
                            !selected_tile ||
                            adjacent_tiles[i]->energy > selected_tile->energy
                        )
                    ) {
                        selected_tile = adjacent_tiles[i];
                        ev_direction = i;
                    }
                }
                if (selected_tile) {
                    ++born_cell_count;
                    tile->ev.type = EVENT_DIVISION;
                    tile->ev.direction = ev_direction;
                    selected_tile->ev.type = EVENT_BIRTH;
                    selected_tile->ev.direction = ev_direction;
                    selected_tile->cell.age = 0;
                    tile->cell.energy /= 2;
                    selected_tile->cell.energy = tile->cell.energy;
                }
            }
        }
    }
    return;
}

bool is_running = true;

int32_t scroll_x, scroll_y;

static bool ux_creation(void) {
    uintmax_t potential_w = UINTMAX_MAX, potential_h = UINTMAX_MAX;
    if (GUI_INPUT_WORLD_WIDTH.buffer[0] != '\0' && GUI_INPUT_WORLD_HEIGHT.buffer[0] != '\0') {
        errno = 0;
        potential_w = strtoumax(GUI_INPUT_WORLD_WIDTH.buffer, NULL, 10);
        if (errno != 0) {
            return false;
        }
        errno = 0;
        potential_h = strtoumax(GUI_INPUT_WORLD_HEIGHT.buffer, NULL, 10);
        if (errno != 0) {
            return false;
        }
        GUI_BUTTON_GENERATE.is_enabled =
            potential_w >= 10 &&
            potential_w <= UINT16_MAX &&
            potential_h >= 10 &&
            potential_h <= UINT16_MAX;
    } else {
        GUI_BUTTON_GENERATE.is_enabled = false;
    }
    if (GUI_BUTTON_GENERATE.is_pressed) {
        errno = 0;
        const uint32_t seed = GUI_INPUT_SEED.buffer[0] == '\0' ?
            0 : strtoumax(GUI_INPUT_SEED.buffer, NULL, 10);
        if (errno != 0) {
            return false;
        }
        rng_srand(seed);
        world.tilemap = (struct tile *)calloc(
            (uint32_t)potential_w * (uint32_t)potential_h,
            sizeof(struct tile)
        );
        if (!world.tilemap) {
            snprintf(
                GUI_TEXT_ERROR.buffer,
                GUI_TEXT_ERROR.max + 1,
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
        for (uint16_t x = 0; x < world.w; ++x) {
            for (uint16_t y = 0; y < world.h; ++y) {
                if (TILE_AT(x, y).cell.energy != 0) {
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
    static uint32_t last_gen = UINT32_MAX;
    static uint8_t zoom = DEFAULT_ZOOM, speed = DEFAULT_SPEED;
    static int32_t cam_x = 0, cam_y = 0, drag_x = 0, drag_y = 0;
    static uint16_t pointer_x = 0, pointer_y = 0;
    static bool
        is_pointing = false,
        is_dragging = false,
        has_acted = false,
        auto_mode = false;
    static uint32_t animation_tick = 0;
    if (!is_ready) {
        snprintf(
            GUI_TEXT_ZOOM.buffer,
            GUI_TEXT_ZOOM.max + 1,
            "%u%%",
            (uint32_t)DEFAULT_ZOOM
        );
        snprintf(
            GUI_TEXT_SPEED.buffer,
            GUI_TEXT_SPEED.max + 1,
            "%ux",
            (uint32_t)DEFAULT_SPEED
        );
        snprintf(
            GUI_TEXT_REPORT_WORLD_SIZE.buffer,
            GUI_TEXT_REPORT_WORLD_SIZE.max + 1,
            "World size: %ux%u",
            world.w,
            world.h
        );
        snprintf(
            GUI_TEXT_REPORT_SEED.buffer,
            GUI_TEXT_REPORT_SEED.max + 1,
            "Seed: %u",
            rng_seed
        );
        is_ready = true;
    }
    if (world.gen != last_gen) {
        snprintf(
            GUI_TEXT_REPORT_GEN.buffer,
            GUI_TEXT_REPORT_GEN.max + 1,
            "Generation: %u",
            world.gen
        );
        snprintf(
            GUI_TEXT_REPORT_LIVE_CELL_COUNT.buffer,
            GUI_TEXT_REPORT_LIVE_CELL_COUNT.max + 1,
            "Live cells: %u",
            live_cell_count
        );
        last_gen = world.gen;
    }
    GUI_ICON_BUTTON_START.is_enabled = !auto_mode;
    GUI_ICON_BUTTON_STOP.is_enabled = auto_mode;
    GUI_ICON_BUTTON_STEP.is_enabled = !auto_mode && animation_tick == 0;
    GUI_ICON_BUTTON_ZOOM_IN.is_enabled = zoom < MAX_ZOOM;
    GUI_ICON_BUTTON_ZOOM_OUT.is_enabled = zoom > MIN_ZOOM;
    GUI_ICON_BUTTON_SPEED_UP.is_enabled = speed < MAX_SPEED;
    GUI_ICON_BUTTON_SLOW_DOWN.is_enabled = speed > MIN_SPEED;
    GUI_ICON_BUTTON_QUIT.is_enabled = true;
    if (GUI_ICON_BUTTON_QUIT.is_pressed) {
        is_ready = false;
        last_gen = UINT32_MAX;
        zoom = DEFAULT_ZOOM;
        speed = DEFAULT_SPEED;
        cam_x = 0;
        cam_y = 0;
        pointer_x = 0;
        pointer_y = 0;
        is_dragging = false;
        auto_mode = false;
        free(world.tilemap);
        world.tilemap = NULL;
        ux_state = UX_CREATION;
        return true;
    }
    int32_t mouse_x, mouse_y;
    uint32_t mouse_state = SDL_GetMouseState(&mouse_x, &mouse_y);
    const struct nk_rect panel_controls_pos_rect = GUI_PANEL_CONTROLS.pos();
    if (
        mouse_x >= 0 &&
        mouse_y >= panel_controls_pos_rect.h &&
        mouse_x < window_w &&
        mouse_y < window_h &&
        (
            (scroll_y > 0 && zoom < MAX_ZOOM) ||
            (scroll_y < 0 && zoom > MIN_ZOOM)
        )
    ) {
        const uint8_t new_zoom = zoom + (scroll_y < 0 ? -ZOOM_SPEED : ZOOM_SPEED);
        const double factor = (double)new_zoom / zoom;
        cam_x = (cam_x - mouse_x) * factor + mouse_x;
        cam_y =
            (cam_y + panel_controls_pos_rect.h - mouse_y) *
            factor - panel_controls_pos_rect.h + mouse_y;
        zoom = new_zoom;
        snprintf(
            GUI_TEXT_ZOOM.buffer,
            GUI_TEXT_ZOOM.max + 1,
            "%u%%",
            (uint32_t)zoom
        );
    }
    const uint32_t curr_tick = SDL_GetTicks();
    if (GUI_ICON_BUTTON_STEP.is_pressed) {
        if (!has_acted && animation_tick == 0) {
            advance();
            animation_tick = curr_tick;
        }
        has_acted = true;
    } else if (GUI_ICON_BUTTON_ZOOM_IN.is_pressed) {
        if (!has_acted && zoom < MAX_ZOOM) {
            const uint8_t new_zoom = zoom + ZOOM_SPEED;
            const double factor = (double)new_zoom / zoom;
            const int32_t
                center_x = window_w / 2,
                center_y = (window_h - panel_controls_pos_rect.h) / 2;
            cam_x = (cam_x - center_x) * factor + center_x;
            cam_y = (cam_y - center_y) * factor + center_y;
            zoom = new_zoom;
            snprintf(
                GUI_TEXT_ZOOM.buffer,
                GUI_TEXT_ZOOM.max + 1,
                "%u%%",
                (uint32_t)zoom
            );
        }
        has_acted = true;
    } else if (GUI_ICON_BUTTON_ZOOM_OUT.is_pressed) {
        if (!has_acted && zoom > MIN_ZOOM) {
            const uint8_t new_zoom = zoom - ZOOM_SPEED;
            const double factor = (double)new_zoom / zoom;
            const int32_t
                center_x = window_w / 2,
                center_y = (window_h - panel_controls_pos_rect.h) / 2;
            cam_x = (cam_x - center_x) * factor + center_x;
            cam_y = (cam_y - center_y) * factor + center_y;
            zoom = new_zoom;
            snprintf(
                GUI_TEXT_ZOOM.buffer,
                GUI_TEXT_ZOOM.max + 1,
                "%u%%",
                (uint32_t)zoom
            );
        }
        has_acted = true;
    } else if (GUI_ICON_BUTTON_SPEED_UP.is_pressed) {
        if (!has_acted && speed < MAX_SPEED) {
            speed *= 2;
            snprintf(
                GUI_TEXT_SPEED.buffer,
                GUI_TEXT_SPEED.max + 1,
                "%ux",
                (uint32_t)speed
            );
        }
        has_acted = true;
    } else if (GUI_ICON_BUTTON_SLOW_DOWN.is_pressed) {
        if (!has_acted && speed > MIN_SPEED) {
            speed /= 2;
            snprintf(
                GUI_TEXT_SPEED.buffer,
                GUI_TEXT_SPEED.max + 1,
                "%ux",
                (uint32_t)speed
            );
        }
        has_acted = true;
    } else {
        has_acted = false;
    }
    const int32_t
        tile_w = TILE_WIDTH * zoom / 100,
        tile_h = TILE_HEIGHT * zoom / 100;
    if (
        mouse_x >= 0 &&
        mouse_y >= panel_controls_pos_rect.h &&
        mouse_x < window_w &&
        mouse_y < window_h
    ) {
        const int32_t
            disp_x = mouse_x - cam_x,
            disp_y = mouse_y - panel_controls_pos_rect.h - cam_y;
        if (
            disp_x >= 0 &&
            disp_y >= 0 &&
            disp_x < world.w * tile_w &&
            disp_y < world.h * tile_h
        ) {
            pointer_x = disp_x / tile_w;
            pointer_y = disp_y / tile_h;
            is_pointing = true;
            snprintf(
                GUI_TEXT_REPORT_POINTER_POS.buffer,
                GUI_TEXT_REPORT_POINTER_POS.max + 1,
                "X: %u; Y: %u",
                (uint32_t)pointer_x,
                (uint32_t)pointer_y
            );
            struct tile *tile = &TILE_AT(pointer_x, pointer_y);
            snprintf(
                GUI_TEXT_REPORT_CURR_TILE_ENERGY.buffer,
                GUI_TEXT_REPORT_CURR_TILE_ENERGY.max + 1,
                "Tile energy: %u",
                (uint32_t)tile->energy
            );
            if (tile->cell.energy != 0) {
                snprintf(
                    GUI_TEXT_REPORT_CURR_CELL_AGE.buffer,
                    GUI_TEXT_REPORT_CURR_CELL_AGE.max + 1,
                    "Cell age: %u",
                    (uint32_t)tile->cell.age
                );
                snprintf(
                    GUI_TEXT_REPORT_CURR_CELL_ENERGY.buffer,
                    GUI_TEXT_REPORT_CURR_CELL_ENERGY.max + 1,
                    "Cell energy: %u",
                    (uint32_t)tile->cell.energy
                );
            } else {
                memset(
                    GUI_TEXT_REPORT_CURR_CELL_AGE.buffer,
                    '\0',
                    GUI_TEXT_REPORT_CURR_CELL_AGE.max
                );
                memset(
                    GUI_TEXT_REPORT_CURR_CELL_ENERGY.buffer,
                    '\0',
                    GUI_TEXT_REPORT_CURR_CELL_ENERGY.max
                );
            }
        } else {
            is_pointing = false;
        }
    } else {
        is_pointing = false;
    }
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
            int32_t dx = mouse_x - drag_x, dy = mouse_y - drag_y;
            cam_x += dx;
            cam_y += dy;
            drag_x = mouse_x;
            drag_y = mouse_y;
        }
    } else {
        is_dragging = false;
    }
    if (GUI_ICON_BUTTON_START.is_pressed) {
        auto_mode = true;
    } else if (GUI_ICON_BUTTON_STOP.is_pressed) {
        auto_mode = false;
    }
    if (auto_mode && animation_tick == 0) {
        advance();
        animation_tick = curr_tick;
    }
    if (animation_tick != 0 && curr_tick >= animation_tick + ANIMATION_MS / speed) {
        animation_tick = 0;
    }
    if (
        window_w > world.w * tile_w ||
        (cam_x > 0 && cam_x < -((int32_t)world.w * (int32_t)tile_w - window_w))
    ) {
        cam_x = -((int32_t)world.w * (int32_t)tile_w - window_w) / 2;
    } else if (cam_x > 0) {
        cam_x = 0;
    } else if (cam_x < -((int32_t)world.w * (int32_t)tile_w - window_w)) {
        cam_x = -((int32_t)world.w * (int32_t)tile_w - window_w);
    }
    if (
        window_h - panel_controls_pos_rect.h > world.h * tile_h ||
        (
            cam_y > 0 &&
            cam_y < -(
                (int32_t)world.h * (int32_t)tile_h - window_h + panel_controls_pos_rect.h
            )
        )
    ) {
        cam_y = -(
            (int32_t)world.h * (int32_t)tile_h - window_h + panel_controls_pos_rect.h
        ) / 2;
    } else if (cam_y > 0) {
        cam_y = 0;
    } else if (
        cam_y < -((int32_t)world.h * (int32_t)tile_h - window_h + panel_controls_pos_rect.h)
    ) {
        cam_y = -((int32_t)world.h * (int32_t)tile_h - window_h + panel_controls_pos_rect.h);
    }
    SDL_Rect
        srcrect = { .w = TILE_WIDTH, .h = TILE_HEIGHT },
        dstrect = { .w = tile_w,     .h = tile_h      };
    for (uint16_t x = 0; x < world.w; ++x) {
        for (uint16_t y = 0; y < world.h; ++y) {
            dstrect.x = (int32_t)x * tile_w + cam_x;
            dstrect.y = (int32_t)y * tile_h + panel_controls_pos_rect.h + cam_y;
            if (
                dstrect.x + dstrect.w < 0 ||
                dstrect.y + dstrect.h < 0 ||
                dstrect.x >= window_w ||
                dstrect.y >= window_h
            ) {
                continue;
            }
            struct tile *tile = &TILE_AT(x, y);
            if (
                SDL_SetRenderDrawColor(
                    renderer,
                    COLOR_CUSTOM_QUALITY.r,
                    COLOR_CUSTOM_QUALITY.g,
                    COLOR_CUSTOM_QUALITY.b,
                    0x02 * (
                        tile->energy <= GENERATION_TILE_INIT_ENERGY_CAP ?
                        tile->energy : GENERATION_TILE_INIT_ENERGY_CAP + 1
                    )
                ) != 0 ||
                SDL_RenderFillRect(renderer, &dstrect) != 0
            ) {
                return false;
            }
            select_still_frame(
                &srcrect,
                STILL_TILE
            );
            if (SDL_RenderCopy(renderer, texture, &srcrect, &dstrect) != 0) {
                return false;
            }
            select_still_frame(
                &srcrect,
                STILL_CELL
            );
            bool to_draw = tile->cell.energy != 0;
            if (animation_tick != 0) {
                switch (tile->ev.type) {
                case EVENT_NONE:
                    select_animation_frame(
                        &srcrect,
                        ANIMATION_CELL_TWITCH,
                        curr_tick - animation_tick,
                        speed,
                        DIRECTION_NONE
                    );
                    break;
                case EVENT_DEATH:
                    select_animation_frame(
                        &srcrect,
                        ANIMATION_CELL_DEATH,
                        curr_tick - animation_tick,
                        speed,
                        DIRECTION_NONE
                    );
                    to_draw = true;
                    break;
                case EVENT_BIRTH:
                    select_animation_frame(
                        &srcrect,
                        ANIMATION_CELL_BIRTH,
                        curr_tick - animation_tick,
                        speed,
                        tile->ev.direction
                    );
                    break;
                case EVENT_DIVISION:
                    select_animation_frame(
                        &srcrect,
                        ANIMATION_CELL_DIVISION,
                        curr_tick - animation_tick,
                        speed,
                        tile->ev.direction
                    );
                }
            }
            if (to_draw && SDL_RenderCopy(renderer, texture, &srcrect, &dstrect) != 0) {
                return false;
            }
        }
    }
    if (!is_pointing) {
        return true;
    }
    dstrect.x = (int32_t)pointer_x * tile_w + cam_x;
    dstrect.y = (int32_t)pointer_y * tile_h + panel_controls_pos_rect.h + cam_y;
    if (
        dstrect.x + dstrect.w >= 0 &&
        dstrect.y + dstrect.h >= 0 &&
        dstrect.x < window_w &&
        dstrect.y < window_h
    ) {
        select_still_frame(
            &srcrect,
            STILL_POINTER
        );
        if (SDL_RenderCopy(renderer, texture, &srcrect, &dstrect) != 0) {
            return false;
        }
    }
    return true;
}

static inline bool tick(void) {
    SDL_GetWindowSize(window, &window_w, &window_h);
    scroll_x = 0;
    scroll_y = 0;
    SDL_Event ev;
    nk_input_begin(nk_ctx);
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            nk_input_end(nk_ctx);
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
        quit();
        return 1;
    }
    while (is_running) {
        if (!tick()) {
            fprintf(stderr, "%s\n", SDL_GetError());
            end_gui();
            quit();
            return 1;
        }
    }
    free(world.tilemap);
    end_gui();
    quit();
    return 0;
}
