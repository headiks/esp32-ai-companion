package com.companion.app

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat

private val Accent = Color(0xFFB6BED6)
private val Soft = Color(0xFFDCD6C4)
private val Bg = Color(0xFF0E0E12)
private val Card = Color(0xFF1C1C22)

private val ColorsScheme = darkColorScheme(
    primary = Accent, onPrimary = Color.Black, secondary = Soft, background = Bg, surface = Card,
    onBackground = Color.White, onSurface = Color.White, surfaceVariant = Card,
)

class MainActivity : ComponentActivity() {
    private lateinit var client: BleClient
    private val permissions = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
        if (result.values.all { it }) client.savedAddress?.let { client.reconnectSaved() }
    }

    private fun needed(): Array<String> =
        if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        client = BleClient.get(this)
        val missing = needed().filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) permissions.launch(missing.toTypedArray())
        else client.savedAddress?.let { client.reconnectSaved() }
        setContent {
            MaterialTheme(colorScheme = ColorsScheme) {
                Surface(Modifier.fillMaxSize(), color = Bg) { App(client) { startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) } }
            }
        }
    }
}

@Composable
private fun App(client: BleClient, openNotificationAccess: () -> Unit) {
    val link by client.link.collectAsState()
    val state by client.state.collectAsState()
    val error by client.lastError.collectAsState()
    var tab by remember { mutableIntStateOf(0) }
    val tabs = listOf("Связь", "Питомец", "Игры", "Эмоции", "Настройки", "Журнал")
    Column(Modifier.fillMaxSize().systemBarsPadding()) {
        Header(link, state)
        error?.let {
            Text(it, color = Accent, modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp).clickable { client.lastError.value = null })
        }
        ScrollableTabRow(selectedTabIndex = tab, containerColor = Bg, edgePadding = 8.dp) {
            tabs.forEachIndexed { i, t -> Tab(selected = tab == i, onClick = { tab = i }, text = { Text(t) }) }
        }
        Box(Modifier.weight(1f).padding(12.dp)) {
            when (tab) {
                0 -> ConnectTab(client, link)
                1 -> PetTab(client, state)
                2 -> GamesTab(client)
                3 -> EmotionTab(client)
                4 -> SettingsTab(client, state, openNotificationAccess)
                else -> LogTab(client)
            }
        }
    }
}

// Same character as on the device: a slanted lid and a pupil pressed against it.
@Composable
private fun EyesPreview(modifier: Modifier = Modifier) {
    Canvas(modifier) {
        val w = size.width
        val h = size.height
        val eyeW = w * 0.42f
        for (side in listOf(-1f, 1f)) {
            val cx = w / 2 + side * w * 0.26f
            val cy = h * 0.32f
            val path = Path().apply {
                addArc(Rect(cx - eyeW / 2, cy - h * 0.6f, cx + eyeW / 2, cy + h * 0.6f), 0f, 180f)
                close()
            }
            drawPath(path, Color.White)
            drawCircle(Color.Black, h * 0.17f, Offset(cx - side * eyeW * 0.08f, cy + h * 0.16f))
        }
    }
}

@Composable
private fun Header(link: LinkState, s: PetState) {
    Row(Modifier.fillMaxWidth().padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(96.dp, 52.dp).clip(RoundedCornerShape(26.dp)).background(Color.Black).padding(10.dp)) { EyesPreview(Modifier.fillMaxSize()) }
        Spacer(Modifier.width(14.dp))
        Column {
            Text(s.emotion, fontSize = 20.sp, fontWeight = FontWeight.Bold, color = Soft)
            val linkText = when (link) {
                LinkState.Authenticated -> "подключено · ${s.status}"
                LinkState.Connected -> "подключено, ждём PIN"
                LinkState.Connecting -> "подключение…"
                LinkState.Scanning -> "поиск…"
                LinkState.Disconnected -> "нет связи"
            }
            Text(linkText, color = Color.Gray)
        }
        Spacer(Modifier.weight(1f))
        Text("${s.battery}%${if (s.charging) " ⚡" else ""}", color = Color.White)
    }
}

@Composable
private fun ConnectTab(c: BleClient, link: LinkState) {
    val devices by c.devices.collectAsState()
    var pin by remember { mutableStateOf(c.pin) }
    Column {
        OutlinedTextField(pin, { pin = it; c.pin = it }, label = { Text("PIN с экрана «Об устройстве»") }, singleLine = true, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(10.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { c.startScan() }) { Text("Найти") }
            OutlinedButton(onClick = { c.reconnectSaved() }, enabled = c.savedAddress != null) { Text("Переподключить") }
            OutlinedButton(onClick = { c.disconnect() }, enabled = link != LinkState.Disconnected) { Text("Отключить") }
        }
        Spacer(Modifier.height(10.dp))
        Text("Тест с эмулятором на ПК: включите в нём сервер на вкладке «Устройство»", color = Color.Gray, fontSize = 12.sp)
        var host by remember { mutableStateOf(c.savedTcpHost) }
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(host, { host = it }, label = { Text("Адрес ПК") }, singleLine = true, modifier = Modifier.weight(1f))
            Spacer(Modifier.width(8.dp))
            OutlinedButton(onClick = { c.connectTcp(host.trim(), 7777) }, enabled = host.isNotBlank()) { Text("Эмулятор") }
        }
        Spacer(Modifier.height(10.dp))
        LazyColumn(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            items(devices) { d ->
                Card(Modifier.fillMaxWidth().clickable { c.connect(d.address) }) {
                    Column(Modifier.padding(12.dp)) {
                        Text(d.name, fontWeight = FontWeight.Bold)
                        Text(d.address, color = Color.Gray, fontSize = 12.sp)
                    }
                }
            }
        }
    }
}

private val STAT_TITLES = mapOf(
    "happiness" to "Счастье", "energy" to "Энергия", "curiosity" to "Любопытство", "attention" to "Внимание", "affection" to "Привязанность",
    "stress" to "Стресс", "sleepiness" to "Сонливость", "hunger" to "Голод", "fun" to "Веселье", "health" to "Здоровье",
)

@Composable
private fun PetTab(c: BleClient, s: PetState) {
    Column(Modifier.verticalScroll(rememberScrollState())) {
        val actions = listOf("Покормить" to "feed", "Погладить" to "pet", "Поиграть" to "play", "Уложить" to "sleep", "Разбудить" to "wake")
        actions.chunked(3).forEach { row ->
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(bottom = 6.dp)) {
                row.forEach { (t, v) -> Button(onClick = { c.send(Protocol.command("action", "value" to v)) }, modifier = Modifier.weight(1f), contentPadding = PaddingValues(4.dp)) { Text(t, fontSize = 13.sp) } }
                repeat(3 - row.size) { Spacer(Modifier.weight(1f)) }
            }
        }
        PetState.STAT_NAMES.forEach { name ->
            val value = s.stats[name] ?: 0
            var dragging by remember { mutableStateOf<Float?>(null) }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(STAT_TITLES[name] ?: name, Modifier.width(118.dp), fontSize = 13.sp)
                Slider(
                    value = dragging ?: value.toFloat(), valueRange = 0f..100f, modifier = Modifier.weight(1f),
                    onValueChange = { dragging = it },
                    onValueChangeFinished = { dragging?.let { v -> c.send(Protocol.command("set_stat", "name" to name, "value" to v.toInt())) }; dragging = null },
                )
                Text("$value", Modifier.width(32.dp), color = Soft, fontSize = 13.sp)
            }
        }
        val games = listOf("Лабиринт" to "maze", "Орбита" to "orbit", "Монетка" to "coin", "Кубики" to "dice", "Шар" to "magic_ball", "Дуэль" to "duel",
            "Шейкер" to "shaker", "Сейф" to "safe", "Баланс" to "balance", "Зарядка" to "workout", "Кротики" to "mole", "Змейка" to "snake",
            "Память" to "simon", "Крестики" to "tictactoe", "Реакция" to "reaction", "Угадай" to "guess", "Светлячок" to "pet_game", "Часы" to "digital_clock", "Погода" to "weather", "Лицо" to "face")
        Text("На экран устройства", color = Color.Gray, modifier = Modifier.padding(top = 8.dp, bottom = 4.dp))
        games.chunked(3).forEach { row ->
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(bottom = 6.dp)) {
                row.forEach { (t, v) -> OutlinedButton(onClick = { c.send(Protocol.command("screen", "value" to v)) }, modifier = Modifier.weight(1f), contentPadding = PaddingValues(4.dp)) { Text(t, fontSize = 13.sp) } }
            }
        }
    }
}

@Composable
private fun EmotionTab(c: BleClient) {
    var text by remember { mutableStateOf("") }
    Column {
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(text, { text = it }, label = { Text("Сказать питомцу") }, modifier = Modifier.weight(1f), singleLine = true)
            Spacer(Modifier.width(8.dp))
            Button(onClick = { c.send(Protocol.command("chat", "text" to text)); text = "" }) { Text("→") }
        }
        Spacer(Modifier.height(10.dp))
        LazyVerticalGrid(GridCells.Fixed(2), verticalArrangement = Arrangement.spacedBy(6.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            items(Protocol.EMOTIONS) { e ->
                FilledTonalButton(onClick = { c.send(Protocol.command("emotion", "value" to e)) }) { Text(e, fontSize = 13.sp) }
            }
        }
    }
}

@Composable
private fun SettingsTab(c: BleClient, s: PetState, openNotificationAccess: () -> Unit) {
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val prefs = remember { ctx.getSharedPreferences("companion", Context.MODE_PRIVATE) }
    var forward by remember { mutableStateOf(prefs.getBoolean("forward", false)) }
    var title by remember { mutableStateOf("Привет") }
    var body by remember { mutableStateOf("Сообщение с телефона") }
    Column(Modifier.verticalScroll(rememberScrollState())) {
        SliderRow("Яркость", s.brightness, 5f..100f) { c.send(Protocol.command("setting", "name" to "brightness", "value" to it)) }
        SliderRow("Громкость", s.volume, 0f..100f) { c.send(Protocol.command("setting", "name" to "volume", "value" to it)) }
        SwitchRow("Звук", s.sound) { c.send(Protocol.command("setting", "name" to "sound", "value" to it)) }
        SwitchRow("Микрофоны", s.mic) { c.send(Protocol.command("setting", "name" to "mic", "value" to it)) }
        SwitchRow("Пересылать уведомления телефона", forward) {
            forward = it
            prefs.edit().putBoolean("forward", it).apply()
            if (it) openNotificationAccess()
        }
        Text("Характер", color = Color.Gray, modifier = Modifier.padding(top = 8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            listOf("Игривый" to "playful", "Спокойный" to "calm", "Ворчун" to "grumpy", "Застенч." to "shy").forEach { (t, v) ->
                OutlinedButton(onClick = { c.send(Protocol.command("setting", "name" to "personality", "value" to v)) }, modifier = Modifier.weight(1f), contentPadding = PaddingValues(2.dp)) { Text(t, fontSize = 11.sp) }
            }
        }
        Text("Wi-Fi и координаты для погоды", color = Color.Gray, modifier = Modifier.padding(top = 12.dp))
        var ssid by remember { mutableStateOf("") }
        var pass by remember { mutableStateOf("") }
        var lat by remember { mutableStateOf("") }
        var lon by remember { mutableStateOf("") }
        OutlinedTextField(ssid, { ssid = it }, label = { Text("Сеть") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
        OutlinedTextField(pass, { pass = it }, label = { Text("Пароль") }, modifier = Modifier.fillMaxWidth(), singleLine = true,
            visualTransformation = androidx.compose.ui.text.input.PasswordVisualTransformation())
        Button(onClick = { c.send(Protocol.command("wifi_config", "ssid" to ssid, "password" to pass)) }, enabled = ssid.isNotEmpty(), modifier = Modifier.padding(top = 6.dp)) { Text("Сохранить Wi-Fi") }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            OutlinedTextField(lat, { lat = it }, label = { Text("Широта") }, modifier = Modifier.weight(1f), singleLine = true)
            OutlinedTextField(lon, { lon = it }, label = { Text("Долгота") }, modifier = Modifier.weight(1f), singleLine = true)
        }
        Button(onClick = {
            val a = lat.replace(',', '.').toDoubleOrNull()
            val b = lon.replace(',', '.').toDoubleOrNull()
            if (a != null && b != null) {
                c.send(Protocol.command("setting", "name" to "latitude", "value" to a))
                c.send(Protocol.command("setting", "name" to "longitude", "value" to b))
            } else c.lastError.value = "Координаты должны быть числами"
        }, modifier = Modifier.padding(top = 6.dp)) { Text("Сохранить координаты") }
        Text("Тестовое уведомление", color = Color.Gray, modifier = Modifier.padding(top = 12.dp))
        OutlinedTextField(title, { title = it }, label = { Text("Заголовок") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
        OutlinedTextField(body, { body = it }, label = { Text("Текст") }, modifier = Modifier.fillMaxWidth())
        Button(onClick = { c.send(Protocol.command("notify", "title" to title, "text" to body, "icon" to "message")) }, modifier = Modifier.padding(top = 6.dp)) { Text("Отправить") }
    }
}

@Composable
private fun SliderRow(title: String, value: Int, range: ClosedFloatingPointRange<Float>, onDone: (Int) -> Unit) {
    var drag by remember { mutableStateOf<Float?>(null) }
    Text("$title: ${(drag ?: value.toFloat()).toInt()}", fontSize = 13.sp)
    Slider(drag ?: value.toFloat(), { drag = it }, valueRange = range, onValueChangeFinished = { drag?.let { onDone(it.toInt()) }; drag = null })
}

@Composable
private fun SwitchRow(title: String, value: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(title, Modifier.weight(1f))
        Switch(value, onChange)
    }
}

@Composable
private fun LogTab(c: BleClient) {
    val log by c.log.collectAsState()
    LazyColumn { items(log.reversed()) { Text(it, fontSize = 11.sp, color = Color.LightGray, modifier = Modifier.padding(vertical = 2.dp)) } }
}
