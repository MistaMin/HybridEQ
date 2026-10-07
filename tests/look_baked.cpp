// Checks that the plugin's baked Designs/LookTriggers.csv is a valid design: it parses, every look in it is
// valid (known knob style, plate and faceplate, well-formed colour), and every driving parameter is covered.
// The file is the source of truth (developer mode autosaves into it), so it is allowed to differ from the
// built-in defaults.  Run from the repository root.
#include "../Source/UI/LookTriggers.h"
#include <goodlookinui/LookTable.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
int main() {
    goodlookinui::LookTable baked(LookTriggers::defaultLooks());
    std::ifstream in("Designs/LookTriggers.csv"); std::stringstream ss; ss << in.rdbuf();
    assert(in.good() || !ss.str().empty());
    assert(baked.fromCsv(ss.str()));
    std::set<std::string> params;
    for (const auto& l : baked.all()) { assert(goodlookinui::validLook(l)); params.insert(l.parameter); }
    for (const char* p : {"lcSlope", "hcSlope", "lowType", "mid1Type", "mid2Type", "highType", "preampType"}) assert(params.count(p));
    std::cout << "baked looks CSV: " << baked.all().size() << " valid looks covering " << params.size() << " parameters\n";
}
