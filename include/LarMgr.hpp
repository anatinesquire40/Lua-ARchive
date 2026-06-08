//using namespace libzippp;
struct LarManifest {
	std::string name;
	std::string version;
	std::string main;
    bool disableExternalLua;
	std::vector<std::string> deps;
    int manifestref;
};

class LarManager {
public:
    LarManager(lua_State* L, const std::string& path, bool isMain);
    ~LarManager();

    zip_t* getArchive();

    bool exists(const std::string& path);

    std::string readFile(const std::string& path);
    std::string readFile(const std::string& path, size_t offset, size_t bytes);

    LarManifest* getManifest();

private:
    void loadManifest(lua_State* L, bool isMain);

    LarManifest* manifest;
    zip_t* archive;
};
struct MultiLar {
    std::unique_ptr<LarManager> main;
	std::vector<std::unique_ptr<LarManager>> dependencies;
};
bool findFileInMultiLar(
    const std::string& filename,
    MultiLar* multiLar,
    zip_t** outArchive);