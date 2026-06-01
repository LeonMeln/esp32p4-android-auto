package com.aabridge.aa_bridge

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log

/**
 * Joins a specific WiFi network (the head unit's SoftAP) on demand and binds
 * the app's process to it so plain HTTP requests from Dart route over that
 * link rather than the phone's default (cellular/home WiFi) network.
 *
 * Uses the Android 10+ [WifiNetworkSpecifier] / [ConnectivityManager.requestNetwork]
 * path — the join is app-scoped and local-only (no internet), exactly what we
 * need to POST a firmware image to http://192.168.4.1/ota and nothing else.
 *
 * The bound network is released by [unbind]; callers MUST call it once the OTA
 * upload finishes (or fails) so the phone falls back to its normal network.
 */
object WifiBridge {
    private const val TAG = "WifiBridge"

    private var cm: ConnectivityManager? = null
    private var callback: ConnectivityManager.NetworkCallback? = null
    private var boundNetwork: Network? = null

    private val main = Handler(Looper.getMainLooper())

    /**
     * Request + bind to the SoftAP. [onResult] is invoked exactly once on the
     * main thread: true on success (process now bound to the AP), false with a
     * reason otherwise.
     */
    fun join(
        context: Context,
        ssid: String,
        password: String,
        timeoutMs: Int,
        onResult: (Boolean, String?) -> Unit,
    ) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            onResult(false, "Android 10+ required for in-app WiFi join")
            return
        }
        // Tear down any previous request first.
        unbind(context)

        val manager = context.applicationContext
            .getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        cm = manager

        val specifier = WifiNetworkSpecifier.Builder()
            .setSsid(ssid)
            .setWpa2Passphrase(password)
            .build()

        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            // SoftAP has no internet — do NOT require it, or the request never
            // resolves.
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .setNetworkSpecifier(specifier)
            .build()

        var settled = false
        val cb = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                if (settled) return
                settled = true
                boundNetwork = network
                @Suppress("DEPRECATION")
                val ok = manager.bindProcessToNetwork(network)
                Log.i(TAG, "joined \"$ssid\", bindProcessToNetwork=$ok")
                main.post { onResult(ok, if (ok) null else "bindProcessToNetwork failed") }
            }

            override fun onUnavailable() {
                if (settled) return
                settled = true
                Log.w(TAG, "join \"$ssid\" unavailable (declined/timeout)")
                main.post { onResult(false, "WiFi join declined or timed out") }
            }
        }
        callback = cb

        try {
            manager.requestNetwork(request, cb, timeoutMs)
        } catch (e: Exception) {
            settled = true
            callback = null
            main.post { onResult(false, "requestNetwork: ${e.message}") }
        }
    }

    /** Unbind the process and release the SoftAP request. Safe to call twice. */
    fun unbind(context: Context) {
        val manager = cm
            ?: (context.applicationContext
                .getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager)
        try {
            @Suppress("DEPRECATION")
            manager.bindProcessToNetwork(null)
        } catch (_: Exception) {
        }
        callback?.let {
            try {
                manager.unregisterNetworkCallback(it)
            } catch (_: Exception) {
            }
        }
        callback = null
        boundNetwork = null
        Log.i(TAG, "unbound")
    }
}
