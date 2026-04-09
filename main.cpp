#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <ctime>
#include <format>
#include <iostream>
#include <limits>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>

#include <nuklear.h>
#include <nuklear_sdl_renderer.h>

constexpr const char
    *TITLE        = "evolution-sim",
    *VERSION      = "v0.2.1 preview",
    *RELEASE_DATE = "04/09/2026";

namespace rng {
    std::uint32_t state, seed;
    void srand(std::uint32_t new_seed) noexcept {
        state = seed = new_seed;
    }
    std::uint32_t rand() noexcept {
        std::uint32_t output = (state += 0x9E3779B9);
        output = (output ^ (output >> 16)) * 0x85EBCA6B;
        output = (output ^ (output >> 13)) * 0xC2B2AE35;
        return output ^ (output >> 16);
    }
    std::uint32_t rand(std::uint32_t max) noexcept {
        return rand() % max;
    }
    bool chance(std::uint32_t denominator) noexcept {
        return rand() % denominator == 0;
    }
}

constexpr std::int32_t
    WINDOW_WIDTH      = 1280,
    WINDOW_HEIGHT     =  720,
    MIN_WINDOW_WIDTH  =  853,
    MIN_WINDOW_HEIGHT =  480;

constexpr std::int32_t
    TILE_WIDTH = 180, TILE_HEIGHT = 180, ICON_WIDTH = 20, ICON_HEIGHT = 20;

constexpr std::uint16_t ANIMATION_MS = 500, ANIMATION_FPS = 8;

constexpr std::int32_t FONT_SIZE = 24;

constexpr const char
    *TEXTURE_PATH = "texture_atlas.png",
    *FONT_PATH = "Ubuntu-R.ttf";

constexpr struct nk_color
    COLOR_BG        { .r = 0x8C, .g = 0x00, .b = 0x3F, .a = 0xFF },
    COLOR_FG        { .r = 0xFF, .g = 0x4B, .b = 0x87, .a = 0xFF },
    COLOR_INACTIVE  { .r = 0xB4, .g = 0x00, .b = 0x51, .a = 0xFF },
    COLOR_ACTIVE    { .r = 0x78, .g = 0x00, .b = 0x36, .a = 0xFF },
    COLOR_TRIGGER   { .r = 0x50, .g = 0x00, .b = 0x24, .a = 0xFF },
    COLOR_BORDER    { .r = 0xC8, .g = 0x00, .b = 0x5A, .a = 0xFF },
    COLOR_CUSTOM_QOL{ .r = 0x39, .g = 0xFF, .b = 0x14, .a = 0xFF };

class Context {
    bool is_inited;
public:
    SDL_Window *window;
    SDL_Renderer *renderer;
    struct nk_context *nk_ctx;
    SDL_Texture *texture;
    struct nk_font *font;
    struct nk_font_atlas *font_atlas;
    std::array<struct nk_image, 8> icons;
    struct nk_style_button misc_style_button_disabled;
    std::int32_t window_w, window_h, scroll_x, scroll_y;
    Context() :
        is_inited{ false },
        window{ nullptr },
        renderer{ nullptr },
        nk_ctx{ nullptr },
        texture{ nullptr },
        font{ nullptr },
        font_atlas{ nullptr },
        icons{},
        misc_style_button_disabled{},
        window_w{ 0 },
        window_h{ 0 },
        scroll_x{ 0 },
        scroll_y{ 0 }
    {
        if (
            SDL_Init(SDL_INIT_VIDEO) != 0 ||
            !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)
        ) {
            return;
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
            return;
        }
        SDL_SetWindowMinimumSize(window, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
        renderer = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );
        if (!renderer) {
            return;
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        nk_ctx = nk_sdl_init(window, renderer);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
        SDL_Surface *surface = IMG_Load(TEXTURE_PATH);
        if (!surface) {
            return;
        }
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) {
            return;
        }
        nk_sdl_font_stash_begin(&font_atlas);
        font = nk_font_atlas_add_from_file(
            font_atlas,
            FONT_PATH,
            FONT_SIZE,
            nullptr
        );
        nk_sdl_font_stash_end();
        if (!font) {
            return;
        }
        nk_style_set_font(nk_ctx, &font->handle);
        std::int32_t texture_w, texture_h;
        if (SDL_QueryTexture(texture, nullptr, nullptr, &texture_w, &texture_h) != 0) {
            return;
        }
        for (std::uint8_t i = 0; i < icons.size(); ++i) {
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
        nk_ctx->style.button.image_padding = nk_vec2(-2, -2);
        misc_style_button_disabled = nk_ctx->style.button;
        misc_style_button_disabled.normal.data.color = COLOR_TRIGGER;
        misc_style_button_disabled.hover.data.color = COLOR_TRIGGER;
        misc_style_button_disabled.active.data.color = COLOR_TRIGGER;
        is_inited = true;
    }
    ~Context() noexcept {
        if (!is_inited) {
            return;
        }
        nk_font_atlas_cleanup(font_atlas);
        SDL_DestroyTexture(texture);
        nk_sdl_shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
    operator bool() const {
        return is_inited;
    }
    bool render(SDL_Rect &srcrect, SDL_Rect &dstrect) {
        return SDL_RenderCopy(renderer, texture, &srcrect, &dstrect) == 0;
    }
};

Context ctx;

namespace Still {
    enum : std::uint8_t {
        Tile,
        Cell,
        Cursor,
        Pointer
    };
}

void select_still_frame(
    SDL_Rect &srcrect,
    std::uint8_t atlas_idx
) {
    srcrect.x = atlas_idx * TILE_WIDTH;
    srcrect.y = 0;
}

namespace Animation {
    enum : std::uint8_t {
        Twitch = 1,
        Death,
        Pulse,
        Synthesize,
        SpawnUp,
        SpawnDown,
        SpawnLeft,
        SpawnRight,
        DivideUp,
        DivideDown,
        DivideLeft,
        DivideRight,
        MoveToUp,
        MoveToDown,
        MoveToLeft,
        MoveToRight,
        MoveFromUp,
        MoveFromDown,
        MoveFromLeft,
        MoveFromRight,
        SynthesizeAndMoveToUp,
        SynthesizeAndMoveToDown,
        SynthesizeAndMoveToLeft,
        SynthesizeAndMoveToRight,
        SynthesizeAndMoveFromUp,
        SynthesizeAndMoveFromDown,
        SynthesizeAndMoveFromLeft,
        SynthesizeAndMoveFromRight
    };
}

void select_animation_frame(
    SDL_Rect &srcrect,
    std::uint8_t atlas_id,
    std::uint32_t tick_diff,
    std::uint8_t speed
) {
    static std::uint32_t
        last_tick_diff = std::numeric_limits<std::uint32_t>::max(),
        frame,
        last_speed = 0,
        spf;
    if (speed != last_speed) {
        last_speed = speed;
        spf = (1000 / (ANIMATION_FPS * speed) + !!(1000 % (ANIMATION_FPS * speed)));
    }
    if (tick_diff != last_tick_diff) {
        last_tick_diff = tick_diff;
        frame = tick_diff / spf;
    }
    srcrect.x = frame * TILE_WIDTH;
    srcrect.y = atlas_id * TILE_HEIGHT;
}

enum class UXState : std::uint8_t {
    Any,
    Creation,
    Generation,
    Sim
};

UXState active_ux_state = UXState::Creation;

using Position = struct nk_rect (*)();

class GUIElement {
public:
    enum class Type : std::uint8_t {
        Text,
        Input,
        Button,
        IconButton,
        Panel
    };
    Type type;
    UXState ux_state;
    Position pos;
    GUIElement(Type type, UXState ux_state, const Position &pos) :
        type{ type },
        ux_state{ ux_state },
        pos{ pos }
    {}
    virtual ~GUIElement() = default;
    virtual void render() = 0;
};

class GUITextElement : public GUIElement {
public:
    enum nk_text_align alignment;
    std::string text;
    GUITextElement(
        UXState ux_state,
        const Position &pos,
        enum nk_text_align alignment,
        const std::string &text = {}
    ) :
        GUIElement{ Type::Text, ux_state, pos },
        alignment{ alignment },
        text{ text }
    {}
    void render() override {
        nk_text(ctx.nk_ctx, text.c_str(), text.length(), alignment);
    }
};

class GUIInputElement : public GUIElement {
public:
    std::uint16_t max;
    std::vector<char> buffer;
    nk_bool (*filter)(const struct nk_text_edit *, nk_rune);
    GUIInputElement(
        UXState ux_state,
        const Position &pos,
        std::uint16_t max,
        nk_bool (*filter)(const struct nk_text_edit *, nk_rune)
    ) :
        GUIElement{ Type::Input, ux_state, pos },
        max{ max },
        buffer(max + 1, '\0'),
        filter{ filter }
    {}
    void render() override {
        nk_edit_string_zero_terminated(
            ctx.nk_ctx, NK_EDIT_FIELD, buffer.data(), max + 1, filter
        );
    }
    void clear() {
        std::fill(buffer.begin(), buffer.end(), '\0');
    }
    std::string get_value() const {
        return std::string(buffer.data());
    }
};

class GUIButtonElement : public GUIElement {
public:
    bool is_enabled, is_pressed;
    std::string text;
    GUIButtonElement(
        UXState ux_state,
        const Position &pos,
        const std::string &text
    ) :
        GUIElement{ Type::Button, ux_state, pos },
        is_enabled{ false },
        is_pressed{ false },
        text{ text }
    {}
    void render() override {
        if (is_enabled) {
            is_pressed = nk_button_label(ctx.nk_ctx, text.c_str());
        } else {
            is_pressed = false;
            nk_button_label_styled(
                ctx.nk_ctx, &ctx.misc_style_button_disabled, text.c_str()
            );
        }
    }
};

class GUIIconButtonElement : public GUIElement {
public:
    bool is_enabled, is_pressed;
    const struct nk_image &icon;
    GUIIconButtonElement(
        UXState ux_state,
        const Position &pos,
        const struct nk_image &icon
    ) :
        GUIElement{ Type::IconButton, ux_state, pos },
        is_enabled{ false },
        is_pressed{ false },
        icon{ icon }
    {}
    void render() override {
        if (is_enabled) {
            is_pressed = nk_button_image(ctx.nk_ctx, icon);
        } else {
            is_pressed = false;
            nk_button_image_styled(
                ctx.nk_ctx, &ctx.misc_style_button_disabled, icon
            );
        }
    }
};

class GUIPanelElement : public GUIElement {
public:
    GUIPanelElement(
        UXState ux_state,
        const Position &pos
    ) :
        GUIElement{ Type::Panel, ux_state, pos }
    {}
    void render() override {
        nk_fill_rect(nk_window_get_canvas(ctx.nk_ctx), pos(), 0, COLOR_BG);
        nk_stroke_rect(nk_window_get_canvas(ctx.nk_ctx), pos(), 0, 1, COLOR_BORDER);
    }
};

namespace gui {
    constexpr std::int32_t
        ELEMENT_MARGIN = 10,
        TEXT_ELEMENT_HEIGHT = 24,
        INPUT_ELEMENT_HEIGHT = 40,
        BUTTON_ELEMENT_HEIGHT = 40,
        ICON_BUTTON_ELEMENT_WIDTH = 30,
        ICON_BUTTON_ELEMENT_HEIGHT = 30,
        PANEL_ELEMENT_OUTLINE = 2;
    extern GUITextElement text_mul, text_zoom, text_speed;
    extern GUIPanelElement panel_controls;
    GUITextElement
        text_world_size{
            UXState::Creation,
            []() -> struct nk_rect {
                return nk_rect(
                    ctx.window_w / 2 - 150,
                    ctx.window_h / 2 - 3 * ELEMENT_MARGIN / 2 -
                    INPUT_ELEMENT_HEIGHT - TEXT_ELEMENT_HEIGHT,
                    300,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT,
            "World size (min. 10x10):"
        },
        text_seed{
            UXState::Creation,
            []() -> struct nk_rect {
                return nk_rect(
                    ctx.window_w / 2 - 150,
                    ctx.window_h / 2 + ELEMENT_MARGIN / 2,
                    300,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT,
            "Seed (leave blank for random):"
        },
        text_mul{
            UXState::Creation,
            []() -> struct nk_rect {
                return nk_rect(
                    ctx.window_w / 2 - 11,
                    ctx.window_h / 2 - ELEMENT_MARGIN / 2 -
                    (TEXT_ELEMENT_HEIGHT + INPUT_ELEMENT_HEIGHT) / 2,
                    22,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_CENTERED,
            "x"
        },
        text_error{
            UXState::Creation,
            []() -> struct nk_rect {
                return nk_rect(
                    ctx.window_w / 2 - 300,
                    ctx.window_h / 2 + 11 * ELEMENT_MARGIN / 2 +
                    3 * TEXT_ELEMENT_HEIGHT + INPUT_ELEMENT_HEIGHT +
                    BUTTON_ELEMENT_HEIGHT,
                    600,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_CENTERED
        },
        text_generating{
            UXState::Generation,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ctx.window_h / 2 - TEXT_ELEMENT_HEIGHT / 2,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_CENTERED
        },
        text_zoom{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    6 * ELEMENT_MARGIN + 5 * ICON_BUTTON_ELEMENT_WIDTH,
                    3 * ELEMENT_MARGIN / 2,
                    60,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_CENTERED
        },
        text_speed{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect text_zoom_rect = text_zoom.pos();
                return nk_rect(
                    10 * ELEMENT_MARGIN + 8 * ICON_BUTTON_ELEMENT_WIDTH +
                    text_zoom_rect.w,
                    3 * ELEMENT_MARGIN / 2,
                    30,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_CENTERED
        },
        text_report_ptr_pos{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect panel_controls_rect = panel_controls.pos();
                return nk_rect(
                    ELEMENT_MARGIN,
                    panel_controls_rect.y + panel_controls_rect.h +
                    ELEMENT_MARGIN,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_curr_tile_energy{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect panel_controls_rect = panel_controls.pos();
                return nk_rect(
                    ELEMENT_MARGIN,
                    panel_controls_rect.y + panel_controls_rect.h +
                    ELEMENT_MARGIN + 2 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_curr_cell_age{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect panel_controls_rect = panel_controls.pos();
                return nk_rect(
                    ELEMENT_MARGIN,
                    panel_controls_rect.y + panel_controls_rect.h +
                    ELEMENT_MARGIN + 3 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_curr_cell_energy{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect panel_controls_rect = panel_controls.pos();
                return nk_rect(
                    ELEMENT_MARGIN,
                    panel_controls_rect.y + panel_controls_rect.h +
                    ELEMENT_MARGIN + 4 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_curr_cell_undergone_evolutions{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect panel_controls_rect = panel_controls.pos();
                return nk_rect(
                    ELEMENT_MARGIN,
                    panel_controls_rect.y + panel_controls_rect.h +
                    ELEMENT_MARGIN + 6 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_curr_cell_ongoing_evolution{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect panel_controls_rect = panel_controls.pos();
                return nk_rect(
                    ELEMENT_MARGIN,
                    panel_controls_rect.y + panel_controls_rect.h +
                    ELEMENT_MARGIN + 8 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_world_size{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ctx.window_h - ELEMENT_MARGIN - 4 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_seed{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ctx.window_h - ELEMENT_MARGIN - 3 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_gen{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ctx.window_h - ELEMENT_MARGIN - 2 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_report_live_cell_count{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ctx.window_h - ELEMENT_MARGIN - TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_LEFT
        },
        text_version{
            UXState::Any,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ctx.window_h - ELEMENT_MARGIN - 2 * TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_RIGHT,
            VERSION
        },
        text_release_date{
            UXState::Any,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ctx.window_h - ELEMENT_MARGIN - TEXT_ELEMENT_HEIGHT,
                    ctx.window_w - 2 * ELEMENT_MARGIN,
                    TEXT_ELEMENT_HEIGHT
                );
            },
            NK_TEXT_ALIGN_RIGHT,
            RELEASE_DATE
        };
    GUIInputElement
        input_world_w{
            UXState::Creation,
            []() -> struct nk_rect {
                const struct nk_rect text_mul_rect = text_mul.pos();
                return nk_rect(
                    ctx.window_w / 2 - 150,
                    ctx.window_h / 2 - ELEMENT_MARGIN / 2 -
                    INPUT_ELEMENT_HEIGHT,
                    300 / 2 - text_mul_rect.w / 2 - ELEMENT_MARGIN / 2,
                    INPUT_ELEMENT_HEIGHT
                );
            },
            5,
            nk_filter_decimal
        },
        input_world_h{
            UXState::Creation,
            []() -> struct nk_rect {
                const struct nk_rect text_mul_rect = text_mul.pos();
                return nk_rect(
                    ctx.window_w / 2 + 16,
                    ctx.window_h / 2 - ELEMENT_MARGIN / 2 -
                    INPUT_ELEMENT_HEIGHT,
                    300 / 2 - text_mul_rect.w / 2 - ELEMENT_MARGIN / 2,
                    INPUT_ELEMENT_HEIGHT
                );
            },
            5,
            nk_filter_decimal
        },
        input_seed{
            UXState::Creation,
            []() -> struct nk_rect {
                return nk_rect(
                    ctx.window_w / 2 - 150,
                    ctx.window_h / 2 + 3 * ELEMENT_MARGIN / 2 +
                    TEXT_ELEMENT_HEIGHT,
                    300,
                    INPUT_ELEMENT_HEIGHT
                );
            },
            10,
            nk_filter_decimal
        };
    GUIButtonElement btn_generate{
        UXState::Creation,
        []() -> struct nk_rect {
            return nk_rect(
                ctx.window_w / 2 - 100,
                ctx.window_h / 2 + 7 * ELEMENT_MARGIN / 2 +
                2 * ELEMENT_MARGIN + INPUT_ELEMENT_HEIGHT,
                200,
                BUTTON_ELEMENT_HEIGHT
            );
        },
        "Generate"
    };
    GUIIconButtonElement
        icon_btn_start{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    ELEMENT_MARGIN,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[0]
        },
        icon_btn_stop{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    2 * ELEMENT_MARGIN + ICON_BUTTON_ELEMENT_WIDTH,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[1]
        },
        icon_btn_step{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    3 * ELEMENT_MARGIN + 2 * ICON_BUTTON_ELEMENT_WIDTH,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[2]
        },
        icon_btn_zoom_in{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    5 * ELEMENT_MARGIN + 4 * ICON_BUTTON_ELEMENT_WIDTH,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[3]
        },
        icon_btn_zoom_out{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect text_zoom_rect = text_zoom.pos();
                return nk_rect(
                    7 * ELEMENT_MARGIN + 5 * ICON_BUTTON_ELEMENT_WIDTH +
                    text_zoom_rect.w,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[4]
        },
        icon_btn_speed_up{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect text_zoom_rect = text_zoom.pos();
                return nk_rect(
                    9 * ELEMENT_MARGIN + 7 * ICON_BUTTON_ELEMENT_WIDTH +
                    text_zoom_rect.w,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[5]
        },
        icon_btn_slow_down{
            UXState::Sim,
            []() -> struct nk_rect {
                const struct nk_rect
                    text_zoom_rect = text_zoom.pos(),
                    text_speed_rect = text_speed.pos();
                return nk_rect(
                    11 * ELEMENT_MARGIN + 8 * ICON_BUTTON_ELEMENT_WIDTH +
                    text_zoom_rect.w + text_speed_rect.w,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[6]
        },
        icon_btn_quit{
            UXState::Sim,
            []() -> struct nk_rect {
                return nk_rect(
                    ctx.window_w - ELEMENT_MARGIN -
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ELEMENT_MARGIN,
                    ICON_BUTTON_ELEMENT_WIDTH,
                    ICON_BUTTON_ELEMENT_HEIGHT
                );
            },
            ctx.icons[7]
        };
    GUIPanelElement panel_controls{
        UXState::Sim,
        []() -> struct nk_rect {
            return nk_rect(
                -PANEL_ELEMENT_OUTLINE,
                -PANEL_ELEMENT_OUTLINE,
                ctx.window_w + 2 * PANEL_ELEMENT_OUTLINE,
                ICON_BUTTON_ELEMENT_HEIGHT + 2 * ELEMENT_MARGIN +
                2 * PANEL_ELEMENT_OUTLINE
            );
        }
    };
    std::array<std::reference_wrapper<GUIElement>, 32> elements{{
        text_world_size,
        text_seed,
        text_mul,
        input_world_w,
        input_world_h,
        input_seed,
        btn_generate,
        text_error,
        text_generating,
        panel_controls,
        icon_btn_start,
        icon_btn_stop,
        icon_btn_step,
        icon_btn_zoom_in,
        icon_btn_zoom_out,
        text_zoom,
        icon_btn_speed_up,
        icon_btn_slow_down,
        text_speed,
        icon_btn_quit,
        text_report_ptr_pos,
        text_report_curr_tile_energy,
        text_report_curr_cell_age,
        text_report_curr_cell_energy,
        text_report_curr_cell_undergone_evolutions,
        text_report_curr_cell_ongoing_evolution,
        text_report_world_size,
        text_report_seed,
        text_report_gen,
        text_report_live_cell_count,
        text_version,
        text_release_date
    }};
    void render() {
        if (
            nk_begin(
                ctx.nk_ctx,
                "",
                nk_rect(0, 0, ctx.window_w, ctx.window_h),
                NK_WINDOW_NO_SCROLLBAR
            )
        ) {
            nk_layout_space_begin(
                ctx.nk_ctx,
                NK_STATIC,
                ctx.window_h,
                elements.size()
            );
            for (GUIElement &element : elements) {
                if (
                    active_ux_state != element.ux_state &&
                    element.ux_state != UXState::Any
                ) {
                    continue;
                }
                nk_layout_space_push(ctx.nk_ctx, element.pos());
                element.render();
            }
            nk_layout_space_end(ctx.nk_ctx);
        }
        nk_end(ctx.nk_ctx);
    }
}

template <std::uint8_t width>
struct Info {
    std::bitset<width> data;
    bool operator[](std::uint8_t indice) const {
        return data[indice];
    }
    Info &operator+=(std::uint8_t indice) {
        data[indice] = true;
        return *this;
    }
    Info &operator-=(std::uint8_t indice) {
        data[indice] = false;
        return *this;
    }
    bool none() const {
        return data.count() == 0;
    }
    template <typename... Ignore>
    bool any(Ignore ...ignore) const {
        std::array<std::uint8_t, sizeof...(ignore)> ignored_indices{
            static_cast<std::uint8_t>(ignore)...
        };
        std::bitset<width> copy = data;
        for (std::uint8_t ignored_indice : ignored_indices) {
            copy[ignored_indice] = false;
        }
        return copy.count() != 0;
    }
    Info &clear() {
        data = 0;
        return *this;
    }
};

struct Evolution {
    enum : std::uint8_t {
        Motility,
        Polydivision,
        Energosynthesis,
        COUNT
    };
    std::string name;
    std::uint32_t eligibility, cost, timescale;
    std::uint16_t acq_prob, loss_prob;
};

constexpr std::array<Evolution, 3> EVOLUTIONS{{
    {
        .name = "Motility",
        .eligibility = 20,
        .cost = 20,
        .timescale = 5,
        .acq_prob = 2,
        .loss_prob = 25
    },
    {
        .name = "Polydivision",
        .eligibility = 40,
        .cost = 10,
        .timescale = 10,
        .acq_prob = 4,
        .loss_prob = 40
    },
    {
        .name = "Energosynthesis",
        .eligibility = 100,
        .cost = 30,
        .timescale = 15,
        .acq_prob = 10,
        .loss_prob = 8
    }
}};

using EvolutionInfo = Info<EVOLUTIONS.size()>;

struct Cell {
    std::uint32_t age, energy;
    const Evolution *ongoing_evolution;
    std::uint32_t ongoing_evolution_progress;
    EvolutionInfo undergone_evolutions, utilized_evolutions;
};

namespace Event {
    enum : std::uint8_t {
        Death,
        Pulse,
        Synthesize,
        SpawnUp,
        SpawnDown,
        SpawnLeft,
        SpawnRight,
        DivideUp,
        DivideDown,
        DivideLeft,
        DivideRight,
        MoveToUp,
        MoveToDown,
        MoveToLeft,
        MoveToRight,
        MoveFromUp,
        MoveFromDown,
        MoveFromLeft,
        MoveFromRight,
        SynthesizeAndMoveToUp,
        SynthesizeAndMoveToDown,
        SynthesizeAndMoveToLeft,
        SynthesizeAndMoveToRight,
        SynthesizeAndMoveFromUp,
        SynthesizeAndMoveFromDown,
        SynthesizeAndMoveFromLeft,
        SynthesizeAndMoveFromRight,
        COUNT
    };
}

using EventInfo = Info<Event::COUNT>;

constexpr std::uint8_t EVENT_TO_ANIMATION_OFFSET =
    std::to_underlying(Animation::Death) - std::to_underlying(Event::Death);

struct Tile {
    std::uint32_t energy;
    EventInfo active_evs;
    Cell cell;
};

struct World {
    std::uint32_t gen;
    std::uint16_t w, h;
    std::uint32_t size;
    Tile *tilemap, *ptr;
    Tile &operator[](std::uint16_t x, std::uint16_t y) noexcept {
        return tilemap[y * static_cast<std::uint32_t>(w) + x];
    }
    bool create(std::uint16_t new_w, std::uint16_t new_h) noexcept {
        w = new_w;
        h = new_h;
        size = static_cast<std::uint32_t>(w) * h;
        try {
            tilemap = new Tile[w * h]{};
            return true;
        } catch (const std::bad_alloc &) {
            return false;
        }
    }
    void destroy() noexcept {
        gen = 0;
        w = 0;
        h = 0;
        size = 0;
        delete[] tilemap;
        ptr = nullptr;
    }
    std::uint16_t get_ptr_x() noexcept {
        return std::distance(tilemap, ptr) % w;
    }
    std::uint16_t get_ptr_y() noexcept {
        return std::distance(tilemap, ptr) / w;
    }
};

World world;

std::array<Tile *, 4> find_adjacent_tiles(std::uint16_t x, std::uint16_t y) {
    return {{
        y > 0 ? &world[x, y - 1] : nullptr,
        y < world.h - 1 ? &world[x, y + 1] : nullptr,
        x > 0 ? &world[x - 1, y] : nullptr,
        x < world.w - 1 ? &world[x + 1, y] : nullptr
    }};
}

constexpr std::uint32_t
    GENERATION_TILE_INIT_ENERGY_CAP = 75,
    GENERATION_TILE_INIT_ENERGY_SUM = 77 * 76 / 2;

constexpr std::uint32_t MAX_GENERATION_OPS_PER_TICK = 1000000;

std::uint32_t generate() {
    static std::uint16_t x = 0, y = 0;
    static std::uint32_t tiles_generated;
    std::uint32_t operations = 0;
    if (x == 0 && y == 0) {
        tiles_generated = 0;
    }
    for (; x < world.w; ++x) {
        for (; y < world.h; ++y) {
            if (++operations > MAX_GENERATION_OPS_PER_TICK) {
                return tiles_generated;
            }
            Tile &tile = world[x, y];
            std::uint32_t base = rng::rand(GENERATION_TILE_INIT_ENERGY_SUM);
            for (std::uint32_t i = 0; i <= GENERATION_TILE_INIT_ENERGY_CAP; ++i) {
                if (base <= GENERATION_TILE_INIT_ENERGY_CAP - i) {
                    tile.energy = i;
                    break;
                }
                base -= GENERATION_TILE_INIT_ENERGY_CAP + 1 - i;
            }
            if (rng::chance(10)) {
                tile.cell.energy = rng::rand(6) + 5;
            }
            ++tiles_generated;
        }
        y = 0;
    }
    x = 0;
    y = 0;
    return tiles_generated;
}

void advance_age() {
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            Tile &tile = world[x, y];
            tile.active_evs.clear();
            tile.cell.utilized_evolutions.clear();
            if (world.gen % 10 == 0) {
                ++tile.energy;
            }
            if (tile.cell.energy != 0) {
                ++tile.cell.age;
            }
        }
    }
}

void advance_harvesting() {
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            Tile &tile = world[x, y];
            if (tile.cell.energy == 0 || tile.cell.ongoing_evolution) {
                continue;
            }
            std::uint32_t
                harvested_energy =
                    (tile.energy >= 100 ? 3 : tile.energy >= 50 ? 2 : 1) +
                    (tile.cell.age >= 40 ? 2 : tile.cell.age >= 20 ? 1 : 0),
                actual_harvested_energy = std::min(harvested_energy, tile.energy);
            tile.energy -= actual_harvested_energy;
            tile.cell.energy += actual_harvested_energy;
            if (tile.cell.undergone_evolutions[Evolution::Energosynthesis]) {
                std::uint8_t free_neighbor_count = 0;
                if (x > 0) {
                    if (world[x - 1, y].cell.energy == 0) {
                        ++free_neighbor_count;
                    }
                    if (y > 0 && world[x - 1, y - 1].cell.energy == 0) {
                        ++free_neighbor_count;
                    }
                    if (y < world.h - 1 && world[x - 1, y + 1].cell.energy == 0) {
                        ++free_neighbor_count;
                    }
                }
                if (x < world.w - 1) {
                    if (world[x + 1, y].cell.energy == 0) {
                        ++free_neighbor_count;
                    }
                    if (y > 0 && world[x + 1, y - 1].cell.energy == 0) {
                        ++free_neighbor_count;
                    }
                    if (y < world.h - 1 && world[x + 1, y + 1].cell.energy == 0) {
                        ++free_neighbor_count;
                    }
                }
                if (y > 0 && world[x, y - 1].cell.energy == 0) {
                    ++free_neighbor_count;
                }
                if (y < world.h - 1 && world[x, y + 1].cell.energy == 0) {
                    ++free_neighbor_count;
                }
                if (free_neighbor_count >= 1 || rng::chance(2)) {
                    tile.cell.utilized_evolutions += Evolution::Energosynthesis;
                    tile.active_evs += Event::Synthesize;
                    ++tile.cell.energy;
                    if (free_neighbor_count >= 4) {
                        ++tile.cell.energy;
                        if (free_neighbor_count == 8) {
                            ++tile.cell.energy;
                        }
                    }
                }
            }
        }
    }
}

void advance_living() {
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            Tile &tile = world[x, y];
            if (tile.cell.energy == 0) {
                continue;
            }
            const std::uint32_t living_cost =
                tile.cell.age / 100 + !!(tile.cell.age % 100);
            if (tile.cell.energy > living_cost) {
                tile.cell.energy -= living_cost;
                continue;
            }
            tile.cell.energy = 0;
            tile.active_evs -= Event::Synthesize;
            tile.energy += tile.cell.age;
            tile.cell.age = 0;
            tile.cell.ongoing_evolution = nullptr;
            tile.cell.ongoing_evolution_progress = 0;
            tile.cell.undergone_evolutions.clear();
            if (world.ptr == &tile) {
                world.ptr = nullptr;
            }
            tile.active_evs += Event::Death;
        }
    }
}

void advance_pulsing() {
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            Tile &tile = world[x, y];
            if (!tile.cell.ongoing_evolution) {
                continue;
            }
            if (
                ++tile.cell.ongoing_evolution_progress ==
                tile.cell.ongoing_evolution->timescale
            ) {
                tile.cell.undergone_evolutions +=
                    std::distance(EVOLUTIONS.data(), tile.cell.ongoing_evolution);
                tile.cell.ongoing_evolution = nullptr;
                tile.cell.ongoing_evolution_progress = 0;
            }
            tile.active_evs += Event::Pulse;
        }
    }
}

void advance_instinct() {
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            Tile &tile = world[x, y];
            if (tile.active_evs.any(Event::Synthesize)) {
                continue;
            }
            if (
                tile.cell.undergone_evolutions[Evolution::Motility] &&
                tile.cell.energy >= 3
            ) {
                Tile *selected_tile = &tile;
                std::array<Tile *, 4> adjacent_tiles = find_adjacent_tiles(x, y);
                std::uint8_t direction = 0;
                for (std::uint8_t i = 0; i < 4; ++i) {
                    if (
                        adjacent_tiles[i] &&
                        adjacent_tiles[i]->cell.energy == 0 &&
                        adjacent_tiles[i]->energy > selected_tile->energy
                    ) {
                        selected_tile = adjacent_tiles[i];
                        direction = i;
                    }
                }
                if (selected_tile != &tile) {
                    bool is_synthesizing = tile.active_evs[Event::Synthesize];
                    if (is_synthesizing) {
                        tile.active_evs -= Event::Synthesize;
                        tile.active_evs += Event::SynthesizeAndMoveFromUp + direction;
                    } else {
                        tile.active_evs += Event::MoveFromUp + direction;
                    }
                    selected_tile->cell = tile.cell;
                    tile.cell.age = 0;
                    tile.cell.energy = 0;
                    tile.cell.undergone_evolutions.clear();
                    if (world.ptr == &tile) {
                        world.ptr = selected_tile;
                    }
                    selected_tile->cell.utilized_evolutions += Evolution::Motility;
                    if (is_synthesizing) {
                        selected_tile->active_evs += Event::SynthesizeAndMoveToUp + direction;
                    } else {
                        selected_tile->active_evs += Event::MoveToUp + direction;
                    }
                    continue;
                }
            }
        }
    }
}

void advance_reproduction() {
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            Tile &tile = world[x, y];
            if (
                tile.active_evs.any(
                    Event::Synthesize,
                    Event::SynthesizeAndMoveToUp,
                    Event::SynthesizeAndMoveToDown,
                    Event::SynthesizeAndMoveToLeft,
                    Event::SynthesizeAndMoveToRight
                ) ||
                tile.cell.age < 10 ||
                tile.cell.energy < 10 ||
                (
                    tile.cell.undergone_evolutions[Evolution::Polydivision] &&
                    tile.cell.energy < 20
                ) ||
                !rng::chance(
                    tile.cell.undergone_evolutions[Evolution::Polydivision] ? 16 : 8
                )
            ) {
                continue;
            }
            std::array<Tile *, 4> adjacent_tiles = find_adjacent_tiles(x, y);
            if (tile.cell.undergone_evolutions[Evolution::Polydivision]) {
                std::bitset<4> tile_selections;
                for (std::uint8_t i = 0; i < 4; ++i) {
                    if (
                        adjacent_tiles[i] &&
                        adjacent_tiles[i]->cell.energy == 0
                    ) {
                        tile_selections[i] = true;
                    }
                }
                tile.cell.energy /= tile_selections.count() + 1;
                for (std::uint8_t i = 0; i < 4; ++i) {
                    if (tile_selections[i]) {
                        tile.cell.utilized_evolutions += Evolution::Polydivision;
                        tile.active_evs += Event::DivideUp + i;
                        adjacent_tiles[i]->cell.age = 0;
                        adjacent_tiles[i]->cell.energy = tile.cell.energy;
                        for (std::uint8_t j = 0; j < Evolution::COUNT; ++j) {
                            if (tile.cell.undergone_evolutions[j] && rng::chance(2)) {
                                adjacent_tiles[i]->cell.undergone_evolutions += j;
                            }
                        }
                        adjacent_tiles[i]->active_evs += Event::SpawnUp + i;
                    }
                }
                for (std::uint8_t i = 0; i < Evolution::COUNT; ++i) {
                    if (tile.cell.undergone_evolutions[i] && !rng::chance(2)) {
                        tile.cell.undergone_evolutions -= i;
                    }
                }
                continue;
            }
            Tile *selected_tile = nullptr;
            std::uint8_t direction = 0;
            for (std::uint8_t i = 0; i < 4; ++i) {
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
                tile.active_evs += Event::DivideUp + direction;
                tile.cell.energy /= 2;
                selected_tile->cell.age = 0;
                selected_tile->cell.energy = tile.cell.energy;
                for (std::uint8_t i = 0; i < Evolution::COUNT; ++i) {
                    if (tile.cell.undergone_evolutions[i]) {
                        if (rng::chance(2)) {
                            selected_tile->cell.undergone_evolutions += i;
                        }
                        if (!rng::chance(2)) {
                            tile.cell.undergone_evolutions -= i;
                        }
                    }
                }
                selected_tile->active_evs += Event::SpawnUp + direction;
            }
        }
    }
}

void advance_evolution() {
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            Tile &tile = world[x, y];
            if (tile.cell.ongoing_evolution) {
                continue;
            }
            bool regressive_evolution_happened = false;
            for (std::uint8_t i = 0; i < Evolution::COUNT; ++i) {
                if (
                    tile.cell.undergone_evolutions[i] &&
                    !tile.cell.utilized_evolutions[i] &&
                    rng::chance(EVOLUTIONS[i].loss_prob)
                ) {
                    tile.cell.undergone_evolutions -= i;
                    regressive_evolution_happened = true;
                }
            }
            if (
                regressive_evolution_happened ||
                tile.active_evs.any(
                    Event::Synthesize,
                    Event::SynthesizeAndMoveToUp,
                    Event::SynthesizeAndMoveToDown,
                    Event::SynthesizeAndMoveToLeft,
                    Event::SynthesizeAndMoveToRight
                )
            ) {
                continue;
            }
            for (std::uint8_t i = 0; i < Evolution::COUNT; ++i) {
                if (
                    !tile.cell.undergone_evolutions[i] &&
                    tile.cell.age >= EVOLUTIONS[i].eligibility &&
                    tile.cell.energy >= EVOLUTIONS[i].cost &&
                    rng::chance(EVOLUTIONS[i].acq_prob)
                ) {
                    tile.cell.ongoing_evolution = &EVOLUTIONS[i];
                    break;
                }
            }
        }
    }
}

void advance() {
    ++world.gen;
    advance_age();
    advance_harvesting();
    advance_living();
    advance_pulsing();
    advance_instinct();
    advance_reproduction();
    advance_evolution();
}

std::uint32_t count_live_cells() noexcept {
    std::uint32_t live_cell_count = 0;
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            if (world[x, y].cell.energy != 0) {
                ++live_cell_count;
            }
        }
    }
    return live_cell_count;
}

bool ux_creation() {
    std::uint32_t
        potential_w = std::numeric_limits<std::uint32_t>::max(),
        potential_h = std::numeric_limits<std::uint32_t>::max();
    if (gui::input_world_w.buffer.front() && gui::input_world_h.buffer.front()) {
        potential_w = std::stoul(gui::input_world_w.get_value());
        potential_h = std::stoul(gui::input_world_h.get_value());
        gui::btn_generate.is_enabled =
            potential_w >= 10 &&
            potential_w <= std::numeric_limits<std::uint16_t>::max() &&
            potential_h >= 10 &&
            potential_h <= std::numeric_limits<std::uint16_t>::max();
    } else {
        gui::btn_generate.is_enabled = false;
    }
    if (gui::btn_generate.is_pressed) {
        rng::srand(
            gui::input_seed.buffer.front() ?
                std::stoull(gui::input_seed.get_value()) :
                static_cast<std::uint32_t>(std::time(nullptr))
        );
        if (!world.create(potential_w, potential_h)) {
            gui::text_error.text = "Out of memory! Try making a smaller world!";
            return true;
        }
        gui::input_world_w.clear();
        gui::input_world_h.clear();
        gui::input_seed.clear();
        active_ux_state = UXState::Generation;
        gui::text_generating.text = "Generating world... 0%";
    }
    return true;
}

bool ux_generation() {
    std::uint32_t tiles_generated = generate();
    gui::text_generating.text = std::format(
        "Generating world... {}%",
        static_cast<std::uint32_t>(
            static_cast<double>(tiles_generated) / world.size * 100
        )
    );
    if (tiles_generated == world.size) {
        active_ux_state = UXState::Sim;
    }
    return true;
}

constexpr std::uint8_t
    ZOOM_SPEED    =   5,
    MIN_ZOOM      =   5,
    DEFAULT_ZOOM  =  25,
    MAX_ZOOM      = 100,
    MIN_SPEED     =   1,
    DEFAULT_SPEED =   2,
    MAX_SPEED     =   4;

bool ux_sim() {
    static bool is_ready = false;
    static std::uint32_t last_gen;
    static std::uint8_t zoom, last_zoom, speed, last_speed;
    static std::uint16_t animation_ms;
    static std::int32_t cam_x, cam_y, drag_x, drag_y, tile_w, tile_h;
    static bool is_dragging, has_acted, auto_mode, requires_clear, requires_report;
    static std::uint32_t animation_tick;
    if (!is_ready) {
        last_gen = std::numeric_limits<std::uint32_t>::max();
        zoom = DEFAULT_ZOOM;
        last_zoom = 0;
        speed = DEFAULT_SPEED;
        last_speed = 0;
        animation_ms = ANIMATION_MS / DEFAULT_SPEED;
        cam_x = 0;
        cam_y = 0;
        drag_x = 0;
        drag_y = 0;
        tile_w = TILE_WIDTH * DEFAULT_ZOOM / 100;
        tile_h = TILE_HEIGHT * DEFAULT_ZOOM / 100;
        is_dragging = false;
        has_acted = true;
        auto_mode = false;
        requires_clear = false;
        requires_report = false;
        animation_tick = 0;
        gui::text_report_world_size.text =
            std::format("World size: {}x{}", world.w, world.h);
        gui::text_report_seed.text = std::format("Seed: {}", rng::seed);
        is_ready = true;
    }
    gui::icon_btn_start.is_enabled = !auto_mode;
    gui::icon_btn_stop.is_enabled = auto_mode;
    gui::icon_btn_step.is_enabled = !auto_mode && animation_tick == 0;
    gui::icon_btn_zoom_in.is_enabled = zoom < MAX_ZOOM;
    gui::icon_btn_zoom_out.is_enabled = zoom > MIN_ZOOM;
    gui::icon_btn_speed_up.is_enabled = speed < MAX_SPEED;
    gui::icon_btn_slow_down.is_enabled = speed > MIN_SPEED;
    gui::icon_btn_quit.is_enabled = true;
    if (gui::icon_btn_quit.is_pressed) {
        is_ready = false;
        world.destroy();
        active_ux_state = UXState::Creation;
        return true;
    }
    std::int32_t mouse_x, mouse_y;
    std::uint32_t mouse_state = SDL_GetMouseState(&mouse_x, &mouse_y);
    const struct nk_rect gui_panel_controls_rect = gui::panel_controls.pos();
    const std::int32_t y_bound = gui_panel_controls_rect.y + gui_panel_controls_rect.h;
    const bool is_mouse_within_bounds =
        mouse_x >= 0 &&
        mouse_y >= y_bound &&
        mouse_x < ctx.window_w &&
        mouse_y < ctx.window_h;
    if (
        is_mouse_within_bounds &&
        (
            (ctx.scroll_y > 0 && zoom < MAX_ZOOM) ||
            (ctx.scroll_y < 0 && zoom > MIN_ZOOM)
        )
    ) {
        const std::uint8_t new_zoom =
            zoom + (ctx.scroll_y < 0 ? -ZOOM_SPEED : ZOOM_SPEED);
        const double factor = static_cast<double>(new_zoom) / zoom;
        zoom = new_zoom;
        cam_x = (cam_x - mouse_x) * factor + mouse_x;
        cam_y = (cam_y - mouse_y + y_bound) * factor + mouse_y - y_bound;
    }
    const std::uint32_t curr_tick = SDL_GetTicks();
    const std::int32_t
        disp_x = mouse_x - cam_x,
        disp_y = mouse_y - y_bound - cam_y;
    if (gui::icon_btn_start.is_pressed) {
        if (!has_acted) {
            auto_mode = true;
        }
        has_acted = true;
    } else if (gui::icon_btn_stop.is_pressed) {
        if (!has_acted) {
            auto_mode = false;
        }
        has_acted = true;
    } else if (gui::icon_btn_step.is_pressed) {
        if (!has_acted && animation_tick == 0) {
            advance();
            animation_tick = curr_tick;
        }
        has_acted = true;
    } else if (gui::icon_btn_zoom_in.is_pressed) {
        if (!has_acted && zoom < MAX_ZOOM) {
            const std::uint8_t new_zoom = zoom + ZOOM_SPEED;
            const double factor = static_cast<double>(new_zoom) / zoom;
            zoom = new_zoom;
            const std::int32_t
                center_x = ctx.window_w / 2,
                center_y = (ctx.window_h - y_bound) / 2;
            cam_x = (cam_x - center_x) * factor + center_x;
            cam_y = (cam_y - center_y) * factor + center_y;
        }
        has_acted = true;
    } else if (gui::icon_btn_zoom_out.is_pressed) {
        if (!has_acted && zoom > MIN_ZOOM) {
            const std::uint8_t new_zoom = zoom - ZOOM_SPEED;
            const double factor = static_cast<double>(new_zoom) / zoom;
            zoom = new_zoom;
            const std::int32_t
                center_x = ctx.window_w / 2,
                center_y = (ctx.window_h - y_bound) / 2;
            cam_x = (cam_x - center_x) * factor + center_x;
            cam_y = (cam_y - center_y) * factor + center_y;
        }
        has_acted = true;
    } else if (gui::icon_btn_speed_up.is_pressed) {
        if (!has_acted && speed < MAX_SPEED) {
            speed <<= 1;
        }
        has_acted = true;
    } else if (gui::icon_btn_slow_down.is_pressed) {
        if (!has_acted && speed > MIN_SPEED) {
            speed >>= 1;
        }
        has_acted = true;
    } else if (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        if (!has_acted) {
            if (
                is_mouse_within_bounds &&
                disp_x >= 0 &&
                disp_y >= 0 &&
                disp_x < world.w * tile_w &&
                disp_y < world.h * tile_h
            ) {
                world.ptr = &world[disp_x / tile_w, disp_y / tile_h];
                requires_report = true;
                requires_clear = true;
            } else {
                world.ptr = nullptr;
            }
        }
        has_acted = true;
    } else {
        has_acted = false;
    }
    if (zoom != last_zoom) {
        tile_w = TILE_WIDTH * zoom / 100;
        tile_h = TILE_HEIGHT * zoom / 100;
        gui::text_zoom.text = std::format("{}%", static_cast<std::uint32_t>(zoom));
        last_zoom = zoom;
    }
    if (speed != last_speed) {
        animation_ms = ANIMATION_MS / speed;
        gui::text_speed.text = std::format("{}x", static_cast<std::uint32_t>(speed));
        last_speed = speed;
    }
    if (
        is_mouse_within_bounds &&
        (mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT))
    ) {
        if (!is_dragging) {
            is_dragging = true;
            drag_x = mouse_x;
            drag_y = mouse_y;
        } else {
            std::int32_t dx = mouse_x - drag_x, dy = mouse_y - drag_y;
            cam_x += dx;
            cam_y += dy;
            drag_x = mouse_x;
            drag_y = mouse_y;
        }
    } else {
        is_dragging = false;
    }
    if (auto_mode && animation_tick == 0) {
        advance();
        animation_tick = curr_tick;
    } else if (animation_tick != 0 && curr_tick >= animation_tick + animation_ms) {
        animation_tick = 0;
    }
    if (world.gen != last_gen) {
        gui::text_report_gen.text = std::format("Generation: {}", world.gen);
        gui::text_report_live_cell_count.text =
            std::format("Live cells: {}", count_live_cells());
        if (world.ptr) {
            requires_report = true;
        }
        last_gen = world.gen;
    }
    if (!world.ptr && requires_clear) {
        gui::text_report_ptr_pos.text.clear();
        gui::text_report_curr_tile_energy.text.clear();
        gui::text_report_curr_cell_age.text.clear();
        gui::text_report_curr_cell_energy.text.clear();
        gui::text_report_curr_cell_undergone_evolutions.text.clear();
        gui::text_report_curr_cell_ongoing_evolution.text.clear();
        requires_clear = false;
    } else if (world.ptr && requires_report) {
        gui::text_report_ptr_pos.text =
            std::format("XY: {}, {}", world.get_ptr_x(), world.get_ptr_y());
        gui::text_report_curr_tile_energy.text =
            std::format("Tile energy: {}", world.ptr->energy);
        if (world.ptr->cell.energy != 0) {
            gui::text_report_curr_cell_age.text =
                std::format("Cell age: {}", world.ptr->cell.age);
            gui::text_report_curr_cell_energy.text =
                std::format("Cell energy: {}", world.ptr->cell.energy);
            if (world.ptr->cell.undergone_evolutions.any()) {
                gui::text_report_curr_cell_undergone_evolutions.text =
                    "Undergone evolutions: ";
                bool requires_comma = false;
                for (std::uint8_t i = 0; i < Evolution::COUNT; ++i) {
                    if (world.ptr->cell.undergone_evolutions[i]) {
                        gui::text_report_curr_cell_undergone_evolutions.text +=
                            std::format(
                                "{}{}",
                                requires_comma ? ", " : "", EVOLUTIONS[i].name
                            );
                        requires_comma = true;
                    }
                }
            } else {
                gui::text_report_curr_cell_undergone_evolutions.text.clear();
            }
            if (world.ptr->cell.ongoing_evolution) {
                gui::text_report_curr_cell_ongoing_evolution.text =
                    std::format(
                        "Currently evolving: {} ({}%)",
                        world.ptr->cell.ongoing_evolution->name,
                        100 * world.ptr->cell.ongoing_evolution_progress /
                        world.ptr->cell.ongoing_evolution->timescale
                    );
            } else {
                gui::text_report_curr_cell_ongoing_evolution.text.clear();
            }
        } else {
            gui::text_report_curr_cell_age.text.clear();
            gui::text_report_curr_cell_energy.text.clear();
            gui::text_report_curr_cell_undergone_evolutions.text.clear();
            gui::text_report_curr_cell_ongoing_evolution.text.clear();
        }
        requires_report = false;
    }
    if (
        ctx.window_w > world.w * tile_w ||
        (cam_x > 0 && cam_x < -(world.w * tile_w - ctx.window_w))
    ) {
        cam_x = -(world.w * tile_w - ctx.window_w) / 2;
    } else if (cam_x > 0) {
        cam_x = 0;
    } else if (cam_x < -(world.w * tile_w - ctx.window_w)) {
        cam_x = -(world.w * tile_w - ctx.window_w);
    }
    if (
        ctx.window_h - y_bound > world.h * tile_h ||
        (cam_y > 0 && cam_y < -(world.h * tile_h - ctx.window_h + y_bound))
    ) {
        cam_y = -(world.h * tile_h - ctx.window_h + y_bound) / 2;
    } else if (cam_y > 0) {
        cam_y = 0;
    } else if (cam_y < -(world.h * tile_h - ctx.window_h + y_bound)) {
        cam_y = -(world.h * tile_h - ctx.window_h + y_bound);
    }
    SDL_Rect
        srcrect{ .w = TILE_WIDTH, .h = TILE_HEIGHT },
        dstrect{ .w = tile_w,     .h = tile_h      };
    for (std::uint16_t x = 0; x < world.w; ++x) {
        for (std::uint16_t y = 0; y < world.h; ++y) {
            dstrect.x = static_cast<std::uint32_t>(x) * tile_w + cam_x;
            dstrect.y = static_cast<std::uint32_t>(y) * tile_h + y_bound + cam_y;
            if (
                dstrect.x + dstrect.w < 0 ||
                dstrect.y + dstrect.h < y_bound ||
                dstrect.x >= ctx.window_w ||
                dstrect.y >= ctx.window_h
            ) {
                continue;
            }
            Tile &tile = world[x, y];
            if (
                SDL_SetRenderDrawColor(
                    ctx.renderer,
                    COLOR_CUSTOM_QOL.r,
                    COLOR_CUSTOM_QOL.g,
                    COLOR_CUSTOM_QOL.b,
                    0x02 * std::min(tile.energy, GENERATION_TILE_INIT_ENERGY_CAP + 1)
                ) != 0 ||
                SDL_RenderFillRect(ctx.renderer, &dstrect) != 0
            ) {
                return false;
            }
            select_still_frame(
                srcrect,
                Still::Tile
            );
            if (!ctx.render(srcrect, dstrect)) {
                return false;
            }
            if (animation_tick == 0) {
                if (tile.cell.energy != 0) {
                    select_still_frame(
                        srcrect,
                        Still::Cell
                    );
                    if (!ctx.render(srcrect, dstrect)) {
                        return false;
                    }
                }
            } else {
                if (tile.active_evs.none() && tile.cell.energy != 0) {
                    select_animation_frame(
                        srcrect,
                        Animation::Twitch,
                        curr_tick - animation_tick,
                        speed
                    );
                    if (!ctx.render(srcrect, dstrect)) {
                        return false;
                    }
                }
                for (std::uint8_t ev = 0; ev < Event::COUNT; ++ev) {
                    if (tile.active_evs[ev]) {
                        select_animation_frame(
                            srcrect,
                            ev + EVENT_TO_ANIMATION_OFFSET,
                            curr_tick - animation_tick,
                            speed
                        );
                        if (!ctx.render(srcrect, dstrect)) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    if (
        is_mouse_within_bounds &&
        disp_x >= 0 &&
        disp_y >= 0 &&
        disp_x < world.w * tile_w &&
        disp_y < world.h * tile_h
    ) {
        dstrect.x = disp_x / tile_w * tile_w + cam_x;
        dstrect.y = disp_y / tile_h * tile_h + y_bound + cam_y;
        if (
            dstrect.x + dstrect.w >= 0 &&
            dstrect.y + dstrect.h >= y_bound &&
            dstrect.x < ctx.window_w &&
            dstrect.y < ctx.window_h
        ) {
            select_still_frame(
                srcrect,
                Still::Cursor
            );
            if (!ctx.render(srcrect, dstrect)) {
                return false;
            }
        }
    }
    if (!world.ptr) {
        return true;
    }
    dstrect.x = world.get_ptr_x() * tile_w + cam_x;
    dstrect.y = world.get_ptr_y() * tile_h + y_bound + cam_y;
    if (
        dstrect.x + dstrect.w >= 0 &&
        dstrect.y + dstrect.h >= y_bound &&
        dstrect.x < ctx.window_w &&
        dstrect.y < ctx.window_h
    ) {
        select_still_frame(
            srcrect,
            Still::Pointer
        );
        if (!ctx.render(srcrect, dstrect)) {
            return false;
        }
    }
    return true;
}

bool is_running = true;

bool tick() {
    SDL_GetWindowSize(ctx.window, &ctx.window_w, &ctx.window_h);
    ctx.scroll_x = 0;
    ctx.scroll_y = 0;
    SDL_Event ev;
    nk_input_begin(ctx.nk_ctx);
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            nk_input_end(ctx.nk_ctx);
            is_running = false;
            return true;
        case SDL_MOUSEWHEEL:
            ctx.scroll_x = ev.wheel.x;
            ctx.scroll_y = ev.wheel.y;
        }
        nk_sdl_handle_event(&ev);
    }
    nk_input_end(ctx.nk_ctx);
    gui::render();
    if (
        SDL_SetRenderDrawColor(
            ctx.renderer,
            COLOR_BG.r,
            COLOR_BG.g,
            COLOR_BG.b,
            COLOR_BG.a
        ) != 0 ||
        SDL_RenderClear(ctx.renderer) != 0
    ) {
        return false;
    }
    switch (active_ux_state) {
    case UXState::Creation:
        if (!ux_creation()) {
            return false;
        }
        break;
    case UXState::Generation:
        if (!ux_generation()) {
            return false;
        }
        break;
    case UXState::Sim:
        if (!ux_sim()) {
            return false;
        }
        break;
    default:
        return false;
    }
    nk_sdl_render(NK_ANTI_ALIASING_ON);
    SDL_RenderPresent(ctx.renderer);
    return true;
}

int main(int, char *[]) {
    std::println("{} {} - {}", TITLE, VERSION, RELEASE_DATE);
    try {
        if (!ctx) {
            std::println(std::cerr, "[SDL error] {}", SDL_GetError());
            return 1;
        }
        while (is_running) {
            if (!tick()) {
                std::println(std::cerr, "[SDL error] {}", SDL_GetError());
                return 1;
            }
        }
    } catch (const std::exception &exception) {
        std::println(std::cerr, "[C++ exception] {}", exception.what());
        return 1;
    }
    return 0;
}
