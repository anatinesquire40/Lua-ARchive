typedef struct {
    size_t assetsIndex;
    size_t cur;
    zip_t* archive;
    zip_file_t* file;
    std::string path;
    size_t size;
} AssetEntry;
class LarAssets {
public:
    LarAssets(lua_State* L, MultiLar* mlar);
    ~LarAssets();

    static int l_open(lua_State* L);
    AssetEntry* open(const std::string& fname);

    static int l_read(lua_State* L);

    static int l_lines(lua_State* L);

    static int l_seek(lua_State* L);
    size_t seek(AssetEntry* asset, std::string& mod, size_t offset);

    static int l_size(lua_State* L);
    size_t size(AssetEntry* asset);

    static int l_close(lua_State* L);
    void close(AssetEntry* asset);

private:
    MultiLar* mlar;
    std::unordered_map<size_t, std::unique_ptr<AssetEntry>> assets;
    size_t nextId;
};