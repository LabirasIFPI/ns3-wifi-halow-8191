// Synthetic received TIM regression; no AP association or radio reception is tested.
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/extension-headers.h"
#include <iostream>

using namespace ns3;

namespace ns3 {
class StaWholePageSleepCheck
{
public:
  static void Receive (Ptr<StaWifiMac> sta, S1gBeaconHeader beacon,
                       uint32_t aid, bool next)
  {
    sta->SetAID (aid);
    if (next)
      {
        sta->GoToSleepNextTIM (beacon);
      }
    else
      {
        // Exercise the early-return path, including an empty downlink bitmap.
        sta->S1gTIMReceived (beacon);
      }
  }
};
}

static uint32_t checks = 0;
static void CheckSleep (Ptr<WifiPhy> phy, bool expected)
{
  NS_ABORT_MSG_IF (phy->IsStateSleep () != expected, "Unexpected PHY sleep state");
  std::cout << "time_us=" << Simulator::Now ().GetMicroSeconds ()
            << " sleeping=" << phy->IsStateSleep () << std::endl;
  ++checks;
}

int main (int argc, char **argv)
{
  uint32_t period = 4;
  uint32_t aid = 1;
  uint32_t page = 0;
  bool next = false;
  bool buffered = false;
  bool precedingBlocks = false;
  CommandLine cmd;
  cmd.AddValue ("period", "Whole-page recurrence in beacons", period);
  cmd.AddValue ("aid", "Synthetic STA AID", aid);
  cmd.AddValue ("page", "Announced page", page);
  cmd.AddValue ("next", "Sleep starting at the next beacon boundary", next);
  cmd.AddValue ("buffered", "Synthetic TIM announces data for this STA", buffered);
  cmd.AddValue ("precedingBlocks", "Add preceding multi-subblock TIM blocks", precedingBlocks);
  cmd.Parse (argc, argv);
  NS_ABORT_MSG_IF (period < 1 || period > 4 || aid < 1 || aid > 8191 || page > 3,
                   "Invalid check parameters");
  NS_ABORT_MSG_IF (next && (aid >> 11) != page, "Next-TIM case expects our own page");
  NS_ABORT_MSG_IF (buffered && (next || (aid >> 11) != page), "Buffered case requires our page and receive path");

  NodeContainer nodes;
  nodes.Create (1);
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ChannelWidth", UintegerValue (2));
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ah);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
      "DataMode", StringValue ("OfdmRate650KbpsBW2MHz"),
      "ControlMode", StringValue ("OfdmRate650KbpsBW2MHz"));
  S1gWifiMacHelper mac = S1gWifiMacHelper::Default ();
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("sleep-check")),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);
  Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (devices.Get (0));
  Ptr<StaWifiMac> sta = DynamicCast<StaWifiMac> (device->GetMac ());

  pageSlice slice;
  slice.SetPageindex (page);
  slice.SetPagePeriod (period);
  slice.SetPageSliceCount (0);
  slice.SetPageSliceLen (1);
  slice.SetBlockOffset (0);
  slice.SetTIMOffset (0);
  slice.SetPageBitmap (buffered ? (uint32_t (1) << ((aid >> 6) & 31)) : 0);
  TIM tim;
  tim.SetBitmapControl (0);
  tim.SetPageIndex (page);
  tim.SetPageSliceNum (31);
  tim.SetDTIMPeriod (1);
  tim.SetDTIMCount (0);
  if (buffered)
    {
      if (precedingBlocks) {
        for (uint8_t index = 0; index < ((aid >> 6) & 31); ++index) {
          TIM::EncodedBlock preceding;
          preceding.SetBlockOffset(index);
          preceding.SetBlockBitmap(0x82); // bits 1 and 7 expose the legacy ==1 bug.
          uint8_t bits[] = {0x55, 0xaa};
          preceding.SetEncodedInfo(bits, 2);
          tim.SetPartialVBitmap(preceding);
        }
      }
      TIM::EncodedBlock block;
      block.SetBlockOffset ((aid >> 6) & 31);
      block.SetBlockBitmap (1 << ((aid >> 3) & 7));
      uint8_t bitmap = 1 << (aid & 7);
      block.SetEncodedInfo (&bitmap, 1);
      tim.SetPartialVBitmap (block);
    }
  S1gBeaconCompatibility compatibility;
  compatibility.SetBeaconInterval (102400);
  S1gBeaconHeader beacon;
  beacon.SetBeaconCompatibility (compatibility);
  beacon.SetpageSlice (slice);
  beacon.SetTIM (tim);
  // Overwrite originals to ensure the beacon owns its copied bitmap bytes.
  tim.GetPartialVBitmap()[0] = 0;
  slice.SetPageBitmap(0x01010101);
  Simulator::Schedule (MicroSeconds (5000), &StaWholePageSleepCheck::Receive,
                       sta, beacon, aid, next);
  Simulator::Schedule (MicroSeconds (6000), &CheckSleep, device->GetPhy (), !next && !buffered);
  const uint32_t wake = ((aid >> 11) == page ? period : 1) * 102400;
  Simulator::Schedule (MicroSeconds (wake - 1), &CheckSleep,
                       device->GetPhy (), (!next && !buffered) || period > 1);
  Simulator::Schedule (MicroSeconds (wake + 1), &CheckSleep, device->GetPhy (), false);
  Simulator::Stop (MicroSeconds (wake + 2));
  Simulator::Run ();
  NS_ABORT_MSG_IF (checks != 3, "Not all PHY observations ran");
  Simulator::Destroy ();
  std::cout << "PASS: synthetic TIM sleep/wake; expected wake_us=" << wake << std::endl;
}
