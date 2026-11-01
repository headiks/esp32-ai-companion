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

    @Test fun aiKeepsPairsAndHighCards() {
        assertEquals(listOf(true, true, false, false, false), Poker.aiHolds(cards("9s 9h 2d 5c Ks")))
        assertEquals(2, Poker.aiHolds(cards("2s 5h 9d Jc Ks")).count { it })
        assertTrue(Poker.aiHolds(cards("5s 6s 7s 8s 9s")).all { it })
    }

    @Test fun pokerRoundKeepsHeldCardsAndPaysOut() {
        val g = PokerGame(Random(3))
        assertEquals(90, g.chips)
        val kept = g.player[1]
        g.toggle(1)
        assertEquals("01000", g.marks())
        g.draw()
        assertEquals(kept, g.player[1])
        assertEquals(PokerPhase.Showdown, g.phase)
        assertTrue(g.chips >= 90)
        assertEquals(10, (g.player + g.opponent).toSet().size)
    }

    @Test fun pokerStopsWhenBroke() {
        val g = PokerGame(Random(4), startChips = 10)
        g.draw()
        repeat(40) { if (g.phase != PokerPhase.Broke) { g.deal(); g.draw() } }
        assertTrue(g.chips >= 0)
    }

    @Test fun blackjackValues() {
        assertEquals(21, BlackjackRules.value(cards("As Kh")))
        assertEquals(12, BlackjackRules.value(cards("As Ah")))
        assertEquals(21, BlackjackRules.value(cards("As 5h 5d")))
        assertEquals(13, BlackjackRules.value(cards("As 5h 7d")))
        assertEquals(30, BlackjackRules.value(cards("Ks Qh Jd")))
    }

    @Test fun blackjackDealerDrawsToSeventeen() {
        repeat(30) { seed ->
            val g = BlackjackGame(Random(seed))
            if (g.phase == BlackjackPhase.Player) g.stand()
            assertEquals(BlackjackPhase.Done, g.phase)
            assertTrue(BlackjackRules.value(g.dealer) >= 17)
        }
    }

    @Test fun blackjackBustEndsRound() {
        val g = BlackjackGame(Random(5))
        while (g.phase == BlackjackPhase.Player) g.hit()
        assertEquals(BlackjackPhase.Done, g.phase)
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
