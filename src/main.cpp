namespace fs = std::filesystem;
#ifdef _WIN32
constexpr char PATH_SEPARATOR = ';';
#else
constexpr char PATH_SEPARATOR = ':';
#endif
bool findLarFile(const std::string& file, std::string& out)
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
static bool loadDeps(lua_State* L, MultiLar* multiLar)
{
    LarManifest* manifest = multiLar->main->getManifest();

    for (const std::string& dep : manifest->deps)
    {
        std::string depFile;

        if (!findLarFile(dep, depFile))
        {
            lua_pushfstring(
                L,
                "dependency '%s' not found",
                dep.c_str()
            );
            return false;
        }

        multiLar->dependencies.emplace_back(
            std::make_unique<LarManager>(L, depFile, false)
        );
    }

    return true;
}
static bool loadMain(lua_State* L, MultiLar* multilar, packagezSearcher* searcher) {
	LarManager* mainLar = multilar->main.get();
	LarManifest* manifest = mainLar->getManifest();
    if (!searcher->findFile(L, manifest->main))
    {
        lua_pop(L, 1);
        return false;
    }
    std::string fileName = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (luaL_loadentry(L, multilar->main.get(), fileName.c_str()) != LUA_OK)
    {
        lua_pushfstring(L, "Failed to load main file: %s", lua_tostring(L, -1));
        return false;
	}
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        lua_pushfstring(L, "Failed to execute main file: %s", lua_tostring(L, -1));
        return false;
	}
    return true;
}

static int mainc(const std::vector<std::string>& args, lua_State* L)
{
    if (args.size() < 3) {
        std::cerr << "[ERROR] Usage: <exe> --build <dir> -o <output>\n";
        return 1;
    }

    std::string dirName;
    std::string outputLar;
    bool skipNext = false;
    bool debugInfo = false;

    for (size_t i = 2; i < args.size(); i++)
    {
        if (skipNext) {
            skipNext = false;
            continue;
        }

        const std::string& arg = args[i];

        if (arg == "-o" || arg == "--output")
        {
            if (i + 1 >= args.size()) {
                std::cerr << "[ERROR] Missing value after " << arg << "\n";
                return 1;
            }

            outputLar = args[i + 1];
            skipNext = true;
        }
        else if (arg == "--debug-info")
        {
            debugInfo = true;
        }
        else {
            dirName = arg;
        }
    }

    if (dirName.empty()) {
        std::cerr << "[ERROR] No input directory specified\n";
        return 1;
    }

    if (outputLar.empty()) {
        std::cerr << "[ERROR] No output file specified\n";
        return 1;
    }

    if (!fs::exists(dirName) || !fs::is_directory(dirName)) {
        std::cerr << "[ERROR] Invalid input directory\n";
        return 1;
    }

    int err = 0;
    zip_t* za = zip_open(outputLar.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);

    if (!za) {
        zip_error_t ze;
        zip_error_init_with_code(&ze, err);
        std::cerr << "[ERROR] Failed to create archive: "
            << zip_error_strerror(&ze) << "\n";
        zip_error_fini(&ze);
        return 1;
    }

    for (const auto& entry : fs::recursive_directory_iterator(
        dirName,
        fs::directory_options::skip_permission_denied))
    {
        std::error_code ec;

        if (!entry.is_regular_file(ec)) {
            continue;
        }

        fs::path path = entry.path();
        fs::path rel = fs::relative(path, dirName, ec);

        if (ec) continue;

        std::string relStr = rel.generic_string();
        // =========================
        // LUA FILE BRANCH
        // =========================
        if (path.extension() == ".lua")
        {
            if (luaL_loadfile(L, path.string().c_str()) != LUA_OK)
            {
                std::cerr << "[ERROR] Failed to load Lua file: " << path << "\n";
                std::cerr << "[LUA] " << lua_tostring(L, -1) << "\n";
                lua_pop(L, 1);
                zip_close(za);
                return 1;
            }

            std::string outStr = relStr + "c";

            // usa tu función ya adaptada a libzip
            lua_entrydump(L, za, outStr.c_str(), !debugInfo);
        }
        // =========================
        // RAW FILE BRANCH
        // =========================
        else
        {
            if (!fs::exists(path)) {
                continue;
            }

            zip_source_t* src =
                zip_source_file(za, path.string().c_str(), 0, 0);

            if (!src)
            {
                std::cerr << "[ERROR] zip_source_file failed for " << path << "\n";
                zip_close(za);
                return 1;
            }

            zip_int64_t idx =
                zip_file_add(za, relStr.c_str(), src, ZIP_FL_OVERWRITE);

            if (idx < 0)
            {
                std::cerr << "[ERROR] zip_file_add failed for " << relStr << "\n";
                zip_source_free(src);
                zip_close(za);
                return 1;
            }
        }
    }
    zip_close(za);
    lua_close(L);
    return 0;
}
int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv, argv + argc);

    if (argc < 2)
    {
        std::cout << "No lar provided\n"
            << "Usage: " << args[0] << " <lar> [args...]\n";
        return 1;
    }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    if (args[1] == "--build")
    {
        return mainc(args, L);
    }
    std::string larFile;

    if (!findLarFile(args[1], larFile))
    {
        return luaL_error(
            L,
            "error in loading %s: file not found",
            args[1].c_str()
        );
    }

    MultiLar multiLar{ std::make_unique<LarManager>(L, larFile, true), {} };
    if (!loadDeps(L, &multiLar))
    {
        std::cerr << lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_close(L);
        return 1;
    }
    packagezSearcher searcher(L, &multiLar);
    LarAssets larAssets(L, &multiLar);
    if (!loadMain(L, &multiLar, &searcher))
    {
        std::cerr << lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, multiLar.main->getManifest()->manifestref);
    lua_close(L);
    return 0;
}