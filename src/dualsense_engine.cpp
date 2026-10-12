#include "dualsense_engine.h"

#include <cstring>
#include <mutex>
#include <new>

namespace {

/* Offsets inside the 47-byte "common" block shared by the USB and BT reports
 * (struct dualsense_output_report_common in Linux hid-playstation). */
constexpr size_t COMMON_SIZE   = 47;
constexpr size_t C_FLAG0       = 0;
constexpr size_t C_FLAG1       = 1;
constexpr size_t C_MOTOR_RIGHT = 2;   /* weak / high-frequency   */
constexpr size_t C_MOTOR_LEFT  = 3;   /* strong / low-frequency  */
constexpr size_t C_MIC_LED     = 8;
constexpr size_t C_TRIG_RIGHT  = 10;  /* mode + 10 params = 11 bytes */
constexpr size_t C_TRIG_LEFT   = 21;
constexpr size_t C_FLAG2       = 38;
constexpr size_t C_LB_SETUP    = 41;
constexpr size_t C_LED_BRIGHT  = 42;
constexpr size_t C_PLAYER_LEDS = 43;
constexpr size_t C_LIGHTBAR    = 44;  /* r, g, b */
constexpr size_t TRIG_BLOCK    = 11;

/* valid_flag0 */
constexpr uint8_t F0_RUMBLE_COMPAT = 0x01;
constexpr uint8_t F0_HAPTICS       = 0x02;
constexpr uint8_t F0_TRIG_RIGHT    = 0x04;
constexpr uint8_t F0_TRIG_LEFT     = 0x08;
/* valid_flag1 */
constexpr uint8_t F1_MIC_LED       = 0x01;
constexpr uint8_t F1_LIGHTBAR      = 0x04;
constexpr uint8_t F1_PLAYER_LEDS   = 0x10;
/* valid_flag2 */
constexpr uint8_t F2_LED_BRIGHT    = 0x01;
constexpr uint8_t F2_LB_SETUP      = 0x02;
constexpr uint8_t F2_RUMBLE_V2     = 0x04;

constexpr uint8_t LIGHTBAR_SETUP_LIGHT_OUT = 0x02;

/* Trigger effect mode bytes */
constexpr uint8_t M_OFF = 0x05, M_FEEDBACK = 0x21, M_BOW = 0x22, M_GALLOP = 0x23,
                  M_WEAPON = 0x25, M_VIBRATION = 0x26;
constexpr uint8_t M_SIMPLE_FEEDBACK = 0x01, M_SIMPLE_WEAPON = 0x02, M_SIMPLE_VIBRATION = 0x06;

uint32_t crc32_ieee(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

}  // namespace

struct ds_ctx {
    std::mutex m;
    ds_transport transport;
    uint8_t seq = 0;
    uint8_t common[COMMON_SIZE] = {};
    uint8_t pending[3] = {};   /* flags to emit on the next report  */
    uint8_t touched[3] = {};   /* flags for everything ever set     */
    bool lightbar_ready = false;
    bool rumble_v2 = false;

    void mark(uint8_t f0, uint8_t f1, uint8_t f2) {
        pending[0] |= f0; pending[1] |= f1; pending[2] |= f2;
        touched[0] |= f0; touched[1] |= f1; touched[2] |= f2;
    }

    void set_trigger(ds_side side, uint8_t mode, const uint8_t* p, size_t n) {
        uint8_t* t = &common[side == DS_LEFT ? C_TRIG_LEFT : C_TRIG_RIGHT];
        std::memset(t, 0, TRIG_BLOCK);
        t[0] = mode;
        if (p && n) std::memcpy(t + 1, p, n > 10 ? 10 : n);
        mark(side == DS_LEFT ? F0_TRIG_LEFT : F0_TRIG_RIGHT, 0, 0);
    }
};

/* ---------- helpers (call with lock held) ---------- */
namespace {

int off(ds_ctx* c, ds_side s) {
    c->set_trigger(s, M_OFF, nullptr, 0);
    return 1;
}

/* Pack per-zone 3-bit values (value-1) and an active-zone bitmap. */
void pack_zones(const uint8_t v[10], uint16_t& active, uint32_t& packed) {
    active = 0; packed = 0;
    for (int i = 0; i < 10; ++i) {
        if (v[i] > 0) {
            packed |= (uint32_t)((v[i] - 1) & 0x07) << (3 * i);
            active |= (uint16_t)(1u << i);
        }
    }
}

int zoned(ds_ctx* c, ds_side s, uint8_t mode, const uint8_t v[10], uint8_t freq) {
    uint16_t active; uint32_t packed;
    pack_zones(v, active, packed);
    if (!active) return off(c, s);
    uint8_t p[10] = {
        (uint8_t)active, (uint8_t)(active >> 8),
        (uint8_t)packed, (uint8_t)(packed >> 8), (uint8_t)(packed >> 16), (uint8_t)(packed >> 24),
        0, 0, freq, 0
    };
    c->set_trigger(s, mode, p, 10);
    return 1;
}

}  // namespace

/* ---------- lifecycle ---------- */
extern "C" {

DS_API ds_ctx* ds_create(ds_transport t) {
    if (t != DS_TRANSPORT_USB && t != DS_TRANSPORT_BLUETOOTH) return nullptr;
    ds_ctx* c = new (std::nothrow) ds_ctx();
    if (c) c->transport = t;
    return c;
}

DS_API void ds_destroy(ds_ctx* c) { delete c; }

DS_API int ds_is_dualsense(uint16_t vid, uint16_t pid) {
    return vid == DS_VID_SONY && (pid == DS_PID_DUALSENSE || pid == DS_PID_DUALSENSE_EDGE);
}

/* ---------- official trigger effects ---------- */
DS_API int ds_trigger_off(ds_ctx* c, ds_side s) {
    if (!c) return 0;
    std::lock_guard<std::mutex> g(c->m);
    return off(c, s);
}

DS_API int ds_trigger_feedback(ds_ctx* c, ds_side s, uint8_t pos, uint8_t str) {
    if (!c || pos > 9 || str > 8) return 0;
    std::lock_guard<std::mutex> g(c->m);
    uint8_t v[10] = {};
    for (int i = pos; i < 10; ++i) v[i] = str;
    return zoned(c, s, M_FEEDBACK, v, 0);
}

DS_API int ds_trigger_weapon(ds_ctx* c, ds_side s, uint8_t start, uint8_t end, uint8_t str) {
    if (!c || start < 2 || start > 7 || end > 8 || end <= start || str > 8) return 0;
    std::lock_guard<std::mutex> g(c->m);
    if (!str) return off(c, s);
    uint16_t zones = (uint16_t)((1u << start) | (1u << end));
    uint8_t p[3] = {(uint8_t)zones, (uint8_t)(zones >> 8), (uint8_t)(str - 1)};
    c->set_trigger(s, M_WEAPON, p, 3);
    return 1;
}

DS_API int ds_trigger_vibration(ds_ctx* c, ds_side s, uint8_t pos, uint8_t amp, uint8_t freq) {
    if (!c || pos > 9 || amp > 8) return 0;
    std::lock_guard<std::mutex> g(c->m);
    if (!amp || !freq) return off(c, s);
    uint8_t v[10] = {};
    for (int i = pos; i < 10; ++i) v[i] = amp;
    return zoned(c, s, M_VIBRATION, v, freq);
}

DS_API int ds_trigger_multi_feedback(ds_ctx* c, ds_side s, const uint8_t str[10]) {
    if (!c || !str) return 0;
    for (int i = 0; i < 10; ++i) if (str[i] > 8) return 0;
    std::lock_guard<std::mutex> g(c->m);
    return zoned(c, s, M_FEEDBACK, str, 0);
}

DS_API int ds_trigger_slope_feedback(ds_ctx* c, ds_side s, uint8_t start, uint8_t end,
                                     uint8_t s0, uint8_t s1) {
    if (!c || start > 8 || end > 9 || end <= start || s0 < 1 || s0 > 8 || s1 < 1 || s1 > 8) return 0;
    uint8_t v[10] = {};
    float slope = (float)((int)s1 - (int)s0) / (float)(end - start);
    for (int i = start; i < 10; ++i) {
        float x = (i <= end) ? (float)s0 + slope * (float)(i - start) : (float)s1;
        v[i] = (uint8_t)(x + 0.5f);   /* values are positive: round half up */
    }
    std::lock_guard<std::mutex> g(c->m);
    return zoned(c, s, M_FEEDBACK, v, 0);
}

DS_API int ds_trigger_multi_vibration(ds_ctx* c, ds_side s, uint8_t freq, const uint8_t amp[10]) {
    if (!c || !amp) return 0;
    for (int i = 0; i < 10; ++i) if (amp[i] > 8) return 0;
    std::lock_guard<std::mutex> g(c->m);
    if (!freq) return off(c, s);
    return zoned(c, s, M_VIBRATION, amp, freq);
}

/* ---------- unofficial effects ---------- */
DS_API int ds_trigger_bow(ds_ctx* c, ds_side s, uint8_t start, uint8_t end, uint8_t str, uint8_t snap) {
    if (!c || start > 8 || end > 8 || start >= end || str > 8 || snap > 8) return 0;
    std::lock_guard<std::mutex> g(c->m);
    if (!(end > 0 && str > 0 && snap > 0)) return off(c, s);
    uint16_t zones = (uint16_t)((1u << start) | (1u << end));
    uint32_t pair = (uint32_t)(((str - 1) & 7) | (((snap - 1) & 7) << 3));
    uint8_t p[5] = {(uint8_t)zones, (uint8_t)(zones >> 8), (uint8_t)pair, (uint8_t)(pair >> 8), 0};
    c->set_trigger(s, M_BOW, p, 4);
    return 1;
}

DS_API int ds_trigger_galloping(ds_ctx* c, ds_side s, uint8_t start, uint8_t end,
                                uint8_t first, uint8_t second, uint8_t freq) {
    if (!c || start > 8 || end > 9 || start >= end || second > 7 || first > 6 || first >= second) return 0;
    std::lock_guard<std::mutex> g(c->m);
    if (!freq) return off(c, s);
    uint16_t zones = (uint16_t)((1u << start) | (1u << end));
    uint32_t ratio = (uint32_t)((second & 7) | ((first & 7) << 3));
    uint8_t p[4] = {(uint8_t)zones, (uint8_t)(zones >> 8), (uint8_t)ratio, freq};
    c->set_trigger(s, M_GALLOP, p, 4);
    return 1;
}

/* ---------- legacy effects ---------- */
DS_API int ds_trigger_simple_feedback(ds_ctx* c, ds_side s, uint8_t pos, uint8_t str) {
    if (!c) return 0;
    std::lock_guard<std::mutex> g(c->m);
    uint8_t p[2] = {pos, str};
    c->set_trigger(s, M_SIMPLE_FEEDBACK, p, 2);
    return 1;
}

DS_API int ds_trigger_simple_weapon(ds_ctx* c, ds_side s, uint8_t start, uint8_t end, uint8_t str) {
    if (!c) return 0;
    std::lock_guard<std::mutex> g(c->m);
    uint8_t p[3] = {start, end, str};
    c->set_trigger(s, M_SIMPLE_WEAPON, p, 3);
    return 1;
}

DS_API int ds_trigger_simple_vibration(ds_ctx* c, ds_side s, uint8_t freq, uint8_t amp, uint8_t pos) {
    if (!c) return 0;
    std::lock_guard<std::mutex> g(c->m);
    uint8_t p[3] = {freq, amp, pos};
    c->set_trigger(s, M_SIMPLE_VIBRATION, p, 3);
    return 1;
}

DS_API int ds_trigger_raw(ds_ctx* c, ds_side s, uint8_t mode, const uint8_t* params, size_t len) {
    if (!c || len > 10 || (len && !params) || (mode >= 0xFC && mode <= 0xFE)) return 0;
    std::lock_guard<std::mutex> g(c->m);
    c->set_trigger(s, mode, params, len);
    return 1;
}

/* ---------- lights ---------- */
DS_API int ds_set_lightbar(ds_ctx* c, uint8_t r, uint8_t g_, uint8_t b) {
    if (!c) return 0;
    std::lock_guard<std::mutex> g(c->m);
    c->common[C_LIGHTBAR] = r; c->common[C_LIGHTBAR + 1] = g_; c->common[C_LIGHTBAR + 2] = b;
    uint8_t f2 = 0;
    if (!c->lightbar_ready) {            /* one-shot: stop the startup lightbar animation */
        c->common[C_LB_SETUP] = LIGHTBAR_SETUP_LIGHT_OUT;
        f2 = F2_LB_SETUP;
        c->lightbar_ready = true;
    }
    c->mark(0, F1_LIGHTBAR, f2);
    return 1;
}

DS_API int ds_set_player_leds(ds_ctx* c, uint8_t mask) {
    if (!c || mask > 0x1F) return 0;
    std::lock_guard<std::mutex> g(c->m);
    c->common[C_PLAYER_LEDS] = mask;
    c->mark(0, F1_PLAYER_LEDS, 0);
    return 1;
}

DS_API int ds_set_player_led_brightness(ds_ctx* c, ds_brightness b) {
    if (!c || b > DS_BRIGHTNESS_LOW) return 0;
    std::lock_guard<std::mutex> g(c->m);
    c->common[C_LED_BRIGHT] = (uint8_t)b;
    c->mark(0, 0, F2_LED_BRIGHT);
    return 1;
}

DS_API int ds_set_mic_led(ds_ctx* c, uint8_t mode) {
    if (!c || mode > 2) return 0;
    std::lock_guard<std::mutex> g(c->m);
    c->common[C_MIC_LED] = mode;
    c->mark(0, F1_MIC_LED, 0);
    return 1;
}

/* ---------- rumble ---------- */
DS_API int ds_set_rumble(ds_ctx* c, uint8_t strong, uint8_t weak, int v2) {
    if (!c) return 0;
    std::lock_guard<std::mutex> g(c->m);
    c->common[C_MOTOR_LEFT] = strong;
    c->common[C_MOTOR_RIGHT] = weak;
    c->rumble_v2 = v2 != 0;
    c->mark(F0_RUMBLE_COMPAT | F0_HAPTICS, 0, c->rumble_v2 ? F2_RUMBLE_V2 : 0);
    return 1;
}

/* ---------- output ---------- */
DS_API size_t ds_report_size(ds_ctx* c) {
    if (!c) return 0;
    return c->transport == DS_TRANSPORT_USB ? DS_USB_REPORT_SIZE : DS_BT_REPORT_SIZE;
}

DS_API size_t ds_build_report(ds_ctx* c, uint8_t* out, size_t cap) {
    if (!c || !out) return 0;
    std::lock_guard<std::mutex> g(c->m);
    const size_t size = c->transport == DS_TRANSPORT_USB ? DS_USB_REPORT_SIZE : DS_BT_REPORT_SIZE;
    if (cap < size) return 0;

    uint8_t common[COMMON_SIZE];
    std::memcpy(common, c->common, COMMON_SIZE);
    common[C_FLAG0] = c->pending[0];
    common[C_FLAG1] = c->pending[1];
    common[C_FLAG2] = c->pending[2];

    std::memset(out, 0, size);
    if (c->transport == DS_TRANSPORT_USB) {
        out[0] = 0x02;
        std::memcpy(out + 1, common, COMMON_SIZE);
    } else {
        out[0] = 0x31;
        out[1] = (uint8_t)(c->seq << 4);
        c->seq = (uint8_t)((c->seq + 1) & 0x0F);
        out[2] = 0x10;                                   /* DS_OUTPUT_TAG */
        std::memcpy(out + 3, common, COMMON_SIZE);
        const uint8_t seed = 0xA2;                       /* BT output CRC seed */
        uint32_t crc = crc32_ieee(&seed, 1);
        crc = ~crc32_ieee(out, size - 4, crc);
        out[size - 4] = (uint8_t)crc;
        out[size - 3] = (uint8_t)(crc >> 8);
        out[size - 2] = (uint8_t)(crc >> 16);
        out[size - 1] = (uint8_t)(crc >> 24);
    }

    c->pending[0] = c->pending[1] = c->pending[2] = 0;
    if (c->common[C_MOTOR_LEFT] || c->common[C_MOTOR_RIGHT]) {   /* keep rumble alive */
        c->pending[0] |= F0_RUMBLE_COMPAT | F0_HAPTICS;
        if (c->rumble_v2) c->pending[2] |= F2_RUMBLE_V2;
    }
    return size;
}

DS_API void ds_mark_all_dirty(ds_ctx* c) {
    if (!c) return;
    std::lock_guard<std::mutex> g(c->m);
    for (int i = 0; i < 3; ++i) c->pending[i] = c->touched[i];
}

DS_API int ds_has_pending(ds_ctx* c) {
    if (!c) return 0;
    std::lock_guard<std::mutex> g(c->m);
    return (c->pending[0] | c->pending[1] | c->pending[2]) != 0;
}

}
