// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothLeAttributePermission
{
    None((int)0),
    Read((int)1),
    ReadEncrypted((int)2),
    ReadEncryptedMitm((int)4),
    Write((int)16),
    WriteEncrypted((int)32),
    WriteEncryptedMitm((int)64),
    WriteSigned((int)128),
    WriteSignedMitm((int)256);

    private final int value;
    private BluetoothLeAttributePermission(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothLeAttributePermission from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothLeAttributePermission.None;
            case 1:
                return BluetoothLeAttributePermission.Read;
            case 2:
                return BluetoothLeAttributePermission.ReadEncrypted;
            case 4:
                return BluetoothLeAttributePermission.ReadEncryptedMitm;
            case 16:
                return BluetoothLeAttributePermission.Write;
            case 32:
                return BluetoothLeAttributePermission.WriteEncrypted;
            case 64:
                return BluetoothLeAttributePermission.WriteEncryptedMitm;
            case 128:
                return BluetoothLeAttributePermission.WriteSigned;
            case 256:
                return BluetoothLeAttributePermission.WriteSignedMitm;
            default:
                throw new IllegalArgumentException("Unknown BluetoothLeAttributePermission value: " + v);
        }
    }
}