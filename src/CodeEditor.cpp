#include "CodeEditor.hpp"

#include "script/CowScript.hpp"
#include "Scene.hpp"

#include <algorithm>
#include <cctype>
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
    out.close();
    b.dirty = false;
    log("Saved " + b.path);
    // Mirror the on-disk change into localStorage so .cow edits survive a web
    // page reload (parallels Scene::saveToJSON).
    Scene::snapshotScriptsToLocalStorage();
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

namespace
{
    inline bool isWordChar(unsigned char c)
    {
        return std::isalnum(c) != 0 || c == '_';
    }
    inline bool isSpaceChar(unsigned char c)
    {
        return c == ' ' || c == '\t';
    }
}

int CodeEditor::prevWordBoundary(const std::string &text, int byte)
{
    if (byte <= 0)
        return 0;
    int p = byte;
    // Skip spaces and tabs (but not newlines — keep them as their own boundary).
    while (p > 0 && isSpaceChar((unsigned char)text[p - 1]))
        --p;
    if (p > 0 && text[p - 1] == '\n')
    {
        // Land just after a newline if that's what we crossed.
        return p == byte ? p - 1 : p;
    }
    if (p > 0)
    {
        bool word = isWordChar((unsigned char)text[p - 1]);
        while (p > 0)
        {
            unsigned char c = (unsigned char)text[p - 1];
            if (c == '\n' || isSpaceChar(c))
                break;
            if (isWordChar(c) != word)
                break;
            --p;
        }
    }
    return p;
}

int CodeEditor::nextWordBoundary(const std::string &text, int byte)
{
    int n = (int)text.size();
    if (byte >= n)
        return n;
    int p = byte;
    unsigned char first = (unsigned char)text[p];
    if (first == '\n')
        return p + 1;
    if (isSpaceChar(first))
    {
        while (p < n && isSpaceChar((unsigned char)text[p]))
            ++p;
        return p;
    }
    bool word = isWordChar(first);
    while (p < n)
    {
        unsigned char c = (unsigned char)text[p];
        if (c == '\n' || isSpaceChar(c))
            break;
        if (isWordChar(c) != word)
            break;
        ++p;
    }
    while (p < n && isSpaceChar((unsigned char)text[p]))
        ++p;
    return p;
}

bool CodeEditor::hasSelection(const Buffer &buf)
{
    return buf.selectionAnchor >= 0 && buf.selectionAnchor != buf.cursor;
}

int CodeEditor::selBegin(const Buffer &buf)
{
    if (!hasSelection(buf))
        return buf.cursor;
    return std::min(buf.selectionAnchor, buf.cursor);
}

int CodeEditor::selEnd(const Buffer &buf)
{
    if (!hasSelection(buf))
        return buf.cursor;
    return std::max(buf.selectionAnchor, buf.cursor);
}

void CodeEditor::clearSelection(Buffer &buf)
{
    buf.selectionAnchor = -1;
}

void CodeEditor::moveCursorTo(Buffer &buf, int newPos, bool extend)
{
    if (extend)
    {
        if (buf.selectionAnchor < 0)
            buf.selectionAnchor = buf.cursor;
    }
    else
    {
        buf.selectionAnchor = -1;
    }
    buf.cursor = std::max(0, std::min(newPos, (int)buf.text.size()));
}

void CodeEditor::pushUndo(Buffer &buf, EditKind kind)
{
    // Group consecutive same-kind edits (typing / single backspaces) into one
    // undo step so a single Ctrl+Z reverts a whole word burst, not one char.
    if (!buf.undoStack.empty() && buf.lastEditKind == kind &&
        (kind == EditKind::Insert || kind == EditKind::Delete))
    {
        // Don't push a new entry — the existing top still represents the state
        // before the burst began.
        buf.redoStack.clear();
        buf.lastEditKind = kind;
        return;
    }
    UndoState s;
    s.text = buf.text;
    s.cursor = buf.cursor;
    s.anchor = buf.selectionAnchor;
    buf.undoStack.push_back(std::move(s));
    if (buf.undoStack.size() > 500)
        buf.undoStack.erase(buf.undoStack.begin());
    buf.redoStack.clear();
    buf.lastEditKind = kind;
}

void CodeEditor::deleteSelection(Buffer &buf)
{
    if (!hasSelection(buf))
        return;
    int a = selBegin(buf);
    int b = selEnd(buf);
    pushUndo(buf, EditKind::Other);
    buf.text.erase(a, b - a);
    buf.cursor = a;
    buf.selectionAnchor = -1;
    buf.dirty = true;
}

void CodeEditor::insertText(Buffer &buf, const std::string &s, EditKind kind)
{
    if (hasSelection(buf))
    {
        // Treat replace-selection as an atomic "Other" edit so it stays a single undo step.
        deleteSelection(buf);
        buf.lastEditKind = EditKind::Other;
    }
    pushUndo(buf, kind);
    buf.text.insert(buf.cursor, s);
    buf.cursor += (int)s.size();
    buf.dirty = true;
}

void CodeEditor::applyUndo(Buffer &buf)
{
    if (buf.undoStack.empty())
        return;
    UndoState cur;
    cur.text = buf.text;
    cur.cursor = buf.cursor;
    cur.anchor = buf.selectionAnchor;
    buf.redoStack.push_back(std::move(cur));

    UndoState prev = std::move(buf.undoStack.back());
    buf.undoStack.pop_back();
    buf.text = std::move(prev.text);
    buf.cursor = std::min(prev.cursor, (int)buf.text.size());
    buf.selectionAnchor = prev.anchor;
    buf.dirty = true;
    buf.lastEditKind = EditKind::Other;
}

void CodeEditor::applyRedo(Buffer &buf)
{
    if (buf.redoStack.empty())
        return;
    UndoState cur;
    cur.text = buf.text;
    cur.cursor = buf.cursor;
    cur.anchor = buf.selectionAnchor;
    buf.undoStack.push_back(std::move(cur));

    UndoState next = std::move(buf.redoStack.back());
    buf.redoStack.pop_back();
    buf.text = std::move(next.text);
    buf.cursor = std::min(next.cursor, (int)buf.text.size());
    buf.selectionAnchor = next.anchor;
    buf.dirty = true;
    buf.lastEditKind = EditKind::Other;
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

    if (editorFocusRequested)
    {
        ImGui::SetWindowFocus();
        editorFocusRequested = false;
    }

    ImVec2 origin = ImGui::GetCursorScreenPos();
    // Visible content area, captured before the Dummy advances the layout
    // cursor — we need it for the cursor-stays-in-view math below.
    ImVec2 visibleSize = ImGui::GetContentRegionAvail();
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

    // Selection band(s). Compute the visual column of an arbitrary byte offset,
    // honoring tab expansion, then paint a rectangle per affected line.
    if (hasSelection(buf))
    {
        ImU32 selectionBg = IM_COL32(80, 130, 200, 110);
        int sBegin = selBegin(buf);
        int sEnd = selEnd(buf);
        int beginLine, beginCol, endLine, endCol;
        byteToLineCol(sBegin, starts, beginLine, beginCol);
        byteToLineCol(sEnd, starts, endLine, endCol);

        auto visualColAt = [&](int line, int byte) {
            int lineStart = starts[line];
            int v = 0;
            for (int i = lineStart; i < byte; ++i)
            {
                if (buf.text[i] == '\t')
                    v += TAB_WIDTH - (v % TAB_WIDTH);
                else
                    v++;
            }
            return v;
        };

        for (int line = beginLine; line <= endLine; ++line)
        {
            int lineStart = starts[line];
            int lineEnd = (line + 1 < lineCount) ? starts[line + 1] - 1 : (int)buf.text.size();
            int segFrom = (line == beginLine) ? sBegin : lineStart;
            int segTo = (line == endLine) ? sEnd : lineEnd;
            int v0 = visualColAt(line, segFrom);
            int v1 = visualColAt(line, segTo);
            float x0 = origin.x + (GUTTER_CHARS + v0) * charWidth;
            float x1 = origin.x + (GUTTER_CHARS + v1) * charWidth;
            // Show trailing-newline selection as a thin sliver so empty lines stay visible.
            if (line < endLine && x1 == x0)
                x1 += charWidth * 0.5f;
            float y = origin.y + line * lineHeight;
            dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + lineHeight), selectionBg);
        }
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

    // Make sure the cursor stays in view. cx/cy are in screen coords but scroll
    // values are window-local, so convert through (cursorLine * lineHeight) and
    // (visualCol * charWidth) — once contentHeight exceeds the visible area
    // mixing the two spaces causes the scroll to be set to bogus screen-sized
    // values and the editor jitters.
    {
        float cursorLocalY = cursorLine * lineHeight;
        float cursorLocalX = (GUTTER_CHARS + visualCol) * charWidth;
        float viewMinY = ImGui::GetScrollY();
        float viewMaxY = viewMinY + visibleSize.y;
        if (cursorLocalY < viewMinY)
            ImGui::SetScrollY(cursorLocalY);
        else if (cursorLocalY + lineHeight > viewMaxY)
            ImGui::SetScrollY(cursorLocalY + lineHeight - visibleSize.y);
        float viewMinX = ImGui::GetScrollX();
        float viewMaxX = viewMinX + visibleSize.x;
        if (cursorLocalX < viewMinX + GUTTER_CHARS * charWidth)
            ImGui::SetScrollX(std::max(0.0f, cursorLocalX - GUTTER_CHARS * charWidth));
        else if (cursorLocalX > viewMaxX - charWidth * 2)
            ImGui::SetScrollX(cursorLocalX - visibleSize.x + charWidth * 4);
    }

    // Capture clicks/focus on the editor area
    bool hovered = ImGui::IsWindowHovered();
    bool active_focus = ImGui::IsWindowFocused();

    auto mousePosToByte = [&](ImVec2 mp) {
        float relX = mp.x - origin.x - GUTTER_CHARS * charWidth;
        float relY = mp.y - origin.y;
        int line = std::max(0, std::min(lineCount - 1, (int)std::floor(relY / lineHeight)));
        int targetVisualCol = std::max(0, (int)std::round(relX / charWidth));
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
        return b;
    };

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGui::SetWindowFocus();
        active_focus = true;
        int b = mousePosToByte(ImGui::GetMousePos());
        bool extend = ImGui::GetIO().KeyShift;
        moveCursorTo(buf, b, extend);
        buf.draggingSelection = true;
    }
    if (buf.draggingSelection)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            int b = mousePosToByte(ImGui::GetMousePos());
            if (b != buf.cursor)
                moveCursorTo(buf, b, true);
        }
        else
        {
            buf.draggingSelection = false;
        }
    }

    if (active_focus)
        handleInput(buf, font, charWidth, lineHeight, origin, region);

    // Right-click context menu (Cut / Copy / Paste / Select All). Place the
    // cursor at the click target first so the menu acts on the right spot.
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::SetWindowFocus();
        if (!hasSelection(buf))
        {
            int b = mousePosToByte(ImGui::GetMousePos());
            moveCursorTo(buf, b, false);
        }
        ImGui::OpenPopup("##CodeCtx");
    }
    if (ImGui::BeginPopup("##CodeCtx"))
    {
        bool sel = hasSelection(buf);
        const char *clip = ImGui::GetClipboardText();
        bool hasClip = clip && *clip;
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, sel))
        {
            int a = selBegin(buf);
            int b = selEnd(buf);
            ImGui::SetClipboardText(std::string(buf.text.data() + a, buf.text.data() + b).c_str());
            pushUndo(buf, EditKind::Other);
            deleteSelection(buf);
            buf.dirty = true;
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, sel))
        {
            int a = selBegin(buf);
            int b = selEnd(buf);
            ImGui::SetClipboardText(std::string(buf.text.data() + a, buf.text.data() + b).c_str());
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, hasClip))
            insertText(buf, std::string(clip), EditKind::Other);
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A"))
        {
            buf.selectionAnchor = 0;
            buf.cursor = (int)buf.text.size();
        }
        ImGui::EndPopup();
    }

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

    // Ctrl+S: save
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        saveActive();
        return;
    }

    // Ctrl+Z: undo, Ctrl+Y (or Ctrl+Shift+Z): redo
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, true))
    {
        applyUndo(buf);
        return;
    }
    if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_Y, true) ||
                 (shift && ImGui::IsKeyPressed(ImGuiKey_Z, true))))
    {
        applyRedo(buf);
        return;
    }

    // Ctrl+A: select all.
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        buf.selectionAnchor = 0;
        buf.cursor = (int)buf.text.size();
        buf.lastEditKind = EditKind::None;
        return;
    }

    // Ctrl+C: copy selection (if any) to clipboard.
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        if (hasSelection(buf))
        {
            int a = selBegin(buf);
            int b = selEnd(buf);
            std::string s(buf.text.data() + a, buf.text.data() + b);
            ImGui::SetClipboardText(s.c_str());
        }
        return;
    }

    // Ctrl+V: paste, replacing any selection.
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        const char *clip = ImGui::GetClipboardText();
        if (clip && *clip)
        {
            insertText(buf, std::string(clip), EditKind::Other);
        }
        return;
    }

    // Backspace / Ctrl+Backspace
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true))
    {
        if (hasSelection(buf))
        {
            deleteSelection(buf);
        }
        else if (buf.cursor > 0)
        {
            int target = ctrl ? prevWordBoundary(buf.text, buf.cursor)
                              : prevUtf8Boundary(buf.text, buf.cursor);
            pushUndo(buf, EditKind::Delete);
            buf.text.erase(target, buf.cursor - target);
            buf.cursor = target;
            buf.dirty = true;
        }
        return;
    }

    // Delete / Ctrl+Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, true))
    {
        if (hasSelection(buf))
        {
            deleteSelection(buf);
        }
        else if (buf.cursor < (int)buf.text.size())
        {
            int target = ctrl ? nextWordBoundary(buf.text, buf.cursor)
                              : nextUtf8Boundary(buf.text, buf.cursor);
            pushUndo(buf, EditKind::Delete);
            buf.text.erase(buf.cursor, target - buf.cursor);
            buf.dirty = true;
        }
        return;
    }

    // Arrow movement (extend selection when Shift held; jump words when Ctrl held).
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
    {
        int target = ctrl ? prevWordBoundary(buf.text, buf.cursor)
                          : prevUtf8Boundary(buf.text, buf.cursor);
        moveCursorTo(buf, target, shift);
        buf.lastEditKind = EditKind::None;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
    {
        int target = ctrl ? nextWordBoundary(buf.text, buf.cursor)
                          : nextUtf8Boundary(buf.text, buf.cursor);
        moveCursorTo(buf, target, shift);
        buf.lastEditKind = EditKind::None;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
    {
        int line, col;
        cursorLineCol(line, col);
        if (line > 0)
            moveCursorTo(buf, lineColToByte(buf.text, starts, line - 1, col), shift);
        buf.lastEditKind = EditKind::None;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
    {
        int line, col;
        cursorLineCol(line, col);
        if (line + 1 < lineCount)
            moveCursorTo(buf, lineColToByte(buf.text, starts, line + 1, col), shift);
        buf.lastEditKind = EditKind::None;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, true))
    {
        int line, col;
        cursorLineCol(line, col);
        moveCursorTo(buf, starts[line], shift);
        buf.lastEditKind = EditKind::None;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int lineEnd = (line + 1 < lineCount) ? starts[line + 1] - 1 : (int)buf.text.size();
        moveCursorTo(buf, lineEnd, shift);
        buf.lastEditKind = EditKind::None;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int target = std::max(0, line - 20);
        moveCursorTo(buf, lineColToByte(buf.text, starts, target, col), shift);
        buf.lastEditKind = EditKind::None;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true))
    {
        int line, col;
        cursorLineCol(line, col);
        int target = std::min(lineCount - 1, line + 20);
        moveCursorTo(buf, lineColToByte(buf.text, starts, target, col), shift);
        buf.lastEditKind = EditKind::None;
    }

    // Enter: insert newline, preserve leading indent of current line
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, true))
    {
        if (hasSelection(buf))
            deleteSelection(buf);
        // Re-read line starts since deleting a selection may have shifted offsets.
        starts = lineStarts(buf.text);
        lineCount = (int)starts.size();
        int line, col;
        cursorLineCol(line, col);
        int lineStart = starts[line];
        std::string indent;
        for (int i = lineStart; i < (int)buf.text.size() && (buf.text[i] == ' ' || buf.text[i] == '\t'); ++i)
            indent.push_back(buf.text[i]);
        insertText(buf, "\n" + indent, EditKind::Other);
        return;
    }

    // Tab: insert spaces
    if (ImGui::IsKeyPressed(ImGuiKey_Tab, true))
    {
        if (hasSelection(buf))
            deleteSelection(buf);
        starts = lineStarts(buf.text);
        int line, col;
        cursorLineCol(line, col);
        int spaces = TAB_WIDTH - (col % TAB_WIDTH);
        insertText(buf, std::string(spaces, ' '), EditKind::Other);
        return;
    }

    // Character input
    if (!ctrl)
    {
        std::string burst;
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
            burst.append(utf8, n);
        }
        if (!burst.empty())
        {
            insertText(buf, burst, EditKind::Insert);
        }
    }
    io.InputQueueCharacters.resize(0);
}
