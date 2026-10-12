// JNI bindings: com.dualsense.engine.DualSenseNative
#include <jni.h>
#include "dualsense_engine.h"

#define JNI(ret, name) extern "C" JNIEXPORT ret JNICALL Java_com_dualsense_engine_DualSenseNative_##name
static inline ds_ctx* H(jlong h) { return reinterpret_cast<ds_ctx*>(h); }
static inline ds_side S(jint s) { return s ? DS_RIGHT : DS_LEFT; }

JNI(jlong, create)(JNIEnv*, jclass, jint transport) {
    return reinterpret_cast<jlong>(ds_create(static_cast<ds_transport>(transport)));
}
JNI(void, destroy)(JNIEnv*, jclass, jlong h) { ds_destroy(H(h)); }
JNI(jboolean, isDualSense)(JNIEnv*, jclass, jint vid, jint pid) {
    return ds_is_dualsense((uint16_t)vid, (uint16_t)pid) ? JNI_TRUE : JNI_FALSE;
}

JNI(jboolean, triggerOff)(JNIEnv*, jclass, jlong h, jint s) { return ds_trigger_off(H(h), S(s)); }
JNI(jboolean, triggerFeedback)(JNIEnv*, jclass, jlong h, jint s, jint pos, jint str) {
    return ds_trigger_feedback(H(h), S(s), (uint8_t)pos, (uint8_t)str);
}
JNI(jboolean, triggerWeapon)(JNIEnv*, jclass, jlong h, jint s, jint a, jint b, jint str) {
    return ds_trigger_weapon(H(h), S(s), (uint8_t)a, (uint8_t)b, (uint8_t)str);
}
JNI(jboolean, triggerVibration)(JNIEnv*, jclass, jlong h, jint s, jint pos, jint amp, jint hz) {
    return ds_trigger_vibration(H(h), S(s), (uint8_t)pos, (uint8_t)amp, (uint8_t)hz);
}
JNI(jboolean, triggerMultiFeedback)(JNIEnv* env, jclass, jlong h, jint s, jbyteArray zones) {
    if (!zones || env->GetArrayLength(zones) != 10) return JNI_FALSE;
    uint8_t z[10];
    env->GetByteArrayRegion(zones, 0, 10, reinterpret_cast<jbyte*>(z));
    return ds_trigger_multi_feedback(H(h), S(s), z);
}
JNI(jboolean, triggerRaw)(JNIEnv* env, jclass, jlong h, jint s, jint mode, jbyteArray params) {
    uint8_t p[10] = {};
    jsize n = params ? env->GetArrayLength(params) : 0;
    if (n > 10) return JNI_FALSE;
    if (n) env->GetByteArrayRegion(params, 0, n, reinterpret_cast<jbyte*>(p));
    return ds_trigger_raw(H(h), S(s), (uint8_t)mode, p, (size_t)n);
}

JNI(jboolean, setLightbar)(JNIEnv*, jclass, jlong h, jint r, jint g, jint b) {
    return ds_set_lightbar(H(h), (uint8_t)r, (uint8_t)g, (uint8_t)b);
}
JNI(jboolean, setPlayerLeds)(JNIEnv*, jclass, jlong h, jint mask) {
    return mask >= 0 && mask <= 0x1F && ds_set_player_leds(H(h), (uint8_t)mask);
}
JNI(jboolean, setRumble)(JNIEnv*, jclass, jlong h, jint strong, jint weak) {
    return ds_set_rumble(H(h), (uint8_t)strong, (uint8_t)weak, 0);
}

JNI(jint, buildReport)(JNIEnv* env, jclass, jlong h, jbyteArray out) {
    uint8_t buf[DS_MAX_REPORT_SIZE];
    size_t n = ds_build_report(H(h), buf, sizeof buf);
    if (!n || env->GetArrayLength(out) < (jsize)n) return 0;
    env->SetByteArrayRegion(out, 0, (jsize)n, reinterpret_cast<jbyte*>(buf));
    return (jint)n;
}
JNI(void, markAllDirty)(JNIEnv*, jclass, jlong h) { ds_mark_all_dirty(H(h)); }
JNI(jboolean, hasPending)(JNIEnv*, jclass, jlong h) { return ds_has_pending(H(h)) ? JNI_TRUE : JNI_FALSE; }

JNI(jboolean, triggerSlope)(JNIEnv*, jclass, jlong h, jint s, jint a, jint b, jint s0, jint s1) {
    return ds_trigger_slope_feedback(H(h), S(s), (uint8_t)a, (uint8_t)b, (uint8_t)s0, (uint8_t)s1);
}
JNI(jboolean, triggerBow)(JNIEnv*, jclass, jlong h, jint s, jint a, jint b, jint str, jint snap) {
    return ds_trigger_bow(H(h), S(s), (uint8_t)a, (uint8_t)b, (uint8_t)str, (uint8_t)snap);
}
JNI(jboolean, triggerGalloping)(JNIEnv*, jclass, jlong h, jint s, jint a, jint b, jint f1, jint f2, jint hz) {
    return ds_trigger_galloping(H(h), S(s), (uint8_t)a, (uint8_t)b, (uint8_t)f1, (uint8_t)f2, (uint8_t)hz);
}
JNI(jboolean, triggerMultiVibration)(JNIEnv* env, jclass, jlong h, jint s, jint hz, jbyteArray amps) {
    if (!amps || env->GetArrayLength(amps) != 10) return JNI_FALSE;
    uint8_t z[10];
    env->GetByteArrayRegion(amps, 0, 10, reinterpret_cast<jbyte*>(z));
    return ds_trigger_multi_vibration(H(h), S(s), (uint8_t)hz, z);
}
JNI(jboolean, triggerSimpleFeedback)(JNIEnv*, jclass, jlong h, jint s, jint pos, jint str) {
    return ds_trigger_simple_feedback(H(h), S(s), (uint8_t)pos, (uint8_t)str);
}
JNI(jboolean, triggerSimpleWeapon)(JNIEnv*, jclass, jlong h, jint s, jint a, jint b, jint str) {
    return ds_trigger_simple_weapon(H(h), S(s), (uint8_t)a, (uint8_t)b, (uint8_t)str);
}
JNI(jboolean, triggerSimpleVibration)(JNIEnv*, jclass, jlong h, jint s, jint hz, jint amp, jint pos) {
    return ds_trigger_simple_vibration(H(h), S(s), (uint8_t)hz, (uint8_t)amp, (uint8_t)pos);
}
JNI(jboolean, setPlayerLedBrightness)(JNIEnv*, jclass, jlong h, jint b) {
    return ds_set_player_led_brightness(H(h), (ds_brightness)b);
}
JNI(jboolean, setMicLed)(JNIEnv*, jclass, jlong h, jint mode) {
    return ds_set_mic_led(H(h), (uint8_t)mode);
}
