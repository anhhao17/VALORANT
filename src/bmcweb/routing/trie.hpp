#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "../logging.hpp"

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
        LOG_DEBUG("Added route pattern: {} with index: {}", pattern, ruleIndex);
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

        LOG_DEBUG("Trie::find() called with URL: {}", url);

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
            LOG_DEBUG("Exact match found for URL: {}", url);
            return {exactMatch->ruleIndices, params};
        }

        // Try wildcard matching - handle wildcards in the middle of patterns
        current = root_;
        std::string prefix;
        
        for (size_t i = 0; i < url.length(); i++)
        {
            char c = url[i];
            
            // Check if current node has a wildcard child (for patterns like "/api/streams/*/start")
            if (current->children.find('*') != current->children.end())
            {
                auto wildcardNode = current->children['*'];
                
                // Try to match the rest of the pattern after the wildcard
                std::string remainingUrl = url.substr(i);
                std::string wildcardParam;
                
                // Extract the wildcard parameter (everything until next '/' or end)
                size_t paramEnd = remainingUrl.find('/');
                if (paramEnd != std::string::npos)
                {
                    wildcardParam = remainingUrl.substr(0, paramEnd);
                    std::string afterWildcard = remainingUrl.substr(paramEnd);
                    
                    // Try to match the rest of the pattern after the wildcard
                    auto matchAfterWildcard = wildcardNode;
                    bool matchFound = true;
                    
                    for (char wc : afterWildcard)
                    {
                        if (matchAfterWildcard->children.find(wc) == matchAfterWildcard->children.end())
                        {
                            matchFound = false;
                            break;
                        }
                        matchAfterWildcard = matchAfterWildcard->children[wc];
                    }
                    
                    if (matchFound && matchAfterWildcard->isEnd)
                    {
                        params.push_back(wildcardParam);
                        LOG_DEBUG("Wildcard match found with param: {}, remaining: {}", wildcardParam, afterWildcard);
                        return {matchAfterWildcard->ruleIndices, params};
                    }
                }
                else
                {
                    // Wildcard at end of pattern
                    if (wildcardNode->isEnd)
                    {
                        params.push_back(remainingUrl);
                        LOG_DEBUG("Wildcard match at end with param: {}", remainingUrl);
                        return {wildcardNode->ruleIndices, params};
                    }
                }
            }
            
            if (current->children.find(c) == current->children.end())
            {
                LOG_DEBUG("No match found for URL: {}", url);
                return {{}, params};  // No match
            }
            current = current->children[c];
            prefix += c;
        }

        // Check if we're at an endpoint
        if (current->isEnd)
        {
            LOG_DEBUG("Endpoint found at end of URL traversal");
            return {current->ruleIndices, params};
        }

        LOG_DEBUG("No match found for URL: {}", url);
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