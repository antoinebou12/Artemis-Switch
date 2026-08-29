# Compatibility fixes for the pinned jmpangilinan/netbird-switch backend.
#
# The PoC predates current NetBird relay selection. It authenticated to a relay
# instance but discarded the instance URL, advertised the generic relay URL to
# peers, and left the Signal ConnectStream receive side as a stub. Current
# NetBird peers select a common relay through Signal OFFER/ANSWER messages, so
# that combination can leave both peers registered while WireGuard packets are
# silently sent to different relay instances.
#
# Keep the git submodule pristine and apply deterministic source rewrites only
# to the staged build tree. Every rewrite is guarded by an exact source match so
# a future submodule update fails at configure time instead of producing a
# subtly incompatible binary.

function(_artemis_nb_replace file label old_text new_text)
    file(READ "${file}" _content)
    string(FIND "${_content}" "${old_text}" _match)
    if (_match EQUAL -1)
        message(FATAL_ERROR
            "NetBird compatibility rewrite '${label}' no longer matches ${file}")
    endif ()
    string(REPLACE "${old_text}" "${new_text}" _patched "${_content}")
    if ("${_patched}" STREQUAL "${_content}")
        message(FATAL_ERROR "NetBird compatibility rewrite '${label}' made no change")
    endif ()
    file(WRITE "${file}" "${_patched}")
    message(STATUS "Applied NetBird compatibility rewrite: ${label}")
endfunction()

function(artemis_apply_netbird_current_relay_compat stage)
    # h2client: expose a receive primitive for long-lived gRPC streams.
    set(_old [==[
uint32_t h2_stream_open(H2Conn *conn,
                       const char *service_method,
                       const char *metadata_key, const char *metadata_val,
                       char *error, size_t error_size);
]==])
    set(_new [==[
uint32_t h2_stream_open(H2Conn *conn,
                       const char *service_method,
                       const char *metadata_key, const char *metadata_val,
                       char *error, size_t error_size);

// Receive one protobuf message from an already-open bidirectional gRPC stream.
// Returns a malloc'd protobuf body. An idle stream reports "timeout" so callers
// can re-check their shutdown flag without treating silence as a disconnect.
uint8_t* h2_stream_recv(H2Conn *conn, uint32_t stream_id,
                        size_t *out_len, char *error, size_t error_size);
]==])
    _artemis_nb_replace("${stage}/include/h2client.h" "declare stream receive" "${_old}" "${_new}")

    set(_old [==[
    uint32_t next_stream_id;
    bool preface_done;
    char sni[256];
} Conn;
]==])
    set(_new [==[
    uint32_t next_stream_id;
    bool preface_done;
    char sni[256];
    uint8_t *stream_buf;       // buffered gRPC DATA from bidirectional stream
    size_t stream_buf_len;
    uint8_t *h2_frame_buf;     // incomplete HTTP/2 frames across WS messages
    size_t h2_frame_buf_len;
} Conn;
]==])
    _artemis_nb_replace("${stage}/source/h2client.c" "buffer signal stream data" "${_old}" "${_new}")

    set(_old [==[
    snprintf(error,error_size,"no response headers");
    return 0;
}

// ─── POLL (non-blocking frame read, for keep-alive) ────────────
]==])
    set(_new [==[
    snprintf(error,error_size,"no response headers");
    return 0;
}

static uint8_t* h2_stream_take_message(Conn *c, size_t *out_len,
                                       char *error, size_t error_size) {
    if (c->stream_buf_len < 5) return NULL;
    if (c->stream_buf[0] != 0) {
        snprintf(error, error_size, "compressed gRPC stream frame");
        return NULL;
    }

    size_t msg_len = ((uint32_t)c->stream_buf[1] << 24) |
                     ((uint32_t)c->stream_buf[2] << 16) |
                     ((uint32_t)c->stream_buf[3] << 8) |
                     (uint32_t)c->stream_buf[4];
    if (msg_len > (1024 * 1024)) {
        snprintf(error, error_size, "gRPC stream message too large: %zu", msg_len);
        return NULL;
    }
    if (c->stream_buf_len < 5 + msg_len) return NULL;

    uint8_t *out = malloc(msg_len ? msg_len : 1);
    if (!out) {
        snprintf(error, error_size, "malloc");
        return NULL;
    }
    if (msg_len) memcpy(out, c->stream_buf + 5, msg_len);

    size_t remain = c->stream_buf_len - 5 - msg_len;
    if (remain) memmove(c->stream_buf, c->stream_buf + 5 + msg_len, remain);
    c->stream_buf_len = remain;
    if (remain == 0) {
        free(c->stream_buf);
        c->stream_buf = NULL;
    } else {
        uint8_t *shrunk = realloc(c->stream_buf, remain);
        if (shrunk) c->stream_buf = shrunk;
    }

    *out_len = msg_len;
    return out;
}

uint8_t* h2_stream_recv(H2Conn *conn, uint32_t stream_id,
                        size_t *out_len, char *error, size_t error_size) {
    *out_len = 0;
    *error = 0;
    Conn *c = (Conn*)conn;
    if (!c || !c->preface_done || stream_id == 0) {
        snprintf(error, error_size, "not connected");
        return NULL;
    }

    uint8_t *ready = h2_stream_take_message(c, out_len, error, error_size);
    if (ready || error[0]) return ready;

    size_t wl = 0;
    uint8_t *f = ws_recv(c, &wl);
    if (!f) {
        snprintf(error, error_size, "timeout");
        return NULL;
    }

    // A WebSocket message is only a transport chunk. HTTP/2 frame boundaries
    // may cross it, so retain incomplete bytes for the next receive call.
    const size_t h2_limit = 2 * 1024 * 1024;
    if (wl > h2_limit || c->h2_frame_buf_len > h2_limit - wl) {
        free(f);
        snprintf(error, error_size, "HTTP/2 receive buffer limit exceeded");
        return NULL;
    }
    uint8_t *h2 = realloc(c->h2_frame_buf, c->h2_frame_buf_len + wl);
    if (!h2 && wl) {
        free(f);
        snprintf(error, error_size, "malloc");
        return NULL;
    }
    c->h2_frame_buf = h2;
    if (wl) memcpy(c->h2_frame_buf + c->h2_frame_buf_len, f, wl);
    c->h2_frame_buf_len += wl;
    free(f);

    size_t pos = 0;
    while (pos + 9 <= c->h2_frame_buf_len) {
        const uint8_t *frame = c->h2_frame_buf + pos;
        uint32_t fl = ((uint32_t)frame[0] << 16) |
                      ((uint32_t)frame[1] << 8) | frame[2];
        uint8_t ft = frame[3], ff = frame[4];
        uint32_t fsid = ((uint32_t)(frame[5] & 0x7F) << 24) |
                        ((uint32_t)frame[6] << 16) |
                        ((uint32_t)frame[7] << 8) | frame[8];
        if (fl > 1024 * 1024) {
            snprintf(error, error_size, "HTTP/2 frame too large: %u", fl);
            c->h2_frame_buf_len = 0;
            free(c->h2_frame_buf);
            c->h2_frame_buf = NULL;
            return NULL;
        }
        if (pos + 9 + fl > c->h2_frame_buf_len) break;

        if (ft == 6 && fsid == 0 && fl == 8 && !(ff & 1)) {
            h2_send_frame(c, 8, 6, 1, 0, frame + 9);
        } else if (fsid == stream_id && ft == 0) {
            size_t data_off = pos + 9;
            size_t data_len = fl;
            if ((ff & 8) && data_len > 0) {
                uint8_t pad = c->h2_frame_buf[data_off];
                if ((size_t)pad + 1 <= data_len) {
                    data_off++;
                    data_len -= (size_t)pad + 1;
                } else {
                    snprintf(error, error_size, "invalid HTTP/2 DATA padding");
                    return NULL;
                }
            }
            if (data_len) {
                const size_t grpc_limit = 1024 * 1024 + 5;
                if (data_len > grpc_limit ||
                    c->stream_buf_len > grpc_limit - data_len) {
                    snprintf(error, error_size, "gRPC receive buffer limit exceeded");
                    return NULL;
                }
                uint8_t *nb = realloc(c->stream_buf, c->stream_buf_len + data_len);
                if (!nb) {
                    snprintf(error, error_size, "malloc");
                    return NULL;
                }
                c->stream_buf = nb;
                memcpy(c->stream_buf + c->stream_buf_len,
                       c->h2_frame_buf + data_off, data_len);
                c->stream_buf_len += data_len;
            }
        } else if (fsid == stream_id && ft == 3) {
            snprintf(error, error_size, "signal stream reset");
            return NULL;
        } else if (fsid == 0 && ft == 7) {
            snprintf(error, error_size, "signal connection goaway");
            return NULL;
        }
        pos += 9 + fl;
    }
    if (pos) {
        const size_t remain = c->h2_frame_buf_len - pos;
        if (remain)
            memmove(c->h2_frame_buf, c->h2_frame_buf + pos, remain);
        c->h2_frame_buf_len = remain;
        if (!remain) {
            free(c->h2_frame_buf);
            c->h2_frame_buf = NULL;
        }
    }

    ready = h2_stream_take_message(c, out_len, error, error_size);
    if (ready || error[0]) return ready;
    snprintf(error, error_size, "timeout");
    return NULL;
}

// ─── POLL (non-blocking frame read, for keep-alive) ────────────
]==])
    _artemis_nb_replace("${stage}/source/h2client.c" "receive Signal gRPC stream" "${_old}" "${_new}")

    set(_old [==[
    mbedtls_ssl_config_free(&c->cf);
    mbedtls_ctr_drbg_free(&c->d);
    free(c);
]==])
    set(_new [==[
    mbedtls_ssl_config_free(&c->cf);
    mbedtls_ctr_drbg_free(&c->d);
    free(c->stream_buf);
    c->stream_buf = NULL;
    c->stream_buf_len = 0;
    free(c->h2_frame_buf);
    c->h2_frame_buf = NULL;
    c->h2_frame_buf_len = 0;
    free(c);
]==])
    _artemis_nb_replace("${stage}/source/h2client.c" "free Signal stream buffer" "${_old}" "${_new}")

    # Signal: use separate receive/send connections and consume OFFER/ANSWER.
    set(_old [==[
typedef struct {
    H2Conn *h2;
    uint32_t stream_id;
    char wg_pubkey_b64[64];   // our public key (base64)
    uint8_t wg_priv[32];       // our private key (raw)
    volatile bool running;
    Thread recv_thread;        // background receive thread
} SignalClient;
]==])
    set(_new [==[
typedef void (*SignalMessageCallback)(const char *remote_pubkey_b64,
                                      int message_type,
                                      const char *relay_url,
                                      void *user);

typedef struct {
    H2Conn *h2;                // dedicated ConnectStream receive connection
    H2Conn *send_h2;           // dedicated unary Send connection
    uint32_t stream_id;
    char wg_pubkey_b64[64];   // our public key (base64)
    uint8_t wg_priv[32];       // our private key (raw)
    SignalMessageCallback message_cb;
    void *message_cb_user;
    volatile bool running;
    Thread recv_thread;        // background receive thread
} SignalClient;
]==])
    _artemis_nb_replace("${stage}/include/signal_client.h" "extend Signal client state" "${_old}" "${_new}")

    set(_old [==[
// Send an OFFER to a remote peer with our relay address
bool signal_send_offer(SignalClient *sc, const char *remote_pubkey_b64,
                       const char *relay_addr, uint16_t wg_port,
                       char *error, size_t error_size);

// Stop and close
]==])
    set(_new [==[
// Send an OFFER/ANSWER to a remote peer with our selected relay address.
bool signal_send_offer(SignalClient *sc, const char *remote_pubkey_b64,
                       const char *relay_addr, uint16_t wg_port,
                       char *error, size_t error_size);
bool signal_send_answer(SignalClient *sc, const char *remote_pubkey_b64,
                        const char *relay_addr, uint16_t wg_port,
                        char *error, size_t error_size);

void signal_set_message_callback(SignalClient *sc, SignalMessageCallback cb,
                                 void *user);

// Stop and close
]==])
    _artemis_nb_replace("${stage}/include/signal_client.h" "declare Signal negotiation API" "${_old}" "${_new}")

    set(_old [==[
    sc->stream_id = sid;
    fprintf(stderr, "[SIGNAL] ConnectStream registered on stream %u\n", sid);
    return true;
]==])
    set(_new [==[
    sc->stream_id = sid;
    fprintf(stderr, "[SIGNAL] ConnectStream registered on stream %u\n", sid);

    sc->send_h2 = h2_connect_path(host, port, ip, "/ws-proxy/signal", error, error_size);
    if (!sc->send_h2) {
        fprintf(stderr, "[SIGNAL] send channel failed: %s\n", error);
        h2_shutdown(sc->h2);
        h2_close(sc->h2);
        sc->h2 = NULL;
        sc->stream_id = 0;
        return false;
    }
    fprintf(stderr, "[SIGNAL] dedicated send channel connected\n");
    return true;
]==])
    _artemis_nb_replace("${stage}/source/signal_client.c" "separate Signal send channel" "${_old}" "${_new}")

    set(_old [==[
bool signal_send_offer(SignalClient *sc, const char *remote_pubkey_b64,
                       const char *relay_addr, uint16_t wg_port,
                       char *error, size_t error_size) {
    *error = 0;
    if (!sc || !sc->h2) { snprintf(error, error_size, "not connected"); return false; }
    
    // Build Body protobuf:
    // type=0(OFFER, field 1 varint), payload="a:b" (field 2, required)
    // wgListenPort=0 (field 3 varint)
    // relayServerAddress (field 8, LENDELIM)
    uint8_t body_buf[512], *bp = body_buf;
    bp = pb_varint(pb_tag(bp, 1, PB_VARINT), 0);  // type=OFFER
    bp = pb_str(bp, 2, "a:b", 3);  // dummy ICE creds (required by UnMarshalCredential)
    bp = pb_varint(pb_tag(bp, 3, PB_VARINT), wg_port);  // wgListenPort
    bp = pb_str(bp, 8, relay_addr, strlen(relay_addr));  // relayServerAddress
    size_t body_len = (size_t)(bp - body_buf);
    
    // Build Message protobuf:
    // key=2 (our pubkey), remoteKey=3 (peer pubkey), body=4 (Body)
    uint8_t msg_buf[1024], *mp = msg_buf;
    mp = pb_str(mp, 2, sc->wg_pubkey_b64, strlen(sc->wg_pubkey_b64));
    mp = pb_str(mp, 3, remote_pubkey_b64, strlen(remote_pubkey_b64));
    mp = pb_bytes(mp, 4, body_buf, body_len);
    size_t msg_len = (size_t)(mp - msg_buf);
    
    // NaCl encrypt Body with peer's public key
    uint8_t peer_pk[32];
    if (b64_decode(remote_pubkey_b64, strlen(remote_pubkey_b64), peer_pk, 32) != 32) {
        snprintf(error, error_size, "bad peer key"); return false;
    }
    uint8_t nonce[24]; nacl_randombytes(nonce, 24);
    uint8_t enc_body[1024];
    memcpy(enc_body, nonce, 24);
    if (nacl_box(enc_body + 24, body_buf, body_len, nonce, peer_pk, sc->wg_priv) != 0) {
        snprintf(error, error_size, "encrypt fail"); return false;
    }
    size_t enc_len = 24 + body_len + 16;
    
    // Build EncryptedMessage:
    // key=2, remoteKey=3, body=4 (encrypted)
    uint8_t em_buf[2048], *ep = em_buf;
    ep = pb_str(ep, 2, sc->wg_pubkey_b64, strlen(sc->wg_pubkey_b64));
    ep = pb_str(ep, 3, remote_pubkey_b64, strlen(remote_pubkey_b64));
    ep = pb_bytes(ep, 4, enc_body, enc_len);
    size_t em_len = (size_t)(ep - em_buf);
    
    // Send via unary RPC: signalexchange.SignalExchange/Send
    size_t rl;
    uint8_t *resp = h2_call(sc->h2, "signalexchange.SignalExchange/Send",
                             em_buf, em_len, &rl, error, error_size);
    if (resp) {
        fprintf(stderr, "[SIGNAL] OFFER sent to %s, resp=%zu bytes\n", remote_pubkey_b64, rl);
        free(resp);
        return true;
    }
    fprintf(stderr, "[SIGNAL] OFFER send failed: %s\n", error);
    return false;
}
]==])
    set(_new [==[
static bool signal_send_message(SignalClient *sc, const char *remote_pubkey_b64,
                                const char *relay_addr, uint16_t wg_port,
                                uint64_t message_type, const char *label,
                                char *error, size_t error_size) {
    *error = 0;
    if (!sc || !sc->send_h2) { snprintf(error, error_size, "not connected"); return false; }

    uint8_t body_buf[512], *bp = body_buf;
    bp = pb_varint(pb_tag(bp, 1, PB_VARINT), message_type);
    bp = pb_str(bp, 2, "a:b", 3);
    bp = pb_varint(pb_tag(bp, 3, PB_VARINT), wg_port);
    if (relay_addr && relay_addr[0])
        bp = pb_str(bp, 8, relay_addr, strlen(relay_addr));
    size_t body_len = (size_t)(bp - body_buf);

    uint8_t peer_pk[32];
    if (b64_decode(remote_pubkey_b64, strlen(remote_pubkey_b64), peer_pk, 32) != 32) {
        snprintf(error, error_size, "bad peer key"); return false;
    }
    uint8_t nonce[24]; nacl_randombytes(nonce, 24);
    uint8_t enc_body[1024];
    memcpy(enc_body, nonce, 24);
    if (nacl_box(enc_body + 24, body_buf, body_len, nonce, peer_pk, sc->wg_priv) != 0) {
        snprintf(error, error_size, "encrypt fail"); return false;
    }
    size_t enc_len = 24 + body_len + 16;

    uint8_t em_buf[2048], *ep = em_buf;
    ep = pb_str(ep, 2, sc->wg_pubkey_b64, strlen(sc->wg_pubkey_b64));
    ep = pb_str(ep, 3, remote_pubkey_b64, strlen(remote_pubkey_b64));
    ep = pb_bytes(ep, 4, enc_body, enc_len);
    size_t em_len = (size_t)(ep - em_buf);

    size_t rl = 0;
    uint8_t *resp = h2_call(sc->send_h2, "signalexchange.SignalExchange/Send",
                            em_buf, em_len, &rl, error, error_size);
    if (resp) {
        fprintf(stderr, "[SIGNAL] %s sent to %s relay=%s resp=%zu bytes\n",
                label, remote_pubkey_b64,
                (relay_addr && relay_addr[0]) ? relay_addr : "<none>", rl);
        free(resp);
        return true;
    }
    fprintf(stderr, "[SIGNAL] %s send failed: %s\n", label, error);
    return false;
}

bool signal_send_offer(SignalClient *sc, const char *remote_pubkey_b64,
                       const char *relay_addr, uint16_t wg_port,
                       char *error, size_t error_size) {
    return signal_send_message(sc, remote_pubkey_b64, relay_addr, wg_port,
                               0, "OFFER", error, error_size);
}

bool signal_send_answer(SignalClient *sc, const char *remote_pubkey_b64,
                        const char *relay_addr, uint16_t wg_port,
                        char *error, size_t error_size) {
    return signal_send_message(sc, remote_pubkey_b64, relay_addr, wg_port,
                               1, "ANSWER", error, error_size);
}
]==])
    _artemis_nb_replace("${stage}/source/signal_client.c" "send Signal OFFER and ANSWER" "${_old}" "${_new}")

    set(_old [==[
static void signal_recv_thread(void *arg) {
    SignalClient *sc = (SignalClient*)arg;
    fprintf(stderr, "[SIGNAL] recv thread started\n");
    
    while (sc->running) {
        // Read a frame from the stream
        // The stream is bidirectional — incoming messages are DATA frames
        // For now, just poll for WebSocket frames (simplified)
        size_t wl = 0;
        uint8_t *f = NULL;
        
        // Try to read from the WebSocket (would need ws_recv exposed from h2client)
        // For now, just log that we're running
        svcSleepThread(5e9);  // 5 second poll
        fprintf(stderr, "[SIGNAL] recv loop alive\n");
    }
    fprintf(stderr, "[SIGNAL] recv thread stopped\n");
}
]==])
    set(_new [==[
static bool signal_process_message(SignalClient *sc, const uint8_t *em, size_t em_len) {
    size_t key_len = 0, enc_len = 0, type_value = 0, relay_len = 0;
    const uint8_t *key = pb_find(em, em_len, 2, PB_LENDELIM, &key_len);
    const uint8_t *enc = pb_find(em, em_len, 4, PB_LENDELIM, &enc_len);
    if (!key || !enc || key_len == 0 || key_len >= 64 || enc_len < 40) {
        fprintf(stderr, "[SIGNAL] malformed encrypted message len=%zu\n", em_len);
        return false;
    }

    char remote_key[64] = {0};
    memcpy(remote_key, key, key_len);
    uint8_t peer_pk[32];
    if (b64_decode(remote_key, key_len, peer_pk, sizeof(peer_pk)) != 32) {
        fprintf(stderr, "[SIGNAL] invalid sender key\n");
        return false;
    }

    size_t plain_len = enc_len - 24 - 16;
    uint8_t *plain = malloc(plain_len ? plain_len : 1);
    if (!plain) return false;
    if (nacl_box_open(plain, enc + 24, enc_len - 24, enc,
                      peer_pk, sc->wg_priv) != 0) {
        fprintf(stderr, "[SIGNAL] decrypt failed from %.12s...\n", remote_key);
        free(plain);
        return false;
    }

    pb_find(plain, plain_len, 1, PB_VARINT, &type_value);
    const uint8_t *relay = pb_find(plain, plain_len, 8, PB_LENDELIM, &relay_len);
    char relay_url[512] = {0};
    if (relay && relay_len) {
        if (relay_len >= sizeof(relay_url)) relay_len = sizeof(relay_url) - 1;
        memcpy(relay_url, relay, relay_len);
    }

    fprintf(stderr, "[SIGNAL] RX type=%zu peer=%.12s... relay=%s\n",
            type_value, remote_key, relay_url[0] ? relay_url : "<none>");
    if (sc->message_cb)
        sc->message_cb(remote_key, (int)type_value, relay_url, sc->message_cb_user);

    free(plain);
    return true;
}

void signal_set_message_callback(SignalClient *sc, SignalMessageCallback cb,
                                 void *user) {
    if (!sc) return;
    sc->message_cb = cb;
    sc->message_cb_user = user;
}

static void signal_recv_thread(void *arg) {
    SignalClient *sc = (SignalClient*)arg;
    fprintf(stderr, "[SIGNAL] recv thread started\n");

    while (sc->running) {
        char err[128] = {0};
        size_t msg_len = 0;
        uint8_t *msg = h2_stream_recv(sc->h2, sc->stream_id,
                                      &msg_len, err, sizeof(err));
        if (!msg) {
            if (!sc->running) break;
            if (err[0] && strcmp(err, "timeout") != 0) {
                fprintf(stderr, "[SIGNAL] receive error: %s\n", err);
                // Protocol/frame errors are fatal for this HTTP/2 connection.
                // Do not spin forever on the same retained bytes; ownership of
                // reconnect remains with the NetBird lifecycle thread.
                sc->running = false;
                break;
            }
            svcSleepThread(25e6);
            continue;
        }
        signal_process_message(sc, msg, msg_len);
        free(msg);
    }
    fprintf(stderr, "[SIGNAL] recv thread stopped\n");
}
]==])
    _artemis_nb_replace("${stage}/source/signal_client.c" "receive and decrypt Signal negotiation" "${_old}" "${_new}")

    set(_old [==[
void signal_stop(SignalClient *sc) {
    if (!sc || !sc->running) return;
    sc->running = false;
    threadWaitForExit(&sc->recv_thread);
    threadClose(&sc->recv_thread);
    signal_close(sc);
}

void signal_close(SignalClient *sc) {
    if (sc->h2) { h2_close(sc->h2); sc->h2 = NULL; }
    sc->stream_id = 0;
}
]==])
    set(_new [==[
void signal_stop(SignalClient *sc) {
    if (!sc) return;
    if (sc->running) {
        sc->running = false;
        if (sc->h2) h2_shutdown(sc->h2);
        threadWaitForExit(&sc->recv_thread);
        threadClose(&sc->recv_thread);
    }
    signal_close(sc);
}

void signal_close(SignalClient *sc) {
    if (!sc) return;
    if (sc->send_h2) { h2_close(sc->send_h2); sc->send_h2 = NULL; }
    if (sc->h2) { h2_close(sc->h2); sc->h2 = NULL; }
    sc->stream_id = 0;
}
]==])
    _artemis_nb_replace("${stage}/source/signal_client.c" "shutdown Signal receiver safely" "${_old}" "${_new}")

    # Relay: retain AUTH_RESPONSE instance URL and parse rels:// URLs internally.
    set(_old [==[
    volatile bool needs_reconnect;  // set when read loop exits; cleared after reconnect
    WgMutex read_mu;   // protects in_* + transform_in (Go: in.Mutex)
    WgMutex write_mu;  // protects out_* + transform_out (Go: out.Mutex)
    uint32_t canary_end;    // 0xDEADC0DE
]==])
    set(_new [==[
    volatile bool needs_reconnect;  // set when read loop exits; cleared after reconnect
    WgMutex read_mu;   // protects in_* + transform_in (Go: in.Mutex)
    WgMutex write_mu;  // protects out_* + transform_out (Go: out.Mutex)
    char requested_url[512];
    char instance_url[512];
    uint32_t canary_end;    // 0xDEADC0DE
]==])
    _artemis_nb_replace("${stage}/source/relay_client.c" "store relay instance URL" "${_old}" "${_new}")

    set(_old [==[
    *error = 0;
    RelayClient *rc = calloc(1, sizeof(RelayClient));
    if (!rc) { snprintf(error, error_size, "malloc"); return NULL; }
    rc->canary_begin = 0xDEADC0DE;
    rc->canary_end   = 0xDEADC0DE;
    wg_mutex_init(&rc->read_mu, false);
    wg_mutex_init(&rc->write_mu, false);
]==])
    set(_new [==[
    *error = 0;

    char parsed_host[256] = {0};
    if (!host || !host[0] || port == 0) {
        const char *authority = url ? url : "";
        if (strncmp(authority, "rels://", 7) == 0) authority += 7;
        else if (strncmp(authority, "rel://", 6) == 0) authority += 6;
        const char *slash = strchr(authority, '/');
        size_t authority_len = slash ? (size_t)(slash - authority) : strlen(authority);
        const char *colon = NULL;
        for (size_t i = 0; i < authority_len; i++)
            if (authority[i] == ':') colon = authority + i;
        size_t host_len = colon ? (size_t)(colon - authority) : authority_len;
        if (host_len == 0 || host_len >= sizeof(parsed_host)) {
            snprintf(error, error_size, "bad relay URL: %s", url ? url : "<null>");
            return NULL;
        }
        memcpy(parsed_host, authority, host_len);
        parsed_host[host_len] = '\0';
        if (port == 0)
            port = colon ? (uint16_t)strtoul(colon + 1, NULL, 10) : 443;
        host = parsed_host;
    }
    if (!host || !host[0]) {
        snprintf(error, error_size, "missing relay host");
        return NULL;
    }
    if (port == 0) port = 443;

    RelayClient *rc = calloc(1, sizeof(RelayClient));
    if (!rc) { snprintf(error, error_size, "malloc"); return NULL; }
    rc->canary_begin = 0xDEADC0DE;
    rc->canary_end   = 0xDEADC0DE;
    snprintf(rc->requested_url, sizeof(rc->requested_url), "%s", url ? url : "");
    wg_mutex_init(&rc->read_mu, false);
    wg_mutex_init(&rc->write_mu, false);
    NB_INFO("[RELAY] connecting requested=%s host=%s port=%u\n",
            rc->requested_url, host, port);
]==])
    _artemis_nb_replace("${stage}/source/relay_client.c" "parse relay URL on reconnect" "${_old}" "${_new}")

    set(_old [==[
    NB_INFO("[RELAY] connected, instance=%.*s\n", (int)(rlen-2), resp+2);
    free(resp);
    
    rc->connected = true;
]==])
    set(_new [==[
    size_t instance_len = rlen > 2 ? rlen - 2 : 0;
    if (instance_len >= sizeof(rc->instance_url)) instance_len = sizeof(rc->instance_url) - 1;
    if (instance_len) memcpy(rc->instance_url, resp + 2, instance_len);
    rc->instance_url[instance_len] = '\0';
    NB_INFO("[RELAY] connected requested=%s instance=%s\n",
            rc->requested_url,
            rc->instance_url[0] ? rc->instance_url : "<none>");
    free(resp);
    
    rc->connected = true;
]==])
    _artemis_nb_replace("${stage}/source/relay_client.c" "retain relay AUTH instance" "${_old}" "${_new}")

    set(_old [==[
bool relay_needs_reconnect(RelayClient *rc) {
    return rc && rc->needs_reconnect;
}
]==])
    set(_new [==[
bool relay_needs_reconnect(RelayClient *rc) {
    return rc && rc->needs_reconnect;
}

const char* relay_instance_url(RelayClient *rc) {
    if (!rc) return NULL;
    if (rc->instance_url[0]) return rc->instance_url;
    return rc->requested_url[0] ? rc->requested_url : NULL;
}
]==])
    _artemis_nb_replace("${stage}/source/relay_client.c" "expose relay instance URL" "${_old}" "${_new}")

    set(_old [==[
// Check if relay needs reconnect (read loop exited). Returns true if reconnect needed.
bool relay_needs_reconnect(RelayClient *rc);

#endif
]==])
    set(_new [==[
// Check if relay needs reconnect (read loop exited). Returns true if reconnect needed.
bool relay_needs_reconnect(RelayClient *rc);

// Exact relay instance returned by AUTH_RESPONSE. Falls back to requested URL.
const char* relay_instance_url(RelayClient *rc);

#endif
]==])
    _artemis_nb_replace("${stage}/include/relay_client.h" "declare relay instance getter" "${_old}" "${_new}")

    # Core: current NetBird common-relay selection and reconnect behavior.
    set(_old [==[
static char           g_our_ip[48] = {0};
static Mutex          g_lwip_mutex;  // protects lwIP (NO_SYS=1 = not threadsafe)
]==])
    set(_new [==[
static char           g_our_ip[48] = {0};
static Mutex          g_lwip_mutex;  // protects lwIP (NO_SYS=1 = not threadsafe)
static Mutex          g_relay_switch_mutex;
static char           g_selected_relay_url[512] = {0};
static char           g_pending_relay_url[512] = {0};
static bool           g_relay_switch_pending = false;
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "track common relay selection" "${_old}" "${_new}")

    set(_old [==[
static void nb_socks_reset(void);  // defined after g_socks below

// ─── Relay send queue ─────────────────────────────────────────
]==])
    set(_new [==[
static void nb_socks_reset(void);  // defined after g_socks below

static RelayClient* connect_relay_url(const char *url, char *error, size_t error_size) {
    if (!g_login || !url || !url[0]) {
        snprintf(error, error_size, "missing relay state");
        return NULL;
    }
    return relay_connect(url, NULL, 0,
                         (const char*)g_login->wg_pub_b64,
                         g_login->relay_token_payload, g_login->relay_token_payload_len,
                         g_login->relay_token_sig, g_login->relay_token_sig_len,
                         error, error_size);
}

static const char* selected_relay_url(void) {
    if (g_selected_relay_url[0]) return g_selected_relay_url;
    if (g_relay) {
        const char *instance = relay_instance_url(g_relay);
        if (instance && instance[0]) return instance;
    }
    return (g_login && g_login->relay_url[0]) ? g_login->relay_url : "";
}

static void queue_relay_switch(const char *url) {
    if (!url || !url[0]) return;
    mutexLock(&g_relay_switch_mutex);
    snprintf(g_pending_relay_url, sizeof(g_pending_relay_url), "%s", url);
    g_relay_switch_pending = true;
    mutexUnlock(&g_relay_switch_mutex);
    NB_INFO("[RELAY] queued common-relay switch to %s\n", url);
}

static void signal_message_update(const char *remote_pubkey, int message_type,
                                  const char *remote_relay, void *user) {
    (void)user;
    if (!g_login || !remote_pubkey || !remote_pubkey[0]) return;

    bool local_is_controller = strcmp((const char*)g_login->wg_pub_b64,
                                      remote_pubkey) > 0;
    NB_INFO("[SIGNAL] peer=%.12s... type=%d role=%s remote_relay=%s\n",
            remote_pubkey, message_type,
            local_is_controller ? "controller" : "follower",
            (remote_relay && remote_relay[0]) ? remote_relay : "<none>");

    if (!local_is_controller && remote_relay && remote_relay[0]) {
        const char *current = selected_relay_url();
        if (!current[0] || strcmp(current, remote_relay) != 0)
            queue_relay_switch(remote_relay);
    }

    if (message_type == 0 && g_signal) {
        const char *answer_relay = local_is_controller
            ? selected_relay_url()
            : ((remote_relay && remote_relay[0]) ? remote_relay : selected_relay_url());
        char err[256] = {0};
        if (!signal_send_answer(g_signal, remote_pubkey, answer_relay, 0,
                                err, sizeof(err)))
            NB_INFO("[SIGNAL] ANSWER failed: %s\n", err);
    }
}

// ─── Relay send queue ─────────────────────────────────────────
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "add common-relay negotiation helpers" "${_old}" "${_new}")

    set(_old [==[
        if (!have && (++idle_ticks % 50 == 0))
            NB_INFO("[NB-SENDQ] idle tick %u q=%u\n", idle_ticks,
                    (unsigned)(g_sendq_head - g_sendq_tail));

        if (have && g_relay) {
]==])
    set(_new [==[
        if (!have && (++idle_ticks % 50 == 0))
            NB_INFO("[NB-SENDQ] idle tick %u q=%u\n", idle_ticks,
                    (unsigned)(g_sendq_head - g_sendq_tail));

        char switch_url[512] = {0};
        mutexLock(&g_relay_switch_mutex);
        if (g_relay_switch_pending) {
            snprintf(switch_url, sizeof(switch_url), "%s", g_pending_relay_url);
            g_relay_switch_pending = false;
            g_pending_relay_url[0] = '\0';
        }
        mutexUnlock(&g_relay_switch_mutex);
        if (switch_url[0] && g_login) {
            const char *current = relay_instance_url(g_relay);
            if (!current || strcmp(current, switch_url) != 0) {
                char rerr[256] = {0};
                NB_INFO("[RELAY] switching common relay from %s to %s\n",
                        current ? current : "<none>", switch_url);
                RelayClient *replacement = connect_relay_url(switch_url, rerr, sizeof(rerr));
                if (replacement) {
                    relay_start_read_loop(replacement);
                    RelayClient *old = g_relay;
                    g_relay = replacement;
                    const char *instance = relay_instance_url(replacement);
                    snprintf(g_selected_relay_url, sizeof(g_selected_relay_url), "%s",
                             (instance && instance[0]) ? instance : switch_url);
                    if (old) relay_stop(old);
                    NB_INFO("[RELAY] common relay active: %s\n", g_selected_relay_url);
                } else {
                    NB_INFO("[RELAY] common relay switch failed: %s\n", rerr);
                    queue_relay_switch(switch_url);
                }
            }
        }

        if (have && g_relay) {
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "switch to negotiated relay on writer thread" "${_old}" "${_new}")

    set(_old [==[
        if (relay_needs_reconnect(g_relay) && g_login && g_login->relay_url[0]) {
            u64 now = armGetSystemTick();
            if (now >= next_reconnect_tick) {
                next_reconnect_tick = now + 5ULL * 19200000ULL;  // 5s between attempts
                NB_INFO("[RELAY] reconnecting to %s...\n", g_login->relay_url);
                relay_stop(g_relay);
                char rerr[256] = {0};
                g_relay = relay_connect(g_login->relay_url, NULL, 0,
                                        (const char*)g_login->wg_pub_b64,
                                        g_login->relay_token_payload, g_login->relay_token_payload_len,
                                        g_login->relay_token_sig, g_login->relay_token_sig_len,
                                        rerr, sizeof(rerr));
                if (g_relay) {
                    relay_start_read_loop(g_relay);
                    NB_INFO("[RELAY] reconnected\n");
                } else {
                    NB_INFO("[RELAY] reconnect failed: %s\n", rerr);
                }
            }
        }
]==])
    set(_new [==[
        if (relay_needs_reconnect(g_relay) && g_login && selected_relay_url()[0]) {
            u64 now = armGetSystemTick();
            if (now >= next_reconnect_tick) {
                next_reconnect_tick = now + 5ULL * 19200000ULL;
                char reconnect_url[512] = {0};
                snprintf(reconnect_url, sizeof(reconnect_url), "%s", selected_relay_url());
                NB_INFO("[RELAY] reconnecting to selected relay %s...\n", reconnect_url);
                relay_stop(g_relay);
                g_relay = NULL;
                char rerr[256] = {0};
                g_relay = connect_relay_url(reconnect_url, rerr, sizeof(rerr));
                if (g_relay) {
                    relay_start_read_loop(g_relay);
                    const char *instance = relay_instance_url(g_relay);
                    if (instance && instance[0])
                        snprintf(g_selected_relay_url, sizeof(g_selected_relay_url), "%s", instance);
                    NB_INFO("[RELAY] reconnected to %s\n", selected_relay_url());
                } else {
                    NB_INFO("[RELAY] reconnect failed: %s\n", rerr);
                }
            }
        }
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "reconnect to selected relay" "${_old}" "${_new}")

    set(_old [==[
    mutexInit(&g_lwip_mutex);
    fprintf(stderr, "[NETBIRD] init: url=%s\n", server_url);
]==])
    set(_new [==[
    mutexInit(&g_lwip_mutex);
    mutexInit(&g_relay_switch_mutex);
    g_selected_relay_url[0] = '\0';
    g_pending_relay_url[0] = '\0';
    g_relay_switch_pending = false;
    fprintf(stderr, "[NETBIRD] init: url=%s\n", server_url);
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "initialize relay negotiation state" "${_old}" "${_new}")

    set(_old [==[
    } else {
        signal_start_recv(g_signal, g_login->relay_url);
    }
]==])
    set(_new [==[
    } else {
        signal_set_message_callback(g_signal, signal_message_update, NULL);
        signal_start_recv(g_signal, g_login->relay_url);
    }
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "enable Signal negotiation callback" "${_old}" "${_new}")

    set(_old [==[
    const char *rurl = g_login->relay_url;
    if (strncmp(rurl, "rels://", 7) == 0) rurl += 7;
    char rhost[256] = {0};
    uint16_t rport = 443;
    const char *rc = strchr(rurl, ':');
    size_t rlen = rc ? (size_t)(rc - rurl) : strlen(rurl);
    if (rlen >= sizeof(rhost)) rlen = sizeof(rhost)-1;
    memcpy(rhost, rurl, rlen);
    if (rc) rport = (uint16_t)strtoul(rc+1, NULL, 10);

    char rerr[256];
    g_relay = relay_connect(g_login->relay_url, rhost, rport,
                            (const char*)g_login->wg_pub_b64,
                            g_login->relay_token_payload, g_login->relay_token_payload_len,
                            g_login->relay_token_sig, g_login->relay_token_sig_len,
                            rerr, sizeof(rerr));
]==])
    set(_new [==[
    char rerr[256];
    g_relay = connect_relay_url(g_login->relay_url, rerr, sizeof(rerr));
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "use URL-aware relay connector" "${_old}" "${_new}")

    set(_old [==[
    relay_start_read_loop(g_relay);
    relay_sendq_start();   // writer thread must exist before WG output starts
    wg_set_relay_send(relay_wg_send);
    fprintf(stderr, "[NETBIRD] relay connected\n");
]==])
    set(_new [==[
    const char *initial_instance = relay_instance_url(g_relay);
    snprintf(g_selected_relay_url, sizeof(g_selected_relay_url), "%s",
             (initial_instance && initial_instance[0]) ? initial_instance : g_login->relay_url);
    relay_start_read_loop(g_relay);
    relay_sendq_start();   // writer thread must exist before WG output starts
    wg_set_relay_send(relay_wg_send);
    fprintf(stderr, "[NETBIRD] relay connected, selected=%s\n", g_selected_relay_url);
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "record initial relay instance" "${_old}" "${_new}")

    set(_old [==[
                signal_send_offer(g_signal, g_login->peers[i].public_key,
                                  g_login->relay_url, 0, sig_err, sizeof(sig_err));
]==])
    set(_new [==[
                const char *offer_relay = selected_relay_url();
                bool controller = strcmp((const char*)g_login->wg_pub_b64,
                                         g_login->peers[i].public_key) > 0;
                NB_INFO("[SIGNAL] TX OFFER peer=%.12s... role=%s relay=%s\n",
                        g_login->peers[i].public_key,
                        controller ? "controller" : "follower", offer_relay);
                signal_send_offer(g_signal, g_login->peers[i].public_key,
                                  offer_relay, 0, sig_err, sizeof(sig_err));
]==])
    _artemis_nb_replace("${stage}/source/netbird_core.c" "advertise exact relay instance" "${_old}" "${_new}")
endfunction()
