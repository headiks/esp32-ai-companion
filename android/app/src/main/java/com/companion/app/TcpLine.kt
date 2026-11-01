package com.companion.app

import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import java.util.concurrent.Executors
import kotlin.concurrent.thread

/** JSON lines over TCP, for testing against the PC emulator. Callbacks arrive on a background thread. */
class TcpLine(
    private val host: String,
    private val port: Int,
    private val onConnected: () -> Unit,
    private val onLine: (String) -> Unit,
    private val onClosed: (String?) -> Unit,
) {
    private val socket = Socket()
    // One writer in order: the protocol needs auth to arrive before the commands sent right after it.
    private val writer = Executors.newSingleThreadExecutor { r -> Thread(r, "tcp-send").apply { isDaemon = true } }
    @Volatile private var closing = false

    fun open() {
        thread(name = "tcp-line", isDaemon = true) {
            var error: String? = null
            try {
                socket.connect(InetSocketAddress(host, port), 4000)
                onConnected()
                socket.getInputStream().bufferedReader(Charsets.UTF_8).forEachLine(onLine)
            } catch (e: IOException) {
                if (!closing) error = e.message ?: "connection failed"
            }
            runCatching { socket.close() }
            onClosed(error)
        }
    }

    fun send(line: String) {
        // Android forbids network calls on the main thread, where most commands originate.
        runCatching {
            writer.execute {
                runCatching { socket.getOutputStream().apply { write((line + "\n").toByteArray(Charsets.UTF_8)); flush() } }
            }
        }
    }

    fun close() {
        closing = true
        writer.shutdown()
        runCatching { socket.close() }
    }
}
