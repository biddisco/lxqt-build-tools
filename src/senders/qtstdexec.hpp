/*
 * Taken from https://git.qt.io/vivoutil/libunifex-with-qt
 *            https://git.qt.io/vivoutil/libunifex-with-qt/-/blob/main/qtstdexec.h
 * Ville Voutilainen
 */
#pragma once

#ifndef QTHREADSENDER_H
# define QTHREADSENDER_H

# include <QAbstractEventDispatcher>
# include <QCoreApplication>
# include <QEventLoop>
# include <QMetaObject>
# include <QObject>
# include <QThread>
# include <QTimer>
# include <exception>
# include <exec/async_scope.hpp>
# include <exec/materialize.hpp>
# include <exec/start_now.hpp>
# include <stdexec/concepts.hpp>
# include <stdexec/execution.hpp>
# include <tuple>
# include <type_traits>

namespace QtStdExec {

  template <class Recv>
  class QThreadOperationState;

  class QThreadScheduler
  {
public:
    QThreadScheduler()
      : m_thread(QCoreApplication::instance()->thread())
    {
    }

    explicit QThreadScheduler(QThread* thread)
      : m_thread(thread)
    {
    }
    QThread* thread() { return m_thread; }
    struct default_env
    {
      QThread* thread;

      // Newer stdexec queries environments through a member query() function.
      template <typename CPO>
      QThreadScheduler query(stdexec::get_completion_scheduler_t<CPO>) const noexcept
      {
        return QThreadScheduler(thread);
      }
    };

    class QThreadSender
    {
  public:
      using sender_concept = stdexec::sender_t;
      using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(),
          stdexec::set_error_t(std::exception_ptr)>;

      explicit QThreadSender(QThread* thread)
        : m_thread(thread)
      {
      }
      QThread* thread() { return m_thread; }

      default_env get_env() const& noexcept { return {m_thread}; }

      template <class Recv>
      QThreadOperationState<Recv> connect(Recv&& receiver)
      {
        return QThreadOperationState<Recv>(std::move(receiver), thread());
      }

  private:
      QThread* m_thread;
    };

    QThreadSender schedule() const { return QThreadSender(m_thread); }

    default_env get_env() const& noexcept { return {m_thread}; }

    friend bool operator==(QThreadScheduler const& a, QThreadScheduler const& b) noexcept
    {
      return a.m_thread == b.m_thread;
    }
    friend bool operator!=(QThreadScheduler const& a, QThreadScheduler const& b) noexcept
    {
      return a.m_thread != b.m_thread;
    }

private:
    QThread* m_thread = nullptr;
  };

  inline QThreadScheduler qThreadAsScheduler(QThread* thread) { return QThreadScheduler(thread); }

  inline QThreadScheduler qThreadAsScheduler(QThread& thread) { return QThreadScheduler(&thread); }

  template <class Recv>
  class QThreadOperationState
  {
public:
    using operation_state_concept = stdexec::operation_state_t;
    QThreadOperationState(Recv&& receiver, QThread* thread)
      : m_receiver(std::move(receiver))
      , m_thread(thread)
    {
    }
    void start() noexcept
    {
      QMetaObject::invokeMethod(
          m_thread->eventDispatcher(), [this]() { stdexec::set_value(std::move(m_receiver)); },
          Qt::QueuedConnection);
    }

private:
    Q_DISABLE_COPY_MOVE(QThreadOperationState)
    Recv m_receiver;
    QThread* m_thread;
  };

  template <class Recv, class QObj, class Ret, class... Args>
  class QObjectOperationState;

  template <class QObj, class Ret, class... Args>
  class QObjectSender
  {
    struct default_env
    {
      QThread* thread;

      template <typename CPO>
      QThreadScheduler query(stdexec::get_completion_scheduler_t<CPO>) const noexcept
      {
        return QThreadScheduler(thread);
      }
    };

public:
    using sender_concept = stdexec::sender_t;
    using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(Args...),
        stdexec::set_error_t(std::exception_ptr), stdexec::set_stopped_t()>;

    using m_ptr_type = Ret (QObj::*)(Args...);
    QObjectSender(QObj* obj, m_ptr_type ptr)
      : m_obj(obj)
      , m_ptr(ptr)
    {
    }
    QObj* object() { return m_obj; }
    m_ptr_type member_ptr() { return m_ptr; }

    default_env get_env() const& noexcept { return {m_obj->thread()}; }

    template <class Recv>
    QObjectOperationState<Recv, QObj, Ret, Args...> connect(Recv&& receiver)
    {
      return QObjectOperationState<Recv, QObj, Ret, Args...>(std::move(receiver), m_obj, m_ptr);
    }

private:
    QObj* m_obj;
    m_ptr_type m_ptr;
  };

  template <class Recv, class QObj, class Ret, class... Args>
  class QObjectOperationState
  {
public:
    using operation_state_concept = stdexec::operation_state_t;
    using m_ptr_type = Ret (QObj::*)(Args...);
    QObjectOperationState(Recv&& receiver, QObj* obj, m_ptr_type ptr)
      : m_receiver(std::move(receiver))
      , m_obj(obj)
      , m_ptr(ptr)
    {
    }

private:
    struct stop_callback_t
    {
      QObjectOperationState* self;

      void operator()() const noexcept
      {
        self->m_stop_callback.reset();
        QObject::disconnect(self->m_connection);
        if (!self->m_completed.test_and_set(std::memory_order_acq_rel))
        {
          QMetaObject::invokeMethod(
              self->m_obj->thread()->eventDispatcher(),
              [this]() { stdexec::set_stopped(std::move(self->m_receiver)); },
              Qt::QueuedConnection);
        }
      }
    };

private:
    using stop_token_type = stdexec::stop_token_of_t<stdexec::env_of_t<Recv>>;
    using stop_callback_type = typename stop_token_type::template callback_type<stop_callback_t>;

public:
    void start() noexcept
    {
      m_stop_callback.emplace(
          stdexec::get_stop_token(stdexec::get_env(m_receiver)), stop_callback_t{this});
      m_connection = QObject::connect(
          m_obj, m_ptr, m_obj,
          [this](Args... args) {
            QObject::disconnect(m_connection);
            m_stop_callback.reset();
            if (!m_completed.test_and_set(std::memory_order_acq_rel))
            {
              QMetaObject::invokeMethod(
                  m_obj,
                  [this, &args...] {
                    stdexec::set_value(std::move(m_receiver), std::forward<Args>(args)...);
                  },
                  Qt::QueuedConnection);
            }
          },
          Qt::SingleShotConnection);
    }
    ~QObjectOperationState() {}

private:
    Recv m_receiver;
    QObj* m_obj;
    m_ptr_type m_ptr;
    QMetaObject::Connection m_connection;
    std::atomic_flag m_completed{false};
    std::optional<stop_callback_type> m_stop_callback;
  };

  template <class QObj, class Ret, class... Args>
  inline QObjectSender<QObj, Ret, Args...> qObjectAsSender(QObj* obj, Ret (QObj::*ptr)(Args...))
  {
    return QObjectSender<QObj, Ret, Args...>(obj, ptr);
  }

  template <class QObj, class Ret, class... Args>
  inline auto qObjectAsTupleSender(QObj* obj, Ret (QObj::*ptr)(Args...))
  {
    return QObjectSender<QObj, Ret, Args...>(obj, ptr) | stdexec::then([](Args... args) {
      return std::tuple<std::remove_reference_t<Args>...>(std::move(args)...);
    });
  }

  struct QEventLoopWaitReceiver
  {
    using receiver_concept = stdexec::receiver_t;
    void set_value(auto&&...) noexcept {}
    void set_error(auto&&) noexcept {}
    void set_stopped() noexcept {}
  };

  template <class Sender>
  auto qEventLoopWait(Sender&& sender)
  {
    QEventLoop nested_loop;
    QTimer loop_end_timer{&nested_loop};
    loop_end_timer.setSingleShot(true);
    QObject::connect(&loop_end_timer, &QTimer::timeout, [&] { nested_loop.quit(); });
    auto wrapped_sender = std::forward<Sender>(sender) | exec::materialize();
    using result = stdexec::value_types_of_t<decltype(wrapped_sender)>;
    std::optional<result> res;
    auto result_sender = std::move(wrapped_sender) |
        stdexec::then([&res](auto tag, auto&&... args) {
          res.emplace(result(std::tuple(tag, std::forward<decltype(args)>(args)...)));
        }) |
        stdexec::continues_on(qThreadAsScheduler(QCoreApplication::instance()->thread())) |
        stdexec::then([&loop_end_timer](auto&&...) { loop_end_timer.start(); });
    auto opstate = stdexec::connect(std::move(result_sender), QEventLoopWaitReceiver());
    stdexec::start(opstate);
    nested_loop.exec();
    return res;
  }

  class QAsyncScopeGuard
  {
private:
    exec::async_scope& m_scope;

public:
    QAsyncScopeGuard(exec::async_scope& scope)
      : m_scope(scope)
    {
    }
    ~QAsyncScopeGuard()
    {
      auto cleanupSender = m_scope.on_empty();
      m_scope.request_stop();
      qEventLoopWait(cleanupSender);
    }
  };

}    // namespace QtStdExec

#endif    // QTHREADSENDER_H
