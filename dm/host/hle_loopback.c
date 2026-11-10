// UDP between the game's own sockets, for games that talk to themselves over 127.0.0.1 (profile key net_loopback, DM_NET_LOOPBACK overrides it).
// A Switch with no network still has the bsd service's loopback: socket, bind and datagrams to 127.0.0.1 work, and only traffic to another host fails. Divinity: Original
// Sin 2 runs single player as a RakNet server and a client in one process: on New Game its server (RakPeer on the ServerWorker thread, main+0x1adc06c) opens a UDP socket,
// and with every socket call failing (hle_offline.c, hle_net.c) it never had one: the server sat in its LoadSession state and the client in StartServer, at the loading
// screen, forever. The datagrams go from socket to socket inside the host: nothing reaches the device's network, and a destination outside 127.0.0.0/8 fails with
// ENETUNREACH as on a console with no network. Only what RakNet's Berkeley sockets use is here: Socket, Bind, GetSockName, SendTo, RecvFrom, Set/GetSockOpt, Fcntl,
// Close and Poll on AF_INET datagram sockets; any other socket keeps failing with ENETDOWN.
#include "hle.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

enum { LB_BASE = 0x200, LB_MAX = 64,   // descriptors 512..575, apart from the event descriptors (hle_net.c numbers those from 0, below 64, and its Select sees only those: a loopback socket is waited on with Poll or a blocking RecvFrom; RakNet blocks in RecvFrom)
       LB_QMAX = 512, LB_QBYTES = 1 << 20, LB_DGRAM_MAX = 65507, LB_EPHEMERAL = 49152 };
// FreeBSD's numbers, which Horizon's bsd service uses
enum { E_BADF = 9, E_INVAL = 22, E_AGAIN = 35, E_MSGSIZE = 40, E_AFNOSUPPORT = 47, E_ADDRINUSE = 48, E_ADDRNOTAVAIL = 49, E_NETDOWN = 50, E_NETUNREACH = 51,
       E_MFILE = 24, E_NOTCONN = 57 };
enum { AF_INET_ = 2, SOCK_DGRAM_ = 2, IPPROTO_UDP_ = 17, SOL_SOCKET_ = 0xffff, SO_REUSEADDR_ = 4, SO_BROADCAST_ = 0x20, SO_SNDBUF_ = 0x1001, SO_RCVBUF_ = 0x1002,
       SO_RCVTIMEO_ = 0x1006, SO_ERROR_ = 0x1007, SO_TYPE_ = 0x1008, MSG_PEEK_ = 2, MSG_DONTWAIT_ = 0x80, F_GETFL_ = 3, F_SETFL_ = 4, O_RDWR_ = 2, O_NONBLOCK_ = 4,
       POLLIN_ = 1, POLLOUT_ = 4, POLLRDNORM_ = 0x40, POLLNVAL_ = 0x20 };

typedef struct { uint8_t len, family; uint16_t port; uint32_t addr; uint8_t zero[8]; } sk_in;   // sockaddr_in as Horizon lays it out; port and address in network order
typedef struct dgram { struct dgram *next; uint32_t from_addr; uint16_t from_port; uint32_t n; uint8_t data[]; } dgram;
static struct { int used, bound, nonblock, sndbuf, rcvbuf; uint32_t addr; uint16_t port; int64_t rcvtimeo_ns; dgram *head, *tail; int nq; size_t qbytes; } g_lb[LB_MAX];
static pthread_mutex_t g_lb_m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_lb_c = PTHREAD_COND_INITIALIZER;
static __thread int t_lberr;   // the last loopback call's error on this thread; 0 = none, and GetLastErrno says ENETDOWN as for the other socket calls
static uint16_t g_next_port = LB_EPHEMERAL;

void dm_guest_cond_wait_ns(pthread_cond_t *c, pthread_mutex_t *m, int64_t ns);   // hle_thread.c

int dm_lb_enabled(void) {
    static int on = -1;
    if (on < 0) { const char *e = getenv("DM_NET_LOOPBACK"); on = e ? atoi(e) != 0 : DM_GAME_NET_LOOPBACK;
        if (on) LOG("socket: UDP between the game's own sockets on 127.0.0.1 (net_loopback; nothing reaches the network)"); }
    return on;
}
int dm_lb_errno(void) { return t_lberr ? t_lberr : E_NETDOWN; }
void dm_lb_offline_call(void) { t_lberr = 0; }   // a socket call that failed offline: GetLastErrno says ENETDOWN again
static int fail(int e) { t_lberr = e; return -1; }
static int slot(int fd) { return fd >= LB_BASE && fd < LB_BASE + LB_MAX && g_lb[fd - LB_BASE].used ? fd - LB_BASE : -1; }   // with g_lb_m held
int dm_lb_owns(int fd) { pthread_mutex_lock(&g_lb_m); int s = slot(fd); pthread_mutex_unlock(&g_lb_m); return s >= 0; }
static int local(uint32_t a_be) { uint32_t a = __builtin_bswap32(a_be); return (a >> 24) == 127 || a == 0; }   // 127.0.0.0/8, or 0.0.0.0, which BSD sends to this host
static int port_used(uint16_t port_be) { for (int i = 0; i < LB_MAX; i++) if (g_lb[i].used && g_lb[i].bound && g_lb[i].port == port_be) return 1; return 0; }
static void bind_ephemeral(int s) {   // with g_lb_m held: as the system does for a socket sending before any bind
    for (int k = 0; k < 65536 - LB_EPHEMERAL; k++) {
        uint16_t p = g_next_port; g_next_port = g_next_port == 65535 ? LB_EPHEMERAL : g_next_port + 1;
        if (!port_used(__builtin_bswap16(p))) { g_lb[s].port = __builtin_bswap16(p); g_lb[s].addr = 0; g_lb[s].bound = 1; return; }
    }
}
static void put_addr(void *a, unsigned *alen, uint32_t addr, uint16_t port) {
    if (!a || !alen) return;
    sk_in sa = {sizeof sa, AF_INET_, port, addr, {0}};
    memcpy(a, &sa, *alen < sizeof sa ? *alen : sizeof sa); *alen = sizeof sa;
}

// The sockets this takes: AF_INET datagram sockets, with net_loopback. The caller keeps its offline answer for every other kind
int dm_lb_takes(int domain, int type, int proto) { return dm_lb_enabled() && domain == AF_INET_ && (type & 0xffff) == SOCK_DGRAM_ && (!proto || proto == IPPROTO_UDP_); }
int dm_lb_socket(int domain, int type, int proto) {
    if (!dm_lb_takes(domain, type, proto)) return fail(E_AFNOSUPPORT);
    pthread_mutex_lock(&g_lb_m);
    for (int i = 0; i < LB_MAX; i++) if (!g_lb[i].used) {
        memset(&g_lb[i], 0, sizeof g_lb[i]); g_lb[i].used = 1; g_lb[i].nonblock = (type & 0x20000000) != 0;   // SOCK_NONBLOCK
        g_lb[i].sndbuf = g_lb[i].rcvbuf = 65536;
        pthread_mutex_unlock(&g_lb_m); t_lberr = 0;
        static int once; if (!once++) LOG("socket: loopback UDP socket %d made", LB_BASE + i);
        return LB_BASE + i;
    }
    pthread_mutex_unlock(&g_lb_m);
    LOG("[RED] socket: more than %d loopback sockets", LB_MAX);
    return fail(E_MFILE);
}
int dm_lb_bind(int fd, const void *addr, unsigned alen) {
    sk_in sa; if (!addr || alen < 8) return fail(E_INVAL);
    memset(&sa, 0, sizeof sa); memcpy(&sa, addr, alen < sizeof sa ? alen : sizeof sa);
    if (sa.family != AF_INET_) return fail(E_AFNOSUPPORT);
    if (!local(sa.addr)) return fail(E_ADDRNOTAVAIL);   // no interface but loopback
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd), r = 0;
    if (s < 0) r = fail(E_BADF);
    else if (g_lb[s].bound) r = fail(E_INVAL);
    else if (!sa.port) bind_ephemeral(s);
    else if (port_used(sa.port)) r = fail(E_ADDRINUSE);
    else { g_lb[s].port = sa.port; g_lb[s].addr = sa.addr; g_lb[s].bound = 1; }
    if (!r && s >= 0) { g_lb[s].addr = sa.addr; LOG("socket: loopback socket %d bound to port %u", fd, __builtin_bswap16(g_lb[s].port)); }
    pthread_mutex_unlock(&g_lb_m);
    if (!r) t_lberr = 0;
    return r;
}
int dm_lb_getsockname(int fd, void *addr, unsigned *alen) {
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd);
    if (s >= 0) put_addr(addr, alen, g_lb[s].addr, g_lb[s].port);
    pthread_mutex_unlock(&g_lb_m);
    if (s < 0) return fail(E_BADF);
    t_lberr = 0; return 0;
}
long dm_lb_sendto(int fd, const void *buf, unsigned long n, int flags, const void *to, unsigned tolen) {
    (void)flags;
    sk_in sa; memset(&sa, 0, sizeof sa);
    if (!to) return fail(E_NOTCONN);
    memcpy(&sa, to, tolen < sizeof sa ? tolen : sizeof sa);
    if (n > LB_DGRAM_MAX) return fail(E_MSGSIZE);
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd);
    if (s < 0) { pthread_mutex_unlock(&g_lb_m); return fail(E_BADF); }
    if (sa.family != AF_INET_ || !local(sa.addr)) {   // another host, or a broadcast: no route without a network
        pthread_mutex_unlock(&g_lb_m);
        static int once; if (!once++) LOG("socket: loopback socket %d sends to %u.%u.%u.%u, not on this host: ENETUNREACH", fd, sa.addr & 255, sa.addr >> 8 & 255, sa.addr >> 16 & 255, sa.addr >> 24);
        return fail(E_NETUNREACH);
    }
    if (!g_lb[s].bound) bind_ephemeral(s);
    int d = -1;
    for (int i = 0; i < LB_MAX; i++) if (g_lb[i].used && g_lb[i].bound && g_lb[i].port == sa.port) { d = i; break; }
    // A port nobody bound, or a full queue: the datagram is lost, as UDP's are, and the send still succeeds
    static int logged;   // the first datagrams, to see who talks to whom
    if (logged < 8) { logged++; LOG("socket: loopback %d (port %u) → port %u: %lu bytes%s", fd, __builtin_bswap16(g_lb[s].port), __builtin_bswap16(sa.port), n,
                                   d < 0 ? ", nobody bound there: lost" : g_lb[d].nq >= LB_QMAX || g_lb[d].qbytes + n > LB_QBYTES ? ", queue full: lost" : ""); }
    if (d >= 0 && g_lb[d].nq < LB_QMAX && g_lb[d].qbytes + n <= LB_QBYTES) {
        dgram *g = malloc(sizeof *g + n);
        if (g) {
            g->next = NULL; g->from_addr = __builtin_bswap32(0x7f000001); g->from_port = g_lb[s].port; g->n = (uint32_t)n; memcpy(g->data, buf, n);
            if (g_lb[d].tail) g_lb[d].tail->next = g; else g_lb[d].head = g;
            g_lb[d].tail = g; g_lb[d].nq++; g_lb[d].qbytes += n;
            pthread_cond_broadcast(&g_lb_c);
        }
    }
    pthread_mutex_unlock(&g_lb_m);
    t_lberr = 0; return (long)n;
}
long dm_lb_recvfrom(int fd, void *buf, unsigned long n, int flags, void *from, unsigned *fromlen) {
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd);
    if (s < 0) { pthread_mutex_unlock(&g_lb_m); return fail(E_BADF); }
    uint64_t start = guest_ns();
    while (!g_lb[s].head) {
        // A blocking socket waits, in slices like the event descriptors' reads so the thread gives its core up; SO_RCVTIMEO bounds the wait. Close wakes it with EBADF
        int64_t limit = g_lb[s].rcvtimeo_ns, left = limit ? limit - (int64_t)(guest_ns() - start) : 100000000;
        if (g_lb[s].nonblock || (flags & MSG_DONTWAIT_) || left <= 0) { pthread_mutex_unlock(&g_lb_m); return fail(E_AGAIN); }
        dm_guest_cond_wait_ns(&g_lb_c, &g_lb_m, dm_detclock_enabled() && left > 1000000 ? 1000000 : left < 100000000 ? left : 100000000);
        if ((s = slot(fd)) < 0) { pthread_mutex_unlock(&g_lb_m); return fail(E_BADF); }
    }
    dgram *g = g_lb[s].head;
    unsigned long k = g->n < n ? g->n : n;   // a short buffer gets the start of the datagram, as BSD does; the rest is lost
    if (buf && k) memcpy(buf, g->data, k);
    put_addr(from, fromlen, g->from_addr, g->from_port);
    if (!(flags & MSG_PEEK_)) {
        g_lb[s].head = g->next; if (!g->next) g_lb[s].tail = NULL;
        g_lb[s].nq--; g_lb[s].qbytes -= g->n; free(g);
    }
    pthread_mutex_unlock(&g_lb_m);
    t_lberr = 0; return (long)k;
}
int dm_lb_setsockopt(int fd, int level, int opt, const void *v, unsigned n) {
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd);
    if (s >= 0 && level == SOL_SOCKET_ && v) {
        if (opt == SO_RCVTIMEO_ && n >= 16) { int64_t tv[2]; memcpy(tv, v, 16); g_lb[s].rcvtimeo_ns = tv[0] * 1000000000 + tv[1] * 1000; }   // {seconds, microseconds}, 64-bit each
        else if ((opt == SO_RCVBUF_ || opt == SO_SNDBUF_) && n >= 4) { int x; memcpy(&x, v, 4); if (opt == SO_RCVBUF_) g_lb[s].rcvbuf = x; else g_lb[s].sndbuf = x; }
    }
    pthread_mutex_unlock(&g_lb_m);
    if (s < 0) return fail(E_BADF);
    t_lberr = 0; return 0;   // the rest (SO_REUSEADDR, SO_BROADCAST, SO_LINGER, IP options) changes nothing on loopback
}
int dm_lb_getsockopt(int fd, int level, int opt, void *v, unsigned *n) {
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd), x = 0;
    if (s >= 0 && level == SOL_SOCKET_) x = opt == SO_RCVBUF_ ? g_lb[s].rcvbuf : opt == SO_SNDBUF_ ? g_lb[s].sndbuf : opt == SO_TYPE_ ? SOCK_DGRAM_ : 0;   // SO_ERROR: none
    pthread_mutex_unlock(&g_lb_m);
    if (s < 0) return fail(E_BADF);
    if (v && n && *n >= 4) { memcpy(v, &x, 4); *n = 4; }
    t_lberr = 0; return 0;
}
int dm_lb_fcntl(int fd, int cmd, int arg) {
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd), r = 0;
    if (s < 0) r = fail(E_BADF);
    else if (cmd == F_GETFL_) r = O_RDWR_ | (g_lb[s].nonblock ? O_NONBLOCK_ : 0);
    else if (cmd == F_SETFL_) g_lb[s].nonblock = (arg & O_NONBLOCK_) != 0;
    pthread_mutex_unlock(&g_lb_m);
    if (r >= 0) t_lberr = 0;
    return r;
}
int dm_lb_close(int fd) {
    pthread_mutex_lock(&g_lb_m);
    int s = slot(fd);
    if (s >= 0) {
        for (dgram *g = g_lb[s].head, *nx; g; g = nx) { nx = g->next; free(g); }
        memset(&g_lb[s], 0, sizeof g_lb[s]);
        pthread_cond_broadcast(&g_lb_c);
    }
    pthread_mutex_unlock(&g_lb_m);
    if (s < 0) return fail(E_BADF);
    t_lberr = 0; return 0;
}
// Poll over loopback sockets (hle_net.c dm_sk_poll hands a set with any of them here); descriptors of other kinds report nothing
typedef struct { int32_t fd; int16_t events, revents; } lb_pollfd;
int dm_lb_any(const void *fds, uint64_t n) { const lb_pollfd *p = fds; for (uint64_t i = 0; i < n; i++) if (dm_lb_owns(p[i].fd)) return 1; return 0; }
static int lb_ready(lb_pollfd *p, uint64_t n) {   // with g_lb_m held
    int ready = 0;
    for (uint64_t i = 0; i < n; i++) {
        p[i].revents = 0;
        if (p[i].fd < LB_BASE || p[i].fd >= LB_BASE + LB_MAX) continue;
        int s = slot(p[i].fd);
        if (s < 0) p[i].revents = POLLNVAL_;
        else { if (g_lb[s].head) p[i].revents |= p[i].events & (POLLIN_ | POLLRDNORM_); p[i].revents |= p[i].events & POLLOUT_; }
        ready += p[i].revents != 0;
    }
    return ready;
}
int dm_lb_poll(void *fds, uint64_t n, int ms) {
    lb_pollfd *p = fds;
    int64_t limit = (ms < 0 ? 100 : (int64_t)ms) * 1000000;   // an endless wait comes back with 0 after 100 ms, as dm_sk_poll's
    uint64_t start = guest_ns();
    pthread_mutex_lock(&g_lb_m);
    int ready;
    while (!(ready = lb_ready(p, n))) {
        int64_t left = limit - (int64_t)(guest_ns() - start);
        if (left <= 0) break;
        dm_guest_cond_wait_ns(&g_lb_c, &g_lb_m, dm_detclock_enabled() && left > 1000000 ? 1000000 : left);
    }
    pthread_mutex_unlock(&g_lb_m);
    t_lberr = 0; return ready;
}
