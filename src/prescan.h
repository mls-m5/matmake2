#pragma once

#include "filesystem.h"
#include "processedcommand.h"
#include "sourcetype.h"
#include "tasklist.h"
#include "json/json.h"
#include <iostream>
#include <optional>

struct PrescanResult {
    std::string name;
    std::vector<std::string> imports;
    std::vector<std::string> includes;
};

// Load in parsed information from prescaned file if it exist and is newer than
// source
std::optional<PrescanResult> parsePrescanResults(filesystem::path expandedFile,
                                                 filesystem::path jsonFile,
                                                 filesystem::path source) {

    if (!filesystem::exists(expandedFile) || !filesystem::exists(jsonFile)) {
        return {}; // Not found -> create files
    }
    else if (filesystem::last_write_time(jsonFile) <
             filesystem::last_write_time(source)) {
        return {}; // Its old -> redo
    }

    const auto json = Json::loadFile(jsonFile);

    auto result = PrescanResult{};

    if (auto f = json.find("name"); f != json.end()) {
        result.name = f->string();
    }

    if (auto f = json.find("modules"); f != json.end()) {
        result.imports.reserve(f->size());
        for (auto &j : json["modules"]) {
            result.imports.push_back(j.string());
        }
    }

    if (auto f = json.find("include"); f != json.end()) {
        result.includes.reserve(f->size());
        for (auto &j : *f) {
            result.includes.push_back(j.string());
        }
    }

    return result;
}

//! Parse a expanded file and alse write result to the provide json path
PrescanResult parseExpandedFile(filesystem::path expandedFile,
                                filesystem::path jsonFile) {
    auto file = std::ifstream{expandedFile};

    if (!file.is_open()) {
        throw std::runtime_error{"could not open expanded file " +
                                 expandedFile.string()};
    }

    // Do some more fancy way to detect all cases here
    constexpr auto importStatement = std::string_view{"import "};
    constexpr auto exportStatement = std::string_view{"export module "};

    auto ret = PrescanResult{};

    std::map<std::string, size_t> includes;

    for (std::string line; getline(file, line);) {
        if (line.empty()) {
            continue;
        }
        if (line.rfind(importStatement, 0) == 0) {
            if (auto f = line.find(';'); f != std::string::npos) {
                ret.imports.push_back(
                    {line.begin() + importStatement.size(), line.begin() + f});
            }
        }
        else if (line.rfind(exportStatement, 0) == 0) {
            if (auto f = line.find(';'); f != std::string::npos) {
                ret.name = {line.begin() + exportStatement.size(),
                            line.begin() + f};
            }
        }
        else if (line.front() == '#' && line.size() > 4 && line[2] >= '0' &&
                 line[2] <= '9') {
            // Example
            // # 790 "./include/hello.h" 3
            if (auto f = line.find('"'); f != std::string::npos) {
                auto f2 = line.rfind('"');

                if (f2 > f) {
                    auto include =
                        std::string{line.begin() + f + 1, line.begin() + f2};
                    if (include.front() != '<') {
                        if (include.front() != '/') { // Ignore system includes
                            ++includes[include];
                        }
                    }
                }
            }
        }
    }

    file.close();

    for (auto &i : includes) {
        ret.includes.push_back(i.first);
    }

    auto json = Json{Json::Object};

    if (!ret.name.empty()) {
        json["name"] = ret.name;
    }

    {
        auto &inJson = json["modules"];

        inJson.type = Json::Array;

        for (auto &in : ret.imports) {
            inJson.push_back(Json{}.string(in));
        }
    }

    {
        auto &impJson = json["include"];

        impJson = Json::Array;

        for (auto &imp : ret.includes) {
            impJson.push_back(Json{}.string(imp));
        }
    }

    std::ofstream{jsonFile} << json;

    return ret;
}

PrescanResult prescan(Task &task) {
    auto expandedFile = task.out();
    auto source = task.in().front()->out();
    auto jsonFile = task.dir() / (source.string() + ".json");

    if (auto p = parsePrescanResults(expandedFile, jsonFile, source); p) {
        return std::move(*p);
    }

    auto command = ProcessedCommand{task.commandAt("eem")}.expand(task);

    std::cout << "prescanning with: " << command << "\n";

    if (system(command.c_str())) {
        throw std::runtime_error{"failed to prescan " + task.out().string() +
                                 "\nwith command " + command};
    }
    else if (!filesystem::exists(expandedFile)) {

        throw std::runtime_error{"could not find expanded file " +
                                 expandedFile.string()};
    }

    return parseExpandedFile(expandedFile, jsonFile);
}

void prescan(TaskList &list) {
    createDirectories(list);

    std::vector<std::pair<Task *, std::string>> connections;

    for (auto &task : list) {
        if (auto t = getType(task->out());
            t == SourceType::ExpandedModuleSource) {
            auto prescanResult = prescan(*task);

            auto pcm = task->parent();

            pcm->name(prescanResult.name);

            for (auto &in : prescanResult.imports) {
                connections.push_back({pcm, in});
            }
        }
    }

    //! All files must be prescanned before this can happend
    for (auto &pair : connections) {
        auto inTask = list.find("@" + pair.second);
        pair.first->pushIn(inTask);
    }
}