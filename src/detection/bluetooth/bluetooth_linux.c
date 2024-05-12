#include "bluetooth.h"
#include "util/stringUtils.h"

#ifdef FF_HAVE_DBUS
#include "common/dbus.h"

/* Example dbus reply, striped to only the relevant parts:
array [                                                     //root
    dict entry(                                             //object
        object path "/org/bluez/hci0/dev_03_21_8B_91_16_4D"
        array [
           dict entry(                                      //property
              string "org.bluez.Device1"
              array [
                 dict entry(                                //value
                    string "Address"
                    variant string "03:21:8B:91:16:4D"
                 )
                 dict entry(                                //value
                    string "Name"
                    variant string "JBL TUNE160BT"
                 )
                 dict entry(                                //value
                    string "Icon"
                    variant string "audio-headset"
                 )
                 dict entry(                                //value
                    string "Connected"
                    variant boolean true
                 )
              ]
           )
           dict entry(                                      //property
              string "org.bluez.Battery1"
              array [
                 dict entry(                                //value
                    string "Percentage"
                    variant byte 100
                 )
              ]
           )
        ]
    )
]
*/

static void detectBluetoothValue(FFDBusData* dbus, DBusMessageIter* iter, FFBluetoothResult* device)
{
    if(dbus->lib->ffdbus_message_iter_get_arg_type(iter) != DBUS_TYPE_DICT_ENTRY)
        return;

    DBusMessageIter dictIter;
    dbus->lib->ffdbus_message_iter_recurse(iter, &dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_STRING)
        return;

    const char* deviceProperty;
    dbus->lib->ffdbus_message_iter_get_basic(&dictIter, &deviceProperty);

    dbus->lib->ffdbus_message_iter_next(&dictIter);

    if(ffStrEquals(deviceProperty, "Address"))
        ffDBusGetValue(dbus, &dictIter, &device->address);
    else if(ffStrEquals(deviceProperty, "Name"))
        ffDBusGetValue(dbus, &dictIter, &device->name);
    else if(ffStrEquals(deviceProperty, "Icon"))
        ffDBusGetValue(dbus, &dictIter, &device->type);
    else if(ffStrEquals(deviceProperty, "Percentage"))
        ffDBusGetByte(dbus, &dictIter, &device->battery);
    else if(ffStrEquals(deviceProperty, "Connected"))
        ffDBusGetBool(dbus, &dictIter, &device->connected);
}

static void detectBluetoothProperty(FFDBusData* dbus, DBusMessageIter* iter, FFBluetoothResult* device)
{
    if(dbus->lib->ffdbus_message_iter_get_arg_type(iter) != DBUS_TYPE_DICT_ENTRY)
        return;

    DBusMessageIter dictIter;
    dbus->lib->ffdbus_message_iter_recurse(iter, &dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_STRING)
        return;

    const char* propertyType;
    dbus->lib->ffdbus_message_iter_get_basic(&dictIter, &propertyType);

    if(strstr(propertyType, "Device") == NULL && strstr(propertyType, "Battery") == NULL)
        return; //We don't care about other properties

    dbus->lib->ffdbus_message_iter_next(&dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_ARRAY)
        return;

    DBusMessageIter arrayIter;
    dbus->lib->ffdbus_message_iter_recurse(&dictIter, &arrayIter);

    while(true)
    {
        detectBluetoothValue(dbus, &arrayIter, device);
        FF_DBUS_ITER_CONTINUE(dbus, &arrayIter);
    }
}

static void detectBluetoothObject(FFlist* devices, FFDBusData* dbus, DBusMessageIter* iter)
{
    if(dbus->lib->ffdbus_message_iter_get_arg_type(iter) != DBUS_TYPE_DICT_ENTRY)
        return;

    DBusMessageIter dictIter;
    dbus->lib->ffdbus_message_iter_recurse(iter, &dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_OBJECT_PATH)
        return;

    const char* objectPath;
    dbus->lib->ffdbus_message_iter_get_basic(&dictIter, &objectPath);

    //We don't want adapter objects
    if(strstr(objectPath, "dev_") == NULL)
        return;

    dbus->lib->ffdbus_message_iter_next(&dictIter);

    if(dbus->lib->ffdbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_ARRAY)
        return;

    DBusMessageIter arrayIter;
    dbus->lib->ffdbus_message_iter_recurse(&dictIter, &arrayIter);

    FFBluetoothResult* device = ffListAdd(devices);
    ffStrbufInit(&device->name);
    ffStrbufInit(&device->address);
    ffStrbufInit(&device->type);
    device->battery = 0;
    device->connected = false;

    while(true)
    {
        detectBluetoothProperty(dbus, &arrayIter, device);
        FF_DBUS_ITER_CONTINUE(dbus, &arrayIter);
    }

    if(device->name.length == 0)
    {
        ffStrbufDestroy(&device->name);
        ffStrbufDestroy(&device->address);
        ffStrbufDestroy(&device->type);
        --devices->length;
    }
}

static void detectBluetoothRoot(FFlist* devices, FFDBusData* dbus, DBusMessageIter* iter)
{
    if(dbus->lib->ffdbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
        return;

    DBusMessageIter arrayIter;
    dbus->lib->ffdbus_message_iter_recurse(iter, &arrayIter);

    while(true)
    {
        detectBluetoothObject(devices, dbus, &arrayIter);
        FF_DBUS_ITER_CONTINUE(dbus, &arrayIter);
    }
}

static const char* detectBluetooth(FFlist* devices)
{
    FFDBusData dbus;
    const char* error = ffDBusLoadData(DBUS_BUS_SYSTEM, &dbus);
    if(error)
        return error;

    DBusMessage* managedObjects = ffDBusGetMethodReply(&dbus, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if(!managedObjects)
        return "Failed to call GetManagedObjects";

    DBusMessageIter rootIter;
    if(!dbus.lib->ffdbus_message_iter_init(managedObjects, &rootIter))
    {
        dbus.lib->ffdbus_message_unref(managedObjects);
        return "Failed to get root iterator of GetManagedObjects";
    }

    detectBluetoothRoot(devices, &dbus, &rootIter);

    dbus.lib->ffdbus_message_unref(managedObjects);
    return NULL;
}

#endif

const char* ffDetectBluetooth(FF_MAYBE_UNUSED FFlist* devices /* FFBluetoothResult */)
{
    #ifdef FF_HAVE_DBUS
        return detectBluetooth(devices);
    #else
        return "Fastfetch was compiled without DBus support";
    #endif
}
