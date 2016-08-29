import ctypes
import os
import time

MILD_USAGE_PERCENT = 3
MEDIUM_USAGE_PERCENT = 30
HEAVY_USAGE_PERCENT = 75

##
## Define some Minoca specific C constants.
##

SystemInformationKe = 1
KeInformationProcessorUsage = 4

STATUS_SUCCESS = 0L
STATUS_NOT_FOUND = -2L
STATUS_BUFFER_TOO_SMALL = -35L

SYS_OPEN_FLAG_READ = 0x00000010L
SYS_OPEN_FLAG_WRITE = 0x00000020L

USB_RELAY_DEVICE_INFORMATION_UUID = \
    "\xE4\x1C\x2C\x99\x40\x4B\x22\x66\x73\xF4\x9B\xA1\xC8\xA3\x9E\xAF"

USB_LED_DEVICE_INFORMATION_UUID = \
    "\xE4\x1C\x2C\x99\x40\x4B\x22\x66\x73\xF4\x9B\xA1\xC9\xA3\x9E\xAF"
    
##
## Define some Minoca specific structures.
##

class PROCESSOR_CYCLE_ACCOUNTING(ctypes.Structure):
    _fields_ = [
        ("user_cycles", ctypes.c_ulonglong),
        ("kernel_cycles", ctypes.c_ulonglong),
        ("interrupt_cycles", ctypes.c_ulonglong),
        ("idle_cycles", ctypes.c_ulonglong)
    ]
    
class PROCESSOR_USAGE_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("processor_number", ctypes.c_ulong),
        ("cycle_counter_frequency", ctypes.c_ulonglong),
        ("usage", PROCESSOR_CYCLE_ACCOUNTING),
    ]
    
class DEVICE_INFORMATION_RESULT(ctypes.Structure):
    _fields_ = [
        ("uuid", ctypes.c_ubyte * 16),
        ("device_id", ctypes.c_ulonglong)
    ]
   
class ONE_RING_DEVICE_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("device_type", ctypes.c_uint),
        ("serial_number", ctypes.c_char * 16)
    ]
    
def minoca_get_set_system_information(c_subsystem, 
                                      c_information_type, 
                                      c_data, 
                                      c_size, 
                                      c_set):
                                      
    libminocaos = ctypes.cdll.LoadLibrary("libminocaos.so")
    get_set_system_information = libminocaos.OsGetSetSystemInformation
    get_set_system_information.argtypes = [ctypes.c_int, 
                                           ctypes.c_int, 
                                           ctypes.c_void_p, 
                                           ctypes.POINTER(ctypes.c_uint), 
                                           ctypes.c_int]
                                           
    get_set_system_information.restype = ctypes.c_ulong
    result = get_set_system_information(c_subsystem, 
                                        c_information_type, 
                                        c_data, 
                                        ctypes.byref(c_size), 
                                        c_set)
                                        
    return result
    
def minoca_locate_device_information(c_uuid,
                                     c_deviceid,
                                     c_results,
                                     c_resultcount):

    libminocaos = ctypes.cdll.LoadLibrary("libminocaos.so")
    locate_device_information = libminocaos.OsLocateDeviceInformation
    locate_device_information.argtypes = \
                                     [ctypes.c_char_p,
                                      ctypes.POINTER(ctypes.c_ulonglong),
                                      ctypes.POINTER(DEVICE_INFORMATION_RESULT),
                                      ctypes.POINTER(ctypes.c_int)]

    locate_device_information.restype = ctypes.c_ulong
    deviceid_pointer = None
    if c_deviceid is not None:
        deviceid_pointer = ctypes.byref(c_deviceid)

    return locate_device_information(c_uuid,
                                     deviceid_pointer,
                                     c_results,
                                     ctypes.byref(c_resultcount))

def minoca_open_device_id(c_device_id, 
                          c_flags, 
                          c_handle):

    libminocaos = ctypes.cdll.LoadLibrary("libminocaos.so")
    open_device_id = libminocaos.OsOpenDevice
    open_device_id.argtypes = [ctypes.c_ulonglong, 
                               ctypes.c_ulong, 
                               ctypes.POINTER(ctypes.c_int)]
                               
    open_device_id.restype = ctypes.c_ulong
    return open_device_id(c_device_id, c_flags, ctypes.byref(c_handle))
    
def minoca_get_usb_device_id(uuid):
    size = ctypes.c_long(5)
    result_array = DEVICE_INFORMATION_RESULT * size.value
    information_uuid = ctypes.create_string_buffer(uuid)
                
    device_results = result_array()
    result = minoca_locate_device_information(information_uuid,
                                              None,
                                              device_results,
                                              size)
                                              
    if result == STATUS_BUFFER_TOO_SMALL:
        result_array = DEVICE_INFORMATION_RESULT * size.value
        device_results = result_array()
        result = minoca_locate_device_information(information_uuid,
                                          None,
                                          device_results,
                                          size)
                                          
    if result != STATUS_SUCCESS:
        print("Failed to get USB Device: %d" % result)
        return None
        
    if size.value < 1:
        print("No device connected.")
        return None
        
    device_id = device_results[0].device_id
    print("USB device has device ID %x" % device_id)
    return device_id
    
def minoca_open_usb_relay():
    return minoca_open_usb_device(USB_RELAY_DEVICE_INFORMATION_UUID)
    
def minoca_open_usb_led():
    return minoca_open_usb_device(USB_LED_DEVICE_INFORMATION_UUID)
    
def minoca_open_usb_device(uuid):
    device_id = minoca_get_usb_device_id(uuid)
    if device_id is None:
        return None
        
    flags = SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE
    handle = ctypes.c_int(-1)
    result = minoca_open_device_id(device_id, flags, handle)
    if result != STATUS_SUCCESS:
        print("Failed to open device ID %x: %d\n" % (device_id, result))
        
    file_handle = os.fdopen(handle.value, 'wb+', 0)
    if file_handle is None:
        print("fdopen failed.\n")
        
    return file_handle
    
def minoca_get_processor_usage(processor=-1):
    information = PROCESSOR_USAGE_INFORMATION()
    information.processor_number = processor                                      
    size = ctypes.c_uint(ctypes.sizeof(information))
    status = minoca_get_set_system_information(SystemInformationKe, 
                                               KeInformationProcessorUsage, 
                                               ctypes.byref(information), 
                                               size, 
                                               0)
                                               
    if status != STATUS_SUCCESS:
        print("Failed to get processor usage: %d" % status)
        return None
        
    return information
    
if __name__ == '__main__':
    previous_information = minoca_get_processor_usage()
    relay = minoca_open_usb_relay()
    led = minoca_open_usb_led()
    while True:
        time.sleep(1)
        information = minoca_get_processor_usage()
        kdelta = information.usage.kernel_cycles - \
                 previous_information.usage.kernel_cycles
                 
        udelta = information.usage.user_cycles - \
                 previous_information.usage.user_cycles
                 
        edelta = information.usage.interrupt_cycles - \
                 previous_information.usage.interrupt_cycles
                 
        idelta = information.usage.idle_cycles - \
                 previous_information.usage.idle_cycles
                 
        tdelta = kdelta + udelta + edelta + idelta
        
        if tdelta == 0:
            continue
        
        busy_percent = (kdelta + udelta + edelta) * 100.0 / tdelta
        if busy_percent < MILD_USAGE_PERCENT:
            light = 0x0
            
        elif busy_percent < MEDIUM_USAGE_PERCENT:
            light = 0x4
            
        elif busy_percent < HEAVY_USAGE_PERCENT:
            light = 0x6
            
        else:
            light = 0x7
              
        if led is not None:
            string = '% 5.1f% 5.1f' % \
                     (kdelta * 100.0 / tdelta, udelta * 100.0 / tdelta)
                     
            led.write(string)
            
        if relay is not None:
            relay.write(bytearray([light]))
            
        else:
            print("U %.1f K %.1f E %.1f I %.1f: %.1f %d" % (
                  kdelta * 100.0 / tdelta,
                  udelta * 100.0 / tdelta,
                  edelta * 100.0 / tdelta,
                  idelta * 100.0 / tdelta,
                  busy_percent,
                  light))
            
        previous_information = information

    relay.close()
    led.close()
    