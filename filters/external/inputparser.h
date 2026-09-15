#pragma once

#include <vector>
#include <string>
#include <algorithm>

class InputParser {
public:
    InputParser() = delete;
    InputParser(std::vector<std::string> args) {
        _tokens = std::move(args);
    }
    /*
    void PrintArgs() const {
        std::cout << "parameters: " << _tokens.size() << '\n';
        for (auto const& arg : _tokens) {
            std::cout << "parameter: " << arg << '\n';
        }
    }
    */
    const auto& getCmdOption(const std::string_view option) const {
        auto itr = std::find(_tokens.begin(), _tokens.end(), option);
        if (itr != _tokens.end() && ++itr != _tokens.end()) {
            return *itr;
        }
        static const std::string empty_string("");
        return empty_string;
    }
    const auto& getThisOption() const {
        return getCmdOption(_option);
    }
    bool cmdOptionExists(const std::string_view option) const {
        const auto res = (std::find(_tokens.begin(), _tokens.end(), option) != _tokens.end());
        if (res)
            _option = option;
        return res;
    }
private:
    mutable std::string _option;
    std::vector <std::string> _tokens;
};