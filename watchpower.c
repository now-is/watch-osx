#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <stdio.h>
#include <strings.h>

IONotificationPortRef   notifyPortRef;
io_connect_t            root_port;
io_object_t             notifierObject;
void*                   ev = NULL;

void finish (char *ev);

void usage (char *prog) {
    fprintf(stderr, "Usage: %s [cansleep|willsleep|willpoweron|haspoweredon]\n", prog);
}

void cb (void *ev, io_service_t service, natural_t messageType, void *messageArgument) {

    switch (messageType) {
    case kIOMessageCanSystemSleep:
        IOAllowPowerChange(root_port, (long) messageArgument);
        if (!strncasecmp(ev, "cansleep", 16)) finish(ev);
        break;

    case kIOMessageSystemWillSleep:
        IOAllowPowerChange(root_port, (long) messageArgument);
        if (!strncasecmp(ev, "willsleep", 16)) finish(ev);
        break;

    case kIOMessageSystemWillPowerOn:
        if (!strncasecmp(ev, "willpoweron", 16)) finish(ev);
        break;

    case kIOMessageSystemHasPoweredOn:
        if (!strncasecmp(ev, "haspoweredon", 16)) finish(ev);
        break;

    default:
        break;
    }
}

void finish (char *ev) {
    printf("%s\n", ev);
    CFRunLoopRemoveSource(
        CFRunLoopGetCurrent(),
        IONotificationPortGetRunLoopSource(notifyPortRef),
        kCFRunLoopCommonModes
    );
    IODeregisterForSystemPower(&notifierObject);
    // IORegisterForSystemPower implicitly opened Root Power Domain IOService
    IOServiceClose(root_port);
    IONotificationPortDestroy(notifyPortRef);
    CFRunLoopStop(CFRunLoopGetMain());
}

int main (int argc, char **argv) {
    if (argc != 2 || !strncasecmp(argv[1], "-h", 16)) {
        usage (argv[0]);
        return 1;
    }

    root_port = IORegisterForSystemPower(argv[1], &notifyPortRef, cb, &notifierObject);
    if (!root_port) {
        fprintf(stderr, "IORegisterForSystemPower failed\n");
        return 1;
    }

    CFRunLoopAddSource(
        CFRunLoopGetCurrent(),
        IONotificationPortGetRunLoopSource(notifyPortRef),
        kCFRunLoopCommonModes
    );

    CFRunLoopRun();
    return (0);
}

