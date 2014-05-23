#ifndef OBELISK_DEFINE_HPP
#define OBELISK_DEFINE_HPP

#include <bitcoin/bitcoin.hpp>

// Now we use the generic helper definitions in libbitcoin to
// define BCS_API and BCS_INTERNAL.
// BCS_API is used for the public API symbols. It either DLL imports or
// DLL exports (or does nothing for static build)
// BCS_INTERNAL is used for non-api symbols.

#if defined BCS_STATIC
#define BCS_API
#define BCS_INTERNAL
#elif defined BCS_DLL
#define BCS_API      BC_HELPER_DLL_EXPORT
#define BCS_INTERNAL BC_HELPER_DLL_LOCAL
#else
#define BCS_API      BC_HELPER_DLL_IMPORT
#define BCS_INTERNAL BC_HELPER_DLL_LOCAL
#endif

#endif

