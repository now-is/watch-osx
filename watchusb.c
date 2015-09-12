/*
    From File:  https://developer.apple.com/library/mac/samplecode/USBPrivateDataSample/Listings/USBPrivateDataSample_c.html
    Copyright:  © Copyright 2001-2006 Apple Computer, Inc. All rights reserved.
*/

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

typedef struct MyPrivateData {
    int         doNotReport;
    io_object_t notification;
    CFStringRef deviceName;
} MyPrivateData;

static IONotificationPortRef    gNotifyPort;
static io_iterator_t            gAddedIter;
static CFRunLoopRef             gRunLoop;

//================================================================================================
//  Other messages are defined in IOMessage.h.
//================================================================================================

void DeviceNotification (void *refCon, io_service_t service, natural_t messageType, void *messageArgument) {
    kern_return_t   kr;
    MyPrivateData   *privateDataRef = (MyPrivateData *) refCon;

    if (messageType == kIOMessageServiceIsTerminated) {
        if (!privateDataRef->doNotReport) {
            printf("-%s\n", CFStringGetCStringPtr(privateDataRef->deviceName, kCFStringEncodingUTF8));
        }
        CFRelease(privateDataRef->deviceName);
        kr = IOObjectRelease(privateDataRef->notification);
        free(privateDataRef);
    }
}

//================================================================================================
//  Callback for our IOServiceAddMatchingNotification.
//  We will look at all the devices that were added and we will:
//
//  1.  Create some private data to relate to each device
//  2.  Submit an IOServiceAddInterestNotification of type kIOGeneralInterest for this device,
//      using the refCon field to store a pointer to our private data.  When we get called with
//      this interest notification, we can grab the refCon and access our private data.
//================================================================================================
void DeviceAdded (void *refCon, io_iterator_t iterator) {
    kern_return_t       kr;
    io_service_t        usbDevice;
    IOCFPlugInInterface **plugInInterface = NULL;
    SInt32              score;
    HRESULT             res;

    while ((usbDevice = IOIteratorNext(iterator))) {
        io_name_t       deviceName;
        MyPrivateData   *privateDataRef = NULL;

        // Add some app-specific information about this device.
        // Create a buffer to hold the data.
        privateDataRef = malloc(sizeof(MyPrivateData));
        bzero(privateDataRef, sizeof(MyPrivateData));

        // Get the USB device's name.
        kr = IORegistryEntryGetName(usbDevice, deviceName);
        if (KERN_SUCCESS != kr) {
            deviceName[0] = '\0';
        }

        // Save the device's name to our private data.
        privateDataRef->deviceName = CFStringCreateWithCString(
            kCFAllocatorDefault, deviceName, kCFStringEncodingUTF8
        );
        if (refCon) {
            privateDataRef->doNotReport = ((MyPrivateData *)refCon)->doNotReport;
        }

        // notify to stderr
        if (!privateDataRef->doNotReport) {
            printf("+%s\n", CFStringGetCStringPtr(privateDataRef->deviceName, kCFStringEncodingUTF8));
        }
        privateDataRef->doNotReport = 0;

        // Register for an interest notification of this device being removed. Use a reference to our
        // private data as the refCon which will be passed to the notification callback.
        kr = IOServiceAddInterestNotification(
            gNotifyPort,                    // notifyPort
            usbDevice,                      // service
            kIOGeneralInterest,             // interestType
            DeviceNotification,             // callback
            privateDataRef,                 // refCon
            &(privateDataRef->notification) // notification
        );

        if (KERN_SUCCESS != kr) {
            fprintf(stderr, "IOServiceAddInterestNotification returned 0x%08x.\n", kr);
        }

        // Done with this USB device; release the reference added by IOIteratorNext
        kr = IOObjectRelease(usbDevice);
    }
}

void SignalHandler (int sigraised) {
    fprintf(stderr, "\nInterrupted.\n");
    exit(0);
}

int main (int argc, const char *argv[]) {
    CFMutableDictionaryRef  matchingDict;
    CFRunLoopSourceRef      runLoopSource;
    CFNumberRef             numberRef;
    kern_return_t           kr;
    sig_t                   oldHandler;

    // Unless interrutped, stay in our run loop forever.
    oldHandler = signal(SIGINT, SignalHandler);
    if (oldHandler == SIG_ERR) {
        fprintf(stderr, "Could not establish new signal handler.");
    }

    // Set up the matching criteria for the devices we're interested in. The matching criteria needs to follow
    // the same rules as kernel drivers: mainly it needs to follow the USB Common Class Specification, pp. 6-7.
    // See also Technical Q&A QA1076 "Tips on USB driver matching on Mac OS X"
    // <http://developer.apple.com/qa/qa2001/qa1076.html>.
    //
    // NOTE: One exception is that you can use the matching dictionary "as is", i.e. without adding any matching
    // criteria to it and it will match every IOUSBDevice in the system. IOServiceAddMatchingNotification will
    // consume this dictionary reference, so there is no need to release it later on.

    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);    // Interested in instances of class
                                                                // IOUSBDevice and its subclasses
    if (matchingDict == NULL) {
        fprintf(stderr, "IOServiceMatching returned NULL.\n");
        return -1;
    }

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.

    gNotifyPort     = IONotificationPortCreate(kIOMasterPortDefault);
    runLoopSource   = IONotificationPortGetRunLoopSource(gNotifyPort);
    gRunLoop        = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);

    // notification to be called when a device is first matched by I/O Kit
    kr = IOServiceAddMatchingNotification(
        gNotifyPort,                  // notifyPort
        kIOFirstMatchNotification,    // notificationType
        matchingDict,                 // matching
        DeviceAdded,                  // callback
        NULL,                         // refCon XXX : free on exit
        &gAddedIter                   // notification
    );

    // get already-present devices and arm the notification, do not report
    MyPrivateData *privateDataRef = malloc(sizeof(MyPrivateData));
    bzero(privateDataRef, sizeof(MyPrivateData));
    privateDataRef->doNotReport = 1;
    DeviceAdded(privateDataRef, gAddedIter);

    // receive notifications
    CFRunLoopRun();

    // We should never get here
    fprintf(stderr, "Unexpectedly back from CFRunLoopRun()!\n");
    return 0;
}
