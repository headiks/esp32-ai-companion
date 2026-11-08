package com.companion.app

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.random.Random

fun clockNow(): String = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())

/** What the companion's screen shows on top of the face: a sender and a text, or a short reaction. */
class Bubble(val text: String, val from: String, val voice: Boolean = false) {
    val short get() = from.isEmpty() && text.length <= 16
}

enum class FxKind { Heart, Spark, Food, Crumb, Ball, Chip }

class Fx(val kind: FxKind, val dx: Float = 0f, val dy: Float = 0f, val size: Float = 0f, val delay: Int = 0, val text: String = "",
         val icon: ImageVector? = null)

/** Keyframed nudges of the whole face, as in the mockup (all with the springy curve). */
enum class Nudge(val ms: Int) { Chew(1000), Shake(450), Squash(700), Jump(700), Wake(500), Angry(400), Poke(450), Bounce(400) }

/**
 * The hero is the device's screen: the same text and reactions. Lives above the tabs, so an answer that came while the
 * chat was open is still on the face when the pet tab comes back.
 */
class Stage(private val scope: CoroutineScope) {
    var bubble by mutableStateOf<Bubble?>(null); private set
    var typed by mutableIntStateOf(0); private set
    val fx = mutableStateListOf<Fx>()
    var nudge by mutableStateOf<Pair<Nudge, Long>?>(null); private set
    var look by mutableStateOf(Offset.Zero)
    var hold = false
    private var lockLook = false
    private var job: Job? = null

    /** Shown for max(2.2 s, 55 ms a letter + 1.6 s) and longer while a finger rests on the text. */
    fun say(text: String, ms: Long? = null, from: String = "", voice: Boolean = false) {
        val b = Bubble(text, from, voice)
        bubble = b
        typed = 0
        job?.cancel()
        job = scope.launch {
            launch { while (typed < text.length) { delay(28); typed += 2 } }
            delay(ms ?: maxOf(2200L, text.length * 55L + 1600))
            while (hold) delay(800)
            if (bubble === b) bubble = null
        }
    }

    fun add(items: List<Fx>, ms: Long) {
        fx.addAll(items)
        scope.launch { delay(ms); fx.removeAll(items) }
    }

    fun play(n: Nudge) { nudge = n to System.nanoTime() }

    fun later(ms: Long, body: () -> Unit) { scope.launch { delay(ms); body() } }

    /** Eyes follow something: steps of the look every 520 ms, then free again. */
    fun follow(points: List<Offset>, ms: Long) {
        lockLook = true
        points.forEachIndexed { i, p -> later(i * 520L) { look = p } }
        later(ms) { lockLook = false }
    }

    suspend fun lookAround(asleep: () -> Boolean) {
        while (true) {
            delay(2300)
            if (lockLook) continue
            look = if (asleep()) Offset.Zero else Offset((Random.nextFloat() * 2 - 1) * 4, (Random.nextFloat() * 2 - 1) * 2.5f)
        }
    }

    fun hearts(n: Int, small: Boolean) = List(n) {
        Fx(FxKind.Heart, (Random.nextFloat() - .5f) * (if (small) 80 else 170), -(40 + Random.nextFloat() * (if (small) 40 else 90)),
            (if (small) 10 else 14) + Random.nextFloat() * 14, Random.nextInt(300))
    }

    /** "+22 сытость": real changes the device reported, the big ones only. */
    fun chips(before: Map<String, Int>, after: Map<String, Int>) {
        val changed = NEEDS.mapNotNull { n -> val d = (after[n.key] ?: 0) - (before[n.key] ?: 0); if (kotlin.math.abs(d) >= 4) n to d else null }.take(3)
        add(changed.mapIndexed { i, (n, d) -> Fx(FxKind.Chip, (i - 1) * 6f, delay = i * 180, text = "${if (d > 0) "+" else "−"}${kotlin.math.abs(d)} ${n.chip}") }, 2000)
    }
}

/** Every bar is "full is good": hunger turns into fullness. */
class Need(val key: String, val title: String, val chip: String, val icon: ImageVector)

val NEEDS = listOf(
    Need("happiness", "Счастье", "счастье", Icons.Rounded.Mood), Need("food", "Сытость", "сытость", Icons.Rounded.Restaurant),
    Need("energy", "Энергия", "энергия", Icons.Rounded.Bolt), Need("fun", "Веселье", "веселье", Icons.Rounded.Celebration),
    Need("health", "Здоровье", "здоровье", Icons.Rounded.Favorite),
)

fun needs(s: PetState): Map<String, Int> = mapOf(
    "happiness" to (s.stats["happiness"] ?: 0), "food" to 100 - (s.stats["hunger"] ?: 100), "energy" to (s.stats["energy"] ?: 0),
    "fun" to (s.stats["fun"] ?: 0), "health" to (s.stats["health"] ?: 0),
)

fun PetState.asleep() = status == "asleep" || emotion == "sleeping"

private val FOODS = listOf(Icons.Rounded.Cookie, Icons.Rounded.Icecream, Icons.Rounded.LunchDining, Icons.Rounded.BakeryDining,
    Icons.Rounded.EggAlt, Icons.Rounded.Egg)

@Composable
fun PetTab(c: BleClient, s: PetState, stage: Stage) {
    val asleep = s.asleep()
    val food = needs(s)["food"] ?: 0
    // The action, then the state at once: the chips show what the device really changed.
    fun act(v: String) { c.send(Protocol.command("action", "value" to v)); c.send(Protocol.command("get_state")) }
    fun chipsLater(ms: Long) { val before = needs(c.state.value); stage.later(ms) { stage.chips(before, needs(c.state.value)) } }
    fun feed() {
        if (asleep) return stage.say("Zzz…")
        if (food > 92) { stage.say("Я сыт!"); stage.play(Nudge.Shake); return }
        chipsLater(760)
        act("feed")
        stage.add(listOf(Fx(FxKind.Food, (Random.nextFloat() - .5f) * 60, icon = FOODS.random())) +
            List(7) { Fx(FxKind.Crumb, (Random.nextFloat() - .5f) * 80, 10 + Random.nextFloat() * 40, delay = 720) }, 1700)
        stage.later(760) { stage.say(listOf("Ням-ням!", "Вкусно!", "Хрум-хрум").random()); stage.play(Nudge.Chew) }
    }
    fun pet() {
        chipsLater(300)
        act("pet")
        if (asleep) { stage.add(stage.hearts(3, true), 1600); stage.say("мрр…"); return }
        stage.say(listOf("Мурр…", "Ещё погладь!", "Приятно").random())
        stage.add(stage.hearts(9, false), 1700)
        stage.play(Nudge.Squash)
    }
    fun play() {
        if (asleep) return stage.say("Zzz…")
        if ((s.stats["energy"] ?: 0) < 15) return stage.say("Нет сил…")
        chipsLater(2250)
        act("play")
        stage.add(listOf(Fx(FxKind.Ball)), 2300)
        stage.follow(listOf(Offset(-4f, 2f), Offset(-2f, -3f), Offset(1f, 3f), Offset(4f, -3f), Offset(4f, 2f)), 2250)
        stage.later(2250) {
            stage.say(listOf("Ещё!", "Поймал!", "Ура!").random())
            stage.add(List(7) { Fx(FxKind.Spark, (Random.nextFloat() - .5f) * 170, -(30 + Random.nextFloat() * 80), 12 + Random.nextFloat() * 10, Random.nextInt(200)) }, 1500)
            stage.play(Nudge.Jump)
        }
    }
    fun sleep() {
        if (!asleep) { act("sleep"); stage.look = Offset.Zero; stage.say("Спокойной ночи", 1800) }
        else { act("wake"); stage.say("Я проснулся!"); stage.play(Nudge.Wake) }
    }
    fun poke() {
        if (asleep) {
            act("wake")
            c.send(Protocol.command("emotion", "value" to "angry", "hold_ms" to 1500))
            stage.say("Не буди!"); stage.play(Nudge.Angry); return
        }
        // A tap on the device's own face opens its menu, so the phone asks for the reaction directly.
        val (e, t) = listOf("surprised" to "Ой!", "shy" to "Хи-хи", "curious" to "Что там?").random()
        c.send(Protocol.command("emotion", "value" to e, "hold_ms" to 1500))
        stage.say(t, 1500)
        stage.play(Nudge.Poke)
    }

    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).enter()) {
        Hero(s.emotion, asleep, stage, ::poke)
        Text(if (asleep) "Спит и восстанавливает энергию" else "Нажмите на лицо, чтобы потрогать", Modifier.fillMaxWidth().padding(bottom = 16.dp),
            color = Night.Quiet, fontSize = 12.5.sp, textAlign = TextAlign.Center)
        Row(Modifier.fillMaxWidth().padding(start = 14.dp, end = 14.dp, bottom = 18.dp)) {
            Action(Icons.Rounded.Restaurant, "Покормить", false, ::feed)
            Action(Icons.Rounded.PanTool, "Погладить", false, ::pet)
            Action(Icons.Rounded.SportsSoccer, "Поиграть", false, ::play)
            if (asleep) Action(Icons.Rounded.WbSunny, "Разбудить", true, ::sleep) else Action(Icons.Rounded.Bedtime, "Уложить", false, ::sleep)
        }
        val values = needs(s)
        Column(Modifier.padding(horizontal = 16.dp).fillMaxWidth().clip(RoundedCornerShape(24.dp)).background(Night.Surface)
            .padding(horizontal = 18.dp, vertical = 16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            NEEDS.forEach { n -> NeedBar(n, values[n.key] ?: 0) }
        }
        Spacer(Modifier.height(16.dp))
    }
}

@Composable
private fun RowScope.Action(icon: ImageVector, label: String, on: Boolean, onClick: () -> Unit) {
    val src = remember { MutableInteractionSource() }
    val bg by animateColorAsState(if (on) Night.Accent else Night.Bubble, tween(300), label = "act")
    Column(Modifier.weight(1f).clickable(src, null, role = Role.Button, onClick = onClick), horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Box(Modifier.size(60.dp).pressScale(src).clip(CircleShape).background(bg), contentAlignment = Alignment.Center) {
            MIcon(icon, 26.dp, if (on) Night.OnAccent else Night.Text)
        }
        Text(label, color = Night.Text2, fontSize = 12.sp)
    }
}

@Composable
private fun NeedBar(n: Need, value: Int) {
    val w by animateFloatAsState(value.coerceIn(0, 100) / 100f, tween(900, easing = CubicBezierEasing(.3f, 1.2f, .5f, 1f)), label = "need")
    val color by animateColorAsState(if (value < 25) Night.Low else Night.Fill, tween(400), label = "needColor")
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        MIcon(n.icon, 18.dp, Night.Muted)
        Text(n.title, Modifier.width(84.dp), fontSize = 13.5.sp)
        Box(Modifier.weight(1f).height(6.dp).clip(RoundedCornerShape(3.dp)).background(Night.Bg)) {
            Box(Modifier.fillMaxWidth(w.coerceIn(0f, 1f)).fillMaxHeight().clip(RoundedCornerShape(3.dp)).background(color))
        }
        Text("$value", Modifier.width(26.dp), color = Night.Muted, fontSize = 12.sp, textAlign = TextAlign.End)
    }
}

/** The 240 dp copy of the device screen: face, text, effects. */
@Composable
private fun Hero(emotion: String, asleep: Boolean, stage: Stage, onPoke: () -> Unit) {
    val b = stage.bubble
    val message = b != null && !b.short
    val asleepNow by rememberUpdatedState(asleep)
    LaunchedEffect(Unit) { stage.lookAround { asleepNow } }
    val glowT = rememberInfiniteTransition(label = "glow")
    val glow by glowT.animateFloat(0f, 1f, infiniteRepeatable(tween(2000, easing = Ease), RepeatMode.Reverse), label = "glow")
    val shift by animateFloatAsState(if (message) 1f else 0f, tween(500, easing = Springy), label = "shift")
    val dim by animateFloatAsState(if (asleep) 0.7f else 1f, tween(800), label = "dim")
    val lx by animateFloatAsState(stage.look.x, tween(500, easing = CubicBezierEasing(.3f, 1.2f, .5f, 1f)), label = "lx")
    val ly by animateFloatAsState(stage.look.y, tween(500, easing = CubicBezierEasing(.3f, 1.2f, .5f, 1f)), label = "ly")
    val n = stage.nudge
    val p = progressOf(n, n?.first?.ms ?: 1, Springy)
    BoxWithConstraints(Modifier.fillMaxWidth().padding(top = 14.dp, bottom = 8.dp), contentAlignment = Alignment.TopCenter) {
        Box(Modifier.size(240.dp)
            .drawBehind {
                if (asleep) return@drawBehind // no glow in sleep
                val r = size.minDimension / 2
                val far = r + 64.dp.toPx()
                drawCircle(Brush.radialGradient(0f to Night.Accent.copy(alpha = .2f * glow), r / far to Night.Accent.copy(alpha = .2f * glow), 1f to Color.Transparent, center = center, radius = far), far)
                drawCircle(Color(0xFF2A2A30), r + 0.5.dp.toPx(), style = androidx.compose.ui.graphics.drawscope.Stroke(1.dp.toPx()))
            }
            .clip(CircleShape).background(Color.Black).border(6.dp, Color(0xFF1C1C21), CircleShape)
            .clickable(remember { MutableInteractionSource() }, null, onClick = onPoke),
            contentAlignment = Alignment.Center) {
            Text(clockNow(), Modifier.align(Alignment.TopCenter).padding(top = 26.dp), color = Night.Quiet, fontSize = 12.sp)
            Box(Modifier.graphicsLayer {
                alpha = dim
                val k = n?.first
                translationX = (lx + when (k) { Nudge.Shake -> frames(p, 0f, -8f, 8f, -5f, 0f); Nudge.Angry -> frames(p, 0f, -7f, 7f, 0f); else -> 0f }) * density
                translationY = (ly - 42f * shift + when (k) {
                    Nudge.Jump -> frames(p, 0f, -16f, 0f, -6f, 0f); Nudge.Wake -> frames(p, 0f, -14f, 0f); Nudge.Bounce -> frames(p, 0f, -10f, 0f); else -> 0f
                }) * density
                val s = 1f - 0.28f * shift
                val poke = if (k == Nudge.Poke) frames(p, 1f, .9f, 1.05f, 1f) else 1f
                scaleX = s * poke * (if (k == Nudge.Squash) frames(p, 1f, 1.12f, .95f, 1f) else 1f)
                scaleY = s * poke * when (k) { Nudge.Squash -> frames(p, 1f, .9f, 1.05f, 1f); Nudge.Chew -> frames(p, 1f, .78f, 1f, .8f, 1f, .86f, 1f); else -> 1f }
            }) { Eyes(emotion, 151.dp, blink = !asleep) }
            if (asleep) Zzz()
            if (message && b != null) HeroText(b, stage)
        }
        Box(Modifier.size(240.dp)) { stage.fx.forEach { f -> key(f) { FxItem(f) } } }
        if (b != null && b.short) key(b) {
            Text(b.text, Modifier.align(Alignment.TopStart).offset(x = maxWidth / 2 + 62.dp, y = (-8).dp).widthIn(max = 150.dp).pop(350, 0f, 1f)
                .clip(RoundedCornerShape(16.dp, 16.dp, 16.dp, 4.dp)).background(Night.Text).padding(horizontal = 13.dp, vertical = 8.dp),
                color = Color(0xFF141418), fontSize = 13.sp, fontWeight = FontWeight.SemiBold, lineHeight = 17.sp)
        }
    }
}

@Composable
private fun BoxScope.Zzz() {
    val t = rememberInfiniteTransition(label = "zz")
    listOf(Triple(58, 70, 16), Triple(50, 64, 22), Triple(62, 60, 13)).forEachIndexed { i, (right, top, size) ->
        val v by t.animateFloat(0f, 1f, infiniteRepeatable(tween(2400, easing = CubicBezierEasing(0f, 0f, .58f, 1f)), initialStartOffset = androidx.compose.animation.core.StartOffset(i * 800)), label = "z$i")
        Text("z", Modifier.align(Alignment.TopEnd).padding(end = right.dp, top = top.dp).graphicsLayer {
            translationX = 28f * v * density; translationY = -54f * v * density
            val sc = 0.6f + 0.65f * v; scaleX = sc; scaleY = sc
            alpha = if (v < 0.3f) v / 0.3f else 1f - (v - 0.3f) / 0.7f
        }, color = Night.Accent, fontSize = size.sp, fontWeight = FontWeight.Bold)
    }
}

/** Sender and the typed text under the raised eyes; scrolls, fades at the edges and stays while touched. */
@Composable
private fun BoxScope.HeroText(b: Bubble, stage: Stage) {
    val scroll = rememberScrollState()
    LaunchedEffect(stage.typed) { if (!stage.hold) scroll.scrollTo(scroll.maxValue) }
    Column(Modifier.align(Alignment.TopCenter).padding(start = 26.dp, end = 26.dp, top = 118.dp).enter(dy = 14.dp, ms = 350),
        horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        if (b.from.isNotEmpty()) Text(b.from, color = Night.Accent, fontSize = 11.5.sp, fontWeight = FontWeight.SemiBold)
        Box(Modifier.heightIn(max = 64.dp)
            .graphicsLayer { compositingStrategy = CompositingStrategy.Offscreen }
            .drawWithContent {
                drawContent()
                val top = 6.dp.toPx() / size.height
                val bottom = 1f - 10.dp.toPx() / size.height
                drawRect(Brush.verticalGradient(0f to Color.Transparent, top to Color.Black, bottom to Color.Black, 1f to Color.Transparent), blendMode = BlendMode.DstIn)
            }
            .pointerInput(Unit) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false, pass = PointerEventPass.Initial)
                    stage.hold = true
                    do { val e = awaitPointerEvent(PointerEventPass.Initial) } while (e.changes.any { it.pressed })
                    stage.hold = false
                }
            }
            .clickable(remember { MutableInteractionSource() }, null) {} // a touch on the text is not a poke
            .verticalScroll(scroll)) {
            Text(b.text.take(stage.typed), Modifier.fillMaxWidth().padding(start = 4.dp, end = 4.dp, top = 2.dp, bottom = 6.dp),
                color = Night.Fill, fontSize = 13.sp, lineHeight = 17.5.sp, textAlign = TextAlign.Center)
        }
        if (stage.typed >= b.text.length && b.text.length > 90) Text("листайте", color = Night.Quiet, fontSize = 10.sp)
    }
}

private val Burst = CubicBezierEasing(.2f, .7f, .3f, 1f)

@Composable
private fun BoxScope.FxItem(f: Fx) {
    val ms = when (f.kind) { FxKind.Heart, FxKind.Spark -> 1300; FxKind.Food -> 800; FxKind.Crumb -> 650; FxKind.Ball -> 2200; FxKind.Chip -> 1500 }
    val easing = when (f.kind) {
        FxKind.Heart, FxKind.Spark -> Burst; FxKind.Food -> CubicBezierEasing(.4f, 0f, .6f, 1f)
        FxKind.Ball -> CubicBezierEasing(.42f, 0f, .58f, 1f); else -> CubicBezierEasing(0f, 0f, .58f, 1f)
    }
    val p = remember { Animatable(0f) }
    LaunchedEffect(Unit) { delay(f.delay.toLong()); p.animateTo(1f, tween(ms, easing = easing)) }
    val t = p.value
    val started = p.isRunning || t > 0f
    when (f.kind) {
        FxKind.Heart, FxKind.Spark -> Text(if (f.kind == FxKind.Heart) "♥" else "✦", Modifier.align(Alignment.Center).graphicsLayer {
            translationX = f.dx * t * density; translationY = (f.dy * t - 10f) * density
            val s = 0.4f + 0.7f * t; scaleX = s; scaleY = s
            alpha = if (!started) 0f else if (t < .15f) t / .15f else 1f - (t - .15f) / .85f
        }, color = if (f.kind == FxKind.Heart) Night.Accent else Night.Text, fontSize = f.size.sp, lineHeight = f.size.sp)
        FxKind.Crumb -> Box(Modifier.align(Alignment.Center).offset(y = 14.dp).size(5.dp).graphicsLayer {
            translationX = f.dx * t * density; translationY = f.dy * t * density
            val s = 0.4f + 0.7f * t; scaleX = s; scaleY = s
            alpha = if (!started) 0f else if (t < .15f) t / .15f else 1f - (t - .15f) / .85f
        }.clip(CircleShape).background(Night.Sand))
        FxKind.Food -> MIcon(f.icon ?: Icons.Rounded.Cookie, 40.dp, Night.Sand, Modifier.align(Alignment.Center).graphicsLayer {
            val a = (t / .75f).coerceAtMost(1f)
            val b = ((t - .75f) / .25f).coerceIn(0f, 1f)
            translationX = (f.dx * (1 - a)) * density
            translationY = (if (t < .75f) 150f + (12f - 150f) * a else 12f + 4f * b) * density
            val s = if (t < .75f) 1.4f + (.9f - 1.4f) * a else .9f * (1 - b); scaleX = s; scaleY = s
            rotationZ = if (t < .75f) -20f + 28f * a else 8f
            alpha = when { t < .2f -> t / .2f; t < .75f -> 1f; else -> 1f - b }
        })
        FxKind.Ball -> MIcon(Icons.Rounded.SportsBaseball, 30.dp, Night.Fill, Modifier.align(Alignment.Center).graphicsLayer {
            translationX = frames(t, -130f, -60f, 10f, 80f, 150f) * density
            translationY = frames(t, 50f, -60f, 50f, -55f, 50f) * density
            alpha = if (t < .1f) t / .1f else 1f - (t - .1f) / .9f
        })
        FxKind.Chip -> Text(f.text, Modifier.align(Alignment.BottomCenter).padding(bottom = (240 * .18f).dp).graphicsLayer {
            translationX = f.dx * density
            translationY = (if (t < .15f) 10f * (1 - t / .15f) else -50f * (t - .15f) / .85f) * density
            alpha = when { !started -> 0f; t < .15f -> t / .15f; t < .8f -> 1f; else -> 1f - (t - .8f) / .2f }
        }.clip(RoundedCornerShape(12.dp)).background(Night.Accent.copy(alpha = .18f)).padding(horizontal = 10.dp, vertical = 4.dp),
            color = Night.AccentLight, fontSize = 12.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
    }
}
