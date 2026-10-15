package com.pz71.searcher

object Native {
    init { System.loadLibrary("bb") }

    external fun init(): Int
    external fun start(mode: Int, start: String, end: String, part: String,
                       target: String, threads: Int, filesDir: String): Int
    external fun stop()
    external fun status(): String
}