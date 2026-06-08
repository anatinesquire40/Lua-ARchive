LUALIB_API int luaL_loadentry(lua_State* L, MultiLar* mlar, const char* name);
LUALIB_API int luaL_loadentry(lua_State* L, LarManager* lar, const char* name);
LUALIB_API int lua_entrydump(lua_State* L, zip_t* archive, const char* entry, bool strip);