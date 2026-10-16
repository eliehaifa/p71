package com.pz71.searcher

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

data class Preset(
    val name: String,
    val start: String,
    val end: String,
    val target: String
)

object Presets {
    private const val PREFS = "pz71_presets"
    private const val KEY = "items"

    // Only seeded with addresses I'm confident about.
    // Add your own for 60, 64, 66, etc. via the Save button.
    private val SEED = listOf(
        Preset(
            "Puzzle 71",
            "0x4" + "0".repeat(17),
            "0x7" + "F".repeat(17),
            "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"
        )
    )

    fun list(ctx: Context): MutableList<Preset> {
        val prefs = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val raw = prefs.getString(KEY, null)
        if (raw == null) {
            val out = SEED.toMutableList()
            save(ctx, out)
            return out
        }
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