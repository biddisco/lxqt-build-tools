#pragma once

#include <exception>
#include <string_view>
#include <type_traits>
#include <utility>
//
#include <exec/start_detached.hpp>
#include <stdexec/execution.hpp>
//
#include "debug/logging.hpp"

namespace grox::senders {

  inline auto detached_log = grox::log::create("DETACHED");

  // -----------------------------------------------------------------
  // Fire-and-forget a sender, logging any error and swallowing it so
  // that the new stdexec start_detached (which rejects senders that can
  // complete with set_error) accepts the sender.
  // -----------------------------------------------------------------
  template <typename Sender>
  void start_detached(Sender&& sender, std::string_view context = "")
  {
    auto safe =
        std::forward<Sender>(sender) | stdexec::upon_error([context](auto&& error) noexcept {
          try
          {
            if constexpr (std::is_same_v<std::decay_t<decltype(error)>, std::exception_ptr>)
            {
              std::rethrow_exception(error);
            }
            else
            {
              if (context.empty())
              {
                GROX_LOG_ERROR(detached_log, "Detached sender failed: {}", error);
              }
              else
              {
                GROX_LOG_ERROR(detached_log, "{} - Detached sender failed: {}", context, error);
              }
            }
          }
          catch (std::exception const& e)
          {
            if (context.empty())
            {
              GROX_LOG_ERROR(detached_log, "Detached sender failed: {}", e.what());
            }
            else
            {
              GROX_LOG_ERROR(detached_log, "{} - Detached sender failed: {}", context, e.what());
            }
          }
          catch (...)
          {
            if (context.empty())
            {
              GROX_LOG_ERROR(detached_log, "Detached sender failed: unknown error");
            }
            else
            {
              GROX_LOG_ERROR(detached_log, "{} - Detached sender failed: unknown error", context);
            }
          }
        });
    exec::start_detached(std::move(safe));
  }

}    // namespace grox::senders
