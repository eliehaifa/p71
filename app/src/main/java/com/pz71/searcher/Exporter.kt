package com.pz71.searcher

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

object Exporter {

    private fun buildJson(ctx: Context): String {
        val root = JSONObject()
        root.put("app", "PZ Searcher")
        root.put("version", 2)
        root.put("exported_at_ms", System.currentTimeMillis())

        // found keys
        val found = JSONArray()
        for (k in FoundStore.list(ctx)) {
            val o = JSONObject()
            o.put("hex", k.hex)
            o.put("wif", k.wif)
            o.put("address", k.address)
            o.put("timestamp_ms", k.timestampMs)
            found.put(o)
        }
        root.put("found", found)

        // settings
        val prefs = ctx.getSharedPreferences("pz71_settings", Context.MODE_PRIVATE)
        val settings = JSONObject()
        for ((k, v) in prefs.all) {
            if (v is Boolean) settings.put(k, v)
        }
        root.put("settings", settings)

        return root.toString(2)
    }

    fun exportToFilesDir(ctx: Context): File {
        val f = File(ctx.filesDir, "pz71-export.json")
        f.writeText(buildJson(ctx))
        return f
    }

    /** Returns "ok:<count>" or "err:<message>" */
    fun importFromJson(ctx: Context, json: String): String {
        return try {
            val root = JSONObject(json)
            var count = 0
            val found = root.optJSONArray("found") ?: JSONArray()
            for (i in 0 until found.length()) {
                val o = found.getJSONObject(i)
                FoundStore.add(
                    ctx,
                    FoundKey(
                        o.optString("hex"),
                        o.optString("wif"),
                        o.optString("address"),
                        o.optLong("timestamp_ms", 0L)
                    )
                )
                count++
            }
            val settings = root.optJSONObject("settings")
            if (settings != null) {
                val prefs = ctx.getSharedPreferences("pz71_settings", Context.MODE_PRIVATE)
                val ed = prefs.edit()
                for (k in settings.keys()) {
                    if (settings.opt(k) is Boolean) ed.putBoolean(k, settings.getBoolean(k))
                }
                ed.apply()
            }
            "ok:$count"
        } catch (e: Exception) {
            "err:${e.message}"
        }
    }
}