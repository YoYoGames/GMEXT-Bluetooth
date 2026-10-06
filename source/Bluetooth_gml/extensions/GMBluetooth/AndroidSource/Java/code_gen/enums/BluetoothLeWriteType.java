// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothLeWriteType
{
    WithResponse((int)0),
    WithoutResponse((int)1);

    private final int value;
    private BluetoothLeWriteType(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothLeWriteType from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothLeWriteType.WithResponse;
            case 1:
                return BluetoothLeWriteType.WithoutResponse;
            default:
                return null;
        }
    }
}