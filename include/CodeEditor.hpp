#ifndef CODE_EDITOR_HPP
#define CODE_EDITOR_HPP

#include <imgui.h>

#include <string>
#include <vector>
#include <functional>

// A minimal embeddable code editor that draws monospace, syntax-highlighted text
// using ImDrawList and processes keyboard input through ImGui::GetIO().
//
// Each instance manages a buffer of open files; render() draws an inner tab bar
// for the files together with the active editor surface. The editor is scoped to
// .cow scripts; the syntax highlighter is supplied by cowscript::highlight().
class CodeEditor
{
public:
    using LogFn = std::function<void(const std::string &)>;

    struct Buffer
    {
        std::string path;     // absolute or relative path on disk
        std::string display;  // shown in the tab
        std::string text;     // current buffer contents
        int cursor = 0;       // byte offset of caret
        float scrollY = 0.0f;
        float scrollX = 0.0f;
        bool dirty = false;
        bool focusRequested = false;
    };

    void setLogger(LogFn fn) { logger = std::move(fn); }

    // Open a file from disk (creates the buffer if not already open and brings it to front).
    // Returns true if the file was loaded.
    bool openFile(const std::string &path);

    // Create a new in-memory script file (not yet saved to disk).
    void newFile(const std::string &suggestedName = "untitled.cow");

    // Save the currently active buffer to disk. Returns true on success.
    bool saveActive();

    void render();

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    bool hasActiveBuffer() const { return active >= 0 && active < (int)buffers.size(); }
    const std::string &activePath() const;

private:
    void renderEditor(Buffer &buf);
    void handleInput(Buffer &buf, ImFont *font, float charWidth, float lineHeight, ImVec2 origin, ImVec2 widgetSize);
    void log(const std::string &msg);

    static std::vector<int> lineStarts(const std::string &text);
    static void byteToLineCol(int byte, const std::vector<int> &starts, int &outLine, int &outCol);
    static int lineColToByte(const std::string &text, const std::vector<int> &starts, int line, int col);
    static int prevUtf8Boundary(const std::string &text, int byte);
    static int nextUtf8Boundary(const std::string &text, int byte);

    std::vector<Buffer> buffers;
    int active = -1;
    bool visible = true;
    LogFn logger;
};

#endif // CODE_EDITOR_HPP
