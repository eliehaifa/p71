package com.pz71.searcher

import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.InputType
import android.text.method.ScrollingMovementMethod
import android.view.ViewGroup
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.io.File

class MainActivity : AppCompatActivity() {

    private val bgColor     = Color.parseColor("#0E1016")
    private val cardColor   = Color.parseColor("#181B22")
    private val inputBg     = Color.parseColor("#232830")
    private val textColor   = Color.parseColor("#E6EAF2")
    private val mutedColor  = Color.parseColor("#7C8698")
    private val accentColor = Color.parseColor("#2EE59D")
    private val dangerColor = Color.parseColor("#FF6B6B")

    private lateinit var etStart: EditText
    private lateinit var etEnd: EditText
    private lateinit var etTarget: EditText
    private lateinit var etPart: EditText
    private lateinit var etThreads: EditText
    private lateinit var etChunk: EditText
    private lateinit var cbRandom: CheckBox
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button
    private lateinit var tvState: TextView
    private lateinit var tvBig: TextView
    private lateinit var tvSub: TextView
    private lateinit var tvKey: TextView
    private lateinit var tvLog: TextView

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false
    private val logLines = ArrayDeque<String>()
    private var lastFoundHex: String? = null

    private fun dp(v: Float): Int = (v * resources.displayMetrics.density).toInt()

    private fun rounded(color: Int, radiusDp: Float): GradientDrawable {
        val d = GradientDrawable()
        d.setColor(color)
        d.cornerRadius = dp(radiusDp).toFloat()
        return d
    }

    private fun sectionLabel(t: String): TextView = TextView(this).apply {
        text = t
        setTextColor(mutedColor)
        textSize = 11f
        setTypeface(typeface, Typeface.BOLD)
        letterSpacing = 0.15f
        setPadding(0, dp(20f), 0, dp(8f))
    }

    private fun fieldLabel(t: String): TextView = TextView(this).apply {
        text = t
        setTextColor(mutedColor)
        textSize = 12f
        setPadding(0, dp(10f), 0, dp(4f))
    }

    private fun input(value: String): EditText = EditText(this).apply {
        setText(value)
        setTextColor(textColor)
        setHintTextColor(mutedColor)
        textSize = 13f
        setPadding(dp(12f), dp(12f), dp(12f), dp(12f))
        background = rounded(inputBg, 8f)
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )
        setSingleLine(true)
        inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
    }

    private fun pill(text: String, bg: Int, fg: Int): Button = Button(this).apply {
        this.text = text
        setTextColor(fg)
        textSize = 15f
        setTypeface(typeface, Typeface.BOLD)
        background = rounded(bg, 10f)
        layoutParams = LinearLayout.LayoutParams(0, dp(52f), 1f).also { it.marginEnd = dp(8f) }
    }

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

                val stateColor = when (state) {
                    "running" -> accentColor
                    "found" -> accentColor
                    "stopped" -> dangerColor
                    "done" -> mutedColor
                    else -> mutedColor
                }
                tvState.text = state.uppercase()
                tvState.setTextColor(stateColor)

                val secs = elapsed.toDoubleOrNull() ?: 0.0
                val hh = (secs / 3600).toInt()
                val mm = ((secs % 3600) / 60).toInt()
                val ss = (secs % 60).toInt()
                tvBig.text = String.format("%02d:%02d:%02d", hh, mm, ss)

                tvSub.text = "$kps keys/s   $keys checked   $pct%"
                tvKey.text = "key 0x$cur"

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
        while (logLines.size > 300) logLines.removeFirst()
        tvLog.text = logLines.joinToString("\n")
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = ScrollView(this).apply { setBackgroundColor(bgColor) }
        val ll = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18f), dp(24f), dp(18f), dp(24f))
        }
        root.addView(ll)

        // header
        ll.addView(TextView(this).apply {
            text = "PZ71 Searcher"
            setTextColor(textColor)
            textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
        })
        ll.addView(TextView(this).apply {
            text = "Bitcoin puzzle 71 range scanner"
            setTextColor(mutedColor)
            textSize = 12f
            setPadding(0, dp(4f), 0, dp(6f))
        })

        // status card
        val statusCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18f), dp(18f), dp(18f), dp(18f))
            background = rounded(cardColor, 14f)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).also { it.topMargin = dp(18f) }
        }
        tvState = TextView(this).apply {
            text = "IDLE"
            setTextColor(mutedColor)
            textSize = 12f
            setTypeface(typeface, Typeface.BOLD)
            letterSpacing = 0.2f
        }
        statusCard.addView(tvState)
        tvBig = TextView(this).apply {
            text = "00:00:00"
            setTextColor(textColor)
            textSize = 40f
            setTypeface(typeface, Typeface.MONOSPACE)
            setPadding(0, dp(4f), 0, dp(2f))
        }
        statusCard.addView(tvBig)
        tvSub = TextView(this).apply {
            text = "0 keys/s   0 checked   0%"
            setTextColor(mutedColor)
            textSize = 13f
            setTypeface(typeface, Typeface.MONOSPACE)
        }
        statusCard.addView(tvSub)
        tvKey = TextView(this).apply {
            text = "key 0x0"
            setTextColor(mutedColor)
            textSize = 11f
            setTypeface(typeface, Typeface.MONOSPACE)
            setPadding(0, dp(6f), 0, 0)
        }
        statusCard.addView(tvKey)
        ll.addView(statusCard)

        // settings
        ll.addView(sectionLabel("SEARCH RANGE"))

        ll.addView(fieldLabel("Range start  (hex or decimal)"))
        etStart = input("0x4" + "0".repeat(17))
        ll.addView(etStart)

        ll.addView(fieldLabel("Range end"))
        etEnd = input("0x7" + "F".repeat(17))
        ll.addView(etEnd)

        ll.addView(sectionLabel("TARGET"))

        ll.addView(fieldLabel("Address (base58) or hash160 (hex)"))
        etTarget = input("1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU")
        ll.addView(etTarget)

        ll.addView(sectionLabel("MODE"))

        ll.addView(fieldLabel("Part  (full | half1 | half2 | q1..q4 | N:K)"))
        etPart = input("full")
        ll.addView(etPart)

        ll.addView(fieldLabel("Chunk size  (1 = truly random, 1049600 = fast chunks)"))
        etChunk = input("1049600")
        ll.addView(etChunk)

        ll.addView(fieldLabel("Threads"))
        etThreads = input(Runtime.getRuntime().availableProcessors().toString())
        ll.addView(etThreads)

        cbRandom = CheckBox(this).apply {
            text = "Random start points"
            setTextColor(textColor)
            textSize = 14f
            isChecked = true
            setPadding(0, dp(10f), 0, 0)
        }
        ll.addView(cbRandom)

        // buttons
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).also { it.topMargin = dp(20f) }
        }
        btnStart = pill("START", accentColor, Color.parseColor("#0E1016"))
        btnStop  = pill("STOP", dangerColor, Color.WHITE).apply { isEnabled = false }
        row.addView(btnStart)
        row.addView(btnStop)
        ll.addView(row)

        // log
        ll.addView(sectionLabel("LOG"))
        val logBox = LinearLayout(this).apply {
            background = rounded(cardColor, 12f)
            setPadding(dp(12f), dp(12f), dp(12f), dp(12f))
        }
        tvLog = TextView(this).apply {
            setTextColor(mutedColor)
            textSize = 11f
            setTypeface(typeface, Typeface.MONOSPACE)
            movementMethod = ScrollingMovementMethod()
            text = "ready."
        }
        logBox.addView(tvLog)
        ll.addView(logBox)

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
            val chunk = etChunk.text.toString().trim().toLongOrNull()?.toInt() ?: 1049600

            appendLog("starting: mode=$mode threads=$thr chunk=$chunk part=$part")
            val rc = Native.start(mode, start, end, part, tgt, thr, chunk, filesDir.absolutePath)
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