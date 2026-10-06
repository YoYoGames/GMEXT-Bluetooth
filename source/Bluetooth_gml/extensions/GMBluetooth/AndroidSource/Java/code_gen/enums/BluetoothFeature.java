// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothFeature
{
    LeCentral((int)0),
    LePassiveScan((int)1),
    LeAdvertise((int)2),
    LeAdvertiseName((int)3),
    LeAdvertiseServiceUuids((int)4),
    LeAdvertiseServiceData((int)5),
    LeAdvertiseManufacturerData((int)6),
    LeAdvertiseTxPower((int)7),
    LeAdvertiseIncludeTxPower((int)8),
    LeAdvertiseNonConnectable((int)9),
    LeServer((int)10),
    LeServerDescriptorRequests((int)11),
    LeServerSignedWrite((int)12),
    LeServerConnectionEvents((int)13),
    LePairing((int)14),
    Classic((int)15),
    ClassicServer((int)16),
    ClassicPairing((int)17),
    ClassicDiscoverable((int)18),
    ClassicDiscoverableStop((int)19),
    PermissionRequest((int)20),
    LeMtuRequest((int)21),
    LeReadRssi((int)22),
    LeConnectionPriority((int)23),
    RequestEnable((int)24),
    PairedDevicesQuery((int)25);

    private final int value;
    private BluetoothFeature(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothFeature from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothFeature.LeCentral;
            case 1:
                return BluetoothFeature.LePassiveScan;
            case 2:
                return BluetoothFeature.LeAdvertise;
            case 3:
                return BluetoothFeature.LeAdvertiseName;
            case 4:
                return BluetoothFeature.LeAdvertiseServiceUuids;
            case 5:
                return BluetoothFeature.LeAdvertiseServiceData;
            case 6:
                return BluetoothFeature.LeAdvertiseManufacturerData;
            case 7:
                return BluetoothFeature.LeAdvertiseTxPower;
            case 8:
                return BluetoothFeature.LeAdvertiseIncludeTxPower;
            case 9:
                return BluetoothFeature.LeAdvertiseNonConnectable;
            case 10:
                return BluetoothFeature.LeServer;
            case 11:
                return BluetoothFeature.LeServerDescriptorRequests;
            case 12:
                return BluetoothFeature.LeServerSignedWrite;
            case 13:
                return BluetoothFeature.LeServerConnectionEvents;
            case 14:
                return BluetoothFeature.LePairing;
            case 15:
                return BluetoothFeature.Classic;
            case 16:
                return BluetoothFeature.ClassicServer;
            case 17:
                return BluetoothFeature.ClassicPairing;
            case 18:
                return BluetoothFeature.ClassicDiscoverable;
            case 19:
                return BluetoothFeature.ClassicDiscoverableStop;
            case 20:
                return BluetoothFeature.PermissionRequest;
            case 21:
                return BluetoothFeature.LeMtuRequest;
            case 22:
                return BluetoothFeature.LeReadRssi;
            case 23:
                return BluetoothFeature.LeConnectionPriority;
            case 24:
                return BluetoothFeature.RequestEnable;
            case 25:
                return BluetoothFeature.PairedDevicesQuery;
            default:
                return null;
        }
    }
}