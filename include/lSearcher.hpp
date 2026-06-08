class packagezSearcher
{
public:
	packagezSearcher(lua_State* L, MultiLar* mlar);
	static int l_search(lua_State* L);
	bool search(lua_State* L, const std::string& moduleName);
	bool findFile(lua_State* L, const std::string& name);
	bool searchPath(
		lua_State* L,
		const std::string& name,
		const std::string& path,
		const std::string& sep,
		const std::string& dirsep);
	std::string packagezSearcher::getPackagezPath(lua_State* L);
private:
	MultiLar* multiLar;
};