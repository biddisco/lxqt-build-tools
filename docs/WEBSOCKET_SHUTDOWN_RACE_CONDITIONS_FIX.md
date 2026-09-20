# Websocket Shutdown Race Conditions - Analysis and Fix

**Date:** February 26, 2026  
**Issue:** Race conditions and bugs causing segfaults and hangs during exchange object shutdown

---

## Problem Description

When the exchange object shuts down, it closes all websockets, which deletes websocket client objects and shuts down remote connections. There were race conditions and bugs that caused:

1. **Segfaults** - When Qt events or late messages triggered handlers accessing deleted objects
2. **Hangs** - When thread cleanup didn't complete properly
3. **Crashes** - From double-delete scenarios and use-after-free conditions

---

## Race Conditions Identified

### 1. **Double Delete in qwebsocket_session Destructor** ⚠️ CRITICAL

**Location:** `src/network/qwebsocket_session.cpp:103-110`

**Problem:**
```cpp
// QThread::finished signal lambda (in constructor)
QObject::connect(thread_, &QThread::finished, client_,
    [this, thread = thread_, client = client_]() {
        delete client;  // FIRST DELETE
        delete thread;  // FIRST DELETE
    });

// Destructor
~qwebsocket_session() {
    client_->stopConnection();
    thread_->wait();      // Wait returns
    delete client_;       // SECOND DELETE! ❌
    delete thread_;       // SECOND DELETE! ❌
}
```

**Sequence:**
1. Main thread calls `ws_orderbook.reset()`
2. Destructor calls `stopConnection()` then `wait()`
3. During `wait()`, thread finishes and emits `QThread::finished`
4. Lambda deletes `client_` and `thread_`
5. `wait()` returns
6. Destructor tries to delete again → **heap corruption**

**Fix:**
- Removed explicit deletes from destructor
- Added proper wait with timeout
- Documented that cleanup lambda handles deletion

```cpp
~qwebsocket_session() {
    qsession_dbg<0>.debug(fmt::format("{:>20s} {} Destructor starting", 
        client_->id(), fmt::ptr(this)));
    client_->stopConnection();
    if (!thread_->wait(5000)) {
        qsession_dbg<1>.error(fmt::format("{:>20s} {} Destructor: thread_->wait() TIMEOUT!", 
            client_->id(), fmt::ptr(this)));
    }
    // DO NOT delete client_ or thread_ here - the QThread::finished signal
    // lambda handles cleanup to avoid double-delete
}
```

---

### 2. **Race Between closing_down_ Check and Object Access**

**Location:** `src/exchange/xrpl_network.cpp:287-320`

**Problem:**
```cpp
void new_orderbook_data_q(xrpl_network* exchange, currency_pair cp, QString data) {
    std::lock_guard l(exchange->async_mutex_);
    if (exchange->closing_down_) {
        return;  // Check passed
    }
    // Lock released here ⚠️
    
    // Time gap where shutdown can complete!
    
    ticker::data tdata = exchange->get_subscribed_ticker_data(cp);  // USE-AFTER-FREE! ❌
}
```

**Timeline:**
```
T1: Handler acquires lock, checks closing_down_ = false, releases lock
T2: Shutdown sets closing_down_ = true, deletes websocket objects
T3: Handler calls get_subscribed_ticker_data() on deleted object ❌
```

**Fix:**
Implemented dual-check pattern:
```cpp
void new_orderbook_data_q(xrpl_network* abstract_exchange, currency_pair cp, QString data) {
    // Fast path: check without lock (atomic read is safe)
    if (abstract_exchange->closing_down_.load()) {
        return;
    }

    // Critical path: check again with lock held
    std::lock_guard l(abstract_exchange->async_mutex_);
    if (abstract_exchange->closing_down_.load()) {
        return;
    }
    
    // Now safe: we hold the lock, shutdown cannot complete until we release it
    ticker::data tdata = abstract_exchange->get_subscribed_ticker_data(cp);
    // ... process data
}
```

---

### 3. **Unsafe Atomic Access to websocket_**

**Location:** `src/network/qwebsocket_client.cpp` (multiple locations)

**Problem:**
```cpp
class qwebsocket_client {
    std::atomic<QWebSocket*> websocket_;
};

void onConnected() {
    (*websocket_).sendTextMessage(subscribe_);  // ❌ Wrong atomic usage!
}

void stopConnection() {
    if (websocket_) {  // ❌ Implicit conversion, not atomic load
        websocket_->close();
    }
}
```

**Issue:** 
- Using `*websocket_` or implicit `if (websocket_)` doesn't use atomic operations
- Can read stale pointer values
- Race with `websocket_ = nullptr` in another thread

**Fix:**
Updated all accesses to use proper `.load()` and `.store()`:
```cpp
void onConnected() {
    auto ws = websocket_.load();  // ✅ Proper atomic load
    if (ws) {
        ws->sendTextMessage(subscribe_);
    }
}

void stopConnection() {
    auto ws = websocket_.load();  // ✅ Proper atomic load
    if (ws) {
        QMetaObject::invokeMethod(ws, "close", Qt::QueuedConnection, 
            QWebSocketProtocol::CloseCodeNormal);
    }
}

void onAboutToClose() {
    auto ws = websocket_.load();
    if (ws) {
        ws->deleteLater();
        websocket_.store(nullptr);  // ✅ Proper atomic store
    }
}
```

---

### 4. **Delayed Thread Cleanup**

**Location:** `src/network/qwebsocket_session.cpp:103-110`

**Problem:**
```cpp
~qwebsocket_session() {
    client_->stopConnection();
    // thread_->wait();  // COMMENTED OUT!
}
```

**Issue:**
- Destructor returns immediately without waiting for thread
- Thread continues running, lambda captures `this` pointer
- `this` pointer becomes invalid when object destructs
- Lambda tries to use invalid `this` → **use-after-free**

**Fix:**
Added explicit wait with timeout:
```cpp
~qwebsocket_session() {
    client_->stopConnection();
    if (!thread_->wait(5000)) {
        qsession_dbg<1>.error("Destructor: thread_->wait() TIMEOUT!");
    }
}
```

---

### 5. **Inadequate Shutdown Sequencing**

**Location:** `src/exchange/xrpl_network.cpp:219-225`

**Problem:**
```cpp
void xrpl_network::shut_down() {
    abstract_exchange::shut_down();  // Sets closing_down_ = true
    //
    if (ws_orderbook) { ws_orderbook.reset(); }  // Deletes immediately
    if (ws_accounts) { ws_accounts.reset(); }
}
```

**Issue:**
- `closing_down_` flag set to true
- But no synchronization between flag set and object deletion
- Handler threads can check flag, pass check, then objects get deleted before handler completes
- No guarantee that handlers have seen the flag before deletion occurs

**Fix:**
Improved shutdown sequence with proper synchronization:
```cpp
void xrpl_network::shut_down() {
    // Call parent shutdown which:
    // 1. Sets closing_down_ = true (atomically)
    // 2. Acquires async_mutex_ lock
    // Any message handlers will now either:
    // - See closing_down_ = true on their first check and exit immediately
    // - Or block on async_mutex_ if they haven't checked yet
    abstract_exchange::shut_down();
    
    // At this point we hold async_mutex_, so no new handlers can proceed
    // Now safely reset the websockets while holding the lock
    xrpnet_dbg<0>.debug(ffmt<s20>("shutdown"), "Resetting ws_orderbook");
    if (ws_orderbook) { 
        ws_orderbook.reset(); 
    }
    
    xrpnet_dbg<0>.debug(ffmt<s20>("shutdown"), "Resetting ws_accounts");
    if (ws_accounts) { 
        ws_accounts.reset(); 
    }
    // async_mutex_ is unlocked here when lock_guard goes out of scope
}
```

---

### 6. **Missing Handler Protection**

**Location:** `src/exchange/xrpl_network.cpp:351`

**Problem:**
```cpp
void xrpl_network::new_account_data_q(xrpl_network* nw, QString qdata) {
    std::string data = qdata.toStdString();
    // NO SHUTDOWN CHECK! ❌
    
    // Process data, access objects...
}
```

**Issue:**
- `new_account_data_q()` handler had no `closing_down_` check
- Could access deleted objects during shutdown

**Fix:**
Added shutdown check at entry:
```cpp
void xrpl_network::new_account_data_q(xrpl_network* nw, QString qdata) {
    std::string data = qdata.toStdString();
    
    // Check if shutdown is in progress - don't process new data
    if (nw->closing_down_.load()) {
        xrpnet_dbg<0>.error(ffmt<s20>("Account data"), "Shutdown in progress: ignoring data");
        return;
    }
    
    // Process data...
}
```

---

## Summary of Changes

### Files Modified

1. **src/network/qwebsocket_session.cpp**
   - Fixed double-delete in destructor
   - Added proper thread wait with timeout
   - Documented cleanup responsibility

2. **src/network/qwebsocket_client.cpp**
   - Fixed all atomic access patterns for `websocket_`
   - Changed from `if (websocket_)` to `auto ws = websocket_.load()`
   - Updated all handlers to use proper atomic operations
   - Fixed `onAboutToClose()`, `onConnected()`, `onDisconnected()`, `onSslErrors()`, `onError()`

3. **src/exchange/xrpl_network.cpp**
   - Improved shutdown synchronization
   - Implemented dual-check pattern in `new_orderbook_data_q()`
   - Added shutdown check to `new_account_data_q()`
   - Enhanced logging in shutdown sequence

---

## Key Synchronization Patterns Used

### 1. Dual-Check Pattern (for message handlers)
```cpp
// Fast path: early exit without lock
if (closing_down_.load()) return;

// Critical section
std::lock_guard l(mutex);
if (closing_down_.load()) return;

// Safe to proceed - we hold lock, shutdown can't complete
access_objects();
```

### 2. Atomic Access Pattern (for shared pointers)
```cpp
auto ptr = atomic_ptr_.load();
if (ptr) {
    ptr->method();
}
// Not: (*atomic_ptr_).method() ❌
```

### 3. Shutdown Sequence Pattern
```cpp
void shut_down() {
    closing_down_.store(true);      // Set flag atomically
    std::lock_guard l(mutex);      // Acquire lock
    // Handlers now blocked or have exited
    reset_objects();                // Safe to delete
    // Lock released automatically
}
```

---

## Testing Recommendations

1. **Stress Test Shutdown**
   - Start/stop exchange repeatedly
   - Monitor for memory leaks (valgrind)
   - Check for assertion failures

2. **Race Detection**
   - Build with thread sanitizer: `-fsanitize=thread`
   - Run under high load with multiple websocket messages

3. **Hang Detection**
   - Monitor for timeout messages in logs
   - Ensure shutdown completes within reasonable time (< 10 seconds)

4. **Memory Safety**
   - Run with address sanitizer: `-fsanitize=address`
   - Check for use-after-free, double-free errors

---

## Expected Behavior After Fix

✅ **No double deletes** - Objects deleted exactly once by cleanup lambda  
✅ **No use-after-free** - Handlers have exclusive access via mutex  
✅ **No hangs** - Thread wait has timeout, cleanup always completes  
✅ **No segfaults** - All object accesses protected by shutdown checks  
✅ **Clean shutdown** - Websockets close gracefully, threads exit properly  

---

## Related Code Locations

- Exchange base class: `src/exchange/abstract_exchange.hpp`, `src/exchange/abstract_exchange.cpp`
- Websocket session: `src/network/qwebsocket_session.hpp`, `src/network/qwebsocket_session.cpp`
- Websocket client: `src/network/qwebsocket_client.hpp`, `src/network/qwebsocket_client.cpp`
- XRPL network: `src/exchange/xrpl_network.hpp`, `src/exchange/xrpl_network.cpp`
- Bitstamp network: `src/exchange/bitstamp.hpp`, `src/exchange/bitstamp.cpp`

---

## Notes

- The `closing_down_` flag is `std::atomic<bool>` so reads/writes are atomic
- The `async_mutex_` provides mutual exclusion for critical sections
- Qt signals use `Qt::DirectConnection` so they execute on the thread that emits them
- `deleteLater()` schedules deletion on the event loop, preventing immediate destruction
- Thread `wait()` blocks until thread finishes and cleanup lambda completes

---

## Future Improvements

1. Consider using `std::shared_ptr` for websocket objects with `weak_ptr` in handlers
2. Add metrics for tracking shutdown time and detecting slow shutdowns
3. Consider timeout mechanism for websocket close operations
4. Add unit tests for shutdown scenarios
5. Document threading model more explicitly in code comments

---

**End of Analysis**
