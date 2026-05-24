#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace embed::bmcweb::routing
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
    TrieNode() : isEnd(false) {}

    std::unordered_map<char, std::shared_ptr<TrieNode>> children;
    bool isEnd;
    std::vector<unsigned int> ruleIndices;  // Indices of matching rules (for different HTTP methods)
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
        current->ruleIndices.push_back(ruleIndex);
    }

    /**
     * @brief Find matching rule for a URL
     * @param url Request URL to match
     * @return Pair of rule indices and extracted parameters
     */
    std::pair<std::vector<unsigned int>, std::vector<std::string>> find(const std::string& url) const
    {
        auto current = root_;
        std::vector<std::string> params;

        if (url.empty())
        {
            return {{}, params};
        }

        // Try exact match first
        auto exactMatch = root_;
        bool exactMatchFound = true;
        
        for (char c : url)
        {
            if (exactMatch->children.find(c) == exactMatch->children.end())
            {
                exactMatchFound = false;
                break;
            }
            exactMatch = exactMatch->children[c];
        }

        if (exactMatchFound && exactMatch->isEnd)
        {
            return {exactMatch->ruleIndices, params};
        }

        // Try wildcard matching
        current = root_;
        std::string prefix;
        
        for (size_t i = 0; i < url.length(); i++)
        {
            char c = url[i];
            if (current->children.find(c) == current->children.end())
            {
                // Check if current node has a wildcard child
                if (current->children.find('*') != current->children.end())
                {
                    // Found wildcard, match the rest
                    current = current->children['*'];
                    params.push_back(url.substr(i));
                    if (current->isEnd)
                    {
                        return {current->ruleIndices, params};
                    }
                }
                return {{}, params};  // No match
            }
            current = current->children[c];
            prefix += c;
            
            // Check if current node has a wildcard child (for patterns like "/api/users/*")
            if (current->children.find('*') != current->children.end())
            {
                auto wildcardNode = current->children['*'];
                if (wildcardNode->isEnd && i < url.length() - 1)
                {
                    // Wildcard matches the rest of the URL
                    params.push_back(url.substr(i + 1));
                    return {wildcardNode->ruleIndices, params};
                }
            }
        }

        // Check if we're at an endpoint
        if (current->isEnd)
        {
            return {current->ruleIndices, params};
        }

        return {{}, params};  // No match
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

}  // namespace embed::bmcweb::routing