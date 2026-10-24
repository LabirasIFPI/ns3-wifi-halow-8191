// Focused AP announcement check; does not validate STA sleep or data delivery.
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/extension-headers.h"
#include <iostream>

using namespace ns3;

static uint32_t observed = 0;
static bool rotate = true;

static void CheckBeacon (S1gBeaconHeader beacon, RPS::RawAssignment)
{
  const uint8_t expected = rotate ? observed % 4 : 0;
  NS_ABORT_MSG_IF (beacon.GetTIM ().GetPageIndex () != expected,
                   "Unexpected TIM page");
  NS_ABORT_MSG_IF (beacon.GetpageSlice ().GetPageindex () != expected,
                   "Unexpected Page Slice page");
  NS_ABORT_MSG_IF (beacon.GetRPS ().GetRawAssigmentObj (0).GetRawGroupPage () != expected,
                   "RAW and announced page differ");
  NS_ABORT_MSG_IF (beacon.GetTIM ().GetDTIMCount () != 0 ||
                   beacon.GetTIM ().GetDTIMPeriod () != 1,
                   "Each beacon must be a DTIM in this profile");
  NS_ABORT_MSG_IF (beacon.GetpageSlice ().GetPagePeriod () != (rotate ? 4 : 1),
                   "Incorrect page recurrence period");
  std::cout << "beacon=" << observed << " page=" << unsigned (expected)
            << " time_s=" << Simulator::Now ().GetSeconds () << std::endl;
  ++observed;
}

int main (int argc, char **argv)
{
  bool mixedPages = false;
  bool wrongPeriod = false;
  CommandLine cmd;
  cmd.AddValue ("rotate", "Enable page rotation", rotate);
  cmd.AddValue ("mixedPages", "Negative test: two pages in one RPS", mixedPages);
  cmd.AddValue ("wrongPeriod", "Negative test: incorrect page period", wrongPeriod);
  cmd.Parse (argc, argv);

  RPSVector raw;
  for (uint8_t page = 0; page < (rotate ? 4 : 1); ++page)
    {
      RPS *rps = new RPS;
      RPS::RawAssignment group;
      group.SetRawControl (0);
      group.SetSlotCrossBoundary (1);
      group.SetSlotFormat (1);
      group.SetSlotDurationCount (160);
      group.SetSlotNum (1);
      group.SetRawGroup ((2047u << 13) | ((page == 0 ? 1u : 0u) << 2) | page);
      rps->SetRawAssignment (group);
      if (mixedPages && page == 0)
        {
          group.SetRawGroup ((2047u << 13) | 1u);
          rps->SetRawAssignment (group);
        }
      raw.rpsset.push_back (rps);
    }
  pageSlice slice;
  slice.SetPageindex (0);
  slice.SetPagePeriod (rotate && !wrongPeriod ? 4 : 1);
  slice.SetPageSliceCount (0);
  slice.SetPageSliceLen (1);
  slice.SetBlockOffset (0);
  slice.SetTIMOffset (0);
  TIM tim;
  tim.SetPageIndex (0);
  tim.SetDTIMPeriod (1);
  tim.SetDTIMCount (0);

  NodeContainer ap;
  ap.Create (1);
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ap);
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
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (Ssid ("page-check")),
               "NRawStations", UintegerValue (0), "RPSsetup", RPSVectorValue (raw),
               "PageSliceSet", pageSliceValue (slice), "TIMSet", TIMValue (tim),
               "RotateRawPages", BooleanValue (rotate));
  NetDeviceContainer devices = wifi.Install (phy, mac, ap);
  Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (devices.Get (0));
  bool connected = device->GetMac ()->TraceConnectWithoutContext (
      "S1gBeaconBroadcasted", MakeCallback (&CheckBeacon));
  NS_ABORT_MSG_IF (!connected, "Beacon trace not connected");
  Simulator::Stop (Seconds (0.8));
  Simulator::Run ();
  NS_ABORT_MSG_IF (observed != 8, "Expected eight beacon construction events");
  Simulator::Destroy ();
  for (auto rps : raw.rpsset)
    {
      delete rps;
    }
  std::cout << "PASS: AP announcement fields only; STA behavior not tested." << std::endl;
}
