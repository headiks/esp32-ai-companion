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

    /** The strongest five of the hole cards and the board. */
    fun best(cards: List<Card>): Long {
        require(cards.size in 5..7)
        var top = Long.MIN_VALUE
        for (skipA in cards.indices) for (skipB in skipA until cards.size) {
            val five = cards.filterIndexed { i, _ -> cards.size == 5 || (cards.size == 6 && i != skipA) || (cards.size == 7 && i != skipA && i != skipB && skipA != skipB) }
            if (five.size == 5) top = max(top, score(five))
        }
        return top
    }

    fun categoryOf(score: Long): Int {
        var s = score
        repeat(5) { s /= 15 }
        return s.toInt()
    }
}

enum class Street(val title: String) {
    Preflop("Префлоп"), Flop("Флоп"), Turn("Тёрн"), River("Ривер"), Showdown("Вскрытие"), GameOver("Игра окончена")
}

/** Fixed-limit Texas Hold'em: two private cards each, five shared on the table. Player 0 is the human. */
class HoldemGame(val players: Int = 2, private val rng: Random = Random.Default, startChips: Int = 200) {
    val stacks = IntArray(players) { startChips }
    val hole = List(players) { mutableListOf<Card>() }
    val board = mutableListOf<Card>()
    val folded = BooleanArray(players)
    val streetBet = IntArray(players)
    val total = IntArray(players)
    var street = Street.Showdown; private set
    var message = ""; private set
    var dealer = -1; private set
    var current = 0; private set
    private var deck = Deck(rng)
    private val needsAct = mutableSetOf<Int>()
    private var raises = 0
    val pot get() = total.sum()
    val smallBlind = 5
    private val betSize get() = if (street == Street.Turn || street == Street.River) 20 else 10

    init { newHand() }

    fun inHand(p: Int) = hole[p].isNotEmpty() && !folded[p]
    private fun canAct(p: Int) = inHand(p) && stacks[p] > 0
    fun toCall(p: Int) = (streetBet.maxOrNull() ?: 0) - streetBet[p]
    fun whoActs(): Int? = if (street == Street.Showdown || street == Street.GameOver) null else current
    val canRaise get() = raises < 3 && stacks[current] > toCall(current)

    private fun pay(p: Int, amount: Int) {
        val paid = min(amount, stacks[p])
        stacks[p] -= paid
        streetBet[p] += paid
        total[p] += paid
    }

    private fun next(from: Int, pick: (Int) -> Boolean): Int {
        var p = from
        repeat(players) {
            p = (p + 1) % players
            if (pick(p)) return p
        }
        return from
    }

    fun newHand() {
        if (stacks[0] <= 0 || stacks.count { it > 0 } <= 1) {
            street = Street.GameOver
            message = if (stacks[0] > 0) "Вы забрали все фишки!" else "Фишки кончились"
            return
        }
        deck = Deck(rng)
        board.clear()
        folded.fill(false)
        streetBet.fill(0)
        total.fill(0)
        hole.forEach { it.clear() }
        (0 until players).filter { stacks[it] > 0 }.forEach { p -> hole[p].add(deck.draw()); hole[p].add(deck.draw()) }
        dealer = next(dealer.coerceAtLeast(0)) { stacks[it] > 0 }
        street = Street.Preflop
        message = ""
        raises = 0
        // Heads-up the dealer posts the small blind, as in the standard rules.
        val active = (0 until players).count { hole[it].isNotEmpty() }
        val sb = if (active == 2) dealer else next(dealer) { hole[it].isNotEmpty() }
        val bb = next(sb) { hole[it].isNotEmpty() }
        pay(sb, smallBlind)
        pay(bb, smallBlind * 2)
        needsAct.clear()
        needsAct.addAll((0 until players).filter { canAct(it) })
        current = next(bb) { it in needsAct }
        if (needsAct.size <= 1 && toCall(needsAct.firstOrNull() ?: 0) <= 0) runOut()
    }

    /** Check when nothing is owed, otherwise call. */
    fun call(p: Int): Boolean {
        if (whoActs() != p) return false
        pay(p, toCall(p))
        needsAct.remove(p)
        advance()
        return true
    }

    fun raise(p: Int): Boolean {
        if (whoActs() != p || !canRaise) return false
        pay(p, toCall(p) + betSize)
        raises++
        needsAct.clear()
        needsAct.addAll((0 until players).filter { it != p && canAct(it) })
        advance()
        return true
    }

    fun fold(p: Int): Boolean {
        if (whoActs() != p) return false
        folded[p] = true
        needsAct.remove(p)
        advance()
        return true
    }

    private fun advance() {
        val left = (0 until players).filter { inHand(it) }
        if (left.size == 1) {
            award(left)
            return
        }
        if (needsAct.isNotEmpty()) {
            current = next(current) { it in needsAct }
            return
        }
        nextStreet()
    }

    private fun nextStreet() {
        streetBet.fill(0)
        raises = 0
        when (street) {
            Street.Preflop -> { repeat(3) { board.add(deck.draw()) }; street = Street.Flop }
            Street.Flop -> { board.add(deck.draw()); street = Street.Turn }
            Street.Turn -> { board.add(deck.draw()); street = Street.River }
            else -> { showdown(); return }
        }
        needsAct.clear()
        needsAct.addAll((0 until players).filter { canAct(it) })
        if (needsAct.size <= 1) runOut() // everyone else is all-in: nobody left to bet against
        else current = next(dealer) { it in needsAct }
    }

    private fun runOut() {
        needsAct.clear()
        while (board.size < 5) board.add(deck.draw())
        showdown()
    }

    private fun showdown() {
        val scores = (0 until players).map { p -> if (inHand(p)) Poker.best(hole[p] + board) else null }
        val won = distribute(total, scores)
        for (p in 0 until players) stacks[p] += won[p]
        street = Street.Showdown
        val winners = (0 until players).filter { won[it] > total[it] || (won[it] > 0 && won[it] == total[it] && scores[it] != null) }
        val best = scores.filterNotNull().maxOrNull()!!
        message = when {
            0 in winners && winners.size == 1 -> "Вы выиграли: ${Poker.NAMES[Poker.categoryOf(best)]}"
            0 in winners -> "Банк поделён: ${Poker.NAMES[Poker.categoryOf(best)]}"
            else -> "Игрок ${winners.firstOrNull()?.plus(1) ?: 2} выиграл: ${Poker.NAMES[Poker.categoryOf(best)]}"
        }
    }

    private fun award(left: List<Int>) {
        stacks[left[0]] += pot
        street = Street.Showdown
        message = if (left[0] == 0) "Все спасовали, банк ваш" else "Игрок ${left[0] + 1} забирает банк"
    }

    /** One decision for the current player by simple rules, with chance so it cannot be read. */
    fun autoPlay(): Boolean {
        val p = whoActs() ?: return false
        val strength = if (board.isEmpty()) {
            val (a, b) = hole[p]
            when {
                a.rank == b.rank -> 0.85f
                min(a.rank, b.rank) >= 11 -> 0.7f
                a.suit == b.suit || abs(a.rank - b.rank) == 1 -> 0.5f
                max(a.rank, b.rank) >= 12 -> 0.4f
                else -> 0.25f
            }
        } else when (Poker.categoryOf(Poker.best(hole[p] + board))) {
            0 -> 0.25f
            1 -> 0.6f
            else -> 0.9f
        }
        val roll = rng.nextFloat()
        return when {
            toCall(p) > 0 && strength < 0.35f && roll < 0.7f -> fold(p)
            strength > 0.7f && roll < 0.45f && canRaise -> raise(p)
            else -> call(p)
        }
    }

    fun aiAct(): Boolean = whoActs().let { it != null && it != 0 } && autoPlay()

    companion object {
        /** Shares the pot in layers, so someone all-in for less can only win what everybody matched. */
        fun distribute(total: IntArray, scores: List<Long?>): IntArray {
            val won = IntArray(total.size)
            var prev = 0
            for (level in total.filter { it > 0 }.toSortedSet()) {
                val layer = (level - prev) * total.count { it >= level }
                prev = level
                var eligible = total.indices.filter { total[it] >= level && scores[it] != null }
                if (eligible.isEmpty()) eligible = total.indices.filter { scores[it] != null } // only folded money at this height
                val top = eligible.maxOf { scores[it]!! }
                val winners = eligible.filter { scores[it] == top }
                winners.forEachIndexed { i, w -> won[w] += layer / winners.size + if (i < layer % winners.size) 1 else 0 }
            }
            return won
        }
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
