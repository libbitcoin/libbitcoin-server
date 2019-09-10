/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/web/default_page_data.hpp>

#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::system;

// TODO: Generate this file via libbitcoin-build(?)
std::string get_default_page_data(const config::endpoint& query,
    const config::endpoint& heartbeat, const config::endpoint& block,
    const config::endpoint& transaction)
{
    const char* pre_image = R"heredoc(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
  <meta http-equiv=content-language content=en>
  <meta http-equiv="content-type" content="text/html; charset=utf-8">
  <meta name="robots" content="noindex">
  <link rel="icon" href="data:image/x-icon;base64,AAABAAEAEhAAAAEAIADoBAAAFgAAACgAAAASAAAAIAAAAAEAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAzUswZNlXJbDdXyJA3V8hqN1bIazlYxjoAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOVXGEjZWyXY4VcdpOFbHTT1VwhU6UsUfAAAAADZWyV83VsiUOFjHNwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOFfHZDhWxzs7WMQaNlXJOQAAAABAVb8MAAAAAAAAAAA3UsgcN1XIVDhXx2QAAAAAAAAAADdVyDM3V8iKN1bIgjhWx4k3VsiLN1bIxzhXx6U3V8iiN1bIUzZRyRM1V8o1NljJPTNNzAoAAAAAAAAAADdXyGoAAAAAAID/AjZXyY0AAAAANlbJdjNVzA83W8gON1XIfjVVyhg5VcYSNlXJWjdXyN03VshrN1bIXDZWyXY3VchmAAAAADhXx1IjoP98I57+zSaI4sEijd9pMG7ZfwD//wEAAAAANlbJXwAAAAAAAAAAAAAAADhXx20AAAAAAAAAAACA/wI4VseAQGC/CDdWyHMioP+6IqD//yJikf8iYY7/Il2H/yNdhvkig8zIJoLdtiGf/00gn/8QAAAAADdXyGoAAAAAAAAAAAAAAAA4VseAOFfHKTdWyH0iof/YIqD//yJRcf8ic67/Ik9u/yI/Uf8iRl//Ikxo/yJomf8idbL9IpLk2SaK4swjoP9mJKD/KwD//wE3VsiDN1bIgzVYykM1kNbNOpDTzS2Ax+MjdbHyIlJz/yJObP8iNUD/IlJ1/yJzr/8iWoD/IlBu/yJklP8iRFv/InSv/yGEzOwliOLaKIft1ySd/E5ylacdUG6mVkBZyVCAo7k6WZDQhz+b35kqdKrVInGs+CJfiv8iVXj/Ikde/yJVef8iTmz/IlBv/yJJYv8iZZT/IqD//yKg/5YAAAAAOVXGEjZVyVoAAAAALl3RCwAAAACqqqoDaYbBTmKf0mU8nuSQK3mz0iJupfciYo//IlR2/yI7Sf8igcf/IqD//yGg/5EAAAAAAAAAADdWyHMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAqqqqA3mcvjtWoNlxOZ/plyt+udAjnvzzIqD//yOg/4sAAAAAAAAAADhYxzc3V8hwN1bIXDNmzAUAAAAAOlHFFgAAAAAAAAAAAAAAAAAA/wEAAAAAAAAAAIC/vwRUeMZ5T4GwxVmSu3UAAAAAAAAAAAAAAABAQL8EOFXHTjZVyUtAQL8ENljJPTNVzA8AAAAAAAAAADdSyBwAAAAAAAAAAAAAAAA4Vsd3K1XVBgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADdVyDM2VslfNlfJhDZWyXEAAAAAAAAAADlVxjY5VcYbAAAAADRXyyw4V8dtAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADdWyEE4VcdyN1fIijZWyX83VshrNlbJaDZXyV4AAP8BAAAAAAAAAAD/gcAA/gRAAP4WAADAAYAAkACAAALsAAAALgAAAAAAAAAAAAAAAAAAlAAAAN/AAADC7AAA4G5AAPhkwAD/AMAA" type="image/x-icon" />
  <title>Libbitcoin Realtime Web Explorer</title>
  <style>
    body { font-family: Verdana, sans-serif; font-size: 11px; }
    a:link { color: #444444; text-decoration: none; }
    a:visited { color: #444444; text-decoration: none;}
    a:hover { color: #444444; text-decoration: underline; }
    a:active { color: #444444; text-decoration: none; }
    .main-div { font-family: Verdana, sans-serif; font-size: 11px; width: 90%; margin-left: 5%; margin-right: 5%; }
    div.center{ margin: 0 auto; }
    .center{ text-align: center; margin-left: auto; margin-right: auto; }
    .heading{ font-size: 14px; font-weight: bold; }
    .width80{ width: 80%; }
    .margint10{ margin-top: 10px; }
    caption{ display: table-caption; text-align: center; caption-side: top; }
    table.grey-grid-table { border: 2px solid #FFFFFF; width: 100%; text-align: center; border-collapse: collapse; }
    table.grey-grid-table td, table.grey-grid-table th { border: 1px solid #FFFFFF; padding: 3px 4px; }
    table.grey-grid-table tbody td { font-size: 11px; }
    table.grey-grid-table td:nth-child(even) { background: #EBEBEB; }
    table.grey-grid-table thead { background: #FFFFFF; border-bottom: 4px solid #333333; }
    table.grey-grid-table thead th { font-size: 13px; font-weight: bold; color: #333333; text-align: center; border-left: 2px solid #333333; }
    table.grey-grid-table thead th:first-child { border-left: none; }
    table.grey-grid-table tfoot { font-size: 12px; font-weight: bold; color: #333333; border-top: 4px solid #333333; }
    table.grey-grid-table tfoot td { font-size: 12px; }
    .modal { display: none; position: fixed; z-index: 1; padding-top: 120px; left: 0; top: 0; width: 100%; height: 100%; background-color: #000000; background-color: rgba(0,0,0,0.6); text-align: left; }
    .modal-container { background-color: #ededed; margin: auto; padding: 10px; border: 1px solid #888; width: 80%; max-height: 80%; overflow-y: auto; }
    .modal-close { color: #999999; float: right; font-size: 35px; font-weight: bold; }
    .modal-close:hover, .close:focus { color: #000000; text-decoration: none; cursor: pointer; }
  </style>
  <script type="text/javascript">
    var modal = null;
    var query_ws = null;
    var satoshi_per_btc = 100000000;
    var block_list = [];
    var height_map = {};
    var height_list = [];
    var transaction_list = [];
    var max_visible_blocks = 5;
    var max_visible_transactions = 10;
    function socket_error(message) { console.log('Socket error: '); console.dir(message); }
    function json_string(json_object) { return JSON.stringify(json_object, null, 1); }
    function handle_transaction(tx) {
        let value = 0;
        let num_inputs = tx.result.transaction.inputs.length;
        let num_outputs = tx.result.transaction.outputs.length;
        tx.result.transaction.outputs.forEach(function(output) { value += parseInt(output.value); });
        value = parseFloat(value / satoshi_per_btc).toFixed(8);
        let transaction_id = 'transaction-' + tx.result.transaction.hash;
        let url = '<a href="#" id="' + transaction_id + '" onclick="javascript:show_message(\'' + transaction_id + '\');">' + tx.result.transaction.hash + '</a>';
        let row = '<tr><td>' + url + '</td><td>' + value + '</td><td>' + num_inputs + '</td><td>'+ num_outputs + '</td><td>' + new Date().toLocaleString() + '</td></tr>';
        transaction_list.push({ 'row': row, 'container': transaction_id, 'data': json_string(tx) });
        if (transaction_list.length > max_visible_transactions)
            transaction_list.shift();
        let tx_data = '';
        for (var i = transaction_list.length - 1; i >= 0; --i)
            tx_data += transaction_list[i].row;
        document.getElementById('transactions').innerHTML = tx_data;
        for (var i = transaction_list.length - 1; i >= 0; --i)
        {
            document.getElementById(transaction_list[i].container).dataset.type = 'Transaction';
            document.getElementById(transaction_list[i].container).dataset.message = transaction_list[i].data;
        }
    }
    function fetch_height_from_block_hash(url, hash, height_container_id) {
        let height_ws = new WebSocket(url);
        height_ws.onopen = function (event) {
            let sequence = parseInt(Math.random() * 1000000);
            let height_request = JSON.stringify({
                  'id' : sequence,
                        'method' : 'getblockheight',
                        'params' : [ hash ]
                        });
            height_map[sequence] = {
              'id': height_container_id,
              'hash': hash,
              'socket': height_ws,
              'height': 'Retrieving ...'
            };
            height_ws.send(height_request);
        };
        height_ws.onclose = function(message) {};
        height_ws.onerror = function(message) { socket_error(message); };
        height_ws.onmessage = function(message) {
            let json = JSON.parse(message.data);
            let entry = height_map[json.sequence];
            if ((entry != undefined) && (document.getElementById(entry.id) != null)) {
                entry.height = json.height;
                document.getElementById(entry.id).innerHTML = json.height;
            }
            else {
                delete height_map[json.sequence];
            }
            entry.socket.close();
        };
    }
    function handle_block(block, query_url)
    {
        let block_hash = block.result.header.hash;
        let height_container_id = block_hash + '-height';
        let height = 'Retrieving ...';
        if (document.getElementById(height_container_id) != null)
            height = document.getElementById(height_container_id).innerHTML;
        let time_stamp = new Date(block.result.header.time_stamp * 1000).toLocaleString();
        let version = block.result.header.version;
        let block_id = 'block-' + block.result.header.hash;
        let url = '<a href="#" id="' + block_id + '" onclick="javascript:show_message(\'' + block_id + '\');">' + block.result.header.hash + '</a>';
              let row = '<tr><td><span id="' + height_container_id +
          '">' + height + '</span></td><td>' + url + '</td><td>' + time_stamp +
                  '</td><td>' + version + '</td></tr>';
              block_list.push({ 'row': row, 'container': block_id, 'data': json_string(block) });
              if (block_list.length > max_visible_blocks)
                  block_list.shift();
              let block_data = '';
              for (var i = block_list.length - 1; i >= 0; --i)
                  block_data += block_list[i].row;
              document.getElementById('blocks').innerHTML = block_data;
              for (var i = block_list.length - 1; i >= 0; --i) {
                  document.getElementById(block_list[i].container).dataset.type = 'Block';
                  document.getElementById(block_list[i].container).dataset.message = block_list[i].data;
              }
              // Update already known heights.
              Object.keys(height_map).forEach(function(sequence) {
                      let value = height_map[sequence];
                      if (document.getElementById(value.id) != null)
                          document.getElementById(value.id).innerHTML = value.height;
                  });
              // Fetch and set the current block height.
              fetch_height_from_block_hash(query_url, block_hash, height_container_id);
    }
    function handle_heartbeat(heartbeat) {
        let heartbeat_id = 'heartbeat-' + heartbeat.sequence;
        let url = '<a href="#" id="' + heartbeat_id + '" onclick="javascript:show_message(\'' + heartbeat_id + '\');">' + heartbeat.height + '</a>';
        let row = '<tr><td>' + new Date().toLocaleString() +
            '</td><td>' + url + '</td></tr>';
        document.getElementById('heartbeats').innerHTML = row;
        document.getElementById(heartbeat_id).dataset.type = 'Heartbeat';
        document.getElementById(heartbeat_id).dataset.message = json_string(heartbeat);
    }
    function start_subscription(name, url, query_url) {
        console.log(name + ' trying to open: ' + url);
        let ws = new WebSocket(url);
        ws.onopen = function (event) { console.log(name + ' client connected'); };
        ws.onmessage = function(message) {
            if (name == 'heartbeat') handle_heartbeat(JSON.parse(message.data));
            if (name == 'transaction') handle_transaction(JSON.parse(message.data));
            if (name == 'block') handle_block(JSON.parse(message.data), query_url);
        };
        ws.onclose = function(message) {};
        ws.onerror = function(message) { socket_error(message); };
    }
    function setup_query(url, open_handler, close_handler, data_handler, error_handler) {
        query_ws = new WebSocket(url); query_ws.onopen = open_handler; query_ws.onclose = close_handler;
        query_ws.onmessage = data_handler; query_ws.onerror = error_handler;
    }
    function issue_query(json_request, container) { query_ws.send(json_request); }
    function show_message(container) { document.getElementById('modal-content').innerHTML = '<p class="heading">Exploring ' + document.getElementById(container).dataset.type + '</p><pre>' + document.getElementById(container).dataset.message + '</pre>'; modal.style.display = "block"; };
    function hide_message() { modal.style.display = "none"; document.getElementById('modal-content').innerHTML = ''; };
    function setup_modal() {
        modal = document.getElementById('alert-modal');
        document.getElementById("modal-closer").onclick = function() { hide_message(); }
        window.onclick = function(event) { if (event.target == modal) { hide_message(); } }
        hide_message();
    }
    document.addEventListener('DOMContentLoaded', setup_modal);
    function ready() {
        DECLARATIONS;
        start_subscription('block', BLOCK_SERVICE_URL, QUERY_SERVICE_URL);
        start_subscription('heartbeat', HEARTBEAT_SERVICE_URL, QUERY_SERVICE_URL);
        start_subscription('transaction', TRANSACTION_SERVICE_URL, QUERY_SERVICE_URL);
    }
    window.onload=ready();
  </script>
  <noscript>Sorry, your browser does not support JavaScript!</noscript>
  </head>
  <body>
  <div id="main" class="main-div">
    <div class="width80 center">
      <div id="alert-modal" class="modal"><div class="modal-container"><span id="modal-closer" class="modal-close">&times;</span><div id="modal-content" style="width=100%; height=100%; overflow-x: auto;"></div></div></div>
      <a target="blank_" href="https://github.com/libbitcoin">
      <img width="100" height="87" src=")heredoc";

    const char* image = R"heredoc(data:image/png;base64,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")heredoc";

    const char* post_image = R"heredoc( class="margint10">
      </a>
      <div>
        <table class="grey-grid-table margint10">
          <caption class="heading">Heartbeat</caption>
          <thead>
            <tr>
              <th>Last updated</th>
              <th>Block Height</th>
            </tr>
          </thead>
          <tbody id="heartbeats"></tbody>
        </table>
      </div>
      <div>
        <table class="grey-grid-table margint10">
          <caption class="heading">Recent Blocks</caption>
          <thead>
            <tr>
              <th>Block Height</th>
              <th>Block Hash</th>
              <th>Block Age</th>
              <th>Block Version</th>
            </tr>
          </thead>
          <tbody id="blocks"></tbody>
        </table>
      </div>
      <div>
        <table class="grey-grid-table margint10">
          <caption class="heading">Latest Transactions</caption>
          <thead>
            <tr>
              <th>Transaction Hash</th>
              <th>Total Transacted (BTC)</th>
              <th># Inputs</th>
              <th># Outputs</th>
              <th>Received Time</th>
            </tr>
          </thead>
          <tbody id="transactions"></tbody>
        </table>
      </div>
      <div>
        <p>Powered by <a target="blank_" href="https://github.com/libbitcoin">Libbitcoin</a>.</p>
      </div>
    </div>
  </div>
</body>
</html>)heredoc";

    const std::string websocket_scheme = "ws";

    // Simplify by modifying bc::config::endpoint(?)
    const auto query_ws_endpoint = config::endpoint(websocket_scheme,
        query.host(), query.port()).to_local();
    const auto heartbeat_ws_endpoint = config::endpoint(websocket_scheme,
        heartbeat.host(), heartbeat.port()).to_local();
    const auto block_ws_endpoint = config::endpoint(websocket_scheme,
        block.host(), block.port()).to_local();
    const auto transaction_ws_endpoint = config::endpoint(websocket_scheme,
        transaction.host(), transaction.port()).to_local();

    std::stringstream ss;
    ss << "let QUERY_SERVICE_URL = '" << query_ws_endpoint << "';\n";
    ss << "let HEARTBEAT_SERVICE_URL = '" << heartbeat_ws_endpoint << "';\n";
    ss << "let BLOCK_SERVICE_URL = '" << block_ws_endpoint << "';\n";
    ss << "let TRANSACTION_SERVICE_URL = '" << transaction_ws_endpoint << "';\n";

    std::stringstream html;
    html << pre_image << image << post_image;
    auto pre_declarations = html.str();

    const std::string marker = "DECLARATIONS;";
    const auto start = pre_declarations.find(marker);

    return pre_declarations.replace(pre_declarations.begin() + start,
        pre_declarations.begin() + start + marker.size(), ss.str());
}

} // namespace server
} // namespace libbitcoin
