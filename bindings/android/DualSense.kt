package com.dualsense.engine

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.*
import android.os.Build

/** Thin JNI facade. Values: side 0 = left, 1 = right. */
object DualSenseNative {
    init { System.loadLibrary("DualSenseEngine") }
    @JvmStatic external fun create(transport: Int): Long          // 0 = USB
    @JvmStatic external fun destroy(h: Long)
    @JvmStatic external fun isDualSense(vid: Int, pid: Int): Boolean
    @JvmStatic external fun triggerOff(h: Long, side: Int): Boolean
    @JvmStatic external fun triggerFeedback(h: Long, side: Int, pos: Int, strength: Int): Boolean
    @JvmStatic external fun triggerWeapon(h: Long, side: Int, start: Int, end: Int, strength: Int): Boolean
    @JvmStatic external fun triggerVibration(h: Long, side: Int, pos: Int, amp: Int, hz: Int): Boolean
    @JvmStatic external fun triggerMultiFeedback(h: Long, side: Int, zones: ByteArray): Boolean
    @JvmStatic external fun triggerRaw(h: Long, side: Int, mode: Int, params: ByteArray?): Boolean
    @JvmStatic external fun setLightbar(h: Long, r: Int, g: Int, b: Int): Boolean
    @JvmStatic external fun setPlayerLeds(h: Long, mask: Int): Boolean
    @JvmStatic external fun setRumble(h: Long, strong: Int, weak: Int): Boolean
    @JvmStatic external fun triggerSlope(h: Long, side: Int, start: Int, end: Int, s0: Int, s1: Int): Boolean
    @JvmStatic external fun triggerBow(h: Long, side: Int, start: Int, end: Int, strength: Int, snap: Int): Boolean
    @JvmStatic external fun triggerGalloping(h: Long, side: Int, start: Int, end: Int, f1: Int, f2: Int, hz: Int): Boolean
    @JvmStatic external fun triggerMultiVibration(h: Long, side: Int, hz: Int, amps: ByteArray): Boolean
    @JvmStatic external fun triggerSimpleFeedback(h: Long, side: Int, pos: Int, strength: Int): Boolean
    @JvmStatic external fun triggerSimpleWeapon(h: Long, side: Int, start: Int, end: Int, strength: Int): Boolean
    @JvmStatic external fun triggerSimpleVibration(h: Long, side: Int, hz: Int, amp: Int, pos: Int): Boolean
    @JvmStatic external fun setPlayerLedBrightness(h: Long, level: Int): Boolean
    @JvmStatic external fun setMicLed(h: Long, mode: Int): Boolean
    @JvmStatic external fun buildReport(h: Long, out: ByteArray): Int
    @JvmStatic external fun markAllDirty(h: Long)
    @JvmStatic external fun hasPending(h: Long): Boolean
}

class DualSenseUsb(private val ctx: Context) {
    private val usb = ctx.getSystemService(Context.USB_SERVICE) as UsbManager
    private var conn: UsbDeviceConnection? = null
    private var iface: UsbInterface? = null
    private var epOut: UsbEndpoint? = null
    private var epIn: UsbEndpoint? = null
    val engine: Long = DualSenseNative.create(0)

    fun findDevice(): UsbDevice? = usb.deviceList.values.firstOrNull {
        DualSenseNative.isDualSense(it.vendorId, it.productId)
    }

    fun requestPermission(dev: UsbDevice, onResult: (Boolean) -> Unit) {
        if (usb.hasPermission(dev)) { onResult(true); return }
        val action = "${ctx.packageName}.USB_PERMISSION"
        ctx.registerReceiver(object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                c.unregisterReceiver(this)
                onResult(i.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false))
            }
        }, IntentFilter(action), if (Build.VERSION.SDK_INT >= 33) Context.RECEIVER_NOT_EXPORTED else 0)
        // Explicit package + IMMUTABLE flag (Android 14 requirement for implicit-looking intents).
        val intent = Intent(action).setPackage(ctx.packageName)
        usb.requestPermission(dev, PendingIntent.getBroadcast(ctx, 0, intent, PendingIntent.FLAG_IMMUTABLE))
    }

    fun open(dev: UsbDevice): Boolean {
        for (i in 0 until dev.interfaceCount) {
            val itf = dev.getInterface(i)
            if (itf.interfaceClass != UsbConstants.USB_CLASS_HID) continue
            var out: UsbEndpoint? = null; var inp: UsbEndpoint? = null
            for (e in 0 until itf.endpointCount) {
                val ep = itf.getEndpoint(e)
                if (ep.type != UsbConstants.USB_ENDPOINT_XFER_INT) continue
                if (ep.direction == UsbConstants.USB_DIR_OUT) out = ep else inp = ep
            }
            if (out == null) continue
            val c = usb.openDevice(dev) ?: return false
            if (!c.claimInterface(itf, true)) { c.close(); return false }
            conn = c; iface = itf; epOut = out; epIn = inp
            DualSenseNative.markAllDirty(engine)
            return true
        }
        return false
    }

    /** Builds and sends the pending report, if any. Call from a worker thread. */
    fun flush(): Boolean {
        val c = conn ?: return false
        val ep = epOut ?: return false
        if (!DualSenseNative.hasPending(engine)) return true
        val buf = ByteArray(78)
        val n = DualSenseNative.buildReport(engine, buf)
        return n > 0 && c.bulkTransfer(ep, buf, n, 50) == n   // interrupt OUT
    }

    /** Blocks up to timeoutMs; returns bytes read (USB input report id 0x01 is 64 bytes). */
    fun readInput(buf: ByteArray, timeoutMs: Int = 20): Int {
        val c = conn ?: return -1
        val ep = epIn ?: return -1
        return c.bulkTransfer(ep, buf, buf.size, timeoutMs)
    }

    fun close() {
        conn?.let { c -> iface?.let { c.releaseInterface(it) }; c.close() }
        conn = null
        DualSenseNative.destroy(engine)
    }
}
