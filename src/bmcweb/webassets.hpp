#pragma once

#include <algorithm>
#include <array>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "app.hpp"
#include "async_resp.hpp"
#include "http/types.hpp"
#include "logging.hpp"

namespace embed::bmcweb
{

namespace webassets
{

static constexpr std::string_view rootpath("webui/dist");

struct StaticFile
{
    std::filesystem::path absolutePath;
    std::string_view contentType;
    std::string etag;
    bool renamed = false;
};

inline std::string getStaticEtag(const std::filesystem::path& webpath)
{
    // Build tools output production chunks in the form:
    // <filename>.<hash>.<extension>
    // Vite example: app.DhhjLIym.js (8 alphanumeric chars)
    // Try to detect this, so we can use the hash as the ETAG
    std::string filename = webpath.filename().string();
    size_t lastDot = filename.rfind('.');
    if (lastDot == std::string::npos || lastDot == 0)
    {
        return "";
    }

    size_t secondLastDot = filename.rfind('.', lastDot - 1);
    if (secondLastDot == std::string::npos)
    {
        return "";
    }

    std::string hash = filename.substr(secondLastDot + 1, lastDot - secondLastDot - 1);

    // Build tool hashes are 8 characters long
    if (hash.size() != 8)
    {
        return "";
    }

    // Build tool hashes only include alphanumeric characters
    if (hash.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") !=
        std::string::npos)
    {
        return "";
    }

    return "\"" + hash + "\"";
}

inline std::string_view getFiletypeForExtension(std::string_view extension)
{
    constexpr static std::array<std::pair<std::string_view, std::string_view>, 17> contentTypes{
        {{".css", "text/css;charset=UTF-8"},
         {".eot", "application/vnd.ms-fontobject"},
         {".gif", "image/gif"},
         {".html", "text/html;charset=UTF-8"},
         {".ico", "image/x-icon"},
         {".jpeg", "image/jpeg"},
         {".jpg", "image/jpeg"},
         {".js", "application/javascript;charset=UTF-8"},
         {".json", "application/json"},
         {".map", "application/json"},
         {".png", "image/png;charset=UTF-8"},
         {".svg", "image/svg+xml"},
         {".ttf", "application/x-font-ttf"},
         {".woff", "application/x-font-woff"},
         {".woff2", "application/x-font-woff2"},
         {".xml", "application/xml"}}};

    const auto* contentType = std::ranges::find_if(
        contentTypes, [&extension](const auto& val) { return val.first == extension; });

    if (contentType == contentTypes.end())
    {
        LOG_ERROR("Cannot determine content-type for file with extension {}", extension);
        return "";
    }

    return contentType->second;
}

inline void handleStaticAsset(
    const Request& req, const std::shared_ptr<AsyncResp>& asyncResp, const StaticFile& file)
{
    LOG_DEBUG(
        "Serving static file: {} (content-type: {})", file.absolutePath.string(),
        std::string(file.contentType));

    if (!file.contentType.empty())
    {
        asyncResp->res.set(http::field::content_type, std::string(file.contentType));
    }

    if (!file.etag.empty())
    {
        asyncResp->res.set(http::field::etag, file.etag);

        // Don't cache paths that don't have the etag in them, like
        // index, which gets transformed to /
        if (!file.renamed)
        {
            // Anything with a hash can be cached forever and is immutable
            asyncResp->res.set(http::field::cache_control, "max-age=31556926, immutable");
        }

        std::string cachedEtag = req.getHeaderValue(http::field::if_none_match);
        if (cachedEtag == file.etag)
        {
            asyncResp->res.result(http::status::not_modified);
            return;
        }
    }

    // Read file content
    std::ifstream fileStream(file.absolutePath.string(), std::ios::binary);
    if (!fileStream)
    {
        LOG_ERROR("Failed to open file: {}", file.absolutePath.string());
        asyncResp->res.result(http::status::internal_server_error);
        return;
    }

    std::string content(
        (std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    fileStream.close();

    LOG_DEBUG("File content size: {} bytes", content.length());
    asyncResp->res.body(content);
    asyncResp->res.result(http::status::ok);
}

inline void addFile(App& app, const std::filesystem::directory_entry& dir)
{
    StaticFile file;
    file.absolutePath = dir.path();

    std::string absPathStr = file.absolutePath.string();
    std::string rootPathStr(rootpath);

    if (absPathStr.size() < rootPathStr.size())
    {
        LOG_ERROR("File path is shorter than root path: {}", absPathStr);
        return;
    }

    // Calculate relative path by removing root path
    std::string relativePathStr = absPathStr.substr(rootPathStr.size());
    std::filesystem::path relativePath(relativePathStr);
    std::string extension = relativePath.extension().string();
    std::filesystem::path webpath = relativePath;

    file.etag = getStaticEtag(webpath);

    // Don't map index.html to root path to avoid trie conflicts
    // Keep it as /index.html instead
    if (webpath.filename().string().starts_with("index.") && extension == ".html")
    {
        // Keep the original path, don't remap to "/"
        // This avoids conflicts with "/assets/..." routes
    }

    file.contentType = getFiletypeForExtension(extension);

    // Register route for this file
    std::string routePath = webpath.string();
    LOG_INFO(
        "Registering static route: '{}' -> '{}' (content-type: '{}')", routePath,
        file.absolutePath.string(), std::string(file.contentType));
    app.route<>(routePath).setHandler(
        [file = std::move(file)](const Request& req, const std::shared_ptr<AsyncResp>& asyncResp) {
            handleStaticAsset(req, asyncResp, file);
        });
}

inline void requestRoutes(App& app)
{
    std::error_code ec;
    std::string rootPathStr(rootpath);
    std::filesystem::path rootPath(rootPathStr);

    if (!std::filesystem::exists(rootPath))
    {
        LOG_ERROR(
            "WebUI directory {} does not exist, static file hosting disabled", rootPath.string());
        return;
    }

    std::filesystem::recursive_directory_iterator dirIter(rootPath, ec);
    if (ec)
    {
        LOG_ERROR("Unable to open {} static file hosting disabled: {}", rootPathStr, ec.message());
        return;
    }

    // Collect all files
    std::vector<std::filesystem::directory_entry> paths(
        std::filesystem::begin(dirIter), std::filesystem::end(dirIter));

    // Sort by path length (longest first) to avoid trie conflicts
    std::ranges::sort(
        paths,
        [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
            return a.path().string().length() > b.path().string().length();
        });

    for (const std::filesystem::directory_entry& dir : paths)
    {
        if (std::filesystem::is_directory(dir))
        {
            // Skip hidden directories
            if (dir.path().filename().string().starts_with("."))
            {
                dirIter.disable_recursion_pending();
            }
        }
        else if (std::filesystem::is_regular_file(dir))
        {
            addFile(app, dir);
        }
    }

    // Add fallback route for "/" to serve index.html
    std::filesystem::path indexPath = rootPath / "index.html";
    if (std::filesystem::exists(indexPath))
    {
        StaticFile file;
        file.absolutePath = indexPath;
        file.contentType = "text/html;charset=UTF-8";
        file.etag = getStaticEtag(indexPath);
        file.renamed = true;

        LOG_INFO(
            "Registering fallback route: '/' -> '{}' (content-type: '{}')",
            file.absolutePath.string(), std::string(file.contentType));
        app.route<>("/").setHandler(
            [file = std::move(file)](
                const Request& req, const std::shared_ptr<AsyncResp>& asyncResp) {
                handleStaticAsset(req, asyncResp, file);
            });
    }

    LOG_INFO("Static file hosting enabled from: {}", rootPathStr);
}

}  // namespace webassets

}  // namespace embed::bmcweb