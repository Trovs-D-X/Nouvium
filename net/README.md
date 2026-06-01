# `net/` Directory — Project Carl

The `net/` directory serves as the networking core of the **Carl** project. It is implemented in **C** and designed to handle low-level network communications, raw socket configurations, and protocol abstraction layers (IPv4/IPv6) efficiently.

---

## 📌 Overview

This component manages everything related to network I/O, socket multiplexing, and packet structures. By separation of concerns, it isolates OS-specific networking APIs from the core logic of Carl, offering a modular framework to expand or audit network protocols.

---

## 🗂️ Directory Structure

The directory is structured into specialized subfolders to segregate protocol logic and transport mechanisms:

* **`ipv4/`**
  Handles standard IPv4 addressing, routing structures, and custom headers. Contains logic for parsing and constructing IPv4 packets.
  
* **`ipv6/`**
  Implements IPv6 next-generation networking protocols, dual-stack configurations, and expanded header handling (`struct sockaddr_in6`).
  
* **`sockets/`**
  Houses core socket abstraction layers. This includes low-level socket creation, binding, listening, non-blocking I/O configurations (`fcntl`, `O_NONBLOCK`), and I/O multiplexing logic (such as `select`, `poll`, or `epoll`).

---

## 🚀 Key Modules & Files

While specific subdirectories contain domain-specific logic, the root of `net/` generally manages general interfaces:

| File / Folder | Responsibility | Key C API / Structures Used |
| :--- | :--- | :--- |
| `sockets/` | Raw/Stream socket management, multiplexing | `socket()`, `setsockopt()`, `epoll_wait()` |
| `ipv4/` | IPv4 packet manipulation and utilities | `struct sockaddr_in`, `inet_pton()` |
| `ipv6/` | IPv6 packet handling & modern routing | `struct sockaddr_in6`, `inet_ntop()` |
| `net_utils.c` | Hex-dumping network streams, byte-ordering | `htons()`, `ntohl()`, `memcpy()` |
| `errors.c` | Network error translation and logging | `errno`, `strerror()`, `gai_strerror()` |

---

## 🛠️ Low-Level Implementations & Concepts

### 1. Concurrency & Non-Blocking Sockets
To support simultaneous connections without blocking execution, sockets are configured asynchronously:
```c
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
