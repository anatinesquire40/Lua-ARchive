bool findFileInMultiLar(
    const std::string& filename,
    MultiLar* multiLar,
    zip_t** outArchive)
{
    if (!multiLar || !outArchive)
        return false;

    // 1. buscar en main
    if (multiLar->main && multiLar->main->exists(filename))
    {
        *outArchive = multiLar->main->getArchive();
        return true;
    }

    // 2. buscar en dependencias
    for (const auto& dep : multiLar->dependencies)
    {
        if (dep && dep->exists(filename))
        {
            *outArchive = dep->getArchive();
            return true;
        }
    }

    return false;
}
struct ZipLuaReader {
    zip_file_t* file;
    bool finished;
    char buffer[BUFSIZ];
};
static const char* zip_lua_reader(lua_State* L, void* data, size_t* size)
{
    ZipLuaReader* ctx = static_cast<ZipLuaReader*>(data);

    if (ctx->finished)
    {
        *size = 0;
        return nullptr;
    }

    zip_int64_t r = zip_fread(ctx->file, ctx->buffer, sizeof(ctx->buffer));

    if (r < 0)
    {
        *size = 0;
        ctx->finished = true;
        return nullptr;
    }

    if (r == 0)
    {
        *size = 0;
        ctx->finished = true;
        return nullptr;
    }

    *size = static_cast<size_t>(r);
    return ctx->buffer;
}

LUALIB_API int luaL_loadentry(lua_State* L, LarManager* lar, const char* name)
{
    if (!lar || !name)
    {
        lua_pushnil(L);
        lua_pushliteral(L, "invalid arguments");
        return 2;
    }

    zip_t* archive = lar->getArchive();

    zip_file_t* file = zip_fopen(archive, name, 0);
    if (!file)
    {
        lua_pushnil(L);
        lua_pushfstring(L, "failed to open entry: %s", name);
        return 2;
    }


    ZipLuaReader ctx;
    ctx.file = file;
    ctx.finished = false;

    int status = lua_load(
        L,
        zip_lua_reader,
        &ctx,
        ("@" + std::string(name)).c_str(),
        nullptr
    );

    zip_fclose(file);

    return status;
}

LUALIB_API int luaL_loadentry(lua_State* L, MultiLar* mlar, const char* name)
{
    if (!mlar || !name)
        return luaL_error(L, "invalid arguments");

    zip_t* archive = nullptr;

    if (!findFileInMultiLar(name, mlar, &archive))
    {
        lua_pushnil(L);
        lua_pushfstring(L, "entry not found: %s", name);
        return 2;
    }

    zip_file_t* file = zip_fopen(archive, name, 0);
    if (!file)
    {
        lua_pushnil(L);
        lua_pushfstring(L, "failed to open entry: %s", name);
        return 2;
    }

    ZipLuaReader ctx;
    ctx.file = file;
    ctx.finished = false;

    int status = lua_load(
        L,
        zip_lua_reader,
        &ctx,
        ("@" + std::string(name)).c_str(),
        nullptr
    );

    zip_fclose(file);

    return status;
}



struct DumpBuffer
{
    char* data = nullptr;
    size_t size = 0;
};

static int writer(lua_State*, const void* b, size_t n, void* ud)
{
    auto* buf = static_cast<DumpBuffer*>(ud);

    char* newData = static_cast<char*>(
        realloc(buf->data, buf->size + n)
        );

    if (!newData)
        return 1;

    memcpy(newData + buf->size, b, n);

    buf->data = newData;
    buf->size += n;

    return 0;
}

LUALIB_API int lua_entrydump(lua_State* L, zip_t* archive, const char* entry, bool strip)
{
    luaL_checktype(L, -1, LUA_TFUNCTION);

    DumpBuffer buffer;

    if (lua_dump(L, writer, &buffer, strip) != LUA_OK)
    {
        free(buffer.data);
        return luaL_error(L, "lua_dump failed");
    }

    zip_source_t* src =
        zip_source_buffer(
            archive,
            buffer.data,
            buffer.size,
            1
        );
    if (!src)
    {
        free(buffer.data);
        return luaL_error(L, "zip_source_buffer failed");
    }
    if (zip_file_add(archive, entry, src, ZIP_FL_OVERWRITE) < 0)
    {
        zip_source_free(src);
        return luaL_error(L, "zip_file_add failed");
    }
    lua_pop(L, 1);
    return 0;
}