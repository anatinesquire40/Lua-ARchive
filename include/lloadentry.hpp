bool luaL_loadentry(LuaPP::State& state, LuaPP::LTValue& out, MultiLar* mlar, const std::string& name);
bool luaL_loadentry(LuaPP::State& state, LuaPP::LTValue& out, LarManager* lar, const std::string& name);
bool lua_entrydump(LuaPP::State& state, const LuaPP::LTValue& func, zip_t* archive, const char* entry, bool strip);