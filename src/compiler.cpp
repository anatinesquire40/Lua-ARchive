namespace fs = std::filesystem;
int mainc(const std::vector<std::string>& args, LuaPP::State& state)
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
            auto filefunc = state.mknilv();
            if (!state.loadfile(filefunc, path.string()))
            {
                std::cerr << "[ERROR] Failed to load Lua file: " << path << "\n";
                std::cerr << "[LUA] " << filefunc.getstring() << "\n";
                zip_close(za);
                return 1;
            }

            std::string outStr = relStr + "c";
            lua_entrydump(state, filefunc, za, outStr.c_str(), !debugInfo);
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
    return 0;
}