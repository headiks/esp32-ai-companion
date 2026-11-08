package com.companion.app.games

import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.random.Random

/** Names at the table, the same in every card game: you, and the computers. */
fun seatName(p: Int, players: Int) = when { p == 0 -> "Вы"; players == 2 -> "Компьютер"; else -> "Компьютер $p" }

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
    /** What each seat did last on this street: "чек", "колл", "рейз", "пас". */
    val lastMove = Array(players) { "" }
    var winners: List<Int> = emptyList(); private set
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
        lastMove.fill("")
        winners = emptyList()
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
        lastMove[p] = if (toCall(p) > 0) "колл" else "чек"
        pay(p, toCall(p))
        needsAct.remove(p)
        advance()
        return true
    }

    fun raise(p: Int): Boolean {
        if (whoActs() != p || !canRaise) return false
        lastMove[p] = "рейз"
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
        lastMove[p] = "пас"
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
        for (i in lastMove.indices) if (!folded[i]) lastMove[i] = ""
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
        winners = (0 until players).filter { won[it] > total[it] || (won[it] > 0 && won[it] == total[it] && scores[it] != null) }
        val best = scores.filterNotNull().maxOrNull()!!
        message = when {
            0 in winners && winners.size == 1 -> "Вы выиграли: ${Poker.NAMES[Poker.categoryOf(best)]}"
            0 in winners -> "Банк поделён: ${Poker.NAMES[Poker.categoryOf(best)]}"
            else -> "${seatName(winners.firstOrNull() ?: 1)} выиграл: ${Poker.NAMES[Poker.categoryOf(best)]}"
        }
    }

    private fun award(left: List<Int>) {
        stacks[left[0]] += pot
        street = Street.Showdown
        winners = left
        message = if (left[0] == 0) "Все сбросили карты, банк ваш" else "${seatName(left[0])} забирает банк"
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

    fun seatName(p: Int) = seatName(p, players)

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

/** A closed road through the control points, resampled to even steps so an index is a distance. Coordinates: x 0..1, y 0..[RaceGame.ARENA_H]. */
class RaceTrack(val name: String, points: List<Pair<Float, Float>>) {
    val xs = FloatArray(SAMPLES)
    val ys = FloatArray(SAMPLES)
    val length: Float

    init {
        val dense = ArrayList<Pair<Float, Float>>()
        val n = points.size
        for (i in 0 until n) for (k in 0 until 40) {
            val t = k / 40f
            val (p0, p1, p2, p3) = List(4) { points[(i - 1 + it + n) % n] }
            fun cr(a: Float, b: Float, c: Float, d: Float) =
                0.5f * (2 * b + (c - a) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (3 * b - a - 3 * c + d) * t * t * t)
            dense += cr(p0.first, p1.first, p2.first, p3.first) to cr(p0.second, p1.second, p2.second, p3.second)
        }
        val d = FloatArray(dense.size + 1)
        for (i in dense.indices) {
            val a = dense[i]; val b = dense[(i + 1) % dense.size]
            d[i + 1] = d[i] + hypot(b.first - a.first, b.second - a.second)
        }
        length = d.last()
        var j = 0
        for (i in 0 until SAMPLES) {
            val want = length * i / SAMPLES
            while (d[j + 1] < want) j++
            val f = (want - d[j]) / (d[j + 1] - d[j])
            val a = dense[j]; val b = dense[(j + 1) % dense.size]
            xs[i] = a.first + (b.first - a.first) * f
            ys[i] = a.second + (b.second - a.second) * f
        }
    }

    fun direction(i: Int): Float {
        val a = i.mod(SAMPLES); val b = (i + 2).mod(SAMPLES)
        return atan2(ys[b] - ys[a], xs[b] - xs[a])
    }

    companion object { const val SAMPLES = 600 }
}

val RACE_TRACKS = listOf(
    RaceTrack("Овал", listOf(0.3f to 0.12f, 0.7f to 0.12f, 0.88f to 0.3f, 0.88f to 0.95f, 0.7f to 1.13f, 0.3f to 1.13f, 0.12f to 0.95f, 0.12f to 0.3f)),
    RaceTrack("Восьмёрка", listOf(0.25f to 0.2f, 0.5f to 0.1f, 0.75f to 0.2f, 0.8f to 0.38f, 0.5f to 0.62f, 0.2f to 0.86f, 0.25f to 1.05f, 0.5f to 1.15f, 0.75f to 1.05f, 0.8f to 0.86f, 0.5f to 0.62f, 0.2f to 0.38f)),
    RaceTrack("Серпантин", listOf(0.3f to 0.12f, 0.72f to 0.12f, 0.88f to 0.27f, 0.72f to 0.41f, 0.44f to 0.41f, 0.29f to 0.56f, 0.44f to 0.71f, 0.72f to 0.71f, 0.88f to 0.86f, 0.74f to 1.13f, 0.3f to 1.14f, 0.1f to 1.0f, 0.08f to 0.3f)),
)

class RaceCar(val human: Boolean, x: Float, y: Float, heading: Float, progress: Int, val skill: Float, val line: Float) {
    var x = x; internal set
    var y = y; internal set
    var heading = heading; internal set
    var speed = 0f; internal set
    /** Samples driven since the start line: it only counts along the road, so a shortcut across the field gives nothing. */
    var progress = progress; internal set
    var finishTime: Float? = null; internal set
    var offRoad = false; internal set
    internal var lostFor = 0f
    val finished get() = finishTime != null
}

/** Ring race seen from above, four cars: [humans] players, computers in the other seats. */
class RaceGame(val track: RaceTrack, val humans: Int, val laps: Int = 3, rng: Random = Random.Default) {
    val cars: List<RaceCar> = List(4) { i ->
        val back = 6 + i * 11 // staggered grid behind the line
        val at = -back
        val a = track.direction(at)
        val side = if (i % 2 == 0) -0.028f else 0.028f
        RaceCar(i < humans,
            track.xs[at.mod(RaceTrack.SAMPLES)] - sin(a) * side,
            track.ys[at.mod(RaceTrack.SAMPLES)] + cos(a) * side,
            a, at,
            if (i < humans) 1f else 0.86f + rng.nextFloat() * 0.1f,
            if (i < humans) 0f else (rng.nextFloat() - 0.5f) * ROAD_HALF)
    }
    var countdown = 3f; private set
    var elapsed = 0f; private set
    private var over_ = false
    val over get() = over_

    /** Finishers by time, then the rest by how far they got. */
    val standings get() = cars.sortedWith(compareBy<RaceCar> { it.finishTime ?: Float.MAX_VALUE }.thenByDescending { it.progress })

    fun lap(car: RaceCar) = (car.progress.floorDiv(RaceTrack.SAMPLES) + 1).coerceIn(1, laps)

    /** The steering that follows the road. Computers use it, and so does a car that has already finished. */
    fun autoSteer(car: RaceCar): Float {
        val i = car.progress + 16
        val tx = track.xs[i.mod(RaceTrack.SAMPLES)] - sin(track.direction(i)) * car.line
        val ty = track.ys[i.mod(RaceTrack.SAMPLES)] + cos(track.direction(i)) * car.line
        return (angleTo(car.heading, atan2(ty - car.y, tx - car.x)) * 3f).coerceIn(-1f, 1f)
    }

    /** [steer] and [brake] are per human player; steer -1 is full left of the car, 1 is full right. */
    fun step(dt: Float, steer: FloatArray, brake: BooleanArray) {
        if (over_) return
        if (countdown > 0f) { countdown -= dt; return }
        elapsed += dt
        cars.forEachIndexed { i, car ->
            val pilot = car.finished || !car.human
            val s = if (pilot) autoSteer(car) else steer[i].coerceIn(-1f, 1f)
            val braking = !pilot && brake[i]
            var top = TOP_SPEED * car.skill
            if (pilot) top *= 1f - 0.45f * abs(angleTo(car.heading, track.direction(car.progress + 30))).coerceAtMost(1f) // computers slow down for bends
            if (car.finished) top *= 0.5f
            if (braking) top = 0.08f
            if (car.offRoad) top *= 0.45f
            car.speed += (top - car.speed) * min(1f, dt * (if (car.speed > top) 4f else 1.5f))
            car.heading += s * TURN_RATE * min(1f, car.speed / 0.1f) * dt
            car.x = (car.x + cos(car.heading) * car.speed * dt).coerceIn(0.01f, 0.99f)
            car.y = (car.y + sin(car.heading) * car.speed * dt).coerceIn(0.01f, ARENA_H - 0.01f)
        }
        // Cars shoulder each other instead of passing through.
        for (a in cars.indices) for (b in a + 1 until cars.size) {
            val p = cars[a]; val q = cars[b]
            val dx = q.x - p.x; val dy = q.y - p.y
            val d = hypot(dx, dy)
            if (d in 0.0001f..CAR_SPACE) {
                val push = (CAR_SPACE - d) / 2 / d
                p.x -= dx * push; p.y -= dy * push
                q.x += dx * push; q.y += dy * push
            }
        }
        for (car in cars) follow(car, dt)
        val first = cars.mapNotNull { it.finishTime }.minOrNull()
        if (cars.filter { it.human }.all { it.finished } || (first != null && elapsed > first + LAST_CALL)) over_ = true
    }

    private fun follow(car: RaceCar, dt: Float) {
        // The search window keeps a car on its own stretch where the road crosses or runs alongside itself.
        var best = 0
        var bestD = Float.MAX_VALUE
        for (o in -25..70) {
            val i = (car.progress + o).mod(RaceTrack.SAMPLES)
            val d = hypot(track.xs[i] - car.x, track.ys[i] - car.y)
            if (d < bestD) { bestD = d; best = o }
        }
        car.progress += best
        car.offRoad = bestD > ROAD_HALF
        car.lostFor = if (bestD > ROAD_HALF * 2.5f) car.lostFor + dt else 0f
        if (car.lostFor > 2f) { // far from its stretch of road for a while: back on it, not stuck until the end
            val i = car.progress.mod(RaceTrack.SAMPLES)
            car.x = track.xs[i]; car.y = track.ys[i]
            car.heading = track.direction(i)
            car.speed = 0.05f
            car.lostFor = 0f
        }
        if (!car.finished && car.progress >= laps * RaceTrack.SAMPLES) car.finishTime = elapsed
    }

    private fun angleTo(from: Float, to: Float): Float {
        var d = (to - from) % (2 * PI.toFloat())
        if (d > PI) d -= 2 * PI.toFloat()
        if (d < -PI) d += 2 * PI.toFloat()
        return d
    }

    companion object {
        const val ARENA_H = 1.25f
        const val ROAD_HALF = 0.06f
        const val TOP_SPEED = 0.34f
        const val TURN_RATE = 2.8f
        const val CAR_SPACE = 0.026f
        const val LAST_CALL = 30f // seconds the others get after the winner, so one player gone for tea does not hold the table
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
        vy = -0.62f
    }

    /** [target] is where the paddle should be, 0 to 1: the lean of the hand maps to a place, level is the middle. */
    fun step(dt: Float, target: Float) {
        if (over || won) return
        paddle = (paddle + (target - paddle) * min(1f, dt * 14f)).coerceIn(paddleWidth / 2, 1 - paddleWidth / 2)
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
                vy = -min(abs(vy) * 1.02f, 1.1f) // capped: past this no hand can follow the ball
                val aim = (ballX - paddle) / (paddleWidth / 2) * 0.75f // where it lands steers it
                // Never straight up: a ball bouncing on one column could not reach the remaining bricks.
                vx = if (abs(aim) >= 0.12f) aim else if (vx < 0f) -0.12f else 0.12f
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
