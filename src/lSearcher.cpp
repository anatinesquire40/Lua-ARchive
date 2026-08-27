constexpr char* LUA_SEARCHERS = "searchers";
constexpr size_t LUA_SEARCHERS_IDX = 2;
packagezSearcher::packagezSearcher(LuaPP::State& state, MultiLar* mlar)
    : multiLar(mlar)
{
    LarManifest* mainManifest = multiLar->ptrmain->getManifest();

    auto package = state.getGlobal("package");
    package.checktable();

    state.setField(package, "zpath", state.mkvalue("?.luac;?/init.luac"));

    auto searchers = state.getField(package, LUA_SEARCHERS);
    auto search_func = state.mkfunction(l_search, state.mkvec(state.mkvalue(this)));
    if (mainManifest->disableExternalLua)
    {
        state.setField(searchers, LUA_SEARCHERS_IDX, search_func, true);
	}
	else {
		state.pushArrayBack(searchers, search_func, true);
	}
}
std::string packagezSearcher::getPackagezPath(LuaPP::State& state) {
    auto package = state.getGlobal("package");
	auto zpath = state.getField(package, "zpath");
	return zpath.checkstring();
}

bool packagezSearcher::findFile(LuaPP::State& state, std::string& out, const std::string& name)
{
    std::string zpath = getPackagezPath(state);
    if (zpath.empty()) {
        out = "package.zpath is not set";
        return false;
    }
    return searchPath(state, out, name, zpath, ".", "/");
}
LuaPP::VLTValue packagezSearcher::search(LuaPP::State& state, const std::string& moduleName)
{
    auto res = state.mkvec();
    std::string foundPath;
    if (!findFile(state, foundPath, moduleName)) { res.push_back(state.mkvalue(foundPath)); return res; }
	auto chunk = state.mknilv();
    if (!luaL_loadentry(
        state,
        chunk,
        multiLar,
        foundPath))
    {
        state.error("Failed to load module '" + moduleName +"': " + chunk.getstring());
        return res;
    }
    res.push_back(std::move(chunk));
    res.push_back(state.mkvalue(foundPath));
    return res;
}
luapp_function(packagezSearcher::l_search)
{
	return upvals[0].getud<packagezSearcher>()->
        search(state, state.checkindex(args, 0).checkstring());
}
static std::string buildNotFoundError(const std::string& path) {
    std::stringstream ss;

    std::size_t start = 0;
    std::size_t pos;

    while ((pos = path.find("?", start)) != std::string::npos) {
        ss << "no file '" << path.substr(start, pos - start) << "'\n\t";
        start = pos + 1;
    }

    ss << "no file '" << path.substr(start) << "'";
    return ss.str();
}

static bool readable(const std::string& filename, MultiLar* multiLar) {
    if (multiLar->ptrmain->exists(filename)) {
        return true;
    }
    if (multiLar->lastConstructed == INVALID_INDEX) {
        return false;
    }
    for (auto& pair : multiLar->state.ipairs(multiLar->dependencies))
    {
        auto it = pair.value().checkud<LarManager>();
        if (it->exists(filename))
            return true;
    }

    return false;
}

bool packagezSearcher::searchPath(
    LuaPP::State& state,
    std::string& out,
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
    while ((pos = searchPaths.find("?", pos)) != std::string::npos) {
        searchPaths.replace(pos, 1, moduleName);
        pos += moduleName.size();
    }

    std::stringstream ss(searchPaths);
    std::string candidate;
    while (std::getline(ss, candidate, ';')) {
        if (readable(candidate, multiLar)) {
            out = std::move(candidate);
            return true;
        }
    }
    out = buildNotFoundError(searchPaths);
    return false;
}