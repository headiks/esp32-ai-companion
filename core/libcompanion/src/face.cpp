#include "companion/face.h"
#include <string.h>

namespace cc {

static constexpr float kEyeX = 122;   // distance of each eye from the screen centre
static constexpr float kEyeY = -42;   // ellipse centre sits above screen centre: lids cut its top

// Shorthand: dx dy  rx ry  up slope  lo slope  cheek  pupilR px py
#define POSE(...) EyePose{__VA_ARGS__}
#define NOCHEEK 90.f  // just below the eye: blending toward a smile stays proportional

static const EmotionDef kDefs[kEmotionCount] = {
    // Neutral: the reference look. Slanted flat lid, pupil pressed against it, slightly sly.
    {POSE(0, 0, 90, 72, 2, .19f, 72, 0, NOCHEEK, 19, -8, 23), POSE(0, 0, 90, 72, 2, .19f, 72, 0, NOCHEEK, 19, -8, 23),
     260, .86f, 13, 22, 1700, 2.2f, .28f, 0, 0, 0, Accent::None, PupilShape::Round, 40, 15, false},
    // Happy: lids lift, cheeks push up into a smile.
    {POSE(0, -4, 90, 74, -74, 0, 74, 0, 28, 21, 0, -12), POSE(0, -4, 90, 74, -74, 0, 74, 0, 28, 21, 0, -12),
     220, .62f, 16, 16, 1400, 4.f, .9f, 0, 0, 0, Accent::None, PupilShape::Round, 60, 25, false},
    // VeryHappy: full "^ ^" arcs, bouncing.
    {POSE(0, -10, 90, 74, -74, 0, 74, 0, -14, 0, 0, 0), POSE(0, -10, 90, 74, -74, 0, 74, 0, -14, 0, 0, 0),
     200, .55f, 0, 0, 3000, 9.f, 2.1f, 0, 0, 0, Accent::Sparkle, PupilShape::Round, 50, 0, false},
    // Sad: nose side of the lid rises, pupil large and low, whole face sinks.
    {POSE(4, 10, 84, 66, -2, -.34f, 66, 0, NOCHEEK, 23, 0, 24), POSE(4, 10, 84, 66, -2, -.34f, 66, 0, NOCHEEK, 23, 0, 24),
     760, 1.f, 7, 10, 3200, 2.f, .18f, 0, 0, 10, Accent::Tear, PupilShape::Round, 220, 10, false},
    // Angry: strong slant, small pupils, trembling with tension.
    {POSE(8, 4, 92, 64, 10, .46f, 64, 0, NOCHEEK, 15, 6, 30), POSE(8, 4, 92, 64, 10, .46f, 64, 0, NOCHEEK, 15, 6, 30),
     150, .68f, 6, 6, 2200, 0, 0, 1.2f, 0, 0, Accent::Steam, PupilShape::Round, 20, 0, false},
    // Annoyed: flat half lids and a side glance.
    {POSE(0, 2, 90, 62, 12, .05f, 54, -.02f, NOCHEEK, 16, 0, 26), POSE(0, 2, 90, 62, 12, .05f, 54, -.02f, NOCHEEK, 16, 0, 26),
     300, .9f, 9, 6, 2600, 1.f, .2f, 0, 34, 0, Accent::None, PupilShape::Round, 90, 0, false},
    // Surprised: eyes pop wide, tiny pupils.
    {POSE(-4, -10, 88, 86, -86, 0, 86, 0, NOCHEEK, 15, 0, 0), POSE(-4, -10, 88, 86, -86, 0, 86, 0, NOCHEEK, 15, 0, 0),
     120, .46f, 3, 4, 2500, 0, 0, 0, 0, 0, Accent::Bang, PupilShape::Round, 15, 0, false},
    // Curious: one eye opens more, tilted head.
    {POSE(0, -8, 92, 84, -44, 0, 84, 0, NOCHEEK, 22, 4, -6), POSE(0, 8, 88, 68, -4, .14f, 68, 0, NOCHEEK, 20, 0, 14),
     280, .7f, 12, 30, 1100, 1.5f, .5f, 0, 10, -10, Accent::Question, PupilShape::Round, 80, 10, false},
    // Thinking: pupils up and away, one lid lower.
    {POSE(0, -2, 88, 72, -24, -.08f, 72, 0, NOCHEEK, 16, 0, -4), POSE(0, 2, 88, 72, 6, .1f, 72, 0, NOCHEEK, 16, 0, 2),
     340, .9f, 7, 14, 1900, 1.f, .25f, 0, -34, -22, Accent::Dots, PupilShape::Round, 130, 0, true},
    // Sleepy: heavy lids, slow breathing.
    {POSE(0, 8, 90, 68, 30, .02f, 68, 0, NOCHEEK, 20, 0, 34), POSE(0, 8, 90, 68, 30, .02f, 68, 0, NOCHEEK, 20, 0, 34),
     900, 1.f, 16, 8, 3800, 3.f, .16f, 0, 0, 8, Accent::None, PupilShape::Round, 260, 0, false},
    // Sleeping: only the lower arc of the eye stays visible.
    {POSE(0, 14, 90, 66, 54, 0, 66, 0, NOCHEEK, 0, 0, 40), POSE(0, 14, 90, 66, 54, 0, 66, 0, NOCHEEK, 0, 0, 40),
     1400, 1.f, 0, 0, 9000, 4.f, .13f, 0, 0, 0, Accent::Zzz, PupilShape::Round, 500, 0, false},
    // WakingUp: lids drag open, double blinks handled by doubleBlinkPct.
    {POSE(0, 6, 90, 70, 18, .08f, 70, 0, NOCHEEK, 18, 0, 26), POSE(0, 6, 90, 70, 18, .08f, 70, 0, NOCHEEK, 18, 0, 26),
     1100, .95f, 40, 18, 900, 1.5f, .25f, 0, 0, 0, Accent::None, PupilShape::Round, 180, 90, false},
    // Scared: wide, pupils shrink to dots, shaking, eyes pull together.
    {POSE(14, -6, 78, 84, -70, -.2f, 84, 0, NOCHEEK, 10, 0, 0), POSE(14, -6, 78, 84, -70, -.2f, 84, 0, NOCHEEK, 10, 0, 0),
     110, .55f, 12, 18, 420, 0, 0, 2.6f, 0, 0, Accent::None, PupilShape::Round, 10, 30, false},
    // Affection: soft raised cheeks, heart pupils.
    {POSE(0, -2, 90, 76, -76, 0, 76, 0, 36, 28, 2, -8), POSE(0, -2, 90, 76, -76, 0, 76, 0, 36, 28, 2, -8),
     420, .8f, 11, 6, 2800, 3.f, .55f, 0, 0, 0, Accent::Hearts, PupilShape::Heart, 120, 30, false},
    // Bored: half closed, looking away and down.
    {POSE(0, 8, 90, 64, 20, -.05f, 64, 0, NOCHEEK, 18, 0, 26), POSE(0, 8, 90, 64, 20, -.05f, 64, 0, NOCHEEK, 18, 0, 26),
     700, 1.f, 8, 18, 3400, 1.5f, .15f, 0, -26, 12, Accent::None, PupilShape::Round, 200, 0, true},
    // Attentive: open, centred, pupils big and still.
    {POSE(0, -4, 90, 78, -32, .05f, 78, 0, NOCHEEK, 23, 0, 0), POSE(0, -4, 90, 78, -32, .05f, 78, 0, NOCHEEK, 23, 0, 0),
     180, .75f, 9, 4, 2600, .8f, .4f, 0, 0, 0, Accent::None, PupilShape::Round, 40, 0, false},
    // Confused: mismatched eyes and pupil sizes.
    {POSE(0, -6, 86, 80, -48, -.05f, 80, 0, NOCHEEK, 14, 0, -4), POSE(0, 6, 90, 62, 14, .24f, 62, 0, NOCHEEK, 22, 0, 22),
     320, .7f, 15, 24, 800, 1.f, .35f, 0, 0, 0, Accent::Question, PupilShape::Round, 100, 40, true},
    // Excited: wide and smiling, big pupils, quick bounce.
    {POSE(0, -6, 92, 84, -84, 0, 84, 0, 44, 27, 0, -14), POSE(0, -6, 92, 84, -84, 0, 84, 0, 44, 27, 0, -14),
     160, .5f, 18, 14, 700, 7.f, 2.8f, 0, 0, 0, Accent::Sparkle, PupilShape::Round, 30, 20, false},
    // Mischievous: the reference look turned up, one lid raised, plotting something.
    {POSE(2, -2, 92, 70, 8, .34f, 70, 0, 46, 17, -12, 26), POSE(2, -2, 92, 70, -8, .2f, 70, 0, 46, 18, -6, 16),
     260, .7f, 12, 18, 1500, 1.2f, .6f, 0, 20, 2, Accent::None, PupilShape::Round, 80, 10, false},
    // Shy: eyes shrink together and look away, cheeks glow.
    {POSE(8, 10, 80, 62, -6, -.14f, 62, 0, 40, 20, 0, 8), POSE(8, 10, 80, 62, -6, -.14f, 62, 0, 40, 20, 0, 8),
     420, .85f, 22, 12, 1300, 1.f, .4f, 0, -24, 16, Accent::Blush, PupilShape::Round, 120, 40, true},
    // Proud: lids half lowered, chin up.
    {POSE(0, -10, 92, 68, 14, -.13f, 68, 0, 50, 17, 0, 28), POSE(0, -10, 92, 68, 14, -.13f, 68, 0, 50, 17, 0, 28),
     340, .8f, 8, 6, 2600, 1.2f, .3f, 0, 0, -6, Accent::Sparkle, PupilShape::Round, 90, 0, false},
    // Dizzy: spirals for pupils, the gaze circles.
    {POSE(0, 0, 86, 80, -74, 0, 80, 0, NOCHEEK, 24, 0, 0), POSE(0, 0, 86, 80, -74, 0, 80, 0, NOCHEEK, 24, 0, 0),
     260, .6f, 4, 0, 5000, 2.f, .9f, 0, 0, 0, Accent::None, PupilShape::Spiral, 40, 0, false},
    // Yawning: squeezed shut in the middle by a procedural curve, see composeLayers.
    {POSE(0, 6, 90, 72, 26, 0, 72, 0, NOCHEEK, 20, 0, 30), POSE(0, 6, 90, 72, 26, 0, 72, 0, NOCHEEK, 20, 0, 30),
     700, 1.f, 0, 4, 3000, 2.f, .15f, 0, 0, 6, Accent::None, PupilShape::Round, 200, 0, false},
    // Cold: narrow, shivering.
    {POSE(12, 4, 80, 64, -6, -.2f, 58, 0, NOCHEEK, 14, 0, 8), POSE(12, 4, 80, 64, -6, -.2f, 58, 0, NOCHEEK, 14, 0, 8),
     320, .8f, 20, 8, 1400, 0, 0, 1.7f, 0, 4, Accent::Frost, PupilShape::Round, 80, 20, false},
    // Hot: heavy melting lids, sweat.
    {POSE(-2, 10, 94, 66, 24, -.08f, 66, 0, NOCHEEK, 21, 0, 28), POSE(-2, 10, 94, 66, 24, -.08f, 66, 0, NOCHEEK, 21, 0, 28),
     620, .95f, 14, 10, 2600, 2.5f, .2f, 0, 0, 8, Accent::Sweat, PupilShape::Round, 150, 0, false},
    // Dreamy: big soft pupils drifting upward.
    {POSE(0, -2, 90, 76, -24, -.12f, 70, 0, 52, 27, 0, -18), POSE(0, -2, 90, 76, -24, -.12f, 70, 0, 52, 27, 0, -18),
     800, 1.f, 9, 26, 2400, 2.5f, .17f, 0, 12, -14, Accent::None, PupilShape::Round, 240, 0, true},
    // Focused: narrow beam, almost no drift.
    {POSE(6, 0, 88, 62, 8, .15f, 50, 0, NOCHEEK, 16, 0, 18), POSE(6, 0, 88, 62, 8, .15f, 50, 0, NOCHEEK, 16, 0, 18),
     220, .85f, 5, 3, 3000, .4f, .3f, 0, 0, 0, Accent::None, PupilShape::Round, 50, 0, false},
    // Winking: one happy eye, one closed.
    {POSE(0, -4, 90, 74, -70, 0, 74, 0, 36, 22, 0, -10), POSE(0, 2, 90, 66, 54, 0, 66, 0, NOCHEEK, 0, 0, 30),
     170, .6f, 0, 8, 2000, 2.f, .8f, 0, 0, 0, Accent::None, PupilShape::Round, 40, 0, false},
    // Smug: flat half lids over a small smile.
    {POSE(0, -4, 92, 68, 16, .02f, 68, 0, 40, 17, 0, 30), POSE(0, -4, 92, 68, 16, .02f, 68, 0, 40, 17, 0, 30),
     320, .85f, 9, 8, 2400, 1.f, .3f, 0, 0, -2, Accent::None, PupilShape::Round, 90, 0, false},
    // Skeptical: one brow far up, the other eye narrowed.
    {POSE(0, -8, 90, 80, -40, 0, 80, 0, NOCHEEK, 17, 0, 0), POSE(0, 6, 90, 62, 16, .16f, 56, 0, NOCHEEK, 17, 0, 22),
     300, .8f, 8, 8, 2000, .6f, .3f, 0, 14, 0, Accent::None, PupilShape::Round, 80, 0, false},
    // Guilty: sinks low and avoids looking at you.
    {POSE(6, 12, 82, 60, 2, -.3f, 60, 0, NOCHEEK, 22, 0, 26), POSE(6, 12, 82, 60, 2, -.3f, 60, 0, NOCHEEK, 22, 0, 26),
     560, .95f, 14, 12, 1500, 1.f, .2f, 0, -18, 22, Accent::None, PupilShape::Round, 150, 20, true},
    // Embarrassed: eyes pull in, quick nervous blinks, cheeks glow.
    {POSE(10, 8, 78, 66, -16, -.2f, 66, 0, 34, 18, 0, 8), POSE(10, 8, 78, 66, -16, -.2f, 66, 0, 34, 18, 0, 8),
     300, .7f, 34, 14, 800, 1.f, .8f, .5f, 0, 14, Accent::Blush, PupilShape::Round, 70, 50, true},
    // Hopeful: inner corners lifted, big pupils looking up.
    {POSE(0, -8, 88, 82, -50, -.22f, 82, 0, NOCHEEK, 26, 0, -14), POSE(0, -8, 88, 82, -50, -.22f, 82, 0, NOCHEEK, 26, 0, -14),
     420, .8f, 10, 6, 2400, 1.5f, .4f, 0, 0, -14, Accent::None, PupilShape::Round, 110, 10, false},
    // Lonely: small and low, looking away, a tear now and then.
    {POSE(6, 14, 80, 58, 6, -.28f, 58, 0, NOCHEEK, 20, 0, 26), POSE(6, 14, 80, 58, 6, -.28f, 58, 0, NOCHEEK, 20, 0, 26),
     900, 1.f, 7, 12, 3600, 1.5f, .13f, 0, -30, 10, Accent::Tear, PupilShape::Round, 280, 10, true},
    // Grateful: closed soft arcs with hearts.
    {POSE(0, -2, 84, 72, -72, 0, 72, 0, -2, 0, 0, 0), POSE(0, -2, 84, 72, -72, 0, 72, 0, -2, 0, 0, 0),
     360, .75f, 0, 0, 3000, 2.5f, .5f, 0, 0, 0, Accent::Hearts, PupilShape::Round, 110, 0, false},
    // Jealous: tight slit, glaring to the side.
    {POSE(0, 2, 92, 58, 14, .12f, 46, -.04f, NOCHEEK, 15, 0, 22), POSE(0, 2, 92, 58, 14, .12f, 46, -.04f, NOCHEEK, 15, 0, 22),
     300, .85f, 6, 5, 2600, .5f, .2f, 0, 36, 4, Accent::None, PupilShape::Round, 90, 0, false},
    // Determined: slanted like anger, but big pupils and no trembling.
    {POSE(8, 0, 92, 66, 6, .38f, 66, 0, NOCHEEK, 22, 2, 26), POSE(8, 0, 92, 66, 6, .38f, 66, 0, NOCHEEK, 22, 2, 26),
     200, .8f, 5, 3, 3000, .6f, .5f, 0, 0, -2, Accent::None, PupilShape::Round, 40, 0, false},
    // Terrified: eyes as tall as they get, pupils specks, whole face shaking.
    {POSE(18, -10, 74, 88, -80, -.1f, 88, 0, NOCHEEK, 7, 0, 0), POSE(18, -10, 74, 88, -80, -.1f, 88, 0, NOCHEEK, 7, 0, 0),
     90, .5f, 6, 26, 300, 0, 0, 4.f, 0, 0, Accent::Sweat, PupilShape::Round, 10, 0, false},
    // Disgusted: lower lids pushed up, one eye squeezed more.
    {POSE(0, 4, 88, 58, 10, .1f, 36, .06f, NOCHEEK, 15, 0, 14), POSE(0, 8, 84, 48, 18, .22f, 30, -.08f, NOCHEEK, 14, 0, 12),
     240, .8f, 12, 6, 1800, .8f, .4f, 0, -20, 6, Accent::None, PupilShape::Round, 60, 0, false},
    // Hungry: wide eyes fixed on something below, drooling a little.
    {POSE(0, 2, 92, 78, -30, -.1f, 78, 0, NOCHEEK, 24, 0, 28), POSE(0, 2, 92, 78, -30, -.1f, 78, 0, NOCHEEK, 24, 0, 28),
     380, .8f, 10, 8, 1500, 1.2f, .3f, 0, 0, 26, Accent::Drool, PupilShape::Round, 120, 0, false},
    // Starstruck: stars for pupils.
    {POSE(0, -8, 90, 84, -84, 0, 84, 0, NOCHEEK, 30, 0, -4), POSE(0, -8, 90, 84, -84, 0, 84, 0, NOCHEEK, 30, 0, -4),
     200, .55f, 8, 4, 2000, 4.f, 1.2f, 0, 0, 0, Accent::Sparkle, PupilShape::Star, 40, 0, false},
    // Laughing: tight arcs, shaking with it.
    {POSE(0, -8, 92, 70, -70, 0, 70, 0, -20, 0, 0, 0), POSE(0, -8, 92, 70, -70, 0, 70, 0, -20, 0, 0, 0),
     180, .6f, 0, 0, 3000, 6.f, 4.f, 2.4f, 0, 0, Accent::None, PupilShape::Round, 30, 0, false},
    // Crying: eyes shut and sagging, streams down.
    {POSE(4, 10, 86, 62, 40, -.22f, 62, 0, NOCHEEK, 0, 0, 40), POSE(4, 10, 86, 62, 40, -.22f, 62, 0, NOCHEEK, 0, 0, 40),
     600, .9f, 0, 0, 4000, 3.f, 1.5f, .8f, 0, 0, Accent::Stream, PupilShape::Round, 180, 0, false},
    // Relieved: lids sink, a small smile, slow exhale.
    {POSE(0, 4, 90, 70, 30, -.12f, 70, 0, 46, 16, 0, 40), POSE(0, 4, 90, 70, 30, -.12f, 70, 0, 46, 16, 0, 40),
     700, 1.f, 8, 4, 3200, 3.f, .15f, 0, 0, 4, Accent::None, PupilShape::Round, 200, 0, false},
    // Sneaky: thin slits, pupils flicking sideways.
    {POSE(4, 2, 92, 50, 10, .08f, 44, 0, NOCHEEK, 15, 0, 24), POSE(4, 2, 92, 50, 10, .08f, 44, 0, NOCHEEK, 15, 0, 24),
     240, .8f, 5, 40, 700, .4f, .3f, 0, 40, 0, Accent::None, PupilShape::Round, 60, 0, true},
    // Nostalgic: soft and far away, up and to the side.
    {POSE(0, -2, 88, 72, -8, -.14f, 70, 0, 60, 22, 0, -6), POSE(0, -2, 88, 72, -8, -.14f, 70, 0, 60, 22, 0, -6),
     900, 1.f, 8, 10, 4200, 2.f, .14f, 0, -28, -16, Accent::None, PupilShape::Round, 260, 0, true},
    // Playful: uneven eyes, big pupils, bouncing.
    {POSE(0, -6, 90, 78, -60, 0, 78, 0, 40, 24, 0, -6), POSE(0, 0, 90, 70, -10, .16f, 70, 0, 50, 22, 0, 8),
     200, .55f, 15, 20, 800, 5.f, 1.6f, 0, 8, 0, Accent::None, PupilShape::Round, 50, 20, false},
    // Amazed: fully round, pupils nearly filling the eye.
    {POSE(-2, -8, 90, 84, -84, 0, 84, 0, NOCHEEK, 34, 0, 0), POSE(-2, -8, 90, 84, -84, 0, 84, 0, NOCHEEK, 34, 0, 0),
     200, .5f, 4, 4, 2600, .8f, .3f, 0, 0, -4, Accent::None, PupilShape::Round, 40, 0, false},
    // Exhausted: uneven drooping lids, barely holding on.
    {POSE(0, 14, 88, 60, 34, -.1f, 60, 0, NOCHEEK, 19, 0, 34), POSE(0, 18, 88, 58, 42, -.1f, 58, 0, NOCHEEK, 19, 0, 36),
     1000, 1.f, 12, 4, 4000, 3.f, .1f, 0, 0, 14, Accent::Sweat, PupilShape::Round, 280, 0, false},
    // Suspicious: narrowed, watching from the corner, slowly checking both sides.
    {POSE(2, 0, 90, 62, 10, .06f, 48, 0, NOCHEEK, 15, 0, 22), POSE(2, 0, 90, 66, 2, .1f, 52, 0, NOCHEEK, 15, 0, 18),
     320, .85f, 5, 10, 1100, .5f, .2f, 0, 30, 0, Accent::None, PupilShape::Round, 90, 0, true},
};

const EmotionDef& emotionDef(Emotion e) { return kDefs[(int)e]; }

AnimationEngine::AnimationEngine() {
  cur_[0] = kDefs[0].left;
  cur_[1] = kDefs[0].right;
  memset(vel_, 0, sizeof(vel_));
}

void AnimationEngine::setEmotion(Emotion e) {
  if (e == emotion_) return;
  emotion_ = e;
  age_ = 0;
  soundPending_ = true;
  settled_ = false;
  nextSaccade_ = 300;
}

bool AnimationEngine::soundCueReady() {
  if (!soundPending_ || age_ < emotionDef(emotion_).soundDelayMs) return false;
  soundPending_ = false;
  return true;
}

void AnimationEngine::lookAt(float x, float y, float holdMs) {
  lookOffset((x - kScreenW / 2.f) * 0.3f, (y - kScreenH / 2.f) * 0.25f, holdMs);
}

void AnimationEngine::lookOffset(float dx, float dy, float holdMs) {
  lookX_ = dx; lookY_ = dy; lookHold_ = holdMs;
}

void AnimationEngine::independentLook(float lx, float ly, float rx, float ry, float holdMs) {
  indepX_[0] = lx; indepY_[0] = ly; indepX_[1] = rx; indepY_[1] = ry; indepHold_ = holdMs;
}

void AnimationEngine::blink(float ms, bool twice) {
  if (blinkT_ >= 0) return;
  blinkDur_ = ms; blinkT_ = 0; blinkTwice_ = twice;
}

void AnimationEngine::kick(float vy) { kickV_ = vy; }

void AnimationEngine::updateBlink(float dt) {
  const EmotionDef& d = emotionDef(emotion_);
  float rate = d.blinksPerMin * blinkScale_ * lerpf(0.75f, 1.25f, arousal_);
  if (blinkT_ < 0 && blinkPause_ <= 0 && rate > 0.01f) {
    nextBlink_ -= dt;
    if (nextBlink_ <= 0) {
      bool twice = rng_.range(0, 99) < d.doubleBlinkPct;
      float dur = emotion_ == Emotion::Sleepy ? 520.f : lerpf(260.f, 150.f, arousal_);
      blink(dur, twice);
      // Humans blink irregularly: Poisson-like spacing around the configured rate.
      float mean = 60000.f / rate;
      nextBlink_ = mean * rng_.rangef(0.45f, 1.55f);
    }
  }
  float amt = 0;
  if (blinkT_ >= 0) {
    blinkT_ += dt;
    float p = blinkT_ / blinkDur_;
    // Fast close, slower open, as real eyelids do.
    amt = p < 0.38f ? smooth01(p / 0.38f) : 1 - smooth01((p - 0.38f) / 0.62f);
    if (p >= 1) {
      amt = 0;
      blinkT_ = -1;
      if (blinkTwice_) { blinkTwice_ = false; blinkPause_ = blinkDur_ * 0.5f; }
    }
  } else if (blinkPause_ > 0) {
    blinkPause_ -= dt;
    if (blinkPause_ <= 0) blinkT_ = 0;
  }
  float tgt = forceClosed_ ? 1.f : 0.f;
  closedAmt_ += (tgt - closedAmt_) * clampf(dt / (forceClosed_ ? 420.f : 700.f), 0, 1);
  blinkAmt_ = amt > closedAmt_ ? amt : closedAmt_;
}

void AnimationEngine::updateGaze(float dt, const EmotionDef& d) {
  if (lookHold_ > 0) {
    lookHold_ -= dt;
    gazeTX_ = lookX_; gazeTY_ = lookY_;
    nextSaccade_ = 600;
  } else if (emotion_ == Emotion::Dizzy) {
    gazeTX_ = 26 * cosf(t_ * 0.007f);
    gazeTY_ = 22 * sinf(t_ * 0.007f);
  } else if (pursuit_ > 0) {
    pursuit_ -= dt;
    gazeTX_ = 42 * sinf(t_ * 0.0013f) + 14 * sinf(t_ * 0.0041f);
    gazeTY_ = 22 * sinf(t_ * 0.0019f + 1.f);
    nextSaccade_ = 500;
  } else {
    nextSaccade_ -= dt;
    if (nextSaccade_ <= 0) {
      if (d.wanderLook) {
        // Slow wandering stays on one side of the preferred direction, then switches.
        gazeTX_ = d.gazeX * (rng_.range(0, 2) == 0 ? -1.f : 1.f) + rng_.rangef(-d.saccadeAmp, d.saccadeAmp) * 0.5f;
        gazeTY_ = d.gazeY + rng_.rangef(-d.saccadeAmp, d.saccadeAmp) * 0.25f;
      } else {
        gazeTX_ = d.gazeX + rng_.rangef(-d.saccadeAmp, d.saccadeAmp);
        gazeTY_ = d.gazeY + rng_.rangef(-d.saccadeAmp, d.saccadeAmp) * 0.5f;
        if (rng_.range(0, 3) == 0) { gazeTX_ = d.gazeX; gazeTY_ = d.gazeY; }  // often returns to the viewer
      }
      nextSaccade_ = d.saccadeEveryMs * rng_.rangef(0.5f, 1.5f) * lerpf(1.4f, 0.7f, arousal_);
      // Big gaze shifts often come with a blink.
      if (fabsf(gazeTX_ - gazeX_) > 18 && rng_.range(0, 3) == 0) blink(150);
    }
  }
  // Fixation is never perfectly still: microsaccades.
  nextMicro_ -= dt;
  if (nextMicro_ <= 0) {
    microX_ = rng_.rangef(-2.6f, 2.6f);
    microY_ = rng_.rangef(-1.6f, 1.6f);
    nextMicro_ = rng_.rangef(280, 1200);
  }
  if (indepHold_ > 0) indepHold_ -= dt;
  // Saccades are fast: about 40 ms time constant.
  float k = clampf(dt / 42.f, 0, 1);
  // Eyes counter-rotate against a device tilt, so the creature keeps looking at the world.
  gazeX_ += (gazeTX_ + microX_ - tiltX_ * 16 - gazeX_) * k;
  gazeY_ += (gazeTY_ + microY_ - tiltY_ * 12 - gazeY_) * k;
}

void AnimationEngine::setDrive(float arousal, float valence, float light, float tiltX, float tiltY) {
  arousal_ = clampf(arousal, 0, 1);
  valence_ = clampf(valence, -1, 1);
  light_ = clampf(light, 0, 1);
  tiltX_ += (clampf(tiltX, -1, 1) - tiltX_) * 0.12f;
  tiltY_ += (clampf(tiltY, -1, 1) - tiltY_) * 0.12f;
}

// Smooth pseudo-noise from incommensurate sines: never repeats visibly and costs three sinf calls.
static inline float wander(float t, float seed) {
  return (sinf(t + seed) + 0.6f * sinf(t * 2.33f + seed * 1.7f) + 0.35f * sinf(t * 5.1f + seed * 2.9f)) / 1.95f;
}

// A head tilt in degrees that belongs to the emotion.
static float emotionRoll(Emotion e) {
  switch (e) {
    case Emotion::Curious: return 7;
    case Emotion::Confused: return -6;
    case Emotion::Shy: return 5;
    case Emotion::Dreamy: return 3;
    case Emotion::Winking: return -4;
    case Emotion::Sleepy: return 2.5f;
    case Emotion::Skeptical: return 5;
    case Emotion::Playful: return -6;
    case Emotion::Nostalgic: return -3;
    case Emotion::Lonely: return 4;
    case Emotion::Exhausted: return 6;
    case Emotion::Hopeful: return -3;
    default: return 0;
  }
}

void AnimationEngine::update(float dtMs) {
  float dt = clampf(dtMs, 0, 100) * speed_;
  t_ += dt;
  age_ += dt;
  const EmotionDef& d = emotionDef(emotion_);
  const EmotionDef& d2 = emotionDef(blendE_);
  blend_ += (blendTarget_ - blend_) * clampf(dt / 1800.f, 0, 1);
  updateBlink(dt);
  updateGaze(dt, d);

  // Spring toward the pose. Lids move ahead of the eye shape and the right eye lags slightly: no two
  // parameters arrive together, which is what removes the "preset" look of a transition.
  static const float kRate[kPoseFields] = {.8f, .8f, .9f, .9f, 1.25f, 1.25f, 1.1f, 1.1f, 1.f, 1.f, 1.f, 1.f};
  float omega = 4.6f / d.transitionMs;
  const float* tgtA[2] = {(const float*)&d.left, (const float*)&d.right};
  const float* tgtB[2] = {(const float*)&d2.left, (const float*)&d2.right};
  int steps = (int)(dt / 8.0f) + 1;
  float h = dt / steps;
  float err = 0;
  for (int s = 0; s < steps; s++) {
    for (int e = 0; e < 2; e++) {
      float* x = (float*)&cur_[e];
      float* v = (float*)&vel_[e];
      float eyeRate = e ? 0.93f : 1.f;
      // A closed eye or a "^" arc has no pupil to mix: blending would open it.
      float bl = tgtA[e][9] == 0 ? 0 : blend_;
      for (int i = 0; i < kPoseFields; i++) {
        float w = omega * kRate[i] * eyeRate;
        float target = lerpf(tgtA[e][i], tgtB[e][i], bl);
        v[i] += (w * w * (target - x[i]) - 2 * d.damping * w * v[i]) * h;
        x[i] += v[i] * h;
        if (s == steps - 1) err += fabsf(target - x[i]);
      }
    }
  }
  settled_ = err < 1.5f;

  // Body: breathing, sway and jitter, all scaled by arousal.
  float ts = t_ * 0.001f;
  const float tau = 6.2831853f;
  float bob = d.bobAmp * sinf(ts * tau * d.bobHz);
  float breathHz = lerpf(0.14f, 0.34f, arousal_);
  float breath = lerpf(2.0f, 1.0f, arousal_) * sinf(ts * tau * breathHz);
  float sway = 2.2f * wander(ts * 0.31f, 1.3f);
  float tx = 0, ty = 0;
  if (d.tremble > 0) {
    tx = d.tremble * sinf(ts * 91.f) * sinf(ts * 13.f);
    ty = d.tremble * 0.6f * sinf(ts * 77.f + 1.f);
  }
  kickV_ += (-0.00012f * kickY_ - 0.012f * kickV_) * dt;
  kickY_ += kickV_ * dt;
  bodyX_ = sway + tx + tiltX_ * 7;
  bodyY_ = bob + breath + ty + kickY_ + tiltY_ * 6;
  // Volume preservation: the eye stretches when thrown and when the gaze snaps.
  float gazeSpeed = dt > 0 ? (fabsf(gazeX_ - lastGX_) + fabsf(gazeY_ - lastGY_)) / dt : 0;
  lastGX_ = gazeX_; lastGY_ = gazeY_;
  float want = clampf(-kickV_ * 0.5f, -0.1f, 0.12f) + clampf(gazeSpeed * 0.05f, 0, 0.05f);
  squash_ += (want - squash_) * clampf(dt / 40.f, 0, 1);
  float rollTarget = (emotionRoll(emotion_) + wander(ts * 0.17f, 9.f) * 2.4f + tiltX_ * 9) * 0.0174533f;
  roll_ += (rollTarget - roll_) * clampf(dt / 320.f, 0, 1);
  composeLayers();
}

void AnimationEngine::composeLayers() {
  float ts = t_ * 0.001f;
  float amp = lerpf(0.6f, 1.2f, arousal_);
  float rollSlope = roll_;  // small angle: tan(a) = a
  for (int e = 0; e < 2; e++) {
    EyePose p = cur_[e];
    float side = e ? 1.f : -1.f;     // screen side, pose space is mirrored
    // Lids breathe on their own and follow the gaze, as real lids do when looking down.
    // A wide open eye has its lid above the ellipse: pushing it down there would flatten the top.
    float lidFree = smooth01((p.up + p.ry * 0.9f) / (p.ry * 0.5f));
    p.up += (wander(ts * 0.55f, 1.3f + e * 2.1f) * 2.4f * amp + (1 - arousal_) * 7.f + gazeY_ * 0.32f) * lidFree;
    p.upSlope += wander(ts * 0.31f, 4.f + e) * 0.02f - valence_ * 0.04f;
    if (p.cheek < p.ry) p.cheek -= (valence_ > 0 ? valence_ : 0) * 4.f;
    if (p.pupilR > 1) {
      // Pupil: hippus noise, darkness and interest dilate it.
      p.pupilR *= (1 + 0.07f * wander(ts * 0.9f, 7.f)) * lerpf(1.18f, 0.9f, light_) * (1 + 0.14f * (arousal_ - 0.5f));
    }
    p.ry *= 1 + squash_;
    p.rx *= 1 - squash_ * 0.7f;
    // Whole head rolls: one eye up, the other down, lids rotate along.
    p.dy += side * kEyeX * roll_;
    p.upSlope += -side * rollSlope;
    p.loSlope += -side * rollSlope;
    if (emotion_ == Emotion::Yawning) {
      float ph = clampf(age_ / 2600.f, 0, 1);
      float q = sinf(ph * 3.14159f);
      p.up = lerpf(p.up, 58, q * q);
      p.ry *= 1 + 0.12f * q;
      p.rx *= 1 - 0.05f * q;
      p.dy += 8 * q;
    }
    out_[e] = p;
  }
}

// ---- drawing ---------------------------------------------------------------------------------

void FaceEngine::drawEye(Canvas& c, const AnimationEngine& a, int eye, float ox, float oy, float sc, uint16_t color) const {
  const EyePose& p = eye == 0 ? a.leftPose() : a.rightPose();
  float mirror = eye == 0 ? 1.f : -1.f;  // +x in pose space is toward the nose
  // Nearest-neighbour blending of the emotion's pose is done by springs, so values may overshoot.
  float ry = p.ry < 8 ? 8 : p.ry, rx = p.rx < 8 ? 8 : p.rx;
  float cx = ox + (-mirror * kEyeX + mirror * p.dx + a.bodyX()) * sc;
  float cy = oy + (kEyeY + p.dy + a.bodyY()) * sc;

  float up = p.up, lo = p.lo;
  // Blink lowers the upper lid to a resting arc on the lower edge of the eye.
  float closed = ry - 11.f;
  float b = a.blinkAmount();
  up = lerpf(up, closed, b);
  float slope = p.upSlope * (1 - b);
  float gx = a.gazeX() + a.eyeGazeOffset(eye, false);
  float gy = a.gazeY() + a.eyeGazeOffset(eye, true);

  EyeShape s;
  s.cx = cx; s.cy = cy; s.rx = rx * sc; s.ry = ry * sc;
  s.upY = cy + up * sc; s.upSlope = slope * mirror;
  s.loY = cy + lo * sc; s.loSlope = p.loSlope * mirror;
  // Always the same cut: a far-away cheek simply misses the eye, so it can rise smoothly.
  s.cheekY = cy + p.cheek * sc;
  s.cheekRx = rx * 1.18f * sc;
  s.cheekRy = ry * 1.2f * sc;
  c.eye(s, color);

  if (p.pupilR < 1 || b > 0.72f) return;
  float px = cx + (mirror * p.px + gx * 0.75f) * sc;
  float py = cy + (p.py + gy * 0.75f) * sc;
  // Keep the pupil on the eye even at extreme gaze.
  float nx = (px - cx) / (s.rx - 6 * sc), ny = (py - cy) / (s.ry - 6 * sc);
  float len = sqrtf(nx * nx + ny * ny);
  if (len > 1) { px = cx + (px - cx) / len; py = cy + (py - cy) / len; }
  float r = p.pupilR * sc * (1 - b * 0.25f);
  PupilShape shape = emotionDef(a.emotion()).pupil;
  if (shape == PupilShape::Heart && a.emotionAge() > 40) {
    float beat = 1 + 0.08f * sinf(a.timeMs() * 0.0075f);
    c.heart(px, py - r * 0.2f, r * 0.9f * beat, kBlack);
  } else if (shape == PupilShape::Star) {
    // Four tapered rays from a centre, slowly turning and twinkling.
    float rot = a.timeMs() * 0.0012f, tw = r * (1 + 0.15f * sinf(a.timeMs() * 0.009f));
    for (int k = 0; k < 4; k++) {
      float ang = rot + k * 1.5708f, ca = cosf(ang), sa = sinf(ang);
      c.line(px - ca * tw * 0.45f, py - sa * tw * 0.45f, px + ca * tw, py + sa * tw, 11 * sc, kBlack);
      c.line(px + ca * tw * 0.3f, py + sa * tw * 0.3f, px + ca * tw * 1.1f, py + sa * tw * 1.1f, 5 * sc, kBlack);
    }
    c.fillCircle(px, py, r * 0.32f, kBlack);
  } else if (shape == PupilShape::Spiral) {
    // Rings wobbling out of phase read as a spinning spiral.
    float w = a.timeMs() * 0.009f;
    c.ring(px + 3 * sc * cosf(w), py + 3 * sc * sinf(w), r, 3.5f * sc, kBlack);
    c.ring(px - 2 * sc * cosf(w), py - 2 * sc * sinf(w), r * 0.55f, 3 * sc, kBlack);
    c.fillCircle(px + 2 * sc * cosf(w * 1.3f), py + 2 * sc * sinf(w * 1.3f), r * 0.16f, kBlack);
  } else {
    c.fillCircle(px, py, r, kBlack);
  }
}

void FaceEngine::drawAccent(Canvas& c, const AnimationEngine& a, float ox, float oy, float sc) const {
  const EmotionDef& d = emotionDef(a.emotion());
  float t = a.emotionAge();
  float ts = a.timeMs() * 0.001f;
  if (d.accent == Accent::None || sc < 0.8f) return;
  switch (d.accent) {
    case Accent::Zzz: {
      for (int i = 0; i < 3; i++) {
        float ph = fmodf(ts * 0.22f + i * 0.33f, 1.f);
        if (t < 1200) break;
        int x = (int)(ox + 96 + ph * 70 + 8 * sinf(ph * 9));
        int y = (int)(oy - 70 - ph * 90);
        c.text(x, y, "z", i == 1 ? FontSize::L : FontSize::M, ph > 0.8f ? kDim : kAccent);
      }
      break;
    }
    case Accent::Tear: {
      float ph = fmodf(ts * 0.28f, 1.f);
      if (t < 900 || ph > 0.75f) break;
      float y = oy + 46 + ph * 120, x = ox - 150;
      c.fillCircle(x, y, 7, kAccent);
      c.line(x, y - 11, x, y - 3, 4, kAccent);
      break;
    }
    case Accent::Hearts: {
      for (int i = 0; i < 2; i++) {
        float ph = fmodf(ts * 0.3f + i * 0.5f, 1.f);
        if (ph > 0.85f) continue;
        float x = ox + (i ? 110.f : -118.f) + 10 * sinf(ph * 7);
        c.heart(x, oy - 150 - ph * 44, 11 + 5 * sinf(ph * 3.1f), kAccent);
      }
      break;
    }
    case Accent::Bang:
      if (t < 800) {
        float grow = smooth01(t / 90);
        c.roundRect(ox - 5, oy - 186 * grow, 10, 42 * grow, 5, kSoft);
        c.fillCircle(ox, oy - 132 * grow, 6 * grow, kSoft);
      }
      break;
    case Accent::Question:
      if (fmodf(ts * 0.35f, 1.f) < 0.55f)
        c.text((int)ox + 6, (int)oy - 170, "?", FontSize::L, kSoft, Align::Center);
      break;
    case Accent::Sparkle:
      for (int i = 0; i < 3; i++) {
        float ph = fmodf(ts * 0.9f + i * 0.37f, 1.f);
        float sz = 9 * sinf(ph * 3.14159f);
        float x = ox + (i == 0 ? -128.f : i == 1 ? 132.f : 4.f), y = oy + (i == 2 ? -176.f : -158.f);
        c.line(x - sz, y, x + sz, y, 3, kSoft);
        c.line(x, y - sz, x, y + sz, 3, kSoft);
      }
      break;
    case Accent::Steam:
      for (int i = 0; i < 2; i++) {
        float ph = fmodf(ts * 1.1f + i * 0.5f, 1.f);
        float x = ox + (i ? 40.f : -40.f), y = oy - 104 - ph * 44;
        float r = 3 + ph * 7;
        c.ring(x, y, r, 3, ph > 0.7f ? kDim : kAccent);
      }
      break;
    case Accent::Blush:
      for (int i = 0; i < 2; i++) {
        float x = ox + (i ? 122.f : -122.f);
        for (int k = 0; k < 3; k++) c.line(x - 16 + k * 14, oy + 58, x - 22 + k * 14, oy + 70, 4, kRose);
      }
      break;
    case Accent::Sweat: {
      float ph = fmodf(ts * 0.42f, 1.f);
      float y = oy - 100 + ph * 150, x = ox + 196 - ph * 8;
      c.fillCircle(x, y, 6, kSoft);
      c.line(x, y - 10, x, y - 2, 3.5f, kSoft);
      break;
    }
    case Accent::Stream:
      for (int i = 0; i < 2; i++) {
        float x = ox + (i ? 110.f : -110.f);
        for (int k = 0; k < 4; k++) {
          float ph = fmodf(ts * 0.9f + k * 0.25f, 1.f);
          c.fillCircle(x + 4 * sinf(ph * 6 + i), oy + 34 + ph * 150, 6 - ph * 3, kAccent);
        }
      }
      break;
    case Accent::Drool: {
      float ph = fmodf(ts * 0.35f, 1.f);
      float x = ox - 112, y = oy + 42 + ph * ph * 34;
      c.line(x, oy + 36, x, y, 3, kSoft);
      c.fillCircle(x, y, 4.5f, kSoft);
      break;
    }
    case Accent::Frost:
      for (int i = 0; i < 5; i++) {
        float ph = fmodf(ts * 0.2f + i * 0.21f, 1.f);
        float x = ox - 130 + i * 65 + 8 * sinf(ts * 2 + i), y = oy - 170 + ph * 40;
        float sz = 5;
        c.line(x - sz, y, x + sz, y, 2, kWhite);
        c.line(x, y - sz, x, y + sz, 2, kWhite);
      }
      break;
    case Accent::Dots:
      for (int i = 0; i < 3; i++) {
        float ph = fmodf(ts * 0.8f - i * 0.18f, 1.f);
        float s = 3.5f + 2.5f * sinf(ph * 6.283f);
        c.fillCircle(ox - 16 + i * 16, oy - 130, s, kSoft);
      }
      break;
    default: break;
  }
}

void FaceEngine::draw(Canvas& c, const AnimationEngine& a, uint16_t color) const {
  drawScaled(c, a, kScreenW / 2.f, kScreenH / 2.f, 1.f, color);
}

void FaceEngine::drawScaled(Canvas& c, const AnimationEngine& a, float cx, float cy, float sc, uint16_t color) const {
  drawEye(c, a, 0, cx, cy, sc, color);
  drawEye(c, a, 1, cx, cy, sc, color);
  drawAccent(c, a, cx, cy, sc);
}

void FaceEngine::drawWeather(Canvas& c, Weather w, float tMs, bool night) const {
  const float t = tMs;
  auto hash = [](int i, float k) { return fmodf(sinf(i * 12.9898f + k) * 43758.5f, 1.f); };
  auto frac01 = [](float v) { v = fmodf(v, 1.f); return v < 0 ? v + 1 : v; };
  if (w == Weather::Clear && night) {
    for (int i = 0; i < 26; i++) {
      float x = frac01(hash(i, 1.f)) * kScreenW, y = frac01(hash(i, 2.f)) * kScreenH;
      float tw = 0.5f + 0.5f * sinf(t * 0.0016f * (0.5f + frac01(hash(i, 3.f))) + i);
      c.fillRect((int)x, (int)y, tw > 0.8f ? 3 : 2, tw > 0.8f ? 3 : 2, tw > 0.5f ? kGray : kDim);
    }
  }
  if (w == Weather::Clouds || w == Weather::Storm || w == Weather::Rain) {
    for (int i = 0; i < 4; i++) {
      float x = frac01(i * 0.27f + t * 0.000012f * (1 + i)) * 620 - 80, y = 54.f + i * 15;
      c.fillCircle(x, y, 34.f + i * 5, kDark);
      c.fillCircle(x + 42, y + 6, 26.f + i * 4, kDark);
      c.fillCircle(x - 38, y + 8, 22, kDark);
    }
  }
  if (w == Weather::Rain || w == Weather::Storm) {
    int n = w == Weather::Storm ? 34 : 22;
    float slant = w == Weather::Storm ? 8.f : 3.f;
    for (int i = 0; i < n; i++) {
      float sp = 0.42f + 0.25f * frac01(hash(i, 5.f));
      float y = frac01(t * sp / 520.f + frac01(hash(i, 6.f))) * 520 - 30;
      float x = frac01(hash(i, 7.f)) * kScreenW - slant * y / 40;
      c.line(x, y, x + slant, y + 15, 1.6f, kDim);
    }
  }
  if (w == Weather::Snow) {
    for (int i = 0; i < 26; i++) {
      float sp = 0.03f + 0.05f * frac01(hash(i, 8.f));
      float y = frac01(t * sp / 500.f + frac01(hash(i, 9.f))) * 500 - 16;
      float x = frac01(hash(i, 10.f)) * kScreenW + 16 * sinf(t * 0.001f + i);
      c.fillCircle(x, y, 1.6f + 1.8f * frac01(hash(i, 11.f)), kGray);
    }
  }
  if (w == Weather::Fog) {
    for (int i = 0; i < 5; i++) {
      float y = 90.f + i * 72 + 8 * sinf(t * 0.0006f + i);
      c.line(30 + 40 * sinf(t * 0.0004f + i), y, 436 - 40 * sinf(t * 0.0005f + i * 2), y, 9, kDark);
    }
  }
  if (w == Weather::Wind) {
    for (int i = 0; i < 8; i++) {
      float x = frac01(t * 0.0004f * (1 + 0.3f * i) + i * 0.23f) * 640 - 90;
      float y = 60.f + i * 48 + 10 * sinf(t * 0.002f + i);
      c.line(x, y, x + 60, y - 3, 2, kDim);
    }
  }
}

Rect FaceEngine::bounds(const AnimationEngine&) const {
  // Includes room for accents and overshoot. The CO5300 needs even start and width.
  return Rect{0, 20, kScreenW, kScreenH - 60};
}

}  // namespace cc
