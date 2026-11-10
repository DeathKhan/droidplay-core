// UDP between the game's own sockets (hle_loopback.c, net_loopback): what RakNet's single-player server and client need from 127.0.0.1 (Divinity: Original Sin 2), and that
// nothing else reaches past the host. Close and Poll come through hle_net.c as the game calls them
#include "../hle_loopback.c"
#include "../hle_net.c"
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void dm_guest_sleep_ns(int64_t ns) { struct timespec ts = {ns / 1000000000, ns % 1000000000}; nanosleep(&ts, 0); }
void dm_guest_cond_wait_ns(pthread_cond_t *c, pthread_mutex_t *m, int64_t ns) { struct timespec ts = {ns / 1000000000, ns % 1000000000}; pthread_cond_timedwait_relative_np(c, m, &ts); }
uint64_t guest_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec; }
int dm_detclock_enabled(void) { return 0; }

static sk_in addr(uint32_t a, uint16_t port) { sk_in s = {sizeof s, AF_INET_, __builtin_bswap16(port), __builtin_bswap32(a), {0}}; return s; }
static int g_srv; static uint16_t g_srv_port = 23253;
static void *sender(void *arg) {   // another thread's datagram ends a blocking receive, as RakNet's own wake-up packet ends its receive thread's
    (void)arg; dm_guest_sleep_ns(20000000);
    int c = dm_lb_socket(2, 2, 0); sk_in to = addr(0x7f000001, g_srv_port);
    assert(dm_lb_sendto(c, "wake", 4, 0, &to, sizeof to) == 4);
    assert(dm_lb_close(c) == 0);
    return NULL;
}
static void *closer(void *fd) { dm_guest_sleep_ns(20000000); assert(h_sk_Close((int)(intptr_t)fd) == 0); return NULL; }

int main(void) {
    setenv("DM_NET_LOOPBACK", "1", 1);
    assert(dm_lb_takes(2, 2, 0) && dm_lb_takes(2, 2, 17) && dm_lb_takes(2, 2 | 0x20000000, 0));
    assert(!dm_lb_takes(2, 1, 0) && !dm_lb_takes(28, 2, 0) && !dm_lb_takes(2, 2, 6));   // TCP, IPv6: still offline

    // The server binds its port on every address, the client takes an ephemeral one
    g_srv = dm_lb_socket(2, 2, 0); int cli = dm_lb_socket(2, 2, 0);
    assert(g_srv >= LB_BASE && cli >= LB_BASE && g_srv != cli && dm_lb_owns(g_srv) && !dm_lb_owns(5));
    sk_in any = addr(0, g_srv_port), zero = addr(0, 0), got; unsigned n = sizeof got;
    assert(dm_lb_bind(g_srv, &any, sizeof any) == 0);
    assert(dm_lb_bind(g_srv, &any, sizeof any) == -1 && dm_lb_errno() == E_INVAL);
    int other = dm_lb_socket(2, 2, 0);
    assert(dm_lb_bind(other, &any, sizeof any) == -1 && dm_lb_errno() == E_ADDRINUSE);
    sk_in lan = addr(0xc0a80002, 0);
    assert(dm_lb_bind(other, &lan, sizeof lan) == -1 && dm_lb_errno() == E_ADDRNOTAVAIL);   // no interface but loopback
    assert(dm_lb_close(other) == 0);
    assert(dm_lb_bind(cli, &zero, sizeof zero) == 0 && dm_lb_getsockname(cli, &got, &n) == 0 && n == sizeof got);
    uint16_t cport = __builtin_bswap16(got.port);
    assert(got.family == AF_INET_ && cport >= LB_EPHEMERAL);

    // A datagram to 127.0.0.1 reaches the socket bound to its port, from 127.0.0.1 and the sender's port
    sk_in to = addr(0x7f000001, g_srv_port), from; unsigned fl = sizeof from; char buf[64] = {0};
    assert(dm_lb_sendto(cli, "hello", 5, 0, &to, sizeof to) == 5);
    assert(dm_lb_recvfrom(g_srv, buf, sizeof buf, MSG_PEEK_, NULL, NULL) == 5);   // still there
    assert(dm_lb_recvfrom(g_srv, buf, sizeof buf, 0, &from, &fl) == 5 && !memcmp(buf, "hello", 5));
    assert(fl == sizeof from && from.family == AF_INET_ && from.addr == __builtin_bswap32(0x7f000001) && __builtin_bswap16(from.port) == cport);
    assert(dm_lb_sendto(g_srv, "0123456789", 10, 0, &from, sizeof from) == 10);   // the answer goes back to the client's port
    assert(dm_lb_recvfrom(cli, buf, 4, 0, NULL, NULL) == 4 && !memcmp(buf, "0123", 4));   // a short buffer takes the start, the rest is lost
    sk_in to0 = addr(0, g_srv_port);
    assert(dm_lb_sendto(cli, "x", 1, 0, &to0, sizeof to0) == 1 && dm_lb_recvfrom(g_srv, buf, sizeof buf, 0, NULL, NULL) == 1);   // 0.0.0.0 is this host too

    // Nothing leaves the host: another address or a broadcast has no route; a port nobody bound loses the datagram, and the send succeeds as UDP's do
    sk_in far = addr(0x0a000001, g_srv_port), bc = addr(0xffffffff, g_srv_port), nobody = addr(0x7f000001, 1);
    assert(dm_lb_sendto(cli, "x", 1, 0, &far, sizeof far) == -1 && dm_lb_errno() == E_NETUNREACH);
    assert(dm_lb_sendto(cli, "x", 1, 0, &bc, sizeof bc) == -1 && dm_lb_errno() == E_NETUNREACH);
    assert(dm_lb_sendto(cli, "x", 1, 0, &nobody, sizeof nobody) == 1);
    static char big[LB_DGRAM_MAX + 1];
    assert(dm_lb_sendto(cli, big, sizeof big, 0, &to, sizeof to) == -1 && dm_lb_errno() == E_MSGSIZE);

    // Non-blocking: by fcntl, or for one call
    assert(dm_lb_recvfrom(g_srv, buf, sizeof buf, MSG_DONTWAIT_, NULL, NULL) == -1 && dm_lb_errno() == E_AGAIN);
    assert(dm_lb_fcntl(g_srv, F_GETFL_, 0) == O_RDWR_);
    assert(dm_lb_fcntl(g_srv, F_SETFL_, O_RDWR_ | O_NONBLOCK_) == 0 && dm_lb_fcntl(g_srv, F_GETFL_, 0) == (O_RDWR_ | O_NONBLOCK_));
    assert(dm_lb_recvfrom(g_srv, buf, sizeof buf, 0, NULL, NULL) == -1 && dm_lb_errno() == E_AGAIN);
    assert(dm_lb_fcntl(g_srv, F_SETFL_, O_RDWR_) == 0);

    // Blocking: another thread's datagram ends the wait; SO_RCVTIMEO bounds it
    pthread_t th; pthread_create(&th, NULL, sender, NULL);
    uint64_t t0 = guest_ns();
    assert(dm_lb_recvfrom(g_srv, buf, sizeof buf, 0, NULL, NULL) == 4 && !memcmp(buf, "wake", 4) && guest_ns() - t0 < 5000000000u);
    pthread_join(th, NULL);
    int64_t tv[2] = {0, 20000};
    assert(dm_lb_setsockopt(g_srv, SOL_SOCKET_, SO_RCVTIMEO_, tv, sizeof tv) == 0);
    t0 = guest_ns();
    assert(dm_lb_recvfrom(g_srv, buf, sizeof buf, 0, NULL, NULL) == -1 && dm_lb_errno() == E_AGAIN && guest_ns() - t0 >= 15000000);
    int sz = 262144; int v = 0; unsigned vn = sizeof v;
    assert(dm_lb_setsockopt(g_srv, SOL_SOCKET_, SO_RCVBUF_, &sz, sizeof sz) == 0 && dm_lb_getsockopt(g_srv, SOL_SOCKET_, SO_RCVBUF_, &v, &vn) == 0 && v == sz && vn == 4);
    assert(dm_lb_setsockopt(g_srv, SOL_SOCKET_, SO_BROADCAST_, &sz, sizeof sz) == 0);

    // Poll through hle_net.c: readable once a datagram waits, writable always
    sk_pollfd p = {g_srv, SK_POLLIN, 0x7f};
    assert(dm_sk_poll(&p, 1, 10) == 0 && p.revents == 0);
    assert(dm_lb_sendto(cli, "p", 1, 0, &to, sizeof to) == 1);
    assert(h_sk_Poll(&p, 1, 1000) == 1 && p.revents == SK_POLLIN);
    sk_pollfd w = {cli, SK_POLLOUT, 0};
    assert(h_nsk_Poll(&w, 1, 0) == 1 && w.revents == SK_POLLOUT);

    // Close (hle_net.c's Close comes here for these) wakes a blocked receive with EBADF, and the descriptor is free again
    int64_t forever[2] = {0, 0};
    assert(dm_lb_setsockopt(cli, SOL_SOCKET_, SO_RCVTIMEO_, forever, sizeof forever) == 0);
    pthread_create(&th, NULL, closer, (void *)(intptr_t)cli);
    assert(dm_lb_recvfrom(cli, buf, sizeof buf, 0, NULL, NULL) == -1 && dm_lb_errno() == E_BADF);
    pthread_join(th, NULL);
    assert(h_sk_Close(cli) == -1);   // not a loopback socket any more: hle_net.c's offline answer
    assert(dm_lb_close(g_srv) == 0 && dm_lb_recvfrom(g_srv, buf, 1, 0, NULL, NULL) == -1 && dm_lb_errno() == E_BADF);
    int again = dm_lb_socket(2, 2, 0);
    assert(again >= LB_BASE && dm_lb_bind(again, &any, sizeof any) == 0);   // the port is free once its socket closed
    assert(dm_lb_close(again) == 0);

    // The table is bounded
    int all[LB_MAX];
    for (int i = 0; i < LB_MAX; i++) assert((all[i] = dm_lb_socket(2, 2, 0)) >= LB_BASE);
    assert(dm_lb_socket(2, 2, 0) == -1 && dm_lb_errno() == E_MFILE);
    for (int i = 0; i < LB_MAX; i++) assert(dm_lb_close(all[i]) == 0);
    dm_lb_offline_call();
    assert(dm_lb_errno() == E_NETDOWN);   // a call that failed offline says ENETDOWN again
    puts("ok t_net_loopback");
    return 0;
}
