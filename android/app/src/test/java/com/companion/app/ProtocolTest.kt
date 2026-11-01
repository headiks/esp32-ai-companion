package com.companion.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ProtocolTest {
    @Test fun chunksKeepUtf8AndEndWithNewline() {
        val line = "{\"title\":\"" + "я".repeat(300) + "\"}"
        val parts = Protocol.chunks(line)
        assertTrue(parts.all { it.size <= Protocol.CHUNK })
        val joined = parts.fold(ByteArray(0)) { a, b -> a + b }
        assertEquals(line + "\n", String(joined, Charsets.UTF_8))
    }

    @Test fun assemblerHandlesSplitMultibyte() {
        val data = "{\"a\":\"я\"}\n{\"b\":1}\n".toByteArray()
        val a = LineAssembler()
        val first = a.feed(data.copyOfRange(0, 7))   // splits inside the letter
        val rest = a.feed(data.copyOfRange(7, data.size))
        assertEquals(emptyList<String>(), first)
        assertEquals(listOf("{\"a\":\"я\"}", "{\"b\":1}"), rest)
    }
}
