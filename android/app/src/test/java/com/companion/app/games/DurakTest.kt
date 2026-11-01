package com.companion.app.games

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.random.Random

class DurakTest {
    private fun newGame(players: Int = 2, transfer: Boolean = false, seed: Int = 1) = DurakGame(players, transfer, Random(seed))

    @Test fun dealAndTrump() {
        val g = newGame(players = 3)
        g.hands.forEach { assertEquals(6, it.size) }
        assertEquals(18, g.deckLeft)
        assertEquals(36, g.allCards().toSet().size)
        assertEquals(g.trumpCard.suit, g.trump)
    }

    @Test fun lowestTrumpLeads() {
        repeat(30) { seed ->
            val g = newGame(seed = seed)
            val low = g.hands.flatten().filter { it.suit == g.trump }.minOfOrNull { it.rank } ?: return@repeat
            assertTrue(g.hands[g.attacker].any { it.suit == g.trump && it.rank == low })
        }
    }

    @Test fun beatingRules() {
        val g = newGame()
        val other = (g.trump + 1) % 4
        assertTrue(g.beats(Card(7, other), Card(8, other)))
        assertFalse(g.beats(Card(8, other), Card(7, other)))
        assertFalse(g.beats(Card(7, other), Card(14, (g.trump + 2) % 4)))
        assertTrue(g.beats(Card(14, other), Card(6, g.trump)))
        assertFalse(g.beats(Card(7, g.trump), Card(14, other)))
        assertTrue(g.beats(Card(7, g.trump), Card(8, g.trump)))
    }

    /** Whole games with every player on autopilot: nothing lost, always an ending. */
    @Test fun fullGamesFinishAndConserveCards() {
        for (players in 2..4) for (transfer in listOf(false, true)) repeat(150) { seed ->
            val g = newGame(players, transfer, seed)
            var moves = 0
            while (!g.finished) {
                assertTrue("game stuck p=$players t=$transfer seed=$seed", g.autoPlay())
                assertTrue("too long", ++moves < 5000)
                assertEquals(36 - g.discarded, g.allCards().size)
                assertEquals(36 - g.discarded, g.allCards().toSet().size)
                assertTrue("table limit", g.table.size <= 6)
            }
            if (g.durak != null) assertTrue(g.hands[g.durak!!].isNotEmpty())
            assertTrue(g.hands.count { it.isNotEmpty() } <= 1)
        }
    }

    private fun gameWhere(transfer: Boolean = false, check: (DurakGame) -> Boolean): DurakGame {
        for (seed in 0 until 5000) {
            val g = newGame(2, transfer, seed)
            if (check(g)) return g
        }
        throw AssertionError("no such deal")
    }

    @Test fun severalCardsOfOneRankCanLead() {
        val g = gameWhere { g -> g.hands[g.attacker].groupBy { it.rank }.values.any { it.size >= 2 } }
        val pair = g.hands[g.attacker].groupBy { it.rank }.values.first { it.size >= 2 }.take(2)
        assertTrue(g.attack(g.attacker, pair))
        assertEquals(2, g.table.size)
    }

    @Test fun mixedRanksCannotLead() {
        val g = gameWhere { g -> g.hands[g.attacker].map { it.rank }.toSet().size >= 2 }
        val h = g.hands[g.attacker]
        val mixed = listOf(h.first(), h.first { it.rank != h.first().rank })
        assertFalse(g.attack(g.attacker, mixed))
        assertTrue(g.table.isEmpty())
        assertEquals(6, g.hands[g.attacker].size)
    }

    @Test fun cardsNotInHandAreRejected() {
        val g = newGame()
        val foreign = g.hands[g.defender].first()
        assertFalse(g.attack(g.attacker, listOf(foreign)))
        assertFalse(g.attack(g.defender, listOf(foreign)))
    }

    @Test fun defenderMustBeatWithValidCards() {
        val g = gameWhere { g ->
            val a = g.hands[g.attacker].first()
            g.hands[g.defender].any { g.beats(a, it) } && g.hands[g.defender].any { !g.beats(a, it) }
        }
        val a = g.hands[g.attacker].first()
        assertTrue(g.attack(g.attacker, listOf(a)))
        assertFalse(g.defend(listOf(g.hands[g.defender].first { !g.beats(a, it) })))
        assertEquals(1, g.uncovered.size)
        assertTrue(g.defend(listOf(g.hands[g.defender].first { g.beats(a, it) })))
    }

    @Test fun defendingWithSeveralCardsIsAllOrNothing() {
        val g = newGame(seed = 3)
        var done = false
        for (seed in 0 until 5000) {
            val x = newGame(2, false, seed)
            val pair = x.hands[x.attacker].groupBy { it.rank }.values.firstOrNull { it.size >= 2 }?.take(2) ?: continue
            val covers = pair.map { a -> x.hands[x.defender].firstOrNull { x.beats(a, it) } }
            if (covers.any { it == null } || covers[0] == covers[1]) continue
            val useless = x.hands[x.defender].firstOrNull { c -> pair.none { x.beats(it, c) } } ?: continue
            assertTrue(x.attack(x.attacker, pair))
            val before = x.hands[x.defender].size
            assertFalse(x.defend(listOf(covers[0]!!, useless)))
            assertEquals(before, x.hands[x.defender].size)
            assertEquals(2, x.uncovered.size)
            assertTrue(x.defend(listOf(covers[0]!!, covers[1]!!)))
            done = true
            break
        }
        assertTrue(done && g.players == 2)
    }

    @Test fun takingMovesTableToDefenderAndKeepsAttacker() {
        val g = newGame(seed = 5)
        val first = g.attacker
        val def = g.defender
        assertTrue(g.attack(first, listOf(g.hands[first].first())))
        assertTrue(g.take())
        while (g.whoActs() == first && g.table.isNotEmpty()) assertTrue(g.pass(first))
        assertEquals(first, g.attacker)
        assertEquals(7, g.hands[def].size)
    }

    @Test fun afterBeatenBoutDefenderAttacks() {
        val g = gameWhere { g ->
            val a = g.hands[g.attacker].first()
            g.hands[g.defender].any { g.beats(a, it) }
        }
        val first = g.attacker
        val def = g.defender
        val a = g.hands[first].first()
        g.attack(first, listOf(a))
        g.defend(listOf(g.hands[def].first { g.beats(a, it) }))
        if (g.table.isNotEmpty()) assertTrue(g.pass(first))
        assertEquals(def, g.attacker)
        assertEquals(2, g.discarded)
        g.hands.forEach { assertEquals(6, it.size) }
    }

    @Test fun transferOnlyInTransferMode() {
        val pick: (DurakGame) -> Boolean = { g -> g.hands[g.attacker].any { a -> g.hands[g.defender].any { it.rank == a.rank } } }
        val plain = gameWhere(false, pick)
        var a = plain.hands[plain.attacker].first { c -> plain.hands[plain.defender].any { it.rank == c.rank } }
        plain.attack(plain.attacker, listOf(a))
        assertFalse(plain.transfer(listOf(plain.hands[plain.defender].first { it.rank == a.rank })))

        val g = gameWhere(true, pick)
        val oldAttacker = g.attacker
        val oldDefender = g.defender
        a = g.hands[oldAttacker].first { c -> g.hands[oldDefender].any { it.rank == c.rank } }
        g.attack(oldAttacker, listOf(a))
        assertTrue(g.transfer(listOf(g.hands[oldDefender].first { it.rank == a.rank })))
        assertEquals(oldDefender, g.attacker)
        assertEquals(oldAttacker, g.defender)
        assertEquals(2, g.table.size)
    }

    @Test fun transferRefusedAfterBeatingStarted() {
        var checked = false
        for (seed in 0 until 5000) {
            val g = newGame(2, true, seed)
            val pair = g.hands[g.attacker].groupBy { it.rank }.values.firstOrNull { it.size >= 2 }?.take(2) ?: continue
            val rank = pair[0].rank
            val same = g.hands[g.defender].firstOrNull { it.rank == rank } ?: continue
            val cover = g.hands[g.defender].firstOrNull { it != same && g.beats(pair[0], it) } ?: continue
            g.attack(g.attacker, pair)
            assertTrue(g.defend(listOf(cover)))
            assertFalse(g.canTransfer(listOf(same)))
            checked = true
            break
        }
        assertTrue(checked)
    }

    @Test fun throwInLimitedByDefenderHand() {
        for (seed in 0 until 300) {
            val g = newGame(2, false, seed)
            var guard = 0
            while (!g.finished && guard++ < 3000) {
                g.autoPlay()
                assertTrue(g.table.count { it.cover == null } <= g.hands[g.defender].size || g.taking || g.table.isEmpty())
            }
        }
    }

    @Test fun computersWaitForTheHuman() {
        val g = newGame(seed = 11)
        var guard = 0
        while (g.aiAct() && guard++ < 200) { /* only computers move here */ }
        val who = g.whoActs()
        assertNotNull(g)
        assertTrue(g.finished || who == 0)
        assertFalse(g.aiAct())
    }
}
