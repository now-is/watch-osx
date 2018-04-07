#include <CoreServices/CoreServices.h>
#include <sys/event.h>

#define NUM_EVENT_FDS   16

void cb (
    ConstFSEventStreamRef s,
    void *info,
    size_t num,
    void *eventPaths,
    const FSEventStreamEventFlags f[],
    const FSEventStreamEventId id[]
) {
    char **paths = eventPaths;
    for (int i = 0; i < num; i++)
        printf("DIRMOD\t%s\n", paths[i]);
    CFRunLoopStop(CFRunLoopGetMain());
}

int main (int argc, char **argv) {
    int rc = 0;
#define die(e...) do { rc = ~0; fprintf(stderr, e); goto finally; } while (0)

    FSEventStreamRef stream = NULL;

    if (argc < 2 || argc > 1 + NUM_EVENT_FDS)
        die("Usage: %s dir1 [... dir%d]\n", argv[0], NUM_EVENT_FDS);

    int num;
    CFStringRef dirs[NUM_EVENT_FDS];
    for (num = 1; num < argc; num++) {
        if (num > NUM_EVENT_FDS) break;
        dirs[num-1] = CFStringCreateWithCString(NULL, argv[num], kCFStringEncodingUTF8);
    }
    num--;

    CFArrayRef paths = CFArrayCreate(NULL, (const void **)dirs, num, NULL);

    stream = FSEventStreamCreate(
        NULL, // default memory allocator
        &cb,
        NULL, // no cbinfo
        paths,
        kFSEventStreamEventIdSinceNow,
        0.1, // we exit at the first event, no need to "debounce" cb
        kFSEventStreamCreateFlagNone
    );

    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);
    CFRunLoopRun();

finally:
    if (stream) FSEventStreamRelease(stream);
    return rc;
}

