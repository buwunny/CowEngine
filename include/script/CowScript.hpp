#ifndef COW_SCRIPT_HPP
#define COW_SCRIPT_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace cowscript
{

    struct Value
    {
        enum Type
        {
            Null,
            Number,
            Bool,
            Str,
            Handle
        } type = Null;
        double num = 0.0;
        bool boolean = false;
        std::string str;
        // For Handle values: `str` carries the kind (e.g. "transform"),
        // `handle` carries an opaque pointer into engine state.
        void *handle = nullptr;

        static Value makeNumber(double n)
        {
            Value v;
            v.type = Number;
            v.num = n;
            return v;
        }
        static Value makeBool(bool b)
        {
            Value v;
            v.type = Bool;
            v.boolean = b;
            return v;
        }
        static Value makeString(std::string s)
        {
            Value v;
            v.type = Str;
            v.str = std::move(s);
            return v;
        }
        static Value makeHandle(std::string kind, void *ptr)
        {
            Value v;
            v.type = Handle;
            v.str = std::move(kind);
            v.handle = ptr;
            return v;
        }
        static Value makeNull() { return Value{}; }

        bool truthy() const;
        std::string toString() const;
        double toNumber() const;
    };

    using BuiltinFn = std::function<Value(const std::vector<Value> &)>;
    using PropertyGetFn = std::function<Value(const Value &target, const std::string &prop)>;
    using PropertySetFn = std::function<void(const Value &target, const std::string &prop, const Value &value)>;

    class Script
    {
    public:
        Script();
        ~Script();
        Script(const Script &) = delete;
        Script &operator=(const Script &) = delete;

        // Parse the source code. Returns an empty string on success or a human-readable
        // error message describing the parse problem.
        std::string compile(const std::string &source);

        // Register a built-in function callable from the script under `name`.
        void setBuiltin(const std::string &name, BuiltinFn fn);

        // Register property accessors used when the script does `handle.prop` or
        // `handle.prop = value`. Both must be set for full read/write support; a
        // missing handler causes a runtime error on use.
        void setPropertyGetter(PropertyGetFn fn);
        void setPropertySetter(PropertySetFn fn);

        // True if the script defines `on <name>(...)`.
        bool hasEvent(const std::string &name) const;

        // Invoke the `on <name>(args)` event handler if it exists. Returns an empty string
        // on success, or a runtime-error message describing the failure.
        std::string callEvent(const std::string &name, const std::vector<Value> &args);

        const std::string &source() const { return src; }

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
        std::string src;
    };

    // Read a script file. Searches common candidate paths (cwd, ASSET_ROOT, parents).
    // Returns the source. If outFoundPath is not null, writes the resolved path to it.
    std::string readScriptFile(const std::string &path, std::string *outFoundPath = nullptr);

    // Token used by the editor for syntax highlighting.
    enum class TokenKind
    {
        Text,
        Comment,
        Keyword,
        Number,
        String,
        Identifier,
        Builtin,
        Operator,
        Punctuation,
        Whitespace
    };

    struct Token
    {
        TokenKind kind = TokenKind::Text;
        int start = 0;
        int length = 0;
    };

    // Tokenize source code into syntax-highlightable spans. Always produces a contiguous,
    // covering set of tokens (so the editor can render them sequentially).
    std::vector<Token> highlight(const std::string &source);

} // namespace cowscript

#endif // COW_SCRIPT_HPP
