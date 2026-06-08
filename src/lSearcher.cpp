packagezSearcher::packagezSearcher(lua_State* L, MultiLar* mlar)
    : multiLar(mlar)
{
    LarManifest* mainManifest = multiLar->main->getManifest();

    lua_getglobal(L, "package");
    luaL_checktype(L, -1, LUA_TTABLE);

    lua_pushstring(L, "?.luac;?/init.luac");
    lua_setfield(L, -2, "zpath");

    lua_getfield(L, -1, "searchers");
    int oldSearchers = lua_gettop(L);

    lua_newtable(L);
    int newSearchers = lua_gettop(L);

    size_t len = lua_rawlen(L, oldSearchers);

    if (mainManifest->disableExternalLua)
    {
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, l_search, 1);
        lua_rawseti(L, newSearchers, 1);

        for (size_t i = 1; i <= len; i++)
        {
            lua_rawgeti(L, oldSearchers, i);
            lua_rawseti(L, newSearchers, i + 1);
        }
    }
    else
    {
        for (size_t i = 1; i <= len; i++)
        {
            lua_rawgeti(L, oldSearchers, i);
            lua_rawseti(L, newSearchers, i);
        }

        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, l_search, 1);
        lua_rawseti(L, newSearchers, len + 1);
    }

    lua_setfield(L, -3, "searchers");
    lua_pop(L, 2);
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_pushstring(L, "LarManifest");
    lua_rawgeti(L, LUA_REGISTRYINDEX, mainManifest->manifestref);
    lua_settable(L, -3);
}
std::string packagezSearcher::getPackagezPath(lua_State* L) {
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "zpath");
	std::string path = lua_tostring(L, -1);
	lua_pop(L, 2);
	return path;
}

bool packagezSearcher::findFile(lua_State* L, const std::string& name)
{
    std::string zpath = getPackagezPath(L);
    if (zpath.empty()) {
        lua_pushstring(L, "package.zpath is not set");
        return false;
    }
    if (!searchPath(L, name, zpath, ".", "/"))
    {
        return false;
    }
    return true;
}
bool packagezSearcher::search(lua_State* L, const std::string& moduleName)
{
    if (!findFile(L, moduleName)) return false;
	std::string foundPath = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (luaL_loadentry(
        L,
        multiLar,
        foundPath.c_str()) != LUA_OK)
    {
        luaL_error(L, "Failed to load module '%s': %s", moduleName.c_str(), lua_tostring(L, -1));
        return false;
    }
    lua_pushstring(L, foundPath.c_str());
    return true;
}
int packagezSearcher::l_search(lua_State* L)
{
	packagezSearcher* thz = reinterpret_cast<packagezSearcher*>(lua_touserdata(L, lua_upvalueindex(1)));
    std::string moduleName = luaL_checkstring(L, 1);
    if (!thz->search(L, moduleName))
    {
		return 1;
    }
	return 2;
}
static std::string buildNotFoundError(const std::string& path) {
    std::stringstream ss;

    std::size_t start = 0;
    std::size_t pos;

    while ((pos = path.find(LUA_PATH_SEP, start)) != std::string::npos) {
        ss << "no file '" << path.substr(start, pos - start) << "'\n\t";
        start = pos + 1;
    }

    ss << "no file '" << path.substr(start) << "'";
    return ss.str();
}

static bool readable(const std::string& filename, MultiLar* multiLar) {
    if (multiLar->main->exists(filename)) {
        return true;
    }

    for (const auto& dep : multiLar->dependencies) {
        if (dep->exists(filename)) {
            return true;
        }
    }

    return false;
}

bool packagezSearcher::searchPath(
    lua_State* L,
    const std::string& name,
    const std::string& path,
    const std::string& sep,
    const std::string& dirsep)
{
    std::string moduleName = name;

    if (!sep.empty()) {
        size_t pos = 0;
        while ((pos = moduleName.find(sep, pos)) != std::string::npos) {
            moduleName.replace(pos, sep.size(), dirsep);
            pos += dirsep.size();
        }
    }

    std::string searchPaths = path;

    size_t pos = 0;
    while ((pos = searchPaths.find(LUA_PATH_MARK, pos)) != std::string::npos) {
        searchPaths.replace(pos, 1, moduleName);
        pos += moduleName.size();
    }

    std::stringstream ss(searchPaths);
    std::string candidate;

    while (std::getline(ss, candidate, LUA_PATH_SEP[0])) {
        if (readable(candidate, multiLar)) {
            lua_pushstring(L, candidate.c_str());
            return true;
        }
    }

    lua_pushstring(L, buildNotFoundError(searchPaths).c_str());
    return false;
}