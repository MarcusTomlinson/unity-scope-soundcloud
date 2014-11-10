/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of version 3 of the GNU Lesser General Public License as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Pete Woods <pete.woods@canonical.com>
 */

#include <api/client.h>
#include <api/track.h>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <core/net/error.h>
#include <core/net/http/client.h>
#include <core/net/http/content_type.h>
#include <core/net/http/response.h>
#include <json/json.h>

namespace http = core::net::http;
namespace io = boost::iostreams;
namespace json = Json;
namespace net = core::net;

using namespace api;
using namespace std;

namespace {

template<typename T>
static deque<T> get_typed_list(const string &filter, const json::Value &root) {
    deque<T> results;
    for (json::ArrayIndex index = 0; index < root.size(); ++index) {
        json::Value item = root[index];

        string kind = item["kind"].asString();

        if (kind == filter) {
            results.emplace_back(T(item));
        }
    }
    return results;
}

}

class Client::Priv {
public:
    Priv(Config::Ptr config) :
            client_(http::make_client()), worker_ { [this]() {client_->run();} }, config_(
                    config), cancelled_(false) {
    }

    ~Priv() {
        client_->stop();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    std::shared_ptr<core::net::http::Client> client_;

    std::thread worker_;

    Config::Ptr config_;

    std::atomic<bool> cancelled_;

    void get(const net::Uri::Path &path,
            const net::Uri::QueryParameters &parameters,
            http::Request::Handler &handler) {

        http::Request::Configuration configuration;
        net::Uri::QueryParameters complete_parameters(parameters);
        if (config_->authenticated) {
            configuration.header.add("Authorization",
                    "Bearer " + config_->access_token);
        } else {
            complete_parameters.emplace_back("client_id", config_->client_id);
        }

        net::Uri uri = net::make_uri(config_->apiroot, path,
                complete_parameters);
        configuration.uri = client_->uri_to_string(uri);
        cerr << "URI: " << configuration.uri << endl;
        configuration.header.add("User-Agent", config_->user_agent + " (gzip)");
        configuration.header.add("Accept-Encoding", "gzip");

        auto request = client_->head(configuration);
        request->async_execute(handler);
    }

    http::Request::Progress::Next progress_report(
            const http::Request::Progress&) {
        return cancelled_ ?
                http::Request::Progress::Next::abort_operation :
                http::Request::Progress::Next::continue_operation;
    }

    template<typename T>
    future<T> async_get(const net::Uri::Path &path,
            const net::Uri::QueryParameters &parameters,
            const function<T(const json::Value &root)> &func) {
        auto prom = make_shared<promise<T>>();

        http::Request::Handler handler;
        handler.on_progress(
                bind(&Client::Priv::progress_report, this, placeholders::_1));
        handler.on_error([prom](const net::Error& e)
        {
            prom->set_exception(make_exception_ptr(e));
        });
        handler.on_response(
                [prom,func](const http::Response& response)
                {
                    string decompressed;

                    if(!response.body.empty()) {
                        try {
                            io::filtering_ostream os;
                            os.push(io::gzip_decompressor());
                            os.push(io::back_inserter(decompressed));
                            os << response.body;
                            boost::iostreams::close(os);
                        } catch(io::gzip_error &e) {
                            prom->set_exception(make_exception_ptr(e));
                            return;
                        }
                    }

                    json::Value root;
                    json::Reader reader;
                    reader.parse(decompressed, root);

                    if (response.status != http::Status::ok) {
                        prom->set_exception(make_exception_ptr(domain_error(root["error"].asString())));
                    } else {
                        prom->set_value(func(root));
                    }
                });

        get(path, parameters, handler);

        return prom->get_future();
    }
};

Client::Client(Config::Ptr config) :
        p(new Priv(config)) {
}

future<deque<Track>> Client::search_tracks(const std::deque<std::pair<SP, std::string>> &parameters) {
    net::Uri::QueryParameters params;
    for(const auto &p: parameters) {
        switch(p.first){
        case SP::genre:
            params.emplace_back(make_pair("genres", p.second));
            break;
        case SP::limit:
            params.emplace_back(make_pair("limit", p.second));
            break;
        case SP::query:
            params.emplace_back(make_pair("q", p.second));
            break;
        }
    }
    return p->async_get<deque<Track>>( { "tracks.json" }, params,
            [](const json::Value &root) {
                return get_typed_list<Track>("track", root);
            });
}

void Client::cancel() {
    p->cancelled_ = true;
}

Config::Ptr Client::config() {
    return p->config_;
}

