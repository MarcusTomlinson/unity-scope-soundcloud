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

#ifndef API_CLIENT_H_
#define API_CLIENT_H_

#include <api/config.h>
#include <api/track.h>

#include <atomic>
#include <deque>
#include <future>
#include <map>
#include <string>
#include <core/net/http/request.h>
#include <core/net/uri.h>

namespace Json {
class Value;
}

namespace api {

/**
 * Search parameters
 */
enum class SP {
    genre, limit, query
};

/**
 * Provide a nice way to access the HTTP API.
 *
 * We don't want our scope's code to be mixed together with HTTP and JSON handling.
 */
class Client {
public:
    Client(Config::Ptr config);

    virtual ~Client() = default;


    virtual std::future<std::deque<Track>> search_tracks(
            const std::deque<std::pair<SP, std::string>> &parameters);

    /**
     * Cancel any pending queries (this method can be called from a different thread)
     */
    virtual void cancel();

    virtual Config::Ptr config();

protected:
    class Priv;
    friend Priv;

    std::shared_ptr<Priv> p;
};

}

#endif // API_CLIENT_H_

