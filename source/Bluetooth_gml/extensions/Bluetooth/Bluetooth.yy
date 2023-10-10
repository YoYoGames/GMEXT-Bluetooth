{
  "resourceType": "GMExtension",
  "resourceVersion": "1.2",
  "name": "Bluetooth",
  "optionsFile": "options.json",
  "options": [],
  "exportToGame": true,
  "supportedTargets": -1,
  "extensionVersion": "1.0.0",
  "packageId": "",
  "productId": "",
  "author": "",
  "date": "2023-05-25T20:50:43.056408+01:00",
  "license": "",
  "description": "",
  "helpfile": "",
  "iosProps": true,
  "tvosProps": false,
  "androidProps": true,
  "html5Props": false,
  "installdir": "",
  "files": [
    {"resourceType":"GMExtensionFile","resourceVersion":"1.0","name":"","filename":"Bluetooth.ext","origname":"","init":"bt_init","final":"bt_end","kind":1,"uncompress":false,"functions":[
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_init","externalName":"bt_init","kind":1,"help":"bt_init()","hidden":true,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_scan_start","externalName":"bt_classic_scan_start","kind":1,"help":"bt_classic_scan_start","hidden":false,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_scan_is_active","externalName":"bt_classic_scan_is_active","kind":1,"help":"bt_classic_scan_is_active","hidden":false,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_scan_stop","externalName":"bt_classic_scan_stop","kind":1,"help":"bt_classic_scan_stop","hidden":false,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_discoverability_enable","externalName":"bt_classic_discoverability_enable","kind":1,"help":"bt_classic_discoverability_enable(seconds)","hidden":false,"returnType":1,"argCount":0,"args":[
            2,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_discoverability_is_active","externalName":"bt_classic_discoverability_is_active","kind":1,"help":"bt_classic_discoverability_is_active","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_server_start","externalName":"bt_classic_server_start","kind":1,"help":"bt_classic_server_start(service_name, uuid, insecure)","hidden":false,"returnType":1,"argCount":0,"args":[
            1,
            1,
            2,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_server_stop","externalName":"bt_classic_server_stop","kind":1,"help":"bt_classic_server_stop()","hidden":false,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_socket_open","externalName":"bt_classic_socket_open","kind":1,"help":"bt_classic_socket_open","hidden":false,"returnType":1,"argCount":0,"args":[
            1,
            1,
            2,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_socket_send","externalName":"bt_classic_socket_send","kind":1,"help":"bt_classic_socket_send(socketId, bufferId, offset, length)","hidden":false,"returnType":2,"argCount":0,"args":[
            2,
            2,
            2,
            2,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_socket_receive","externalName":"bt_classic_socket_receive","kind":1,"help":"bt_classic_socket_receive(sockedId, bufferId, offset)","hidden":false,"returnType":2,"argCount":0,"args":[
            2,
            2,
            2,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_scan_start","externalName":"bt_le_scan_start","kind":1,"help":"bt_le_scan_start()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_scan_stop","externalName":"bt_le_scan_stop","kind":1,"help":"bt_le_scan_stop()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_scan_is_active","externalName":"bt_le_scan_is_active","kind":1,"help":"bt_le_scan_is_active()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_peripheral_open","externalName":"bt_le_peripheral_open","kind":1,"help":"bt_le_peripheral_open(peripheralAddress)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_peripheral_get_services","externalName":"bt_le_peripheral_get_services","kind":1,"help":"bt_le_peripheral_get_services(peripheralAddress)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_peripheral_is_open","externalName":"bt_le_peripheral_is_open","kind":1,"help":"bt_le_peripheral_is_open(peripheralAddress)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_service_get_characteristics","externalName":"bt_le_service_get_characteristics","kind":1,"help":"bt_le_service_get_characteristics(peripheralAddress, serviceUuid)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_end","externalName":"bt_end","kind":1,"help":"bt_end()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_characteristic_get_descriptors","externalName":"bt_le_characteristic_get_descriptors","kind":1,"help":"bt_le_characteristic_get_descriptors(peripheralAddress, serviceUuid, characteristicUuid)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_characteristic_read","externalName":"bt_le_characteristic_read","kind":1,"help":"bt_le_characteristic_read(peripheralAddress, serviceUuid, characteristicUuid)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_characteristic_write_request","externalName":"bt_le_characteristic_write_request","kind":1,"help":"bt_le_characteristic_write_request(peripheralAddress, serviceUuid, characteristicUuid, value)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_characteristic_write_command","externalName":"bt_le_characteristic_write_command","kind":1,"help":"bt_le_characteristic_write_command(peripheralAddress, serviceUuid, characteristicUuid, value)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_characteristic_notify","externalName":"bt_le_characteristic_notify","kind":1,"help":"bt_le_characteristic_notify(peripheralAddress, serviceUuid, characteristicUuid)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_characteristic_indicate","externalName":"bt_le_characteristic_indicate","kind":1,"help":"bt_le_characteristic_indicate(peripheralAddress, serviceUuid, characteristicUuid)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_characteristic_unsubscribe","externalName":"bt_le_characteristic_unsubscribe","kind":1,"help":"bt_le_characteristic_unsubscribe(peripheralAddress, serviceUuid, characteristicUuid)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_server_add_service","externalName":"bt_le_server_add_service","kind":1,"help":"bt_le_server_add_service(serviceData)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_advertise_start","externalName":"bt_le_advertise_start","kind":1,"help":"bt_le_advertise_start()","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_advertise_stop","externalName":"bt_le_advertise_stop","kind":1,"help":"bt_le_advertise_stop()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_get_paired_devices","externalName":"bt_le_get_paired_devices","kind":1,"help":"bt_le_get_paired_devices()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_advertise_is_active","externalName":"bt_le_advertise_is_active","kind":1,"help":"bt_le_advertise_is_active()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_server_clear_services","externalName":"bt_le_server_clear_services","kind":1,"help":"bt_le_server_clear_services()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_server_close","externalName":"bt_le_server_close","kind":1,"help":"bt_le_server_close()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_server_notify_value","externalName":"bt_le_server_notify_value","kind":1,"help":"bt_le_server_notify_value(serviceUuid, characteristicUuid, value)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_peripheral_close_all","externalName":"bt_le_peripheral_close_all","kind":1,"help":"bt_le_peripheral_close_all()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_peripheral_close","externalName":"bt_le_peripheral_close","kind":1,"help":"bt_le_peripheral_close(peripheralAddress)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_peripheral_is_connected","externalName":"bt_le_peripheral_is_connected","kind":1,"help":"bt_le_peripheral_is_connected(peripheralAddress)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_peripheral_is_paired","externalName":"bt_le_peripheral_is_paired","kind":1,"help":"bt_le_peripheral_is_paired(peripheralAddress)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_descriptor_read","externalName":"bt_le_descriptor_read","kind":1,"help":"bt_le_descriptor_read(peripheralAddress, serviceUuid, characteristicUuid, descriptorUuid)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_descriptor_write","externalName":"bt_le_descriptor_write","kind":1,"help":"bt_le_descriptor_write(peripheralAddress, serviceUuid, characteristicUuid, descriptorUuid, value)","hidden":false,"returnType":2,"argCount":0,"args":[
            1,
            1,
            1,
            1,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_server_respond_read","externalName":"bt_le_server_respond_read","kind":1,"help":"bt_le_server_respond_read(respondHandle, status, value)","hidden":false,"returnType":2,"argCount":0,"args":[
            2,
            2,
            1,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_server_respond_write","externalName":"bt_le_server_respond_write","kind":1,"help":"bt_le_server_respond_write(reponseHandle, status)","hidden":false,"returnType":2,"argCount":0,"args":[
            2,
            2,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_socket_close","externalName":"bt_classic_socket_close","kind":1,"help":"bt_classic_socket_close(socketId)","hidden":false,"returnType":2,"argCount":0,"args":[
            2,
          ],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_socket_close_all","externalName":"bt_classic_socket_close_all","kind":1,"help":"bt_classic_socket_close_all()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_is_enabled","externalName":"bt_is_enabled","kind":1,"help":"bt_is_enabled()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_request_enable","externalName":"bt_request_enable","kind":1,"help":"bt_request_enable()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_get_address","externalName":"bt_get_address","kind":1,"help":"bt_get_address()","hidden":false,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_get_name","externalName":"bt_get_name","kind":1,"help":"bt_get_name()","hidden":false,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_get_paired_devices","externalName":"bt_get_paired_devices","kind":1,"help":"bt_get_paired_devices","hidden":false,"returnType":1,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_is_supported","externalName":"bt_le_is_supported","kind":1,"help":"bt_le_is_supported()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_classic_is_supported","externalName":"bt_classic_is_supported","kind":1,"help":"bt_classic_is_supported()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
        {"resourceType":"GMExtensionFunction","resourceVersion":"1.0","name":"bt_le_server_open","externalName":"bt_le_server_open","kind":1,"help":"bt_le_server_open()","hidden":false,"returnType":2,"argCount":0,"args":[],"documentation":"",},
      ],"constants":[
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_SUCCESS","value":"0","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_WRITE_NOT_PERMITTED","value":"3","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_REQUEST_NOT_SUPPORTED","value":"6","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_READ_NOT_PERMITTED","value":"2","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_INVALID_OFFSET","value":"7","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_INVALID_ATTRIBUTE_LENGTH","value":"13","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_INSUFFICIENT_ENCRYPTION","value":"15","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_INSUFFICIENT_AUTHORIZATION","value":"8","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_INSUFFICIENT_AUTHENTICATION","value":"5","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_CONNECTION_CONGESTED","value":"143","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_FAILURE","value":"257","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_ACTION_NOT_STARTED","value":"-10","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_BROADCAST","value":"1","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_EXTENDED_PROPS","value":"128","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_INDICATE","value":"32","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_NOTIFY","value":"16","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_READ","value":"2","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_SIGNED_WRITE","value":"64","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_WRITE","value":"8","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PROPERTY_WRITE_NO_RESPONSE","value":"4","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_READ","value":"1","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_READ_ENCRYPTED","value":"2","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_READ_ENCRYPTED_MITM","value":"4","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_WRITE","value":"16","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_WRITE_ENCRYPTED","value":"32","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_WRITE_ENCRYPTED_MITM","value":"64","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_WRITE_SIGNED","value":"128","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_PERMISSION_WRITE_SIGNED_MITM","value":"256","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_SERVICE_NOT_FOUND","value":"-1","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_CHARACTERISTIC_NOT_FOUND","value":"-2","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_STATUS_DESCRIPTOR_NOT_FOUND","value":"-3","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_SERVICE_TYPE_PRIMARY","value":"0","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_SERVICE_TYPE_SECONDARY","value":"1","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_ADVERTISE_MODE_BALANCED","value":"1","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_ADVERTISE_MODE_LOW_LATENCY","value":"2","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_ADVERTISE_MODE_LOW_POWER","value":"0","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_ADVERTISE_TX_POWER_HIGH","value":"3","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_ADVERTISE_TX_POWER_LOW","value":"1","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_ADVERTISE_TX_POWER_MEDIUM","value":"2","hidden":false,},
        {"resourceType":"GMExtensionConstant","resourceVersion":"1.0","name":"BT_ADVERTISE_TX_POWER_ULTRA_LOW","value":"0","hidden":false,},
      ],"ProxyFiles":[
        {"resourceType":"GMProxyFile","resourceVersion":"1.0","name":"BluetoothWindows.dll","TargetMask":6,},
        {"resourceType":"GMProxyFile","resourceVersion":"1.0","name":"libBluetoothMac.dylib","TargetMask":1,},
      ],"copyToTargets":78,"usesRunnerInterface":false,"order":[
        {"name":"bt_init","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_end","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_is_enabled","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_request_enable","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_get_address","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_get_name","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_get_paired_devices","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_scan_start","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_scan_is_active","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_scan_stop","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_discoverability_enable","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_discoverability_is_active","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_server_start","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_server_stop","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_socket_open","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_socket_close","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_socket_close_all","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_socket_send","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_classic_socket_receive","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_scan_start","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_scan_stop","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_scan_is_active","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_peripheral_open","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_peripheral_get_services","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_peripheral_is_open","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_service_get_characteristics","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_characteristic_get_descriptors","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_characteristic_read","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_characteristic_write_request","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_characteristic_write_command","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_characteristic_notify","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_characteristic_indicate","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_characteristic_unsubscribe","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_server_add_service","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_advertise_start","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_advertise_stop","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_get_paired_devices","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_advertise_is_active","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_server_clear_services","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_server_close","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_server_notify_value","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_peripheral_close_all","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_peripheral_close","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_peripheral_is_connected","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_peripheral_is_paired","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_descriptor_read","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_descriptor_write","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_server_respond_read","path":"extensions/Bluetooth/Bluetooth.yy",},
        {"name":"bt_le_server_respond_write","path":"extensions/Bluetooth/Bluetooth.yy",},
      ],},
  ],
  "HTML5CodeInjection": "",
  "classname": "Bluetooth",
  "tvosclassname": null,
  "tvosdelegatename": null,
  "iosdelegatename": "",
  "androidclassname": "Bluetooth",
  "sourcedir": "",
  "androidsourcedir": "",
  "macsourcedir": "",
  "maccompilerflags": "",
  "tvosmaccompilerflags": "",
  "maclinkerflags": "",
  "tvosmaclinkerflags": "",
  "iosplistinject": "\n<key>NSBluetoothPeripheralUsageDescription</key><string>Advertisement would like to use bluetooth.</string>\n<key>NSBluetoothAlwaysUsageDescription</key><string>This app uses Bluetooth for multiplayer features or electronic devices communication</string>\n",
  "tvosplistinject": "",
  "androidinject": "",
  "androidmanifestinject": "",
  "androidactivityinject": "",
  "gradleinject": "",
  "androidcodeinjection": "",
  "hasConvertedCodeInjection": true,
  "ioscodeinjection": "<YYIosPlist>\n<key>NSBluetoothPeripheralUsageDescription</key><string>Advertisement would like to use bluetooth.</string>\n<key>NSBluetoothAlwaysUsageDescription</key><string>This app uses Bluetooth for multiplayer features or electronic devices communication</string>\n</YYIosPlist>",
  "tvoscodeinjection": "",
  "iosSystemFrameworkEntries": [
    {"resourceType":"GMExtensionFrameworkEntry","resourceVersion":"1.0","name":"CoreBluetooth.framework","weakReference":false,"embed":0,},
  ],
  "tvosSystemFrameworkEntries": [],
  "iosThirdPartyFrameworkEntries": [],
  "tvosThirdPartyFrameworkEntries": [],
  "IncludedResources": [],
  "androidPermissions": [
    "android.permission.BLUETOOTH",
    "android.permission.BLUETOOTH_ADMIN",
    "android.permission.BLUETOOTH_CONNECT",
    "android.permission.BLUETOOTH_ADVERTISE",
    "android.permission.ACCESS_FINE_LOCATION",
    "android.permission.BLUETOOTH_SCAN",
    "android.permission.ACCESS_COARSE_LOCATION",
    "android.permission.BLUETOOTH_ADVERTISE",
  ],
  "copyToTargets": 78,
  "iosCocoaPods": "",
  "tvosCocoaPods": "",
  "iosCocoaPodDependencies": "",
  "tvosCocoaPodDependencies": "",
  "parent": {
    "name": "Extensions",
    "path": "folders/Bluetooth/Extensions.yy",
  },
}