// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothState
{
    Unknown((int)0),
    Resetting((int)1),
    Unsupported((int)2),
    Unauthorized((int)3),
    PoweredOff((int)4),
    PoweredOn((int)5);

    private final int value;
    private BluetoothState(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothState from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothState.Unknown;
            case 1:
                return BluetoothState.Resetting;
            case 2:
                return BluetoothState.Unsupported;
            case 3:
                return BluetoothState.Unauthorized;
            case 4:
                return BluetoothState.PoweredOff;
            case 5:
                return BluetoothState.PoweredOn;
            default:
                throw new IllegalArgumentException("Unknown BluetoothState value: " + v);
        }
    }
}