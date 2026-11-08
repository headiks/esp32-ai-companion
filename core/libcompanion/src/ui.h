// Internal: the screen base class and the shared pieces of design 1a. One static instance of every screen, no heap.
#pragma once
#include "companion/app.h"
#include "ui/components.h"

namespace cc {

class Screen {
 public:
  virtual ~Screen() {}
  virtual void enter(App&) {}
  virtual void tick(App&, float) {}
  virtual void draw(App&, Canvas&) = 0;
  virtual void onGesture(App&, const GestureEvent&) {}
  virtual bool wantsDoubleTap() const { return false; }
  virtual bool capturesSwipes() const { return false; }  // games use the whole screen
  virtual bool onMotion(App&, MotionEvent) { return false; }  // true: the game used it, no pet reaction
};

// Each file of screens hands out its own; nullptr for screens it does not have.
Screen* systemScreen(ScreenId id);  // ui_system.cpp: face, menus, talk, notifications, settings, about
Screen* timeScreen(ScreenId id);    // ui_time.cpp: clocks, timer, stopwatch, weather
Screen* playScreen(ScreenId id);    // ui_play.cpp: pet, games list, firefly, tic-tac-toe, guess
Screen* gameScreen(ScreenId id);    // ui_games.cpp: maze, orbit, snake, Simon
Screen* partyScreen(ScreenId id);   // ui_party.cpp: the rest of the games, remotes, Bluetooth pairing

Screen* screenInstance(ScreenId id);

// Design 1a templates shared by the games (spec 17 and 18).
// Start: the game's name, a focused mini face, its rule, the record, "Старт".
void gameStart(Canvas& c, App& a, const char* name, const char* rule, const char* record);
// Result: a full accent ring, a face for the outcome, the number (or a word), a caption, "Выход" and "Ещё".
void gameResult(Canvas& c, App& a, const char* value, const char* caption, Emotion face);
extern const ui::Button kStartButton, kExitButton, kAgainButton;

// The character centred on (cx, cy), `width` wide: MiniFace of the spec, live, without overlays.
void miniFace(Canvas& c, App& a, float cx, float cy, float width);

}  // namespace cc
