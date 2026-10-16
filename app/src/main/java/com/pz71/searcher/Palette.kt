package com.pz71.searcher

import android.graphics.Color

data class Palette(
    val bg: Int,
    val card: Int,
    val input: Int,
    val text: Int,
    val muted: Int,
    val accent: Int,
    val danger: Int,
    val info: Int
)

object Palettes {
    val DARK = Palette(
        bg     = Color.parseColor("#0E1016"),
        card   = Color.parseColor("#181B22"),
        input  = Color.parseColor("#232830"),
        text   = Color.parseColor("#E6EAF2"),
        muted  = Color.parseColor("#7C8698"),
        accent = Color.parseColor("#2EE59D"),
        danger = Color.parseColor("#FF6B6B"),
        info   = Color.parseColor("#6EC1FF")
    )
    val AMOLED = Palette(
        bg     = Color.parseColor("#000000"),
        card   = Color.parseColor("#0A0A0A"),
        input  = Color.parseColor("#151515"),
        text   = Color.parseColor("#F0F0F0"),
        muted  = Color.parseColor("#888888"),
        accent = Color.parseColor("#2EE59D"),
        danger = Color.parseColor("#FF6B6B"),
        info   = Color.parseColor("#6EC1FF")
    )
    val LIGHT = Palette(
        bg     = Color.parseColor("#F5F7FA"),
        card   = Color.parseColor("#FFFFFF"),
        input  = Color.parseColor("#E8ECF1"),
        text   = Color.parseColor("#0E1016"),
        muted  = Color.parseColor("#5A6470"),
        accent = Color.parseColor("#0FA968"),
        danger = Color.parseColor("#D63838"),
        info   = Color.parseColor("#2A80C4")
    )

    fun byKey(key: String?): Palette = when (key) {
        "amoled" -> AMOLED
        "light"  -> LIGHT
        else     -> DARK
    }
}