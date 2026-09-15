# GMBluetooth Demo — Implementation Guide (Classic + BLE)

This is a reference document, not applied code. It lists every fix and code
block needed to finish the demo project you're building in the GameMaker IDE.
Nothing here has been written into the project — copy/paste what you need
directly in the IDE. `tm_bt_gmrt` / `tm_bt_gmrt_calssic` are not touched by
any of this; all logic below is fresh, for the new `obj_bt_*` objects.

## 1. Fixes needed in the IDE (do these first)

1. **Menu routing bug** — room `rm_bt_menu`, the Classic **Server** button
   instance currently has creation code:
   ```gml
   text = "Server"
   goto = rm_bt_classic_client   // wrong — should be rm_bt_classic_server
   ```
   Change `goto` to `rm_bt_classic_server`. Right now both Classic buttons
   go to the Client room.

2. **Missing controller instances** — neither `rm_bt_classic_server` nor
   `rm_bt_classic_client` currently has an instance of its controller
   object placed in the room (each only has the `obj_button_goto` back
   button). Drag one `obj_bt_classic_server` instance into
   `rm_bt_classic_server`, and one `obj_bt_classic_client` instance into
   `rm_bt_classic_client`. Without this, entering either room does nothing.

3. **Add the Async - Dialog event** to both `obj_bt_classic_server` and
   `obj_bt_classic_client` (and later the BLE equivalents) via the IDE:
   right-click the object → **Add Event → Other → Async - Dialog**. This
   creates `Other_63.gml` for you (confirmed: eventType 7 / eventNum 63 is
   the Async - Dialog event, matching the naming convention already used
   by `Other_63.gml` files in sibling extension projects). Do not hand-edit
   the `.yy` event list — let the IDE add it.

## 2. Confirmed API notes

- `bluetooth_classic_server_start(name, service_uuid)` takes the SPP
  service UUID directly.
- There is **no "connect by address" function** — a device handle only
  ever comes from a scan's `device_found` callback populating the native
  device cache. Both the BLE and Classic client screens must run a scan
  (even briefly / in the background) before they can connect to anything.
- `get_string_async(str, title)` resolves in the **Other → Async - Dialog**
  event (`Other_63.gml`), read via `async_load[? "id"]` / `"status"` /
  `"result"` — same pattern used project-wide for GameMaker async events.

## 3. Classic Server — `obj_bt_classic_server`

`Create_0.gml` (already correct as you have it — kept here for
completeness/reference):

```gml
bluetooth_classic_server_start(DEMO_CLASSIC_SERVICE_NAME, DEMO_CLASSIC_SERVICE_UUID);

global.classic_conn = -1;
send_dialog_id = -1;

bluetooth_set_callback_classic_client_connected(function(_connection, _device) {
    global.classic_conn = _connection;
    show_debug_message("[GML] classic client connected conn=" + string(_connection));
});

bluetooth_set_callback_classic_data(function(_connection, _available_bytes) {
    var _buf = buffer_create(_available_bytes, buffer_grow, 1);
    var _n = bluetooth_classic_receive(_connection, _buf, 0, _available_bytes);
    var _text = buffer_peek(_buf, 0, buffer_string);
    buffer_delete(_buf);
    show_debug_message("[GML] classic RX (" + string(_n) + " bytes): " + _text);
});

bluetooth_set_callback_classic_disconnected(function(_connection, _error_code, _message) {
    show_debug_message("[GML] classic client disconnected: " + _message);
    global.classic_conn = -1;
});
```

`KeyPress_32.gml` (space bar) — replace the hardcoded `"Helloo"` send with
opening the async dialog instead of sending directly:

```gml
if (global.classic_conn == -1) {
    show_debug_message("[GML] no client connected yet");
} else {
    send_dialog_id = get_string_async("Message to send", "");
}
```

`Other_63.gml` (new, via Add Event → Other → Async - Dialog):

```gml
if (async_load[? "id"] == send_dialog_id) {
    if (async_load[? "status"]) {
        var _text = async_load[? "result"];
        if (string_length(_text) > 0 && global.classic_conn != -1) {
            var _buf = buffer_create(string_byte_length(_text) + 1, buffer_fixed, 1);
            buffer_write(_buf, buffer_string, _text);
            bluetooth_classic_send(global.classic_conn, _buf, 0, buffer_get_size(_buf));
            buffer_delete(_buf);
            show_debug_message("[GML] classic TX: " + _text);
        }
    }
    send_dialog_id = -1;
}
```

`CleanUp_0.gml` (already correct, unchanged):

```gml
bluetooth_classic_server_stop();
```

## 4. Classic Client — `obj_bt_classic_client`

This screen scans, shows a clickable list of discovered devices
(per your instruction: build a real list, don't auto-connect), and logs
every device to the console with `show_debug_message`.

`Create_0.gml` (replaces the current file — the current one references an
undefined `_device` and must be rewritten):

```gml
show_debug_message("========== BLUETOOTH CLASSIC CLIENT (DEMO) ==========");

// Android 12+ runtime permissions (same set tm_bt_gmrt_calssic requests)
if (!os_check_permission("android.permission.ACCESS_FINE_LOCATION") || !os_check_permission("android.permission.BLUETOOTH_SCAN") || !os_check_permission("android.permission.BLUETOOTH_CONNECT")) {
    os_request_permission("android.permission.ACCESS_FINE_LOCATION", "android.permission.BLUETOOTH_SCAN", "android.permission.BLUETOOTH_CONNECT");
}

bt_ready = bluetooth_initialize();
last_permission_status = BluetoothPermissionStatus.Unknown;
auto_scan_after_permission = true;

global.classic_conn = -1;
send_dialog_id = -1;

// devices[] is a fresh list rebuilt each time device_found fires, holding
// { device, name, address } structs for the click-to-connect list below.
devices = [];

ui_margin = 24;
ui_gap = 12;
ui_row_h = 56;

bluetooth_print_device = function(_device) {
    var _name = bluetooth_device_get_name(_device);
    var _addr = bluetooth_device_has_address(_device) ? bluetooth_device_get_address(_device) : "<no address>";
    show_debug_message("----------------------------------------");
    show_debug_message($"Handle:  {_device}");
    show_debug_message($"Name:    {_name}");
    show_debug_message($"Address: {_addr}");
};

bluetooth_request_permissions = function() {
    if (!bt_ready) return;
    if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) return;
    bluetooth_permission_request();
};

bluetooth_start_classic_scan = function() {
    if (!bt_ready) return;
    if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted) {
        bluetooth_request_permissions();
        return;
    }
    if (bluetooth_classic_scan_is_running()) return;
    bluetooth_device_clear();
    devices = [];
    var _error = bluetooth_classic_scan_start();
    show_debug_message($"[GML] bluetooth_classic_scan_start() = {_error}");
};

if (bt_ready) {
    bluetooth_set_callback_device_found(function(_device) {
        bluetooth_print_device(_device);
        array_push(devices, {
            device: _device,
            name: bluetooth_device_get_name(_device),
            address: bluetooth_device_has_address(_device) ? bluetooth_device_get_address(_device) : "<no address>"
        });
    });

    bluetooth_set_callback_scan_stopped(function(_error, _message) {
        show_debug_message($"[GML] classic scan stopped: {_error} {_message}, devices found: {array_length(devices)}");
    });

    bluetooth_set_callback_classic_data(function(_connection, _available_bytes) {
        var _buf = buffer_create(_available_bytes, buffer_grow, 1);
        var _n = bluetooth_classic_receive(_connection, _buf, 0, _available_bytes);
        show_debug_message("[GML] classic RX (" + string(_n) + " bytes): " + buffer_peek(_buf, 0, buffer_string));
        buffer_delete(_buf);
    });

    bluetooth_set_callback_classic_disconnected(function(_connection, _error_code, _message) {
        show_debug_message("[GML] classic disconnected: " + _message);
        global.classic_conn = -1;
    });

    bluetooth_device_clear();
    last_permission_status = bluetooth_permission_get_status();
    if (last_permission_status == BluetoothPermissionStatus.Granted) {
        auto_scan_after_permission = false;
        bluetooth_start_classic_scan();
    } else {
        bluetooth_request_permissions();
    }
}
```

`Step_0.gml` (new — draws the device list and handles both list-row clicks
to connect, and Space to open the send dialog once connected):

```gml
if (!bt_ready) exit;

var _permission = bluetooth_permission_get_status();
if (auto_scan_after_permission && _permission == BluetoothPermissionStatus.Granted) {
    auto_scan_after_permission = false;
    bluetooth_start_classic_scan();
}
last_permission_status = _permission;

// Space = send (only meaningful once connected; dialog result handled in Other_63)
if (keyboard_check_pressed(vk_space) && global.classic_conn != -1) {
    send_dialog_id = get_string_async("Message to send", "");
}

// Click-to-connect device list
if (device_mouse_check_button_pressed(0, mb_left) && global.classic_conn == -1) {
    var _mx = device_mouse_x_to_gui(0);
    var _my = device_mouse_y_to_gui(0);
    var _list_top = 96;
    var _count = array_length(devices);
    for (var i = 0; i < _count; i++) {
        var _row_y = _list_top + i * (ui_row_h + ui_gap);
        if (point_in_rectangle(_mx, _my, ui_margin, _row_y, display_get_gui_width() - ui_margin, _row_y + ui_row_h)) {
            var _entry = devices[i];
            show_debug_message($"[GML] connecting to {_entry.name} ({_entry.address})");
            bluetooth_classic_scan_stop();
            bluetooth_classic_connect(_entry.device, DEMO_CLASSIC_SERVICE_UUID,
                function(_error_code, _message, _connection, _device) {
                    show_debug_message("[GML] classic connect " + string(_error_code) + " " + _message);
                    if (_error_code == BluetoothError.Ok) global.classic_conn = _connection;
                });
            break;
        }
    }
}
```

`Draw_64.gml` (GUI draw — Add Event → Draw → Draw GUI; shows scan status,
connection status, and the clickable device list):

```gml
draw_set_font(fnt_gm_20);
draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_set_color(c_white);

if (global.classic_conn != -1) {
    draw_text(ui_margin, 24, "Connected. Press SPACE to send a message.");
} else if (bluetooth_classic_scan_is_running()) {
    draw_text(ui_margin, 24, "Scanning... tap a device below to connect.");
} else {
    draw_text(ui_margin, 24, "Scan finished. Tap a device below to connect.");
}

if (global.classic_conn == -1) {
    var _list_top = 96;
    var _count = array_length(devices);
    for (var i = 0; i < _count; i++) {
        var _row_y = _list_top + i * (ui_row_h + ui_gap);
        var _entry = devices[i];
        draw_rectangle(ui_margin, _row_y, display_get_gui_width() - ui_margin, _row_y + ui_row_h, true);
        draw_text(ui_margin + 12, _row_y + ui_row_h / 2 - 10, _entry.name + "  (" + _entry.address + ")");
    }
}
```

`Other_63.gml` (new, via Add Event → Other → Async - Dialog):

```gml
if (async_load[? "id"] == send_dialog_id) {
    if (async_load[? "status"]) {
        var _text = async_load[? "result"];
        if (string_length(_text) > 0 && global.classic_conn != -1) {
            var _buf = buffer_create(string_byte_length(_text) + 1, buffer_fixed, 1);
            buffer_write(_buf, buffer_string, _text);
            bluetooth_classic_send(global.classic_conn, _buf, 0, buffer_get_size(_buf));
            buffer_delete(_buf);
            show_debug_message("[GML] classic TX: " + _text);
        }
    }
    send_dialog_id = -1;
}
```

`CleanUp_0.gml` (new):

```gml
if (bluetooth_classic_scan_is_running()) bluetooth_classic_scan_stop();
if (global.classic_conn != -1) {
    bluetooth_classic_disconnect(global.classic_conn);
    global.classic_conn = -1;
}
```

## 5. BLE Server / BLE Client

Same shared profile and code blocks as previously designed (Nordic-UART-style
service, `DEMO_SERVICE_UUID` / `DEMO_CHAR_RX_UUID` / `DEMO_CHAR_TX_UUID`,
already defined in `bluetooth_ble_definitions.gml`). The only change from the
earlier draft: the "Send" action opens `get_string_async` and the result is
read in **Other_63.gml**, exactly like the Classic screens above, instead of
being illustrated as a comment.

**BLE Server** (`obj_bt_ble_server`, not yet created):

`Create_0.gml`:
```gml
bluetooth_le_server_start();
send_dialog_id = -1;
global.ble_server_conn = -1;

var _service_json = json_stringify({
    uuid: DEMO_SERVICE_UUID,
    characteristics: [
        { uuid: DEMO_CHAR_RX_UUID, properties: GATT_PROPERTY_WRITE | GATT_PROPERTY_WRITE_NO_RESPONSE, permissions: GATT_PERMISSION_WRITE },
        { uuid: DEMO_CHAR_TX_UUID, properties: GATT_PROPERTY_NOTIFY, permissions: 0 }
    ]
});

bluetooth_le_server_add_service(_service_json, function(_error_code, _message) {
    show_debug_message("[GML] add_service " + string(_error_code) + " " + _message);
    if (_error_code == BluetoothError.Ok) {
        bluetooth_le_advertise_start(
            json_stringify({ txPowerLevel: 0 }),
            json_stringify({ includeName: true, services: [{ uuid: DEMO_SERVICE_UUID }] }),
            function(_err, _msg) { show_debug_message("[GML] advertise_start " + string(_err) + " " + _msg); }
        );
    }
});

bluetooth_set_callback_le_server_connection_state_changed(function(_connection, _connected, _device) {
    global.ble_server_conn = _connected ? _connection : -1;
    show_debug_message("[GML] LE client " + (_connected ? "connected" : "disconnected") + " conn=" + string(_connection));
});

bluetooth_set_callback_le_server_write_request(function(_request_id, _connection, _service_uuid, _characteristic_uuid, _descriptor_uuid) {
    var _buf = buffer_create(512, buffer_grow, 1);
    var _n = bluetooth_le_server_write_request_get_value(_request_id, _buf, 0, 512);
    show_debug_message("[GML] RX write (" + string(_n) + " bytes): " + buffer_peek(_buf, 0, buffer_string));
    buffer_delete(_buf);
    bluetooth_le_server_respond_write(_request_id, BluetoothError.Ok);
});
```

`KeyPress_32.gml` (space = send):
```gml
if (global.ble_server_conn != -1) send_dialog_id = get_string_async("Message to notify", "");
```

`Other_63.gml`:
```gml
if (async_load[? "id"] == send_dialog_id) {
    if (async_load[? "status"]) {
        var _text = async_load[? "result"];
        if (string_length(_text) > 0) {
            var _buf = buffer_create(string_byte_length(_text) + 1, buffer_fixed, 1);
            buffer_write(_buf, buffer_string, _text);
            bluetooth_le_server_notify_value(DEMO_SERVICE_UUID, DEMO_CHAR_TX_UUID, 0, _buf, 0, buffer_get_size(_buf));
            buffer_delete(_buf);
            show_debug_message("[GML] LE TX notify: " + _text);
        }
    }
    send_dialog_id = -1;
}
```

`CleanUp_0.gml`:
```gml
bluetooth_le_advertise_stop();
bluetooth_le_server_clear_services();
bluetooth_le_server_stop();
```

**BLE Client** (`obj_bt_ble_client`, not yet created) — same device-list
pattern as Classic Client, using BLE scan/connect instead:

`Create_0.gml`:
```gml
bt_ready = bluetooth_initialize();
global.ble_conn = -1;
global.rx_char = -1;
global.tx_char = -1;
send_dialog_id = -1;
devices = [];
auto_scan_after_permission = true;

bluetooth_set_callback_device_found(function(_device) {
    if (bluetooth_device_get_transport(_device) != BluetoothTransport.LowEnergy) return;
    show_debug_message($"[GML] LE device found: {bluetooth_device_get_name(_device)}");
    array_push(devices, {
        device: _device,
        name: bluetooth_device_get_name(_device),
        address: bluetooth_device_has_address(_device) ? bluetooth_device_get_address(_device) : "<no address>"
    });
});

bluetooth_set_callback_le_characteristic_value_changed(function(_characteristic, _connection) {
    var _buf = buffer_create(512, buffer_grow, 1);
    var _n = bluetooth_le_characteristic_get_value(_characteristic, _buf, 0, 512);
    show_debug_message("[GML] TX notify (" + string(_n) + " bytes): " + buffer_peek(_buf, 0, buffer_string));
    buffer_delete(_buf);
});

bluetooth_set_callback_le_disconnected(function(_connection, _error_code, _message) {
    show_debug_message("[GML] LE disconnected: " + _message);
    global.ble_conn = -1;
});

if (bt_ready) {
    if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) {
        auto_scan_after_permission = false;
        bluetooth_device_clear();
        bluetooth_le_scan_start();
    } else {
        bluetooth_permission_request();
    }
}
```

`Step_0.gml` (device list click → connect → discover services/characteristics
→ subscribe to TX; Space → send once `global.rx_char` is set):

```gml
if (!bt_ready) exit;

if (auto_scan_after_permission && bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) {
    auto_scan_after_permission = false;
    bluetooth_device_clear();
    bluetooth_le_scan_start();
}

if (keyboard_check_pressed(vk_space) && global.rx_char != -1) {
    send_dialog_id = get_string_async("Message to write", "");
}

if (device_mouse_check_button_pressed(0, mb_left) && global.ble_conn == -1) {
    var _mx = device_mouse_x_to_gui(0);
    var _my = device_mouse_y_to_gui(0);
    var _list_top = 96, _row_h = 56, _gap = 12, _margin = 24;
    var _count = array_length(devices);
    for (var i = 0; i < _count; i++) {
        var _row_y = _list_top + i * (_row_h + _gap);
        if (point_in_rectangle(_mx, _my, _margin, _row_y, display_get_gui_width() - _margin, _row_y + _row_h)) {
            var _entry = devices[i];
            bluetooth_le_scan_stop();
            bluetooth_le_connect(_entry.device, function(_error_code, _message, _connection, _device) {
                show_debug_message("[GML] LE connect " + string(_error_code) + " " + _message);
                if (_error_code != BluetoothError.Ok) return;
                global.ble_conn = _connection;
                bluetooth_le_services_discover(_connection, function(_err2, _msg2, _conn2) {
                    var _service = noone;
                    var _n = bluetooth_le_service_get_count(_conn2);
                    for (var j = 0; j < _n; j++) {
                        var _s = bluetooth_le_service_get_at(_conn2, j);
                        if (bluetooth_le_service_get_uuid(_s) == DEMO_SERVICE_UUID) { _service = _s; break; }
                    }
                    if (_service == noone) { show_debug_message("[GML] demo service not found"); return; }
                    bluetooth_le_characteristics_discover(_service, function(_err3, _msg3, _svc) {
                        var _cn = bluetooth_le_characteristic_get_count(_svc);
                        for (var k = 0; k < _cn; k++) {
                            var _c = bluetooth_le_characteristic_get_at(_svc, k);
                            var _uuid = bluetooth_le_characteristic_get_uuid(_c);
                            if (_uuid == DEMO_CHAR_RX_UUID) global.rx_char = _c;
                            if (_uuid == DEMO_CHAR_TX_UUID) {
                                global.tx_char = _c;
                                bluetooth_le_characteristic_subscribe(_c, BluetoothLeSubscribeMode.Notify, function(_e, _m, _ch) {
                                    show_debug_message("[GML] subscribed to TX: " + string(_e));
                                });
                            }
                        }
                    });
                });
            });
            break;
        }
    }
}
```

`Draw_64.gml` (Draw GUI): same list-drawing block as the Classic Client's,
gated on `global.ble_conn == -1`.

`Other_63.gml`:
```gml
if (async_load[? "id"] == send_dialog_id) {
    if (async_load[? "status"]) {
        var _text = async_load[? "result"];
        if (string_length(_text) > 0 && global.rx_char != -1) {
            var _buf = buffer_create(string_byte_length(_text) + 1, buffer_fixed, 1);
            buffer_write(_buf, buffer_string, _text);
            bluetooth_le_characteristic_write(global.rx_char, _buf, 0, buffer_get_size(_buf), 1,
                function(_err, _msg, _ch) { show_debug_message("[GML] write result " + string(_err)); });
            buffer_delete(_buf);
        }
    }
    send_dialog_id = -1;
}
```

`CleanUp_0.gml`:
```gml
if (bluetooth_le_scan_is_running()) bluetooth_le_scan_stop();
if (global.ble_conn != -1) {
    bluetooth_le_disconnect(global.ble_conn);
    global.ble_conn = -1;
}
```

## 6. Rooms still needed

`rm_bt_ble_server` and `rm_bt_ble_client` don't exist yet (only referenced
from the menu). Create them the same way as the Classic rooms: a back
button (`obj_button_goto`, default goto is the menu) plus one instance of
the corresponding controller object.

## 7. Classic connection lifecycle — findings & the missing `obj_bt_classic_connection`

You've since restructured the Classic screens yourself: `obj_bt_classic_discoverable`
(toggle button), `obj_bt_classic_device` (one clickable button instance per
scanned device, spawned by `obj_bt_classic_client`), and `obj_bt_classic_connection`
(meant to hold the live connection and stream mouse position both ways) now
exist alongside `obj_bt_classic_server`/`obj_bt_classic_client`. Checked every
`obj_bt_classic_*` file on disk — here's what's actually there vs. what's
needed.

### 7.1 Confirmed bugs

1. **`obj_bt_classic_discoverable/Mouse_4.gml` calls `bluetooth_classic_discoverable_start()`
   with zero arguments.** The function takes one required `duration_seconds: int32`
   (`GMBluetooth.yy` compiles it with `argCount:1`) — this throws a wrong-number-of-arguments
   error the moment the button is clicked. Fix:
   ```gml
   if (bluetooth_classic_discoverable_is_running())
       bluetooth_classic_discoverable_stop()
   else
       bluetooth_classic_discoverable_start(120)  // seconds; 0 = indefinite on platforms that allow it
   ```

2. **`obj_bt_classic_server/Create_0.gml` never initializes `global.classic_conn`.**
   It's only ever set inside the `classic_client_connected` callback, so anything
   that reads it before a client connects (e.g. `KeyPress_32.gml`'s `global.classic_conn`
   check) touches an undefined global. Client's Create already does this correctly
   (`global.classic_conn = -1;`) — Server's needs the same line added.

### 7.2 The real gap: nothing ever spawns `obj_bt_classic_connection`

Both connect paths currently just stash the raw handle:
- `obj_bt_classic_server/Create_0.gml`'s `classic_client_connected` callback → `global.classic_conn = _connection;`
- `obj_bt_classic_device/Mouse_4.gml`'s connect callback → `global.classic_conn = _connection;`

`obj_bt_classic_connection` itself has **zero `.gml` files on disk** — its `.yy`
declares one Step event with no code behind it at all. It needs Create, Step,
Draw and CleanUp events added via the IDE (**Add Event**, same as the
Async - Dialog note in section 1.3 — don't hand-edit the `.yy` event list).

**Callback routing note**: `bluetooth_set_callback_classic_data` /
`_classic_disconnected` are process-wide single-slot callbacks (registered once
in Server/Client's Create), not per-connection. Since this demo only ever has
one live Classic connection at a time, the simplest correct fix is to keep
those registrations where they are and have them *forward* into whichever
`obj_bt_classic_connection` instance is currently alive, tracked via a new
`global.classic_conn_inst`. No per-instance callback re-registration needed.

**`obj_bt_classic_server/Create_0.gml`** — replace with:
```gml
show_debug_message("Bluetooth Classic Server")

global.classic_conn = -1;
global.classic_conn_inst = noone;

bluetooth_classic_server_start(DEMO_CLASSIC_SERVICE_NAME, DEMO_CLASSIC_SERVICE_UUID);

bluetooth_set_callback_classic_client_connected(function(_connection, _device) {
    global.classic_conn = _connection;
    global.classic_conn_inst = instance_create_depth(0, 0, 0, obj_bt_classic_connection, {connection: _connection});
    show_debug_message("[GML] classic client connected conn=" + string(_connection));
});

bluetooth_set_callback_classic_data(function(_connection, _available_bytes) {
    var _buf = buffer_create(_available_bytes, buffer_grow, 1);
    var _n = bluetooth_classic_receive(_connection, _buf, 0, _available_bytes);
    if (instance_exists(global.classic_conn_inst)) global.classic_conn_inst.on_receive(_buf, _n);
    buffer_delete(_buf);
});

bluetooth_set_callback_classic_disconnected(function(_connection, _error_code, _message) {
    show_debug_message("[GML] classic client disconnected: " + _message);
    if (instance_exists(global.classic_conn_inst)) instance_destroy(global.classic_conn_inst);
});
```
(`KeyPress_32.gml`'s hardcoded `"Helloo"` test send is unrelated leftover from
before the dialog-based send flow — harmless to keep, safe to delete once
you're relying on the connection object instead.)

**`obj_bt_classic_client/Create_0.gml`** — same forwarding pattern, two callbacks change:
```gml
global.classic_conn = -1;
global.classic_conn_inst = noone;   // add next to the existing global.classic_conn = -1;
```
```gml
bluetooth_set_callback_classic_data(function(_connection, _available_bytes) {
    var _buf = buffer_create(_available_bytes, buffer_grow, 1);
    var _n = bluetooth_classic_receive(_connection, _buf, 0, _available_bytes);
    if (instance_exists(global.classic_conn_inst)) global.classic_conn_inst.on_receive(_buf, _n);
    buffer_delete(_buf);
});

bluetooth_set_callback_classic_disconnected(function(_connection, _error_code, _message) {
    show_debug_message("[GML] classic disconnected: " + _message);
    if (instance_exists(global.classic_conn_inst)) instance_destroy(global.classic_conn_inst);
});
```

**`obj_bt_classic_device/Mouse_4.gml`** — spawn the connection object on success:
```gml
bluetooth_classic_scan_stop();
bluetooth_classic_connect(device, DEMO_CLASSIC_SERVICE_UUID,
    function(_error_code, _message, _connection, _device) {
        show_debug_message("[GML] classic connect " + string(_error_code) + " " + _message);
        if (_error_code == BluetoothError.Ok) {
            global.classic_conn = _connection;
            global.classic_conn_inst = instance_create_depth(0, 0, 0, obj_bt_classic_connection, {connection: _connection});
        }
    });
```

### 7.3 `obj_bt_classic_connection` — full implementation

`connection` arrives via the `instance_create_depth(..., {connection: _connection})`
struct argument, same mechanism `obj_bt_classic_device` already uses for `device`
— no assignment code needed for it in Create.

`Create_0.gml` (new — Add Event → Create):
```gml
// connection is injected by whichever caller spawned this instance
// (obj_bt_classic_server's client_connected callback, or
// obj_bt_classic_device's connect-success callback).
remote_x = -1;
remote_y = -1;
has_remote = false;

on_receive = function(_buf, _n) {
    if (_n < 8) return; // wait for a full x/y packet (2 x int32)
    remote_x = buffer_peek(_buf, 0, buffer_s32);
    remote_y = buffer_peek(_buf, 4, buffer_s32);
    has_remote = true;
};
```

`Step_0.gml` (replaces the current empty Step):
```gml
if (!bluetooth_classic_connection_is_connected(connection)) exit;

var _buf = buffer_create(8, buffer_fixed, 1);
buffer_write(_buf, buffer_s32, mouse_x);
buffer_write(_buf, buffer_s32, mouse_y);
bluetooth_classic_send(connection, _buf, 0, buffer_get_size(_buf));
buffer_delete(_buf);
```

`Draw_0.gml` (new — Add Event → Draw → Draw; room-space, so `mouse_x`/`mouse_y`
line up directly with the drawn marker):
```gml
draw_set_color(c_white);
draw_text(16, 16, "You: " + string(mouse_x) + ", " + string(mouse_y));

if (has_remote) {
    draw_set_color(c_red);
    draw_circle(remote_x, remote_y, 12, false);
    draw_text(remote_x + 16, remote_y - 8, "Peer");
    draw_set_color(c_white);
}
```

`CleanUp_0.gml` (new — Add Event → Cleanup; fires both on explicit
`instance_destroy()` and on room/game end, so this is the one place that
needs to disconnect):
```gml
if (bluetooth_classic_connection_is_connected(connection)) {
    bluetooth_classic_disconnect(connection);
}

if (global.classic_conn_inst == id) {
    global.classic_conn_inst = noone;
    global.classic_conn = -1;
}
```

Sending every Step (8 bytes/frame) is trivial for RFCOMM — no throttling needed
for a demo. This treats each `classic_data` event as exactly one 8-byte position
packet, which holds as long as sends aren't bursty; fine for this use case since
there's exactly one packet in flight per frame.

### 7.4 Leftover redundant code (not breaking anything, safe to remove)

- **`obj_bt_classic_client/Step_0.gml`** — the whole "Click-to-connect device
  list" block (the `device_mouse_check_button_pressed`/`point_in_rectangle`
  loop over `devices[]`) duplicates what `obj_bt_classic_device/Mouse_4.gml`
  now does per-instance. `Draw_0.gml`'s matching row-drawing block is already
  commented out — this is the one live remnant. Safe to delete once you
  confirm clicking the button instances works end to end.
- **`obj_bt_classic_client/KeyPress_32.gml`** — empty file, dead code from
  before the vk_space handling moved into `Step_0.gml`. Harmless either way;
  remove the event via the IDE if you want to tidy it up.
