// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothTransport
{
    Unknown((int)0),
    Classic((int)1),
    LowEnergy((int)2);

    private final int value;
    private BluetoothTransport(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothTransport from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothTransport.Unknown;
            case 1:
                return BluetoothTransport.Classic;
            case 2:
                return BluetoothTransport.LowEnergy;
            default:
                throw new IllegalArgumentException("Unknown BluetoothTransport value: " + v);
        }
    }
}