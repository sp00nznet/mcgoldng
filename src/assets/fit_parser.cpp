#include "assets/fit_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>

namespace mcgng {

// FitBlock implementation

const FitVariable* FitBlock::findVariable(const std::string& varName) const {
    for (const auto& var : variables) {
        if (FitParser::iequals(var.name, varName)) {
            return &var;
        }
    }
    return nullptr;
}

std::optional<int64_t> FitBlock::getInt(const std::string& varName) const {
    const auto* var = findVariable(varName);
    if (var && std::holds_alternative<int64_t>(var->value)) {
        return std::get<int64_t>(var->value);
    }
    return std::nullopt;
}

std::optional<uint64_t> FitBlock::getUInt(const std::string& varName) const {
    const auto* var = findVariable(varName);
    if (var && std::holds_alternative<int64_t>(var->value)) {
        return static_cast<uint64_t>(std::get<int64_t>(var->value));
    }
    return std::nullopt;
}

std::optional<double> FitBlock::getFloat(const std::string& varName) const {
    const auto* var = findVariable(varName);
    if (var && std::holds_alternative<double>(var->value)) {
        return std::get<double>(var->value);
    }
    return std::nullopt;
}

std::optional<bool> FitBlock::getBool(const std::string& varName) const {
    const auto* var = findVariable(varName);
    if (var && std::holds_alternative<bool>(var->value)) {
        return std::get<bool>(var->value);
    }
    return std::nullopt;
}

std::optional<std::string> FitBlock::getString(const std::string& varName) const {
    const auto* var = findVariable(varName);
    if (var && std::holds_alternative<std::string>(var->value)) {
        return std::get<std::string>(var->value);
    }
    return std::nullopt;
}

std::optional<std::vector<int64_t>> FitBlock::getIntArray(const std::string& varName) const {
    const auto* var = findVariable(varName);
    if (var && std::holds_alternative<std::vector<int64_t>>(var->value)) {
        return std::get<std::vector<int64_t>>(var->value);
    }
    return std::nullopt;
}

std::optional<std::vector<double>> FitBlock::getFloatArray(const std::string& varName) const {
    const auto* var = findVariable(varName);
    if (var && std::holds_alternative<std::vector<double>>(var->value)) {
        return std::get<std::vector<double>>(var->value);
    }
    return std::nullopt;
}

// FitParser implementation

std::string FitParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

bool FitParser::iequals(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) {
        return false;
    }
    for (size_t i = 0; i < a.length(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

void FitParser::clear() {
    m_blocks.clear();
    m_valid = false;
    m_errorMessage.clear();
}

bool FitParser::parseFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        m_errorMessage = "Failed to open file: " + path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseString(buffer.str());
}

bool FitParser::parseBuffer(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        m_errorMessage = "Empty buffer";
        return false;
    }
    return parseString(std::string(reinterpret_cast<const char*>(data), size));
}

bool FitParser::parseString(const std::string& content) {
    clear();

    std::istringstream stream(content);
    std::string line;
    FitBlock* currentBlock = nullptr;
    bool foundHeader = false;
    bool foundFooter = false;
    int lineNumber = 0;

    while (std::getline(stream, line)) {
        ++lineNumber;

        // Handle both \r\n and \n line endings
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '/' || trimmed[0] == ';') {
            continue;
        }

        // Check for header
        if (!foundHeader) {
            if (trimmed.find(FIT_HEADER) != std::string::npos) {
                foundHeader = true;
            }
            continue;
        }

        // Check for footer
        if (trimmed.find(FIT_FOOTER) != std::string::npos) {
            foundFooter = true;
            break;
        }

        // Check for block start
        if (trimmed[0] == '[') {
            size_t end = trimmed.find(']');
            if (end == std::string::npos) {
                m_errorMessage = "Invalid block header at line " + std::to_string(lineNumber);
                return false;
            }

            FitBlock block;
            block.name = trimmed.substr(1, end - 1);
            m_blocks.push_back(std::move(block));
            currentBlock = &m_blocks.back();
            continue;
        }

        // Parse variable
        if (currentBlock && !parseLine(trimmed, currentBlock)) {
            // Non-fatal warning for unparseable lines
            // Some files have malformed entries that should be skipped
        }
    }

    if (!foundHeader) {
        m_errorMessage = "FITini header not found";
        return false;
    }

    m_valid = true;
    return true;
}

bool FitParser::parseLine(const std::string& line, FitBlock* currentBlock) {
    if (!currentBlock) {
        return false;
    }

    FitVariable var;
    if (parseVariable(line, var)) {
        currentBlock->variables.push_back(std::move(var));
        return true;
    }

    return false;
}

bool FitParser::parseVariable(const std::string& line, FitVariable& var) {
    // Pattern: TYPE[COUNT]? NAME = VALUE
    // Examples:
    //   ul myVar = 12345
    //   st myString = "hello"
    //   f[3] myArray = 1.0, 2.0, 3.0

    // Find the equals sign
    size_t equalsPos = line.find('=');
    if (equalsPos == std::string::npos) {
        return false;
    }

    std::string leftSide = trim(line.substr(0, equalsPos));
    std::string valueStr = trim(line.substr(equalsPos + 1));

    // Parse left side: TYPE[COUNT]? NAME
    std::istringstream leftStream(leftSide);
    std::string typeWithArray;
    leftStream >> typeWithArray;

    // Check if this is an array type
    size_t bracketOpen = typeWithArray.find('[');
    if (bracketOpen != std::string::npos) {
        size_t bracketClose = typeWithArray.find(']', bracketOpen);
        if (bracketClose != std::string::npos) {
            var.typePrefix = typeWithArray.substr(0, bracketOpen);
            std::string countStr = typeWithArray.substr(bracketOpen + 1, bracketClose - bracketOpen - 1);
            var.arraySize = std::stoul(countStr);
            var.isArray = true;
        } else {
            return false;
        }
    } else {
        var.typePrefix = typeWithArray;
        var.isArray = false;
        var.arraySize = 0;
    }

    // Get the variable name (rest of left side after type)
    std::string remaining;
    std::getline(leftStream, remaining);
    var.name = trim(remaining);

    if (var.name.empty()) {
        return false;
    }

    // Parse the value based on type
    return parseValue(var.typePrefix, valueStr, var);
}

bool FitParser::parseValue(const std::string& typePrefix, const std::string& valueStr,
                           FitVariable& var) {
    // Handle arrays
    if (var.isArray) {
        if (typePrefix == "f") {
            // Float array
            std::vector<double> values;
            std::string token;
            std::istringstream stream(valueStr);
            while (std::getline(stream, token, ',')) {
                token = trim(token);
                if (!token.empty()) {
                    values.push_back(std::stod(token));
                }
            }
            var.value = values;
            return true;
        } else {
            // Integer array (l, ul, s, us, c, uc)
            std::vector<int64_t> values;
            std::string token;
            std::istringstream stream(valueStr);
            while (std::getline(stream, token, ',')) {
                token = trim(token);
                if (!token.empty()) {
                    // Handle hex values
                    if (token.find("0x") == 0 || token.find("0X") == 0) {
                        values.push_back(std::stoll(token, nullptr, 16));
                    } else {
                        values.push_back(std::stoll(token));
                    }
                }
            }
            var.value = values;
            return true;
        }
    }

    // Handle scalar values
    if (typePrefix == "st") {
        // String - extract quoted content
        size_t firstQuote = valueStr.find('"');
        size_t lastQuote = valueStr.rfind('"');
        if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote != lastQuote) {
            var.value = valueStr.substr(firstQuote + 1, lastQuote - firstQuote - 1);
        } else {
            // Unquoted string - take as-is
            var.value = valueStr;
        }
        return true;
    }

    if (typePrefix == "b") {
        // Boolean
        std::string upper = valueStr;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                      [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        var.value = (upper == "TRUE" || upper == "1" || upper == "YES");
        return true;
    }

    if (typePrefix == "f") {
        // Float
        var.value = std::stod(valueStr);
        return true;
    }

    // Integer types: l, ul, s, us, c, uc
    if (typePrefix == "l" || typePrefix == "ul" ||
        typePrefix == "s" || typePrefix == "us" ||
        typePrefix == "c" || typePrefix == "uc") {
        // Handle hex values
        if (valueStr.find("0x") == 0 || valueStr.find("0X") == 0) {
            var.value = std::stoll(valueStr, nullptr, 16);
        } else {
            var.value = std::stoll(valueStr);
        }
        return true;
    }

    // Unknown type
    m_errorMessage = "Unknown type prefix: " + typePrefix;
    return false;
}

const FitBlock* FitParser::findBlock(const std::string& name) const {
    for (const auto& block : m_blocks) {
        if (iequals(block.name, name)) {
            return &block;
        }
    }
    return nullptr;
}

} // namespace mcgng
