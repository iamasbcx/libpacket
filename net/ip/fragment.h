#ifndef NET_IP_FRAGMENT_H
#define NET_IP_FRAGMENT_H

#include <stdint.h>

namespace net {
  namespace ip {
    // IP fragment.
    class fragment {
      public:
        // Constructor.
        fragment() = default;
        fragment(uint16_t offset, const void* data, uint16_t len, bool last);

        // Destructor.
        ~fragment() = default;

        // Assign.
        void assign(uint16_t offset, const void* data, uint16_t len, bool last);

        // Get offset.
        uint16_t offset() const;

        // Get data.
        const void* data() const;

        // Get length.
        uint16_t length() const;

        // Last fragment?
        bool last() const;

      private:
        // Offset.
        uint16_t _M_offset;

        // Data.
        const void* _M_data;

        // Length.
        uint16_t _M_length;

        // Last fragment?
        bool _M_last;

        // Disable copy constructor and assignment operator.
        fragment(const fragment&) = delete;
        fragment& operator=(const fragment&) = delete;
    };

    inline fragment::fragment(uint16_t offset,
                              const void* data,
                              uint16_t len,
                              bool last)
      : _M_offset(offset),
        _M_data(data),
        _M_length(len),
        _M_last(last)
    {
    }

    inline void fragment::assign(uint16_t offset,
                                 const void* data,
                                 uint16_t len,
                                 bool last)
    {
      _M_offset = offset;
      _M_data = data;
      _M_length = len;
      _M_last = last;
    }

    inline uint16_t fragment::offset() const
    {
      return _M_offset;
    }

    inline const void* fragment::data() const
    {
      return _M_data;
    }

    inline uint16_t fragment::length() const
    {
      return _M_length;
    }

    inline bool fragment::last() const
    {
      return _M_last;
    }
  }
}

#endif // NET_IP_FRAGMENT_H
