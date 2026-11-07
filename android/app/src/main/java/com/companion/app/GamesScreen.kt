package com.companion.app

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Canvas
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.automirrored.outlined.HelpOutline
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.companion.app.games.*
import kotlinx.coroutines.delay
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.sin

/** A lean of this many g reaches the side, about 25 degrees: comfortable for the wrist. */
private const val FULL_LEAN = 2.4f
private val Rose = Color(0xFFDAB0B8)
private val Mint = Color(0xFFAED0C0)
private val Sky = Color(0xFFAAC6DC)
private val Sand = Color(0xFFDCD6C4)
private val Grey = Color(0xFF6E6E74)
private val Dark = Color(0xFF26262B)
private val Panel = Color(0xFF1C1C22)
private val Accent = Color(0xFFB6BED6)

// The device shows its own controls while it is a gamepad, so the phone does not explain them.
private enum class BigGame(val title: String, val icon: ImageVector) {
    Race("Гонки", Icons.Outlined.DirectionsCar), Durak("Дурак", Icons.Outlined.Style),
    Poker("Покер", Icons.Outlined.Toll), Breakout("Арканоид", Icons.Outlined.ViewModule),
}

/** Games and tools that run on the device's own screen. */
private val DEVICE_SCREENS = listOf(
    Triple(Icons.Outlined.AutoAwesomeMotion, "21", "blackjack"), Triple(Icons.Outlined.Explore, "Лабиринт", "maze"),
    Triple(Icons.Outlined.TrackChanges, "Орбита", "orbit"), Triple(Icons.Outlined.MonetizationOn, "Монетка", "coin"),
    Triple(Icons.Outlined.Casino, "Кубики", "dice"), Triple(Icons.Outlined.AutoAwesome, "Шар", "magic_ball"),
    Triple(Icons.Outlined.SportsKabaddi, "Дуэль", "duel"), Triple(Icons.Outlined.Vibration, "Шейкер", "shaker"),
    Triple(Icons.Outlined.Lock, "Сейф", "safe"), Triple(Icons.Outlined.Balance, "Баланс", "balance"),
    Triple(Icons.Outlined.FitnessCenter, "Зарядка", "workout"), Triple(Icons.Outlined.Gavel, "Кротики", "mole"),
    Triple(Icons.Outlined.Gesture, "Змейка", "snake"), Triple(Icons.Outlined.Psychology, "Память", "simon"),
    Triple(Icons.Outlined.Tag, "Крестики", "tictactoe"), Triple(Icons.Outlined.Speed, "Реакция", "reaction"),
    Triple(Icons.AutoMirrored.Outlined.HelpOutline, "Угадай", "guess"), Triple(Icons.Outlined.Flare, "Светлячок", "pet_game"),
    Triple(Icons.Outlined.Schedule, "Часы", "digital_clock"), Triple(Icons.Outlined.Cloud, "Погода", "weather"),
    Triple(Icons.Outlined.Visibility, "Лицо", "face"),
)

/** Host side of controller mode: the game runs here, the small device is the gamepad. */
@Composable
fun GamesTab(client: BleClient) {
    var game by rememberSaveable { mutableStateOf<BigGame?>(null) }
    val current = game
    if (current == null) {
        val link by client.link.collectAsState()
        LazyVerticalGrid(GridCells.Fixed(4), verticalArrangement = Arrangement.spacedBy(8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp),
            contentPadding = PaddingValues(bottom = 16.dp)) {
            items(BigGame.entries, span = { GridItemSpan(2) }) { g ->
                Column(Modifier.clip(RoundedCornerShape(20.dp)).background(Panel).clickable { game = g }.padding(vertical = 24.dp),
                    horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(g.icon, null, Modifier.size(36.dp))
                    Spacer(Modifier.height(10.dp))
                    Text(g.title, fontSize = 16.sp)
                }
            }
            if (link == LinkState.Authenticated) {
                item(span = { GridItemSpan(maxLineSpan) }) { Text("На устройстве", color = Grey, fontSize = 13.sp, modifier = Modifier.padding(top = 16.dp)) }
                items(DEVICE_SCREENS) { (icon, title, screen) ->
                    Column(Modifier.clip(RoundedCornerShape(16.dp)).background(Panel).clickable { client.send(Protocol.command("screen", "value" to screen)) }
                        .padding(vertical = 12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                        Icon(icon, null, Modifier.size(24.dp), tint = Sand)
                        Spacer(Modifier.height(6.dp))
                        Text(title, fontSize = 11.sp, color = Grey, maxLines = 1)
                    }
                }
            }
        }
        return
    }
    BackHandler { game = null }
    KeepControllerAlive(client)
    Column(Modifier.fillMaxSize()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = { game = null }) { Icon(Icons.AutoMirrored.Outlined.ArrowBack, "Назад") }
            Spacer(Modifier.width(4.dp))
            Text(current.title, fontWeight = FontWeight.Bold, fontSize = 18.sp)
        }
        Spacer(Modifier.height(8.dp))
        Box(Modifier.weight(1f).fillMaxWidth()) {
            when (current) {
                BigGame.Race -> RaceHost(client)
                BigGame.Durak -> DurakHost(client)
                BigGame.Poker -> PokerHost(client)
                BigGame.Breakout -> BreakoutHost(client)
            }
        }
    }
    DisposableEffect(Unit) { onDispose { client.send(Protocol.command("controller", "value" to "stop")) } }
}

@Composable
private fun KeepControllerAlive(client: BleClient) {
    LaunchedEffect(Unit) {
        while (true) {
            delay(3000)
            if (client.link.value == LinkState.Authenticated) client.send(Protocol.command("controller", "value" to "ping"))
        }
    }
}

private fun BleClient.startPad(title: String, hint: String) =
    send(Protocol.command("controller", "value" to "start", "mode" to "pad", "title" to title, "hint" to hint))

private fun BleClient.showCards(title: String, cards: List<Card>, marks: String = "", hint: String = "", info: String = "", target: Card? = null) =
    send(Protocol.command("controller", "value" to "start", "mode" to "cards", "title" to title,
        "labels" to cards.joinToString(",") { it.label }, "marks" to marks, "hint" to hint, "info" to info,
        "target" to (target?.label ?: "")))

/** Runs [body] once per display frame with the elapsed seconds, capped so a pause cannot teleport anything. */
@Composable
private fun FrameLoop(body: (Float) -> Unit) {
    val latest by rememberUpdatedState(body)
    LaunchedEffect(Unit) {
        var last = 0L
        while (true) {
            androidx.compose.runtime.withFrameNanos { now ->
                if (last != 0L) latest(((now - last) / 1e9f).coerceAtMost(0.05f))
                last = now
            }
        }
    }
}

private val CarColors = listOf(Color.White, Rose, Mint, Sky)

private fun carColor(game: RaceGame, car: RaceCar) = if (car.human) CarColors[game.cars.indexOf(car)] else Grey

private fun carName(game: RaceGame, car: RaceCar) = if (car.human) "Игрок ${game.cars.indexOf(car) + 1}" else "Комп."

private fun DrawScope.drawTrack(track: RaceTrack, scale: Float, ox: Float, oy: Float) {
    val path = Path()
    for (i in 0 until RaceTrack.SAMPLES step 3) {
        val x = ox + track.xs[i] * scale
        val y = oy + track.ys[i] * scale
        if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
    }
    path.close()
    val road = RaceGame.ROAD_HALF * 2 * scale
    drawPath(path, Grey, style = Stroke(road + scale * 0.008f, join = StrokeJoin.Round))
    drawPath(path, Dark, style = Stroke(road, join = StrokeJoin.Round))
    drawPath(path, Sand.copy(alpha = 0.3f), style = Stroke(scale * 0.003f, pathEffect = PathEffect.dashPathEffect(floatArrayOf(scale * 0.02f, scale * 0.025f))))
    // Chequered line: sand and black, so that pure white on the track is only ever the first player's car.
    val a = track.direction(0)
    val nx = -sin(a); val ny = cos(a)
    val sq = road / 6
    for (row in 0..1) for (k in 0 until 6) {
        val along = (row - 0.5f) * sq
        val across = (k + 0.5f) * sq - road / 2
        val cx = ox + track.xs[0] * scale + cos(a) * along + nx * across
        val cy = oy + track.ys[0] * scale + sin(a) * along + ny * across
        drawLine(if ((k + row) % 2 == 0) Sand else Color.Black, Offset(cx - nx * sq / 2, cy - ny * sq / 2), Offset(cx + nx * sq / 2, cy + ny * sq / 2), strokeWidth = sq)
    }
}

/** A button that reports both press and release, several at once: every player has their own. */
@Composable
private fun HoldPad(label: String, color: Color, modifier: Modifier, onChange: (Boolean) -> Unit) {
    Box(modifier.height(60.dp).clip(RoundedCornerShape(12.dp)).background(color.copy(alpha = 0.16f))
        .pointerInput(Unit) { awaitEachGesture { awaitFirstDown(); onChange(true); waitForUpOrCancellation(); onChange(false) } },
        contentAlignment = Alignment.Center) { Text(label, color = color, fontSize = 26.sp) }
}

@Composable
private fun RaceHost(client: BleClient) {
    var setup by remember { mutableStateOf(true) }
    var players by remember { mutableIntStateOf(1) }
    var trackIndex by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) { client.startPad("Гонки", "наклон руль · держи экран тормоз") }
    if (setup) {
        GameSetup(onStart = { setup = false }) {
            PlayerPicker(1..4, players) { players = it }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                RACE_TRACKS.forEachIndexed { i, t ->
                    Column(Modifier.weight(1f).clip(RoundedCornerShape(12.dp))
                        .background(if (i == trackIndex) Dark else Color.Transparent)
                        .clickable { trackIndex = i }.padding(6.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                        Canvas(Modifier.fillMaxWidth().height(130.dp)) {
                            val scale = minOf(size.width, size.height / RaceGame.ARENA_H)
                            drawTrack(t, scale, (size.width - scale) / 2, (size.height - scale * RaceGame.ARENA_H) / 2)
                        }
                        Text(t.name, fontSize = 13.sp)
                    }
                }
            }
        }
        return
    }
    RaceTable(client, RACE_TRACKS[trackIndex], players) { setup = true }
}

/** Every game starts the same way: choose on top, one big button at the bottom. */
@Composable
private fun GameSetup(onStart: () -> Unit, content: @Composable ColumnScope.() -> Unit) {
    Column(Modifier.fillMaxSize().padding(top = 8.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
        content()
        Spacer(Modifier.weight(1f))
        Button(onClick = onStart, Modifier.fillMaxWidth().height(56.dp)) {
            Icon(Icons.Outlined.PlayArrow, null)
            Spacer(Modifier.width(8.dp))
            Text("Играть", fontSize = 18.sp)
        }
        Spacer(Modifier.height(8.dp))
    }
}

/** Segmented choice: the selected option is filled, so no caption is needed to tell which one is on. */
@Composable
private fun <T> Picker(options: List<Pair<String, T>>, selected: T, icon: ImageVector? = null, onSelect: (T) -> Unit) {
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel).padding(4.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        options.forEach { (label, value) ->
            val on = value == selected
            Row(Modifier.weight(1f).height(48.dp).clip(RoundedCornerShape(10.dp)).background(if (on) Accent else Color.Transparent).clickable { onSelect(value) },
                horizontalArrangement = Arrangement.Center, verticalAlignment = Alignment.CenterVertically) {
                icon?.let { Icon(it, null, Modifier.size(18.dp), tint = if (on) Color.Black else Grey) }
                Text(label, color = if (on) Color.Black else Color.White, fontWeight = FontWeight.Bold, fontSize = 16.sp)
            }
        }
    }
}

/** How many people play; empty seats go to the computer. */
@Composable
private fun PlayerPicker(range: IntRange, players: Int, onSelect: (Int) -> Unit) =
    Picker(range.map { "$it" to it }, players, Icons.Outlined.Person, onSelect)

@Composable
private fun RaceTable(client: BleClient, track: RaceTrack, players: Int, onSetup: () -> Unit) {
    var round by remember { mutableIntStateOf(0) }
    val game = remember(round) { RaceGame(track, players) }
    var frame by remember { mutableIntStateOf(0) }
    val tilt by client.tilt.collectAsState()
    val link by client.link.collectAsState()
    val hasDevice = link == LinkState.Authenticated
    var deviceBrake by remember { mutableStateOf(false) }
    // Plain arrays, read once per frame: a press must not recompose the table.
    val left = remember { BooleanArray(4) }
    val right = remember { BooleanArray(4) }
    val steer = remember { FloatArray(4) }
    val brake = remember { BooleanArray(4) }
    LaunchedEffect(round) {
        client.actions.collect { a ->
            when (a.action) {
                "down" -> deviceBrake = true
                "up" -> deviceBrake = false
                "tap" -> if (game.over) round++
            }
        }
    }
    FrameLoop { dt ->
        for (i in 0 until players) {
            steer[i] = (if (right[i]) 1f else 0f) - (if (left[i]) 1f else 0f)
            brake[i] = left[i] && right[i]
        }
        if (hasDevice) { steer[0] = tilt.first * FULL_LEAN; brake[0] = deviceBrake }
        game.step(dt, steer, brake)
        frame++
    }
    frame.hashCode() // read during composition, not only in the canvas: the standings must update too
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            game.standings.forEachIndexed { place, car ->
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text("${place + 1}. ${carName(game, car)}", color = carColor(game, car), fontSize = 12.sp)
                    Text(if (car.finished) "финиш" else "круг ${game.lap(car)}/${game.laps}", color = Grey, fontSize = 11.sp)
                }
            }
        }
        Box(Modifier.weight(1f).fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(Color(0xFF101013))) {
            Canvas(Modifier.fillMaxSize()) {
                frame.hashCode() // also read here: Compose skips a draw lambda whose captures did not change, freezing the picture
                val scale = minOf(size.width, size.height / RaceGame.ARENA_H)
                val ox = (size.width - scale) / 2
                val oy = (size.height - scale * RaceGame.ARENA_H) / 2
                drawTrack(game.track, scale, ox, oy)
                val l = scale * 0.042f
                val wd = scale * 0.025f
                for (car in game.cars) {
                    val p = Offset(ox + car.x * scale, oy + car.y * scale)
                    rotate(Math.toDegrees(car.heading.toDouble()).toFloat(), p) {
                        drawRoundRect(carColor(game, car), Offset(p.x - l / 2, p.y - wd / 2), Size(l, wd), CornerRadius(wd * 0.3f))
                        drawRect(Dark, Offset(p.x + l * 0.08f, p.y - wd * 0.34f), Size(l * 0.2f, wd * 0.68f))
                    }
                }
            }
            if (game.countdown > 0f) Text("${ceil(game.countdown).toInt()}", Modifier.align(Alignment.Center), fontSize = 64.sp, fontWeight = FontWeight.Bold)
            if (game.over) Card(Modifier.align(Alignment.Center).padding(16.dp)) {
                Column(Modifier.padding(16.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("Финиш", fontSize = 22.sp, fontWeight = FontWeight.Bold)
                    game.standings.forEachIndexed { place, car ->
                        val time = car.finishTime?.let { "%d:%04.1f".format(it.toInt() / 60, it % 60) } ?: "не доехал"
                        Text("${place + 1}. ${carName(game, car)} — $time", color = carColor(game, car))
                    }
                    Button(onClick = { round++ }) { Text("Ещё раз") }
                    OutlinedButton(onClick = onSetup) { Text("Другая трасса") }
                }
            }
        }
        // Two players side by side per row: four people around one phone need the room for the track.
        (if (hasDevice) 1 until players else 0 until players).chunked(2).forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for (i in row) {
                    HoldPad("◀", CarColors[i], Modifier.weight(1f)) { left[i] = it }
                    HoldPad("▶", CarColors[i], Modifier.weight(1f)) { right[i] = it }
                }
                if (row.size == 1) Spacer(Modifier.weight(2f))
            }
        }
    }
}

/** A face-down card is [card] == null. Text scales with the width, so the same card works in a hand and in a fan. */
@Composable
private fun PlayingCard(card: Card?, selected: Boolean, held: Boolean, modifier: Modifier = Modifier, onClick: (() -> Unit)? = null) {
    val red = card != null && (card.suit == 1 || card.suit == 2)
    val shape = RoundedCornerShape(16)
    BoxWithConstraints(
        modifier.aspectRatio(0.68f).clip(shape)
            .background(if (card == null) Dark else Color(0xFFF2F1EC))
            .border(if (selected) 3.dp else 0.dp, if (selected) Sky else Color.Transparent, shape)
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier),
        contentAlignment = Alignment.Center,
    ) {
        val w = maxWidth.value
        if (card != null) {
            val ink = if (red) Color(0xFFA8606E) else Color.Black
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(card.rankName, color = ink, fontWeight = FontWeight.Bold, fontSize = (w * 0.33f).sp)
                Text("♠♥♦♣"[card.suit] + Char(0xFE0E).toString(), color = ink, fontSize = (w * 0.36f).sp) // FE0E: a glyph, never an emoji
            }
        } else Box(Modifier.fillMaxSize().padding(maxWidth * 0.12f).border(1.dp, Grey.copy(alpha = 0.5f), RoundedCornerShape(12)))
        if (held) Box(Modifier.align(Alignment.TopCenter).padding(4.dp).size(8.dp).clip(RoundedCornerShape(4.dp)).background(Mint))
    }
}

/** The card arrives from [from], relative to where it lands, straightening as it comes: a deal, a throw, a cover. */
private fun Modifier.dealIn(from: DpOffset, delayMs: Int = 0): Modifier = composed {
    val p = remember { Animatable(0f) }
    LaunchedEffect(Unit) { p.animateTo(1f, tween(420, delayMs, FastOutSlowInEasing)) }
    graphicsLayer {
        val k = 1f - p.value
        translationX = from.x.toPx() * k
        translationY = from.y.toPx() * k
        rotationZ = 14f * k
        alpha = if (p.value > 0f) 1f else 0f // hidden until its turn in a staggered deal
    }
}

/** Starts face down, so a card that should be seen turns over in view; turns back if [faceUp] goes false. */
@Composable
private fun FlipCard(card: Card, faceUp: Boolean, modifier: Modifier = Modifier, delayMs: Int = 0) {
    val angle = remember { Animatable(0f) }
    LaunchedEffect(faceUp) { angle.animateTo(if (faceUp) 180f else 0f, tween(420, delayMs, FastOutSlowInEasing)) }
    val front = angle.value > 90f
    PlayingCard(if (front) card else null, false, false, modifier.graphicsLayer {
        rotationY = if (front) angle.value - 180f else angle.value
        cameraDistance = 12f * density
    })
}

/** Cards leaving the table together: to the player who takes them, or off to the discard. */
@Composable
private fun LeavingCards(cards: List<Card>, to: DpOffset, onDone: () -> Unit) {
    val p = remember { Animatable(0f) }
    LaunchedEffect(Unit) { p.animateTo(1f, tween(560, easing = FastOutSlowInEasing)); onDone() }
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        cards.forEachIndexed { k, c ->
            val spread = k - (cards.size - 1) / 2f
            PlayingCard(c, false, false, Modifier.width(52.dp).graphicsLayer {
                val t = p.value
                translationX = spread * 22.dp.toPx() * (1 - t) + to.x.toPx() * t
                translationY = to.y.toPx() * t
                rotationZ = spread * 6f * (1 - t) + 30f * t
                alpha = 1f - t * t
            })
        }
    }
}

@Composable
private fun EmptySlot(modifier: Modifier) =
    Box(modifier.aspectRatio(0.68f).border(1.dp, Grey.copy(alpha = 0.35f), RoundedCornerShape(16)))

/** A computer player at the top of the table: a fan of backs, one per card, up to six. */
@Composable
private fun OpponentSeat(name: String, cards: Int, active: Boolean, taking: Boolean) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(Modifier.size(72.dp, 46.dp), contentAlignment = Alignment.BottomCenter) {
            val n = minOf(cards, 6)
            for (k in 0 until n) {
                val at = k - (n - 1) / 2f
                PlayingCard(null, false, false, Modifier.width(26.dp).graphicsLayer {
                    transformOrigin = TransformOrigin(0.5f, 1f)
                    rotationZ = at * 10f
                    translationX = at * 4.dp.toPx()
                })
            }
        }
        Text("$name · $cards", color = if (active) Accent else Grey, fontSize = 12.sp, fontWeight = if (active) FontWeight.Bold else FontWeight.Normal)
        Text(if (taking) "берёт" else " ", color = Rose, fontSize = 11.sp)
    }
}

/** The stock with the trump card under it, as on a real table; only the trump stays once the stock is gone. */
@Composable
private fun DeckPile(left: Int, trump: Card) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(Modifier.size(64.dp, 46.dp)) {
            PlayingCard(trump, false, false, Modifier.width(30.dp).align(Alignment.CenterEnd).graphicsLayer { rotationZ = 90f })
            if (left > 1) PlayingCard(null, false, false, Modifier.width(30.dp).align(Alignment.CenterStart))
        }
        Text(if (left > 0) "$left" else "козырь", color = Grey, fontSize = 12.sp)
        Text(" ", fontSize = 11.sp)
    }
}

/**
 * Texas Hold'em. This screen is the table everyone sees: shared cards, pot and seats. The player's two cards live on
 * the device and stay face down here until the showdown. Without a device they are shown here, or nobody could play.
 */
@Composable
private fun PokerHost(client: BleClient) {
    var game by remember { mutableStateOf<HoldemGame?>(null) }
    val current = game
    if (current == null) {
        LaunchedEffect(Unit) { client.startPad("Покер", "выберите число игроков на телефоне") }
        var players by remember { mutableIntStateOf(2) }
        GameSetup(onStart = { game = HoldemGame(players) }) { PlayerPicker(2..4, players) { players = it } }
        return
    }
    PokerTable(client, current) { game = null }
}

@Composable
private fun PokerTable(client: BleClient, game: HoldemGame, onExit: () -> Unit) {
    var version by remember { mutableIntStateOf(0) }
    val link by client.link.collectAsState()
    val deviceHoldsCards = link == LinkState.Authenticated

    fun callLabel() = if (game.whoActs() == 0 && game.toCall(0) > 0) "Поддержать ${game.toCall(0)}" else "Чек"
    fun sync() {
        version++
        val mine = game.whoActs() == 0
        val hint = when {
            game.street == Street.GameOver -> "тап новая игра"
            game.street == Street.Showdown -> "тап новая раздача"
            mine && game.canRaise -> "тап ${callLabel().lowercase()}\n↑ поднять · ↓ сбросить"
            mine -> "тап ${callLabel().lowercase()} · ↓ сбросить"
            else -> "ждём соперников"
        }
        val state = when {
            game.street.ordinal >= Street.Showdown.ordinal -> game.message
            game.folded[0] -> "Вы сбросили карты"
            else -> "${game.street.title} · банк ${game.pot}"
        }
        client.showCards("Фишки ${game.stacks[0]}", game.hole[0], "", hint, state)
    }
    fun act(action: String) {
        when (action) {
            "tap" -> when (game.street) {
                Street.GameOver -> { onExit(); return }
                Street.Showdown -> game.newHand()
                else -> game.call(0)
            }
            "swipe_up" -> game.raise(0)
            "swipe_down" -> game.fold(0)
        }
        sync()
    }
    LaunchedEffect(game, deviceHoldsCards) { sync() }
    LaunchedEffect(game) {
        while (true) { // computers act one at a time, so the table can follow
            delay(900)
            if (game.aiAct()) sync()
        }
    }
    LaunchedEffect(game) { client.actions.collect { a -> if (a.action in listOf("tap", "swipe_up", "swipe_down")) act(a.action) } }

    // Read here instead of key(version): the game object changes in place, and a key would restart every card animation.
    version.hashCode()
    val open = game.street.ordinal >= Street.Showdown.ordinal
    Column(Modifier.fillMaxSize(), horizontalAlignment = Alignment.CenterHorizontally) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
            for (p in 1 until game.players) Seat(game, p, open, game.whoActs() == p, version)
        }
        Spacer(Modifier.weight(1f))
        Text(game.street.title, color = Grey, fontSize = 13.sp)
        Spacer(Modifier.height(6.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            for (i in 0 until 5) Box(Modifier.weight(1f)) {
                val c = game.board.getOrNull(i)
                // The flop lands one card after another; each turns over once it is down.
                val delay = if (i < 3) i * 140 else 0
                if (c == null) EmptySlot(Modifier.fillMaxWidth())
                else key(c) { FlipCard(c, true, Modifier.fillMaxWidth().dealIn(DpOffset(0.dp, (-160).dp), delay), delay + 260) }
            }
        }
        Spacer(Modifier.height(8.dp))
        Text("Банк: ${game.pot}", fontWeight = FontWeight.Bold, fontSize = 18.sp, color = Sand)
        if (game.message.isNotEmpty()) Text(game.message, fontSize = 16.sp, textAlign = TextAlign.Center)
        Spacer(Modifier.weight(1f))
        Text(if (deviceHoldsCards && !open) "Ваши карты на устройстве" else "Ваши карты", color = Grey, fontSize = 13.sp)
        Row(Modifier.width(150.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            game.hole[0].forEachIndexed { i, c ->
                key(c) { FlipCard(c, open || !deviceHoldsCards, Modifier.weight(1f).dealIn(DpOffset(0.dp, (-320).dp), i * 120), i * 120 + 260) }
            }
        }
        Text("Фишки: ${game.stacks[0]}", color = Grey, fontSize = 13.sp)
        // On the table too: the same moves as on the device, and no others exist.
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            if (open) Button(onClick = { act("tap") }) { Text(if (game.street == Street.GameOver) "Выйти" else "Новая раздача") }
            else {
                val mine = game.whoActs() == 0
                Button(onClick = { act("tap") }, enabled = mine) { Text(callLabel()) }
                OutlinedButton(onClick = { act("swipe_up") }, enabled = mine && game.canRaise) { Text("Поднять") }
                OutlinedButton(onClick = { act("swipe_down") }, enabled = mine) { Text("Сбросить") }
            }
        }
    }
}

/** [version] only makes the seat redraw after a move: the game it reads is the same object every time. */
@Composable
private fun Seat(game: HoldemGame, p: Int, open: Boolean, active: Boolean, @Suppress("UNUSED_PARAMETER") version: Int) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text("Игрок ${p + 1}", color = if (active) Accent else Grey, fontSize = 13.sp, fontWeight = if (active) FontWeight.Bold else FontWeight.Normal)
        if (game.inHand(p)) {
            Row(Modifier.width(76.dp), horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                game.hole[p].forEachIndexed { i, c -> key(c) { FlipCard(c, open, Modifier.weight(1f).dealIn(DpOffset(0.dp, 260.dp), i * 120)) } }
            }
        } else Text(if (game.hole[p].isEmpty()) "выбыл" else "сбросил", color = Grey, fontSize = 13.sp)
        Text("${game.stacks[p]}", color = Sand, fontSize = 13.sp)
    }
}

@Composable
private fun BreakoutHost(client: BleClient) {
    var round by remember { mutableIntStateOf(0) }
    val game = remember(round) { BreakoutGame() }
    var frame by remember { mutableIntStateOf(0) }
    var touchX by remember { mutableStateOf<Float?>(null) }
    val tilt by client.tilt.collectAsState()
    LaunchedEffect(Unit) { client.startPad("Арканоид", "наклон ракетка · тап запуск") }
    LaunchedEffect(round) {
        client.actions.collect { a ->
            if (a.action == "tap") { if (game.over || game.won) round++ else game.launch() }
        }
    }
    FrameLoop { dt ->
        game.step(dt, touchX ?: (0.5f + tilt.first * FULL_LEAN * 0.5f))
        frame++
    }
    val colors = listOf(Rose, Sand, Mint, Sky, Color.White)
    frame.hashCode() // read during composition, not only in the canvas: the score and messages must update too
    Box(Modifier.fillMaxSize().clip(RoundedCornerShape(16.dp)).background(Color(0xFF101013))
        .pointerInput(Unit) { detectDragGestures(onDragEnd = { touchX = null }, onDragCancel = { touchX = null }) { c, _ -> touchX = c.position.x / size.width } }
        .pointerInput(round) { detectTapGestures(onTap = { if (game.over || game.won) round++ else game.launch() }) }) {
        Canvas(Modifier.fillMaxSize()) {
            frame.hashCode() // see the race: without this read the picture freezes
            val w = size.width
            val h = size.height
            val bw = w / game.cols
            val bh = h * 0.05f
            for (r in 0 until game.rows) for (c in 0 until game.cols) {
                if (game.bricks[r * game.cols + c]) {
                    drawRoundRect(colors[r % colors.size], Offset(c * bw + 2, h * 0.08f + r * bh + 2), Size(bw - 4, bh - 4), CornerRadius(5f))
                }
            }
            drawRoundRect(Color.White, Offset((game.paddle - game.paddleWidth / 2) * w, h * 0.93f), Size(game.paddleWidth * w, h * 0.018f), CornerRadius(8f))
            drawCircle(Color.White, w * 0.018f, Offset(game.ballX * w, game.ballY * h))
        }
        Text("${game.score}   жизни: ${game.lives}", Modifier.align(Alignment.TopCenter).padding(6.dp), color = Grey)
        if (!game.launched && !game.over && !game.won) Text("Нажмите — старт", Modifier.align(Alignment.Center).padding(24.dp), color = Sand, fontSize = 18.sp)
        if (game.over || game.won) {
            Text(if (game.won) "Победа: ${game.score}\nНажмите — ещё раз" else "Счёт: ${game.score}\nНажмите — ещё раз",
                Modifier.align(Alignment.Center), fontSize = 20.sp, textAlign = TextAlign.Center)
        }
    }
}

// ---- Durak --------------------------------------------------------------------------------------
private val SUIT_NAMES = listOf("пики", "черви", "бубны", "трефы")

/**
 * Device flow: lean to walk through the hand, tap marks cards, swipe up throws everything marked (or the focused card)
 * on the table, swipe down takes or ends the bout, swipe right transfers. The phone offers the same by touch.
 */
@Composable
private fun DurakHost(client: BleClient) {
    var game by remember { mutableStateOf<DurakGame?>(null) }
    val current = game
    if (current == null) {
        LaunchedEffect(Unit) { client.startPad("Дурак", "выберите режим на телефоне") }
        var players by remember { mutableIntStateOf(2) }
        var transfer by remember { mutableStateOf(false) }
        GameSetup(onStart = { game = DurakGame(players, transfer) }) {
            PlayerPicker(2..4, players) { players = it }
            Picker(listOf("Подкидной" to false, "Переводной" to true), transfer) { transfer = it }
        }
        return
    }
    DurakTable(client, current) { game = null }
}

/** Cards of a finished bout on their way: [to] is the player who took them, null is the discard. */
private class Leaving(val cards: List<Card>, val to: Int?)

private class LastTable(var cards: List<Card>, var defender: Int, var discarded: Int)

@Composable
private fun DurakTable(client: BleClient, game: DurakGame, onExit: () -> Unit) {
    var version by remember { mutableIntStateOf(0) }
    var focus by remember { mutableIntStateOf(0) }
    val marked = remember { mutableStateListOf<Card>() }
    var message by remember { mutableStateOf("") }
    var leaving by remember { mutableStateOf<Leaving?>(null) }
    // The table as last shown: when a bout ends, it says which cards fly away and to whom.
    val last = remember { LastTable(emptyList(), game.defender, game.discarded) }
    val hand: List<Card> = game.hands[0]
    val link by client.link.collectAsState()
    val deviceHoldsCards = link == LinkState.Authenticated

    fun chosen(): List<Card> = marked.filter { it in hand }.ifEmpty { listOfNotNull(hand.getOrNull(focus)) }
    // While the defender takes, "бито" would be a lie: the cards go to them, not to the discard.
    fun passLabel() = when { game.defender == 0 -> "Беру"; game.taking -> "Пас"; else -> "Бито" }
    fun role(): String = when {
        game.finished -> when (game.durak) { null -> "Ничья"; 0 -> "Вы дурак!"; else -> "Вы вышли, дурак: игрок ${game.durak!! + 1}" }
        // Whoever takes is named while the others still throw in: they will not lead the next bout.
        game.taking -> if (game.defender == 0) "Вы берёте" else "Игрок ${game.defender + 1} берёт"
        game.whoActs() != 0 -> "Ходит игрок ${game.whoActs()!! + 1}"
        game.defender == 0 -> "Отбивайтесь"
        game.table.isEmpty() -> "Ваш ход"
        else -> "Подкидывайте или бито"
    }
    fun hint(): String = when {
        game.finished -> "тап новая игра"
        game.whoActs() != 0 -> "ждём соперника"
        game.defender == 0 && game.transferMode -> "наклон выбор · тап отметить\n↑ бить · → перевод · ↓ беру"
        game.defender == 0 -> "наклон выбор · тап отметить\n↑ бить · ↓ беру"
        game.table.isEmpty() -> "наклон выбор · тап отметить\n↑ на стол"
        else -> "наклон выбор · тап отметить\n↑ подкинуть · ↓ ${passLabel().lowercase()}"
    }
    fun sync() {
        version++
        val now = game.table.flatMap { listOfNotNull(it.attack, it.cover) }
        if (now.isEmpty() && last.cards.isNotEmpty()) leaving = Leaving(last.cards, if (game.discarded > last.discarded) null else last.defender)
        last.cards = now
        last.defender = game.defender
        last.discarded = game.discarded
        if (focus >= hand.size) focus = (hand.size - 1).coerceAtLeast(0)
        client.showCards("Козырь: ${SUIT_NAMES[game.trump]}", hand,
            hand.joinToString("") { if (it in marked) "1" else "0" }, hint(), role(),
            if (game.defender == 0) game.uncovered.firstOrNull()?.attack else null)
    }
    fun afterMove(ok: Boolean, failure: String) {
        if (ok) { marked.clear(); message = "" } else message = failure
        sync()
    }
    fun toggle(i: Int) {
        val c = hand.getOrNull(i) ?: return
        focus = i
        if (c in marked) marked.remove(c) else marked.add(c)
        sync()
    }
    fun play() { // swipe up: put the cards on the table, as an attack or a defence depending on the role
        if (game.whoActs() != 0) return
        val cards = chosen()
        when {
            game.defender != 0 -> afterMove(game.attack(0, cards), "Эти карты сюда не подходят")
            game.defend(cards) -> afterMove(true, "")
            game.transfer(cards) -> afterMove(true, "") // cannot beat but same rank: the only sensible reading is a transfer
            else -> afterMove(false, "Этими картами не отбиться")
        }
    }
    fun transfer() { if (game.whoActs() == 0) afterMove(game.transfer(chosen()), "Перевести нельзя") }
    fun passOrTake() {
        if (game.whoActs() != 0) return
        if (game.defender == 0) afterMove(game.take(), "") else afterMove(game.pass(0), "Первый ход пропустить нельзя")
    }

    LaunchedEffect(game, deviceHoldsCards) { sync() }
    LaunchedEffect(game) {
        // Computers move one at a time with a pause, so a person can follow what lands on the table.
        while (true) {
            delay(750)
            if (game.aiAct()) sync()
        }
    }
    LaunchedEffect(game) {
        client.actions.collect { a ->
            when (a.action) {
                "select" -> { focus = a.index; version++ }
                "tap" -> if (game.finished) onExit() else toggle(a.index)
                "swipe_up", "long_press" -> { focus = a.index; play() }
                "swipe_down" -> passOrTake()
                "swipe_right" -> { focus = a.index; transfer() }
            }
        }
    }

    // Read here instead of key(version): the game object changes in place, and a key would restart every card animation.
    version.hashCode()
    val screenW = LocalConfiguration.current.screenWidthDp.dp
    // Where a seat is, seen from the middle of the table: the human below, the computers in a row on top.
    fun seat(p: Int) = if (p == 0) DpOffset(0.dp, 320.dp) else DpOffset(screenW * 0.8f * (p.toFloat() / game.players - 0.5f), (-260).dp)
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Row(Modifier.weight(1f), horizontalArrangement = Arrangement.SpaceEvenly) {
                for (p in 1 until game.players) {
                    OpponentSeat("Игрок ${p + 1}", game.hands[p].size, game.whoActs() == p, game.taking && game.defender == p)
                }
            }
            DeckPile(game.deckLeft, game.trumpCard)
        }
        // Table: pairs, up to six, in two rows. The cover sits on its attack card, shifted so both stay readable.
        Box(Modifier.weight(1f).fillMaxWidth()) {
            Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterVertically)) {
                game.table.chunked(3).forEach { row ->
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp, Alignment.CenterHorizontally)) {
                        row.forEach { t ->
                            key(t.attack) {
                                Box(Modifier.size(84.dp, 96.dp)) {
                                    PlayingCard(t.attack, false, false, Modifier.width(52.dp).dealIn(seat(t.from)))
                                    t.cover?.let { c -> key(c) { PlayingCard(c, false, false, Modifier.width(52.dp).offset(26.dp, 18.dp).dealIn(seat(game.defender))) } }
                                }
                            }
                        }
                    }
                }
            }
            leaving?.let { l ->
                key(l) { LeavingCards(l.cards, l.to?.let { seat(it) } ?: DpOffset(screenW * 0.7f, (-30).dp)) { if (leaving === l) leaving = null } }
            }
        }
        Text(role(), Modifier.fillMaxWidth(), textAlign = TextAlign.Center, fontWeight = FontWeight.Bold, fontSize = 17.sp)
        if (message.isNotEmpty()) Text(message, Modifier.fillMaxWidth(), textAlign = TextAlign.Center, color = Rose, fontSize = 13.sp)
        Spacer(Modifier.height(6.dp))
        // Shared table: while the device holds the hand, nobody at the table sees it here. Without a device it is the only way to play.
        if (deviceHoldsCards) {
            Text("Ваши карты на устройстве: ${hand.size}", Modifier.fillMaxWidth(), textAlign = TextAlign.Center, color = Grey)
        } else BoxWithConstraints(Modifier.fillMaxWidth().height(120.dp)) {
            version.hashCode() // the hand is changed in place too
            val cardW = 62.dp
            val n = hand.size
            val step = if (n > 1) minOf(cardW + 4.dp, (maxWidth - cardW) / (n - 1)) else 0.dp
            val total = cardW + step * (n - 1).coerceAtLeast(0)
            val start = (maxWidth - total) / 2
            hand.forEachIndexed { i, c ->
                key(c) {
                    // Cards slide to their new places as the hand changes; new ones come down from the table.
                    val x by animateDpAsState(start + step * i, tween(300), label = "x")
                    val y by animateDpAsState(if (c in marked) 0.dp else 18.dp, tween(180), label = "y")
                    PlayingCard(c, i == focus, c in marked, Modifier.width(cardW).offset(x, y).dealIn(DpOffset(0.dp, (-280).dp), i * 60)) { toggle(i) }
                }
            }
        }
        Spacer(Modifier.height(6.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally)) {
            val mine = game.whoActs() == 0
            if (!deviceHoldsCards) {
                Button(onClick = { play() }, enabled = mine) { Text("На стол") }
                if (game.transferMode) OutlinedButton(onClick = { transfer() }, enabled = mine && game.defender == 0) { Text("Перевод") }
                OutlinedButton(onClick = { passOrTake() }, enabled = mine) { Text(passLabel()) }
            }
            if (game.finished) Button(onClick = onExit) { Text("Заново") }
        }
    }
}
