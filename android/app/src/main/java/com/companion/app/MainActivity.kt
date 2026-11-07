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
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Send
import androidx.compose.material.icons.automirrored.outlined.VolumeUp
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat

private val Accent = Color(0xFFB6BED6)
private val Soft = Color(0xFFDCD6C4)
private val Bg = Color(0xFF0E0E12)
private val Card = Color(0xFF1C1C22)
private val Muted = Color(0xFF8A8A92)
private val Good = Color(0xFFAED0C0)
private val Low = Color(0xFFDAB0B8)

private val ColorsScheme = darkColorScheme(
    primary = Accent, onPrimary = Color.Black, secondary = Soft, background = Bg, surface = Card,
    onBackground = Color.White, onSurface = Color.White, surfaceVariant = Card, surfaceContainer = Card,
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
                App(client) { startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }
            }
        }
    }
}

private enum class Page(val title: String, val icon: ImageVector) {
    Pet("Питомец", Icons.Outlined.Pets),
    Emotions("Эмоции", Icons.Outlined.Mood),
    Games("Игры", Icons.Outlined.SportsEsports),
    Setup("Настройки", Icons.Outlined.Settings),
}

@Composable
private fun App(client: BleClient, openNotificationAccess: () -> Unit) {
    val link by client.link.collectAsState()
    val state by client.state.collectAsState()
    val error by client.lastError.collectAsState()
    var tab by rememberSaveable { mutableStateOf(Page.Pet) }
    val snackbar = remember { SnackbarHostState() }
    LaunchedEffect(error) {
        error?.let { snackbar.showSnackbar(it); client.lastError.value = null }
    }
    Scaffold(
        containerColor = Bg,
        snackbarHost = { SnackbarHost(snackbar) },
        bottomBar = {
            NavigationBar(containerColor = Bg) {
                Page.entries.forEach { t ->
                    NavigationBarItem(selected = tab == t, onClick = { tab = t }, icon = { Icon(t.icon, null) }, label = { Text(t.title) })
                }
            }
        },
    ) { pad ->
        Column(Modifier.padding(pad).fillMaxSize()) {
            Header(link, state)
            Box(Modifier.weight(1f).padding(horizontal = 16.dp)) {
                // Games run on the phone alone too; everything else needs the device.
                when {
                    tab == Page.Games -> GamesTab(client)
                    link != LinkState.Authenticated -> ConnectScreen(client, link)
                    tab == Page.Pet -> PetTab(client, state)
                    tab == Page.Emotions -> EmotionTab(client, state)
                    else -> SettingsTab(client, state, openNotificationAccess)
                }
            }
        }
    }
}

/** The device's own face for this emotion, rendered by the core (tools/gen_emotion_icons.py). */
@Composable
private fun EmotionIcon(emotion: String, modifier: Modifier = Modifier, tint: Color = Color.White) =
    Icon(painterResource(EMOTION_ICONS[emotion] ?: R.drawable.emo_neutral), null, modifier, tint = tint)

@Composable
private fun Header(link: LinkState, s: PetState) {
    val connected = link == LinkState.Authenticated
    Row(Modifier.fillMaxWidth().padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(72.dp, 48.dp).clip(RoundedCornerShape(24.dp)).background(Color.Black).padding(horizontal = 10.dp, vertical = 6.dp)) {
            EmotionIcon(if (connected) s.emotion else "sleepy", Modifier.fillMaxSize())
        }
        Spacer(Modifier.width(14.dp))
        Column(Modifier.weight(1f)) {
            val title = if (connected) Protocol.EMOTION_TITLES[s.emotion]?.replaceFirstChar { it.uppercase() } ?: s.emotion else "Компаньон"
            Text(title, fontSize = 20.sp, fontWeight = FontWeight.Medium)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(6.dp).clip(CircleShape).background(if (connected) Good else if (link == LinkState.Disconnected) Low else Soft))
                Spacer(Modifier.width(6.dp))
                val linkText = when (link) {
                    LinkState.Authenticated -> "подключено · ${Protocol.STATUS_TITLES[s.status] ?: s.status}"
                    LinkState.Connected -> "проверка PIN…"
                    LinkState.Connecting -> "подключение…"
                    LinkState.Scanning -> "поиск…"
                    LinkState.Disconnected -> "нет связи"
                }
                Text(linkText, color = Muted, fontSize = 13.sp)
            }
        }
        if (connected) {
            val battery = when {
                s.charging -> Icons.Outlined.BatteryChargingFull
                s.battery < 20 -> Icons.Outlined.BatteryAlert
                else -> Icons.Outlined.BatteryStd
            }
            Icon(battery, null, Modifier.size(18.dp), tint = Muted)
            Text("${s.battery}%", color = Muted, fontSize = 14.sp)
        }
    }
}

@Composable
private fun ConnectScreen(c: BleClient, link: LinkState) {
    val devices by c.devices.collectAsState()
    var pin by remember { mutableStateOf(c.pin) }
    var host by remember { mutableStateOf(c.savedTcpHost) }
    var pcOpen by rememberSaveable { mutableStateOf(false) }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Spacer(Modifier.height(16.dp))
        Icon(Icons.Outlined.BluetoothSearching, null, Modifier.size(48.dp), tint = Muted)
        Text("Подключите компаньона", fontSize = 20.sp, fontWeight = FontWeight.Medium)
        Spacer(Modifier.height(4.dp))
        OutlinedTextField(pin, { pin = it.trim(); c.pin = pin }, label = { Text("PIN") }, placeholder = { Text("на экране «Об устройстве»") },
            singleLine = true, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword), modifier = Modifier.fillMaxWidth())
        when (link) {
            LinkState.Scanning, LinkState.Connecting, LinkState.Connected -> Row(Modifier.height(52.dp), verticalAlignment = Alignment.CenterVertically) {
                CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                Spacer(Modifier.width(12.dp))
                Text(if (link == LinkState.Scanning) "Ищем…" else "Подключаемся…")
                Spacer(Modifier.width(12.dp))
                TextButton(onClick = { if (link == LinkState.Scanning) c.stopScan() else c.disconnect() }) { Text("Отмена") }
            }
            else -> Button(onClick = { c.startScan() }, Modifier.fillMaxWidth().height(52.dp)) {
                Icon(Icons.Outlined.Search, null)
                Spacer(Modifier.width(8.dp))
                Text("Найти устройство")
            }
        }
        devices.forEach { d ->
            Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(Card).clickable { c.connect(d.address) }.padding(16.dp),
                verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Outlined.Bluetooth, null, tint = Muted)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(d.name)
                    Text(d.address, color = Muted, fontSize = 12.sp)
                }
            }
        }
        if (c.savedAddress != null && link == LinkState.Disconnected) {
            TextButton(onClick = { c.reconnectSaved() }) { Text("Подключиться к прошлому") }
        }
        Spacer(Modifier.height(16.dp))
        // The PC emulator is a developer's tool: folded away so it never competes with the device.
        TextButton(onClick = { pcOpen = !pcOpen }) {
            Text("Эмулятор на ПК", color = Muted)
            Icon(if (pcOpen) Icons.Outlined.KeyboardArrowUp else Icons.Outlined.KeyboardArrowDown, null, tint = Muted)
        }
        if (pcOpen) Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(host, { host = it }, label = { Text("Адрес ПК") }, singleLine = true, modifier = Modifier.weight(1f))
            Spacer(Modifier.width(8.dp))
            Button(onClick = {
                // "адрес" or "адрес:порт"
                val h = host.trim()
                c.connectTcp(h.substringBefore(':'), h.substringAfter(':', "").toIntOrNull() ?: 7777)
            }, enabled = host.isNotBlank()) { Text("Подключить") }
        }
    }
}

private val STAT_TITLES = mapOf(
    "happiness" to "Счастье", "energy" to "Энергия", "curiosity" to "Любопытство", "attention" to "Внимание", "affection" to "Привязанность",
    "stress" to "Стресс", "sleepiness" to "Сонливость", "hunger" to "Голод", "fun" to "Веселье", "health" to "Здоровье",
)

/** What a person reads at a glance: every bar is "full is good", so hunger turns into fullness. */
private data class Need(val stat: String, val title: String, val icon: ImageVector, val inverted: Boolean = false)

private val NEEDS = listOf(
    Need("happiness", "Счастье", Icons.Outlined.SentimentSatisfied), Need("hunger", "Сытость", Icons.Outlined.Restaurant, inverted = true),
    Need("energy", "Энергия", Icons.Outlined.Bolt), Need("fun", "Веселье", Icons.Outlined.Celebration),
    Need("health", "Здоровье", Icons.Outlined.FavoriteBorder),
)

@Composable
private fun PetTab(c: BleClient, s: PetState) {
    val asleep = s.status == "asleep" || s.emotion == "sleeping"
    fun act(v: String) = c.send(Protocol.command("action", "value" to v))
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(20.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
            RoundAction(Icons.Outlined.Restaurant, "Покормить") { act("feed") }
            RoundAction(Icons.Outlined.PanTool, "Погладить") { act("pet") }
            RoundAction(Icons.Outlined.SportsSoccer, "Поиграть") { act("play") }
            if (asleep) RoundAction(Icons.Outlined.WbSunny, "Разбудить") { act("wake") } else RoundAction(Icons.Outlined.Bedtime, "Уложить") { act("sleep") }
        }
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Card).padding(20.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            NEEDS.forEach { n ->
                val raw = s.stats[n.stat] ?: 0
                NeedBar(n.icon, n.title, if (n.inverted) 100 - raw else raw)
            }
        }
        ChatField(c)
    }
}

@Composable
private fun RoundAction(icon: ImageVector, label: String, onClick: () -> Unit) {
    Column(Modifier.clip(RoundedCornerShape(16.dp)).clickable(onClick = onClick).padding(6.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Box(Modifier.size(60.dp).clip(CircleShape).background(Card), contentAlignment = Alignment.Center) { Icon(icon, null, Modifier.size(26.dp)) }
        Spacer(Modifier.height(6.dp))
        Text(label, fontSize = 13.sp, color = Muted)
    }
}

@Composable
private fun NeedBar(icon: ImageVector, title: String, value: Int) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, Modifier.size(18.dp), tint = Muted)
        Spacer(Modifier.width(12.dp))
        Text(title, Modifier.width(84.dp), fontSize = 14.sp)
        Box(Modifier.weight(1f).height(6.dp).clip(RoundedCornerShape(3.dp)).background(Bg)) {
            Box(Modifier.fillMaxWidth(value.coerceIn(0, 100) / 100f).fillMaxHeight().clip(RoundedCornerShape(3.dp)).background(if (value < 30) Low else Soft))
        }
    }
}

@Composable
private fun ChatField(c: BleClient) {
    var text by remember { mutableStateOf("") }
    fun send() {
        if (text.isBlank()) return
        c.send(Protocol.command("chat", "text" to text))
        text = ""
    }
    OutlinedTextField(text, { text = it }, placeholder = { Text("Сказать питомцу…") }, singleLine = true, shape = RoundedCornerShape(28.dp),
        modifier = Modifier.fillMaxWidth(),
        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send), keyboardActions = KeyboardActions(onSend = { send() }),
        trailingIcon = { IconButton(onClick = { send() }, enabled = text.isNotBlank()) { Icon(Icons.AutoMirrored.Outlined.Send, "Отправить") } })
}

@Composable
private fun EmotionTab(c: BleClient, s: PetState) {
    LazyVerticalGrid(GridCells.Fixed(3), verticalArrangement = Arrangement.spacedBy(8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp),
        contentPadding = PaddingValues(bottom = 16.dp)) {
        items(Protocol.EMOTIONS) { e ->
            val current = e == s.emotion
            Column(
                Modifier.clip(RoundedCornerShape(16.dp)).background(Card)
                    .border(1.5.dp, if (current) Accent else Color.Transparent, RoundedCornerShape(16.dp))
                    .clickable { c.send(Protocol.command("emotion", "value" to e)) }.padding(vertical = 12.dp, horizontal = 4.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                EmotionIcon(e, Modifier.size(72.dp, 60.dp), if (current) Accent else Soft)
                Spacer(Modifier.height(6.dp))
                Text(Protocol.EMOTION_TITLES[e] ?: e, fontSize = 12.sp, color = Muted, textAlign = TextAlign.Center, maxLines = 2, lineHeight = 14.sp)
            }
        }
    }
}

private data class Personality(val icon: ImageVector, val title: String, val value: String)

private val PERSONALITIES = listOf(
    Personality(Icons.Outlined.SentimentVerySatisfied, "Игривый", "playful"), Personality(Icons.Outlined.Spa, "Спокойный", "calm"),
    Personality(Icons.Outlined.SentimentVeryDissatisfied, "Ворчун", "grumpy"), Personality(Icons.Outlined.VisibilityOff, "Застенчивый", "shy"),
)

@Composable
private fun SettingsTab(c: BleClient, s: PetState, openNotificationAccess: () -> Unit) {
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val prefs = remember { ctx.getSharedPreferences("companion", Context.MODE_PRIVATE) }
    var forward by remember { mutableStateOf(prefs.getBoolean("forward", false)) }
    fun setting(name: String, value: Any) = c.send(Protocol.command("setting", "name" to name, "value" to value))
    // The personality comes only with get_settings, not with the state: ask on open and after every change.
    fun refresh() = c.send(Protocol.command("get_settings"))
    LaunchedEffect(Unit) { refresh() }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Group {
            SliderRow(Icons.Outlined.LightMode, "Яркость", s.brightness, 5f..100f) { setting("brightness", it) }
            SliderRow(Icons.AutoMirrored.Outlined.VolumeUp, "Громкость", s.volume, 0f..100f) { setting("volume", it) }
            SwitchRow(Icons.Outlined.MusicNote, "Звуки", s.sound) { setting("sound", it) }
            SwitchRow(Icons.Outlined.Mic, "Микрофон", s.mic) { setting("mic", it) }
            SwitchRow(Icons.Outlined.Notifications, "Уведомления телефона", forward) {
                forward = it
                prefs.edit().putBoolean("forward", it).apply()
                if (it) openNotificationAccess()
            }
        }
        Group {
            Text("Характер", color = Muted, fontSize = 13.sp, modifier = Modifier.padding(vertical = 8.dp))
            PERSONALITIES.chunked(2).forEach { row ->
                Row(Modifier.padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    row.forEach { p ->
                        val selected = s.personality == p.value
                        val fg = if (selected) Color.Black else Color.White
                        Row(Modifier.weight(1f).clip(RoundedCornerShape(12.dp)).background(if (selected) Accent else Bg)
                            .clickable { setting("personality", p.value); refresh() }.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                            Icon(p.icon, null, Modifier.size(18.dp), tint = fg)
                            Spacer(Modifier.width(8.dp))
                            Text(p.title, color = fg, fontSize = 14.sp)
                        }
                    }
                }
            }
        }
        Expandable(Icons.Outlined.Wifi, "Wi-Fi") {
            var ssid by remember { mutableStateOf("") }
            var pass by remember { mutableStateOf("") }
            OutlinedTextField(ssid, { ssid = it }, label = { Text("Сеть") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
            OutlinedTextField(pass, { pass = it }, label = { Text("Пароль") }, modifier = Modifier.fillMaxWidth(), singleLine = true,
                visualTransformation = PasswordVisualTransformation(), keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password))
            Button(onClick = { c.send(Protocol.command("wifi_config", "ssid" to ssid, "password" to pass)) }, enabled = ssid.isNotEmpty()) { Text("Сохранить") }
        }
        Expandable(Icons.Outlined.LocationOn, "Место для погоды") {
            var lat by remember { mutableStateOf("") }
            var lon by remember { mutableStateOf("") }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                val numbers = KeyboardOptions(keyboardType = KeyboardType.Decimal)
                OutlinedTextField(lat, { lat = it }, label = { Text("Широта") }, modifier = Modifier.weight(1f), singleLine = true, keyboardOptions = numbers)
                OutlinedTextField(lon, { lon = it }, label = { Text("Долгота") }, modifier = Modifier.weight(1f), singleLine = true, keyboardOptions = numbers)
            }
            Button(onClick = {
                val a = lat.replace(',', '.').toDoubleOrNull()
                val b = lon.replace(',', '.').toDoubleOrNull()
                if (a != null && b != null && a in -90.0..90.0 && b in -180.0..180.0) {
                    setting("latitude", a)
                    setting("longitude", b)
                } else c.lastError.value = "Широта от -90 до 90, долгота от -180 до 180"
            }, enabled = lat.isNotBlank() && lon.isNotBlank()) { Text("Сохранить") }
        }
        Expandable(Icons.Outlined.Code, "Для разработчиков") { DeveloperTools(c, s) }
        TextButton(onClick = { c.disconnect() }, Modifier.fillMaxWidth()) { Text("Отключиться", color = Muted) }
        Spacer(Modifier.height(8.dp))
    }
}

@Composable
private fun Group(content: @Composable ColumnScope.() -> Unit) =
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Card).padding(horizontal = 16.dp, vertical = 6.dp), content = content)

@Composable
private fun DeveloperTools(c: BleClient, s: PetState) {
    var title by remember { mutableStateOf("Привет") }
    var body by remember { mutableStateOf("Сообщение с телефона") }
    Text("Тестовое уведомление", color = Muted, fontSize = 13.sp)
    OutlinedTextField(title, { title = it }, label = { Text("Заголовок") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
    OutlinedTextField(body, { body = it }, label = { Text("Текст") }, modifier = Modifier.fillMaxWidth())
    Button(onClick = { c.send(Protocol.command("notify", "title" to title, "text" to body, "icon" to "message")) }) { Text("Отправить") }
    Text("Показатели", color = Muted, fontSize = 13.sp, modifier = Modifier.padding(top = 8.dp))
    PetState.STAT_NAMES.forEach { name ->
        val value = s.stats[name] ?: 0
        var dragging by remember { mutableStateOf<Float?>(null) }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(STAT_TITLES[name] ?: name, Modifier.width(110.dp), fontSize = 13.sp)
            Slider(
                value = dragging ?: value.toFloat(), valueRange = 0f..100f, modifier = Modifier.weight(1f),
                onValueChange = { dragging = it },
                onValueChangeFinished = { dragging?.let { v -> c.send(Protocol.command("set_stat", "name" to name, "value" to v.toInt())) }; dragging = null },
            )
            Text("$value", Modifier.width(32.dp), color = Soft, fontSize = 13.sp)
        }
    }
    val log by c.log.collectAsState()
    Text("Журнал", color = Muted, fontSize = 13.sp, modifier = Modifier.padding(top = 8.dp))
    log.takeLast(40).reversed().forEach { Text(it, fontSize = 11.sp, color = Color.LightGray) }
}

@Composable
private fun Expandable(icon: ImageVector, title: String, content: @Composable ColumnScope.() -> Unit) {
    var open by rememberSaveable(title) { mutableStateOf(false) }
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Card)) {
        Row(Modifier.fillMaxWidth().clickable { open = !open }.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(icon, null, Modifier.size(20.dp), tint = Muted)
            Spacer(Modifier.width(14.dp))
            Text(title, Modifier.weight(1f))
            Icon(if (open) Icons.Outlined.KeyboardArrowUp else Icons.Outlined.KeyboardArrowDown, null, tint = Muted)
        }
        if (open) Column(Modifier.padding(start = 16.dp, end = 16.dp, bottom = 16.dp), verticalArrangement = Arrangement.spacedBy(8.dp), content = content)
    }
}

@Composable
private fun SliderRow(icon: ImageVector, title: String, value: Int, range: ClosedFloatingPointRange<Float>, onDone: (Int) -> Unit) {
    var drag by remember { mutableStateOf<Float?>(null) }
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, Modifier.size(20.dp), tint = Muted)
        Spacer(Modifier.width(14.dp))
        // The row shows only an icon, so the name goes to screen readers.
        Slider(drag ?: value.toFloat(), { drag = it }, Modifier.weight(1f).semantics { contentDescription = title }, valueRange = range,
            onValueChangeFinished = { drag?.let { onDone(it.toInt()) }; drag = null })
    }
}

@Composable
private fun SwitchRow(icon: ImageVector, title: String, value: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().clickable { onChange(!value) }.padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, Modifier.size(20.dp), tint = Muted)
        Spacer(Modifier.width(14.dp))
        Text(title, Modifier.weight(1f))
        Switch(value, onChange)
    }
}
