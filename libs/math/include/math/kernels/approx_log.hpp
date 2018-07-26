#pragma once

namespace fetch {
  namespace kernels {

    template< typename vector_register_type >
    struct ApproxLog {
      void operator() (vector_register_type const &x, vector_register_type &y) const {
        y = approx_log( x );
      }
    };

  }
}
