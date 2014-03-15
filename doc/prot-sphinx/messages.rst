.. _tut-messages:

********
Messages
********

ZeroMQ is a binary transport library. It accepts char byte arrays.
One call to `socket.send(message)` will send message over the socket.

With ZeroMQ, you send char byte arrays (binary data) over its transport
mechanisms. We can also send sequences of binary data that arrive together
at the same time.

For instance to send some data to a socket we first construct a message:
::

    zmq::message_t message(data.size());
    // Pointer to beginning of message so we can copy data to it.
    uint8_t* message_data = reinterpret_cast<uint8_t*>(message.data());
    std::copy(data.begin(), data.end(), message_data);

And then we send to the socket.
::

    try
    {
        if (!socket.send(message, 0))
            return false;
    }
    catch (zmq::error_t error)
    {
        BITCOIN_ASSERT(error.num() != 0);
        return false;
    }
    return true;

Note the 2nd argument of :func:`socket_t::send`. By specifying ZMQ_SNDMORE,
we can send a series of messages which are only available to the receiving
end once a message with the 2nd argument set to 0.

Imagine we send 5 messages:
::

    socket.send(msg1, ZMQ_SNDMORE)
    socket.send(msg2, ZMQ_SNDMORE)
    socket.send(msg5, 0)

When the message 5 arrives to the server, then message 1-4 will be available
too for the server to receive. ZeroMQ calls this a *frame*.

Different ZeroMQ socket types can modify the frame by popping off beginning
messages before passing them to the API.

In this document, we use the class:`czmqpp::message` class
which implements the folowing methods.

.. cpp:function:: void czmqpp::append(part)

   Add a new field to the message.

.. cpp:function:: parts_list czmqpp::parts()

   Return array of all appended parts.

.. cpp:function:: bool czmqpp::send(socket)

   Send entire frame to socket ending on last appended part.

.. cpp:function:: bool czmqpp::receive(socket)

   Receive from the socket until no more messages are available in this frame.

We are able to receive all the messages from a socket until no-more are left
by calling :func:`socket_t::getsockopt` and testing ZMQ_RCVMORE.
::

    int64_t more = 1;
    while (more)
    {
        // ... Receive message from socket.
        // Test if more messages available to receive in this frame.
        size_t more_size = sizeof (more);
        socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
    }

Request & Reply
===============

The format of request and reply fields are very similar.

============== ====================
Request Fields Type(Size)
============== ====================
command        string
id             uint32(4)
data           data
============== ====================

`command` is the remote method invoked on the worker.

`id` is a random value chosen by the client for corralating server replies with
requests the client sent.

`data` is the remote method parameter serialized as binary data.

============== ====================
Reply Fields   Type(Size)
============== ====================
command        string
id             uint32(4)
data           data
============== ====================

