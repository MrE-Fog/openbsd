/* $OpenBSD: tls13_internal.h,v 1.63 2020/04/18 14:07:56 jsing Exp $ */
/*
 * Copyright (c) 2018 Bob Beck <beck@openbsd.org>
 * Copyright (c) 2018 Theo Buehler <tb@openbsd.org>
 * Copyright (c) 2018, 2019 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HEADER_TLS13_INTERNAL_H
#define HEADER_TLS13_INTERNAL_H

#include <openssl/evp.h>
#include <openssl/ssl.h>

#include "bytestring.h"

__BEGIN_HIDDEN_DECLS

#define TLS13_HS_CLIENT		1
#define TLS13_HS_SERVER		2

#define TLS13_IO_SUCCESS	 1
#define TLS13_IO_EOF		 0
#define TLS13_IO_FAILURE	-1
#define TLS13_IO_ALERT		-2
#define TLS13_IO_WANT_POLLIN	-3
#define TLS13_IO_WANT_POLLOUT	-4
#define TLS13_IO_WANT_RETRY	-5 /* Retry the previous call immediately. */
#define TLS13_IO_USE_LEGACY	-6

#define TLS13_ERR_VERIFY_FAILED		16
#define TLS13_ERR_HRR_FAILED		17
#define TLS13_ERR_TRAILING_DATA		18
#define TLS13_ERR_NO_SHARED_CIPHER	19

typedef void (*tls13_alert_cb)(uint8_t _alert_desc, void *_cb_arg);
typedef ssize_t (*tls13_phh_recv_cb)(void *_cb_arg, CBS *_cbs);
typedef void (*tls13_phh_sent_cb)(void *_cb_arg);
typedef ssize_t (*tls13_read_cb)(void *_buf, size_t _buflen, void *_cb_arg);
typedef ssize_t (*tls13_write_cb)(const void *_buf, size_t _buflen,
    void *_cb_arg);
typedef void (*tls13_handshake_message_cb)(void *_cb_arg);

/*
 * Buffers.
 */
struct tls13_buffer;

struct tls13_buffer *tls13_buffer_new(size_t init_size);
int tls13_buffer_set_data(struct tls13_buffer *buf, CBS *data);
void tls13_buffer_free(struct tls13_buffer *buf);
ssize_t tls13_buffer_extend(struct tls13_buffer *buf, size_t len,
    tls13_read_cb read_cb, void *cb_arg);
void tls13_buffer_cbs(struct tls13_buffer *buf, CBS *cbs);
int tls13_buffer_finish(struct tls13_buffer *buf, uint8_t **out,
    size_t *out_len);

/*
 * Secrets.
 */
struct tls13_secret {
	uint8_t *data;
	size_t len;
};

/* RFC 8446 Section 7.1  Page 92 */
struct tls13_secrets {
	const EVP_MD *digest;
	int resumption;
	int init_done;
	int early_done;
	int handshake_done;
	int schedule_done;
	int insecure; /* Set by tests */
	struct tls13_secret zeros;
	struct tls13_secret empty_hash;
	struct tls13_secret extracted_early;
	struct tls13_secret binder_key;
	struct tls13_secret client_early_traffic;
	struct tls13_secret early_exporter_master;
	struct tls13_secret derived_early;
	struct tls13_secret extracted_handshake;
	struct tls13_secret client_handshake_traffic;
	struct tls13_secret server_handshake_traffic;
	struct tls13_secret derived_handshake;
	struct tls13_secret extracted_master;
	struct tls13_secret client_application_traffic;
	struct tls13_secret server_application_traffic;
	struct tls13_secret exporter_master;
	struct tls13_secret resumption_master;
};

struct tls13_secrets *tls13_secrets_create(const EVP_MD *digest,
    int resumption);
void tls13_secrets_destroy(struct tls13_secrets *secrets);

int tls13_hkdf_expand_label(struct tls13_secret *out, const EVP_MD *digest,
    const struct tls13_secret *secret, const char *label,
    const struct tls13_secret *context);

int tls13_derive_early_secrets(struct tls13_secrets *secrets, uint8_t *psk,
    size_t psk_len, const struct tls13_secret *context);
int tls13_derive_handshake_secrets(struct tls13_secrets *secrets,
    const uint8_t *ecdhe, size_t ecdhe_len, const struct tls13_secret *context);
int tls13_derive_application_secrets(struct tls13_secrets *secrets,
    const struct tls13_secret *context);
int tls13_update_client_traffic_secret(struct tls13_secrets *secrets);
int tls13_update_server_traffic_secret(struct tls13_secrets *secrets);

/*
 * Key shares.
 */
struct tls13_key_share;

struct tls13_key_share *tls13_key_share_new(uint16_t group_id);
struct tls13_key_share *tls13_key_share_new_nid(int nid);
void tls13_key_share_free(struct tls13_key_share *ks);

uint16_t tls13_key_share_group(struct tls13_key_share *ks);
int tls13_key_share_peer_pkey(struct tls13_key_share *ks, EVP_PKEY *pkey);
int tls13_key_share_generate(struct tls13_key_share *ks);
int tls13_key_share_public(struct tls13_key_share *ks, CBB *cbb);
int tls13_key_share_peer_public(struct tls13_key_share *ks, uint16_t group,
    CBS *cbs);
int tls13_key_share_derive(struct tls13_key_share *ks, uint8_t **shared_key,
    size_t *shared_key_len);

/*
 * Record Layer.
 */
struct tls13_record_layer;

struct tls13_record_layer *tls13_record_layer_new(tls13_read_cb wire_read,
    tls13_write_cb wire_write, tls13_alert_cb alert_cb,
    tls13_phh_recv_cb phh_recv_cb,
    tls13_phh_sent_cb phh_sent_cb, void *cb_arg);
void tls13_record_layer_free(struct tls13_record_layer *rl);
void tls13_record_layer_allow_ccs(struct tls13_record_layer *rl, int allow);
void tls13_record_layer_allow_legacy_alerts(struct tls13_record_layer *rl, int allow);
void tls13_record_layer_rbuf(struct tls13_record_layer *rl, CBS *cbs);
void tls13_record_layer_set_aead(struct tls13_record_layer *rl,
    const EVP_AEAD *aead);
void tls13_record_layer_set_hash(struct tls13_record_layer *rl,
    const EVP_MD *hash);
void tls13_record_layer_set_legacy_version(struct tls13_record_layer *rl,
    uint16_t version);
void tls13_record_layer_handshake_completed(struct tls13_record_layer *rl);
int tls13_record_layer_set_read_traffic_key(struct tls13_record_layer *rl,
    struct tls13_secret *read_key);
int tls13_record_layer_set_write_traffic_key(struct tls13_record_layer *rl,
    struct tls13_secret *write_key);
ssize_t tls13_record_layer_send_pending(struct tls13_record_layer *rl);
ssize_t tls13_record_layer_phh(struct tls13_record_layer *rl, CBS *cbs);

ssize_t tls13_read_handshake_data(struct tls13_record_layer *rl, uint8_t *buf, size_t n);
ssize_t tls13_write_handshake_data(struct tls13_record_layer *rl, const uint8_t *buf,
    size_t n);
ssize_t tls13_pending_application_data(struct tls13_record_layer *rl);
ssize_t tls13_peek_application_data(struct tls13_record_layer *rl, uint8_t *buf, size_t n);
ssize_t tls13_read_application_data(struct tls13_record_layer *rl, uint8_t *buf, size_t n);
ssize_t tls13_write_application_data(struct tls13_record_layer *rl, const uint8_t *buf,
    size_t n);

ssize_t tls13_send_alert(struct tls13_record_layer *rl, uint8_t alert_desc);

/*
 * Handshake Messages.
 */
struct tls13_handshake_msg;

struct tls13_handshake_msg *tls13_handshake_msg_new(void);
void tls13_handshake_msg_free(struct tls13_handshake_msg *msg);
void tls13_handshake_msg_data(struct tls13_handshake_msg *msg, CBS *cbs);
int tls13_handshake_msg_set_buffer(struct tls13_handshake_msg *msg, CBS *cbs);
uint8_t tls13_handshake_msg_type(struct tls13_handshake_msg *msg);
int tls13_handshake_msg_content(struct tls13_handshake_msg *msg, CBS *cbs);
int tls13_handshake_msg_start(struct tls13_handshake_msg *msg, CBB *body,
    uint8_t msg_type);
int tls13_handshake_msg_finish(struct tls13_handshake_msg *msg);
int tls13_handshake_msg_recv(struct tls13_handshake_msg *msg,
    struct tls13_record_layer *rl);
int tls13_handshake_msg_send(struct tls13_handshake_msg *msg,
    struct tls13_record_layer *rl);

struct tls13_handshake_stage {
	uint8_t	hs_type;
	uint8_t	message_number;
};

struct ssl_handshake_tls13_st;

struct tls13_error {
	int code;
	int subcode;
	int errnum;
	const char *file;
	int line;
	char *msg;
};

struct tls13_ctx {
	struct tls13_error error;

	SSL *ssl;
	struct ssl_handshake_tls13_st *hs;
	uint8_t	mode;
	struct tls13_handshake_stage handshake_stage;
	int handshake_completed;

	int close_notify_sent;
	int close_notify_recv;

	const EVP_AEAD *aead;
	const EVP_MD *hash;

	struct tls13_record_layer *rl;
	struct tls13_handshake_msg *hs_msg;
	uint8_t key_update_request;
	uint8_t alert;
	int phh_count;
	time_t phh_last_seen;

	tls13_handshake_message_cb handshake_message_sent_cb;
	tls13_handshake_message_cb handshake_message_recv_cb;
};
#ifndef TLS13_PHH_LIMIT_TIME
#define TLS13_PHH_LIMIT_TIME 3600
#endif
#ifndef TLS13_PHH_LIMIT
#define TLS13_PHH_LIMIT 100
#endif

struct tls13_ctx *tls13_ctx_new(int mode);
void tls13_ctx_free(struct tls13_ctx *ctx);

const EVP_AEAD *tls13_cipher_aead(const SSL_CIPHER *cipher);
const EVP_MD *tls13_cipher_hash(const SSL_CIPHER *cipher);

/*
 * Legacy interfaces.
 */
int tls13_legacy_accept(SSL *ssl);
int tls13_legacy_connect(SSL *ssl);
int tls13_legacy_return_code(SSL *ssl, ssize_t ret);
ssize_t tls13_legacy_wire_read_cb(void *buf, size_t n, void *arg);
ssize_t tls13_legacy_wire_write_cb(const void *buf, size_t n, void *arg);
int tls13_legacy_pending(const SSL *ssl);
int tls13_legacy_read_bytes(SSL *ssl, int type, unsigned char *buf, int len,
    int peek);
int tls13_legacy_write_bytes(SSL *ssl, int type, const void *buf, int len);
int tls13_legacy_shutdown(SSL *ssl);

/*
 * Message Types - RFC 8446, Section B.3.
 *
 * Values listed as "_RESERVED" were used in previous versions of TLS and are
 * listed here for completeness.  TLS 1.3 implementations MUST NOT send them but
 * might receive them from older TLS implementations.
 */
#define	TLS13_MT_HELLO_REQUEST_RESERVED		0
#define	TLS13_MT_CLIENT_HELLO			1
#define	TLS13_MT_SERVER_HELLO			2
#define	TLS13_MT_HELLO_VERIFY_REQUEST_RESERVED	3
#define	TLS13_MT_NEW_SESSION_TICKET		4
#define	TLS13_MT_END_OF_EARLY_DATA		5
#define	TLS13_MT_HELLO_RETRY_REQUEST_RESERVED	6
#define	TLS13_MT_ENCRYPTED_EXTENSIONS		8
#define	TLS13_MT_CERTIFICATE			11
#define	TLS13_MT_SERVER_KEY_EXCHANGE_RESERVED	12
#define	TLS13_MT_CERTIFICATE_REQUEST		13
#define	TLS13_MT_SERVER_HELLO_DONE_RESERVED	14
#define	TLS13_MT_CERTIFICATE_VERIFY		15
#define	TLS13_MT_CLIENT_KEY_EXCHANGE_RESERVED	16
#define	TLS13_MT_FINISHED			20
#define	TLS13_MT_CERTIFICATE_URL_RESERVED	21
#define	TLS13_MT_CERTIFICATE_STATUS_RESERVED	22
#define	TLS13_MT_SUPPLEMENTAL_DATA_RESERVED	23
#define	TLS13_MT_KEY_UPDATE			24
#define	TLS13_MT_MESSAGE_HASH			254

int tls13_handshake_msg_record(struct tls13_ctx *ctx);
int tls13_handshake_perform(struct tls13_ctx *ctx);

int tls13_client_hello_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_client_hello_sent(struct tls13_ctx *ctx);
int tls13_client_hello_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_client_hello_retry_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_client_hello_retry_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_client_end_of_early_data_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_client_end_of_early_data_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_client_certificate_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_client_certificate_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_client_certificate_verify_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_client_certificate_verify_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_client_finished_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_client_finished_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_client_finished_sent(struct tls13_ctx *ctx);
int tls13_server_hello_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_server_hello_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_server_hello_sent(struct tls13_ctx *ctx);
int tls13_server_hello_retry_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_server_hello_retry_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_server_encrypted_extensions_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_server_certificate_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_server_certificate_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_server_certificate_request_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_server_certificate_request_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_server_certificate_verify_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_server_certificate_verify_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_server_finished_recv(struct tls13_ctx *ctx, CBS *cbs);
int tls13_server_finished_send(struct tls13_ctx *ctx, CBB *cbb);
int tls13_server_finished_sent(struct tls13_ctx *ctx);

void tls13_error_clear(struct tls13_error *error);

int tls13_cert_add(CBB *cbb, X509 *cert);

int tls13_error_set(struct tls13_error *error, int code, int subcode,
    const char *file, int line, const char *fmt, ...);
int tls13_error_setx(struct tls13_error *error, int code, int subcode,
    const char *file, int line, const char *fmt, ...);

#define tls13_set_error(ctx, code, subcode, fmt, ...) \
	tls13_error_set(&(ctx)->error, (code), (subcode), __FILE__, __LINE__, \
	    (fmt), __VA_ARGS__)
#define tls13_set_errorx(ctx, code, subcode, fmt, ...) \
	tls13_error_setx(&(ctx)->error, (code), (subcode), __FILE__, __LINE__, \
	    (fmt), __VA_ARGS__)

extern uint8_t tls13_downgrade_12[8];
extern uint8_t tls13_downgrade_11[8];
extern uint8_t tls13_cert_verify_pad[64];
extern uint8_t tls13_cert_client_verify_context[];
extern uint8_t tls13_cert_server_verify_context[];

__END_HIDDEN_DECLS

#endif
