// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothLeCharacteristicProperty
{
    None((int)0),
    Broadcast((int)1),
    Read((int)2),
    WriteWithoutResponse((int)4),
    Write((int)8),
    Notify((int)16),
    Indicate((int)32),
    AuthenticatedSignedWrites((int)64),
    ExtendedProperties((int)128);

    private final int value;
    private BluetoothLeCharacteristicProperty(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothLeCharacteristicProperty from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothLeCharacteristicProperty.None;
            case 1:
                return BluetoothLeCharacteristicProperty.Broadcast;
            case 2:
                return BluetoothLeCharacteristicProperty.Read;
            case 4:
                return BluetoothLeCharacteristicProperty.WriteWithoutResponse;
            case 8:
                return BluetoothLeCharacteristicProperty.Write;
            case 16:
                return BluetoothLeCharacteristicProperty.Notify;
            case 32:
                return BluetoothLeCharacteristicProperty.Indicate;
            case 64:
                return BluetoothLeCharacteristicProperty.AuthenticatedSignedWrites;
            case 128:
                return BluetoothLeCharacteristicProperty.ExtendedProperties;
            default:
                throw new IllegalArgumentException("Unknown BluetoothLeCharacteristicProperty value: " + v);
        }
    }
}