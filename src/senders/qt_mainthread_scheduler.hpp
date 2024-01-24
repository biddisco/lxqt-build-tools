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
//
#include "senders/qt_helpers.hpp"

QMainWindow* getMainWindow()
{
  foreach (QWidget* w, qApp->topLevelWidgets())
    if (QMainWindow* mainWin = qobject_cast<QMainWindow*>(w))
      return mainWin;
  return nullptr;
}

namespace stdexec {

  namespace qtmain {
    struct qt_schedule_t
    {
    };

    struct scheduler
    {
      using t = scheduler;
      using id = scheduler;

      template <class Tag = qt_schedule_t>
      STDEXEC_ATTRIBUTE((host, device))
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
  }    // namespace qtmain

  template <>
  struct __sexpr_impl<qtmain::qt_schedule_t> : __sexpr_defaults
  {
    static constexpr auto get_attrs =    //
      [](__ignore) noexcept
      -> __env::__prop<qtmain::scheduler(get_completion_scheduler_t<set_value_t>)> {
      return __mkprop(qtmain::scheduler{}, get_completion_scheduler<set_value_t>);
    };

    static constexpr auto get_completion_signatures =    //
      [](__ignore, __ignore) noexcept -> completion_signatures<set_value_t()> { return {}; };

    static constexpr auto start =    //
      []<class Receiver>(__ignore, Receiver& rcvr) noexcept -> void {
      QMainWindow* mainwin = getMainWindow();
      // call set_value from inside the lambda on the application thread
      grox::qt::experimental::detail::qt_function_type func = [rcvr = std::move(rcvr)](bool x) {
        set_value((Receiver &&) rcvr);
        return x;
      };

      // Do not use DirectConnection as it will execute on the same thread
      QMetaObject::invokeMethod(mainwin, "schedule_function", Qt::AutoConnection,
        Q_ARG(grox::qt::experimental::detail::qt_function_type, func));
    };
  };
}    // namespace stdexec

namespace grox {
  // A scheduler that executes its continuation on the Qt main thread
  using qt_mainthread_scheduler = stdexec::qtmain::scheduler;
}    // namespace grox
