// Design 1a "Мягкий": every colour, font and size of the interface. No other file names a colour.
// Rules behind the numbers (documentation/design/1a/claude-code-prompt.md):
// - background is always black: an unlit AMOLED pixel draws no current and does not wear;
// - flat fills only, bright colours under 15 % of the frame, big fields in SURFACE with a bright outline;
// - text and buttons inside the r210 circle, only arcs live on the rim (r196..222);
// - text at least 22 px, touch targets at least 72 px.
#pragma once
#include "companion/canvas.h"

namespace cc {
namespace theme {

constexpr uint16_t BG = rgb565(0x00, 0x00, 0x00);
constexpr uint16_t INK = rgb565(0xF4, 0xF1, 0xEA);        // main text, eyes
constexpr uint16_t INK_2 = rgb565(0x8A, 0x86, 0x7E);      // secondary text
constexpr uint16_t INK_3 = rgb565(0x3A, 0x38, 0x35);      // outlines, far list rows
constexpr uint16_t TRACK = rgb565(0x24, 0x23, 0x22);      // tracks of arcs and bars
constexpr uint16_t TRACK_2 = rgb565(0x1C, 0x1C, 0x1B);    // tracks of thick rings
constexpr uint16_t SURFACE = rgb565(0x16, 0x16, 0x15);    // cards, plates, pill buttons
constexpr uint16_t ACCENT = rgb565(0xA9, 0xB4, 0xD8);     // main action, progress
constexpr uint16_t ACCENT_2 = rgb565(0xE8, 0xA0, 0x8E);   // hearts and diamonds, love, errors, blush

// Only on the screens named.
namespace simon {
constexpr uint16_t GREEN = rgb565(0xA8, 0xC4, 0x9A), YELLOW = rgb565(0xE2, 0xCF, 0x8E);
constexpr uint16_t DIM_BLUE = rgb565(0x26, 0x2A, 0x36), DIM_RED = rgb565(0x3A, 0x27, 0x23);
constexpr uint16_t DIM_GREEN = rgb565(0x22, 0x2C, 0x1E), DIM_YELLOW = rgb565(0x34, 0x30, 0x1C);
}  // namespace simon
namespace sky {
constexpr uint16_t CLOUD = rgb565(0x26, 0x27, 0x29), STORM_CLOUD = rgb565(0x1F, 0x20, 0x22), DROP = rgb565(0x3D, 0x42, 0x54);
constexpr uint16_t FOG = rgb565(0x2A, 0x2A, 0x2C), WIND = rgb565(0x4A, 0x49, 0x45), SNOW_DIM = rgb565(0x5F, 0x5C, 0x56);
}  // namespace sky
constexpr uint16_t FIREFLY_HALO = rgb565(0x1D, 0x20, 0x29);
constexpr uint16_t SNAKE_GRID = rgb565(0x2A, 0x29, 0x27), SNAKE_TAIL = rgb565(0x4D, 0x4B, 0x47);

// The spec gives these in RGB565 too: a typo in either table stops the build.
static_assert(INK == 0xF79D && INK_2 == 0x8C2F && INK_3 == 0x39C6 && TRACK == 0x2104 && TRACK_2 == 0x18E3, "palette");
static_assert(SURFACE == 0x10A2 && ACCENT == 0xADBB && ACCENT_2 == 0xED11, "palette");
static_assert(simon::GREEN == 0xAE33 && simon::YELLOW == 0xE671 && simon::DIM_BLUE == 0x2146 && simon::DIM_GREEN == 0x2163, "palette");
static_assert(sky::CLOUD == 0x2125 && sky::DROP == 0x3A0A && sky::FOG == 0x2945 && sky::WIND == 0x4A48 && FIREFLY_HALO == 0x1905, "palette");

// Fonts, by role.
constexpr FontSize NUM_XXL = FontSize::NumXXL;  // 120: counters filling the screen
constexpr FontSize NUM_XL = FontSize::NumXL;    // 104: time
constexpr FontSize NUM_96 = FontSize::Num96;    // timer, temperature, guessed number
constexpr FontSize NUM_92 = FontSize::Num92;    // setting value, stopwatch
constexpr FontSize NUM_84 = FontSize::Num84;    // game result
constexpr FontSize NUM_L = FontSize::NumL;      // 64
constexpr FontSize NUM_L56 = FontSize::NumL56;  // 56: scores
constexpr FontSize NUM_M = FontSize::NumM;      // 48: PIN
constexpr FontSize NUM_M40 = FontSize::NumM40;  // 40
constexpr FontSize NUM_S = FontSize::NumS;      // 36
constexpr FontSize NUM_S30 = FontSize::NumS30;  // 30: diagnostics values
constexpr FontSize TXT_MENU_SEL = FontSize::MenuSel;  // 42, Unbounded with letters: selected row, big words
constexpr FontSize TXT_UNB34 = FontSize::Unb34;       // card ranks, coin side, fortune answer
constexpr FontSize TXT_UNB28 = FontSize::Unb28;       // device name, small ranks
constexpr FontSize TXT_TITLE = FontSize::Title;       // 30
constexpr FontSize TXT_28 = FontSize::Txt28;
constexpr FontSize TXT_BODY = FontSize::Body;         // 26
constexpr FontSize TXT_CAP = FontSize::Cap;           // 24
constexpr FontSize TXT_MIN = FontSize::Min;           // 22, the smallest text allowed
constexpr FontSize TXT_BTN = FontSize::Btn;           // 22 semibold, on buttons

// Geometry of the round 466 px screen.
constexpr int CX = 233, CY = 233;
constexpr int SAFE_R = 210;                 // text and buttons stay inside
constexpr int RIM_IN = 196, RIM_OUT = 222;  // arcs only
constexpr int TOUCH_MIN = 72;
constexpr int BTN_PRIMARY = 92, BTN_SECONDARY = 72, PILL_H = 72;
constexpr int LIST_STEP = 64;
constexpr float HINT_MS = 3000;             // gesture hints show this long after entering a screen

}  // namespace theme
}  // namespace cc
