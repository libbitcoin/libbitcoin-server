.. _tut-example:

*******
Example
*******

:func:`blockchain::fetch_last_height` takes no arguments and returns the
height of the last block.
::

    # Blank destination
    destination = worker_uuid()
    command = "blockchain.fetch_last_height"
    # Random uint32_t
    id = rand()
    # Blank parameter list
    data = buffer()

The load balancer will query a random worker, route our request to the worker,
the worker will perform the request, send back a reply with the same ID as
the request, and finally the load balancer will forward the reply back
onto the client.

Every Obelisk reply always begins with a 4 byte error_code. If 0 then no
error occured, otherwise the value will be set to a libbitcoin error code
value.
::

    uint32_t val = deserial.read_4_bytes();
    std::error_code ec;
    if (val)
        ec = static_cast<bc::error::error_code_t>(val);

See libbitcoin's <bitcoin/error.hpp> for a list of possible error_code values
and their meanings.

Then the result return values come afterwards::

    size_t last_height = deserial.read_4_bytes();

The full C++ code::

    template <typename Deserializer>
    bool read_error_code(Deserializer& deserial,
        size_t data_size, std::error_code& ec)
    {
        if (data_size < 4)
        {
            log_error() << "No error_code in response.";
            return false;
        }
        uint32_t val = deserial.read_4_bytes();
        if (val)
            ec = static_cast<bc::error::error_code_t>(val);
        return true;
    }

    // ...

    void wrap_fetch_last_height(const data_chunk& data,
        blockchain::fetch_handler_last_height handle_fetch)
    {
        if (data.size() != 8)
        {
            log_error() << "Malformed response for blockchain.fetch_history";
            return;
        }
        std::error_code ec;
        auto deserial = make_deserializer(data.begin(), data.end());
        bool read_success = read_error_code(deserial, data.size(), ec);
        BITCOIN_ASSERT(read_success);
        size_t last_height = deserial.read_4_bytes();
        handle_fetch(ec, last_height);
    }

The serialization of values is the same as in Bitcoin. See libbitcoin's
<bitcoin/utility/serializer.hpp> for more info on how types are serialized.

