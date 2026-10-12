<div align="center">

# ♡ DualSense Engine ♡

### `A tiny, no-dependency C++17 engine for making your DualSense feel a little more ✧ magical ✧`

`୨୧` ─────────────── `୨୧`

</div>

Adaptive triggers, lightbar, player LEDs and rumble for the ****Sony DualSense and DualSense Edge****, as a small

C++17 library with a plain C API, plus a ready-made ****Android USB binding****.

Most DualSense trigger tools target Windows and Linux and rely on ****hidapi****, ****hidraw**** or ****SDL****, none of which a normal

Android app can use. This library builds the controller's output reports itself and ships an Android binding

(JNI + Kotlin) that delivers them through `UsbManager`.

> ****Status:**** the report builder is covered by unit tests, including a Bluetooth CRC32 check against an
independent implementation, and the Android binding compiles and has been built into a test app. Behaviour of
each effect on real hardware is still being verified, so please open an issue with what you observe. The
unofficial effects (bow, galloping) and the legacy effects may differ between firmware versions.

## ✧ Features**

- Builds USB (`0x02`, 63 bytes) and Bluetooth (`0x31`, 78 bytes with sequence number and CRC32) output reports

- Every adaptive trigger effect, with validation: invalid arguments return `0` and leave state untouched

- Only changed fields are flagged as valid, so untouched features are never overwritten with zeros

- Independent state per controller (opaque `ds_ctx` handle), thread-safe per context

- No dependencies. The core is two files and does no I/O, so it works with any transport

- Android: JNI bindings and a Kotlin helper that finds the controller, requests permission, claims the HID

  interface, sends reports and reads input

### ♡ Trigger effects**

| Function | Parameters |

| `ds_trigger_feedback` | start zone 0-9, strength 0-8 |

| `ds_trigger_weapon` | start 2-7, end (start+1)-8, strength 0-8 |

| `ds_trigger_vibration` | start zone 0-9, amplitude 0-8, frequency in Hz (> 0) |

| `ds_trigger_multi_feedback` | strength 0-8 for each of 10 zones |

| `ds_trigger_slope_feedback` | start 0-8, end (start+1)-9, start and end strength 1-8 |

| `ds_trigger_multi_vibration` | shared frequency, amplitude 0-8 for each of 10 zones |

| `ds_trigger_bow` **(unofficial)** | start 0-8, end (start+1)-8, strength 0-8, snap 0-8 |

| `ds_trigger_galloping` **(unofficial)** | start 0-8, end 1-9, first foot 0-6, second foot 1-7, frequency |

| `ds_trigger_simple_feedback`, `_weapon`, `_vibration` **(legacy)** | raw bytes 0-255, no safety checks |

| `ds_trigger_raw` | any mode byte plus up to 10 parameter bytes (rejects `0xFC`-`0xFE`) |

| `ds_trigger_off` | releases the trigger |

Parameters that mean "no effect" (strength or amplitude 0, for example) produce an Off effect.

Zones 0-9 describe how far the trigger is pulled.

## ✿ Quick start (C/C++)**

```c

#include "dualsense_engine.h"

ds_ctx* c = ds_create(DS_TRANSPORT_USB);

ds_trigger_weapon(c, DS_RIGHT, 4, 6, 8);     // click between zones 4 and 6

ds_trigger_feedback(c, DS_LEFT, 3, 5);       // steady resistance past zone 3

ds_set_lightbar(c, 255, 0, 128);

uint8_t report[DS_MAX_REPORT_SIZE];

size_t n = ds_build_report(c, report, sizeof report);   // 63 bytes for USB

/* write report[0..n) to the controller's interrupt OUT endpoint */

ds_destroy(c);

```

How the output model works:

- `ds_build_report()` consumes the pending flags, so call it only when you intend to send the result.

- `ds_has_pending()` tells you whether there is anything to send.

- After a reconnect, call `ds_mark_all_dirty()` to resend everything you ever set.

- Rumble is re-sent automatically while either motor is non-zero, because the controller times it out on its own.

- Rumble "v2" (`ds_set_rumble(..., v2 = 1)`) selects the revised rumble feel on newer firmware. It is off by

  default because the library does not read the controller's firmware version.

## ✿ Quick start (Android)**

Requirements: Android 8.0 (API 26) or newer with USB host support, a data-capable USB-C cable or OTG adapter,

and the NDK with CMake.

****1. Build the native library.**** In your module's `build.gradle.kts`, point CMake at this repo's `CMakeLists.txt`

(copy or submodule the repo into your project):

```kotlin

android {

    defaultConfig {

        ndk { abiFilters += listOf("arm64-v8a", "armeabi-v7a") }

    }

    externalNativeBuild {

        cmake { path = file("../dualsense-engine/CMakeLists.txt") }

    }

}

```

On Android the build automatically includes `bindings/android/dualsense_jni.cpp` and applies the 16 KB page-size

linker flag needed on Android 15+ devices.

****2. Add the Kotlin helper.**** Copy `bindings/android/DualSense.kt` into

`src/main/java/com/dualsense/engine/`. It declares `DualSenseNative` (the JNI facade) and `DualSenseUsb`.

****3. Declare USB host support**** in `AndroidManifest.xml`:

```xml

<uses-feature android:name="android.hardware.usb.host" android:required="true" />

```

****4. Use it.**** Sides are `0` for left (L2) and `1` for right (R2):

```kotlin

val usb = DualSenseUsb(context)

val dev = usb.findDevice() ?: return          // no DualSense on USB host

usb.requestPermission(dev) { granted ->

    if (granted && usb.open(dev)) {

        DualSenseNative.triggerWeapon(usb.engine, 1, 4, 6, 8)

        DualSenseNative.setLightbar(usb.engine, 255, 0, 128)

        Thread {

            val input = ByteArray(64)

            while (running) {

                usb.readInput(input, 15)      // USB input report 0x01

                usb.flush()                   // sends pending changes, if any

            }

        }.start()

    }

}

```

Call `usb.flush()` from a worker thread, never the UI thread. Call `usb.close()` when you are done.

****Android caveats****

- Android apps cannot send Bluetooth HID output reports through public APIs, so use the USB transport.

- `DualSenseUsb.open()` calls `claimInterface(..., force = true)`, which detaches Android's own gamepad driver.

  While your app holds the interface the controller stops working as a normal gamepad in other apps and in your

  game's regular input path. Read input yourself with `readInput()` and forward it, and unplug and replug the

  controller to hand it back to Android.

- The USB permission prompt is shown by Android and must be accepted by the user.

## ♡ API reference**

All setters return `1` on success and `0` on invalid arguments, with state unchanged.

| Group | Functions |

|---|---|

| Lifecycle | `ds_create(transport)`, `ds_destroy(ctx)`, `ds_is_dualsense(vid, pid)` |

| Triggers | the `ds_trigger_*` functions listed above (`ds_side` is `DS_LEFT` or `DS_RIGHT`) |

| Lights | `ds_set_lightbar(r, g, b)`, `ds_set_player_leds(mask5)`, `ds_set_player_led_brightness(level)`, `ds_set_mic_led(mode)` |

| Rumble | `ds_set_rumble(left_strong, right_weak, v2)` |

| Output | `ds_build_report(ctx, out, cap)`, `ds_report_size(ctx)`, `ds_has_pending(ctx)`, `ds_mark_all_dirty(ctx)` |

Constants: `DS_VID_SONY`, `DS_PID_DUALSENSE` (`0x0CE6`), `DS_PID_DUALSENSE_EDGE` (`0x0DF2`),

`DS_USB_REPORT_SIZE` (63), `DS_BT_REPORT_SIZE` (78), `DS_MAX_REPORT_SIZE`.

Player LED brightness levels: `DS_BRIGHTNESS_HIGH`, `_MEDIUM`, `_LOW`. Mic LED modes: 0 off, 1 on, 2 pulse.

Full signatures and parameter ranges are in [`include/dualsense_engine.h`](include/dualsense_engine.h).

## ✧ Building and testing**

Requires a C++17 compiler and CMake 3.18+.

```bash

cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build

ctest --test-dir build --output-on-failure

```

CMake options: `-DDS_BUILD_SHARED=OFF` for a static library, `-DDS_BUILD_TESTS=OFF` to skip the tests.

Without CMake, add `src/dualsense_engine.cpp` and `include/dualsense_engine.h` to any project, or run just the

tests:

```bash

g++ -std=c++17 -Wall -Wextra -Iinclude src/dualsense_engine.cpp tests/test_engine.cpp -o ds_test && ./ds_test

```

The tests need no hardware. They check report layout and flags, effect encoding, parameter validation,

Bluetooth framing and CRC32.

## ♡ How it works**

```

 your code (C, C++, Kotlin, ...)

        |

        v

 C++ engine  -----------------  builds the output report bytes

        |

        v

 your transport  -------------  Android UsbManager, hidapi, libusb, ...

        |

        v

 DualSense controller

```

The USB output report (offsets include the report ID at byte 0):

| Offset | Content |

|---|---|

| 0 | Report ID `0x02` |

| 1 | Valid flags 0: rumble `0x01` and `0x02`, right trigger `0x04`, left trigger `0x08` |

| 2 | Valid flags 1: mic LED `0x01`, lightbar `0x04`, player LEDs `0x10` |

| 3 / 4 | Right (weak) / left (strong) rumble motor |

| 9 | Mic LED mode |

| 11-21 | Right trigger: mode byte plus 10 parameter bytes |

| 22-32 | Left trigger: mode byte plus 10 parameter bytes |

| 39 | Valid flags 2: LED brightness `0x01`, lightbar setup `0x02`, rumble v2 `0x04` |

| 42 | Lightbar setup |

| 43 | Player LED brightness |

| 44 | Player LED mask |

| 45-47 | Lightbar R, G, B |

The valid flags tell the controller which fields to apply, which is why changing one feature never overwrites

another.

Official trigger effects use mode bytes `0x21` (feedback), `0x25` (weapon), `0x26` (vibration), `0x22` (bow) and

`0x23` (galloping). Parameters are packed as a 10-bit zone bitmask followed by 3-bit values. For example, weapon

with start 4, end 6, strength 8 produces `25 50 00 07`.

The Bluetooth report wraps the same 47-byte block: `0x31`, a sequence nibble, tag `0x10`, the block, padding,

and a little-endian CRC32 over `0xA2` plus the preceding 74 bytes.

## ⚠ Known limitations**

- Sony does not document these reports, so some valid ranges (the real maximum vibration frequency, for example)

  are only known by testing. The engine rejects combinations known to be invalid and passes the rest.

- The "Machine" trigger effect is not implemented, since its byte layout could not be verified.

- The library does not parse input reports or read the controller's firmware version.

- On Android, only the USB transport is available, and the controller is detached from Android's gamepad driver

  while your app holds it (see above).

## ❀ Troubleshooting**

| Symptom | Likely cause |

|---|---|

| `findDevice()` returns null | Charge-only cable, no OTG support, or the controller is only connected over Bluetooth |

| Permission callback gives `false` | The user dismissed the Android prompt; request permission again |

| `open()` returns false | Android refused to release the interface; unplug, replug and retry |

| No input from `readInput()` | Cable or power problem; replug the controller |

| An effect does nothing | The setter returned `0` (invalid parameters), or the effect is unofficial or legacy and your firmware ignores it |

## ♡ Credits**

Report layout and flag bits were checked against the Linux `hid-playstation` driver. Trigger effect encodings

follow Nielk1's DualSense trigger effect generator. Thanks to Nielk1 and the wider community for documenting

this protocol.

This project is not affiliated with or endorsed by Sony Interactive Entertainment. "DualSense" and

"PlayStation" are trademarks of Sony Interactive Entertainment... its just a fun thing I made cuz I was bored :p
