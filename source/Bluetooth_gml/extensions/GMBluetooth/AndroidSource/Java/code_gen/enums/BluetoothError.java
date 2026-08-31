// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothError
{
    Ok((int)0),
    Unknown((int)1),
    NotSupported((int)2),
    NotInitialized((int)3),
    BluetoothDisabled((int)4),
    PermissionDenied((int)5),
    InvalidArgument((int)6),
    InvalidHandle((int)7),
    Busy((int)8),
    Timeout((int)9),
    NotFound((int)10),
    ConnectionFailed((int)11),
    Disconnected((int)12),
    OperationFailed((int)13);

    private final int value;
    private BluetoothError(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothError from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothError.Ok;
            case 1:
                return BluetoothError.Unknown;
            case 2:
                return BluetoothError.NotSupported;
            case 3:
                return BluetoothError.NotInitialized;
            case 4:
                return BluetoothError.BluetoothDisabled;
            case 5:
                return BluetoothError.PermissionDenied;
            case 6:
                return BluetoothError.InvalidArgument;
            case 7:
                return BluetoothError.InvalidHandle;
            case 8:
                return BluetoothError.Busy;
            case 9:
                return BluetoothError.Timeout;
            case 10:
                return BluetoothError.NotFound;
            case 11:
                return BluetoothError.ConnectionFailed;
            case 12:
                return BluetoothError.Disconnected;
            case 13:
                return BluetoothError.OperationFailed;
            default:
                throw new IllegalArgumentException("Unknown BluetoothError value: " + v);
        }
    }
}