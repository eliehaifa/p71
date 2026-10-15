package com.pz71.searcher

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.method.ScrollingMovementMethod
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var etStart: EditText
    private lateinit var etEnd: EditText
    private lateinit var etTarget: EditText
    private lateinit var etThreads: EditText
    private lateinit var etPart: EditText
    private lateinit var cbRandom: CheckBox
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button
    private lateinit var tvStatus: TextView
    private lateinit var tvLog: TextView

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false
    private val logLines = ArrayDeque<String>()
    private var lastFoundHex: String? = null

    private val poll = object : Runnable {
        override fun run() {
            val s = Native.status()
            val parts = s.split("|")
            if (parts.size >= 9) {
                val state = parts[0]
                val elapsed = parts[1]
                val keys = parts[2]
                val kps = parts[3]
                val pct = parts[4]
                val cur = parts[5]
                val foundHex = parts[6]
                val foundWif = parts[7]
                val foundAddr = parts[8]
                val msg = if (parts.size > 9) parts[9] else ""

                tvStatus.text = "state=$state  time=${elapsed}s  keys=$keys  ${kps} k/s  ${pct}%\nkey=0x$cur"

                if (foundHex.isNotEmpty() && foundHex != lastFoundHex) {
                    lastFoundHex = foundHex
                    appendLog("=== MATCH ===")
                    appendLog("hex: $foundHex")
                    appendLog("WIF: $foundWif")
                    appendLog("addr: $foundAddr")
                    appendLog("saved to: ${File(filesDir, "FOUND_KEY.txt").absolutePath}")
                }
                if (msg.isNotEmpty()) appendLog(msg)

                if (state == "running") {
                    handler.postDelayed(this, 500)
                    return
                }
            }
            polling = false
            btnStart.isEnabled = true
            btnStop.isEnabled = false
        }
    }

    private fun appendLog(line: String) {
        logLines.addLast(line)
        while (logLines.size > 200) logLines.removeFirst()
        tvLog.text = logLines.joinToString("\n")
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = android.widget.ScrollView(this)
        val ll = android.widget.LinearLayout(this)
        ll.orientation = android.widget.LinearLayout.VERTICAL
        val pad = (16 * resources.displayMetrics.density).toInt()
        ll.setPadding(pad, pad, pad, pad)
        root.addView(ll)

        fun label(t: String): TextView {
            val v = TextView(this); v.text = t; return v
        }

        ll.addView(label("PZ71 Searcher"))
        ll.addView(label("Range start (hex 0x.. or decimal):"))
        etStart = EditText(this)
        etStart.setText("0x40000000000000000")
        ll.addView(etStart)

        ll.addView(label("Range end:"))
        etEnd = EditText(this)
        etEnd.setText("0x7FFFFFFFFFFFFFFFFF")
        ll.addView(etEnd)

        ll.addView(label("Target (base58 addr or 40-hex hash160):"))
        etTarget = EditText(this)
        etTarget.setText("1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU")
        ll.addView(etTarget)

        ll.addView(label("Part (full | half1 | half2 | q1..q4 | N:K):"))
        etPart = EditText(this)
        etPart.setText("full")
        ll.addView(etPart)

        ll.addView(label("Threads:"))
        etThreads = EditText(this)
        etThreads.setText(Runtime.getRuntime().availableProcessors().toString())
        ll.addView(etThreads)

        cbRandom = CheckBox(this)
        cbRandom.text = "Random chunks (uncheck = sequential)"
        cbRandom.isChecked = true
        ll.addView(cbRandom)

        val row = android.widget.LinearLayout(this)
        row.orientation = android.widget.LinearLayout.HORIZONTAL
        btnStart = Button(this); btnStart.text = "Start"
        btnStop  = Button(this); btnStop.text  = "Stop"; btnStop.isEnabled = false
        row.addView(btnStart); row.addView(btnStop)
        ll.addView(row)

        tvStatus = TextView(this)
        tvStatus.text = "idle"
        ll.addView(tvStatus)

        tvLog = TextView(this)
        tvLog.movementMethod = ScrollingMovementMethod()
        tvLog.textSize = 11f
        ll.addView(tvLog)

        setContentView(root)

        Thread {
            val ok = Native.init() == 1
            runOnUiThread { appendLog(if (ok) "self-test: passed" else "self-test: FAILED") }
        }.start()

        btnStart.setOnClickListener {
            val mode = if (cbRandom.isChecked) 1 else 0
            val start = etStart.text.toString().trim()
            val end   = etEnd.text.toString().trim()
            val tgt   = etTarget.text.toString().trim()
            val part  = etPart.text.toString().trim()
            val thr   = etThreads.text.toString().trim().toIntOrNull() ?: 1

            appendLog("starting: mode=$mode threads=$thr part=$part")
            val rc = Native.start(mode, start, end, part, tgt, thr, filesDir.absolutePath)
            if (rc != 0) {
                appendLog("start failed: rc=$rc")
                return@setOnClickListener
            }
            btnStart.isEnabled = false
            btnStop.isEnabled = true
            lastFoundHex = null
            if (!polling) {
                polling = true
                handler.post(poll)
            }
        }

        btnStop.setOnClickListener {
            Native.stop()
            appendLog("stop requested")
        }
    }

    override fun onDestroy() {
        handler.removeCallbacks(poll)
        super.onDestroy()
    }
}