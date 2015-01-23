/*
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
#include <string>
#include <boost/program_options.hpp>
#include <bitcoin/bitcoin.hpp>
#include "metadata.hpp"

namespace libbitcoin {
namespace server {

using namespace boost::program_options;

const options_description metadata_type::load_options()
{
    return options_description();
}

const positional_options_description metadata_type::load_arguments()
{
    return positional_options_description();
}

const options_description metadata_type::load_settings()
{
    return options_description();
}

const options_description metadata_type::load_environment()
{
    return options_description();
}

// TODO: map this data into the metadata functions above.

//bool log_requests = false;
//bool listener_enabled = true;
//bool publisher_enabled = false;
//uint32_t txpool_capacity = 2000;
//uint32_t outgoing_connections = 8;
//uint32_t history_db_active_height = 0;
//std::string name;
//std::string service = "tcp://*:9091";
//std::string heartbeat = "tcp://*:9092";
//std::string hosts_file = "hosts";
//std::string error_file = "error.log"
//std::string output_file = "debug.log";
//std::string blockchain_path = "blockchain/";
//std::string tx_publish;
//std::string block_publish;
//std::string certificate = "";
//std::string client_allowed_certs = "ALLOW_ALL_CERTS";
//nodes_list nodes;
//ipaddress_list whitelist;

} // namespace server
} // namespace libbitcoin

