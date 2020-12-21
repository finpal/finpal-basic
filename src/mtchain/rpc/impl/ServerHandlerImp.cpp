//------------------------------------------------------------------------------
/*
    This file is part of FinPald: https://github.com/finpal/finpal-basic
    Copyright (c) 2019 ~ 2020 FinPal Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/app/main/Application.h>
#include <mtchain/app/misc/NetworkOPs.h>
#include <mtchain/beast/rfc2616.h>
#include <mtchain/beast/net/IPAddressConversion.h>
#include <mtchain/json/json_reader.h>
#include <mtchain/rpc/json_body.h>
#include <mtchain/rpc/ServerHandler.h>
#include <mtchain/server/Server.h>
#include <mtchain/server/impl/JSONRPCUtil.h>
#include <mtchain/rpc/impl/ServerHandlerImp.h>
#include <mtchain/basics/contract.h>
#include <mtchain/basics/Log.h>
#include <mtchain/basics/make_SSLContext.h>
#include <mtchain/core/JobQueue.h>
#include <mtchain/json/to_string.h>
#include <mtchain/net/RPCErr.h>
#include <mtchain/overlay/Overlay.h>
#include <mtchain/resource/ResourceManager.h>
#include <mtchain/resource/Fees.h>
#include <mtchain/rpc/impl/Tuning.h>
#include <mtchain/rpc/RPCHandler.h>
#include <mtchain/server/SimpleWriter.h>
#include <beast/core/detail/base64.hpp>
#include <beast/http/fields.hpp>
#include <beast/http/string_body.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/type_traits.hpp>
#include <boost/optional.hpp>
#include <boost/regex.hpp>
#include <algorithm>
#include <stdexcept>

namespace mtchain {

// Returns `true` if the HTTP request is a Websockets Upgrade
// http://en.wikipedia.org/wiki/HTTP/1.1_Upgrade_header#Use_with_WebSockets
static
bool
isWebsocketUpgrade(
    http_request_type const& request)
{
    if (is_upgrade(request))
        return beast::detail::ci_equal(
            request.fields["Upgrade"], "websocket");
    return false;
}

static
bool
isStatusRequest(
    http_request_type const& request)
{
    return request.version >= 11 && request.url == "/" &&
            request.body.size() == 0 && request.method == "GET";
}

static
Handoff
unauthorizedResponse(
    http_request_type const& request)
{
    using namespace beast::http;
    Handoff handoff;
    response<string_body> msg;
    msg.version = request.version;
    msg.status = 401;
    msg.reason = "Unauthorized";
    msg.fields.insert("Server", BuildInfo::getFullVersionString());
    msg.fields.insert("Content-Type", "text/html");
    msg.body = "Invalid protocol.";
    prepare(msg, beast::http::connection::close);
    handoff.response = std::make_shared<SimpleWriter>(msg);
    return handoff;
}

// VFALCO TODO Rewrite to use beast::http::fields
static
bool
authorized (
    Port const& port,
    std::map<std::string, std::string> const& h)
{
    if (port.user.empty() || port.password.empty())
        return true;

    auto const it = h.find ("authorization");
    if ((it == h.end ()) || (it->second.substr (0, 6) != "Basic "))
        return false;
    std::string strUserPass64 = it->second.substr (6);
    boost::trim (strUserPass64);
    std::string strUserPass = beast::detail::base64_decode (strUserPass64);
    std::string::size_type nColon = strUserPass.find (":");
    if (nColon == std::string::npos)
        return false;
    std::string strUser = strUserPass.substr (0, nColon);
    std::string strPassword = strUserPass.substr (nColon + 1);
    return strUser == port.user && strPassword == port.password;
}


ServerHandlerImp::ServerHandlerImp (Application& app, Stoppable& parent,
    boost::asio::io_service& io_service, JobQueue& jobQueue,
        NetworkOPs& networkOPs, Resource::Manager& resourceManager,
            CollectorManager& cm)
    : Stoppable("ServerHandler", parent)
    , app_ (app)
    , m_resourceManager (resourceManager)
    , m_journal (app_.journal("Server"))
    , m_networkOPs (networkOPs)
    , m_server (make_Server(
        *this, io_service, app_.journal("Server")))
    , m_jobQueue (jobQueue)
{
    auto const& group (cm.group ("rpc"));
    rpc_requests_ = group->make_counter ("requests");
    rpc_size_ = group->make_event ("size");
    rpc_time_ = group->make_event ("time");
}

ServerHandlerImp::~ServerHandlerImp()
{
    m_server = nullptr;
}

void
ServerHandlerImp::setup (Setup const& setup, beast::Journal journal)
{
    setup_ = setup;
    m_server->ports (setup.ports);
}

//------------------------------------------------------------------------------

void
ServerHandlerImp::onStop()
{
    m_server->close();
}

//------------------------------------------------------------------------------

bool
ServerHandlerImp::onAccept (Session& session,
    boost::asio::ip::tcp::endpoint endpoint)
{
    std::lock_guard<std::mutex> l(countlock_);

    auto const c = ++count_[session.port()];

    if (session.port().limit && c >= session.port().limit)
    {
        JLOG (m_journal.trace()) <<
            session.port().name << " is full; dropping " <<
            endpoint;
        return false;
    }

    return true;
}

auto
ServerHandlerImp::onHandoff (Session& session,
    std::unique_ptr <beast::asio::ssl_bundle>&& bundle,
        http_request_type&& request,
            boost::asio::ip::tcp::endpoint remote_address) ->
    Handoff
{
    const bool is_ws =
        (session.port().protocol.count("wss") > 0) ||
        (session.port().protocol.count("wss2") > 0);

    if(isWebsocketUpgrade(request))
    {
        if(is_ws)
        {
            Handoff handoff;
            auto const ws = session.websocketUpgrade();
            auto is = std::make_shared<WSInfoSub>(m_networkOPs, ws);
            is->getConsumer() = requestInboundEndpoint(
                m_resourceManager,
                    beast::IPAddressConversion::from_asio(remote_address),
                        session.port(), is->user());
            ws->appDefined = std::move(is);
            ws->run();
            handoff.moved = true;
            return handoff;
        }

        return unauthorizedResponse(request);
    }

    if(session.port().protocol.count("peer") > 0)
    {
        return app_.overlay().onHandoff(std::move(bundle),
            std::move(request), remote_address);
    }

    if (is_ws && isStatusRequest(request))
        return statusResponse(request);

    // Pass to legacy onRequest
    return {};
}

auto
ServerHandlerImp::onHandoff (Session& session,
    boost::asio::ip::tcp::socket&& socket,
        http_request_type&& request,
            boost::asio::ip::tcp::endpoint remote_address) ->
    Handoff
{
    if(isWebsocketUpgrade(request))
    {
        if (session.port().protocol.count("ws2") > 0 ||
            session.port().protocol.count("ws") > 0)
        {
            Handoff handoff;
            auto const ws = session.websocketUpgrade();
            auto is = std::make_shared<WSInfoSub>(m_networkOPs, ws);
            is->getConsumer() = requestInboundEndpoint(
                m_resourceManager, beast::IPAddressConversion::from_asio(
                    remote_address), session.port(), is->user());
            ws->appDefined = std::move(is);
            ws->run();
            handoff.moved = true;
            return handoff;
        }

        return unauthorizedResponse(request);
    }

    if ((session.port().protocol.count("ws") > 0 ||
         session.port().protocol.count("ws2") > 0) &&
       isStatusRequest(request))
        return statusResponse(request);

    // Otherwise pass to legacy onRequest or websocket
    return {};
}

static inline
Json::Output makeOutput (Session& session)
{
    return [&](boost::string_ref const& b)
    {
        session.write (b.data(), b.size());
    };
}

// HACK!
static
std::map<std::string, std::string>
build_map(beast::http::fields const& h)
{
    std::map <std::string, std::string> c;
    for (auto const& e : h)
    {
        auto key (e.first);
        // TODO Replace with safe C++14 version
        std::transform (key.begin(), key.end(), key.begin(), ::tolower);
        c [key] = e.second;
    }
    return c;
}

template<class ConstBufferSequence>
static
std::string
buffers_to_string(ConstBufferSequence const& bs)
{
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    std::string s;
    s.reserve(buffer_size(bs));
    for(auto const& b : bs)
        s.append(buffer_cast<char const*>(b),
            buffer_size(b));
    return s;
}

void
ServerHandlerImp::onRequest (Session& session)
{
    // Make sure RPC is enabled on the port
    if (session.port().protocol.count("http") == 0 &&
        session.port().protocol.count("https") == 0)
    {
        HTTPReply (403, "Forbidden", makeOutput (session), app_.journal ("RPC"));
        session.close (true);
        return;
    }

    // Check user/password authorization
    if (! authorized (
            session.port(), build_map(session.request().fields)))
    {
        HTTPReply (403, "Forbidden", makeOutput (session), app_.journal ("RPC"));
        session.close (true);
        return;
    }

    m_jobQueue.postCoro(jtCLIENT, "RPC-Client",
        [this, detach = session.detach()](std::shared_ptr<JobQueue::Coro> c)
        {
            processSession(detach, c);
        });
}

void
ServerHandlerImp::onWSMessage(
    std::shared_ptr<WSSession> session,
        std::vector<boost::asio::const_buffer> const& buffers)
{
    Json::Value jv;
    auto const size = boost::asio::buffer_size(buffers);
    if (size > RPC::Tuning::maxRequestSize ||
        ! Json::Reader{}.parse(jv, buffers) ||
        ! jv ||
        ! jv.isObject())
    {
        Json::Value jvResult(Json::objectValue);
        jvResult[jss::type] = jss::error;
        jvResult[jss::error] = "jsonInvalid";
        jvResult[jss::value] = buffers_to_string(buffers);
        beast::streambuf sb;
        Json::stream(jvResult,
            [&sb](auto const p, auto const n)
            {
                sb.commit(boost::asio::buffer_copy(
                    sb.prepare(n), boost::asio::buffer(p, n)));
            });
        JLOG(m_journal.trace())
            << "Websocket sending '" << jvResult << "'";
        session->send(std::make_shared<
            StreambufWSMsg<decltype(sb)>>(std::move(sb)));
        session->complete();
        return;
    }

    JLOG(m_journal.trace())
        << "Websocket received '" << jv << "'";

    m_jobQueue.postCoro(jtCLIENT, "WS-Client",
        [this, session = std::move(session),
            jv = std::move(jv)](auto const& c)
        {
            auto const jr =
                this->processSession(session, c, jv);
            auto const s = to_string(jr);
            auto const n = s.length();
            beast::streambuf sb(n);
            sb.commit(boost::asio::buffer_copy(
                sb.prepare(n), boost::asio::buffer(s.c_str(), n)));
            session->send(std::make_shared<
                StreambufWSMsg<decltype(sb)>>(std::move(sb)));
            session->complete();
        });
}

void
ServerHandlerImp::onClose (Session& session,
    boost::system::error_code const&)
{
    std::lock_guard<std::mutex> l(countlock_);
    --count_[session.port()];
}

void
ServerHandlerImp::onStopped (Server&)
{
    stopped();
}

//------------------------------------------------------------------------------

Json::Value
ServerHandlerImp::processSession(
    std::shared_ptr<WSSession> const& session,
        std::shared_ptr<JobQueue::Coro> const& coro,
            Json::Value const& jv)
{
    auto is = std::static_pointer_cast<WSInfoSub> (session->appDefined);
    if (is->getConsumer().disconnect())
    {
        session->close();
        // FIX: This rpcError is not delivered since the session
        // was just closed.
        return rpcError(rpcSLOW_DOWN);
    }

    // Requests without "command" are invalid.
    Json::Value jr(Json::objectValue);
    if ((!jv.isMember(jss::command) && !jv.isMember(jss::method)) ||
        (jv.isMember(jss::command) && jv.isMember(jss::method) &&
         jv[jss::command].asString() != jv[jss::method].asString()))
    {
        jr[jss::type] = jss::response;
        jr[jss::status] = jss::error;
        jr[jss::error] = jss::missingCommand;
        jr[jss::request] = jv;
        if (jv.isMember (jss::id))
            jr[jss::id]  = jv[jss::id];
        if (jv.isMember(jss::jsonrpc))
            jr[jss::jsonrpc] = jv[jss::jsonrpc];
        if (jv.isMember(jss::FinPalrpc))
            jr[jss::FinPalrpc] = jv[jss::FinPalrpc];

        is->getConsumer().charge(Resource::feeInvalidRPC);
        return jr;
    }

    Resource::Charge loadType = Resource::feeReferenceRPC;
    auto required = RPC::roleRequired(jv.isMember(jss::command) ?
                                      jv[jss::command].asString() :
                                      jv[jss::method].asString());
    auto role = requestRole(
        required,
        session->port(),
        jv,
        beast::IP::from_asio(session->remote_endpoint().address()),
        is->user());
    if (Role::FORBID == role)
    {
        loadType = Resource::feeInvalidRPC;
        jr[jss::result] = rpcError (rpcFORBIDDEN);
    }
    else
    {
        RPC::Context context{
            app_.journal("RPCHandler"),
            jv,
            app_,
            loadType,
            app_.getOPs(),
            app_.getLedgerMaster(),
            is->getConsumer(),
            role,
            coro,
            is,
            {is->user(), is->forwarded_for()}
            };
        RPC::doCommand(context, jr[jss::result]);
    }

    is->getConsumer().charge(loadType);
    if (is->getConsumer().warn())
        jr[jss::warning] = jss::load;

    // Currently we will simply unwrap errors returned by the RPC
    // API, in the future maybe we can make the responses
    // consistent.
    //
    // Regularize result. This is duplicate code.
    if (jr[jss::result].isMember(jss::error))
    {
        jr = jr[jss::result];
        jr[jss::status] = jss::error;
        jr[jss::request] = jv;

    }
    else
    {
        jr[jss::status] = jss::success;

        // For testing resource limits on this connection.
        if (is->getConsumer().isUnlimited() &&
            jv[jss::command].asString() == "ping")
                jr[jss::unlimited] = true;
    }

    if (jv.isMember(jss::id))
        jr[jss::id] = jv[jss::id];
    if (jv.isMember(jss::jsonrpc))
        jr[jss::jsonrpc] = jv[jss::jsonrpc];
    if (jv.isMember(jss::FinPalrpc))
        jr[jss::FinPalrpc] = jv[jss::FinPalrpc];
    jr[jss::type] = jss::response;
    return jr;
}

// Run as a coroutine.
void
ServerHandlerImp::processSession (std::shared_ptr<Session> const& session,
    std::shared_ptr<JobQueue::Coro> coro)
{
    auto closeSess = processRequest (
        session->port(), buffers_to_string(
            session->request().body.data()),
                session->remoteAddress().at_port (0),
                    makeOutput (*session), coro,
        [&]
        {
            auto const iter =
                session->request().fields.find(
                    "X-Forwarded-For");
            if(iter != session->request().fields.end())
                return iter->second;
            return std::string{};
        }(),
        [&]
        {
            auto const iter =
                session->request().fields.find(
                    "X-User");
            if(iter != session->request().fields.end())
                return iter->second;
            return std::string{};
        }());

    if(!closeSess && is_keep_alive(session->request()))
        session->complete();
    else
        session->close (true);
}

int
ServerHandlerImp::processRequest (Port const& port,
    std::string const& request, beast::IP::Endpoint const& remoteIPAddress,
        Output&& output, std::shared_ptr<JobQueue::Coro> coro,
        std::string forwardedFor, std::string user)
{
    auto rpcJ = app_.journal ("RPC");

    Json::Value jsonRPC;
    {
        Json::Reader reader;
        if ((request.size () > RPC::Tuning::maxRequestSize) ||
            ! reader.parse (request, jsonRPC) ||
            ! jsonRPC ||
            ! jsonRPC.isObject ())
        {
            HTTPReply (400, "Unable to parse request", output, rpcJ);
            return 0;
        }
    }

    /* ---------------------------------------------------------------------- */
    // Determine role/usage so we can charge for invalid requests
    Json::Value const& method = jsonRPC [jss::method];

    auto role = Role::FORBID;
    auto required = RPC::roleRequired(method.asString());
    if (jsonRPC.isMember(jss::params) &&
        jsonRPC[jss::params].isArray() &&
        jsonRPC[jss::params].size() > 0 &&
        jsonRPC[jss::params][Json::UInt(0)].isObject())
    {
        role = requestRole(required, port, jsonRPC[jss::params][Json::UInt(0)],
            remoteIPAddress, user);
    }
    else
    {
        role = requestRole(required, port, Json::objectValue,
            remoteIPAddress, user);
    }

    Resource::Consumer usage;
    if (isUnlimited(role))
    {
        usage = m_resourceManager.newUnlimitedEndpoint(
            remoteIPAddress.to_string());
    }
    else
    {
        usage = m_resourceManager.newInboundEndpoint(remoteIPAddress);
        if (usage.disconnect())
        {
            HTTPReply(503, "Server is overloaded", output, rpcJ);
            return 0;
        }
    }

    if (role == Role::FORBID)
    {
        usage.charge(Resource::feeInvalidRPC);
        HTTPReply (403, "Forbidden", output, rpcJ);
        return 0;
    }

    if (! method)
    {
        usage.charge(Resource::feeInvalidRPC);
        HTTPReply (400, "Null method", output, rpcJ);
        return 0;
    }

    if (! method.isString ())
    {
        usage.charge(Resource::feeInvalidRPC);
        HTTPReply (400, "method is not string", output, rpcJ);
        return 0;
    }

    std::string strMethod = method.asString ();
    if (strMethod.empty())
    {
        usage.charge(Resource::feeInvalidRPC);
        HTTPReply (400, "method is empty", output, rpcJ);
        return 0;
    }

    // Extract request parameters from the request Json as `params`.
    //
    // If the field "params" is empty, `params` is an empty object.
    //
    // Otherwise, that field must be an array of length 1 (why?)
    // and we take that first entry and validate that it's an object.
    Json::Value params = jsonRPC [jss::params];

    if (! params)
        params = Json::Value (Json::objectValue);

    else if (!params.isArray () || params.size() != 1)
    {
        usage.charge(Resource::feeInvalidRPC);
        HTTPReply (400, "params unparseable", output, rpcJ);
        return 0;
    }
    else
    {
        params = std::move (params[0u]);
        if (!params.isObject())
        {
            usage.charge(Resource::feeInvalidRPC);
            HTTPReply (400, "params unparseable", output, rpcJ);
            return 0;
        }
    }

    /**
     * Clear header-assigned values if not positively identified from a
     * secure_gateway.
     */
    if (role != Role::IDENTIFIED)
    {
        forwardedFor.clear();
        user.clear();
    }

    JLOG(m_journal.debug()) << "Query: " << strMethod << params;

    // Provide the JSON-RPC method as the field "command" in the request.
    params[jss::command] = strMethod;
    JLOG (m_journal.trace())
        << "doRpcCommand:" << strMethod << ":" << params;

    Resource::Charge loadType = Resource::feeReferenceRPC;
    auto const start (std::chrono::high_resolution_clock::now ());

    RPC::Context context {m_journal, params, app_, loadType, m_networkOPs,
        app_.getLedgerMaster(), usage, role, coro, InfoSub::pointer(),
        {user, forwardedFor}, &output};
    Json::Value result;
    RPC::doCommand (context, result);

    if (context.noReply)
    {
        JLOG (m_journal.warn()) << "No need our reply for \n" << pretty(jsonRPC);
        return context.closeSess;
    }

    // Always report "status".  On an error report the request as received.
    if (result.isMember (jss::error))
    {
        result[jss::status] = jss::error;
        result[jss::request] = params;
        JLOG (m_journal.debug())  <<
            "rpcError: " << result [jss::error] <<
            ": " << result [jss::error_message];
    }
    else
    {
        result[jss::status]  = jss::success;
    }

    usage.charge (loadType);
    if (usage.warn())
        result[jss::warning] = jss::load;

    Json::Value reply (Json::objectValue);
    reply[jss::result] = std::move (result);
    if (jsonRPC.isMember(jss::jsonrpc))
        reply[jss::jsonrpc] = jsonRPC[jss::jsonrpc];
    if (jsonRPC.isMember(jss::FinPalrpc))
        reply[jss::FinPalrpc] = jsonRPC[jss::FinPalrpc];
    if (jsonRPC.isMember(jss::id))
        reply[jss::id] = jsonRPC[jss::id];
    auto response = to_string (reply);

    rpc_time_.notify (static_cast <beast::insight::Event::value_type> (
        std::chrono::duration_cast <std::chrono::milliseconds> (
            std::chrono::high_resolution_clock::now () - start)));
    ++rpc_requests_;
    rpc_size_.notify (static_cast <beast::insight::Event::value_type> (
        response.size ()));

    response += '\n';

    if (auto stream = m_journal.debug())
    {
        static const int maxSize = 10000;
        if (response.size() <= maxSize)
            stream << "Reply: " << response;
        else
            stream << "Reply: " << response.substr (0, maxSize);
    }

    HTTPReply (200, response, output, rpcJ);
    return 0;
}

//------------------------------------------------------------------------------

/*  This response is used with load balancing.
    If the server is overloaded, status 500 is reported. Otherwise status 200
    is reported, meaning the server can accept more connections.
*/
Handoff
ServerHandlerImp::statusResponse(
    http_request_type const& request) const
{
    using namespace beast::http;
    Handoff handoff;
    response<string_body> msg;
    std::string reason;
    if (app_.serverOkay(reason))
    {
        msg.status = 200;
        msg.reason = "OK";
        msg.body = "<!DOCTYPE html><html><head><title>" + systemName() +
            " Test page for " + systemName() + "d</title></head><body><h1>" +
            systemName() + " Test</h1><p>This page shows " + systemName() + "d http(s) "
                    "connectivity is working.</p></body></html>";
    }
    else
    {
        msg.status = 500;
        msg.reason = "Internal Server Error";
        msg.body = "<HTML><BODY>Server cannot accept clients: " +
            reason + "</BODY></HTML>";
    }
    msg.version = request.version;
    msg.fields.insert("Server", BuildInfo::getFullVersionString());
    msg.fields.insert("Content-Type", "text/html");
    prepare(msg, beast::http::connection::close);
    handoff.response = std::make_shared<SimpleWriter>(msg);
    return handoff;
}

//------------------------------------------------------------------------------

void
ServerHandler::Setup::makeContexts()
{
    for(auto& p : ports)
    {
        if (p.secure())
        {
            if (p.ssl_key.empty() && p.ssl_cert.empty() &&
                    p.ssl_chain.empty())
                p.context = make_SSLContext(p.ssl_ciphers);
            else
                p.context = make_SSLContextAuthed (
                    p.ssl_key, p.ssl_cert, p.ssl_chain,
                    p.ssl_ciphers);
        }
        else
        {
            p.context = std::make_shared<
                boost::asio::ssl::context>(
                    boost::asio::ssl::context::sslv23);
        }
    }
}

static
Port
to_Port(ParsedPort const& parsed, std::ostream& log)
{
    Port p;
    p.name = parsed.name;

    if (! parsed.ip)
    {
        log << "Missing 'ip' in [" << p.name << "]\n";
        Throw<std::exception> ();
    }
    p.ip = *parsed.ip;

    if (! parsed.port)
    {
        log << "Missing 'port' in [" << p.name << "]\n";
        Throw<std::exception> ();
    }
    else if (*parsed.port == 0)
    {
        log << "Port " << *parsed.port << "in [" << p.name << "] is invalid\n";
        Throw<std::exception> ();
    }
    p.port = *parsed.port;
    if (parsed.admin_ip)
        p.admin_ip = *parsed.admin_ip;
    if (parsed.secure_gateway_ip)
        p.secure_gateway_ip = *parsed.secure_gateway_ip;

    if (parsed.protocol.empty())
    {
        log << "Missing 'protocol' in [" << p.name << "]\n";
        Throw<std::exception> ();
    }
    p.protocol = parsed.protocol;

    p.user = parsed.user;
    p.password = parsed.password;
    p.admin_user = parsed.admin_user;
    p.admin_password = parsed.admin_password;
    p.ssl_key = parsed.ssl_key;
    p.ssl_cert = parsed.ssl_cert;
    p.ssl_chain = parsed.ssl_chain;
    p.ssl_ciphers = parsed.ssl_ciphers;
    p.pmd_options = parsed.pmd_options;

    return p;
}

static
std::vector<Port>
parse_Ports (
    Config const& config,
    std::ostream& log)
{
    std::vector<Port> result;

    if (! config.exists("server"))
    {
        log <<
            "Required section [server] is missing\n";
        Throw<std::exception> ();
    }

    ParsedPort common;
    parse_Port (common, config["server"], log);

    auto const& names = config.section("server").values();
    result.reserve(names.size());
    for (auto const& name : names)
    {
        if (! config.exists(name))
        {
            log <<
                "Missing section: [" << name << "]\n";
            Throw<std::exception> ();
        }
        ParsedPort parsed = common;
        parsed.name = name;
        parse_Port(parsed, config[name], log);
        result.push_back(to_Port(parsed, log));
    }

    if (config.standalone())
    {
        auto it = result.begin ();

        while (it != result.end())
        {
            auto& p = it->protocol;

            // Remove the peer protocol, and if that would
            // leave the port empty, remove the port as well
            if (p.erase ("peer") && p.empty())
                it = result.erase (it);
            else
                ++it;
        }
    }
    else
    {
        auto const count = std::count_if (
            result.cbegin(), result.cend(),
            [](Port const& p)
            {
                return p.protocol.count("peer") != 0;
            });

        if (count > 1)
        {
            log << "Error: More than one peer protocol configured in [server]\n";
            Throw<std::exception> ();
        }

        if (count == 0)
            log << "Warning: No peer protocol configured\n";
    }

    return result;
}

// Fill out the client portion of the Setup
static
void
setup_Client (ServerHandler::Setup& setup)
{
    decltype(setup.ports)::const_iterator iter;
    for (iter = setup.ports.cbegin();
            iter != setup.ports.cend(); ++iter)
        if (iter->protocol.count("http") > 0 ||
                iter->protocol.count("https") > 0)
            break;
    if (iter == setup.ports.cend())
        return;
    setup.client.secure =
        iter->protocol.count("https") > 0;
    setup.client.ip = iter->ip.to_string();
    // VFALCO HACK! to make localhost work
    if (setup.client.ip == "0.0.0.0")
        setup.client.ip = "127.0.0.1";
    setup.client.port = iter->port;
    setup.client.user = iter->user;
    setup.client.password = iter->password;
    setup.client.admin_user = iter->admin_user;
    setup.client.admin_password = iter->admin_password;
}

// Fill out the overlay portion of the Setup
static
void
setup_Overlay (ServerHandler::Setup& setup)
{
    auto const iter = std::find_if(
        setup.ports.cbegin(), setup.ports.cend(),
        [](Port const& port)
        {
            return port.protocol.count("peer") != 0;
        });
    if (iter == setup.ports.cend())
    {
        setup.overlay.port = 0;
        return;
    }
    setup.overlay.ip = iter->ip;
    setup.overlay.port = iter->port;
}

ServerHandler::Setup
setup_ServerHandler(
    Config const& config,
    std::ostream&& log)
{
    ServerHandler::Setup setup;
    setup.ports = parse_Ports(config, log);

    setup_Client(setup);
    setup_Overlay(setup);

    return setup;
}

std::unique_ptr <ServerHandler>
make_ServerHandler (Application& app, Stoppable& parent,
    boost::asio::io_service& io_service, JobQueue& jobQueue,
        NetworkOPs& networkOPs, Resource::Manager& resourceManager,
            CollectorManager& cm)
{
    return std::make_unique<ServerHandlerImp>(app, parent,
        io_service, jobQueue, networkOPs, resourceManager, cm);
}

} //
