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
#include <QCoreApplication>
#include <QMainWindow>

// Quick and dirty scheduler to invoke function on Qt mainwin thread
// Mainwindow must have an invokable function with the (arbitrary) signature used here
//
// Q_INVOKABLE bool schedule_function(qt_function_type func);

// @TODO: Remove stdexec::detail __xxx usage

namespace grox::senders {
  using qt_function_type = std::function<void(void)>;

  inline QMainWindow* getMainWindow()
  {
    foreach (QWidget* w, qApp->topLevelWidgets())
      if (QMainWindow* mainWin = qobject_cast<QMainWindow*>(w))
        return mainWin;
    return nullptr;
  }
}    // namespace grox::senders

namespace stdexec {

  namespace qt_detail {
    struct qt_schedule_t
    {
    };

    struct scheduler
    {
      template <class Tag = qt_schedule_t>
      friend auto tag_invoke(schedule_t, scheduler)
      {
        return __make_sexpr<Tag>();
      }

      friend forward_progress_guarantee tag_invoke(
        get_forward_progress_guarantee_t, scheduler) noexcept
      {
        return forward_progress_guarantee::weakly_parallel;
      }

      bool operator==(const scheduler&) const noexcept = default;
    };
  }    // namespace qt_detail

  template <>
  struct __sexpr_impl<qt_detail::qt_schedule_t> : __sexpr_defaults
  {
    static constexpr auto get_attrs =    //
      [](__ignore) noexcept
      -> __env::__with<qt_detail::scheduler, get_completion_scheduler_t<set_value_t>> {
      return __env::__with(qt_detail::scheduler{}, get_completion_scheduler<set_value_t>);
    };

    static constexpr auto get_completion_signatures =    //
      [](__ignore, __ignore) noexcept -> completion_signatures<set_value_t()> { return {}; };

    static constexpr auto start =    //
      []<class Receiver>(__ignore, Receiver& rcvr) noexcept -> void {
      QMainWindow* mainwin = grox::senders::getMainWindow();

      // Create a lambda that calls the continuation, and then pass that to the mainwindow
      // schedule_function member that will be called via invoke on the Qt application thread
      grox::senders::qt_function_type func = [rcvr = std::move(rcvr)]() {
        set_value((Receiver &&) rcvr);
      };

      // Do not use DirectConnection as it will execute on the same thread
      QMetaObject::invokeMethod(mainwin, "schedule_function", Qt::AutoConnection,
        Q_ARG(grox::senders::qt_function_type, func));
    };
  };
}    // namespace stdexec

namespace grox::senders {
  using qt_mainthread_scheduler = stdexec::qt_detail::scheduler;
}    // namespace grox::senders
