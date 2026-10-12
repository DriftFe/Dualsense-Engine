#undef NDEBUG   /* keep assert() active in Release builds */
#include "dualsense_engine.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static void dump(const char* n, const uint8_t* b, size_t len) {
    printf("%s:", n);
    for (size_t i = 0; i < len; ++i) printf(" %02x", b[i]);
    printf("\n");
}

int main() {
    uint8_t r[DS_MAX_REPORT_SIZE];

    /* USB: weapon on right trigger, lightbar, player LEDs */
    ds_ctx* u = ds_create(DS_TRANSPORT_USB);
    assert(ds_trigger_weapon(u, DS_RIGHT, 2, 5, 8));
    assert(ds_set_lightbar(u, 10, 20, 30));
    assert(ds_set_player_leds(u, 0x04));
    assert(ds_build_report(u, r, sizeof r) == DS_USB_REPORT_SIZE);
    dump("usb", r, DS_USB_REPORT_SIZE);
    assert(r[0] == 0x02);
    assert(r[1] == 0x04);                       /* only right trigger */
    assert(r[2] == 0x14);                /* lightbar + player  */
    assert(r[11] == 0x25 && r[12] == 0x24 && r[13] == 0x00 && r[14] == 0x07);
    assert(r[39] == 0x02 && r[42] == 0x02);     /* lightbar setup     */
    assert(r[44] == 0x04);
    assert(r[45] == 10 && r[46] == 20 && r[47] == 30);
    assert(!ds_has_pending(u));
    /* second build carries no flags */
    ds_build_report(u, r, sizeof r);
    assert(r[1] == 0 && r[2] == 0 && r[39] == 0);
    ds_mark_all_dirty(u);
    ds_build_report(u, r, sizeof r);
    assert(r[1] == 0x04 && r[2] == 0x14 && r[39] == 0x02);

    /* feedback zones 3..9 strength 5 */
    assert(ds_trigger_feedback(u, DS_LEFT, 3, 5));
    ds_build_report(u, r, sizeof r);
    assert(r[22] == 0x21 && r[23] == 0xF8 && r[24] == 0x03);

    /* invalid args leave state alone */
    assert(!ds_trigger_weapon(u, DS_LEFT, 1, 5, 4));
    assert(!ds_trigger_feedback(u, DS_LEFT, 10, 1));
    assert(!ds_trigger_raw(u, DS_LEFT, 0xFC, 0, 0));
    assert(!ds_has_pending(u));

    /* BT */
    ds_ctx* b = ds_create(DS_TRANSPORT_BLUETOOTH);
    ds_trigger_vibration(b, DS_LEFT, 2, 6, 30);
    ds_set_rumble(b, 100, 50, 0);
    assert(ds_build_report(b, r, sizeof r) == DS_BT_REPORT_SIZE);
    dump("bt", r, DS_BT_REPORT_SIZE);
    assert(r[0] == 0x31 && r[1] == 0x00 && r[2] == 0x10);
    assert(r[3] == 0x0B);                       /* flag0: left trig + rumble */
    assert(r[3 + 3] == 100 && r[3 + 2] == 50);
    ds_build_report(b, r, sizeof r);
    assert(r[1] == 0x10);                       /* seq increments */
    assert(r[3] == 0x03);                       /* rumble stays armed */
    ds_destroy(u); ds_destroy(b);
    puts("OK");
}
