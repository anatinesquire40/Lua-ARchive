LarManager::LarManager(lua_State* L, const std::string& path, bool isMain) {
	int err = 0;

	archive = zip_open(path.c_str(), ZIP_RDONLY, &err);
	if (!archive) {
		luaL_error(L, "Failed to open LAR file: %s (err %d)", path.c_str(), err);
	}

	loadManifest(L, isMain);
}
LarManager::~LarManager() {
	if (archive) {
		zip_close(archive);
	}

	delete manifest;
}
zip_t* LarManager::getArchive() {
	return archive;
}

bool LarManager::exists(const std::string& path) {
	zip_stat_t st;
	return zip_stat(archive, path.c_str(), 0, &st) == 0;
}
std::string LarManager::readFile(const std::string& path) {
	zip_file_t* file = zip_fopen(archive, path.c_str(), 0);
	if (!file) return {};

	zip_stat_t st;
	if (zip_stat(archive, path.c_str(), 0, &st) != 0) {
		zip_fclose(file);
		return {};
	}

	std::string data;
	data.resize(st.size);

	zip_int64_t r = zip_fread(file, data.data(), st.size);
	zip_fclose(file);

	if (r < 0)
		return {};

	return data;
}
std::string LarManager::readFile(const std::string& path, size_t offset, size_t bytes) {
	zip_file_t* file = zip_fopen(archive, path.c_str(), 0);
	if (!file) return {};

	zip_stat_t st;
	if (zip_stat(archive, path.c_str(), 0, &st) != 0) {
		zip_fclose(file);
		return {};
	}

	if (offset >= st.size) {
		zip_fclose(file);
		return {};
	}

	zip_fseek(file, offset, SEEK_SET);

	size_t toRead = std::min(bytes, st.size - offset);

	std::string data;
	data.resize(toRead);

	zip_int64_t r = zip_fread(file, data.data(), toRead);

	zip_fclose(file);

	if (r < 0)
		return {};

	return data;
}

static void depsTableToVector(lua_State* L, int index, std::vector<std::string>& deps) {
	size_t len = lua_rawlen(L, index);
	for (size_t i = 1; i <= len; ++i) {
		lua_rawgeti(L, index, i);
		if (lua_type(L, -1) == LUA_TSTRING) {
			deps.push_back(lua_tostring(L, -1));
		}
		lua_pop(L, 1);
	}
}

void LarManager::loadManifest(lua_State* L, bool isMain) {
	if (luaL_loadentry(L, this, "manifest.luac") != LUA_OK)
	{
		luaL_error(L, "Failed to load manifest.luac: %s", lua_tostring(L, -1));
	}
	lua_newtable(L);
	lua_setupvalue(L, -2, 1); // avoid global access
	if (lua_pcall(L, 0, 1, 0) != LUA_OK)
	{
		luaL_error(L, "Failed to execute manifest.luac: %s", lua_tostring(L, -1));
	}
	manifest = new LarManifest{};
	int tableManifest = lua_absindex(L, -1);
	lua_getfield(L, tableManifest, "name");
	manifest->name = luaL_checkstring(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, tableManifest, "version");
	manifest->version = luaL_checkstring(L, -1);
	manifest->disableExternalLua = false;
	manifest->main = "";
	manifest->deps = {};
	manifest->manifestref = 0;
	lua_pop(L, 1);
	if (isMain)
	{
		lua_getfield(L, tableManifest, "main");
		manifest->main = lua_tostring(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, tableManifest, "dependencies");
		if (lua_istable(L, -1))
			depsTableToVector(L, -1, manifest->deps);
		lua_pop(L, 1);
		lua_getfield(L, tableManifest, "disableExternalLua");
		if (lua_isboolean(L, -1))
			manifest->disableExternalLua = lua_toboolean(L, -1);
		lua_pop(L, 1);
		manifest->manifestref = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	else
	{
		lua_pop(L, 1);
	}
}

LarManifest* LarManager::getManifest() {
	return manifest;
}