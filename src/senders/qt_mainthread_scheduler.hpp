// Taken from stdexec inline scheduler and modified accordingly

/*
 * Copyright (c) 2021-2022 NVIDIA Corporation
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <exception>
#include <type_traits>
//
#include <stdexec/execution.hpp>
//
#include <pika/assert.hpp>
#include <pika/async_base/scheduling_properties.hpp>
#include <pika/config.hpp>
#include <pika/errors/try_catch_exception_ptr.hpp>
#include <pika/execution_base/sender.hpp>
#include <pika/execution_base/stdexec_forward.hpp>
//
#include <QCoreApplication>
#include <QMainWindow>

namespace grox::senders {
  using qt_function_type = std::function<void(void)>;

  inline QMainWindow* getMainWindow()
  {
    foreach (QWidget* w, qApp->topLevelWidgets())
    {
      try
      {
        if (QMainWindow* mainWin = qobject_cast<QMainWindow*>(w)) return mainWin;
      }
      catch (...)
      {
      }
    }
    throw std::runtime_error("Mainwindow could not be obtained");
    return nullptr;
  }

  struct qt_mainthread_scheduler
  {
    constexpr qt_mainthread_scheduler() = default;

    /// \cond NOINTERNAL
    bool operator==(qt_mainthread_scheduler const& rhs) const noexcept { return true; }

    bool operator!=(qt_mainthread_scheduler const& rhs) const noexcept { return !(*this == rhs); }

    template <typename F>
    void execute(F&& f) const
    {
      // Do not use DirectConnection as it will execute on the same thread
      QMetaObject::invokeMethod(
          grox::senders::getMainWindow(), PIKA_FORWARD(F, f), Qt::AutoConnection);
    }

    template <typename F>
    friend void tag_invoke(stdexec::execute_t, qt_mainthread_scheduler const& sched, F&& f)
    {
      sched.execute(PIKA_FORWARD(F, f));
    }

    template <typename Scheduler, typename Receiver>
    struct operation_state
    {
      PIKA_NO_UNIQUE_ADDRESS std::decay_t<Scheduler> scheduler;
      PIKA_NO_UNIQUE_ADDRESS std::decay_t<Receiver> receiver;

      template <typename Scheduler_, typename Receiver_>
      operation_state(Scheduler_&& scheduler, Receiver_&& receiver)
        : scheduler(PIKA_FORWARD(Scheduler_, scheduler))
        , receiver(PIKA_FORWARD(Receiver_, receiver))
      {
      }

      operation_state(operation_state&&) = delete;
      operation_state(operation_state const&) = delete;
      operation_state& operator=(operation_state&&) = delete;
      operation_state& operator=(operation_state const&) = delete;

      friend void tag_invoke(stdexec::start_t, operation_state& os) noexcept
      {
        pika::detail::try_catch_exception_ptr(
            [&]() {
              os.scheduler.execute([&os]() mutable {
                pika::execution::experimental::set_value(PIKA_MOVE(os.receiver));
              });
            },
            [&](std::exception_ptr ep) {
              pika::execution::experimental::set_error(PIKA_MOVE(os.receiver), PIKA_MOVE(ep));
            });
      }
    };

    template <typename Scheduler>
    struct sender
    {
      PIKA_STDEXEC_SENDER_CONCEPT

      PIKA_NO_UNIQUE_ADDRESS std::decay_t<Scheduler> scheduler;

      template <template <typename...> class Tuple, template <typename...> class Variant>
      using value_types = Variant<Tuple<>>;

      template <template <typename...> class Variant>
      using error_types = Variant<std::exception_ptr>;

      static constexpr bool sends_done = false;

      using completion_signatures = pika::execution::experimental::completion_signatures<
          pika::execution::experimental::set_value_t(),
          pika::execution::experimental::set_error_t(std::exception_ptr)>;

      template <typename Receiver>
      friend operation_state<Scheduler, Receiver>
      tag_invoke(stdexec::connect_t, sender&& s, Receiver&& receiver)
      {
        return {PIKA_MOVE(s.scheduler), PIKA_FORWARD(Receiver, receiver)};
      }

      template <typename Receiver>
      friend operation_state<Scheduler, Receiver>
      tag_invoke(stdexec::connect_t, sender const& s, Receiver&& receiver)
      {
        return {s.scheduler, PIKA_FORWARD(Receiver, receiver)};
      }

      struct env
      {
        PIKA_NO_UNIQUE_ADDRESS std::decay_t<Scheduler> scheduler;

        friend std::decay_t<Scheduler> tag_invoke(
            pika::execution::experimental::get_completion_scheduler_t<
                pika::execution::experimental::set_value_t>,
            env const& e) noexcept
        {
          return e.scheduler;
        }
      };

      friend env tag_invoke(pika::execution::experimental::get_env_t, sender const& s) noexcept
      {
        return {s.scheduler};
      }
    };

    friend sender<qt_mainthread_scheduler> tag_invoke(
        stdexec::schedule_t, qt_mainthread_scheduler&& sched)
    {
      return {PIKA_MOVE(sched)};
    }

    friend sender<qt_mainthread_scheduler> tag_invoke(
        stdexec::schedule_t, qt_mainthread_scheduler const& sched)
    {
      return {sched};
    }
  };
}    // namespace grox::senders

namespace grox::senders {
  using qt_mainthread_scheduler = grox::senders::qt_mainthread_scheduler;
}    // namespace grox::senders
