/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_BS_LOCALIZE_HPP
#define LIBBITCOIN_BS_LOCALIZE_HPP

/// Localizable messages.

#define BS_WINDOW_TITLE L"Libbitcoin Server"
#define BS_WINDOW_TEXT L"Flushing tables..."

#define BS_OPERATION_INTERRUPT \
    "Press CTRL-C to cancel operation."
#define BS_OPERATION_CANCELED \
    "CTRL-C detected, canceling operation..."

// --settings
#define BS_SETTINGS_MESSAGE \
    "These are the configuration settings that can be set."
#define BS_INFORMATION_MESSAGE \
    "Runs a full bitcoin node server."

// --initchain
#define BS_INITIALIZING_CHAIN \
    "Initializing %1% directory..."
#define BS_INITCHAIN_DIRECTORY_ERROR \
    "Failed creating directory %1% with error '%2%'."
#define BS_INITCHAIN_CREATING \
    "Please wait while creating the database..."
#define BS_INITCHAIN_CREATED \
    "Created the database in %1% secs."
#define BS_INITCHAIN_COMPLETE \
    "Created and initialized the database."
#define BS_INITCHAIN_DATABASE_CREATE_FAILURE \
    "Database creation failed with error '%1%'."
#define BS_INITCHAIN_DATABASE_INITIALIZE \
    "Storing genesis block."
#define BS_INITCHAIN_DATABASE_INITIALIZE_FAILURE \
    "Failure storing genesis block."

// --restore
#define BS_SNAPSHOT_INVALID \
    "Database snapshot disallowed due to corruption '%1%'."
#define BS_RESTORING_CHAIN \
    "Please wait while restoring from most recent snapshot..."
#define BS_RESTORE_MISSING_FLUSH_LOCK \
    "Database is not corrupted, flush lock file is absent."
#define BS_RESTORE_INVALID \
    "Database restore disallowed when corrupt '%1%'."
#define BS_RESTORE_FAILURE \
    "Database restore failed with error '%1%'."
#define BS_RESTORE_COMPLETE \
    "Restored the database in %1% secs."

// --measure
#define BS_MEASURE_SIZES \
    "Body sizes...\n" \
    "   header    :%1%\n" \
    "   txs       :%2%\n" \
    "   tx        :%3%\n" \
    "   point     :%4%\n" \
    "   input     :%5%\n" \
    "   output    :%6%\n" \
    "   ins       :%7%\n" \
    "   outs      :%8%\n" \
    "   candidate :%9%\n" \
    "   confirmed :%10%\n" \
    "   duplicate :%11%\n" \
    "   prevout   :%12%\n" \
    "   strong_tx :%13%\n" \
    "   valid_bk  :%14%\n" \
    "   valid_tx  :%15%\n" \
    "   filter_bk :%16%\n" \
    "   filter_tx :%17%\n" \
    "   address   :%18%"
#define BS_MEASURE_RECORDS \
    "Table records...\n" \
    "   header    :%1%\n" \
    "   tx        :%2%\n" \
    "   point     :%3%\n" \
    "   ins       :%4%\n" \
    "   outs      :%5%\n" \
    "   candidate :%6%\n" \
    "   confirmed :%7%\n" \
    "   duplicate :%8%\n" \
    "   strong_tx :%9%\n" \
    "   filter_bk :%10%\n" \
    "   address   :%11%"
#define BS_MEASURE_SLABS \
    "Table slabs..."
#define BS_MEASURE_SLABS_ROW \
    "   @tx       :%1%, inputs:%2%, outputs:%3%"
#define BS_MEASURE_STOP \
    "   input     :%1%\n" \
    "   output    :%2%\n" \
    "   seconds   :%3%"
#define BS_MEASURE_BUCKETS \
    "Head buckets...\n" \
    "   header    :%1%\n" \
    "   txs       :%2%\n" \
    "   tx        :%3%\n" \
    "   point     :%4%\n" \
    "   duplicate :%5%\n" \
    "   prevout   :%6%\n" \
    "   strong_tx :%7%\n" \
    "   valid_bk  :%8%\n" \
    "   valid_tx  :%9%\n" \
    "   filter_bk :%10%\n" \
    "   filter_tx :%11%\n" \
    "   address   :%12%"
#define BS_MEASURE_COLLISION_RATES \
    "Collision rates...\n" \
    "   header    :%1%\n" \
    "   tx        :%2%\n" \
    "   point     :%3%\n" \
    "   strong_tx :%4%\n" \
    "   valid_tx  :%5%\n" \
    "   address   :%6%"
#define BS_MEASURE_PROGRESS_START \
    "Thinking..."
#define BS_MEASURE_PROGRESS \
    "Chain progress...\n" \
    "   fork pt   :%1%\n" \
    "   top conf  :%2%:%3%\n" \
    "   top cand  :%4%:%5%\n" \
    "   top assoc :%6%\n" \
    "   associated:%7%\n" \
    "   wire conf :%8%\n" \
    "   wire cand :%9%"

// --read
#define BS_READ_ROW \
    ": %1% in %2% secs."

// --write
#define BS_WRITE_ROW \
    ": %1% in %2% span."

// run/general

#define BS_CREATE \
    "create::%1%(%2%)"
#define BS_OPEN \
    "open::%1%(%2%)"
#define BS_CLOSE \
    "close::%1%(%2%)"
#define BS_BACKUP \
    "snapshot::%1%(%2%)"
#define BS_RESTORE \
    "restore::%1%(%2%)"
#define BS_RELOAD \
    "reload::%1%(%2%)"
#define BS_CONDITION \
    "condition::%1%(%2%)"

#define BS_NODE_INTERRUPT \
    "Press CTRL-C to stop the node."
#define BS_DATABASE_STARTED \
    "Database is started (clean)."
#define BS_DATABASE_STARTED_DIRTY \
    "Database is started (dirty)."
#define BS_NETWORK_STARTING \
    "Please wait while network is starting..."
#define BS_NODE_START_FAIL \
    "Node failed to start with error '%1%'."
#define BS_NODE_UNAVAILABLE \
    "Command not available until node started."
#define BS_NODE_INTERRUPTED \
    "Node was interrupted by signal (%1%)."

#define BS_NODE_BACKUP_STARTED \
    "Snapshot is started."
#define BS_NODE_BACKUP_FAIL \
    "Snapshot failed with error '%1%'."
#define BS_NODE_BACKUP_COMPLETE \
    "Snapshot complete in %1% secs."

#define BS_RELOAD_SPACE \
    "Free [%1%] bytes of disk space to restart."
#define BS_RELOAD_INVALID \
    "Reload disallowed due to database corruption '%1%'."
#define BS_NODE_RELOAD_STARTED \
    "Reload from disk full is started."
#define BS_NODE_RELOAD_COMPLETE \
    "Reload from disk full in %1% secs."
#define BS_NODE_RELOAD_FAIL \
    "Reload failed with error '%1%'."

#define BS_NODE_UNRECOVERABLE \
    "Node is not in recoverable condition."
#define BS_NODE_DISK_FULL \
    "Node cannot resume because its disk is full."
#define BS_NODE_OK \
    "Node is ok."

#define BS_NODE_REPORT_WORK \
    "Requested channel work report [%1%]."

#define BS_NODE_STARTED \
    "Node is started."
#define BS_NODE_RUNNING \
    "Node is running."

#define BS_UNINITIALIZED_DATABASE \
    "The %1% database directory does not exist."
#define BS_UNINITIALIZED_CHAIN \
    "The %1% database is not initialized, delete and retry."
#define BS_DATABASE_START_FAIL \
    "Database failed to start with error '%1%'."
#define BS_DATABASE_STOPPING \
    "Please wait while database is stopping..."
#define BS_DATABASE_STOP_FAIL \
    "Database failed to stop with error '%1%'."
#define BS_DATABASE_TIMED_STOP \
    "Database stopped successfully in %1% secs."

#define BS_NETWORK_STOPPING \
    "Please wait while network is stopping..."
#define BS_NODE_STOP_CODE \
    "Node stopped with code %1%."
#define BS_NODE_STOPPED \
    "Node stopped successfully."
#define BS_CHANNEL_LOG_PERIOD \
    "Log period: %1%"
#define BS_CHANNEL_STOP_TARGET \
    "Stop target: %1%"

#define BS_VERSION_MESSAGE \
    "Version Information...\n" \
    "libbitcoin-server:     %1%\n" \
    "libbitcoin-node:       %2%\n" \
    "libbitcoin-network:    %3%\n" \
    "libbitcoin-database:   %4%\n" \
    "libbitcoin-system:     %5%"

#define BS_HARDWARE_HEADER \
    "Hardware configuration..."
#define BS_HARDWARE_TABLE1 \
    "platform:%1%."
#define BS_HARDWARE_TABLE2 \
    "platform:%1% compiled:%2%."

#define BS_LOG_TABLE_HEADER \
    "Log system configuration..."
#define BS_LOG_TABLE \
    "compiled:%1% enabled:%2%."

#define BS_LOG_INITIALIZE_FAILURE \
    "Failed to initialize logging."
#define BS_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BS_USING_DEFAULT_CONFIG \
    "Using default configuration settings."
#define BS_LOG_HEADER \
    "====================== startup ======================="
#define BS_NODE_FOOTER \
    "====================== shutdown ======================"
#define BS_NODE_TERMINATE \
    "Press <enter> to exit..."

#endif
