#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <mdbx.h>

#define MDBX_ENV_META "mdbx.meta.env"
#define MDBX_TXN_META "mdbx.meta.txn"
#define MDBX_DBI_META "mdbx.meta.dbi"
#define MDBX_CURSOR_META "mdbx.meta.cursor"

static int
lua_mdbx_env_init(lua_State *L) {
	MDBX_env *env, **penv;
	const char *path;
	unsigned int flags, maxreaders, maxdbs;
	int rc;
	size_t path_len;

	path = luaL_checklstring(L, 1, &path_len);
	if (path_len == 0) {
		lua_pushnil(L);
		lua_pushliteral(L, "empty path");
		return (1);
	}

	flags = lua_tonumber(L, 2);
	if (flags == 0) {
		flags = MDBX_NOSUBDIR;
#ifdef __FreeBSD__
		flags |= MDBX_NOTLS;
#endif
	}

	maxreaders = lua_tonumber(L, 3);
	if (maxreaders == 0) {
		maxreaders = 1;
	}

	maxdbs = lua_tonumber(L, 4);
	if (maxdbs == 0) {
		maxdbs = 120;
	}

	rc = mdbx_env_create(&env);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushliteral(L, "fail to create env");
		return (1);
	}

	if (maxreaders > 0) {
		rc = mdbx_env_set_maxreaders(env, maxreaders);
		if (rc != MDBX_SUCCESS) {
			lua_pushnil(L);
			lua_pushfstring(L, "fail to set maxreaders: %s", mdbx_strerror(rc));
			return (1);
		}
	}

	if (maxdbs > 0) {
		rc = mdbx_env_set_maxdbs(env, maxdbs);
		if (rc != MDBX_SUCCESS) {
			lua_pushnil(L);
			lua_pushfstring(L, "fail to set maxdbs: %s", mdbx_strerror(rc));
			return (1);
		}
	}

	rc = mdbx_env_open(env, path, flags, 0664);
	if (rc != MDBX_SUCCESS) {
		mdbx_env_close(env);
		lua_pushnil(L);
		lua_pushfstring(L, "fail to open env: %s", mdbx_strerror(rc));
		return (1);
	}

	penv = lua_newuserdata(L, sizeof(env));
	*penv = env;
	luaL_getmetatable(L, MDBX_ENV_META);
	lua_setmetatable(L, -2);

	return (1);
}

static MDBX_env *
lua_mdbx_env_fetch(lua_State *L, int index) {
	return *((MDBX_env **)luaL_checkudata(L, index, MDBX_ENV_META));
}

static int
lua_mdbx_env_get_path(lua_State *L) {
	MDBX_env *env = lua_mdbx_env_fetch(L, 1);
	const char *path;
	int rc;

	rc = mdbx_env_get_path(env, &path);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushliteral(L, "fail to get path");
		return (1);
	}

	lua_pushstring(L, path);

	return (1);
}

static int
lua_mdbx_env_get_fd(lua_State *L) {
	MDBX_env *env = lua_mdbx_env_fetch(L, 1);
	int rc, fd;

	rc = mdbx_env_get_fd(env, &fd);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushliteral(L, "fail to get fd");
		return (1);
	}

	lua_pushinteger(L, fd);

	return (1);
}

static int
lua_mdbx_env_get_maxdbs(lua_State *L) {
	MDBX_env *env = lua_mdbx_env_fetch(L, 1);
	int rc;
	unsigned int dbs;

	rc = mdbx_env_get_maxdbs(env, &dbs);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushliteral(L, "fail to get maxdbs");
		return (1);
	}

	lua_pushinteger(L, dbs);

	return (1);
}

static int
lua_mdbx_env_get_maxreaders(lua_State *L) {
	MDBX_env *env = lua_mdbx_env_fetch(L, 1);
	int rc;
	unsigned int readers;

	rc = mdbx_env_get_maxreaders(env, &readers);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushliteral(L, "fail to get maxreaders");
		return (1);
	}

	lua_pushinteger(L, readers);

	return (1);
}

static int
lua_mdbx_env_set_option(lua_State *L) {
	MDBX_env *env = lua_mdbx_env_fetch(L, 1);
	int option, value, rc;

	option = luaL_checkinteger(L, 2);
	value = luaL_checkinteger(L, 3);

	rc = mdbx_env_set_option(env, option, value);
	if (rc != MDBX_SUCCESS) {
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "fail to set option: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_env_close(lua_State *L) {
	MDBX_env *env = lua_mdbx_env_fetch(L, 1);

	if (env != NULL) {
		mdbx_env_close(env);
		env = NULL;
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	return (1);
}

static int
lua_mdbx_env_begin_transaction(lua_State *L) {
	MDBX_env *env = lua_mdbx_env_fetch(L, 1);
	MDBX_txn *txn, **ptxn;
	int flags = MDBX_TXN_READWRITE, rc;

	if (lua_gettop(L) >= 2) {
		flags = lua_tonumber(L, 2);
	}

	rc = mdbx_txn_begin(env, NULL, flags, &txn);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushfstring(L, "fail to begin transaction: %s", mdbx_strerror(rc));
		return (1);
	}

	ptxn = lua_newuserdata(L, sizeof(txn));
	*ptxn = txn;
	luaL_getmetatable(L, MDBX_TXN_META);
	lua_setmetatable(L, -2);

	return (1);
}

static void
lua_mdbx_env_mt(lua_State *L) {
	luaL_newmetatable(L, MDBX_ENV_META);

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, lua_mdbx_env_get_path);
	lua_setfield(L, -2, "get_path");

	lua_pushcfunction(L, lua_mdbx_env_get_fd);
	lua_setfield(L, -2, "get_fd");

	lua_pushcfunction(L, lua_mdbx_env_get_maxdbs);
	lua_setfield(L, -2, "get_maxdbs");

	lua_pushcfunction(L, lua_mdbx_env_get_maxreaders);
	lua_setfield(L, -2, "get_maxreaders");

	lua_pushcfunction(L, lua_mdbx_env_set_option);
	lua_setfield(L, -2, "set_option");

	lua_pushcfunction(L, lua_mdbx_env_begin_transaction);
	lua_setfield(L, -2, "begin_transaction");

	lua_pushcfunction(L, lua_mdbx_env_close);
	lua_setfield(L, -2, "__gc");
}

static MDBX_txn *
lua_mdbx_txn_fetch(lua_State *L, int index) {
	return *((MDBX_txn **)luaL_checkudata(L, index, MDBX_TXN_META));
}

static int
lua_mdbx_txn_break(lua_State *L) {
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 1);
	int rc;

	rc = mdbx_txn_break(txn);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushfstring(L, "fail to break txn: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_txn_commit(lua_State *L) {
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 1);
	int rc;

	rc = mdbx_txn_commit(txn);
	if (rc != MDBX_SUCCESS) {
		txn = NULL;
		lua_pushnil(L);
		lua_pushfstring(L, "fail to commit txn: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_txn_renew(lua_State *L) {
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 1);
	int rc;

	rc = mdbx_txn_renew(txn);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushfstring(L, "fail to renew txn: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_txn_reset(lua_State *L) {
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 1);
	int rc;

	rc = mdbx_txn_reset(txn);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushfstring(L, "fail to reset txn: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_txn_abort(lua_State *L) {
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 1);
	int rc;

	rc = mdbx_txn_abort(txn);
	if (rc != MDBX_SUCCESS) {
		txn = NULL;
		lua_pushnil(L);
		lua_pushfstring(L, "fail to abort txn: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_txn_open_dbi(lua_State *L) {
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 1);
	MDBX_dbi dbi, *pdbi;
	int flags = MDBX_DB_DEFAULTS, rc;

	if (lua_gettop(L) >= 2) {
		flags = lua_tonumber(L, 2);
	}

	rc = mdbx_dbi_open(txn, NULL, flags, &dbi);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushfstring(L, "fail to open dbi: %s", mdbx_strerror(rc));
		return (1);
	}

	pdbi = lua_newuserdata(L, sizeof(dbi));
	*pdbi = dbi;
	luaL_getmetatable(L, MDBX_DBI_META);
	lua_setmetatable(L, -2);

	return (1);
}

static void
lua_mdbx_txn_mt(lua_State *L) {
	luaL_newmetatable(L, MDBX_TXN_META);

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, lua_mdbx_txn_break);
	lua_setfield(L, -2, "break");

	lua_pushcfunction(L, lua_mdbx_txn_commit);
	lua_setfield(L, -2, "commit");

	lua_pushcfunction(L, lua_mdbx_txn_renew);
	lua_setfield(L, -2, "renew");

	lua_pushcfunction(L, lua_mdbx_txn_reset);
	lua_setfield(L, -2, "reset");

	lua_pushcfunction(L, lua_mdbx_txn_abort);
	lua_setfield(L, -2, "abort");

	lua_pushcfunction(L, lua_mdbx_txn_open_dbi);
	lua_setfield(L, -2, "open_dbi");
}

static MDBX_dbi
lua_mdbx_dbi_fetch(lua_State *L, int index) {
	return *((MDBX_dbi *)luaL_checkudata(L, index, MDBX_DBI_META));
}

static int
lua_mdbx_dbi_close(lua_State *L) {
	MDBX_dbi dbi = lua_mdbx_dbi_fetch(L, 1);
	MDBX_env *env = lua_mdbx_env_fetch(L, 2);
	int rc;

	rc = mdbx_dbi_close(env, dbi);
	if (rc != MDBX_SUCCESS) {
		lua_pushnil(L);
		lua_pushfstring(L, "fail to close dbi: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_dbi_delete(lua_State *L) {
	MDBX_dbi dbi = lua_mdbx_dbi_fetch(L, 1);
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 2);
	MDBX_val k;
	int rc;

	k.iov_base = (char *)luaL_checklstring(L, 3, &k.iov_len);
	if (k.iov_len == 0) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "empty key");
		return (1);
	}

	rc = mdbx_del(txn, dbi, &k, NULL);
	if (rc != MDBX_SUCCESS) {
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "fail to delete key(%s): %s", k.iov_base, mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_dbi_put(lua_State *L) {
	MDBX_dbi dbi = lua_mdbx_dbi_fetch(L, 1);
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 2);
	MDBX_val k, v;
	int rc, type, siz;
	size_t value_size, output_size;
	double num;
	unsigned char c, *value;
	char *output, dummy[1];

	k.iov_base = (char *)luaL_checklstring(L, 3, &k.iov_len);
	if (k.iov_len == 0) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "empty key");
		return (1);
	}

	if (k.iov_len > 255) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "key too long");
		return (1);
	}

	type = lua_type(L, 4);
	switch(type) {
	case LUA_TSTRING:
		value = (unsigned char *)luaL_checklstring(L, 4, &value_size);
		output_size = value_size + 1;
		output = malloc(output_size);
		memset(output, 0, output_size);
		snprintf(output, output_size, "s%s", value);
		v.iov_base = output;
		v.iov_len = output_size;
		break;
	case LUA_TNUMBER:
		num = luaL_checknumber(L, 4);
		siz = snprintf(dummy, sizeof(dummy), "%f", num);
		output = malloc(siz + 1);
		memset(output, 0, siz + 1);
		snprintf(output, siz + 1, "n%f", num);
		v.iov_base = output;
		v.iov_len = siz + 1;
		break;
	case LUA_TBOOLEAN:
		c = lua_toboolean(L, 4) ? 1 : 0;
		output = malloc(2);
		memset(output, 0, 2);
		snprintf(output, 2, "b%d", c);
		v.iov_base = output;
		v.iov_len = 2;
		break;
	default:
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "bad value type");
		return (1);
	}

	rc = mdbx_put(txn, dbi, &k, &v, 0);

	if (output) {
		free(output);
	}

	if (rc != MDBX_SUCCESS) {
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "fail to set data: %s", mdbx_strerror(rc));
		return (1);
	}

	lua_pushboolean(L, 1);

	return (1);
}

static int
lua_mdbx_dbi_get(lua_State *L) {
	MDBX_dbi dbi = lua_mdbx_dbi_fetch(L, 1);
	MDBX_txn *txn = lua_mdbx_txn_fetch(L, 2);
	MDBX_val k, v;
	int rc;
	double num;
	unsigned char c;

	k.iov_base = (char *)luaL_checklstring(L, 3, &k.iov_len);
	if (k.iov_len == 0) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "empty key");
		return (1);
	}

	rc = mdbx_get(txn, dbi, &k, &v);
	if (rc != MDBX_SUCCESS) {
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "error to get key(%s): %s", k.iov_base, mdbx_strerror(rc));
		return (1);
	}

	switch (((char *)v.iov_base)[0]) {
	case 's':
		lua_pushlstring(L, ((char *)v.iov_base) + 1, v.iov_len - 1);
		break;
	case 'n':
		num = atof((char *)v.iov_base + 1);
		lua_pushnumber(L, num);
		break;
	case 'b':
		c = ((unsigned char *)v.iov_base)[1];
		lua_pushboolean(L, c == 49 ? 1 : 0);
		break;
	default:
		lua_pushnil(L);
	}

	return (1);
}

static void
lua_mdbx_dbi_mt(lua_State *L) {
	luaL_newmetatable(L, MDBX_DBI_META);

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, lua_mdbx_dbi_close);
	lua_setfield(L, -2, "close");

	lua_pushcfunction(L, lua_mdbx_dbi_delete);
	lua_setfield(L, -2, "delete");

	lua_pushcfunction(L, lua_mdbx_dbi_put);
	lua_setfield(L, -2, "put");

	lua_pushcfunction(L, lua_mdbx_dbi_get);
	lua_setfield(L, -2, "get");
}

int
luaopen_mdbx(lua_State *L) {
	lua_mdbx_env_mt(L);
	lua_mdbx_txn_mt(L);
	lua_mdbx_dbi_mt(L);

	lua_createtable(L, 0, 2);
	lua_pushliteral(L, "v");
	lua_setfield(L, -2, "__mode");
	lua_pushvalue(L, -1);
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, "mdbx.refs");

	lua_newtable(L);
	lua_pushcfunction(L, lua_mdbx_env_init);
	lua_setfield(L, -2, "env");

	lua_pushinteger(L, MDBX_MAX_DBI);
	lua_setfield(L, -2, "MDBX_MAX_DBI");

	lua_pushinteger(L, MDBX_MAXDATASIZE);
	lua_setfield(L, -2, "MDBX_MAXDATASIZE");

	lua_pushinteger(L, MDBX_MIN_PAGESIZE);
	lua_setfield(L, -2, "MDBX_MIN_PAGESIZE");

	lua_pushinteger(L, MDBX_MAX_PAGESIZE);
	lua_setfield(L, -2, "MDBX_MAX_PAGESIZE");

	lua_pushinteger(L, MDBX_LOG_FATAL);
	lua_setfield(L, -2, "MDBX_LOG_FATAL");

	lua_pushinteger(L, MDBX_LOG_ERROR);
	lua_setfield(L, -2, "MDBX_LOG_ERROR");

	lua_pushinteger(L, MDBX_LOG_WARN);
	lua_setfield(L, -2, "MDBX_LOG_WARN");

	lua_pushinteger(L, MDBX_LOG_NOTICE);
	lua_setfield(L, -2, "MDBX_LOG_NOTICE");

	lua_pushinteger(L, MDBX_LOG_VERBOSE);
	lua_setfield(L, -2, "MDBX_LOG_VERBOSE");

	lua_pushinteger(L, MDBX_LOG_DEBUG);
	lua_setfield(L, -2, "MDBX_LOG_DEBUG");

	lua_pushinteger(L, MDBX_LOG_TRACE);
	lua_setfield(L, -2, "MDBX_LOG_TRACE");

	lua_pushinteger(L, MDBX_LOG_EXTRA);
	lua_setfield(L, -2, "MDBX_LOG_EXTRA");

	lua_pushinteger(L, MDBX_LOG_DONTCHANGE);
	lua_setfield(L, -2, "MDBX_LOG_DONTCHANGE");

	lua_pushinteger(L, MDBX_DBG_ASSERT);
	lua_setfield(L, -2, "MDBX_DBG_ASSERT");

	lua_pushinteger(L, MDBX_DBG_AUDIT);
	lua_setfield(L, -2, "MDBX_DBG_AUDIT");

	lua_pushinteger(L, MDBX_DBG_JITTER);
	lua_setfield(L, -2, "MDBX_DBG_JITTER");

	lua_pushinteger(L, MDBX_DBG_DUMP);
	lua_setfield(L, -2, "MDBX_DBG_DUMP");

	lua_pushinteger(L, MDBX_DBG_LEGACY_MULTIOPEN);
	lua_setfield(L, -2, "MDBX_DBG_LEGACY_MULTIOPEN");

	lua_pushinteger(L, MDBX_DBG_LEGACY_OVERLAP);
	lua_setfield(L, -2, "MDBX_DBG_LEGACY_OVERLAP");

	lua_pushinteger(L, MDBX_DBG_DONTCHANGE);
	lua_setfield(L, -2, "MDBX_DBG_DONTCHANGE");

	lua_pushinteger(L, MDBX_ENV_DEFAULTS);
	lua_setfield(L, -2, "MDBX_ENV_DEFAULTS");

	lua_pushinteger(L, MDBX_NOSUBDIR);
	lua_setfield(L, -2, "MDBX_NOSUBDIR");

	lua_pushinteger(L, MDBX_RDONLY);
	lua_setfield(L, -2, "MDBX_RDONLY");

	lua_pushinteger(L, MDBX_EXCLUSIVE);
	lua_setfield(L, -2, "MDBX_EXCLUSIVE");

	lua_pushinteger(L, MDBX_ACCEDE);
	lua_setfield(L, -2, "MDBX_ACCEDE");

	lua_pushinteger(L, MDBX_NOTLS);
	lua_setfield(L, -2, "MDBX_NOTLS");

	lua_pushinteger(L, MDBX_NORDAHEAD);
	lua_setfield(L, -2, "MDBX_NORDAHEAD");

	lua_pushinteger(L, MDBX_NOMEMINIT);
	lua_setfield(L, -2, "MDBX_NOMEMINIT");

	lua_pushinteger(L, MDBX_COALESCE);
	lua_setfield(L, -2, "MDBX_COALESCE");

	lua_pushinteger(L, MDBX_LIFORECLAIM);
	lua_setfield(L, -2, "MDBX_LIFORECLAIM");

	lua_pushinteger(L, MDBX_PAGEPERTURB);
	lua_setfield(L, -2, "MDBX_PAGEPERTURB");

	lua_pushinteger(L, MDBX_SYNC_DURABLE);
	lua_setfield(L, -2, "MDBX_SYNC_DURABLE");

	lua_pushinteger(L, MDBX_NOMETASYNC);
	lua_setfield(L, -2, "MDBX_NOMETASYNC");

	lua_pushinteger(L, MDBX_SAFE_NOSYNC);
	lua_setfield(L, -2, "MDBX_SAFE_NOSYNC");

	lua_pushinteger(L, MDBX_MAPASYNC);
	lua_setfield(L, -2, "MDBX_MAPASYNC");

	lua_pushinteger(L, MDBX_UTTERLY_NOSYNC);
	lua_setfield(L, -2, "MDBX_UTTERLY_NOSYNC");

	lua_pushinteger(L, MDBX_TXN_READWRITE);
	lua_setfield(L, -2, "MDBX_TXN_READWRITE");

	lua_pushinteger(L, MDBX_TXN_RDONLY);
	lua_setfield(L, -2, "MDBX_TXN_RDONLY");

	lua_pushinteger(L, MDBX_TXN_RDONLY_PREPARE);
	lua_setfield(L, -2, "MDBX_TXN_RDONLY_PREPARE");

	lua_pushinteger(L, MDBX_TXN_TRY);
	lua_setfield(L, -2, "MDBX_TXN_TRY");

	lua_pushinteger(L, MDBX_TXN_NOMETASYNC);
	lua_setfield(L, -2, "MDBX_TXN_NOMETASYNC");

	lua_pushinteger(L, MDBX_DB_DEFAULTS);
	lua_setfield(L, -2, "MDBX_DB_DEFAULTS");

	lua_pushinteger(L, MDBX_REVERSEKEY);
	lua_setfield(L, -2, "MDBX_REVERSEKEY");

	lua_pushinteger(L, MDBX_DUPSORT);
	lua_setfield(L, -2, "MDBX_DUPSORT");

	lua_pushinteger(L, MDBX_INTEGERKEY);
	lua_setfield(L, -2, "MDBX_INTEGERKEY");

	lua_pushinteger(L, MDBX_DUPFIXED);
	lua_setfield(L, -2, "MDBX_DUPFIXED");

	lua_pushinteger(L, MDBX_INTEGERDUP);
	lua_setfield(L, -2, "MDBX_INTEGERDUP");

	lua_pushinteger(L, MDBX_REVERSEDUP);
	lua_setfield(L, -2, "MDBX_REVERSEDUP");

	lua_pushinteger(L, MDBX_CREATE);
	lua_setfield(L, -2, "MDBX_CREATE");

	lua_pushinteger(L, MDBX_DB_ACCEDE);
	lua_setfield(L, -2, "MDBX_DB_ACCEDE");

	lua_pushinteger(L, MDBX_UPSERT);
	lua_setfield(L, -2, "MDBX_UPSERT");

	lua_pushinteger(L, MDBX_NOOVERWRITE);
	lua_setfield(L, -2, "MDBX_NOOVERWRITE");

	lua_pushinteger(L, MDBX_NODUPDATA);
	lua_setfield(L, -2, "MDBX_NODUPDATA");

	lua_pushinteger(L, MDBX_CURRENT);
	lua_setfield(L, -2, "MDBX_CURRENT");

	lua_pushinteger(L, MDBX_ALLDUPS);
	lua_setfield(L, -2, "MDBX_ALLDUPS");

	lua_pushinteger(L, MDBX_RESERVE);
	lua_setfield(L, -2, "MDBX_RESERVE");

	lua_pushinteger(L, MDBX_APPEND);
	lua_setfield(L, -2, "MDBX_APPEND");

	lua_pushinteger(L, MDBX_APPENDDUP);
	lua_setfield(L, -2, "MDBX_APPENDDUP");

	lua_pushinteger(L, MDBX_MULTIPLE);
	lua_setfield(L, -2, "MDBX_MULTIPLE");

	lua_pushinteger(L, MDBX_opt_max_db);
	lua_setfield(L, -2, "MDBX_opt_max_db");

	lua_pushinteger(L, MDBX_opt_max_readers);
	lua_setfield(L, -2, "MDBX_opt_max_readers");

	lua_pushinteger(L, MDBX_opt_sync_bytes);
	lua_setfield(L, -2, "MDBX_opt_sync_bytes");

	lua_pushinteger(L, MDBX_opt_sync_period);
	lua_setfield(L, -2, "MDBX_opt_sync_period");

	lua_pushinteger(L, MDBX_opt_rp_augment_limit);
	lua_setfield(L, -2, "MDBX_opt_rp_augment_limit");

	lua_pushinteger(L, MDBX_opt_loose_limit);
	lua_setfield(L, -2, "MDBX_opt_loose_limit");

	lua_pushinteger(L, MDBX_opt_dp_reserve_limit);
	lua_setfield(L, -2, "MDBX_opt_dp_reserve_limit");

	lua_pushinteger(L, MDBX_opt_txn_dp_limit);
	lua_setfield(L, -2, "MDBX_opt_txn_dp_limit");

	lua_pushinteger(L, MDBX_opt_txn_dp_initial);
	lua_setfield(L, -2, "MDBX_opt_txn_dp_initial");

	lua_pushinteger(L, MDBX_opt_spill_max_denominator);
	lua_setfield(L, -2, "MDBX_opt_spill_max_denominator");

	lua_pushinteger(L, MDBX_opt_spill_min_denominator);
	lua_setfield(L, -2, "MDBX_opt_spill_min_denominator");

	lua_pushinteger(L, MDBX_opt_spill_parent4child_denominator);
	lua_setfield(L, -2, "MDBX_opt_spill_parent4child_denominator");

	return (1);
}
