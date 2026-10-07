// JUCE-free checks for the per-parameter / per-section look trigger table.
#include "../Source/UI/LookTriggers.h"
#include <goodlookinui/Design.h>
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <string>
int main() {
    using namespace LookTriggers;
    std::map<std::string, std::set<std::string>> looksByParam;   // parameter -> distinct (style,colour,plate)
    std::set<std::string> seenPair;
    for (const auto& e : entries) {
        std::string c = e.colour;
        assert(c.size() == 7 && c[0] == '#' && c.find_first_not_of("0123456789abcdefABCDEF", 1) == std::string::npos);
        assert(seenPair.insert(std::string(e.parameter) + "/" + e.choice).second);
        assert(e.style && goodlookinui::findStyle(e.style) && "unknown knob style");
        if (e.plate) assert(goodlookinui::findFaceplate(e.plate) && "unknown plate");
        if (e.faceplate) assert(goodlookinui::findFaceplate(e.faceplate));
        // each choice of one parameter must look different from its siblings
        assert(looksByParam[e.parameter].insert(std::string(e.style) + c + (e.plate ? e.plate : "-")).second);
        assert(find(e.parameter, e.choice) == &e);
    }
    // the faceplate driver defines a faceplate for each of its 5 choices
    int driven = 0;
    for (const auto& e : entries) if (std::string(e.parameter) == faceplateDriver) { assert(e.faceplate); ++driven; }
    assert(driven == 5);
    assert(!find("lowType", "N-Type") && !find("nope", "Brit"));
    // requested looks
    auto look = [](const char* p, const char* c) { auto* e = find(p, c); assert(e); return e; };
    assert(std::string(look("lowType", "Bax-EQ")->style) == "Skt" && std::string(look("lowType", "Bax-EQ")->plate) == "Black");
    assert(std::string(look("highType", "Bax-EQ")->style) == "Skt");
    assert(std::string(look("mid1Type", "A-Type")->style) == "A" && std::string(look("mid1Type", "A-Type")->plate) == "Black");
    assert(std::string(look("mid2Type", "A-Type")->plate) == "Cream");                    // 550 vs 5500
    assert(std::string(look("mid1Type", "N-EQ")->style) == "N" && std::string(look("mid1Type", "N-EQ")->plate) == "Marine");
    assert(std::string(look("mid1Type", "Brit")->style) == "Brit" && std::string(look("mid1Type", "Brit")->plate) == "Granite");
    assert(std::string(look("lowType", "FSF")->style) == "FS" && std::string(look("lowType", "FSF")->plate) == "Royal");
    // the same choice can look different on different parameters
    assert(std::string(look("mid1Type", "Brit")->colour) != look("mid2Type", "Brit")->colour);
    assert(std::string(look("lowType", "Brit")->colour) != look("highType", "Brit")->colour);
    std::cout << looksByParam.size() << " parameters, " << std::size(entries) << " entries: per-section looks OK\n";
}
