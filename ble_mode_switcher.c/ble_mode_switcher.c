#include <furi.h>
#include <furi_hal_bt.h>
#include <furi_hal_version.h>

#include <extra_profiles/hid_profile.h>
#include <bt/bt_service/bt.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>

#include <string.h>

typedef enum {
    BleModeSerial,
    BleModeHid,
    BleModeBeacon,
} BleMode;

typedef struct {
    Bt* bt;
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* submenu;
} BleModeSwitcher;

static void ble_mode_switcher_set_header(BleModeSwitcher* app, const char* mode) {
    FuriString* header = furi_string_alloc_printf("Flipper BLE: %s", mode);
    submenu_set_header(app->submenu, furi_string_get_cstr(header));
    furi_string_free(header);
}

static bool ble_mode_switcher_start_beacon(void) {
    GapExtraBeaconConfig config = {
        .min_adv_interval_ms = 50,
        .max_adv_interval_ms = 150,
        .adv_channel_map = GapAdvChannelMapAll,
        .adv_power_level = GapAdvPowerLevel_0dBm,
        .address_type = GapAddressTypePublic,
    };
    memcpy(config.address, furi_hal_version_get_ble_mac(), sizeof(config.address));
    config.address[0] ^= 0xFF;
    config.address[3] ^= 0xFF;

    static const uint8_t beacon_data[] = {'F', 'Z', ' ', 'W', 'i', 'F', 'i'};

    furi_hal_bt_extra_beacon_stop();
    return furi_hal_bt_extra_beacon_set_config(&config) &&
           furi_hal_bt_extra_beacon_set_data(beacon_data, sizeof(beacon_data)) &&
           furi_hal_bt_extra_beacon_start();
}

static bool ble_mode_switcher_set_mode(BleModeSwitcher* app, BleMode mode) {
    furi_hal_bt_extra_beacon_stop();
    bt_disconnect(app->bt);
    furi_delay_ms(200);

    if(mode == BleModeBeacon) {
        if(!bt_profile_restore_default(app->bt) || !ble_mode_switcher_start_beacon()) {
            return false;
        }
        ble_mode_switcher_set_header(app, "Beacon");
        return true;
    }

    if(mode == BleModeHid) {
        if(!bt_profile_start(app->bt, ble_profile_hid, NULL)) return false;
    } else {
        if(!bt_profile_restore_default(app->bt)) return false;
    }

    furi_hal_bt_start_advertising();
    ble_mode_switcher_set_header(app, mode == BleModeHid ? "HID" : "Serial");
    return true;
}

static void ble_mode_switcher_menu_callback(void* context, uint32_t index) {
    BleModeSwitcher* app = context;
    const bool success = ble_mode_switcher_set_mode(app, (BleMode)index);
    if(!success) {
        ble_mode_switcher_set_header(app, "Mode change failed");
    }
}

int32_t ble_mode_switcher_app(void* args) {
    UNUSED(args);

    BleModeSwitcher app = {
        .bt = furi_record_open(RECORD_BT),
        .gui = furi_record_open(RECORD_GUI),
        .dispatcher = view_dispatcher_alloc(),
        .submenu = submenu_alloc(),
    };

    view_dispatcher_attach_to_gui(app.dispatcher, app.gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app.dispatcher, 0, submenu_get_view(app.submenu));
    submenu_set_header(app.submenu, "Flipper BLE only");
    submenu_add_item(
        app.submenu, "BLE Serial", BleModeSerial, ble_mode_switcher_menu_callback, &app);
    submenu_add_item(app.submenu, "BLE HID", BleModeHid, ble_mode_switcher_menu_callback, &app);
    submenu_add_item(
        app.submenu, "BLE Beacon", BleModeBeacon, ble_mode_switcher_menu_callback, &app);
    view_dispatcher_switch_to_view(app.dispatcher, 0);

    view_dispatcher_run(app.dispatcher);

    furi_hal_bt_extra_beacon_stop();
    bt_disconnect(app.bt);
    furi_delay_ms(200);
    bt_profile_restore_default(app.bt);

    view_dispatcher_remove_view(app.dispatcher, 0);
    submenu_free(app.submenu);
    view_dispatcher_free(app.dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);
    return 0;
}
