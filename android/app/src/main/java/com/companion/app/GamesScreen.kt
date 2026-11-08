package com.companion.app

import androidx.activity.compose.BackHandler
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowForward
import androidx.compose.material.icons.automirrored.rounded.HelpOutline
import androidx.compose.material.icons.rounded.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
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

private class BigGameInfo(val players: String, val about: String, val controls: List<String>)

// The device shows its own controls while it is a gamepad, so the phone only names them on the card.
private enum class BigGame(val title: String, val icon: ImageVector, val info: BigGameInfo) {
    Race("Гонки", Icons.Rounded.DirectionsCar, BigGameInfo("1–4 игрока", "Кольцевая гонка на три круга. Свободные места занимает компьютер, трава замедляет.",
        listOf("Игрок 1 — наклон компаньона", "Игроки 2–4 — кнопки на телефоне"))),
    Durak("Дурак", Icons.Rounded.Style, BigGameInfo("2–4 игрока", "Колода 36 карт, подкидной или переводной. Ваши карты видны только на компаньоне.",
        listOf("карта — касание", "беру · бито · перевод"))),
    Poker("Покер", Icons.Rounded.Toll, BigGameInfo("2–4 игрока", "Техасский холдем с фиксированным лимитом. Две ваши карты видны только на компаньоне.",
        listOf("пас · чек · рейз", "с телефона или компаньона"))),
    Breakout("Арканоид", Icons.Rounded.ViewModule, BigGameInfo("1 игрок", "Выбейте все кирпичи и не уроните шарик.",
        listOf("наклон — ракетка", "касание — запуск"))),
}

/** Games and tools that run on the device's own screen. */
private val DEVICE_SCREENS = listOf(
    Triple(Icons.Rounded.AutoAwesomeMotion, "21", "blackjack"), Triple(Icons.Rounded.Explore, "Лабиринт", "maze"),
    Triple(Icons.Rounded.TrackChanges, "Орбита", "orbit"), Triple(Icons.Rounded.MonetizationOn, "Монетка", "coin"),
    Triple(Icons.Rounded.Casino, "Кубики", "dice"), Triple(Icons.Rounded.AutoAwesome, "Шар", "magic_ball"),
    Triple(Icons.Rounded.SportsKabaddi, "Дуэль", "duel"), Triple(Icons.Rounded.Vibration, "Шейкер", "shaker"),
    Triple(Icons.Rounded.Lock, "Сейф", "safe"), Triple(Icons.Rounded.Balance, "Баланс", "balance"),
    Triple(Icons.Rounded.FitnessCenter, "Зарядка", "workout"), Triple(Icons.Rounded.Gavel, "Кротики", "mole"),
    Triple(Icons.Rounded.Gesture, "Змейка", "snake"), Triple(Icons.Rounded.Psychology, "Память", "simon"),
    Triple(Icons.Rounded.Tag, "Крестики", "tictactoe"),
    Triple(Icons.AutoMirrored.Rounded.HelpOutline, "Угадай", "guess"), Triple(Icons.Rounded.Flare, "Светлячок", "pet_game"),
    Triple(Icons.Rounded.Schedule, "Часы", "digital_clock"), Triple(Icons.Rounded.Cloud, "Погода", "weather"),
    Triple(Icons.Rounded.Visibility, "Лицо", "face"),
)

/** Host side of controller mode: the game runs here, the small device is the gamepad. */
@Composable
fun GamesTab(client: BleClient) {
    var game by rememberSaveable { mutableStateOf<BigGame?>(null) }
    val current = game
    if (current == null) {
        GamesHome(client) { game = it }
        return
    }
    BackHandler { game = null }
    KeepControllerAlive(client)
    var sub by remember { mutableStateOf("") }
    key(current) {
        Column(Modifier.fillMaxSize().slideIn()) {
            Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, bottom = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                BackTitle(current.title, 20, onBack = { game = null })
                Spacer(Modifier.weight(1f))
                Text(sub, color = Night.Muted, fontSize = 12.5.sp)
            }
            Box(Modifier.weight(1f).fillMaxWidth()) {
                val toList = { game = null }
                when (current) {
                    BigGame.Race -> RaceHost(client, toList) { sub = it }
                    BigGame.Durak -> DurakHost(client, toList) { sub = it }
                    BigGame.Poker -> PokerHost(client, toList) { sub = it }
                    BigGame.Breakout -> BreakoutHost(client) { sub = it }
                }
            }
        }
    }
    DisposableEffect(Unit) { onDispose { client.send(Protocol.command("controller", "value" to "stop")) } }
}

@Composable
private fun GamesHome(client: BleClient, open: (BigGame) -> Unit) {
    val link by client.link.collectAsState()
    LazyColumn(Modifier.fillMaxSize().enter(), contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 4.dp, bottom = 16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item {
            Column(Modifier.padding(start = 4.dp, end = 4.dp, top = 2.dp, bottom = 6.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("Игры за столом", fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
                Text("Положите телефон на стол, его видят все. Компаньон в руке становится контроллером.", color = Night.Muted,
                    fontSize = 13.5.sp, lineHeight = 19.5.sp)
            }
        }
        items(BigGame.entries) { g -> GameCard(g) { open(g) } }
        if (link == LinkState.Authenticated) {
            item { Text("На устройстве", Modifier.padding(start = 4.dp, top = 8.dp), color = Night.Muted, fontSize = 13.sp) }
            items(DEVICE_SCREENS.chunked(4)) { row ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    row.forEach { (icon, title, screen) ->
                        Column(Modifier.weight(1f).tap { client.send(Protocol.command("screen", "value" to screen)) }.clip(RoundedCornerShape(18.dp))
                            .background(Night.Surface).padding(vertical = 12.dp), horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            MIcon(icon, 24.dp, Night.Accent)
                            Text(title, fontSize = 11.5.sp, color = Night.Text2, maxLines = 1)
                        }
                    }
                    repeat(4 - row.size) { Spacer(Modifier.weight(1f)) }
                }
            }
        }
    }
}

@Composable
private fun GameCard(g: BigGame, onClick: () -> Unit) {
    Column(Modifier.fillMaxWidth().tap(onClick).clip(RoundedCornerShape(26.dp)).background(Night.Surface).padding(18.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Box(Modifier.size(50.dp).clip(RoundedCornerShape(16.dp)).background(Night.Hover), contentAlignment = Alignment.Center) { MIcon(g.icon, 28.dp, Night.Accent) }
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(g.title, fontSize = 19.sp, fontWeight = FontWeight.SemiBold)
                Text(g.info.players, color = Night.Muted, fontSize = 12.5.sp)
            }
            MIcon(Icons.AutoMirrored.Rounded.ArrowForward, 22.dp, Night.Quiet)
        }
        Text(g.info.about, color = Night.Text2, fontSize = 13.5.sp, lineHeight = 19.5.sp)
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            g.info.controls.forEach { c ->
                Box(Modifier.height(26.dp).clip(RoundedCornerShape(13.dp)).background(Night.Hover).padding(horizontal = 10.dp), contentAlignment = Alignment.Center) {
                    Text(c, color = Night.Chip, fontSize = 11.5.sp, maxLines = 1)
                }
            }
        }
    }
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

// `buttons`: labels the device draws (design 1a). Cards: fold, main move, raise; pad: the hold pill. Older firmware ignores them.
private fun BleClient.startPad(title: String, hint: String, info: String = "", buttons: String = "") =
    send(Protocol.command("controller", "value" to "start", "mode" to "pad", "title" to title, "hint" to hint, "info" to info,
        "buttons" to buttons))

private fun BleClient.showCards(title: String, cards: List<Card>, marks: String = "", hint: String = "", info: String = "", target: Card? = null,
                                buttons: String = "") =
    send(Protocol.command("controller", "value" to "start", "mode" to "cards", "title" to title,
        "labels" to cards.joinToString(",") { it.label }, "marks" to marks, "hint" to hint, "info" to info,
        "target" to (target?.label ?: ""), "buttons" to buttons))

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

// ---- Setup ---------------------------------------------------------------------------------------

/** Every game starts the same way: players on top, the game's own choice below, one big button at the bottom. */
@Composable
private fun GameSetup(players: Int, range: IntRange, seats: List<Pair<String, String>>, onPlayers: (Int) -> Unit, onStart: () -> Unit,
                      content: @Composable ColumnScope.() -> Unit) {
    Column(Modifier.fillMaxSize().padding(start = 16.dp, end = 16.dp, bottom = 14.dp)) {
        Column(Modifier.weight(1f).verticalScroll(androidx.compose.foundation.rememberScrollState())) {
            Text("Игроки", Modifier.padding(start = 4.dp, end = 4.dp, top = 8.dp, bottom = 8.dp), color = Night.Muted, fontSize = 13.sp)
            Segments(range.map { "$it" to it }, players, Icons.Rounded.Person, onSelect = onPlayers)
            Column(Modifier.padding(start = 4.dp, end = 4.dp, top = 10.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                seats.forEachIndexed { i, (name, role) ->
                    key(name, players) {
                        Row(Modifier.height(36.dp).enter(ms = 300), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                            Box(Modifier.size(10.dp).clip(CircleShape).background(seatColor(i, seats, name)))
                            Text(name, Modifier.width(110.dp), fontSize = 14.sp, fontWeight = FontWeight.Medium)
                            Text(role, color = Night.Muted, fontSize = 12.5.sp)
                        }
                    }
                }
            }
            content()
        }
        PillButton("Играть", Modifier.fillMaxWidth().padding(top = 12.dp), primary = true, height = 58.dp, icon = Icons.Rounded.PlayArrow, fontSize = 18, onClick = onStart)
    }
}

/** People get the player colours, computers the greys, in seat order. */
private fun seatColor(i: Int, seats: List<Pair<String, String>>, name: String): Color =
    if (name.startsWith("Компьютер")) Night.Computers[seats.take(i).count { it.first.startsWith("Компьютер") } % 3] else Night.Players[i]

private fun cardSeats(players: Int) = List(players) { i -> if (i == 0) "Вы" to "карты на компаньоне" else seatName(i, players) to "соперник" }

@Composable
private fun SectionLabel(text: String) = Text(text, Modifier.padding(start = 4.dp, end = 4.dp, top = 16.dp, bottom = 8.dp), color = Night.Muted, fontSize = 13.sp)

// ---- Race ----------------------------------------------------------------------------------------

private fun carColor(game: RaceGame, car: RaceCar): Color {
    val i = game.cars.indexOf(car)
    return if (car.human) Night.Players[i] else Night.Computers[(i - game.cars.count { it.human }).coerceAtLeast(0) % 3]
}

private fun carName(game: RaceGame, car: RaceCar) = if (car.human) "Игрок ${game.cars.indexOf(car) + 1}" else "Комп."

private fun DrawScope.drawTrack(track: RaceTrack, scale: Float, ox: Float, oy: Float, preview: Boolean = false) {
    val path = Path()
    for (i in 0 until RaceTrack.SAMPLES step 3) {
        val x = ox + track.xs[i] * scale
        val y = oy + track.ys[i] * scale
        if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
    }
    path.close()
    val road = RaceGame.ROAD_HALF * 2 * scale
    drawPath(path, Color(0xFF55535C), style = Stroke(road * 46f / 42f, join = StrokeJoin.Round))
    drawPath(path, Color(0xFF24242A), style = Stroke(road, join = StrokeJoin.Round))
    val dash = if (preview) floatArrayOf(scale * 0.05f, scale * 0.05f) else floatArrayOf(scale * 0.026f, scale * 0.032f)
    drawPath(path, Night.Disabled, style = Stroke(if (preview) scale * 0.009f else scale * 0.004f, pathEffect = PathEffect.dashPathEffect(dash)))
    if (preview) return
    // Chequered start line: light sand and black, the cars keep their own colours.
    val a = track.direction(0)
    val nx = -sin(a); val ny = cos(a)
    val sq = road / 8
    for (row in 0..1) for (k in 0 until 8) {
        val along = (row - 0.5f) * sq
        val across = (k + 0.5f) * sq - road / 2
        val cx = ox + track.xs[0] * scale + cos(a) * along + nx * across
        val cy = oy + track.ys[0] * scale + sin(a) * along + ny * across
        drawLine(if ((k + row) % 2 == 0) Night.Fill else Color(0xFF111111), Offset(cx - nx * sq / 2, cy - ny * sq / 2), Offset(cx + nx * sq / 2, cy + ny * sq / 2), strokeWidth = sq)
    }
}

@Composable
private fun RaceHost(client: BleClient, toList: () -> Unit, setSub: (String) -> Unit) {
    var setup by remember { mutableStateOf(true) }
    var players by remember { mutableIntStateOf(1) }
    var trackIndex by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) { client.startPad("Гонки", "наклон руль · держи экран тормоз", buttons = "Держи — тормоз") }
    LaunchedEffect(setup, players) { setSub(if (setup) "" else "3 круга · ${if (players == 1) "1 игрок" else "$players игрока"}") }
    if (setup) {
        val seats = List(4) { i -> if (i < players) "Игрок ${i + 1}" to (if (i == 0) "компаньон, наклон" else "кнопки на телефоне") else "Компьютер" to "свободное место" }
        GameSetup(players, 1..4, seats, { players = it }, { setup = false }) {
            SectionLabel("Трасса")
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                RACE_TRACKS.forEachIndexed { i, t ->
                    val on = i == trackIndex
                    val bg by animateColorAsState(if (on) Color(0xFF232329) else Color(0xFF17171C), tween(250), label = "track")
                    Column(Modifier.weight(1f).clip(RoundedCornerShape(18.dp)).background(bg)
                        .border(1.5.dp, if (on) Night.Accent else Color.Transparent, RoundedCornerShape(18.dp))
                        .clickable { trackIndex = i }.padding(vertical = 10.dp, horizontal = 6.dp), horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        Canvas(Modifier.size(76.dp, 104.dp)) {
                            val scale = minOf(size.width, size.height / RaceGame.ARENA_H)
                            drawTrack(t, scale, (size.width - scale) / 2, (size.height - scale * RaceGame.ARENA_H) / 2, preview = true)
                        }
                        Text(t.name, fontSize = 12.5.sp)
                    }
                }
            }
        }
        return
    }
    RaceTable(client, RACE_TRACKS[trackIndex], players, toList)
}

/** A button that reports both press and release, several at once: every player has their own. */
@Composable
private fun HoldPad(icon: ImageVector, description: String, color: Color, onChange: (Boolean) -> Unit) {
    Box(Modifier.size(44.dp).clip(RoundedCornerShape(14.dp)).background(color).semantics { contentDescription = description }
        .pointerInput(Unit) { awaitEachGesture { awaitFirstDown(); onChange(true); waitForUpOrCancellation(); onChange(false) } },
        contentAlignment = Alignment.Center) { MIcon(icon, 26.dp, Night.OnAccent) }
}

/** The big 3-2-1: each number lands large and shrinks away within its second. */
@Composable
private fun BoxScope.CountPop(text: String, size: Int, color: Color, ms: Int) {
    key(text) {
        val p = remember { Animatable(0f) }
        LaunchedEffect(Unit) { p.animateTo(1f, tween(ms)) }
        Text(text, Modifier.align(Alignment.Center).graphicsLayer {
            val t = p.value
            val s = if (t < .25f) 1.9f - .9f * (t / .25f) else 1f - .1f * ((t - .25f) / .75f)
            scaleX = s; scaleY = s
            alpha = if (t < .25f) t / .25f else 1f - (t - .25f) / .75f
        }, color = color, fontSize = size.sp, fontWeight = FontWeight.Bold)
    }
}

@Composable
private fun RaceTable(client: BleClient, track: RaceTrack, players: Int, toList: () -> Unit) {
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
    // The device is player 1's dashboard: its speed in km/h, a few times a second.
    LaunchedEffect(round, hasDevice) {
        while (hasDevice) {
            delay(400)
            val kmh = (game.cars[0].speed / RaceGame.TOP_SPEED * 120).toInt()
            client.startPad("Гонки", "км/ч · наклон — руль", info = "$kmh", buttons = "Держи — тормоз")
        }
    }
    var since by remember(round) { mutableFloatStateOf(0f) }
    FrameLoop { dt ->
        for (i in 0 until players) {
            steer[i] = (if (right[i]) 1f else 0f) - (if (left[i]) 1f else 0f)
            brake[i] = left[i] && right[i]
        }
        if (hasDevice) { steer[0] = tilt.first * FULL_LEAN; brake[0] = deviceBrake }
        game.step(dt, steer, brake)
        if (game.countdown <= 0f) since += dt
        frame++
    }
    frame.hashCode() // read during composition, not only in the canvas: the standings must update too
    Box(Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxWidth().padding(start = 14.dp, end = 14.dp, bottom = 4.dp)) {
                game.standings.forEachIndexed { place, car ->
                    Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(2.dp)) {
                        Text("${place + 1}. ${carName(game, car)}", color = if (car.human) carColor(game, car) else Night.Muted, fontSize = 12.5.sp,
                            fontWeight = FontWeight.SemiBold, maxLines = 1)
                        Text(if (car.finished) "финиш" else "круг ${game.lap(car)}/${game.laps}", color = Night.Quiet, fontSize = 11.sp)
                    }
                }
            }
            Box(Modifier.weight(1f).fillMaxWidth().padding(horizontal = 22.dp, vertical = 6.dp)) {
                Canvas(Modifier.fillMaxSize()) {
                    frame.hashCode() // also read here: Compose skips a draw lambda whose captures did not change, freezing the picture
                    val scale = minOf(size.width, size.height / RaceGame.ARENA_H)
                    val ox = (size.width - scale) / 2
                    val oy = (size.height - scale * RaceGame.ARENA_H) / 2
                    drawTrack(game.track, scale, ox, oy)
                    val l = scale * 0.053f
                    val wd = scale * 0.03f
                    for (car in game.cars) {
                        val p = Offset(ox + car.x * scale, oy + car.y * scale)
                        rotate(Math.toDegrees(car.heading.toDouble()).toFloat(), p) {
                            drawRoundRect(carColor(game, car), Offset(p.x - l / 2, p.y - wd / 2), Size(l, wd), CornerRadius(wd * 0.3f))
                            drawRoundRect(Night.OnAccent, Offset(p.x + l * 0.06f, p.y - wd * 0.35f), Size(l * 0.25f, wd * 0.7f), CornerRadius(wd * 0.12f))
                        }
                    }
                }
                if (game.countdown > 0f) CountPop("${ceil(game.countdown).toInt()}", 96, Night.Text, 1000)
                else if (since < 0.8f) CountPop("Старт!", 54, Night.Accent, 800)
            }
            // Players 2–4 on the phone, two buttons each in their colour; without a device player 1 too.
            val pads = (if (hasDevice) 1 until players else 0 until players).toList()
            if (pads.isEmpty()) Text("Рулите наклоном компаньона, держите палец на экране — тормоз", Modifier.fillMaxWidth().padding(start = 14.dp, end = 14.dp, top = 4.dp, bottom = 8.dp),
                color = Night.Muted, fontSize = 12.5.sp, textAlign = TextAlign.Center)
            pads.chunked(3).forEach { row ->
                Row(Modifier.fillMaxWidth().padding(start = 14.dp, end = 14.dp, top = 4.dp, bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(18.dp, Alignment.CenterHorizontally)) {
                    for (i in row) Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            HoldPad(Icons.Rounded.ChevronLeft, "влево, игрок ${i + 1}", Night.Players[i]) { left[i] = it }
                            HoldPad(Icons.Rounded.ChevronRight, "вправо, игрок ${i + 1}", Night.Players[i]) { right[i] = it }
                        }
                        Text("Игрок ${i + 1}", color = Night.Muted, fontSize = 10.5.sp)
                    }
                }
            }
        }
        if (game.over) Overlay("Финиш", "Три круга пройдены", actions = {
            PillButton("К играм", onClick = toList)
            PillButton("Ещё заезд", primary = true) { round++ }
        }) {
            game.standings.forEachIndexed { place, car ->
                val time = car.finishTime?.let { "%.1f с".format(it) } ?: "не доехал"
                Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(if (place == 0) Night.Hover else Color.Transparent)
                    .padding(horizontal = 12.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text("${place + 1}", Modifier.width(18.dp), color = if (place == 0) Night.Accent else Night.Muted, fontWeight = FontWeight.Bold)
                    Box(Modifier.size(10.dp).clip(CircleShape).background(carColor(game, car)))
                    Text(carName(game, car), Modifier.weight(1f), fontSize = 15.sp)
                    Text(time, color = Night.Muted, fontSize = 13.sp)
                }
            }
        }
    }
}

/** End of a game: a blurred-looking veil, a big title, rows, two buttons. */
@Composable
private fun BoxScope.Overlay(title: String, sub: String, actions: @Composable RowScope.() -> Unit, rows: @Composable ColumnScope.() -> Unit = {}) {
    Column(Modifier.matchParentSize().background(Night.Bg.copy(alpha = .92f)).clickable(remember { androidx.compose.foundation.interaction.MutableInteractionSource() }, null) {}
        .padding(24.dp).enter(ms = 400), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(14.dp, Alignment.CenterVertically)) {
        Text(title, fontSize = 28.sp, fontWeight = FontWeight.Bold, textAlign = TextAlign.Center)
        if (sub.isNotEmpty()) Text(sub, color = Night.Muted, fontSize = 14.sp, textAlign = TextAlign.Center)
        Column(Modifier.widthIn(max = 260.dp).padding(vertical = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp), content = rows)
        Row(Modifier.padding(top = 6.dp), horizontalArrangement = Arrangement.spacedBy(10.dp), content = actions)
    }
}

// ---- Cards ---------------------------------------------------------------------------------------

/** Russian ranks on the table; the device keeps its own Latin labels. */
private val Card.ruRank get() = when (rank) { 14 -> "Т"; 13 -> "К"; 12 -> "Д"; 11 -> "В"; else -> rank.toString() }

/** A face-down card is [card] == null. Everything scales with the width, so one card works in a hand, a fan and on the table. */
@Composable
private fun PlayingCard(card: Card?, selected: Boolean, held: Boolean, modifier: Modifier = Modifier, dim: Boolean = false, onClick: (() -> Unit)? = null) {
    val red = card != null && (card.suit == 1 || card.suit == 2)
    BoxWithConstraints(modifier.aspectRatio(1f / 1.42f), contentAlignment = Alignment.Center) {
        val w = maxWidth.value
        val shape = RoundedCornerShape((w * .16f).dp)
        Box(Modifier.fillMaxSize().graphicsLayer { alpha = if (dim) .35f else 1f }.shadow(5.dp, shape, ambientColor = Color.Black, spotColor = Color.Black)
            .clip(shape).background(if (card == null) Color(0xFF1C1C22) else Night.Text)
            .border(if (selected) 3.dp else 1.dp, when { selected -> Night.Accent; card == null -> Color(0xFF303039); else -> Color.White }, shape)
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier), contentAlignment = Alignment.Center) {
            if (card != null) {
                val ink = if (red) Color(0xFFB65A6E) else Color(0xFF17171B)
                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy((-2).dp)) {
                    Text(card.ruRank, color = ink, fontWeight = FontWeight.Bold, fontSize = (w * .36f).sp, lineHeight = (w * .4f).sp)
                    Text("♠♥♦♣"[card.suit] + Char(0xFE0E).toString(), color = ink, fontSize = (w * .4f).sp, lineHeight = (w * .44f).sp) // FE0E: a glyph, never an emoji
                }
            } else CardBack(w)
            if (held) Box(Modifier.align(Alignment.TopCenter).padding(4.dp).size(8.dp).clip(CircleShape).background(Night.Players[2]))
        }
    }
}

/** The back: an inner frame with fine diagonal stripes. */
@Composable
private fun CardBack(w: Float) {
    val r = (w * .1f).dp
    Box(Modifier.fillMaxWidth(.7f).fillMaxHeight(.78f).clip(RoundedCornerShape(r)).background(Color(0xFF202028)).drawBehind {
        val step = 3.dp.toPx()
        clipRect {
            var x = -size.height
            while (x < size.width) {
                drawLine(Color(0xFF26262F), Offset(x, size.height), Offset(x + size.height, 0f), strokeWidth = step * 0.7f)
                x += step * 2
            }
        }
    }.border(1.5.dp, Night.Line2, RoundedCornerShape(r)))
}

/** The card arrives from [from], relative to where it lands, straightening as it comes: a deal, a throw, a cover. */
private fun Modifier.dealIn(from: DpOffset, delayMs: Int = 0): Modifier = composed {
    val p = remember { Animatable(0f) }
    LaunchedEffect(Unit) { p.animateTo(1f, tween(450, delayMs, androidx.compose.animation.core.CubicBezierEasing(.2f, .8f, .3f, 1f))) }
    graphicsLayer {
        val k = 1f - p.value
        translationX = from.x.toPx() * k
        translationY = from.y.toPx() * k
        rotationZ = (if (from.y < 0.dp) -14f else 12f) * k
        alpha = if (p.value > 0f) 1f else 0f // hidden until its turn in a staggered deal
    }
}

/** Starts face down, so a card that should be seen turns over in view; turns back if [faceUp] goes false. */
@Composable
private fun FlipCard(card: Card, faceUp: Boolean, modifier: Modifier = Modifier, delayMs: Int = 0, dim: Boolean = false) {
    val angle = remember { Animatable(0f) }
    LaunchedEffect(faceUp) { angle.animateTo(if (faceUp) 180f else 0f, tween(450, delayMs, FastOutSlowInEasing)) }
    val front = angle.value > 90f
    PlayingCard(if (front) card else null, false, false, modifier.graphicsLayer {
        rotationY = if (front) angle.value - 180f else angle.value
        cameraDistance = 12f * density
    }, dim = dim)
}

/** Cards leaving the table together: to the player who takes them, or off to the discard. */
@Composable
private fun LeavingCards(cards: List<Card>, to: DpOffset, onDone: () -> Unit) {
    val p = remember { Animatable(0f) }
    LaunchedEffect(Unit) { p.animateTo(1f, tween(560, easing = FastOutSlowInEasing)); onDone() }
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        cards.forEachIndexed { k, c ->
            val spread = k - (cards.size - 1) / 2f
            PlayingCard(c, false, false, Modifier.width(50.dp).graphicsLayer {
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
private fun EmptySlot(width: Dp) =
    Box(Modifier.width(width).aspectRatio(1f / 1.42f).drawBehind {
        drawRoundRect(Color(0xFF2E2E35), style = Stroke(1.dp.toPx(), pathEffect = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 3.dp.toPx()))),
            cornerRadius = CornerRadius(9.dp.toPx()))
    })

@Composable
private fun pulseTag(on: Boolean): Float {
    if (!on) return 1f
    val t = rememberInfiniteTransition(label = "tag")
    return t.animateFloat(1f, .3f, infiniteRepeatable(tween(700), RepeatMode.Reverse), label = "tag").value
}

/** A computer at the top of the table: a fan of backs, up to seven, its name and what it is doing. */
@Composable
private fun OpponentSeat(name: String, cards: Int, tag: String, out: Boolean) {
    Column(Modifier.graphicsLayer { alpha = if (out) .4f else 1f }, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Box(Modifier.size(96.dp, 52.dp)) {
            val m = minOf(cards, 7)
            for (k in 0 until m) {
                val o = k - (m - 1) / 2f
                PlayingCard(null, false, false, Modifier.offset(x = (36 + o * 9).dp, y = (4 + kotlin.math.abs(o) * 1.5f).dp).width(26.dp).graphicsLayer {
                    transformOrigin = TransformOrigin(0.5f, 1.2f)
                    rotationZ = o * 9f
                })
            }
        }
        Text("$name · $cards", color = Night.Text2, fontSize = 12.5.sp)
        val a = pulseTag(tag.isNotEmpty())
        Text(tag, Modifier.height(16.dp).graphicsLayer { alpha = a }, color = if (tag == "берёт") Night.Rose else Night.Accent, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
    }
}

/** The stock with the trump card under it, as on a real table; only the trump's suit stays once the stock is gone. */
@Composable
private fun DeckPile(left: Int, trump: Card) {
    Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Box(Modifier.size(58.dp, 52.dp)) {
            if (left > 0) {
                PlayingCard(trump, false, false, Modifier.offset(x = 16.dp, y = 2.dp).width(30.dp).graphicsLayer { rotationZ = 90f })
                if (left > 1) PlayingCard(null, false, false, Modifier.offset(x = 4.dp, y = 2.dp).width(30.dp))
            } else Text("♠♥♦♣"[trump.suit] + Char(0xFE0E).toString(), Modifier.align(Alignment.Center), fontSize = 28.sp,
                color = if (trump.suit == 1 || trump.suit == 2) Color(0xFFB65A6E) else Night.Text2)
        }
        Text(if (left > 0) "$left" else "колода пуста", color = Night.Muted, fontSize = 12.sp)
    }
}

// ---- Poker ---------------------------------------------------------------------------------------

/**
 * Texas Hold'em. This screen is the table everyone sees: shared cards, pot and seats. The player's two cards live on
 * the device and stay face down here until the showdown. Without a device they are shown here, or nobody could play.
 */
@Composable
private fun PokerHost(client: BleClient, toList: () -> Unit, setSub: (String) -> Unit) {
    var game by remember { mutableStateOf<HoldemGame?>(null) }
    var players by remember { mutableIntStateOf(2) }
    var hand by remember { mutableIntStateOf(1) }
    val current = game
    LaunchedEffect(current, hand) { setSub(if (current == null) "" else "раздача $hand") }
    if (current == null) {
        LaunchedEffect(Unit) { client.startPad("Покер", "выберите число игроков на телефоне") }
        GameSetup(players, 2..4, cardSeats(players), { players = it }, { hand = 1; game = HoldemGame(players) }) {
            Text("Техасский холдем с фиксированным лимитом. Блайнды 5 и 10, у каждого 200 фишек. Ходить можно с телефона или с компаньона.",
                Modifier.padding(top = 16.dp).fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Night.Surface).padding(16.dp),
                color = Night.Text2, fontSize = 13.5.sp, lineHeight = 20.sp)
        }
        return
    }
    key(current) { PokerTable(client, current, toList, onNewGame = { hand = 1; game = HoldemGame(players) }) { hand++ } }
}

@Composable
private fun PokerTable(client: BleClient, game: HoldemGame, toList: () -> Unit, onNewGame: () -> Unit, onNewHand: () -> Unit) {
    var version by remember { mutableIntStateOf(0) }
    val link by client.link.collectAsState()
    val deviceHoldsCards = link == LinkState.Authenticated

    fun callLabel() = if (game.whoActs() == 0 && game.toCall(0) > 0) "Колл ${game.toCall(0)}" else "Чек"
    fun sync() {
        version++
        val mine = game.whoActs() == 0
        val hint = when {
            game.street == Street.GameOver -> "тап новая игра"
            game.street == Street.Showdown -> "тап новая раздача"
            mine && game.canRaise -> "тап ${callLabel().lowercase()}\n↑ рейз · ↓ пас"
            mine -> "тап ${callLabel().lowercase()} · ↓ пас"
            else -> "ждём соперников"
        }
        val state = when {
            game.street.ordinal >= Street.Showdown.ordinal -> game.message
            game.folded[0] -> "Вы сбросили карты"
            else -> game.street.title
        }
        // On the player's move the device shows the three moves as buttons; otherwise the hint says what is going on.
        val buttons = if (mine && game.street.ordinal < Street.Showdown.ordinal)
            "Пас,${if (game.toCall(0) > 0) "Колл" else "Чек"},Рейз" else ""
        client.showCards("фишки ${game.stacks[0]} · банк ${game.pot}", game.hole[0], "", hint, state, buttons = buttons)
    }
    fun act(action: String) {
        when (action) {
            "tap" -> when (game.street) {
                Street.GameOver -> { onNewGame(); return }
                Street.Showdown -> { game.newHand(); onNewHand() }
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
    val mine = game.whoActs() == 0
    Box(Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize().padding(bottom = 14.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 10.dp), horizontalArrangement = Arrangement.SpaceAround) {
                for (p in 1 until game.players) Seat(game, p, open, version)
            }
            Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp, Alignment.CenterVertically)) {
                key(game.street) { Text(game.street.title, Modifier.enter(ms = 300), color = Night.Muted, fontSize = 13.sp) }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    for (i in 0 until 5) {
                        val c = game.board.getOrNull(i)
                        // The flop lands one card after another; each turns over once it is down.
                        val delay = if (i < 3) i * 140 else 0
                        if (c == null) EmptySlot(56.dp)
                        else key(c) { FlipCard(c, true, Modifier.width(56.dp).dealIn(DpOffset(0.dp, (-160).dp), delay), delay + 260) }
                    }
                }
                key(open, game.pot) {
                    Text(if (open) game.message else "Банк: ${game.pot}", Modifier.padding(horizontal = 16.dp).pop(), fontSize = 22.sp, fontWeight = FontWeight.Bold,
                        color = Night.Fill, textAlign = TextAlign.Center)
                }
            }
            Text(if (deviceHoldsCards && !open) "Ваши карты на компаньоне" else "Ваши карты", color = Night.Quiet, fontSize = 12.5.sp)
            Spacer(Modifier.height(6.dp))
            Row(Modifier.border(2.dp, if (0 in game.winners) Night.Accent else Color.Transparent, RoundedCornerShape(10.dp)).padding(3.dp),
                horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                game.hole[0].forEachIndexed { i, c ->
                    key(c) { FlipCard(c, open && !game.folded[0] || !deviceHoldsCards, Modifier.width(44.dp).dealIn(DpOffset(0.dp, (-320).dp), i * 120), i * 120 + 260, dim = game.folded[0]) }
                }
            }
            Spacer(Modifier.height(6.dp))
            Text("Фишки: ${game.stacks[0]}" + (game.lastMove[0].takeIf { it.isNotEmpty() }?.let { " · $it" } ?: ""), color = Night.Muted, fontSize = 13.sp)
            Spacer(Modifier.height(10.dp))
            // On the table too: the same moves as on the device, and no others exist.
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (open) PillButton(if (game.street == Street.GameOver) "Новая игра" else "Следующая раздача", primary = true) { act("tap") }
                else {
                    PillButton("Пас", enabled = mine) { act("swipe_down") }
                    PillButton(callLabel(), Modifier.widthIn(min = 96.dp), primary = true, enabled = mine) { act("tap") }
                    PillButton("Рейз", enabled = mine && game.canRaise) { act("swipe_up") }
                }
            }
        }
        if (game.street == Street.GameOver) Overlay(game.message, "", actions = {
            PillButton("К играм", onClick = toList)
            PillButton("Новая игра", primary = true, onClick = onNewGame)
        })
    }
}

/** [version] only makes the seat redraw after a move: the game it reads is the same object every time. */
@Composable
private fun Seat(game: HoldemGame, p: Int, open: Boolean, @Suppress("UNUSED_PARAMETER") version: Int) {
    val active = game.whoActs() == p
    val out = game.hole[p].isEmpty()
    Column(Modifier.graphicsLayer { alpha = if (out) .35f else 1f }, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(5.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(game.seatName(p), color = if (active) Night.Text else Night.Text2, fontSize = 13.sp, fontWeight = if (active) FontWeight.SemiBold else FontWeight.Normal)
            if (game.dealer == p) Box(Modifier.size(18.dp).clip(CircleShape).background(Night.Fill), contentAlignment = Alignment.Center) {
                Text("D", color = Night.OnAccent, fontSize = 10.sp, fontWeight = FontWeight.Bold)
            }
        }
        val ring = when { p in game.winners -> 2.dp to Night.Accent; active -> 1.dp to Night.Line2; else -> 0.dp to Color.Transparent }
        Row(Modifier.border(ring.first, ring.second, RoundedCornerShape(10.dp)).padding(3.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            if (out) repeat(2) { PlayingCard(null, false, false, Modifier.width(34.dp), dim = true) }
            else game.hole[p].forEachIndexed { i, c -> key(c) { FlipCard(c, open && !game.folded[p], Modifier.width(34.dp).dealIn(DpOffset(0.dp, 260.dp), i * 120), dim = game.folded[p]) } }
        }
        Text("${game.stacks[p]}", color = Night.Fill, fontSize = 13.sp)
        val last = if (active) "думает…" else game.lastMove[p]
        Text(last, Modifier.height(15.dp), color = if (last == "пас") Night.Rose else Night.Accent, fontSize = 11.5.sp)
    }
}

// ---- Breakout ------------------------------------------------------------------------------------

@Composable
private fun BreakoutHost(client: BleClient, setSub: (String) -> Unit) {
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
    val colors = listOf(Night.Rose, Night.Sand, Night.Players[2], Night.Accent, Night.Fill)
    frame.hashCode() // read during composition, not only in the canvas: the score and messages must update too
    LaunchedEffect(game.score, game.lives) { setSub("счёт ${game.score} · жизни ${game.lives}") }
    Box(Modifier.fillMaxSize().padding(start = 16.dp, end = 16.dp, bottom = 14.dp).clip(RoundedCornerShape(24.dp)).background(Night.Surface)
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
                    drawRoundRect(colors[r % colors.size], Offset(c * bw + 2, h * 0.08f + r * bh + 2), Size(bw - 4, bh - 4), CornerRadius(6f))
                }
            }
            drawRoundRect(Color.White, Offset((game.paddle - game.paddleWidth / 2) * w, h * 0.93f), Size(game.paddleWidth * w, h * 0.018f), CornerRadius(8f))
            drawCircle(Color.White, w * 0.018f, Offset(game.ballX * w, game.ballY * h))
        }
        if (!game.launched && !game.over && !game.won) Text("Нажмите — старт", Modifier.align(Alignment.Center).padding(24.dp), color = Night.Fill, fontSize = 18.sp)
        if (game.over || game.won) {
            Text(if (game.won) "Победа: ${game.score}\nНажмите — ещё раз" else "Счёт: ${game.score}\nНажмите — ещё раз",
                Modifier.align(Alignment.Center), fontSize = 20.sp, textAlign = TextAlign.Center, fontWeight = FontWeight.SemiBold)
        }
    }
}

// ---- Durak --------------------------------------------------------------------------------------
private val SUIT_NAMES = listOf("пики", "черви", "бубны", "трефы")
private val SUITS = "♠♥♦♣"

@Composable
private fun DurakHost(client: BleClient, toList: () -> Unit, setSub: (String) -> Unit) {
    var game by remember { mutableStateOf<DurakGame?>(null) }
    var players by remember { mutableIntStateOf(2) }
    var transfer by remember { mutableStateOf(false) }
    val current = game
    LaunchedEffect(current) { setSub(if (current == null) "" else "${if (transfer) "переводной" else "подкидной"} · козырь ${SUITS[current.trump]}") }
    if (current == null) {
        LaunchedEffect(Unit) { client.startPad("Дурак", "выберите режим на телефоне") }
        GameSetup(players, 2..4, cardSeats(players), { players = it }, { game = DurakGame(players, transfer) }) {
            SectionLabel("Вариант")
            Segments(listOf("Подкидной" to false, "Переводной" to true), transfer, fontSize = 15) { transfer = it }
            Text(if (transfer) "Отбивающийся может перевести ход дальше, положив карту того же достоинства."
                else "Подкидывать можно карты того же достоинства, что уже лежат на столе.",
                Modifier.padding(start = 4.dp, end = 4.dp, top = 12.dp), color = Night.Text2, fontSize = 13.sp, lineHeight = 19.5.sp)
        }
        return
    }
    key(current) { DurakTable(client, current, toList) { game = DurakGame(players, transfer) } }
}

/** Cards of a finished bout on their way: [to] is the player who took them, null is the discard. */
private class Leaving(val cards: List<Card>, val to: Int?)

private class LastTable(var cards: List<Card>, var defender: Int, var discarded: Int)

/**
 * Device flow: lean to walk through the hand, tap marks cards, swipe up throws everything marked (or the focused card)
 * on the table, swipe down takes or ends the bout, swipe right transfers. The phone offers the same by touch.
 */
@Composable
private fun DurakTable(client: BleClient, game: DurakGame, toList: () -> Unit, onAgain: () -> Unit) {
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
    fun name(p: Int) = seatName(p, game.players)

    fun chosen(): List<Card> = marked.filter { it in hand }.ifEmpty { listOfNotNull(hand.getOrNull(focus)) }
    // While the defender takes, "бито" would be a lie: the cards go to them, not to the discard.
    fun passLabel() = when { game.defender == 0 -> "Беру"; game.taking -> "Пас"; else -> "Бито" }
    // The device's own status line; the phone shows status() below.
    fun role(): String = when {
        game.finished -> when (game.durak) { null -> "Ничья"; 0 -> "Вы дурак!"; else -> "Вы вышли, дурак: ${name(game.durak!!)}" }
        // Whoever takes is named while the others still throw in: they will not lead the next bout.
        game.taking -> if (game.defender == 0) "Вы берёте" else "${name(game.defender)} берёт"
        game.whoActs() != 0 -> "Ходит ${name(game.whoActs()!!)}"
        game.defender == 0 -> "Отбивайтесь"
        game.table.isEmpty() -> "Ваш ход"
        else -> "Подкидывайте или бито"
    }
    fun status(): Pair<String, String> {
        val where = if (deviceHoldsCards) "Выберите карту на компаньоне" else "Выберите карту внизу"
        val a = game.whoActs()
        return when {
            game.finished -> "Партия окончена" to ""
            a != 0 && a != null -> "${name(a)} думает…" to "${name(a)} ${if (a == game.defender) "отбивается" else "ходит"}…"
            game.defender == 0 -> "Отбейтесь или возьмите" to
                (if (game.transferMode && game.table.none { it.cover != null }) "Можно перевести картой того же достоинства" else where)
            game.table.isEmpty() -> "Ваш ход" to where
            game.taking -> "${name(game.defender)} берёт" to "Подкиньте или «Пас»"
            else -> "Подкиньте или «Бито»" to "Подходят карты того же достоинства"
        }
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
                "tap" -> if (game.finished) onAgain() else toggle(a.index)
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
    val (st, sub) = status()
    Box(Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxWidth().padding(start = 10.dp, end = 12.dp), verticalAlignment = Alignment.Top) {
                Row(Modifier.weight(1f), horizontalArrangement = Arrangement.SpaceAround) {
                    for (p in 1 until game.players) {
                        val tag = when {
                            game.taking && game.defender == p -> "берёт"
                            game.whoActs() == p -> if (p == game.defender) "отбивается" else "ходит"
                            else -> ""
                        }
                        OpponentSeat(name(p), game.hands[p].size, tag, game.hands[p].isEmpty() && game.deckLeft == 0)
                    }
                }
                DeckPile(game.deckLeft, game.trumpCard)
            }
            // Table: attack and cover in pairs, the cover shifted and turned so both stay readable.
            Box(Modifier.weight(1f).fillMaxWidth()) {
                if (game.table.isEmpty() && leaving == null) Text("стол пуст", Modifier.align(Alignment.Center), color = Night.Disabled, fontSize = 13.sp)
                Column(Modifier.fillMaxSize().padding(horizontal = 26.dp, vertical = 10.dp), verticalArrangement = Arrangement.spacedBy(18.dp, Alignment.CenterVertically)) {
                    game.table.chunked(3).forEach { row ->
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp, Alignment.CenterHorizontally)) {
                            row.forEach { t ->
                                key(t.attack) {
                                    Box(Modifier.size(64.dp, 90.dp)) {
                                        PlayingCard(t.attack, false, false, Modifier.width(50.dp).dealIn(seat(t.from)))
                                        t.cover?.let { c -> key(c) { PlayingCard(c, false, false, Modifier.offset(13.dp, 16.dp).width(50.dp).graphicsLayer { rotationZ = 9f }.dealIn(seat(game.defender))) } }
                                    }
                                }
                            }
                        }
                    }
                }
                leaving?.let { l ->
                    key(l) { LeavingCards(l.cards, l.to?.let { seat(it) } ?: DpOffset(screenW * 0.7f, (-30).dp)) { if (leaving === l) leaving = null } }
                }
                if (game.discarded > 0) Text("бито: ${game.discarded}", Modifier.align(Alignment.BottomStart).padding(start = 18.dp), color = Night.Quiet, fontSize = 11.5.sp)
            }
            Column(Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, bottom = 14.dp), horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(6.dp)) {
                key(st) { Text(st, Modifier.enter(ms = 300), fontSize = 19.sp, fontWeight = FontWeight.SemiBold, textAlign = TextAlign.Center) }
                if (sub.isNotEmpty()) Text(sub, color = Night.Muted, fontSize = 13.sp, textAlign = TextAlign.Center)
                if (message.isNotEmpty()) Text(message, color = Night.Rose, fontSize = 13.sp, textAlign = TextAlign.Center)
                // Shared table: while the device holds the hand, nobody at the table sees it here. Without a device it is the only way to play.
                if (deviceHoldsCards) Text("Ваши карты на компаньоне: ${hand.size}", color = Night.Quiet, fontSize = 12.5.sp)
                else {
                    BoxWithConstraints(Modifier.fillMaxWidth().height(112.dp)) {
                        version.hashCode() // the hand is changed in place too
                        val cardW = 58.dp
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
                    val mine = game.whoActs() == 0
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        PillButton("На стол", primary = true, enabled = mine) { play() }
                        if (game.transferMode) PillButton("Перевод", enabled = mine && game.defender == 0) { transfer() }
                        PillButton(passLabel(), enabled = mine) { passOrTake() }
                    }
                }
            }
        }
        if (game.finished) {
            val fool = game.durak
            Overlay(when (fool) { null -> "Ничья"; 0 -> "Вы — дурак"; else -> "Вы не дурак!" },
                when (fool) { null -> "Все вышли одновременно"; 0 -> "Карты остались только у вас"; else -> "Остался с картами: ${name(fool)}" }, actions = {
                    PillButton("К играм", onClick = toList)
                    PillButton("Ещё партия", primary = true, onClick = onAgain)
                })
        }
    }
}
