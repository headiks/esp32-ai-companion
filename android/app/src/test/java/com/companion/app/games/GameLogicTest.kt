package com.companion.app.games

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.random.Random

class GameLogicTest {
    private fun cards(s: String) = s.trim().split(" ").map { t ->
        val rank = when (val r = t.dropLast(1)) { "A" -> 14; "K" -> 13; "Q" -> 12; "J" -> 11; else -> r.toInt() }
        Card(rank, "shdc".indexOf(t.last()))
    }

    @Test fun labelsMatchDeviceFormat() {
        assertEquals("As", Card(14, 0).label)
        assertEquals("10h", Card(10, 1).label)
    }

    @Test fun deckHas52DistinctCards() {
        val d = Deck(Random(1))
        assertEquals(52, List(52) { d.draw() }.toSet().size)
    }

    @Test fun pokerCategories() {
        val expected = listOf(
            "2s 5h 9d Jc Ks" to 0, "2s 2h 9d Jc Ks" to 1, "2s 2h 9d 9c Ks" to 2, "2s 2h 2d 9c Ks" to 3,
            "2s 3h 4d 5c 6s" to 4, "As 2h 3d 4c 5s" to 4, "2s 5s 9s Js Ks" to 5, "2s 2h 2d 9c 9s" to 6,
            "2s 2h 2d 2c 9s" to 7, "5s 6s 7s 8s 9s" to 8,
        )
        for ((h, c) in expected) assertEquals(h, c, Poker.category(cards(h)))
    }

    @Test fun pokerOrdering() {
        fun s(h: String) = Poker.score(cards(h))
        assertTrue(s("2s 2h 2d 9c 9s") > s("Ks Kh Kd 2c 5s"))
        assertTrue(s("As Ah 3d 4c 5s") > s("Ks Kh Ad Qc Js"))
        assertTrue(s("9s 9h 7d 7c 2s") > s("9d 9c 7s 7h 2c") == false)
        assertEquals(s("9s 9h 7d 7c 2s"), s("9d 9c 7s 7h 2c"))
        assertTrue(s("2s 3h 4d 5c 6s") > s("As 2h 3d 4c 5s"), "the wheel is the lowest straight")
    }

    private fun assertTrue(msg: String, c: Boolean) = org.junit.Assert.assertTrue(msg, c)
    private fun assertTrue(c: Boolean, msg: String) = org.junit.Assert.assertTrue(msg, c)

    @Test fun bestFiveOfSeven() {
        assertEquals(5, Poker.categoryOf(Poker.best(cards("2s 5s 9s Js Ks 3h 4d"))))
        assertEquals(6, Poker.categoryOf(Poker.best(cards("9s 9h 9d 2c 2s 7h Kd"))))
        assertEquals(4, Poker.categoryOf(Poker.best(cards("Ah 2d 3c 4s 5h Kd Qc"))))
        assertEquals(Poker.score(cards("Ks Kh Ad Qc Js")), Poker.best(cards("Ks Kh Ad Qc Js 2h 3d")))
    }

    @Test fun holdemDealsTwoCardsAndPostsBlinds() {
        val g = HoldemGame(3, Random(1))
        g.hole.forEach { assertEquals(2, it.size) }
        assertTrue(g.board.isEmpty())
        assertEquals(15, g.pot)
        assertEquals(Street.Preflop, g.street)
    }

    @Test fun holdemStreetsDealThreeOneOne() {
        val g = HoldemGame(2, Random(2))
        val sizes = mutableListOf<Int>()
        var guard = 0
        while (g.street != Street.Showdown && guard++ < 50) {
            g.call(g.current)
            if (g.board.size !in sizes) sizes.add(g.board.size)
        }
        assertEquals(listOf(0, 3, 4, 5), sizes.filter { it != 0 }.let { listOf(0) + it })
        assertEquals(Street.Showdown, g.street)
    }

    @Test fun holeCardsNeverChange() {
        val g = HoldemGame(2, Random(3))
        val mine = g.hole[0].toList()
        var guard = 0
        while (g.street != Street.Showdown && guard++ < 50) g.call(g.current)
        assertEquals(mine, g.hole[0])
    }

    @Test fun holdemOnlyCurrentPlayerMayAct() {
        val g = HoldemGame(3, Random(4))
        val other = (g.current + 1) % 3
        val pot = g.pot
        assertFalse(g.call(other))
        assertFalse(g.raise(other))
        assertFalse(g.fold(other))
        assertEquals(pot, g.pot)
    }

    @Test fun foldingLeavesTheOtherWithThePot() {
        val g = HoldemGame(2, Random(5))
        val who = g.current
        val other = 1 - who
        val before = g.stacks[other]
        val pot = g.pot
        assertTrue(g.fold(who))
        assertEquals(Street.Showdown, g.street)
        assertEquals(before + pot, g.stacks[other])
    }

    @Test fun raisesAreCappedPerStreet() {
        val g = HoldemGame(2, Random(6), startChips = 1000)
        var count = 0
        while (g.canRaise && g.raise(g.current)) count++
        assertEquals(3, count)
    }

    @Test fun potSharedInLayers() {
        // The best hand is all-in for 10: it wins that layer only, the rest goes to the next best.
        assertEquals(listOf(40, 30, 0), HoldemGame.distribute(intArrayOf(30, 10, 30), listOf<Long?>(100L, 200L, 50L)).toList())
    }

    @Test fun potSplitsOnATieAndSkipsFolded() {
        assertEquals(listOf(30, 30, 0), HoldemGame.distribute(intArrayOf(20, 20, 20), listOf(9L, 9L, null)).toList())
        assertEquals(listOf(31, 30, 0), HoldemGame.distribute(intArrayOf(20, 20, 21), listOf(9L, 9L, null)).toList())
    }

    /** Whole matches with computers in every seat: chips are never created or lost and it always ends. */
    @Test fun fullMatchesConserveChips() {
        for (players in 2..4) repeat(60) { seed ->
            val g = HoldemGame(players, Random(seed), startChips = 100)
            var hands = 0
            while (g.street != Street.GameOver && hands < 400) {
                var moves = 0
                while (g.whoActs() != null) {
                    assertTrue(g.autoPlay())
                    assertTrue("betting never ends", ++moves < 200)
                    if (g.whoActs() != null) assertEquals(100 * players, g.stacks.sum() + g.pot) // once settled, the pot is already paid out
                    assertTrue(g.stacks.all { it >= 0 })
                }
                assertEquals(100 * players, g.stacks.sum())
                g.newHand()
                hands++
            }
        }
    }

    @Test fun raceSteersAndStaysInBounds() {
        val g = RaceGame(Random(1))
        repeat(200) { g.step(0.016f, 1f, true) }
        assertTrue(g.x > 1f && g.x <= 1.2f)
        assertTrue(g.distance > 0f)
    }

    @Test fun raceObstaclesEventuallyEndTheGame() {
        val g = RaceGame(Random(2))
        var steps = 0
        while (!g.over && steps++ < 200000) g.step(0.016f, 0f, true)
        assertTrue(g.over)
        val score = g.score
        g.step(0.016f, 0f, true)
        assertEquals(score, g.score)
    }

    @Test fun gasIsFasterThanCoasting() {
        val a = RaceGame(Random(9)); val b = RaceGame(Random(9))
        repeat(60) { a.step(0.016f, 0f, true); b.step(0.016f, 0f, false) }
        assertTrue(a.distance > b.distance)
    }

    @Test fun breakoutWaitsForLaunchAndLosesLives() {
        val g = BreakoutGame(Random(1))
        repeat(50) { g.step(0.016f, 1f) }
        assertEquals(g.paddle, g.ballX, 0.0001f)
        assertFalse(g.launched)
        g.launch()
        var steps = 0
        while (g.launched && steps++ < 20000) g.step(0.016f, 1f)
        assertTrue(g.lives == 2 || g.score > 0)
    }

    @Test fun breakoutAutopilotBreaksBricksAndStaysInArena() {
        val g = BreakoutGame(Random(2))
        g.launch()
        repeat(200000) {
            if (g.won || g.over) return@repeat
            if (!g.launched) g.launch()
            g.step(0.016f, ((g.ballX - g.paddle) * 12).coerceIn(-1f, 1f))
            assertTrue(g.ballX in 0f..1f && g.ballY >= 0f)
        }
        assertTrue(g.score >= 200)
    }
}
