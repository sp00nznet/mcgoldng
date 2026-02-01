#ifndef MCGNG_FIT_PARSER_H
#define MCGNG_FIT_PARSER_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>

namespace mcgng {

/**
 * FIT (FITini) Configuration File Parser
 *
 * Format: Text-based INI with typed variables
 *
 * File structure:
 *   FITini
 *   [BlockName]
 *   ul variableName = 12345
 *   st stringVar = "value"
 *   b boolVar = TRUE
 *   [AnotherBlock]
 *   ...
 *   FITend
 *
 * Type prefixes:
 *   ul  = unsigned long
 *   l   = long
 *   us  = unsigned short
 *   s   = short
 *   uc  = unsigned char
 *   c   = char
 *   f   = float
 *   b   = bool (TRUE/FALSE)
 *   st  = string (quoted)
 *
 * Arrays: type[count] name = val1, val2, val3, ...
 *   e.g., ul[3] myArray = 1, 2, 3
 */

// Value types that can be stored in FIT files
using FitValue = std::variant<
    int64_t,           // For all integer types (l, ul, s, us, c, uc)
    double,            // For float (f)
    bool,              // For boolean (b)
    std::string,       // For string (st)
    std::vector<int64_t>,   // For integer arrays
    std::vector<double>     // For float arrays
>;

/**
 * Parsed FIT variable entry
 */
struct FitVariable {
    std::string name;
    std::string typePrefix;   // Original type prefix (ul, l, st, etc.)
    FitValue value;
    bool isArray = false;
    size_t arraySize = 0;     // For arrays
};

/**
 * Parsed FIT block (section)
 */
struct FitBlock {
    std::string name;
    std::vector<FitVariable> variables;

    // Convenience accessors
    const FitVariable* findVariable(const std::string& name) const;

    // Type-safe getters
    std::optional<int64_t> getInt(const std::string& name) const;
    std::optional<uint64_t> getUInt(const std::string& name) const;
    std::optional<double> getFloat(const std::string& name) const;
    std::optional<bool> getBool(const std::string& name) const;
    std::optional<std::string> getString(const std::string& name) const;
    std::optional<std::vector<int64_t>> getIntArray(const std::string& name) const;
    std::optional<std::vector<double>> getFloatArray(const std::string& name) const;
};

/**
 * FIT Configuration File Parser
 */
class FitParser {
public:
    static constexpr const char* FIT_HEADER = "FITini";
    static constexpr const char* FIT_FOOTER = "FITend";

    FitParser() = default;

    /**
     * Parse a FIT file from a path.
     * @param path Path to the .cfg or .fit file
     * @return true on success
     */
    bool parseFile(const std::string& path);

    /**
     * Parse FIT data from a buffer.
     * @param data Pointer to file data
     * @param size Size of data
     * @return true on success
     */
    bool parseBuffer(const uint8_t* data, size_t size);

    /**
     * Parse FIT data from a string.
     * @param content File content as string
     * @return true on success
     */
    bool parseString(const std::string& content);

    /**
     * Check if parsing was successful.
     */
    bool isValid() const { return m_valid; }

    /**
     * Get all parsed blocks.
     */
    const std::vector<FitBlock>& getBlocks() const { return m_blocks; }

    /**
     * Get the number of blocks.
     */
    size_t getNumBlocks() const { return m_blocks.size(); }

    /**
     * Find a block by name (case-insensitive).
     * @param name Block name
     * @return Pointer to block or nullptr
     */
    const FitBlock* findBlock(const std::string& name) const;

    /**
     * Get last error message.
     */
    const std::string& getError() const { return m_errorMessage; }

    /**
     * Clear parsed data.
     */
    void clear();

    /**
     * Case-insensitive string comparison utility.
     */
    static bool iequals(const std::string& a, const std::string& b);

private:
    std::vector<FitBlock> m_blocks;
    bool m_valid = false;
    std::string m_errorMessage;

    bool parseLine(const std::string& line, FitBlock* currentBlock);
    bool parseVariable(const std::string& line, FitVariable& var);
    bool parseValue(const std::string& typePrefix, const std::string& valueStr,
                    FitVariable& var);

    static std::string trim(const std::string& str);
};

} // namespace mcgng

#endif // MCGNG_FIT_PARSER_H
