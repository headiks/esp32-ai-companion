package com.companion.app

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.BluetoothSearching
import androidx.compose.material.icons.automirrored.rounded.VolumeUp
import androidx.compose.material.icons.rounded.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat

class MainActivity : ComponentActivity() {
    private lateinit var client: BleClient
    private val permissions = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
        if (result.values.all { it }) client.savedAddress?.let { client.reconnectSaved() }
    }

    private fun needed(): Array<String> =
        if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        client = BleClient.get(this)
        val missing = needed().filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) permissions.launch(missing.toTypedArray())
        else client.savedAddress?.let { client.reconnectSaved() }
        setContent {
            MaterialTheme(colorScheme = NightColors, typography = Typography().let { t ->
                t.copy(bodyLarge = t.bodyLarge.copy(fontFamily = Onest), bodyMedium = t.bodyMedium.copy(fontFamily = Onest), labelLarge = t.labelLarge.copy(fontFamily = Onest))
            }) {
                ProvideTextStyle(TextStyle(fontFamily = Onest, color = Night.Text)) {
                    App(client) { startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }
                }
            }
        }
    }
}

private enum class Tab(val title: String, val icon: ImageVector) {
    Pet("Питомец", Icons.Rounded.Pets), Chat("Чат", Icons.Rounded.Forum), Games("Игры", Icons.Rounded.SportsEsports),
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun App(client: BleClient, openNotificationAccess: () -> Unit) {
    val link by client.link.collectAsState()
    val state by client.state.collectAsState()
    val error by client.lastError.collectAsState()
    val chat by client.chat.collectAsState()
    val unread by client.chatUnread.collectAsState()
    var tab by rememberSaveable { mutableStateOf(Tab.Pet) }
    var chatOpen by rememberSaveable { mutableStateOf(false) }
    var settings by rememberSaveable { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    val stage = remember { Stage(scope) }
    val snackbar = remember { SnackbarHostState() }
    val connected = link == LinkState.Authenticated
    val mood = Protocol.STATUS_TITLES[state.status] ?: "доволен"
    LaunchedEffect(error) {
        error?.let { snackbar.showSnackbar(it); client.lastError.value = null }
    }
    // The hero is the device's screen: an answer from the chat shows up on the face too.
    var lastSaid by rememberSaveable { mutableLongStateOf(chat.lastOrNull()?.id ?: 0L) }
    LaunchedEffect(chat.size) {
        val m = chat.lastOrNull() ?: return@LaunchedEffect
        if (m.id != lastSaid && !m.me) stage.say(m.text, 4200)
        lastSaid = m.id
    }
    BackHandler(settings) { settings = false }
    Box(Modifier.fillMaxSize().background(Night.Bg)) {
        Column(Modifier.fillMaxSize().systemBarsPadding().imePadding()) {
            Header(link, state, mood, onFace = { tab = Tab.Pet; settings = false }, onSettings = { settings = !settings })
            Box(Modifier.weight(1f).fillMaxWidth()) {
                when {
                    settings -> key("settings") {
                        Column(Modifier.fillMaxSize().slideIn().padding(horizontal = 16.dp)) {
                            BackTitle("Настройки", onBack = { settings = false })
                            Spacer(Modifier.height(10.dp))
                            if (connected) SettingsScreen(client, state, openNotificationAccess) else ConnectScreen(client, link)
                        }
                    }
                    // Games run on the phone alone too; the pet and its chat need the device.
                    tab == Tab.Games -> key(tab) { GamesTab(client) }
                    !connected -> key("connect") { Box(Modifier.padding(horizontal = 16.dp).enter()) { ConnectScreen(client, link) } }
                    tab == Tab.Pet -> key(tab) { PetTab(client, state, stage) }
                    else -> key(tab) { ChatTab(client, state, mood, chatOpen) { chatOpen = it } }
                }
            }
            if (!WindowInsets.isImeVisible) NavBar(tab, unread > 0 && tab != Tab.Chat) { t ->
                if (t == tab && t == Tab.Chat) chatOpen = false
                tab = t
                settings = false
            }
        }
        SnackbarHost(snackbar, Modifier.align(Alignment.BottomCenter).systemBarsPadding().padding(bottom = 80.dp))
    }
}

@Composable
private fun Header(link: LinkState, s: PetState, mood: String, onFace: () -> Unit, onSettings: () -> Unit) {
    val connected = link == LinkState.Authenticated
    Row(Modifier.fillMaxWidth().padding(start = 20.dp, end = 12.dp, top = 8.dp, bottom = 12.dp), verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        FaceBadge(if (connected) s.emotion else "sleepy", 64.dp, 42.dp, 34.dp, Modifier.clickable(onClick = onFace))
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            val title = if (connected) Protocol.EMOTION_TITLES[s.emotion]?.replaceFirstChar { it.uppercase() } ?: s.emotion else "Компаньон"
            Text(title, fontSize = 20.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                val dot = pulse(2400)
                Box(Modifier.size(6.dp).graphicsLayer { alpha = if (connected) dot else 1f }.clip(CircleShape)
                    .background(if (connected) Night.Online else if (link == LinkState.Disconnected) Night.Rose else Night.Sand))
                val linkText = when (link) {
                    LinkState.Authenticated -> "подключено · $mood"
                    LinkState.Connected -> "проверка PIN…"
                    LinkState.Connecting -> "подключение…"
                    LinkState.Scanning -> "поиск…"
                    LinkState.Disconnected -> "нет связи"
                }
                Text(linkText, color = Night.Muted, fontSize = 12.5.sp, maxLines = 1)
            }
        }
        if (connected) Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(2.dp)) {
            val battery = when {
                s.charging -> Icons.Rounded.BatteryChargingFull
                s.battery < 20 -> Icons.Rounded.BatteryAlert
                else -> Icons.Rounded.Battery5Bar
            }
            MIcon(battery, 18.dp, Night.Muted)
            Text("${s.battery}%", color = Night.Muted, fontSize = 13.sp)
        }
        RoundIconButton(Icons.Rounded.Settings, "Настройки", 40.dp, Night.Muted, onSettings)
    }
}

/** Three tabs; a pill slides under the active icon, a pink dot marks an unread answer. */
@Composable
private fun NavBar(tab: Tab, chatDot: Boolean, onSelect: (Tab) -> Unit) {
    val at by animateFloatAsState(tab.ordinal.toFloat(), tween(400, easing = Springy), label = "pill")
    BoxWithConstraints(Modifier.fillMaxWidth().padding(start = 22.dp, end = 22.dp, top = 8.dp, bottom = 10.dp).height(58.dp)) {
        val cell = maxWidth / Tab.entries.size
        Box(Modifier.offset(x = cell * at + (cell - 64.dp) / 2).size(64.dp, 34.dp).clip(RoundedCornerShape(17.dp)).background(Night.Pill))
        Row(Modifier.fillMaxSize()) {
            Tab.entries.forEach { t ->
                val fg by animateColorAsState(if (t == tab) Night.Text else Night.Muted, tween(300), label = "tab")
                Column(Modifier.weight(1f).fillMaxHeight().clickable(remember { MutableInteractionSource() }, null, role = Role.Tab) { onSelect(t) },
                    horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    Box(Modifier.height(34.dp), contentAlignment = Alignment.Center) {
                        MIcon(t.icon, 22.dp, fg)
                        if (t == Tab.Chat && chatDot) Box(Modifier.align(Alignment.TopEnd).offset(x = 6.dp, y = 4.dp).size(7.dp).clip(CircleShape).background(Night.Rose))
                    }
                    Text(t.title, color = fg, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                }
            }
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
        Spacer(Modifier.height(12.dp))
        Box(Modifier.size(72.dp).clip(CircleShape).background(Night.Surface), contentAlignment = Alignment.Center) {
            MIcon(Icons.AutoMirrored.Rounded.BluetoothSearching, 34.dp, Night.Accent)
        }
        Text("Подключите компаньона", fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
        Text("PIN — на экране «Об устройстве»", color = Night.Muted, fontSize = 13.5.sp)
        Spacer(Modifier.height(4.dp))
        NightField(pin, { pin = it.trim(); c.pin = pin }, "PIN", Modifier.fillMaxWidth(), KeyboardOptions(keyboardType = KeyboardType.NumberPassword))
        when (link) {
            LinkState.Scanning, LinkState.Connecting, LinkState.Connected -> Row(Modifier.height(58.dp), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                CircularProgressIndicator(Modifier.size(20.dp), color = Night.Accent, strokeWidth = 2.dp)
                Text(if (link == LinkState.Scanning) "Ищем…" else "Подключаемся…")
                PillButton("Отмена", height = 40.dp) { if (link == LinkState.Scanning) c.stopScan() else c.disconnect() }
            }
            else -> PillButton("Найти устройство", Modifier.fillMaxWidth(), primary = true, height = 58.dp, icon = Icons.Rounded.Search, fontSize = 18) { c.startScan() }
        }
        devices.forEach { d ->
            Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Night.Surface).clickable { c.connect(d.address) }.padding(16.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                MIcon(Icons.Rounded.Bluetooth, 22.dp, Night.Accent)
                Column(Modifier.weight(1f)) {
                    Text(d.name, fontSize = 15.5.sp, fontWeight = FontWeight.SemiBold)
                    Text(d.address, color = Night.Muted, fontSize = 12.sp)
                }
            }
        }
        if (c.savedAddress != null && link == LinkState.Disconnected) PillButton("Подключиться к прошлому") { c.reconnectSaved() }
        Spacer(Modifier.height(12.dp))
        // The PC emulator is a developer's tool: folded away so it never competes with the device.
        Row(Modifier.clip(RoundedCornerShape(17.dp)).clickable { pcOpen = !pcOpen }.padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically) {
            Text("Эмулятор на ПК", color = Night.Muted, fontSize = 13.5.sp)
            MIcon(if (pcOpen) Icons.Rounded.KeyboardArrowUp else Icons.Rounded.KeyboardArrowDown, 20.dp, Night.Muted)
        }
        if (pcOpen) Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            NightField(host, { host = it }, "Адрес ПК", Modifier.weight(1f))
            PillButton("Подключить", primary = true, enabled = host.isNotBlank(), height = 50.dp) {
                // "адрес" or "адрес:порт"
                val h = host.trim()
                c.connectTcp(h.substringBefore(':'), h.substringAfter(':', "").toIntOrNull() ?: 7777)
            }
        }
        Spacer(Modifier.height(16.dp))
    }
}

private data class Personality(val icon: ImageVector, val title: String, val value: String)

private val PERSONALITIES = listOf(
    Personality(Icons.Rounded.SentimentVerySatisfied, "Игривый", "playful"), Personality(Icons.Rounded.Spa, "Спокойный", "calm"),
    Personality(Icons.Rounded.SentimentVeryDissatisfied, "Ворчун", "grumpy"), Personality(Icons.Rounded.VisibilityOff, "Застенчивый", "shy"),
)

@Composable
private fun SettingsScreen(c: BleClient, s: PetState, openNotificationAccess: () -> Unit) {
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val prefs = remember { ctx.getSharedPreferences("companion", Context.MODE_PRIVATE) }
    var forward by remember { mutableStateOf(prefs.getBoolean("forward", false)) }
    fun setting(name: String, value: Any) = c.send(Protocol.command("setting", "name" to name, "value" to value))
    // The personality comes only with get_settings, not with the state: ask on open and after every change.
    fun refresh() = c.send(Protocol.command("get_settings"))
    LaunchedEffect(Unit) { refresh() }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Group {
            SliderRow(Icons.Rounded.LightMode, "Яркость", s.brightness, 5f..100f) { setting("brightness", it) }
            SliderRow(Icons.AutoMirrored.Rounded.VolumeUp, "Громкость", s.volume, 0f..100f) { setting("volume", it) }
            SwitchRow(Icons.Rounded.MusicNote, "Звуки", s.sound) { setting("sound", it) }
            SwitchRow(Icons.Rounded.Mic, "Микрофон", s.mic) { setting("mic", it) }
            SwitchRow(Icons.Rounded.Notifications, "Уведомления телефона", forward) {
                forward = it
                prefs.edit().putBoolean("forward", it).apply()
                if (it) openNotificationAccess()
            }
        }
        Group {
            Text("Характер", color = Night.Muted, fontSize = 13.sp, modifier = Modifier.padding(vertical = 8.dp))
            PERSONALITIES.chunked(2).forEach { row ->
                Row(Modifier.padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    row.forEach { p ->
                        val on = s.personality == p.value
                        val fg = if (on) Night.OnAccent else Night.Text
                        Row(Modifier.weight(1f).height(46.dp).clip(RoundedCornerShape(14.dp)).background(if (on) Night.Accent else Night.Bg)
                            .clickable { setting("personality", p.value); refresh() }.padding(horizontal = 12.dp),
                            verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            MIcon(p.icon, 18.dp, fg)
                            Text(p.title, color = fg, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                        }
                    }
                }
            }
        }
        Expandable(Icons.Rounded.Wifi, "Wi-Fi") {
            var ssid by remember { mutableStateOf("") }
            var pass by remember { mutableStateOf("") }
            NightField(ssid, { ssid = it }, "Сеть", Modifier.fillMaxWidth())
            NightField(pass, { pass = it }, "Пароль", Modifier.fillMaxWidth(), KeyboardOptions(keyboardType = KeyboardType.Password), visual = PasswordVisualTransformation())
            PillButton("Сохранить", primary = true, enabled = ssid.isNotEmpty()) { c.send(Protocol.command("wifi_config", "ssid" to ssid, "password" to pass)) }
        }
        Expandable(Icons.Rounded.LocationOn, "Место для погоды") {
            var lat by remember { mutableStateOf("") }
            var lon by remember { mutableStateOf("") }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                val numbers = KeyboardOptions(keyboardType = KeyboardType.Decimal)
                NightField(lat, { lat = it }, "Широта", Modifier.weight(1f), numbers)
                NightField(lon, { lon = it }, "Долгота", Modifier.weight(1f), numbers)
            }
            PillButton("Сохранить", primary = true, enabled = lat.isNotBlank() && lon.isNotBlank()) {
                val a = lat.replace(',', '.').toDoubleOrNull()
                val b = lon.replace(',', '.').toDoubleOrNull()
                if (a != null && b != null && a in -90.0..90.0 && b in -180.0..180.0) {
                    setting("latitude", a)
                    setting("longitude", b)
                } else c.lastError.value = "Широта от -90 до 90, долгота от -180 до 180"
            }
        }
        PillButton("Отключиться", Modifier.fillMaxWidth()) { c.disconnect() }
        Spacer(Modifier.height(8.dp))
    }
}

@Composable
private fun Group(content: @Composable ColumnScope.() -> Unit) =
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(24.dp)).background(Night.Surface).padding(horizontal = 18.dp, vertical = 8.dp), content = content)

@Composable
private fun Expandable(icon: ImageVector, title: String, content: @Composable ColumnScope.() -> Unit) {
    var open by rememberSaveable(title) { mutableStateOf(false) }
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(24.dp)).background(Night.Surface)) {
        Row(Modifier.fillMaxWidth().clickable { open = !open }.padding(18.dp), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            MIcon(icon, 20.dp, Night.Muted)
            Text(title, Modifier.weight(1f), fontSize = 15.sp)
            MIcon(if (open) Icons.Rounded.KeyboardArrowUp else Icons.Rounded.KeyboardArrowDown, 22.dp, Night.Muted)
        }
        if (open) Column(Modifier.padding(start = 18.dp, end = 18.dp, bottom = 18.dp).enter(ms = 300), verticalArrangement = Arrangement.spacedBy(8.dp), content = content)
    }
}

private val NightSlider @Composable get() = SliderDefaults.colors(thumbColor = Night.Accent, activeTrackColor = Night.Accent, inactiveTrackColor = Night.Bg)

@Composable
private fun SliderRow(icon: ImageVector, title: String, value: Int, range: ClosedFloatingPointRange<Float>, onDone: (Int) -> Unit) {
    var drag by remember { mutableStateOf<Float?>(null) }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        MIcon(icon, 20.dp, Night.Muted)
        // The row shows only an icon, so the name goes to screen readers.
        Slider(drag ?: value.toFloat(), { drag = it }, Modifier.weight(1f).semantics { contentDescription = title }, valueRange = range,
            colors = NightSlider, onValueChangeFinished = { drag?.let { onDone(it.toInt()) }; drag = null })
    }
}

@Composable
private fun SwitchRow(icon: ImageVector, title: String, value: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().clickable { onChange(!value) }.padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        MIcon(icon, 20.dp, Night.Muted)
        Text(title, Modifier.weight(1f), fontSize = 15.sp)
        Switch(value, onChange, colors = SwitchDefaults.colors(checkedThumbColor = Night.OnAccent, checkedTrackColor = Night.Accent,
            uncheckedThumbColor = Night.Muted, uncheckedTrackColor = Night.Bg, uncheckedBorderColor = Night.Line2))
    }
}
