#include <new>
#include "net/ip/tcp/connections.h"
#include "net/ip/tcp/flags.h"

void net::ip::tcp::connections::clear()
{
  if (_M_conns) {
    delete [] _M_conns;
    _M_conns = nullptr;
  }

  _M_size = 0;
  _M_nconns = 0;

  _M_free.clear();
}

bool net::ip::tcp::connections::init(size_t size,
                                     size_t maxconns,
                                     uint64_t timeout,
                                     uint64_t time_wait)
{
  // Sanity checks.
  if ((size >= min_size) &&
      (size <= max_size) &&
      ((size & (size - 1)) == 0) &&
      (maxconns >= min_connections) &&
      (maxconns <= max_connections) &&
      (timeout >= min_timeout) &&
      (time_wait >= min_time_wait)) {
    _M_conns = new (std::nothrow) stack[size];

    if (_M_conns) {
      _M_max_connections = maxconns;

      // Allocate free connections.
      if (allocate_connections()) {
        connection::time_wait = time_wait;

        _M_size = size;
        _M_mask = size - 1;

        _M_timeout = timeout * 1000000ull;
        _M_time_wait = time_wait * 1000000ull;

        return true;
      }
    }
  }

  return false;
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(const iphdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp)
{
  direction dir;
  return process_(hash(iphdr, tcphdr), iphdr, tcphdr, timestamp, dir);
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(const iphdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp,
                                   direction& dir)
{
  return process_(hash(iphdr, tcphdr), iphdr, tcphdr, timestamp, dir);
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(const ip6_hdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp)
{
  direction dir;
  return process_(hash(iphdr, tcphdr), iphdr, tcphdr, timestamp, dir);
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(const ip6_hdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp,
                                   direction& dir)
{
  return process_(hash(iphdr, tcphdr), iphdr, tcphdr, timestamp, dir);
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(uint32_t hash,
                                   const iphdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp)
{
  direction dir;
  return process_(hash, iphdr, tcphdr, timestamp, dir);
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(uint32_t hash,
                                   const iphdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp,
                                   direction& dir)
{
  return process_(hash, iphdr, tcphdr, timestamp, dir);
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(uint32_t hash,
                                   const ip6_hdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp)
{
  direction dir;
  return process_(hash, iphdr, tcphdr, timestamp, dir);
}

const net::ip::tcp::connection*
net::ip::tcp::connections::process(uint32_t hash,
                                   const ip6_hdr* iphdr,
                                   const tcphdr* tcphdr,
                                   uint64_t timestamp,
                                   direction& dir)
{
  return process_(hash, iphdr, tcphdr, timestamp, dir);
}

template<typename IpHeader>
const net::ip::tcp::connection*
net::ip::tcp::connections::process_(uint32_t hash,
                                    const IpHeader* iphdr,
                                    const tcphdr* tcphdr,
                                    uint64_t timestamp,
                                    direction& dir)
{
  const uint32_t bucket = hash & _M_mask;

  stack& stack = _M_conns[bucket];

  // Number of connections in the bucket.
  size_t nconns = stack.count();

  // Search connection.
  for (size_t i = stack.count(); i > 0; i--) {
    connection* conn = stack.get(i - 1);

    // If the connection has not been closed or the time wait interval has not
    // elapsed yet...
    if ((conn->state() != connection::state::closed) ||
        (conn->last_timestamp() + _M_time_wait > timestamp)) {
      // If the connection has not expired...
      if (conn->last_timestamp() + _M_timeout > timestamp) {
        // If it is the connection we are looking for...
        if (conn->match(iphdr, tcphdr, dir)) {
          // Process TCP segment.
          if (conn->process(dir, tcphdr->th_flags, timestamp)) {
            stack.count(nconns);

            return conn;
          }

          // If not the last connection...
          if (i < nconns) {
            stack.get()[i - 1] = stack.get()[nconns - 1];
          }

          stack.count(--nconns);

          remove(conn);

          return nullptr;
        }
      } else {
        // If not the last connection...
        if (i < nconns) {
          stack.get()[i - 1] = stack.get()[nconns - 1];
        }

        nconns--;

        remove(conn);
      }
    } else {
      // If not the last connection...
      if (i < nconns) {
        stack.get()[i - 1] = stack.get()[nconns - 1];
      }

      nconns--;

      remove(conn);
    }
  }

  stack.count(nconns);

  // Connection not found.

  // Get free connection.
  connection* conn = get_free_connection();

  if (conn) {
    // Add connection to the bucket.
    if (stack.push(conn)) {
      // If the SYN bit has been set...
      if ((tcphdr->th_flags & syn) != 0) {
        enum connection::state state;

        // If the ACK bit has not been set...
        if ((tcphdr->th_flags & ack) == 0) {
          // Save direction.
          dir = direction::from_client;

          state = connection::state::connection_requested;
        } else {
          // Save direction.
          dir = direction::from_server;

          state = connection::state::connection_established;
        }

        // Initialize connection.
        init(conn, dir, iphdr, tcphdr, state, timestamp);
      } else {
        // Generally server's port is smaller than client's port.
        dir = (ntohs(tcphdr->source) > ntohs(tcphdr->dest)) ?
                direction::from_client :
                direction::from_server;

        // Initialize connection.
        init(conn,
             dir,
             iphdr,
             tcphdr,
             connection::state::data_transfer,
             0);
      }

      // Set timestamp of the last packet.
      conn->touch(timestamp);

      // Increment number of connections.
      _M_nconns++;

      return conn;
    }

    // Free connection.
    if (!_M_free.push(conn)) {
      free(conn);
    }
  }

  return nullptr;
}

void net::ip::tcp::connections::remove_expired(uint64_t now)
{
  for (size_t i = _M_size; i > 0; i--) {
    stack& stack = _M_conns[i - 1];

    // Get connections.
    connection** conns = stack.get();

    size_t k = 0;
    for (size_t j = 0; j < stack.count(); j++) {
      // Get connection.
      connection* conn = conns[j];

      // If the connection has not been closed...
      if (conn->state() != connection::state::closed) {
        // If the connection has not expired...
        if (conn->last_timestamp() + _M_timeout > now) {
          conns[k++] = conns[j];
        } else {
          if (_M_expiredfn) {
            _M_expiredfn(conn, _M_user);
          }

          // Remove connection.
          remove(conn);
        }
      } else if (conn->last_timestamp() + _M_time_wait > now) {
        conns[k++] = conns[j];
      } else {
        if (_M_expiredfn) {
          _M_expiredfn(conn, _M_user);
        }

        // Remove connection.
        remove(conn);
      }
    }

    // Set number of connections.
    stack.count(k);
  }
}

void net::ip::tcp::connections::stack::clear()
{
  if (_M_conns) {
    for (size_t i = _M_used; i > 0; i--) {
      free(_M_conns[i - 1]);
    }

    free(_M_conns);
    _M_conns = nullptr;
  }

  _M_used = 0;
  _M_size = 0;
}

inline bool net::ip::tcp::connections::stack::push(connection* conn)
{
  if (_M_used < _M_size) {
    _M_conns[_M_used++] = conn;
    return true;
  } else {
    const size_t size = (_M_size > 0) ? _M_size * 2 : connection_allocation;

    connection** conns = static_cast<connection**>(
                           realloc(_M_conns, size * sizeof(connection*))
                         );

    if (conns) {
      _M_conns = conns;
      _M_size = size;

      _M_conns[_M_used++] = conn;

      return true;
    }

    return false;
  }
}

bool net::ip::tcp::connections::allocate_connections(size_t count)
{
  // Compute number of connections which can still be created.
  const size_t diff = _M_max_connections - _M_nconns;

  // If new connections can still be created...
  if (diff > 0) {
    if (diff < count) {
      count = diff;
    }

    for (size_t i = count; i > 0; i--) {
      // Create connection.
      connection* conn = static_cast<connection*>(
                           malloc(sizeof(connection))
                         );

      // If the connection could be created.
      if (conn) {
        // Add connection to the free pool.
        if (_M_free.push(conn)) {
          // Set connection id.
          conn->id(_M_connid++);
        } else {
          free(conn);
          break;
        }
      } else {
        break;
      }
    }

    return !_M_free.empty();
  }

  return false;
}
