// All interface screens. Unified look: black, white, greys and muted pastels, content fits the round panel.
#include "ui.h"
#include <stdio.h>
#include <string.h>

namespace cc {

static const char* const kScreenNames[(int)ScreenId::Count] = {
    "face", "menu", "pet", "analog_clock", "digital_clock", "timer", "stopwatch", "notifications",
    "settings", "diagnostics", "games", "info", "tictactoe", "guess", "pet_game",
    "maze", "orbit", "snake", "simon", "coin", "dice", "magic_ball", "balance", "duel", "shaker", "safe", "workout", "mole", "blackjack", "weather", "controller", "mouse",
    "talk", "setting_value", "clocks", "remotes", "ble_pairing"};

const char* screenName(ScreenId s) { return kScreenNames[(int)s]; }
bool screenFromName(const char* n, ScreenId* out) {
  for (int i = 0; i < (int)ScreenId::Count; i++)
    if (!strcmp(n, kScreenNames[i])) { *out = (ScreenId)i; return true; }
  return false;
}

void miniFace(Canvas& c, App& a, float cx, float cy, float width) {
  float k = width / 354;  // the face block is 354 x 176 at full size, its centre 88 px below its top
  a.face.drawMini(c, a.anim, cx - width / 2, cy - 88 * k, width, a.eyeColor());
}

Screen* screenInstance(ScreenId id) {
  if (Screen* s = systemScreen(id)) return s;
  if (Screen* s = timeScreen(id)) return s;
  if (Screen* s = playScreen(id)) return s;
  switch (id) {
    case ScreenId::Maze: case ScreenId::Orbit: case ScreenId::Snake: case ScreenId::Simon: return gameScreen(id);
    default: return partyScreen(id);
  }
}


}  // namespace cc
