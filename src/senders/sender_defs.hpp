#pragma once
//
#include <cstdint>
#include <exception>
//
#include <exec/any_sender_of.hpp>
#include <stdexec/execution.hpp>

// ----------------------------------------------------------------------------
template <class... Ts>
using any_sender_of =
    typename exec::any_receiver_ref<stdexec::completion_signatures<Ts...>>::template any_sender<>;

using any_void_sender = any_sender_of<stdexec::set_value_t(),    //
    stdexec::set_stopped_t(), stdexec::set_error_t(std::exception_ptr)>;

using any_bool_sender = any_sender_of<stdexec::set_value_t(bool),    //
    stdexec::set_stopped_t(), stdexec::set_error_t(std::exception_ptr)>;

using any_bytearray_sender = any_sender_of<stdexec::set_value_t(QByteArray),    //
    stdexec::set_stopped_t(), stdexec::set_error_t(std::exception_ptr)>;

using any_uint64_sender = any_sender_of<stdexec::set_value_t(std::uint64_t),    //
    stdexec::set_stopped_t(), stdexec::set_error_t(std::exception_ptr)>;
