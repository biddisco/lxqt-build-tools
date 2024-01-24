//  Copyright (c) 2023 ETH Zurich
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <exception>
#include <type_traits>
#include <utility>
//
#include <pika/modules/execution.hpp>
#include <pika/modules/executors.hpp>
#include <pika/modules/resource_partitioner.hpp>
#include <pika/modules/runtime.hpp>
#include <pika/modules/schedulers.hpp>
#include <pika/modules/thread_manager.hpp>

namespace grox::qt::experimental::detail {

  using namespace pika::debug::detail;

  // -----------------------------------------------------------------
  // by convention the title is 7 chars (for alignment)
  template <int Level>
  inline constexpr print_threshold<Level, 4> qt_trig("QT_TRIGG");

  namespace ex = pika::execution::experimental;
  namespace pe = pika::execution;
  using request_callback_function_type = pika::util::detail::unique_function<void(int)>;
  using qt_function_type = std::function<bool(bool)>;

  // -----------------------------------------------------------------
  // return a scheduler on the default pool with added priority if requested
  inline auto default_pool_scheduler(pe::thread_priority p)
  {
    return ex::with_priority(
      ex::thread_pool_scheduler{&pika::resource::get_thread_pool("default")}, p);
  }
  inline auto default_pool_scheduler()
  {
    return ex::thread_pool_scheduler{&pika::resource::get_thread_pool("default")};
  }

  // -----------------------------------------------------------------
  // return a scheduler on the qt main application thread
  inline auto qt_application_scheduler()
  {
    // qt_function_type
    //     MyQObject *myQObject = m_object;
    //     myMessage = QString("TCP event received.");
    //     QMetaObject::invokeMethod(myQObject
    //                                , "mySlotName"
    //                                , Qt::AutoConnection // Can also use any other except DirectConnection
    //                                , Q_ARG(QString, myMessage)); // And some more args if needed
  }

}    // namespace grox::qt::experimental::detail
