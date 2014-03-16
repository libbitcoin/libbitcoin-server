.. _tut-connecting:

**********
Connecting
**********

We use the ZeroMQ ROUTER-DEALER combination with the backend worker to perform
asynchronous request-reply pairs.

To connect to the server using ZeroMQ in C++, we use:
::

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_DEALER);
    socket.connect("tcp://host:port");
    // Configure socket to not wait at close time.
    int linger = 0;
    socket.setsockopt(ZMQ_LINGER, &linger, sizeof (linger));

We then poll for replies from the server. The details of :func:`zmq_poll`
may differ depending on the language.
::

    void backend_cluster::update()
    {
        //  Poll socket for a reply, with timeout
        zmq::pollitem_t items[] = { { socket_, 0, ZMQ_POLLIN, 0 } };
        zmq::poll(&items[0], 1, 0);
        //  If we got a reply, process it
        if (items[0].revents & ZMQ_POLLIN)
            receive_incoming();
        // Finally resend any expired requests that we didn't get
        // a response to yet.
        strand_.randomly_queue(
            &backend_cluster::resend_expired, this);
    }

    void backend_cluster::receive_incoming()
    {
        incoming_message response;
        if (!response.recv(socket_))
            return;
        strand_.randomly_queue(
            &backend_cluster::process, this, response);
    }

