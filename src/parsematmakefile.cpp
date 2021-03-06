#include "parsematmakefile.h"
#include "line.h"

Json parseMatmakefile(std::istream &file) {

    auto json = Json{Json::Array};

    auto lastTarget = Json{};

    auto finishTarget = [&lastTarget, &json] {
        if (!lastTarget.empty()) {
            json.push_back(lastTarget);
        }
        lastTarget = Json{Json::Object};
    };

    for (auto line = Line{file}; line.type != Line::End; line = Line{file}) {
        switch (line.type) {
        case Line::Normal:
            // Something without '='
            if (lastTarget.size() > 0 && line.indent > 0) {
                lastTarget.back().emplace_back(Json{line.name});
                lastTarget.back().type = Json::Array;
            }
            else {
                finishTarget();
                lastTarget["name"] = line.name;
            }
            break;
        case Line::Assignment:
            // For example "x = y"
            if (!line.name.empty() && !line.value.empty() &&
                line.name.front() == '-') {
                // Some flags like "-stdlib=c++"
                lastTarget.back().emplace_back(
                    Json{line.name + "=" + line.value.front()});
                lastTarget.back().type = Json::Array;
            }
            else if (line.value.size() == 1) {
                lastTarget[line.name] = line.value.front();
            }
            else if (line.value.size() > 1) {
                lastTarget[line.name].assign(line.value.begin(),
                                             line.value.end());
            }
            break;
        case Line::UnfinishedAssignment:
            // For example "  x ="
        default:
            lastTarget[line.name];
            break;
        }
    }

    finishTarget();

    return json;
}

Json parseMatmakefile(filesystem::path path) {
    auto file = std::ifstream{path};

    if (!file.is_open()) {
        throw std::runtime_error{"could not open matmakefile: " +
                                 path.string()};
    }

    return parseMatmakefile(file);
}
