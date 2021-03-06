#include "tasklist.h"
#include "parsedepfile.h"
#include "json/json.h"
#include <iostream>

namespace {

void connectTasks(TaskList &list, const Json &json) {
    for (size_t i = 0; i < json.size(); ++i) {
        auto &task = list.at(i);
        auto &data = json.at(i);

        if (auto f = data.find("in"); f != data.end()) {
            if (f->type == Json::Array) {
                for (auto &value : *f) {
                    auto ftask = list.find(value.string());
                    if (ftask) {
                        task.pushIn(ftask);
                    }
                    else {
                        throw std::runtime_error{"could not find task " +
                                                 value.string()};
                    }
                }
            }
            else {
                task.pushIn(list.find(f->value));
            }
        }

        if (auto f = data.find("deps"); f != data.end()) {
            if (f->type == Json::Array) {
                for (auto &value : *f) {
                    auto ftask = list.find(value.string());
                    if (ftask) {
                        //                        task.pushTrigger(ftask);
                        task.pushIn(ftask);
                    }
                    else {
                        throw std::runtime_error{"could not find task " +
                                                 value.string()};
                    }
                }
            }
            else {
                task.pushIn(list.find(f->value));
            }
        }
    }
}

} // namespace

void calculateState(TaskList &list) {
    for (auto &task : list) {
        auto depfile = task->depfile();
        if (depfile.empty()) {
            continue;
        }
        for (auto &f : parseDepFile(depfile).deps) {
            task->pushIn(list.find(f.string()));
        }
    }

    for (auto &task : list) {
        task->updateState();
    }

    for (auto &task : list) {
        task->pruneTriggers();
    }
}

std::unique_ptr<TaskList> parseTasks(filesystem::path path) {
    auto list = std::make_unique<TaskList>();
    auto json = Json{};
    auto file = std::ifstream{path};

    if (!file.is_open()) {
        throw std::runtime_error{"could not load tasks from " + path.string()};
    }
    file >> json;

    if (json.type != Json::Array) {
        throw std::runtime_error{"expected array in " + path.string()};
    }

    list->reserve(json.size());

    for (const auto &jtask : json) {
        list->emplace().parse(jtask);
    }

    connectTasks(*list, json);

    calculateState(*list);

    return list;
}

void printFlat(const TaskList &list) {
    for (auto &task : list) {
        std::cout << "task: name = " << task->name() << "\n";
        std::cout << "  out = " << task->out() << "\n";
        if (task->parent()) {
            std::cout << "  parent = " << task->parent()->out() << "\n";
        }

        {
            auto &in = task->in();
            for (auto &i : in) {
                std::cout << "  in: " << i->out() << "\n";
            }
        }
    }
}

void createDirectories(const TaskList &tasks) {

    auto directories = std::map<filesystem::path, int>{};

    for (auto &task : tasks) {
        ++directories[task->out().parent_path()];
    }

    for (auto &it : directories) {
        if (it.first.empty()) {
            continue;
        }
        else if (!filesystem::exists(it.first)) {
            filesystem::create_directories(it.first);
        }
        else if (!filesystem::is_directory(it.first)) {
            throw std::runtime_error{"expected " + it.first.string() +
                                     " to be a directory "
                                     "but it is a file"};
        }
    }
}
