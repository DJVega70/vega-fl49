#pragma once
// Which product this build is: Vega Productions DAW, or the VEGA FL49 edition (FLkey 2 49 at its centre).
// CMake sets VEGA_FL49=1 for the VEGA FL49 build (-DVEGA_FL49=ON).
#ifndef VEGA_FL49
 #define VEGA_FL49 0
#endif
#if VEGA_FL49
 #define VEGA_APP_NAME "VEGA FL49"
#else
 #define VEGA_APP_NAME "Vega Productions DAW"
#endif
