/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_CONFIG_HPP
#define LIBBITCOIN_SERVER_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define LOG_WORKER  "worker"
#define LOG_REQUEST "request"

class config_type;

/**
 * Lod configurable vlaues from environment variables, settings file, and
 * command line positional and non-positional options.
 */
bool BCS_API load_config(config_type& metadata, std::string& message, int argc,
    const char* argv[]);

} // namespace server
} // namespace libbitcoin

#endif
