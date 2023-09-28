
#include "BluetoothTools.h"


char* GUIDtoString(GUID guid)
{
    char* str = (char*)malloc(35+1);
    sprintf(str, "%02X-%02X-%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        guid.Data1,
        guid.Data2,
        guid.Data3,
        guid.Data4[0],
        guid.Data4[1],
        guid.Data4[2],
        guid.Data4[3],
        guid.Data4[4],
        guid.Data4[5],
        guid.Data4[6],
        guid.Data4[7]);

    return str;
}

//https://stackoverflow.com/a/4341294
GUID StringToGUID(char* str)
{
    GUID guid;

    unsigned long p0;
    int p1, p2, p3, p4, p5, p6, p7, p8, p9, p10;

    int err = sscanf_s(str, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        &p0, &p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9, &p10);

    guid.Data1 = p0;
    guid.Data2 = p1;
    guid.Data3 = p2;
    guid.Data4[0] = p3;
    guid.Data4[1] = p4;
    guid.Data4[2] = p5;
    guid.Data4[3] = p6;
    guid.Data4[4] = p7;
    guid.Data4[5] = p8;
    guid.Data4[6] = p9;
    guid.Data4[7] = p10;

    return guid;
}

std::string AddressToString(const BYTE bytes[])
{
    char address[18];
    sprintf(address, "%02X:%02X:%02X:%02X:%02X:%02X",
        bytes[5],
        bytes[4],
        bytes[3],
        bytes[2],
        bytes[1],
        bytes[0]);

    return address;
}

BTH_ADDR StringToAddress(const std::string& str) {
    int b[6];
    sscanf(str.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &b[5], &b[4], &b[3], &b[2], &b[1], &b[0]);
    BTH_ADDR address = ((ULONGLONG)b[5] << 40) |
        ((ULONGLONG)b[4] << 32) |
        ((ULONGLONG)b[3] << 24) |
        ((ULONGLONG)b[2] << 16) |
        ((ULONGLONG)b[1] << 8) |
        (ULONGLONG)b[0];
    return address;
}

std::string AddressToString(BTH_ADDR address) {
    char buffer[18];
    sprintf(buffer, "%02x:%02x:%02x:%02x:%02x:%02x",
        (int)((address >> 40) & 0xff),
        (int)((address >> 32) & 0xff),
        (int)((address >> 24) & 0xff),
        (int)((address >> 16) & 0xff),
        (int)((address >> 8) & 0xff),
        (int)(address & 0xff));
    return buffer;
}