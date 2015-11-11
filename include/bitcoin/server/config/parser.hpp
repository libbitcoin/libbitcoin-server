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
#ifndef LIBBITCOIN_SERVER_PARSER_HPP
#define LIBBITCOIN_SERVER_PARSER_HPP

#include <string>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/server/config/configuration.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/**
 * Parse configurable values from environment variables, settings file, and
 * command line positional and non-positional options.
 */
class BCS_API parser
{
public:
    static bool parse(parser& metadata, std::string& message, int argc,
        const char* argv[]);

    const boost::program_options::options_description load_settings();
    const boost::program_options::options_description load_environment();
    const boost::program_options::options_description load_options();
    const boost::program_options::positional_options_description 
        load_arguments();

    configuration settings;
};

} // namespace server
} // namespace libbitcoin

#endif
