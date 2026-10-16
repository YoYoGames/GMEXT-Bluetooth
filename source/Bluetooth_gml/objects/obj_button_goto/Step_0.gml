// Lock a demo room the platform cannot run, from the capability matrix
// obj_bt_init cached; any other room (the menu itself) stays unlocked.
var _caps = global.bt_capabilities;

switch (goto)
{
    case rm_bt_le_client:
        locked = !_caps.le;
        break;

    case rm_bt_ble_server:
        locked = !(_caps.le && _caps.le_server && _caps.le_advertise);
        break;

    case rm_bt_classic_client:
        locked = !_caps.classic;
        break;

    case rm_bt_classic_server:
        locked = !(_caps.classic && _caps.classic_server);
        break;

    default:
        locked = false;
        break;
}

event_inherited();
