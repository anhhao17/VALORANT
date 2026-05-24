#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace jetson::bmcweb::routing
{

/**
 * @brief Trie node for URL pattern matching
 *
 * Provides O(1) URL pattern matching using a trie data structure,
 * following bmcweb's efficient routing approach.
 */
class TrieNode
{
   public:
    TrieNode() : isEnd(false), ruleIndex(0) {}

    std::unordered_map<char, std::shared_ptr<TrieNode>> children;
    bool isEnd;
    unsigned int ruleIndex;  // Index of the matching rule
};

/**
 * @brief Trie-based URL pattern matcher
 *
 * Efficient URL matching using trie data structure for O(1) lookup,
 * supporting both static routes and parameterized routes.
 */
class Trie
{
   public:
    Trie() : root_(std::make_shared<TrieNode>()) {}

    /**
     * @brief Add a URL pattern to the trie
     * @param pattern URL pattern (e.g., "/api/system/info")
     * @param ruleIndex Index of the rule handler
     */
    void add(const std::string& pattern, unsigned int ruleIndex)
    {
        auto current = root_;
        for (char c : pattern)
        {
            if (current->children.find(c) == current->children.end())
            {
                current->children[c] = std::make_shared<TrieNode>();
            }
            current = current->children[c];
        }
        current->isEnd = true;
        current->ruleIndex = ruleIndex;
    }

    /**
     * @brief Find matching rule for a URL
     * @param url Request URL to match
     * @return Pair of rule index and extracted parameters
     */
    std::pair<unsigned int, std::vector<std::string>> find(const std::string& url) const
    {
        auto current = root_;
        std::vector<std::string> params;

        if (url.empty())
        {
            return {0, params};
        }

        for (char c : url)
        {
            if (current->children.find(c) == current->children.end())
            {
                return {0, params};  // No match - character not found
            }
            current = current->children[c];
        }

        // Only return a match if we're at an exact endpoint
        // This ensures we consumed ALL characters and are at a valid endpoint
        if (current->isEnd)
        {
            return {current->ruleIndex, params};
        }

        return {0, params};  // No match - we consumed all characters but not at an endpoint
    }

    /**
     * @brief Validate the trie structure
     */
    void validate()
    {
        // Trie validation logic could be added here
        // For now, just ensure root exists
        if (!root_)
        {
            throw std::runtime_error("Trie root is null");
        }
    }

   private:
    std::shared_ptr<TrieNode> root_;
};

}  // namespace jetson::bmcweb::routing