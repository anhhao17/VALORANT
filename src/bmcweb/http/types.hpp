#pragma once

#include <boost/beast/http.hpp>
#include <string>

namespace jetson::bmcweb
{

namespace http = boost::beast::http;

// HTTP type aliases for convenience
using Verb = http::verb;
using status = http::status;

} // namespace jetson::bmcweb