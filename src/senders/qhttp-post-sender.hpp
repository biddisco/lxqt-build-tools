#pragma once

#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>
//
#include <fmt/format.h>
//
#include <pika/assert.hpp>
#include <pika/config.hpp>
//
#include <pika/modules/execution.hpp>
#include <pika/modules/executors.hpp>
#include <pika/modules/resource_partitioner.hpp>
#include <pika/modules/runtime.hpp>
#include <pika/modules/schedulers.hpp>
//
#include "debug/logging.hpp"
#include "network/qhttp-request-client.hpp"
#include "senders/pika_stdexec.hpp"

// @TODO: This code is built on top of the pika implementation of stdexec
// it ought to be rewritten to use 'pure' stdexec and reduce pika dependencies
#if !defined(PIKA_HAVE_STDEXEC)
# pragma error("Pika must be compiled with STDEXEC support")
#endif

// Take a net:http::client and post its contents to the qt networking library
// attach a handler to it that will pass the response over to a pika thread

// -----------------------------------------------------------------
namespace grox::senders {

  inline auto qt_trig_log = grox::log::create("QT_TRIGG");

  namespace ex = pika::execution::experimental;

  enum http_request_type
  {
    http_post = 0,
    http_get = 1,
    http_unset = 2,
  };

  // -----------------------------------------------------------------
  namespace detail {
    // -----------------------------------------------------------------
    // route calls through an impl layer for ADL isolation
    template <typename Sender>
    struct qhttp_post_sender_impl
    {
      struct qhttp_post_sender_type;
    };

    template <typename Sender>
    using qhttp_post_sender = typename qhttp_post_sender_impl<Sender>::qhttp_post_sender_type;

    // -----------------------------------------------------------------
    // qt adapter - sender type
    template <typename Sender>
    struct qhttp_post_sender_impl<Sender>::qhttp_post_sender_type
    {
      using is_sender = void;
      std::decay_t<Sender> sender;
      http_request_type req_type{http_request_type::http_unset};

      // stexec requires set_value_t to match the signature of what we call set_value on
      // when we are finished.
      using completion_signatures = ex::completion_signatures<ex::set_value_t(QByteArray),
          ex::set_error_t(std::exception_ptr)>;

      // -----------------------------------------------------------------
      // operation state for an internal receiver
      template <typename Receiver>
      struct operation_state
      {
        // -----------------------------------------------------------------
        // The receiver receives inputs from the previous sender,
        // invokes the request with a callback on the signal/slot
        struct qhttp_post_receiver
        {
          using is_receiver = void;
          operation_state& op_state;

          template <typename Error>
          friend constexpr void
          tag_invoke(ex::set_error_t, qhttp_post_receiver r, Error&& error) noexcept
          {
            ex::set_error(std::move(r.op_state.receiver_), std::forward<Error>(error));
          }

          friend constexpr void tag_invoke(ex::set_stopped_t, qhttp_post_receiver r) noexcept
          {
            ex::set_stopped(std::move(r.op_state.receiver_));
          }

          // receive the client and set a callback to be triggered when the request completes
          friend constexpr void tag_invoke(
              ex::set_value_t, qhttp_post_receiver r, net::http::client_ptr client) noexcept
          {
            r.op_state.client_ = client;
            assert(r.op_state.client_ != nullptr);

            GROX_LOG_TRACE(qt_trig_log, "{:>20} set_value_t req {}", "qhttp_post_recv",
                fmt::ptr(r.op_state.client_));

            pika::detail::try_catch_exception_ptr(
                [&]() mutable {
                  {
                    // The callback will call set_value/set_error inside a new task
                    // and execution will continue on that thread
                    auto handler = [client = r.op_state.client_,
                                       receiver = std::move(r.op_state.receiver_)](
                                       QByteArray data) {
                      // pass the result onto a new pika task and invoke the continuation
                      auto snd0 =                        //
                          ex::just(std::move(data)) |    //
                          ex::continues_on(default_pool_scheduler()) |
                          ex::then([receiver = std::move(receiver)](QByteArray byteArray) mutable {
                            std::string_view strv(byteArray.constData(), byteArray.length());
                            GROX_LOG_TRACE(qt_trig_log, "{:>20} {}", "qt->pika", strv);
                            ex::set_value(std::move(receiver), std::move(byteArray));
                          });
                      ex::start_detached(std::move(snd0));
                    };
                    if (r.op_state.req_type_ == http_request_type::http_get)
                    {
                      client->get_request(std::move(handler));
                    }
                    else if (r.op_state.req_type_ == http_request_type::http_post)
                    {
                      client->post_request(std::move(handler));
                    }
                    else { throw std::logic_error("Request mode should not be unset"); }
                  }
                },
                [&](std::exception_ptr ep) {
                  ex::set_error(std::move(r.op_state.receiver_), std::move(ep));
                });
          }

          friend constexpr ex::env<> tag_invoke(ex::get_env_t, qhttp_post_receiver const&) noexcept
          {
            return {};
          }
        };

        // -----------------------------------------------------------------
        using operation_state_type =
            ex::connect_result_t<std::decay_t<Sender>, qhttp_post_receiver>;
        // -----------------------------------------------------------------
        std::decay_t<Receiver> receiver_;
        operation_state_type op_state;
        net::http::client_ptr client_;
        http_request_type req_type_{http_request_type::http_unset};

        // -----------------------------------------------------------------
        template <typename Receiver_, typename Sender_>
        operation_state(Receiver_&& receiver, Sender_&& sender, http_request_type req_type)
          : receiver_(std::forward<Receiver_>(receiver))
          , op_state(ex::connect(std::forward<Sender_>(sender), qhttp_post_receiver{*this}))
          , client_(nullptr)
          , req_type_{req_type}
        {
          GROX_LOG_TRACE(qt_trig_log, "{:>20} {}", "create", fmt::ptr(client_));
        }

        ~operation_state()
        {
          GROX_LOG_TRACE(qt_trig_log, "{:>20} {}", "destroy", fmt::ptr(client_));
        }

        friend constexpr auto tag_invoke(ex::start_t, operation_state& os) noexcept
        {
          return ex::start(os.op_state);
        }
      };

      template <typename Receiver>
      friend constexpr auto
      tag_invoke(ex::connect_t, qhttp_post_sender_type const& s, Receiver&& receiver)
      {
        return operation_state<Receiver>(std::forward<Receiver>(receiver), s.sender);
      }

      template <typename Receiver>
      friend constexpr auto
      tag_invoke(ex::connect_t, qhttp_post_sender_type&& s, Receiver&& receiver)
      {
        return operation_state<Receiver>(
            std::forward<Receiver>(receiver), std::move(s.sender), s.req_type);
      }
    };

  }    // namespace detail

  // -----------------------------------------------------------------
  inline constexpr struct qhttp_post_t final : pika::functional::detail::tag_fallback<qhttp_post_t>
  {
private:
    template <typename Sender, PIKA_CONCEPT_REQUIRES_(ex::is_sender_v<std::decay_t<Sender>>)>
    friend constexpr PIKA_FORCEINLINE auto
    tag_fallback_invoke(qhttp_post_t, Sender&& sender, http_request_type req_type)
    {
      return detail::qhttp_post_sender<Sender>{std::forward<Sender>(sender), req_type};
    }

    //
    // tag invoke overload for qhttp_post
    //
    friend constexpr PIKA_FORCEINLINE auto tag_fallback_invoke(
        qhttp_post_t, http_request_type req_type = http_request_type::http_post)
    {
      return ex::detail::partial_algorithm<qhttp_post_t, http_request_type>{req_type};
    }

  } qhttp_post{};

}    // namespace grox::senders
