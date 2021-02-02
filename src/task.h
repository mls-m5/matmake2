﻿#pragma once

#include "filesystem.h"
#include "processedcommand.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Task {
    using TimePoint = filesystem::file_time_type;

    Task() = default;
    Task(const class Json &json) {
        parse(json);
    }
    Task(const Task &) = delete;
    Task(Task &&) = default;
    Task &operator=(const Task &) = delete;
    Task &operator=(Task &&) = default;

    std::string name() const {
        if (_name.empty()) {
            return _out.string();
        }
        else {
            return _name;
        }
    }

    void name(std::string value) {
        _name = std::move(value);
    }

    filesystem::path out() const {
        if (_out.empty()) {
            return {};
        }
        else if (*_out.begin() == ".") {
            return _out;
        }
        else {
            return dir() / _out;
        }
    }

    filesystem::path rawOut() const {
        return _out;
    }

    void out(filesystem::path path) {
        _out = path;
    }

    void pushIn(Task *in) {
        if (!in) {
            return;
        }
        if (std::find(_in.begin(), _in.end(), in) == _in.end()) {
            _in.push_back(in);
            in->parent(this);
        }
    }

    auto &in() {
        return _in;
    }

    auto &in() const {
        return _in;
    }

    std::string concatIn() const {
        if (_in.empty()) {
            return {};
        }

        std::string ret;

        for (auto &in : _in) {
            ret += in->out().string() + " ";
        }

        return ret;
    }

    void parent(Task *parent) {
        _parent = parent;
    }

    auto parent() {
        return _parent;
    }

    auto parent() const {
        return _parent;
    }

    void dir(filesystem::path dir) {
        _dir = dir;
    }

    filesystem::path dir() const {
        if (_parent) {
            return _parent->dir() / _dir; // Fix double slashes somhow
        }
        else {
            return _dir;
        }
    }

    std::string property(std::string name) const {
        if (name == "command") {
            return _command;
        }
        else if (name == "out") {
            return out().string();
        }
        else if (name == "dir") {
            return dir().string();
        }
        else if (name == "depfile") {
            return depfile().string();
        }
        else if (name == "in") {
            return concatIn();
        }
        else if (name == "c++") {
            return cxx().string();
        }
        return {};
    }

    void depfile(filesystem::path path) {
        _depfile = path;
    }

    filesystem::path depfile() const {
        return _depfile;
    }

    void command(std::string command) {
        _command = command;
    }

    std::string command() const {
        if (_command.empty()) {
            if (_parent) {
                return _parent->command();
            }
        }
        else {
            if (_command.front() == '[' && _command.back() == ']') {
                return commandAt(_command.substr(1, _command.size() - 2));
            }
            else {
                return _command;
            }
        }
        throw std::runtime_error{"no command specified for target " + name()};
    }

    void commands(std::map<std::string, std::string> commands) {
        _commands = std::move(commands);
    }

    const std::map<std::string, std::string> &commands() const {
        if (_commands.empty() && _parent) {
            return _parent->commands();
        }
        return _commands;
    }

    // Get a single command from the commands-list
    std::string commandAt(std::string name) const {
        if (auto f = _commands.find(name); f != _commands.end()) {
            return f->second;
        }
        else if (_parent) {
            return _parent->commandAt(name);
        }
        else {
            throw std::runtime_error{"could not find " + name + " on target " +
                                     this->name()};
        }
    }

    void cxx(filesystem::path cxx) {
        _cxx = cxx;
    }

    filesystem::path cxx() const {
        if (!_cxx.empty()) {
            return _cxx;
        }
        else if (_parent) {
            return _parent->cxx();
        }
        else {
            return {};
        }
    }

    TimePoint changedTime() {
        return _changedTime;
    }

    void parse(const class Json &jtask);

    void updateState() {
        _changedTime = filesystem::last_write_time(out());
    }

    Json dump();

    //! Print tree view from node
    void print(bool verbose = false, size_t indentation = 0);

private:
    Task *_parent = nullptr;
    filesystem::path _out;
    filesystem::path _dir;
    filesystem::path _depfile;
    filesystem::path _cxx;
    std::string _name;                            // If empty-same as out
    std::string _command;                         // If empty use parents
    std::map<std::string, std::string> _commands; // Parents build command

    // Fixed during connection step
    std::vector<Task *> _in; // Files that needs to be built before this file

    // Others used to calculate state
    size_t waiting = 0;
    std::vector<Task *> _triggers; // Files that mark this task as dirty
    std::vector<Task *> _subscribers;
    TimePoint _changedTime;
};
