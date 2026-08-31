// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothPermissionStatus
{
    Unknown((int)0),
    Granted((int)1),
    Denied((int)2);

    private final int value;
    private BluetoothPermissionStatus(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothPermissionStatus from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothPermissionStatus.Unknown;
            case 1:
                return BluetoothPermissionStatus.Granted;
            case 2:
                return BluetoothPermissionStatus.Denied;
            default:
                throw new IllegalArgumentException("Unknown BluetoothPermissionStatus value: " + v);
        }
    }
}