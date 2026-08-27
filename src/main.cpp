namespace fs = std::filesystem;
#ifdef _WIN32
constexpr char PATH_SEPARATOR = ';';
#else
constexpr char PATH_SEPARATOR = ':';
#endif
bool findLarFile(const std::string& file, fs::path& out)
{
    fs::path p(file);

    if (fs::exists(p))
    {
        out = fs::absolute(p).string();
        return true;
    }
    size_t bufSize;
    char* pathEnv = nullptr;
    _dupenv_s(&pathEnv, &bufSize, "LAR_PATH");
    if (!pathEnv)
        return false;

    std::stringstream ss(pathEnv);
    std::string dir;
    free(pathEnv);
    while (std::getline(ss, dir, PATH_SEPARATOR))
    {
        fs::path candidate = fs::path(dir) / file;

        if (fs::exists(candidate))
        {
            out = fs::absolute(candidate).string();
            return true;
        }
    }

    return false;
}

static bool loadDeps(LuaPP::State& state, MultiLar* multiLar, std::string& errmsg)
{
    LarManifest* manifest = multiLar->ptrmain->getManifest();
    auto deps_size = manifest->deps.size();
    state.resize_table(multiLar->dependencies, static_cast<unsigned>(deps_size), 0);
    for (size_t i = 0; i < deps_size; i++)
    {
        auto next = i + 1;
        const std::string& dep = manifest->deps[i];
        fs::path depFile;

        if (!findLarFile(dep, depFile))
        {
            errmsg = "dependency '" + dep + "' not found";
            return false;
        }
        LarManager* ptr = nullptr;
        auto depud = state.mkuserdata(ptr);
        if (!ptr) { errmsg = "couldn't reserve dep userdata"; return false; }
        new (ptr) LarManager(state, depFile, false);
        state.setMetatable(depud, multiLar->mtlar);
        state.setField(multiLar->dependencies, next, depud);
		multiLar->lastConstructed = next;
    }

    return true;
}
static bool loadMain(LuaPP::State& state, MultiLar* multilar, packagezSearcher* searcher, std::string& errmsg) {
	LarManager* mainLar = multilar->ptrmain;
	LarManifest* manifest = mainLar->getManifest();
    std::string fileName;
    if (!searcher->findFile(state, fileName, manifest->main))
    {
		errmsg = fileName; // fileName contains the error message
        return false;
    }
    auto chunk = state.mknilv();
    if (!luaL_loadentry(state, chunk, mainLar, fileName))
    {
        errmsg = "Failed to load main file: " + chunk.getstring();
        return false;
	}
	LuaPP::VLTValue args;
    auto gargs = state.getGlobal("arg");
    for (auto& pair : state.ipairs(gargs)) {
        args.push_back(state.copy(pair.value()));
    }
    bool status = true;
    state.call(chunk, args, [&errmsg, &status](LuaPP::State& state, std::string msg) -> void {
        errmsg = "Failed to execute main file: " + state.getTraceback(msg);
        status = false;
    });
    return status;
}

static void createargtable(const std::vector<std::string>& args, LuaPP::State& state) {
    auto argtable = state.newtable();
    for (size_t i = 2; i < args.size(); ++i) {
        state.pushArrayBack(argtable, state.mkvalue(args[i]), true);
    }
    state.setGlobal("arg", argtable);
}
extern "C" int main(int argc, char** argv)
{
    std::vector<std::string> args(argv, argv + argc);

    if (argc < 2)
    {
        std::cout
            << "No lar provided\n\n"
            << "Usage:\n"
            << "  " << args[0] << " <lar> [args...]\n"
            << "      Run a .lar package.\n\n"
            << "  " << args[0] << " --build <directory> -o <output.lar> [--debug-info]\n"
            << "      Build a .lar package from a directory.\n";
        return 1;
    }

    LuaPP::State state;
    state.loadlibs();
    if (args[1] == "--build")
    {
        return mainc(args, state);
    }
    fs::path larFile;

    if (!findLarFile(args[1], larFile))
    {
        std::cerr << "error in loading " << args[1] << ": file not found\n";
        return 1;
    }
    createargtable(args, state);
    MultiLar* multiLar = nullptr;
    auto mlarobj = state.mkobj(multiLar, state, larFile);
    MultiLar::createMultiLarModule(state, mlarobj);
    std::string errmsg;
    if (!loadDeps(state, multiLar, errmsg))
    {
        std::cerr << errmsg;
        return 1;
    }
    packagezSearcher searcher(state, multiLar);
    LarAssets larAssets(state, multiLar);
    if (!loadMain(state, multiLar, &searcher, errmsg))
    {
        std::cerr << errmsg;
    }
    return 0;
}