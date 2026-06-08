#define getthz() LarAssets* thz = reinterpret_cast<LarAssets*>(lua_touserdata(L, lua_upvalueindex(1)));
#define getasset() AssetEntry* entry = reinterpret_cast<AssetEntry*>(lua_touserdata(L, 1));
LarAssets::LarAssets(lua_State* L, MultiLar* mlar)
	: mlar(mlar), nextId(0)
{
	luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
	lua_newtable(L);

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, LarAssets::l_open, 1);
	lua_setfield(L, -2, "open");

	lua_setfield(L, -2, "LarAssets");
	lua_pop(L, 1);
}
LarAssets::~LarAssets() {}
AssetEntry* LarAssets::open(const std::string& fname)
{
	zip_t* archive = nullptr;

	std::string fullPath = "assets/" + fname;

	if (!findFileInMultiLar(fullPath, mlar, &archive))
		return nullptr;

	zip_stat_t st;
	if (zip_stat(archive, fullPath.c_str(), 0, &st) != 0)
		return nullptr;

	zip_file_t* file = zip_fopen(archive, fullPath.c_str(), 0);
	if (!file)
		return nullptr;

	auto asset = std::make_unique<AssetEntry>();
	asset->assetsIndex = nextId++;
	asset->cur = 0;
	asset->archive = archive;
	asset->file = file;
	asset->path = fullPath;
	asset->size = st.size;

	AssetEntry* ptr = asset.get();
	assets[asset->assetsIndex] = std::move(asset);

	return ptr;
}
static const luaL_Reg mt_funcs[] = {
	{"read", LarAssets::l_read},
	{"lines", LarAssets::l_lines},
	{"seek", LarAssets::l_seek},
	{"size", LarAssets::l_size},
	{"close", LarAssets::l_close},
	{NULL, NULL},
};
static void asignMetatable(lua_State* L, int udindex, LarAssets* thz)
{
	int index = lua_absindex(L, udindex);
	lua_newtable(L);
	int metaindex = lua_gettop(L);
	lua_newtable(L);
	lua_pushlightuserdata(L, thz);
	luaL_setfuncs(L, mt_funcs, 1);
	lua_setfield(L, metaindex, "__index");
	lua_pushstring(L, "AssetEntry");
	lua_setfield(L, metaindex, "__name");
	lua_setmetatable(L, index);
}
int LarAssets::l_open(lua_State* L)
{
	getthz()
	std::string fname = lua_tostring(L, 1);
	AssetEntry* asset = thz->open(fname);
	if (!asset)
	{
		lua_pushnil(L);
		lua_pushfstring(L, "Failed to open %s: entry doesn't exists", fname.c_str());
		return 2;
	}
	lua_pushlightuserdata(L, asset);
	asignMetatable(L, -1, thz);
	return 1;
}
int LarAssets::l_read(lua_State* L)
{
	getthz()
		getasset()

		if (!entry->file)
		{
			lua_pushnil(L);
			return 1;
		}

	if (lua_type(L, 2) == LUA_TNUMBER)
	{
		int n = (int)lua_tointeger(L, 2);
		if (n <= 0) return 0;

		std::string buf;
		buf.resize(n);

		zip_int64_t r = zip_fread(entry->file, buf.data(), n);

		if (r <= 0)
		{
			lua_pushnil(L);
			return 1;
		}

		entry->cur += (size_t)r;

		lua_pushlstring(L, buf.data(), (size_t)r);
		return 1;
	}

	std::string mode = luaL_optstring(L, 2, "*l");

	if (mode == "*a")
	{
		std::string buf;
		buf.resize(entry->size - entry->cur);

		zip_int64_t r = zip_fread(entry->file, buf.data(), buf.size());

		if (r <= 0)
		{
			lua_pushnil(L);
			return 1;
		}

		entry->cur = entry->size;

		lua_pushlstring(L, buf.data(), (size_t)r);
		return 1;
	}

	if (mode == "*l" || mode == "*L")
	{
		std::string line;
		char c;

		bool includeNewline = (mode == "*L");

		while (zip_fread(entry->file, &c, 1) == 1)
		{
			entry->cur++;

			if (c == '\n')
			{
				if (includeNewline)
					line.push_back(c);
				break;
			}

			line.push_back(c);
		}

		if (line.empty())
			return 0;

		lua_pushlstring(L, line.data(), line.size());
		return 1;
	}

	return luaL_error(L, "invalid read mode: %s", mode.c_str());
}
int LarAssets::l_size(lua_State* L)
{
	getthz()
	getasset()

	lua_pushinteger(L, (lua_Integer)thz->size(entry));
	return 1;
}

size_t LarAssets::size(AssetEntry* asset)
{
	return asset->size;
}
int LarAssets::l_seek(lua_State* L)
{
	getthz()
	getasset()

	std::string mode = luaL_optstring(L, 2, "cur");
	lua_Integer offset = luaL_optinteger(L, 3, 0);

	lua_pushinteger(L, thz->seek(entry, mode, offset));
	return 1;
}
size_t LarAssets::seek(AssetEntry* asset, std::string& mode, size_t offset)
{
	if (mode == "set")
		asset->cur = offset;
	else if (mode == "cur")
		asset->cur += offset;
	else if (mode == "end")
		asset->cur = asset->size + offset;

	if (asset->file)
	{
		zip_fclose(asset->file);
		asset->file = zip_fopen(asset->archive, asset->path.c_str(), 0);
		zip_fseek(asset->file, asset->cur, SEEK_SET);
	}

	return asset->cur;
}

static int lines_iter(lua_State* L)
{
	getthz()

		AssetEntry* entry =
		(AssetEntry*)lua_touserdata(L, lua_upvalueindex(2));

	zip_file_t* tmp = zip_fopen(entry->archive, entry->path.c_str(), 0);
	if (!tmp)
		return 0;

	zip_fseek(tmp, entry->cur, SEEK_SET);

	std::string line;
	char c;

	while (zip_fread(tmp, &c, 1) == 1)
	{
		entry->cur++;

		if (c == '\n')
			break;

		line.push_back(c);
	}

	zip_fclose(tmp);

	if (line.empty())
		return 0;

	lua_pushlstring(L, line.data(), line.size());
	return 1;
}
int LarAssets::l_lines(lua_State* L)
{
	getthz()
	getasset()

	lua_pushlightuserdata(L, thz);
	lua_pushlightuserdata(L, entry);
	lua_pushcclosure(L, lines_iter, 2);

	return 1;
}
int LarAssets::l_close(lua_State* L)
{
	getthz()
	getasset()
	thz->close(entry);
	return 0;
}
void LarAssets::close(AssetEntry* entry)
{
	if (entry->file)
		zip_fclose(entry->file);

	assets.erase(entry->assetsIndex);
}