#ifndef DUALSENSE_ENGINE_H
#define DUALSENSE_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  define DS_API __declspec(dllexport)
#else
#  define DS_API __attribute__((visibility("default")))
#endif

#define DS_VID_SONY            0x054C
#define DS_PID_DUALSENSE       0x0CE6
#define DS_PID_DUALSENSE_EDGE  0x0DF2

#define DS_USB_REPORT_SIZE     63   /* report id 0x02 */
#define DS_BT_REPORT_SIZE      78   /* report id 0x31, CRC32 trailer */
#define DS_MAX_REPORT_SIZE     78

typedef enum { DS_TRANSPORT_USB = 0, DS_TRANSPORT_BLUETOOTH = 1 } ds_transport;
typedef enum { DS_LEFT = 0, DS_RIGHT = 1 } ds_side;
typedef enum { DS_BRIGHTNESS_HIGH = 0, DS_BRIGHTNESS_MEDIUM = 1, DS_BRIGHTNESS_LOW = 2 } ds_brightness;

typedef struct ds_ctx ds_ctx;

DS_API ds_ctx* ds_create(ds_transport transport);
DS_API void    ds_destroy(ds_ctx* ctx);
DS_API int     ds_is_dualsense(uint16_t vid, uint16_t pid);

/* ---- Adaptive triggers: official effects (zones 0-9, strength 0-8) ---- */
DS_API int ds_trigger_off(ds_ctx*, ds_side);
/* Resists beyond `position`. position 0-9, strength 0-8. */
DS_API int ds_trigger_feedback(ds_ctx*, ds_side, uint8_t position, uint8_t strength);
/* Click between start and end. start 2-7, end start+1..8, strength 0-8. */
DS_API int ds_trigger_weapon(ds_ctx*, ds_side, uint8_t start, uint8_t end, uint8_t strength);
/* Buzz beyond `position`. position 0-9, amplitude 0-8, frequency in Hz (>0). */
DS_API int ds_trigger_vibration(ds_ctx*, ds_side, uint8_t position, uint8_t amplitude, uint8_t frequency);
/* strength[10]: per-zone resistance 0-8. */
DS_API int ds_trigger_multi_feedback(ds_ctx*, ds_side, const uint8_t strength[10]);
/* Linear ramp. start 0-8, end start+1..9, strengths 1-8. */
DS_API int ds_trigger_slope_feedback(ds_ctx*, ds_side, uint8_t start, uint8_t end,
                                     uint8_t start_strength, uint8_t end_strength);
/* amplitude[10]: per-zone 0-8, one shared frequency (Hz). */
DS_API int ds_trigger_multi_vibration(ds_ctx*, ds_side, uint8_t frequency, const uint8_t amplitude[10]);

/* ---- Unofficial effects left in firmware (may vanish in future firmware) ---- */
DS_API int ds_trigger_bow(ds_ctx*, ds_side, uint8_t start, uint8_t end, uint8_t strength, uint8_t snap);
DS_API int ds_trigger_galloping(ds_ctx*, ds_side, uint8_t start, uint8_t end,
                                uint8_t first_foot, uint8_t second_foot, uint8_t frequency);

/* ---- Legacy full-byte effects (no parameter protection; raw 0-255) ---- */
DS_API int ds_trigger_simple_feedback(ds_ctx*, ds_side, uint8_t position, uint8_t strength);
DS_API int ds_trigger_simple_weapon(ds_ctx*, ds_side, uint8_t start, uint8_t end, uint8_t strength);
DS_API int ds_trigger_simple_vibration(ds_ctx*, ds_side, uint8_t frequency, uint8_t amplitude, uint8_t position);

/* Escape hatch: mode byte + up to 10 parameter bytes. Rejects 0xFC-0xFE
 * (debug/calibration modes that can wedge the trigger until reset). */
DS_API int ds_trigger_raw(ds_ctx*, ds_side, uint8_t mode, const uint8_t* params, size_t len);

/* ---- Lights ---- */
DS_API int ds_set_lightbar(ds_ctx*, uint8_t r, uint8_t g, uint8_t b);
DS_API int ds_set_player_leds(ds_ctx*, uint8_t mask5);          /* bits 0-4 */
DS_API int ds_set_player_led_brightness(ds_ctx*, ds_brightness);
DS_API int ds_set_mic_led(ds_ctx*, uint8_t mode);               /* 0 off, 1 on, 2 pulse */

/* ---- Rumble (classic compatible mode) ----
 * Resend periodically: the controller times rumble out on its own.
 * `v2` selects the revised rumble feel on newer firmware (Linux enables it
 * only after checking the firmware version); leave 0 unless you do the same. */
DS_API int ds_set_rumble(ds_ctx*, uint8_t left_strong, uint8_t right_weak, int v2);

/* ---- Output ---- */
/* Writes the next report to `out`. Returns bytes written, 0 if cap too small.
 * Clears one-shot flags, so call it only when you intend to send the result. */
DS_API size_t ds_build_report(ds_ctx*, uint8_t* out, size_t cap);
/* Re-arm every field you ever set (use after a reconnect). */
DS_API void   ds_mark_all_dirty(ds_ctx*);
/* True if there is anything to send. */
DS_API int    ds_has_pending(ds_ctx*);
DS_API size_t ds_report_size(ds_ctx*);

#ifdef __cplusplus
}
#endif
#endif /* DUALSENSE_ENGINE_H */
