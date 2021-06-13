#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <mdbx.h>

#define LUA_MDBX_TYPENAME "MDBX_env"

static int
lua_mdbx_open(lua_State *L) {
    MDBX_env **env;
    const char *path;
    unsigned int flags = MDBX_NOSUBDIR;
    int rc, nargs;
    size_t path_len;

    nargs = lua_gettop(L);
    if (nargs != 1) {
        return luaL_error(L, "expecting 1 arguments, "
            "but only seed %d\n", nargs);
    }

    if (lua_isnil(L, 1)) {
        lua_pushnil(L);;
        lua_pushliteral(L, "nil path");
        return 2;
    }

    path = luaL_checklstring(L, 1, &path_len);
    if (path_len == 0) {
        lua_pushnil(L);;
        lua_pushliteral(L, "empty path");
        return 2;
    }

    env = lua_newuserdata(L, sizeof(MDBX_env *));
    rc = mdbx_env_create(env);
    if (rc != MDBX_SUCCESS) {
        lua_pushnil(L);;
        lua_pushliteral(L, "fail to create env");
        return 2;
    }

    rc = mdbx_env_open(*env, path, flags, 0664);
    if (rc != MDBX_SUCCESS) {
        mdbx_env_close(*env);
        lua_pushnil(L);;
        lua_pushfstring(L, "fail to open env: %s", mdbx_strerror(rc));
        return 2;
    }

    luaL_getmetatable(L, LUA_MDBX_TYPENAME);
    lua_setmetatable(L, -2);

    return 1;
}

static int
lua_mdbx_mget(lua_State *L) {
    int nsubkeys, nargs, i, rc;
    MDBX_env **env;
    MDBX_txn *txn;
    MDBX_dbi dbi;

    nargs = lua_gettop(L);
    if (nargs != 2) {
        return luaL_error(L, "expecting 2 arguments, "
            "but only seen %d", nargs);
    }

    env = luaL_checkudata(L, 1, LUA_MDBX_TYPENAME);
    if (!lua_istable(L, 2)) {
        return luaL_error(L, "expecting table on second argument");
    }

    nsubkeys = lua_objlen(L, 2);
    if (nsubkeys == 0) {
        return luaL_error(L, "at least one key on second argument");
    }

    MDBX_val keys[nsubkeys];
    for (i = 0; i < nsubkeys; i++) {
        lua_rawgeti(L, 2, i + 1);
        keys[i].iov_base = (unsigned char *)luaL_checklstring(L, -1, &keys[i].iov_len);
    }

    rc = mdbx_txn_begin(*env, NULL, 0, &txn);
    if (rc != MDBX_SUCCESS) {
        lua_pushnil(L);
        lua_pushfstring(L, "fail to begin txn: %s", mdbx_strerror(rc));
        return 2;
    }

    rc = mdbx_dbi_open(txn, NULL, 0, &dbi);
    if (rc != MDBX_SUCCESS) {
        lua_pushnil(L);
        lua_pushfstring(L, "fail to open dbi: %s", mdbx_strerror(rc));
        return 2;
    }

    lua_createtable(L, 0, nsubkeys);
    for (i = 0; i < nsubkeys; i++) {
        MDBX_val v;
        double num;
        unsigned int c;

        if (keys[i].iov_len == 0 || keys[i].iov_len > 255) {
            continue;
        }

        rc = mdbx_get(txn, dbi, &keys[i], &v);
        if (rc != MDBX_SUCCESS) {
            continue;
        }

        switch (((char *)v.iov_base)[0]) {
        case 's':
            lua_pushlstring(L, ((char *)v.iov_base) + 1, v.iov_len - 2);
            break;

        case 'n':
            num = atof((char *)v.iov_base + 1);
            lua_pushnumber(L, num);
            break;

        case 'b':
            c = ((unsigned char*)v.iov_base)[1];
            lua_pushboolean(L, c == 49 ? 1 : 0);
            break;

        default:
            lua_pushnil(L);
        }

        lua_setfield(L, -2, keys[i].iov_base);
    }

    mdbx_dbi_close(*env, dbi);
    mdbx_txn_abort(txn);
    txn = NULL;

    return 1;
}

static int
lua_mdbx_get(lua_State *L) {
    int nresult;

    lua_createtable(L, 1, 0);
    lua_insert(L, 2);
    lua_rawseti(L, 2, 1);
    nresult = lua_mdbx_mget(L);

    if (nresult != 1) {
        return nresult;
    }

    if (!lua_istable(L, -1)) {
        lua_pushnil(L);
        lua_pushliteral(L, "invalid result from mget");
        return 2;
    }

    lua_pushnil(L);
    if (lua_next(L, -2) == 0) {
        lua_pushnil(L);
    }

    return 1;
}

static int
lua_mdbx_set(lua_State *L) {
    MDBX_env **env;
    MDBX_txn *txn;
    MDBX_dbi dbi;
    MDBX_val k, v;
    int rc, nargs, type, siz;
    double num;
    unsigned char c, *value;
    char *output, dummy[1];
    size_t value_size, output_size;

    nargs = lua_gettop(L);
    if (nargs != 3) {
        return luaL_error(L, "expecting 3 arguments, "
            "but only seen %d", nargs);
    }

    env = luaL_checkudata(L, 1, LUA_MDBX_TYPENAME);
    if (lua_isnil(L, 2)) {
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "nil key");
        return 2;
    }

    k.iov_base = (unsigned char *)luaL_checklstring(L, 2, &k.iov_len);
    if (k.iov_len == 0) {
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "empty key");
        return 2;
    }

    if (k.iov_len > 255) {
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "key too long");
        return 2;
    }

    type = lua_type(L, 3);
    switch (type) {
    case LUA_TSTRING:
        value = (unsigned char *)luaL_checklstring(L, 3, &value_size);
        output_size = value_size + 2;
        output = malloc(output_size);
        memset(output, 0, output_size);
        snprintf(output, output_size, "s%s", value);
        v.iov_len = output_size;
        v.iov_base = output;
        break;

    case LUA_TNUMBER:
        num = luaL_checknumber(L, 3);
        siz = snprintf(dummy, sizeof(dummy), "%f", num);
        output_size = siz + 1;
        output = malloc(output_size);
        memset(output, 0, output_size);
        snprintf(output, output_size, "n%f", num);
        v.iov_len = output_size;
        v.iov_base = output;
        break;

    case LUA_TBOOLEAN:
        c = lua_toboolean(L, 3) ? 1 : 0;
        output = malloc(3);
        memset(output, 0, 3);
        snprintf(output, 3, "b%d", c);
        v.iov_len = 3;
        v.iov_base = output;
        break;
    default:
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "vad value type");
        return 2;
    }

    rc = mdbx_txn_begin(*env, NULL, 0, &txn);
    if (rc != MDBX_SUCCESS) {
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "fail to begin txn");

        if (output) {
            free(output);
        }

        return 2;
    }

    rc = mdbx_dbi_open(txn, NULL, 0, &dbi);
    if (rc != MDBX_SUCCESS) {
        mdbx_txn_abort(txn);
        txn = NULL;

        lua_pushboolean(L, 0);
        lua_pushliteral(L, "fail to open  dbi");

        if (output) {
            free(output);
        }

        return 2;
    }

    rc = mdbx_put(txn, dbi, &k, &v, 0);
    if (output) {
        free(output);
    }

    if (rc != MDBX_SUCCESS) {
        mdbx_dbi_close(*env, dbi);
        mdbx_txn_abort(txn);
        txn = NULL;

        lua_pushboolean(L, 0);
        lua_pushliteral(L, "fail to set data");

        return 2;
    }

    mdbx_dbi_close(*env, dbi);
    rc = mdbx_txn_commit(txn);
    txn = NULL;
    if (rc != MDBX_SUCCESS) {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "fail to commit txn: %s", mdbx_strerror(rc));

        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int
lua_mdbx_close(lua_State *L) {
    MDBX_env **env;

    env = luaL_checkudata(L, 1, LUA_MDBX_TYPENAME);
    if (env != NULL) {
        mdbx_env_close(*env);
        *env = NULL;
        lua_pushboolean(L, 1);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

int
luaopen_mdbx(lua_State *L) {
    static const struct luaL_Reg methods[] = {
        { "open", lua_mdbx_open },
        { NULL, NULL },
    };

    static const struct luaL_Reg mdbx_methods[] = {
        { "get", lua_mdbx_get },
        { "mget", lua_mdbx_mget },
        { "set", lua_mdbx_set },
        { NULL, NULL }
    };

    luaL_register(L, "mdbx", methods);
    luaL_register(L, NULL, mdbx_methods);

    if (luaL_newmetatable(L, LUA_MDBX_TYPENAME)) {
        luaL_register(L, NULL, mdbx_methods);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, lua_mdbx_close);
        lua_settable(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushvalue(L, -2);
        lua_settable(L, -3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "must not access this metatable");
        lua_settable(L, -3);
    }

    lua_pop(L, 1);

    return 1;
}
