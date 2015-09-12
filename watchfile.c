/* taken from
 * https://developer.apple.com/library/mac/documentation/Darwin/Conceptual/FSEvents_ProgGuide/KernelQueues/KernelQueues.html
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/wait.h>
#include <unistd.h>
 
#define NUM_EVENT_FDS   16
#define NUM_EVENT_SLOTS 1

char *flagstring (int flags);
 
int main (int argc, char **argv) {
    int ev_fd = 0, num = 0, kq;
    struct kevent ev_fds[NUM_EVENT_FDS];
    struct kevent ev_data[NUM_EVENT_SLOTS];
 
#define die(e...) do { rc = ~0; fprintf(stderr, e); goto finally; } while (0)

    int rc = 0;
    if (argc < 2 || argc > 1 + NUM_EVENT_FDS)
        die("Usage: %s file1 [... file%d]\n", argv[0], NUM_EVENT_FDS);

    if ((kq = kqueue()) < 0)
        die("Could not open kernel queue : %s.\n", strerror(errno));
 
    for (num = 1; num < argc; num++) {
        if (num > NUM_EVENT_FDS) break;
        ev_fd = open(argv[num], O_EVTONLY);
        if (ev_fd <= 0)
            die("%s : %s.\n", argv[num], strerror(errno));

        // monitor a list of events
        EV_SET(
            &ev_fds[num-1],
            ev_fd,
            EVFILT_VNODE,           // expect an fd in ev_fd and watch it
            EV_ADD | EV_CLEAR,      // add events, clear them when retrieved
            NOTE_ATTRIB | NOTE_DELETE | NOTE_EXTEND | NOTE_LINK
                | NOTE_RENAME | NOTE_REVOKE | NOTE_WRITE,
            0,
            argv[num]               // that's all we need as "user data"
        );
    }

    num--;

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
 
// string for a set of flags
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
