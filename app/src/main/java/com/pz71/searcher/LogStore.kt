package com.pz71.searcher

import android.content.Context
import java.io.File

object LogStore {
    private fun file(ctx: Context) = File(ctx.filesDir, "pz71.log")

    @Synchronized
    fun append(ctx: Context, line: String) {
        try {
            file(ctx).appendText(line + "\n")
            // cap at ~500KB
            val f = file(ctx)
            if (f.length() > 500_000) {
                val keep = f.readLines().takeLast(2000)
                f.writeText(keep.joinToString("\n") + "\n")
            }
        } catch (_: Exception) {}
    }

    @Synchronized
    fun readLast(ctx: Context, n: Int): List<String> {
        return try {
            val f = file(ctx)
            if (!f.exists()) emptyList()
            else f.readLines().takeLast(n)
        } catch (_: Exception) { emptyList() }
    }

    @Synchronized
    fun clear(ctx: Context) {
        try { file(ctx).writeText("") } catch (_: Exception) {}
    }
}