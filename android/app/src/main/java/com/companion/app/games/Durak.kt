package com.companion.app.games

import kotlin.random.Random

/** [from] is who put the attack card down: the table shows it arriving from that seat. */
class TableCard(val attack: Card, var cover: Card? = null, val from: Int = 0)

/** Durak for 2 to 4 players with a 36 card deck. Player 0 is the human; the rest are played by [aiAct]. */
class DurakGame(val players: Int = 2, val transferMode: Boolean = false, private val rng: Random = Random.Default) {
    val hands = List(players) { mutableListOf<Card>() }
    val table = mutableListOf<TableCard>()
    private val deck = ArrayDeque((0..3).flatMap { s -> (6..14).map { r -> Card(r, s) } }.shuffled(rng))
    val trumpCard: Card = deck.last()
    val trump = trumpCard.suit
    var attacker = 0; private set
    var defender = 1; private set
    var taking = false; private set
    var finished = false; private set
    /** Who lost; null means a draw. Meaningful once [finished]. */
    var durak: Int? = null; private set
    var discarded = 0; private set
    private val passed = mutableSetOf<Int>()
    private var cap = 6
    val deckLeft get() = deck.size

    init {
        require(players in 2..4)
        fill(0 until players)
        // The lowest trump leads; without any trumps in hand, player 0 does.
        val first = (0 until players).minByOrNull { p -> hands[p].filter { it.suit == trump }.minOfOrNull { it.rank } ?: 99 } ?: 0
        startBout(first)
    }

    private fun active(p: Int) = hands[p].isNotEmpty()
    private fun nextActive(from: Int): Int {
        var p = from
        repeat(players) {
            p = (p + 1) % players
            if (active(p)) return p
        }
        return from
    }

    private fun fill(order: Iterable<Int>) {
        for (p in order) while (hands[p].size < 6 && deck.isNotEmpty()) hands[p].add(deck.removeFirst())
        hands.forEach { h -> h.sortWith(compareBy({ it.suit == trump }, { it.rank }, { it.suit })) }
    }

    private fun startBout(first: Int) {
        table.clear()
        passed.clear()
        taking = false
        val alive = (0 until players).count { active(it) }
        if (alive <= 1) {
            finished = true
            durak = (0 until players).firstOrNull { active(it) }
            return
        }
        attacker = if (active(first)) first else nextActive(first)
        defender = nextActive(attacker)
        cap = minOf(6, hands[defender].size)
    }

    fun beats(attack: Card, card: Card) =
        (card.suit == attack.suit && card.rank > attack.rank) || (card.suit == trump && attack.suit != trump)

    val uncovered get() = table.filter { it.cover == null }
    private val tableRanks get() = table.flatMap { listOfNotNull(it.attack.rank, it.cover?.rank) }.toSet()

    /** Whose decision the game is waiting for; null only when finished. */
    fun whoActs(): Int? {
        if (finished) return null
        if (table.isEmpty()) return attacker
        if (!taking && uncovered.isNotEmpty()) return defender
        var p = attacker
        repeat(players) {
            if (p != defender && active(p) && p !in passed && canThrow(p)) return p
            p = (p + 1) % players
        }
        return null
    }

    private fun canThrow(p: Int) = table.size < cap && hands[p].any { it.rank in tableRanks }

    /** Puts attack cards on the table: a first move may be several of one rank, later ones must match the table. */
    fun attack(p: Int, cards: List<Card>): Boolean {
        if (whoActs() != p || p == defender || cards.isEmpty() || !hands[p].containsAll(cards)) return false
        if (table.size + cards.size > cap) return false
        val ok = if (table.isEmpty()) cards.all { it.rank == cards[0].rank } else cards.all { it.rank in tableRanks }
        if (!ok) return false
        hands[p].removeAll(cards)
        cards.forEach { table.add(TableCard(it, from = p)) }
        passed.clear()
        advance()
        return true
    }

    /** Beats the uncovered cards with the given ones, each taking the first card it can. All or nothing. */
    fun defend(cards: List<Card>): Boolean {
        if (whoActs() != defender || taking || cards.isEmpty() || !hands[defender].containsAll(cards)) return false
        val plan = mutableListOf<Pair<TableCard, Card>>()
        val free = uncovered.toMutableList()
        for (c in cards) {
            val t = free.firstOrNull { beats(it.attack, c) } ?: return false
            free.remove(t)
            plan.add(t to c)
        }
        for ((t, c) in plan) t.cover = c
        hands[defender].removeAll(cards)
        passed.clear()
        advance()
        return true
    }

    fun canTransfer(cards: List<Card>): Boolean {
        if (!transferMode || taking || cards.isEmpty() || whoActs() != defender || table.any { it.cover != null }) return false
        val rank = table.firstOrNull()?.attack?.rank ?: return false
        if (!cards.all { it.rank == rank } || !hands[defender].containsAll(cards)) return false
        // The next defender must be able to answer everything that lands on them.
        return hands[nextActive(defender)].size >= table.size + cards.size
    }

    fun transfer(cards: List<Card>): Boolean {
        if (!canTransfer(cards)) return false
        val old = defender
        hands[old].removeAll(cards)
        cards.forEach { table.add(TableCard(it, from = old)) }
        attacker = old
        defender = nextActive(old)
        cap = minOf(6, hands[defender].size)
        passed.clear()
        advance()
        return true
    }

    /** The defender gives up: the others may still add cards, then everything goes to the defender's hand. */
    fun take(): Boolean {
        if (whoActs() != defender || taking) return false
        taking = true
        passed.clear()
        advance()
        return true
    }

    /** An attacker with nothing more to add. */
    fun pass(p: Int): Boolean {
        if (whoActs() != p || p == defender || table.isEmpty()) return false
        passed.add(p)
        advance()
        return true
    }

    private fun advance() {
        if (whoActs() == null) endBout()
    }

    private fun endBout() {
        val oldAttacker = attacker
        val oldDefender = defender
        val took = taking
        if (took) {
            hands[oldDefender].addAll(table.flatMap { listOfNotNull(it.attack, it.cover) })
        } else {
            discarded += table.sumOf { t -> if (t.cover != null) 2 else 1 as Int }
        }
        table.clear()
        // Refill order: attacker first, the defender last.
        val order = (0 until players).map { (oldAttacker + it) % players }.filter { it != oldDefender } + oldDefender
        fill(order)
        val next = if (took) (oldDefender + 1) % players else oldDefender
        startBout(next)
    }

    /**
     * One decision for the current player by simple rules: cheap cards first, trumps saved. The occasional random
     * choice matters: purely deterministic players can pass the same cards back and forth for ever.
     */
    fun autoPlay(): Boolean {
        val p = whoActs() ?: return false
        val hand = hands[p]
        if (p == defender && !taking) {
            if (transferMode && table.none { it.cover != null } && rng.nextInt(4) != 0) {
                val rank = table[0].attack.rank
                val same = hand.filter { it.rank == rank && it.suit != trump }.take(1)
                if (same.isNotEmpty() && transfer(same)) return true
            }
            val free = uncovered
            val used = mutableListOf<Card>()
            for (t in free) {
                val c = (hand - used.toSet()).filter { beats(t.attack, it) }
                    .minWithOrNull(compareBy({ it.suit == trump }, { it.rank })) ?: return take()
                used.add(c)
            }
            return defend(used)
        }
        if (table.isEmpty()) {
            val low = hand.minWithOrNull(compareBy({ it.suit == trump }, { it.rank })) ?: return false
            return attack(p, listOf(if (rng.nextInt(5) == 0) hand.random(rng) else low))
        }
        val add = hand.filter { it.rank in tableRanks && (it.suit != trump || deckLeft == 0) }
            .minByOrNull { it.rank }
        return if (add != null && attack(p, listOf(add))) true else pass(p)
    }

    /** Runs one computer decision if it is a computer's turn. */
    fun aiAct(): Boolean = whoActs().let { it != null && it != 0 } && autoPlay()

    /** Every card is somewhere, exactly once. */
    fun allCards(): List<Card> = hands.flatten() + table.flatMap { listOfNotNull(it.attack, it.cover) } + deck
}
