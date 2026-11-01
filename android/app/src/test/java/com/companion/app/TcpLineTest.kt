package com.companion.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.ServerSocket
import java.util.concurrent.CountDownLatch
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread

class TcpLineTest {
    @Test fun exchangesLinesWithUtf8() {
        val server = ServerSocket(0)
        val received = LinkedBlockingQueue<String>()
        thread {
            server.accept().use { s ->
                val r = s.getInputStream().bufferedReader(Charsets.UTF_8)
                val line = r.readLine()
                s.getOutputStream().write(("echo $line\n").toByteArray(Charsets.UTF_8))
                s.getOutputStream().flush()
                r.readLine()
            }
        }
        val opened = CountDownLatch(1)
        val link = TcpLine("127.0.0.1", server.localPort, { opened.countDown() }, { received.add(it) }, {})
        link.open()
        assertTrue(opened.await(3, TimeUnit.SECONDS))
        link.send("привет")
        assertEquals("echo привет", received.poll(3, TimeUnit.SECONDS))
        link.close()
        server.close()
    }

    @Test fun linesArriveInTheOrderSent() {
        val server = ServerSocket(0)
        val got = LinkedBlockingQueue<String>()
        thread { server.accept().use { s -> s.getInputStream().bufferedReader().forEachLine { got.add(it) } } }
        val opened = CountDownLatch(1)
        val link = TcpLine("127.0.0.1", server.localPort, { opened.countDown() }, {}, {})
        link.open()
        assertTrue(opened.await(3, TimeUnit.SECONDS))
        repeat(300) { link.send("line$it") }
        repeat(300) { assertEquals("line$it", got.poll(3, TimeUnit.SECONDS)) }
        link.close()
        server.close()
    }

    @Test fun refusedConnectionReportsError() {
        val free = ServerSocket(0).let { val p = it.localPort; it.close(); p }
        val closed = LinkedBlockingQueue<String>()
        TcpLine("127.0.0.1", free, {}, {}, { closed.add(it ?: "") }).open()
        val error = closed.poll(5, TimeUnit.SECONDS)
        assertNotNull(error)
        assertTrue(error!!.isNotEmpty())
    }

    @Test fun closingByUsIsNotAnError() {
        val server = ServerSocket(0)
        thread { runCatching { server.accept().getInputStream().read() } }
        val opened = CountDownLatch(1)
        val closed = LinkedBlockingQueue<Array<String?>>()
        val link = TcpLine("127.0.0.1", server.localPort, { opened.countDown() }, {}, { closed.add(arrayOf(it)) })
        link.open()
        assertTrue(opened.await(3, TimeUnit.SECONDS))
        link.close()
        assertNull(closed.poll(3, TimeUnit.SECONDS)!![0])
        server.close()
    }
}
