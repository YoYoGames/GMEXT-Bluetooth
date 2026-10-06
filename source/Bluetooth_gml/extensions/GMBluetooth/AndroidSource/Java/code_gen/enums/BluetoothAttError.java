// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.enums;

public enum BluetoothAttError
{
    Success((int)0),
    InvalidHandle((int)1),
    ReadNotPermitted((int)2),
    WriteNotPermitted((int)3),
    InvalidPdu((int)4),
    InsufficientAuthentication((int)5),
    RequestNotSupported((int)6),
    InvalidOffset((int)7),
    InsufficientAuthorization((int)8),
    PrepareQueueFull((int)9),
    AttributeNotFound((int)10),
    AttributeNotLong((int)11),
    InsufficientEncryptionKeySize((int)12),
    InvalidAttributeValueLength((int)13),
    UnlikelyError((int)14),
    InsufficientEncryption((int)15),
    UnsupportedGroupType((int)16),
    InsufficientResources((int)17);

    private final int value;
    private BluetoothAttError(int v)
    {
        this.value = v;
    }
    public int value()
    {
        return this.value;
    }
    public static BluetoothAttError from(int v)
    {
        switch (v)
        {
            case 0:
                return BluetoothAttError.Success;
            case 1:
                return BluetoothAttError.InvalidHandle;
            case 2:
                return BluetoothAttError.ReadNotPermitted;
            case 3:
                return BluetoothAttError.WriteNotPermitted;
            case 4:
                return BluetoothAttError.InvalidPdu;
            case 5:
                return BluetoothAttError.InsufficientAuthentication;
            case 6:
                return BluetoothAttError.RequestNotSupported;
            case 7:
                return BluetoothAttError.InvalidOffset;
            case 8:
                return BluetoothAttError.InsufficientAuthorization;
            case 9:
                return BluetoothAttError.PrepareQueueFull;
            case 10:
                return BluetoothAttError.AttributeNotFound;
            case 11:
                return BluetoothAttError.AttributeNotLong;
            case 12:
                return BluetoothAttError.InsufficientEncryptionKeySize;
            case 13:
                return BluetoothAttError.InvalidAttributeValueLength;
            case 14:
                return BluetoothAttError.UnlikelyError;
            case 15:
                return BluetoothAttError.InsufficientEncryption;
            case 16:
                return BluetoothAttError.UnsupportedGroupType;
            case 17:
                return BluetoothAttError.InsufficientResources;
            default:
                return null;
        }
    }
}