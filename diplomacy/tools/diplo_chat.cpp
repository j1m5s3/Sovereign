// diplo_chat: talk to an AI leader from the command line, in a freshly generated game.
//
//   diplo_chat --rules data/rules [--civ CIVILIZATION_EGYPT] [--port 8080] [--scripted] [--say "line" ...]
//
// With a llama-server running on this machine (`llama-server -m model.gguf --port 8080`) the
// leader speaks through the model; otherwise the scripted fallback answers. Each line shows the
// rules' verdict on anything proposed. Without --say it reads lines from stdin until "bye".
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "sovereign/game.h"
#include "sovereign_diplomacy/dialogue.h"
#include "sovereign_diplomacy/http.h"

using namespace sov;
using namespace sov::diplomacy;

int main(int argc, char** argv) {
    std::string rulesDir = "data/rules", civ = "CIVILIZATION_EGYPT";
    int port = 8080;
    bool scriptedOnly = false;
    std::vector<std::string> lines;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--rules" && i + 1 < argc) rulesDir = argv[++i];
        else if (a == "--civ" && i + 1 < argc) civ = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (a == "--scripted") scriptedOnly = true;
        else if (a == "--say" && i + 1 < argc) lines.push_back(argv[++i]);
        else {
            std::fprintf(stderr, "unknown argument %s\n", a.c_str());
            return 2;
        }
    }
    Rules rules;
    std::string err;
    if (!rules.load({rulesDir}, &err)) {
        std::fprintf(stderr, "cannot load rules: %s\n", err.c_str());
        return 1;
    }
    GameSetup setup;
    setup.seed = 11;
    setup.mapSize = "MAPSIZE_DUEL";
    setup.cityStates = 0;
    setup.players.push_back({rules.civs[0].id == civ ? rules.civs[1].id : rules.civs[0].id, true});
    setup.players.push_back({civ, false});
    auto game = Game::create(rules, setup, &err);
    if (!game) {
        std::fprintf(stderr, "cannot create a game: %s\n", err.c_str());
        return 1;
    }
    // Skip the introductions: the two civs have met.
    GameState s = game->state();
    for (Player& p : s.players) p.met.assign(s.players.size(), 1);
    game = Game::fromScenario(rules, std::move(s));

    SocketTransport net("127.0.0.1", port, 60);
    LlamaModel llama(net);
    const bool server = !scriptedOnly && net.healthy();
    std::printf("%s\n", server ? "Model server found: the leader speaks through it." : "No model server: scripted replies.");
    Conversation talk(*game, 1, 0, server ? static_cast<Model*>(&llama) : nullptr);
    const Persona& p = talk.persona();
    std::printf("You stand before %s of %s (%s). Agenda: %s.\n\n", p.leaderName.c_str(), p.civName.c_str(), relationshipName(p.relationship),
                p.agendaName.c_str());
    auto turn = [&](const std::string& words) {
        const Exchange e = talk.say(words);
        std::printf("> %s\n%s: %s\n", e.words.c_str(), p.leaderName.c_str(), e.reply.c_str());
        if (e.verdict != Verdict::None) {
            static const char* const names[] = {"", "would accept", "refuses", "cannot be done"};
            std::printf("   [rules: %s]\n", names[static_cast<int>(e.verdict)]);
        }
        if (e.scripted && server) std::printf("   [scripted fallback]\n");
        return e.reading.intent != Intent::Leave;
    };
    if (!lines.empty()) {
        for (const std::string& l : lines) turn(l);
    } else {
        std::string l;
        while (std::printf("you: "), std::getline(std::cin, l) && turn(l)) {}
    }
    const Command summary = talk.summaryCommand();
    std::printf("\nRecorded for the leader's memory: %s\n", summary.text.empty() ? "(nothing)" : summary.text.c_str());
    return 0;
}
