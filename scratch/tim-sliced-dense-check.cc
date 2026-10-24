// Dense bitmap roundtrip and actual STA parser; synthetic reception, no radio capacity claim.
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/extension-headers.h"
#include <iostream>
using namespace ns3;
namespace ns3 {
class StaWholePageSleepCheck {
public:
 static bool Read (Ptr<StaWifiMac> sta, uint32_t aid, S1gBeaconHeader beacon) {
   sta->SetAID (aid); sta->WakeUp (); sta->S1gTIMReceived (beacon);
   return sta->m_dataBuffered;
 }
};
}
int main () {
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


  uint32_t checked = 0;
  for (uint32_t page = 0; page < 4; ++page) {
    S1gBeaconHeader announcements[2];
    for (uint8_t slice = 0; slice < 2; ++slice) {
      TIM tim; tim.SetDTIMPeriod (2); tim.SetDTIMCount (slice);
      tim.SetPageIndex (page); tim.SetPageSliceNum (slice);
      for (uint8_t block = slice * 16; block < (slice + 1) * 16; ++block) {
        TIM::EncodedBlock encoded; encoded.SetBlockOffset (block); encoded.SetBlockBitmap (255);
        uint8_t bits[8] = {255,255,255,255,255,255,255,255};
        if (page == 0 && block == 0) bits[0] &= 254; // AID 0 is reserved.
        encoded.SetEncodedInfo (bits, 8); tim.SetPartialVBitmap (encoded);
      }
      NS_ABORT_MSG_IF (tim.m_length != 160, "Dense slice must fit in 160 bytes");
      Buffer buffer; buffer.AddAtEnd (tim.GetSerializedSize ()); tim.Serialize (buffer.Begin ());
      TIM decoded; decoded.Deserialize (buffer.Begin ());
      NS_ABORT_MSG_IF (decoded.m_length != 160, "Dense slice roundtrip failed");
      S1gBeaconCompatibility compatibility; compatibility.SetBeaconInterval (102400);
      announcements[slice].SetBeaconCompatibility (compatibility);
      announcements[slice].SetChangeSequence (0); announcements[slice].SetNextTBTT (0);
      announcements[slice].SetCompressedSSID (0); announcements[slice].SetAccessNetwork (0);
      RPS rps; announcements[slice].SetRPS (rps);
      AuthenticationCtrl auth; auth.SetControlType (false); auth.SetThreshold (1000);
      announcements[slice].SetAuthCtrl (auth);
      announcements[slice].SetTIM (decoded);
      if (slice == 0) {
        pageSlice ps; ps.SetPageindex (page); ps.SetPagePeriod (8);
        ps.SetPageSliceCount (2); ps.SetPageSliceLen (16); ps.SetBlockOffset (0);
        ps.SetTIMOffset (0); ps.SetPageBitmap (0xffffffff);
        announcements[slice].SetpageSlice (ps);
      } // No Page Slice element in the second beacon, as on the wire.
    }
    // Exercise actual beacon serialization: the non-DTIM has no Page Slice IE.
    for (uint8_t slice = 0; slice < 2; ++slice) {
      Ptr<Packet> wire = Create<Packet> (); wire->AddHeader (announcements[slice]);
      S1gBeaconHeader received; wire->RemoveHeader (received); announcements[slice] = received;
    }
    if (page == 0)
      NS_ABORT_MSG_IF (StaWholePageSleepCheck::Read (sta, 1024, announcements[1]),
                       "STA must not use missing DTIM metadata");
    for (uint32_t local = page == 0 ? 1 : 0; local < 2048; ++local) {
      const uint32_t aid = page * 2048 + local;
      bool first = StaWholePageSleepCheck::Read (sta, aid, announcements[0]);
      bool second = StaWholePageSleepCheck::Read (sta, aid, announcements[1]);
      NS_ABORT_MSG_IF (first != (local < 1024) || second != (local >= 1024), "STA decoded wrong slice or lost pending bit");
      ++checked;
    }
  }
  Simulator::Destroy ();
  std::cout << "PASS: " << checked << " AIDs, two dense slices/page, 160 bytes/slice" << std::endl;
}
