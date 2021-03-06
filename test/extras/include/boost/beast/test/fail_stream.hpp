//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_FAIL_STREAM_HPP
#define BOOST_BEAST_TEST_FAIL_STREAM_HPP

#include <boost/beast/core/async_result.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/test/fail_counter.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace beast {
namespace test {

/** A stream wrapper that fails.

    On the Nth operation, the stream will fail with the specified
    error code, or the default error code of invalid_argument.
*/
template<class NextLayer>
class fail_stream
{
    boost::optional<fail_counter> fc_;
    fail_counter* pfc_;
    NextLayer next_layer_;

public:
    using next_layer_type =
        typename std::remove_reference<NextLayer>::type;

    using lowest_layer_type =
        typename get_lowest_layer<next_layer_type>::type;

    fail_stream(fail_stream&&) = delete;
    fail_stream(fail_stream const&) = delete;
    fail_stream& operator=(fail_stream&&) = delete;
    fail_stream& operator=(fail_stream const&) = delete;

    template<class... Args>
    explicit
    fail_stream(std::size_t n, Args&&... args)
        : fc_(n)
        , pfc_(&*fc_)
        , next_layer_(std::forward<Args>(args)...)
    {
    }

    template<class... Args>
    explicit
    fail_stream(fail_counter& fc, Args&&... args)
        : pfc_(&fc)
        , next_layer_(std::forward<Args>(args)...)
    {
    }

    next_layer_type&
    next_layer()
    {
        return next_layer_;
    }

    lowest_layer_type&
    lowest_layer()
    {
        return next_layer_.lowest_layer();
    }

    lowest_layer_type const&
    lowest_layer() const
    {
        return next_layer_.lowest_layer();
    }

    boost::asio::io_service&
    get_io_service()
    {
        return next_layer_.get_io_service();
    }

    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers)
    {
        pfc_->fail();
        return next_layer_.read_some(buffers);
    }

    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers, error_code& ec)
    {
        if(pfc_->fail(ec))
            return 0;
        return next_layer_.read_some(buffers, ec);
    }

    template<class MutableBufferSequence, class ReadHandler>
    async_return_type<
        ReadHandler, void(error_code, std::size_t)>
    async_read_some(MutableBufferSequence const& buffers,
        ReadHandler&& handler)
    {
        error_code ec;
        if(pfc_->fail(ec))
        {
            async_completion<ReadHandler,
                void(error_code, std::size_t)> init{handler};
            next_layer_.get_io_service().post(
                bind_handler(init.completion_handler, ec, 0));
            return init.result.get();
        }
        return next_layer_.async_read_some(buffers,
            std::forward<ReadHandler>(handler));
    }

    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers)
    {
        pfc_->fail();
        return next_layer_.write_some(buffers);
    }

    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers, error_code& ec)
    {
        if(pfc_->fail(ec))
            return 0;
        return next_layer_.write_some(buffers, ec);
    }

    template<class ConstBufferSequence, class WriteHandler>
    async_return_type<
        WriteHandler, void(error_code, std::size_t)>
    async_write_some(ConstBufferSequence const& buffers,
        WriteHandler&& handler)
    {
        error_code ec;
        if(pfc_->fail(ec))
        {
            async_completion<WriteHandler,
                void(error_code, std::size_t)> init{handler};
            next_layer_.get_io_service().post(
                bind_handler(init.completion_handler, ec, 0));
            return init.result.get();
        }
        return next_layer_.async_write_some(buffers,
            std::forward<WriteHandler>(handler));
    }

    friend
    void
    teardown(
        websocket::role_type role,
        fail_stream<NextLayer>& stream,
        boost::system::error_code& ec)
    {
        if(stream.pfc_->fail(ec))
            return;
        using beast::websocket::teardown;
        teardown(role, stream.next_layer(), ec);
    }

    template<class TeardownHandler>
    friend
    void
    async_teardown(
        websocket::role_type role,
        fail_stream<NextLayer>& stream,
        TeardownHandler&& handler)
    {
        error_code ec;
        if(stream.pfc_->fail(ec))
        {
            stream.get_io_service().post(
                bind_handler(std::move(handler), ec));
            return;
        }
        using beast::websocket::async_teardown;
        async_teardown(role, stream.next_layer(),
            std::forward<TeardownHandler>(handler));
    }
};

} // test
} // beast
} // boost

#endif
