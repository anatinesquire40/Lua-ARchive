#define getthz() LarAssets* thz = upvals[0].getud<LarAssets>();
#define getasset() AssetEntry* entry = state.checkindex(args, 0).checkud<AssetEntry>();
LarAssets::LarAssets(LuaPP::State& state, MultiLar* mlar)
	: mlar(mlar)
{
	auto loaded = state.getField(state.getGlobal("package"), "loaded");
	auto assets_lib = state.newtable();
	auto open_func = state.mkfunction(l_open, state.mkvec(state.mkvalue(this)));

	state.setField(assets_lib, "open", open_func);
	state.setField(loaded, "LarAssets", assets_lib);
	initMetatable(state);
}
LarAssets::~LarAssets() {}

bool LarAssets::open(const std::string& fname, AssetEntry* asset)
{
	zip_t* archive = nullptr;
	asset->path = "assets/" + fname;

	if (!findFileInMultiLar(asset->path, mlar, &archive))
		return false;

	zip_stat_t st;
	if (zip_stat(archive, asset->path.c_str(), 0, &st) != 0)
		return false;

	zip_file_t* file = zip_fopen(archive, asset->path.c_str(), 0);
	if (!file)
		return false;
	asset->cur = 0;
	asset->archive = archive;
	asset->file = file;
	asset->size = st.size;

	return true;
}
static constexpr LuaPP::fReg mt_funcs[] = {
	{"read", LarAssets::l_read},
	{"lines", LarAssets::l_lines},
	{"seek", LarAssets::l_seek},
	{"size", LarAssets::l_size},
	{"close", LarAssets::l_close},
};
void LarAssets::initMetatable(LuaPP::State& state)
{
	metatable = state.newtable();
	auto __indext = state.newtable();
	LuaPP::VLTValue upvalues_mt;
	upvalues_mt.push_back(state.mkvalue(this));
	state.setFuncs(mt_funcs, __indext, sizeof(mt_funcs), upvalues_mt);
	state.setField(metatable, "__index", __indext);
	state.setField(metatable, "__name", state.mkvalue("AssetEntry"));
}
luapp_function(LarAssets::l_open)
{
	auto ret = state.mkvec();
	getthz()
	std::string fname = state.checkindex(args, 0).checkstring();
	AssetEntry* asset = nullptr;
	auto ud = state.mkuserdata(asset);
	new (asset) AssetEntry();
	if (!thz->open(fname, asset))
	{
		ret.push_back(state.mknilv());
		ret.push_back(state.mkvalue("Failed to open "+ fname + ": entry doesn't exists"));
		return ret;
	}
	state.setMetatable(ud, thz->metatable);
	ret.push_back(std::move(ud));
	return ret;
}
luapp_function(LarAssets::l_read)
{
#define failnil() { ret.push_back(state.mknilv()); return ret; }
	auto ret = state.mkvec();
	getthz()
	getasset()
	auto& option = state.optindex(args, 1);
	if (!entry->file)
	{
		ret.push_back(state.mknilv());
		return ret;
	}

	if (option.isint() || option.isnum())
	{
		size_t n = (size_t)option.getint();
		if (n <= 0) failnil()

		std::string buf;
		buf.resize(n);

		zip_int64_t r = zip_fread(entry->file, buf.data(), n);

		if (r <= 0) failnil()

		entry->cur += (size_t)r;

		buf.resize(r);
		ret.push_back(state.mkvalue(buf));
		return ret;
	}

	std::string mode;
	if (option.isstring()) {
		mode = option.getstring();
	} else {
		mode = "*l";
	}
	if (mode == "*a")
	{
		std::string buf;
		buf.resize(entry->size - entry->cur);

		zip_int64_t r = zip_fread(entry->file, buf.data(), buf.size());

		if (r <= 0) failnil()

		entry->cur = entry->size;

		buf.resize(r);
		ret.push_back(state.mkvalue(buf));
		return ret;
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

		if (line.empty()) failnil()

		ret.push_back(state.mkvalue(line));
		return ret;
	}

	state.error("invalid read mode: " + mode);
	return ret;
}
luapp_function(LarAssets::l_size)
{
	getthz()
	getasset()
	return state.mkvec(state.mkvalue(static_cast<LuaPP::Integer>(thz->size(entry))));;
}

size_t LarAssets::size(AssetEntry* asset)
{
	return asset->size;
}
luapp_function(LarAssets::l_seek)
{
	getthz()
	getasset()
	auto& mode = state.optindex(args, 1);
	std::string smode = "cur";
	if (mode.isstring())
		smode = mode.getstring();
	auto& offset = state.optindex(args, 2);
	size_t ioffset = 0;
	if (offset.isint() || offset.isnum())
		ioffset = (size_t)offset.getint();
	return state.mkvec(state.mkvalue(static_cast<LuaPP::Integer>(thz->seek(entry, smode, ioffset))));
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

static luapp_function(lines_iter)
{
	getthz()

	AssetEntry* entry = upvals[1].getud<AssetEntry>();

	zip_file_t* tmp = zip_fopen(entry->archive, entry->path.c_str(), 0);
	if (!tmp)
		return {};

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
		return {};
	return state.mkvec(state.mkvalue(line));
}
luapp_function(LarAssets::l_lines)
{
	getthz()
	getasset()
	return state.mkvec(state.mkfunction(lines_iter, state.mkvec(state.mkvalue(thz), state.mkvalue(entry))));
}
luapp_function(LarAssets::l_close)
{
	getthz()
	getasset()
	thz->close(entry);
	entry->~AssetEntry();
	return {};
}
void LarAssets::close(AssetEntry* asset)
{
	if (asset->file)
	{
		zip_fclose(asset->file);
		asset->file = nullptr;
	}
}