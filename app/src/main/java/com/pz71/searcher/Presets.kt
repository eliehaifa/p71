package com.pz71.searcher

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.math.BigInteger

data class Preset(
    val name: String,
    val start: String,
    val end: String,
    val target: String
)

object Presets {
    private const val PREFS = "pz71_presets"
    private const val KEY = "items"
    private const val SEEDED = "seeded_v2"

    private fun pStart(n: Int): String =
        "0x" + BigInteger.ONE.shiftLeft(n - 1).toString(16)
    private fun pEnd(n: Int): String =
        "0x" + BigInteger.ONE.shiftLeft(n).subtract(BigInteger.ONE).toString(16)

    private val SEED = listOf(
        Preset("Puzzle 71  · 7.1 BTC", pStart(71), pEnd(71), "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"),
        Preset("Puzzle 70  · 0.7 BTC", pStart(70), pEnd(70), "19YZECXj3SxEZMoUeJ1yiPsw8xANe7M7QR"),
        Preset("Puzzle 69  · 0.69 BTC", pStart(69), pEnd(69), "19vkiEajfhuZ8bs8Zu2jgmC6oqZbWqhxhG"),
        Preset("Puzzle 68  · 0.68 BTC", pStart(68), pEnd(68), "1MVDYgVaSN6iKKEsbzRUAYFrJYJadLYZvvZ"),
        Preset("Puzzle 67  · 0.67 BTC", pStart(67), pEnd(67), "1BY8GQbnueYofwSuFAT3USAhGjPrkxDdW9"),
        Preset("Puzzle 66  · 0.66 BTC", pStart(66), pEnd(66), "13zb1hQbWVsc2S7ZTZnP2G4undNNpdh5so")
    )

    fun list(ctx: Context): MutableList<Preset> {
        val prefs = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val seeded = prefs.getBoolean(SEEDED, false)

        if (!seeded) {
            // First run with v2 seeds: install seeds (replacing any old single-preset state)
            val out = SEED.toMutableList()
            save(ctx, out)
            prefs.edit().putBoolean(SEEDED, true).apply()
            return out
        }

        val raw = prefs.getString(KEY, null) ?: return SEED.toMutableList()
        val arr = JSONArray(raw)
        val out = mutableListOf<Preset>()
        for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            out.add(
                Preset(
                    o.optString("name"),
                    o.optString("start"),
                    o.optString("end"),
                    o.optString("target")
                )
            )
        }
        return out
    }

    fun add(ctx: Context, p: Preset) {
        val items = list(ctx)
        items.removeAll { it.name == p.name }
        items.add(0, p)
        save(ctx, items)
    }

    fun remove(ctx: Context, name: String) {
        val items = list(ctx)
        items.removeAll { it.name == name }
        save(ctx, items)
    }

    fun resetToSeeds(ctx: Context) {
        save(ctx, SEED.toMutableList())
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit().putBoolean(SEEDED, true).apply()
    }

    private fun save(ctx: Context, items: List<Preset>) {
        val arr = JSONArray()
        for (p in items) {
            val o = JSONObject()
            o.put("name", p.name)
            o.put("start", p.start)
            o.put("end", p.end)
            o.put("target", p.target)
            arr.put(o)
        }
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit().putString(KEY, arr.toString()).apply()
    }
}