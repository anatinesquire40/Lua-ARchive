#define luapp_function(fname) LuaPP::VLTValue fname(LuaPP::State& state, const LuaPP::VLTValue& args, const LuaPP::VUpValue& upvals)
struct LarManifest {
	std::string name;
	std::string version;
	std::string main;
    bool disableExternalLua;
	std::vector<std::string> deps;
    LuaPP::LTValue manifestt;
};

class LarManager {
public:
    LarManager(LuaPP::State& state, const std::filesystem::path& path, bool isMain);
    ~LarManager();

    zip_t* getArchive();

    bool exists(const std::string& path);

    std::string readFile(const std::string& path);
    std::string readFile(const std::string& path, size_t offset, size_t bytes);
    static luapp_function(getLarFilePath);
    static luapp_function(getLarFileName);
    static luapp_function(getLarPath);
    static luapp_function(getManifestt);
    static luapp_function(__gc);
    LarManifest* getManifest();
    std::filesystem::path larpath;
private:
    void loadManifest(LuaPP::State& state, bool isMain);

    LarManifest manifest;
    zip_t* archive;
};
inline constexpr size_t INVALID_INDEX = SIZE_MAX;
struct MultiLar {
    MultiLar(LuaPP::State& state, const std::filesystem::path& path);
    static void createMultiLarModule(LuaPP::State& state, const LuaPP::LTValue& obj);
    ~MultiLar();
    static luapp_function(getDependencies);
    static luapp_function(getMain);
    static luapp_function(loadDependency);
    LuaPP::LTValue main;
    LarManager* ptrmain = nullptr;
    LuaPP::LTValue mtlar;
    LuaPP::LTValue dependencies;
    size_t lastConstructed = INVALID_INDEX;
    LuaPP::State& state;
};
bool findFileInMultiLar(
    const std::string& filename,
    MultiLar* multiLar,
    zip_t** outArchive);