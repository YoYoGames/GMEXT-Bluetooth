// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothLeSubscribeMode
{
    Unsubscribe((int)0),
    Notify((int)1),
    Indicate((int)2);

    private final int value;
    private BluetoothLeSubscribeMode(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothLeSubscribeMode from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothLeSubscribeMode.Unsubscribe;
            case 1:
                return BluetoothLeSubscribeMode.Notify;
            case 2:
                return BluetoothLeSubscribeMode.Indicate;
            default:
                throw new IllegalArgumentException("Unknown BluetoothLeSubscribeMode value: " + v);
        }
    }
}