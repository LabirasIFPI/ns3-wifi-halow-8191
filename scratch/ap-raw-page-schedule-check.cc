// Tests AP timing calculations, not transmission over the radio.
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include <iostream>

using namespace ns3;

namespace ns3 {
class ApRawPageScheduleCheck
{
public:
  static void CheckDelays (Ptr<ApWifiMac> ap)
  {
    const uint8_t order[] = {2, 0, 3, 1};
    for (uint32_t current = 0; current < 4; ++current)
      {
        ap->m_pageslice.SetPageindex (order[current]);
        ap->m_lastBeaconTime = Seconds (0);
        for (uint32_t target = 0; target < 4; ++target)
          {
            const uint16_t aid = (uint16_t (order[target]) << 11) | 1;
            const uint32_t intervals = target > current ? target - current : target + 4 - current;
            const Time expected = MicroSeconds (102400 * intervals) - Simulator::Now ();
            NS_ABORT_MSG_IF (ap->GetNextRawPageBeaconDelay (aid) != expected,
                             "Incorrect future page beacon delay");
          }
      }
    std::cout << "PASS: 16 page transitions at time_us="
              << Simulator::Now ().GetMicroSeconds () << std::endl;
  }

  static void Run (bool missing)
  {
    Ptr<ApWifiMac> ap = CreateObject<ApWifiMac> ();
    ap->SetAttribute ("BeaconGeneration", BooleanValue (false));
    ap->SetAttribute ("RotateRawPages", BooleanValue (true));
    ap->SetBeaconInterval (MicroSeconds (102400));
    ap->m_pageslice.SetBlockOffset (0);
    ap->m_pageslice.SetPageSliceCount (0);
    const uint8_t order[] = {2, 0, 3, 1};
    for (uint8_t page : order)
      {
        RPS *rps = new RPS;
        for (uint32_t half = 0; half < 2; ++half)
          {
            RPS::RawAssignment group;
            group.SetRawControl (0);
            group.SetSlotCrossBoundary (1);
            group.SetSlotFormat (1);
            group.SetSlotDurationCount (half == 0 ? 10 : 20);
            group.SetSlotNum (3);
            const uint32_t first = half == 0 ? (page == 0 ? 1 : 0) : 1024;
            const uint32_t last = half == 0 ? 1023 : (missing && page == 3 ? 2046 : 2047);
            group.SetRawGroup ((last << 13) | (first << 2) | page);
            rps->SetRawAssignment (group);
          }
        ap->m_rpsset.rpsset.push_back (rps);
      }
    if (missing)
      {
        ap->GetSlotStartTimeFromAid (8191); // Must reject an uncovered AID.
        NS_FATAL_ERROR ("Uncovered AID was accepted");
      }
    for (uint16_t aid = 1; aid <= 8191; ++aid)
      {
        const uint16_t local = aid & 2047;
        const uint64_t expected = local < 1024 ? 1700 * (local % 3)
                                               : 5100 + 2900 * (local % 3);
        NS_ABORT_MSG_IF (ap->GetSlotStartTimeFromAid (aid) != MicroSeconds (expected),
                         "Incorrect group/slot offset for AID " << aid);
      }
    std::cout << "PASS: AIDs 1..8191, 2 groups/page, 3 slots/group, page order 2,0,3,1" << std::endl;
    CheckDelays (ap);
    Simulator::Schedule (MicroSeconds (5000), &CheckDelays, ap);
    Simulator::Schedule (MicroSeconds (102399), &CheckDelays, ap);
    Simulator::Run ();
    Simulator::Destroy ();
    for (auto rps : ap->m_rpsset.rpsset)
      {
        delete rps;
      }
    ap->m_rpsset.rpsset.clear ();
    ap->Dispose ();
  }
};
}

int main (int argc, char **argv)
{
  bool missing = false;
  CommandLine cmd;
  cmd.AddValue ("missing", "Negative test: omit AID 8191 from RAW", missing);
  cmd.Parse (argc, argv);
  ApRawPageScheduleCheck::Run (missing);
}
