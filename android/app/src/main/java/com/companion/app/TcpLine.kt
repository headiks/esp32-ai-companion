package com.companion.app

import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
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
    private val writeLock = Any()
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
        // Sockets refuse network calls on Android's main thread, and BLE callers are not on a fixed thread either.
        thread(name = "tcp-send", isDaemon = true) {
            synchronized(writeLock) {
                runCatching {
                    socket.getOutputStream().apply { write((line + "\n").toByteArray(Charsets.UTF_8)); flush() }
                }
            }
        }
    }

    fun close() {
        closing = true
        runCatching { socket.close() }
    }
}
