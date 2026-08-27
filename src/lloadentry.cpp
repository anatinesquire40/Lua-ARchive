bool findFileInMultiLar(
    const std::string& filename,
    MultiLar* multiLar,
    zip_t** outArchive)
{
    if (!multiLar || !outArchive)
        return false;
    if (multiLar->ptrmain->exists(filename))
    {
        *outArchive = multiLar->ptrmain->getArchive();
        return true;
    }
    for (auto& pair : multiLar->state.ipairs(multiLar->dependencies))
    {
        auto dep = pair.value().checkud<LarManager>();
        if (dep->exists(filename))
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
static std::string zip_lua_reader(LuaPP::State& state, void* data)
{
    ZipLuaReader* ctx = static_cast<ZipLuaReader*>(data);
    std::string result;
    if (ctx->finished)
    {
        return result;
    }

    zip_int64_t r = zip_fread(ctx->file, ctx->buffer, sizeof(ctx->buffer));

    if (r < 0)
    {
        ctx->finished = true;
        return result;
    }

    if (r == 0)
    {
        ctx->finished = true;
        return result;
    }
    result = std::string(ctx->buffer, static_cast<size_t>(r));
    return result;
}

bool luaL_loadentry(LuaPP::State& state, LuaPP::LTValue& out, LarManager* lar, const std::string& name)
{
    if (!lar || name.empty())
    {
        out = state.mkvalue("invalid arguments");
        return false;
    }

    zip_t* archive = lar->getArchive();

    zip_file_t* file = zip_fopen(archive, name.c_str(), 0);
    if (!file)
    {
        out = state.mkvalue("failed to open entry: " + name);
        return false;
    }


    ZipLuaReader ctx;
    ctx.file = file;
    ctx.finished = false;

    bool status = state.load(
        out,
        LuaPP::Reader(zip_lua_reader),
        &ctx,
        "@" + name
    );

    zip_fclose(file);

    return status;
}

bool luaL_loadentry(LuaPP::State& state, LuaPP::LTValue& out, MultiLar* mlar, const std::string& name)
{
    if (!mlar || name.empty())
    {
		out = state.mkvalue("invalid arguments");
        return false;
    }

    zip_t* archive = nullptr;

    if (!findFileInMultiLar(name, mlar, &archive))
    {
        out = state.mkvalue("entry not found: " + name);
        return false;
    }

    zip_file_t* file = zip_fopen(archive, name.c_str(), 0);
    if (!file)
    {
        out = state.mkvalue("failed to open entry: " + name);
        return false;
    }

    ZipLuaReader ctx;
    ctx.file = file;
    ctx.finished = false;

    bool status = state.load(
        out,
        LuaPP::Reader(zip_lua_reader),
        &ctx,
        "@" + name
    );

    zip_fclose(file);

    return status;
}



struct DumpBuffer
{
    char* data = nullptr;
    size_t size = 0;
};

static bool writer(LuaPP::State& state, void* ud, const void* b, size_t n)
{
    auto* buf = static_cast<DumpBuffer*>(ud);

    char* newData = static_cast<char*>(
        realloc(buf->data, buf->size + n)
        );

    if (!newData)
        return false;

    memcpy(newData + buf->size, b, n);

    buf->data = newData;
    buf->size += n;

    return true;
}

bool lua_entrydump(LuaPP::State& state, const LuaPP::LTValue& func, zip_t* archive, const char* entry, bool strip)
{
    func.checkfunction();

    DumpBuffer buffer;

    if (!state.dumpf(func, LuaPP::Writer(writer), &buffer, strip))
    {
        free(buffer.data);
        return false;
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
        return false;
    }
    if (zip_file_add(archive, entry, src, ZIP_FL_OVERWRITE) < 0)
    {
        zip_source_free(src);
        return false;
    }
    return true;
}