#include <CoreServices/CoreServices.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define NUM_EVENT_FDS   16
#define NUM_EVENT_SLOTS 1

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
 
char *flagstring (int flags) {
    static char ret[64];
    static char *or = "";

# define retcat(f,s) if (flags & (f)) { strcat(ret, or); strcat(ret, (s)); or = "|"; }

    ret[0]='\0';
    retcat(NOTE_ATTRIB,	"ATTRIB")
    retcat(NOTE_DELETE,	"DELETE")
    retcat(NOTE_EXTEND,	"EXTEND")
    retcat(NOTE_LINK,	"LINK")
    retcat(NOTE_RENAME,	"RENAME")
    retcat(NOTE_REVOKE,	"REVOKE")
    retcat(NOTE_WRITE,	"WRITE")
    return ret;
}

int watch_dirs (int num, CFStringRef dirs[]) {
    int rc = 0;
#define die(e...) do { rc = ~0; fprintf(stderr, e); goto finally; } while (0)

    CFArrayRef paths = CFArrayCreate(NULL, (const void **)dirs, num, NULL);

    FSEventStreamRef stream = FSEventStreamCreate(
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

/* taken from
 * https://developer.apple.com/library/mac/documentation/Darwin/Conceptual/FSEvents_ProgGuide/KernelQueues/KernelQueues.html
 */

int watch_files (int num, struct kevent ev_fds []) {
    int rc = 0;
#define die(e...) do { rc = ~0; fprintf(stderr, e); goto finally; } while (0)

    int kq;
    struct kevent ev_data[NUM_EVENT_SLOTS];
    if ((kq = kqueue()) < 0)
        die("Could not open kernel queue : %s.\n", strerror(errno));
 
    // handle NUM_EVENT_SLOTS events, i.e. first event
    // NULL means block indefinitely
    int event_count = kevent(
        kq, ev_fds, num, ev_data, NUM_EVENT_SLOTS, NULL
    );

    if ((event_count < 0) || (ev_data[0].flags == EV_ERROR))
        die("Event count %d : %s.\n", event_count, strerror(errno));

    if (event_count) {
        printf(
            "%s\t%s\n",
            flagstring(ev_data[0].fflags),
            (char *) ev_data[0].udata // path
        );
    }

finally:
    // in normal circumstances, there are num fds
    // in abnormal circumstances, num is 0 anyway
    for (num -= 1; num >= 0; num--)
        if (ev_fds[num].ident) close(ev_fds[num].ident);
    return rc;
}

int main (int argc, char **argv) {
    int rc = 0;
#define die(e...) do { rc = ~0; fprintf(stderr, e); goto finally; } while (0)

    if (argc < 2 || argc > 1 + NUM_EVENT_FDS)
        die("Usage: %s path1 [... path%d]\n", argv[0], NUM_EVENT_FDS);

    int ev_fd = 0, num_dirs = 0, num_files = 0;
    struct stat st;
    struct kevent ev_fds[NUM_EVENT_FDS];
    CFStringRef dirs[NUM_EVENT_FDS];

    int num;
    for (num = 1; num < argc; num++) {

        if (stat(argv[num], &st))
            fprintf(stderr, "Ignoring %s : Could not determine type.\n", argv[num]);

        if (st.st_mode & S_IFDIR) {
            dirs[num_dirs] = CFStringCreateWithCString(NULL, argv[num], kCFStringEncodingUTF8);
            num_dirs++;
        }
        else if (st.st_mode & S_IFREG) {
            ev_fd = open(argv[num], O_EVTONLY);
            if (ev_fd <= 0)
                die("%s : %s.\n", argv[num], strerror(errno));

            // monitor a list of events
            EV_SET(
                &ev_fds[num_files],
                ev_fd,
                EVFILT_VNODE,           // expect an fd in ev_fd and watch it
                EV_ADD | EV_CLEAR,      // add events, clear them when retrieved
                NOTE_ATTRIB | NOTE_DELETE | NOTE_EXTEND | NOTE_LINK
                    | NOTE_RENAME | NOTE_REVOKE | NOTE_WRITE,
                0,
                argv[num]               // that's all we need as "user data"
            );
            num_files++;
        }
        else {
            fprintf(stderr, "Ignoring %s : Could not determine type.\n", argv[num]);
        }
    }

    pid_t pid, pid_files = -2, pid_dirs = -2;

    if (num_files) {
        pid_files = fork();
        if (pid_files == -1) die("Could not spawn file watcher.\n");
        if (!pid_files) exit(watch_files(num_files, ev_fds));
    }

    if (num_dirs) {
        pid_dirs = fork();
        if (pid_dirs == -1) die("Could not spawn directory watcher.\n");
        if (!pid_dirs) exit(watch_dirs(num_dirs, dirs));
    }

    int stat_loc;
    pid = wait(&stat_loc);
    if (pid_dirs > 0 && (pid != pid_dirs)) kill(pid_dirs, SIGTERM);
    if (pid_files > 0 && (pid != pid_files)) kill(pid_files, SIGTERM);

finally:
    for (num = 0; num < num_files; num++)
        if (ev_fds[num].ident) close(ev_fds[num].ident);
    exit(rc);
}

int spawn (int f ()) {
    return 0;
}
