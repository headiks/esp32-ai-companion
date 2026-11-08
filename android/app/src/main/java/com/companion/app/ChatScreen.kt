package com.companion.app

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.speech.tts.TextToSpeech
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.animateColorAsState
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
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.ArrowUpward
import androidx.compose.material.icons.rounded.Mic
import androidx.compose.material.icons.rounded.Pause
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import kotlinx.coroutines.delay
import kotlinx.coroutines.withTimeoutOrNull
import java.util.Locale
import java.util.concurrent.atomic.AtomicLong
import kotlin.math.abs
import kotlin.math.sin
import kotlin.random.Random

private val ids = AtomicLong()

/** One chat line. [voice] is the length in seconds of a voice message, its words (if heard) are [text]. */
data class ChatMsg(
    val me: Boolean, val text: String, val time: String = clockNow(), val emotion: String? = null,
    val voice: Float? = null, val bars: List<Int> = emptyList(),
    val id: Long = ids.incrementAndGet(), val created: Long = System.currentTimeMillis(),
)

private val QUICK = listOf("Как дела?", "Хочешь поиграть?", "Расскажи что-нибудь", "Спокойной ночи")

fun fmtDur(sec: Float) = "%d:%02d".format(sec.toInt() / 60, sec.toInt() % 60)

/** 26 bars of a voice message from the levels heard while recording. */
fun mkBars(levels: List<Float>): List<Int> = List(26) { i ->
    if (levels.isEmpty()) (5 + abs(sin(i * 1.7 + Random.nextDouble() * 1.5)) * 17).toInt()
    else (4 + levels[i * levels.size / 26] * 20).toInt()
}

@Composable
fun ChatTab(c: BleClient, s: PetState, mood: String, open: Boolean, onOpen: (Boolean) -> Unit) {
    val chat by c.chat.collectAsState()
    val waiting by c.waitingReply.collectAsState()
    val unread by c.chatUnread.collectAsState()
    DisposableEffect(open) {
        c.chatOpen = open
        if (open) c.chatUnread.value = 0
        onDispose { c.chatOpen = false }
    }
    // An answer can get lost (no Wi-Fi, a busy gateway): the dots must not run forever.
    LaunchedEffect(waiting) { if (waiting) { delay(30_000); c.waitingReply.value = false } }
    if (!open) ChatList(s, chat, waiting, unread) { onOpen(true) }
    else {
        BackHandler { onOpen(false) }
        Dialog(c, s, mood, chat, waiting) { onOpen(false) }
    }
}

@Composable
private fun ChatList(s: PetState, chat: List<ChatMsg>, waiting: Boolean, unread: Int, onOpen: () -> Unit) {
    Column(Modifier.fillMaxSize().padding(start = 12.dp, end = 12.dp, top = 4.dp, bottom = 12.dp).enter()) {
        Text("Чаты", Modifier.padding(start = 8.dp, end = 8.dp, top = 2.dp, bottom = 12.dp), fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
        val last = chat.lastOrNull()
        val lastText = when {
            waiting -> "печатает…"
            last == null -> "Скажите питомцу что-нибудь"
            else -> (if (last.me) "Вы: " else "") + (if (last.voice != null) "Голосовое сообщение" else last.text)
        }
        Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).clickable(onClick = onOpen).padding(horizontal = 8.dp, vertical = 10.dp).enter(ms = 350),
            verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Box {
                FaceBadge(s.emotion, 52.dp, 52.dp, 36.dp)
                Box(Modifier.align(Alignment.BottomEnd).padding(1.dp).size(12.dp).clip(CircleShape).background(Night.Bg).padding(2.5.dp).clip(CircleShape).background(Night.Online))
            }
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(3.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Компаньон", Modifier.weight(1f), fontSize = 15.5.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
                    Text(last?.time ?: "", color = Night.Quiet, fontSize = 11.5.sp)
                }
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(lastText, Modifier.weight(1f), color = when { waiting -> Night.Accent; unread > 0 -> Night.Chip; else -> Night.Muted },
                        fontSize = 13.5.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    if (unread > 0) Box(Modifier.heightIn(min = 20.dp).widthIn(min = 20.dp).clip(RoundedCornerShape(10.dp)).background(Night.Accent).padding(horizontal = 6.dp),
                        contentAlignment = Alignment.Center) { Text("$unread", color = Night.OnAccent, fontSize = 11.sp, fontWeight = FontWeight.Bold) }
                }
            }
        }
    }
}

private class Rec(val start: Long) {
    var cancel by mutableStateOf(false)
    val levels = mutableStateListOf<Float>()
    var heard = ""
    var level = -1f // from the recognizer; below zero it is not listening yet
}

@Composable
private fun Dialog(c: BleClient, s: PetState, mood: String, chat: List<ChatMsg>, waiting: Boolean, onBack: () -> Unit) {
    val ctx = LocalContext.current
    var input by rememberSaveable { mutableStateOf("") }
    var flash by remember { mutableStateOf<String?>(null) }
    var flashId by remember { mutableIntStateOf(0) }
    var rec by remember { mutableStateOf<Rec?>(null) }
    var recT by remember { mutableFloatStateOf(0f) }
    var playing by remember { mutableStateOf<Long?>(null) }
    var playPos by remember { mutableFloatStateOf(0f) }
    val shown = remember { mutableStateMapOf<Long, Boolean>() }
    val list = rememberLazyListState()
    val connected = c.link.collectAsState().value == LinkState.Authenticated
    fun showFlash(t: String) { flash = t; flashId++ }
    LaunchedEffect(flashId) { if (flash != null) { delay(1600); flash = null } }

    fun send(text: String = input) {
        val t = text.trim()
        if (t.isEmpty() || waiting) return
        c.say(ChatMsg(me = true, text = t), t)
        input = ""
    }

    // Speech: Android's recognizer hears the words, its loudness drives the live wave. Text-to-speech plays a message back.
    val recognizer = remember { if (SpeechRecognizer.isRecognitionAvailable(ctx)) SpeechRecognizer.createSpeechRecognizer(ctx) else null }
    val tts = remember { TextToSpeech(ctx) { } }
    DisposableEffect(Unit) { onDispose { recognizer?.destroy(); tts.stop(); tts.shutdown() } }
    val mic = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { ok -> if (ok) showFlash("Голосовое: удерживайте, чтобы записать") }

    fun startRec() {
        if (ContextCompat.checkSelfPermission(ctx, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            mic.launch(Manifest.permission.RECORD_AUDIO); return
        }
        tts.stop(); playing = null
        val r = Rec(System.currentTimeMillis())
        rec = r; recT = 0f; flash = null
        recognizer?.setRecognitionListener(object : RecognitionListener {
            override fun onRmsChanged(rms: Float) { r.level = ((rms + 2f) / 12f).coerceIn(0.06f, 1f) }
            override fun onPartialResults(b: Bundle) { b.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)?.firstOrNull()?.let { r.heard = it } }
            override fun onResults(b: Bundle) { b.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)?.firstOrNull()?.let { r.heard = it } }
            override fun onReadyForSpeech(p: Bundle?) {}
            override fun onBeginningOfSpeech() {}
            override fun onBufferReceived(b: ByteArray?) {}
            override fun onEndOfSpeech() {}
            override fun onError(e: Int) {}
            override fun onEvent(t: Int, p: Bundle?) {}
        })
        recognizer?.startListening(Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
            .putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            .putExtra(RecognizerIntent.EXTRA_LANGUAGE, "ru-RU").putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true))
    }
    var finishing by remember { mutableStateOf<Pair<Rec, Boolean>?>(null) }
    fun stopRec(sendIt: Boolean) {
        val r = rec ?: return
        rec = null
        recognizer?.stopListening()
        finishing = r to sendIt
    }
    LaunchedEffect(rec) {
        val r = rec ?: return@LaunchedEffect
        while (rec === r) {
            delay(100)
            r.levels += if (r.level >= 0) r.level else 0.2f + Random.nextFloat() * 0.6f
            recT = (System.currentTimeMillis() - r.start) / 1000f
            if (recT >= 60f) stopRec(true)
        }
    }
    LaunchedEffect(finishing) {
        val (r, sendIt) = finishing ?: return@LaunchedEffect
        val dur = (System.currentTimeMillis() - r.start) / 1000f
        if (sendIt && dur >= 0.7f) {
            delay(600) // the recognizer's final words arrive a moment after the mic stops
            val words = r.heard.trim().replaceFirstChar { it.uppercase() }
            c.say(ChatMsg(me = true, text = words, voice = dur, bars = mkBars(r.levels)), words)
            if (words.isEmpty()) showFlash("Слов не разобрать — скажите ещё раз")
        } else if (sendIt) showFlash("Слишком коротко — удерживайте дольше")
        finishing = null
    }
    fun play(m: ChatMsg) {
        if (playing == m.id) { tts.stop(); playing = null; return }
        tts.stop()
        playing = m.id; playPos = 0f
        if (m.text.isNotEmpty()) { tts.setLanguage(Locale("ru", "RU")); tts.speak(m.text, TextToSpeech.QUEUE_FLUSH, null, "m${m.id}") }
    }
    LaunchedEffect(playing) {
        val id = playing ?: return@LaunchedEffect
        val m = chat.firstOrNull { it.id == id } ?: return@LaunchedEffect
        val t0 = System.currentTimeMillis()
        while (playing == id) {
            delay(80)
            val pos = (System.currentTimeMillis() - t0) / (1000f * (m.voice ?: 1f))
            if (pos >= 1f) { if (tts.isSpeaking && pos < 3f) playPos = .99f else { playing = null; playPos = 0f } } else playPos = pos
        }
    }
    LaunchedEffect(chat.size, waiting) { list.animateScrollToItem(chat.size + if (waiting) 1 else 0) } // the note is item 0

    Column(Modifier.fillMaxSize().slideIn()) {
        Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, bottom = 8.dp), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            RoundIconButton(Icons.AutoMirrored.Rounded.ArrowBack, "Назад", 40.dp, onClick = onBack)
            FaceBadge(s.emotion, 38.dp, 38.dp, 27.dp)
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(1.dp)) {
                Text("Компаньон", fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
                Text(if (connected) "рядом · $mood" else "нет связи", color = if (connected) Night.Online else Night.Muted, fontSize = 12.sp)
            }
        }
        Box(Modifier.fillMaxWidth().height(1.dp).background(Night.Bubble))
        Box(Modifier.weight(1f)) {
            LazyColumn(Modifier.fillMaxSize(), list, PaddingValues(start = 16.dp, end = 16.dp, top = 10.dp, bottom = 12.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                item {
                    Text("Ответы появляются и на экране компаньона. Удерживайте микрофон справа, чтобы сказать голосом — питомец услышит.",
                        Modifier.fillMaxWidth().padding(horizontal = 10.dp, vertical = 4.dp), color = Night.Quiet, fontSize = 11.5.sp, textAlign = TextAlign.Center, lineHeight = 16.sp)
                }
                items(chat, key = { it.id }) { m ->
                    MessageItem(m, playing == m.id, if (playing == m.id) playPos else 0f, shown[m.id] == true, { play(m) }) { shown[m.id] = shown[m.id] != true }
                }
                if (waiting) item(key = "typing") { TypingDots() }
            }
            flash?.let { f ->
                key(flashId) {
                    Text(f, Modifier.align(Alignment.BottomEnd).padding(end = 16.dp, bottom = 4.dp).pop()
                        .clip(RoundedCornerShape(12.dp)).background(Night.Hover).padding(horizontal = 12.dp, vertical = 7.dp), color = Night.Chip, fontSize = 12.sp)
                }
            }
        }
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(start = 16.dp, end = 16.dp, top = 4.dp, bottom = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            QUICK.forEach { q ->
                Box(Modifier.height(34.dp).clip(RoundedCornerShape(17.dp)).border(1.dp, Night.Line, RoundedCornerShape(17.dp)).clickable { send(q) }
                    .padding(horizontal = 14.dp), contentAlignment = Alignment.Center) { Text(q, color = Night.Chip, fontSize = 13.sp, maxLines = 1) }
            }
        }
        Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, top = 4.dp, bottom = 8.dp), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            val r = rec
            if (r == null) NightField(input, { input = it }, "Сказать питомцу…", Modifier.weight(1f),
                KeyboardOptions(imeAction = ImeAction.Send), KeyboardActions(onSend = { send() }))
            else RecordingBar(r, recT, Modifier.weight(1f))
            if (input.isNotBlank() && r == null) {
                val src = remember { androidx.compose.foundation.interaction.MutableInteractionSource() }
                Box(Modifier.size(50.dp).pressScale(src).clip(CircleShape).background(Night.Accent).clickable(src, null) { send() }
                    .semantics { contentDescription = "Отправить" }, contentAlignment = Alignment.Center) { MIcon(Icons.Rounded.ArrowUpward, 22.dp, Night.OnAccent) }
            } else MicButton(r != null, ::startRec, { cancel -> rec?.cancel = cancel }, ::stopRec) { showFlash("Голосовое: удерживайте, чтобы записать") }
        }
    }
}

/** Tap: a hint. Hold longer than 220 ms: record. Slide 70 dp left: cancel. Release: send. */
@Composable
private fun MicButton(recording: Boolean, start: () -> Unit, slide: (Boolean) -> Unit, stop: (Boolean) -> Unit, tap: () -> Unit) {
    val scale by animateFloatAsState(if (recording) 1.3f else 1f, spring(dampingRatio = 0.5f, stiffness = 500f), label = "mic")
    val bg by animateColorAsState(if (recording) Night.Accent else Night.Bubble, tween(300), label = "micBg")
    val onStart by rememberUpdatedState(start)
    val onSlide by rememberUpdatedState(slide)
    val onStop by rememberUpdatedState(stop)
    val onTap by rememberUpdatedState(tap)
    Box(Modifier.size(50.dp).graphicsLayer { scaleX = scale; scaleY = scale }.clip(CircleShape).background(bg)
        .semantics { contentDescription = "Голосовое" }
        .pointerInput(Unit) {
            awaitEachGesture {
                val down = awaitFirstDown()
                val up = withTimeoutOrNull(220) { waitForUpOrCancellation() }
                if (up != null) { onTap(); return@awaitEachGesture }
                onStart()
                var cancel = false
                while (true) {
                    val e = awaitPointerEvent()
                    val ch = e.changes.firstOrNull { it.id == down.id } ?: break
                    val now = ch.position.x - down.position.x < -70.dp.toPx()
                    if (now != cancel) { cancel = now; onSlide(now) }
                    if (!ch.pressed) break
                    ch.consume()
                }
                onStop(!cancel)
            }
        }, contentAlignment = Alignment.Center) { MIcon(Icons.Rounded.Mic, 24.dp, if (recording) Night.OnAccent else Night.Accent) }
}

@Composable
private fun RecordingBar(r: Rec, t: Float, modifier: Modifier) {
    val dot = pulse(1000)
    Row(modifier.height(50.dp).enter(dy = 14.dp, ms = 200).graphicsLayer { alpha = if (r.cancel) .45f else 1f }
        .clip(RoundedCornerShape(25.dp)).background(Night.Field).border(1.dp, Night.Line, RoundedCornerShape(25.dp)).padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Box(Modifier.size(10.dp).graphicsLayer { alpha = dot }.clip(CircleShape).background(Night.Rose))
        Text(fmtDur(t), Modifier.width(34.dp), fontSize = 14.sp)
        Row(Modifier.weight(1f).height(26.dp), horizontalArrangement = Arrangement.spacedBy(2.dp, Alignment.End), verticalAlignment = Alignment.CenterVertically) {
            r.levels.takeLast(24).forEach { l -> Box(Modifier.width(3.dp).height((4 + l * 20).dp).clip(RoundedCornerShape(2.dp)).background(Night.Accent)) }
        }
        Text(if (r.cancel) "отпустите — отмена" else "‹ влево — отмена", color = if (r.cancel) Night.Rose else Night.Muted, fontSize = 12.sp, maxLines = 1)
    }
}

@Composable
private fun MessageItem(m: ChatMsg, playing: Boolean, pos: Float, showText: Boolean, onPlay: () -> Unit, onToggle: () -> Unit) {
    val fresh = remember { System.currentTimeMillis() - m.created < 1500 }
    Column(Modifier.fillMaxWidth().then(if (fresh) Modifier.pop(300, if (m.me) 1f else 0f, 1f) else Modifier),
        horizontalAlignment = if (m.me) Alignment.End else Alignment.Start, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        val shape = if (m.me) RoundedCornerShape(20.dp, 20.dp, 6.dp, 20.dp) else RoundedCornerShape(20.dp, 20.dp, 20.dp, 6.dp)
        val bg = if (m.me) Night.Accent else Night.Bubble
        val fg = if (m.me) Night.OnAccent else Night.Text
        if (m.voice == null) {
            Text(m.text, Modifier.fillMaxWidth(0.8f).wrapContentWidth(if (m.me) Alignment.End else Alignment.Start).clip(shape).background(bg)
                .padding(horizontal = 14.dp, vertical = 10.dp), color = fg, fontSize = 14.5.sp, lineHeight = 20.5.sp)
        } else Column(Modifier.widthIn(max = 320.dp).clip(shape).background(bg).padding(start = 8.dp, end = 12.dp, top = 8.dp, bottom = 8.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                val src = remember { androidx.compose.foundation.interaction.MutableInteractionSource() }
                Box(Modifier.size(40.dp).pressScale(src).clip(CircleShape).background(if (m.me) Night.OnAccent else Night.Accent).clickable(src, null, onClick = onPlay)
                    .semantics { contentDescription = if (playing) "Пауза" else "Слушать" }, contentAlignment = Alignment.Center) {
                    MIcon(if (playing) Icons.Rounded.Pause else Icons.Rounded.PlayArrow, 24.dp, if (m.me) Night.Accent else Night.OnAccent)
                }
                Row(Modifier.height(26.dp), horizontalArrangement = Arrangement.spacedBy(2.dp), verticalAlignment = Alignment.CenterVertically) {
                    m.bars.forEachIndexed { i, h ->
                        val played = i.toFloat() / m.bars.size < pos
                        Box(Modifier.width(3.dp).height(h.dp).clip(RoundedCornerShape(2.dp)).background(
                            if (played) (if (m.me) Night.OnAccent else Night.Accent) else (if (m.me) Night.OnAccent.copy(alpha = .3f) else Color(0xFF4A4A55))))
                    }
                }
                Text(fmtDur(if (playing) m.voice * pos else m.voice), Modifier.widthIn(min = 30.dp).graphicsLayer { alpha = .8f }, color = fg, fontSize = 12.sp)
                if (m.text.isNotEmpty()) Box(Modifier.size(28.dp).clip(RoundedCornerShape(9.dp))
                    .background(if (m.me) Night.OnAccent.copy(alpha = .15f) else Color(0xFF2A2A32)).clickable(onClick = onToggle), contentAlignment = Alignment.Center) {
                    Text("Aa", color = fg, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                }
            }
            if (showText) Text(m.text, Modifier.padding(start = 6.dp, end = 4.dp, bottom = 2.dp).enter(ms = 250), color = fg, fontSize = 13.5.sp, lineHeight = 19.sp)
        }
        val tag = if (m.me) m.time else Protocol.EMOTION_TITLES[m.emotion] ?: m.time
        Text(tag, Modifier.padding(horizontal = 6.dp), color = Night.Quiet, fontSize = 11.sp)
    }
}

@Composable
private fun TypingDots() {
    val t = rememberInfiniteTransition(label = "dots")
    Row(Modifier.pop().clip(RoundedCornerShape(18.dp, 18.dp, 18.dp, 6.dp)).background(Night.Bubble).padding(horizontal = 16.dp, vertical = 14.dp),
        horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        repeat(3) { i ->
            val v by t.animateFloat(0f, 0f, infiniteRepeatable(keyframes {
                durationMillis = 1100
                0f at 0; 1f at 440; 0f at 880
            }, initialStartOffset = androidx.compose.animation.core.StartOffset(i * 150)), label = "dot$i")
            Box(Modifier.size(7.dp).graphicsLayer { alpha = .25f + .75f * v; translationY = -3f * v * density }.clip(CircleShape).background(Night.Accent))
        }
    }
}
