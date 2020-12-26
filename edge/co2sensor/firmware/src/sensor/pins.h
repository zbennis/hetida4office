#if defined(PINS_CUSTOM)
  #include "pins/custom.h"
#elif defined(PINS_ITSYBITSY)
  #include "pins/itsybitsy.h"
#else
  #error "Pin assignment is not known."
#endif
