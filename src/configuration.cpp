/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/configuration.hpp>

#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

static const configuration mainnet_defaults()
{
    return configuration(server::settings::mainnet, node::settings::mainnet,
        blockchain::settings::mainnet, network::settings::mainnet);
};

static const configuration testnet_defaults()
{
    return configuration(server::settings::testnet, node::settings::testnet,
        blockchain::settings::testnet, network::settings::testnet);
};

const configuration configuration::mainnet = mainnet_defaults();
const configuration configuration::testnet = testnet_defaults();

configuration::configuration()
{
}

configuration::configuration(const server::settings& server_settings,
    const node::settings& node_settings,
    const bc::blockchain::settings& chain_settings,
    const network::settings& network_settings)
  : node::configuration(node_settings, chain_settings, network_settings),
    server(server_settings)
{
}

} // namespace server
} // namespace libbitcoin
