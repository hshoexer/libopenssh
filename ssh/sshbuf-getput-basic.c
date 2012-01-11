/*	$OpenBSD$	*/
/*
 * Copyright (c) 2011 Damien Miller
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "err.h"
#define SSHBUF_INTERNAL
#include "sshbuf.h"

int
sshbuf_get(struct sshbuf *buf, void *v, size_t len)
{
	u_char *p = sshbuf_ptr(buf);
	int r;

	if ((r = sshbuf_consume(buf, len)) < 0)
		return r;
	if (v != NULL)
		memcpy(v, p, len);
	return 0;
}

int
sshbuf_get_u64(struct sshbuf *buf, u_int64_t *valp)
{
	u_char *p = sshbuf_ptr(buf);
	int r;

	if ((r = sshbuf_consume(buf, 8)) < 0)
		return r;
	if (valp != NULL)
		*valp = PEEK_U64(p);
	return 0;
}

int
sshbuf_get_u32(struct sshbuf *buf, u_int32_t *valp)
{
	u_char *p = sshbuf_ptr(buf);
	int r;

	if ((r = sshbuf_consume(buf, 4)) < 0)
		return r;
	if (valp != NULL)
		*valp = PEEK_U32(p);
	return 0;
}

int
sshbuf_get_u16(struct sshbuf *buf, u_int16_t *valp)
{
	u_char *p = sshbuf_ptr(buf);
	int r;

	if ((r = sshbuf_consume(buf, 2)) < 0)
		return r;
	if (valp != NULL)
		*valp = PEEK_U16(p);
	return 0;
}

int
sshbuf_get_u8(struct sshbuf *buf, u_char *valp)
{
	u_char *p = sshbuf_ptr(buf);
	int r;

	if ((r = sshbuf_consume(buf, 1)) < 0)
		return r;
	if (valp != NULL)
		*valp = (u_int8_t)*p;
	return 0;
}

int
sshbuf_get_string(struct sshbuf *buf, u_char **valp, size_t *lenp)
{
	const u_char *val;
	size_t len;
	int r;

	if ((r = sshbuf_get_string_direct(buf, &val, &len)) < 0)
		return r;
	if (valp != NULL) {
		if ((*valp = malloc(len + 1)) == NULL) {
			SSHBUF_DBG(("SSH_ERR_ALLOC_FAIL"));
			return SSH_ERR_ALLOC_FAIL;
		}
		memcpy(*valp, val, len);
		(*valp)[len] = '\0';
	}
	if (lenp != NULL)
		*lenp = len;
	return 0;
}

int
sshbuf_get_string_direct(struct sshbuf *buf, const u_char **valp, size_t *lenp)
{
	size_t len;
	const u_char *p;
	int r;

	if ((r = sshbuf_peek_string_direct(buf, &p, &len)) < 0)
		return r;
	if (valp != 0)
		*valp = p;
	if (lenp != NULL)
		*lenp = len;
	if (sshbuf_consume(buf, len + 4) != 0) {
		/* Shouldn't happen */
		SSHBUF_DBG(("SSH_ERR_INTERNAL_ERROR"));
		SSHBUF_ABORT();
		return SSH_ERR_INTERNAL_ERROR;
	}
	return 0;
}

int
sshbuf_peek_string_direct(const struct sshbuf *buf, const u_char **valp,
    size_t *lenp)
{
	u_int32_t len;
	u_char *p = sshbuf_ptr(buf);

	if (sshbuf_len(buf) < 4) {
		SSHBUF_DBG(("SSH_ERR_MESSAGE_INCOMPLETE"));
		return SSH_ERR_MESSAGE_INCOMPLETE;
	}
	len = PEEK_U32(p);
	if (sshbuf_len(buf) - 4 < len) {
		SSHBUF_DBG(("SSH_ERR_MESSAGE_INCOMPLETE"));
		return SSH_ERR_MESSAGE_INCOMPLETE;
	}
	if (valp != 0)
		*valp = p + 4;
	if (lenp != NULL)
		*lenp = len;
	return 0;
}

int
sshbuf_get_cstring(struct sshbuf *buf, char **valp, size_t *lenp)
{
	u_int32_t len;
	u_char *p = sshbuf_ptr(buf), *z;
	int r;

	if (sshbuf_len(buf) < 4) {
		SSHBUF_DBG(("SSH_ERR_MESSAGE_INCOMPLETE"));
		return SSH_ERR_MESSAGE_INCOMPLETE;
	}
	len = PEEK_U32(p);
	if (sshbuf_len(buf) < (size_t)len + 4) {
		SSHBUF_DBG(("SSH_ERR_MESSAGE_INCOMPLETE"));
		return SSH_ERR_MESSAGE_INCOMPLETE;
	}
	/* Allow a \0 only at the end of the string */
	if ((z = memchr(p + 4, '\0', len)) != NULL && z < p + 4 + len - 1) {
		SSHBUF_DBG(("SSH_ERR_INVALID_FORMAT"));
		return SSH_ERR_INVALID_FORMAT;
	}
	if ((r = sshbuf_consume(buf, 4 + (size_t)len)) < 0)
		return -1;
	if (valp != NULL) {
		if ((*valp = malloc(len + 1)) == NULL) {
			SSHBUF_DBG(("SSH_ERR_ALLOC_FAIL"));
			return SSH_ERR_ALLOC_FAIL;
		}
		memcpy(*valp, p + 4, len);
		(*valp)[len] = '\0';
	}
	if (lenp != NULL)
		*lenp = (size_t)len;
	return 0;
}

int
sshbuf_put(struct sshbuf *buf, const void *v, size_t len)
{
	u_char *p;
	int r;

	if ((r = sshbuf_reserve(buf, len, &p)) < 0)
		return r;
	memcpy(p, v, len);
	return 0;
}

int
sshbuf_putb(struct sshbuf *buf, const struct sshbuf *v)
{
	return sshbuf_put(buf, sshbuf_ptr(v), sshbuf_len(v));
}

int
sshbuf_putf(struct sshbuf *buf, const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = sshbuf_putfv(buf, fmt, ap);
	va_end(ap);
	return r;
}

int
sshbuf_putfv(struct sshbuf *buf, const char *fmt, va_list ap)
{
	va_list ap2;
	int r, len;
	u_char *p;

	va_copy(ap2, ap);
	if ((len = vsnprintf(NULL, 0, fmt, ap2)) < 0) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if (len == 0) {
		r = 0;
		goto out; /* Nothing to do */
	}
	va_end(ap2);
	va_copy(ap2, ap);
	if ((r = sshbuf_reserve(buf, (size_t)len + 1, &p)) < 0)
		goto out;
	if ((r = vsnprintf(p, len + 1, fmt, ap2)) != len) {
		r = SSH_ERR_INTERNAL_ERROR;
		goto out; /* Shouldn't happen */
	}
	/* Consume terminating \0 */
	if ((r = sshbuf_consume_end(buf, 1)) != 0)
		goto out;
	r = 0;
 out:
	va_end(ap2);
	return r;
}

int
sshbuf_put_u64(struct sshbuf *buf, u_int64_t val)
{
	u_char *p;
	int r;

	if ((r = sshbuf_reserve(buf, 8, &p)) < 0)
		return r;
	POKE_U64(p, val);
	return 0;
}

int
sshbuf_put_u32(struct sshbuf *buf, u_int32_t val)
{
	u_char *p;
	int r;

	if ((r = sshbuf_reserve(buf, 4, &p)) < 0)
		return r;
	POKE_U32(p, val);
	return 0;
}

int
sshbuf_put_u16(struct sshbuf *buf, u_int16_t val)
{
	u_char *p;
	int r;

	if ((r = sshbuf_reserve(buf, 2, &p)) < 0)
		return r;
	POKE_U16(p, val);
	return 0;
}

int
sshbuf_put_u8(struct sshbuf *buf, u_char val)
{
	u_char *p;
	int r;

	if ((r = sshbuf_reserve(buf, 1, &p)) < 0)
		return r;
	p[0] = val;
	return 0;
}

int
sshbuf_put_string(struct sshbuf *buf, const void *v, size_t len)
{
	u_char *d;
	int r;

	if (len > 0xFFFFFFFF - 4) {
		SSHBUF_DBG(("SSH_ERR_NO_BUFFER_SPACE"));
		return SSH_ERR_NO_BUFFER_SPACE;
	}
	if ((r = sshbuf_reserve(buf, len + 4, &d)) < 0)
		return r;
	POKE_U32(d, len);
	memcpy(d + 4, v, len);
	return 0;
}

int
sshbuf_put_cstring(struct sshbuf *buf, const char *v)
{
	return sshbuf_put_string(buf, (u_char *)v, strlen(v));
}

int
sshbuf_put_stringb(struct sshbuf *buf, const struct sshbuf *v)
{
	return sshbuf_put_string(buf, sshbuf_ptr(v), sshbuf_len(v));
}

