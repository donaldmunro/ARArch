#ifndef _TBBRouter_H
#define _TBBRouter_H

#include <cstdio>

#include <tbb/flow_graph.h>

#include "mar/render/Renderer.h"
#include "mar/Repository.h"
#include "mar/architecture/tbb/TBBDetector.h"
#include "mar/architecture/tbb/TBBTracker.h"

namespace toMAR
{
    using RouterOutputTuple = std::tuple<uint64_t, uint64_t, uint64_t, //detector, tracker, renderer x 4
                                         uint64_t, uint64_t, uint64_t,
                                         uint64_t, uint64_t, uint64_t,
                                         uint64_t, uint64_t, uint64_t>;

   struct TBBRouterParameters
   {
      int portStart = -1;
      Detector* detector = nullptr;
      Tracker* tracker = nullptr;
      Renderer* renderer = nullptr;
      bool isDelete = false;

      TBBRouterParameters(int detectorPort, Detector *detector, Tracker *tracker,
                          Renderer *renderer, bool isDelete) : portStart(detectorPort),
                                                               detector(detector), tracker(tracker),
                                                               renderer(renderer),
                                                               isDelete(isDelete)
      {}
   };


   class TBBRouter
   //=============
   {
   public:
      explicit TBBRouter(std::unordered_map<unsigned long, TBBRouterParameters>& map,
                         std::string name = "") :
            repository(Repository::instance()), map(map), name(name)
      {}

      void operator()(const uintptr_t in,
                      tbb::flow::multifunction_node<uintptr_t,
                      RouterOutputTuple>::output_ports_type& out);

      void set_name(std::string n) { name = n; }
      std::string get_name() { return name; }

   private:
      Repository* repository;
      std::unordered_map<unsigned long, TBBRouterParameters> map;
      std::string name;
   };
}
#endif
