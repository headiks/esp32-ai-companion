package com.companion.app

import org.json.JSONObject
import java.util.UUID

/** Mirrors protocol/commands.json and protocol/services.md. */
object Protocol {
    val SERVICE: UUID = UUID.fromString("6e7c0001-5a1e-4c6b-9d3f-0c0a1f2b3c4d")
    val RX: UUID = UUID.fromString("6e7c0002-5a1e-4c6b-9d3f-0c0a1f2b3c4d")
    val TX: UUID = UUID.fromString("6e7c0003-5a1e-4c6b-9d3f-0c0a1f2b3c4d")
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    const val CHUNK = 180

    val EMOTIONS = listOf(
        "neutral", "happy", "very_happy", "sad", "angry", "annoyed", "surprised", "curious", "thinking",
        "sleepy", "sleeping", "waking_up", "scared", "affection", "bored", "attentive", "confused", "excited",
        "mischievous", "shy", "proud", "dizzy", "yawning", "cold", "hot", "dreamy", "focused", "winking",
        "smug", "skeptical", "guilty", "embarrassed", "hopeful", "lonely", "grateful", "jealous", "determined", "terrified", "disgusted", "hungry", "starstruck", "laughing", "crying", "relieved", "sneaky", "nostalgic", "playful", "amazed", "exhausted", "suspicious",
    )

    val EMOTION_TITLES = mapOf("neutral" to "спокойствие", "happy" to "радость", "very_happy" to "восторг", "sad" to "грусть", "angry" to "злость", "annoyed" to "раздражение", "surprised" to "удивление", "curious" to "любопытство", "thinking" to "раздумье", "sleepy" to "сонливость", "sleeping" to "сон", "waking_up" to "пробуждение", "scared" to "испуг", "affection" to "нежность", "bored" to "скука", "attentive" to "внимание", "confused" to "замешательство", "excited" to "воодушевление", "mischievous" to "озорство", "shy" to "застенчивость", "proud" to "гордость", "dizzy" to "головокружение", "yawning" to "зевота", "cold" to "холодно", "hot" to "жарко", "dreamy" to "мечтательность", "focused" to "сосредоточенность", "winking" to "подмигивание", "smug" to "самодовольство", "skeptical" to "скепсис", "guilty" to "вина", "embarrassed" to "смущение", "hopeful" to "надежда", "lonely" to "одиночество", "grateful" to "благодарность", "jealous" to "ревность", "determined" to "решимость", "terrified" to "ужас", "disgusted" to "отвращение", "hungry" to "голод", "starstruck" to "звёзды в глазах", "laughing" to "смех", "crying" to "плач", "relieved" to "облегчение", "sneaky" to "хитрость", "nostalgic" to "ностальгия", "playful" to "игривость", "amazed" to "изумление", "exhausted" to "изнеможение", "suspicious" to "подозрение")
    val STATUS_TITLES = mapOf("content" to "доволен", "hungry" to "голоден", "tired" to "устал", "bored" to "скучает", "joyful" to "счастлив", "asleep" to "спит", "irritated" to "раздражён")

    fun command(type: String, vararg fields: Pair<String, Any?>): JSONObject {
        val o = JSONObject().put("type", type).put("source", "android")
        for ((k, v) in fields) o.put(k, v)
        return o
    }

    /** Splits a line into BLE-sized chunks on UTF-8 bytes; the receiver reassembles up to the newline. */
    fun chunks(line: String, size: Int = CHUNK): List<ByteArray> {
        val bytes = (line + "\n").toByteArray(Charsets.UTF_8)
        return (bytes.indices step size).map { bytes.copyOfRange(it, minOf(it + size, bytes.size)) }
    }
}

/** Reassembles notifications into JSON lines. */
class LineAssembler {
    private val buf = StringBuilder()
    private var pending = ByteArray(0)

    fun feed(data: ByteArray): List<String> {
        val all = pending + data
        val lines = mutableListOf<String>()
        var start = 0
        for (i in all.indices) {
            if (all[i] == '\n'.code.toByte()) {
                lines += String(all, start, i - start, Charsets.UTF_8)
                start = i + 1
            }
        }
        pending = all.copyOfRange(start, all.size)
        return lines
    }
}

data class PetState(
    val emotion: String = "-",
    val status: String = "-",
    val screen: String = "-",
    val stats: Map<String, Int> = emptyMap(),
    val battery: Int = 0,
    val charging: Boolean = false,
    val volume: Int = 60,
    val brightness: Int = 70,
    val sound: Boolean = true,
    val mic: Boolean = true,
    val unread: Int = 0,
    val personality: String = "",
) {
    companion object {
        val STAT_NAMES = listOf("happiness", "energy", "curiosity", "attention", "affection", "stress", "sleepiness", "hunger", "fun", "health")
    }

    fun merge(o: JSONObject): PetState {
        val merged = stats.toMutableMap()
        for (s in STAT_NAMES) if (o.has(s)) merged[s] = o.getInt(s)
        return copy(
            emotion = o.optString("emotion", emotion),
            status = o.optString("pet_status", status),
            screen = o.optString("screen", screen),
            stats = merged,
            battery = o.optInt("battery", battery),
            charging = o.optBoolean("charging", charging),
            volume = o.optInt("volume", volume),
            brightness = o.optInt("brightness", brightness),
            sound = o.optBoolean("sound", sound),
            mic = o.optBoolean("mic", mic),
            unread = o.optInt("unread", unread),
            personality = o.optString("personality", personality),
        )
    }
}
