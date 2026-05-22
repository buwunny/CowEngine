#include "CodeEditor.hpp"

#include "script/CowScript.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    constexpr int TAB_WIDTH = 4;
    constexpr float GUTTER_CHARS = 6.0f; // space reserved for line numbers + padding

    ImU32 colorForToken(cowscript::TokenKind k)
    {
        switch (k)
        {
        case cowscript::TokenKind::Comment:
            return IM_COL32(110, 145, 110, 255);
        case cowscript::TokenKind::Keyword:
            return IM_COL32(198, 120, 221, 255);
        case cowscript::TokenKind::Number:
            return IM_COL32(255, 175, 100, 255);
        case cowscript::TokenKind::String:
            return IM_COL32(152, 195, 121, 255);
        case cowscript::TokenKind::Builtin:
            return IM_COL32(86, 180, 233, 255);
        case cowscript::TokenKind::Operator:
            return IM_COL32(200, 200, 220, 255);
        case cowscript::TokenKind::Punctuation:
            return IM_COL32(180, 180, 200, 255);
        case cowscript::TokenKind::Identifier:
            return IM_COL32(220, 220, 220, 255);
        case cowscript::TokenKind::Whitespace:
        case cowscript::TokenKind::Text:
        default:
            return IM_COL32(220, 220, 220, 255);
        }
    }

    // Read a file, searching the same candidate dirs that scripts do, so the
    // editor can open paths recorded in scene.json verbatim.
    std::string readFileWithFallback(const std::string &path, std::string *outResolved)
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> candidates;
        candidates.emplace_back(path);
#ifdef ASSET_ROOT
        candidates.emplace_back(fs::path(ASSET_ROOT) / path);
#endif
        candidates.emplace_back(fs::path("./") / path);
        candidates.emplace_back(fs::path("../") / path);
        candidates.emplace_back(fs::path("../../") / path);
        for (auto &c : candidates)
        {
            std::ifstream in(c);
            if (in)
            {
                std::stringstream ss;
                ss << in.rdbuf();
                if (outResolved)
                    *outResolved = c.string();
                return ss.str();
            }
        }
        return "";
    }
}

const std::string &CodeEditor::activePath() const
{
    static const std::string empty;
    if (!hasActiveBuffer())
        return empty;
    return buffers[active].path;
}

void CodeEditor::log(const std::string &msg)
{
    if (logger)
        logger(msg);
}

bool CodeEditor::openFile(const std::string &path)
{
    // If already open, just focus it.
    for (size_t i = 0; i < buffers.size(); ++i)
    {
        if (buffers[i].path == path)
        {
            active = static_cast<int>(i);
            buffers[i].focusRequested = true;
            return true;
        }
    }

    std::string resolved;
    std::string source = readFileWithFallback(path, &resolved);
    if (source.empty() && resolved.empty())
    {
        // Create a new empty file at the given path.
        Buffer b;
        b.path = path;
        b.display = std::filesystem::path(path).filename().string();
        if (b.display.empty())
            b.display = path;
        b.dirty = true;
        b.focusRequested = true;
        buffers.push_back(std::move(b));
        active = static_cast<int>(buffers.size()) - 1;
        log("New file: " + path);
        return true;
    }

    Buffer b;
    b.path = resolved.empty() ? path : resolved;
    b.display = std::filesystem::path(b.path).filename().string();
    if (b.display.empty())
        b.display = b.path;
    b.text = std::move(source);
    b.focusRequested = true;
    buffers.push_back(std::move(b));
    active = static_cast<int>(buffers.size()) - 1;
    log("Opened " + path);
    return true;
}

void CodeEditor::newFile(const std::string &suggestedName)
{
    Buffer b;
    b.path = suggestedName; // not yet on disk
    b.display = suggestedName;
    b.dirty = true;
    b.focusRequested = true;
    buffers.push_back(std::move(b));
    active = static_cast<int>(buffers.size()) - 1;
}

bool CodeEditor::saveActive()
{
    if (!hasActiveBuffer())
        return false;
    Buffer &b = buffers[active];
    std::ofstream out(b.path);
    if (!out)
    {
        log("Save failed: " + b.path);
        return false;
    }
    out << b.text;
    b.dirty = false;
    log("Saved " + b.path);
    return true;
}

std::vector<int> CodeEditor::lineStarts(const std::string &text)
{
    std::vector<int> starts;
    starts.push_back(0);
    for (int i = 0; i < (int)text.size(); ++i)
    {
        if (text[i] == '\n')
            starts.push_back(i + 1);
    }
    return starts;
}

void CodeEditor::byteToLineCol(int byte, const std::vector<int> &starts, int &outLine, int &outCol)
{
    int lo = 0, hi = (int)starts.size() - 1;
    while (lo < hi)
    {
        int mid = (lo + hi + 1) / 2;
        if (starts[mid] <= byte)
            lo = mid;
        else
            hi = mid - 1;
    }
    outLine = lo;
    outCol = byte - starts[lo];
}

int CodeEditor::lineColToByte(const std::string &text, const std::vector<int> &starts, int line, int col)
{
    if (starts.empty())
        return 0;
    line = std::max(0, std::min(line, (int)starts.size() - 1));
    int lineStart = starts[line];
    int lineEnd = (line + 1 < (int)starts.size()) ? starts[line + 1] - 1 : (int)text.size();
    int width = lineEnd - lineStart;
    col = std::max(0, std::min(col, width));
    return lineStart + col;
}

int CodeEditor::prevUtf8Boundary(const std::string &text, int byte)
{
    if (byte <= 0)
        return 0;
    int b = byte - 1;
    while (b > 0 && (static_cast<unsigned char>(text[b]) & 0xC0) == 0x80)
        --b;
    return b;
}

int CodeEditor::nextUtf8Boundary(const std::string &text, int byte)
{
    int n = (int)text.size();
    if (byte >= n)
        return n;
    int b = byte + 1;
    while (b < n && (static_cast<unsigned char>(text[b]) & 0xC0) == 0x80)
        ++b;
    return b;
}

void CodeEditor::render()
{
    if (!visible)
        return;

    if (ImGui::BeginTabBar("##CowFiles", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_TabListPopupButton | ImGuiTabBarFlags_FittingPolicyScroll))
    {
        for (size_t i = 0; i < buffers.size(); ++i)
        {
            Buffer &b = buffers[i];
            bool open = true;
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (b.dirty)
                flags |= ImGuiTabItemFlags_UnsavedDocument;
            if (b.focusRequested)
            {
                flags |= ImGuiTabItemFlags_SetSelected;
                b.focusRequested = false;
            }
            std::string label = b.display + "##" + b.path;
            if (ImGui::BeginTabItem(label.c_str(), &open, flags))
            {
                active = static_cast<int>(i);
                renderEditor(b);
                ImGui::EndTabItem();
            }
            if (!open)
            {
                buffers.erase(buffers.begin() + i);
                if (active >= (int)buffers.size())
                    active = (int)buffers.size() - 1;
                ImGui::EndTabBar();
                return;
            }
        }
        ImGui::EndTabBar();
    }

    if (buffers.empty())
    {
        ImGui::TextDisabled("No file open. Use the buttons above to open or create a .cow script.");
    }
}

void CodeEditor::renderEditor(Buffer &buf)
{
    ImFont *font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize();
    float lineHeight = ImGui::GetTextLineHeight();
    // ProggyClean (default ImGui font) is monospace; measure 'M' as the cell width.
    float charWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "M").x;
    if (charWidth <= 0.0f)
        charWidth = fontSize * 0.5f;

    ImVec2 region = ImGui::GetContentRegionAvail();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.13f, 1.0f));
    ImGui::BeginChild("##editorSurface", region, true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    auto starts = lineStarts(buf.text);
    int lineCount = (int)starts.size();

    // Reserve a dummy region so scrollbars know the content size; then we draw on top of it.
    float contentHeight = lineCount * lineHeight + lineHeight * 2.0f;
    // Find longest visible line for horizontal width.
    int maxLen = 0;
    for (int i = 0; i < lineCount; ++i)
    {
        int lineStart = starts[i];
        int lineEnd = (i + 1 < lineCount) ? starts[i + 1] - 1 : (int)buf.text.size();
        int len = lineEnd - lineStart;
        if (len > maxLen)
            maxLen = len;
    }
    float contentWidth = (GUTTER_CHARS + maxLen + 4) * charWidth;
    ImGui::Dummy(ImVec2(contentWidth, contentHeight));

    // Tokenize once for the whole buffer.
    auto tokens = cowscript::highlight(buf.text);

    // Render line by line.
    ImU32 gutterColor = IM_COL32(110, 110, 130, 220);
    ImU32 currentLineBg = IM_COL32(255, 255, 255, 12);
    ImU32 cursorColor = IM_COL32(240, 240, 255, 220);

    int cursorLine = 0, cursorCol = 0;
    byteToLineCol(buf.cursor, starts, cursorLine, cursorCol);

    // Highlight current line band.
    {
        float y = origin.y + cursorLine * lineHeight;
        dl->AddRectFilled(ImVec2(origin.x, y),
                          ImVec2(origin.x + contentWidth, y + lineHeight),
                          currentLineBg);
    }

    // Walk tokens and render
    auto drawSegment = [&](int byteFrom, int byteTo, ImU32 color)
    {
        if (byteFrom >= byteTo)
            return;
        // segment may span newlines; split per line.
        int p = byteFrom;
        while (p < byteTo)
        {
            int line, col;
            byteToLineCol(p, starts, line, col);
            int lineEnd = (line + 1 < lineCount) ? starts[line + 1] - 1 : (int)buf.text.size();
            int segEnd = std::min(byteTo, lineEnd);
            if (segEnd > p)
            {
                std::string seg(buf.text.data() + p, buf.text.data() + segEnd);
                // Expand tabs to spaces for rendering width.
                std::string rendered;
                rendered.reserve(seg.size());
                int colCursor = col;
                for (char ch : seg)
                {
                    if (ch == '\t')
                    {
                        int spaces = TAB_WIDTH - (colCursor % TAB_WIDTH);
                        rendered.append(spaces, ' ');
                        colCursor += spaces;
                    }
                    else
                    {
                        rendered.push_back(ch);
                        colCursor++;
                    }
                }
                float x = origin.x + (GUTTER_CHARS + col) * charWidth;
                float y = origin.y + line * lineHeight;
                dl->AddText(font, fontSize, ImVec2(x, y), color, rendered.c_str());
            }
            p = segEnd;
            if (p < buf.text.size() && buf.text[p] == '\n')
                ++p;
        }
    };

    for (const auto &tok : tokens)
    {
        if (tok.kind == cowscript::TokenKind::Whitespace)
            continue;
        drawSegment(tok.start, tok.start + tok.length, colorForToken(tok.kind));
    }

    // Gutter (line numbers) drawn over background. Use a fixed-position overlay so
    // it sticks to the left edge regardless of horizontal scroll.
    ImVec2 windowPos = ImGui::GetWindowPos();
    float scrollX = ImGui::GetScrollX();
    float gutterX = origin.x; // already adjusted for scroll by GetCursorScreenPos
    (void)windowPos;
    (void)scrollX;
    for (int i = 0; i < lineCount; ++i)
    {
        char buf16[16];
        std::snprintf(buf16, sizeof(buf16), "%4d", i + 1);
        float y = origin.y + i * lineHeight;
        dl->AddText(font, fontSize, ImVec2(gutterX, y), gutterColor, buf16);
    }

    // Cursor: convert col to visual column accounting for tabs.
    int visualCol = 0;
    {
        int lineStart = starts[cursorLine];
        for (int i = lineStart; i < buf.cursor; ++i)
        {
            char ch = buf.text[i];
            if (ch == '\t')
                visualCol += TAB_WIDTH - (visualCol % TAB_WIDTH);
            else
                visualCol++;
        }
    }
    float cx = origin.x + (GUTTER_CHARS + visualCol) * charWidth;
    float cy = origin.y + cursorLine * lineHeight;
    // Blink the cursor based on time.
    double t = ImGui::GetTime();
    if ((int)(t * 2.0) % 2 == 0)
        dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + lineHeight), cursorColor, 1.5f);

    // Make sure the cursor stays in view.
    {
        float viewMinY = ImGui::GetScrollY();
        float viewMaxY = viewMinY + ImGui::GetContentRegionAvail().y;
        if (cy < viewMinY)
            ImGui::SetScrollY(cy);
        else if (cy + lineHeight > viewMaxY)
            ImGui::SetScrollY(cy + lineHeight - ImGui::GetContentRegionAvail().y);
        float viewMinX = ImGui::GetScrollX();
        float viewMaxX = viewMinX + ImGui::GetContentRegionAvail().x;
        if (cx < viewMinX + GUTTER_CHARS * charWidth)
            ImGui::SetScrollX(std::max(0.0f, cx - GUTTER_CHARS * charWidth));
        else if (cx > viewMaxX - charWidth * 2)
            ImGui::SetScrollX(cx - ImGui::GetContentRegionAvail().x + charWidth * 4);
    }

    // Capture clicks/focus on the editor area
    bool hovered = ImGui::IsWindowHovered();
    bool active_focus = ImGui::IsWindowFocused();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGui::SetWindowFocus();
        active_focus = true;

        ImVec2 mp = ImGui::GetMousePos();
        float relX = mp.x - origin.x - GUTTER_CHARS * charWidth;
        float relY = mp.y - origin.y;
        int line = std::max(0, std::min(lineCount - 1, (int)std::floor(relY / lineHeight)));
        int targetVisualCol = std::max(0, (int)std::round(relX / charWidth));
        // Convert visual col back to byte col, accounting for tabs.
        int lineStart = starts[line];
        int lineEnd = (line + 1 < lineCount) ? starts[line + 1] - 1 : (int)buf.text.size();
        int b = lineStart;
        int v = 0;
        while (b < lineEnd && v < targetVisualCol)
        {
            char ch = buf.text[b];
            if (ch == '\t')
            {
                int spaces = TAB_WIDTH - (v % TAB_WIDTH);
                if (v + spaces > targetVisualCol)
                    break;
                v += spaces;
                ++b;
            }
            else
            {
                v += 1;
                b = nextUtf8Boundary(buf.text, b);
            }
        }
        buf.cursor = b;
    }

    if (active_focus)
        handleInput(buf, font, charWidth, lineHeight, origin, region);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void CodeEditor::handleInput(Buffer &buf, ImFont *, float, float, ImVec2, ImVec2)
{
    ImGuiIO &io = ImGui::GetIO();

    auto starts = lineStarts(buf.text);
    int lineCount = (int)starts.size();

    auto cursorLineCol = [&](int &line, int &col)
    {
        byteToLineCol(buf.cursor, starts, line, col);
    };

    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;
    (void)shift;

    // Ctrl+S: save
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        saveActive();
    }

    // Ctrl+V: paste
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        const char *clip = ImGui::GetClipboardText();
        if (clip)
        {
            std::string s(clip);
            buf.text.insert(buf.cursor, s);
            buf.cursor += (int)s.size();
            buf.dirty = true;
        }
        return;
    }

    // Backspace
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true))
    {
        if (buf.cursor > 0)
        {
            int prev = prevUtf8Boundary(buf.text, buf.cursor);
            buf.text.erase(prev, buf.cursor - prev);
            buf.cursor = prev;
            buf.dirty = true;
        }
    }

    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, true))
    {
        if (buf.cursor < (int)buf.text.size())
        {
            int next = nextUtf8Boundary(buf.text, buf.cursor);
            buf.text.erase(buf.cursor, next - buf.cursor);
            buf.dirty = true;
        }
    }

    // Arrows
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
    {
        buf.cursor = prevUtf8Boundary(buf.text, buf.cursor);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
    {
        buf.cursor = nextUtf8Boundary(buf.text, buf.cursor);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
    {
        int line, col;
        cursorLineCol(line, col);
        if (line > 0)
            buf.cursor = lineColToByte(buf.text, starts, line - 1, col);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
    {
        int line, col;
        cursorLineCol(line, col);
        if (line + 1 < lineCount)
            buf.cursor = lineColToByte(buf.text, starts, line + 1, col);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, true))
    {
        int line, col;
        cursorLineCol(line, col);
        buf.cursor = starts[line];
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int lineEnd = (line + 1 < lineCount) ? starts[line + 1] - 1 : (int)buf.text.size();
        buf.cursor = lineEnd;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int target = std::max(0, line - 20);
        buf.cursor = lineColToByte(buf.text, starts, target, col);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int target = std::min(lineCount - 1, line + 20);
        buf.cursor = lineColToByte(buf.text, starts, target, col);
    }

    // Enter: insert newline, preserve leading indent of current line
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int lineStart = starts[line];
        std::string indent;
        for (int i = lineStart; i < (int)buf.text.size() && (buf.text[i] == ' ' || buf.text[i] == '\t'); ++i)
            indent.push_back(buf.text[i]);
        std::string ins = "\n" + indent;
        buf.text.insert(buf.cursor, ins);
        buf.cursor += (int)ins.size();
        buf.dirty = true;
    }

    // Tab: insert spaces
    if (ImGui::IsKeyPressed(ImGuiKey_Tab, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int spaces = TAB_WIDTH - (col % TAB_WIDTH);
        buf.text.insert(buf.cursor, std::string(spaces, ' '));
        buf.cursor += spaces;
        buf.dirty = true;
    }

    // Character input
    for (ImWchar ch : io.InputQueueCharacters)
    {
        if (ch == 0)
            continue;
        if (ch == '\r')
            continue;
        if (ch < 32 && ch != '\n' && ch != '\t')
            continue;
        char utf8[5] = {0};
        int n = 0;
        if (ch < 0x80)
        {
            utf8[n++] = (char)ch;
        }
        else if (ch < 0x800)
        {
            utf8[n++] = (char)(0xC0 | ((ch >> 6) & 0x1F));
            utf8[n++] = (char)(0x80 | (ch & 0x3F));
        }
        else
        {
            utf8[n++] = (char)(0xE0 | ((ch >> 12) & 0x0F));
            utf8[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            utf8[n++] = (char)(0x80 | (ch & 0x3F));
        }
        buf.text.insert(buf.cursor, utf8, n);
        buf.cursor += n;
        buf.dirty = true;
    }
    io.InputQueueCharacters.resize(0);
}
