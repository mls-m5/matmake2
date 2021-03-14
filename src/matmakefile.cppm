
#include "defaultfile.cppm"
#include "property.cppm"
#include "json/json.h"
#include <iostream>

export module matmakefile;

// Contains data loaded from json object
class MatmakeNode {

    std::map<std::string, Property> _properties;
    std::string _name;

    std::map<std::string, std::string> _commands;

public:
    MatmakeNode(const Json &json) {
        if (json.type != Json::Object) {
            throw std::runtime_error{"Json: Wrong type when expected object " +
                                     std::string{json.pos}};
        }

        for (auto &child : json) {
            if (child.name == "name") {
                _name = child.value;
            }

            if (child.name == "commands") {
                for (auto &command : child) {
                    _commands[command.name] = command.string();
                }
            }
            else {
                _properties[child.name] = Property{child};
            }
        }
    }

    const Property *property(std::string name) const {
        if (auto f = _properties.find(name); f != _properties.end()) {
            return &f->second;
        }
        else {
            return nullptr;
        }
    }

    const auto &ocommands() const {
        return _commands;
    }

    const std::string &name() const {
        return _name;
    }

    void print(std::ostream &stream) {
        stream << "target: " << _name << "\n";
        for (auto &property : _properties) {
            stream << "  " << property.first << " = " << property.second
                   << "\n";
        }
    }
};

class MatmakeFile {
public:
    MatmakeFile(const Json &json) {
        if (json.type != Json::Array) {
            throw std::runtime_error{"Json: Wrong type when expected array " +
                                     std::string{json.pos}};
        }

        _nodes.reserve(json.size());

        for (auto &j : json) {
            _nodes.emplace_back(j);
        }

        //! Put default targets on end if they do not exist
        auto def = defaultCompiler();

        std::vector<MatmakeNode> defaultNodes;
        defaultNodes.reserve(def.size());

        for (auto &j : def) {
            defaultNodes.emplace_back(j);
        }

        for (auto &n : defaultNodes) {
            if (!find(n.name())) {
                _nodes.emplace_back(std::move(n));
            }
        }
    }

    void print(std::ostream &stream = std::cout) {
        for (auto &child : _nodes) {
            child.print(stream);
        }
    }

    const std::vector<MatmakeNode> nodes() const {
        return _nodes;
    }

    const MatmakeNode *find(std::string name) const {
        for (auto &child : _nodes) {
            if (child.name() == name) {
                return &child;
            }
        }

        return nullptr;
    }

    std::vector<MatmakeNode> _nodes;
};
