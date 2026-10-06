// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothLeAdvertiseTxPower
{
    UltraLow((int)0),
    Low((int)1),
    Medium((int)2),
    High((int)3);

    private final int value;
    private BluetoothLeAdvertiseTxPower(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothLeAdvertiseTxPower from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothLeAdvertiseTxPower.UltraLow;
            case 1:
                return BluetoothLeAdvertiseTxPower.Low;
            case 2:
                return BluetoothLeAdvertiseTxPower.Medium;
            case 3:
                return BluetoothLeAdvertiseTxPower.High;
            default:
                return null;
        }
    }
}