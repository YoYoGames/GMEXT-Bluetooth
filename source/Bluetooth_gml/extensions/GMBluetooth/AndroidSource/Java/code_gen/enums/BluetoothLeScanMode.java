// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothLeScanMode
{
    LowPower((int)0),
    Balanced((int)1),
    LowLatency((int)2);

    private final int value;
    private BluetoothLeScanMode(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothLeScanMode from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothLeScanMode.LowPower;
            case 1:
                return BluetoothLeScanMode.Balanced;
            case 2:
                return BluetoothLeScanMode.LowLatency;
            default:
                return null;
        }
    }
}