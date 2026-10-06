// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothLeConnectionPriority
{
    Balanced((int)0),
    High((int)1),
    LowPower((int)2);

    private final int value;
    private BluetoothLeConnectionPriority(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothLeConnectionPriority from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothLeConnectionPriority.Balanced;
            case 1:
                return BluetoothLeConnectionPriority.High;
            case 2:
                return BluetoothLeConnectionPriority.LowPower;
            default:
                return null;
        }
    }
}