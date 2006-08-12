/*
 * lua-rdiff
 * (c) 2006 Javier Guerra G.
 * $Id: lua_rdiff.c,v 1.1.1.2 2006-08-12 16:33:34 jguerra Exp $
 */
 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

#include <librsync.h>

#define TOO_LOW 512

struct lua_cb_obj {
	rs_driven_cb	*cb;		/* callback */
	rs_copy_cb		*rcb;		/* random access callback */
	char *			buf;
	size_t			len;
	size_t			bufsize;
	lua_State		*L;
	int				luacb;
};
typedef struct lua_cb_obj lua_cb_obj;

static void set_cbbuf (lua_cb_obj *cb, const char *buf, size_t len) {
	if (buf == NULL || (cb->buf != NULL && cb->bufsize < len)) {
		free (cb->buf);
		cb->buf = NULL;
		cb->len = 0;
		cb->bufsize = 0;
	}
	if (cb->buf == NULL && buf != NULL) {
		cb->buf = malloc (len);
		if (cb->buf != NULL)
			cb->bufsize = len;
	}
	if (cb->buf != NULL && cb->bufsize >= len) {
		memcpy (cb->buf, buf, len);
		cb->len = len;
	}
}

static void resize_cbbuf (lua_cb_obj *cb, size_t len) {
	char *newbuf;
	
	if (cb->bufsize >= len)
		return;
	
	newbuf = malloc (len);
	if (newbuf != NULL) {
		if (cb->buf != NULL) {
			memcpy (newbuf, cb->buf, cb->len);
			free (cb->buf);
		}
		cb->buf = newbuf;
		cb->bufsize = len;
	}
}

/*
static void add_cbbuf (lua_cb_obj *cb, const char *buf, size_t len) {
	if (!buf)
		return;
	
	if (cb->buf == NULL || cb->len == 0)
		set_cbbuf (cb, buf, len);
	
	resize_cbbuf (cb, cb->len + len);
	
	if (cb->bufsize >= cb->len + len) {
		memcpy (cb->buf+cb->len, buf, len);
		cb->len += len;
	}
}
*/

static rs_result string_fill_cb (rs_job_t *job, rs_buffers_t *buf, void *opaque)
{
	lua_cb_obj	*cb = (lua_cb_obj *)opaque;
	
	assert (cb->buf);
	assert (cb->bufsize >= 0);
	
	if (buf->next_in == NULL) {
		buf->next_in = cb->buf;
		buf->avail_in = cb->len;
		buf->eof_in = 0;
		
	} else if (buf->next_in >= cb->buf && buf->next_in < cb->buf+cb->len) {
		buf->avail_in = cb->len - (buf->next_in - cb->buf);
		buf->eof_in = 0;
		
	} else {
		buf->avail_in = 0;
		buf->eof_in = 1;
	}
	
	return RS_DONE;
}

static rs_result string_rfill_cb (void *opaque, rs_long_t pos, size_t *len, void **buf)
{
	lua_cb_obj	*cb = (lua_cb_obj *)opaque;
	
	if (pos <= cb->len) {
		*buf = cb->buf + pos;
		*len = cb->len - pos;
		return RS_DONE;
		
	} else {
		return RS_IO_ERROR;
	}
}

static rs_result luafunc_fill_cb (rs_job_t *job, rs_buffers_t *buf, void *opaque)
{
	lua_cb_obj	*cb = (lua_cb_obj *)opaque;
	rs_result	r = RS_DONE;
	const char	*s;
	size_t		slen;
		
	if (cb->buf != NULL) {
		r = string_fill_cb (job, buf, opaque);
		if (!buf->eof_in)
			return r;
	}
		
	lua_getref (cb->L, cb->luacb);
	switch (lua_pcall (cb->L, 0, 1, 0)) {
		
		case 0:
			s = lua_tolstring (cb->L, -1, &slen);
			if (s != NULL) {
				set_cbbuf (cb, s, slen);
				buf->next_in = cb->buf;
				buf->avail_in = cb->len;
				buf->eof_in = 0;
			} else
				buf->eof_in = 1;
			return RS_DONE;
			break;
		
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
		default:
			lua_pop (cb->L, 1);
			return RS_INTERNAL_ERROR;
			break;
	}
}

static rs_result luafunc_rfill_cb (void *opaque, rs_long_t pos, size_t *len, void **buf)
{
	lua_cb_obj	*cb = (lua_cb_obj *)opaque;
	const char	*s;
	size_t		slen;

	lua_getref (cb->L, cb->luacb);
	lua_pushnumber (cb->L, pos);
	lua_pushnumber (cb->L, *len);
	
	if (lua_pcall (cb->L, 2, 1, 0) == 0
		&& (s = lua_tolstring (cb->L, -1, &slen)) != NULL)
	{
		if (*len > slen)
			*len = slen;
		memcpy (buf, s, *len);
		
		return RS_DONE;
	}
	
	lua_pop (cb->L, 1);
	return RS_INTERNAL_ERROR;
}

static rs_result null_sink_cb (rs_job_t *job, rs_buffers_t *buf, void *opaque)
{
	lua_cb_obj	*cb = (lua_cb_obj *)opaque;
		
	if (buf->next_out == NULL) {
		assert(buf->avail_out == 0);
		resize_cbbuf (cb, RS_DEFAULT_BLOCK_LEN);
		buf->next_out = cb->buf;
		buf->avail_out = cb->bufsize;
		
		return RS_DONE;
	}
	
	cb->len = buf->next_out-cb->buf;
	buf->avail_out = cb->bufsize - cb->len;
	
	if (buf->avail_out < TOO_LOW) {
		resize_cbbuf (cb, cb->len + RS_DEFAULT_BLOCK_LEN);
		buf->next_out = cb->buf+cb->len;
		buf->avail_out = cb->bufsize - cb->len;
	}
	
	return RS_DONE;
}

static rs_result luafunc_sink_cb (rs_job_t *job, rs_buffers_t *buf, void *opaque)
{
	lua_cb_obj	*cb = (lua_cb_obj *)opaque;
	
	if (buf->next_out == NULL) {
		assert(buf->avail_out == 0);
		resize_cbbuf (cb, RS_DEFAULT_BLOCK_LEN);
		buf->next_out = cb->buf;
		buf->avail_out = cb->bufsize;
		
		return RS_DONE;
	}
	
	cb->len = buf->next_out-cb->buf;
	lua_getref (cb->L, cb->luacb);
	lua_pushlstring (cb->L, cb->buf, cb->len);
	lua_pcall (cb->L, 1, 0, 0);
	
	buf->next_out = cb->buf;
	buf->avail_out = cb->bufsize;	
	
	return RS_DONE;
}


static int get_source_cb (lua_State *L, int narg, lua_cb_obj *cb) {
	const char *instring;
	size_t slen;
	
	memset (cb, 0, sizeof (*cb));
	
	instring = lua_tolstring (L, narg, &slen);
	if (instring != NULL) {
		cb->cb = string_fill_cb;
		cb->rcb = string_rfill_cb;
		cb->len = slen;
		cb->buf = malloc (slen);
		if (!cb->buf)
			return 0;
		cb->bufsize = slen;
		memcpy (cb->buf, instring, slen);
		return 1;
		
	} else if (lua_type (L, narg) == LUA_TFUNCTION) {
		cb->cb = luafunc_fill_cb;
		cb->rcb = luafunc_rfill_cb;
		cb->L = L;
		lua_pushvalue (L, narg);
		cb->luacb = lua_ref (L, 1);
		return 1;
	}
	
	return 0;
}

static int get_sink_cb (lua_State *L, int narg, lua_cb_obj *cb) {
	memset (cb, 0, sizeof (*cb));
		
	switch (lua_type (L, narg)) {
		
		case LUA_TNONE:
		case LUA_TNIL:
			cb->cb = null_sink_cb;
			return 1;
			break;
			
		case LUA_TFUNCTION:
			cb->cb = luafunc_sink_cb;
			cb->L = L;
			lua_pushvalue (L, narg);
			cb->luacb = lua_ref (L, 1);
			return 1;
			break;
		
		default:
			return 0;
	}
}


static void lua_cb_free (lua_cb_obj *cb) {
	set_cbbuf (cb, NULL, 0);
	if (cb->L != NULL)
		lua_unref (cb->L, cb->luacb);

	memset (cb, 0, sizeof (*cb));
}

/*
 * rdiff.signature (source [, sink])
 *
 * source can be a string or a function
 * If sink isn't given, rdiff.signature returns
 * a string on success and nil,error on failure.
 * If sink is a function, it's called repeatedly
 * with new output. On sucess rdiff.signature
 * returns true, and nil,error on failure.
 */
static int lrd_signature (lua_State *L) {
	rs_job_t        *job;
	rs_buffers_t    buf;
	rs_result       r;
	lua_cb_obj		source, sink;
		
	if (!get_source_cb (L, 1, &source))
		luaL_argerror (L, 1, "Not a suitable source");
	
	if (!get_sink_cb (L, 2, &sink)) {
		lua_cb_free (&source);
		luaL_argerror (L, 2, "Not a suitable destination");
	}
	
	job = rs_sig_begin (RS_DEFAULT_BLOCK_LEN, RS_DEFAULT_STRONG_LEN);
	r = rs_job_drive (job, &buf, source.cb, &source, sink.cb, &sink);
	rs_job_free(job);
	
	if (r == RS_DONE) {
		if (sink.L == NULL)
			lua_pushlstring (L, sink.buf, sink.len);
		else
			lua_pushboolean (L, 1);
	} else {
		lua_pushnil (L);
	}
	
	lua_cb_free (&sink);
	lua_cb_free (&source);
	
	if (r != RS_DONE) {
		lua_pushfstring (L, "%s", rs_strerror (r));
		return 2;
	} else
		return 1;
}

/*
 * rdiff.delta (signature, new [, sink])
 */
static int lrd_delta (lua_State *L)
{
	lua_cb_obj sign, src, delta;
	rs_signature_t  *sumset;
	
	if (!get_source_cb (L, 1, &sign))
		luaL_argerror (L, 1, "Not a suitable source");
	if (!get_source_cb (L, 2, &src)) {
		lua_cb_free (&sign);
		luaL_argerror (L, 2, "Not a suitable source");
	}
	if (!get_sink_cb (L, 3, &delta)) {
		lua_cb_free (&sign);
		lua_cb_free (&src);
		luaL_argerror (L, 3, "Not a suitable destination");
	}
	
	/* load signature */
	{
		rs_job_t *job;
		rs_buffers_t buf;
		rs_result r;
		
		job = rs_loadsig_begin (&sumset);
		r = rs_job_drive (job, &buf, sign.cb, &sign, NULL, NULL);
		rs_job_free (job);
		lua_cb_free (&sign);
		
		if (r != RS_DONE ) {
			lua_cb_free (&src);
			lua_cb_free (&delta);
			lua_pushnil (L);
			lua_pushfstring (L, "%s", rs_strerror (r));
			return 2;
		}
		if ((r = rs_build_hash_table(sumset)) != RS_DONE) {
			lua_cb_free (&src);
			lua_cb_free (&delta);
			lua_pushnil (L);
			lua_pushfstring (L, "%s", rs_strerror (r));
			return 2;
		}
	}
	
	/* calc delta */
	{
		rs_job_t *job;
		rs_buffers_t buf;
		rs_result r;
		
		job = rs_delta_begin (sumset);
		r = rs_job_drive (job, &buf, src.cb, &src, delta.cb, &delta);
		rs_job_free (job);
		rs_free_sumset(sumset);
		lua_cb_free (&src);
		if (r != RS_DONE) {
			lua_pushnil (L);
			lua_pushfstring (L, "%s", rs_strerror (r));
			return 2;
		}
	}
	
	if (delta.L == NULL)
		lua_pushlstring (L, delta.buf, delta.len);
	else
		lua_pushboolean (L, 1);
	
	lua_cb_free (&delta);
	return 1;
}

/*
 * rdiff.patch (delta, base [, sink])
 */
static int lrd_patch (lua_State *L)
{
	lua_cb_obj delta, base, sink;
	rs_job_t *job;
	rs_buffers_t buf;
	rs_result r;
	
	if (!get_source_cb (L, 1, &delta))
		luaL_argerror (L, 1, "Not a suitable source");
	if (!get_source_cb (L, 2, &base)) {
		lua_cb_free (&delta);
		luaL_argerror (L, 2, "Not a suitable source");
	}
	if (!get_sink_cb (L, 3, &sink)) {
		lua_cb_free (&delta);
		lua_cb_free (&base);
		luaL_argerror (L, 3, "Not a suitable destination");
	}
	
	job = rs_patch_begin (base.rcb, &base);
	r = rs_job_drive (job, &buf, delta.cb, &delta, sink.cb, &sink);
	rs_job_free (job);
	lua_cb_free (&delta);
	lua_cb_free (&base);
	
	if (r != RS_DONE) {
		lua_cb_free (&sink);
		lua_pushnil (L);
		lua_pushfstring (L, "%s", rs_strerror (r));
		return 2;
	}
	
	if (sink.L == NULL)
		lua_pushlstring (L, sink.buf, sink.len);
	else
		lua_pushboolean (L, 1);
	
	lua_cb_free (&sink);
	return 1;	
}

static const struct luaL_reg rdiff_funcs[] = {
	{"signature", lrd_signature},
	{"delta", lrd_delta},
	{"patch", lrd_patch},
	{NULL, NULL}
};

/*
** Assumes the table is on top of the stack.
*/
static void set_info (lua_State *L) {
	lua_pushliteral (L, "_COPYRIGHT");
	lua_pushliteral (L, "Copyright (C) 2006 Javier Guerra");
	lua_settable (L, -3);
	lua_pushliteral (L, "_DESCRIPTION");
	lua_pushliteral (L, "librsync binding");
	lua_settable (L, -3);
	lua_pushliteral (L, "_NAME");
	lua_pushliteral (L, "lua_rsync");
	lua_settable (L, -3);
	lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, "0.1");
	lua_settable (L, -3);
}

int luaopen_lua_rdiff (lua_State *L);
int luaopen_lua_rdiff (lua_State *L)
{
	luaL_openlib (L, "rdiff", rdiff_funcs, 0);
	set_info (L);

	return 1;
}
