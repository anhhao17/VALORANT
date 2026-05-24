#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include "http/HttpTypes.h"
#include "http/Router.h"

using namespace jetson::http;

// Helper: run a coroutine synchronously on a fresh io_context.
template <typename T>
static T sync_await(boost::asio::awaitable<T> aw)
{
    boost::asio::io_context ioc;
    std::optional<T> result;
    std::exception_ptr ex;
    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            try
            {
                result = co_await std::move(aw);
            }
            catch (...)
            {
                ex = std::current_exception();
            }
        },
        boost::asio::detached);
    ioc.run();
    if (ex)
        std::rethrow_exception(ex);
    return std::move(*result);
}

static Request make_request(Verb method, std::string_view target)
{
    Request req;
    req.method(method);
    req.target(target);
    req.version(11);
    return req;
}

// ── Route matching ────────────────────────────────────────────────────────────

TEST(RouterTest, ExactPathMatchReturns200)
{
    Router router;
    router.add_route(Verb::get, "/ping", [](Request) -> boost::asio::awaitable<Response> {
        co_return make_text_response(Status::ok, "pong");
    });

    auto res = sync_await(router.dispatch(make_request(Verb::get, "/ping")));
    EXPECT_EQ(res.result(), Status::ok);
    EXPECT_EQ(res.body(), "pong");
}

TEST(RouterTest, UnmatchedPathReturns404)
{
    Router router;
    auto res = sync_await(router.dispatch(make_request(Verb::get, "/missing")));
    EXPECT_EQ(res.result(), Status::not_found);
}

TEST(RouterTest, MethodMismatchReturns404)
{
    Router router;
    router.add_route(Verb::post, "/data", [](Request) -> boost::asio::awaitable<Response> {
        co_return make_text_response(Status::ok, "ok");
    });

    auto res = sync_await(router.dispatch(make_request(Verb::get, "/data")));
    EXPECT_EQ(res.result(), Status::not_found);
}

TEST(RouterTest, QueryStringStrippedBeforeMatching)
{
    Router router;
    router.add_route(Verb::get, "/health", [](Request) -> boost::asio::awaitable<Response> {
        co_return make_text_response(Status::ok, "healthy");
    });

    auto res = sync_await(router.dispatch(make_request(Verb::get, "/health?debug=1")));
    EXPECT_EQ(res.result(), Status::ok);
}

TEST(RouterTest, WildcardPatternMatches)
{
    Router router;
    router.add_route(Verb::get, "/static/*", [](Request) -> boost::asio::awaitable<Response> {
        co_return make_text_response(Status::ok, "file");
    });

    auto res = sync_await(router.dispatch(make_request(Verb::get, "/static/app.js")));
    EXPECT_EQ(res.result(), Status::ok);
}

// ── Not-found handler ─────────────────────────────────────────────────────────

TEST(RouterTest, CustomNotFoundHandlerCalled)
{
    Router router;
    router.set_not_found_handler([](Request) -> boost::asio::awaitable<Response> {
        co_return make_text_response(Status::ok, "custom-404");
    });

    auto res = sync_await(router.dispatch(make_request(Verb::get, "/anything")));
    EXPECT_EQ(res.result(), Status::ok);
    EXPECT_EQ(res.body(), "custom-404");
}

// ── Middleware ────────────────────────────────────────────────────────────────

TEST(RouterTest, MiddlewareRunsAroundHandler)
{
    Router router;
    std::string log;

    router.add_middleware([&log](Request req, Handler next) -> boost::asio::awaitable<Response> {
        log += "before;";
        auto res = co_await next(std::move(req));
        log += "after;";
        co_return res;
    });

    router.add_route(Verb::get, "/x", [&log](Request) -> boost::asio::awaitable<Response> {
        log += "handler;";
        co_return make_text_response(Status::ok, "x");
    });

    sync_await(router.dispatch(make_request(Verb::get, "/x")));
    EXPECT_EQ(log, "before;handler;after;");
}

TEST(RouterTest, MultipleMiddlewareExecuteInOrder)
{
    Router router;
    std::string log;

    // Middleware added first runs outermost (first before, last after)
    router.add_middleware([&log](Request req, Handler next) -> boost::asio::awaitable<Response> {
        log += "A";
        auto res = co_await next(std::move(req));
        log += "A";
        co_return res;
    });
    router.add_middleware([&log](Request req, Handler next) -> boost::asio::awaitable<Response> {
        log += "B";
        auto res = co_await next(std::move(req));
        log += "B";
        co_return res;
    });

    router.add_route(Verb::get, "/y", [](Request) -> boost::asio::awaitable<Response> {
        co_return make_text_response(Status::ok, "y");
    });

    sync_await(router.dispatch(make_request(Verb::get, "/y")));
    EXPECT_EQ(log, "ABBA");
}
