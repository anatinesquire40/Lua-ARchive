LarManager::LarManager(LuaPP::State& state, const std::filesystem::path& path, bool isMain) : larpath(path) {
	int err = 0;
	auto str = larpath.string();
	archive = zip_open(str.c_str(), ZIP_RDONLY, &err);
	if (!archive) {
		state.error(
			"Failed to open LAR file: " + str +
			" (err " + std::to_string(err) + ")"
		);
	}
	loadManifest(state, isMain);
}
LarManager::~LarManager() {
	if (archive) {
		zip_close(archive);
		archive = nullptr;
	}
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

	data.resize(static_cast<size_t>(r));
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

	if (zip_fseek(file, offset, SEEK_SET) != 0) {
		zip_fclose(file);
		return {};
	}

	size_t toRead = std::min(bytes, st.size - offset);

	std::string data;
	data.resize(toRead);

	zip_int64_t r = zip_fread(file, data.data(), toRead);

	zip_fclose(file);

	if (r < 0)
		return {};
	data.resize(static_cast<size_t>(r));
	return data;
}

static void depsTableToVector(LuaPP::State& state, const LuaPP::LTValue& strvec, std::vector<std::string>& deps) {
	for (auto& pair : state.ipairs(strvec)) {
		auto& value = pair.value();
		if (value.isstring()) {
			deps.push_back(value.getstring());
		}
	}
}

void LarManager::loadManifest(LuaPP::State& state, bool isMain) {
	auto chunk = state.mknilv();
	if (!luaL_loadentry(state, chunk, this, "manifest.luac"))
	{
		state.error("Failed to load manifest.luac: " + chunk.getstring());
	}
	auto envnew = state.newtable();
	state.setUpValue(chunk, 1, envnew); // avoid global access
	LuaPP::VLTValue args;
	auto manifestret = state.call(chunk, args, [](LuaPP::State& state, std::string msg) -> void {
		state.error("Failed to execute manifest.luac: " + msg);
	});
	auto& tableManifest = state.checkindex(manifestret, 0);
	manifest.name = state.getField(tableManifest, "name").checkstring();
	manifest.version = state.getField(tableManifest, "version").checkstring();
	manifest.disableExternalLua = false;
	manifest.manifestt = state.copy(tableManifest);
	if (isMain)
	{
		manifest.main = state.getField(tableManifest, "main").checkstring();
		auto deps = state.getField(tableManifest, "dependencies");
		if (deps.istable())
			depsTableToVector(state, deps, manifest.deps);
		manifest.disableExternalLua = state.getField(tableManifest, "disableExternalLua").getbool();
		
	}
}
#define classfunc(T) auto thz = state.checkindex(args, 0).checkud<T>();
luapp_function(LarManager::getLarFileName) {
	classfunc(LarManager)
	return state.mkvec(state.mkvalue(thz->larpath.filename().string()));
}
luapp_function(LarManager::getLarFilePath) {
	classfunc(LarManager)
	return state.mkvec(state.mkvalue(thz->larpath.string()));
}
luapp_function(LarManager::getLarPath) {
	classfunc(LarManager)
	return state.mkvec(state.mkvalue(thz->larpath.parent_path().string()));
}
luapp_function(LarManager::getManifestt) {
	classfunc(LarManager)
	return state.mkvec(state.copy(thz->manifest.manifestt));
}
luapp_function(LarManager::__gc) {
	classfunc(LarManager)
	thz->~LarManager();
	return {};
}
LarManifest* LarManager::getManifest() {
	return &manifest;
}
static constexpr LuaPP::fReg lar_mtfuncs[] = {
	{"getManifest", LarManager::getManifestt},
	{"getLarFileName", LarManager::getLarFileName},
	{"getLarFilePath", LarManager::getLarFilePath},
	{"getLarPath", LarManager::getLarPath}
};
static constexpr LuaPP::fReg multilar_funcs[] = {
	{"getDependencies", MultiLar::getDependencies},
	{"getMain", MultiLar::getMain},
	{"loadDependency", MultiLar::loadDependency}
};
void MultiLar::createMultiLarModule(LuaPP::State& state, const LuaPP::LTValue& obj) {
	auto mlar_mt = state.getMetatable(obj);
	auto __index = state.newtable();
	state.setFuncs(multilar_funcs, __index, sizeof(multilar_funcs));
	state.setField(mlar_mt, "__index", __index, true);
	state.setField(mlar_mt, "__name", state.mkvalue("MultiLarInstance"), true);
	auto package_loaded = state.getField(state.getGlobal("package"), "loaded", true);
	state.setField(package_loaded, "MultiLar", obj, true);
}

MultiLar::MultiLar(LuaPP::State& state, const std::filesystem::path& path)
	: main(state.mknilv()), dependencies(state.newtable()), state(state), mtlar(state.newtable()) {
	state.setField(mtlar, "__gc", state.mkfunction(LarManager::__gc), true);
	auto __index = state.newtable();
	state.setFuncs(lar_mtfuncs, __index, sizeof(lar_mtfuncs));
	state.setField(mtlar, "__index", __index, true);
	state.setField(mtlar, "__name", state.mkvalue("LarInstance"), true);
	LarManager* ptr = nullptr;
	main = state.mkuserdata(ptr);
	state.setMetatable(main, mtlar);
	if (!ptr) state.error("out of memory: init multilar");
	ptrmain = new (ptr) LarManager(state, path, true);
}
luapp_function(MultiLar::getDependencies) {
	classfunc(MultiLar)
	return state.mkvec(state.copy(thz->dependencies));
}
luapp_function(MultiLar::getMain) {
	classfunc(MultiLar)
	return state.mkvec(state.copy(thz->main));
}
extern bool findLarFile(const std::string& file, std::filesystem::path& out);
#define ret_error(str) state.mkvec(state.mkvalue(false), state.mkvalue(str))
#define ret_true state.mkvec(state.mkvalue(true))
luapp_function(MultiLar::loadDependency) {
	classfunc(MultiLar)
	auto name = state.checkindex(args, 1).checkstring();
	std::filesystem::path larfile;
	if (!findLarFile(name, larfile))
	{
		return ret_error("dependency " + name + " not found");
	}
	LarManager* ptr = nullptr;
	auto depud = state.mkuserdata(ptr);
	if (!ptr) return ret_error("couldn't reserve dep userdata"); 
	new (ptr) LarManager(state, larfile, false);
	state.setMetatable(depud, thz->mtlar);
	state.pushArrayBack(thz->dependencies, depud, true);
	thz->lastConstructed++;
	return ret_true;
}
MultiLar::~MultiLar() {
	if (lastConstructed != INVALID_INDEX) {
		lastConstructed = INVALID_INDEX;
		ptrmain = nullptr;
	}
}