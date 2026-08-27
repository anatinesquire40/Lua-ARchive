typedef struct {
    size_t cur;
    zip_t* archive;
    zip_file_t* file;
    std::string path;
    size_t size;
} AssetEntry;
class LarAssets {
public:
    LarAssets(LuaPP::State& state, MultiLar* mlar);
    ~LarAssets();

	void initMetatable(LuaPP::State& state);

    static luapp_function(l_open);
    bool open(const std::string& fname, AssetEntry* asset);

    static luapp_function(l_read);

    static luapp_function(l_lines);

    static luapp_function(l_seek);
    size_t seek(AssetEntry* asset, std::string& mod, size_t offset);

    static luapp_function(l_size);
    size_t size(AssetEntry* asset);

    static luapp_function(l_close);
    void close(AssetEntry* asset);
private:
    MultiLar* mlar;
    LuaPP::LTValue metatable;
};