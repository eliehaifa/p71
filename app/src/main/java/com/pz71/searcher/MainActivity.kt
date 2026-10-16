package com.pz71.searcher

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.media.AudioManager
import android.media.ToneGenerator
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.os.VibrationEffect
import android.os.Vibrator
import android.provider.Settings
import android.text.InputType
import android.text.method.ScrollingMovementMethod
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.google.android.material.tabs.TabLayout
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : AppCompatActivity() {

    private val bgColor     = Color.parseColor("#0E1016")
    private val cardColor   = Color.parseColor("#181B22")
    private val inputBg     = Color.parseColor("#232830")
    private val textColor   = Color.parseColor("#E6EAF2")
    private val mutedColor  = Color.parseColor("#7C8698")
    private val accentColor = Color.parseColor("#2EE59D")
    private val dangerColor = Color.parseColor("#FF6B6B")
    private val infoColor   = Color.parseColor("#6EC1FF")

    private lateinit var contentHost: FrameLayout
    private lateinit var searchRoot: ScrollView
    private lateinit var foundRoot: ScrollView
    private lateinit var settingsRoot: ScrollView
    private lateinit var tabLayout: TabLayout

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

    private lateinit var foundList: LinearLayout

    private lateinit var cbKeepAwake: CheckBox
    private lateinit var cbSound: CheckBox
    private lateinit var cbVibrate: CheckBox
    private lateinit var cbNotif: CheckBox
    private lateinit var btnBattery: Button
    private lateinit var btnClearFound: Button
    private lateinit var btnExport: Button
    private lateinit var btnImport: Button
    private lateinit var btnBenchmark: Button
    private lateinit var btnClearLog: Button
    private lateinit var tvAbout: TextView

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false
    private val logLines = ArrayDeque<String>()
    private var lastFoundHex: String? = null
    private var wakeLock: PowerManager.WakeLock? = null
    private var benchmarkActive = false
    private var lastResumeSaveMs = 0L

    private val prefs by lazy { getSharedPreferences("pz71_settings", Context.MODE_PRIVATE) }
    private val resumePrefs by lazy { getSharedPreferences("pz71_resume", Context.MODE_PRIVATE) }

    private fun dp(v: Float): Int = (v * resources.displayMetrics.density).toInt()

    private fun rounded(color: Int, radiusDp: Float): GradientDrawable {
        val d = GradientDrawable()
        d.setColor(color); d.cornerRadius = dp(radiusDp).toFloat()
        return d
    }

    private fun sectionLabel(t: String): TextView = TextView(this).apply {
        text = t; setTextColor(mutedColor); textSize = 11f
        setTypeface(typeface, Typeface.BOLD)
        this.letterSpacing = 0.15f
        setPadding(0, dp(20f), 0, dp(8f))
    }

    private fun fieldLabel(t: String): TextView = TextView(this).apply {
        text = t; setTextColor(mutedColor); textSize = 12f
        setPadding(0, dp(10f), 0, dp(4f))
    }

    private fun input(value: String): EditText = EditText(this).apply {
        setText(value); setTextColor(textColor); setHintTextColor(mutedColor)
        textSize = 13f
        setPadding(dp(12f), dp(12f), dp(12f), dp(12f))
        background = rounded(inputBg, 8f)
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        maxLines = 1
        inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
    }

    private fun pill(text: String, bg: Int, fg: Int): Button = Button(this).apply {
        this.text = text; setTextColor(fg); textSize = 15f
        setTypeface(typeface, Typeface.BOLD)
        background = rounded(bg, 10f)
        layoutParams = LinearLayout.LayoutParams(0, dp(52f), 1f).also { it.rightMargin = dp(8f) }
    }

    private fun bigButton(text: String, fg: Int): Button = Button(this).apply {
        this.text = text; setTextColor(fg); textSize = 13f
        background = rounded(inputBg, 10f)
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, dp(50f)
        ).also { it.topMargin = dp(8f) }
    }

    private fun toggle(text: String, checked: Boolean): CheckBox = CheckBox(this).apply {
        this.text = text; setTextColor(textColor); textSize = 14f
        isChecked = checked; setPadding(0, dp(6f), 0, dp(6f))
    }

    private fun timestamp(): String =
        SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())

    private fun appendLog(tag: String, line: String) {
        val entry = "[$tag] ${timestamp()}  $line"
        logLines.addLast(entry)
        while (logLines.size > 400) logLines.removeFirst()
        val text = logLines.joinToString("\n")
        tvLog.text = text
        tvLog.post { (tvLog.parent as? ScrollView)?.fullScroll(View.FOCUS_DOWN) }
        Thread { LogStore.append(this, entry) }.start()
    }

    // =========================================================
    //  SEARCH TAB
    // =========================================================
    private fun buildSearchTab(): ScrollView {
        val root = ScrollView(this).apply {
            setBackgroundColor(bgColor)
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        }
        val ll = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18f), dp(20f), dp(18f), dp(24f))
        }
        root.addView(ll)

        ll.addView(TextView(this).apply {
            text = "PZ Searcher"; setTextColor(textColor); textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
        })
        ll.addView(TextView(this).apply {
            text = "Bitcoin puzzle range scanner"
            setTextColor(mutedColor); textSize = 12f
            setPadding(0, dp(4f), 0, dp(6f))
        })

        val card = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18f), dp(18f), dp(18f), dp(18f))
            background = rounded(cardColor, 14f)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).also { it.topMargin = dp(14f) }
        }
        tvState = TextView(this).apply {
            text = "IDLE"; setTextColor(mutedColor); textSize = 12f
            setTypeface(typeface, Typeface.BOLD)
            this.letterSpacing = 0.2f
        }
        card.addView(tvState)
        tvBig = TextView(this).apply {
            text = "00:00:00"; setTextColor(textColor); textSize = 40f
            typeface = Typeface.MONOSPACE
            setPadding(0, dp(4f), 0, dp(2f))
        }
        card.addView(tvBig)
        tvSub = TextView(this).apply {
            text = "0 keys/s   0 checked   0%"
            setTextColor(mutedColor); textSize = 13f
            typeface = Typeface.MONOSPACE
        }
        card.addView(tvSub)
        tvKey = TextView(this).apply {
            text = "key 0x0"; setTextColor(mutedColor); textSize = 11f
            typeface = Typeface.MONOSPACE
            setPadding(0, dp(6f), 0, 0)
        }
        card.addView(tvKey)
        ll.addView(card)

        ll.addView(sectionLabel("SEARCH RANGE"))
        ll.addView(fieldLabel("Range start  (hex or decimal)"))
        etStart = input("0x4" + "0".repeat(17)); ll.addView(etStart)
        ll.addView(fieldLabel("Range end"))
        etEnd = input("0x7" + "F".repeat(17)); ll.addView(etEnd)

        ll.addView(sectionLabel("TARGET"))
        ll.addView(fieldLabel("Address (base58) or hash160 (hex)"))
        etTarget = input("1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU")
        ll.addView(etTarget)

        ll.addView(sectionLabel("MODE"))
        ll.addView(fieldLabel("Part  (full | half1 | half2 | q1..q4 | N:K)"))
        etPart = input("full"); ll.addView(etPart)
        ll.addView(fieldLabel("Chunk size  (1 = truly random, 1049600 = fast chunks)"))
        etChunk = input("1049600"); ll.addView(etChunk)
        ll.addView(fieldLabel("Threads"))
        etThreads = input(Runtime.getRuntime().availableProcessors().toString())
        ll.addView(etThreads)
        cbRandom = toggle("Random start points", true); ll.addView(cbRandom)

        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).also { it.topMargin = dp(18f) }
        }
        btnStart = pill("START", accentColor, Color.parseColor("#0E1016"))
        btnStop  = pill("STOP", dangerColor, Color.WHITE).apply { isEnabled = false }
        row.addView(btnStart); row.addView(btnStop)
        ll.addView(row)

        ll.addView(sectionLabel("LOG"))
        val logCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = rounded(cardColor, 12f)
            setPadding(dp(12f), dp(12f), dp(12f), dp(12f))
        }
        val logScroll = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(240f))
        }
        tvLog = TextView(this).apply {
            setTextColor(mutedColor); textSize = 11f
            typeface = Typeface.MONOSPACE
            movementMethod = ScrollingMovementMethod()
            text = "ready."
        }
        logScroll.addView(tvLog)
        logCard.addView(logScroll)
        ll.addView(logCard)

        return root
    }

    // =========================================================
    //  FOUND TAB
    // =========================================================
    private fun buildFoundTab(): ScrollView {
        val root = ScrollView(this).apply {
            setBackgroundColor(bgColor); visibility = View.GONE
        }
        val ll = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18f), dp(20f), dp(18f), dp(24f))
        }
        root.addView(ll)
        ll.addView(TextView(this).apply {
            text = "Found keys"; setTextColor(textColor); textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
        })
        ll.addView(TextView(this).apply {
            text = "Keys that matched the target. Tap to copy hex."
            setTextColor(mutedColor); textSize = 12f
            setPadding(0, dp(4f), 0, dp(12f))
        })
        foundList = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        ll.addView(foundList)
        return root
    }

    private fun rebuildFoundList() {
        if (!::foundList.isInitialized) return
        foundList.removeAllViews()
        val items = FoundStore.list(this)
        if (items.isEmpty()) {
            foundList.addView(TextView(this).apply {
                text = "No keys found yet."
                setTextColor(mutedColor); textSize = 13f
                setPadding(0, dp(12f), 0, dp(12f))
            })
            return
        }
        val df = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US)
        for (k in items) {
            val card = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(dp(16f), dp(14f), dp(16f), dp(14f))
                background = rounded(cardColor, 12f)
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
                ).also { it.bottomMargin = dp(10f) }
            }
            card.addView(TextView(this).apply {
                text = df.format(Date(k.timestampMs))
                setTextColor(mutedColor); textSize = 11f
            })
            card.addView(TextView(this).apply {
                text = "hex:  ${k.hex}"
                setTextColor(accentColor); textSize = 13f
                typeface = Typeface.MONOSPACE
                setPadding(0, dp(6f), 0, 0); setTextIsSelectable(true)
            })
            card.addView(TextView(this).apply {
                text = "WIF:  ${k.wif}"
                setTextColor(textColor); textSize = 12f
                typeface = Typeface.MONOSPACE
                setPadding(0, dp(2f), 0, 0); setTextIsSelectable(true)
            })
            card.addView(TextView(this).apply {
                text = "addr: ${k.address}"
                setTextColor(textColor); textSize = 12f
                typeface = Typeface.MONOSPACE
                setPadding(0, dp(2f), 0, 0); setTextIsSelectable(true)
            })
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                setPadding(0, dp(10f), 0, 0)
            }
            val bCopy = Button(this).apply {
                text = "COPY"; setTextColor(bgColor)
                background = rounded(accentColor, 8f)
                layoutParams = LinearLayout.LayoutParams(0, dp(40f), 1f).also { it.rightMargin = dp(6f) }
                setOnClickListener {
                    val clip = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                    clip.setPrimaryClip(ClipData.newPlainText("key",
                        "hex=${k.hex}\nwif=${k.wif}\naddr=${k.address}"))
                    Toast.makeText(this@MainActivity, "Copied", Toast.LENGTH_SHORT).show()
                }
            }
            val bShare = Button(this).apply {
                text = "SHARE"; setTextColor(textColor)
                background = rounded(inputBg, 8f)
                layoutParams = LinearLayout.LayoutParams(0, dp(40f), 1f)
                setOnClickListener {
                    val i = Intent(Intent.ACTION_SEND).apply {
                        type = "text/plain"
                        putExtra(Intent.EXTRA_TEXT,
                            "PZ71 key found\nhex=${k.hex}\nwif=${k.wif}\naddr=${k.address}")
                    }
                    startActivity(Intent.createChooser(i, "Share key"))
                }
            }
            row.addView(bCopy); row.addView(bShare)
            card.addView(row)
            card.setOnClickListener {
                val clip = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                clip.setPrimaryClip(ClipData.newPlainText("key", k.hex))
                Toast.makeText(this@MainActivity, "Copied hex", Toast.LENGTH_SHORT).show()
            }
            foundList.addView(card)
        }
    }

    // =========================================================
    //  SETTINGS TAB
    // =========================================================
    private fun buildSettingsTab(): ScrollView {
        val root = ScrollView(this).apply {
            setBackgroundColor(bgColor); visibility = View.GONE
        }
        val ll = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18f), dp(20f), dp(18f), dp(24f))
        }
        root.addView(ll)

        ll.addView(TextView(this).apply {
            text = "Settings"; setTextColor(textColor); textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
        })
        ll.addView(TextView(this).apply {
            text = "Behaviour, battery, data, tools"
            setTextColor(mutedColor); textSize = 12f
            setPadding(0, dp(4f), 0, dp(4f))
        })

        ll.addView(sectionLabel("BEHAVIOUR"))
        cbKeepAwake = toggle("Keep screen awake while searching", prefs.getBoolean("keepAwake", true))
        cbSound     = toggle("Play sound on found", prefs.getBoolean("sound", true))
        cbVibrate   = toggle("Vibrate on found", prefs.getBoolean("vibrate", true))
        cbNotif     = toggle("Show notification on found", prefs.getBoolean("notif", true))
        ll.addView(cbKeepAwake); ll.addView(cbSound)
        ll.addView(cbVibrate);   ll.addView(cbNotif)

        val savePrefs = View.OnClickListener {
            prefs.edit()
                .putBoolean("keepAwake", cbKeepAwake.isChecked)
                .putBoolean("sound", cbSound.isChecked)
                .putBoolean("vibrate", cbVibrate.isChecked)
                .putBoolean("notif", cbNotif.isChecked)
                .apply()
            applyKeepAwake()
        }
        cbKeepAwake.setOnClickListener(savePrefs)
        cbSound.setOnClickListener(savePrefs)
        cbVibrate.setOnClickListener(savePrefs)
        cbNotif.setOnClickListener(savePrefs)

        ll.addView(sectionLabel("BATTERY"))
        ll.addView(fieldLabel("Android may throttle background work. Disable battery optimization for this app for reliable long runs."))
        btnBattery = bigButton("OPEN BATTERY SETTINGS", textColor)
        btnBattery.setOnClickListener { openBatterySettings() }
        ll.addView(btnBattery)

        ll.addView(sectionLabel("TOOLS"))
        btnBenchmark = bigButton("RUN BENCHMARK (5M keys)", infoColor)
        btnBenchmark.setOnClickListener { runBenchmark() }
        ll.addView(btnBenchmark)

        ll.addView(sectionLabel("DATA"))
        btnExport = bigButton("EXPORT FOUND + SETTINGS", accentColor)
        btnExport.setOnClickListener { doExport() }
        ll.addView(btnExport)

        btnImport = bigButton("IMPORT FROM JSON", textColor)
        btnImport.setOnClickListener { doImport() }
        ll.addView(btnImport)

        btnClearFound = bigButton("CLEAR FOUND KEYS", dangerColor)
        btnClearFound.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle("Clear found keys?")
                .setMessage("This removes all keys from the Found tab.")
                .setPositiveButton("Clear") { _, _ ->
                    FoundStore.clear(this@MainActivity); rebuildFoundList()
                    Toast.makeText(this@MainActivity, "Cleared", Toast.LENGTH_SHORT).show()
                }
                .setNegativeButton("Cancel", null).show()
        }
        ll.addView(btnClearFound)

        btnClearLog = bigButton("CLEAR PERSISTENT LOG", dangerColor)
        btnClearLog.setOnClickListener {
            LogStore.clear(this)
            logLines.clear()
            tvLog.text = "log cleared."
            Toast.makeText(this@MainActivity, "Log cleared", Toast.LENGTH_SHORT).show()
        }
        ll.addView(btnClearLog)

        ll.addView(sectionLabel("ABOUT"))
        tvAbout = TextView(this).apply {
            text = buildAboutText()
            setTextColor(mutedColor); textSize = 12f
            typeface = Typeface.MONOSPACE
            setPadding(dp(14f), dp(14f), dp(14f), dp(14f))
            background = rounded(cardColor, 10f)
        }
        ll.addView(tvAbout)

        return root
    }

    private fun buildAboutText(): String {
        val abis = if (Build.VERSION.SDK_INT >= 21)
            Build.SUPPORTED_ABIS.joinToString(", ") else "unknown"
        return """
            PZ Searcher v2.1
            Package:  ${packageName}
            Android:  ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})
            Device:   ${Build.MANUFACTURER} ${Build.MODEL}
            ABIs:     $abis

            Engine:  custom secp256k1, batched
            affine inversion, SIMD SHA-256 /
            RIPEMD-160, self-tested on launch.

            Log file:  filesDir/pz71.log
            Export dir: filesDir/pz71-export.json
        """.trimIndent()
    }

    private fun openBatterySettings() {
        try {
            startActivity(Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS).apply {
                data = Uri.parse("package:$packageName")
            })
        } catch (e: Exception) {
            try { startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)) }
            catch (e2: Exception) {
                Toast.makeText(this, "Cannot open battery settings", Toast.LENGTH_SHORT).show()
            }
        }
    }

    // =========================================================
    //  EXPORT / IMPORT / BENCHMARK
    // =========================================================
    private fun doExport() {
        try {
            val f = Exporter.exportToFilesDir(this)
            Toast.makeText(this, "Saved: ${f.absolutePath}", Toast.LENGTH_LONG).show()
            appendLog("OK", "exported to ${f.absolutePath}")

            val i = Intent(Intent.ACTION_SEND).apply {
                type = "application/json"
                putExtra(Intent.EXTRA_STREAM, androidx.core.content.FileProvider.getUriForFile(
                    this, "$packageName.fileprovider", f))
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            // Only send if we actually have a provider (we may not; fall through silently)
            try { startActivity(Intent.createChooser(i, "Share export")) } catch (_: Exception) {}
        } catch (e: Exception) {
            appendLog("ERR", "export failed: ${e.message}")
            Toast.makeText(this, "Export failed: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun doImport() {
        val i = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/json"
        }
        try { startActivityForResult(i, 9001) }
        catch (e: Exception) { Toast.makeText(this, "No file picker", Toast.LENGTH_SHORT).show() }
    }

    @Deprecated("legacy")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == 9001 && resultCode == RESULT_OK && data?.data != null) {
            try {
                val text = contentResolver.openInputStream(data.data!!)?.bufferedReader()?.use { it.readText() }
                if (text == null) { Toast.makeText(this, "Empty file", Toast.LENGTH_SHORT).show(); return }
                val r = Exporter.importFromJson(this, text)
                if (r.startsWith("ok:")) {
                    val n = r.substring(3)
                    rebuildFoundList()
                    Toast.makeText(this, "Imported $n keys", Toast.LENGTH_LONG).show()
                    appendLog("OK", "imported $n keys")
                } else {
                    Toast.makeText(this, "Import failed: ${r.substringAfter(":")}", Toast.LENGTH_LONG).show()
                    appendLog("ERR", "import failed: ${r.substringAfter(":")}")
                }
            } catch (e: Exception) {
                Toast.makeText(this, "Import error: ${e.message}", Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun runBenchmark() {
        if (polling) { Toast.makeText(this, "Search in progress", Toast.LENGTH_SHORT).show(); return }
        benchmarkActive = true
        appendLog("INFO", "benchmark start: 5,000,000 keys, 1 thread")
        val rc = Native.start(
            0, "1", "5000000", "full",
            "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU", 1, 5000000,
            filesDir.absolutePath
        )
        if (rc != 0) {
            appendLog("ERR", "benchmark start failed rc=$rc")
            benchmarkActive = false
            return
        }
        btnStart.isEnabled = false
        btnStop.isEnabled = false
        lastFoundHex = null
        polling = true
        handler.post(poll)
    }

    // =========================================================
    //  Effects
    // =========================================================
    private fun beep() {
        try {
            val tg = ToneGenerator(AudioManager.STREAM_NOTIFICATION, 100)
            tg.startTone(ToneGenerator.TONE_PROP_BEEP, 400)
            Handler(Looper.getMainLooper()).postDelayed({ tg.release() }, 600)
        } catch (_: Exception) {}
    }

    private fun vibrateFound() {
        try {
            val v = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
            if (Build.VERSION.SDK_INT >= 26) {
                v.vibrate(VibrationEffect.createWaveform(
                    longArrayOf(0, 200, 120, 200, 120, 400), -1))
            } else {
                @Suppress("DEPRECATION")
                v.vibrate(longArrayOf(0, 200, 120, 200, 120, 400), -1)
            }
        } catch (_: Exception) {}
    }

    private fun applyKeepAwake() {
        val keep = prefs.getBoolean("keepAwake", true)
        if (keep && polling) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    private fun acquirePartialWakeLock() {
        try {
            val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
            wakeLock?.let { if (it.isHeld) it.release() }
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "pz71:search")
            wakeLock?.setReferenceCounted(false)
            wakeLock?.acquire(12L * 60L * 60L * 1000L)
        } catch (_: Exception) {}
    }

    private fun releaseWakeLock() {
        try { wakeLock?.let { if (it.isHeld) it.release() } } catch (_: Exception) {}
        wakeLock = null
    }

    // =========================================================
    //  Resume state
    // =========================================================
    private fun saveResumeState(currentKey: String) {
        resumePrefs.edit()
            .putString("key", currentKey)
            .putString("target", etTarget.text.toString().trim())
            .putString("start", etStart.text.toString().trim())
            .putString("end", etEnd.text.toString().trim())
            .putString("part", etPart.text.toString().trim())
            .putString("threads", etThreads.text.toString().trim())
            .putString("chunk", etChunk.text.toString().trim())
            .putBoolean("random", cbRandom.isChecked)
            .putLong("ts", System.currentTimeMillis())
            .apply()
    }

    private fun clearResumeState() {
        resumePrefs.edit().clear().apply()
    }

    private fun maybeOfferResume() {
        val key = resumePrefs.getString("key", null) ?: return
        val target = resumePrefs.getString("target", "") ?: ""
        val start = resumePrefs.getString("start", "") ?: ""
        val end = resumePrefs.getString("end", "") ?: ""
        if (key.isEmpty() || target.isEmpty()) return
        // only meaningful for sequential mode
        if (resumePrefs.getBoolean("random", true)) return

        AlertDialog.Builder(this)
            .setTitle("Resume last session?")
            .setMessage("Last position:\n0x$key\n\nTarget:\n$target\n\nRange:\n$start  →  $end")
            .setPositiveButton("Resume") { _, _ ->
                etStart.setText("0x$key")
                etTarget.setText(target)
                etEnd.setText(end)
                appendLog("INFO", "resuming from 0x$key")
            }
            .setNegativeButton("Start fresh") { _, _ ->
                clearResumeState()
                appendLog("INFO", "resume discarded")
            }
            .setNeutralButton("Later", null)
            .show()
    }

    // =========================================================
    //  Found
    // =========================================================
    private fun handleFound(hex: String, wif: String, addr: String) {
        FoundStore.add(this, FoundKey(hex, wif, addr, System.currentTimeMillis()))
        rebuildFoundList()

        if (prefs.getBoolean("sound", true))   beep()
        if (prefs.getBoolean("vibrate", true)) vibrateFound()
        if (prefs.getBoolean("notif", true) && hasNotifPermission())
            Notifier.found(this, hex, wif, addr)

        appendLog("OK", "MATCH FOUND hex=$hex")
        appendLog("OK", "  wif=$wif")
        appendLog("OK", "  addr=$addr")

        tabLayout.selectTab(tabLayout.getTabAt(1))

        window.addFlags(
            WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON or
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        )
        clearResumeState()
    }

    private fun hasNotifPermission(): Boolean {
        if (Build.VERSION.SDK_INT < 33) return true
        return ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) ==
                PackageManager.PERMISSION_GRANTED
    }

    private fun requestNotifPermission() {
        if (Build.VERSION.SDK_INT >= 33 && !hasNotifPermission()) {
            ActivityCompat.requestPermissions(
                this, arrayOf(Manifest.permission.POST_NOTIFICATIONS), 7001)
        }
    }

    // =========================================================
    //  Poll
    // =========================================================
    private val poll = object : Runnable {
        override fun run() {
            val s = Native.status()
            val parts = s.split("|")
            if (parts.size >= 9) {
                val state   = parts[0]
                val elapsed = parts[1]
                val keys    = parts[2]
                val kps     = parts[3]
                val pct     = parts[4]
                val cur     = parts[5]
                val foundHex  = parts[6]
                val foundWif  = parts[7]
                val foundAddr = parts[8]

                val stateColor = when (state) {
                    "running" -> accentColor
                    "found"   -> accentColor
                    "stopped" -> dangerColor
                    "done"    -> infoColor
                    else      -> mutedColor
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
                    handleFound(foundHex, foundWif, foundAddr)
                }

                if (state == "running") {
                    // save resume state every ~30 seconds
                    val now = System.currentTimeMillis()
                    if (!benchmarkActive && !cbRandom.isChecked && now - lastResumeSaveMs > 30_000) {
                        lastResumeSaveMs = now
                        try {
                            val k = Native.getCurrentKey()
                            if (k.isNotEmpty() && k != "0") saveResumeState(k)
                        } catch (_: Exception) {}
                    }
                    handler.postDelayed(this, 500)
                    return
                }

                // finished / stopped / done
                if (benchmarkActive) {
                    benchmarkActive = false
                    appendLog("OK", "benchmark: $kps keys/s over $keys keys in ${elapsed}s")
                    Toast.makeText(this@MainActivity, "Benchmark: $kps keys/s", Toast.LENGTH_LONG).show()
                } else if (state == "done") {
                    appendLog("INFO", "range finished")
                    clearResumeState()
                } else if (state == "stopped") {
                    appendLog("INFO", "stopped")
                }
            }
            polling = false
            btnStart.isEnabled = true
            btnStop.isEnabled = false
            releaseWakeLock()
            applyKeepAwake()
        }
    }

    // =========================================================
    //  Lifecycle
    // =========================================================
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Notifier.ensureChannel(this)
        requestNotifPermission()

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(bgColor)
        }

        tabLayout = TabLayout(this).apply {
            setBackgroundColor(cardColor)
            setSelectedTabIndicatorColor(accentColor)
            setTabTextColors(mutedColor, textColor)
            tabGravity = TabLayout.GRAVITY_FILL
            tabMode = TabLayout.MODE_FIXED
        }
        val tabBar = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(tabLayout, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(48f)))
        }
        root.addView(tabBar)

        contentHost = FrameLayout(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        }
        root.addView(contentHost)

        searchRoot   = buildSearchTab()
        foundRoot    = buildFoundTab()
        settingsRoot = buildSettingsTab()
        contentHost.addView(searchRoot)
        contentHost.addView(foundRoot)
        contentHost.addView(settingsRoot)

        tabLayout.addTab(tabLayout.newTab().setText(R.string.tab_search))
        tabLayout.addTab(tabLayout.newTab().setText(R.string.tab_found))
        tabLayout.addTab(tabLayout.newTab().setText(R.string.tab_settings))
        tabLayout.addOnTabSelectedListener(object : TabLayout.OnTabSelectedListener {
            override fun onTabSelected(tab: TabLayout.Tab) {
                searchRoot.visibility   = if (tab.position == 0) View.VISIBLE else View.GONE
                foundRoot.visibility    = if (tab.position == 1) View.VISIBLE else View.GONE
                settingsRoot.visibility = if (tab.position == 2) View.VISIBLE else View.GONE
                if (tab.position == 1) rebuildFoundList()
            }
            override fun onTabUnselected(tab: TabLayout.Tab) {}
            override fun onTabReselected(tab: TabLayout.Tab) {}
        })

        setContentView(root)

        // restore persistent log
        Thread {
            val prev = LogStore.readLast(this, 200)
            runOnUiThread {
                logLines.clear()
                for (line in prev) logLines.addLast(line)
                if (logLines.isNotEmpty()) {
                    tvLog.text = logLines.joinToString("\n")
                    tvLog.post { (tvLog.parent as? ScrollView)?.fullScroll(View.FOCUS_DOWN) }
                }
            }
        }.start()

        // self-test
        Thread {
            val ok = Native.init() == 1
            runOnUiThread { appendLog(if (ok) "OK" else "ERR",
                if (ok) "self-test passed" else "self-test FAILED") }
        }.start()

        rebuildFoundList()
        applyKeepAwake()

        // offer resume if a session exists (post so it appears after layout)
        handler.postDelayed({ maybeOfferResume() }, 600)

        btnStart.setOnClickListener {
            val mode = if (cbRandom.isChecked) 1 else 0
            val start = etStart.text.toString().trim()
            val end   = etEnd.text.toString().trim()
            val tgt   = etTarget.text.toString().trim()
            val part  = etPart.text.toString().trim()
            val thr   = etThreads.text.toString().trim().toIntOrNull() ?: 1
            val chunk = etChunk.text.toString().trim().toLongOrNull()?.toInt() ?: 1049600

            appendLog("INFO", "starting mode=$mode threads=$thr chunk=$chunk part=$part")
            val rc = Native.start(mode, start, end, part, tgt, thr, chunk, filesDir.absolutePath)
            if (rc != 0) { appendLog("ERR", "start failed rc=$rc"); return@setOnClickListener }
            btnStart.isEnabled = false
            btnStop.isEnabled = true
            lastFoundHex = null
            lastResumeSaveMs = 0L
            acquirePartialWakeLock()
            polling = true
            applyKeepAwake()
            handler.post(poll)
        }

        btnStop.setOnClickListener {
            Native.stop()
            appendLog("INFO", "stop requested")
        }
    }

    override fun onDestroy() {
        handler.removeCallbacks(poll)
        releaseWakeLock()
        super.onDestroy()
    }
}