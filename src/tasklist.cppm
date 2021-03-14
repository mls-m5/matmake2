
#include "filesystem.h"
#include "parsedepfile.cppm"
#include "task.cppm"
#include "json/json.h"
#include <iostream>
#include <memory>
#include <vector>

export module tasklist;

struct TaskList;

inline void createDirectories(const TaskList &tasks);

inline void calculateState(TaskList &list);

inline std::unique_ptr<TaskList> parseTasks(filesystem::path path);

inline void printFlat(const TaskList &list);

struct TaskList {

    TaskList() = default;
    TaskList(const TaskList &) = delete;
    TaskList &operator=(const TaskList &) = delete;
    TaskList(TaskList &&) = default;
    TaskList &operator=(TaskList &&) = default;

    std::vector<std::unique_ptr<Task>> _tasks;

    void reserve(size_t size) {
        _tasks.reserve(size);
    }

    Task &emplace() {
        _tasks.emplace_back(std::make_unique<Task>());
        return *_tasks.back();
    }

    void insert(TaskList tasks) {
        _tasks.reserve(_tasks.size() + tasks._tasks.size());

        for (auto &task : tasks._tasks) {
            _tasks.push_back(std::move(task));
        }

        tasks.clear();
    }

    Task *find(std::string name) {
        if (name.empty()) {
            return nullptr;
        }
        if (name.front() == '@') {
            name = name.substr(1);
            for (auto &t : _tasks) {
                if (t->name() == name) {
                    return t.get();
                }
            }
        }
        else if (name.rfind("./") == 0) {
            for (auto &t : _tasks) {
                if (t->rawOut() == name) {
                    return t.get();
                }
            }
        }
        else {
            for (auto &t : _tasks) {
                if (t->out() == name) {
                    return t.get();
                }
            }
        }

        return nullptr;
    }

    void clear() {
        _tasks.clear();
    }

    Task &at(size_t i) {
        return *_tasks.at(i);
    }

    auto begin() {
        return _tasks.begin();
    }

    auto end() {
        return _tasks.end();
    }

    auto begin() const {
        return _tasks.begin();
    }

    auto end() const {
        return _tasks.end();
    }

    auto &front() {
        return *_tasks.front();
    }

    auto &back() {
        return *_tasks.back();
    }

    bool empty() {
        return _tasks.empty();
    }
};

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
