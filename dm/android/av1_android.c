// AV1 movies on Android's MediaCodec ("video/av01"): the nn::codec video decoders of the Switch 2's SDK (hle_codec_video.c) hand over one temporal unit at a time and
// take a picture back (hle_movie_av1.h; VideoToolbox or dav1d on Apple, hle_movie_av1.c). Android picks the decoder: a hardware one where the chip has it (Snapdragon
// 8 Gen 2 on: c2.qti.av1.decoder), else its own software decoder (Android 10 on). Ocarina of Time's title runs over a 1920x1080 AV1 movie, and a new file opens with
// eight minutes of one; without this they stayed black (apple_stubs.c).
// MediaCodec decodes on its own time: a unit goes in and its picture reaches an AImageReader (YUV_420_888 with explicit strides, as media_android.c's H.264 takes them:
// the byte-buffer output of Qualcomm's decoders is a tiled vendor format) a few milliseconds later. mav1_decode waits up to AV1_WAIT_MS for the oldest picture not handed
// out yet, so a decoder that holds pictures back gives each a call or two late; at a reset the ones still inside are dropped. The colour matrix is BT.709 from 720
// lines up, else BT.601, video range (the sequence header's colour description is not read; media_android.c's rule).
// Banana Native (git.eden-emu.dev/moceure/BananaNative) found AV1 decoded on the CPU stuttered as phones warmed up and moved its movies to the hardware decoder; no code taken.
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkImageReader.h>
#include "dm.h"
#include "hle_movie_av1.h"

#define QMAX 8
#define AV1_WAIT_MS 20
typedef struct { uint8_t *buf; int w, h; } pic_t;   // NV12, stride w
struct mav1 {
    AMediaCodec *mc; AImageReader *rd; ANativeWindow *win; int red, seq; char name[64];
    pthread_mutex_t mu; pthread_cond_t cv; pic_t q[QMAX]; int nq; long sent, arrived;
    pic_t cur;   // the picture the last mav1_decode handed out, valid until the next call
};
mav1 *mav1_new(void) {
    mav1 *a = calloc(1, sizeof *a); if (!a) die("movie: AV1 decoder allocation failed");
    pthread_mutex_init(&a->mu, NULL); pthread_cond_init(&a->cv, NULL);
    return a;
}
static void drop_queue(mav1 *a) { pthread_mutex_lock(&a->mu); for (int i = 0; i < a->nq; i++) free(a->q[i].buf); a->nq = 0; pthread_mutex_unlock(&a->mu); }
static void close_codec(mav1 *a) {
    if (a->mc) { AMediaCodec_stop(a->mc); AMediaCodec_delete(a->mc); a->mc = NULL; }
    if (a->rd) { AImageReader_delete(a->rd); a->rd = NULL; a->win = NULL; }   // the window belongs to the reader
}
void mav1_reset(mav1 *a) { close_codec(a); drop_queue(a); a->seq = 0; }
void mav1_free(mav1 *a) { if (!a) return; mav1_reset(a); free(a->cur.buf); pthread_mutex_destroy(&a->mu); pthread_cond_destroy(&a->cv); free(a); }
const char *mav1_backend(const mav1 *a) { return a->name[0] ? a->name : "MediaCodec"; }
int mav1_av1c(const uint8_t *obu, size_t n, uint8_t c[4], int *w, int *h) { (void)obu; (void)n; (void)c; (void)w; (void)h; return 0; }   // hle_movie_av1.c's (its test); the decoder here reads the sequence header in-band

// whether a temporal unit (OBUs with their sizes) holds a sequence header (OBU type 1): the decoder starts there
static int has_seq_header(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n;) {
        uint8_t h = p[i++]; int type = h >> 3 & 15, ext = h >> 2 & 1, sized = h >> 1 & 1;
        if (ext) i++;
        size_t len = n - (i < n ? i : n);
        if (sized) { len = 0; for (int s = 0; s < 56 && i < n; s += 7) { uint8_t b = p[i++]; len |= (size_t)(b & 0x7f) << s; if (!(b & 0x80)) break; } }
        if (type == 1) return 1;
        if (len > n - (i < n ? i : n)) return 0;
        i += len;
    }
    return 0;
}
// a picture from the reader → the queue, as NV12 (cropped)
static void on_image(void *ctx, AImageReader *rd) {
    mav1 *a = ctx; AImage *img = NULL;
    while (AImageReader_acquireNextImage(rd, &img) == AMEDIA_OK && img) {
        int32_t w = 0, h = 0; AImageCropRect cr;
        AImage_getWidth(img, &w); AImage_getHeight(img, &h);
        if (AImage_getCropRect(img, &cr) == AMEDIA_OK && cr.right > cr.left && cr.bottom > cr.top) { w = cr.right - cr.left; h = cr.bottom - cr.top; } else cr.left = cr.top = 0;
        uint8_t *yp = NULL, *up = NULL, *vp = NULL; int yl = 0, ul = 0, vl = 0, ys = 0, us = 0, vs = 0, ups = 0, vps = 0;
        AImage_getPlaneData(img, 0, &yp, &yl); AImage_getPlaneData(img, 1, &up, &ul); AImage_getPlaneData(img, 2, &vp, &vl);
        AImage_getPlaneRowStride(img, 0, &ys); AImage_getPlaneRowStride(img, 1, &us); AImage_getPlaneRowStride(img, 2, &vs);
        AImage_getPlanePixelStride(img, 1, &ups); AImage_getPlanePixelStride(img, 2, &vps);
        if (!yp || !up || !vp || ys <= 0 || us <= 0 || vs <= 0 || ups <= 0 || vps <= 0) {   // a buffer the CPU cannot map whole (UBWC): dropped, as media_android.c does
            if (!a->red++) LOG("[RED] movie: an AV1 picture without readable planes (Y %p, U %p, V %p, strides %d %d %d): dropped", (void *)yp, (void *)up, (void *)vp, ys, us, vs);
            AImage_delete(img); img = NULL; continue;
        }
        w &= ~1; h &= ~1;
        uint8_t *buf = w > 0 && h > 0 ? malloc((size_t)w * h * 3 / 2) : NULL;
        if (buf) {
            for (int r = 0; r < h; r++) memcpy(buf + (size_t)r * w, yp + (size_t)(r + cr.top) * ys + cr.left, (size_t)w);
            uint8_t *uv = buf + (size_t)w * h;
            for (int r = 0; r < h / 2; r++) {
                const uint8_t *u = up + (size_t)(r + cr.top / 2) * us + (size_t)(cr.left / 2) * ups, *v = vp + (size_t)(r + cr.top / 2) * vs + (size_t)(cr.left / 2) * vps;
                uint8_t *o = uv + (size_t)r * w;
                if (ups == 2 && vps == 2 && v == u + 1) memcpy(o, u, (size_t)w);   // already interleaved (NV12)
                else for (int x = 0; x < w / 2; x++) { o[2 * x] = u[x * ups]; o[2 * x + 1] = v[x * vps]; }
            }
            pthread_mutex_lock(&a->mu); a->arrived++;
            if (a->nq == QMAX) { if (!(a->red++ & 63)) LOG("[RED] movie: AV1 picture queue full, dropping the oldest picture"); free(a->q[0].buf); memmove(a->q, a->q + 1, --a->nq * sizeof a->q[0]); }
            a->q[a->nq++] = (pic_t){buf, w, h};
            pthread_cond_broadcast(&a->cv); pthread_mutex_unlock(&a->mu);
        }
        AImage_delete(img); img = NULL;
    }
}
static int open_codec(mav1 *a) {
    close_codec(a);
    if (AImageReader_new(1920, 1088, AIMAGE_FORMAT_YUV_420_888, QMAX, &a->rd) != AMEDIA_OK || AImageReader_getWindow(a->rd, &a->win) != AMEDIA_OK) {
        if (!a->red++) LOG("[RED] movie: no image reader for the AV1 decoder: the movie stays black"); close_codec(a); return 0; }
    AImageReader_ImageListener l = {a, on_image}; AImageReader_setImageListener(a->rd, &l);
    a->mc = AMediaCodec_createDecoderByType("video/av01");
    if (!a->mc) { if (!a->red++) LOG("[RED] movie: no AV1 decoder on this device: the movie stays black"); close_codec(a); return 0; }
    AMediaFormat *f = AMediaFormat_new();
    AMediaFormat_setString(f, AMEDIAFORMAT_KEY_MIME, "video/av01");
    AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_WIDTH, 1920); AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_HEIGHT, 1080);   // the stream's own size comes from its sequence header
    AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7F420888);   // flexible YUV: linear NV12 the CPU can read (media_android.c)
    media_status_t st = AMediaCodec_configure(a->mc, f, a->win, NULL, 0); AMediaFormat_delete(f);
    if (st == AMEDIA_OK) st = AMediaCodec_start(a->mc);
    if (st != AMEDIA_OK) { if (!a->red++) LOG("[RED] movie: the AV1 decoder refused its configuration (%d): the movie stays black", st); close_codec(a); return 0; }
    char *nm = NULL; if (AMediaCodec_getName(a->mc, &nm) == AMEDIA_OK && nm) { snprintf(a->name, sizeof a->name, "MediaCodec %s", nm); AMediaCodec_releaseName(a->mc, nm); }
    LOG("[movie] AV1: %s", mav1_backend(a));
    return 1;
}
static void send_out(mav1 *a) {   // the decoder's finished pictures to the reader
    for (;;) {
        AMediaCodecBufferInfo bi; ssize_t i = AMediaCodec_dequeueOutputBuffer(a->mc, &bi, 0);
        if (i >= 0) { if (bi.size > 0) { pthread_mutex_lock(&a->mu); a->sent++; pthread_mutex_unlock(&a->mu); } AMediaCodec_releaseOutputBuffer(a->mc, (size_t)i, bi.size > 0); continue; }
        if (i == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED || i == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) continue;
        return;
    }
}
int mav1_decode(mav1 *a, const uint8_t *data, size_t n, mav1_picture *out) {
    if (!a->seq) { if (!has_seq_header(data, n)) return 0; a->seq = 1; }
    if (!a->mc && !open_codec(a)) return -1;
    int queued = 0;
    for (int tries = 0; tries < 200 && !queued; tries++) {   // up to 2 s for an input buffer: the decoder frees them as it finishes pictures
        ssize_t i = AMediaCodec_dequeueInputBuffer(a->mc, 10000);
        if (i < 0) { send_out(a); continue; }
        size_t cap = 0; uint8_t *b = AMediaCodec_getInputBuffer(a->mc, (size_t)i, &cap);
        if (!b || cap < n) { if (!a->red++) LOG("[RED] movie: AV1 temporal unit of %zu bytes over the decoder's %zu", n, cap); AMediaCodec_queueInputBuffer(a->mc, (size_t)i, 0, 0, 0, 0); return -1; }
        memcpy(b, data, n); AMediaCodec_queueInputBuffer(a->mc, (size_t)i, 0, n, 0, 0); queued = 1;
    }
    if (!queued) { if (!a->red++) LOG("[RED] movie: the AV1 decoder took no input for 2 s"); return -1; }
    send_out(a);
    pthread_mutex_lock(&a->mu);
    if (!a->nq) {   // the picture of this unit (or an earlier one) on its way to the reader
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_nsec += AV1_WAIT_MS * 1000000L; if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        while (!a->nq) { pthread_mutex_unlock(&a->mu); send_out(a); pthread_mutex_lock(&a->mu); if (a->nq || pthread_cond_timedwait(&a->cv, &a->mu, &ts)) break; }
    }
    if (!a->nq) { pthread_mutex_unlock(&a->mu); return 0; }
    free(a->cur.buf); a->cur = a->q[0]; memmove(a->q, a->q + 1, --a->nq * sizeof a->q[0]);
    pthread_mutex_unlock(&a->mu);
    *out = (mav1_picture){a->cur.buf, a->cur.buf + (size_t)a->cur.w * a->cur.h, (size_t)a->cur.w, (size_t)a->cur.w, a->cur.w, a->cur.h, a->cur.h >= 720 ? 2 : 1, 0};
    return 1;
}
