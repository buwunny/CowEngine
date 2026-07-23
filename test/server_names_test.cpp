// Headless test for the server side of player names: what GameServer accepts
// off the wire, and who it tells about whom. Drives GameServer directly through
// its transport hooks, so it needs no sockets. Verifies:
//   * a name from a ClientHello is relayed to the other players
//   * a joiner learns the names of everyone ALREADY in the room (the regression:
//     PlayerJoin only went out to existing players, so a late joiner met the
//     others through nameless snapshots and could never label their avatars)
//   * junk names are cleaned up rather than relayed verbatim: control characters
//     and non-ASCII stripped, length clamped, and an empty result replaced by an
//     assigned "Cow <n>"

#include "net/Protocol.hpp"
#include "server/GameServer.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace net;

static int failures = 0;
#define CHECK(c) do { if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while(0)

namespace
{
    struct Outbox
    {
        struct Entry
        {
            uint32_t session;
            Message msg;
        };
        std::vector<Entry> entries;

        // Names this session was told about, in the order they arrived.
        std::vector<std::string> joinNamesFor(uint32_t session) const
        {
            std::vector<std::string> out;
            for (const auto &e : entries)
                if (e.session == session)
                    if (const auto *j = std::get_if<PlayerJoin>(&e.msg))
                        out.push_back(j->name);
            return out;
        }
        bool sawJoinName(uint32_t session, const std::string &name) const
        {
            for (const auto &n : joinNamesFor(session))
                if (n == name)
                    return true;
            return false;
        }
    };

    // Connect a session and hello it in one step, as a real client does.
    void join(GameServer &server, uint32_t session, const std::string &name)
    {
        server.onConnect(session);
        ClientHello hello;
        hello.name = name;
        server.onMessage(session, hello);
    }
}

int main()
{
    Outbox out;
    GameServer server;
    server.setSend([&](uint32_t session, const Message &m)
                   { out.entries.push_back({session, m}); });
    if (!server.init("scenes/scene.json"))
    {
        printf("FAIL: GameServer::init\n");
        return 1;
    }

    // Two players arrive in order. Each must hear about the other, whichever
    // side of the join they were on.
    join(server, 1, "Bessie");
    join(server, 2, "Daisy");
    CHECK(out.sawJoinName(1, "Daisy")); // the newcomer announced to the incumbent
    CHECK(out.sawJoinName(2, "Bessie")); // the room's roster replayed to the newcomer
    printf("  session 1 heard: ");
    for (const auto &n : out.joinNamesFor(1)) printf("'%s' ", n.c_str());
    printf("\n  session 2 heard: ");
    for (const auto &n : out.joinNamesFor(2)) printf("'%s' ", n.c_str());
    printf("\n");

    // Nobody is announced to themselves — a player's own tag is never drawn,
    // and a self-join would make the client build an avatar for itself.
    CHECK(!out.sawJoinName(1, "Bessie"));
    CHECK(!out.sawJoinName(2, "Daisy"));

    // Untrusted names. Each of these is relayed to session 1, so read the name
    // it was handed rather than trusting the one that was sent.
    auto relayedName = [&](const std::string &raw) {
        size_t before = out.entries.size();
        static uint32_t nextSession = 10;
        join(server, nextSession++, raw);
        for (size_t i = before; i < out.entries.size(); ++i)
            if (out.entries[i].session == 1)
                if (const auto *j = std::get_if<PlayerJoin>(&out.entries[i].msg))
                    return j->name;
        return std::string("<none>");
    };

    const std::string tabbed = relayedName("  Moo\tDeng  ");
    CHECK(tabbed == "Moo Deng"); // trimmed, inner whitespace collapsed to a space

    const std::string controls = relayedName("a\nb\r\x01z");
    CHECK(controls == "abz"); // control characters dropped, not relayed

    const std::string tooLong = relayedName("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK(tooLong.size() == kMaxPlayerNameLen);

    const std::string empty = relayedName("");
    CHECK(empty.rfind("Cow ", 0) == 0); // assigned rather than left blank

    const std::string junk = relayedName("\x01\x02\x03");
    CHECK(junk.rfind("Cow ", 0) == 0); // nothing printable survived -> assigned

    printf("  sanitised: '%s' | '%s' | '%s' (%zu) | '%s' | '%s'\n",
           tabbed.c_str(), controls.c_str(), tooLong.c_str(), tooLong.size(),
           empty.c_str(), junk.c_str());

    if (failures == 0) printf("ALL PASS\n");
    else printf("%d FAILURES\n", failures);
    return failures ? 1 : 0;
}
