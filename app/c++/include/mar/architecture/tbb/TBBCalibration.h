#ifndef _TBBCALIBRATION_H
#define _TBBCALIBRATION_H

#include <cstdio>

#include <tbb/flow_graph.h>

namespace toMAR
{
   class TBBCalibration
   //=============
   {
   public:
      TBBCalibration() {}

      uint64_t operator()(uint64_t seqno) { return seqno; }
   };
}
#endif
