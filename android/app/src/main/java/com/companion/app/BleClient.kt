package com.companion.app

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import org.json.JSONException
import org.json.JSONObject
import java.util.ArrayDeque

enum class LinkState { Disconnected, Scanning, Connecting, Connected, Authenticated }
data class FoundDevice(val name: String, val address: String)

/** Single BLE connection shared by the UI and the notification forwarder. Permissions are checked by MainActivity. */
@SuppressLint("MissingPermission")
class BleClient private constructor(private val context: Context) {
    companion object {
        @Volatile private var instance: BleClient? = null
        fun get(context: Context) = instance ?: synchronized(this) {
            instance ?: BleClient(context.applicationContext).also { instance = it }
        }
    }

    val link = MutableStateFlow(LinkState.Disconnected)
    val state = MutableStateFlow(PetState())
    val devices = MutableStateFlow<List<FoundDevice>>(emptyList())
    val log = MutableStateFlow<List<String>>(emptyList())
    val lastError = MutableStateFlow<String?>(null)

    private val prefs = context.getSharedPreferences("companion", Context.MODE_PRIVATE)
    private val main = Handler(Looper.getMainLooper())
    private val adapter get() = context.getSystemService(BluetoothManager::class.java)?.adapter
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private val writes = ArrayDeque<ByteArray>()
    private var writing = false
    private val assembler = LineAssembler()
    private var nextId = 1
    private var targetAddress: String? = prefs.getString("address", null)
    private var wantConnected = false
    private var reconnectDelay = 1000L

    var pin: String
        get() = prefs.getString("pin", "") ?: ""
        set(v) = prefs.edit().putString("pin", v).apply()
    val savedAddress get() = targetAddress

    private fun addLog(s: String) = log.update { (it + s).takeLast(100) }

    private val scanCb = object : ScanCallback() {
        override fun onScanResult(type: Int, r: ScanResult) {
            val d = FoundDevice(r.device.name ?: r.scanRecord?.deviceName ?: "Companion", r.device.address)
            devices.update { if (it.any { x -> x.address == d.address }) it else it + d }
        }
        override fun onScanFailed(code: Int) { lastError.value = "Поиск не удался: $code" }
    }

    fun startScan() {
        val scanner = adapter?.bluetoothLeScanner
        if (scanner == null) { lastError.value = "Bluetooth выключен"; return }
        devices.value = emptyList()
        link.value = LinkState.Scanning
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(Protocol.SERVICE)).build()
        scanner.startScan(listOf(filter), ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCb)
        main.postDelayed({ stopScan() }, 10_000)
    }

    fun stopScan() {
        adapter?.bluetoothLeScanner?.stopScan(scanCb)
        if (link.value == LinkState.Scanning) link.value = LinkState.Disconnected
    }

    fun connect(address: String) {
        stopScan()
        targetAddress = address
        prefs.edit().putString("address", address).apply()
        wantConnected = true
        reconnectDelay = 1000
        openGatt()
    }

    fun reconnectSaved() { targetAddress?.let { connect(it) } }

    private fun openGatt() {
        val device: BluetoothDevice = adapter?.getRemoteDevice(targetAddress ?: return) ?: return
        gatt?.close()
        link.value = LinkState.Connecting
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    fun disconnect() {
        wantConnected = false
        main.removeCallbacksAndMessages(null)
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        link.value = LinkState.Disconnected
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                reconnectDelay = 1000
                g.requestMtu(247)
                return
            }
            rx = null
            synchronized(writes) { writes.clear(); writing = false }
            link.value = LinkState.Disconnected
            if (wantConnected) {
                // Automatic recovery: back off up to 30 s while the device is away.
                main.postDelayed({ if (wantConnected) openGatt() }, reconnectDelay)
                reconnectDelay = minOf(reconnectDelay * 2, 30_000)
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) { g.discoverServices() }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val svc = g.getService(Protocol.SERVICE)
            val tx = svc?.getCharacteristic(Protocol.TX)
            rx = svc?.getCharacteristic(Protocol.RX)
            if (tx == null || rx == null) { lastError.value = "Это не устройство компаньона"; g.disconnect(); return }
            g.setCharacteristicNotification(tx, true)
            val cccd = tx.getDescriptor(Protocol.CCCD)
            @Suppress("DEPRECATION")
            run {
                cccd.value = android.bluetooth.BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                g.writeDescriptor(cccd)
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: android.bluetooth.BluetoothGattDescriptor, status: Int) {
            link.value = LinkState.Connected
            send(Protocol.command("hello"))
            send(Protocol.command("auth", "pin" to pin))
            send(Protocol.command("set_time", "epoch" to System.currentTimeMillis() / 1000,
                "tz_minutes" to java.util.TimeZone.getDefault().getOffset(System.currentTimeMillis()) / 60000))
            send(Protocol.command("get_state"))
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION") onData(c.value)
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) = onData(value)

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            synchronized(writes) { writing = false }
            pump()
        }
    }

    private fun onData(data: ByteArray) {
        for (line in assembler.feed(data)) {
            try {
                val o = JSONObject(line)
                if (o.optString("error") == "auth_failed" || o.optString("error") == "auth_required") {
                    lastError.value = "Неверный PIN: посмотрите экран «Об устройстве»"
                }
                if (o.has("status") && o.getString("status") == "success" && link.value == LinkState.Connected && !o.has("protocol")) {
                    link.value = LinkState.Authenticated
                }
                state.update { it.merge(o) }
                if (!o.has("event")) addLog("← $line")
            } catch (e: JSONException) {
                addLog("плохой JSON: $line")
            }
        }
    }

    fun send(cmd: JSONObject) {
        cmd.put("request_id", nextId++)
        val line = cmd.toString()
        addLog("→ $line")
        synchronized(writes) { Protocol.chunks(line).forEach { writes.add(it) } }
        pump()
    }

    @Suppress("DEPRECATION")
    private fun pump() {
        val g = gatt ?: return
        val c = rx ?: return
        synchronized(writes) {
            if (writing || writes.isEmpty()) return
            val chunk = writes.poll()!!
            writing = true
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeCharacteristic(c, chunk, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
            } else {
                c.value = chunk
                c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                g.writeCharacteristic(c)
            }
        }
    }
}
