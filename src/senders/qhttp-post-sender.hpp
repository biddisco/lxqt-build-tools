#pragma once

#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>
//
#include <pika/assert.hpp>
#include <pika/config.hpp>
//
#include <pika/concepts/concepts.hpp>
#include <pika/datastructures/variant.hpp>
#include <pika/debugging/demangle_helper.hpp>
#include <pika/debugging/print.hpp>
#include <pika/execution/algorithms/detail/helpers.hpp>
#include <pika/execution/algorithms/detail/partial_algorithm.hpp>
#include <pika/execution/algorithms/transfer.hpp>
#include <pika/execution_base/any_sender.hpp>
#include <pika/execution_base/receiver.hpp>
#include <pika/execution_base/sender.hpp>
#include <pika/executors/thread_pool_scheduler.hpp>
#include <pika/functional/detail/tag_fallback_invoke.hpp>
#include <pika/functional/invoke.hpp>
#include <pika/synchronization/condition_variable.hpp>
//
#include "network/qhttp-request-client.hpp"
#include "senders/qt_helpers.hpp"
//
#if !defined(PIKA_HAVE_STDEXEC)
# pragma error("Pika must be compiled with STDEXEC support")
#endif

// Take a net:http::client and post its contents to the qwt qt networking library
// attach a handler to it that will pass the response over to a pika thread
namespace grox::qt::experimental::detail {
  namespace ex = pika::execution::experimental;
  namespace pe = pika::execution;
  using namespace pika::debug::detail;

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

    // stexec requires set_value_t to match the signature of what we call set_value on
    // when we are finished.
    using completion_signatures =
      ex::completion_signatures<ex::set_value_t(QByteArray&&), ex::set_error_t(std::exception_ptr)>;

    //
    static constexpr bool sends_done = false;

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
          ex::set_error(PIKA_MOVE(r.op_state.receiver_), PIKA_FORWARD(Error, error));
        }

        friend constexpr void tag_invoke(ex::set_stopped_t, qhttp_post_receiver r) noexcept
        {
          ex::set_stopped(PIKA_MOVE(r.op_state.receiver_));
        }

        // receive the client and set a callback to be triggered when the request completes
        friend constexpr void tag_invoke(
          ex::set_value_t, qhttp_post_receiver r, net::http::client_ptr client) noexcept
        {
          r.op_state.client_ = client;
          assert(r.op_state.client_ != nullptr);

          PIKA_DETAIL_DP(qt_trig<5>,
            debug(str<>("qhttp_post_recv"), "set_value_t", "req", ptr(r.op_state.client_)));

          pika::detail::try_catch_exception_ptr(
            [&]() mutable {
              {
                // The callback will call set_value/set_error inside a new task
                // and execution will continue on that thread
                auto handler = [client = r.op_state.client_,
                                 receiver = std::move(r.op_state.receiver_)](QByteArray&& data) {
                  // pass the result onto a new pika task and invoke the continuation
                  auto snd0 = ex::just(std::move(data)) |
                    ex::transfer(default_pool_scheduler(pika::execution::thread_priority::normal)) |
                    ex::then([receiver = std::move(receiver)](QByteArray&& byteArray) mutable {
                      std::string_view strv(byteArray.constData(), byteArray.length());
                      PIKA_DETAIL_DP(qt_trig<5>,
                        debug(str<>("set_value_error_helper"), fmt::format("{}", strv)));
                      ex::set_value(std::move(receiver), std::move(byteArray));
                    });
                  ex::start_detached(std::move(snd0));
                };
                client->post_request(std::move(handler));
              }
            },
            [&](std::exception_ptr ep) {
              ex::set_error(PIKA_MOVE(r.op_state.receiver_), PIKA_MOVE(ep));
            });
        }

        friend constexpr ex::empty_env tag_invoke(
          ex::get_env_t, qhttp_post_receiver const&) noexcept
        {
          return {};
        }
      };

      // -----------------------------------------------------------------
      using operation_state_type = ex::connect_result_t<std::decay_t<Sender>, qhttp_post_receiver>;
      // -----------------------------------------------------------------
      std::decay_t<Receiver> receiver_;
      operation_state_type op_state;
      net::http::client_ptr client_;
      // -----------------------------------------------------------------

      template <typename Receiver_, typename Sender_>
      operation_state(Receiver_&& receiver, Sender_&& sender)
        : receiver_(PIKA_FORWARD(Receiver_, receiver))
        , op_state(ex::connect(PIKA_FORWARD(Sender_, sender), qhttp_post_receiver{*this}))
        , client_(nullptr)
      {
        PIKA_DETAIL_DP(qt_trig<0>, debug(str<>("create"), client_));
      }

      ~operation_state()
      {
        PIKA_DETAIL_DP(qt_trig<0>, debug(str<>("destroy"), client_));
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
      return operation_state<Receiver>(PIKA_FORWARD(Receiver, receiver), s.sender);
    }

    template <typename Receiver>
    friend constexpr auto tag_invoke(ex::connect_t, qhttp_post_sender_type&& s, Receiver&& receiver)
    {
      return operation_state<Receiver>(PIKA_FORWARD(Receiver, receiver), PIKA_MOVE(s.sender));
    }
  };

}    // namespace grox::qt::experimental::detail

namespace grox::qt::experimental {
  namespace ex = pika::execution::experimental;
  namespace pe = pika::execution;

  inline constexpr struct qhttp_post_t final : pika::functional::detail::tag_fallback<qhttp_post_t>
  {
private:
    template <typename Sender, PIKA_CONCEPT_REQUIRES_(ex::is_sender_v<std::decay_t<Sender>>)>
    friend constexpr PIKA_FORCEINLINE auto tag_fallback_invoke(qhttp_post_t, Sender&& sender)
    {
      return detail::qhttp_post_sender<Sender>{PIKA_FORWARD(Sender, sender)};
    }

    //
    // tag invoke overload for qhttp_post
    //
    friend constexpr PIKA_FORCEINLINE auto tag_fallback_invoke(qhttp_post_t)
    {
      return ex::detail::partial_algorithm<qhttp_post_t>{};
    }

  } qhttp_post{};

}    // namespace grox::qt::experimental
