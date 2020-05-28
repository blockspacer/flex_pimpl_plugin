#pragma once

#include <string>

#define GEN_CAT(a, b) GEN_CAT_I(a, b)
#define GEN_CAT_I(a, b) GEN_CAT_II(~, a ## b)
#define GEN_CAT_II(p, res) res

#define GEN_UNIQUE_NAME(base) GEN_CAT(base, __COUNTER__)

#define _injectPimplStorage(settings) \
  __attribute__((annotate("{gen};{funccall};inject_pimpl_storage(" settings ")")))

#define _injectPimplMethodCalls(settings) \
  __attribute__((annotate("{gen};{funccall};inject_pimpl_method_calls(" settings ")")))

#define _reflectForPimpl(settings) \
  __attribute__((annotate("{gen};{funccall};reflect_for_pimpl(" settings ")")))

#define _skipForPimpl() \
  __attribute__((annotate("skip_pimpl")))
