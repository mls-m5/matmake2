#pragma once

#include "os.h"
#include "translateconfig.h"
#include <map>
#include <string>

enum class FlagStyle {
    Inherit,
    Gcc,
    Msvc,
};

enum class TranslatableString {
    ExeExtension,
    ObjectExtension, // eg. ".o" or ".obj"
    IncludeModuleString,
};

namespace {

const std::map<std::string, std::string> gccConfigs = {
    {"debug", "-g"},
    {"modules", "-fmodules-ts"},
    {"thread", "-pthread"},
};
const std::map<std::string, std::string> msvcConfigs = {
    {"debug", "/DEBUG"},
    {"modules", "/std:latest"},
    {"thread", "/MD"}, // Use multithreaded standard library
};

const std::map<std::string, std::string> gccExtensions = {
    {".exe", ""},
    {".so", ".so"},
    {".a", ".a"},
    {".o", ".o"},
};

const std::map<std::string, std::string> msvcExtensions = {
    {".exe", ".exe"},
    {".so", ".dll"},
    {".a", ".lib"},
    {".o", ".obj"},
};

const std::map<std::string, std::string> gccCmdToExt = {
    {"exe", ".exe"},
    {"so", ".so"},
    {"static", ".a"},
};

const std::map<std::string, std::string> msvcCmdToExt = {
    {"exe", ".exe"},
    {"so", ".dll"},
    {"static", ".lib"},
};

//! Strings starting with "c++"
std::string translateStandard(std::string config, FlagStyle style) {
    switch (style) {
    case FlagStyle::Msvc:
        return "/std:" + config;
    default: // Gcc
        return "-std=" + config;
    }
}

std::string osExeExtension() {
    if constexpr (getOs() == Os::Windows) {
        return ".exe";
    }
    else {
        return {};
    }
}

} // namespace

inline std::string translateConfig(std::string config, FlagStyle style) {
    if (config.rfind("c++") != std::string::npos) {
        return translateStandard(config, style);
    }

    switch (style) {
    case FlagStyle::Msvc:
        if (auto f = msvcConfigs.find(config); f != msvcConfigs.end()) {
            return f->second;
        }
        break;
    default:
        if (auto f = gccConfigs.find(config); f != gccConfigs.end()) {
            return f->second;
        }
    }
    return {};
}

inline std::string includePrefix(FlagStyle style) {
    switch (style) {
    case FlagStyle::Msvc:
        return "/I";
    default:
        return "-I";
    }
}

inline std::string translateString(TranslatableString name, FlagStyle style) {
    if (style == FlagStyle::Msvc) {
        switch (name) {
        case TranslatableString::ExeExtension:
            return ".exe";
        case TranslatableString::ObjectExtension:
            return ".obj";
        case TranslatableString::IncludeModuleString:
            return "/module:reference ";
        }
    }
    else {
        switch (name) {
        case TranslatableString::ExeExtension:
            return "";
        case TranslatableString::ObjectExtension:
            return ".o";
        case TranslatableString::IncludeModuleString:
            return "-fmodule-file=";
        }
    }

    return {};
}

inline std::string extension(std::string ext, FlagStyle style) {
    if (ext.empty()) {
        return {};
    }

    if (ext.front() != '.') {
        ext.insert(ext.begin(), '.');
    }

    if (ext == ".exe") {
        return translateString(TranslatableString::ExeExtension, style);
    }

    switch (style) {
    case FlagStyle::Msvc:
        if (auto f = msvcExtensions.find(ext); f != msvcExtensions.end()) {
            return f->second;
        }
        break;
    default:
        if (auto f = gccExtensions.find(ext); f != gccExtensions.end()) {
            return f->second;
        }
        break;
    }
    return ext;
}

inline std::string extensionFromCommandType(std::string command,
                                            FlagStyle style) {
    if (command.empty()) {
        return {};
    }

    if (command.front() == '[' && command.back() == ']') {
        command = command.substr(1, command.size() - 2);
    }

    if (command == "exe" || command == "test") {
        if (style == FlagStyle::Msvc) {
            // Guessing that if you run msvc on linux you do it through wine
            return ".exe";
        }
        return osExeExtension();
    }
    switch (style) {
    case FlagStyle::Msvc:
        if (auto f = msvcCmdToExt.find(command); f != msvcCmdToExt.end()) {
            return f->second;
        }
        break;
    default:
        if (auto f = gccCmdToExt.find(command); f != gccCmdToExt.end()) {
            return f->second;
        }
        break;
    }
    return {};
}
