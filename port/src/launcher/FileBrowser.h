// Built-in, controller-navigable file browser (fallback for native dialogs, e.g. Steam Deck Game Mode).
#pragma once
#include <string>
#include <vector>

namespace zl {

class FileBrowser {
public:
    void open(const std::string& startDir = "");
    // L7: generic file picker. exts = lower-case extensions incl. dot (e.g. ".json"); empty = any file.
    // titleId = launcher string id for the title (default "fb.title"); allowFolder = show "use this folder".
    void openFiles(const std::string& startDir, const std::vector<std::string>& exts, const char* titleId,
                   bool allowFolder);
    bool isOpen() const { return open_; }
    void close() { open_ = false; }
    // Draws the browser; returns true when the user picked a path (result()).
    bool frame();
    const std::string& result() const { return result_; }

private:
    struct Entry { std::string name, path; bool dir; bool place; };
    void load(const std::string& dir);
    std::vector<Entry> entries_;
    std::vector<std::string> labels_;
    std::string dir_, result_;
    int sel_ = 0;
    bool open_ = false;
    bool generic_ = false, allowFolder_ = true;   // L7: generic file mode
    std::vector<std::string> exts_;
    std::string titleId_ = "fb.title";
};

}  // namespace zl
