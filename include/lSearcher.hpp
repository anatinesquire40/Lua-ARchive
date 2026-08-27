class packagezSearcher
{
public:
	packagezSearcher(LuaPP::State& state, MultiLar* mlar);
	static luapp_function(l_search);
	LuaPP::VLTValue search(LuaPP::State& state, const std::string& moduleName);
	bool findFile(LuaPP::State& state, std::string& out, const std::string& name);
	bool searchPath(
		LuaPP::State& state,
		std::string& out,
		const std::string& name,
		const std::string& path,
		const std::string& sep,
		const std::string& dirsep);
	std::string packagezSearcher::getPackagezPath(LuaPP::State& state);
private:
	MultiLar* multiLar;
};