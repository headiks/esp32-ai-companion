package com.companion.app.games

import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.random.Random

/** Rules and physics only, no Android: covered by plain unit tests. */
data class Card(val rank: Int, val suit: Int) {
    /** Device label: rank then suit letter, such as "10h". */
    val label: String
        get() = rankName + "shdc"[suit]
    val rankName: String
        get() = when (rank) {
            14 -> "A"; 13 -> "K"; 12 -> "Q"; 11 -> "J"
            else -> rank.toString()
        }
}

class Deck(private val rng: Random) {
    private val cards = ArrayDeque((0..3).flatMap { s -> (2..14).map { r -> Card(r, s) } }.shuffled(rng))
    fun draw(): Card = cards.removeFirst()
}

object Poker {
    val NAMES = listOf("Старшая карта", "Пара", "Две пары", "Тройка", "Стрит", "Флеш", "Фулл-хаус", "Каре", "Стрит-флеш")

    /** Higher wins. Category sits in the top digits, then ranks by importance. */
    fun score(hand: List<Card>): Long {
        require(hand.size == 5)
        val byCount = hand.groupingBy { it.rank }.eachCount().entries
            .sortedWith(compareByDescending<Map.Entry<Int, Int>> { it.value }.thenByDescending { it.key })
        val counts = byCount.map { it.value }
        var order = byCount.map { it.key }
        val flush = hand.all { it.suit == hand[0].suit }
        val sorted = hand.map { it.rank }.sortedDescending()
        var straight = counts.size == 5 && sorted[0] - sorted[4] == 4
        if (sorted == listOf(14, 5, 4, 3, 2)) { // the wheel: the ace counts low
            straight = true
            order = listOf(5, 4, 3, 2, 1)
        }
        val category = when {
            straight && flush -> 8
            counts[0] == 4 -> 7
            counts[0] == 3 && counts[1] == 2 -> 6
            flush -> 5
            straight -> 4
            counts[0] == 3 -> 3
            counts[0] == 2 && counts[1] == 2 -> 2
            counts[0] == 2 -> 1
            else -> 0
        }
        return order.fold(category.toLong()) { acc, r -> acc * 15 + r } * pow15(5 - order.size)
    }

    private fun pow15(n: Int): Long = (0 until n).fold(1L) { a, _ -> a * 15 }

    fun category(hand: List<Card>): Int {
        var s = score(hand)
        repeat(5) { s /= 15 }
        return s.toInt()
    }

    /** Which cards the computer keeps: anything paired or better, otherwise its two highest. */
    fun aiHolds(hand: List<Card>): List<Boolean> {
        if (category(hand) >= 4) return List(5) { true }
        val counts = hand.groupingBy { it.rank }.eachCount()
        if (counts.values.any { it >= 2 }) return hand.map { counts[it.rank]!! >= 2 }
        val top = hand.map { it.rank }.sortedDescending().take(2).toMutableList()
        return hand.map { c -> top.remove(c.rank) }
    }
}

enum class PokerPhase { Choosing, Showdown, Broke }

class PokerGame(private val rng: Random = Random.Default, startChips: Int = 100) {
    var chips = startChips; private set
    var player = emptyList<Card>(); private set
    var opponent = emptyList<Card>(); private set
    val held = BooleanArray(5)
    var phase = PokerPhase.Choosing; private set
    var message = ""; private set
    private var deck = Deck(rng)
    private val ante = 10

    init { deal() }

    fun deal() {
        if (chips < ante) { phase = PokerPhase.Broke; message = "Фишки кончились"; return }
        deck = Deck(rng)
        chips -= ante
        player = List(5) { deck.draw() }
        opponent = List(5) { deck.draw() }
        held.fill(false)
        phase = PokerPhase.Choosing
        message = "Выберите карты, которые оставить"
    }

    fun toggle(i: Int) {
        if (phase == PokerPhase.Choosing && i in 0..4) held[i] = !held[i]
    }

    fun draw() {
        if (phase != PokerPhase.Choosing) return
        player = player.mapIndexed { i, c -> if (held[i]) c else deck.draw() }
        val keep = Poker.aiHolds(opponent)
        opponent = opponent.mapIndexed { i, c -> if (keep[i]) c else deck.draw() }
        val a = Poker.score(player)
        val b = Poker.score(opponent)
        val name = Poker.NAMES[Poker.category(player)]
        message = when {
            a > b -> { chips += 2 * ante + Poker.category(player) * ante; "Победа: $name" }
            a == b -> { chips += ante; "Ничья" }
            else -> "Проигрыш: $name против ${Poker.NAMES[Poker.category(opponent)]}"
        }
        phase = PokerPhase.Showdown
    }

    /** Marks string for the device: '1' for held cards. */
    fun marks(): String = held.joinToString("") { if (it) "1" else "0" }
}

object BlackjackRules {
    fun value(hand: List<Card>): Int {
        var total = hand.sumOf { if (it.rank == 14) 11 else min(it.rank, 10).toInt() }
        var aces = hand.count { it.rank == 14 }
        while (total > 21 && aces > 0) { total -= 10; aces-- }
        return total
    }
}

enum class BlackjackPhase { Player, Done, Broke }

class BlackjackGame(private val rng: Random = Random.Default, startChips: Int = 100) {
    var chips = startChips; private set
    var player = emptyList<Card>(); private set
    var dealer = emptyList<Card>(); private set
    var phase = BlackjackPhase.Player; private set
    var message = ""; private set
    private var deck = Deck(rng)
    private val bet = 10

    init { deal() }

    fun deal() {
        if (chips < bet) { phase = BlackjackPhase.Broke; message = "Фишки кончились"; return }
        deck = Deck(rng)
        chips -= bet
        player = listOf(deck.draw(), deck.draw())
        dealer = listOf(deck.draw(), deck.draw())
        phase = BlackjackPhase.Player
        message = "Ещё или хватит?"
        if (BlackjackRules.value(player) == 21) stand()
    }

    fun hit() {
        if (phase != BlackjackPhase.Player) return
        player = player + deck.draw()
        when {
            BlackjackRules.value(player) > 21 -> { phase = BlackjackPhase.Done; message = "Перебор" }
            BlackjackRules.value(player) == 21 -> stand()
        }
    }

    fun stand() {
        if (phase != BlackjackPhase.Player) return
        while (BlackjackRules.value(dealer) < 17) dealer = dealer + deck.draw()
        val p = BlackjackRules.value(player)
        val d = BlackjackRules.value(dealer)
        message = when {
            d > 21 || p > d -> { chips += 2 * bet; "Победа: $p против $d" }
            p == d -> { chips += bet; "Ничья: $p" }
            else -> "Проигрыш: $p против $d"
        }
        phase = BlackjackPhase.Done
    }
}

data class Obstacle(val x: Float, val z: Float)

/** Road seen from the car: x from -1 to 1 across the road, z from 1 at the horizon to 0 at the car. */
class RaceGame(private val rng: Random = Random.Default) {
    var x = 0f; private set
    var speed = 0.3f; private set
    var distance = 0f; private set
    var lives = 3; private set
    var obstacles = emptyList<Obstacle>(); private set
    var crashFlash = 0f; private set
    val over get() = lives <= 0
    val score get() = (distance * 10).toInt()
    private var spawnIn = 1.5f

    fun step(dt: Float, steer: Float, gas: Boolean) {
        if (over) return
        val top = if (gas) 1.1f else 0.55f
        speed += (top - speed) * min(1f, dt * 1.2f)
        x = (x + steer.coerceIn(-1f, 1f) * dt * 1.9f).coerceIn(-1.2f, 1.2f)
        if (abs(x) > 1f) speed *= 1f - min(1f, dt * 1.5f) // the shoulder is slow
        distance += speed * dt
        crashFlash = max(0f, crashFlash - dt)
        val moved = obstacles.map { it.copy(z = it.z - speed * dt * 0.75f) }
        val hit = moved.firstOrNull { it.z in 0f..0.07f && abs(it.x - x) < 0.3f }
        if (hit != null) {
            lives--
            speed = 0.15f
            crashFlash = 0.5f
        }
        obstacles = moved.filter { it.z > 0f && it !== hit }
        spawnIn -= dt * speed
        if (spawnIn <= 0f) {
            spawnIn = rng.nextFloat() * 0.6f + 0.25f
            obstacles = obstacles + Obstacle(rng.nextFloat() * 1.7f - 0.85f, 1f)
        }
    }
}

/** Unit-square arena: tilt moves the paddle at the bottom. */
class BreakoutGame(private val rng: Random = Random.Default) {
    val cols = 8
    val rows = 5
    val bricks = BooleanArray(cols * rows) { true }
    var paddle = 0.5f; private set
    var ballX = 0.5f; private set
    var ballY = 0.88f; private set
    var launched = false; private set
    var lives = 3; private set
    var score = 0; private set
    val paddleWidth = 0.22f
    val over get() = lives <= 0
    val won get() = bricks.none { it }
    private var vx = 0f
    private var vy = 0f

    fun launch() {
        if (launched || over || won) return
        launched = true
        vx = (rng.nextFloat() - 0.5f) * 0.6f
        vy = -0.72f
    }

    fun step(dt: Float, tilt: Float) {
        if (over || won) return
        paddle = (paddle + tilt.coerceIn(-1f, 1f) * dt * 1.8f).coerceIn(paddleWidth / 2, 1 - paddleWidth / 2)
        if (!launched) { ballX = paddle; ballY = 0.88f; return }
        var left = dt
        while (left > 0f) { // short steps: a fast ball cannot pass through a brick
            val h = min(left, 0.008f)
            left -= h
            ballX += vx * h
            ballY += vy * h
            if (ballX < 0f) { ballX = 0f; vx = abs(vx) }
            if (ballX > 1f) { ballX = 1f; vx = -abs(vx) }
            if (ballY < 0f) { ballY = 0f; vy = abs(vy) }
            if (vy > 0 && ballY >= 0.92f && ballY <= 0.95f && abs(ballX - paddle) < paddleWidth / 2) {
                vy = -abs(vy) * 1.02f
                vx = (ballX - paddle) / (paddleWidth / 2) * 0.75f // where it lands steers it
            }
            if (ballY > 1f) {
                lives--
                launched = false
                ballX = paddle
                ballY = 0.88f
                return
            }
            val c = (ballX * cols).toInt().coerceIn(0, cols - 1)
            val r = ((ballY - 0.08f) / 0.05f).toInt()
            if (ballY >= 0.08f && r in 0 until rows && bricks[r * cols + c]) {
                bricks[r * cols + c] = false
                vy = -vy
                score += 10
            }
        }
    }
}
