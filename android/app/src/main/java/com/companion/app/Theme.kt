package com.companion.app

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.Easing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.keyframes
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.Image

/** Design v2 «Ночь»: tokens from the handoff README, nothing else in the app picks its own colours. */
object Night {
    val Bg = Color(0xFF0F0F12)
    val Surface = Color(0xFF1B1B20)
    val Bubble = Color(0xFF1D1D23)
    val Hover = Color(0xFF26262E)
    val Line = Color(0xFF2C2C33)
    val Line2 = Color(0xFF3A3A44)
    val Field = Color(0xFF151519)
    val Text = Color(0xFFF2EFE8)
    val Text2 = Color(0xFFA19EA6)
    val Muted = Color(0xFF8D8A93)
    val Quiet = Color(0xFF6D6A72)
    val Disabled = Color(0xFF4A4850)
    val Chip = Color(0xFFCDC9C0)
    val Accent = Color(0xFFAAB3DC)
    val OnAccent = Color(0xFF15151A)
    val Pill = Color(0xFF34334A)
    val AccentLight = Color(0xFFCDD3EF)
    val Eye = Color(0xFFEFEADF)
    val Fill = Color(0xFFE6E0CF)
    val Low = Color(0xFFD9A1AD)
    val Online = Color(0xFF9FD8B8)
    val Rose = Color(0xFFE8A8B6)
    val Sand = Color(0xFFE6D6A8)
    val Players = listOf(Accent, Rose, Color(0xFFA9D8C0), Sand)
    val Computers = listOf(Color(0xFF5D5B63), Color(0xFF77747D), Color(0xFF8D8A93))
}

val Onest = FontFamily(
    Font(R.font.onest_regular, FontWeight.Normal),
    Font(R.font.onest_medium, FontWeight.Medium),
    Font(R.font.onest_semibold, FontWeight.SemiBold),
    Font(R.font.onest_semibold, FontWeight.Bold), // no 700 in assets/fonts: semibold is the heaviest cut
)

val NightColors = darkColorScheme(
    primary = Night.Accent, onPrimary = Night.OnAccent, secondary = Night.Fill, background = Night.Bg, surface = Night.Surface,
    onBackground = Night.Text, onSurface = Night.Text, surfaceVariant = Night.Surface, surfaceContainer = Night.Surface,
)

/** cubic-bezier(.3,1.3,.5,1): the slight overshoot of the nav pill and the face. */
val Springy: Easing = CubicBezierEasing(0.3f, 1.3f, 0.5f, 1f)
val Ease: Easing = CubicBezierEasing(0.25f, 0.1f, 0.25f, 1f)

/** Entrance of a screen: `rise` (from below, 450 ms) for tabs, `slideIn` (from the right, 350 ms) for inner screens. */
fun Modifier.enter(dx: Dp = 0.dp, dy: Dp = 14.dp, ms: Int = 450): Modifier = composed {
    val p = remember { Animatable(0f) }
    LaunchedEffect(Unit) { p.animateTo(1f, tween(ms, easing = Ease)) }
    graphicsLayer {
        alpha = p.value
        translationX = dx.toPx() * (1 - p.value)
        translationY = dy.toPx() * (1 - p.value)
    }
}

fun Modifier.slideIn(): Modifier = enter(dx = 28.dp, dy = 0.dp, ms = 350)

/** `pop`: scale .7 → 1.05 → 1 while fading in. */
fun Modifier.pop(ms: Int = 300, originX: Float = 0.5f, originY: Float = 0.5f): Modifier = composed {
    val p = remember { Animatable(0f) }
    LaunchedEffect(Unit) { p.animateTo(1f, tween(ms, easing = Ease)) }
    graphicsLayer {
        val t = p.value
        val s = if (t < 0.6f) 0.7f + 0.35f * (t / 0.6f) else 1.05f - 0.05f * ((t - 0.6f) / 0.4f)
        scaleX = s; scaleY = s
        alpha = (t / 0.6f).coerceAtMost(1f)
        transformOrigin = androidx.compose.ui.graphics.TransformOrigin(originX, originY)
    }
}

/** Opacity 1 → .3 → 1, as the connection dot and the recording dot. */
@Composable
fun pulse(periodMs: Int): Float {
    val t = rememberInfiniteTransition(label = "pulse")
    return t.animateFloat(1f, 1f, infiniteRepeatable(keyframes {
        durationMillis = periodMs
        0.3f at periodMs / 2 using Ease
    }), label = "pulse").value
}

/** Scale on press: .9 pressed, back with a little bounce. */
fun Modifier.pressScale(source: MutableInteractionSource, pressed: Float = 0.9f): Modifier = composed {
    val down by source.collectIsPressedAsState()
    val s by animateFloatAsState(if (down) pressed else 1f, spring(dampingRatio = 0.45f, stiffness = 700f), label = "press")
    graphicsLayer { scaleX = s; scaleY = s }
}

fun Modifier.tap(onClick: () -> Unit): Modifier = composed {
    val src = remember { MutableInteractionSource() }
    pressScale(src, 0.97f).clickable(src, null, role = Role.Button, onClick = onClick)
}

/**
 * The companion's eyes, rendered by the real core (tools/gen_emotion_icons.py): the app shows the face the device makes.
 * The device blinks on its own; here the picture blinks every 4.6 s like the mockup.
 */
@Composable
fun Eyes(emotion: String, width: Dp, modifier: Modifier = Modifier, blink: Boolean = true, color: Color = Night.Eye) {
    val t = rememberInfiniteTransition(label = "blink")
    val sy = if (!blink) 1f else t.animateFloat(1f, 1f, infiniteRepeatable(keyframes {
        durationMillis = 4600
        1f at 4186; 0.08f at 4324; 1f at 4462
    }), label = "blink").value
    Image(painterResource(EMOTION_ICONS[emotion] ?: R.drawable.emo_neutral), null,
        modifier.width(width).aspectRatio(192f / 180f).graphicsLayer { scaleY = sy }, colorFilter = ColorFilter.tint(color))
}

/** Black pill or circle with the live face: header, chat list, dialog. */
@Composable
fun FaceBadge(emotion: String, w: Dp, h: Dp, eyes: Dp, modifier: Modifier = Modifier) {
    Box(modifier.size(w, h).clip(RoundedCornerShape(h / 2)).background(Color.Black).border(1.dp, Color(0xFF2A2A30), RoundedCornerShape(h / 2)),
        contentAlignment = Alignment.Center) { Eyes(emotion, eyes) }
}

@Composable
fun MIcon(icon: ImageVector, size: Dp, tint: Color = Night.Text, modifier: Modifier = Modifier, description: String? = null) =
    Icon(icon, description, modifier.size(size), tint = tint)

/** Round icon button without a fill until pressed, 40–42 dp: back arrows and the settings gear. */
@Composable
fun RoundIconButton(icon: ImageVector, description: String, size: Dp = 42.dp, tint: Color = Night.Text, onClick: () -> Unit) {
    val src = remember { MutableInteractionSource() }
    val down by src.collectIsPressedAsState()
    Box(Modifier.size(size).clip(CircleShape).background(if (down) Night.Bubble else Color.Transparent)
        .clickable(src, null, role = Role.Button, onClick = onClick), contentAlignment = Alignment.Center) {
        MIcon(icon, 24.dp, tint, description = description)
    }
}

/** Back arrow and a title: every inner screen starts with it. */
@Composable
fun BackTitle(title: String, size: Int = 21, onBack: () -> Unit, trailing: @Composable RowScope.() -> Unit = {}) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        RoundIconButton(Icons.AutoMirrored.Rounded.ArrowBack, "Назад", onClick = onBack)
        Text(title, fontSize = size.sp, fontWeight = FontWeight.SemiBold)
        trailing()
    }
}

/** Pill button of the games: primary is lavender, the rest are outlined, `enabled = false` greys out. */
@Composable
fun PillButton(label: String, modifier: Modifier = Modifier, primary: Boolean = false, enabled: Boolean = true, height: Dp = 48.dp,
               icon: ImageVector? = null, fontSize: Int = 15, onClick: () -> Unit) {
    val src = remember { MutableInteractionSource() }
    val bg = if (primary && enabled) Night.Accent else Color.Transparent
    val fg = when { !enabled -> Night.Disabled; primary -> Night.OnAccent; else -> Night.AccentLight }
    Row(modifier.height(height).pressScale(src, 0.97f).clip(RoundedCornerShape(height / 2)).background(bg)
        .then(if (primary && enabled) Modifier else Modifier.border(1.dp, if (enabled) Night.Line2 else Color(0xFF26262C), RoundedCornerShape(height / 2)))
        .clickable(src, null, enabled = enabled, role = Role.Button, onClick = onClick).padding(horizontal = 20.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.CenterHorizontally), verticalAlignment = Alignment.CenterVertically) {
        icon?.let { MIcon(it, 24.dp, fg) }
        Text(label, color = fg, fontSize = fontSize.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
    }
}

/** Text field of the design: a 50 dp pill, outline #2C2C33 on #151519. */
@Composable
fun NightField(value: String, onChange: (String) -> Unit, placeholder: String, modifier: Modifier = Modifier,
               keyboard: KeyboardOptions = KeyboardOptions.Default, actions: KeyboardActions = KeyboardActions.Default,
               visual: VisualTransformation = VisualTransformation.None) {
    BasicTextField(value, onChange, modifier.height(50.dp), singleLine = true, keyboardOptions = keyboard, keyboardActions = actions,
        visualTransformation = visual, cursorBrush = SolidColor(Night.Accent),
        textStyle = TextStyle(color = Night.Text, fontSize = 15.sp, fontFamily = Onest),
        decorationBox = { inner ->
            Box(Modifier.fillMaxSize().clip(RoundedCornerShape(25.dp)).background(Night.Field).border(1.dp, Night.Line, RoundedCornerShape(25.dp))
                .padding(horizontal = 20.dp), contentAlignment = Alignment.CenterStart) {
                if (value.isEmpty()) Text(placeholder, color = Night.Quiet, fontSize = 15.sp, maxLines = 1)
                inner()
            }
        })
}

/** Segmented choice: the active option is lavender. */
@Composable
fun <T> Segments(options: List<Pair<String, T>>, selected: T, icon: ImageVector? = null, fontSize: Int = 17, onSelect: (T) -> Unit) {
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(Night.Surface).padding(4.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        options.forEach { (label, value) ->
            val on = value == selected
            val bg by androidx.compose.animation.animateColorAsState(if (on) Night.Accent else Color.Transparent, tween(250), label = "seg")
            val fg = if (on) Night.OnAccent else Night.Text2
            Row(Modifier.weight(1f).height(46.dp).clip(RoundedCornerShape(14.dp)).background(bg).clickable { onSelect(value) },
                horizontalArrangement = Arrangement.spacedBy(4.dp, Alignment.CenterHorizontally), verticalAlignment = Alignment.CenterVertically) {
                icon?.let { MIcon(it, 18.dp, fg) }
                Text(label, color = fg, fontWeight = FontWeight.SemiBold, fontSize = fontSize.sp)
            }
        }
    }
}

/** Animation progress 0..1 restarted whenever [key] changes; null key means idle at 1. */
@Composable
fun progressOf(key: Any?, ms: Int, easing: Easing = LinearEasing): Float {
    val p = remember { Animatable(1f) }
    LaunchedEffect(key) { if (key != null) { p.snapTo(0f); p.animateTo(1f, tween(ms, easing = easing)) } }
    return p.value
}

/** Evenly spaced keyframes like CSS `0%,25%,50%…`: linear between the given values. */
fun frames(t: Float, vararg v: Float): Float {
    if (v.size == 1) return v[0]
    val x = t.coerceIn(0f, 1f) * (v.size - 1)
    val i = x.toInt().coerceAtMost(v.size - 2)
    return v[i] + (v[i + 1] - v[i]) * (x - i)
}
