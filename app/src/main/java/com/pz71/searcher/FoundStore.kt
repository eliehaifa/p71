package com.pz71.searcher

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

data class FoundKey(
    val hex: String,
    val wif: String,
    val address: String,
    val timestampMs: Long
)

object FoundStore {
    private const val PREFS = "pz71_found"
    private const val KEY = "items"

    fun list(ctx: Context): MutableList<FoundKey> {
        val prefs = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val raw = prefs.getString(KEY, null) ?: return mutableListOf()
        val arr = JSONArray(raw)
        val out = mutableListOf<FoundKey>()
        for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            out.add(
                FoundKey(
                    o.optString("hex"),
                    o.optString("wif"),
                    o.optString("addr"),
                    o.optLong("ts", 0L)
                )
            )
        }
        return out
    }

    fun add(ctx: Context, k: FoundKey) {
        val items = list(ctx)
        if (items.any { it.hex == k.hex }) return
        items.add(0, k)
        save(ctx, items)
    }

    fun clear(ctx: Context) {
        save(ctx, mutableListOf())
    }

    private fun save(ctx: Context, items: List<FoundKey>) {
        val arr = JSONArray()
        for (k in items) {
            val o = JSONObject()
            o.put("hex", k.hex)
            o.put("wif", k.wif)
            o.put("addr", k.address)
            o.put("ts", k.timestampMs)
            arr.put(o)
        }
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit().putString(KEY, arr.toString()).apply()
    }
}